/// @file NexusRegistrationModalContent.h
/// @brief Modal for linking an existing local mod to a NexusMods entry.
#ifndef NEXUSREGISTRATIONMODALCONTENT_H
#define NEXUSREGISTRATIONMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/NexusFileInfo.h"
#include <QString>
#include <QList>

class NexusModsClient;
class NexusModsAuth;
class ModalManager;
class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class QListWidget;

/// @brief Links an already-installed mod to its NexusMods page (metadata only; no re-download).
///
/// The user enters a NexusMods URL; the modal fetches mod info and file listings.
/// On @c Accepted, call the result getters to obtain the NexusMods metadata to
/// write back to the mod's @c ModInfo.
class NexusRegistrationModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit NexusRegistrationModalContent(NexusModsClient *client,
                                          NexusModsAuth *auth,
                                          ModalManager *modalManager,
                                          const QString &modId,
                                          const QString &modName,
                                          QWidget *parent = nullptr);

    QList<NexusFileInfo> getSelectedFiles() const { return m_selectedFiles; }
    QString getModId() const { return m_currentModId; }
    QString getAuthor() const { return m_author; }
    QString getDescription() const { return m_description; }
    QString getUrl() const { return m_url; }

private slots:
    void onFetchClicked();
    void onAuthenticateClicked();
    void onAuthStarted(const QString &browserUrl);
    void onAuthComplete(const QString &apiKey);
    void onAuthFailed(const QString &error);
    void onModInfoReceived(const QString &author, const QString &description, const QString &version);
    void onModFilesReceived(const QList<NexusFileInfo> &files);
    void onError(const QString &error);

private:
    void setupUi();
    QWidget* createInputPage();
    QWidget* createAuthPage();
    void showInputPage();
    void showAuthPage();
    void updateFooterButtons();
    QString formatFileSize(qint64 bytes) const;

    NexusModsClient *m_client;
    NexusModsAuth *m_auth;
    ModalManager *m_modalManager;
    QString m_localModId;
    QString m_localModName;

    QStackedWidget *m_stack;
    QLineEdit *m_urlEdit;
    QPushButton *m_fetchButton;
    QPushButton *m_authenticateButton;
    QPushButton *m_cancelButton;
    QLabel *m_authStatusLabel;

    QList<NexusFileInfo> m_selectedFiles;

    QString m_currentModId;
    QString m_author;
    QString m_description;
    QString m_url;

    enum Page { InputPage, AuthPage };
};

#endif // NEXUSREGISTRATIONMODALCONTENT_H
