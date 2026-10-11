---
synopsis: Optional lexy parser backend
---

Nix can now be built with an experimental [lexy parser](@docroot@/development/lexy.md)
alongside the existing Bison parser. Enable `libexpr:lexy` in a Meson build or
build the `nix-lexy` flake package, then select it with `parser = lexy` in
`nix.conf` or `--option parser lexy` on the command line. Bison remains the
default backend.

Both backends require the `pipe-operators` experimental feature for pipes and
support the legacy `f or` argument syntax with the same deprecation warnings.
Syntax diagnostics may differ between backends.
