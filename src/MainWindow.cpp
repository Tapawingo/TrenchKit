#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "views/mod_manager/InstallPathWidget.h"
#include "views/mod_manager/ProfileManagerWidget.h"
#include "views/mod_manager/ModListWidget.h"
#include "views/mod_manager/RightPanelWidget.h"
#include "views/mod_manager/ActivityLogWidget.h"
#include "views/mod_manager/BackupWidget.h"
#include "views/mod_manager/LaunchWidget.h"
#include "views/settings/SettingsWidget.h"
#include "core/utils/FoxholeDetector.h"
#include "core/utils/UpdateArchiveExtractor.h"
#include "core/managers/ModManager.h"
#include "core/managers/ProfileManager.h"
#include "core/utils/Theme.h"
#include "common/modals/MessageModal.h"
#include <QMessageBox>
#include <QSettings>
#include <QShowEvent>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QEvent>
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMetaObject>
#include <QShortcut>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#else
#include <QWindow>
#endif

namespace {
QString defaultUpdatesDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base).filePath("updates");
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_installPathWidget(new InstallPathWidget(this))
    , m_profileManagerWidget(new ProfileManagerWidget(this))
    , m_modListWidget(new ModListWidget(this))
    , m_rightPanelWidget(new RightPanelWidget(this))
    , m_modManager(new ModManager(this))
    , m_profileManager(new ProfileManager(this))
    , m_modLoadWatcher(new QFutureWatcher<bool>(this))
    , m_unregisteredModsWatcher(new QFutureWatcher<void>(this))
    , m_updater(new UpdaterService("Tapawingo", "TrenchKit", this))
    , m_nexusClient(new NexusModsClient(this))
    , m_nexusAuth(new NexusModsAuth(this))
    , m_itchClient(new ItchClient(this))
    , m_itchAuth(new ItchAuth(this))
    , m_modUpdateService(new ModUpdateService(m_modManager, m_nexusClient, this))
    , m_itchUpdateService(new ItchModUpdateService(m_modManager, m_itchClient, this))
    , m_modalManager(new ModalManager(this))
{
    ui->setupUi(this);

    m_pendingProfileImportPath = findProfileImportPath();

    setWindowIcon(QIcon(":/icon.png"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setStyleSheet(Theme::getStyleSheet());

    m_backgroundTexture = QPixmap(":/tex_main.png");

    setupTitleBar();
    setupInstallPath();
    setupProfileManager();
    setupModList();
    setupRightPanel();
    setupSettingsOverlay();
    m_globalSearchShortcut = new QShortcut(QKeySequence::Find, this);
    m_globalSearchShortcut->setContext(Qt::ApplicationShortcut);
    connect(m_globalSearchShortcut, &QShortcut::activated, this, [this]() {
        if (!m_modListWidget || !m_modalManager) {
            return;
        }
        if (m_modalManager->hasOpenModal()) {
            return;
        }
        if (ui->bodyStack && m_settingsPage &&
            ui->bodyStack->currentWidget() == m_settingsPage) {
            return;
        }
        m_modListWidget->activateSearch();
    });

    connect(m_updater, &UpdaterService::updateAvailable,
            this, &MainWindow::onUpdateAvailable);
    connect(m_updater, &UpdaterService::upToDate,
            this, &MainWindow::onUpdateUpToDate);
    connect(m_updater, &UpdaterService::downloadProgress,
            this, &MainWindow::onUpdateDownloadProgress);
    connect(m_updater, &UpdaterService::downloadFinished,
            this, &MainWindow::onUpdateDownloadFinished);
    connect(m_updater, &UpdaterService::errorOccurred,
            this, &MainWindow::onUpdateCheckError);
    connect(m_updater, &UpdaterService::checkingStarted, this, [this]() {
        if (m_settingsWidget) {
            m_settingsWidget->setCheckStatus(tr("Checking..."));
        }
    });

    ui->hbox->setStretch(0, 1);
    ui->hbox->setStretch(1, 2);
    ui->hbox->setStretch(2, 1);

    ui->centralwidget->installEventFilter(this);
    ui->centralwidget->setAutoFillBackground(true);

    connect(m_modLoadWatcher, &QFutureWatcher<bool>::finished,
            this, &MainWindow::onModsLoadComplete);
    connect(m_unregisteredModsWatcher, &QFutureWatcher<void>::finished,
            this, &MainWindow::onUnregisteredModsDetectionComplete);

    m_verificationWatcher = new QFutureWatcher<QList<ModManager::VerificationIssue>>(this);
    connect(m_verificationWatcher,
            &QFutureWatcher<QList<ModManager::VerificationIssue>>::finished,
            this, &MainWindow::onVerificationComplete);

    m_syncWatcher = new QFutureWatcher<void>(this);

    setMinimumSize(800, 560);
    resize(1000, 700);
#ifndef Q_OS_WIN
    setMouseTracking(true);
#endif

    // Add initial log entry
    ActivityLogWidget * const log = m_rightPanelWidget->getActivityLog();
    log->addLogEntry(tr("TrenchKit Started"), ActivityLogWidget::LogLevel::Info);

    QTimer::singleShot(150, this, &MainWindow::startUpdateCheck);
}

MainWindow::~MainWindow() {
    saveSettings();
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        m_firstShow = false;
        QTimer::singleShot(0, this, &MainWindow::loadSettings);
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        ui->titleBar->setMaximized(isMaximized());
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->centralwidget) {
        if (event->type() == QEvent::Paint) {
            QPaintEvent *paintEvent = static_cast<QPaintEvent*>(event);
            Q_UNUSED(paintEvent);
            QPainter painter(ui->centralwidget);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);

            if (!m_backgroundTexture.isNull()) {
                QRect widgetRect = ui->centralwidget->rect();

                if (widgetRect.size() != m_lastSize) {
                    m_cachedScaledTexture = m_backgroundTexture.scaled(
                        widgetRect.size(),
                        Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation
                    );
                    m_lastSize = widgetRect.size();
                }

                int x = (widgetRect.width() - m_cachedScaledTexture.width()) / 2;
                int y = (widgetRect.height() - m_cachedScaledTexture.height()) / 2;

                painter.drawPixmap(x, y, m_cachedScaledTexture);
            }

            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupTitleBar() {
    ui->titleBar->setUpdateVisible(false);

    connect(ui->titleBar, &TitleBar::minimizeClicked, this, &MainWindow::onMinimizeClicked);
    connect(ui->titleBar, &TitleBar::maximizeClicked, this, &MainWindow::onMaximizeClicked);
    connect(ui->titleBar, &TitleBar::closeClicked, this, &MainWindow::onCloseClicked);
    connect(ui->titleBar, &TitleBar::updateClicked, this, &MainWindow::onUpdateClicked);
    connect(ui->titleBar, &TitleBar::settingsClicked, this, &MainWindow::onSettingsClicked);
}

void MainWindow::onMinimizeClicked() {
    showMinimized();
}

void MainWindow::onMaximizeClicked() {
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

#ifndef Q_OS_WIN
static Qt::Edges resizeEdges(const QPoint &pos, const QRect &r, int b) {
    Qt::Edges e;
    if (pos.x() < b)               e |= Qt::LeftEdge;
    if (pos.x() > r.width()  - b)  e |= Qt::RightEdge;
    if (pos.y() < b)               e |= Qt::TopEdge;
    if (pos.y() > r.height() - b)  e |= Qt::BottomEdge;
    return e;
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !isMaximized()) {
        const Qt::Edges edges = resizeEdges(event->pos(), rect(), 8);
        if (edges && windowHandle()) {
            windowHandle()->startSystemResize(edges);
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!isMaximized()) {
        const Qt::Edges e = resizeEdges(event->pos(), rect(), 8);
        if      ((e & Qt::LeftEdge  && e & Qt::TopEdge)    ||
                 (e & Qt::RightEdge && e & Qt::BottomEdge)) setCursor(Qt::SizeFDiagCursor);
        else if ((e & Qt::RightEdge && e & Qt::TopEdge)    ||
                 (e & Qt::LeftEdge  && e & Qt::BottomEdge)) setCursor(Qt::SizeBDiagCursor);
        else if (e & (Qt::LeftEdge | Qt::RightEdge))        setCursor(Qt::SizeHorCursor);
        else if (e & (Qt::TopEdge  | Qt::BottomEdge))       setCursor(Qt::SizeVerCursor);
        else                                                 unsetCursor();
    }
    QMainWindow::mouseMoveEvent(event);
}
#endif

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const qreal dpr = devicePixelRatio();
            const int gx = static_cast<int>(GET_X_LPARAM(msg->lParam) / dpr);
            const int gy = static_cast<int>(GET_Y_LPARAM(msg->lParam) / dpr);
            const QRect r = frameGeometry();
            constexpr int B = 8;

            const bool l   = gx >= r.left()      && gx <  r.left()   + B;
            const bool rr  = gx >  r.right() - B && gx <= r.right();
            const bool t   = gy >= r.top()        && gy <  r.top()    + B;
            const bool bot = gy >  r.bottom() - B && gy <= r.bottom();

            if      (t   && l)  { *result = HTTOPLEFT;     return true; }
            else if (t   && rr) { *result = HTTOPRIGHT;    return true; }
            else if (bot && l)  { *result = HTBOTTOMLEFT;  return true; }
            else if (bot && rr) { *result = HTBOTTOMRIGHT; return true; }
            else if (l)         { *result = HTLEFT;        return true; }
            else if (rr)        { *result = HTRIGHT;       return true; }
            else if (t)         { *result = HTTOP;         return true; }
            else if (bot)       { *result = HTBOTTOM;      return true; }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::onCloseClicked() {
    close();
}

void MainWindow::startUpdateCheck() {
    if (!m_updater) return;
    if (!m_settingsWidget || !m_settingsWidget->applyStoredSettings()) {
        return;
    }
    m_updater->checkForUpdates();
}

void MainWindow::setupInstallPath() {
    ui->leftBox->addWidget(m_installPathWidget);
    connect(m_installPathWidget, &InstallPathWidget::validPathSelected,
            this, &MainWindow::onInstallPathChanged);
}

void MainWindow::setupProfileManager() {
    m_profileManager->setModManager(m_modManager);
    m_profileManagerWidget->setProfileManager(m_profileManager);
    m_profileManagerWidget->setModalManager(m_modalManager);
    ui->leftBox->addWidget(m_profileManagerWidget);

    ActivityLogWidget *log = m_rightPanelWidget->getActivityLog();

    connect(m_profileManagerWidget, &ProfileManagerWidget::profileLoadRequested,
            this, [this, log](const QString &) {
        m_modListWidget->refreshModList();
        log->addLogEntry(tr("Profile Loaded"), ActivityLogWidget::LogLevel::Info);
    });

    connect(m_profileManager, &ProfileManager::errorOccurred,
            this, [this](const QString &error) {
        qWarning() << "ProfileManager:" << error;
        MessageModal::warning(m_modalManager, tr("Profile Error"), error);
    });
}

void MainWindow::setupModList() {
    ui->middleBox->addWidget(m_modListWidget);
    m_modListWidget->setModManager(m_modManager);
    m_modListWidget->setModalManager(m_modalManager);
    m_modListWidget->setNexusServices(m_nexusClient, m_nexusAuth);
    m_modListWidget->setItchServices(m_itchClient, m_itchAuth);
    m_modListWidget->setUpdateService(m_modUpdateService);
    m_modListWidget->setItchUpdateService(m_itchUpdateService);

    connect(m_modListWidget, &ModListWidget::verifyModsRequested,
            this, &MainWindow::runModVerification);
}

void MainWindow::setupRightPanel() {
    ui->rightBox->addWidget(m_rightPanelWidget);
    m_rightPanelWidget->setModManager(m_modManager);
    m_rightPanelWidget->setModalManager(m_modalManager);

    connect(m_modListWidget, &ModListWidget::modSelectionChanged,
            m_rightPanelWidget, &RightPanelWidget::onModSelectionChanged);

    connect(m_rightPanelWidget, &RightPanelWidget::addModRequested,
            m_modListWidget, &ModListWidget::onAddModClicked);
    connect(m_rightPanelWidget, &RightPanelWidget::removeModRequested,
            m_modListWidget, &ModListWidget::onRemoveModClicked);
    connect(m_rightPanelWidget, &RightPanelWidget::moveUpRequested,
            m_modListWidget, &ModListWidget::onMoveUpClicked);
    connect(m_rightPanelWidget, &RightPanelWidget::moveDownRequested,
            m_modListWidget, &ModListWidget::onMoveDownClicked);

    connect(m_installPathWidget, &InstallPathWidget::validPathSelected,
            m_rightPanelWidget, &RightPanelWidget::setFoxholeInstallPath);

    connect(m_rightPanelWidget, &RightPanelWidget::errorOccurred,
            this, [this](const QString &error) {
        qWarning() << "RightPanel:" << error;
        MessageModal::warning(m_modalManager, tr("Error"), error);
    });

    // Connect activity log
    ActivityLogWidget *log = m_rightPanelWidget->getActivityLog();

    // ModListWidget logging
    connect(m_modListWidget, &ModListWidget::modAdded, this, [log](const QString &name) {
        log->addLogEntry(tr("Mod Added: %1").arg(name), ActivityLogWidget::LogLevel::Success);
    });
    connect(m_modListWidget, &ModListWidget::modRemoved, this, [log](const QString &name) {
        log->addLogEntry(tr("Mod Removed: %1").arg(name), ActivityLogWidget::LogLevel::Info);
    });
    connect(m_modListWidget, &ModListWidget::modReordered, this, [log]() {
        log->addLogEntry(tr("Mods Reordered"), ActivityLogWidget::LogLevel::Info);
    });

    // BackupWidget logging
    connect(m_rightPanelWidget->findChild<BackupWidget*>(), &BackupWidget::backupCreated,
            this, [log](int fileCount) {
        log->addLogEntry(tr("Backup Created: %1 files").arg(fileCount),
                        ActivityLogWidget::LogLevel::Success);
    });
    connect(m_rightPanelWidget->findChild<BackupWidget*>(), &BackupWidget::backupRestored,
            this, [this, log](const QString &backupName) {
        log->addLogEntry(tr("Backup Restored: %1").arg(backupName), ActivityLogWidget::LogLevel::Success);
        if (m_modManager && m_modManager->loadMods()) {
            m_modManager->syncEnabledModsWithPaks();
        }
        if (m_modListWidget) {
            m_modListWidget->refreshModList();
            m_modListWidget->rescanConflicts();
        }
    });

    // LaunchWidget logging
    connect(m_rightPanelWidget->findChild<LaunchWidget*>(), &LaunchWidget::gameLaunched,
            this, [log](bool withMods) {
        QString mode = withMods ? tr("With Mods") : tr("Vanilla");
        log->addLogEntry(tr("Game Launched: %1").arg(mode), ActivityLogWidget::LogLevel::Info);
    });
    connect(m_rightPanelWidget->findChild<LaunchWidget*>(), &LaunchWidget::modsRestored,
            this, [log](int modCount) {
        log->addLogEntry(tr("Mods Restored: %1 mods").arg(modCount),
                        ActivityLogWidget::LogLevel::Success);
    });
}

void MainWindow::setupSettingsOverlay() {
    m_settingsPage = ui->settingsPage;
    if (!m_settingsPage) {
        return;
    }
    auto *overlayLayout = ui->settingsLayout;
    if (!overlayLayout) {
        overlayLayout = new QVBoxLayout(m_settingsPage);
    }
    m_settingsWidget = new SettingsWidget(m_settingsPage, m_updater);
    m_settingsWidget->setNexusServices(m_nexusClient, m_nexusAuth);
    m_settingsWidget->setItchServices(m_itchClient, m_itchAuth);
    m_settingsWidget->setModalManager(m_modalManager);
    overlayLayout->addWidget(m_settingsWidget);

    connect(m_settingsWidget, &SettingsWidget::cancelRequested,
            this, &MainWindow::hideSettingsOverlay);
    connect(m_settingsWidget, &SettingsWidget::settingsApplied,
            this, &MainWindow::onSettingsApplied);
    connect(m_settingsWidget, &SettingsWidget::manualCheckRequested, this, [this]() {
        if (m_updater) {
            m_updater->checkForUpdates();
        }
    });
}

void MainWindow::loadSettings() {
    QSettings settings("TrenchKit", "FoxholeModManager");

    m_modListWidget->setLoadingState(true, tr("Loading mods"));

    QFuture<bool> modLoadFuture = QtConcurrent::run([this]() {
        return m_modManager->loadMods();
    });
    m_modLoadWatcher->setFuture(modLoadFuture);

    QFuture<bool> profileLoadFuture = QtConcurrent::run([this]() {
        return m_profileManager->loadProfiles();
    });

    auto *profileWatcher = new QFutureWatcher<bool>(this);
    connect(profileWatcher, &QFutureWatcher<bool>::finished, this, [this, profileWatcher]() {
        bool success = profileWatcher->result();
        profileWatcher->deleteLater();

        if (success) {
            m_profileManagerWidget->refreshProfileList();
        }
    });
    profileWatcher->setFuture(profileLoadFuture);

    QString savedPath = settings.value("foxholeInstallPath").toString();

    if (!savedPath.isEmpty()) {
        m_modManager->setInstallPath(savedPath);

        QFuture<bool> validationFuture = QtConcurrent::run([savedPath]() {
            return FoxholeDetector::isValidInstallPath(savedPath);
        });

        auto *watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, savedPath]() {
            bool isValid = watcher->result();
            watcher->deleteLater();

            if (isValid) {
                m_installPathWidget->setInstallPath(savedPath);

                m_modListWidget->setLoadingState(true, tr("Detecting mods"));
                QFuture<void> detectionFuture = QtConcurrent::run([this]() {
                    m_modManager->detectUnregisteredMods();
                });
                m_unregisteredModsWatcher->setFuture(detectionFuture);
            } else {
                m_installPathWidget->startAutoDetection();
            }
        });
        watcher->setFuture(validationFuture);
    } else {
        m_installPathWidget->startAutoDetection();
    }
}

void MainWindow::saveSettings() {
    QSettings settings("TrenchKit", "FoxholeModManager");
    settings.setValue("foxholeInstallPath", m_installPathWidget->installPath());
}

void MainWindow::onInstallPathChanged(const QString &path) {
    QSettings settings("TrenchKit", "FoxholeModManager");
    settings.setValue("foxholeInstallPath", path);

    m_modManager->setInstallPath(path);
    m_installPathReady = false;

    ActivityLogWidget *log = m_rightPanelWidget->getActivityLog();
    log->addLogEntry(tr("Install Path Updated"), ActivityLogWidget::LogLevel::Info);

    m_modListWidget->setLoadingState(true, tr("Detecting mods"));
    QFuture<void> detectionFuture = QtConcurrent::run([this]() {
        m_modManager->detectUnregisteredMods();
    });
    m_unregisteredModsWatcher->setFuture(detectionFuture);
}

void MainWindow::onModsLoadComplete() {
    bool success = m_modLoadWatcher->result();

    m_modListWidget->setLoadingState(false);

    if (success) {
        m_modsLoaded = true;
        m_modListWidget->refreshModList();

        if (m_modUpdateService) {
            QTimer::singleShot(0, this, [this]() {
                m_modUpdateService->checkAllModsForUpdates();
            });
        }
        if (m_itchUpdateService) {
            QTimer::singleShot(0, this, [this]() {
                m_itchUpdateService->checkAllModsForUpdates();
            });
        }

        if (!m_pendingProfileImportPath.isEmpty() && m_profileManagerWidget) {
            const QString importPath = m_pendingProfileImportPath;
            m_pendingProfileImportPath.clear();
            QTimer::singleShot(0, this, [this, importPath]() {
                m_profileManagerWidget->importProfileFromPath(importPath);
            });
        }
    }

    trySyncEnabledMods();
    runModVerification();
}

void MainWindow::runModVerification() {
    if (!m_verificationWatcher || m_verificationWatcher->isRunning()) return;
    QPointer<ModManager> mgr(m_modManager);
    m_verificationWatcher->setFuture(QtConcurrent::run([mgr]() {
        return mgr ? mgr->verifyMods() : QList<ModManager::VerificationIssue>{};
    }));
}

void MainWindow::onVerificationComplete() {
    const auto issues = m_verificationWatcher->result();
    ActivityLogWidget *log = m_rightPanelWidget->getActivityLog();
    if (issues.isEmpty()) {
        log->addLogEntry(tr("All mods verified OK."), ActivityLogWidget::LogLevel::Success);
        return;
    }
    for (const auto &issue : issues) {
        qWarning() << "Mod verification:" << issue.modName << "-" << issue.issue;
        log->addLogEntry(tr("Mod issue: %1 — %2").arg(issue.modName, issue.issue),
                         ActivityLogWidget::LogLevel::Warning);
    }
    QStringList names;
    names.reserve(issues.size());
    for (const auto &issue : issues)
        names.append(QStringLiteral("• ") + issue.modName);
    MessageModal::warning(m_modalManager, tr("Mod Verification"),
        tr("%1 mod(s) have missing files:\n\n%2\n\nYou may need to re-add these mods.")
           .arg(issues.size()).arg(names.join('\n')));
}

QString MainWindow::findProfileImportPath() const {
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg.endsWith(".tkprofile", Qt::CaseInsensitive)) {
            return arg;
        }
    }
    return QString();
}

