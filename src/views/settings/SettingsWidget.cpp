#include "SettingsWidget.h"

#include "common/modals/ModalManager.h"
#include "common/modals/MessageModal.h"
#include "common/modals/InputModal.h"
#include "core/services/UpdaterService.h"
#include "core/api/NexusModsClient.h"
#include "core/api/NexusModsAuth.h"
#include "core/api/ItchClient.h"
#include "core/api/ItchAuth.h"
#include "core/utils/Logger.h"
#include "core/utils/Theme.h"
#include "core/utils/TranslationManager.h"
#include "common/widgets/PanelFrame.h"
#include "common/widgets/GradientFrame.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#endif
#ifdef Q_OS_LINUX
#include <QProcess>
#endif

SettingsWidget::SettingsWidget(QWidget *parent, UpdaterService *updater)
    : QWidget(parent),
      m_updater(updater) {
    buildUi();
    loadSettings(false);
}

void SettingsWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SettingsWidget::retranslateUi() {
    if (m_titleLabel) m_titleLabel->setText(tr("Settings"));
    if (m_languageLabel) m_languageLabel->setText(tr("Language"));
    if (m_languageCombo) {
        m_languageCombo->setItemText(0, tr("System default"));
    }
    if (m_updaterHeader) m_updaterHeader->setText(tr("Updater"));
    if (m_currentVersionLabel) m_currentVersionLabel->setText(tr("Current version"));
    if (m_sourceLabel) m_sourceLabel->setText(tr("Source"));
    if (m_channelLabel) m_channelLabel->setText(tr("Update channel"));
    if (m_updateChannelCombo) {
        m_updateChannelCombo->setItemText(0, tr("Stable"));
        m_updateChannelCombo->setItemText(1, tr("Pre-release"));
    }
    if (m_autoCheckLabel) m_autoCheckLabel->setText(tr("Check for updates on startup"));
    if (m_downloadLabel) m_downloadLabel->setText(tr("Download location (optional)"));
    if (m_downloadDirEdit) m_downloadDirEdit->setPlaceholderText(
        tr("Default: %1").arg(
            QDir::toNativeSeparators(
                QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                    .filePath(QStringLiteral("updates")))));
    if (m_downloadBrowseButton) m_downloadBrowseButton->setText(tr("Browse..."));
    if (m_checkLabel) m_checkLabel->setText(tr("Check for updates now"));
    if (m_checkNowButton) m_checkNowButton->setText(tr("Check now"));
    if (m_shortcutsHeader) m_shortcutsHeader->setText(tr("Shortcuts"));
    if (m_desktopShortcutLabel) m_desktopShortcutLabel->setText(tr("Desktop shortcut"));
    if (m_addDesktopShortcutButton) m_addDesktopShortcutButton->setText(tr("Add Desktop Shortcut"));
    if (m_startMenuLabel) m_startMenuLabel->setText(tr("Start menu"));
    if (m_addStartMenuShortcutButton) m_addStartMenuShortcutButton->setText(tr("Add to Start Menu"));
    if (m_nexusHeader) m_nexusHeader->setText(tr("Nexus Mods Integration"));
    if (m_nexusConnectionLabel) m_nexusConnectionLabel->setText(tr("Connection status"));
    if (m_nexusAuthLabel) m_nexusAuthLabel->setText(tr("Authentication"));
    if (m_nexusAuthButton) m_nexusAuthButton->setText(tr("Authenticate (SSO)"));
    if (m_nexusClearButton) m_nexusClearButton->setText(tr("Clear API Key"));
    if (m_itchHeader) m_itchHeader->setText(tr("Itch.io Integration"));
    if (m_itchConnectionLabel) m_itchConnectionLabel->setText(tr("Connection status"));
    if (m_itchAuthLabel) m_itchAuthLabel->setText(tr("Authentication"));
    if (m_itchAuthButton) m_itchAuthButton->setText(tr("Add API Key"));
    if (m_itchClearButton) m_itchClearButton->setText(tr("Clear API Key"));
    if (m_loggingHeader) m_loggingHeader->setText(tr("Logging"));
    if (m_logLocationLabel) m_logLocationLabel->setText(tr("Log files location"));
    if (m_logActionsLabel) m_logActionsLabel->setText(tr("Access logs"));
    if (m_openLogsButton) m_openLogsButton->setText(tr("Open Log Folder"));
    if (m_copyLogPathButton) m_copyLogPathButton->setText(tr("Copy Log Path"));
    if (m_fileAssocHeader) m_fileAssocHeader->setText(tr("File Associations"));
    if (m_tkprofileLabel) m_tkprofileLabel->setText(tr("Profile files (.tkprofile)"));
    if (m_tkprofileAssociateButton) m_tkprofileAssociateButton->setText(tr("Set as Default Handler"));
    if (m_settingsCancelButton) m_settingsCancelButton->setText(tr("Cancel"));
    if (m_settingsSaveButton) m_settingsSaveButton->setText(tr("Save"));
}

bool SettingsWidget::applyStoredSettings() {
    loadSettings(true);
    return m_autoCheckCheckbox ? m_autoCheckCheckbox->isChecked() : false;
}

void SettingsWidget::loadSettingsIntoUi() {
    loadSettings(false);
}

void SettingsWidget::setCurrentVersion(const QString &version) {
    if (m_versionLabel) {
        m_versionLabel->setText(version.isEmpty() ? QStringLiteral("—") : version);
    }
}

