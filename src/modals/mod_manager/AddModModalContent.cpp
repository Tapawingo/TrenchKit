#include "AddModModalContent.h"
#include "FileSelectionModalContent.h"
#include "NexusDownloadModalContent.h"
#include "ItchDownloadModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/managers/ModManager.h"
#include "core/api/NexusModsClient.h"
#include "core/api/NexusModsAuth.h"
#include "core/api/ItchClient.h"
#include "core/api/ItchAuth.h"
#include "core/utils/ArchiveExtractor.h"
#include "core/utils/PakFileReader.h"
#include "core/utils/Theme.h"
#include <QEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QCoreApplication>

AddModModalContent::AddModModalContent(ModManager *modManager,
                                       NexusModsClient *nexusClient,
                                       NexusModsAuth *nexusAuth,
                                       ItchClient *itchClient,
                                       ItchAuth *itchAuth,
                                       ModalManager *modalManager,
                                       QWidget *parent)
    : BaseModalContent(parent)
    , m_modManager(modManager)
    , m_nexusClient(nexusClient)
    , m_nexusAuth(nexusAuth)
    , m_itchClient(itchClient)
    , m_itchAuth(itchAuth)
    , m_modalManager(modalManager)
{
    setTitle(tr("Add Mod"));
    setupUi();
    setPreferredSize(QSize(350, 280));

    // Closing the modal stops whatever is unpacking or copying in the background.
    connect(this, &BaseModalContent::rejected, this, [this]() {
        m_cancelled = true;
        if (m_activeToken) {
            m_activeToken->cancel();
        }
    });

    if (!m_modManager) {
        m_fromFileButton->setEnabled(false);
        m_fromNexusButton->setEnabled(false);
        m_fromItchButton->setEnabled(false);
    } else {
        m_fromNexusButton->setEnabled(m_nexusClient && m_nexusAuth);
        m_fromItchButton->setEnabled(m_itchClient && m_itchAuth);
    }
}

void AddModModalContent::setupUi() {
    bodyLayout()->setSpacing(16);

    m_fromFileButton = new QPushButton(tr("From File"), this);
    m_fromFileButton->setMinimumHeight(40);
    m_fromFileButton->setCursor(Qt::PointingHandCursor);
    connect(m_fromFileButton, &QPushButton::clicked, this, &AddModModalContent::onFromFileClicked);
    bodyLayout()->addWidget(m_fromFileButton);

    m_fromNexusButton = new QPushButton(tr("From Nexus Mods"), this);
    m_fromNexusButton->setMinimumHeight(40);
    m_fromNexusButton->setCursor(Qt::PointingHandCursor);
    connect(m_fromNexusButton, &QPushButton::clicked, this, &AddModModalContent::onFromNexusClicked);
    bodyLayout()->addWidget(m_fromNexusButton);

    m_fromItchButton = new QPushButton(tr("From itch.io"), this);
    m_fromItchButton->setMinimumHeight(40);
    m_fromItchButton->setCursor(Qt::PointingHandCursor);
    connect(m_fromItchButton, &QPushButton::clicked, this, &AddModModalContent::onFromItchClicked);
    bodyLayout()->addWidget(m_fromItchButton);

    bodyLayout()->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    connect(m_cancelButton, &QPushButton::clicked, this, &AddModModalContent::reject);
    footerLayout()->addWidget(m_cancelButton);

    retranslateUi();
}

void AddModModalContent::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    BaseModalContent::changeEvent(event);
}

void AddModModalContent::retranslateUi() {
    setTitle(tr("Add Mod"));
    m_fromFileButton->setText(tr("From File"));
    m_fromNexusButton->setText(tr("From Nexus Mods"));
    m_fromItchButton->setText(tr("From itch.io"));
    m_cancelButton->setText(tr("Cancel"));
}

