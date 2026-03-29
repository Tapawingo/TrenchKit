/**
 * @file InputModal.h
 * @brief Modal dialog that prompts the user for a single line of text input.
 */
#ifndef INPUTMODAL_H
#define INPUTMODAL_H

#include "BaseModalContent.h"
#include <QString>

class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief Single-field text input modal; call @c textValue() after @c Accepted.
 */
class InputModal : public BaseModalContent {
    Q_OBJECT

public:
    explicit InputModal(const QString &title,
                       const QString &label,
                       const QString &defaultValue = QString(),
                       QWidget *parent = nullptr);

    /**
     * @brief Returns the text the user entered; valid only after @c result() == @c Accepted.
     */
    QString textValue() const;

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi(const QString &label, const QString &defaultValue);
    void retranslateUi();

    QLineEdit *m_lineEdit;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
};

#endif // INPUTMODAL_H
