#include "ProfileManagerWidget.h"
#include "ProfileRowWidget.h"
#include "common/widgets/GradientFrame.h"
#include "DraggableProfileList.h"
#include "common/modals/ModalManager.h"
#include "common/modals/MessageModal.h"
#include "modals/mod_manager/ConflictResolutionModalContent.h"
#include "common/modals/InputModal.h"
#include "core/managers/ModManager.h"
#include "core/managers/ProfileManager.h"
#include "ModEnableProgress.h"
#include "core/utils/Theme.h"
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEventLoop>
#include <QInputDialog>
#include <QFileDialog>

ProfileManagerWidget::ProfileManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(new QLabel(this))
    , m_profileList(new DraggableProfileList(this))
    , m_createButton(new QPushButton(this))
    , m_loadButton(new QPushButton(this))
    , m_updateButton(new QPushButton(this))
    , m_importIconButton(new QToolButton(this))
    , m_layout(new QVBoxLayout(this))
{
    setupUi();
}

void ProfileManagerWidget::setupUi() {
    GradientFrame *frame = new GradientFrame(this);
    QVBoxLayout *frameLayout = new QVBoxLayout(frame);

    auto *titleLayout = new QHBoxLayout();
    m_titleLabel->setObjectName("profileTitle");

    m_importIconButton->setIcon(QIcon(":/icon_import.png"));
    m_importIconButton->setIconSize(QSize(20, 20));
    m_importIconButton->setFixedSize(28, 28);
    m_importIconButton->setObjectName("importIconButton");
    m_importIconButton->setCursor(Qt::PointingHandCursor);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_importIconButton);

    m_profileList->setObjectName("profileList");
    m_profileList->setMinimumHeight(150);
    m_profileList->setSpacing(Theme::Spacing::PROFILE_LIST_ITEM_SPACING);

    m_createButton->setObjectName("profileButton");
    m_createButton->setCursor(Qt::PointingHandCursor);

    m_loadButton->setObjectName("profileButton");
    m_loadButton->setEnabled(false);
    m_loadButton->setCursor(Qt::PointingHandCursor);

    m_updateButton->setObjectName("profileButton");
    m_updateButton->setEnabled(false);
    m_updateButton->setCursor(Qt::PointingHandCursor);


    frameLayout->addLayout(titleLayout);
    frameLayout->addWidget(m_profileList, 1);
    frameLayout->addWidget(m_createButton);
    frameLayout->addWidget(m_loadButton);
    frameLayout->addWidget(m_updateButton);

    m_layout->setContentsMargins(
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN,
        Theme::Spacing::CONTAINER_MARGIN
    );
    m_layout->setSpacing(Theme::Spacing::CONTAINER_SPACING);
    m_layout->addWidget(frame);

    setLayout(m_layout);

    frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_profileList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    setupConnections();
    retranslateUi();
}

void ProfileManagerWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ProfileManagerWidget::retranslateUi() {
    m_titleLabel->setText(tr("Profiles"));
    m_importIconButton->setToolTip(tr("Import Profile"));
    m_createButton->setText(tr("New Profile"));
    m_loadButton->setText(tr("Select Profile"));
    m_updateButton->setText(tr("Update Selected"));
}

void ProfileManagerWidget::setupConnections() {
    connect(m_createButton, &QPushButton::clicked, this, &ProfileManagerWidget::onCreateClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &ProfileManagerWidget::onLoadClicked);
    connect(m_updateButton, &QPushButton::clicked, this, &ProfileManagerWidget::onUpdateClicked);
    connect(m_importIconButton, &QToolButton::clicked, this, &ProfileManagerWidget::onImportClicked);

    connect(m_profileList, &DraggableProfileList::itemSelectionChanged,
            this, &ProfileManagerWidget::onItemSelectionChanged);
    connect(m_profileList, &DraggableProfileList::itemDoubleClicked,
            this, &ProfileManagerWidget::onItemDoubleClicked);
    connect(m_profileList, &DraggableProfileList::itemsReordered,
            this, &ProfileManagerWidget::onItemsReordered);
}

