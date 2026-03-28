#include "ItchRegistrationModalContent.h"
#include "FileSelectionModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/api/ItchClient.h"
#include "core/api/ItchAuth.h"
#include "core/utils/ItchUrlParser.h"
#include "core/utils/Theme.h"
#include <algorithm>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

ItchRegistrationModalContent::ItchRegistrationModalContent(ItchClient *client,
                                                           ItchAuth *auth,
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
    setTitle(tr("Register Mod with itch.io"));
    setupUi();
    setPreferredSize(QSize(550, 400));

    connect(m_client, &ItchClient::gameIdReceived, this, &ItchRegistrationModalContent::onGameInfoReceived);
    connect(m_client, &ItchClient::uploadsReceived, this, &ItchRegistrationModalContent::onUploadsReceived);
    connect(m_client, &ItchClient::errorOccurred, this, &ItchRegistrationModalContent::onError);
}

void ItchRegistrationModalContent::setupUi() {
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createInputPage());
    m_stack->addWidget(createAuthPage());

    bodyLayout()->addWidget(m_stack);

    m_fetchButton = new QPushButton(tr("Fetch Game Info"), this);
    m_fetchButton->setDefault(true);
    connect(m_fetchButton, &QPushButton::clicked, this, &ItchRegistrationModalContent::onFetchClicked);
    footerLayout()->addWidget(m_fetchButton);

    m_authenticateButton = new QPushButton(tr("Submit"), this);
    m_authenticateButton->setDefault(true);
    connect(m_authenticateButton, &QPushButton::clicked, this, &ItchRegistrationModalContent::onAuthenticateClicked);
    footerLayout()->addWidget(m_authenticateButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setAutoDefault(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &ItchRegistrationModalContent::reject);
    footerLayout()->addWidget(m_cancelButton);

    showInputPage();
}

QWidget* ItchRegistrationModalContent::createInputPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *modLabel = new QLabel(tr("Registering: <b>%1</b>").arg(m_localModName), page);
    modLabel->setWordWrap(true);
    modLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                           .arg(Theme::Colors::TEXT_PRIMARY));
    layout->addWidget(modLabel);

    layout->addSpacing(10);

    auto *instructionLabel = new QLabel(tr("Enter itch.io URL for this mod:"), page);
    instructionLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                                   .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(instructionLabel);

    m_urlEdit = new QLineEdit(page);
    m_urlEdit->setPlaceholderText("https://creator.itch.io/game-name");
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &ItchRegistrationModalContent::onFetchClicked);
    layout->addWidget(m_urlEdit);

    layout->addStretch();

    return page;
}

QWidget* ItchRegistrationModalContent::createAuthPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(tr("<b>API Key Required</b>"), page);
    titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                             .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(titleLabel);

    m_authStatusLabel = new QLabel(page);
    m_authStatusLabel->setTextFormat(Qt::RichText);
    m_authStatusLabel->setOpenExternalLinks(true);
    m_authStatusLabel->setWordWrap(true);
    m_authStatusLabel->setText(
        tr("To access itch.io, you need an API key.<br><br>"
        "1. Visit <a href='https://itch.io/user/settings/api-keys'>https://itch.io/user/settings/api-keys</a><br>"
        "2. Click \"Generate new API key\"<br>"
        "3. Copy the key and paste it below:")
    );
    m_authStatusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }")
                                    .arg(Theme::Colors::TEXT_SECONDARY));
    layout->addWidget(m_authStatusLabel);

    m_apiKeyEdit = new QLineEdit(page);
    m_apiKeyEdit->setPlaceholderText(tr("Paste your API key here..."));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    connect(m_apiKeyEdit, &QLineEdit::returnPressed, this, &ItchRegistrationModalContent::onAuthenticateClicked);
    layout->addWidget(m_apiKeyEdit);

    layout->addStretch();

    return page;
}

void ItchRegistrationModalContent::showInputPage() {
    m_stack->setCurrentIndex(InputPage);
    updateFooterButtons();
}

void ItchRegistrationModalContent::showAuthPage() {
    m_stack->setCurrentIndex(AuthPage);
    updateFooterButtons();
}

void ItchRegistrationModalContent::updateFooterButtons() {
    int currentPage = m_stack->currentIndex();

    m_fetchButton->setVisible(currentPage == InputPage);
    m_authenticateButton->setVisible(currentPage == AuthPage);
    m_cancelButton->setVisible(true);
}

