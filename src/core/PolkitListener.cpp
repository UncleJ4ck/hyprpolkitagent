#include <QDebug>
#include <QInputDialog>

#include "PolkitListener.hpp"
#include "../QMLIntegration.hpp"
#include "Agent.hpp"
#include <polkitqt1-agent-session.h>

#include <print>

using namespace PolkitQt1::Agent;

CPolkitListener::CPolkitListener(QObject* parent) : Listener(parent) {
    ;
}

void CPolkitListener::startAuth(const PendingAuth& req) {
    if (req.identities.isEmpty()) {
        if (req.result) {
            req.result->setError("No identities, this is a problem with your system configuration.");
            req.result->setCompleted();
        }
        std::print("> REJECTING: No idents\n");
        return;
    }

    session.selectedUser = req.identities.at(0);
    session.cookie       = req.cookie;
    session.result       = req.result;
    session.actionId     = req.actionId;
    session.message      = req.message;
    session.iconName     = req.iconName;
    session.details      = req.details;
    session.gainedAuth   = false;
    session.cancelled    = false;
    session.inProgress   = true;

    g_pAgent->initAuthPrompt();

    reattempt();
}

void CPolkitListener::startNextQueued() {
    if (session.inProgress || m_queue.empty())
        return;

    const PendingAuth next = m_queue.front();
    m_queue.pop_front();
    startAuth(next);
}

void CPolkitListener::initiateAuthentication(const QString& actionId, const QString& message, const QString& iconName, const PolkitQt1::Details& details, const QString& cookie,
                                             const PolkitQt1::Identity::List& identities, AsyncResult* result) {

    std::print("> New authentication session\n");

    PendingAuth req;
    req.actionId   = actionId;
    req.message    = message;
    req.iconName   = iconName;
    req.details    = details;
    req.cookie     = cookie;
    req.identities = identities;
    req.result     = result;

    if (identities.isEmpty()) {
        if (result) {
            result->setError("No identities, this is a problem with your system configuration.");
            result->setCompleted();
        }
        std::print("> REJECTING: No idents\n");
        return;
    }

    if (session.inProgress) {
        m_queue.push_back(req);
        std::print("> QUEUED: Another session present. Queue size: {}\n", m_queue.size());
        return;
    }

    startAuth(req);
}

void CPolkitListener::reattempt() {
    session.cancelled = false;

    session.session = new Session(session.selectedUser, session.cookie, session.result);
    connect(session.session, SIGNAL(request(QString, bool)), this, SLOT(request(QString, bool)));
    connect(session.session, SIGNAL(completed(bool)), this, SLOT(completed(bool)));
    connect(session.session, SIGNAL(showError(QString)), this, SLOT(showError(QString)));
    connect(session.session, SIGNAL(showInfo(QString)), this, SLOT(showInfo(QString)));

    session.session->initiate();
}

bool CPolkitListener::initiateAuthenticationFinish() {
    std::print("> initiateAuthenticationFinish()\n");
    return true;
}

void CPolkitListener::cancelAuthentication() {
    std::print("> cancelAuthentication()\n");

    session.cancelled = true;

    finishAuth();
}

void CPolkitListener::request(const QString& request, bool echo) {
    std::print("> PKS request: {} echo: {}\n", request.toStdString(), echo);
}

void CPolkitListener::completed(bool gainedAuthorization) {
    std::print("> PKS completed: {}\n", gainedAuthorization ? "Auth successful" : "Auth unsuccessful");

    session.gainedAuth = gainedAuthorization;

    if (!gainedAuthorization && g_pAgent->authState.qmlIntegration)
        g_pAgent->authState.qmlIntegration->setError("Authentication failed");

    finishAuth();
}

void CPolkitListener::showError(const QString& text) {
    std::print("> PKS showError: {}\n", text.toStdString());

    if (g_pAgent->authState.qmlIntegration)
        g_pAgent->authState.qmlIntegration->setError(text);
}

void CPolkitListener::showInfo(const QString& text) {
    std::print("> PKS showInfo: {}\n", text.toStdString());
}

void CPolkitListener::finishAuth() {
    if (!session.inProgress) {
        std::print("> finishAuth: ODD. !session.inProgress\n");
        return;
    }

    if (!session.gainedAuth && !session.cancelled) {
        std::print("> finishAuth: Did not gain auth. Reattempting.\n");
        if (g_pAgent->authState.qmlIntegration)
            g_pAgent->authState.qmlIntegration->blockInput(false);
        session.session->deleteLater();
        reattempt();
        return;
    }

    std::print("> finishAuth: Gained auth, cleaning up.\n");

    session.inProgress = false;

    if (session.session) {
        session.session->result()->setCompleted();
        session.session->deleteLater();
    } else
        session.result->setCompleted();

    g_pAgent->resetAuthState();

    startNextQueued();
}

void CPolkitListener::submitPassword(const QString& pass) {
    if (!session.session)
        return;

    session.session->setResponse(pass);
    if (g_pAgent->authState.qmlIntegration)
        g_pAgent->authState.qmlIntegration->blockInput(true);
}

void CPolkitListener::cancelPending() {
    if (!session.session)
        return;

    session.session->cancel();

    session.cancelled = true;

    finishAuth();
}