void ProfileManagerWidget::setProfileManager(ProfileManager *profileManager) {
    m_profileManager = profileManager;

    if (m_profileManager) {
        connect(m_profileManager, &ProfileManager::profilesChanged,
                this, &ProfileManagerWidget::onProfilesChanged);
        connect(m_profileManager, &ProfileManager::activeProfileChanged,
                this, &ProfileManagerWidget::onActiveProfileChanged);

        refreshProfileList();
    }
}

void ProfileManagerWidget::refreshProfileList() {
    if (!m_profileManager) {
        return;
    }

    m_profileList->clear();

    QList<ProfileInfo> profiles = m_profileManager->getProfiles();
    QString activeProfileId = m_profileManager->getActiveProfileId();

    for (const ProfileInfo &profile : profiles) {
        auto *rowWidget = new ProfileRowWidget(profile.id, profile.name, this);

        if (profile.id == activeProfileId) {
            rowWidget->setActive(true);
        }

        connect(rowWidget, &ProfileRowWidget::exportRequested,
                this, &ProfileManagerWidget::onExportClicked);
        connect(rowWidget, &ProfileRowWidget::renameRequested,
                this, &ProfileManagerWidget::onRenameClicked);
        connect(rowWidget, &ProfileRowWidget::deleteRequested,
                this, &ProfileManagerWidget::onDeleteClicked);
        connect(rowWidget, &ProfileRowWidget::clicked,
                this, &ProfileManagerWidget::onProfileRowClicked);

        auto *item = new QListWidgetItem(m_profileList);
        item->setSizeHint(rowWidget->sizeHint());
        item->setData(Qt::UserRole, profile.id);

        m_profileList->addItem(item);
        m_profileList->setItemWidget(item, rowWidget);
    }

    updateButtonStates();
}

void ProfileManagerWidget::onCreateClicked() {
    if (!m_modalManager) {
        return;
    }

    auto *modal = new InputModal(tr("Create Profile"), tr("Profile name:"), "");
    connect(modal, &InputModal::accepted, this, [this, modal]() {
        QString name = modal->textValue();
        if (!name.isEmpty()) {
            if (m_profileManager->createProfile(name)) {
                MessageModal::information(m_modalManager, tr("Success"),
                    tr("Profile '%1' created successfully!").arg(name));
            }
        }
    });
    m_modalManager->showModal(modal);
}

void ProfileManagerWidget::onLoadClicked() {
    QString profileId = getSelectedProfileId();
    if (profileId.isEmpty()) {
        return;
    }

    showValidationDialog(profileId);
}

void ProfileManagerWidget::onUpdateClicked() {
    if (!m_modalManager) {
        return;
    }

    QString profileId = getSelectedProfileId();
    if (profileId.isEmpty()) {
        return;
    }

    ProfileInfo profile = m_profileManager->getProfile(profileId);

    auto *modal = new MessageModal(
        tr("Update Profile"),
        tr("Update profile '%1' with current mod configuration?").arg(profile.name),
        MessageModal::Question,
        MessageModal::Yes | MessageModal::No
    );
    connect(modal, &MessageModal::finished, this, [this, modal, profileId]() {
        if (modal->clickedButton() == MessageModal::Yes) {
            if (m_profileManager->updateProfile(profileId)) {
                MessageModal::information(m_modalManager, tr("Success"), tr("Profile updated successfully!"));
            }
        }
    });
    m_modalManager->showModal(modal);
}

void ProfileManagerWidget::onRenameClicked(const QString &profileId) {
    if (!m_modalManager) {
        return;
    }

    QString targetProfileId = profileId.isEmpty() ? getSelectedProfileId() : profileId;
    if (targetProfileId.isEmpty()) {
        return;
    }

    ProfileInfo profile = m_profileManager->getProfile(targetProfileId);

    auto *modal = new InputModal(tr("Rename Profile"), tr("Enter new name:"), profile.name);
    connect(modal, &InputModal::accepted, this, [this, modal, targetProfileId, profile]() {
        QString newName = modal->textValue();
        if (!newName.isEmpty() && newName != profile.name) {
            if (m_profileManager->renameProfile(targetProfileId, newName)) {
                MessageModal::information(m_modalManager, tr("Success"),
                    tr("Profile renamed successfully!"));
            }
        }
    });
    m_modalManager->showModal(modal);
}

