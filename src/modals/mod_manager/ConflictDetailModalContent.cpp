#include "ConflictDetailModalContent.h"
#include "core/utils/Theme.h"
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ConflictDetailModalContent::ConflictDetailModalContent(const QString &modName,
                                                      const ConflictInfo &conflictInfo,
                                                      QWidget *parent)
    : BaseModalContent(parent)
    , m_modName(modName)
    , m_conflictInfo(conflictInfo)
{
    setTitle(tr("Mod File Conflicts"));
    setupUi();
    setPreferredSize(QSize(700, 500));
}

void ConflictDetailModalContent::setupUi() {
    QString modNameText = tr("<b>Mod:</b> %1").arg(m_modName);
    if (m_conflictInfo.isEntirelyOverwritten()) {
        modNameText = tr("<b>Mod:</b> <span style='color: #f48771;'>%1</span>").arg(m_modName);
    }

    auto *modLabel = new QLabel(modNameText, this);
    modLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }")
                           .arg(Theme::Colors::TEXT_PRIMARY));
    bodyLayout()->addWidget(modLabel);

    auto *countLabel = new QLabel(
        tr("<b>Total Conflicts:</b> %1 file(s)")
            .arg(m_conflictInfo.fileConflictCount), this);
    countLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; margin-top: 4px; }")
                             .arg(Theme::Colors::TEXT_PRIMARY));
    bodyLayout()->addWidget(countLabel);

    auto *conflictingModsLabel = new QLabel(formatModListText(), this);
    conflictingModsLabel->setWordWrap(true);
    conflictingModsLabel->setTextFormat(Qt::RichText);
    conflictingModsLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; margin-top: 8px; }")
                                       .arg(Theme::Colors::TEXT_SECONDARY));
    bodyLayout()->addWidget(conflictingModsLabel);

    auto *fileListLabel = new QLabel(tr("<b>Conflicting Files:</b>"), this);
    fileListLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; margin-top: 12px; }")
                                .arg(Theme::Colors::TEXT_PRIMARY));
    bodyLayout()->addWidget(fileListLabel);

    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::NoSelection);
    m_fileList->setStyleSheet(QString(
        "QListWidget {"
        "    background-color: #16130e;"
        "    color: %1;"
        "    border: 1px solid #404040;"
        "    font-size: 12px;"
        "    font-family: 'Consolas', 'DejaVu Sans Mono', 'Liberation Mono', 'Courier New', monospace;"
        "}"
        "QListWidget::item {"
        "    padding: 4px;"
        "    border-bottom: 1px solid #2c2c2c;"
        "}"
    ).arg(Theme::Colors::TEXT_SECONDARY));

    for (const QString &filePath : m_conflictInfo.allConflictingFilePaths) {
        auto *item = new QListWidgetItem(filePath);

        if (m_conflictInfo.overwrittenFilePaths.contains(filePath)) {
            item->setForeground(QColor("#f48771"));
            item->setText(filePath + tr(" (overwritten)"));
        }

        m_fileList->addItem(item);
    }

    bodyLayout()->addWidget(m_fileList, 1);

    auto *okButton = new QPushButton(tr("OK"), this);
    okButton->setCursor(Qt::PointingHandCursor);
    connect(okButton, &QPushButton::clicked, this, &ConflictDetailModalContent::accept);

    footerLayout()->addStretch();
    footerLayout()->addWidget(okButton);
}

QString ConflictDetailModalContent::formatModListText() const {
    QString text = tr("<b>Conflicts with:</b>") + "<br>";

    for (int i = 0; i < m_conflictInfo.conflictingModNames.size(); ++i) {
        QString modName = m_conflictInfo.conflictingModNames[i];
        int otherPriority = m_conflictInfo.conflictingModPriorities[i];

        QString loadOrder;
        QString color = "";

        if (otherPriority < m_conflictInfo.modPriority) {
            loadOrder = " <i>" + tr("(loads before this mod, overridden)") + "</i>";
        } else {
            loadOrder = " <i>" + tr("(loads after this mod, takes priority)") + "</i>";
            if (m_conflictInfo.isEntirelyOverwritten()) {
                color = " style='color: #f48771;'";
            }
        }

        text += QString("• <span%1>%2</span>%3<br>").arg(color, modName, loadOrder);
    }

    return text;
}
