/**
 * @file ModListWidget.h
 * @brief Main mod list panel; displays, filters, and manages all installed mods.
 */
#ifndef MODLISTWIDGET_H
#define MODLISTWIDGET_H

#include "core/managers/ModManager.h"
#include "DraggableModList.h"
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QRegularExpression>
#include <QCheckBox>
#include <QStringList>
#include <QUrl>

class NexusModsClient;
class NexusModsAuth;
class ItchClient;
class ItchAuth;
class ModUpdateService;
class ItchModUpdateService;
class ModalManager;
class ModConflictDetector;
class QLineEdit;
class QShortcut;
struct ModUpdateInfo;
struct ItchUpdateInfo;
struct ConflictInfo;
struct NexusFileInfo;
struct ItchUploadInfo;

/**
 * @brief Widget that hosts the mod list, search bar, and all mod-management actions.
 *
 * Services are injected via setters after construction. Call @c refreshModList()
 * to rebuild the list from @c ModManager, and @c rescanConflicts() to re-run
 * the conflict detector.
 */
class ModListWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModListWidget(QWidget *parent = nullptr);
    ~ModListWidget() override = default;

    void setModManager(ModManager *modManager);
    void setNexusServices(NexusModsClient *client, NexusModsAuth *auth);
    void setItchServices(ItchClient *client, ItchAuth *auth);
    void setUpdateService(ModUpdateService *service);
    void setItchUpdateService(ItchModUpdateService *service);
    void setModalManager(ModalManager *modalManager) { m_modalManager = modalManager; }
    /**
     * @brief Rebuilds the displayed list from the current @c ModManager state.
     */
    void refreshModList();
    void rescanConflicts();
    void setLoadingState(bool loading, const QString &message = "Loading mods");

signals:
    /**
     * @brief Emitted when the selection changes.
     *
     * @p selectedCount is 0 when nothing is selected; @p minRow and @p maxRow are -1.
     */
    void modSelectionChanged(int selectedCount, int minRow, int maxRow, int totalMods);
    void modAdded(const QString &modName);
    void modRemoved(const QString &modName);
    void modReordered();
    void verifyModsRequested();

public slots:
    void onAddModClicked();
    void onRemoveModClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onCheckUpdatesClicked();
    /**
     * @brief Shows and focuses the search bar.
     */
    void activateSearch();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onModsChanged();
    void onModEnabledChanged(const QString &modId, bool enabled);
    void onEnableAllClicked();
    void onItemsReordered();
    void updateLoadingAnimation();
    void onRenameRequested(const QString &modId);
    void onEditMetaRequested(const QString &modId);
    void onRemoveRequested(const QString &modId);
    void onUpdateRequested(const QString &modId);
    void onUpdateFound(const QString &modId, const ModUpdateInfo &updateInfo);
    void onUpdateCheckComplete(int updatesFound);
    void onItchUpdateFound(const QString &modId, const ItchUpdateInfo &updateInfo);
    void onItchUpdateCheckComplete(int updatesFound);
    void onFilesDropped(const QStringList &filePaths);
    void onConflictScanComplete(QMap<QString, ConflictInfo> conflicts);
    void onConflictDetailsRequested(const QString &modId);
    void onRegisterWithNexusRequested(const QString &modId);
    void onRegisterWithItchRequested(const QString &modId);
    void onContextMenuRequested(const QPoint &pos);

private:
    void setupUi();
    void retranslateUi();
    QString getSelectedModId() const;
    QStringList getSelectedModIds() const;
    QList<int> getSelectedRows() const;
    int getSelectedRow() const;
    int getSelectedCount() const;
    void showSelectionContextMenu(const QPoint &globalPos);
    QUrl buildNexusUrl(const ModInfo &mod) const;
    QUrl buildItchUrl(const ModInfo &mod) const;
    void startNexusRegistrationQueue(const QStringList &modIds);
    void startItchRegistrationQueue(const QStringList &modIds);
    void applyNexusRegistration(const QString &modId, const QList<NexusFileInfo> &files,
                                const QString &nexusModId, const QString &author,
                                const QString &description, const QString &nexusUrl);
    void applyItchRegistration(const QString &modId, const QList<ItchUploadInfo> &uploads,
                               const QString &itchGameId, const QString &author,
                               const QString &itchUrl);
    QString extractVersionFromFilename(const QString &filename) const;
    void handlePakFile(const QString &pakPath);
    void handleArchiveFile(const QString &archivePath);
    bool isArchiveFile(const QString &filePath) const;
    void hideSearch(bool clearFilter);
    void updateDragMode();
    bool isFilterActive() const;

    ModManager *m_modManager = nullptr;
    NexusModsClient *m_nexusClient = nullptr;
    NexusModsAuth *m_nexusAuth = nullptr;
    ItchClient *m_itchClient = nullptr;
    ItchAuth *m_itchAuth = nullptr;
    ModUpdateService *m_updateService = nullptr;
    ItchModUpdateService *m_itchUpdateService = nullptr;
    ModalManager *m_modalManager = nullptr;
    ModConflictDetector *m_conflictDetector = nullptr;
    DraggableModList *m_modList;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_loadingLabel;
    QLabel *m_modCountLabel;
    QPushButton *m_checkUpdatesButton;
    QCheckBox *m_enableAllCheckBox = nullptr;
    QTimer *m_loadingTimer;
    QWidget *m_searchContainer = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QString m_filterText;
    int m_loadingDots = 0;

    bool m_updating = false;
    int m_pendingUpdateChecks = 0;
    int m_totalUpdatesFound = 0;
};

#endif // MODLISTWIDGET_H
