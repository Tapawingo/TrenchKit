#include "ToastWidget.h"
#include "core/utils/Theme.h"
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>

namespace {
constexpr int DISPLAY_MS = 3500;
constexpr int FADE_MS = 200;
}

void ToastWidget::show(QWidget *parent, const QString &text, bool isError) {
    if (!parent) {
        return;
    }
    auto *toast = new ToastWidget(parent, text, isError);
    toast->QWidget::show();
    toast->raise();

    auto *fadeIn = new QPropertyAnimation(toast->m_opacityEffect, "opacity", toast);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setDuration(FADE_MS);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(DISPLAY_MS, toast, [toast]() {
        auto *fadeOut = new QPropertyAnimation(toast->m_opacityEffect, "opacity", toast);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        fadeOut->setDuration(FADE_MS);
        connect(fadeOut, &QPropertyAnimation::finished, toast, &QObject::deleteLater);
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

ToastWidget::ToastWidget(QWidget *parent, const QString &text, bool isError)
    : QWidget(parent)
{
    const char *accent = isError ? Theme::Colors::ACCENT_RED : Theme::Colors::ACCENT_GREEN;

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);

    m_label = new QLabel(text, this);
    m_label->setWordWrap(true);
    m_label->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-weight: bold; }")
                           .arg(Theme::Colors::TEXT_PRIMARY));
    layout->addWidget(m_label);

    setStyleSheet(QString("ToastWidget { background-color: %1; border: 1px solid %2; border-radius: 6px; }")
                 .arg(Theme::Colors::BACKGROUND_SECONDARY, accent));

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    setFixedWidth(320);
    adjustSize();
    reposition();
}

void ToastWidget::reposition() {
    if (!parentWidget()) {
        return;
    }
    const int margin = 16;
    move(parentWidget()->width() - width() - margin, margin);
}
