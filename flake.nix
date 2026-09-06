{
  description = "Embedded development environment with Python, Node.js, and ARM GCC";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            # Required if any underlying package requires an unfree license
            allowUnfree = true;
          };
        };

        # Fetch a complete Pico SDK including all git submodules (cyw43-driver, lwip, etc.)
        pico-sdk-full = pkgs.fetchFromGitHub {
          owner = "raspberrypi";
          repo = "pico-sdk";
          # The version tag for that this project expects
          rev = "2.3.0";
          sha256 = "sha256-8ubZW6yQnUTYxQqYI6hi7s3kFVQhe5EaxVvHmo93vgk=";
          fetchSubmodules = true;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Toolchain & Build Systems
            gcc-arm-embedded
            cmake
            ninja

            # Runtimes
            python3
            nodejs_26

						# For Raspberry Pi Pico development
						picotool
						openocd

            # Optional but recommended helpers for embedded work
            git
            gdb
          ];

          shellHook = ''
						# Force CMake to find the Nix-installed Pico SDK
            export PICO_SDK_PATH="${pico-sdk-full}"

            echo "========================================================"
            echo "🛡️  Welcome to the NixOS Embedded Development Shell  🛡️"
            echo "========================================================"
            echo "ARM GCC:  $(arm-none-eabi-gcc --version | head -n 1)"
            echo "CMake:    $(cmake --version | head -n 1)"
            echo "Ninja:    $(ninja --version | head -n 1)"
            echo "Python:   $(python3 --version)"
            echo "Node.js:  $(node --version)"
						echo "Pico SDK: $PICO_SDK_PATH"
            echo "========================================================"
          '';
        };
      });
}

