/// @file ProgressModal.h
/// @brief Modal dialog showing a progress bar with an optional cancel button.
#ifndef PROGRESSMODAL_H
#define PROGRESSMODAL_H

#include "BaseModalContent.h"
#include <QString>

class QEvent;
class QLabel;
class QProgressBar;
class QPushButton;

/// @brief Displays long-running operation progress; optionally cancellable.
///
/// Pass a non-empty @p cancelButtonText to show the cancel button. Connect to
/// @c canceled() to abort the operation. Call @c setValue() from the background
/// thread via a queued signal connection.
class ProgressModal : public BaseModalContent {
    Q_OBJECT

public:
    explicit ProgressModal(const QString &labelText,
                          const QString &cancelButtonText = QString(),
                          int minimum = 0,
                          int maximum = 100,
                          QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setValue(int value);
    void setLabelText(const QString &text);
    void setCancelButtonEnabled(bool enabled);

signals:
    /// @brief Emitted when the user clicks the cancel button.
    void canceled();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi(const QString &labelText, const QString &cancelButtonText);

    QLabel *m_label;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelButton;
};

#endif // PROGRESSMODAL_H
