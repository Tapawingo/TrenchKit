/**
 * @file AddModModalContent.h
 * @brief Modal for adding mods via local file, NexusMods URL, or itch.io URL.
 */
#ifndef ADDMODMODALCONTENT_H
#define ADDMODMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ItchUploadInfo.h"
#include "core/models/NexusFileInfo.h"
#include <QString>
#include <QDateTime>

class QEvent;
class ModManager;
class NexusModsClient;
class NexusModsAuth;
class ItchClient;
class ItchAuth;
class ModalManager;
class QPushButton;
class QLabel;
class QProgressBar;

/**
 * @brief Entry-point modal for adding new mods.
 *
 * Presents three paths: File (pak or archive), NexusMods URL, and itch.io URL.
 * Each path opens a sub-modal that collects the required metadata, which is
 * then handed back to this modal for batch processing.
 */
class AddModModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit AddModModalContent(ModManager *modManager,
                               NexusModsClient *nexusClient,
                               NexusModsAuth *nexusAuth,
                               ItchClient *itchClient,
                               ItchAuth *itchAuth,
                               ModalManager *modalManager,
                               QWidget *parent = nullptr);

signals:
    void modAdded(const QString &modName);

private slots:
    void onFromFileClicked();
    void onFromNexusClicked();
    void onFromItchClicked();
    void processNextFile();

protected:
    void changeEvent(QEvent *event) override;

private:
    /**
     * @brief Aggregated metadata for one pak file queued for installation.
     */
    struct FileToProcess {
        QString filePath;
        QString nexusModId;
        QString nexusFileId;
        QString nexusUrl;
        QString author;
        QString description;
        QString version;
        QString itchGameId;
        QString itchUrl;
        QString itchUploadId;
        QString customModName;
        QDateTime uploadDate;
    };

    void setupUi();
    void retranslateUi();
    void handleArchiveFile(const QString &archivePath, const QString &nexusModId = QString(), const QString &nexusFileId = QString(),
                           const QString &nexusUrl = QString(),
                           const QString &author = QString(), const QString &description = QString(), const QString &version = QString(),
                           const QString &itchGameId = QString(), const QString &itchUrl = QString(),
                           const QString &itchUploadId = QString(),
                           const QDateTime &uploadDate = QDateTime(), bool isBatchProcessing = false);
    void handlePakFile(const QString &pakPath, const QString &nexusModId = QString(), const QString &nexusFileId = QString(),
                       const QString &nexusUrl = QString(),
                       const QString &author = QString(), const QString &description = QString(), const QString &version = QString(),
                       const QString &itchGameId = QString(), const QString &itchUrl = QString(),
                       const QString &itchUploadId = QString(),
                       const QString &customModName = QString(), const QDateTime &uploadDate = QDateTime());
    bool isArchiveFile(const QString &filePath) const;
    void startProcessingFiles(const QList<FileToProcess> &files);

    ModManager *m_modManager;
    NexusModsClient *m_nexusClient;
    NexusModsAuth *m_nexusAuth;
    ItchClient *m_itchClient;
    ItchAuth *m_itchAuth;
    ModalManager *m_modalManager;
    QPushButton *m_fromFileButton;
    QPushButton *m_fromNexusButton;
    QPushButton *m_fromItchButton;
    QPushButton *m_cancelButton;
    QLabel *m_processingLabel = nullptr;
    QProgressBar *m_processingProgress = nullptr;
    QList<FileToProcess> m_filesToProcess;
    int m_currentFileIndex = 0;
    bool m_waitingForModal = false;
};

#endif // ADDMODMODALCONTENT_H