void MainWindow::trySyncEnabledMods() {
    if (!m_modManager || !m_modsLoaded || !m_installPathReady) return;
    if (m_syncWatcher->isRunning()) return;
    const QString paksPath = m_modManager->getPaksPath();
    if (paksPath.isEmpty() || !QDir(paksPath).exists()) return;

    QPointer<ModManager> mgr(m_modManager);
    m_syncWatcher->setFuture(QtConcurrent::run([mgr]() {
        if (mgr) mgr->syncEnabledModsWithPaks();
    }));
}

void MainWindow::onUnregisteredModsDetectionComplete() {
    m_modListWidget->setLoadingState(false);
    m_installPathReady = true;
    trySyncEnabledMods();
}

void MainWindow::onSettingsClicked() {
    showSettingsOverlay();
}

void MainWindow::onSettingsApplied(bool autoCheck) {
    m_updateAvailable = false;
    ui->titleBar->setUpdateVisible(false);
    hideSettingsOverlay();
    if (autoCheck && m_updater) {
        m_updater->checkForUpdates();
    }
}

void MainWindow::onUpdateClicked() {
    if (!m_updateAvailable) {
        MessageModal::information(m_modalManager, tr("Up To Date"), tr("No updates are available."));
        return;
    }
    beginUpdateDownload();
}

