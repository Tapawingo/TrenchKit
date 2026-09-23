#include "BrowserWidget.h"
#include "core/utils/Theme.h"
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

/**
 * @brief Page that intercepts nxm:// navigation; everything else just navigates normally.
 */
class NexusWebEnginePage : public QWebEnginePage {
    Q_OBJECT

public:
    explicit NexusWebEnginePage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent)
    {}

signals:
    void nxmLinkRequested(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType, bool) override {
        if (url.scheme() == QStringLiteral("nxm")) {
            emit nxmLinkRequested(url);
            return false;
        }

        // Downloads often redirect through a CDN host first; rejecting by host would reject the
        // download too. downloadRequested() catches real downloads regardless of host instead.
        return true;
    }

    // Keep popups in the same view; this widget has no chrome to host a separate window.
    QWebEnginePage *createWindow(QWebEnginePage::WebWindowType) override {
        return this;
    }
};

QWebEngineProfile *BrowserWidget::sharedProfile() {
    static QWebEngineProfile *profile = nullptr;
    if (!profile) {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        profile = new QWebEngineProfile(QStringLiteral("nexus"), qApp);
        profile->setPersistentStoragePath(base + QStringLiteral("/browser"));
        profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

        profile->setCachePath(base + QStringLiteral("/browser/cache"));
        profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        profile->setHttpCacheMaximumSize(200 * 1024 * 1024); // 200 MB
    }
    return profile;
}

void BrowserWidget::warmUp() {
    static QWebEngineView *warmupView = nullptr;
    if (warmupView) {
        return;
    }
    warmupView = new QWebEngineView();
    warmupView->setPage(new QWebEnginePage(sharedProfile(), warmupView));
    warmupView->load(QUrl(QStringLiteral("about:blank")));
    // Never parented or shown: it only exists to pay Chromium's one-time engine-startup cost.
}

BrowserWidget::BrowserWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);

    // Every toolbar widget matches the address bar's height so the row reads as one piece.
    constexpr int TOOLBAR_HEIGHT = 28;

    // Override the app-wide QToolButton padding (meant for the big Launch button; would clip these).
    const QString navButtonStyle = QString("QToolButton { padding: 0px; }");

    m_backButton = new QToolButton(this);
    // Text glyph, not setArrowType(): the native arrow renders too large at this button size.
    m_backButton->setText(QStringLiteral("←"));
    m_backButton->setToolTip(tr("Back"));
    m_backButton->setFixedSize(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT);
    m_backButton->setStyleSheet(navButtonStyle);
    toolbar->addWidget(m_backButton);

    m_forwardButton = new QToolButton(this);
    m_forwardButton->setText(QStringLiteral("→"));
    m_forwardButton->setToolTip(tr("Forward"));
    m_forwardButton->setFixedSize(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT);
    m_forwardButton->setStyleSheet(navButtonStyle);
    toolbar->addWidget(m_forwardButton);

    m_reloadButton = new QToolButton(this);
    m_reloadButton->setText(QStringLiteral("⟳"));
    m_reloadButton->setToolTip(tr("Reload"));
    m_reloadButton->setFixedSize(TOOLBAR_HEIGHT, TOOLBAR_HEIGHT);
    m_reloadButton->setStyleSheet(navButtonStyle);
    toolbar->addWidget(m_reloadButton);

    m_addressBar = new QLineEdit(this);
    m_addressBar->setReadOnly(true);
    m_addressBar->setFixedHeight(TOOLBAR_HEIGHT);
    m_addressBar->setStyleSheet(QString("QLineEdit { background-color: %1; color: %2; border: 1px solid %3; "
                                        "border-radius: 4px; padding: 0 8px; }")
                                     .arg(Theme::Colors::BACKGROUND_TERTIARY,
                                          Theme::Colors::TEXT_SECONDARY,
                                          Theme::Colors::BORDER_PRIMARY));
    toolbar->addWidget(m_addressBar, 1);

    m_externalButton = new QPushButton(tr("Open in Browser"), this);
    m_externalButton->setFixedHeight(TOOLBAR_HEIGHT);
    m_externalButton->setStyleSheet(QStringLiteral("QPushButton { padding: 0px 12px; }"));
    toolbar->addWidget(m_externalButton);

    layout->addLayout(toolbar);

    m_downloadBar = new QWidget(this);
    auto *downloadLayout = new QHBoxLayout(m_downloadBar);
    downloadLayout->setContentsMargins(0, 0, 0, 0);
    downloadLayout->setSpacing(8);

    m_downloadLabel = new QLabel(m_downloadBar);
    m_downloadLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }")
                                   .arg(Theme::Colors::TEXT_SECONDARY));
    m_downloadLabel->setMaximumWidth(320); // cap so a long name can't squeeze the bar to a sliver
    downloadLayout->addWidget(m_downloadLabel);

    m_downloadProgressBar = new QProgressBar(m_downloadBar);
    m_downloadProgressBar->setRange(0, 0); // indeterminate until the first progress signal
    m_downloadProgressBar->setFixedHeight(TOOLBAR_HEIGHT);
    m_downloadProgressBar->setMinimumWidth(80);
    downloadLayout->addWidget(m_downloadProgressBar, 1);

    m_downloadBar->setVisible(false);
    layout->addWidget(m_downloadBar);

    m_view = new QWebEngineView(this);
    m_page = new NexusWebEnginePage(sharedProfile(), m_view);
    m_view->setPage(m_page);
    m_view->setZoomFactor(0.5);
    layout->addWidget(m_view, 1);

    connect(m_backButton, &QToolButton::clicked, m_view, &QWebEngineView::back);
    connect(m_forwardButton, &QToolButton::clicked, m_view, &QWebEngineView::forward);
    connect(m_reloadButton, &QToolButton::clicked, m_view, &QWebEngineView::reload);
    connect(m_externalButton, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(m_view->url());
    });
    connect(m_view, &QWebEngineView::urlChanged, this, &BrowserWidget::onUrlChanged);
    connect(m_page, &NexusWebEnginePage::nxmLinkRequested, this, &BrowserWidget::nxmLinkRequested);
    connect(sharedProfile(), &QWebEngineProfile::downloadRequested, this, &BrowserWidget::onDownloadRequested);

    connect(this, &BrowserWidget::downloadStarted, this, [this](const QString &suggestedFileName) {
        showExternalDownload(suggestedFileName);
    });
    connect(this, &BrowserWidget::downloadProgress, this, &BrowserWidget::updateDownloadLabel);
    connect(this, &BrowserWidget::fileDownloaded, this, [this](const QString &, const QString &) {
        hideExternalDownload();
    });
    connect(this, &BrowserWidget::downloadFailed, this, [this](const QString &) {
        hideExternalDownload();
    });
}

