#include "trichrome_controller.hpp"

#include <algorithm>
#include <array>
#include <string>

#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <opencv2/imgproc.hpp>

#include "desktop_decoder.hpp"
#include "obs_log.hpp"
#include "softloaf_trichrome/composer.hpp"
#include "trichrome_cache.hpp"
#include "trichrome_image_provider.hpp"

namespace softloaf::trichrome::desktop {
namespace {

QString BaseName(const std::filesystem::path& path) {
    return QString::fromStdString(path.filename().string());
}

QString PathString(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}

QString RoleName(QChar c) {
    if (c == u'R') return "red";
    if (c == u'G') return "green";
    return "blue";
}

QImage ImageBufToQImage(const ImageBuf& image) {
    if (image.empty() || image.data.depth() != CV_32F || image.data.channels() != 3)
        return {};
    cv::Mat clamped;
    cv::min(image.data, 1.0, clamped);
    cv::max(clamped, 0.0, clamped);
    cv::Mat rgb8;
    clamped.convertTo(rgb8, CV_8UC3, 255.0);
    QImage q(rgb8.data, rgb8.cols, rgb8.rows, static_cast<int>(rgb8.step),
             QImage::Format_RGB888);
    return q.copy();
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

ComposeResult ComposeGroupSequential(const ProjectTrichromeGroup& group,
                                     bool force_mono) {
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
                {source.role, DecodeLinear(source.path, force_mono)});
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

TrichromeCacheInput CacheInputFor(const ProjectTrichromeGroup& group,
                                  const QString& sensor_mode,
                                  const QString& role_order) {
    TrichromeCacheInput input;
    input.sensor_mode = sensor_mode.toStdString();
    input.role_order = role_order.toStdString();
    input.paths.reserve(group.sources.size());
    for (const ProjectTrichromeSource& source : group.sources)
        input.paths.emplace_back(source.path);
    return input;
}

}  // namespace

TrichromeController::TrichromeController(TrichromeImageProvider* image_provider,
                                         QObject* parent)
    : QObject(parent), image_provider_(image_provider) {}

void TrichromeController::chooseFiles() {
    const QStringList picked = QFileDialog::getOpenFileNames(
        nullptr, "Choose trichrome photos", QString(), fileFilter());
    std::vector<std::filesystem::path> paths;
    paths.reserve(picked.size());
    for (const QString& path : picked) paths.emplace_back(path.toStdString());
    addFiles(std::move(paths));
}

void TrichromeController::chooseFolder() {
    const QString folder = QFileDialog::getExistingDirectory(
        nullptr, "Choose trichrome folder");
    if (folder.isEmpty()) return;
    std::vector<std::filesystem::path> paths;
    QDir dir(folder);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& info : entries) {
        std::filesystem::path path(info.absoluteFilePath().toStdString());
        if (IsImportableRawPath(path)) paths.push_back(std::move(path));
    }
    addFiles(std::move(paths));
}

void TrichromeController::clear() {
    ++compose_generation_;
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
        ComposeTaskResult task_result;
        task_result.generation = generation;
        task_result.group_index = group_index;

        TrichromePreviewCache cache;
        const TrichromeCacheInput cache_input =
            CacheInputFor(group, sensor_mode, role_order);
        const CacheLookupResult cached = cache.lookup(cache_input);
        if (cached.hit) {
            task_result.ok = true;
            task_result.cache_hit = true;
            task_result.image = cached.image;
            task_result.status = QString("Frame %1 restored from cache").arg(group_index + 1);
        } else {
            const bool force_mono = sensor_mode != "bayer";
            ObsLog("trichrome.compose", {{"event", "start"},
                                         {"group", std::to_string(group_index)},
                                         {"sensor", sensor_mode.toStdString()}});
            ComposeResult result = ComposeGroupSequential(group, force_mono);
            if (!result.ok) {
                task_result.status = QString::fromStdString(result.reason);
            } else {
                const cv::Vec3d white = EstimateRgbChannelWhite(result.rgb);
                NormalizeRgbByChannelWhite(&result.rgb, white);
                QImage image = ImageBufToQImage(result.rgb);
                if (image.isNull()) {
                    task_result.status = "preview_image_empty";
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
                    task_result.status =
                        QString("Frame %1 composed").arg(group_index + 1);
                }
            }
            ObsLog("trichrome.compose", {{"event", "finish"},
                                         {"group", std::to_string(group_index)},
                                         {"result", task_result.ok ? "ok" : "fail"}});
        }

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
            self->setBusy(false);
            ObsLog("task.finished", {{"task", "PreviewCompose"},
                                     {"generation", std::to_string(task_result.generation)},
                                     {"result", task_result.ok ? "ok" : "fail"},
                                     {"cache", task_result.cache_hit ? "hit" : "miss"}});
        }, Qt::QueuedConnection);
    });
    worker->setObjectName("PreviewComposeWorker");
    worker->setStackSize(8 * 1024 * 1024);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void TrichromeController::exportActive() {
    if (!hasPreview() || current_preview_.isNull()) return;
    const QString suggested = QString("trichrome_frame_%1.png")
        .arg(active_group_ + 1, 4, 10, QLatin1Char('0'));
    const QString path = QFileDialog::getSaveFileName(
        nullptr, "Export trichrome frame", suggested,
        "PNG Image (*.png);;TIFF Image (*.tif *.tiff);;JPEG Image (*.jpg *.jpeg)");
    if (path.isEmpty()) return;
    if (!current_preview_.save(path)) {
        setStatus("Export failed");
        return;
    }
    setStatus("Exported " + QFileInfo(path).fileName());
}

