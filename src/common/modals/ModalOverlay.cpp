#include "ModalOverlay.h"
#include "ModalContainer.h"
#include "BaseModalContent.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QPushButton>

ModalOverlay::ModalOverlay(BaseModalContent *content, QWidget *parent)
    : QWidget(parent)
    , m_content(content)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Widget);

    m_container = new ModalContainer(content, this);

    m_fadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_content, &BaseModalContent::finished, this, [this](int result) {
        Q_UNUSED(result);
        close();
    });

    if (parent) {
        resize(parent->size());
        centerContainer();
    }
}

void ModalOverlay::show() {
    QWidget::show();
    raise();
    fadeIn();
}

void ModalOverlay::close() {
    fadeOut();
}

void ModalOverlay::fadeIn() {
    m_fadeAnimation->stop();
    m_fadeAnimation->setDuration(100);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void ModalOverlay::fadeOut() {
    m_fadeAnimation->stop();
    m_fadeAnimation->setDuration(75);
    m_fadeAnimation->setStartValue(m_opacity);
    m_fadeAnimation->setEndValue(0.0);

    disconnect(m_fadeAnimation, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        emit closed(static_cast<int>(m_content->result()));
        deleteLater();
    });

    m_fadeAnimation->start();
}

void ModalOverlay::setOpacity(qreal opacity) {
    m_opacity = opacity;
    update();
}

void ModalOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    QColor bgColor(0, 0, 0, static_cast<int>(128 * m_opacity));
    painter.fillRect(rect(), bgColor);
}

void ModalOverlay::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        m_content->reject();
        event->accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QWidget *focused = focusWidget();
        if (focused && !qobject_cast<QPushButton*>(focused)) {
            m_content->accept();
            event->accept();
        } else {
            QWidget::keyPressEvent(event);
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}

void ModalOverlay::mousePressEvent(QMouseEvent *event) {
    QRect containerGeometry = m_container->geometry();
    if (!containerGeometry.contains(event->pos())) {
        m_content->reject();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ModalOverlay::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    centerContainer();
}

void ModalOverlay::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    centerContainer();
}

void ModalOverlay::centerContainer() {
    if (!m_container) return;

    int maxHeight = static_cast<int>(height() * 0.8);
    m_container->setMaximumHeight(maxHeight);

    int x = (width() - m_container->width()) / 2;
    int y = (height() - m_container->height()) / 2;
    m_container->move(x, y);
}
