/**
 * @file ItchAuth.h
 * @brief Manual API key entry flow for itch.io authentication.
 */
#ifndef ITCHAUTH_H
#define ITCHAUTH_H

#include <QObject>
#include <QString>

/**
 * @brief Triggers a manual API key entry prompt for itch.io.
 *
 * Unlike NexusMods, itch.io has no automated SSO flow in TrenchKit. Calling
 * @c requestApiKey() emits @c apiKeyRequired, which the UI handles by showing
 * an input dialog. The entered key is then delivered via @c apiKeyEntered.
 */
class ItchAuth final : public QObject {
    Q_OBJECT

public:
    explicit ItchAuth(QObject *parent = nullptr);
    ~ItchAuth() override = default;

public slots:
    /**
     * @brief Signals that the user should be prompted to enter their itch.io API key.
     */
    void requestApiKey();

signals:
    /**
     * @brief Emitted to prompt the UI to show an API key input dialog.
     */
    void apiKeyRequired();
    /**
     * @brief Emitted with the key the user entered.
     */
    void apiKeyEntered(QString apiKey);
    void authenticationCancelled();
};

#endif // ITCHAUTH_H
