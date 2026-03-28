/// @file DraggableProfileList.h
/// @brief QListWidget that supports drag-to-reorder for the profile list.
#ifndef DRAGGABLEPROFILELIST_H
#define DRAGGABLEPROFILELIST_H

#include <QListWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>

/// @brief Profile list with drag-to-reorder; emits @c itemsReordered after a drop.
class DraggableProfileList : public QListWidget {
    Q_OBJECT

public:
    explicit DraggableProfileList(QWidget *parent = nullptr);
    ~DraggableProfileList() override = default;

signals:
    /// @brief Emitted after the user drag-drops a profile row to a new position.
    void itemsReordered();

protected:
    void dropEvent(QDropEvent *event) override;
};

#endif // DRAGGABLEPROFILELIST_H
