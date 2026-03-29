/**
 * @file SettingsWidget.h
 * @brief Settings panel covering updater, API credentials, language, shortcuts, and logging.
 */
#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include <QPointer>
#include <QEvent>

class UpdaterService;
class NexusModsClient;
class NexusModsAuth;
class ItchClient;
class ItchAuth;
class ModalManager;
class PanelFrame;
class GradientFrame;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

/**
 * @brief Full-page settings widget shown over the main window via @c MainWindow::showSettingsOverlay().
 *
 * Covers: update source, update channel, auto-check toggle, download directory,
 * NexusMods and itch.io API key management, language selection, log file access,
 * desktop/start-menu shortcuts, and .tkprofile file association.
 */
class SettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget *parent, UpdaterService *updater);
    ~SettingsWidget() override = default;

    /**
     * @brief Reads stored @c QSettings and applies them to the injected services; returns false on error.
     */
    bool applyStoredSettings();
    /**
     * @brief Populates all UI controls from the current @c QSettings values.
     */
    void loadSettingsIntoUi();
    QString resolvedDownloadDir() const;
    void setCurrentVersion(const QString &version);
    void setCheckStatus(const QString &status);
    void setNexusServices(NexusModsClient *client, NexusModsAuth *auth);
    void setItchServices(ItchClient *client, ItchAuth *auth);
    void setModalManager(ModalManager *modalManager) { m_modalManager = modalManager; }

protected:
    void changeEvent(QEvent *event) override;

signals:
    void cancelRequested();
    /**
     * @brief Emitted when the user clicks Save; @p autoCheck reflects the auto-check setting.
     */
    void settingsApplied(bool autoCheck);
    void manualCheckRequested();

private slots:
    void onOpenLogsClicked();
    void onCopyLogPathClicked();
    void onAddDesktopShortcutClicked();
    void onAddStartMenuShortcutClicked();
    void onAssociateTkprofileClicked();

private:
    void buildUi();
    void retranslateUi();
    void applySettings();
    void loadSettings(bool applyToUpdater);
    static bool parseGithubRepo(const QString &text, QString *owner, QString *repo);
    bool createShortcut(const QString &shortcutPath, const QString &targetPath, const QString &description);
    bool isTkprofileAssociationSet() const;
    bool registerTkprofileAssociation();

    QPointer<UpdaterService> m_updater;
    QPointer<NexusModsClient> m_nexusClient;
    QPointer<NexusModsAuth> m_nexusAuth;
    QPointer<ItchClient> m_itchClient;
    QPointer<ItchAuth> m_itchAuth;
    ModalManager *m_modalManager = nullptr;
    PanelFrame *m_panel = nullptr;
    GradientFrame *m_container = nullptr;
    GradientFrame *m_footer = nullptr;
    QLineEdit *m_updateSourceEdit = nullptr;
    QComboBox *m_updateChannelCombo = nullptr;
    QCheckBox *m_autoCheckCheckbox = nullptr;
    QLineEdit *m_downloadDirEdit = nullptr;
    QPushButton *m_downloadBrowseButton = nullptr;
    QPushButton *m_checkNowButton = nullptr;
    QPushButton *m_settingsSaveButton = nullptr;
    QPushButton *m_settingsCancelButton = nullptr;
    QLabel *m_versionLabel = nullptr;
    QLabel *m_checkStatusLabel = nullptr;
    QLabel *m_nexusStatusLabel = nullptr;
    QPushButton *m_nexusAuthButton = nullptr;
    QPushButton *m_nexusClearButton = nullptr;
    QLabel *m_itchStatusLabel = nullptr;
    QPushButton *m_itchAuthButton = nullptr;
    QPushButton *m_itchClearButton = nullptr;
    QLabel *m_logPathLabel = nullptr;
    QPushButton *m_openLogsButton = nullptr;
    QPushButton *m_copyLogPathButton = nullptr;
    QPushButton *m_addDesktopShortcutButton = nullptr;
    QPushButton *m_addStartMenuShortcutButton = nullptr;
    QLabel *m_tkprofileStatusLabel = nullptr;
    QPushButton *m_tkprofileAssociateButton = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_updaterHeader = nullptr;
    QLabel *m_currentVersionLabel = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QLabel *m_channelLabel = nullptr;
    QLabel *m_autoCheckLabel = nullptr;
    QLabel *m_downloadLabel = nullptr;
    QLabel *m_checkLabel = nullptr;
    QLabel *m_shortcutsHeader = nullptr;
    QLabel *m_desktopShortcutLabel = nullptr;
    QLabel *m_startMenuLabel = nullptr;
    QLabel *m_nexusHeader = nullptr;
    QLabel *m_nexusConnectionLabel = nullptr;
    QLabel *m_nexusAuthLabel = nullptr;
    QLabel *m_itchHeader = nullptr;
    QLabel *m_itchConnectionLabel = nullptr;
    QLabel *m_itchAuthLabel = nullptr;
    QLabel *m_loggingHeader = nullptr;
    QLabel *m_logLocationLabel = nullptr;
    QLabel *m_logActionsLabel = nullptr;
    QLabel *m_fileAssocHeader = nullptr;
    QLabel *m_tkprofileLabel = nullptr;
};

#endif // SETTINGSWIDGET_H
