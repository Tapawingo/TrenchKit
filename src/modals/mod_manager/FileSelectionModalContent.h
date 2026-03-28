/// @file FileSelectionModalContent.h
/// @brief Modal for selecting one or more files or items from a list.
#ifndef FILESELECTIONMODALCONTENT_H
#define FILESELECTIONMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include <QString>
#include <QStringList>
#include <QList>

class QEvent;
class QListWidget;
class QLabel;
class QPushButton;

/// @brief Generic list item with an opaque ID and a display string.
struct FileItem {
    QString id;
    QString displayText;
};

/// @brief Presents a list of files or generic items for single or multi-selection.
///
/// Two constructors: one for a simple list of pak file paths, and one for a
/// list of @c FileItem entries with arbitrary IDs. Optionally shows an
/// "Ignore All" button (emits @c allIgnored) for upload-selection scenarios.
class FileSelectionModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit FileSelectionModalContent(const QStringList &pakFiles,
                                      const QString &archiveName,
                                      bool multiSelect = false,
                                      QWidget *parent = nullptr);

    explicit FileSelectionModalContent(const QList<FileItem> &items,
                                      const QString &title,
                                      const QString &headerText,
                                      bool multiSelect = false,
                                      bool showIgnoreButton = false,
                                      QWidget *parent = nullptr);

    QString getSelectedFile() const { return m_selectedFile; }
    QStringList getSelectedFiles() const { return m_selectedFiles; }
    QStringList getSelectedIds() const { return m_selectedIds; }

public slots:
    void accept() override;
    void ignoreAll();

signals:
    /// @brief Emitted when the user clicks "Ignore All".
    void allIgnored();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi(const QString &headerText);
    void retranslateUi();
    void applyListStyling();

    QListWidget *m_fileList;
    QPushButton *m_okButton;
    QPushButton *m_ignoreButton;
    QPushButton *m_cancelButton;
    QString m_selectedFile;
    QStringList m_selectedFiles;
    QStringList m_selectedIds;
    QList<FileItem> m_fileItems;
    bool m_multiSelect;
    bool m_useFileItems;
    bool m_showIgnoreButton;
};

#endif // FILESELECTIONMODALCONTENT_H
