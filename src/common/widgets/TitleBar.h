#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QPoint>

class QLabel;
class QPushButton;
class QHBoxLayout;

class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override = default;

    void setUpdateVisible(bool visible);

signals:
    void updateClicked();
    void settingsClicked();
    void closeClicked();
    void minimizeClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void setupConnections();

    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QPushButton *m_updateButton;
    QPushButton *m_settingsButton;
    QPushButton *m_minimizeButton;
    QPushButton *m_closeButton;
    QHBoxLayout *m_layout;

    QPoint m_dragPosition;
    bool m_dragging = false;
};

#endif // TITLEBAR_H