void ItchRegistrationModalContent::onFetchClicked() {
    QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("Please enter a URL"));
        return;
    }

    auto result = ItchUrlParser::parseUrl(url);
    if (!result.isValid) {
        MessageModal::warning(m_modalManager, tr("Invalid URL"), result.error);
        return;
    }

    m_url = QStringLiteral("https://%1.itch.io/%2").arg(result.creator, result.gameName);
    m_pendingUrl = url;

    if (!m_client->hasApiKey()) {
        showAuthPage();
        return;
    }

    m_client->getGameId(result.creator, result.gameName);
}

void ItchRegistrationModalContent::onAuthenticateClicked() {
    QString apiKey = m_apiKeyEdit->text().trimmed();
    if (apiKey.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("Please enter an API key"));
        return;
    }

    m_client->setApiKey(apiKey);
    m_authenticateButton->setEnabled(false);
    m_authStatusLabel->setText(tr("API key saved. Fetching game information..."));

    auto result = ItchUrlParser::parseUrl(m_pendingUrl);
    m_url = QStringLiteral("https://%1.itch.io/%2").arg(result.creator, result.gameName);

    QTimer::singleShot(500, this, [this, result]() {
        m_client->getGameId(result.creator, result.gameName);
    });
}

void ItchRegistrationModalContent::onAuthStarted(const QString &browserUrl) {
    Q_UNUSED(browserUrl);
}

void ItchRegistrationModalContent::onAuthComplete(const QString &apiKey) {
    Q_UNUSED(apiKey);
}

void ItchRegistrationModalContent::onAuthFailed(const QString &error) {
    m_authStatusLabel->setText(tr("Authentication failed: %1").arg(error));
    m_authenticateButton->setEnabled(true);
}

void ItchRegistrationModalContent::onGameInfoReceived(const QString &gameId, const QString &, const QString &author) {
    m_currentGameId = gameId;
    m_author = author;

    m_client->getGameUploads(gameId);
}

void ItchRegistrationModalContent::onUploadsReceived(const QList<ItchUploadInfo> &uploads) {
    if (uploads.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Error"), tr("No files found for this game"));
        showInputPage();
        return;
    }

    QList<ItchUploadInfo> sorted = uploads;
    std::sort(sorted.begin(), sorted.end(), [](const ItchUploadInfo &a, const ItchUploadInfo &b) {
        return a.createdAt > b.createdAt;
    });

    QList<FileItem> items;
    for (const ItchUploadInfo &upload : sorted) {
        QString displayText = upload.filename;

        if (!upload.displayName.isEmpty()) {
            displayText = upload.displayName + QString(" (%1)").arg(upload.filename);
        }

        if (upload.sizeBytes > 0) {
            displayText += QString(" - %1").arg(formatFileSize(upload.sizeBytes));
        }

        if (upload.createdAt.isValid()) {
            displayText += QString(" [%1]").arg(upload.createdAt.toString("yyyy-MM-dd"));
        }

        items.append({upload.id, displayText});
    }

    auto *selectionModal = new FileSelectionModalContent(
        items,
        tr("Select Upload - %1").arg(m_localModName),
        tr("Select the upload that matches your local mod '%1':").arg(m_localModName),
        false
    );

    connect(selectionModal, &FileSelectionModalContent::accepted, this, [this, selectionModal, sorted]() {
        QStringList selectedIds = selectionModal->getSelectedIds();
        if (selectedIds.isEmpty()) {
            return;
        }

        QString selectedUploadId = selectedIds.first();

        m_selectedUploads.clear();
        for (const ItchUploadInfo &upload : sorted) {
            if (upload.id == selectedUploadId) {
                m_selectedUploads.append(upload);
                break;
            }
        }

        accept();
    });

    connect(selectionModal, &FileSelectionModalContent::rejected, this, [this]() {
        // User cancelled upload selection, do nothing
    });

    m_modalManager->showModal(selectionModal);
}

void ItchRegistrationModalContent::onError(const QString &error) {
    MessageModal::warning(m_modalManager, tr("Error"), error);
    showInputPage();
}

QString ItchRegistrationModalContent::formatFileSize(qint64 bytes) const {
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
