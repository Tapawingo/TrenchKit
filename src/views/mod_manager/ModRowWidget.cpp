#include "ModRowWidget.h"
#include "ConflictTooltip.h"
#include "core/utils/Theme.h"
#include "core/services/ModConflictDetector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QTimer>

ModRowWidget::ModRowWidget(const ModInfo &mod, QWidget *parent)
    : QWidget(parent)
    , m_modId(mod.id)
    , m_fullModName(mod.name)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setupUi(mod);
}

void ModRowWidget::setupUi(const ModInfo &mod) {
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(
        Theme::Spacing::MOD_ROW_PADDING_HORIZONTAL,
        Theme::Spacing::MOD_ROW_PADDING_VERTICAL,
        3,
        Theme::Spacing::MOD_ROW_PADDING_VERTICAL
    );
    mainLayout->setSpacing(Theme::Spacing::MOD_ROW_INTERNAL_SPACING);

    auto *leftSection = new QVBoxLayout();
    leftSection->setSpacing(0);

    auto *topLayout = new QHBoxLayout();
    topLayout->setSpacing(Theme::Spacing::MOD_ROW_INTERNAL_SPACING);

    m_enabledCheckBox = new QCheckBox(this);
    m_enabledCheckBox->setChecked(mod.enabled);

    m_nameLabel = new QLabel(mod.name, this);
    m_nameLabel->setObjectName("modNameLabel");
    m_nameLabel->setToolTip(mod.name);
    m_nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    topLayout->addWidget(m_enabledCheckBox);
    topLayout->addWidget(m_nameLabel, 1);

    leftSection->addLayout(topLayout);

    m_dateLabel = new QLabel(this);
    m_dateLabel->setObjectName("modDateLabel");
    QString subtitle = tr("Installed: %1").arg(mod.installDate.toString("yyyy-MM-dd hh:mm"));
    const QString author = mod.author.trimmed();
    if (!author.isEmpty()) {
        subtitle = tr("by %1 · Installed: %2").arg(author, mod.installDate.toString("yyyy-MM-dd hh:mm"));
    }
    m_dateLabel->setText(subtitle);

    leftSection->addWidget(m_dateLabel);

    mainLayout->addLayout(leftSection, 1);

    m_conflictButton = new QPushButton(this);
    m_conflictButton->setObjectName("modConflictButton");
    m_conflictButton->setVisible(false);
    m_conflictButton->setIcon(QIcon(":/icon_conflict.png"));
    m_conflictButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_conflictButton->setCursor(Qt::PointingHandCursor);
    m_conflictButton->setFlat(true);
    m_conflictButton->setFocusPolicy(Qt::NoFocus);

    m_conflictTooltip = new ConflictTooltip(this);
    m_conflictTooltip->hide();
    m_conflictButton->installEventFilter(this);

    connect(m_conflictButton, &QPushButton::clicked, this, [this]() {
        emit conflictDetailsRequested(m_modId);
    });

    mainLayout->addWidget(m_conflictButton);

    m_dependencyButton = new QPushButton(this);
    m_dependencyButton->setObjectName("modDependencyButton");
    m_dependencyButton->setVisible(false);
    m_dependencyButton->setIcon(QIcon(":/icon_dependency_missing.png"));
    m_dependencyButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_dependencyButton->setCursor(Qt::PointingHandCursor);
    m_dependencyButton->setFlat(true);
    m_dependencyButton->setFocusPolicy(Qt::NoFocus);
    m_dependencyTooltip = new ConflictTooltip(this);
    m_dependencyTooltip->hide();
    m_dependencyButton->installEventFilter(this);

    mainLayout->addWidget(m_dependencyButton);

    m_noticeButton = new QPushButton(this);
    m_noticeButton->setObjectName("modNoticeButton");
    m_noticeButton->setVisible(false);
    m_noticeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_noticeButton->setCursor(Qt::PointingHandCursor);
    m_noticeButton->setFlat(true);
    m_noticeButton->setFocusPolicy(Qt::NoFocus);
    m_noticeTooltip = new ConflictTooltip(this);
    m_noticeTooltip->hide();
    m_noticeButton->installEventFilter(this);

    mainLayout->addWidget(m_noticeButton);

    m_updateButton = new QPushButton(this);
    m_updateButton->setObjectName("modUpdateButton");
    m_updateButton->setVisible(false);
    m_updateButton->setIcon(QIcon(":/icon_update.png"));
    m_updateButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_updateButton->setCursor(Qt::PointingHandCursor);

    mainLayout->addWidget(m_updateButton);
    mainLayout->addSpacing(6);

    setProperty("selected", false);

    connect(m_enabledCheckBox, &QCheckBox::checkStateChanged,
            this, [this](Qt::CheckState state) {
        emit enabledChanged(m_modId, state == Qt::Checked);
    });

    connect(m_updateButton, &QPushButton::clicked, this, [this]() {
        emit updateRequested(m_modId);
    });

    retranslateUi();
}

