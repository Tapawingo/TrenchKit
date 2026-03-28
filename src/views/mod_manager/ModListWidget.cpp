#include "ModListWidget.h"
#include "ModRowWidget.h"
#include "common/widgets/GradientFrame.h"
#include "common/modals/ModalManager.h"
#include "common/modals/MessageModal.h"
#include "common/modals/InputModal.h"
#include "modals/mod_manager/AddModModalContent.h"
#include "modals/mod_manager/ModMetadataModalContent.h"
#include "modals/mod_manager/ModUpdateModalContent.h"
#include "modals/mod_manager/ItchModUpdateModalContent.h"
#include "modals/mod_manager/FileSelectionModalContent.h"
#include "modals/mod_manager/ConflictDetailModalContent.h"
#include "modals/mod_manager/NexusRegistrationModalContent.h"
#include "modals/mod_manager/ItchRegistrationModalContent.h"
#include "core/utils/Theme.h"
#include "core/utils/ArchiveExtractor.h"
#include "core/services/ModUpdateService.h"
#include "core/models/ModUpdateInfo.h"
#include "core/services/ItchModUpdateService.h"
#include "core/models/ItchUpdateInfo.h"
#include "core/models/NexusFileInfo.h"
#include "core/services/ModConflictDetector.h"
#include "common/modals/BaseModalContent.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QLabel>
#include <QTimer>
#include <QFileInfo>
#include <QPushButton>
#include <QScrollBar>
#include <QLineEdit>
#include <QKeyEvent>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QSet>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QVersionNumber>
#include <functional>
#include <memory>

ModListWidget::ModListWidget(QWidget *parent)
    : QWidget(parent)
    , m_loadingLabel(new QLabel(this))
    , m_modCountLabel(new QLabel(this))
    , m_loadingTimer(new QTimer(this))
    , m_conflictDetector(nullptr)
{
    setupUi();

    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setObjectName("modLoadingLabel");
    m_loadingLabel->hide();

    connect(m_loadingTimer, &QTimer::timeout, this, &ModListWidget::updateLoadingAnimation);
}

void ModListWidget::setModManager(ModManager *modManager) {
    if (m_modManager) {
        disconnect(m_modManager, nullptr, this, nullptr);
    }

    m_modManager = modManager;

    if (m_modManager) {
        connect(m_modManager, &ModManager::modsChanged,
                this, &ModListWidget::onModsChanged);
        connect(m_modManager, &ModManager::errorOccurred,
                this, [this](const QString &error) {
            if (m_modalManager) {
                MessageModal::warning(m_modalManager, "Error", error);
            }
        });

        if (!m_conflictDetector) {
            m_conflictDetector = new ModConflictDetector(m_modManager, this);
            connect(m_conflictDetector, &ModConflictDetector::scanComplete,
                    this, &ModListWidget::onConflictScanComplete);
        }

        connect(m_modManager, &ModManager::modsChanged,
                m_conflictDetector, &ModConflictDetector::scanForConflicts);

        refreshModList();
        m_conflictDetector->scanForConflicts();
    }
}

void ModListWidget::setupUi() {
    GradientFrame *frame = new GradientFrame(this);
    QVBoxLayout *frameLayout = new QVBoxLayout(frame);

    auto *titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(Theme::Spacing::MOD_LIST_TITLE_SPACING);
    titleLayout->setContentsMargins(Theme::Spacing::MOD_ROW_PADDING_HORIZONTAL, 0, 0, 0);

    m_enableAllCheckBox = new QCheckBox(this);
    m_enableAllCheckBox->setObjectName("modEnableAllCheckBox");
    m_enableAllCheckBox->setTristate(true);
    m_enableAllCheckBox->setCursor(Qt::PointingHandCursor);
    m_enableAllCheckBox->setToolTip(tr("Enable or disable all mods"));

    m_titleLabel = new QLabel(tr("Installed Mods:"), this);
    m_titleLabel->setObjectName("modListTitle");

    m_modCountLabel->setObjectName("modCountLabel");
    m_modCountLabel->setText(QStringLiteral("0"));

    m_checkUpdatesButton = new QPushButton(tr("Check for Updates"), this);
    m_checkUpdatesButton->setObjectName("checkUpdatesButton");
    m_checkUpdatesButton->setCursor(Qt::PointingHandCursor);

    titleLayout->addWidget(m_enableAllCheckBox);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_modCountLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_checkUpdatesButton);

    connect(m_checkUpdatesButton, &QPushButton::clicked,
            this, &ModListWidget::onCheckUpdatesClicked);
    connect(m_enableAllCheckBox, &QCheckBox::clicked,
            this, &ModListWidget::onEnableAllClicked);

    m_searchContainer = new QWidget(this);
    auto *searchLayout = new QHBoxLayout(m_searchContainer);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(Theme::Spacing::MOD_LIST_TITLE_SPACING);

    m_searchEdit = new QLineEdit(m_searchContainer);
    m_searchEdit->setPlaceholderText("Search mods...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->installEventFilter(this);

    searchLayout->addWidget(m_searchEdit);
    m_searchContainer->setVisible(false);

    m_modList = new DraggableModList(this);
    m_modList->setSpacing(Theme::Spacing::MOD_LIST_ITEM_SPACING);
    m_modList->setUniformItemSizes(false);

    frameLayout->addLayout(titleLayout);
    frameLayout->addWidget(m_searchContainer);
    frameLayout->addWidget(m_modList, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(frame);

    connect(m_modList, &DraggableModList::itemsReordered,
            this, &ModListWidget::onItemsReordered);

    connect(m_modList, &DraggableModList::filesDropped,
            this, &ModListWidget::onFilesDropped);

    connect(m_modList, &QListWidget::itemSelectionChanged,
            this, [this]() {
        int totalMods = m_modList->count();
        int selectedCount = getSelectedCount();
        int selectedRow = selectedCount == 1 ? getSelectedRow() : -1;

        for (int i = 0; i < totalMods; ++i) {
            QListWidgetItem *item = m_modList->item(i);
            if (item) {
                auto *rowWidget = qobject_cast<ModRowWidget*>(m_modList->itemWidget(item));
                if (rowWidget) {
                    rowWidget->setSelected(item->isSelected());
                }
            }
        }

        emit modSelectionChanged(selectedRow, totalMods);
    });

    m_modList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_modList, &QListWidget::customContextMenuRequested,
            this, &ModListWidget::onContextMenuRequested);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filterText = text.trimmed();
        refreshModList();
        updateDragMode();
    });

}

