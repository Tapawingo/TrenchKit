#include "NexusRegistrationModalContent.h"
#include "FileSelectionModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/api/NexusModsClient.h"
#include "core/api/NexusModsAuth.h"
#include "core/utils/NexusUrlParser.h"
#include "core/utils/Theme.h"
#include <algorithm>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>

NexusRegistrationModalContent::NexusRegistrationModalContent(NexusModsClient *client,
                                                             NexusModsAuth *auth,
                                                             ModalManager *modalManager,
                                                             const QString &modId,
                                                             const QString &modName,
                                                             QWidget *parent)
    : BaseModalContent(parent)
    , m_client(client)
    , m_auth(auth)
    , m_modalManager(modalManager)
    , m_localModId(modId)
    , m_localModName(modName)
{
    setTitle(tr("Register Mod with Nexus Mods"));
    setupUi();
    setPreferredSize(QSize(550, 400));

    connect(m_client, &NexusModsClient::modInfoReceived, this, &NexusRegistrationModalContent::onModInfoReceived);
    connect(m_client, &NexusModsClient::modFilesReceived, this, &NexusRegistrationModalContent::onModFilesReceived);
    connect(m_client, &NexusModsClient::errorOccurred, this, &NexusRegistrationModalContent::onError);

    connect(m_auth, &NexusModsAuth::authenticationStarted, this, &NexusRegistrationModalContent::onAuthStarted, Qt::UniqueConnection);
    connect(m_auth, &NexusModsAuth::authenticationComplete, this, &NexusRegistrationModalContent::onAuthComplete, Qt::UniqueConnection);
    connect(m_auth, &NexusModsAuth::authenticationFailed, this, &NexusRegistrationModalContent::onAuthFailed, Qt::UniqueConnection);
}

void NexusRegistrationModalContent::setupUi() {
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createInputPage());
    m_stack->addWidget(createAuthPage());

    bodyLayout()->addWidget(m_stack);

    m_fetchButton = new QPushButton(tr("Fetch Mod Info"), this);
    m_fetchButton->setDefault(true);
    connect(m_fetchButton, &QPushButton::clicked, this, &NexusRegistrationModalContent::onFetchClicked);
    footerLayout()->addWidget(m_fetchButton);

    m_authenticateButton = new QPushButton(tr("Authenticate"), this);
    m_authenticateButton->setDefault(true);
    connect(m_authenticateButton, &QPushButton::clicked, this, &NexusRegistrationModalContent::onAuthenticateClicked);
    footerLayout()->addWidget(m_authenticateButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setAutoDefault(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &NexusRegistrationModalContent::reject);
    footerLayout()->addWidget(m_cancelButton);

    showInputPage();
}

QWidget* NexusRegistrationModalContent::createInputPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *modLabel = new QLabel(tr("Registering: <b>%1</b>").arg(m_localModName), page);
    modLabel->setWordWrap(true);
    modLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                           .arg(Theme::Colors::TEXT_PRIMARY));
    layout->addWidget(modLabel);

    layout->addSpacing(10);

    auto *instructionLabel = new QLabel(tr("Enter Nexus Mods URL for this mod:"), page);
    instructionLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                                   .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(instructionLabel);

    m_urlEdit = new QLineEdit(page);
    m_urlEdit->setPlaceholderText("https://www.nexusmods.com/foxhole/mods/...");
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &NexusRegistrationModalContent::onFetchClicked);
    layout->addWidget(m_urlEdit);

    layout->addStretch();

    return page;
}

QWidget* NexusRegistrationModalContent::createAuthPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *instructionLabel = new QLabel(tr("Authentication required to access Nexus Mods API."), page);
    instructionLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                                   .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(instructionLabel);

    m_authStatusLabel = new QLabel(tr("Click Authenticate to begin..."), page);
    m_authStatusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }")
                                    .arg(Theme::Colors::TEXT_MUTED));
    m_authStatusLabel->setWordWrap(true);
    layout->addWidget(m_authStatusLabel);

    layout->addStretch();

    return page;
}