void AddModModalContent::onFromFileClicked() {
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Select Mod Files"),
        QString(),
        tr("Mod Files (*.pak *.zip *.rar *.7z *.tar.gz *.tar.bz2 *.tar.xz);;"
           "Pak Files (*.pak);;"
           "Archive Files (*.zip *.rar *.7z *.tar.gz *.tar.bz2 *.tar.xz);;"
           "All Files (*.*)")
    );

    if (filePaths.isEmpty()) {
        return;
    }

    QList<FileToProcess> filesToProcess;
    for (const QString &filePath : filePaths) {
        FileToProcess fileData;
        fileData.filePath = filePath;
        filesToProcess.append(fileData);
    }

    startProcessingFiles(filesToProcess);
}

bool AddModModalContent::isArchiveFile(const QString &filePath) const {
    return ArchiveExtractor::isArchiveFile(filePath);
}

void AddModModalContent::continueBatch() {
    m_waitingForModal = false;
    if (m_cancelled) {
        return;
    }
    m_currentFileIndex++;
    QTimer::singleShot(200, this, &AddModModalContent::processNextFile);
}

void AddModModalContent::handleArchiveFile(const FileToProcess &file, bool isBatchProcessing) {
    // The batch loop stays paused until this archive has been dealt with.
    if (isBatchProcessing) {
        m_waitingForModal = true;
    }
    if (m_processingLabel) {
        m_processingLabel->setText(tr("Unpacking %1...").arg(QFileInfo(file.filePath).fileName()));
    }
    if (m_processingProgress) {
        m_processingProgress->setRange(0, 0);
    }

    m_activeToken = ArchiveExtractor::extractPakFilesAsync(file.filePath, this,
        [=, this](const ArchiveExtractor::ExtractResult &result) {
        if (m_processingProgress) {
            m_processingProgress->setRange(0, static_cast<int>(m_filesToProcess.size()));
            m_processingProgress->setValue(m_currentFileIndex);
        }

        if (isBatchProcessing && (file.filePath.contains("nexus_mod_") || file.filePath.contains("itch_game_"))) {
            QFile::remove(file.filePath);
        }

        if (m_cancelled || result.cancelled) {
            ArchiveExtractor::cleanupTempDir(result.tempDir);
            return;
        }

        if (!result.success) {
            MessageModal::warning(m_modalManager, tr("Error"), result.error);
            if (isBatchProcessing) {
                continueBatch();
            }
            return;
        }

        if (result.pakFiles.isEmpty()) {
            MessageModal::warning(m_modalManager, tr("Error"), tr("No .pak files found in archive"));
            ArchiveExtractor::cleanupTempDir(result.tempDir);
            if (isBatchProcessing) {
                continueBatch();
            }
            return;
        }

        FileToProcess meta = file;
        meta.customModName.clear();

        const auto finish = [=, this](bool anyAdded) {
            ArchiveExtractor::cleanupTempDir(result.tempDir);
            if (isBatchProcessing) {
                continueBatch();
            } else if (anyAdded) {
                accept();
            }
        };

        if (result.pakFiles.size() == 1) {
            addPaksSequentially(result.pakFiles, meta, [finish]() { finish(true); });
            return;
        }

        QStringList fileNames;
        for (const QString &path : result.pakFiles) {
            fileNames.append(QFileInfo(path).fileName());
        }

        auto * const fileModal = new FileSelectionModalContent(fileNames, QFileInfo(file.filePath).fileName(), true);
        connect(fileModal, &FileSelectionModalContent::accepted, this, [=, this]() {
            QStringList selectedPaks;

            for (const QString &fileName : fileModal->getSelectedFiles()) {
                for (const QString &pakPath : result.pakFiles) {
                    if (QFileInfo(pakPath).fileName() == fileName) {
                        selectedPaks.append(pakPath);
                        break;
                    }
                }
            }

            const bool anySelected = !selectedPaks.isEmpty();
            addPaksSequentially(selectedPaks, meta, [finish, anySelected]() { finish(anySelected); });
        });
        connect(fileModal, &FileSelectionModalContent::rejected, this, [=, this]() {
            ArchiveExtractor::cleanupTempDir(result.tempDir);

            if (isBatchProcessing) {
                continueBatch();
            }
        });

        m_modalManager->showModal(fileModal);
    });
}