void ModListWidget::refreshModList() {
    if (!m_modManager) {
        return;
    }

    m_updating = true;

    int currentRow = getSelectedRow();
    int scrollPosition = m_modList->verticalScrollBar()->value();
    QStringList selectedIds = getSelectedModIds();
    QSet<QString> selectedIdSet(selectedIds.begin(), selectedIds.end());
    bool hasSavedSelection = !selectedIdSet.isEmpty();
    m_modList->clear();

    QList<ModInfo> mods = m_modManager->getMods();
    QHash<QString, ModInfo> manifestIndex;
    manifestIndex.reserve(mods.size());
    for (const ModInfo &mod : mods) {
        if (!mod.manifestId.isEmpty()) {
            manifestIndex.insert(mod.manifestId.trimmed().toLower(), mod);
        }
    }
    int enabledCount = 0;
    for (const ModInfo &mod : mods) {
        if (mod.enabled) {
            enabledCount++;
        }
    }
    QList<ModInfo> filteredMods;
    filteredMods.reserve(mods.size());
    for (const ModInfo &mod : mods) {
        if (!isFilterActive()) {
            filteredMods.append(mod);
            continue;
        }

        const QString name = mod.name;
        const QString fileName = mod.fileName;
        if (name.contains(m_filterText, Qt::CaseInsensitive) ||
            fileName.contains(m_filterText, Qt::CaseInsensitive)) {
            filteredMods.append(mod);
        }
    }

    if (isFilterActive()) {
        m_modCountLabel->setText(QString("%1 / %2")
            .arg(filteredMods.size())
            .arg(mods.size()));
    } else {
        m_modCountLabel->setText(QString::number(mods.size()) + " Total");
    }

    if (m_enableAllCheckBox) {
        QSignalBlocker blocker(m_enableAllCheckBox);
        if (mods.isEmpty()) {
            m_enableAllCheckBox->setEnabled(false);
            m_enableAllCheckBox->setCheckState(Qt::Unchecked);
        } else {
            m_enableAllCheckBox->setEnabled(true);
            if (enabledCount == 0) {
                m_enableAllCheckBox->setCheckState(Qt::Unchecked);
            } else if (enabledCount == mods.size()) {
                m_enableAllCheckBox->setCheckState(Qt::Checked);
            } else {
                m_enableAllCheckBox->setCheckState(Qt::PartiallyChecked);
            }
        }
    }

    for (int i = 0; i < filteredMods.size(); ++i) {
        const ModInfo &mod = filteredMods[i];
        auto *modRow = new ModRowWidget(mod, this);
        connect(modRow, &ModRowWidget::enabledChanged,
                this, &ModListWidget::onModEnabledChanged);
        connect(modRow, &ModRowWidget::renameRequested,
                this, &ModListWidget::onRenameRequested);
        connect(modRow, &ModRowWidget::editMetaRequested,
                this, &ModListWidget::onEditMetaRequested);
        connect(modRow, &ModRowWidget::removeRequested,
                this, &ModListWidget::onRemoveRequested);
        connect(modRow, &ModRowWidget::updateRequested,
                this, &ModListWidget::onUpdateRequested);
        connect(modRow, &ModRowWidget::conflictDetailsRequested,
                this, &ModListWidget::onConflictDetailsRequested);
        connect(modRow, &ModRowWidget::registerWithNexusRequested,
                this, &ModListWidget::onRegisterWithNexusRequested);
        connect(modRow, &ModRowWidget::registerWithItchRequested,
                this, &ModListWidget::onRegisterWithItchRequested);
        connect(modRow, &ModRowWidget::contextMenuRequested,
                this, [this](const QPoint &globalPos) {
            if (!m_modList) {
                return;
            }
            QPoint localPos = m_modList->viewport()->mapFromGlobal(globalPos);
            QListWidgetItem *item = m_modList->itemAt(localPos);
            if (item && !item->isSelected()) {
                m_modList->clearSelection();
                item->setSelected(true);
                m_modList->setCurrentItem(item);
            }
            showSelectionContextMenu(globalPos);
        });

        auto *item = new QListWidgetItem(m_modList);
        QSize rowSize = modRow->sizeHint();
        int rowWidth = m_modList->viewport()->width();
        if (rowWidth > 0) {
            rowSize.setWidth(rowWidth);
        }
        item->setSizeHint(rowSize);
        item->setData(Qt::UserRole, mod.id);

        m_modList->addItem(item);
        m_modList->setItemWidget(item, modRow);

        if (m_updateService && m_updateService->hasUpdate(mod.id)) {
            ModUpdateInfo updateInfo = m_updateService->getUpdateInfo(mod.id);
            modRow->setUpdateAvailable(true, updateInfo.availableVersion);
        } else if (m_itchUpdateService && m_itchUpdateService->hasUpdate(mod.id)) {
            ItchUpdateInfo updateInfo = m_itchUpdateService->getUpdateInfo(mod.id);
            modRow->setUpdateAvailable(true, updateInfo.availableVersion);
        }

        if (m_conflictDetector) {
            ConflictInfo conflictInfo = m_conflictDetector->getConflictInfo(mod.id);
            modRow->setConflictInfo(conflictInfo);
        }

        modRow->setNotice(mod.noticeText, mod.noticeIcon);
        {
            QStringList missingDeps;
            QStringList versionIssues;
            QStringList dependencyDisabledIssues;
            QStringList loadOrderIssues;

            for (const auto &dep : mod.manifestDependencies) {
                if (!dep.required) {
                    continue;
                }
                const QString depId = dep.id.trimmed();
                if (depId.isEmpty()) {
                    continue;
                }
                const QString depKey = depId.toLower();
                if (!manifestIndex.contains(depKey)) {
                    missingDeps.append(depId);
                    continue;
                }

                const ModInfo depMod = manifestIndex.value(depKey);
                if (!dep.minVersion.isEmpty() || !dep.maxVersion.isEmpty()) {
                    QVersionNumber depVersion = QVersionNumber::fromString(depMod.version);
                    QVersionNumber minVersion = QVersionNumber::fromString(dep.minVersion);
                    QVersionNumber maxVersion = QVersionNumber::fromString(dep.maxVersion);

                    bool minOk = true;
                    bool maxOk = true;

                    if (!dep.minVersion.isEmpty() && !minVersion.isNull() && !depVersion.isNull()) {
                        minOk = QVersionNumber::compare(depVersion, minVersion) >= 0;
                    }
                    if (!dep.maxVersion.isEmpty() && !maxVersion.isNull() && !depVersion.isNull()) {
                        maxOk = QVersionNumber::compare(depVersion, maxVersion) <= 0;
                    }

                    if (!minOk || !maxOk) {
                        QString requirement = depId;
                        if (!dep.minVersion.isEmpty()) {
                            requirement += QString(" >= %1").arg(dep.minVersion);
                        }
                        if (!dep.maxVersion.isEmpty()) {
                            requirement += QString(" <= %1").arg(dep.maxVersion);
                        }
                        if (!depMod.version.isEmpty()) {
                            requirement += QString(" (have %1)").arg(depMod.version);
                        }
                        versionIssues.append(requirement);
                    }
                }

                if (mod.enabled) {
                    if (!depMod.enabled) {
                        dependencyDisabledIssues.append(depId);
                    }

                    if (depMod.priority >= mod.priority) {
                        loadOrderIssues.append(
                            tr("%1 (dependency: %2, this mod: %3)")
                                .arg(depId,
                                     QString::number(depMod.priority),
                                     QString::number(mod.priority))
                        );
                    }
                }
            }

            QStringList lines;
            if (!missingDeps.isEmpty()) {
                lines.append(tr("Missing dependencies: %1").arg(missingDeps.join(", ")));
            }
            if (!versionIssues.isEmpty()) {
                lines.append(tr("Dependency version mismatch: %1").arg(versionIssues.join(", ")));
            }
            if (!dependencyDisabledIssues.isEmpty()) {
                lines.append(tr("Required dependencies disabled: %1").arg(dependencyDisabledIssues.join(", ")));
            }
            if (!loadOrderIssues.isEmpty()) {
                lines.append(tr("Dependency load order incorrect (must load before this mod): %1")
                                 .arg(loadOrderIssues.join(", ")));
            }
            modRow->setDependencyStatus(lines.join("\n"));
        }

        if (hasSavedSelection) {
            bool isSelected = selectedIdSet.contains(mod.id);
            modRow->setSelected(isSelected);
            if (isSelected) {
                item->setSelected(true);
            }
        } else {
            modRow->setSelected(i == currentRow);
        }
    }

    if (!hasSavedSelection && currentRow >= 0 && currentRow < m_modList->count()) {
        m_modList->setCurrentRow(currentRow);
    }

    QTimer::singleShot(0, this, [this, scrollPosition]() {
        m_modList->verticalScrollBar()->setValue(scrollPosition);
    });

    updateDragMode();

    m_updating = false;
}

