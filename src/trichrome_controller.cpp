#include "trichrome_controller.hpp"

#include <algorithm>
#include <array>
#include <string>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <opencv2/imgproc.hpp>

#include "desktop_decoder.hpp"
#include "obs_log.hpp"
#include "project_store.hpp"
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

struct ExportTaskResult {
    int generation = 0;
    bool ok = false;
    int exported = 0;
    QString status;
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

QImage ComposeGroupToDisplayImage(const ProjectTrichromeGroup& group,
                                  const QString& sensor_mode,
                                  QString* reason) {
    const bool force_mono = sensor_mode != "bayer";
    ComposeResult result = ComposeGroupSequential(group, force_mono);
    if (!result.ok) {
        if (reason) *reason = QString::fromStdString(result.reason);
        return {};
    }
    const cv::Vec3d white = EstimateRgbChannelWhite(result.rgb);
    NormalizeRgbByChannelWhite(&result.rgb, white);
    QImage image = ImageBufToQImage(result.rgb);
    if (image.isNull() && reason) *reason = "preview_image_empty";
    else if (reason) *reason = "ok";
    return image;
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

void TrichromeController::openProject() {
    const QString picked = QFileDialog::getOpenFileName(
        nullptr, "Open SoftLoaf Trichrome project", QString(), projectFileFilter());
    if (picked.isEmpty()) return;
    loadProjectFrom(picked);
}

void TrichromeController::saveProject() {
    if (project_path_.isEmpty()) {
        saveProjectAs();
        return;
    }
    saveProjectTo(project_path_);
}

void TrichromeController::saveProjectAs() {
    QString picked = QFileDialog::getSaveFileName(
        nullptr, "Save SoftLoaf Trichrome project",
        project_path_.isEmpty() ? QString("Untitled.sltrichrome") : project_path_,
        projectFileFilter());
    if (picked.isEmpty()) return;
    if (!picked.endsWith(".sltrichrome", Qt::CaseInsensitive))
        picked += ".sltrichrome";
    saveProjectTo(picked);
}

void TrichromeController::clear() {
    ++compose_generation_;
    ++export_generation_;
    cancelAndDrainWorkers(30000);
    files_.clear();
    groups_model_.clear();
    active_group_ = -1;
    preview_source_.clear();
    current_preview_ = QImage();
    if (image_provider_) image_provider_->clearPreview();
    project_path_.clear();
    emit projectPathChanged();
    setDirty(false);
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
            QString reason;
            QImage image = ComposeGroupToDisplayImage(group, sensor_mode, &reason);
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
                task_result.status =
                    QString("Frame %1 composed").arg(group_index + 1);
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

void TrichromeController::exportActive() {
    if (active_group_ < 0 || active_group_ * 3 + 2 >= static_cast<int>(files_.size())) return;
    const QString suggested = QString("trichrome_frame_%1.png")
        .arg(active_group_ + 1, 4, 10, QLatin1Char('0'));
    const QString path = QFileDialog::getSaveFileName(
        nullptr, "Export trichrome frame", suggested,
        "PNG Image (*.png);;TIFF Image (*.tif *.tiff);;JPEG Image (*.jpg *.jpeg)");
    if (path.isEmpty()) return;
    const int generation = ++export_generation_;
    const int group_index = active_group_;
    const QString sensor_mode = sensor_mode_;
    const ProjectTrichromeGroup group = projectGroupFor(group_index);
    ObsLog("command.received", {{"command", "ExportFrame"},
                                {"group", std::to_string(group_index)}});
    setBusy(true);
    QPointer<TrichromeController> self(this);
    QThread* worker = QThread::create([self, generation, group, group_index,
                                       sensor_mode, path]() {
        ObsLog("task.started", {{"task", "ExportFrame"},
                                {"generation", std::to_string(generation)},
                                {"group", std::to_string(group_index)}});
        ExportTaskResult result;
        result.generation = generation;
        QString reason;
        const QImage image = ComposeGroupToDisplayImage(group, sensor_mode, &reason);
        if (image.isNull()) {
            result.status = reason;
        } else if (!image.save(path)) {
            result.status = "Export failed";
        } else {
            result.ok = true;
            result.exported = 1;
            result.status = "Exported " + QFileInfo(path).fileName();
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, result = std::move(result)]() {
            if (!self) return;
            if (result.generation != self->export_generation_) {
                ObsLog("task.stale_drop", {{"task", "ExportFrame"},
                                           {"generation", std::to_string(result.generation)}});
                return;
            }
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

void TrichromeController::exportAll() {
    const int complete_groups = static_cast<int>(files_.size()) / 3;
    if (complete_groups <= 0) return;
    const QString folder = QFileDialog::getExistingDirectory(
        nullptr, "Choose export folder");
    if (folder.isEmpty()) return;
    const int generation = ++export_generation_;
    const QString sensor_mode = sensor_mode_;
    std::vector<ProjectTrichromeGroup> groups;
    groups.reserve(complete_groups);
    for (int i = 0; i < complete_groups; ++i) groups.push_back(projectGroupFor(i));
    ObsLog("command.received", {{"command", "ExportAll"},
                                {"groups", std::to_string(complete_groups)}});
    setBusy(true);
    QPointer<TrichromeController> self(this);
    QThread* worker = QThread::create([self, generation, groups = std::move(groups),
                                       sensor_mode, folder]() {
        ObsLog("task.started", {{"task", "ExportAll"},
                                {"generation", std::to_string(generation)},
                                {"groups", std::to_string(groups.size())}});
        ExportTaskResult result;
        result.generation = generation;
        for (size_t i = 0; i < groups.size(); ++i) {
            QString reason;
            const QImage image = ComposeGroupToDisplayImage(groups[i], sensor_mode, &reason);
            if (image.isNull()) {
                result.status = QString("Frame %1: %2").arg(i + 1).arg(reason);
                break;
            }
            const QString out = QDir(folder).filePath(
                QString("trichrome_frame_%1.png")
                    .arg(static_cast<int>(i) + 1, 4, 10, QLatin1Char('0')));
            if (!image.save(out)) {
                result.status = QString("Frame %1 export failed").arg(i + 1);
                break;
            }
            ++result.exported;
        }
        result.ok = result.exported == static_cast<int>(groups.size());
        if (result.ok)
            result.status = QString("Exported %1 frames").arg(result.exported);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, result = std::move(result)]() {
            if (!self) return;
            if (result.generation != self->export_generation_) {
                ObsLog("task.stale_drop", {{"task", "ExportAll"},
                                           {"generation", std::to_string(result.generation)}});
                return;
            }
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
    setDirty(true);
    composeActive();
}

void TrichromeController::setRoleOrder(const QString& value) {
    if (value.size() != 3 || role_order_ == value) return;
    role_order_ = value;
    emit roleOrderChanged();
    rebuildGroupModel();
    setDirty(true);
    composeActive();
}

void TrichromeController::setSortMode(const QString& value) {
    if (value != "filename" && value != "selection") return;
    if (sort_mode_ == value) return;
    sort_mode_ = value;
    emit sortModeChanged();
    setDirty(true);
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

void TrichromeController::setDirty(bool dirty) {
    if (dirty_ == dirty) return;
    dirty_ = dirty;
    emit dirtyChanged();
}

void TrichromeController::addFiles(std::vector<std::filesystem::path> paths) {
    bool changed = false;
    for (std::filesystem::path& path : paths) {
        if (!IsImportableRawPath(path)) continue;
        files_.push_back(SourceFile{std::move(path), static_cast<int>(files_.size())});
        changed = true;
    }
    if (changed) setDirty(true);
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
    ++export_generation_;
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

void TrichromeController::shutdown() {
    cancelAndDrainWorkers(30000);
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
    return "Supported images (*.3fr *.fff *.dng *.arw *.cr2 *.cr3 *.nef *.raf *.raw *.rw2 *.orf *.pef *.srw *.tif *.tiff *.jpg *.jpeg *.png);;All files (*)";
}

QString TrichromeController::projectFileFilter() const {
    return "SoftLoaf Trichrome Project (*.sltrichrome);;All files (*)";
}

QString TrichromeController::publishPreview(const QImage& image) {
    if (image.isNull()) return {};
    if (image_provider_) image_provider_->setPreview(image);
    return QString("image://trichrome/preview?rev=%1").arg(++preview_rev_);
}

bool TrichromeController::saveProjectTo(const QString& path) {
    ProjectDocument doc;
    doc.sensor_mode = sensor_mode_.toStdString();
    doc.role_order = role_order_.toStdString();
    doc.sort_mode = sort_mode_.toStdString();
    doc.active_group = active_group_;
    doc.files.reserve(files_.size());
    for (const SourceFile& file : files_)
        doc.files.push_back(ProjectSourceFile{file.path, file.selection_index});
    std::string err;
    if (!SaveProjectDocument(path.toStdString(), doc, &err)) {
        setStatus("Save failed: " + QString::fromStdString(err));
        ObsLog("project.save", {{"result", "fail"}, {"reason", err}});
        return false;
    }
    project_path_ = path;
    emit projectPathChanged();
    setDirty(false);
    setStatus("Saved " + QFileInfo(path).fileName());
    ObsLog("project.save", {{"result", "ok"}, {"path", path.toStdString()}});
    return true;
}

bool TrichromeController::loadProjectFrom(const QString& path) {
    ProjectDocument doc;
    std::string err;
    if (!LoadProjectDocument(path.toStdString(), &doc, &err)) {
        setStatus("Open failed: " + QString::fromStdString(err));
        ObsLog("project.open", {{"result", "fail"}, {"reason", err}});
        return false;
    }
    cancelAndDrainWorkers(30000);
    files_.clear();
    for (const ProjectSourceFile& file : doc.files)
        files_.push_back(SourceFile{file.path, file.selection_index});
    sensor_mode_ = QString::fromStdString(doc.sensor_mode);
    role_order_ = QString::fromStdString(doc.role_order);
    sort_mode_ = QString::fromStdString(doc.sort_mode);
    project_path_ = path;
    active_group_ = doc.active_group;
    emit sensorModeChanged();
    emit roleOrderChanged();
    emit sortModeChanged();
    emit projectPathChanged();
    setDirty(false);
    regroup();
    setDirty(false);
    setStatus("Opened " + QFileInfo(path).fileName());
    ObsLog("project.open", {{"result", "ok"}, {"path", path.toStdString()}});
    return true;
}

}  // namespace softloaf::trichrome::desktop
