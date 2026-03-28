/// @file NexusModsAuth.h
/// @brief WebSocket SSO authentication flow for NexusMods.
#pragma once

#include <QObject>
#include <QWebSocket>
#include <QString>

/// @brief Handles the NexusMods WebSocket-based single-sign-on flow.
///
/// Connects to the NexusMods SSO endpoint, emits @c authenticationStarted with
/// a browser URL the user must visit to approve the connection, then waits for
/// the API key to arrive over the WebSocket.
class NexusModsAuth final : public QObject {
    Q_OBJECT

public:
    explicit NexusModsAuth(QObject *parent = nullptr);
    ~NexusModsAuth() override = default;

public slots:
    /// @brief Opens the WebSocket connection and emits @c authenticationStarted with the approval URL.
    void startAuthentication();
    void cancelAuthentication();

signals:
    /// @brief Emitted once the WebSocket is ready; @p browserUrl must be opened by the user.
    void authenticationStarted(QString browserUrl);
    /// @brief Emitted when the user approves the connection; @p apiKey is ready to use.
    void authenticationComplete(QString apiKey);
    void authenticationFailed(QString error);
    void authenticationCancelled();

private slots:
    void onConnected();
    void onTextMessageReceived(const QString &message);
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    void cleanup();
    QString generateConnectionToken() const;

    QWebSocket m_socket;
    QString m_uuid;
    QString m_token;
    bool m_isAuthenticating = false;
};