void MainWindow::onUpdateCheckError(const QString &message) {
    qWarning() << "Updater:" << message;
    ActivityLogWidget *log = m_rightPanelWidget->getActivityLog();
    log->addLogEntry(tr("Update check failed"), ActivityLogWidget::LogLevel::Warning);
    if (m_settingsWidget) {
        m_settingsWidget->setCheckStatus(tr("Update check failed"));
    }
    if (m_updateDialog) {
        closeUpdateDialog();
        MessageModal::warning(m_modalManager, tr("Update Error"), message);
    }
}

void MainWindow::onUpdateAvailable(const UpdaterService::ReleaseInfo &release) {
    m_updateRelease = release;
    m_updateAvailable = true;
    ui->titleBar->setUpdateVisible(true);
    ActivityLogWidget *log = m_rightPanelWidget->getActivityLog();
    log->addLogEntry(tr("Update available"), ActivityLogWidget::LogLevel::Info);
    if (m_settingsWidget) {
        m_settingsWidget->setCheckStatus(tr("Update available: %1").arg(release.version.toString()));
    }
}

void MainWindow::onUpdateUpToDate(const UpdaterService::ReleaseInfo &latest) {
    Q_UNUSED(latest);
    m_updateAvailable = false;
    ui->titleBar->setUpdateVisible(false);
    if (m_settingsWidget) {
        m_settingsWidget->setCheckStatus(tr("Up to date"));
    }
}

