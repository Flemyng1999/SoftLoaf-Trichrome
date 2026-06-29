#pragma once

#include <filesystem>
#include <vector>

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QUrl>
#include <QVariantList>

#include "softloaf_trichrome/image_buf.hpp"
#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome::desktop {

class TrichromeImageProvider;

class TrichromeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(int activeGroup READ activeGroup NOTIFY activeGroupChanged)
    Q_PROPERTY(QString previewSource READ previewSource NOTIFY previewChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString sensorMode READ sensorMode WRITE setSensorMode NOTIFY sensorModeChanged)
    Q_PROPERTY(QString roleOrder READ roleOrder WRITE setRoleOrder NOTIFY roleOrderChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int completeGroupCount READ completeGroupCount NOTIFY groupsChanged)

 public:
    struct ExportSettings {
        QString format = "tiff";
        QString color_space = "aces_ap0_linear";
        int bit_depth = 16;
    };

    explicit TrichromeController(TrichromeImageProvider* image_provider,
                                 QObject* parent = nullptr);

    QVariantList groups() const { return groups_model_; }
    int activeGroup() const { return active_group_; }
    QString previewSource() const { return preview_source_; }
    QString status() const { return status_; }
    QString sensorMode() const { return sensor_mode_; }
    QString roleOrder() const { return role_order_; }
    QString sortMode() const { return sort_mode_; }
    bool hasPreview() const { return !preview_source_.isEmpty(); }
    bool busy() const { return busy_; }
    int completeGroupCount() const { return static_cast<int>(files_.size()) / 3; }

    Q_INVOKABLE void chooseImport();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setActiveGroup(int index);
    Q_INVOKABLE void composeActive();
    Q_INVOKABLE void chooseExport();
    Q_INVOKABLE void startExport(const QUrl& folder_url,
                                 bool export_all,
                                 const QString& format,
                                 const QString& color_space,
                                 int bit_depth);
    Q_INVOKABLE QString displayPath(const QUrl& url) const;
    Q_INVOKABLE void shutdown();

 public slots:
    void setSensorMode(const QString& value);
    void setRoleOrder(const QString& value);
    void setSortMode(const QString& value);

 signals:
    void groupsChanged();
    void activeGroupChanged();
    void previewChanged();
    void statusChanged();
    void sensorModeChanged();
    void roleOrderChanged();
    void sortModeChanged();
    void busyChanged();

 private:
    struct SourceFile {
        std::filesystem::path path;
        int selection_index = 0;
    };

    void setStatus(QString status);
    void setBusy(bool busy);
    std::vector<std::filesystem::path> resolveImportSelection(
        const std::vector<std::filesystem::path>& picked) const;
    void addFiles(std::vector<std::filesystem::path> paths);
    void addWorker(QThread* worker, const QString& task_name);
    void cancelAndDrainWorkers(int timeout_ms);
    void exportActiveTo(const QString& folder, const ExportSettings& settings);
    void exportAllTo(const QString& folder, const ExportSettings& settings);
    void startBackgroundFrameProcessing();
    void regroup();
    void rebuildGroupModel();
    ProjectTrichromeGroup projectGroupFor(int group_index) const;
    std::vector<std::filesystem::path> pathsForGroup(int group_index) const;
    QString fileFilter() const;
    QString publishPreview(const QImage& image);

    std::vector<SourceFile> files_;
    QVariantList groups_model_;
    int active_group_ = -1;
    QString preview_source_;
    QString status_ = "No photos loaded";
    QString sensor_mode_ = "bayer";
    QString role_order_ = "RGB";
    QString sort_mode_ = "filename";
    bool busy_ = false;
    int preview_rev_ = 0;
    int compose_generation_ = 0;
    int process_generation_ = 0;
    int export_generation_ = 0;
    int active_workers_ = 0;
    QImage current_preview_;
    TrichromeImageProvider* image_provider_ = nullptr;
    std::vector<QPointer<QThread>> workers_;
};

}  // namespace softloaf::trichrome::desktop