void ModListWidget::rescanConflicts() {
    if (m_conflictDetector) {
        m_conflictDetector->scanForConflicts();
    }
}

void ModListWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ModListWidget::retranslateUi() {
    if (m_titleLabel) m_titleLabel->setText(tr("Installed Mods:"));
    if (m_checkUpdatesButton) m_checkUpdatesButton->setText(tr("Check for Updates"));
    if (m_enableAllCheckBox) {
        m_enableAllCheckBox->setText(QString());
        m_enableAllCheckBox->setToolTip(tr("Enable or disable all mods"));
    }
}

void ModListWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    if (!m_modList) {
        return;
    }

    int rowWidth = m_modList->viewport()->width();
    if (rowWidth <= 0) {
        return;
    }

    for (int i = 0; i < m_modList->count(); ++i) {
        QListWidgetItem *item = m_modList->item(i);
        if (!item) {
            continue;
        }
        QSize rowSize = item->sizeHint();
        if (rowSize.width() != rowWidth) {
            rowSize.setWidth(rowWidth);
            item->setSizeHint(rowSize);
        }
    }
}

void ModListWidget::onModsChanged() {
    refreshModList();
}

void ModListWidget::onModEnabledChanged(const QString &modId, bool enabled) {
    if (m_updating || !m_modManager) {
        return;
    }

    if (enabled) {
        m_modManager->enableMod(modId);
    } else {
        m_modManager->disableMod(modId);
    }
}

void ModListWidget::onEnableAllClicked() {
    if (m_updating || !m_modManager || !m_enableAllCheckBox) {
        return;
    }

    QList<ModInfo> mods = m_modManager->getMods();
    if (mods.isEmpty()) {
        return;
    }

    int enabledCount = 0;
    for (const ModInfo &mod : mods) {
        if (mod.enabled) {
            enabledCount++;
        }
    }

    bool enableAll = enabledCount < mods.size();
    m_enableAllCheckBox->setEnabled(false);
    m_modManager->setAllModsEnabled(enableAll);
    m_enableAllCheckBox->setEnabled(true);
}

void ModListWidget::onAddModClicked() {
    if (!m_modManager || !m_modalManager) {
        return;
    }

    auto *modal = new AddModModalContent(m_modManager, m_nexusClient, m_nexusAuth, m_itchClient, m_itchAuth, m_modalManager);
    connect(modal, &AddModModalContent::modAdded, this, &ModListWidget::modAdded);
    m_modalManager->showModal(modal);
}

void ModListWidget::onRemoveModClicked() {
    if (!m_modManager || !m_modalManager) {
        return;
    }

    QStringList modIds = getSelectedModIds();
    if (modIds.isEmpty()) {
        return;
    }

    QList<ModInfo> modsToRemove;
    modsToRemove.reserve(modIds.size());
    for (const QString &modId : modIds) {
        ModInfo mod = m_modManager->getMod(modId);
        if (!mod.id.isEmpty()) {
            modsToRemove.append(mod);
        }
    }

    if (modsToRemove.isEmpty()) {
        return;
    }

    QString prompt;
    if (modsToRemove.size() == 1) {
        prompt = QString("Are you sure you want to remove '%1'?").arg(modsToRemove.first().name);
    } else {
        prompt = QString("Are you sure you want to remove %1 mods?").arg(modsToRemove.size());
    }

    auto *modal = new MessageModal(
        "Remove Mod",
        prompt,
        MessageModal::Question,
        MessageModal::Yes | MessageModal::No
    );
    connect(modal, &MessageModal::finished, this, [this, modal, modsToRemove]() {
        if (modal->clickedButton() != MessageModal::Yes) {
            return;
        }
        for (const ModInfo &mod : modsToRemove) {
            QString modName = mod.name;
            if (!m_modManager->removeMod(mod.id)) {
                MessageModal::warning(m_modalManager, "Error", "Failed to remove mod");
            } else {
                emit modRemoved(modName);
            }
        }
    });
    m_modalManager->showModal(modal);
}

