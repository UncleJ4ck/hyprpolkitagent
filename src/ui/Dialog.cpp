#include "Dialog.hpp"

#include "../core/Agent.hpp"
#include "../config/Config.hpp"

#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Combobox.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <hyprtoolkit/palette/Color.hpp>
#include <hyprtoolkit/core/Input.hpp>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <filesystem>
#include <fstream>
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

static std::string gtkIconTheme() {
    const char* xdg  = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    if (!xdg && !home)
        return {};
    std::string cfg = xdg ? std::string{xdg} : std::string{home} + "/.config";
    std::ifstream f(cfg + "/gtk-3.0/settings.ini");
    for (std::string line; std::getline(f, line);) {
        auto eq = line.find('=');
        if (line.find("gtk-icon-theme-name") != std::string::npos && eq != std::string::npos)
            return line.substr(eq + 1);
    }
    return {};
}

// Load icon bytes: searches per-theme (PNG then SVG) before falling back to other themes.
// Plasma SVGs use CSS `color:` for currentColor fills; resolves them explicitly so librsvg
// renders the correct colours regardless of currentColor cascade quirks.
static std::vector<uint8_t> loadIconData(const std::string& iconName) {
    namespace fs = std::filesystem;

    std::string              theme = gtkIconTheme();
    std::vector<std::string> themes;
    if (!theme.empty())
        themes.push_back(theme);
    for (const char* t : {"hicolor", "AdwaitaLegacy", "Papirus-Dark"})
        themes.push_back(t);

    // Per-theme search: PNG then SVG, so the user's theme SVG wins over a
    // generic PNG from a fallback theme (fixes cross-theme key vs padlock confusion).
    auto findPath = [&]() -> std::string {
        for (const auto& t : themes)
            for (const char* sz : {"48x48", "64x64", "32x32", "scalable", "24x24", "22x22"})
                for (const char* cat : {"actions", "status", "apps", "legacy", "categories"})
                    for (const char* ext : {".png", ".svg"}) {
                        std::string p = "/usr/share/icons/" + t + "/" + sz + "/" + cat + "/" + iconName + ext;
                        if (fs::exists(p))
                            return p;
                    }
        return {};
    };

    std::string path = findPath();
    if (path.empty())
        return {};

    if (!path.ends_with(".svg")) {
        std::ifstream f(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(f), {}};
    }

    std::ifstream f(path);
    std::string   svg((std::istreambuf_iterator<char>(f)), {});

    // Extract CSS palette colour for a given class. Works for both one-liner and
    // multiline block selectors (finds `color:` within the next `}` after the class).
    auto cssColorVal = [&](const std::string& cls) -> std::string {
        auto clsPos = svg.find(cls);
        if (clsPos == std::string::npos)
            return {};
        auto blockEnd = svg.find('}', clsPos);
        if (blockEnd == std::string::npos)
            return {};
        auto colorPos = svg.find("color:", clsPos);
        if (colorPos == std::string::npos || colorPos >= blockEnd)
            return {};
        colorPos += 6;
        while (colorPos < svg.size() && svg[colorPos] == ' ')
            colorPos++;
        auto colorEnd = svg.find_first_of(";} \t\n\r", colorPos);
        if (colorEnd == std::string::npos || colorEnd > blockEnd)
            return {};
        return svg.substr(colorPos, colorEnd - colorPos);
    };

    // Resolve currentColor to the explicit text colour so librsvg renders it
    // correctly when displayed on a coloured background.
    std::string textColor = cssColorVal(".ColorScheme-Text");
    if (!textColor.empty()) {
        for (size_t pos = 0; (pos = svg.find("currentColor", pos)) != std::string::npos;)
            svg.replace(pos, 12, textColor), pos += textColor.size();
    }

    // Fix remaining CSS `color:` → `fill:` so librsvg applies palette rules.
    for (size_t pos = 0; (pos = svg.find("color:", pos)) != std::string::npos;)
        svg.replace(pos, 6, "fill:"), pos += 5;

    return {svg.begin(), svg.end()};
}

