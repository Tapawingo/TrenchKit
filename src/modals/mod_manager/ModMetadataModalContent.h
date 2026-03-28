/// @file ModMetadataModalContent.h
/// @brief Modal for editing a mod's stored metadata fields.
#ifndef MODMETADATAMODALCONTENT_H
#define MODMETADATAMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ModInfo.h"

class QLineEdit;
class QTextEdit;
class QDateTimeEdit;

/// @brief Editable form for all @c ModInfo metadata fields except pak file content.
///
/// Call @c getModInfo() after @c Accepted to retrieve the updated @c ModInfo.
class ModMetadataModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit ModMetadataModalContent(const ModInfo &mod, QWidget *parent = nullptr);

    /// @brief Returns a @c ModInfo populated with the user's edits; valid after @c Accepted.
    ModInfo getModInfo() const;

private:
    void setupUi(const ModInfo &mod);

    QString m_modId;
    QLineEdit *m_nameEdit;
    QLineEdit *m_fileNameEdit;
    QTextEdit *m_descriptionEdit;
    QLineEdit *m_nexusModIdEdit;
    QLineEdit *m_nexusFileIdEdit;
    QLineEdit *m_nexusUrlEdit;
    QLineEdit *m_itchGameIdEdit;
    QLineEdit *m_itchUrlEdit;
    QLineEdit *m_versionEdit;
    QLineEdit *m_authorEdit;
    QDateTimeEdit *m_installDateEdit;
    QDateTimeEdit *m_uploadDateEdit;

    int m_priority;
    bool m_enabled;
    QString m_numberedFileName;
    QString m_itchUrl;
};

#endif // MODMETADATAMODALCONTENT_H