void SettingsWidget::setCheckStatus(const QString &status) {
    if (m_checkStatusLabel) {
        m_checkStatusLabel->setText(status.isEmpty() ? tr("Idle") : status);
    }
}

QString SettingsWidget::resolvedDownloadDir() const {
    QString dir;
    if (m_downloadDirEdit) {
        dir = m_downloadDirEdit->text().trimmed();
    }
    if (dir.isEmpty()) {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        return QDir(base).filePath("updates");
    }
    return dir;
}

void SettingsWidget::buildUi() {
    auto *overlayLayout = new QVBoxLayout(this);
    overlayLayout->setContentsMargins(
        Theme::Spacing::SETTINGS_OUTER_MARGIN,
        Theme::Spacing::SETTINGS_OUTER_MARGIN,
        Theme::Spacing::SETTINGS_OUTER_MARGIN,
        Theme::Spacing::SETTINGS_OUTER_MARGIN);

    m_panel = new PanelFrame(this);
    m_panel->setObjectName("settingsPanel");
    m_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(
        Theme::Spacing::SETTINGS_PANEL_MARGIN,
        Theme::Spacing::SETTINGS_PANEL_MARGIN,
        Theme::Spacing::SETTINGS_PANEL_MARGIN,
        Theme::Spacing::SETTINGS_PANEL_MARGIN);
    panelLayout->setSpacing(Theme::Spacing::SETTINGS_SECTION_SPACING);

    auto *scrollArea = new QScrollArea(m_panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new GradientFrame(scrollArea);
    m_container->setObjectName("settingsContainer");
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *containerLayout = new QGridLayout(m_container);
    containerLayout->setContentsMargins(
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN,
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN,
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN,
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN);
    containerLayout->setHorizontalSpacing(Theme::Spacing::SETTINGS_ROW_SPACING);
    containerLayout->setVerticalSpacing(Theme::Spacing::SETTINGS_ROW_SPACING);
    containerLayout->setColumnStretch(0, 1);
    containerLayout->setColumnStretch(1, 2);
    containerLayout->setRowStretch(26, 1);

    m_titleLabel = new QLabel(tr("Settings"), m_panel);
    m_titleLabel->setObjectName("settingsTitle");
    m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_titleLabel, 0, 0, 1, 2);

    m_languageLabel = new QLabel(tr("Language"), m_panel);
    m_languageLabel->setObjectName("settingsSectionHeader");
    m_languageLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_languageLabel, 1, 0, 1, 2);

    m_languageCombo = new QComboBox(m_panel);
    m_languageCombo->addItem(tr("System default"), QStringLiteral("system"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    const auto languages = TranslationManager::instance().availableLanguages();
    for (const auto &lang : languages) {
        m_languageCombo->addItem(lang.displayName, lang.code);
    }
    m_languageCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    containerLayout->addWidget(m_languageCombo, 2, 0, 1, 2);

    m_updaterHeader = new QLabel(tr("Updater"), m_panel);
    m_updaterHeader->setObjectName("settingsSectionHeader");
    m_updaterHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_updaterHeader, 3, 0, 1, 2);

    m_currentVersionLabel = new QLabel(tr("Current version"), m_panel);
    m_currentVersionLabel->setObjectName("settingsLabel");
    m_currentVersionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_currentVersionLabel, 4, 0);

    m_versionLabel = new QLabel(QStringLiteral("—"), m_panel);
    m_versionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_versionLabel, 4, 1);

    m_sourceLabel = new QLabel(tr("Source"), m_panel);
    m_sourceLabel->setObjectName("settingsLabel");
    m_sourceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_sourceLabel, 5, 0);

    m_updateSourceEdit = new QLineEdit(m_panel);
    m_updateSourceEdit->setPlaceholderText(QStringLiteral("https://github.com/Tapawingo/TrenchKit"));
    m_updateSourceEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_updateSourceEdit->setFocusPolicy(Qt::ClickFocus);
    containerLayout->addWidget(m_updateSourceEdit, 5, 1);

    m_channelLabel = new QLabel(tr("Update channel"), m_panel);
    m_channelLabel->setObjectName("settingsLabel");
    m_channelLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_channelLabel, 6, 0);

    m_updateChannelCombo = new QComboBox(m_panel);
    m_updateChannelCombo->addItem(tr("Stable"), QStringLiteral("stable"));
    m_updateChannelCombo->addItem(tr("Pre-release"), QStringLiteral("prerelease"));
    m_updateChannelCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    containerLayout->addWidget(m_updateChannelCombo, 6, 1);

    m_autoCheckLabel = new QLabel(tr("Check for updates on startup"), m_panel);
    m_autoCheckLabel->setObjectName("settingsLabel");
    m_autoCheckLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_autoCheckLabel, 7, 0);

    m_autoCheckCheckbox = new QCheckBox(m_panel);
    m_autoCheckCheckbox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_autoCheckCheckbox, 7, 1);

    m_downloadLabel = new QLabel(tr("Download location (optional)"), m_panel);
    m_downloadLabel->setObjectName("settingsLabel");
    m_downloadLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_downloadLabel, 8, 0);

    auto *downloadRow = new QHBoxLayout();
    m_downloadDirEdit = new QLineEdit(m_panel);
    m_downloadDirEdit->setPlaceholderText(tr("Default: AppData/Local/TrenchKit/updates"));
    m_downloadDirEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_downloadDirEdit->setFocusPolicy(Qt::ClickFocus);
    m_downloadBrowseButton = new QPushButton(tr("Browse..."), m_panel);
    m_downloadBrowseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_downloadBrowseButton->setCursor(Qt::PointingHandCursor);
    downloadRow->addWidget(m_downloadDirEdit);
    downloadRow->addWidget(m_downloadBrowseButton);
    containerLayout->addLayout(downloadRow, 8, 1);

    m_checkLabel = new QLabel(tr("Check for updates now"), m_panel);
    m_checkLabel->setObjectName("settingsLabel");
    m_checkLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_checkLabel, 9, 0);

    m_checkNowButton = new QPushButton(tr("Check now"), m_panel);
    m_checkNowButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_checkNowButton->setCursor(Qt::PointingHandCursor);
    containerLayout->addWidget(m_checkNowButton, 9, 1);

    m_checkStatusLabel = new QLabel(tr("Idle"), m_panel);
    m_checkStatusLabel->setObjectName("settingsStatus");
    m_checkStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_checkStatusLabel, 10, 1);

    m_shortcutsHeader = new QLabel(tr("Shortcuts"), m_panel);
    m_shortcutsHeader->setObjectName("settingsSectionHeader");
    m_shortcutsHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_shortcutsHeader, 11, 0, 1, 2);

    m_desktopShortcutLabel = new QLabel(tr("Desktop shortcut"), m_panel);
    m_desktopShortcutLabel->setObjectName("settingsLabel");
    m_desktopShortcutLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_desktopShortcutLabel, 12, 0);

    m_addDesktopShortcutButton = new QPushButton(tr("Add Desktop Shortcut"), m_panel);
    m_addDesktopShortcutButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_addDesktopShortcutButton->setCursor(Qt::PointingHandCursor);
    containerLayout->addWidget(m_addDesktopShortcutButton, 12, 1);

    m_startMenuLabel = new QLabel(tr("Start menu"), m_panel);
    m_startMenuLabel->setObjectName("settingsLabel");
    m_startMenuLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_startMenuLabel, 13, 0);

    m_addStartMenuShortcutButton = new QPushButton(tr("Add to Start Menu"), m_panel);
    m_addStartMenuShortcutButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_addStartMenuShortcutButton->setCursor(Qt::PointingHandCursor);
    containerLayout->addWidget(m_addStartMenuShortcutButton, 13, 1);

    m_nexusHeader = new QLabel(tr("Nexus Mods Integration"), m_panel);
    m_nexusHeader->setObjectName("settingsSectionHeader");
    m_nexusHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_nexusHeader, 14, 0, 1, 2);

    m_nexusConnectionLabel = new QLabel(tr("Connection status"), m_panel);
    m_nexusConnectionLabel->setObjectName("settingsLabel");
    m_nexusConnectionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_nexusConnectionLabel, 15, 0);

    m_nexusStatusLabel = new QLabel(tr("Not connected"), m_panel);
    m_nexusStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_nexusStatusLabel, 15, 1);

    m_nexusAuthLabel = new QLabel(tr("Authentication"), m_panel);
    m_nexusAuthLabel->setObjectName("settingsLabel");
    m_nexusAuthLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_nexusAuthLabel, 16, 0);

    auto *nexusButtonRow = new QHBoxLayout();
    m_nexusAuthButton = new QPushButton(tr("Authenticate (SSO)"), m_panel);
    m_nexusAuthButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_nexusAuthButton->setCursor(Qt::PointingHandCursor);
    m_nexusClearButton = new QPushButton(tr("Clear API Key"), m_panel);
    m_nexusClearButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_nexusClearButton->setCursor(Qt::PointingHandCursor);
    nexusButtonRow->addWidget(m_nexusAuthButton);
    nexusButtonRow->addWidget(m_nexusClearButton);
    nexusButtonRow->addStretch();
    containerLayout->addLayout(nexusButtonRow, 16, 1);

    m_itchHeader = new QLabel(tr("Itch.io Integration"), m_panel);
    m_itchHeader->setObjectName("settingsSectionHeader");
    m_itchHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_itchHeader, 17, 0, 1, 2);

    m_itchConnectionLabel = new QLabel(tr("Connection status"), m_panel);
    m_itchConnectionLabel->setObjectName("settingsLabel");
    m_itchConnectionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_itchConnectionLabel, 18, 0);

    m_itchStatusLabel = new QLabel(tr("Not connected"), m_panel);
    m_itchStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_itchStatusLabel, 18, 1);

    m_itchAuthLabel = new QLabel(tr("Authentication"), m_panel);
    m_itchAuthLabel->setObjectName("settingsLabel");
    m_itchAuthLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_itchAuthLabel, 19, 0);

    auto *itchButtonRow = new QHBoxLayout();
    m_itchAuthButton = new QPushButton(tr("Add API Key"), m_panel);
    m_itchAuthButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_itchAuthButton->setCursor(Qt::PointingHandCursor);
    m_itchClearButton = new QPushButton(tr("Clear API Key"), m_panel);
    m_itchClearButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_itchClearButton->setCursor(Qt::PointingHandCursor);
    itchButtonRow->addWidget(m_itchAuthButton);
    itchButtonRow->addWidget(m_itchClearButton);
    itchButtonRow->addStretch();
    containerLayout->addLayout(itchButtonRow, 19, 1);

    m_loggingHeader = new QLabel(tr("Logging"), m_panel);
    m_loggingHeader->setObjectName("settingsSectionHeader");
    m_loggingHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_loggingHeader, 20, 0, 1, 2);

    m_logLocationLabel = new QLabel(tr("Log files location"), m_panel);
    m_logLocationLabel->setObjectName("settingsLabel");
    m_logLocationLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_logLocationLabel, 21, 0);

    m_logPathLabel = new QLabel(Logger::instance().logDirectory(), m_panel);
    m_logPathLabel->setWordWrap(true);
    m_logPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_logPathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    containerLayout->addWidget(m_logPathLabel, 21, 1);

    m_logActionsLabel = new QLabel(tr("Access logs"), m_panel);
    m_logActionsLabel->setObjectName("settingsLabel");
    m_logActionsLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_logActionsLabel, 22, 0);

    auto *logButtonRow = new QHBoxLayout();
    m_openLogsButton = new QPushButton(tr("Open Log Folder"), m_panel);
    m_openLogsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_openLogsButton->setCursor(Qt::PointingHandCursor);
    m_copyLogPathButton = new QPushButton(tr("Copy Log Path"), m_panel);
    m_copyLogPathButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_copyLogPathButton->setCursor(Qt::PointingHandCursor);
    logButtonRow->addWidget(m_openLogsButton);
    logButtonRow->addWidget(m_copyLogPathButton);
    logButtonRow->addStretch();
    containerLayout->addLayout(logButtonRow, 22, 1);

    m_fileAssocHeader = new QLabel(tr("File Associations"), m_panel);
    m_fileAssocHeader->setObjectName("settingsSectionHeader");
    m_fileAssocHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_fileAssocHeader, 23, 0, 1, 2);

    m_tkprofileLabel = new QLabel(tr("Profile files (.tkprofile)"), m_panel);
    m_tkprofileLabel->setObjectName("settingsLabel");
    m_tkprofileLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_tkprofileLabel, 24, 0);

    m_tkprofileStatusLabel = new QLabel(tr("Not associated"), m_panel);
    m_tkprofileStatusLabel->setObjectName("settingsStatus");
    m_tkprofileStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerLayout->addWidget(m_tkprofileStatusLabel, 24, 1);

    m_tkprofileAssociateButton = new QPushButton(tr("Set as Default Handler"), m_panel);
    m_tkprofileAssociateButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_tkprofileAssociateButton->setCursor(Qt::PointingHandCursor);
    containerLayout->addWidget(m_tkprofileAssociateButton, 25, 1);

    scrollArea->setWidget(m_container);
    panelLayout->addWidget(scrollArea);

    m_footer = new GradientFrame(m_panel);
    m_footer->setObjectName("settingsFooter");
    m_footer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *footerLayout = new QHBoxLayout(m_footer);
    footerLayout->setContentsMargins(
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN,
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN,
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN,
        Theme::Spacing::SETTINGS_CONTAINER_MARGIN);
    footerLayout->setSpacing(0);

    m_settingsCancelButton = new QPushButton(tr("Cancel"), m_footer);
    m_settingsCancelButton->setCursor(Qt::PointingHandCursor);
    m_settingsSaveButton = new QPushButton(tr("Save"), m_footer);
    m_settingsSaveButton->setCursor(Qt::PointingHandCursor);
    footerLayout->addWidget(m_settingsCancelButton, 0, Qt::AlignLeft);
    footerLayout->addStretch();
    footerLayout->addWidget(m_settingsSaveButton, 0, Qt::AlignRight);

    panelLayout->addWidget(m_footer);
    overlayLayout->addWidget(m_panel);
    setCurrentVersion(m_updater ? m_updater->currentVersion().toString() : QString());

    connect(m_settingsCancelButton, &QPushButton::clicked,
            this, &SettingsWidget::cancelRequested);
    connect(m_settingsSaveButton, &QPushButton::clicked,
            this, &SettingsWidget::applySettings);
    connect(m_checkNowButton, &QPushButton::clicked, this, [this]() {
        QString owner;
        QString repo;
        if (!parseGithubRepo(m_updateSourceEdit ? m_updateSourceEdit->text().trimmed() : QString(),
                             &owner, &repo)) {
            if (m_modalManager) {
                MessageModal::warning(m_modalManager, tr("Updater Settings"),
                                     tr("Please enter a valid GitHub repository URL (e.g. https://github.com/owner/repo)."));
            }
            return;
        }
        if (m_updater) {
            const QString channel = m_updateChannelCombo
                ? m_updateChannelCombo->currentData().toString()
                : QStringLiteral("stable");
            m_updater->setRepository(owner, repo);
            m_updater->setIncludePrereleases(channel == "prerelease");
        }
        if (m_checkStatusLabel) {
            m_checkStatusLabel->setText(tr("Checking..."));
        }
        emit manualCheckRequested();
    });
    connect(m_downloadBrowseButton, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Download Folder"),
            m_downloadDirEdit ? m_downloadDirEdit->text() : QString());
        if (!dir.isEmpty() && m_downloadDirEdit) {
            m_downloadDirEdit->setText(dir);
        }
    });

    connect(m_openLogsButton, &QPushButton::clicked, this, &SettingsWidget::onOpenLogsClicked);
    connect(m_copyLogPathButton, &QPushButton::clicked, this, &SettingsWidget::onCopyLogPathClicked);
    connect(m_addDesktopShortcutButton, &QPushButton::clicked, this, &SettingsWidget::onAddDesktopShortcutClicked);
    connect(m_addStartMenuShortcutButton, &QPushButton::clicked, this, &SettingsWidget::onAddStartMenuShortcutClicked);
    connect(m_tkprofileAssociateButton, &QPushButton::clicked,
            this, &SettingsWidget::onAssociateTkprofileClicked);
}

