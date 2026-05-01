#pragma once

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE 1

#include <glib.h>
#include <gio/gio.h>
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

#include <deque>
#include <string>
#include <vector>

// GObject subclass declaration. Defined in PolkitListener.cpp.
G_BEGIN_DECLS

#define HPA_TYPE_LISTENER (hpa_listener_get_type())
G_DECLARE_FINAL_TYPE(HpaListener, hpa_listener, HPA, LISTENER, PolkitAgentListener)

G_END_DECLS

class CPolkitListener {
  public:
    struct SIdentity {
        std::string raw;     // e.g. "unix-user:1000"
        std::string display; // "1000" or "alice"
    };

    struct SAuthRequest {
        std::string                actionId;
        std::string                message;
        std::string                iconName;
        std::string                cookie;
        std::vector<SIdentity>     identities;
        // raw polkit identity list, kept alive for the session lifetime.
        GList*                     gIdentities = nullptr;
        // pending GSimpleAsyncResult callback; resolved on completion.
        GTask*                     task        = nullptr;
        // command_line / cmdline / etc. detail value (already stripped from extras).
        std::string                command;
        // remaining details for the disclosure section.
        std::vector<std::pair<std::string, std::string>> details;
        // vendor / vendor_url polkit annotations.
        std::string                vendor;
        std::string                vendorUrl;
    };

    CPolkitListener();
    ~CPolkitListener();

    // Registers our agent with polkitd.
    bool registerAgent();

    // Called by the GObject vtable on every initiate_authentication.
    void initiateAuth(SAuthRequest req);

    // User submitted a password via the dialog.
    void submitResponse(const std::string& password);

    // User cancelled (or session got cancelled externally).
    void cancelCurrent();

    // Switch identity mid-flow. Cancels current PAM session and starts a new one.
    void selectIdentity(const std::string& identityString);

    // The active session's selected user, "" if no session.
    std::string selectedIdentity() const;

  private:
    void startSession();      // creates PolkitAgentSession for the current request
    void completeCurrent(bool gainedAuth, bool cancelled);
    void teardownSession();
    void startNextQueued();

    // PolkitAgentSession callbacks (static C trampolines + member impls).
    static void onRequestStatic(PolkitAgentSession* s, gchar* request, gboolean echoOn, gpointer self);
    static void onShowErrorStatic(PolkitAgentSession* s, gchar* text, gpointer self);
    static void onShowInfoStatic(PolkitAgentSession* s, gchar* text, gpointer self);
    static void onCompletedStatic(PolkitAgentSession* s, gboolean gainedAuth, gpointer self);

    HpaListener*       m_listenerObject  = nullptr;
    gpointer           m_registrationHandle = nullptr;
    PolkitSubject*     m_subject         = nullptr;

    PolkitAgentSession* m_session        = nullptr;

    SAuthRequest       m_current;
    bool               m_inProgress      = false;
    bool               m_gainedAuth      = false;
    bool               m_cancelled       = false;
    PolkitIdentity*    m_selectedUser    = nullptr;

    std::deque<SAuthRequest> m_queue;
};
