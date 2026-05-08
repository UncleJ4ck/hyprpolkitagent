{
  pkgs ? import <nixpkgs> { },
  hyprpolkitagent ? pkgs.callPackage ./default.nix { },
  ...
}:
pkgs.mkShell {
  inputsFrom = [ hyprpolkitagent ];
  nativeBuildInputs = [ pkgs.clang-tools ];

  shellHook = ''
    CMAKE_EXPORT_COMPILE_COMMANDS=1 cmake -S . -B ./build
    ln -s build/compile_commands.json .
  '';
}
