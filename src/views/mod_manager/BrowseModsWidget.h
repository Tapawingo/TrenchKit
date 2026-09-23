/**
 * @file BrowseModsWidget.h
 * @brief Full-page view for browsing and installing mods from NexusMods or itch.io.
 */
#ifndef BROWSEMODSWIDGET_H
#define BROWSEMODSWIDGET_H

#include <QWidget>
#include <QUrl>
#include <QList>

class BrowserWidget;
class NexusModsClient;
class ItchClient;
class ModManager;
class ModalManager;
class QPushButton;
class QLabel;
class QVBoxLayout;
struct ItchUploadInfo;

/// @brief Full page (swapped into @c MainWindow::ui->bodyStack, like Settings) for browsing and
/// installing mods from NexusMods or itch.io's own pages without leaving TrenchKit.
class BrowseModsWidget : public QWidget {
    Q_OBJECT

public:
    explicit BrowseModsWidget(QWidget *parent = nullptr);

    void setServices(NexusModsClient *nexusClient, ItchClient *itchClient,
                     ModManager *modManager, ModalManager *modalManager);

    /// @brief Call each time the page is shown, so browsing always starts fresh at the front page.
    void resetToDefaultSource();

signals:
    void backRequested();

private slots:
    void onNxmLinkRequested(const QUrl &url);
    void onFileDownloaded(const QString &filePath, const QString &suggestedFileName);
    void onDownloadFailed(const QString &reason);

    void onNexusModInfoReceived(const QString &author, const QString &description, const QString &version);
    void onNexusDownloadLinkReceived(const QString &url);
    void onNexusDownloadProgress(qint64 received, qint64 total);
    void onNexusDownloadFinished(const QString &savePath);
    void onNexusError(const QString &error);

    void onItchGameIdReceived(const QString &gameId, const QString &gameTitle, const QString &author);
    void onItchUploadsReceived(const QList<ItchUploadInfo> &uploads);
    void onItchError(const QString &error);

private:
    enum class Source { Nexus, Itch };

    void setupUi();
    void ensureBrowser();
    void navigateToSource(Source source);
    void updateTabStyles();
    void resetItchPending();

    NexusModsClient *m_nexusClient = nullptr;
    ItchClient *m_itchClient = nullptr;
    ModManager *m_modManager = nullptr;
    ModalManager *m_modalManager = nullptr;

    QVBoxLayout *m_contentLayout = nullptr;
    BrowserWidget *m_browser = nullptr;
    QPushButton *m_backButton;
    QPushButton *m_nexusTabButton;
    QPushButton *m_itchTabButton;

    Source m_currentSource = Source::Nexus;

    /// What the in-flight NexusModsClient::getModInfo() call, if any, is for.
    enum class NexusPendingKind { None, NxmRedeem, DirectDownload };
    NexusPendingKind m_nexusPendingKind = NexusPendingKind::None;

    // In-flight NexusModsClient context; empty modId means nothing pending.
    QString m_pendingNxmModId;
    QString m_pendingNxmFileId;
    QString m_pendingNxmKey;
    QString m_pendingNxmExpires;
    QString m_pendingNxmUrl;
    QString m_pendingNxmAuthor;
    QString m_pendingNxmDescription;
    QString m_pendingNxmVersion;
    QString m_pendingNexusDirectFilePath; ///< DirectDownload only: file already captured.

    // In-flight itch.io metadata lookup for a captured direct download.
    QString m_pendingItchFilePath;
    QString m_pendingItchSuggestedName;
    QString m_pendingItchGameId;
    QString m_pendingItchAuthor;
    QString m_pendingItchUrl;
};

#endif // BROWSEMODSWIDGET_H
