#include "NexusDownloadModalContent.h"
#include "FileSelectionModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/api/NexusModsClient.h"
#include "core/api/NexusModsAuth.h"
#include "core/utils/NexusUrlParser.h"
#include "core/utils/Theme.h"
#include <algorithm>
#include <QEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

NexusDownloadModalContent::NexusDownloadModalContent(NexusModsClient *client,
                                                     NexusModsAuth *auth,
                                                     ModalManager *modalManager,
                                                     QWidget *parent)
    : BaseModalContent(parent)
    , m_client(client)
    , m_auth(auth)
    , m_modalManager(modalManager)
    , m_currentDownloadIndex(0)
{
    setTitle(tr("Download from Nexus Mods"));
    setupUi();
    setPreferredSize(QSize(500, 400));

    connect(m_client, &NexusModsClient::modInfoReceived, this, &NexusDownloadModalContent::onModInfoReceived);
    connect(m_client, &NexusModsClient::modFilesReceived, this, &NexusDownloadModalContent::onModFilesReceived);
    connect(m_client, &NexusModsClient::downloadLinkReceived, this, &NexusDownloadModalContent::onDownloadLinkReceived);
    connect(m_client, &NexusModsClient::downloadProgress, this, &NexusDownloadModalContent::onDownloadProgress);
    connect(m_client, &NexusModsClient::downloadFinished, this, &NexusDownloadModalContent::onDownloadFinished);
    connect(m_client, &NexusModsClient::errorOccurred, this, &NexusDownloadModalContent::onError);

    connect(m_auth, &NexusModsAuth::authenticationStarted, this, &NexusDownloadModalContent::onAuthStarted, Qt::UniqueConnection);
    connect(m_auth, &NexusModsAuth::authenticationComplete, this, &NexusDownloadModalContent::onAuthComplete, Qt::UniqueConnection);
    connect(m_auth, &NexusModsAuth::authenticationFailed, this, &NexusDownloadModalContent::onAuthFailed, Qt::UniqueConnection);
}

void NexusDownloadModalContent::setupUi() {
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createInputPage());
    m_stack->addWidget(createAuthPage());
    m_stack->addWidget(createDownloadPage());

    bodyLayout()->addWidget(m_stack);

    m_downloadButton = new QPushButton(tr("Download"), this);
    m_downloadButton->setDefault(true);
    connect(m_downloadButton, &QPushButton::clicked, this, &NexusDownloadModalContent::onDownloadClicked);
    footerLayout()->addWidget(m_downloadButton);

    m_authenticateButton = new QPushButton(tr("Authenticate"), this);
    m_authenticateButton->setDefault(true);
    connect(m_authenticateButton, &QPushButton::clicked, this, &NexusDownloadModalContent::onAuthenticateClicked);
    footerLayout()->addWidget(m_authenticateButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setAutoDefault(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &NexusDownloadModalContent::reject);
    footerLayout()->addWidget(m_cancelButton);

    showInputPage();
}

QWidget* NexusDownloadModalContent::createInputPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    m_inputInstructionLabel = new QLabel(tr("Enter one or more Nexus Mods URLs for Foxhole mods:"), page);
    m_inputInstructionLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                                   .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(m_inputInstructionLabel);

    m_urlEdit = new QPlainTextEdit(page);
    m_urlEdit->setPlaceholderText(tr("Enter one or more Nexus Mods URLs (one per line)..."));
    m_urlEdit->setFixedHeight(80);
    layout->addWidget(m_urlEdit);

    layout->addStretch();

    return page;
}

QWidget* NexusDownloadModalContent::createAuthPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    m_authInstructionLabel = new QLabel(tr("Authentication required to access Nexus Mods API."), page);
    m_authInstructionLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                                   .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(m_authInstructionLabel);

    m_authStatusLabel = new QLabel(tr("Click Authenticate to begin..."), page);
    m_authStatusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }")
                                    .arg(Theme::Colors::TEXT_MUTED));
    m_authStatusLabel->setWordWrap(true);
    layout->addWidget(m_authStatusLabel);

    layout->addStretch();

    return page;
}

QWidget* NexusDownloadModalContent::createDownloadPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    m_statusLabel = new QLabel(tr("Preparing download..."), page);
    m_statusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                                .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(page);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);

    layout->addStretch();

    return page;
}

void NexusDownloadModalContent::showInputPage() {
    m_stack->setCurrentIndex(InputPage);
    updateFooterButtons();
}