void SettingsWidget::applySettings() {
    if (!m_updateSourceEdit || !m_updateChannelCombo || !m_autoCheckCheckbox) {
        emit cancelRequested();
        return;
    }

    const QString sourceText = m_updateSourceEdit->text().trimmed();
    QString owner;
    QString repo;
    if (!parseGithubRepo(sourceText, &owner, &repo)) {
        if (m_modalManager) {
            MessageModal::warning(m_modalManager, tr("Updater Settings"),
                                 tr("Please enter a valid GitHub repository URL (e.g. https://github.com/owner/repo)."));
        }
        return;
    }

    const QString channel = m_updateChannelCombo->currentData().toString();
    const bool includePrereleases = (channel == "prerelease");
    if (m_updater) {
        m_updater->setRepository(owner, repo);
        m_updater->setIncludePrereleases(includePrereleases);
    }

    QSettings settings("TrenchKit", "FoxholeModManager");
    settings.setValue("updater/source", sourceText);
    settings.setValue("updater/channel", channel);
    settings.setValue("updater/autoCheck", m_autoCheckCheckbox->isChecked());
    settings.setValue("updater/downloadDir",
                      m_downloadDirEdit ? m_downloadDirEdit->text() : QString());

    if (m_languageCombo) {
        const QString lang = m_languageCombo->currentData().toString();
        settings.setValue(QStringLiteral("app/language"), lang);
        TranslationManager::instance().setLanguage(lang);
    }

    emit settingsApplied(m_autoCheckCheckbox->isChecked());
}

