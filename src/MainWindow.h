/**
 * @file MainWindow.h
 * @brief Application main window; owns all services and orchestrates the UI.
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFutureWatcher>
#include <QPointer>
#include "core/managers/ModManager.h"
#include "core/services/UpdaterService.h"
#include "core/api/NexusModsClient.h"
#include "core/api/NexusModsAuth.h"
#include "core/api/ItchClient.h"
#include "core/api/ItchAuth.h"
#include "core/services/ModUpdateService.h"
#include "core/services/ItchModUpdateService.h"
#include "common/modals/ModalManager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class InstallPathWidget;
class ModListWidget;
class RightPanelWidget;
class ProfileManagerWidget;
class ProfileManager;
class QProgressDialog;
class SettingsWidget;
class QShortcut;

/**
 * @brief Root widget that owns all services and injects them into child views.
 *
 * Services (@c ModManager, @c ProfileManager, API clients, update services) are
 * created here and passed as raw pointers to widgets. Services are never parented
 * to widgets — only to @c MainWindow.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief Returns the application-wide modal manager; used by views to show modals.
     */
    ModalManager* modalManager() { return m_modalManager; }

protected:
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();
    void onInstallPathChanged(const QString &path);
    void onModsLoadComplete();
    void onUnregisteredModsDetectionComplete();
    void onUpdateClicked();
    void onUpdateCheckError(const QString &message);
    void onUpdateAvailable(const UpdaterService::ReleaseInfo &release);
    void onUpdateUpToDate(const UpdaterService::ReleaseInfo &latest);
    void onUpdateDownloadProgress(qint64 received, qint64 total);
    void onUpdateDownloadFinished(const QString &savePath);
    void onSettingsClicked();
    void runModVerification();
    void onVerificationComplete();

private:
    void setupTitleBar();
    void setupInstallPath();
    void setupProfileManager();
    void setupModList();
    void setupRightPanel();
    void setupSettingsOverlay();
    void loadSettings();
    void saveSettings();
    void startUpdateCheck();
    void beginUpdateDownload();
    void showUpdateDialog();
    void closeUpdateDialog();
    QString selectUpdateAssetName() const;
    bool stageUpdate(const QString &archivePath, const QString &version, const QString &updatesDir, QString *error);
    void launchUpdater(const QString &stagingDir, const QString &updatesDir);
    void showSettingsOverlay();
    void hideSettingsOverlay();
    void onSettingsApplied(bool autoCheck);
    QString findProfileImportPath() const;
    void trySyncEnabledMods();

    Ui::MainWindow *ui;
    InstallPathWidget *m_installPathWidget;
    ProfileManagerWidget *m_profileManagerWidget;
    ModListWidget *m_modListWidget;
    RightPanelWidget *m_rightPanelWidget;
    ModManager *m_modManager;
    ProfileManager *m_profileManager;
    QFutureWatcher<bool> *m_modLoadWatcher;
    QFutureWatcher<void> *m_unregisteredModsWatcher;
    QFutureWatcher<QList<ModManager::VerificationIssue>> *m_verificationWatcher = nullptr;
    QFutureWatcher<void> *m_syncWatcher = nullptr;
    bool m_firstShow = true;
    UpdaterService *m_updater = nullptr;
    NexusModsClient *m_nexusClient = nullptr;
    NexusModsAuth *m_nexusAuth = nullptr;
    ItchClient *m_itchClient = nullptr;
    ItchAuth *m_itchAuth = nullptr;
    ModUpdateService *m_modUpdateService = nullptr;
    ItchModUpdateService *m_itchUpdateService = nullptr;
    UpdaterService::ReleaseInfo m_updateRelease;
    bool m_updateAvailable = false;
    bool m_updateInstallStarted = false;
    bool m_modsLoaded = false;
    bool m_installPathReady = false;
    QPointer<QProgressDialog> m_updateDialog;
    QString m_pendingUpdatePath;
    qint64 m_pendingUpdateSize = -1;
    QString m_pendingProfileImportPath;
    QWidget *m_settingsPage = nullptr;
    SettingsWidget *m_settingsWidget = nullptr;
    ModalManager *m_modalManager = nullptr;
    QShortcut *m_globalSearchShortcut = nullptr;

    QPixmap m_backgroundTexture;
    QPixmap m_cachedScaledTexture;
    QSize m_lastSize;
};

#endif // MAINWINDOW_H
