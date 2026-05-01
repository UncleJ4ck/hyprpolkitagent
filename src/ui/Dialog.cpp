#include "Dialog.hpp"

#include "../core/Agent.hpp"

#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Combobox.hpp>
#include <hyprtoolkit/system/Icons.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <hyprtoolkit/palette/Color.hpp>
#include <hyprtoolkit/core/Input.hpp>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <print>

using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;
using namespace Hyprtoolkit;

CDialog::CDialog(const CPolkitListener::SAuthRequest& req, CSharedPointer<IBackend> backend) : m_backend(backend), m_req(req) {
    build();
}

CDialog::~CDialog() {
    if (m_window)
        m_window->close();
}

void CDialog::show() {
    if (m_window)
        m_window->open();
}

void CDialog::close() {
    if (m_window) {
        m_window->close();
        m_window.reset();
    }
}

void CDialog::setPrompt(const std::string& text, bool echo) {
    m_promptText = text;
    if (m_promptText.empty())
        m_promptText = "Password";
    if (!m_promptText.empty() && m_promptText.back() == ':')
        m_promptText.pop_back();
    m_promptEcho = echo;

    if (m_passwordField) {
        m_currentPassword.clear();
        m_passwordVisible = false;
        // Two-step rebuild: changes text to force updateLabel (only fires on text diff),
        // then restores empty. Both steps needed even when text is already empty.
        m_passwordField->rebuild()->placeholder(std::string{m_promptText})->defaultText(std::string{"\x01"})->password(!echo)->commence();
        m_passwordField->rebuild()->defaultText(std::string{""})->password(!echo)->commence();
        if (m_revealButton)
            m_revealButton->rebuild()->label(std::string{"Show"})->commence();
        m_passwordField->focus();
    }
}

void CDialog::setInfo(const std::string& text) {
    if (m_infoLabel)
        m_infoLabel->rebuild()->text(std::string{text})->commence();
}

void CDialog::setError(const std::string& text) {
    if (m_errorLabel)
        m_errorLabel->rebuild()->text(std::string{text})->commence();
}

