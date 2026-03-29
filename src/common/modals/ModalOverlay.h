/**
 * @file ModalOverlay.h
 * @brief Full-window overlay widget that fades in/out around a ModalContainer.
 */
#ifndef MODALOVERLAY_H
#define MODALOVERLAY_H

#include <QWidget>

class BaseModalContent;
class ModalContainer;
class QPropertyAnimation;

/**
 * @brief Semi-transparent overlay that presents a @c ModalContainer over the main window.
 *
 * Fades in when shown and fades out when @c close() is called. After the fade-out
 * animation completes, the overlay calls @c deleteLater() on itself and emits
 * @c closed(result). @c ModalManager listens for @c closed() to pop the stack.
 */
class ModalOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ getOpacity WRITE setOpacity)

public:
    explicit ModalOverlay(BaseModalContent *content, QWidget *parent = nullptr);
    ~ModalOverlay() override = default;

    BaseModalContent* content() const { return m_content; }

    void show();
    /**
     * @brief Triggers the fade-out animation; emits @c closed(result) when done.
     */
    void close();

signals:
    /**
     * @brief Emitted after the fade-out completes; @p result is from @c BaseModalContent::result().
     */
    void closed(int result);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void fadeIn();
    void fadeOut();
    void centerContainer();

    qreal getOpacity() const { return m_opacity; }
    void setOpacity(qreal opacity);

    BaseModalContent *m_content;
    ModalContainer *m_container;
    QPropertyAnimation *m_fadeAnimation;
    qreal m_opacity = 0.0;
};

#endif // MODALOVERLAY_H
