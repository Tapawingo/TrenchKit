#include "ProgressModal.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ProgressModal::ProgressModal(const QString &labelText,
                            const QString &cancelButtonText,
                            int minimum,
                            int maximum,
                            QWidget *parent)
    : BaseModalContent(parent)
{
    setHeaderVisible(false);
    setupUi(labelText, cancelButtonText);
    setRange(minimum, maximum);
    setPreferredSize(QSize(400, 150));
}

void ProgressModal::setupUi(const QString &labelText, const QString &cancelButtonText) {
    m_label = new QLabel(labelText, this);
    m_label->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }")
                          .arg(Theme::Colors::TEXT_SECONDARY));
    bodyLayout()->addWidget(m_label);

    m_progressBar = new QProgressBar(this);
    bodyLayout()->addWidget(m_progressBar);

    if (!cancelButtonText.isEmpty()) {
        m_cancelButton = new QPushButton(cancelButtonText, this);
        m_cancelButton->setCursor(Qt::PointingHandCursor);
        connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
            emit canceled();
            reject();
        });
        footerLayout()->addWidget(m_cancelButton);
    } else {
        m_cancelButton = nullptr;
    }
}

void ProgressModal::setRange(int minimum, int maximum) {
    m_progressBar->setRange(minimum, maximum);
}

void ProgressModal::setValue(int value) {
    m_progressBar->setValue(value);
}

void ProgressModal::setLabelText(const QString &text) {
    m_label->setText(text);
}

void ProgressModal::setCancelButtonEnabled(bool enabled) {
    if (m_cancelButton) {
        m_cancelButton->setEnabled(enabled);
    }
}
