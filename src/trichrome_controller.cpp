#include "trichrome_controller.hpp"

#include <algorithm>
#include <array>
#include <string>

#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QStandardPaths>

#include <opencv2/imgproc.hpp>

#include "desktop_decoder.hpp"
#include "softloaf_trichrome/composer.hpp"

namespace softloaf::trichrome::desktop {
namespace {

QString BaseName(const std::filesystem::path& path) {
    return QString::fromStdString(path.filename().string());
}

QString PathString(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}

InputSensorType SensorFromString(const QString& sensor) {
    return sensor == "bayer" ? InputSensorType::kBayer : InputSensorType::kMono;
}

QString RoleName(QChar c) {
    if (c == u'R') return "red";
    if (c == u'G') return "green";
    return "blue";
}

InputGroupRole RoleEnum(QChar c) {
    if (c == u'R') return InputGroupRole::kTrichromeRed;
    if (c == u'G') return InputGroupRole::kTrichromeGreen;
    return InputGroupRole::kTrichromeBlue;
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

}  // namespace

TrichromeController::TrichromeController(QObject* parent) : QObject(parent) {}

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
    files_.clear();
    groups_model_.clear();
    active_group_ = -1;
    preview_source_.clear();
    setStatus("No photos loaded");
    emit groupsChanged();
    emit activeGroupChanged();
    emit previewChanged();
}

void TrichromeController::setActiveGroup(int index) {
    if (index == active_group_) return;
    active_group_ = index;
    preview_source_.clear();
    emit activeGroupChanged();
    emit previewChanged();
    composeActive();
}

void TrichromeController::composeActive() {
    if (active_group_ < 0 || active_group_ * 3 + 2 >= static_cast<int>(files_.size())) {
        setStatus("Load at least one complete R/G/B group");
        return;
    }
    setBusy(true);
    const ProjectTrichromeGroup group = projectGroupFor(active_group_);
    const bool force_mono = sensor_mode_ != "bayer";
    ComposeResult result = ComposeGroup(group, [force_mono](const std::filesystem::path& path) {
        return DecodeLinear(path, force_mono);
    });
    if (!result.ok) {
        preview_source_.clear();
        emit previewChanged();
        setStatus(QString::fromStdString(result.reason));
        setBusy(false);
        return;
    }
    const cv::Vec3d white = EstimateRgbChannelWhite(result.rgb);
    NormalizeRgbByChannelWhite(&result.rgb, white);
    preview_source_ = writePreviewPng(result.rgb);
    emit previewChanged();
    setStatus(QString("Frame %1 composed").arg(active_group_ + 1));
    setBusy(false);
}

void TrichromeController::exportActive() {
    if (!hasPreview()) return;
    const QString suggested = QString("trichrome_frame_%1.png")
        .arg(active_group_ + 1, 4, 10, QLatin1Char('0'));
    const QString path = QFileDialog::getSaveFileName(
        nullptr, "Export trichrome frame", suggested,
        "PNG Image (*.png);;TIFF Image (*.tif *.tiff);;JPEG Image (*.jpg *.jpeg)");
    if (path.isEmpty()) return;
    QImage image;
    image.load(QUrl(preview_source_).toLocalFile());
    if (!image.save(path)) {
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

QString TrichromeController::writePreviewPng(const ImageBuf& image) {
    QImage q = ImageBufToQImage(image);
    if (q.isNull()) return {};
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    QDir().mkpath(dir + "/softloaf_trichrome");
    const QString path = QString("%1/softloaf_trichrome/preview_%2.png")
        .arg(dir)
        .arg(++preview_rev_);
    q.save(path);
    return QUrl::fromLocalFile(path).toString();
}

}  // namespace softloaf::trichrome::desktop
