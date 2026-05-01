#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>

#include <hyprtoolkit/core/Backend.hpp>

#include <memory>
#include <string>

#include "PolkitListener.hpp"

class CDialog;

class CAgent {
  public:
    CAgent();
    ~CAgent();

    bool start();

    // Called by listener when polkitd asks for a new authentication.
    // Spawns the dialog and stores the active session pointer.
    void beginAuth(CPolkitListener::SAuthRequest req);

    // Called by listener when the active PAM session is over (success/fail).
    // Tears down the dialog.
    void endAuth();

    // Called by listener when PAM emits a request/info/error.
    void onRequest(const std::string& prompt, bool echo);
    void onInfo(const std::string& text);
    void onError(const std::string& text);

    // Called by the dialog when the user submits or cancels.
    void submitPassword(const std::string& password);
    void cancel();

    // Called by the dialog ComboBox when the user picks another identity.
    void selectIdentity(const std::string& identityString);

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend() { return m_backend; }

    CPolkitListener&                                         listener() { return m_listener; }

  private:
    void                                                     schedulePump();

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> m_backend;

    CPolkitListener                                          m_listener;

    Hyprutils::Memory::CUniquePointer<CDialog>               m_dialog;
};

inline std::unique_ptr<CAgent> g_pAgent;
