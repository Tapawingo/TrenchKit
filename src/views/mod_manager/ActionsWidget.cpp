#include "ActionsWidget.h"
#include "common/widgets/GradientFrame.h"
#include "core/utils/Theme.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>

ActionsWidget::ActionsWidget(QWidget *parent)
    : QWidget(parent)
    , m_addButton(new QPushButton(this))
    , m_removeButton(new QPushButton(this))
    , m_moveUpButton(new QPushButton(this))
    , m_moveDownButton(new QPushButton(this))
    , m_exploreFolderButton(new QPushButton(this))
{
    setupUi();
    setupConnections();
}

void ActionsWidget::setFoxholeInstallPath(const QString &path) {
    m_foxholeInstallPath = path;
}

void ActionsWidget::onModSelectionChanged(int selectedCount, int minRow, int maxRow, int totalMods) {
    m_selectedCount = selectedCount;
    m_minRow = minRow;
    m_maxRow = maxRow;
    m_totalMods = totalMods;

    m_removeButton->setEnabled(selectedCount > 0);
    m_moveUpButton->setEnabled(selectedCount > 0 && minRow > 0);
    m_moveDownButton->setEnabled(selectedCount > 0 && maxRow >= 0 && maxRow < totalMods - 1);
}

void ActionsWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN
    );
    layout->setSpacing(Theme::Spacing::CONTAINER_SPACING);

    GradientFrame *frame = new GradientFrame(this);
    layout->addWidget(frame);

    QVBoxLayout *frameLayout = new QVBoxLayout(frame);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("modActionsTitle");
    frameLayout->addWidget(m_titleLabel);

    m_addButton->setCursor(Qt::PointingHandCursor);
    m_removeButton->setEnabled(false);
    m_removeButton->setCursor(Qt::PointingHandCursor);
    frameLayout->addWidget(m_addButton);
    frameLayout->addWidget(m_removeButton);

    m_moveUpButton->setEnabled(false);
    m_moveUpButton->setCursor(Qt::PointingHandCursor);
    frameLayout->addWidget(m_moveUpButton);

    m_moveDownButton->setEnabled(false);
    m_moveDownButton->setCursor(Qt::PointingHandCursor);
    frameLayout->addWidget(m_moveDownButton);

    frameLayout->addWidget(createSeparator());

    m_exploreFolderButton->setCursor(Qt::PointingHandCursor);
    frameLayout->addWidget(m_exploreFolderButton);

    setLayout(layout);

    retranslateUi();
}

void ActionsWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ActionsWidget::retranslateUi() {
    m_titleLabel->setText(tr("Actions"));
    m_addButton->setText(tr("Add Mod"));
    m_removeButton->setText(tr("Remove Mod"));
    m_moveUpButton->setText(tr("Move Up"));
    m_moveDownButton->setText(tr("Move Down"));
    m_exploreFolderButton->setText(tr("Explore Mod Folder"));
}

void ActionsWidget::setupConnections() {
    connect(m_addButton, &QPushButton::clicked, this, &ActionsWidget::onAddModClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &ActionsWidget::onRemoveModClicked);
    connect(m_moveUpButton, &QPushButton::clicked, this, &ActionsWidget::onMoveUpClicked);
    connect(m_moveDownButton, &QPushButton::clicked, this, &ActionsWidget::onMoveDownClicked);
    connect(m_exploreFolderButton, &QPushButton::clicked, this, &ActionsWidget::onExploreFolderClicked);
}

QFrame* ActionsWidget::createSeparator() {
    QFrame *line = new QFrame(this);
    line->setObjectName("actionsSeparator");
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    return line;
}

void ActionsWidget::onAddModClicked() {
    emit addModRequested();
}

void ActionsWidget::onRemoveModClicked() {
    emit removeModRequested();
}

void ActionsWidget::onMoveUpClicked() {
    emit moveUpRequested();
}

void ActionsWidget::onMoveDownClicked() {
    emit moveDownRequested();
}

void ActionsWidget::onExploreFolderClicked() {
    if (m_foxholeInstallPath.isEmpty()) {
        emit errorOccurred(tr("Foxhole installation path not set"));
        return;
    }

    QString paksPath = m_foxholeInstallPath + "/War/Content/Paks";
    QDir dir(paksPath);

    if (!dir.exists()) {
        emit errorOccurred(tr("Paks folder not found: %1").arg(paksPath));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(paksPath));
}
