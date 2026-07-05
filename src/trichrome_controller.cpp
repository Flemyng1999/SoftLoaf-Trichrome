#include "trichrome_controller.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QColorSpace>
#include <QImage>
#include <QImageWriter>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QThread>

#include <opencv2/imgproc.hpp>

#include "desktop_decoder.hpp"
#include "export_naming.hpp"
#include "native_open_panel.hpp"
#include "obs_log.hpp"
#include "qt_path_utils.hpp"
#include "softloaf_trichrome/composer.hpp"
#include "softloaf_trichrome/color_management.hpp"
#include "softloaf_trichrome/raw_metadata.hpp"
#include "trichrome_cache.hpp"
#include "trichrome_image_provider.hpp"

namespace softloaf::trichrome::desktop {
namespace {

constexpr int kPreviewCacheMaxEdge = 2048;
constexpr int kBackgroundFrameConcurrencyCap = 4;
constexpr const char* kDisplayColorSpace = "srgb";
constexpr uint64_t kGiB = 1024ull * 1024ull * 1024ull;

QString BaseName(const std::filesystem::path& path) {
    return QStringFromPath(path.filename());
}

QString PathString(const std::filesystem::path& path) {
    return QStringFromPath(path);
}

QString RoleName(QChar c) {
    if (c == u'R') return "red";
    if (c == u'G') return "green";
    return "blue";
}

bool PathExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

int LogicalCpuCount() {
    return std::max(1, QThread::idealThreadCount());
}

uint64_t PhysicalMemoryBytes() {
#if defined(__APPLE__)
    uint64_t mem = 0;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0) return mem;
#elif defined(__linux__)
    struct sysinfo info {};
    if (sysinfo(&info) == 0)
        return static_cast<uint64_t>(info.totalram) * static_cast<uint64_t>(info.mem_unit);
#endif
    return 0;
}

int CpuBoundFrameConcurrency(int logical_cpus) {
    if (logical_cpus < 6) return 1;
    if (logical_cpus < 12) return 2;
    if (logical_cpus < 16) return 3;
    return kBackgroundFrameConcurrencyCap;
}

int MemoryBoundFrameConcurrency(uint64_t physical_memory) {
    if (physical_memory == 0) return 2;
    if (physical_memory < 12ull * kGiB) return 1;
    if (physical_memory < 24ull * kGiB) return 2;
    if (physical_memory < 48ull * kGiB) return 3;
    return kBackgroundFrameConcurrencyCap;
}

int BackgroundFrameConcurrency(int frame_count) {
    const int logical_cpus = LogicalCpuCount();
    const uint64_t physical_memory = PhysicalMemoryBytes();
    const int cpu_bound = CpuBoundFrameConcurrency(logical_cpus);
    const int memory_bound = MemoryBoundFrameConcurrency(physical_memory);
    return std::clamp(std::min({cpu_bound, memory_bound, frame_count}),
                      1, kBackgroundFrameConcurrencyCap);
}

struct ComposeTaskResult {
    int generation = 0;
    int group_index = -1;
    bool ok = false;
    bool cache_hit = false;
    QString status;
    QString preview_source;
    QImage image;
};

struct ExportTaskResult {
    int generation = 0;
    bool ok = false;
    int exported = 0;
    int total = 0;
    QString status;
};

QString NormalizeExportFormat(QString format) {
    format = format.trimmed().toLower();
    if (format == "tif") return "tiff";
    if (format == "dng" || format == "tiff") return format;
    return "tiff";
}

QString UnsupportedExportFormatReason(const QString& format) {
    if (format == "dng")
        return QStringLiteral("DNG export is not supported yet. Choose TIFF.");
    return {};
}

QString NormalizeExportColorSpace(QString color_space) {
    color_space = color_space.trimmed().toLower();
    color_space.replace("-", "_");
    color_space.replace(" ", "_");
    if (color_space == "aces_ap0" || color_space == "ap0" ||
        color_space == "aces2065_1" || color_space == "aces_2065_1") {
        return "aces_ap0_linear";
    }
    if (color_space == "acescg" || color_space == "ap1" ||
        color_space == "aces_ap1") {
        return "acescg_linear";
    }
    if (color_space == "prophoto" || color_space == "prophoto_rgb" ||
        color_space == "romm_rgb") {
        return "prophoto_linear";
    }
    if (color_space == "rec2020" || color_space == "bt2020" ||
        color_space == "bt_2020") {
        return "rec_2020_linear";
    }
    if (color_space == "displayp3") return "display_p3";
    if (color_space == "p3") return "display_p3";
    if (color_space == "p3_d65_gamma_26") return "p3_d65_gamma_2_6";
    if (color_space == "rec709") return "rec_709";
    if (color_space == "adobergb") return "adobe_rgb";
    if (color_space == "srgb" || color_space == "aces_ap0_linear" ||
        color_space == "acescg_linear" || color_space == "prophoto_linear" ||
        color_space == "rec_2020_linear" || color_space == "display_p3" ||
        color_space == "p3_d65_gamma_2_6" || color_space == "rec_709" ||
        color_space == "adobe_rgb") {
        return color_space;
    }
    return "aces_ap0_linear";
}

int NormalizeExportBitDepth(const QString& format, int bit_depth) {
    (void)format;
    return bit_depth >= 16 ? 16 : 8;
}

QByteArray ExportFormatName(const QString& format) {
    (void)format;
    return QByteArrayLiteral("tiff");
}

QString ExportExtension(const QString& format) {
    if (format == "dng") return "dng";
    return "tiff";
}

QString ExportColorSpaceLabel(const QString& color_space) {
    const auto* space = color::LookupColorSpace(color_space.toStdString());
    if (space) return QString::fromUtf8(space->label.data(), static_cast<int>(space->label.size()));
    return "sRGB";
}

QImage::Format ExportImageFormat(int bit_depth) {
    return bit_depth >= 16 ? QImage::Format_RGBX64 : QImage::Format_RGB888;
}

bool ExportCanEmbedIcc(const QString& color_space) {
    const auto* space = color::LookupColorSpace(color_space.toStdString());
    return space && !color::CreateIccProfile(*space).empty();
}

QString ExportEncodingLabel(const TrichromeController::ExportSettings& settings) {
    return QString("%1, %2-bit, %3")
        .arg(ExportColorSpaceLabel(settings.color_space))
        .arg(settings.bit_depth)
        .arg(ExportCanEmbedIcc(settings.color_space)
                 ? QStringLiteral("ICC embedded")
                 : QStringLiteral("no ICC profile"));
}

quint16 Quantize16(double value) {
    return static_cast<quint16>(
        std::lround(std::clamp(value, 0.0, 1.0) * 65535.0));
}

uchar Quantize8(double value) {
    return static_cast<uchar>(
        std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

QImage LinearRgbToQImage(const ImageBuf& image,
                         const QString& color_space,
                         int bit_depth) {
    if (image.empty() || image.data.depth() != CV_32F || image.data.channels() != 3)
        return {};

    const auto* space = color::LookupColorSpace(color_space.toStdString());
    if (!space) space = &color::kAcesAp0Linear;
    const QImage::Format format = ExportImageFormat(bit_depth);
    QImage out(image.width(), image.height(), format);
    for (int y = 0; y < image.height(); ++y) {
        const auto* src = image.data.ptr<cv::Vec3f>(y);
        if (format == QImage::Format_RGBX64) {
            auto* dst = reinterpret_cast<quint16*>(out.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const color::Vec3 encoded = color::ConvertLinearSrgbToEncoded(
                    *space, {src[x][0], src[x][1], src[x][2]});
                dst[4 * x + 0] = Quantize16(encoded[0]);
                dst[4 * x + 1] = Quantize16(encoded[1]);
                dst[4 * x + 2] = Quantize16(encoded[2]);
                dst[4 * x + 3] = 0xffff;
            }
        } else {
            auto* dst = out.scanLine(y);
            for (int x = 0; x < image.width(); ++x) {
                const color::Vec3 encoded = color::ConvertLinearSrgbToEncoded(
                    *space, {src[x][0], src[x][1], src[x][2]});
                dst[3 * x + 0] = Quantize8(encoded[0]);
                dst[3 * x + 1] = Quantize8(encoded[1]);
                dst[3 * x + 2] = Quantize8(encoded[2]);
            }
        }
    }

    const std::vector<unsigned char> icc = color::CreateIccProfile(*space);
    if (!icc.empty()) {
        const QByteArray bytes(reinterpret_cast<const char*>(icc.data()),
                               static_cast<int>(icc.size()));
        const QColorSpace qt_space = QColorSpace::fromIccProfile(bytes);
        if (qt_space.isValid()) out.setColorSpace(qt_space);
    }
    return out;
}

bool WriteExportImage(QImage image,
                      const QString& path,
                      const TrichromeController::ExportSettings& settings,
                      QString* reason) {
    QImageWriter writer(path, ExportFormatName(settings.format));
    if (!writer.write(image)) {
        if (reason) *reason = writer.errorString();
        return false;
    }
    if (reason) *reason = "ok";
    return true;
}

ComposeResult ComposeGroupSequential(const ProjectTrichromeGroup& group,
                                     bool force_mono,
                                     DecodeMode decode_mode) {
    ComposeResult result;
    const bool bayer = group.sensor_type == "bayer";
    struct DecodedSource {
        std::string role;
        ImageBuf image;
    };
    std::vector<DecodedSource> decoded_sources;
    decoded_sources.reserve(group.sources.size());
    try {
        for (const ProjectTrichromeSource& source : group.sources) {
            decoded_sources.push_back(
                {source.role,
                 DecodeLinear(PathFromQString(QString::fromUtf8(source.path)), force_mono,
                              decode_mode)});
        }
    } catch (...) {
        result.reason = "compose_decode_exception";
        return result;
    }

    if (bayer) {
        const ImageBuf* role_imgs[3] = {nullptr, nullptr, nullptr};
        for (const DecodedSource& ds : decoded_sources) {
            const int ch = ExpectedChannelForRole(ds.role);
            if (ch >= 0 && ch < 3) role_imgs[ch] = &ds.image;
        }
        if (role_imgs[0] && role_imgs[1] && role_imgs[2]) {
            std::string cross_reason;
            if (!CheckBayerRolesCrossFrame(
                    ResizeLongEdge(*role_imgs[0], 1024),
                    ResizeLongEdge(*role_imgs[1], 1024),
                    ResizeLongEdge(*role_imgs[2], 1024),
                    &result.warnings, &cross_reason)) {
                result.reason = cross_reason;
                return result;
            }
        }
    }

    ImageBuf red;
    ImageBuf green;
    ImageBuf blue;
    for (DecodedSource& source : decoded_sources) {
        if (bayer)
            source.image = ExtractRgbChannel(source.image, ExpectedChannelForRole(source.role));
        if (source.role == "red") red = std::move(source.image);
        else if (source.role == "green") green = std::move(source.image);
        else if (source.role == "blue") blue = std::move(source.image);
    }
    result = ComposeMonoRgb(red, green, blue);
    return result;
}

QImage ComposeGroupToEncodedImage(const ProjectTrichromeGroup& group,
                                  const QString& sensor_mode,
                                  DecodeMode decode_mode,
                                  int max_edge,
                                  const QString& output_color_space,
                                  int bit_depth,
                                  QString* reason) {
    const bool force_mono = sensor_mode != "bayer";
    ComposeResult result = ComposeGroupSequential(group, force_mono, decode_mode);
    if (!result.ok) {
        if (reason) *reason = QString::fromStdString(result.reason);
        return {};
    }
    const cv::Vec3d white = EstimateRgbChannelWhite(result.rgb);
    NormalizeRgbByChannelWhite(&result.rgb, white);
    if (max_edge > 0)
        result.rgb = ResizeLongEdge(result.rgb, max_edge);
    QImage image = LinearRgbToQImage(result.rgb, output_color_space, bit_depth);
    if (image.isNull() && reason) *reason = "preview_image_empty";
    else if (reason) *reason = "ok";
    return image;
}

QImage ComposeGroupToSrgbDisplayImage(const ProjectTrichromeGroup& group,
                                      const QString& sensor_mode,
                                      DecodeMode decode_mode,
                                     int max_edge,
                                     QString* reason) {
    return ComposeGroupToEncodedImage(group, sensor_mode, decode_mode, max_edge,
                                      kDisplayColorSpace, 8, reason);
}

TrichromeCacheInput CacheInputFor(const ProjectTrichromeGroup& group,
                                  const QString& sensor_mode,
                                  const QString& role_order,
                                  DecodeMode decode_mode) {
    TrichromeCacheInput input;
    input.sensor_mode = sensor_mode.toStdString();
    input.role_order = role_order.toStdString();
    input.decode_mode =
        decode_mode == DecodeMode::kPreview ? "preview" : "export";
    input.max_edge = decode_mode == DecodeMode::kPreview
        ? kPreviewCacheMaxEdge
        : 0;
    input.paths.reserve(group.sources.size());
    for (const ProjectTrichromeSource& source : group.sources)
        input.paths.emplace_back(source.path);
    return input;
}

ComposeTaskResult ComposePreviewFrame(int generation,
                                      int group_index,
                                      const ProjectTrichromeGroup& group,
                                      const QString& sensor_mode,
                                      const QString& role_order) {
    ComposeTaskResult task_result;
    task_result.generation = generation;
    task_result.group_index = group_index;

    TrichromePreviewCache cache;
    const TrichromeCacheInput cache_input =
        CacheInputFor(group, sensor_mode, role_order, DecodeMode::kPreview);
    const CacheLookupResult cached = cache.lookup(cache_input);
    if (cached.hit) {
        task_result.ok = true;
        task_result.cache_hit = true;
        task_result.image = cached.image;
        task_result.status = QString("Frame %1 restored from cache").arg(group_index + 1);
        return task_result;
    }

    ObsLog("trichrome.compose", {{"event", "start"},
                                 {"group", std::to_string(group_index)},
                                 {"sensor", sensor_mode.toStdString()},
                                 {"decode_mode", "preview"}});
    QString reason;
    QImage image = ComposeGroupToSrgbDisplayImage(
        group, sensor_mode, DecodeMode::kPreview,
        kPreviewCacheMaxEdge, &reason);
    if (image.isNull()) {
        task_result.status = reason;
    } else {
        std::filesystem::path cache_path;
        std::string write_reason;
        const bool cache_written =
            cache.write(cached.key, image, &cache_path, &write_reason);
        if (!cache_written) {
            ObsLog("cache.write", {{"category", "trichrome_preview"},
                                   {"result", "ignored"},
                                   {"reason", write_reason}});
        }
        task_result.ok = true;
        task_result.image = image;
        task_result.status = QString("Frame %1 composed").arg(group_index + 1);
        ObsLog("display.preview", {{"event", "cache_source_ready"},
                                   {"width", std::to_string(image.width())},
                                   {"height", std::to_string(image.height())},
                                   {"max_edge", std::to_string(kPreviewCacheMaxEdge)}});
    }
    ObsLog("trichrome.compose", {{"event", "finish"},
                                 {"group", std::to_string(group_index)},
                                 {"result", task_result.ok ? "ok" : "fail"}});
    return task_result;
}

}  // namespace

TrichromeController::TrichromeController(TrichromeImageProvider* image_provider,
                                         QObject* parent)
    : QObject(parent), image_provider_(image_provider) {}

void TrichromeController::chooseImport() {
    std::vector<std::filesystem::path> picked =
        NativeOpenFilesOrFolders("Choose trichrome photos or folder");
    if (picked.empty()) return;
    const std::vector<std::filesystem::path> paths = resolveImportSelection(picked);
    if (paths.empty()) return;

    if (!files_.empty()) {
        ++compose_generation_;
        ++process_generation_;
        ++export_generation_;
        cancelAndDrainWorkers(30000);
        files_.clear();
        groups_model_.clear();
        active_group_ = -1;
        preview_source_.clear();
        current_preview_ = QImage();
        if (image_provider_) image_provider_->clearPreview();
        emit groupsChanged();
        emit activeGroupChanged();
        emit previewChanged();
    }
    addFiles(paths);
}

std::vector<std::filesystem::path> TrichromeController::resolveImportSelection(
    const std::vector<std::filesystem::path>& picked) const {
    std::vector<std::filesystem::path> paths;
    bool includes_directory = false;
    for (const std::filesystem::path& picked_path : picked) {
        std::error_code ec;
        if (std::filesystem::is_directory(picked_path, ec)) {
            includes_directory = true;
            const std::string suffix = DetectDominantRawSuffix(picked_path);
            if (suffix.empty()) continue;
            std::vector<std::filesystem::path> folder_paths =
                CollectRollFrames(picked_path, suffix);
            paths.insert(paths.end(), folder_paths.begin(), folder_paths.end());
        } else if (std::filesystem::is_regular_file(picked_path, ec) &&
                   IsImportableRawPath(picked_path)) {
            paths.push_back(picked_path);
        }
    }
    if (includes_directory) {
        std::sort(paths.begin(), paths.end());
        paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    }
    return paths;
}

void TrichromeController::clear() {
    ++compose_generation_;
    ++process_generation_;
    ++export_generation_;
    cancelAndDrainWorkers(30000);
    files_.clear();
    groups_model_.clear();
    active_group_ = -1;
    preview_source_.clear();
    current_preview_ = QImage();
    if (image_provider_) image_provider_->clearPreview();
    setStatus("No photos loaded");
    emit groupsChanged();
    emit activeGroupChanged();
    emit previewChanged();
}

void TrichromeController::setActiveGroup(int index) {
    if (index == active_group_) return;
    active_group_ = index;
    preview_source_.clear();
    current_preview_ = QImage();
    if (image_provider_) image_provider_->clearPreview();
    emit activeGroupChanged();
    emit previewChanged();
    composeActive();
}

void TrichromeController::composeActive() {
    if (active_group_ < 0 || active_group_ * 3 + 2 >= static_cast<int>(files_.size())) {
        setStatus("Load at least one complete R/G/B group");
        return;
    }
    const std::vector<std::filesystem::path> active_paths = pathsForGroup(active_group_);
    for (const auto& path : active_paths) {
        if (!PathExists(path)) {
            setStatus("Frame has missing sources");
            return;
        }
    }
    const int generation = ++compose_generation_;
    const int group_index = active_group_;
    const QString sensor_mode = sensor_mode_;
    const QString role_order = role_order_;
    const ProjectTrichromeGroup group = projectGroupFor(group_index);
    ObsLog("command.received", {{"command", "ComposePreview"},
                                {"group", std::to_string(group_index)}});
    ObsLog("command.accepted", {{"command", "ComposePreview"},
                                {"policy", "latest_wins"},
                                {"generation", std::to_string(generation)}});
    setBusy(true);

    QPointer<TrichromeController> self(this);
    QThread* worker = QThread::create([self, generation, group_index, group,
                                       sensor_mode, role_order]() mutable {
        ObsLog("task.started", {{"task", "PreviewCompose"},
                                {"generation", std::to_string(generation)},
                                {"group", std::to_string(group_index)}});
        ComposeTaskResult task_result =
            ComposePreviewFrame(generation, group_index, group, sensor_mode, role_order);

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, task_result = std::move(task_result)]() mutable {
            if (!self) return;
            if (task_result.generation != self->compose_generation_) {
                ObsLog("task.stale_drop", {{"task", "PreviewCompose"},
                                           {"generation", std::to_string(task_result.generation)}});
                return;
            }
            if (task_result.ok) {
                self->preview_source_ = self->publishPreview(task_result.image);
                self->current_preview_ = task_result.image;
                ObsLog("display.preview", {{"event", "publish"},
                                           {"generation", std::to_string(task_result.generation)},
                                           {"source", "image_provider"}});
            } else {
                self->preview_source_.clear();
                self->current_preview_ = QImage();
                if (self->image_provider_) self->image_provider_->clearPreview();
            }
            emit self->previewChanged();
            self->setStatus(task_result.status);
            ObsLog("task.finished", {{"task", "PreviewCompose"},
                                     {"generation", std::to_string(task_result.generation)},
                                     {"result", task_result.ok ? "ok" : "fail"},
                                     {"cache", task_result.cache_hit ? "hit" : "miss"}});
        }, Qt::QueuedConnection);
    });
    worker->setObjectName("PreviewComposeWorker");
    worker->setStackSize(8 * 1024 * 1024);
    addWorker(worker, "PreviewCompose");
}

