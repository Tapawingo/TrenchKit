/**
 * @file BrowserWidget.h
 * @brief Reusable embedded browser widget (toolbar + QWebEngineView) for in-app site browsing.
 */
#ifndef BROWSERWIDGET_H
#define BROWSERWIDGET_H

#include <QWidget>
#include <QUrl>

class QWebEngineView;
class QWebEngineProfile;
class QWebEngineDownloadRequest;
class QLineEdit;
class QToolButton;
class QPushButton;
class QLabel;
class QProgressBar;
class NexusWebEnginePage;

/**
 * @brief Minimal browser: back/forward/reload/address bar plus a Chromium view.
 *
 * Navigation is unrestricted (host-filtering would reject real downloads redirected through a
 * CDN). An @c nxm:// link is reported via @c nxmLinkRequested() instead of navigated. Downloads
 * are saved to a temp file and reported via @c fileDownloaded(); the caller owns them from there.
 * All instances share one persistent @c QWebEngineProfile (cookies + disk cache) under
 * @c AppData/TrenchKit/browser.
 */
class BrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit BrowserWidget(QWidget *parent = nullptr);
    ~BrowserWidget() override;

    /// @brief Pays Chromium's one-time engine-startup cost early; call once, shortly after the
    /// main window is shown. Safe to call more than once. The warm-up view is kept alive for
    /// the app's lifetime and never shown.
    static void warmUp();

    void navigate(const QUrl &url);
    QWebEngineView* view() const { return m_view; }

    /// @brief Drives the same download-progress bar for a download the caller runs itself
    /// (e.g. redeeming an nxm:// link via the API rather than the page's network stack).
    void showExternalDownload(const QString &name);
    void updateExternalDownloadProgress(qint64 received, qint64 total);
    void hideExternalDownload();

signals:
    void nxmLinkRequested(const QUrl &url);
    void downloadStarted(const QString &suggestedFileName);
    void downloadProgress(qint64 received, qint64 total);
    /// @brief A download completed; @p filePath is a temporary file the caller now owns.
    void fileDownloaded(const QString &filePath, const QString &suggestedFileName);
    void downloadFailed(const QString &reason);

private slots:
    void onDownloadRequested(QWebEngineDownloadRequest *download);
    void onUrlChanged(const QUrl &url);

private:
    static QWebEngineProfile* sharedProfile();
    QString generateTempDownloadPath(const QString &suggestedFileName) const;
    QString formatFileSize(qint64 bytes) const;
    void updateDownloadLabel(qint64 received, qint64 total);

    QWebEngineView *m_view;
    NexusWebEnginePage *m_page;
    QLineEdit *m_addressBar;
    QToolButton *m_backButton;
    QToolButton *m_forwardButton;
    QToolButton *m_reloadButton;
    QPushButton *m_externalButton;

    /// Shown only while a download triggered through the page is in progress.
    QWidget *m_downloadBar;
    QLabel *m_downloadLabel;
    QProgressBar *m_downloadProgressBar;
    QString m_downloadDisplayName; ///< Shown alongside the byte progress.
};

#endif // BROWSERWIDGET_H
