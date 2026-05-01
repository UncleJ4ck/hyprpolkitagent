#include "core/Agent.hpp"

int main(int, char*[]) {
    g_pAgent = std::make_unique<CAgent>();
    return g_pAgent->start() ? 0 : 1;
}
