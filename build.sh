rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake
cmake .
make -j$(nproc)
systemctl --user restart hyprpolkitagent
