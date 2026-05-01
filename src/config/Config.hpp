#pragma once

#include <string>

struct SHPAConfig {
    // general {}
    int   iconSize          = 48;
    int   borderSize        = 1;
    int   rounding          = 8;
    int   passwordFieldWidth = 340;
    int   windowWidth       = 520;
    int   windowHeight      = 400;
    bool  showIcon          = true;
    bool  showDetails       = true;
};

class CConfigManager {
  public:
    CConfigManager();

    // Loads/reloads from disk. Safe to call before entering the event loop.
    void load();

    const SHPAConfig& get() const { return m_cfg; }

  private:
    SHPAConfig  m_cfg;
    std::string m_path;
};

inline CConfigManager* g_pConfigManager = nullptr;
