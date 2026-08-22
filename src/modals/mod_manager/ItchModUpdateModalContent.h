/**
 * @file ItchModUpdateModalContent.h
 * @brief Modal that auto-downloads and installs an itch.io mod update.
 */
#ifndef ITCHMODUPDATEMODALCONTENT_H
#define ITCHMODUPDATEMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ModInfo.h"
#include "core/models/ItchUpdateInfo.h"
#include <QString>

class ModManager;
class ItchClient;
class ModalManager;
class QLabel;
class QProgressBar;
class QPushButton;

/**
 * @brief Shown when the user requests an update for an itch.io-linked mod.
 *
 * Expects an already-resolved @c ItchUpdateInfo naming a single upload to
 * install; if multiple candidate uploads exist, the caller
 * (@c ModListWidget::onUpdateRequested) prompts the user to choose one via
 * @c FileSelectionModalContent before this modal is constructed. Immediately
 * starts downloading the resolved upload on @c showEvent.
 */
class ItchModUpdateModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit ItchModUpdateModalContent(const ModInfo &mod,
                                      const ItchUpdateInfo &updateInfo,
                                      ModManager *modManager,
                                      ItchClient *itchClient,
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
    ItchUpdateInfo m_updateInfo;
    ModManager *m_modManager;
    ItchClient *m_itchClient;
    ModalManager *m_modalManager;

    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_cancelButton;

    QString m_downloadedPath;
};

#endif // ITCHMODUPDATEMODALCONTENT_H