void CDialog::build() {
    m_window = CWindowBuilder::begin()
                   ->preferredSize({520, 400})
                   ->minSize({480, 300})
                   ->maxSize({600, 620})
                   ->appTitle("Authentication Required")
                   ->appClass("hyprpolkitagent")
                   ->commence();

    m_closeListener = m_window->m_events.closeRequest.listen([] { g_pAgent->cancel(); });

    // Enter key submits
    m_keyListener = m_window->m_events.keyboardKey.listen([this](const Input::SKeyboardKeyEvent& ev) {
        if (!ev.down || ev.repeat)
            return;
        if (ev.xkbKeysym == XKB_KEY_Return || ev.xkbKeysym == XKB_KEY_KP_Enter) {
            if (!m_currentPassword.empty())
                g_pAgent->submitPassword(m_currentPassword);
        } else if (ev.xkbKeysym == XKB_KEY_Escape) {
            g_pAgent->cancel();
        }
    });

    // Background
    m_window->m_rootElement->addChild(
        CRectangleBuilder::begin()
            ->color([] { return g_pAgent->backend()->getPalette()->m_colors.background; })
            ->commence());

    // Outer column — 88 % width, centered
    auto outer = CColumnLayoutBuilder::begin()
                     ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {0.88F, 1.F}})
                     ->commence();
    outer->setMargin(16);
    outer->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
    outer->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
    m_window->m_rootElement->addChild(outer);

    // ── Icon ─────────────────────────────────────────────────────────────────
    {
        const char* iconName = m_req.iconName.empty() ? "dialog-password" : m_req.iconName.c_str();
        auto        iconDesc = m_backend->systemIcons()->lookupIcon(iconName);
        if (!iconDesc || !iconDesc->exists())
            iconDesc = m_backend->systemIcons()->lookupIcon("system-lock-screen");
        if (iconDesc && iconDesc->exists()) {
            auto wrap = CRowLayoutBuilder::begin()->commence();
            wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
            wrap->addChild(CImageBuilder::begin()
                               ->icon(iconDesc)
                               ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {48, 48}})
                               ->commence());
            outer->addChild(wrap);
        }
    }

    // ── Title ─────────────────────────────────────────────────────────────────
    {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        wrap->addChild(CTextBuilder::begin()
                           ->text(std::string{"Authentication Required"})
                           ->fontSize({CFontSize::HT_FONT_H1})
                           ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                           ->commence());
        outer->addChild(wrap);
    }

    // ── Subtitle: "for <user>" or identity combobox ───────────────────────────
    {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        if (m_req.identities.size() > 1) {
            auto              cb = CComboboxBuilder::begin();
            std::vector<std::string> items;
            for (const auto& id : m_req.identities)
                items.push_back(id.display);
            cb->items(std::move(items));
            const std::string preferred = "unix-user:" + std::to_string(::geteuid());
            int               sel       = 0;
            for (size_t i = 0; i < m_req.identities.size(); i++) {
                if (m_req.identities[i].raw == preferred) {
                    sel = (int)i;
                    break;
                }
            }
            cb->currentItem((size_t)sel);
            cb->onChanged([this](CSharedPointer<CComboboxElement>, size_t idx) {
                if (idx < m_req.identities.size())
                    g_pAgent->selectIdentity(m_req.identities[idx].raw);
            });
            wrap->addChild(cb->commence());
        } else {
            const std::string display = m_req.identities.empty() ? "" : m_req.identities[0].display;
            wrap->addChild(CTextBuilder::begin()
                               ->text(std::string{"for "} + display)
                               ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.3); })
                               ->commence());
        }
        outer->addChild(wrap);
    }

    // ── Message ───────────────────────────────────────────────────────────────
    {
        std::string msg = m_req.message;
        if (!m_req.command.empty())
            msg = "Authentication is needed to run";

        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        wrap->addChild(CTextBuilder::begin()
                           ->text(std::string{msg})
                           ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                           ->commence());
        outer->addChild(wrap);
    }

    // ── Command box ───────────────────────────────────────────────────────────
    if (!m_req.command.empty()) {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        auto box = CRectangleBuilder::begin()
                       ->color([] { return g_pAgent->backend()->getPalette()->m_colors.base; })
                       ->borderColor([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.6); })
                       ->borderThickness(1)
                       ->rounding(8)
                       ->size({CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {0, 0}})
                       ->commence();
        box->setMargin(6);
        box->addChild(CTextBuilder::begin()
                          ->text(std::string{m_req.command})
                          ->fontFamily(std::string{"monospace"})
                          ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                          ->commence());
        wrap->addChild(box);
        outer->addChild(wrap);
    }

    // ── Password row ──────────────────────────────────────────────────────────
    {
        auto pwRow = CRowLayoutBuilder::begin()->commence();
        pwRow->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        pwRow->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        m_passwordField = CTextboxBuilder::begin()
                              ->placeholder(std::string{m_promptText})
                              ->password(true)
                              ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {340, 34}})
                              ->onTextEdited([this](CSharedPointer<CTextboxElement>, const std::string& s) {
                                  m_currentPassword = s;
                                  if (!s.empty() && m_errorLabel)
                                      m_errorLabel->rebuild()->text(std::string{""})->commence();
                              })
                              ->commence();
        pwRow->addChild(m_passwordField);

        m_revealButton = CButtonBuilder::begin()
                             ->label(std::string{"Show"})
                             ->noBorder(true)
                             ->onMainClick([this](CSharedPointer<CButtonElement>) {
                                 m_passwordVisible = !m_passwordVisible;
                                 // Force updateLabel (only triggers on text change) via sentinel swap
                                 const std::string pw = std::string{m_passwordField->currentText()};
                                 m_passwordField->rebuild()->defaultText(std::string{"\x01"})->password(!m_passwordVisible)->commence();
                                 m_passwordField->rebuild()->defaultText(std::string{pw})->password(!m_passwordVisible)->commence();
                                 m_revealButton->rebuild()->label(std::string{m_passwordVisible ? "Hide" : "Show"})->commence();
                                 m_passwordField->focus();
                             })
                             ->commence();
        pwRow->addChild(m_revealButton);
        outer->addChild(pwRow);
    }

    // ── Error / info labels ───────────────────────────────────────────────────
    {
        auto errWrap = CRowLayoutBuilder::begin()->commence();
        errWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        errWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        m_errorLabel = CTextBuilder::begin()
                           ->text(std::string{""})
                           ->color([] { return CHyprColor{0.9F, 0.4F, 0.4F, 1.F}; })
                           ->commence();
        errWrap->addChild(m_errorLabel);
        outer->addChild(errWrap);

        auto infoWrap = CRowLayoutBuilder::begin()->commence();
        infoWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        infoWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        m_infoLabel = CTextBuilder::begin()
                          ->text(std::string{""})
                          ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.4); })
                          ->commence();
        infoWrap->addChild(m_infoLabel);
        outer->addChild(infoWrap);
    }

    // ── Action buttons ────────────────────────────────────────────────────────
    {
        auto btnRow = CRowLayoutBuilder::begin()->commence();
        btnRow->setMargin(8);
        btnRow->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        btnRow->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        btnRow->addChild(CButtonBuilder::begin()
                             ->label(std::string{"Cancel"})
                             ->noBorder(true)
                             ->onMainClick([](CSharedPointer<CButtonElement>) { g_pAgent->cancel(); })
                             ->commence());

        m_authButton = CButtonBuilder::begin()
                           ->label(std::string{"Authenticate"})
                           ->onMainClick([this](CSharedPointer<CButtonElement>) {
                               if (!m_currentPassword.empty())
                                   g_pAgent->submitPassword(m_currentPassword);
                           })
                           ->commence();
        btnRow->addChild(m_authButton);
        outer->addChild(btnRow);
    }

    // ── Show details (only if there are details to show) ─────────────────────
    if (!m_req.details.empty()) {
        auto detailsWrap = CRowLayoutBuilder::begin()->commence();
        detailsWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        detailsWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        m_detailsButton = CButtonBuilder::begin()
                              ->label(std::string{"Show details"})
                              ->noBorder(true)
                              ->onMainClick([this](CSharedPointer<CButtonElement>) {
                                  m_detailsVisible = !m_detailsVisible;
                                  if (m_detailsVisible)
                                      m_detailsParent->addChild(m_detailsContainer);
                                  else
                                      m_detailsParent->removeChild(m_detailsContainer);
                                  m_detailsButton->rebuild()->label(std::string{m_detailsVisible ? "Hide details" : "Show details"})->commence();
                              })
                              ->commence();
        detailsWrap->addChild(m_detailsButton);
        outer->addChild(detailsWrap);

        // Build the details container (not yet added to tree)
        auto detailsCol = CColumnLayoutBuilder::begin()->commence();
        detailsCol->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        detailsCol->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        for (const auto& [key, val] : m_req.details) {
            auto rowWrap = CRowLayoutBuilder::begin()->commence();
            rowWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            rowWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
            rowWrap->addChild(CTextBuilder::begin()
                                  ->text(key + ": " + val)
                                  ->fontSize({CFontSize::HT_FONT_SMALL})
                                  ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.3); })
                                  ->commence());
            detailsCol->addChild(rowWrap);
        }

        m_detailsContainer = detailsCol;
        m_detailsParent    = outer;
    }

    if (m_passwordField)
        m_passwordField->focus();
}