void TrichromeController::chooseExport() {
    const std::filesystem::path target = NativeChooseExportTarget("Choose export folder");
    if (target.empty()) return;
    exportActiveTo(QStringFromPath(target), ExportSettings{});
}

void TrichromeController::startExport(const QUrl& folder_url,
                                      bool export_all,
                                      const QString& format,
                                      const QString& color_space,
                                      int bit_depth,
                                      const QString& name_suffix) {
    if (exporting_) return;
    QString folder = LocalPathFromUrl(folder_url);
    if (folder.isEmpty()) {
        setStatus("Choose an export folder");
        return;
    }
    ExportSettings settings;
    settings.format = NormalizeExportFormat(format);
    settings.color_space = NormalizeExportColorSpace(color_space);
    settings.bit_depth = NormalizeExportBitDepth(settings.format, bit_depth);
    settings.name_suffix = NormalizeExportNameSuffix(name_suffix);
    const QString unsupported_reason = UnsupportedExportFormatReason(settings.format);
    if (!unsupported_reason.isEmpty()) {
        setExportProgress(false, 0, 0, unsupported_reason);
        setStatus(unsupported_reason);
        return;
    }
    requestBackgroundFrameProcessingStop();
    if (export_all)
        exportAllTo(folder, settings);
    else
        exportActiveTo(folder, settings);
}

