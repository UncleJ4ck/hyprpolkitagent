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
    if (!m_window)
        return;
    m_window->open();
    if (m_passwordField)
        m_passwordField->focus();
}

void CDialog::close() {
    if (m_window) {
        m_window->close();
        m_window.reset();
    }
}

void CDialog::setPrompt(const std::string& text, bool echo) {
    std::string newPrompt = text;
    while (!newPrompt.empty() && (newPrompt.back() == ' ' || newPrompt.back() == '\t'))
        newPrompt.pop_back();
    if (!newPrompt.empty() && newPrompt.back() == ':')
        newPrompt.pop_back();
    while (!newPrompt.empty() && (newPrompt.back() == ' ' || newPrompt.back() == '\t'))
        newPrompt.pop_back();
    if (newPrompt.empty())
        newPrompt = "Password";

    // initial prompt arrives after build() and would clobber the first keystroke
    const bool clean = m_currentPassword.empty() && !m_passwordVisible
                       && newPrompt == m_promptText && echo == m_promptEcho;

    m_promptText = newPrompt;
    m_promptEcho = echo;

    if (clean) {
        if (m_passwordField)
            m_passwordField->focus();
        return;
    }

    if (!m_passwordRow || !m_passwordField || !m_revealButton)
        return;

    m_currentPassword.clear();
    m_passwordVisible = false;
    m_passwordRow->removeChild(m_passwordField);
    m_passwordRow->removeChild(m_revealButton);
    buildPasswordField();
    m_passwordRow->addChild(m_passwordField);
    m_passwordRow->addChild(m_revealButton);
    m_revealButton->rebuild()->label(std::string{"Show"})->commence();
    if (m_authButton && m_authEnabled) {
        m_authEnabled = false;
        m_authButton->rebuild()->label(std::string{"Authenticate"})->noBorder(true)->commence();
    }
    m_passwordField->focus();
}

void CDialog::buildPasswordField() {
    const auto& cfg = g_pConfigManager->get();
    m_passwordField = CTextboxBuilder::begin()
                          ->placeholder(std::string{m_promptText})
                          ->password(!m_passwordVisible)
                          ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE,
                                  {(double)cfg.passwordFieldWidth, 34.0}})
                          ->defaultText(std::string{m_currentPassword})
                          ->onTextEdited([this](CSharedPointer<CTextboxElement>, const std::string& s) {
                              m_currentPassword = s;
                              const bool nowEnabled = !s.empty();
                              if (nowEnabled != m_authEnabled && m_authButton) {
                                  m_authEnabled = nowEnabled;
                                  m_authButton->rebuild()->label(std::string{"Authenticate"})->noBorder(!nowEnabled)->commence();
                              }
                              if (!s.empty() && m_errShown)
                                  setError("");
                          })
                          ->commence();
}

void CDialog::showStatus(CSharedPointer<IElement>& wrap, bool& shown, bool show) {
    if (!m_statusContainer || !wrap)
        return;
    if (show && !shown) {
        m_statusContainer->addChild(wrap);
        shown = true;
    } else if (!show && shown) {
        m_statusContainer->removeChild(wrap);
        shown = false;
    }
}

void CDialog::setInfo(const std::string& text) {
    if (m_infoLabel)
        m_infoLabel->rebuild()->text(std::string{text})->commence();
    showStatus(m_infoWrap, m_infoShown, !text.empty());
}

