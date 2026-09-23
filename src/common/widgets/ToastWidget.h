/**
 * @file ToastWidget.h
 * @brief Small auto-dismissing banner for non-blocking feedback over another widget.
 */
#ifndef TOASTWIDGET_H
#define TOASTWIDGET_H

#include <QWidget>

class QLabel;
class QGraphicsOpacityEffect;

/// @brief Fades in over @p parent, sits for a few seconds, then fades out and deletes itself.
/// Non-blocking alternative to @c MessageModal for feedback during an ongoing flow.
class ToastWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Shows @p text over @p parent's top-right corner; @p isError tints it red instead of green.
    static void show(QWidget *parent, const QString &text, bool isError = false);

private:
    explicit ToastWidget(QWidget *parent, const QString &text, bool isError);

    void reposition();

    QLabel *m_label;
    QGraphicsOpacityEffect *m_opacityEffect;
};

#endif // TOASTWIDGET_H