void AddModModalContent::addPaksSequentially(const QStringList &pakPaths, const FileToProcess &meta,
                                             std::function<void()> onDone) {
    if (pakPaths.isEmpty() || m_cancelled) {
        onDone();
        return;
    }

    FileToProcess next = meta;
    next.filePath = pakPaths.first();
    const QStringList rest = pakPaths.mid(1);
    handlePakFile(next, [this, rest, meta, onDone = std::move(onDone)]() {
        addPaksSequentially(rest, meta, onDone);
    });
}

void AddModModalContent::handlePakFile(const FileToProcess &file, std::function<void()> onDone) {
    QString normalizedPath = file.filePath;
    if (!normalizedPath.endsWith(".pak", Qt::CaseInsensitive)) {
        auto parseResult = PakFileReader::extractFilePaths(normalizedPath);
        if (!parseResult.success) {
            MessageModal::warning(m_modalManager, tr("Error"),
                                  tr("Downloaded file is not a valid .pak or supported archive."));
            onDone();
            return;
        }

        QFileInfo fileInfo(normalizedPath);
        QString newPath = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".pak";
        if (QFile::exists(newPath)) {
            QFile::remove(newPath);
        }
        if (QFile::rename(normalizedPath, newPath)) {
            normalizedPath = newPath;
        }
    }

    const QString modName = !file.customModName.isEmpty() ? file.customModName
                                                          : QFileInfo(normalizedPath).completeBaseName();

    m_activeToken = m_modManager->addModAsync(normalizedPath, {
            .name = modName,
            .nexusModId = file.nexusModId,
            .nexusFileId = file.nexusFileId,
            .nexusUrl = file.nexusUrl,
            .author = file.author,
            .description = file.description,
            .version = file.version,
            .itchGameId = file.itchGameId,
            .itchUrl = file.itchUrl,
            .itchUploadId = file.itchUploadId,
            .uploadDate = file.uploadDate
        }, this, [this, modName, onDone = std::move(onDone)](bool added) {
        if (m_cancelled) {
            return;
        }
        if (!added) {
            MessageModal::warning(m_modalManager, tr("Error"), tr("Failed to add mod: %1").arg(modName));
        } else {
            emit modAdded(modName);
        }
        onDone();
    });
}

void AddModModalContent::onFromNexusClicked() {
    if (!m_nexusClient || !m_nexusAuth) {
        return;
    }

    auto * const nexusModal = new NexusDownloadModalContent(m_nexusClient, m_nexusAuth, m_modalManager);
    connect(nexusModal, &NexusDownloadModalContent::accepted, this, [this, nexusModal]() {
        QList<NexusDownloadResult> results = nexusModal->getDownloadResults();
        if (results.isEmpty()) {
            return;
        }

        QList<FileToProcess> filesToProcess;
        for (const auto &result : results) {
            FileToProcess fileData;
            fileData.filePath = result.filePath;
            fileData.nexusModId = result.modId;
            fileData.nexusFileId = result.fileInfo.id;
            fileData.nexusUrl = result.url;
            fileData.author = result.author;
            fileData.description = result.description;
            fileData.version = result.fileInfo.version;
            filesToProcess.append(fileData);
        }

        startProcessingFiles(filesToProcess);
    });
    m_modalManager->showModal(nexusModal);
}