void MainWindow::onUpdateDownloadProgress(qint64 received, qint64 total) {
    if (!m_updateDialog) return;
    if (total > 0) {
        int percent = static_cast<int>((received * 100) / total);
        m_updateDialog->setRange(0, 100);
        m_updateDialog->setValue(percent);
        const double receivedMb = received / 1024.0 / 1024.0;
        const double totalMb = total / 1024.0 / 1024.0;
        m_updateDialog->setLabelText(
            tr("Downloading update (%1 / %2 MB)")
                .arg(receivedMb, 0, 'f', 1)
                .arg(totalMb, 0, 'f', 1));
        if (!m_updateInstallStarted && received >= total && !m_pendingUpdatePath.isEmpty()) {
            QMetaObject::invokeMethod(this, [this]() {
                if (!m_pendingUpdatePath.isEmpty()) {
                    onUpdateDownloadFinished(m_pendingUpdatePath);
                }
            }, Qt::QueuedConnection);
            return;
        }
    } else {
        m_updateDialog->setRange(0, 0);
        m_updateDialog->setLabelText(tr("Downloading update..."));
    }
}

void MainWindow::onUpdateDownloadFinished(const QString &savePath) {
    if (m_updateInstallStarted) {
        return;
    }
    m_updateInstallStarted = true;
    if (m_updateDialog) {
        m_updateDialog->setCancelButton(nullptr);
        m_updateDialog->setRange(0, 0);
        m_updateDialog->setLabelText(tr("Installing update..."));
    }
    const QString updatesDir = m_settingsWidget
        ? m_settingsWidget->resolvedDownloadDir()
        : defaultUpdatesDir();

    QString version = m_updateRelease.version.toString();
    if (version.isEmpty()) {
        version = m_updateRelease.tagName;
        if (version.startsWith('v') || version.startsWith('V')) {
            version = version.mid(1);
        }
    }

#ifdef Q_OS_LINUX
    if (!qEnvironmentVariable("APPIMAGE").isEmpty()) {
        // AppImage mode: the downloaded file IS the update — no extraction needed
        launchUpdater(savePath, updatesDir);
        return;
    }
#endif

    QString error;
    if (!stageUpdate(savePath, version, updatesDir, &error)) {
        MessageModal::warning(m_modalManager, tr("Update Error"), error);
        return;
    }

    const QString stagingDir = QDir(updatesDir)
                                   .filePath(QString("staging/%1").arg(version));
    launchUpdater(stagingDir, updatesDir);
}

