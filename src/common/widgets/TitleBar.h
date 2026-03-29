/**
 * @file TitleBar.h
 * @brief Custom frameless title bar with drag support and window action buttons.
 */
#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QPoint>

class QLabel;
class QPushButton;
class QHBoxLayout;

/**
 * @brief Replaces the native window title bar; supports click-and-drag window movement.
 *
 * Shows the application icon, title, an optional update indicator button,
 * and minimize/close controls. Connect to the signals to handle actions in
 * @c MainWindow.
 */
class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override = default;

    /**
     * @brief Shows or hides the update notification button.
     */
    void setUpdateVisible(bool visible);
    /**
     * @brief Updates the maximize/restore button to reflect the current window state.
     */
    void setMaximized(bool maximized);

signals:
    void updateClicked();
    void settingsClicked();
    void maximizeClicked();
    void closeClicked();
    void minimizeClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void setupConnections();

    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QPushButton *m_updateButton;
    QPushButton *m_settingsButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_minimizeButton;
    QPushButton *m_closeButton;
    QHBoxLayout *m_layout;

    QPoint m_dragPosition;
    bool m_dragging = false;
};

#endif // TITLEBAR_H