void SettingsWidget::loadSettings(bool applyToUpdater) {
    QSettings settings("TrenchKit", "FoxholeModManager");
    const QString defaultSource = QStringLiteral("https://github.com/Tapawingo/TrenchKit");
    const QString source = settings.value("updater/source", defaultSource).toString();
    QString channel = settings.value("updater/channel", QString()).toString();
    if (channel.isEmpty()) {
        const bool legacyPrerelease = settings.value("updater/includePrereleases", false).toBool();
        channel = legacyPrerelease ? QStringLiteral("prerelease") : QStringLiteral("stable");
    }
    const bool includePrereleases = (channel == "prerelease");
    const bool autoCheck = settings.value("updater/autoCheck", true).toBool();
    const QString downloadDir = settings.value("updater/downloadDir", QString()).toString();

    if (m_updateSourceEdit) {
        m_updateSourceEdit->setText(source);
    }
    if (m_updateChannelCombo) {
        const int channelIndex = m_updateChannelCombo->findData(channel);
        if (channelIndex >= 0) {
            m_updateChannelCombo->setCurrentIndex(channelIndex);
        } else {
            m_updateChannelCombo->setCurrentIndex(0);
        }
    }
    if (m_autoCheckCheckbox) {
        m_autoCheckCheckbox->setChecked(autoCheck);
    }
    if (m_downloadDirEdit) {
        m_downloadDirEdit->setText(downloadDir);
    }

    if (applyToUpdater && m_updater) {
        QString owner;
        QString repo;
        if (parseGithubRepo(source, &owner, &repo)) {
            m_updater->setRepository(owner, repo);
        }
        m_updater->setIncludePrereleases(includePrereleases);
        setCurrentVersion(m_updater->currentVersion().toString());
    }

    if (m_languageCombo) {
        const QString lang = settings.value(QStringLiteral("app/language"), QStringLiteral("system")).toString();
        const int langIndex = m_languageCombo->findData(lang);
        if (langIndex >= 0) {
            m_languageCombo->setCurrentIndex(langIndex);
        } else {
            m_languageCombo->setCurrentIndex(0);
        }
    }

    if (m_tkprofileStatusLabel) {
        m_tkprofileStatusLabel->setText(isTkprofileAssociationSet()
            ? tr("Associated")
            : tr("Not associated"));
    }
}

