#include "native_open_panel.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

#include "softloaf_trichrome/model.hpp"
#include "qt_path_utils.hpp"

namespace softloaf::trichrome::desktop {

std::vector<std::filesystem::path> NativeOpenFilesOrFolders(const char* title) {
    QMessageBox box;
    box.setWindowTitle(title ? QString::fromUtf8(title) : QStringLiteral("Import"));
    box.setText(QStringLiteral("Choose import source"));
    QAbstractButton* files_button =
        box.addButton(QStringLiteral("Files"), QMessageBox::AcceptRole);
    QAbstractButton* folder_button =
        box.addButton(QStringLiteral("Folder"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    std::vector<std::filesystem::path> out;
    if (box.clickedButton() == files_button) {
        const QStringList file_paths = QFileDialog::getOpenFileNames(
            nullptr,
            title ? QString::fromUtf8(title) : QStringLiteral("Choose trichrome photos"),
            QString(),
            QString::fromStdString(SupportedImagesFileFilter()));
        out.reserve(file_paths.size());
        for (const QString& path : file_paths) out.push_back(PathFromQString(path));
    } else if (box.clickedButton() == folder_button) {
        const QString folder = QFileDialog::getExistingDirectory(
            nullptr,
            title ? QString::fromUtf8(title) : QStringLiteral("Choose trichrome folder"));
        if (!folder.isEmpty()) out.push_back(PathFromQString(folder));
    }
    return out;
}

std::filesystem::path NativeChooseExportTarget(const char* title) {
    const QString folder = QFileDialog::getExistingDirectory(
        nullptr,
        title ? QString::fromUtf8(title) : QStringLiteral("Choose export folder"),
        QString(),
        QFileDialog::ShowDirsOnly);
    return folder.isEmpty() ? std::filesystem::path() : PathFromQString(folder);
}

}  // namespace softloaf::trichrome::desktop
