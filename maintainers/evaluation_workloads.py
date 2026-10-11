"""Larger Nixpkgs evaluations; force derivation paths without building outputs.

These deliberately exercise different dependency graphs: native packages,
Python dependency propagation, cross-compilation, and NixOS module composition.
The driver disables import-from-derivation and records each complete expression.
Keep the selections explicit so changes in benchmark scope are reviewable.
"""

import json


def nixpkgs_workloads(root, system):
    def nix_string(value):
        return json.dumps(str(value), ensure_ascii=False).replace("${", "\\${")

    root = f"(builtins.toPath {nix_string(root)})"
    system = nix_string(system)
    packages = f"let pkgs = import {root} {{ system = {system}; }}; in "

    def nixos(configuration):
        # A complete, evaluable host configuration, without host-dependent
        # hardware discovery. The device paths are configuration data only.
        return f"""(import ({root} + "/nixos") {{
  system = {system};
  configuration = {{ pkgs, ... }}: {{
    system.stateVersion = "26.05";
    boot.loader.grub.device = "/dev/vda";
    fileSystems."/" = {{ device = "/dev/disk/by-label/root"; fsType = "ext4"; }};
    {configuration}
  }};
}}).system.drvPath"""

    return {
        "package-set": packages + """builtins.concatStringsSep "\\n" (map (p: p.drvPath)
  (with pkgs; [
    git openssh curl wget nginx postgresql redis sqlite vim neovim emacs
    firefox chromium libreoffice gimp inkscape ffmpeg imagemagick python3
    go rustc cargo nodejs llvmPackages.clang cmake meson ninja pkg-config
    qemu podman
  ]))""",
        "python-environment": packages + """(pkgs.python3.withPackages (ps: with ps; [
  numpy pandas scipy matplotlib requests pytest jupyterlab networkx pillow
  sympy scikit-learn
])).drvPath""",
        "cross-packages": packages + """builtins.concatStringsSep "\\n" (map (p: p.drvPath)
  (with pkgs.pkgsCross.aarch64-multiplatform; [ hello bash openssl zlib curl sqlite ]))""",
        "nixos-server": nixos("""
    networking.hostName = "parser-bench-server";
    services.openssh.enable = true;
    services.nginx = {
      enable = true;
      virtualHosts."bench.invalid".locations."/".return = "200 'parser benchmark'";
    };
    services.postgresql.enable = true;
    services.redis.servers.bench.enable = true;
    services.prometheus.exporters.node.enable = true;
    environment.systemPackages = with pkgs; [ git curl htop ];
"""),
        "nixos-desktop": nixos("""
    networking.hostName = "parser-bench-desktop";
    networking.networkmanager.enable = true;
    services.xserver.enable = true;
    services.displayManager.gdm.enable = true;
    services.desktopManager.gnome.enable = true;
    services.pipewire.enable = true;
    users.users.bench = { isNormalUser = true; extraGroups = [ "wheel" ]; };
    environment.systemPackages = with pkgs; [ firefox libreoffice gimp git ];
"""),
    }
