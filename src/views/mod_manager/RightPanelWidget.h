/**
 * @file RightPanelWidget.h
 * @brief Right-side panel that aggregates the actions, backup, launch, and activity log widgets.
 */
#ifndef RIGHTPANELWIDGET_H
#define RIGHTPANELWIDGET_H

#include <QWidget>
#include <QString>

class ModManager;
class ModalManager;
class ActionsWidget;
class BackupWidget;
class LaunchWidget;
class ActivityLogWidget;

/**
 * @brief Container widget that hosts @c ActionsWidget, @c BackupWidget, @c LaunchWidget,
 * and @c ActivityLogWidget in a vertical stack.
 */
class RightPanelWidget : public QWidget {
    Q_OBJECT

public:
    explicit RightPanelWidget(QWidget *parent = nullptr);
    ~RightPanelWidget() override = default;

    void setModManager(ModManager *modManager);
    void setModalManager(ModalManager *modalManager);
    void setFoxholeInstallPath(const QString &path);

    /**
     * @brief Returns the activity log widget; used by @c MainWindow to add log entries.
     */
    ActivityLogWidget* getActivityLog() const { return m_activityLogWidget; }

signals:
    void addModRequested();
    void removeModRequested();
    void moveUpRequested();
    void moveDownRequested();
    void errorOccurred(const QString &error);

public slots:
    void onModSelectionChanged(int selectedRow, int totalMods);

private:
    void setupUi();
    void setupConnections();

    ModManager *m_modManager = nullptr;
    QString m_foxholeInstallPath;

    ActionsWidget *m_actionsWidget;
    BackupWidget *m_backupWidget;
    ActivityLogWidget *m_activityLogWidget;
    LaunchWidget *m_launchWidget;
};

#endif // RIGHTPANELWIDGET_H
