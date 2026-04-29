#pragma once

#include <deque>

#include <QObject>
#include <QString>

#include <polkitqt1-agent-listener.h>
#include <polkitqt1-identity.h>
#include <polkitqt1-details.h>
#include <polkitqt1-agent-session.h>

class CPolkitListener : public PolkitQt1::Agent::Listener {
    Q_OBJECT;
    Q_DISABLE_COPY(CPolkitListener);

  public:
    CPolkitListener(QObject* parent = nullptr);
    ~CPolkitListener() override {};

    void submitPassword(const QString& pass);
    void cancelPending();
    void selectUser(const QString& identityString);

  public Q_SLOTS:
    void initiateAuthentication(const QString& actionId, const QString& message, const QString& iconName, const PolkitQt1::Details& details, const QString& cookie,
                                const PolkitQt1::Identity::List& identities, PolkitQt1::Agent::AsyncResult* result) override;
    bool initiateAuthenticationFinish() override;
    void cancelAuthentication() override;

    void request(const QString& request, bool echo);
    void completed(bool gainedAuthorization);
    void showError(const QString& text);
    void showInfo(const QString& text);

  private:
    struct PendingAuth {
        QString                          actionId;
        QString                          message;
        QString                          iconName;
        QString                          cookie;
        PolkitQt1::Details               details;
        PolkitQt1::Identity::List        identities;
        PolkitQt1::Agent::AsyncResult*   result = nullptr;
    };

    struct {
        bool                           inProgress = false, cancelled = false, gainedAuth = false;
        QString                        cookie, message, iconName, actionId;
        PolkitQt1::Details             details;
        PolkitQt1::Agent::AsyncResult* result = nullptr;
        PolkitQt1::Identity            selectedUser;
        PolkitQt1::Identity::List      identities;
        PolkitQt1::Agent::Session*     session = nullptr;
    } session;

    std::deque<PendingAuth> m_queue;

    void reattempt();
    void finishAuth();
    void startAuth(const PendingAuth& req);
    void startNextQueued();

    friend class CAgent;
    friend class CQMLIntegration;
};