QString TrichromeController::displayPath(const QUrl& url) const {
    return LocalPathFromUrl(url);
}

void TrichromeController::exportActiveTo(const QString& folder, const ExportSettings& settings) {
    if (active_group_ < 0 || active_group_ * 3 + 2 >= static_cast<int>(files_.size())) {
        setExportProgress(false, 0, 0, {});
        return;
    }
    const std::vector<std::filesystem::path> active_paths = pathsForGroup(active_group_);
    for (const auto& path : active_paths) {
        if (!PathExists(path)) {
            setExportProgress(false, 0, 0, {});
            setStatus("Resolve missing sources before export");
            return;
        }
    }
    if (folder.isEmpty()) {
        setExportProgress(false, 0, 0, {});
        return;
    }
    QDir dir(folder);
    if (!dir.exists() && !dir.mkpath(".")) {
        setExportProgress(false, 0, 0, {});
        setStatus("Could not create export folder");
        return;
    }
    const int generation = ++export_generation_;
    const int group_index = active_group_;
    const QString sensor_mode = sensor_mode_;
    const ProjectTrichromeGroup group = projectGroupFor(group_index);
    ObsLog("command.received", {{"command", "ExportFrame"},
                                {"group", std::to_string(group_index)},
                                {"format", settings.format.toStdString()},
                                {"bit_depth", std::to_string(settings.bit_depth)},
                                {"icc", ExportCanEmbedIcc(settings.color_space) ? "embedded" : "none"},
                                {"color_space", settings.color_space.toStdString()}});
    setExportProgress(true, 0, 1, QString("Exporting frame %1/1...").arg(group_index + 1));
    setStatus(export_progress_text_);
    setBusy(true);
    QPointer<TrichromeController> self(this);
    QThread* worker = QThread::create([self, generation, group, group_index,
                                       sensor_mode, folder, settings]() {
        ObsLog("task.started", {{"task", "ExportFrame"},
                                {"generation", std::to_string(generation)},
                                {"group", std::to_string(group_index)},
                                {"decode_mode", "export"},
                                {"format", settings.format.toStdString()},
                                {"bit_depth", std::to_string(settings.bit_depth)},
                                {"icc", ExportCanEmbedIcc(settings.color_space) ? "embedded" : "none"},
                                {"color_space", settings.color_space.toStdString()}});
        ExportTaskResult result;
        result.generation = generation;
        result.total = 1;
        QString reason;
        const QString out = UniqueExportPathForGroup(
            folder, group, ExportExtension(settings.format), settings.name_suffix);
        const QImage image = ComposeGroupToEncodedImage(
            group, sensor_mode, DecodeMode::kExport, 0,
            settings.color_space, settings.bit_depth, &reason);
        if (image.isNull()) {
            result.status = reason;
        } else {
            if (self) {
                const QString file_name = QFileInfo(out).fileName();
                QMetaObject::invokeMethod(self, [self, generation, file_name]() {
                    if (!self || generation != self->export_generation_) return;
                    self->setExportProgress(true, 0, 1, QString("Writing %1...").arg(file_name));
                    self->setStatus(self->export_progress_text_);
                }, Qt::QueuedConnection);
            }
            if (!WriteExportImage(image, out, settings, &reason)) {
                result.status = "Export failed: " + reason;
            } else {
                result.ok = true;
                result.exported = 1;
                result.status = QString("Exported %1 (%2)")
                    .arg(QFileInfo(out).fileName(), ExportEncodingLabel(settings));
            }
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, result = std::move(result)]() {
            if (!self) return;
            if (result.generation != self->export_generation_) {
                ObsLog("task.stale_drop", {{"task", "ExportFrame"},
                                           {"generation", std::to_string(result.generation)}});
                return;
            }
            self->setExportProgress(false, result.ok ? result.exported : 0,
                                    result.total, result.status);
            self->setStatus(result.status);
            ObsLog("task.finished", {{"task", "ExportFrame"},
                                     {"generation", std::to_string(result.generation)},
                                     {"result", result.ok ? "ok" : "fail"}});
        }, Qt::QueuedConnection);
    });
    worker->setObjectName("ExportFrameWorker");
    worker->setStackSize(8 * 1024 * 1024);
    addWorker(worker, "ExportFrame");
}