void ModListWidget::onMoveUpClicked() {
    if (!m_modManager) {
        return;
    }

    if (getSelectedCount() != 1) {
        return;
    }

    int selectedRow = getSelectedRow();
    if (selectedRow <= 0) {
        return;
    }

    QString modId = getSelectedModId();
    if (modId.isEmpty()) {
        return;
    }

    // Get mod ID from row above
    QListWidgetItem *aboveItem = m_modList->item(selectedRow - 1);
    if (!aboveItem) {
        return;
    }
    QString aboveModId = aboveItem->data(Qt::UserRole).toString();

    // Swap priorities
    ModInfo currentMod = m_modManager->getMod(modId);
    ModInfo aboveMod = m_modManager->getMod(aboveModId);

    m_modManager->setModPriority(modId, aboveMod.priority);
    m_modManager->setModPriority(aboveModId, currentMod.priority);

    // Select the moved row
    m_modList->setCurrentRow(selectedRow - 1);

    emit modReordered();
}

void ModListWidget::onMoveDownClicked() {
    if (!m_modManager) {
        return;
    }

    if (getSelectedCount() != 1) {
        return;
    }

    int selectedRow = getSelectedRow();
    if (selectedRow < 0 || selectedRow >= m_modList->count() - 1) {
        return;
    }

    QString modId = getSelectedModId();
    if (modId.isEmpty()) {
        return;
    }

    // Get mod ID from row below
    QListWidgetItem *belowItem = m_modList->item(selectedRow + 1);
    if (!belowItem) {
        return;
    }
    QString belowModId = belowItem->data(Qt::UserRole).toString();

    // Swap priorities
    ModInfo currentMod = m_modManager->getMod(modId);
    ModInfo belowMod = m_modManager->getMod(belowModId);

    m_modManager->setModPriority(modId, belowMod.priority);
    m_modManager->setModPriority(belowModId, currentMod.priority);

    // Select the moved row
    m_modList->setCurrentRow(selectedRow + 1);

    emit modReordered();
}

QString ModListWidget::getSelectedModId() const {
    QListWidgetItem *item = m_modList->currentItem();
    if (!item) {
        return QString();
    }

    return item->data(Qt::UserRole).toString();
}

QStringList ModListWidget::getSelectedModIds() const {
    QStringList modIds;
    if (!m_modList) {
        return modIds;
    }

    const QList<QListWidgetItem*> items = m_modList->selectedItems();
    modIds.reserve(items.size());
    for (QListWidgetItem *item : items) {
        if (item) {
            modIds.append(item->data(Qt::UserRole).toString());
        }
    }
    return modIds;
}

int ModListWidget::getSelectedRow() const {
    return m_modList->currentRow();
}

int ModListWidget::getSelectedCount() const {
    return m_modList ? m_modList->selectedItems().size() : 0;
}

void ModListWidget::setLoadingState(bool loading, const QString &message) {
    if (loading) {
        m_loadingDots = 0;
        m_loadingLabel->setText(message + ".");

        QRect listGeometry = m_modList->geometry();
        int labelWidth = 200;
        int labelHeight = 30;
        int x = listGeometry.x() + (listGeometry.width() - labelWidth) / 2;
        int y = listGeometry.y() + (listGeometry.height() - labelHeight) / 2;
        m_loadingLabel->setGeometry(x, y, labelWidth, labelHeight);

        m_loadingLabel->show();
        m_loadingLabel->raise();

        m_loadingTimer->start(500);
    } else {
        m_loadingTimer->stop();
        m_loadingLabel->hide();
    }
}

void ModListWidget::updateLoadingAnimation() {
    m_loadingDots = (m_loadingDots + 1) % 4;
    QString baseText = m_loadingLabel->text();
    int dotIndex = baseText.indexOf('.');
    if (dotIndex != -1) {
        baseText = baseText.left(dotIndex);
    }

    QString dots(m_loadingDots, '.');
    m_loadingLabel->setText(baseText + dots);
}

void ModListWidget::onContextMenuRequested(const QPoint &pos) {
    if (!m_modList) {
        return;
    }

    QListWidgetItem *item = m_modList->itemAt(pos);
    if (item && !item->isSelected()) {
        m_modList->clearSelection();
        item->setSelected(true);
        m_modList->setCurrentItem(item);
    }

    if (getSelectedCount() == 0) {
        return;
    }

    showSelectionContextMenu(m_modList->viewport()->mapToGlobal(pos));
}

void ModListWidget::onItemsReordered() {
    if (!m_modManager) {
        return;
    }

    if (isFilterActive()) {
        return;
    }

    m_updating = true;

    QMap<QString, int> newPriorities;
    for (int row = 0; row < m_modList->count(); ++row) {
        QListWidgetItem *item = m_modList->item(row);
        if (item) {
            QString modId = item->data(Qt::UserRole).toString();
            newPriorities[modId] = row;
        }
    }

    m_modManager->batchSetModPriorities(newPriorities);

    m_updating = false;
}

