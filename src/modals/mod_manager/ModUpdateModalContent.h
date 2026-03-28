/// @file ModUpdateModalContent.h
/// @brief Modal that auto-downloads and installs a NexusMods mod update.
#ifndef MODUPDATEMODALCONTENT_H
#define MODUPDATEMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ModInfo.h"
#include "core/models/ModUpdateInfo.h"
#include <QString>

class ModManager;
class NexusModsClient;
class ModalManager;
class QLabel;
class QProgressBar;
class QPushButton;

/// @brief Shown when the user requests an update for a NexusMods-linked mod.
///
/// Immediately starts downloading the update on @c showEvent. The progress
/// bar and status label update as the download proceeds. On completion,
/// replaces the mod's pak via @c ModManager::replaceMod() and emits @c finished.
class ModUpdateModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit ModUpdateModalContent(const ModInfo &mod,
                                  const ModUpdateInfo &updateInfo,
                                  ModManager *modManager,
                                  NexusModsClient *nexusClient,
                                  ModalManager *modalManager,
                                  QWidget *parent = nullptr);

private slots:
    void onDownloadLinkReceived(const QString &url);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString &savePath);
    void onError(const QString &error);
    void onCancelClicked();

private:
    void setupUi();
    void startDownload();
    QString generateTempPath(const QString &fileName) const;

    ModInfo m_mod;
    ModUpdateInfo m_updateInfo;
    ModManager *m_modManager;
    NexusModsClient *m_nexusClient;
    ModalManager *m_modalManager;

    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_cancelButton;

    QString m_downloadedPath;
};

#endif // MODUPDATEMODALCONTENT_H