void AddModModalContent::onFromItchClicked() {
    if (!m_itchClient || !m_itchAuth) {
        return;
    }

    auto * const itchModal = new ItchDownloadModalContent(m_itchClient, m_itchAuth, m_modalManager);
    connect(itchModal, &ItchDownloadModalContent::accepted, this, [this, itchModal]() {
        QList<ItchDownloadResult> results = itchModal->getDownloadResults();
        if (results.isEmpty()) {
            return;
        }

        QList<FileToProcess> filesToProcess;
        for (const auto &result : results) {
            FileToProcess fileData;
            fileData.filePath = result.filePath;
            fileData.itchGameId = result.gameId;
            fileData.itchUrl = result.url;
            fileData.itchUploadId = result.uploadInfo.id;
            fileData.author = result.author;
            fileData.description = result.gameTitle;

            QDateTime uploadDate = result.uploadInfo.updatedAt.isValid()
                ? result.uploadInfo.updatedAt : result.uploadInfo.createdAt;
            fileData.uploadDate = uploadDate;

            QFileInfo fileInfo(result.filePath);
            QString fileName = fileInfo.fileName();
            if (fileName.startsWith("itch_game_")) {
                int secondUnderscore = fileName.indexOf('_', 10);
                int thirdUnderscore = fileName.indexOf('_', secondUnderscore + 1);
                if (thirdUnderscore != -1) {
                    fileData.customModName = QFileInfo(fileName.mid(thirdUnderscore + 1)).completeBaseName();
                }
            }

            filesToProcess.append(fileData);
        }

        startProcessingFiles(filesToProcess);
    });
    m_modalManager->showModal(itchModal);
}

void AddModModalContent::startProcessingFiles(const QList<FileToProcess> &files) {
    m_filesToProcess = files;
    m_currentFileIndex = 0;

    setTitle(tr("Processing Files"));

    m_fromFileButton->setVisible(false);
    m_fromNexusButton->setVisible(false);
    m_fromItchButton->setVisible(false);

    if (!m_processingLabel) {
        m_processingLabel = new QLabel(this);
        m_processingLabel->setAlignment(Qt::AlignCenter);
        m_processingLabel->setStyleSheet("QLabel { font-size: 14px; color: #e1d0ab; padding: 20px; }");
        bodyLayout()->insertWidget(0, m_processingLabel);
    }

    if (!m_processingProgress) {
        m_processingProgress = new QProgressBar(this);
        m_processingProgress->setMinimumHeight(30);
        bodyLayout()->insertWidget(1, m_processingProgress);
    }

    m_processingProgress->setMaximum(files.size());
    m_processingProgress->setValue(0);
    m_processingLabel->setVisible(true);
    m_processingProgress->setVisible(true);

    QTimer::singleShot(100, this, &AddModModalContent::processNextFile);
}

void AddModModalContent::processNextFile() {
    if (m_currentFileIndex >= m_filesToProcess.size()) {
        setTitle(tr("Add Mod"));
        m_fromFileButton->setVisible(true);
        m_fromNexusButton->setVisible(true);
        m_fromItchButton->setVisible(true);
        m_processingLabel->setVisible(false);
        m_processingProgress->setVisible(false);
        m_filesToProcess.clear();
        m_waitingForModal = false;
        accept();
        return;
    }

    if (m_cancelled) {
        return;
    }

    const FileToProcess fileData = m_filesToProcess[m_currentFileIndex];
    m_processingLabel->setText(tr("Processing file %1 of %2...")
        .arg(m_currentFileIndex + 1).arg(m_filesToProcess.size()));
    m_processingProgress->setValue(m_currentFileIndex);

    if (isArchiveFile(fileData.filePath)) {
        // Unpacking runs on a worker thread; the batch resumes from its completion handler.
        handleArchiveFile(fileData, true);
        return;
    }

    m_waitingForModal = true;
    handlePakFile(fileData, [this, filePath = fileData.filePath]() {
        if (filePath.contains("nexus_mod_") || filePath.contains("itch_game_")) {
            QFile::remove(filePath);
        }
        continueBatch();
    });
}