void TrichromeController::exportAllTo(const QString& folder, const ExportSettings& settings) {
    const int complete_groups = static_cast<int>(files_.size()) / 3;
    if (complete_groups <= 0) {
        setExportProgress(false, 0, 0, {});
        return;
    }
    for (int i = 0; i < complete_groups; ++i) {
        for (const auto& path : pathsForGroup(i)) {
            if (!PathExists(path)) {
                setExportProgress(false, 0, 0, {});
                setStatus("Resolve missing sources before export");
                return;
            }
        }
    }
    if (folder.isEmpty()) {
        setExportProgress(false, 0, 0, {});
        return;
    }
    QDir dir(folder);
    if (!dir.exists() && !dir.mkpath(".")) {
        setExportProgress(false, 0, 0, {});
        setStatus("Could not create export folder");
        return;
    }
    const int generation = ++export_generation_;
    const QString sensor_mode = sensor_mode_;
    std::vector<ProjectTrichromeGroup> groups;
    groups.reserve(complete_groups);
    for (int i = 0; i < complete_groups; ++i) groups.push_back(projectGroupFor(i));
    ObsLog("command.received", {{"command", "ExportAll"},
                                {"groups", std::to_string(complete_groups)},
                                {"format", settings.format.toStdString()},
                                {"bit_depth", std::to_string(settings.bit_depth)},
                                {"icc", ExportCanEmbedIcc(settings.color_space) ? "embedded" : "none"},
                                {"color_space", settings.color_space.toStdString()}});
    setExportProgress(true, 0, complete_groups,
                      QString("Exporting frame 1/%1...").arg(complete_groups));
    setStatus(export_progress_text_);
    setBusy(true);
    QPointer<TrichromeController> self(this);
    QThread* worker = QThread::create([self, generation, groups = std::move(groups),
                                       sensor_mode, folder, settings]() {
        ObsLog("task.started", {{"task", "ExportAll"},
                                {"generation", std::to_string(generation)},
                                {"groups", std::to_string(groups.size())},
                                {"decode_mode", "export"},
                                {"format", settings.format.toStdString()},
                                {"bit_depth", std::to_string(settings.bit_depth)},
                                {"icc", ExportCanEmbedIcc(settings.color_space) ? "embedded" : "none"},
                                {"color_space", settings.color_space.toStdString()}});
        ExportTaskResult result;
        result.generation = generation;
        result.total = static_cast<int>(groups.size());
        QSet<QString> reserved_paths;
        for (size_t i = 0; i < groups.size(); ++i) {
            if (self) {
                const int frame = static_cast<int>(i) + 1;
                const int total = static_cast<int>(groups.size());
                QMetaObject::invokeMethod(self, [self, generation, frame, total]() {
                    if (!self || generation != self->export_generation_) return;
                    self->setExportProgress(
                        true, frame - 1, total,
                        QString("Exporting frame %1/%2...").arg(frame).arg(total));
                    self->setStatus(self->export_progress_text_);
                }, Qt::QueuedConnection);
            }
            QString reason;
            const QString out = UniqueExportPathForGroup(
                folder, groups[i], ExportExtension(settings.format),
                settings.name_suffix, &reserved_paths);
            const QImage image = ComposeGroupToEncodedImage(
                groups[i], sensor_mode, DecodeMode::kExport, 0,
                settings.color_space, settings.bit_depth, &reason);
            if (image.isNull()) {
                result.status = QString("Frame %1: %2").arg(i + 1).arg(reason);
                break;
            }
            if (self) {
                const int frame = static_cast<int>(i) + 1;
                const int total = static_cast<int>(groups.size());
                const QString file_name = QFileInfo(out).fileName();
                QMetaObject::invokeMethod(self, [self, generation, frame, total, file_name]() {
                    if (!self || generation != self->export_generation_) return;
                    self->setExportProgress(
                        true, frame - 1, total,
                        QString("Writing %1 (%2/%3)...").arg(file_name).arg(frame).arg(total));
                    self->setStatus(self->export_progress_text_);
                }, Qt::QueuedConnection);
            }
            if (!WriteExportImage(image, out, settings, &reason)) {
                result.status = QString("Frame %1 export failed: %2").arg(i + 1).arg(reason);
                break;
            }
            ++result.exported;
            if (self) {
                const int exported = result.exported;
                const int total = static_cast<int>(groups.size());
                QMetaObject::invokeMethod(self, [self, generation, exported, total]() {
                    if (!self || generation != self->export_generation_) return;
                    self->setExportProgress(
                        true, exported, total,
                        QString("Exported %1/%2 frames").arg(exported).arg(total));
                    self->setStatus(self->export_progress_text_);
                }, Qt::QueuedConnection);
            }
        }
        result.ok = result.exported == static_cast<int>(groups.size());
        if (result.ok)
            result.status = QString("Exported %1 frames (%2)")
                .arg(result.exported)
                .arg(ExportEncodingLabel(settings));
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, result = std::move(result)]() {
            if (!self) return;
            if (result.generation != self->export_generation_) {
                ObsLog("task.stale_drop", {{"task", "ExportAll"},
                                           {"generation", std::to_string(result.generation)}});
                return;
            }
            self->setExportProgress(false, result.exported, result.total, result.status);
            self->setStatus(result.status);
            ObsLog("task.finished", {{"task", "ExportAll"},
                                     {"generation", std::to_string(result.generation)},
                                     {"result", result.ok ? "ok" : "fail"},
                                     {"exported", std::to_string(result.exported)}});
        }, Qt::QueuedConnection);
    });
    worker->setObjectName("ExportAllWorker");
    worker->setStackSize(8 * 1024 * 1024);
    addWorker(worker, "ExportAll");
}

