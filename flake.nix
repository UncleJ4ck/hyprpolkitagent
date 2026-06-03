{
  description = "A polkit authentication agent built with hyprtoolkit";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprlang = {
      url = "github:hyprwm/hyprlang";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprgraphics = {
      url = "github:hyprwm/hyprgraphics";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    aquamarine = {
      url = "github:hyprwm/aquamarine";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprtoolkit = {
      url = "github:hyprwm/hyprtoolkit";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprlang.follows = "hyprlang";
      inputs.hyprgraphics.follows = "hyprgraphics";
      inputs.aquamarine.follows = "aquamarine";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      systems,
      ...
    }@inputs:
    let
      inherit (nixpkgs) lib;
      eachSystem = lib.genAttrs (import systems);
      pkgsFor = eachSystem (
        system:
        import nixpkgs {
          localSystem = system;
          overlays = [ self.overlays.hyprpolkitagent-with-deps ];
        }
      );
    in
    {
      formatter = eachSystem (system: pkgsFor.${system}.nixfmt-tree);

      overlays = import ./nix/overlays.nix { inherit inputs self lib; };

      packages = eachSystem (system: {
        default = self.packages.${system}.hyprpolkitagent;
        inherit (pkgsFor.${system}) hyprpolkitagent;
      });

      devShells = eachSystem (system: {
        default = import ./nix/shell.nix {
          pkgs = pkgsFor.${system};
          inherit (pkgsFor.${system}) hyprpolkitagent;
        };
      });
    };
}
