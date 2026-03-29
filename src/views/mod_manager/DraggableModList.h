/**
 * @file DraggableModList.h
 * @brief QListWidget that supports drag-to-reorder and external file drop for mod installation.
 */
#ifndef DRAGGABLEMODLIST_H
#define DRAGGABLEMODLIST_H

#include <QListWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>

/**
 * @brief Extends QListWidget with two drop modes: row reorder and file drop.
 *
 * Dropping rows within the list emits @c itemsReordered. Dropping .pak or
 * archive files from outside the application emits @c filesDropped.
 */
class DraggableModList : public QListWidget {
    Q_OBJECT

public:
    explicit DraggableModList(QWidget *parent = nullptr);
    ~DraggableModList() override = default;

signals:
    /**
     * @brief Emitted after the user drag-drops a row to a new position.
     */
    void itemsReordered();
    /**
     * @brief Emitted when the user drops mod files onto the list from outside the app.
     */
    void filesDropped(const QStringList &filePaths);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;

private:
    bool isValidModFile(const QString &filePath) const;
};

#endif // DRAGGABLEMODLIST_H
