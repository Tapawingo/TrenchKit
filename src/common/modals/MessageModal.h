/**
 * @file MessageModal.h
 * @brief Modal dialog for informational, warning, question, and error messages.
 */
#ifndef MESSAGEMODAL_H
#define MESSAGEMODAL_H

#include "BaseModalContent.h"
#include <QString>

class QEvent;
class QLabel;
class QPushButton;
class ModalManager;

/**
 * @brief Displays a message with an icon and one or more standard buttons.
 *
 * Use the static convenience helpers (@c information, @c warning, @c critical,
 * @c question) for common cases, or construct directly for custom button
 * combinations. Call @c clickedButton() after @c finished() to determine which
 * button the user pressed.
 */
class MessageModal : public BaseModalContent {
    Q_OBJECT

public:
    enum Icon {
        NoIcon,
        Information,
        Warning,
        Critical,
        Question
    };

    /**
     * @brief Flags controlling which buttons are shown; may be OR-combined.
     */
    enum StandardButton {
        NoButton = 0,
        Ok       = 1,
        Cancel   = 2,
        Yes      = 4,
        No       = 8
    };
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)

    explicit MessageModal(const QString &title,
                         const QString &text,
                         Icon icon = Information,
                         StandardButtons buttons = Ok,
                         QWidget *parent = nullptr);

    /**
     * @brief Returns which button closed the dialog.
     */
    StandardButton clickedButton() const { return m_clickedButton; }

    static void information(ModalManager *manager, const QString &title, const QString &text);
    static void warning(ModalManager *manager, const QString &title, const QString &text);
    static void critical(ModalManager *manager, const QString &title, const QString &text);
    /**
     * @brief Shows a Yes/No question dialog; returns true if the user clicked Yes.
     */
    static bool question(ModalManager *manager, const QString &title, const QString &text);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi(const QString &text, Icon icon, StandardButtons buttons);
    void retranslateUi();
    QString getIconText(Icon icon);

    QLabel *m_textLabel;
    QLabel *m_iconLabel;
    QPushButton *m_yesButton    = nullptr;
    QPushButton *m_noButton     = nullptr;
    QPushButton *m_okButton     = nullptr;
    QPushButton *m_cancelButton = nullptr;
    StandardButton m_clickedButton = NoButton;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MessageModal::StandardButtons)

#endif // MESSAGEMODAL_H
