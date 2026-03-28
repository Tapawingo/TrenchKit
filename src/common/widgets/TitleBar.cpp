#include "TitleBar.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>
#include <QSizePolicy>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_iconLabel(new QLabel(this))
    , m_titleLabel(new QLabel(this))
    , m_updateButton(new QPushButton(this))
    , m_settingsButton(new QPushButton(this))
    , m_minimizeButton(new QPushButton(this))
    , m_closeButton(new QPushButton(this))
    , m_layout(new QHBoxLayout(this))
{
    setupUi();
    setupConnections();
}

void TitleBar::setupUi() {
    setObjectName("titleBar");

    QPixmap toolboxPixmap(":/logo_toolbox.png");
    int toolboxHeight = 38;
    int toolboxWidth = toolboxPixmap.width() * toolboxHeight / toolboxPixmap.height();
    m_iconLabel->setPixmap(toolboxPixmap.scaled(toolboxWidth, toolboxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_iconLabel->setFixedSize(toolboxWidth, toolboxHeight);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setObjectName("titleBarIcon");

    QPixmap titlePixmap(":/logo_title.png");
    int titleHeight = 38;
    int titleWidth = titlePixmap.width() * titleHeight / titlePixmap.height();
    m_titleLabel->setPixmap(titlePixmap.scaled(titleWidth, titleHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_titleLabel->setFixedSize(titleWidth, titleHeight);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("titleBarTitle");

    QIcon updateIcon(":/icon_update.png");
    m_updateButton->setText("");
    m_updateButton->setIcon(updateIcon);
    m_updateButton->setFixedSize(48, 48);
    m_updateButton->setObjectName("updateButton");
    m_updateButton->setFocusPolicy(Qt::NoFocus);
    m_updateButton->setVisible(false);
    m_updateButton->setCursor(Qt::PointingHandCursor);

    QIcon settingsIcon(":/icon_settings.png");
    m_settingsButton->setText("");
    m_settingsButton->setIcon(settingsIcon);
    m_settingsButton->setFixedSize(48, 48);
    m_settingsButton->setObjectName("settingsButton");
    m_settingsButton->setFocusPolicy(Qt::NoFocus);
    m_settingsButton->setCursor(Qt::PointingHandCursor);

    m_minimizeButton->setText("−");
    m_minimizeButton->setFixedSize(48, 48);
    m_minimizeButton->setObjectName("minimizeButton");
    m_minimizeButton->setFocusPolicy(Qt::NoFocus);
    m_minimizeButton->setCursor(Qt::PointingHandCursor);

    m_closeButton->setText("×");
    m_closeButton->setFixedSize(48, 48);
    m_closeButton->setObjectName("closeButton");
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setCursor(Qt::PointingHandCursor);

    m_layout->setContentsMargins(
        Theme::Spacing::TITLE_BAR_MARGIN_LEFT, 
        Theme::Spacing::TITLE_BAR_MARGIN_TOP,
        Theme::Spacing::TITLE_BAR_MARGIN_RIGHT,
        Theme::Spacing::TITLE_BAR_MARGIN_BOTTOM
    );
    m_layout->setSpacing(0);
    m_layout->setAlignment(Qt::AlignVCenter);
    m_layout->addWidget(m_iconLabel);
    m_layout->addSpacing(Theme::Spacing::TITLE_BAR_ICON_SPACING);
    m_layout->addWidget(m_titleLabel);
    m_layout->addStretch();
    m_layout->addWidget(m_updateButton);
    m_layout->addWidget(m_settingsButton);
    m_layout->addWidget(m_minimizeButton);
    m_layout->addWidget(m_closeButton);

    setLayout(m_layout);
}

void TitleBar::setupConnections() {
    connect(m_updateButton, &QPushButton::clicked, this, &TitleBar::updateClicked);
    connect(m_settingsButton, &QPushButton::clicked, this, &TitleBar::settingsClicked);
    connect(m_minimizeButton, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setUpdateVisible(bool visible) {
    m_updateButton->setVisible(visible);
}

void TitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        window()->move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}
