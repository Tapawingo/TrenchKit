/// @file ProfileManagerWidget.h
/// @brief Panel widget for creating, loading, updating, and managing mod profiles.
#ifndef PROFILEMANAGERWIDGET_H
#define PROFILEMANAGERWIDGET_H

#include <QWidget>
#include <QString>
#include <QEvent>

class QLabel;
class QListWidgetItem;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class ProfileManager;
class ModalManager;
class DraggableProfileList;

/// @brief Displays the profile list and provides controls for all profile operations.
///
/// Services are injected via setters. @c importProfileFromPath() is the
/// programmatic entry point for importing a .tkprofile file (e.g. from a
/// file-association launch).
class ProfileManagerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProfileManagerWidget(QWidget *parent = nullptr);
    ~ProfileManagerWidget() override = default;

    void setProfileManager(ProfileManager *profileManager);
    void setModalManager(ModalManager *modalManager) { m_modalManager = modalManager; }
    void refreshProfileList();
    /// @brief Imports a .tkprofile archive from @p filePath; shows conflict resolution if needed.
    bool importProfileFromPath(const QString &filePath);

protected:
    void changeEvent(QEvent *event) override;

signals:
    void profileSelected(const QString &profileId);
    void profileLoadRequested(const QString &profileId);

private slots:
    void onCreateClicked();
    void onLoadClicked();
    void onUpdateClicked();
    void onRenameClicked(const QString &profileId = QString());
    void onExportClicked(const QString &profileId = QString());
    void onImportClicked();
    void onDeleteClicked(const QString &profileId = QString());
    void onProfilesChanged();
    void onActiveProfileChanged(const QString &profileId);
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onProfileRowClicked(const QString &profileId);
    void onItemsReordered();

private:
    bool runImport(const QString &filePath);
    void setupUi();
    void retranslateUi();
    void setupConnections();
    QString getSelectedProfileId() const;
    void updateButtonStates();
    void showValidationDialog(const QString &profileId);

    QLabel *m_titleLabel;
    DraggableProfileList *m_profileList;
    QPushButton *m_createButton;
    QPushButton *m_loadButton;
    QPushButton *m_updateButton;
    QToolButton *m_importIconButton;
    QVBoxLayout *m_layout;

    ProfileManager *m_profileManager = nullptr;
    ModalManager *m_modalManager = nullptr;
};

#endif // PROFILEMANAGERWIDGET_H