void CDialog::setError(const std::string& text) {
    if (m_errorLabel)
        m_errorLabel->rebuild()->text(std::string{text})->commence();
    showStatus(m_errWrap, m_errShown, !text.empty());
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

static std::vector<uint8_t> loadIconData(const std::string& iconName) {
    namespace fs = std::filesystem;

    std::string              theme = gtkIconTheme();
    std::vector<std::string> themes;
    if (!theme.empty())
        themes.push_back(theme);
    for (const char* t : {"hicolor", "AdwaitaLegacy", "Papirus-Dark"})
        themes.push_back(t);

    // user theme wins over fallback themes
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

    // librsvg doesn't inherit currentColor from the parent, resolve it inline
    auto cssColor = [&](const std::string& cls) -> std::string {
        auto p = svg.find(cls);
        if (p == std::string::npos)
            return {};
        auto end = svg.find('}', p);
        if (end == std::string::npos)
            return {};
        auto cp = svg.find("color:", p);
        if (cp == std::string::npos || cp >= end)
            return {};
        cp += 6;
        while (cp < svg.size() && svg[cp] == ' ')
            cp++;
        auto ce = svg.find_first_of(";} \t\n\r", cp);
        if (ce == std::string::npos || ce > end)
            return {};
        return svg.substr(cp, ce - cp);
    };
    const std::string textColor = cssColor(".ColorScheme-Text");
    if (!textColor.empty())
        for (size_t pos = 0; (pos = svg.find("currentColor", pos)) != std::string::npos;)
            svg.replace(pos, 12, textColor), pos += textColor.size();

    // librsvg paints paths via fill:, not color:
    for (size_t pos = 0; (pos = svg.find("color:", pos)) != std::string::npos;)
        svg.replace(pos, 6, "fill:"), pos += 5;

    return {svg.begin(), svg.end()};
}

void CDialog::build() {
    const auto& cfg = g_pConfigManager->get();

    m_window = CWindowBuilder::begin()
                   ->preferredSize({cfg.windowWidth, cfg.windowHeight})
                   ->minSize({540, 440})
                   ->maxSize({700, 700})
                   ->appTitle("Authentication Required")
                   ->appClass("hyprpolkitagent")
                   ->commence();

    m_closeListener = m_window->m_events.closeRequest.listen([] { g_pAgent->cancel(); });

    m_keyListener = m_window->m_events.keyboardKey.listen([this](const Input::SKeyboardKeyEvent& ev) {
        const bool caps = ev.modMask & Input::HT_MODIFIER_CAPS;
        if (caps != m_capsLockOn) {
            m_capsLockOn = caps;
            if (m_capsLockLabel && caps)
                m_capsLockLabel->rebuild()->text(std::string{"Caps Lock is on"})->commence();
            showStatus(m_capsWrap, m_capsShown, caps);
        }

        if (!ev.down || ev.repeat)
            return;
        if (ev.xkbKeysym == XKB_KEY_Return || ev.xkbKeysym == XKB_KEY_KP_Enter) {
            if (!m_currentPassword.empty())
                g_pAgent->submitPassword(m_currentPassword);
        } else if (ev.xkbKeysym == XKB_KEY_Escape) {
            g_pAgent->cancel();
        }
    });

    m_window->m_rootElement->addChild(
        CRectangleBuilder::begin()
            ->color([] { return g_pAgent->backend()->getPalette()->m_colors.background; })
            ->commence());

    auto outer = CColumnLayoutBuilder::begin()
                     ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {0.88F, 0.0F}})
                     ->gap(8)
                     ->commence();
    outer->setMargin(16);
    outer->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
    outer->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
    m_window->m_rootElement->addChild(outer);

    if (cfg.showIcon) {
        // Papirus' dialog-password is a key, swap for a padlock
        std::string iconName = m_req.iconName.empty() ? "object-locked" : m_req.iconName;
        if (iconName == "dialog-password")
            iconName = "object-locked";

        auto bytes = loadIconData(iconName);
        if (bytes.empty())
            bytes = loadIconData("changes-prevent");
        if (bytes.empty())
            bytes = loadIconData("system-lock-screen");
        if (!bytes.empty()) {
            const int bgSize = cfg.iconSize + 16;

            // inject the bg into the svg so the icon stays a single image element
            if (bytes[0] == '<') {
                const std::string rct = "<rect width=\"100%\" height=\"100%\" rx=\"4\" ry=\"4\" fill=\"#33ccff\"/>";
                std::string svg(bytes.begin(), bytes.end());
                const auto svgEnd = svg.find('>');
                if (svgEnd != std::string::npos)
                    svg.insert(svgEnd + 1, rct);
                bytes.assign(svg.begin(), svg.end());
            }

            auto wrap = CRowLayoutBuilder::begin()->commence();
            wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
            wrap->addChild(CImageBuilder::begin()
                               ->data(std::move(bytes))
                               ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE,
                                       {(double)bgSize, (double)bgSize}})
                               ->commence());
            outer->addChild(wrap);
        }
    }

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

    {
        std::string msg = m_req.message;
        if (!m_req.command.empty())
            msg = "Authentication is needed to run";

        constexpr size_t MAX_MSG = 120;
        if (msg.size() > MAX_MSG)
            msg = msg.substr(0, MAX_MSG - 3) + "...";

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

    if (!m_req.command.empty()) {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        std::string cmd = m_req.command;
        constexpr size_t MAX_CMD = 55;
        if (cmd.size() > MAX_CMD)
            cmd = cmd.substr(0, MAX_CMD - 3) + "...";

        auto box = CRectangleBuilder::begin()
                       ->color([] { return g_pAgent->backend()->getPalette()->m_colors.base; })
                       ->borderColor([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.3); })
                       ->borderThickness(2)
                       ->rounding(6)
                       ->size(CDynamicSize{CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {}})
                       ->commence();

        auto inner = CRowLayoutBuilder::begin()->commence();
        inner->setMargin(12);
        inner->addChild(CTextBuilder::begin()
                            ->text(std::move(cmd))
                            ->fontFamily(std::string{"monospace"})
                            ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                            ->commence());
        box->addChild(inner);
        wrap->addChild(box);
        outer->addChild(wrap);
    }

    {
        auto pwRow = CRowLayoutBuilder::begin()->gap(4)->commence();
        pwRow->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        pwRow->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        m_passwordRow = pwRow;

        buildPasswordField();
        pwRow->addChild(m_passwordField);

        m_revealButton = CButtonBuilder::begin()
                             ->label(std::string{"Show"})
                             ->noBorder(true)
                             ->onMainClick([this](CSharedPointer<CButtonElement>) {
                                 // rebuild() doesn't flip password() on a live textbox, swap a fresh one in
                                 m_passwordVisible = !m_passwordVisible;
                                 m_passwordRow->removeChild(m_passwordField);
                                 m_passwordRow->removeChild(m_revealButton);
                                 buildPasswordField();
                                 m_passwordRow->addChild(m_passwordField);
                                 m_passwordRow->addChild(m_revealButton);
                                 m_revealButton->rebuild()->label(std::string{m_passwordVisible ? "Hide" : "Show"})->commence();
                                 m_passwordField->focus();
                             })
                             ->commence();
        pwRow->addChild(m_revealButton);
        outer->addChild(pwRow);
    }

    // wraps are only attached when their label has text, so empty labels reserve no space
    {
        auto statusCol = CColumnLayoutBuilder::begin()->gap(4)->commence();
        statusCol->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        statusCol->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        outer->addChild(statusCol);
        m_statusContainer = statusCol;

        auto makeWrap = [](CSharedPointer<CTextElement> label) {
            auto w = CRowLayoutBuilder::begin()->commence();
            w->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            w->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
            w->addChild(label);
            return w;
        };

        m_capsLockLabel = CTextBuilder::begin()
                              ->text(std::string{""})
                              ->fontSize({CFontSize::HT_FONT_SMALL})
                              ->color([] { return CHyprColor{1.0F, 0.75F, 0.2F, 1.F}; })
                              ->commence();
        m_errorLabel = CTextBuilder::begin()
                           ->text(std::string{""})
                           ->color([] { return CHyprColor{0.9F, 0.4F, 0.4F, 1.F}; })
                           ->commence();
        m_infoLabel = CTextBuilder::begin()
                          ->text(std::string{""})
                          ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.4); })
                          ->commence();

        m_capsWrap = makeWrap(m_capsLockLabel);
        m_errWrap  = makeWrap(m_errorLabel);
        m_infoWrap = makeWrap(m_infoLabel);
    }

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
                           ->noBorder(true)
                           ->onMainClick([this](CSharedPointer<CButtonElement>) {
                               if (!m_currentPassword.empty())
                                   g_pAgent->submitPassword(m_currentPassword);
                           })
                           ->commence();
        btnRow->addChild(m_authButton);
        outer->addChild(btnRow);
    }

    std::vector<std::pair<std::string, std::string>> fields;
    if (!m_req.actionId.empty())  fields.emplace_back("Action",  m_req.actionId);
    if (!m_req.vendor.empty())    fields.emplace_back("Vendor",  m_req.vendor);
    if (!m_req.vendorUrl.empty()) fields.emplace_back("URL",     m_req.vendorUrl);
    for (const auto& [k, v] : m_req.details)
        fields.emplace_back(k, v);

    if (cfg.showDetails && !fields.empty()) {
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

        auto detailsBoxWrap = CRowLayoutBuilder::begin()->commence();
        detailsBoxWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        detailsBoxWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        auto detailsBox = CRectangleBuilder::begin()
                              ->color([] { return g_pAgent->backend()->getPalette()->m_colors.base; })
                              ->borderColor([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.3); })
                              ->borderThickness(2)
                              ->rounding(6)
                              ->size(CDynamicSize{CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {}})
                              ->commence();

        auto detailsCol = CColumnLayoutBuilder::begin()->gap(4)->commence();
        detailsCol->setMargin(12);

        for (const auto& [key, val] : fields) {
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

        detailsBox->addChild(detailsCol);
        detailsBoxWrap->addChild(detailsBox);
        m_detailsContainer = detailsBoxWrap;
        m_detailsParent    = outer;
    }

    if (m_passwordField)
        m_passwordField->focus();
}
