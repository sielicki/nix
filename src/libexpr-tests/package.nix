{
  lib,
  buildPackages,
  stdenv,
  mkMesonExecutable,

  nix-expr,
  nix-expr-c,
  nix-expr-test-support,

  rapidcheck,
  gtest,
  gbenchmark,
  runCommand,

  # Configuration Options

  version,
  resolvePath,
  withBenchmarks ? false,
}:

let
  inherit (lib) fileset;
in

mkMesonExecutable (finalAttrs: {
  pname = "nix-expr-tests";
  inherit version;

  workDir = ./.;
  fileset = fileset.unions [
    ../../nix-meson-build-support
    ./nix-meson-build-support
    ../../.version
    ./.version
    ./meson.build
    ./meson.options
    # Internal allocator tests also build from this isolated component source.
    ../libexpr/parser-allocators.hh
    ../libexpr/lexy/values/storage.hh
    (fileset.fileFilter (file: file.hasExt "cc") ./.)
    (fileset.fileFilter (file: file.hasExt "hh") ./.)
  ];

  buildInputs = [
    nix-expr
    nix-expr-c
    nix-expr-test-support
    rapidcheck
    gtest
  ]
  ++ lib.optionals withBenchmarks [
    gbenchmark
  ];

  mesonFlags = [
    (lib.mesonBool "benchmarks" withBenchmarks)
  ];

  passthru = {
    tests = {
      run =
        runCommand "${finalAttrs.pname}-run"
          {
            meta.broken = !stdenv.hostPlatform.emulatorAvailable buildPackages;
          }
          (
            lib.optionalString stdenv.hostPlatform.isWindows ''
              export HOME="$PWD/home-dir"
              mkdir -p "$HOME"
            ''
            + ''
              export _NIX_TEST_UNIT_DATA=${resolvePath ./data}
              ${stdenv.hostPlatform.emulator buildPackages} ${lib.getExe finalAttrs.finalPackage}
            ''
            + lib.optionalString nix-expr.enableLexy ''
              # Require both parsers: other builds instantiate only Bison tests.
              echo '1 + 2' > parser-smoke.nix
              # The raw checker needs the same stack allowance as the CLI.
              {
                printf '%8192s' "" | tr ' ' '('
                printf '42'
                printf '%8192s\n' "" | tr ' ' ')'
              } > parser-deep.nix
              ${stdenv.hostPlatform.emulator buildPackages} ${lib.getExe' finalAttrs.finalPackage "nix-parser-check"} parser-smoke.nix parser-deep.nix
            ''
            + lib.optionalString withBenchmarks ''
              ${stdenv.hostPlatform.emulator buildPackages} ${lib.getExe' finalAttrs.finalPackage "nix-expr-benchmarks"}
            ''
            + ''
              touch $out
            ''
          );
    };
  };

  meta = {
    platforms = lib.platforms.unix ++ lib.platforms.windows;
    mainProgram = finalAttrs.pname + stdenv.hostPlatform.extensions.executable;
  };

})
