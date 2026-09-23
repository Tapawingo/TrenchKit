#include "BrowseModsWidget.h"
#include "ModInstallHelper.h"
#include "common/widgets/BrowserWidget.h"
#include "common/widgets/ToastWidget.h"
#include "common/modals/ModalManager.h"
#include "core/api/NexusModsClient.h"
#include "core/api/ItchClient.h"
#include "core/managers/ModManager.h"
#include "core/models/ItchUploadInfo.h"
#include "core/utils/ItchUrlParser.h"
#include "core/utils/NexusUrlParser.h"
#include "core/utils/Theme.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWebEngineView>

BrowseModsWidget::BrowseModsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void BrowseModsWidget::setupUi() {
    m_contentLayout = new QVBoxLayout(this);
    m_contentLayout->setContentsMargins(12, 12, 12, 12);
    m_contentLayout->setSpacing(8);

    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_backButton = new QPushButton(tr("‹ Back to Mods"), this);
    m_backButton->setCursor(Qt::PointingHandCursor);
    connect(m_backButton, &QPushButton::clicked, this, &BrowseModsWidget::backRequested);
    topBar->addWidget(m_backButton);

    topBar->addSpacing(16);

    m_nexusTabButton = new QPushButton(tr("NexusMods"), this);
    m_nexusTabButton->setCursor(Qt::PointingHandCursor);
    connect(m_nexusTabButton, &QPushButton::clicked, this, [this]() { navigateToSource(Source::Nexus); });
    topBar->addWidget(m_nexusTabButton);

    m_itchTabButton = new QPushButton(tr("itch.io"), this);
    m_itchTabButton->setCursor(Qt::PointingHandCursor);
    connect(m_itchTabButton, &QPushButton::clicked, this, [this]() { navigateToSource(Source::Itch); });
    topBar->addWidget(m_itchTabButton);

    topBar->addStretch();

    m_contentLayout->addLayout(topBar);
    updateTabStyles();
}

void BrowseModsWidget::setServices(NexusModsClient *nexusClient, ItchClient *itchClient,
                                   ModManager *modManager, ModalManager *modalManager) {
    m_nexusClient = nexusClient;
    m_itchClient = itchClient;
    m_modManager = modManager;
    m_modalManager = modalManager;

    connect(m_nexusClient, &NexusModsClient::modInfoReceived, this, &BrowseModsWidget::onNexusModInfoReceived);
    connect(m_nexusClient, &NexusModsClient::downloadLinkReceived, this, &BrowseModsWidget::onNexusDownloadLinkReceived);
    connect(m_nexusClient, &NexusModsClient::downloadProgress, this, &BrowseModsWidget::onNexusDownloadProgress);
    connect(m_nexusClient, &NexusModsClient::downloadFinished, this, &BrowseModsWidget::onNexusDownloadFinished);
    connect(m_nexusClient, &NexusModsClient::errorOccurred, this, &BrowseModsWidget::onNexusError);

    connect(m_itchClient, &ItchClient::gameIdReceived, this, &BrowseModsWidget::onItchGameIdReceived);
    connect(m_itchClient, &ItchClient::uploadsReceived, this, &BrowseModsWidget::onItchUploadsReceived);
    connect(m_itchClient, &ItchClient::errorOccurred, this, &BrowseModsWidget::onItchError);
}

void BrowseModsWidget::ensureBrowser() {
    if (m_browser) {
        return;
    }

    m_browser = new BrowserWidget(this);
    m_contentLayout->addWidget(m_browser, 1);

    connect(m_browser, &BrowserWidget::nxmLinkRequested, this, &BrowseModsWidget::onNxmLinkRequested);
    connect(m_browser, &BrowserWidget::fileDownloaded, this, &BrowseModsWidget::onFileDownloaded);
    connect(m_browser, &BrowserWidget::downloadFailed, this, &BrowseModsWidget::onDownloadFailed);
}

void BrowseModsWidget::resetToDefaultSource() {
    navigateToSource(Source::Nexus);
}