void MainWindow::beginUpdateDownload() {
    const QString assetName = selectUpdateAssetName();
    if (assetName.isEmpty()) {
        MessageModal::warning(m_modalManager, tr("Update Error"), tr("No compatible update asset was found."));
        return;
    }

    UpdaterService::Asset chosen;
    bool found = false;
    for (const auto &asset : m_updateRelease.assets) {
        if (asset.name.compare(assetName, Qt::CaseInsensitive) == 0) {
            chosen = asset;
            found = true;
            break;
        }
    }
    if (!found) {
        MessageModal::warning(m_modalManager, tr("Update Error"), tr("No compatible update asset was found."));
        return;
    }

    const QString updatesDir = m_settingsWidget
        ? m_settingsWidget->resolvedDownloadDir()
        : defaultUpdatesDir();
    const QString savePath = QDir(updatesDir).filePath(chosen.name);

    QFileInfo existing(savePath);
    m_updateInstallStarted = false;
    m_pendingUpdatePath = savePath;
    m_pendingUpdateSize = chosen.sizeBytes;
    if (existing.exists() && chosen.sizeBytes > 0 && existing.size() == chosen.sizeBytes) {
        onUpdateDownloadFinished(savePath);
        return;
    }

    showUpdateDialog();
    m_updater->downloadAsset(chosen, savePath);
}