bool SettingsWidget::parseGithubRepo(const QString &text, QString *owner, QString *repo) {
    if (!owner || !repo) return false;
    QUrl url(text);
    if (!url.isValid() || url.host().toLower() != QStringLiteral("github.com")) {
        return false;
    }
    const QStringList parts = url.path().split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }
    QString repoPart = parts.at(1);
    if (repoPart.endsWith(".git")) {
        repoPart.chop(4);
    }
    *owner = parts.at(0);
    *repo = repoPart;
    return !owner->isEmpty() && !repo->isEmpty();
}

void SettingsWidget::setNexusServices(NexusModsClient *client, NexusModsAuth *auth) {
    m_nexusClient = client;
    m_nexusAuth = auth;

    if (!m_nexusStatusLabel || !m_nexusAuthButton || !m_nexusClearButton) {
        return;
    }

    auto updateStatus = [this]() {
        if (m_nexusClient && m_nexusClient->hasApiKey()) {
            m_nexusStatusLabel->setText(QStringLiteral("Connected"));
            m_nexusAuthButton->setVisible(false);
            m_nexusClearButton->setVisible(true);
        } else {
            m_nexusStatusLabel->setText(QStringLiteral("Not connected"));
            m_nexusAuthButton->setVisible(true);
            m_nexusClearButton->setVisible(false);
        }
    };

    updateStatus();

    if (m_nexusAuth) {
        disconnect(m_nexusAuth, &NexusModsAuth::authenticationStarted, this, nullptr);
        disconnect(m_nexusAuth, &NexusModsAuth::authenticationComplete, this, nullptr);
        disconnect(m_nexusAuth, &NexusModsAuth::authenticationFailed, this, nullptr);

        connect(m_nexusAuth, &NexusModsAuth::authenticationStarted, this, [this](const QString &) {
            if (m_nexusStatusLabel) {
                m_nexusStatusLabel->setText(QStringLiteral("Authenticating... (check your browser)"));
            }
        });

        connect(m_nexusAuth, &NexusModsAuth::authenticationComplete, this, [this, updateStatus](const QString &apiKey) {
            if (m_nexusClient) {
                m_nexusClient->setApiKey(apiKey);
            }
            if (m_nexusStatusLabel) {
                m_nexusStatusLabel->setText(QStringLiteral("Connected"));
            }
            updateStatus();
            if (m_modalManager) {
                MessageModal::information(m_modalManager, QStringLiteral("Success"),
                                       QStringLiteral("Successfully authenticated with Nexus Mods!"));
            }
        });

        connect(m_nexusAuth, &NexusModsAuth::authenticationFailed, this, [this, updateStatus](const QString &error) {
            updateStatus();
            if (m_modalManager) {
                MessageModal::warning(m_modalManager, QStringLiteral("Authentication Failed"), error);
            }
        });
    }

    if (m_nexusAuthButton) {
        connect(m_nexusAuthButton, &QPushButton::clicked, this, [this]() {
            if (!m_nexusAuth) {
                return;
            }

            // Create a one-time connection to open browser for Settings-initiated auth
            QMetaObject::Connection *conn = new QMetaObject::Connection();
            *conn = connect(m_nexusAuth, &NexusModsAuth::authenticationStarted, this, [conn](const QString &url) {
                QDesktopServices::openUrl(QUrl(url));
                QObject::disconnect(*conn);
                delete conn;
            });
            m_nexusAuth->startAuthentication();
        });
    }

    if (m_nexusClearButton) {
        connect(m_nexusClearButton, &QPushButton::clicked, this, [this, updateStatus]() {
            if (!m_modalManager) {
                return;
            }

            auto *modal = new MessageModal(
                QStringLiteral("Clear API Key"),
                QStringLiteral("Are you sure you want to clear your Nexus Mods API key?"),
                MessageModal::Question,
                MessageModal::Yes | MessageModal::No
            );
            connect(modal, &MessageModal::finished, this, [this, modal, updateStatus]() {
                if (modal->clickedButton() == MessageModal::Yes && m_nexusClient) {
                    m_nexusClient->clearApiKey();
                    updateStatus();
                }
            });
            m_modalManager->showModal(modal);
        });
    }
}