void BrowseModsWidget::navigateToSource(Source source) {
    ensureBrowser();
    m_currentSource = source;
    updateTabStyles();

    if (source == Source::Nexus) {
        m_browser->navigate(QUrl(QStringLiteral("https://www.nexusmods.com/foxhole")));
    } else {
        m_browser->navigate(QUrl(QStringLiteral("https://itch.io/search?q=foxhole")));
    }
}

void BrowseModsWidget::updateTabStyles() {
    const QString active = QString("QPushButton { background-color: %1; font-weight: bold; }")
                           .arg(Theme::Colors::ACCENT_BLUE);
    m_nexusTabButton->setStyleSheet(m_currentSource == Source::Nexus ? active : QString());
    m_itchTabButton->setStyleSheet(m_currentSource == Source::Itch ? active : QString());
}

void BrowseModsWidget::onNxmLinkRequested(const QUrl &url) {
    NexusUrlParser::NxmResult nxm = NexusUrlParser::parseNxmUrl(url.toString());
    if (!nxm.isValid) {
        ToastWidget::show(this, nxm.error, true);
        return;
    }

    m_nexusPendingKind = NexusPendingKind::NxmRedeem;
    m_pendingNxmModId = nxm.modId;
    m_pendingNxmFileId = nxm.fileId;
    m_pendingNxmKey = nxm.key;
    m_pendingNxmExpires = nxm.expires;
    m_pendingNxmUrl = QStringLiteral("https://www.nexusmods.com/foxhole/mods/%1").arg(nxm.modId);
    m_pendingNxmAuthor.clear();
    m_pendingNxmDescription.clear();
    m_pendingNxmVersion.clear();
    m_pendingNexusDirectFilePath.clear();

    m_browser->showExternalDownload(tr("mod %1").arg(nxm.modId));
    m_nexusClient->getModInfo(nxm.modId);
}

void BrowseModsWidget::onNexusModInfoReceived(const QString &author, const QString &description,
                                              const QString &version) {
    if (m_nexusPendingKind == NexusPendingKind::None) {
        return; // Not our request (e.g. another Nexus modal open at the same time).
    }
    m_pendingNxmAuthor = author;
    m_pendingNxmDescription = description;
    m_pendingNxmVersion = version;

    if (m_nexusPendingKind == NexusPendingKind::NxmRedeem) {
        m_nexusClient->getDownloadLink(m_pendingNxmModId, m_pendingNxmFileId, m_pendingNxmKey, m_pendingNxmExpires);
        return;
    }

    // DirectDownload: the file is already on disk, no download link to fetch — install now.
    ModInstallHelper::Metadata meta;
    meta.nexusModId = m_pendingNxmModId;
    meta.nexusFileId = m_pendingNxmFileId;
    meta.nexusUrl = m_pendingNxmUrl;
    meta.author = m_pendingNxmAuthor;
    meta.description = m_pendingNxmDescription;
    meta.version = m_pendingNxmVersion;

    const QString filePath = m_pendingNexusDirectFilePath;
    m_nexusPendingKind = NexusPendingKind::None;

    ModInstallHelper::installFromFile(m_modManager, m_modalManager, filePath, meta, true, this,
        [this](bool installed) {
            if (installed) {
                ToastWidget::show(this, tr("Mod installed."), false);
            }
        });
}

void BrowseModsWidget::onNexusDownloadLinkReceived(const QString &url) {
    if (m_nexusPendingKind != NexusPendingKind::NxmRedeem) {
        return;
    }

    // The CDN URL's path usually ends with the real archive filename.
    const QString cdnFileName = QFileInfo(QUrl(url).path()).fileName();
    m_browser->showExternalDownload(!cdnFileName.isEmpty() ? cdnFileName : tr("mod %1").arg(m_pendingNxmModId));

    const QString fileName = QStringLiteral("nexus_mod_%1_%2.tmp").arg(m_pendingNxmModId, m_pendingNxmFileId);
    const QString savePath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(fileName);
    m_nexusClient->downloadFile(QUrl(url), savePath);
}

