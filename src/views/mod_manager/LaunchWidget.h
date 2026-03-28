/// @file LaunchWidget.h
/// @brief Widget for launching Foxhole with or without mods enabled.
///
/// When launching without mods, the widget temporarily disables all mods,
/// monitors the game process, and re-enables them after the game exits.
#ifndef LAUNCHWIDGET_H
#define LAUNCHWIDGET_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QElapsedTimer>
#include <QPointer>
#include <QEvent>

class QAction;
class QToolButton;
class ModManager;
class ModalManager;
class QTimer;
class MessageModal;

/// @brief Provides a split launch button and manages the mod-disable/restore lifecycle.
///
/// "Launch with mods" deploys mods normally. "Launch without mods" temporarily
/// disables all enabled mods, waits for the game to exit (polled via @c QTimer
/// when QProcess tracking is unavailable), then restores them and emits
/// @c modsRestored.
class LaunchWidget : public QWidget {
    Q_OBJECT

public:
    explicit LaunchWidget(QWidget *parent = nullptr);
    ~LaunchWidget() override = default;

    void setModManager(ModManager *modManager);
    void setModalManager(ModalManager *modalManager) { m_modalManager = modalManager; }
    void setFoxholeInstallPath(const QString &path);

signals:
    void errorOccurred(const QString &error);
    void gameLaunched(bool withMods);
    /// @brief Emitted after the game exits and previously-disabled mods have been re-enabled.
    void modsRestored(int modCount);

private slots:
    void onLaunchWithMods();
    void onLaunchWithoutMods();
    void onGameProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void setupConnections();
    QString getFoxholeExecutablePath() const;
    void onGamePollTimeout();
    void restoreDisabledMods();
    void cancelWaitingForGameStart();
    void startGamePolling();
    bool isGameRunning() const;

    ModManager *m_modManager = nullptr;
    ModalManager *m_modalManager = nullptr;
    QString m_foxholeInstallPath;
    QProcess *m_gameProcess = nullptr;
    QStringList m_modsToRestore;
    QTimer *m_gamePollTimer = nullptr;
    QElapsedTimer m_launchTimer;
    bool m_waitingForGameStart = false;
    bool m_waitingForGameExit = false;
    QPointer<MessageModal> m_waitingModal;

    QToolButton *m_launchButton = nullptr;
    QAction *m_playWithModsAction = nullptr;
    QAction *m_playWithoutModsAction = nullptr;
    bool m_launchWithMods = true;
};

#endif // LAUNCHWIDGET_H
