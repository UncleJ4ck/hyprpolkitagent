#include "Config.hpp"

#include <hyprlang.hpp>
#include <cstdlib>
#include <filesystem>
#include <print>

static std::string configPath() {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string base = xdg ? xdg : (std::string{getenv("HOME")} + "/.config");
    return base + "/hyprpolkitagent/hyprpolkitagent.conf";
}

CConfigManager::CConfigManager() : m_path(configPath()) {}

void CConfigManager::load() {
    namespace fs = std::filesystem;

    if (!fs::exists(m_path)) {
        // Write a default config so users know what can be changed.
        fs::create_directories(fs::path{m_path}.parent_path());
        if (FILE* f = fopen(m_path.c_str(), "w")) {
            fputs("general {\n"
                  "    icon_size           = 48\n"
                  "    border_size         = 1\n"
                  "    rounding            = 8\n"
                  "    password_field_width = 340\n"
                  "    window_width        = 520\n"
                  "    window_height       = 400\n"
                  "    show_icon           = true\n"
                  "    show_details        = true\n"
                  "}\n",
                  f);
            fclose(f);
        }
    }

    // Named variable so member initializers run (brace {} would zero __internal_struct_end).
    Hyprlang::SConfigOptions opts;
    opts.allowMissingConfig = true;
    Hyprlang::CConfig cfg{m_path.c_str(), opts};

    cfg.addConfigValue("general:icon_size",            Hyprlang::INT{48});
    cfg.addConfigValue("general:border_size",          Hyprlang::INT{1});
    cfg.addConfigValue("general:rounding",             Hyprlang::INT{8});
    cfg.addConfigValue("general:password_field_width", Hyprlang::INT{340});
    cfg.addConfigValue("general:window_width",         Hyprlang::INT{520});
    cfg.addConfigValue("general:window_height",        Hyprlang::INT{400});
    cfg.addConfigValue("general:show_icon",            Hyprlang::INT{1});
    cfg.addConfigValue("general:show_details",         Hyprlang::INT{1});

    cfg.commence();
    const auto res = cfg.parse();
    if (res.error)
        std::print(stderr, "hyprpolkitagent: config parse error: {}\n", res.getError());

    auto iv = [&](const char* k) { return std::any_cast<Hyprlang::INT>(cfg.getConfigValue(k)); };

    m_cfg.iconSize           = iv("general:icon_size");
    m_cfg.borderSize         = iv("general:border_size");
    m_cfg.rounding           = iv("general:rounding");
    m_cfg.passwordFieldWidth = iv("general:password_field_width");
    m_cfg.windowWidth        = iv("general:window_width");
    m_cfg.windowHeight       = iv("general:window_height");
    m_cfg.showIcon           = iv("general:show_icon") != 0;
    m_cfg.showDetails        = iv("general:show_details") != 0;
}