void SettingsWidget::setItchServices(ItchClient *client, ItchAuth *auth) {
    m_itchClient = client;
    m_itchAuth = auth;

    auto updateStatus = [this]() {
        if (m_itchClient && m_itchClient->hasApiKey()) {
            m_itchStatusLabel->setText(QStringLiteral("Connected"));
            m_itchAuthButton->setVisible(false);
            m_itchClearButton->setVisible(true);
        } else {
            m_itchStatusLabel->setText(QStringLiteral("Not connected"));
            m_itchAuthButton->setVisible(true);
            m_itchClearButton->setVisible(false);
        }
    };

    updateStatus();

    if (m_itchAuthButton) {
        connect(m_itchAuthButton, &QPushButton::clicked, this, [this, updateStatus]() {
            if (!m_modalManager) {
                return;
            }

            auto *inputModal = new InputModal(
                QStringLiteral("Enter API Key"),
                QStringLiteral("Enter your itch.io API key:\n\n"
                             "1. Visit https://itch.io/user/settings/api-keys\n"
                             "2. Click \"Generate new API key\"\n"
                             "3. Copy the key and paste it below"),
                QString()
            );

            connect(inputModal, &InputModal::accepted, this, [this, inputModal, updateStatus]() {
                QString apiKey = inputModal->textValue().trimmed();
                if (!apiKey.isEmpty()) {
                    if (m_itchClient) {
                        m_itchClient->setApiKey(apiKey);
                        updateStatus();
                        if (m_modalManager) {
                            MessageModal::information(m_modalManager,
                                                   QStringLiteral("Success"),
                                                   QStringLiteral("API key saved successfully!"));
                        }
                    }
                }
            });

            m_modalManager->showModal(inputModal);
        });
    }

    if (m_itchClearButton) {
        connect(m_itchClearButton, &QPushButton::clicked, this, [this, updateStatus]() {
            if (!m_modalManager) {
                return;
            }

            auto *modal = new MessageModal(
                QStringLiteral("Clear API Key"),
                QStringLiteral("Are you sure you want to clear your itch.io API key?"),
                MessageModal::Question,
                MessageModal::Yes | MessageModal::No
            );

            connect(modal, &MessageModal::finished, this, [this, modal, updateStatus]() {
                if (modal->clickedButton() == MessageModal::Yes && m_itchClient) {
                    m_itchClient->clearApiKey();
                    updateStatus();
                }
            });

            m_modalManager->showModal(modal);
        });
    }
}

