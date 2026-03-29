/**
 * @file NexusDownloadModalContent.h
 * @brief Modal for downloading mods from NexusMods URLs.
 */
#ifndef NEXUSDOWNLOADMODALCONTENT_H
#define NEXUSDOWNLOADMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/NexusFileInfo.h"
#include <QString>
#include <QList>

class QEvent;
class NexusModsClient;
class NexusModsAuth;
class ModalManager;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QStackedWidget;

/**
 * @brief Aggregated result for one downloaded NexusMods file.
 */
struct NexusDownloadResult {
    QString filePath;   ///< Local path of the downloaded pak file.
    QString modId;      ///< NexusMods mod ID.
    QString url;        ///< Source NexusMods URL.
    NexusFileInfo fileInfo;
    QString author;
    QString description;
};

/**
 * @brief Three-page modal (Input → Auth → Download) for NexusMods mod downloads.
 *
 * Accepts one or more NexusMods mod/file URLs, optionally runs the SSO auth
 * flow if no API key is stored, then downloads and saves the pak files.
 * Call the result getters after @c Accepted to retrieve downloaded file paths
 * and metadata.
 */
class NexusDownloadModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit NexusDownloadModalContent(NexusModsClient *client,
                                      NexusModsAuth *auth,
                                      ModalManager *modalManager,
                                      QWidget *parent = nullptr);

    QStringList getDownloadedFilePaths() const { return m_downloadedFilePaths; }
    QList<NexusFileInfo> getDownloadedFiles() const { return m_downloadedFiles; }
    QString getModId() const { return m_currentModId; }
    QString getAuthor() const { return m_author; }
    QString getDescription() const { return m_description; }
    QList<NexusDownloadResult> getDownloadResults() const { return m_results; }

private slots:
    void onDownloadClicked();
    void onAuthenticateClicked();
    void onAuthStarted(const QString &browserUrl);
    void onAuthComplete(const QString &apiKey);
    void onAuthFailed(const QString &error);
    void onModInfoReceived(const QString &author, const QString &description, const QString &version);
    void onModFilesReceived(const QList<NexusFileInfo> &files);
    void onDownloadLinkReceived(const QString &url);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString &savePath);
    void onError(const QString &error);

protected:
    void changeEvent(QEvent *event) override;

private:
    /**
     * @brief One queued mod/file URL awaiting download.
     */
    struct PendingMod {
        QString modId;
        QString fileId;
        QString url;
    };

    void setupUi();
    void retranslateUi();
    QWidget* createInputPage();
    QWidget* createAuthPage();
    QWidget* createDownloadPage();
    void showInputPage();
    void showAuthPage();
    void showDownloadPage();
    void startDownloadProcess();
    void startNextDownload();
    void startNextMod();
    void startManualDownloadSequence();
    QString generateTempPath(const QString &fileName) const;
    void updateFooterButtons();
    QString formatFileSize(qint64 bytes) const;

    NexusModsClient *m_client;
    NexusModsAuth *m_auth;
    ModalManager *m_modalManager;

    QStackedWidget *m_stack;
    QPlainTextEdit *m_urlEdit;
    QPushButton *m_downloadButton;
    QPushButton *m_authenticateButton;
    QPushButton *m_cancelButton;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QLabel *m_authStatusLabel;

    QLabel *m_inputInstructionLabel;
    QLabel *m_authInstructionLabel;

    QStringList m_selectedFileIds;
    QList<NexusFileInfo> m_selectedFiles;
    QStringList m_downloadedFilePaths;
    QList<NexusFileInfo> m_downloadedFiles;
    int m_currentDownloadIndex;

    QList<PendingMod> m_pendingMods;
    int m_currentModIndex = 0;
    QList<NexusDownloadResult> m_results;

    QString m_currentModId;
    QString m_currentFileId;
    QString m_pendingUrl;
    QString m_author;
    QString m_description;

    enum Page { InputPage, AuthPage, DownloadPage };
};

#endif // NEXUSDOWNLOADMODALCONTENT_H
