#include "ProfileRowWidget.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>
#include <QMenu>
#include <QContextMenuEvent>

ProfileRowWidget::ProfileRowWidget(const QString &profileId, const QString &profileName, QWidget *parent)
    : QWidget(parent)
    , m_profileId(profileId)
    , m_profileName(profileName)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);
    setupUi(profileName);
}

void ProfileRowWidget::setupUi(const QString &) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(
        Theme::Spacing::PROFILE_ROW_PADDING_HORIZONTAL,
        Theme::Spacing::PROFILE_ROW_PADDING_VERTICAL,
        Theme::Spacing::PROFILE_ROW_PADDING_HORIZONTAL,
        Theme::Spacing::PROFILE_ROW_PADDING_VERTICAL
    );
    layout->setSpacing(Theme::Spacing::PROFILE_ROW_INTERNAL_SPACING);

    m_iconLabel = new QLabel(this);
    QPixmap iconPixmap(":/icon_profile.png");
    m_iconLabel->setPixmap(iconPixmap.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_iconLabel->setFixedSize(20, 20);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName("profileNameLabel");

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_nameLabel, 1);

    setLayout(layout);

    retranslateUi();
    updateStyling();
}

void ProfileRowWidget::setActive(bool active) {
    if (m_isActive != active) {
        m_isActive = active;
        m_nameLabel->setProperty("active", active);
        style()->unpolish(m_nameLabel);
        style()->polish(m_nameLabel);
        retranslateUi();
        updateStyling();
    }
}

void ProfileRowWidget::setSelected(bool selected) {
    if (m_isSelected != selected) {
        m_isSelected = selected;
        updateStyling();
    }
}

void ProfileRowWidget::updateStyling() {
    setProperty("selected", m_isSelected);
    style()->unpolish(this);
    style()->polish(this);
}

void ProfileRowWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ProfileRowWidget::retranslateUi() {
    if (m_isActive) {
        m_nameLabel->setText(m_profileName + QStringLiteral(" ") + tr("(Active)"));
    } else {
        m_nameLabel->setText(m_profileName);
    }
}

void ProfileRowWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_profileId);
    }
    QWidget::mousePressEvent(event);
}

void ProfileRowWidget::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);

    QAction *exportAction = menu.addAction(QIcon(":/icon_export.png"), tr("Export Profile"));
    QAction *renameAction = menu.addAction(QIcon(":/icon_edit.png"), tr("Rename Profile"));
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(QIcon(":/icon_delete.png"), tr("Delete Profile"));

    QAction *selectedAction = menu.exec(event->globalPos());

    if (selectedAction == exportAction) {
        emit exportRequested(m_profileId);
    } else if (selectedAction == renameAction) {
        emit renameRequested(m_profileId);
    } else if (selectedAction == deleteAction) {
        emit deleteRequested(m_profileId);
    }
}
