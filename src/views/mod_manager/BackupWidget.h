/**
 * @file BackupWidget.h
 * @brief Widget for creating and restoring mod collection backups.
 */
#ifndef BACKUPWIDGET_H
#define BACKUPWIDGET_H

#include <QWidget>
#include <QString>
#include <QEvent>

class QLabel;
class QPushButton;
class ModManager;
class ModalManager;

/**
 * @brief Provides backup and restore controls for the managed mod collection.
 *
 * Backups are zip archives stored in AppData/TrenchKit/backups/.
 * @c getBackupsPath() is the canonical path used by both create and restore operations.
 */
class BackupWidget : public QWidget {
    Q_OBJECT

public:
    explicit BackupWidget(QWidget *parent = nullptr);
    ~BackupWidget() override = default;

    void setModManager(ModManager *modManager);
    void setModalManager(ModalManager *modalManager) { m_modalManager = modalManager; }

protected:
    void changeEvent(QEvent *event) override;

signals:
    void errorOccurred(const QString &error);
    /**
     * @brief Emitted after a backup archive is created; @p fileCount is the number of pak files archived.
     */
    void backupCreated(int fileCount);
    /**
     * @brief Emitted after a backup is successfully restored.
     */
    void backupRestored(const QString &backupName);

private slots:
    void onCreateBackupClicked();
    void onRestoreBackupClicked();

private:
    void setupUi();
    void retranslateUi();
    void setupConnections();
    /**
     * @brief Returns the path to the backups storage directory.
     */
    QString getBackupsPath() const;

    ModManager *m_modManager = nullptr;
    ModalManager *m_modalManager = nullptr;

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_createBackupButton;
    QPushButton *m_restoreBackupButton;
};

#endif // BACKUPWIDGET_H
