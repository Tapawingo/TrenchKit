/**
 * @file PanelFrame.h
 * @brief QFrame subclass with a textured background used for interior panel areas.
 */
#ifndef PANELFRAME_H
#define PANELFRAME_H

#include <QFrame>

/**
 * @brief Paints the standard textured panel background used inside mod-list and profile-list panels.
 */
class PanelFrame : public QFrame {
    Q_OBJECT

public:
    explicit PanelFrame(QWidget *parent = nullptr);
    ~PanelFrame() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static const QPixmap& getTexture();
};

#endif // PANELFRAME_H