void MainWindow::showUpdateDialog() {
    if (m_updateDialog) {
        m_updateDialog->close();
        m_updateDialog->deleteLater();
    }
    m_updateDialog = new QProgressDialog(tr("Downloading update..."), tr("Cancel"), 0, 100, this);
    m_updateDialog->setWindowTitle(tr("Update Download"));
    m_updateDialog->setWindowModality(Qt::ApplicationModal);
    m_updateDialog->setAutoClose(false);
    m_updateDialog->setAutoReset(false);
    connect(m_updateDialog, &QProgressDialog::canceled, this, [this]() {
        if (m_updater) {
            m_updater->cancelDownload();
        }
        closeUpdateDialog();
    });
    m_updateDialog->show();
}

void MainWindow::closeUpdateDialog() {
    if (!m_updateDialog) return;
    m_updateDialog->close();
    m_updateDialog->deleteLater();
    m_updateDialog = nullptr;
}

QString MainWindow::selectUpdateAssetName() const {
    QString version = m_updateRelease.version.toString();
    if (version.isEmpty()) {
        version = m_updateRelease.tagName;
        if (version.startsWith('v') || version.startsWith('V')) {
            version = version.mid(1);
        }
    }

    if (version.isEmpty()) {
        return QString();
    }

#if defined(Q_OS_WIN)
    return QStringLiteral("windows-%1.zip").arg(version);
#elif defined(Q_OS_LINUX)
    if (!qEnvironmentVariable("APPIMAGE").isEmpty())
        return QStringLiteral("TrenchKit-%1-x86_64.AppImage").arg(version);
    return QStringLiteral("linux-%1.zip").arg(version);
#else
    return QString();
#endif
}

