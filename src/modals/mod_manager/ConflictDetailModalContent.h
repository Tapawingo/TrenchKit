/// @file ConflictDetailModalContent.h
/// @brief Modal showing the full file conflict details for a single mod.
#ifndef CONFLICTDETAILMODALCONTENT_H
#define CONFLICTDETAILMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/services/ModConflictDetector.h"
#include <QString>

class QListWidget;
class QLabel;
class QVBoxLayout;

/// @brief Read-only view of the @c ConflictInfo for a given mod; lists all conflicting pak paths.
class ConflictDetailModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit ConflictDetailModalContent(const QString &modName,
                                       const ConflictInfo &conflictInfo,
                                       QWidget *parent = nullptr);

private:
    void setupUi();
    QString formatModListText() const;

    QString m_modName;
    ConflictInfo m_conflictInfo;
    QListWidget *m_fileList;
};

#endif // CONFLICTDETAILMODALCONTENT_H
