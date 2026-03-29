#include "BaseModalContent.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

BaseModalContent::BaseModalContent(QWidget *parent)
    : QWidget(parent)
{
    setupLayout();
}

void BaseModalContent::setupLayout() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_headerWidget = new QWidget(this);
    m_headerWidget->setFixedHeight(48);
    auto *headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    headerLayout->setSpacing(0);

    m_titleLabel = new QLabel(m_headerWidget);
    m_titleLabel->setStyleSheet(QString("QLabel { font-size: 14px; font-weight: bold; color: %1; }")
                                .arg(Theme::Colors::TEXT_PRIMARY));
    headerLayout->addWidget(m_titleLabel, 0, Qt::AlignVCenter);

    m_bodyWidget = new QWidget(this);
    m_bodyLayout = new QVBoxLayout(m_bodyWidget);
    m_bodyLayout->setContentsMargins(16, 16, 16, 16);
    m_bodyLayout->setSpacing(Theme::Spacing::FORM_SPACING);

    m_footerWidget = new QWidget(this);
    m_footerWidget->setFixedHeight(56);
    m_footerLayout = new QHBoxLayout(m_footerWidget);
    m_footerLayout->setContentsMargins(16, 0, 16, 0);
    m_footerLayout->setSpacing(8);
    m_footerLayout->addStretch();

    mainLayout->addWidget(m_headerWidget);
    mainLayout->addWidget(m_bodyWidget, 1);
    mainLayout->addWidget(m_footerWidget);
}

void BaseModalContent::setTitle(const QString &title) {
    m_titleLabel->setText(title);
}

void BaseModalContent::setHeaderVisible(bool visible) {
    m_headerWidget->setVisible(visible);
}

void BaseModalContent::accept() {
    m_result = Result::Accepted;
    emit accepted();
    emit finished(static_cast<int>(Result::Accepted));
}

void BaseModalContent::reject() {
    m_result = Result::Rejected;
    emit rejected();
    emit finished(static_cast<int>(Result::Rejected));
}

void BaseModalContent::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    QWidget *firstFocusable = findChild<QLineEdit*>();
    if (!firstFocusable) {
        firstFocusable = findChild<QPushButton*>();
    }
    if (firstFocusable) {
        firstFocusable->setFocus();
    }
}