bool MainWindow::stageUpdate(const QString &archivePath,
                             const QString &version,
                             const QString &updatesDir,
                             QString *error) {
    const QString stagingDir = QDir(updatesDir).filePath(QString("staging/%1").arg(version));

    QDir dir(stagingDir);
    if (dir.exists() && !dir.removeRecursively()) {
        if (error) {
            *error = tr("Failed to clear existing staging directory.");
        }
        return false;
    }
    if (!dir.mkpath(".")) {
        if (error) {
            *error = tr("Failed to create staging directory.");
        }
        return false;
    }

    QString extractError;
    if (!UpdateArchiveExtractor::extractArchive(archivePath, stagingDir, &extractError)) {
        if (error) {
            *error = tr("Failed to extract update: %1").arg(extractError);
        }
        return false;
    }

    return true;
}

void MainWindow::launchUpdater(const QString &stagingDir, const QString &updatesDir) {
#ifdef Q_OS_LINUX
    const QString appImagePath = qEnvironmentVariable("APPIMAGE");
    if (!appImagePath.isEmpty()) {
        // AppImage mode: copy the updater out of the AppImage mount before quitting,
        // because the mount point disappears once the process exits.
        const QString appDir = qEnvironmentVariable("APPDIR");
        const QString updaterSrc = QDir(appDir).filePath("usr/bin/TrenchKitUpdater");
        const QString tmpUpdater = QDir::tempPath() + QStringLiteral("/TrenchKitUpdater_update");
        QFile::remove(tmpUpdater);
        if (!QFile::copy(updaterSrc, tmpUpdater)) {
            MessageModal::warning(m_modalManager, tr("Update Error"),
                                  tr("Failed to stage updater helper."));
            return;
        }
        QFile::setPermissions(tmpUpdater,
            QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        QStringList args;
        args << "--install"
             << "--appimage-path" << appImagePath
             << "--new-file"      << stagingDir   // stagingDir holds new AppImage path in this mode
             << "--updates-dir"   << updatesDir
             << "--pid"           << QString::number(QCoreApplication::applicationPid());

        qInfo() << "Updater: launching AppImage helper" << tmpUpdater;
        qint64 pid = 0;
        if (!QProcess::startDetached(tmpUpdater, args, QDir::tempPath(), &pid)) {
            MessageModal::warning(m_modalManager, tr("Update Error"),
                                  tr("Failed to launch updater helper."));
            return;
        }
        QCoreApplication::quit();
        return;
    }
#endif

#ifdef Q_OS_LINUX
    // Portable zip mode: binary lives at <root>/usr/bin/TrenchKit.
    // Detected by trenchkit.sh existing at the zip root (two levels up from usr/bin/).
    {
        const QString binDir  = QCoreApplication::applicationDirPath();
        const QString zipRoot = QDir(binDir + "/../..").canonicalPath();
        if (QFile::exists(zipRoot + "/trenchkit.sh")) {
            // Stage updater to /tmp under a different name so that:
            //   (a) it survives removeOldAppFiles deleting usr/ from the zip root, and
            //   (b) helperName becomes "TrenchKitUpdater_update" so copyRecursive does
            //       NOT skip the new TrenchKitUpdater from the staged zip.
            const QString tmpUpdater =
                QDir::tempPath() + QStringLiteral("/TrenchKitUpdater_update");
            QFile::remove(tmpUpdater);
            if (!QFile::copy(QDir(binDir).filePath("TrenchKitUpdater"), tmpUpdater)) {
                MessageModal::warning(m_modalManager, tr("Update Error"),
                                      tr("Failed to stage updater helper."));
                return;
            }
            QFile::setPermissions(tmpUpdater,
                QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner);

            QStringList args;
            args << "--install"
                 << "--app-dir"    << zipRoot
                 << "--new-dir"    << stagingDir
                 << "--updates-dir" << updatesDir
                 << "--exe-name"   << QStringLiteral("usr/bin/TrenchKit")
                 << "--pid"        << QString::number(QCoreApplication::applicationPid());

            qInfo() << "Updater: launching zip helper" << tmpUpdater;
            qint64 pid = 0;
            if (!QProcess::startDetached(tmpUpdater, args, QDir::tempPath(), &pid)) {
                MessageModal::warning(m_modalManager, tr("Update Error"),
                                      tr("Failed to launch updater helper."));
                return;
            }
            QCoreApplication::quit();
            return;
        }
    }
#endif

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();

#if defined(Q_OS_WIN)
    const QString updaterExe = QDir(appDir).filePath("TrenchKitUpdater.exe");
#else
    const QString updaterExe = QDir(appDir).filePath("TrenchKitUpdater");
#endif

    if (!QFileInfo::exists(updaterExe)) {
        MessageModal::warning(m_modalManager, tr("Update Error"), tr("Updater helper was not found."));
        return;
    }

    QStringList args;
    args << "--install"
         << "--app-dir"    << appDir
         << "--new-dir"    << stagingDir
         << "--updates-dir" << updatesDir
         << "--exe-name"   << exeName
         << "--pid"        << QString::number(QCoreApplication::applicationPid());

    qInfo() << "Updater: launching helper" << updaterExe;
    qint64 pid = 0;
    const bool started = QProcess::startDetached(updaterExe, args, appDir, &pid);
    if (!started) {
        MessageModal::warning(m_modalManager, tr("Update Error"), tr("Failed to launch updater helper."));
        return;
    }
    QCoreApplication::quit();
}

void MainWindow::showSettingsOverlay() {
    if (!m_settingsPage || !ui->bodyStack || !ui->mainPage) return;
    if (m_settingsWidget) {
        m_settingsWidget->loadSettingsIntoUi();
    }
    ui->bodyStack->setCurrentWidget(m_settingsPage);
}

void MainWindow::hideSettingsOverlay() {
    if (!m_settingsPage || !ui->bodyStack || !ui->mainPage) return;
    ui->bodyStack->setCurrentWidget(ui->mainPage);
}