void TrichromeController::setSensorMode(const QString& value) {
    if (value != "mono" && value != "bayer") return;
    if (sensor_mode_ == value) return;
    sensor_mode_ = value;
    emit sensorModeChanged();
    composeActive();
}

void TrichromeController::setRoleOrder(const QString& value) {
    if (value.size() != 3 || role_order_ == value) return;
    role_order_ = value;
    emit roleOrderChanged();
    rebuildGroupModel();
    composeActive();
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

void TrichromeController::addFiles(std::vector<std::filesystem::path> paths) {
    for (std::filesystem::path& path : paths) {
        if (!IsImportableRawPath(path)) continue;
        files_.push_back(SourceFile{std::move(path), static_cast<int>(files_.size())});
    }
    regroup();
}

void TrichromeController::regroup() {
    ++compose_generation_;
    if (sort_mode_ == "filename") {
        std::sort(files_.begin(), files_.end(), [](const SourceFile& a, const SourceFile& b) {
            return a.path.filename().string() < b.path.filename().string();
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
    composeActive();
}

void TrichromeController::rebuildGroupModel() {
    groups_model_.clear();
    const int group_count = static_cast<int>((files_.size() + 2) / 3);
    for (int group = 0; group < group_count; ++group) {
        QVariantMap item;
        item["index"] = group;
        item["complete"] = group * 3 + 2 < static_cast<int>(files_.size());
        item["title"] = QString("Frame %1").arg(group + 1);
        QVariantList sources;
        for (int offset = 0; offset < 3 && group * 3 + offset < static_cast<int>(files_.size()); ++offset) {
            QVariantMap source;
            source["role"] = RoleName(role_order_[offset]);
            source["roleShort"] = QString(role_order_[offset]);
            source["name"] = BaseName(files_[group * 3 + offset].path);
            source["path"] = PathString(files_[group * 3 + offset].path);
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
        source.path = files_[group_index * 3 + offset].path.string();
        group.sources.push_back(std::move(source));
    }
    return group;
}

QString TrichromeController::fileFilter() const {
    return "Supported images (*.3fr *.fff *.dng *.arw *.cr2 *.cr3 *.nef *.raf *.raw *.rw2 *.orf *.pef *.srw *.tif *.tiff *.jpg *.jpeg *.png);;All files (*)";
}

QString TrichromeController::publishPreview(const QImage& image) {
    if (image.isNull()) return {};
    if (image_provider_) image_provider_->setPreview(image);
    return QString("image://trichrome/preview?rev=%1").arg(++preview_rev_);
}

}  // namespace softloaf::trichrome::desktop