void BrowserWidget::showExternalDownload(const QString &name) {
    m_downloadDisplayName = name;
    m_downloadProgressBar->setRange(0, 0);
    m_downloadProgressBar->setValue(0);
    updateDownloadLabel(0, 0);
    m_downloadBar->setVisible(true);
}

void BrowserWidget::updateExternalDownloadProgress(qint64 received, qint64 total) {
    updateDownloadLabel(received, total);
}

void BrowserWidget::hideExternalDownload() {
    m_downloadBar->setVisible(false);
}

void BrowserWidget::updateDownloadLabel(qint64 received, qint64 total) {
    QString text;
    if (total > 0) {
        m_downloadProgressBar->setRange(0, 100);
        m_downloadProgressBar->setValue(static_cast<int>((received * 100) / total));
        text = tr("Downloading %1: %2 / %3")
              .arg(m_downloadDisplayName, formatFileSize(received), formatFileSize(total));
    } else if (received > 0) {
        text = tr("Downloading %1: %2").arg(m_downloadDisplayName, formatFileSize(received));
    } else {
        text = tr("Downloading %1...").arg(m_downloadDisplayName);
    }

    const QFontMetrics metrics(m_downloadLabel->font());
    m_downloadLabel->setText(metrics.elidedText(text, Qt::ElideMiddle, m_downloadLabel->maximumWidth()));
    m_downloadLabel->setToolTip(text);
}

BrowserWidget::~BrowserWidget() {
    sharedProfile()->disconnect(this);
}

void BrowserWidget::navigate(const QUrl &url) {
    m_view->setUrl(url);
}

void BrowserWidget::onUrlChanged(const QUrl &url) {
    m_addressBar->setText(url.toString());
}

void BrowserWidget::onDownloadRequested(QWebEngineDownloadRequest *download) {
    if (download->page() != m_page) {
        return;
    }

    // Fall back to the URL's filename, then a placeholder, if the site gave no suggested name.
    QString suggested = download->downloadFileName();
    if (suggested.isEmpty()) {
        suggested = QFileInfo(download->url().path()).fileName();
    }
    if (suggested.isEmpty()) {
        suggested = QStringLiteral("download");
    }
    qDebug() << "BrowserWidget: download requested, url:" << download->url()
             << "suggested name:" << suggested;
    const QString path = generateTempDownloadPath(suggested);
    const QFileInfo info(path);

    download->setDownloadDirectory(info.absolutePath());
    download->setDownloadFileName(info.fileName());

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this, download]() {
        emit downloadProgress(download->receivedBytes(), download->totalBytes());
    });

    connect(download, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, download]() {
        if (!download->isFinished()) {
            return;
        }

        const QString filePath = QDir(download->downloadDirectory()).filePath(download->downloadFileName());
        if (download->state() == QWebEngineDownloadRequest::DownloadCompleted) {
            emit fileDownloaded(filePath, download->downloadFileName());
        } else {
            emit downloadFailed(tr("The download was interrupted or failed."));
        }
        download->deleteLater();
    });

    emit downloadStarted(suggested);
    download->accept();
}

QString BrowserWidget::generateTempDownloadPath(const QString &suggestedFileName) const {
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dir = QDir(tempRoot).filePath(QStringLiteral("TrenchKit_download_%1")
                                                     .arg(QUuid::createUuid().toString(QUuid::Id128)));
    QDir().mkpath(dir);
    return QDir(dir).filePath(suggestedFileName);
}

QString BrowserWidget::formatFileSize(qint64 bytes) const {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return tr("%1 GB").arg(bytes / static_cast<double>(GB), 0, 'f', 2);
    } else if (bytes >= MB) {
        return tr("%1 MB").arg(bytes / static_cast<double>(MB), 0, 'f', 2);
    } else if (bytes >= KB) {
        return tr("%1 KB").arg(bytes / static_cast<double>(KB), 0, 'f', 2);
    }
    return tr("%1 bytes").arg(bytes);
}

#include "BrowserWidget.moc"