void NexusDownloadModalContent::showAuthPage() {
    m_authStatusLabel->setText(tr("Click Authenticate to begin..."));
    m_authenticateButton->setEnabled(true);
    m_stack->setCurrentIndex(AuthPage);
    updateFooterButtons();
}

void NexusDownloadModalContent::showDownloadPage() {
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Preparing download..."));
    m_stack->setCurrentIndex(DownloadPage);
    updateFooterButtons();
}

void NexusDownloadModalContent::updateFooterButtons() {
    int currentPage = m_stack->currentIndex();

    m_downloadButton->setVisible(currentPage == InputPage);
    m_authenticateButton->setVisible(currentPage == AuthPage);
    m_cancelButton->setVisible(true);
}

void NexusDownloadModalContent::onDownloadClicked() {
    QString text = m_urlEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("Please enter one or more URLs"));
        return;
    }

    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList errors;
    m_pendingMods.clear();

    for (const QString &line : lines) {
        QString url = line.trimmed();
        if (url.isEmpty()) {
            continue;
        }

        auto result = NexusUrlParser::parseUrl(url);
        if (!result.isValid) {
            errors.append(QString("%1: %2").arg(url, result.error));
        } else {
            QString canonicalUrl = QStringLiteral("https://www.nexusmods.com/foxhole/mods/%1").arg(result.modId);
            m_pendingMods.append({result.modId, result.fileId, canonicalUrl});
        }
    }

    if (m_pendingMods.isEmpty()) {
        QString errorMsg = errors.isEmpty()
            ? tr("No valid URLs entered")
            : tr("No valid URLs found:") + QStringLiteral("\n\n") + errors.join('\n');
        MessageModal::warning(m_modalManager, tr("Invalid URLs"), errorMsg);
        return;
    }

    if (!errors.isEmpty()) {
        QString errorMsg = tr("The following URLs were skipped:") + QStringLiteral("\n\n")
            + errors.join('\n') + QStringLiteral("\n\n")
            + tr("%n valid URL(s) will be processed.", "", m_pendingMods.size());
        auto *modal = new MessageModal(
            tr("Some URLs Invalid"),
            errorMsg,
            MessageModal::Information,
            MessageModal::Ok | MessageModal::Cancel
        );
        connect(modal, &MessageModal::finished, this, [this, modal]() {
            if (modal->clickedButton() == MessageModal::Ok) {
                m_currentModIndex = 0;
                m_results.clear();
                m_downloadedFilePaths.clear();
                m_downloadedFiles.clear();

                m_currentModId = m_pendingMods[0].modId;
                m_currentFileId = m_pendingMods[0].fileId;

                if (!m_client->hasApiKey()) {
                    showAuthPage();
                } else {
                    startDownloadProcess();
                }
            }
            modal->deleteLater();
        });
        m_modalManager->showModal(modal);
        return;
    }

    m_currentModIndex = 0;
    m_results.clear();
    m_downloadedFilePaths.clear();
    m_downloadedFiles.clear();

    m_currentModId = m_pendingMods[0].modId;
    m_currentFileId = m_pendingMods[0].fileId;

    if (!m_client->hasApiKey()) {
        showAuthPage();
        return;
    }

    startDownloadProcess();
}

void NexusDownloadModalContent::onAuthenticateClicked() {
    m_authenticateButton->setEnabled(false);
    m_authStatusLabel->setText(tr("Connecting to authentication server..."));
    m_auth->startAuthentication();
}

void NexusDownloadModalContent::onAuthStarted(const QString &browserUrl) {
    m_authStatusLabel->setText(tr("Opening browser for authentication...\n\nIf browser doesn't open, visit:\n%1").arg(browserUrl));
    QDesktopServices::openUrl(QUrl(browserUrl));
}

void NexusDownloadModalContent::onAuthComplete(const QString &apiKey) {
    m_client->setApiKey(apiKey);
    m_authStatusLabel->setText(tr("Authentication successful!"));
    startDownloadProcess();
}

void NexusDownloadModalContent::onAuthFailed(const QString &error) {
    m_authStatusLabel->setText(tr("Authentication failed: %1").arg(error));
    m_authenticateButton->setEnabled(true);
}

void NexusDownloadModalContent::startDownloadProcess() {
    showDownloadPage();

    if (m_pendingMods.size() > 1) {
        m_statusLabel->setText(tr("Mod %1 of %2: Fetching mod information...")
            .arg(m_currentModIndex + 1).arg(m_pendingMods.size()));
    } else {
        m_statusLabel->setText(tr("Fetching mod information..."));
    }

    m_client->getModInfo(m_currentModId);
}