void BrowseModsWidget::onNexusDownloadProgress(qint64 received, qint64 total) {
    if (m_nexusPendingKind != NexusPendingKind::NxmRedeem) {
        return;
    }
    m_browser->updateExternalDownloadProgress(received, total);
}

void BrowseModsWidget::onNexusDownloadFinished(const QString &savePath) {
    if (m_nexusPendingKind != NexusPendingKind::NxmRedeem) {
        return;
    }

    ModInstallHelper::Metadata meta;
    meta.nexusModId = m_pendingNxmModId;
    meta.nexusFileId = m_pendingNxmFileId;
    meta.nexusUrl = m_pendingNxmUrl;
    meta.author = m_pendingNxmAuthor;
    meta.description = m_pendingNxmDescription;
    meta.version = m_pendingNxmVersion;

    m_nexusPendingKind = NexusPendingKind::None;
    m_browser->hideExternalDownload();

    ModInstallHelper::installFromFile(m_modManager, m_modalManager, savePath, meta, true, this,
        [this](bool installed) {
            if (installed) {
                ToastWidget::show(this, tr("Mod installed."), false);
            }
        });
}

void BrowseModsWidget::onNexusError(const QString &error) {
    if (m_nexusPendingKind == NexusPendingKind::None) {
        return;
    }

    if (m_nexusPendingKind == NexusPendingKind::NxmRedeem) {
        m_browser->hideExternalDownload();
    }

    if (m_nexusPendingKind == NexusPendingKind::DirectDownload) {
        // Already downloaded; install rather than lose it over a metadata lookup failure.
        ModInstallHelper::Metadata meta;
        meta.nexusModId = m_pendingNxmModId;
        meta.nexusFileId = m_pendingNxmFileId;
        meta.nexusUrl = m_pendingNxmUrl;

        const QString filePath = m_pendingNexusDirectFilePath;
        m_nexusPendingKind = NexusPendingKind::None;

        ToastWidget::show(this, tr("Couldn't look up mod details (%1); installing anyway.").arg(error), true);
        ModInstallHelper::installFromFile(m_modManager, m_modalManager, filePath, meta, true, this,
            [this](bool installed) {
                if (installed) {
                    ToastWidget::show(this, tr("Mod installed."), false);
                }
            });
        return;
    }

    m_nexusPendingKind = NexusPendingKind::None;
    ToastWidget::show(this, tr("Download failed: %1").arg(error), true);
}

void BrowseModsWidget::onFileDownloaded(const QString &filePath, const QString &suggestedFileName) {
    if (m_currentSource == Source::Nexus) {
        // A plain (non-nxm://) link, e.g. an old-version file; look up metadata before installing.
        const NexusUrlParser::ParseResult parsed = NexusUrlParser::parseUrl(m_browser->view()->url().toString());
        if (!parsed.isValid || parsed.gameDomain != QStringLiteral("foxhole")) {
            ModInstallHelper::Metadata meta;
            ModInstallHelper::installFromFile(m_modManager, m_modalManager, filePath, meta, true, this,
                [this](bool installed) {
                    if (installed) {
                        ToastWidget::show(this, tr("Mod installed."), false);
                    }
                });
            return;
        }

        m_nexusPendingKind = NexusPendingKind::DirectDownload;
        m_pendingNxmModId = parsed.modId;
        m_pendingNxmFileId = parsed.fileId;
        m_pendingNxmUrl = m_browser->view()->url().toString();
        m_pendingNxmAuthor.clear();
        m_pendingNxmDescription.clear();
        m_pendingNxmVersion.clear();
        m_pendingNexusDirectFilePath = filePath;

        ToastWidget::show(this, tr("Looking up mod details..."), false);
        m_nexusClient->getModInfo(parsed.modId);
        return;
    }

    // itch.io: look up real metadata from the page URL before installing.
    const QString pageUrl = m_browser->view()->url().toString();
    const ItchUrlParser::ParseResult parsed = ItchUrlParser::parseUrl(pageUrl);
    qDebug() << "BrowseModsWidget: itch download from" << pageUrl << "parsed valid:" << parsed.isValid
             << "creator:" << parsed.creator << "game:" << parsed.gameName
             << (parsed.isValid ? QString() : parsed.error);
    if (!parsed.isValid) {
        ModInstallHelper::Metadata meta;
        ModInstallHelper::installFromFile(m_modManager, m_modalManager, filePath, meta, true, this,
            [this](bool installed) {
                if (installed) {
                    ToastWidget::show(this, tr("Mod installed."), false);
                }
            });
        return;
    }

    resetItchPending();
    m_pendingItchFilePath = filePath;
    m_pendingItchSuggestedName = suggestedFileName;
    m_pendingItchUrl = m_browser->view()->url().toString();

    ToastWidget::show(this, tr("Looking up mod details..."), false);
    m_itchClient->getGameId(parsed.creator, parsed.gameName);
}

