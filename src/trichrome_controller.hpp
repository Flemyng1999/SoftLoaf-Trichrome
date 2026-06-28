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
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectPathChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

 public:
    explicit TrichromeController(TrichromeImageProvider* image_provider,
                                 QObject* parent = nullptr);

    QVariantList groups() const { return groups_model_; }
    int activeGroup() const { return active_group_; }
    QString previewSource() const { return preview_source_; }
    QString status() const { return status_; }
    QString sensorMode() const { return sensor_mode_; }
    QString roleOrder() const { return role_order_; }
    QString sortMode() const { return sort_mode_; }
    QString projectPath() const { return project_path_; }
    bool dirty() const { return dirty_; }
    bool hasPreview() const { return !preview_source_.isEmpty(); }
    bool busy() const { return busy_; }

    Q_INVOKABLE void chooseFiles();
    Q_INVOKABLE void chooseFolder();
    Q_INVOKABLE void openProject();
    Q_INVOKABLE void saveProject();
    Q_INVOKABLE void saveProjectAs();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setActiveGroup(int index);
    Q_INVOKABLE void composeActive();
    Q_INVOKABLE void exportActive();
    Q_INVOKABLE void exportAll();
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
    void projectPathChanged();
    void dirtyChanged();
    void busyChanged();

 private:
    struct SourceFile {
        std::filesystem::path path;
        int selection_index = 0;
    };

    void setStatus(QString status);
    void setBusy(bool busy);
    void setDirty(bool dirty);
    void addFiles(std::vector<std::filesystem::path> paths);
    void addWorker(QThread* worker, const QString& task_name);
    void cancelAndDrainWorkers(int timeout_ms);
    void regroup();
    void rebuildGroupModel();
    ProjectTrichromeGroup projectGroupFor(int group_index) const;
    std::vector<std::filesystem::path> pathsForGroup(int group_index) const;
    QString fileFilter() const;
    QString publishPreview(const QImage& image);
    bool saveProjectTo(const QString& path);
    bool loadProjectFrom(const QString& path);
    QString projectFileFilter() const;

    std::vector<SourceFile> files_;
    QVariantList groups_model_;
    int active_group_ = -1;
    QString preview_source_;
    QString status_ = "No photos loaded";
    QString sensor_mode_ = "mono";
    QString role_order_ = "RGB";
    QString sort_mode_ = "filename";
    QString project_path_;
    bool dirty_ = false;
    bool busy_ = false;
    int preview_rev_ = 0;
    int compose_generation_ = 0;
    int export_generation_ = 0;
    int active_workers_ = 0;
    QImage current_preview_;
    TrichromeImageProvider* image_provider_ = nullptr;
    std::vector<QPointer<QThread>> workers_;
};

}  // namespace softloaf::trichrome::desktop
