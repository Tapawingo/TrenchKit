/**
 * @file GradientFrame.h
 * @brief QFrame subclass with a textured gradient background and styled border.
 */
#ifndef GRADIENTFRAME_H
#define GRADIENTFRAME_H

#include <QFrame>

/**
 * @brief Paints a dark textured gradient background used for top-level panel areas.
 */
class GradientFrame : public QFrame {
    Q_OBJECT

public:
    explicit GradientFrame(QWidget *parent = nullptr);
    ~GradientFrame() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static const QPixmap& getTexture();

    static constexpr const char* GRADIENT_LIGHT  = "#141413";
    static constexpr const char* GRADIENT_DARK   = "#141413";
    static constexpr const char* BACKGROUND_COLOR = "#16130e";
    static constexpr int BORDER_WIDTH  = 3;
    static constexpr int BORDER_RADIUS = 0;
};

#endif // GRADIENTFRAME_H