void SettingsWidget::onOpenLogsClicked() {
    QString logDir = Logger::instance().logDirectory();
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
}

void SettingsWidget::onCopyLogPathClicked() {
    QString logPath = Logger::instance().currentLogFile();
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(logPath);

    if (m_copyLogPathButton) {
        m_copyLogPathButton->setText(tr("Copied!"));
        QTimer::singleShot(2000, this, [this]() {
            if (m_copyLogPathButton) {
                m_copyLogPathButton->setText(tr("Copy Log Path"));
            }
        });
    }
}

bool SettingsWidget::createShortcut(const QString &shortcutPath, const QString &targetPath, const QString &description) {
#ifdef Q_OS_WIN
    CoInitialize(nullptr);

    IShellLinkW *shellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&shellLink);

    if (FAILED(hr) || !shellLink) {
        CoUninitialize();
        return false;
    }

    shellLink->SetPath(reinterpret_cast<const wchar_t*>(targetPath.utf16()));
    shellLink->SetDescription(reinterpret_cast<const wchar_t*>(description.utf16()));

    QString workingDir = QFileInfo(targetPath).absolutePath();
    shellLink->SetWorkingDirectory(reinterpret_cast<const wchar_t*>(workingDir.utf16()));

    IPersistFile *persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&persistFile);

    bool success = false;
    if (SUCCEEDED(hr) && persistFile) {
        hr = persistFile->Save(reinterpret_cast<const wchar_t*>(shortcutPath.utf16()), TRUE);
        success = SUCCEEDED(hr);
        persistFile->Release();
    }

    shellLink->Release();
    CoUninitialize();

    return success;
#elif defined(Q_OS_LINUX)
    const QString content =
        QStringLiteral("[Desktop Entry]\nVersion=1.0\nType=Application\n"
                       "Name=TrenchKit\nComment=") + description +
        QStringLiteral("\nExec=\"") + targetPath +
        QStringLiteral("\"\nIcon=io.github.tapawingo.trenchkit\n"
                       "Terminal=false\nCategories=Game;\n");
    QFile f(shortcutPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(content.toUtf8());
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser);
    return true;
#else
    Q_UNUSED(shortcutPath);
    Q_UNUSED(targetPath);
    Q_UNUSED(description);
    return false;
#endif
}

void SettingsWidget::onAddDesktopShortcutClicked() {
#ifdef Q_OS_WIN
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString shortcutPath = desktopPath + "/TrenchKit.lnk";

    QString exePath = QCoreApplication::applicationFilePath();

    if (QFile::exists(shortcutPath)) {
        if (m_modalManager) {
            MessageModal::information(m_modalManager, tr("Shortcut Already Exists"),
                tr("A desktop shortcut for TrenchKit already exists."));
        }
        return;
    }

    bool success = createShortcut(shortcutPath, exePath, QStringLiteral("TrenchKit - Foxhole Mod Manager"));

    if (m_modalManager) {
        if (success) {
            MessageModal::information(m_modalManager, tr("Success"),
                tr("Desktop shortcut created successfully."));
        } else {
            MessageModal::warning(m_modalManager, tr("Error"),
                tr("Failed to create desktop shortcut."));
        }
    }
#elif defined(Q_OS_LINUX)
    const QString desktopPath =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
        + QStringLiteral("/TrenchKit.desktop");
    const QString appImagePath0 = qEnvironmentVariable("APPIMAGE");
    const QString exePath = appImagePath0.isEmpty()
        ? QCoreApplication::applicationFilePath()
        : appImagePath0;
    if (QFile::exists(desktopPath)) {
        if (m_modalManager)
            MessageModal::information(m_modalManager, tr("Shortcut Already Exists"),
                tr("A desktop shortcut for TrenchKit already exists."));
        return;
    }
    const bool linuxSuccess = createShortcut(desktopPath, exePath,
                                             QStringLiteral("TrenchKit - Foxhole Mod Manager"));
    if (m_modalManager) {
        if (linuxSuccess)
            MessageModal::information(m_modalManager, tr("Success"),
                tr("Desktop shortcut created successfully."));
        else
            MessageModal::warning(m_modalManager, tr("Error"),
                tr("Failed to create desktop shortcut."));
    }
#else
    if (m_modalManager) {
        MessageModal::information(m_modalManager, tr("Not Supported"),
            tr("Shortcut creation is only supported on Windows."));
    }
#endif
}