void ModRowWidget::updateModInfo(const ModInfo &mod) {
    m_enabledCheckBox->setChecked(mod.enabled);
    m_fullModName = mod.name;
    m_nameLabel->setToolTip(mod.name);
    m_nameLabel->setText(mod.name);
    QString subtitle = tr("Installed: %1").arg(mod.installDate.toString("yyyy-MM-dd hh:mm"));
    const QString author = mod.author.trimmed();
    if (!author.isEmpty()) {
        subtitle = tr("by %1 · Installed: %2").arg(author, mod.installDate.toString("yyyy-MM-dd hh:mm"));
    }
    m_dateLabel->setText(subtitle);
    setNotice(mod.noticeText, mod.noticeIcon);
}

void ModRowWidget::setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        updateStyling();
    }
}

void ModRowWidget::updateStyling() {
    setProperty("selected", m_selected);
    style()->unpolish(this);
    style()->polish(this);
}

void ModRowWidget::updateNameEliding() {
    if (m_nameLabel && !m_fullModName.isEmpty() && m_nameLabel->width() > 0) {
        QFontMetrics metrics(m_nameLabel->font());
        QString elidedText = metrics.elidedText(m_fullModName, Qt::ElideRight, m_nameLabel->width());
        m_nameLabel->setText(elidedText);
    }
}

void ModRowWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ModRowWidget::retranslateUi() {
}

void ModRowWidget::contextMenuEvent(QContextMenuEvent *event) {
    emit contextMenuRequested(event->globalPos());
}

void ModRowWidget::setUpdateAvailable(bool available, const QString &version) {
    if (m_updateButton) {
        bool wasVisible = m_updateButton->isVisible();
        m_updateButton->setVisible(available);
        if (available) {
            if (!version.isEmpty()) {
                m_updateButton->setToolTip(tr("Update to version %1").arg(version));
            }
            int buttonHeight = m_updateButton->height();
            if (buttonHeight > 0) {
                m_updateButton->setFixedWidth(buttonHeight);
            }
        }
        if (wasVisible != available) {
            QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
        }
    }
}

void ModRowWidget::hideUpdateButton() {
    if (m_updateButton && m_updateButton->isVisible()) {
        m_updateButton->setVisible(false);
        QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
    }
}

void ModRowWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    if (m_updateButton && m_updateButton->isVisible()) {
        int buttonHeight = m_updateButton->height();
        m_updateButton->setFixedWidth(buttonHeight);
    }

    if (m_conflictButton && m_conflictButton->isVisible()) {
        int buttonHeight = m_conflictButton->height();
        m_conflictButton->setFixedWidth(buttonHeight);
    }

    if (m_dependencyButton && m_dependencyButton->isVisible()) {
        int buttonHeight = m_dependencyButton->height();
        m_dependencyButton->setFixedWidth(buttonHeight);
    }

    if (m_noticeButton && m_noticeButton->isVisible()) {
        int buttonHeight = m_noticeButton->height();
        m_noticeButton->setFixedWidth(buttonHeight);
    }

    updateNameEliding();
}

