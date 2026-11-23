{
  description = "Gamma OS: середовище розробки (clang, lld, QEMU, OVMF)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          clang
          lld
          gnumake
          qemu
          gdb
          git
        ];

        # Захисні прапорці обгортки компілятора Nix (stack protector, PIE та ін.)
        # заважають збиранню freestanding-ядра.
        hardeningDisable = [ "all" ];

        # Шлях до прошивки UEFI для QEMU; Makefile зчитує його зі змінної оточення.
        OVMF_FD = "${pkgs.OVMF.fd}/FV/OVMF.fd";
      };
    };
}
