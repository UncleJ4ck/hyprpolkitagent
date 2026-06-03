{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprutils,
  hyprtoolkit,
  hyprgraphics,
  hyprlang,
  aquamarine,
  pixman,
  libdrm,
  sdbus-cpp_2,
  polkit,
  systemd,
  cairo,
  pango,
  libxkbcommon,
  version ? "0",
}:
let
  inherit (lib.sources) cleanSource cleanSourceWith;
  inherit (lib.strings) hasSuffix;
in
stdenv.mkDerivation {
  pname = "hyprpolkitagent";
  inherit version;

  src = cleanSourceWith {
    filter =
      name: _type:
      let
        baseName = baseNameOf (toString name);
      in
      !(hasSuffix ".nix" baseName);
    src = cleanSource ../.;
  };

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    hyprutils
    hyprtoolkit
    hyprgraphics
    hyprlang
    aquamarine
    pixman
    libdrm
    sdbus-cpp_2
    polkit
    systemd
    cairo
    pango
    libxkbcommon
  ];

  meta = {
    description = "A polkit authentication agent built with hyprtoolkit";
    homepage = "https://github.com/hyprwm/hyprpolkitagent";
    license = lib.licenses.bsd3;
    maintainers = [ lib.maintainers.fufexan ];
    mainProgram = "hyprpolkitagent";
    platforms = lib.platforms.linux;
  };
}
