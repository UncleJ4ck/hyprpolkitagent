#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprtoolkit/element/Textbox.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/Text.hpp>

#include <string>

#include "../core/PolkitListener.hpp"

class CDialog {
  public:
    CDialog(const CPolkitListener::SAuthRequest&                            req,
            Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend>        backend);
    ~CDialog();

    void show();
    void close();

    void setPrompt(const std::string& text, bool echo);
    void setInfo(const std::string& text);
    void setError(const std::string& text);

  private:
    void build();

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend>        m_backend;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IWindow>         m_window;

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextboxElement> m_passwordField;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextElement>    m_errorLabel;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextElement>    m_infoLabel;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CButtonElement>  m_revealButton;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CButtonElement>  m_authButton;

    Hyprutils::Signal::CHyprSignalListener                          m_closeListener;

    CPolkitListener::SAuthRequest m_req;
    std::string                   m_currentPassword;
    bool                          m_passwordVisible = false;
    bool                          m_promptEcho      = false;
    std::string                   m_promptText      = "Password";
};
