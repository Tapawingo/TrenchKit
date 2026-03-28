#include "ItchModUpdateModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/managers/ModManager.h"
#include "core/api/ItchClient.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

ItchModUpdateModalContent::ItchModUpdateModalContent(const ModInfo &mod,
                                                     const ItchUpdateInfo &updateInfo,
                                                     ModManager *modManager,
                                                     ItchClient *itchClient,
                                                     ModalManager *modalManager,
                                                     QWidget *parent)
    : BaseModalContent(parent)
    , m_mod(mod)
    , m_updateInfo(updateInfo)
    , m_modManager(modManager)
    , m_itchClient(itchClient)
    , m_modalManager(modalManager)
{
    setTitle(tr("Update Mod"));
    setupUi();
    setPreferredSize(QSize(450, 220));

    connect(m_itchClient, &ItchClient::downloadLinkReceived,
            this, &ItchModUpdateModalContent::onDownloadLinkReceived);
    connect(m_itchClient, &ItchClient::downloadProgress,
            this, &ItchModUpdateModalContent::onDownloadProgress);
    connect(m_itchClient, &ItchClient::downloadFinished,
            this, &ItchModUpdateModalContent::onDownloadFinished);
    connect(m_itchClient, &ItchClient::errorOccurred,
            this, &ItchModUpdateModalContent::onError);

    startDownload();
}

void ItchModUpdateModalContent::setupUi() {
    m_titleLabel = new QLabel(tr("Updating: %1").arg(m_mod.name), this);
    m_titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; font-weight: bold; }")
                               .arg(Theme::Colors::TEXT_SECONDARY));
    bodyLayout()->addWidget(m_titleLabel);

    QString versionText = tr("%1 → %2")
        .arg(m_updateInfo.currentVersion)
        .arg(m_updateInfo.availableVersion);
    m_versionLabel = new QLabel(versionText, this);
    m_versionLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }")
                                 .arg(Theme::Colors::TEXT_SECONDARY));
    bodyLayout()->addWidget(m_versionLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    bodyLayout()->addWidget(m_progressBar);

    m_statusLabel = new QLabel(tr("Preparing download..."), this);
    m_statusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }")
                                .arg(Theme::Colors::TEXT_MUTED));
    bodyLayout()->addWidget(m_statusLabel);

    bodyLayout()->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    connect(m_cancelButton, &QPushButton::clicked, this, &ItchModUpdateModalContent::onCancelClicked);
    footerLayout()->addWidget(m_cancelButton);
}

void ItchModUpdateModalContent::startDownload() {
    m_statusLabel->setText(tr("Getting download link..."));
    m_itchClient->getDownloadLink(m_updateInfo.availableUploadId);
}

void ItchModUpdateModalContent::onDownloadLinkReceived(const QString &url) {
    if (url.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("Failed to get download link."));
        reject();
        return;
    }

    QString fileName = QString("update_%1").arg(m_mod.fileName);
    QString tempPath = generateTempPath(fileName);

    m_statusLabel->setText(tr("Downloading..."));
    m_itchClient->downloadFile(QUrl(url), tempPath);
}

void ItchModUpdateModalContent::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int percentage = static_cast<int>((received * 100) / total);
        m_progressBar->setValue(percentage);

        QString status = tr("Downloading... %1 MB / %2 MB")
            .arg(received / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(total / (1024.0 * 1024.0), 0, 'f', 2);
        m_statusLabel->setText(status);
    } else {
        m_progressBar->setRange(0, 0);
        m_statusLabel->setText(tr("Downloading..."));
    }
}

void ItchModUpdateModalContent::onDownloadFinished(const QString &savePath) {
    m_downloadedPath = savePath;
    m_statusLabel->setText(tr("Installing update..."));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);

    if (!m_modManager->replaceMod(m_mod.id, savePath, m_updateInfo.availableVersion,
                                  m_updateInfo.availableUploadId, m_updateInfo.availableUploadDate)) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("Failed to install update."));
        reject();
        return;
    }

    ModInfo updated = m_modManager->getMod(m_mod.id);
    updated.itchUploadId = m_updateInfo.availableUploadId;
    m_modManager->updateModMetadata(updated);

    MessageModal::information(m_modalManager, tr("Success"), tr("Mod updated successfully!"));
    accept();
}

void ItchModUpdateModalContent::onError(const QString &error) {
    MessageModal::warning(m_modalManager, tr("Error"), error);
    reject();
}

void ItchModUpdateModalContent::onCancelClicked() {
    m_itchClient->cancelDownload();
    reject();
}

QString ItchModUpdateModalContent::generateTempPath(const QString &fileName) const {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString uniqueName = QString("itch_update_%1_%2")
        .arg(m_mod.itchGameId)
        .arg(fileName);

    return QDir(tempDir).filePath(uniqueName);
}