void BrowseModsWidget::onItchGameIdReceived(const QString &gameId, const QString &, const QString &author) {
    if (m_pendingItchFilePath.isEmpty()) {
        return;
    }
    qDebug() << "BrowseModsWidget: itch getGameId ->" << gameId << "author:" << author;
    m_pendingItchGameId = gameId;
    m_pendingItchAuthor = author;
    m_itchClient->getGameUploads(gameId);
}

void BrowseModsWidget::onItchUploadsReceived(const QList<ItchUploadInfo> &uploads) {
    if (m_pendingItchFilePath.isEmpty()) {
        return;
    }

    ModInstallHelper::Metadata meta;
    meta.itchGameId = m_pendingItchGameId;
    meta.itchUrl = m_pendingItchUrl;
    meta.author = m_pendingItchAuthor;

    bool matched = false;
    for (const ItchUploadInfo &upload : uploads) {
        if (upload.filename.compare(m_pendingItchSuggestedName, Qt::CaseInsensitive) == 0) {
            meta.itchUploadId = upload.id;
            meta.customModName = !upload.displayName.isEmpty() ? upload.displayName : upload.filename;
            meta.uploadDate = upload.updatedAt.isValid() ? upload.updatedAt : upload.createdAt;
            matched = true;
            break;
        }
    }
    qDebug() << "BrowseModsWidget: itch getGameUploads ->" << uploads.size() << "upload(s), matched:" << matched
             << "author carried through:" << meta.author;

    const QString filePath = m_pendingItchFilePath;
    resetItchPending();

    ModInstallHelper::installFromFile(m_modManager, m_modalManager, filePath, meta, true, this,
        [this](bool installed) {
            if (installed) {
                ToastWidget::show(this, tr("Mod installed."), false);
            }
        });
}

void BrowseModsWidget::onItchError(const QString &error) {
    if (m_pendingItchFilePath.isEmpty()) {
        return;
    }

    // getGameId() may already have succeeded before getGameUploads() failed; keep that metadata.
    ModInstallHelper::Metadata meta;
    meta.itchGameId = m_pendingItchGameId;
    meta.itchUrl = m_pendingItchUrl;
    meta.author = m_pendingItchAuthor;

    const QString filePath = m_pendingItchFilePath;
    resetItchPending();

    ToastWidget::show(this, tr("Couldn't look up mod details (%1); installing anyway.").arg(error), true);

    ModInstallHelper::installFromFile(m_modManager, m_modalManager, filePath, meta, true, this,
        [this](bool installed) {
            if (installed) {
                ToastWidget::show(this, tr("Mod installed."), false);
            }
        });
}

void BrowseModsWidget::onDownloadFailed(const QString &reason) {
    resetItchPending();
    ToastWidget::show(this, reason, true);
}

void BrowseModsWidget::resetItchPending() {
    m_pendingItchFilePath.clear();
    m_pendingItchSuggestedName.clear();
    m_pendingItchGameId.clear();
    m_pendingItchAuthor.clear();
    m_pendingItchUrl.clear();
}