void NexusDownloadModalContent::onModInfoReceived(const QString &author, const QString &description, const QString &) {
    m_author = author;
    m_description = description;

    if (m_pendingMods.size() > 1) {
        m_statusLabel->setText(tr("Mod %1 of %2: Fetching file list...")
            .arg(m_currentModIndex + 1).arg(m_pendingMods.size()));
    } else {
        m_statusLabel->setText(tr("Fetching file list..."));
    }

    m_client->getModFiles(m_currentModId);
}

void NexusDownloadModalContent::onModFilesReceived(const QList<NexusFileInfo> &files) {
    QList<NexusFileInfo> filtered;
    for (const NexusFileInfo &file : files) {
        if (file.categoryName.toUpper() != "ARCHIVED") {
            filtered.append(file);
        }
    }

    if (filtered.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("No files found for this mod"));
        showInputPage();
        return;
    }

    std::sort(filtered.begin(), filtered.end(), [this](const NexusFileInfo &a, const NexusFileInfo &b) {
        auto getPriority = [](const QString &category) -> int {
            QString cat = category.toUpper();
            if (cat == "MAIN") return 1;
            if (cat == "UPDATE") return 2;
            if (cat == "OPTIONAL") return 3;
            if (cat == "MISCELLANEOUS") return 4;
            if (cat == "OLD_VERSION") return 5;
            return 6;
        };

        int priorityA = getPriority(a.categoryName);
        int priorityB = getPriority(b.categoryName);

        if (priorityA != priorityB) {
            return priorityA < priorityB;
        }

        return a.uploadedTime > b.uploadedTime;
    });

    QList<FileItem> items;
    for (const NexusFileInfo &file : filtered) {
        QString displayText = file.name;

        if (!file.version.isEmpty()) {
            displayText += QString(" (v%1)").arg(file.version);
        }

        if (file.sizeBytes > 0) {
            displayText += QString(" - %1").arg(formatFileSize(file.sizeBytes));
        }

        if (!file.categoryName.isEmpty()) {
            displayText += QString(" [%1]").arg(file.categoryName);
        }

        items.append({file.id, displayText});
    }

    QString selectionTitle = m_pendingMods.size() > 1
        ? tr("Select Files - Mod %1 of %2 - %3").arg(m_currentModIndex + 1).arg(m_pendingMods.size()).arg(m_author)
        : tr("Select Files - %1").arg(m_author);

    auto *selectionModal = new FileSelectionModalContent(
        items,
        selectionTitle,
        tr("Multiple files found. Select one or more (Ctrl+Click or Shift+Click):"),
        true
    );

    connect(selectionModal, &FileSelectionModalContent::accepted, this, [this, selectionModal, filtered]() {
        m_selectedFileIds = selectionModal->getSelectedIds();
        if (m_selectedFileIds.isEmpty()) {
            showInputPage();
            return;
        }

        m_selectedFiles.clear();
        for (const QString &fileId : m_selectedFileIds) {
            for (const NexusFileInfo &file : filtered) {
                if (file.id == fileId) {
                    m_selectedFiles.append(file);
                    break;
                }
            }
        }

        m_currentDownloadIndex = 0;
        m_downloadedFilePaths.clear();
        m_downloadedFiles.clear();
        startNextDownload();
    });

    connect(selectionModal, &FileSelectionModalContent::rejected, this, [this]() {
        showInputPage();
    });

    m_modalManager->showModal(selectionModal);
}

void NexusDownloadModalContent::onDownloadLinkReceived(const QString &url) {
    QString fileName = QString("nexus_mod_%1_%2.tmp").arg(m_currentModId, m_currentFileId);
    QString savePath = generateTempPath(fileName);

    m_statusLabel->setText(tr("Downloading file..."));
    m_client->downloadFile(QUrl(url), savePath);
}

void NexusDownloadModalContent::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int percentage = static_cast<int>((received * 100) / total);
        m_progressBar->setValue(percentage);

        qint64 receivedMB = received / (1024 * 1024);
        qint64 totalMB = total / (1024 * 1024);

        QString statusText;
        if (m_pendingMods.size() > 1) {
            statusText = tr("Mod %1 of %2 - ").arg(m_currentModIndex + 1).arg(m_pendingMods.size());
        }

        statusText += tr("Downloading: %1 MB / %2 MB").arg(receivedMB).arg(totalMB);

        if (m_selectedFiles.size() > 1) {
            statusText += tr(" (File %1 of %2)").arg(m_currentDownloadIndex + 1).arg(m_selectedFiles.size());
        }
        m_statusLabel->setText(statusText);
    }
}