void TrichromeController::setSensorMode(const QString& value) {
    if (value != "mono" && value != "bayer") return;
    if (sensor_mode_ == value) return;
    sensor_mode_ = value;
    emit sensorModeChanged();
    composeActive();
    startBackgroundFrameProcessing();
}

void TrichromeController::setRoleOrder(const QString& value) {
    if (value.size() != 3 || role_order_ == value) return;
    role_order_ = value;
    emit roleOrderChanged();
    rebuildGroupModel();
    composeActive();
    startBackgroundFrameProcessing();
}

void TrichromeController::setSortMode(const QString& value) {
    if (value != "filename" && value != "selection") return;
    if (sort_mode_ == value) return;
    sort_mode_ = value;
    emit sortModeChanged();
    regroup();
}

void TrichromeController::setStatus(QString status) {
    if (status_ == status) return;
    status_ = std::move(status);
    emit statusChanged();
}

void TrichromeController::setBusy(bool busy) {
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void TrichromeController::setExportProgress(bool exporting,
                                            int current,
                                            int total,
                                            QString text) {
    current = std::max(0, current);
    total = std::max(0, total);
    if (total > 0) current = std::min(current, total);
    if (exporting_ == exporting &&
        export_progress_current_ == current &&
        export_progress_total_ == total &&
        export_progress_text_ == text) {
        return;
    }
    exporting_ = exporting;
    export_progress_current_ = current;
    export_progress_total_ = total;
    export_progress_text_ = std::move(text);
    emit exportProgressChanged();
}

void TrichromeController::addFiles(std::vector<std::filesystem::path> paths) {
    bool changed = false;
    for (std::filesystem::path& path : paths) {
        if (!IsImportableRawPath(path)) continue;
        files_.push_back(SourceFile{std::move(path), static_cast<int>(files_.size())});
        changed = true;
    }
    if (!changed) return;
    regroup();
}

void TrichromeController::addWorker(QThread* worker, const QString& task_name) {
    if (!worker) return;
    workers_.push_back(worker);
    ++active_workers_;
    setBusy(active_workers_ > 0);
    ObsLog("worker.started", {{"task", task_name.toStdString()},
                              {"active", std::to_string(active_workers_)}});
    connect(worker, &QThread::finished, this, [this, worker, task_name]() {
        workers_.erase(std::remove_if(workers_.begin(), workers_.end(),
                                      [worker](const QPointer<QThread>& ptr) {
                                          return ptr.isNull() || ptr.data() == worker;
                                      }),
                       workers_.end());
        active_workers_ = std::max(0, active_workers_ - 1);
        setBusy(active_workers_ > 0);
        ObsLog("worker.finished", {{"task", task_name.toStdString()},
                                   {"active", std::to_string(active_workers_)}});
        worker->deleteLater();
    });
    worker->start();
}

void TrichromeController::cancelAndDrainWorkers(int timeout_ms) {
    ++compose_generation_;
    ++process_generation_;
    ++export_generation_;
    setExportProgress(false, 0, 0, {});
    ObsLog("worker.drain", {{"event", "start"},
                            {"active", std::to_string(active_workers_)}});
    const auto snapshot = workers_;
    for (const QPointer<QThread>& worker : snapshot) {
        if (!worker) continue;
        worker->requestInterruption();
        if (!worker->wait(timeout_ms)) {
            ObsLog("worker.drain", {{"event", "timeout"},
                                    {"object", worker->objectName().toStdString()}});
        }
    }
    active_workers_ = 0;
    workers_.clear();
    setBusy(false);
    ObsLog("worker.drain", {{"event", "finish"}});
}

void TrichromeController::requestBackgroundFrameProcessingStop() {
    ++process_generation_;
    for (const QPointer<QThread>& worker : workers_) {
        if (worker && worker->objectName() == "ProcessImportedFramesWorker")
            worker->requestInterruption();
    }
    ObsLog("worker.interrupt", {{"task", "ProcessImportedFrames"},
                                {"reason", "export_requested"}});
}

void TrichromeController::shutdown() {
    cancelAndDrainWorkers(30000);
}

void TrichromeController::startBackgroundFrameProcessing() {
    const int complete_groups = static_cast<int>(files_.size()) / 3;
    if (complete_groups <= 0) return;
    for (int i = 0; i < complete_groups; ++i) {
        for (const auto& path : pathsForGroup(i)) {
            if (!PathExists(path)) {
                setStatus("Frame has missing sources");
                return;
            }
        }
    }

    for (const QPointer<QThread>& worker : workers_) {
        if (worker && worker->objectName() == "ProcessImportedFramesWorker")
            worker->requestInterruption();
    }

    const int generation = ++process_generation_;
    const QString sensor_mode = sensor_mode_;
    const QString role_order = role_order_;
    auto groups = std::make_shared<std::vector<ProjectTrichromeGroup>>();
    groups->reserve(complete_groups);
    for (int i = 0; i < complete_groups; ++i) groups->push_back(projectGroupFor(i));
    const int logical_cpus = LogicalCpuCount();
    const uint64_t physical_memory = PhysicalMemoryBytes();
    const int cpu_bound = CpuBoundFrameConcurrency(logical_cpus);
    const int memory_bound = MemoryBoundFrameConcurrency(physical_memory);
    const int worker_count = BackgroundFrameConcurrency(complete_groups);
    auto next_group = std::make_shared<std::atomic<int>>(0);
    auto processed = std::make_shared<std::atomic<int>>(0);
    auto failed = std::make_shared<std::atomic<int>>(0);
    auto remaining_workers = std::make_shared<std::atomic<int>>(worker_count);

    ObsLog("command.accepted", {{"command", "ProcessImportedFrames"},
                                {"policy", "parallel_limited"},
                                {"generation", std::to_string(generation)},
                                {"groups", std::to_string(complete_groups)},
                                {"workers", std::to_string(worker_count)},
                                {"logical_cpus", std::to_string(logical_cpus)},
                                {"memory_gib", std::to_string(physical_memory / kGiB)},
                                {"cpu_bound", std::to_string(cpu_bound)},
                                {"memory_bound", std::to_string(memory_bound)}});
    setStatus(QString("Processing 0/%1 frames").arg(complete_groups));

    QPointer<TrichromeController> self(this);
    for (int lane = 0; lane < worker_count; ++lane) {
        QThread* worker = QThread::create(
            [self, generation, groups, sensor_mode, role_order, next_group,
             processed, failed, remaining_workers, lane]() mutable {
                const int total = static_cast<int>(groups->size());
                ObsLog("task.started", {{"task", "ProcessImportedFrames"},
                                        {"generation", std::to_string(generation)},
                                        {"groups", std::to_string(total)},
                                        {"decode_mode", "preview"},
                                        {"lane", std::to_string(lane)}});

                while (!QThread::currentThread()->isInterruptionRequested()) {
                    const int group_index = next_group->fetch_add(1);
                    if (group_index >= total) break;

                    ComposeTaskResult task_result = ComposePreviewFrame(
                        generation, group_index, (*groups)[group_index], sensor_mode, role_order);
                    const int processed_count = processed->fetch_add(1) + 1;
                    if (!task_result.ok) failed->fetch_add(1);

                    if (!self) return;
                    QMetaObject::invokeMethod(
                        self,
                        [self, task_result = std::move(task_result), processed_count, total,
                         lane]() mutable {
                            if (!self) return;
                            if (task_result.generation != self->process_generation_) {
                                ObsLog("task.stale_drop",
                                       {{"task", "ProcessImportedFrames"},
                                        {"generation", std::to_string(task_result.generation)},
                                        {"group", std::to_string(task_result.group_index)}});
                                return;
                            }
                            if (task_result.ok &&
                                task_result.group_index == self->active_group_) {
                                self->preview_source_ =
                                    self->publishPreview(task_result.image);
                                self->current_preview_ = task_result.image;
                                emit self->previewChanged();
                                ObsLog("display.preview",
                                       {{"event", "publish"},
                                        {"generation", std::to_string(task_result.generation)},
                                        {"group", std::to_string(task_result.group_index)},
                                        {"source", "background_cache"}});
                            }
                            if (task_result.ok) {
                                self->setStatus(QString("Processed %1/%2 frames")
                                                    .arg(processed_count)
                                                    .arg(total));
                            } else {
                                self->setStatus(QString("Frame %1: %2")
                                                    .arg(task_result.group_index + 1)
                                                    .arg(task_result.status));
                            }
                            ObsLog("task.progress",
                                   {{"task", "ProcessImportedFrames"},
                                    {"generation", std::to_string(task_result.generation)},
                                    {"group", std::to_string(task_result.group_index)},
                                    {"processed", std::to_string(processed_count)},
                                    {"total", std::to_string(total)},
                                    {"result", task_result.ok ? "ok" : "fail"},
                                    {"cache", task_result.cache_hit ? "hit" : "miss"},
                                    {"lane", std::to_string(lane)}});
                        },
                        Qt::QueuedConnection);
                }

                const int remaining = remaining_workers->fetch_sub(1) - 1;
                if (remaining != 0 || !self) return;
                const int processed_count = processed->load();
                const bool all_ok = failed->load() == 0 && processed_count == total;
                QMetaObject::invokeMethod(
                    self,
                    [self, generation, processed_count, total, all_ok]() {
                        if (!self) return;
                        if (generation != self->process_generation_) {
                            ObsLog("task.stale_drop",
                                   {{"task", "ProcessImportedFrames"},
                                    {"generation", std::to_string(generation)}});
                            return;
                        }
                        if (all_ok) {
                            self->setStatus(QString("Processed %1 frames").arg(total));
                        }
                        ObsLog("task.finished", {{"task", "ProcessImportedFrames"},
                                                 {"generation", std::to_string(generation)},
                                                 {"result", all_ok ? "ok" : "fail"},
                                                 {"processed", std::to_string(processed_count)},
                                                 {"total", std::to_string(total)}});
                    },
                    Qt::QueuedConnection);
            });
        worker->setObjectName("ProcessImportedFramesWorker");
        worker->setStackSize(8 * 1024 * 1024);
        addWorker(worker, "ProcessImportedFrames");
    }
}

void TrichromeController::regroup() {
    ++compose_generation_;
    ++process_generation_;
    if (sort_mode_ == "filename") {
        std::sort(files_.begin(), files_.end(), [](const SourceFile& a, const SourceFile& b) {
            return QString::compare(QStringFromPath(a.path.filename()),
                                    QStringFromPath(b.path.filename()),
                                    Qt::CaseSensitive) < 0;
        });
    } else {
        std::sort(files_.begin(), files_.end(), [](const SourceFile& a, const SourceFile& b) {
            return a.selection_index < b.selection_index;
        });
    }
    const int complete_groups = static_cast<int>(files_.size()) / 3;
    if (complete_groups == 0) {
        active_group_ = -1;
        preview_source_.clear();
        current_preview_ = QImage();
        if (image_provider_) image_provider_->clearPreview();
    } else if (active_group_ < 0 || active_group_ >= complete_groups) {
        active_group_ = 0;
    }
    rebuildGroupModel();
    emit activeGroupChanged();
    emit previewChanged();
    setStatus(QString("%1 photos · %2 complete frames")
                  .arg(files_.size())
                  .arg(complete_groups));
    startBackgroundFrameProcessing();
}

void TrichromeController::rebuildGroupModel() {
    groups_model_.clear();
    const int group_count = static_cast<int>((files_.size() + 2) / 3);
    for (int group = 0; group < group_count; ++group) {
        QVariantMap item;
        item["index"] = group;
        const bool has_three = group * 3 + 2 < static_cast<int>(files_.size());
        item["complete"] = has_three;
        item["title"] = QString("Frame %1").arg(group + 1);
        QVariantList sources;
        for (int offset = 0; offset < 3 && group * 3 + offset < static_cast<int>(files_.size()); ++offset) {
            const int source_index = group * 3 + offset;
            QVariantMap source;
            source["role"] = RoleName(role_order_[offset]);
            source["roleShort"] = QString(role_order_[offset]);
            source["name"] = BaseName(files_[source_index].path);
            source["path"] = PathString(files_[source_index].path);
            source["sourceIndex"] = source_index;
            sources.push_back(source);
        }
        item["sources"] = sources;
        groups_model_.push_back(item);
    }
    emit groupsChanged();
}

ProjectTrichromeGroup TrichromeController::projectGroupFor(int group_index) const {
    ProjectTrichromeGroup group;
    group.mode = "trichrome_rgb";
    group.sensor_type = sensor_mode_.toStdString();
    group.strict_order = "RGB";
    group.validation_status = "valid";
    group.group_index = group_index;
    group.group_size = 3;
    group.logical_frame_index = group_index;
    group.artifact_path = QString("frame_%1.npy")
        .arg(group_index, 4, 10, QLatin1Char('0')).toStdString();
    group.artifact_format = "rgb16_npy";
    group.alignment_policy = "identity";
    group.alignment_applied = false;
    for (int offset = 0; offset < 3; ++offset) {
        const QChar role = role_order_[offset];
        ProjectTrichromeSource source;
        source.role = RoleName(role).toStdString();
        source.path = QStringFromPath(files_[group_index * 3 + offset].path)
            .toUtf8()
            .toStdString();
        group.sources.push_back(std::move(source));
    }
    return group;
}

std::vector<std::filesystem::path> TrichromeController::pathsForGroup(int group_index) const {
    std::vector<std::filesystem::path> paths;
    if (group_index < 0 || group_index * 3 + 2 >= static_cast<int>(files_.size()))
        return paths;
    paths.reserve(3);
    for (int offset = 0; offset < 3; ++offset)
        paths.push_back(files_[group_index * 3 + offset].path);
    return paths;
}

QString TrichromeController::fileFilter() const {
    return QString::fromStdString(SupportedImagesFileFilter());
}

QString TrichromeController::publishPreview(const QImage& image) {
    if (image.isNull()) return {};
    if (image_provider_) image_provider_->setPreview(image);
    return QString("image://trichrome/preview?rev=%1").arg(++preview_rev_);
}

}  // namespace softloaf::trichrome::desktop
