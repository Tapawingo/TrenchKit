/**
 * @file ItchRegistrationModalContent.h
 * @brief Modal for linking an existing local mod to an itch.io game page.
 */
#ifndef ITCHREGISTRATIONMODALCONTENT_H
#define ITCHREGISTRATIONMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ItchUploadInfo.h"
#include <QString>
#include <QList>

class ItchClient;
class ItchAuth;
class ModalManager;
class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class QListWidget;

/**
 * @brief Links an already-installed mod to its itch.io game page (metadata only; no re-download).
 *
 * The user enters an itch.io URL; the modal fetches game info and upload listings.
 * On @c Accepted, call the result getters to obtain the itch.io metadata to
 * write back to the mod's @c ModInfo.
 */
class ItchRegistrationModalContent : public BaseModalContent {
    Q_OBJECT

public:
    explicit ItchRegistrationModalContent(ItchClient *client,
                                         ItchAuth *auth,
                                         ModalManager *modalManager,
                                         const QString &modId,
                                         const QString &modName,
                                         QWidget *parent = nullptr);

    QList<ItchUploadInfo> getSelectedUploads() const { return m_selectedUploads; }
    QString getGameId() const { return m_currentGameId; }
    QString getAuthor() const { return m_author; }
    QString getUrl() const { return m_url; }

private slots:
    void onFetchClicked();
    void onAuthenticateClicked();
    void onAuthStarted(const QString &browserUrl);
    void onAuthComplete(const QString &apiKey);
    void onAuthFailed(const QString &error);
    void onGameInfoReceived(const QString &gameId, const QString &title, const QString &author);
    void onUploadsReceived(const QList<ItchUploadInfo> &uploads);
    void onError(const QString &error);

private:
    void setupUi();
    QWidget* createInputPage();
    QWidget* createAuthPage();
    void showInputPage();
    void showAuthPage();
    void updateFooterButtons();
    QString formatFileSize(qint64 bytes) const;

    ItchClient *m_client;
    ItchAuth *m_auth;
    ModalManager *m_modalManager;
    QString m_localModId;
    QString m_localModName;

    QStackedWidget *m_stack;
    QLineEdit *m_urlEdit;
    QLineEdit *m_apiKeyEdit;
    QPushButton *m_fetchButton;
    QPushButton *m_authenticateButton;
    QPushButton *m_cancelButton;
    QLabel *m_authStatusLabel;

    QList<ItchUploadInfo> m_selectedUploads;

    QString m_currentGameId;
    QString m_author;
    QString m_url;
    QString m_pendingUrl;

    enum Page { InputPage, AuthPage };
};

#endif // ITCHREGISTRATIONMODALCONTENT_H
