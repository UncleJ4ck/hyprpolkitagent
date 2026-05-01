#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE 1

#include "PolkitListener.hpp"

#include "Agent.hpp"

#include <print>
#include <unistd.h>

#include <sdbus-c++/sdbus-c++.h>

namespace {
    bool isSessionLocked() {
        try {
            auto conn  = sdbus::createSystemBusConnection();
            auto proxy = sdbus::createProxy(*conn, sdbus::ServiceName{"org.freedesktop.login1"},
                                            sdbus::ObjectPath{"/org/freedesktop/login1/session/auto"});
            sdbus::Variant v;
            proxy->callMethod("Get").onInterface("org.freedesktop.DBus.Properties").withArguments(std::string{"org.freedesktop.login1.Session"}, std::string{"LockedHint"}).storeResultsTo(v);
            return v.get<bool>();
        } catch (...) {
            return false;
        }
    }
}

// ----- HpaListener GObject -----

struct _HpaListener {
    PolkitAgentListener parent_instance;
};

G_DEFINE_FINAL_TYPE(HpaListener, hpa_listener, POLKIT_AGENT_TYPE_LISTENER)

static void hpa_listener_initiate_authentication(PolkitAgentListener* listener, const gchar* action_id, const gchar* message, const gchar* icon_name, PolkitDetails* details, const gchar* cookie,
                                                 GList* identities, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    GTask* task = g_task_new(listener, cancellable, callback, user_data);

    CPolkitListener::SAuthRequest req;
    req.actionId = action_id ? action_id : "";
    req.message  = message ? message : "";
    req.iconName = icon_name ? icon_name : "";
    req.cookie   = cookie ? cookie : "";
    req.task     = task;

    for (GList* l = identities; l; l = l->next) {
        PolkitIdentity* id  = static_cast<PolkitIdentity*>(l->data);
        gchar*          str = polkit_identity_to_string(id);
        std::string     raw = str ? str : "";
        g_free(str);
        std::string display = raw;
        if (raw.starts_with("unix-user:"))
            display = raw.substr(10);
        req.identities.push_back({raw, display});
        req.gIdentities = g_list_append(req.gIdentities, g_object_ref(id));
    }

    if (details) {
        const gchar* cmd = polkit_details_lookup(details, "command_line");
        if (!cmd)
            cmd = polkit_details_lookup(details, "cmdline");
        if (!cmd)
            cmd = polkit_details_lookup(details, "command");
        req.command = cmd ? cmd : "";

        const gchar* v;
        v             = polkit_details_lookup(details, "polkit.vendor");
        req.vendor    = v ? v : "";
        v             = polkit_details_lookup(details, "polkit.vendor_url");
        req.vendorUrl = v ? v : "";

        gchar** keys = polkit_details_get_keys(details);
        if (keys) {
            for (int i = 0; keys[i]; i++) {
                std::string k = keys[i];
                if (k.starts_with("polkit."))
                    continue;
                if (k == "command_line" || k == "cmdline" || k == "command")
                    continue;
                const gchar* val = polkit_details_lookup(details, keys[i]);
                if (val && *val)
                    req.details.emplace_back(k, val);
            }
            g_strfreev(keys);
        }
    }

    g_pAgent->listener().initiateAuth(std::move(req));
}

static gboolean hpa_listener_initiate_authentication_finish(PolkitAgentListener* listener, GAsyncResult* res, GError** error) {
    return g_task_propagate_boolean(G_TASK(res), error);
}

static void hpa_listener_class_init(HpaListenerClass* klass) {
    PolkitAgentListenerClass* alc       = POLKIT_AGENT_LISTENER_CLASS(klass);
    alc->initiate_authentication        = hpa_listener_initiate_authentication;
    alc->initiate_authentication_finish = hpa_listener_initiate_authentication_finish;
}

static void hpa_listener_init(HpaListener*) {}

// ----- CPolkitListener -----

CPolkitListener::CPolkitListener() = default;

CPolkitListener::~CPolkitListener() {
    if (m_session)
        teardownSession();
    if (m_registrationHandle)
        polkit_agent_listener_unregister(m_registrationHandle);
    if (m_listenerObject)
        g_object_unref(m_listenerObject);
    if (m_subject)
        g_object_unref(m_subject);
    if (m_selectedUser)
        g_object_unref(m_selectedUser);
}

bool CPolkitListener::registerAgent() {
    GError* error = nullptr;
    m_subject     = polkit_unix_session_new_for_process_sync(getpid(), nullptr, &error);
    if (!m_subject) {
        std::print(stderr, "failed to create subject: {}\n", error ? error->message : "unknown");
        if (error)
            g_error_free(error);
        return false;
    }

    m_listenerObject = static_cast<HpaListener*>(g_object_new(hpa_listener_get_type(), nullptr));

    m_registrationHandle = polkit_agent_listener_register(POLKIT_AGENT_LISTENER(m_listenerObject), POLKIT_AGENT_REGISTER_FLAGS_NONE, m_subject, "/org/hyprland/PolicyKit1/AuthenticationAgent",
                                                          nullptr, &error);

    if (!m_registrationHandle) {
        std::print(stderr, "failed to register agent: {}\n", error ? error->message : "unknown");
        if (error)
            g_error_free(error);
        return false;
    }

    std::print(stderr, "registered polkit agent\n");
    return true;
}

static void rejectRequest(CPolkitListener::SAuthRequest& req, const char* msg) {
    std::print(stderr, "> rejecting auth: {}\n", msg);
    GError* err = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED, msg);
    g_task_return_error(req.task, err);
    g_object_unref(req.task);
    if (req.gIdentities)
        g_list_free_full(req.gIdentities, g_object_unref);
}

