/// @file ModRowWidget.h
/// @brief Widget representing a single row in the mod list.
#ifndef MODROWWIDGET_H
#define MODROWWIDGET_H

#include "core/models/ModInfo.h"
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QPoint>

class ConflictTooltip;
class QContextMenuEvent;
struct ConflictInfo;

/// @brief Displays one mod's name, enable toggle, date, and status indicators (update/conflict/notice).
class ModRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModRowWidget(const ModInfo &mod, QWidget *parent = nullptr);
    ~ModRowWidget() override = default;

    QString modId() const { return m_modId; }
    void updateModInfo(const ModInfo &mod);
    void setSelected(bool selected);
    /// @brief Shows or hides the update badge; @p version is displayed in the button tooltip.
    void setUpdateAvailable(bool available, const QString &version);
    void hideUpdateButton();
    /// @brief Updates the conflict indicator icon and tooltip from @p info.
    void setConflictInfo(const ConflictInfo &info);
    void clearConflictIndicator();
    void setNotice(const QString &text, const QString &iconType);
    void setDependencyStatus(const QString &text);

signals:
    void enabledChanged(const QString &modId, bool enabled);
    void renameRequested(const QString &modId);
    void editMetaRequested(const QString &modId);
    void removeRequested(const QString &modId);
    void updateRequested(const QString &modId);
    void conflictDetailsRequested(const QString &modId);
    void registerWithNexusRequested(const QString &modId);
    void registerWithItchRequested(const QString &modId);
    void contextMenuRequested(const QPoint &globalPos);

protected:
    void changeEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi(const ModInfo &mod);
    void retranslateUi();
    void updateStyling();
    void updateNameEliding();
    QString formatConflictTooltip(const ConflictInfo &info) const;
    QString noticeIconPath(const QString &iconType) const;

    QString m_modId;
    QString m_fullModName;
    QCheckBox *m_enabledCheckBox;
    QLabel *m_nameLabel;
    QLabel *m_dateLabel;
    QPushButton *m_updateButton;
    QPushButton *m_conflictButton;
    QPushButton *m_dependencyButton;
    QPushButton *m_noticeButton;
    ConflictTooltip *m_conflictTooltip = nullptr;
    ConflictTooltip *m_dependencyTooltip = nullptr;
    ConflictTooltip *m_noticeTooltip = nullptr;
    QString m_conflictTooltipText;
    QString m_dependencyText;
    QString m_noticeText;
    bool m_selected = false;
};

#endif // MODROWWIDGET_H