void SettingsWidget::onAddStartMenuShortcutClicked() {
#ifdef Q_OS_WIN
    QString startMenuPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(startMenuPath + "/TrenchKit");
    QString shortcutPath = startMenuPath + "/TrenchKit/TrenchKit.lnk";

    QString exePath = QCoreApplication::applicationFilePath();

    if (QFile::exists(shortcutPath)) {
        if (m_modalManager) {
            MessageModal::information(m_modalManager, tr("Shortcut Already Exists"),
                tr("A Start Menu shortcut for TrenchKit already exists."));
        }
        return;
    }

    bool success = createShortcut(shortcutPath, exePath, QStringLiteral("TrenchKit - Foxhole Mod Manager"));

    if (m_modalManager) {
        if (success) {
            MessageModal::information(m_modalManager, tr("Success"),
                tr("Start Menu shortcut created successfully."));
        } else {
            MessageModal::warning(m_modalManager, tr("Error"),
                tr("Failed to create Start Menu shortcut."));
        }
    }
#elif defined(Q_OS_LINUX)
    const QString appsPath =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(appsPath);
    const QString shortcutPath = appsPath + QStringLiteral("/TrenchKit.desktop");
    const QString appImagePath1 = qEnvironmentVariable("APPIMAGE");
    const QString exePath = appImagePath1.isEmpty()
        ? QCoreApplication::applicationFilePath()
        : appImagePath1;
    if (QFile::exists(shortcutPath)) {
        if (m_modalManager)
            MessageModal::information(m_modalManager, tr("Shortcut Already Exists"),
                tr("An application menu shortcut for TrenchKit already exists."));
        return;
    }
    const bool linuxSuccess = createShortcut(shortcutPath, exePath,
                                             QStringLiteral("TrenchKit - Foxhole Mod Manager"));
    if (m_modalManager) {
        if (linuxSuccess)
            MessageModal::information(m_modalManager, tr("Success"),
                tr("Application menu shortcut created successfully."));
        else
            MessageModal::warning(m_modalManager, tr("Error"),
                tr("Failed to create application menu shortcut."));
    }
#else
    if (m_modalManager) {
        MessageModal::information(m_modalManager, tr("Not Supported"),
            tr("Shortcut creation is only supported on Windows."));
    }
#endif
}

void SettingsWidget::onAssociateTkprofileClicked() {
#ifdef Q_OS_WIN
    if (registerTkprofileAssociation()) {
        if (m_tkprofileStatusLabel) {
            m_tkprofileStatusLabel->setText(tr("Associated"));
        }
        if (m_modalManager) {
            MessageModal::information(m_modalManager, tr("File Association"),
                tr("TrenchKit is now the default handler for .tkprofile files."));
        }
    } else if (m_modalManager) {
        MessageModal::warning(m_modalManager, tr("File Association"),
            tr("Failed to register .tkprofile file association."));
    }
#else
    if (m_modalManager) {
        MessageModal::information(m_modalManager, tr("Not Supported"),
            tr("File associations are only supported on Windows."));
    }
#endif
}

bool SettingsWidget::isTkprofileAssociationSet() const {
#ifdef Q_OS_WIN
    QSettings classes("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    classes.beginGroup(".tkprofile");
    const QString progId = classes.value(".").toString();
    classes.endGroup();
    return progId == QStringLiteral("TrenchKit.Profile");
#elif defined(Q_OS_LINUX)
    return QFileInfo(
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/mime/packages/application-x-tkprofile.xml"
    ).exists();
#else
    return false;
#endif
}

bool SettingsWidget::registerTkprofileAssociation() {
#ifdef Q_OS_WIN
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString command = QString("\"%1\" \"%2\"").arg(exePath, "%1");

    QSettings classes("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    classes.beginGroup(".tkprofile");
    classes.setValue(".", "TrenchKit.Profile");
    classes.endGroup();

    classes.beginGroup("TrenchKit.Profile");
    classes.setValue(".", "TrenchKit Profile");
    classes.endGroup();

    classes.beginGroup("TrenchKit.Profile\\DefaultIcon");
    classes.setValue(".", exePath + ",0");
    classes.endGroup();

    classes.beginGroup("TrenchKit.Profile\\shell\\open\\command");
    classes.setValue(".", command);
    classes.endGroup();

    classes.sync();
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return classes.status() == QSettings::NoError;
#elif defined(Q_OS_LINUX)
    // 1. Write MIME type XML
    const QString mimeDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/mime/packages";
    if (!QDir().mkpath(mimeDir))
        return false;
    const QString mimeXml = mimeDir + "/application-x-tkprofile.xml";
    QFile f(mimeXml);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
            "  <mime-type type=\"application/x-tkprofile\">\n"
            "    <comment>TrenchKit Profile</comment>\n"
            "    <glob pattern=\"*.tkprofile\"/>\n"
            "  </mime-type>\n"
            "</mime-info>\n");
    f.close();
    QProcess::execute(QStringLiteral("update-mime-database"),
                      { QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + "/mime" });

    // 2. Create/update .desktop file with MimeType
    const QString appsDir =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(appsDir);
    const QString desktopPath = appsDir + "/TrenchKit.desktop";
    const QString appImagePath = qEnvironmentVariable("APPIMAGE");
    const QString exePath = appImagePath.isEmpty()
        ? QCoreApplication::applicationFilePath()
        : appImagePath;
    const QString desktop =
        QStringLiteral("[Desktop Entry]\nVersion=1.0\nType=Application\n"
                       "Name=TrenchKit\n"
                       "Comment=TrenchKit - Foxhole Mod Manager\n"
                       "Exec=\"") + exePath + QStringLiteral("\" \"%f\"\n"
                       "Icon=io.github.tapawingo.trenchkit\n"
                       "Terminal=false\nCategories=Game;\n"
                       "MimeType=application/x-tkprofile;\n");
    QFile df(desktopPath);
    if (df.open(QIODevice::WriteOnly | QIODevice::Text)) {
        df.write(desktop.toUtf8());
        df.close();
        df.setPermissions(df.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser);
    }
    QProcess::execute(QStringLiteral("update-desktop-database"), { appsDir });
    return true;
#else
    return false;
#endif
}