void ProfileManagerWidget::onExportClicked(const QString &profileId) {
    if (!m_modalManager) {
        return;
    }

    QString targetProfileId = profileId.isEmpty() ? getSelectedProfileId() : profileId;
    if (targetProfileId.isEmpty()) {
        return;
    }

    ProfileInfo profile = m_profileManager->getProfile(targetProfileId);
    QString defaultFileName = profile.name + ".tkprofile";

    QString filePath = QFileDialog::getSaveFileName(this, tr("Export Profile"),
        defaultFileName, tr("TrenchKit Profile (*.tkprofile);;JSON Files (*.json)"));

    if (!filePath.isEmpty()) {
        if (m_profileManager->exportProfile(targetProfileId, filePath)) {
            MessageModal::information(m_modalManager, tr("Success"),
                tr("Profile exported to: %1").arg(filePath));
        }
    }
}

void ProfileManagerWidget::onImportClicked() {
    if (!m_modalManager) {
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, tr("Import Profile"),
        "", tr("TrenchKit Profile (*.tkprofile);;JSON Files (*.json)"));

    if (!filePath.isEmpty()) {
        runImport(filePath);
    }
}

bool ProfileManagerWidget::importProfileFromPath(const QString &filePath) {
    if (!m_modalManager || filePath.isEmpty()) {
        return false;
    }
    return runImport(filePath);
}

bool ProfileManagerWidget::runImport(const QString &filePath) {
    if (!m_profileManager || !m_modalManager) {
        return false;
    }

    QString importedProfileId;
    auto resolver = [this](const ModInfo &incoming,
                           const ModInfo &existing,
                           bool checksumMatch) {
        if (!m_modalManager) {
            return ProfileManager::ImportConflictAction::Ignore;
        }

        auto *conflictModal = new ConflictResolutionModalContent(incoming, existing, checksumMatch);
        QEventLoop loop;
        connect(conflictModal, &BaseModalContent::finished, &loop, &QEventLoop::quit);
        m_modalManager->showModal(conflictModal);
        loop.exec();

        switch (conflictModal->selectedAction()) {
            case ConflictResolutionModalContent::Action::Overwrite:
                return ProfileManager::ImportConflictAction::Overwrite;
            case ConflictResolutionModalContent::Action::Duplicate:
                return ProfileManager::ImportConflictAction::Duplicate;
            case ConflictResolutionModalContent::Action::Ignore:
            default:
                return ProfileManager::ImportConflictAction::Ignore;
        }
    };

    if (m_profileManager->importProfile(filePath, importedProfileId, resolver)) {
        MessageModal::information(m_modalManager, tr("Success"), tr("Profile imported successfully!"));

        auto *modal = new MessageModal(
            tr("Load Profile"),
            tr("Would you like to load the imported profile now?"),
            MessageModal::Question,
            MessageModal::Yes | MessageModal::No
        );
        connect(modal, &MessageModal::finished, this, [this, modal, importedProfileId]() {
            if (modal->clickedButton() == MessageModal::Yes) {
                showValidationDialog(importedProfileId);
            }
        });
        m_modalManager->showModal(modal);
        return true;
    }

    return false;
}

void ProfileManagerWidget::onDeleteClicked(const QString &profileId) {
    if (!m_modalManager) {
        return;
    }

    QString targetProfileId = profileId.isEmpty() ? getSelectedProfileId() : profileId;
    if (targetProfileId.isEmpty()) {
        return;
    }

    ProfileInfo profile = m_profileManager->getProfile(targetProfileId);

    auto *modal = new MessageModal(
        tr("Delete Profile"),
        tr("Are you sure you want to delete profile '%1'?").arg(profile.name),
        MessageModal::Question,
        MessageModal::Yes | MessageModal::No
    );
    connect(modal, &MessageModal::finished, this, [this, modal, targetProfileId]() {
        if (modal->clickedButton() == MessageModal::Yes) {
            m_profileManager->deleteProfile(targetProfileId);
        }
    });
    m_modalManager->showModal(modal);
}

void ProfileManagerWidget::onProfilesChanged() {
    refreshProfileList();
}

void ProfileManagerWidget::onActiveProfileChanged(const QString &profileId) {
    Q_UNUSED(profileId);
    refreshProfileList();
}