void CDialog::build() {
    const auto& cfg = g_pConfigManager->get();

    m_window = CWindowBuilder::begin()
                   ->preferredSize({cfg.windowWidth, cfg.windowHeight})
                   ->minSize({480, 300})
                   ->maxSize({700, 700})
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

    // Outer column — 88 % width, centered, 10 px gap between children
    auto outer = CColumnLayoutBuilder::begin()
                     ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {0.88F, 1.F}})
                     ->gap(10)
                     ->commence();
    outer->setMargin(16);
    outer->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
    outer->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
    m_window->m_rootElement->addChild(outer);

    // ── Icon ─────────────────────────────────────────────────────────────────
    if (cfg.showIcon) {
        // dialog-password is a key in Papirus; prefer a padlock for auth UI.
        std::string iconName = m_req.iconName.empty() ? "object-locked" : m_req.iconName;
        if (iconName == "dialog-password")
            iconName = "object-locked";

        auto bytes = loadIconData(iconName);
        if (bytes.empty())
            bytes = loadIconData("changes-prevent");
        if (bytes.empty())
            bytes = loadIconData("system-lock-screen");
        if (!bytes.empty()) {
            auto wrap = CRowLayoutBuilder::begin()->commence();
            wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

            // Accent-coloured rounded square behind the icon (matches reference design).
            const int bgSize = cfg.iconSize + 16;
            auto      bg     = CRectangleBuilder::begin()
                              ->color([] { return g_pAgent->backend()->getPalette()->m_colors.accent; })
                              ->rounding(12)
                              ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE,
                                      {(double)bgSize, (double)bgSize}})
                              ->commence();

            auto imgEl = CImageBuilder::begin()
                             ->data(std::move(bytes))
                             ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE,
                                     {(double)cfg.iconSize, (double)cfg.iconSize}})
                             ->commence();
            imgEl->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            imgEl->setPositionFlag(IElement::HT_POSITION_FLAG_CENTER, true);
            bg->addChild(imgEl);
            wrap->addChild(bg);
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
                           ->clampSize({(double)cfg.passwordFieldWidth, -1.0})
                           ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                           ->commence());
        outer->addChild(wrap);
    }

    // ── Command box ───────────────────────────────────────────────────────────
    if (!m_req.command.empty()) {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        // Truncate long commands so they don't overflow the window
        std::string cmd = m_req.command;
        constexpr size_t MAX_CMD = 55;
        if (cmd.size() > MAX_CMD)
            cmd = cmd.substr(0, MAX_CMD - 3) + "...";

        auto box = CRectangleBuilder::begin()
                       ->color([] { return g_pAgent->backend()->getPalette()->m_colors.base; })
                       ->borderColor([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.55); })
                       ->borderThickness(g_pConfigManager->get().borderSize)
                       ->rounding(g_pConfigManager->get().rounding)
                       ->size(CDynamicSize{CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_AUTO, Vector2D{(double)cfg.passwordFieldWidth, 0.0}})
                       ->commence();
        box->setMargin(6);
        box->addChild(CTextBuilder::begin()
                          ->text(std::move(cmd))
                          ->fontFamily(std::string{"monospace"})
                          ->align(HT_FONT_ALIGN_CENTER)
                          ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                          ->commence());
        wrap->addChild(box);
        outer->addChild(wrap);
    }

    // ── Password row ──────────────────────────────────────────────────────────
    {
        auto pwRow = CRowLayoutBuilder::begin()->gap(4)->commence();
        pwRow->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        pwRow->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        m_passwordField = CTextboxBuilder::begin()
                              ->placeholder(std::string{m_promptText})
                              ->password(true)
                              ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE,
                                      {(double)cfg.passwordFieldWidth, 34.0}})
                              ->onTextEdited([this](CSharedPointer<CTextboxElement>, const std::string& s) {
                                  if (s == "\x01")
                                      return; // sentinel — not real user input
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
                                 // Force updateLabel via sentinel then restore tracked password.
                                 m_passwordField->rebuild()->defaultText(std::string{"\x01"})->password(!m_passwordVisible)->commence();
                                 m_passwordField->rebuild()->defaultText(std::string{m_currentPassword})->password(!m_passwordVisible)->commence();
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
        auto btnRow = CRowLayoutBuilder::begin()->gap(16)->commence();
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

    // ── Show details (only if there are details and user hasn't disabled it) ──
    if (cfg.showDetails && !m_req.details.empty()) {
        auto detailsWrap = CRowLayoutBuilder::begin()->commence();
        detailsWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        detailsWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        m_detailsButton = CButtonBuilder::begin()
                              ->label(std::string{"Show details"})
                              ->noBorder(true)
                              ->noBg(true)
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