void CPolkitListener::initiateAuth(SAuthRequest req) {
    if (m_inProgress) {
        std::print(stderr, "> queued auth request, queue size {}\n", m_queue.size() + 1);
        m_queue.push_back(std::move(req));
        return;
    }

    if (req.identities.empty()) {
        rejectRequest(req, "No identities");
        return;
    }
    if (isSessionLocked()) {
        rejectRequest(req, "Session is locked");
        return;
    }

    m_current = std::move(req);

    const std::string preferred = "unix-user:" + std::to_string(::geteuid());
    int               idx       = 0;
    for (size_t i = 0; i < m_current.identities.size(); i++) {
        if (m_current.identities[i].raw == preferred) {
            idx = (int)i;
            break;
        }
    }
    if (m_selectedUser)
        g_object_unref(m_selectedUser);
    m_selectedUser = static_cast<PolkitIdentity*>(g_object_ref(g_list_nth_data(m_current.gIdentities, idx)));

    m_inProgress = true;
    m_gainedAuth = false;
    m_cancelled  = false;

    g_pAgent->beginAuth(m_current);

    startSession();
}

void CPolkitListener::startSession() {
    m_session = polkit_agent_session_new(m_selectedUser, m_current.cookie.c_str());
    g_signal_connect(m_session, "request", G_CALLBACK(onRequestStatic), this);
    g_signal_connect(m_session, "show-error", G_CALLBACK(onShowErrorStatic), this);
    g_signal_connect(m_session, "show-info", G_CALLBACK(onShowInfoStatic), this);
    g_signal_connect(m_session, "completed", G_CALLBACK(onCompletedStatic), this);
    polkit_agent_session_initiate(m_session);
}

void CPolkitListener::teardownSession() {
    if (!m_session)
        return;
    g_signal_handlers_disconnect_by_data(m_session, this);
    g_object_unref(m_session);
    m_session = nullptr;
}

void CPolkitListener::completeCurrent(bool gainedAuth, bool cancelled) {
    if (!m_inProgress)
        return;

    if (!gainedAuth && !cancelled) {
        // PAM said no but user didn't cancel: clear the field, restart session for retry.
        std::print(stderr, "> auth failed, retrying\n");
        teardownSession();
        startSession();
        return;
    }

    std::print(stderr, "> auth {}\n", gainedAuth ? "succeeded" : "cancelled");

    m_inProgress = false;
    m_gainedAuth = gainedAuth;
    m_cancelled  = cancelled;

    teardownSession();

    g_task_return_boolean(m_current.task, TRUE);
    g_object_unref(m_current.task);
    m_current.task = nullptr;

    if (m_current.gIdentities) {
        g_list_free_full(m_current.gIdentities, g_object_unref);
        m_current.gIdentities = nullptr;
    }
    if (m_selectedUser) {
        g_object_unref(m_selectedUser);
        m_selectedUser = nullptr;
    }

    g_pAgent->endAuth();

    startNextQueued();
}

void CPolkitListener::startNextQueued() {
    if (m_inProgress || m_queue.empty())
        return;
    SAuthRequest next = std::move(m_queue.front());
    m_queue.pop_front();
    initiateAuth(std::move(next));
}

void CPolkitListener::submitResponse(const std::string& password) {
    if (!m_session)
        return;
    polkit_agent_session_response(m_session, password.c_str());
}

void CPolkitListener::cancelCurrent() {
    if (!m_inProgress)
        return;
    m_cancelled = true;
    if (m_session) {
        g_signal_handlers_disconnect_by_data(m_session, this);
        polkit_agent_session_cancel(m_session);
    }
    completeCurrent(false, true);
}

void CPolkitListener::selectIdentity(const std::string& identityString) {
    if (!m_inProgress)
        return;

    int idx = -1;
    for (size_t i = 0; i < m_current.identities.size(); i++) {
        if (m_current.identities[i].raw == identityString) {
            idx = (int)i;
            break;
        }
    }
    if (idx < 0)
        return;

    PolkitIdentity* newId = static_cast<PolkitIdentity*>(g_list_nth_data(m_current.gIdentities, idx));
    if (!newId)
        return;
    if (m_selectedUser && polkit_identity_equal(m_selectedUser, newId))
        return;

    if (m_selectedUser)
        g_object_unref(m_selectedUser);
    m_selectedUser = static_cast<PolkitIdentity*>(g_object_ref(newId));

    teardownSession();
    g_pAgent->onError("");
    g_pAgent->onInfo("");
    startSession();
}

std::string CPolkitListener::selectedIdentity() const {
    if (!m_selectedUser)
        return "";
    gchar*      s   = polkit_identity_to_string(m_selectedUser);
    std::string out = s ? s : "";
    g_free(s);
    return out;
}

void CPolkitListener::onRequestStatic(PolkitAgentSession*, gchar* request, gboolean echoOn, gpointer) {
    g_pAgent->onRequest(request ? request : "", echoOn);
}

void CPolkitListener::onShowErrorStatic(PolkitAgentSession*, gchar* text, gpointer) {
    g_pAgent->onError(text ? text : "");
}

void CPolkitListener::onShowInfoStatic(PolkitAgentSession*, gchar* text, gpointer) {
    g_pAgent->onInfo(text ? text : "");
}

void CPolkitListener::onCompletedStatic(PolkitAgentSession*, gboolean gained, gpointer self) {
    auto* l = static_cast<CPolkitListener*>(self);
    if (!gained)
        g_pAgent->onError("Authentication failed");
    l->completeCurrent(gained, false);
}
