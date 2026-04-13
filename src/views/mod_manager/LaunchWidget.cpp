#include "LaunchWidget.h"
#include "core/managers/ModManager.h"
#include "core/utils/Theme.h"
#include "common/modals/ModalManager.h"
#include "common/modals/MessageModal.h"
#include <QToolButton>
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QTimer>

#ifdef Q_OS_LINUX
static constexpr int FOXHOLE_STEAM_APP_ID = 1454690;
#endif

LaunchWidget::LaunchWidget(QWidget *parent)
    : QWidget(parent)
    , m_gamePollTimer(new QTimer(this))
    , m_launchButton(new QToolButton(this))
{
    setupUi();
    setupConnections();
    m_gamePollTimer->setInterval(2000);
    connect(m_gamePollTimer, &QTimer::timeout, this, [this]() {
        onGamePollTimeout();
    });
}

void LaunchWidget::setModManager(ModManager *modManager) {
    m_modManager = modManager;
}

void LaunchWidget::setFoxholeInstallPath(const QString &path) {
    m_foxholeInstallPath = path;
}

void LaunchWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN
    );
    layout->setSpacing(Theme::Spacing::CONTAINER_SPACING);

    m_launchButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_launchButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_launchButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_launchButton->setCursor(Qt::PointingHandCursor);

    QMenu *menu = new QMenu(this);
    m_playWithModsAction = menu->addAction(QString());
    m_playWithoutModsAction = menu->addAction(QString());

    connect(m_playWithModsAction, &QAction::triggered, this, [this]() {
        m_launchWithMods = true;
        retranslateUi();
    });

    connect(m_playWithoutModsAction, &QAction::triggered, this, [this]() {
        m_launchWithMods = false;
        retranslateUi();
    });

    m_launchButton->setMenu(menu);

    layout->addWidget(m_launchButton);

    setLayout(layout);

    retranslateUi();
}

void LaunchWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void LaunchWidget::retranslateUi() {
    if (m_launchWithMods) {
        m_launchButton->setText(tr("Play with mods"));
    } else {
        m_launchButton->setText(tr("Play without mods"));
    }
    m_playWithModsAction->setText(tr("Play with mods"));
    m_playWithoutModsAction->setText(tr("Play without mods"));
}

void LaunchWidget::setupConnections() {
    connect(m_launchButton, &QToolButton::clicked, this, [this]() {
        if (m_launchWithMods) {
            onLaunchWithMods();
        } else {
            onLaunchWithoutMods();
        }
    });
}

void LaunchWidget::onLaunchWithMods() {
#ifdef Q_OS_LINUX
    if (m_foxholeInstallPath.isEmpty()) {
        emit errorOccurred(tr("Foxhole installation not found. Please check your installation path."));
        return;
    }
    QDesktopServices::openUrl(QUrl(QStringLiteral("steam://run/%1").arg(FOXHOLE_STEAM_APP_ID)));
    emit gameLaunched(true);
    return;
#endif
    QString exePath = getFoxholeExecutablePath();

    if (exePath.isEmpty()) {
        emit errorOccurred(tr("Foxhole executable not found. Please check your installation path."));
        return;
    }

    if (!QProcess::startDetached(exePath, QStringList(), m_foxholeInstallPath)) {
        emit errorOccurred(tr("Failed to launch Foxhole"));
    } else {
        emit gameLaunched(true);
    }
}

void LaunchWidget::onLaunchWithoutMods() {
    if (!m_modManager) {
        emit errorOccurred(tr("Mod manager not initialized"));
        return;
    }

    QString exePath = getFoxholeExecutablePath();

    if (exePath.isEmpty()) {
        emit errorOccurred(tr("Foxhole executable not found. Please check your installation path."));
        return;
    }

    QList<ModInfo> mods = m_modManager->getMods();
    QStringList enabledModIds;

    for (const ModInfo &mod : mods) {
        if (mod.enabled) {
            enabledModIds.append(mod.id);
        }
    }

    auto launchGame = [this, exePath, enabledModIds]() {
        if (!enabledModIds.isEmpty()) {
            m_modsToRestore = enabledModIds;
            for (const QString &modId : enabledModIds) {
                m_modManager->disableMod(modId);
            }
        }

#ifdef Q_OS_LINUX
        QDesktopServices::openUrl(QUrl(QStringLiteral("steam://run/%1").arg(FOXHOLE_STEAM_APP_ID)));
        m_launchTimer.start();
        emit gameLaunched(false);
#else
        if (m_gameProcess) {
            m_gameProcess->deleteLater();
        }

        m_gameProcess = new QProcess(this);
        m_gameProcess->setWorkingDirectory(m_foxholeInstallPath);
        m_gameProcess->setProgram(exePath);

        connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &LaunchWidget::onGameProcessFinished);

        m_gameProcess->start();

        if (!m_gameProcess->waitForStarted(5000)) {
            emit errorOccurred(tr("Failed to launch Foxhole"));

            for (const QString &modId : m_modsToRestore) {
                m_modManager->enableMod(modId);
            }
            m_modsToRestore.clear();
        } else {
            m_launchTimer.start();
            m_waitingForGameStart = false;
            m_waitingForGameExit = false;
            emit gameLaunched(false);
        }
#endif

        if (!m_modsToRestore.isEmpty()) {
            m_waitingForGameStart = true;
            m_waitingForGameExit = false;
            startGamePolling();

            if (m_modalManager) {
                m_waitingModal = new MessageModal(
                    tr("Waiting for Foxhole"),
                    tr("Waiting for Foxhole to start...\n\n"
                    "Cancel to restore mods and stop the launch."),
                    MessageModal::Information,
                    MessageModal::Cancel
                );
                connect(m_waitingModal, &MessageModal::finished, this, [this]() {
                    if (m_waitingForGameStart || m_waitingForGameExit) {
                        cancelWaitingForGameStart();
                    }
                });
                m_modalManager->showModal(m_waitingModal);
            }
        }
    };

    if (!enabledModIds.isEmpty()) {
        auto *modal = new MessageModal(
            tr("Launch without mods"),
            tr("This will temporarily disable %1 enabled mod(s).\n\n"
                    "They will be automatically restored when the game closes.\n\n"
                    "Continue?").arg(enabledModIds.count()),
            MessageModal::Question,
            MessageModal::Yes | MessageModal::No
        );
        connect(modal, &MessageModal::finished, this, [this, modal, launchGame]() {
            if (modal->clickedButton() == MessageModal::Yes) {
                launchGame();
            }
        });
        m_modalManager->showModal(modal);
    } else {
        launchGame();
    }
}

