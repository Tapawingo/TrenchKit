/// @file ProfileRowWidget.h
/// @brief Widget representing a single row in the profile list.
#ifndef PROFILEROWWIDGET_H
#define PROFILEROWWIDGET_H

#include <QWidget>
#include <QString>
#include <QEvent>

class QLabel;

/// @brief Displays one profile's name and active indicator; emits context-action signals.
class ProfileRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProfileRowWidget(const QString &profileId, const QString &profileName, QWidget *parent = nullptr);
    ~ProfileRowWidget() override = default;

    /// @brief Highlights the row as the currently active (last applied) profile.
    void setActive(bool active);
    void setSelected(bool selected);
    QString profileId() const { return m_profileId; }

signals:
    void exportRequested(const QString &profileId);
    void renameRequested(const QString &profileId);
    void deleteRequested(const QString &profileId);
    void clicked(const QString &profileId);

protected:
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupUi(const QString &profileName);
    void retranslateUi();
    void updateStyling();

    QString m_profileId;
    QString m_profileName;
    bool m_isActive = false;
    bool m_isSelected = false;

    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
};

#endif // PROFILEROWWIDGET_H
