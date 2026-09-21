#include "ModUpdateModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/managers/ModManager.h"
#include "core/api/NexusModsClient.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>

ModUpdateModalContent::ModUpdateModalContent(const ModInfo &mod,
                                             const ModUpdateInfo &updateInfo,
                                             ModManager *modManager,
                                             NexusModsClient *nexusClient,
                                             ModalManager *modalManager,
                                             QWidget *parent)
    : BaseModalContent(parent)
    , m_mod(mod)
    , m_updateInfo(updateInfo)
    , m_modManager(modManager)
    , m_nexusClient(nexusClient)
    , m_modalManager(modalManager)
{
    setTitle(tr("Updating Mod"));
    setupUi();
    setPreferredSize(QSize(400, 220));

    connect(m_nexusClient, &NexusModsClient::downloadLinkReceived,
            this, &ModUpdateModalContent::onDownloadLinkReceived);
    connect(m_nexusClient, &NexusModsClient::downloadProgress,
            this, &ModUpdateModalContent::onDownloadProgress);
    connect(m_nexusClient, &NexusModsClient::downloadFinished,
            this, &ModUpdateModalContent::onDownloadFinished);
    connect(m_nexusClient, &NexusModsClient::errorOccurred,
            this, &ModUpdateModalContent::onError);

    QTimer::singleShot(0, this, &ModUpdateModalContent::startDownload);
}

void ModUpdateModalContent::setupUi() {
    m_titleLabel = new QLabel(tr("Updating: %1").arg(m_mod.name), this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; font-weight: bold; }")
                               .arg(Theme::Colors::TEXT_SECONDARY));
    bodyLayout()->addWidget(m_titleLabel);

    m_versionLabel = new QLabel(tr("Version %1 → %2")
                                   .arg(m_updateInfo.currentVersion,
                                        m_updateInfo.availableVersion),
                               this);
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
    connect(m_cancelButton, &QPushButton::clicked, this, &ModUpdateModalContent::onCancelClicked);
    footerLayout()->addWidget(m_cancelButton);
}

void ModUpdateModalContent::startDownload() {
    if (!m_nexusClient || !m_modManager) {
        MessageModal::critical(m_modalManager, tr("Error"), tr("Missing required services"));
        reject();
        return;
    }

    m_statusLabel->setText(tr("Getting download link..."));
    m_nexusClient->getDownloadLink(m_mod.nexusModId, m_updateInfo.availableFileId);
}

void ModUpdateModalContent::onDownloadLinkReceived(const QString &url) {
    QString fileName = QString("update_%1_%2.tmp")
                          .arg(m_mod.nexusModId, m_updateInfo.availableFileId);
    QString savePath = generateTempPath(fileName);

    m_statusLabel->setText(tr("Downloading..."));
    m_nexusClient->downloadFile(QUrl(url), savePath);
}

void ModUpdateModalContent::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int percentage = static_cast<int>((received * 100) / total);
        m_progressBar->setValue(percentage);

        qint64 receivedMB = received / (1024 * 1024);
        qint64 totalMB = total / (1024 * 1024);
        m_statusLabel->setText(tr("Downloading: %1 MB / %2 MB")
                                  .arg(receivedMB).arg(totalMB));
    }
}

void ModUpdateModalContent::onDownloadFinished(const QString &savePath) {
    m_downloadedPath = savePath;
    m_statusLabel->setText(tr("Installing update..."));
    m_progressBar->setValue(100);

    if (!m_modManager->replaceModFromFile(m_mod.id, savePath,
                                  m_updateInfo.availableVersion,
                                  m_updateInfo.availableFileId)) {
        QFile::remove(savePath);
        MessageModal::critical(m_modalManager, tr("Error"), tr("Failed to install mod update"));
        reject();
        return;
    }

    QFile::remove(savePath);

    m_statusLabel->setText(tr("Update complete!"));
    MessageModal::information(m_modalManager, tr("Success"),
                             tr("Mod updated successfully to version %1")
                                 .arg(m_updateInfo.availableVersion));
    accept();
}

void ModUpdateModalContent::onError(const QString &error) {
    m_statusLabel->setText(tr("Error: %1").arg(error));

    if (error == "PREMIUM_REQUIRED") {
        QString modUrl = QString("https://www.nexusmods.com/foxhole/mods/%1?tab=files&file_id=%2")
                            .arg(m_mod.nexusModId, m_updateInfo.availableFileId);

        auto *modal = new MessageModal(
            tr("Premium Required"),
            tr("Direct updates via API require a Nexus Mods Premium account.\n\n"
            "Your browser will open to the download page where you can download the file manually.\n\n"
            "The update will be installed with existing metadata preserved."),
            MessageModal::Information,
            MessageModal::Ok | MessageModal::Cancel
        );

        connect(modal, &MessageModal::finished, this, [this, modUrl, modal]() {
            if (modal->clickedButton() == MessageModal::Ok) {
                QDesktopServices::openUrl(QUrl(modUrl));

                auto *selectModal = new MessageModal(
                    tr("Select Downloaded File"),
                    tr("Please download the file from your browser.\n\n"
                    "Once the download is complete, click OK to locate the file."),
                    MessageModal::Information,
                    MessageModal::Ok | MessageModal::Cancel
                );

                connect(selectModal, &MessageModal::finished, this, [this, selectModal]() {
                    if (selectModal->clickedButton() == MessageModal::Ok) {
                        QString filePath = QFileDialog::getOpenFileName(
                            this,
                            tr("Select Downloaded File"),
                            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                            tr("Mod Files (*.pak *.zip *.rar *.7z *.tar.gz *.tar.bz2 *.tar.xz);;"
                            "Pak Files (*.pak);;"
                            "Archive Files (*.zip *.rar *.7z *.tar.gz *.tar.bz2 *.tar.xz);;"
                            "All Files (*.*)")
                        );

                        if (!filePath.isEmpty()) {
                            m_downloadedPath = filePath;
                            m_statusLabel->setText(tr("Installing update..."));
                            m_progressBar->setValue(100);

                            if (!m_modManager->replaceModFromFile(m_mod.id, filePath,
                                                          m_updateInfo.availableVersion,
                                                          m_updateInfo.availableFileId)) {
                                MessageModal::critical(m_modalManager, tr("Error"), tr("Failed to install mod update"));
                                reject();
                                return;
                            }

                            m_statusLabel->setText(tr("Update complete!"));
                            MessageModal::information(m_modalManager, tr("Success"),
                                                     tr("Mod updated successfully to version %1")
                                                         .arg(m_updateInfo.availableVersion));
                            accept();
                        } else {
                            reject();
                        }
                    } else {
                        reject();
                    }
                });

                m_modalManager->showModal(selectModal);
            } else {
                reject();
            }
        });

        m_modalManager->showModal(modal);
    } else {
        MessageModal::critical(m_modalManager, tr("Error"), error);
        reject();
    }
}

void ModUpdateModalContent::onCancelClicked() {
    m_nexusClient->cancelDownload();
    reject();
}

QString ModUpdateModalContent::generateTempPath(const QString &fileName) const {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(tempDir).filePath(fileName);
}
