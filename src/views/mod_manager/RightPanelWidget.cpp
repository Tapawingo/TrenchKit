#include "RightPanelWidget.h"
#include "ActionsWidget.h"
#include "BackupWidget.h"
#include "ActivityLogWidget.h"
#include "LaunchWidget.h"
#include "core/utils/Theme.h"
#include <QVBoxLayout>

RightPanelWidget::RightPanelWidget(QWidget *parent)
    : QWidget(parent)
    , m_actionsWidget(new ActionsWidget(this))
    , m_backupWidget(new BackupWidget(this))
    , m_activityLogWidget(new ActivityLogWidget(this))
    , m_launchWidget(new LaunchWidget(this))
{
    setupUi();
    setupConnections();
}

void RightPanelWidget::setModManager(ModManager *modManager) {
    m_modManager = modManager;
    m_backupWidget->setModManager(modManager);
    m_launchWidget->setModManager(modManager);
}

void RightPanelWidget::setModalManager(ModalManager *modalManager) {
    m_backupWidget->setModalManager(modalManager);
    m_launchWidget->setModalManager(modalManager);
}

void RightPanelWidget::setFoxholeInstallPath(const QString &path) {
    m_foxholeInstallPath = path;
    m_actionsWidget->setFoxholeInstallPath(path);
    m_launchWidget->setFoxholeInstallPath(path);
}

void RightPanelWidget::onModSelectionChanged(int selectedRow, int totalMods) {
    m_actionsWidget->onModSelectionChanged(selectedRow, totalMods);
}

void RightPanelWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN
    );
    layout->setSpacing(Theme::Spacing::RIGHT_PANEL_SECTION_SPACING);

    layout->addWidget(m_actionsWidget);
    layout->addWidget(m_backupWidget);
    layout->addWidget(m_activityLogWidget);
    layout->addWidget(m_launchWidget);

    setLayout(layout);
}

void RightPanelWidget::setupConnections() {
    connect(m_actionsWidget, &ActionsWidget::addModRequested,
            this, &RightPanelWidget::addModRequested);
    connect(m_actionsWidget, &ActionsWidget::removeModRequested,
            this, &RightPanelWidget::removeModRequested);
    connect(m_actionsWidget, &ActionsWidget::moveUpRequested,
            this, &RightPanelWidget::moveUpRequested);
    connect(m_actionsWidget, &ActionsWidget::moveDownRequested,
            this, &RightPanelWidget::moveDownRequested);
    connect(m_actionsWidget, &ActionsWidget::errorOccurred,
            this, &RightPanelWidget::errorOccurred);

    connect(m_backupWidget, &BackupWidget::errorOccurred,
            this, &RightPanelWidget::errorOccurred);

    connect(m_launchWidget, &LaunchWidget::errorOccurred,
            this, &RightPanelWidget::errorOccurred);
}