void LaunchWidget::onGameProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    if (m_modsToRestore.isEmpty()) {
        return;
    }

    if (m_waitingForGameStart || m_waitingForGameExit) {
        return;
    }

    restoreDisabledMods();
}

QString LaunchWidget::getFoxholeExecutablePath() const {
    if (m_foxholeInstallPath.isEmpty()) {
        return QString();
    }

    QStringList possibleExes = {
        "War-Win64-Shipping.exe",
        "Foxhole.exe",
        "War.exe"
    };

    QDir warDir(m_foxholeInstallPath);
    if (warDir.cd("War") && warDir.cd("Binaries") && warDir.cd("Win64")) {
        for (const QString &exe : possibleExes) {
            QString path = warDir.filePath(exe);
            if (QFile::exists(path)) {
                return path;
            }
        }
    }

    QDir rootDir(m_foxholeInstallPath);
    for (const QString &exe : possibleExes) {
        QString path = rootDir.filePath(exe);
        if (QFile::exists(path)) {
            return path;
        }
    }

    return QString();
}

void LaunchWidget::startGamePolling() {
    if (!m_gamePollTimer->isActive()) {
        m_gamePollTimer->start();
    }
}

void LaunchWidget::onGamePollTimeout() {
    if (m_modsToRestore.isEmpty() || !m_modManager) {
        m_gamePollTimer->stop();
        return;
    }

    const bool running = isGameRunning();

    if (m_waitingForGameStart) {
        if (running) {
            m_waitingForGameStart = false;
            m_waitingForGameExit = true;
            if (m_waitingModal) {
                m_waitingModal->reject();
                m_waitingModal = nullptr;
            }
            return;
        }
        return;
    }

    if (m_waitingForGameExit) {
        if (!running) {
            m_gamePollTimer->stop();
            restoreDisabledMods();
        }
    }
}

void LaunchWidget::restoreDisabledMods() {
    if (m_modsToRestore.isEmpty() || !m_modManager) {
        return;
    }

    const int modCount = m_modsToRestore.count();
    for (const QString &modId : m_modsToRestore) {
        m_modManager->enableMod(modId);
    }

    if (m_waitingModal) {
        m_waitingModal->reject();
        m_waitingModal = nullptr;
    }

    emit modsRestored(modCount);

    MessageModal::information(m_modalManager, tr("Mods Restored"),
        tr("Restored %1 mod(s) that were disabled for vanilla gameplay.")
        .arg(modCount));

    m_modsToRestore.clear();
}

void LaunchWidget::cancelWaitingForGameStart() {
    m_gamePollTimer->stop();
    m_waitingForGameStart = false;
    m_waitingForGameExit = false;
    if (m_waitingModal) {
        m_waitingModal->reject();
        m_waitingModal = nullptr;
    }
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        m_gameProcess->terminate();
        if (!m_gameProcess->waitForFinished(2000)) {
            m_gameProcess->kill();
        }
    }
    restoreDisabledMods();
}

bool LaunchWidget::isGameRunning() const {
#if defined(Q_OS_WIN)
    const QStringList exeNames = {
        QStringLiteral("War-Win64-Shipping.exe"),
        QStringLiteral("Foxhole.exe"),
        QStringLiteral("War.exe")
    };

    for (const QString &exe : exeNames) {
        QProcess tasklist;
        tasklist.start(QStringLiteral("tasklist"),
                       QStringList() << QStringLiteral("/FI")
                                     << QStringLiteral("IMAGENAME eq %1").arg(exe));
        if (!tasklist.waitForFinished(1000)) {
            continue;
        }
        const QString output = QString::fromUtf8(tasklist.readAllStandardOutput());
        if (output.contains(exe, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
#else
    const QStringList procNames = {
        QStringLiteral("FoxholeClient-Win64-Shipping"),
        QStringLiteral("War-Win64-Shipping"),
        QStringLiteral("FoxholeClient"),
    };
    for (const QString &name : procNames) {
        QProcess pgrep;
        pgrep.start(QStringLiteral("pgrep"), QStringList() << QStringLiteral("-f") << name);
        if (pgrep.waitForFinished(1000) && pgrep.exitCode() == 0)
            return true;
    }
    return false;
#endif
}