void ProfileManagerWidget::onItemSelectionChanged() {
    updateButtonStates();

    QString profileId = getSelectedProfileId();
    if (!profileId.isEmpty()) {
        emit profileSelected(profileId);
    }
}

void ProfileManagerWidget::onItemDoubleClicked(QListWidgetItem *item) {
    Q_UNUSED(item);
    onLoadClicked();
}

QString ProfileManagerWidget::getSelectedProfileId() const {
    QListWidgetItem *item = m_profileList->currentItem();
    if (!item) {
        return QString();
    }
    return item->data(Qt::UserRole).toString();
}

void ProfileManagerWidget::updateButtonStates() {
    bool hasSelection = !getSelectedProfileId().isEmpty();

    m_loadButton->setEnabled(hasSelection);
    m_updateButton->setEnabled(hasSelection);
}

void ProfileManagerWidget::onProfileRowClicked(const QString &profileId) {
    for (int i = 0; i < m_profileList->count(); ++i) {
        QListWidgetItem *item = m_profileList->item(i);
        if (item && item->data(Qt::UserRole).toString() == profileId) {
            m_profileList->setCurrentItem(item);

            auto *rowWidget = qobject_cast<ProfileRowWidget*>(m_profileList->itemWidget(item));
            if (rowWidget) {
                rowWidget->setSelected(true);
            }
        } else {
            auto *rowWidget = qobject_cast<ProfileRowWidget*>(m_profileList->itemWidget(item));
            if (rowWidget) {
                rowWidget->setSelected(false);
            }
        }
    }
}

void ProfileManagerWidget::showValidationDialog(const QString &profileId) {
    if (!m_profileManager || !m_modalManager) {
        return;
    }

    ProfileValidationResult validation = m_profileManager->validateProfile(profileId);
    ProfileInfo profile = m_profileManager->getProfile(profileId);

    if (validation.hasMissingMods()) {
        QString message = validation.getMessage();
        message += tr("\n\nMissing mods:\n");

        for (const MissingModInfo &missing : validation.missingMods) {
            message += tr("- Mod ID: %1").arg(missing.modId);
            if (missing.hasNexusId()) {
                message += tr(" (NexusMods: %1)").arg(missing.nexusModId);
            }
            message += QStringLiteral("\n");
        }

        message += tr("\nDo you want to load the profile anyway with available mods only?");

        auto *modal = new MessageModal(
            tr("Missing Mods"),
            message,
            MessageModal::Warning,
            MessageModal::Yes | MessageModal::No
        );
        connect(modal, &MessageModal::finished, this, [this, modal, profileId, profile]() {
            if (modal->clickedButton() == MessageModal::Yes) {
                applyProfileInBackground(profileId, profile, true, false);
            }
        });
        m_modalManager->showModal(modal);
    } else {
        applyProfileInBackground(profileId, profile, false, true);
    }
}

void ProfileManagerWidget::applyProfileInBackground(const QString &profileId, const ProfileInfo &profile,
                                                    bool ignoreWarnings, bool announceSuccess) {
    int modsToEnable = 0;
    for (const ModConfig &config : profile.modConfigs) {
        modsToEnable += config.enabled ? 1 : 0;
    }

    ModEnableProgress::run(m_profileManager->modManager(), m_modalManager, modsToEnable, tr("Loading profile..."),
        [this, profileId, profile, ignoreWarnings, announceSuccess](ModEnableProgress::Done done) {
        return m_profileManager->applyProfileAsync(profileId, ignoreWarnings, this,
            [this, profileId, profile, announceSuccess, done](bool applied) {
            done();
            if (!applied) {
                return;
            }
            emit profileLoadRequested(profileId);
            if (announceSuccess) {
                MessageModal::information(m_modalManager, tr("Success"),
                    tr("Profile '%1' loaded successfully!").arg(profile.name));
            }
        });
    });
}

void ProfileManagerWidget::onItemsReordered() {
    if (!m_profileManager) {
        return;
    }

    QList<QString> orderedProfileIds;
    for (int i = 0; i < m_profileList->count(); ++i) {
        QListWidgetItem *item = m_profileList->item(i);
        if (item) {
            orderedProfileIds.append(item->data(Qt::UserRole).toString());
        }
    }

    m_profileManager->reorderProfiles(orderedProfileIds);
}
