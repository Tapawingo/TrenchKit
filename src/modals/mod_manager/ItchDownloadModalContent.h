/**
 * @file ItchDownloadModalContent.h
 * @brief Modal for downloading mods from itch.io URLs.
 */
#ifndef ITCHDOWNLOADMODALCONTENT_H
#define ITCHDOWNLOADMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ItchUploadInfo.h"
#include <QString>
#include <QStringList>
#include <QList>

class QEvent;
class ItchClient;
class ItchAuth;
class ModalManager;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QStackedWidget;

/**
 * @brief Aggregated result for one downloaded itch.io upload.
 */
struct ItchDownloadResult {
    QString filePath;   ///< Local path of the downloaded pak file.
    QString gameId;     ///< Numeric itch.io game ID.
    QString url;        ///< Source itch.io game page URL.
    QString gameTitle;
    QString author;
    ItchUploadInfo uploadInfo;
};

/**
 * @brief Three-page modal (Input → Auth → Download) for itch.io mod downloads.
 *
 * Accepts one or more itch.io game page URLs. If no API key is stored, shows
 * an API key entry page (@c AuthPage). Downloads the selected upload(s) and
 * saves the pak files. Call the result getters after @c Accepted.
 */
class ItchDownloadModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit ItchDownloadModalContent(ItchClient *client,
                                     ItchAuth *auth,
                                     ModalManager *modalManager,
                                     QWidget *parent = nullptr);

    QString getDownloadedFilePath() const { return m_downloadedPath; }
    QStringList getDownloadedFilePaths() const { return m_downloadedPaths; }
    QString getGameId() const { return m_currentGameId; }
    QString getGameTitle() const { return m_gameTitle; }
    QString getAuthor() const { return m_author; }
    QList<ItchUploadInfo> getDownloadedUploads() const { return m_pendingUploads; }
    QList<ItchDownloadResult> getDownloadResults() const { return m_results; }

signals:
    void downloadComplete(QString filePath);

private slots:
    void onDownloadClicked();
    void onApiKeySubmit();
    void onGameIdReceived(const QString &gameId, const QString &title, const QString &author);
    void onUploadsReceived(const QList<ItchUploadInfo> &uploads);
    void onDownloadLinkReceived(const QString &url);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString &savePath);
    void onError(const QString &error);

protected:
    void changeEvent(QEvent *event) override;

private:
    /**
     * @brief One queued game page URL awaiting download.
     */
    struct PendingGame {
        QString creator;
        QString gameName;
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
    void startNextGame();
    QString generateTempPath(const QString &fileName) const;
    void updateFooterButtons();
    QString formatFileSize(qint64 bytes) const;

    ItchClient *m_client;
    ItchAuth *m_auth;
    ModalManager *m_modalManager;

    QStackedWidget *m_stack;
    QPlainTextEdit *m_urlEdit;
    QLineEdit *m_apiKeyEdit;
    QPushButton *m_downloadButton;
    QPushButton *m_submitApiKeyButton;
    QPushButton *m_cancelButton;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QLabel *m_inputInstructionLabel;
    QLabel *m_authTitleLabel;
    QLabel *m_authInstructionLabel;

    QString m_downloadedPath;
    QStringList m_downloadedPaths;
    QString m_currentGameId;
    QString m_currentUploadId;
    QString m_gameTitle;
    QString m_author;
    QString m_pendingCreator;
    QString m_pendingGameName;

    QList<PendingGame> m_pendingGames;
    int m_currentGameIndex = 0;
    QList<ItchDownloadResult> m_results;

    QList<ItchUploadInfo> m_pendingUploads;
    int m_currentDownloadIndex = 0;

    enum Page { InputPage, AuthPage, DownloadPage };
};

#endif // ITCHDOWNLOADMODALCONTENT_H