bool ModRowWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_conflictButton || watched == m_noticeButton || watched == m_dependencyButton) {
        auto *tooltip = m_conflictTooltip;
        auto *button = m_conflictButton;
        const QString *tooltipText = &m_conflictTooltipText;

        if (watched == m_noticeButton) {
            tooltip = m_noticeTooltip;
            button = m_noticeButton;
            tooltipText = &m_noticeText;
        } else if (watched == m_dependencyButton) {
            tooltip = m_dependencyTooltip;
            button = m_dependencyButton;
            tooltipText = &m_dependencyText;
        }

        switch (event->type()) {
            case QEvent::Enter:
            case QEvent::ToolTip: {
                if (tooltip && button && button->isVisible() && !tooltipText->isEmpty()) {
                    tooltip->setText(*tooltipText);
                    QPoint pos = button->mapToGlobal(QPoint(button->width() + 8, 0));
                    tooltip->showAt(pos);
                }
                return event->type() == QEvent::ToolTip;
            }
            case QEvent::Leave:
            case QEvent::Hide:
            case QEvent::MouseButtonPress:
            case QEvent::FocusOut:
                if (tooltip) {
                    tooltip->hide();
                }
                break;
            default:
                break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ModRowWidget::setConflictInfo(const ConflictInfo &info) {
    if (!m_conflictButton) {
        return;
    }

    bool hasConflicts = info.hasConflicts();
    bool wasVisible = m_conflictButton->isVisible();
    m_conflictButton->setVisible(hasConflicts);

    if (hasConflicts) {
        m_conflictTooltipText = formatConflictTooltip(info);
        if (m_conflictTooltip && m_conflictTooltip->isVisible()) {
            m_conflictTooltip->setText(m_conflictTooltipText);
        }

        if (info.isEntirelyOverwritten()) {
            m_nameLabel->setStyleSheet("QLabel#modNameLabel { color: #f48771; }");
        } else {
            m_nameLabel->setStyleSheet("");
        }

        int buttonHeight = m_conflictButton->height();
        if (buttonHeight > 0) {
            m_conflictButton->setFixedWidth(buttonHeight);
        }
    } else {
        m_conflictTooltipText.clear();
        m_nameLabel->setStyleSheet("");
        if (m_conflictTooltip) {
            m_conflictTooltip->hide();
        }
    }

    if (wasVisible != hasConflicts) {
        QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
    }
}

void ModRowWidget::setNotice(const QString &text, const QString &iconType) {
    if (!m_noticeButton) {
        return;
    }

    m_noticeText = text.trimmed();
    if (m_noticeText.isEmpty()) {
        if (m_noticeTooltip) {
            m_noticeTooltip->hide();
        }
        if (m_noticeButton->isVisible()) {
            m_noticeButton->setVisible(false);
            QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
        }
        return;
    }

    const QString iconPath = noticeIconPath(iconType);
    if (!iconPath.isEmpty()) {
        m_noticeButton->setIcon(QIcon(iconPath));
    }
    m_noticeButton->setToolTip(QString());
    if (m_noticeTooltip && m_noticeTooltip->isVisible()) {
        m_noticeTooltip->setText(m_noticeText);
    }

    if (!m_noticeButton->isVisible()) {
        m_noticeButton->setVisible(true);
        QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
    }

    int buttonHeight = m_noticeButton->height();
    if (buttonHeight > 0) {
        m_noticeButton->setFixedWidth(buttonHeight);
    }
}

void ModRowWidget::clearConflictIndicator() {
    bool wasVisible = false;
    if (m_conflictButton) {
        wasVisible = m_conflictButton->isVisible();
        m_conflictButton->setVisible(false);
    }
    m_conflictTooltipText.clear();
    m_nameLabel->setStyleSheet("");
    if (m_conflictTooltip) {
        m_conflictTooltip->hide();
    }
    if (wasVisible) {
        QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
    }
}

void ModRowWidget::setDependencyStatus(const QString &text) {
    if (!m_dependencyButton) {
        return;
    }

    m_dependencyText = text.trimmed();
    if (m_dependencyText.isEmpty()) {
        if (m_dependencyTooltip) {
            m_dependencyTooltip->hide();
        }
        if (m_dependencyButton->isVisible()) {
            m_dependencyButton->setVisible(false);
            QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
        }
        return;
    }

    m_dependencyButton->setToolTip(QString());
    if (m_dependencyTooltip && m_dependencyTooltip->isVisible()) {
        m_dependencyTooltip->setText(m_dependencyText);
    }
    if (!m_dependencyButton->isVisible()) {
        m_dependencyButton->setVisible(true);
        QTimer::singleShot(0, this, &ModRowWidget::updateNameEliding);
    }

    int buttonHeight = m_dependencyButton->height();
    if (buttonHeight > 0) {
        m_dependencyButton->setFixedWidth(buttonHeight);
    }
}

QString ModRowWidget::noticeIconPath(const QString &iconType) const {
    const QString icon = iconType.trimmed().toLower();
    if (icon == QStringLiteral("warning")) {
        return QStringLiteral(":/icon_notice_warning.png");
    }
    if (icon == QStringLiteral("error")) {
        return QStringLiteral(":/icon_notice_error.png");
    }
    if (icon == QStringLiteral("question")) {
        return QStringLiteral(":/icon_notice_question.png");
    }
    if (icon == QStringLiteral("lightbulb")) {
        return QStringLiteral(":/icon_notice_lightbulb.png");
    }
    return QStringLiteral(":/icon_notice_info.png");
}

QString ModRowWidget::formatConflictTooltip(const ConflictInfo &info) const {
    QString tooltip = QStringLiteral("<b><img src=\":/icon_conflict.png\" width=\"14\" height=\"14\"> ")
        + tr("File Conflicts (%1 files)").arg(info.fileConflictCount)
        + QStringLiteral("</b><br><br>");

    tooltip += QStringLiteral("<b>") + tr("Conflicts with:") + QStringLiteral("</b><br>");

    for (int i = 0; i < static_cast<int>(info.conflictingModNames.size()); ++i) {
        QString modName = info.conflictingModNames[i];
        int otherPriority = info.conflictingModPriorities[i];

        QString loadOrder;
        QString color = "";

        if (otherPriority < info.modPriority) {
            loadOrder = QStringLiteral(" <i>") + tr("(loads before this)") + QStringLiteral("</i>");
        } else {
            loadOrder = QStringLiteral(" <i>") + tr("(loads after this)") + QStringLiteral("</i>");
            if (info.isEntirelyOverwritten()) {
                color = " style='color: #f48771;'";
            }
        }

        tooltip += QString("• <span%1>%2</span>%3<br>").arg(color, modName, loadOrder);
    }

    if (!info.conflictingFilePaths.isEmpty()) {
        tooltip += QStringLiteral("<br><b>") + tr("Sample conflicts:") + QStringLiteral("</b><br>");
        int sampleCount = qMin(5, static_cast<int>(info.conflictingFilePaths.size()));
        for (int i = 0; i < sampleCount; ++i) {
            const QString &filePath = info.conflictingFilePaths[i];

            if (info.overwrittenFilePaths.contains(filePath)) {
                tooltip += QStringLiteral("• <span style='color: #f48771;'>")
                    + tr("%1 (overwritten)").arg(filePath)
                    + QStringLiteral("</span><br>");
            } else {
                tooltip += QString("• %1<br>").arg(filePath);
            }
        }
        if (static_cast<int>(info.conflictingFilePaths.size()) > 5) {
            tooltip += QStringLiteral("• ") + tr("... and %1 more").arg(static_cast<int>(info.conflictingFilePaths.size()) - 5) + QStringLiteral("<br>");
        }
    }

    return tooltip;
}