void NexusDownloadModalContent::onDownloadFinished(const QString &savePath) {
    m_downloadedFilePaths.append(savePath);

    if (m_currentDownloadIndex < m_selectedFiles.size()) {
        m_downloadedFiles.append(m_selectedFiles[m_currentDownloadIndex]);

        m_results.append({
            savePath,
            m_currentModId,
            m_pendingMods[m_currentModIndex].url,
            m_selectedFiles[m_currentDownloadIndex],
            m_author,
            m_description
        });
    }

    m_currentDownloadIndex++;

    if (m_currentDownloadIndex < m_selectedFiles.size()) {
        startNextDownload();
    } else {
        startNextMod();
    }
}

void NexusDownloadModalContent::startNextMod() {
    m_currentModIndex++;

    if (m_currentModIndex >= m_pendingMods.size()) {
        m_statusLabel->setText(tr("All downloads complete!"));
        m_progressBar->setValue(100);
        accept();
        return;
    }

    m_currentModId = m_pendingMods[m_currentModIndex].modId;
    m_currentFileId = m_pendingMods[m_currentModIndex].fileId;
    m_selectedFiles.clear();
    m_selectedFileIds.clear();
    m_currentDownloadIndex = 0;

    startDownloadProcess();
}

void NexusDownloadModalContent::startManualDownloadSequence() {
    if (m_currentDownloadIndex >= m_selectedFiles.size()) {
        startNextMod();
        return;
    }

    const NexusFileInfo &file = m_selectedFiles[m_currentDownloadIndex];
    m_currentFileId = file.id;

    QString modUrl = QString("https://www.nexusmods.com/foxhole/mods/%1?tab=files&file_id=%2")
                        .arg(m_currentModId, m_currentFileId);

    QString title;
    QString message;

    if (m_selectedFiles.size() > 1) {
        title = tr("Download File %1 of %2").arg(m_currentDownloadIndex + 1).arg(m_selectedFiles.size());
        message = tr("Downloading: %1\n\nThe browser will open. Please download the file.\n\nOnce complete, click OK to locate it.")
            .arg(file.name);
    } else {
        title = tr("Select Downloaded File");
        message = tr("Please download the file from your browser.\n\nOnce the download is complete, click OK to locate the file.");
    }

    if (m_pendingMods.size() > 1) {
        title = tr("Mod %1 of %2 - %3").arg(m_currentModIndex + 1).arg(m_pendingMods.size()).arg(title);
    }

    QDesktopServices::openUrl(QUrl(modUrl));

    auto *selectModal = new MessageModal(
        title,
        message,
        MessageModal::Information,
        MessageModal::Ok | MessageModal::Cancel
    );

    connect(selectModal, &MessageModal::finished, this, [this, file, selectModal]() {
        if (selectModal->clickedButton() == MessageModal::Ok) {
            QString dialogTitle = m_selectedFiles.size() > 1
                ? tr("Select Downloaded File - %1").arg(file.name)
                : tr("Select Downloaded File");

            QString filePath = QFileDialog::getOpenFileName(
                this,
                dialogTitle,
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                tr("Mod Files (*.pak *.zip *.rar *.7z *.tar.gz *.tar.bz2 *.tar.xz);;")
                + tr("Pak Files (*.pak);;")
                + tr("Archive Files (*.zip *.rar *.7z *.tar.gz *.tar.bz2 *.tar.xz);;")
                + tr("All Files (*.*)")
            );

            if (!filePath.isEmpty()) {
                if (m_selectedFiles.size() == 1) {
                    m_downloadedFilePaths.clear();
                    m_downloadedFiles.clear();
                }

                m_downloadedFilePaths.append(filePath);
                m_downloadedFiles.append(file);

                m_results.append({
                    filePath,
                    m_currentModId,
                    m_pendingMods[m_currentModIndex].url,
                    file,
                    m_author,
                    m_description
                });

                if (m_selectedFiles.size() > 1) {
                    int progress = ((m_currentDownloadIndex + 1) * 100) / m_selectedFiles.size();
                    m_progressBar->setValue(progress);
                    m_statusLabel->setText(tr("Downloaded %1 of %2 files")
                        .arg(m_currentDownloadIndex + 1).arg(m_selectedFiles.size()));
                }

                m_currentDownloadIndex++;
                startManualDownloadSequence();
            } else {
                if (m_selectedFiles.size() == 1) {
                    reject();
                } else {
                    showInputPage();
                }
            }
        } else {
            if (m_selectedFiles.size() == 1) {
                reject();
            } else {
                showInputPage();
            }
        }
        selectModal->deleteLater();
    });

    m_modalManager->showModal(selectModal);
}