bool ModListWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hideSearch(true);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ModListWidget::activateSearch() {
    if (!m_searchContainer || !m_searchEdit) {
        return;
    }

    m_searchContainer->setVisible(true);
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void ModListWidget::hideSearch(bool clearFilter) {
    if (!m_searchContainer) {
        return;
    }

    if (clearFilter && m_searchEdit) {
        m_searchEdit->clear();
        m_filterText.clear();
        refreshModList();
    }

    m_searchContainer->setVisible(false);
    if (m_modList) {
        m_modList->setFocus();
    }
}

void ModListWidget::updateDragMode() {
    if (!m_modList) {
        return;
    }

    if (isFilterActive()) {
        m_modList->setDragEnabled(false);
        m_modList->setDragDropMode(QAbstractItemView::DropOnly);
    } else {
        m_modList->setDragEnabled(true);
        m_modList->setDragDropMode(QAbstractItemView::DragDrop);
    }
}

bool ModListWidget::isFilterActive() const {
    return !m_filterText.isEmpty();
}

QUrl ModListWidget::buildNexusUrl(const ModInfo &mod) const {
    if (!mod.nexusUrl.isEmpty()) {
        return QUrl(mod.nexusUrl);
    }
    if (!mod.nexusModId.isEmpty()) {
        return QUrl(QStringLiteral("https://www.nexusmods.com/foxhole/mods/%1").arg(mod.nexusModId));
    }
    return {};
}

QUrl ModListWidget::buildItchUrl(const ModInfo &mod) const {
    if (mod.itchUrl.isEmpty()) {
        return {};
    }
    return QUrl(mod.itchUrl);
}

void ModListWidget::showSelectionContextMenu(const QPoint &globalPos) {
    if (!m_modManager) {
        return;
    }

    QStringList modIds = getSelectedModIds();
    if (modIds.isEmpty()) {
        return;
    }

    int selectedCount = modIds.size();
    QString singleModId = selectedCount == 1 ? modIds.first() : QString();

    QMenu menu(this);

    QAction *enableSelected = menu.addAction(tr("Enable Selected"));
    QAction *disableSelected = menu.addAction(tr("Disable Selected"));
    menu.addSeparator();

    QAction *registerNexus = menu.addAction(tr("Register with Nexus Mods"));
    QAction *registerItch = menu.addAction(tr("Register with itch.io"));
    QAction *removeSelected = nullptr;

    if (selectedCount == 1) {
        ModInfo mod = m_modManager->getMod(singleModId);
        QUrl nexusUrl = buildNexusUrl(mod);
        QUrl itchUrl = buildItchUrl(mod);

        QAction *openNexus = nullptr;
        QAction *openItch = nullptr;

        if (!nexusUrl.isEmpty()) {
            openNexus = menu.addAction(tr("Open Nexus Mods Page"));
        }
        if (!itchUrl.isEmpty()) {
            openItch = menu.addAction(tr("Open itch.io Page"));
        }
        menu.addSeparator();
        QAction *renameAction = menu.addAction(tr("Rename"));
        QAction *editMetaAction = menu.addAction(tr("Edit Metadata"));
        menu.addSeparator();
        QAction *removeAction = menu.addAction(tr("Remove"));

        QAction *selectedAction = menu.exec(globalPos);
        if (!selectedAction) {
            return;
        }

        if (selectedAction == enableSelected) {
            m_modManager->setModsEnabled(modIds, true);
        } else if (selectedAction == disableSelected) {
            m_modManager->setModsEnabled(modIds, false);
        } else if (selectedAction == registerNexus) {
            startNexusRegistrationQueue(modIds);
        } else if (selectedAction == registerItch) {
            startItchRegistrationQueue(modIds);
        } else if (selectedAction == openNexus && !nexusUrl.isEmpty()) {
            QDesktopServices::openUrl(nexusUrl);
        } else if (selectedAction == openItch && !itchUrl.isEmpty()) {
            QDesktopServices::openUrl(itchUrl);
        } else if (selectedAction == renameAction) {
            onRenameRequested(singleModId);
        } else if (selectedAction == editMetaAction) {
            onEditMetaRequested(singleModId);
        } else if (selectedAction == removeAction) {
            onRemoveModClicked();
        }
        return;
    }

    menu.addSeparator();
    removeSelected = menu.addAction(tr("Remove Selected"));

    QAction *selectedAction = menu.exec(globalPos);
    if (!selectedAction) {
        return;
    }

    if (selectedAction == enableSelected) {
        m_modManager->setModsEnabled(modIds, true);
    } else if (selectedAction == disableSelected) {
        m_modManager->setModsEnabled(modIds, false);
    } else if (selectedAction == registerNexus) {
        startNexusRegistrationQueue(modIds);
    } else if (selectedAction == registerItch) {
        startItchRegistrationQueue(modIds);
    } else if (selectedAction == removeSelected) {
        onRemoveModClicked();
    }
}

void ModListWidget::onRenameRequested(const QString &modId) {
    if (!m_modManager) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    auto *modal = new InputModal("Rename Mod", "Enter new name:", mod.name);
    connect(modal, &InputModal::accepted, this, [this, modal, modId, mod]() {
        QString newName = modal->textValue();
        if (!newName.isEmpty() && newName != mod.name) {
            ModInfo updatedMod = mod;
            updatedMod.name = newName;
            m_modManager->updateModMetadata(updatedMod);
        }
    });
    m_modalManager->showModal(modal);
}

void ModListWidget::onEditMetaRequested(const QString &modId) {
    if (!m_modManager) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    auto *modal = new ModMetadataModalContent(mod);
    connect(modal, &ModMetadataModalContent::accepted, this, [this, modal]() {
        ModInfo updatedMod = modal->getModInfo();
        m_modManager->updateModMetadata(updatedMod);
    });
    m_modalManager->showModal(modal);
}

void ModListWidget::onRemoveRequested(const QString &modId) {
    if (!m_modManager || !m_modalManager) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    auto *modal = new MessageModal(
        "Remove Mod",
        QString("Are you sure you want to remove '%1'?").arg(mod.name),
        MessageModal::Question,
        MessageModal::Yes | MessageModal::No
    );
    connect(modal, &MessageModal::finished, this, [this, modal, modId, mod]() {
        if (modal->clickedButton() == MessageModal::Yes) {
            QString modName = mod.name;
            if (!m_modManager->removeMod(modId)) {
                MessageModal::warning(m_modalManager, "Error", "Failed to remove mod");
            } else {
                emit modRemoved(modName);
            }
        }
    });
    m_modalManager->showModal(modal);
}

void ModListWidget::setNexusServices(NexusModsClient *client, NexusModsAuth *auth) {
    m_nexusClient = client;
    m_nexusAuth = auth;
}

void ModListWidget::setItchServices(ItchClient *client, ItchAuth *auth) {
    m_itchClient = client;
    m_itchAuth = auth;
}

void ModListWidget::setUpdateService(ModUpdateService *service) {
    if (m_updateService) {
        disconnect(m_updateService, nullptr, this, nullptr);
    }

    m_updateService = service;

    if (m_updateService) {
        connect(m_updateService, &ModUpdateService::updateFound,
                this, &ModListWidget::onUpdateFound);
        connect(m_updateService, &ModUpdateService::checkComplete,
                this, &ModListWidget::onUpdateCheckComplete);
    }
}

void ModListWidget::setItchUpdateService(ItchModUpdateService *service) {
    if (m_itchUpdateService) {
        disconnect(m_itchUpdateService, nullptr, this, nullptr);
    }

    m_itchUpdateService = service;

    if (m_itchUpdateService) {
        connect(m_itchUpdateService, &ItchModUpdateService::updateFound,
                this, &ModListWidget::onItchUpdateFound);
        connect(m_itchUpdateService, &ItchModUpdateService::checkComplete,
                this, &ModListWidget::onItchUpdateCheckComplete);
    }
}

void ModListWidget::onCheckUpdatesClicked() {
    m_checkUpdatesButton->setEnabled(false);
    m_checkUpdatesButton->setText(tr("Checking..."));

    m_pendingUpdateChecks = 0;
    m_totalUpdatesFound = 0;

    if (m_updateService) {
        m_pendingUpdateChecks++;
        m_updateService->checkAllModsForUpdates();
    }

    if (m_itchUpdateService) {
        m_pendingUpdateChecks++;
        m_itchUpdateService->checkAllModsForUpdates();
    }
}

void ModListWidget::onUpdateFound(const QString &modId, const ModUpdateInfo &updateInfo) {
    for (int i = 0; i < m_modList->count(); ++i) {
        QListWidgetItem *item = m_modList->item(i);
        if (item && item->data(Qt::UserRole).toString() == modId) {
            auto *rowWidget = qobject_cast<ModRowWidget*>(m_modList->itemWidget(item));
            if (rowWidget) {
                rowWidget->setUpdateAvailable(true, updateInfo.availableVersion);
            }
            break;
        }
    }
}

void ModListWidget::onUpdateCheckComplete(int updatesFound) {
    m_totalUpdatesFound += updatesFound;
    m_pendingUpdateChecks--;

    if (m_pendingUpdateChecks <= 0) {
        m_checkUpdatesButton->setEnabled(true);
        m_checkUpdatesButton->setText(tr("Check for Updates (%1)").arg(m_totalUpdatesFound));

        QTimer::singleShot(5000, this, [this]() {
            m_checkUpdatesButton->setText(tr("Check for Updates"));
        });
    }
}

void ModListWidget::onItchUpdateFound(const QString &modId, const ItchUpdateInfo &updateInfo) {
    for (int i = 0; i < m_modList->count(); ++i) {
        QListWidgetItem *item = m_modList->item(i);
        if (item && item->data(Qt::UserRole).toString() == modId) {
            auto *rowWidget = qobject_cast<ModRowWidget*>(m_modList->itemWidget(item));
            if (rowWidget) {
                rowWidget->setUpdateAvailable(true, updateInfo.availableVersion);
            }
            break;
        }
    }
}

void ModListWidget::onItchUpdateCheckComplete(int updatesFound) {
    m_totalUpdatesFound += updatesFound;
    m_pendingUpdateChecks--;

    if (m_pendingUpdateChecks <= 0) {
        m_checkUpdatesButton->setEnabled(true);
        m_checkUpdatesButton->setText(tr("Check for Updates (%1)").arg(m_totalUpdatesFound));

        QTimer::singleShot(5000, this, [this]() {
            m_checkUpdatesButton->setText(tr("Check for Updates"));
        });
    }
}

void ModListWidget::onUpdateRequested(const QString &modId) {
    if (!m_modManager) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    auto hideUpdateButton = [this, modId]() {
        for (int i = 0; i < m_modList->count(); ++i) {
            QListWidgetItem *item = m_modList->item(i);
            if (item && item->data(Qt::UserRole).toString() == modId) {
                auto *rowWidget = qobject_cast<ModRowWidget*>(m_modList->itemWidget(item));
                if (rowWidget) {
                    rowWidget->hideUpdateButton();
                }
                break;
            }
        }
    };
    auto finalizeItchUpdate = [this, modId, hideUpdateButton]() {
        hideUpdateButton();
        if (m_itchUpdateService) {
            m_itchUpdateService->clearUpdateForMod(modId);
        }
        if (m_conflictDetector) {
            m_conflictDetector->scanForConflicts();
        }
    };

    if (!mod.nexusModId.isEmpty() && m_nexusClient && m_updateService && m_updateService->hasUpdate(modId)) {
        ModUpdateInfo updateInfo = m_updateService->getUpdateInfo(modId);
        auto *modal = new ModUpdateModalContent(mod, updateInfo, m_modManager, m_nexusClient, m_modalManager);
        connect(modal, &ModUpdateModalContent::accepted, this, hideUpdateButton);
        m_modalManager->showModal(modal);
    } else if (!mod.itchGameId.isEmpty() && m_itchClient && m_itchUpdateService && m_itchUpdateService->hasUpdate(modId)) {
        ItchUpdateInfo updateInfo = m_itchUpdateService->getUpdateInfo(modId);

        // Check if any upload matches the current filename
        ItchUploadInfo matchingUpload;
        bool hasMatchingFilename = false;

        if (!mod.fileName.isEmpty()) {
            for (const ItchUploadInfo &upload : updateInfo.candidateUploads) {
                if (upload.filename == mod.fileName) {
                    matchingUpload = upload;
                    hasMatchingFilename = true;
                    break;
                }
            }
        }

        // If we found a matching filename, auto-select it and skip the file selection modal
        if (hasMatchingFilename) {
            ItchUpdateInfo selectedUpdateInfo = updateInfo;
            selectedUpdateInfo.availableUploadId = matchingUpload.id;
            selectedUpdateInfo.availableUploadDate =
                matchingUpload.updatedAt.isValid() ? matchingUpload.updatedAt : matchingUpload.createdAt;
            selectedUpdateInfo.availableVersion = extractVersionFromFilename(matchingUpload.filename);
            if (selectedUpdateInfo.availableVersion.isEmpty()) {
                selectedUpdateInfo.availableVersion = QString("Updated: %1")
                    .arg(selectedUpdateInfo.availableUploadDate.toString("yyyy-MM-dd"));
            }

            auto *modal = new ItchModUpdateModalContent(
                mod, selectedUpdateInfo, m_modManager, m_itchClient, m_modalManager);
            connect(modal, &ItchModUpdateModalContent::accepted, this, finalizeItchUpdate);
            m_modalManager->showModal(modal);
        }
        // If multiple uploads and no matching filename, show file selection modal
        else if (updateInfo.hasMultipleUploads()) {
            QList<FileItem> fileItems;
            for (const ItchUploadInfo &upload : updateInfo.candidateUploads) {
                fileItems.append({upload.id, upload.filename});
            }

            auto *fileModal = new FileSelectionModalContent(
                fileItems,
                "Select Update File",
                QString("Multiple update files are available for '%1'. Select one:").arg(mod.name),
                false,
                true  // Show "Ignore These Updates" button
            );

            // If user clicks "Ignore These Updates", mark all these uploads as ignored and hide the update button
            connect(fileModal, &FileSelectionModalContent::allIgnored, this,
                    [this, modId, updateInfo, hideUpdateButton]() {
                if (m_itchUpdateService) {
                    QStringList uploadIds;
                    for (const ItchUploadInfo &upload : updateInfo.candidateUploads) {
                        uploadIds.append(upload.id);
                    }
                    m_itchUpdateService->ignoreUpdatesForMod(modId, uploadIds);
                }
                hideUpdateButton();
            });

            connect(fileModal, &FileSelectionModalContent::accepted, this,
                    [this, fileModal, mod, updateInfo, finalizeItchUpdate]() {
                QStringList selectedIds = fileModal->getSelectedIds();
                if (!selectedIds.isEmpty()) {
                    QString selectedUploadId = selectedIds.first();

                    ItchUploadInfo selectedUpload;
                    for (const ItchUploadInfo &upload : updateInfo.candidateUploads) {
                        if (upload.id == selectedUploadId) {
                            selectedUpload = upload;
                            break;
                        }
                    }

                    ItchUpdateInfo selectedUpdateInfo = updateInfo;
                    selectedUpdateInfo.availableUploadId = selectedUpload.id;
                    selectedUpdateInfo.availableUploadDate =
                        selectedUpload.updatedAt.isValid() ? selectedUpload.updatedAt : selectedUpload.createdAt;
                    selectedUpdateInfo.availableVersion = extractVersionFromFilename(selectedUpload.filename);
                    if (selectedUpdateInfo.availableVersion.isEmpty()) {
                        selectedUpdateInfo.availableVersion = QString("Updated: %1")
                            .arg(selectedUpdateInfo.availableUploadDate.toString("yyyy-MM-dd"));
                    }

                    auto *downloadModal = new ItchModUpdateModalContent(
                        mod, selectedUpdateInfo, m_modManager, m_itchClient, m_modalManager);
                    connect(downloadModal, &ItchModUpdateModalContent::accepted,
                            this, finalizeItchUpdate);
                    m_modalManager->showModal(downloadModal);
                }
            });

            m_modalManager->showModal(fileModal);
        }
        // Single upload, proceed directly
        else {
            auto *modal = new ItchModUpdateModalContent(mod, updateInfo, m_modManager, m_itchClient, m_modalManager);
            connect(modal, &ItchModUpdateModalContent::accepted, this, finalizeItchUpdate);
            m_modalManager->showModal(modal);
        }
    }
}

QString ModListWidget::extractVersionFromFilename(const QString &filename) const {
    static const QRegularExpression versionRegex(R"(v?(\d+\.\d+(?:\.\d+)?))");
    QRegularExpressionMatch match = versionRegex.match(filename);

    if (match.hasMatch()) {
        return match.captured(1);
    }

    return QString();
}

void ModListWidget::onFilesDropped(const QStringList &filePaths) {
    if (!m_modManager || !m_modalManager) {
        return;
    }

    for (const QString &filePath : filePaths) {
        if (filePath.endsWith(".pak", Qt::CaseInsensitive)) {
            handlePakFile(filePath);
        } else if (isArchiveFile(filePath)) {
            handleArchiveFile(filePath);
        }
    }
}

void ModListWidget::handlePakFile(const QString &pakPath) {
    QFileInfo fileInfo(pakPath);
    QString modName = fileInfo.baseName();

    if (!m_modManager->addMod(pakPath, {.name = modName})) {
        MessageModal::warning(m_modalManager, "Error",
                            "Failed to add mod: " + modName);
    } else {
        emit modAdded(modName);
    }
}

bool ModListWidget::isArchiveFile(const QString &filePath) const {
    QString lower = filePath.toLower();
    return lower.endsWith(".zip") ||
           lower.endsWith(".rar") ||
           lower.endsWith(".7z") ||
           lower.endsWith(".tar.gz") ||
           lower.endsWith(".tar.bz2") ||
           lower.endsWith(".tar.xz");
}

void ModListWidget::handleArchiveFile(const QString &archivePath) {
    ArchiveExtractor extractor;
    auto result = extractor.extractPakFiles(archivePath);

    if (!result.success) {
        MessageModal::warning(m_modalManager, "Error", result.error);
        return;
    }

    if (result.pakFiles.isEmpty()) {
        MessageModal::warning(m_modalManager, "Error",
                            "No .pak files found in archive");
        ArchiveExtractor::cleanupTempDir(result.tempDir);
        return;
    }

    if (result.pakFiles.size() == 1) {
        handlePakFile(result.pakFiles.first());
        ArchiveExtractor::cleanupTempDir(result.tempDir);
    } else {
        QStringList fileNames;
        for (const QString &path : result.pakFiles) {
            fileNames.append(QFileInfo(path).fileName());
        }

        auto *fileModal = new FileSelectionModalContent(
            fileNames, QFileInfo(archivePath).fileName(), true);

        connect(fileModal, &FileSelectionModalContent::accepted,
                this, [this, result, fileModal]() {
            QStringList selectedFileNames = fileModal->getSelectedFiles();

            for (const QString &fileName : selectedFileNames) {
                for (const QString &path : result.pakFiles) {
                    if (QFileInfo(path).fileName() == fileName) {
                        handlePakFile(path);
                        break;
                    }
                }
            }

            ArchiveExtractor::cleanupTempDir(result.tempDir);
        });

        connect(fileModal, &FileSelectionModalContent::rejected,
                this, [result]() {
            ArchiveExtractor::cleanupTempDir(result.tempDir);
        });

        m_modalManager->showModal(fileModal);
    }
}

void ModListWidget::onConflictScanComplete(QMap<QString, ConflictInfo> conflicts) {
    for (int i = 0; i < m_modList->count(); ++i) {
        QListWidgetItem *item = m_modList->item(i);
        if (!item) {
            continue;
        }

        auto *rowWidget = qobject_cast<ModRowWidget*>(m_modList->itemWidget(item));
        if (rowWidget) {
            QString modId = rowWidget->modId();
            ConflictInfo info = conflicts.value(modId);
            rowWidget->setConflictInfo(info);
        }
    }
}

void ModListWidget::onConflictDetailsRequested(const QString &modId) {
    if (!m_modManager || !m_modalManager || !m_conflictDetector) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    ConflictInfo conflictInfo = m_conflictDetector->getConflictInfo(modId);
    if (!conflictInfo.hasConflicts()) {
        return;
    }

    auto *modal = new ConflictDetailModalContent(mod.name, conflictInfo);
    m_modalManager->showModal(modal);
}

void ModListWidget::startNexusRegistrationQueue(const QStringList &modIds) {
    if (!m_modalManager || !m_nexusClient || !m_nexusAuth || !m_modManager) {
        return;
    }

    auto index = std::make_shared<int>(0);
    auto showNext = std::make_shared<std::function<void()>>();
    *showNext = [this, modIds, index, showNext]() {
        while (*index < modIds.size()) {
            QString modId = modIds[*index];
            ModInfo mod = m_modManager->getMod(modId);
            if (mod.id.isEmpty()) {
                (*index)++;
                continue;
            }

            auto *modal = new NexusRegistrationModalContent(
                m_nexusClient, m_nexusAuth, m_modalManager, mod.id, mod.name);

            connect(modal, &NexusRegistrationModalContent::accepted, this, [this, modal, modId]() {
                applyNexusRegistration(modId, modal->getSelectedFiles(),
                                       modal->getModId(), modal->getAuthor(),
                                       modal->getDescription(), modal->getUrl());
            });

            connect(modal, &BaseModalContent::finished, this, [index, showNext](int result) {
                if (result == BaseModalContent::Rejected) {
                    return;
                }
                (*index)++;
                (*showNext)();
            });

            m_modalManager->showModal(modal);
            return;
        }
    };

    (*showNext)();
}

void ModListWidget::startItchRegistrationQueue(const QStringList &modIds) {
    if (!m_modalManager || !m_itchClient || !m_itchAuth || !m_modManager) {
        return;
    }

    auto index = std::make_shared<int>(0);
    auto showNext = std::make_shared<std::function<void()>>();
    *showNext = [this, modIds, index, showNext]() {
        while (*index < modIds.size()) {
            QString modId = modIds[*index];
            ModInfo mod = m_modManager->getMod(modId);
            if (mod.id.isEmpty()) {
                (*index)++;
                continue;
            }

            auto *modal = new ItchRegistrationModalContent(
                m_itchClient, m_itchAuth, m_modalManager, mod.id, mod.name);

            connect(modal, &ItchRegistrationModalContent::accepted, this, [this, modal, modId]() {
                applyItchRegistration(modId, modal->getSelectedUploads(),
                                      modal->getGameId(), modal->getAuthor(),
                                      modal->getUrl());
            });

            connect(modal, &BaseModalContent::finished, this, [index, showNext](int result) {
                if (result == BaseModalContent::Rejected) {
                    return;
                }
                (*index)++;
                (*showNext)();
            });

            m_modalManager->showModal(modal);
            return;
        }
    };

    (*showNext)();
}

void ModListWidget::applyNexusRegistration(const QString &modId, const QList<NexusFileInfo> &files,
                                           const QString &nexusModId, const QString &author,
                                           const QString &description, const QString &nexusUrl) {
    if (!m_modManager) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    mod.nexusModId = nexusModId;
    mod.author = author;
    mod.description = description;
    mod.nexusUrl = nexusUrl;

    if (!files.isEmpty()) {
        mod.nexusFileId = files.first().id;
        mod.version = files.first().version;
    }

    if (m_modManager->updateModMetadata(mod)) {
        refreshModList();
    }
}

void ModListWidget::applyItchRegistration(const QString &modId, const QList<ItchUploadInfo> &uploads,
                                          const QString &itchGameId, const QString &author,
                                          const QString &itchUrl) {
    if (!m_modManager) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    mod.itchGameId = itchGameId;
    mod.author = author;
    mod.itchUrl = itchUrl;

    if (!uploads.isEmpty()) {
        QString version = extractVersionFromFilename(uploads.first().filename);
        if (!version.isEmpty()) {
            mod.version = version;
        }
    }

    if (m_modManager->updateModMetadata(mod)) {
        refreshModList();
    }
}

void ModListWidget::onRegisterWithNexusRequested(const QString &modId) {
    if (!m_modalManager || !m_nexusClient || !m_nexusAuth) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    auto *modal = new NexusRegistrationModalContent(
        m_nexusClient, m_nexusAuth, m_modalManager, modId, mod.name);

    connect(modal, &NexusRegistrationModalContent::accepted, this, [this, modal, modId]() {
        applyNexusRegistration(modId, modal->getSelectedFiles(),
                               modal->getModId(), modal->getAuthor(),
                               modal->getDescription(), modal->getUrl());
    });

    m_modalManager->showModal(modal);
}

void ModListWidget::onRegisterWithItchRequested(const QString &modId) {
    if (!m_modalManager || !m_itchClient || !m_itchAuth) {
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    auto *modal = new ItchRegistrationModalContent(
        m_itchClient, m_itchAuth, m_modalManager, modId, mod.name);

    connect(modal, &ItchRegistrationModalContent::accepted, this, [this, modal, modId]() {
        applyItchRegistration(modId, modal->getSelectedUploads(),
                              modal->getGameId(), modal->getAuthor(),
                              modal->getUrl());
    });

    m_modalManager->showModal(modal);
}
