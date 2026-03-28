/// @file ModalManager.h
/// @brief Centralized LIFO stack manager for modal overlays.
#ifndef MODALMANAGER_H
#define MODALMANAGER_H

#include <QObject>
#include <QList>

class QWidget;
class BaseModalContent;
class ModalOverlay;

/// @brief Manages a LIFO stack of @c ModalOverlay instances shown over the main window.
///
/// Pass a @c BaseModalContent instance (without a parent) to @c showModal(); the
/// manager takes ownership and deletes it via @c deleteLater() after the overlay
/// fades out. Only the top overlay is interactive; lower overlays remain visible
/// but blocked.
class ModalManager : public QObject {
    Q_OBJECT

public:
    explicit ModalManager(QWidget *mainWindow, QObject *parent = nullptr);
    ~ModalManager() override = default;

    /// @brief Shows @p content in a new @c ModalOverlay on top of the stack.
    /// @note Do not give @p content a parent before calling this.
    void showModal(BaseModalContent *content);
    void closeCurrentModal();
    void closeAllModals();
    bool hasOpenModal() const;

signals:
    void modalOpened();
    /// @brief Emitted when the top overlay closes; @p result is the @c BaseModalContent::Result value.
    void modalClosed(int result);

private slots:
    void onOverlayClosed(int result);

private:
    QWidget *m_mainWindow;
    QList<ModalOverlay*> m_modalStack;
};

#endif // MODALMANAGER_H
