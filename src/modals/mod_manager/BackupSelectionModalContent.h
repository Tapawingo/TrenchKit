/// @file BackupSelectionModalContent.h
/// @brief Modal for choosing a backup archive to restore.
#ifndef BACKUPSELECTIONMODALCONTENT_H
#define BACKUPSELECTIONMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include <QStringList>

class QListWidget;

/// @brief Presents a list of available backups; call @c getSelectedBackup() after @c Accepted.
class BackupSelectionModalContent : public BaseModalContent {
    Q_OBJECT

public:
    /// @param backups Internal backup file paths (returned by @c getSelectedBackup()).
    /// @param displayNames Human-readable labels shown in the list.
    explicit BackupSelectionModalContent(const QStringList &backups,
                                         const QStringList &displayNames,
                                         QWidget *parent = nullptr);

    /// @brief Returns the backup file path corresponding to the selected row.
    QString getSelectedBackup() const;

private:
    void setupUi(const QStringList &displayNames);

    QStringList m_backups;
    QListWidget *m_listWidget;
};

#endif // BACKUPSELECTIONMODALCONTENT_H