void NexusRegistrationModalContent::showInputPage() {
    m_stack->setCurrentIndex(InputPage);
    updateFooterButtons();
}

void NexusRegistrationModalContent::showAuthPage() {
    m_authStatusLabel->setText(tr("Click Authenticate to begin..."));
    m_authenticateButton->setEnabled(true);
    m_stack->setCurrentIndex(AuthPage);
    updateFooterButtons();
}

void NexusRegistrationModalContent::updateFooterButtons() {
    int currentPage = m_stack->currentIndex();

    m_fetchButton->setVisible(currentPage == InputPage);
    m_authenticateButton->setVisible(currentPage == AuthPage);
    m_cancelButton->setVisible(true);
}

void NexusRegistrationModalContent::onFetchClicked() {
    QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("Please enter a URL"));
        return;
    }

    auto result = NexusUrlParser::parseUrl(url);
    if (!result.isValid) {
        MessageModal::warning(m_modalManager, tr("Invalid URL"), result.error);
        return;
    }

    m_currentModId = result.modId;
    m_url = QStringLiteral("https://www.nexusmods.com/foxhole/mods/%1").arg(result.modId);

    if (!m_client->hasApiKey()) {
        showAuthPage();
        return;
    }

    m_client->getModInfo(m_currentModId);
}

void NexusRegistrationModalContent::onAuthenticateClicked() {
    m_authenticateButton->setEnabled(false);
    m_authStatusLabel->setText(tr("Connecting to authentication server..."));
    m_auth->startAuthentication();
}

void NexusRegistrationModalContent::onAuthStarted(const QString &browserUrl) {
    m_authStatusLabel->setText(tr("Opening browser for authentication...\n\nIf browser doesn't open, visit:\n%1").arg(browserUrl));
    QDesktopServices::openUrl(QUrl(browserUrl));
}

void NexusRegistrationModalContent::onAuthComplete(const QString &apiKey) {
    m_client->setApiKey(apiKey);
    m_authStatusLabel->setText(tr("Authentication successful! Fetching mod information..."));
    m_client->getModInfo(m_currentModId);
}

void NexusRegistrationModalContent::onAuthFailed(const QString &error) {
    m_authStatusLabel->setText(tr("Authentication failed: %1").arg(error));
    m_authenticateButton->setEnabled(true);
}

void NexusRegistrationModalContent::onModInfoReceived(const QString &author, const QString &description, const QString &) {
    m_author = author;
    m_description = description;

    m_client->getModFiles(m_currentModId);
}

void NexusRegistrationModalContent::onModFilesReceived(const QList<NexusFileInfo> &files) {
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

    auto *selectionModal = new FileSelectionModalContent(
        items,
        tr("Select File - %1").arg(m_localModName),
        tr("Select the file that matches your local mod '%1':").arg(m_localModName),
        false
    );

    connect(selectionModal, &FileSelectionModalContent::accepted, this, [this, selectionModal, filtered]() {
        QStringList selectedIds = selectionModal->getSelectedIds();
        if (selectedIds.isEmpty()) {
            return;
        }

        QString selectedFileId = selectedIds.first();

        m_selectedFiles.clear();
        for (const NexusFileInfo &file : filtered) {
            if (file.id == selectedFileId) {
                m_selectedFiles.append(file);
                break;
            }
        }

        accept();
    });

    connect(selectionModal, &FileSelectionModalContent::rejected, this, [this]() {
        // User cancelled file selection, do nothing
    });

    m_modalManager->showModal(selectionModal);
}

void NexusRegistrationModalContent::onError(const QString &error) {
    MessageModal::warning(m_modalManager, tr("Error"), error);
    showInputPage();
}

QString NexusRegistrationModalContent::formatFileSize(qint64 bytes) const {
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024 * 1024));
    } else {
        return QString("%1 GB").arg(bytes / (1024 * 1024 * 1024));
    }
}
