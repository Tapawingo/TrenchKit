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
 * Navigation itself is unrestricted (a download's "new tab" often redirects through a CDN host
 * that has nothing to do with the site the browser was pointed at, so filtering by host would
 * reject real downloads before they get far enough to be recognized as one) — the toolbar's
 * "Open in Browser" button is the user's manual escape hatch. An @c nxm:// link is never
 * navigated — it is reported via @c nxmLinkRequested() instead. Files the user downloads
 * through the page are saved to a temporary directory and reported via @c fileDownloaded();
 * the caller is responsible for moving/installing them.
 *
 * All instances share one persistent @c QWebEngineProfile (cookies under
 * @c AppData/TrenchKit/browser), so a login made in one modal is still valid in the next.
 */
class BrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit BrowserWidget(QWidget *parent = nullptr);
    ~BrowserWidget() override;

    void navigate(const QUrl &url);
    QWebEngineView* view() const { return m_view; }

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
};

#endif // BROWSERWIDGET_H