void NexusDownloadModalContent::onError(const QString &error) {
    if (error == "PREMIUM_REQUIRED") {
        QString message;
        QString title = tr("Premium Required");

        if (m_selectedFiles.size() > 1) {
            message = tr(
                "Direct downloads via API require a Nexus Mods Premium account.\n\n"
                "Your browser will open for each of the %1 files you selected. "
                "Please download each file manually.\n\n"
                "You will be prompted to locate each downloaded file."
            ).arg(m_selectedFiles.size());
        } else {
            message = tr("Direct downloads via API require a Nexus Mods Premium account.\n\n"
                     "Your browser will open to the download page where you can download the file manually.\n\n"
                     "The mod will be installed with Nexus metadata for future updates.");
        }

        auto *modal = new MessageModal(
            title,
            message,
            MessageModal::Information,
            MessageModal::Ok | MessageModal::Cancel
        );

        connect(modal, &MessageModal::finished, this, [this, modal]() {
            if (modal->clickedButton() == MessageModal::Ok) {
                m_currentDownloadIndex = 0;
                if (m_selectedFiles.size() > 1) {
                    m_downloadedFilePaths.clear();
                    m_downloadedFiles.clear();
                }
                startManualDownloadSequence();
            } else {
                showInputPage();
            }
            modal->deleteLater();
        });

        m_modalManager->showModal(modal);
    } else {
        MessageModal::warning(m_modalManager, tr("Error"), error);
        showInputPage();
    }
}

QString NexusDownloadModalContent::generateTempPath(const QString &fileName) const {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(tempDir).filePath(fileName);
}

void NexusDownloadModalContent::startNextDownload() {
    if (m_currentDownloadIndex >= m_selectedFiles.size()) {
        return;
    }

    const NexusFileInfo &file = m_selectedFiles[m_currentDownloadIndex];
    m_currentFileId = file.id;

    m_progressBar->setValue(0);

    QString statusText;
    if (m_pendingMods.size() > 1) {
        statusText = tr("Mod %1 of %2 - ").arg(m_currentModIndex + 1).arg(m_pendingMods.size());
    }

    if (m_selectedFiles.size() > 1) {
        statusText += tr("Getting download link for file %1 of %2...")
            .arg(m_currentDownloadIndex + 1).arg(m_selectedFiles.size());
    } else {
        statusText += tr("Getting download link...");
    }

    m_statusLabel->setText(statusText);
    m_client->getDownloadLink(m_currentModId, m_currentFileId);
}

QString NexusDownloadModalContent::formatFileSize(qint64 bytes) const {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return tr("%1 GB").arg(bytes / static_cast<double>(GB), 0, 'f', 2);
    } else if (bytes >= MB) {
        return tr("%1 MB").arg(bytes / static_cast<double>(MB), 0, 'f', 2);
    } else if (bytes >= KB) {
        return tr("%1 KB").arg(bytes / static_cast<double>(KB), 0, 'f', 2);
    } else {
        return tr("%1 bytes").arg(bytes);
    }
}

void NexusDownloadModalContent::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    BaseModalContent::changeEvent(event);
}

void NexusDownloadModalContent::retranslateUi() {
    setTitle(tr("Download from Nexus Mods"));
    if (m_downloadButton) m_downloadButton->setText(tr("Download"));
    if (m_authenticateButton) m_authenticateButton->setText(tr("Authenticate"));
    if (m_cancelButton) m_cancelButton->setText(tr("Cancel"));
    if (m_inputInstructionLabel) m_inputInstructionLabel->setText(tr("Enter one or more Nexus Mods URLs for Foxhole mods:"));
    if (m_authInstructionLabel) m_authInstructionLabel->setText(tr("Authentication required to access Nexus Mods API."));
    if (m_urlEdit) m_urlEdit->setPlaceholderText(tr("Enter one or more Nexus Mods URLs (one per line)..."));
}
