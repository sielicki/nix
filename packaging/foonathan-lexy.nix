{
  lib,
  stdenvNoCC,
  fetchFromGitHub,
}:

# This is the C++ parsing library, not nixpkgs' unrelated Python package `lexy`.
stdenvNoCC.mkDerivation {
  pname = "foonathan-lexy";
  version = "2025.05.0-unstable-2025-10-18";

  src = fetchFromGitHub {
    owner = "foonathan";
    repo = "lexy";
    rev = "c1358c4117752393e9ce27d6c885d9146846f2f9";
    hash = "sha256-oCzLuCgV04jhoD5HaCx71k3+3qGvtWOcdfbZ5pJ6Ikk=";
  };

  dontBuild = true;
  installPhase = ''
    runHook preInstall
    mkdir -p $out/include $out/lib/pkgconfig
    cp -r include/lexy include/lexy_ext $out/include/
    install -Dm644 LICENSE $out/share/licenses/foonathan-lexy/LICENSE
    cat > $out/lib/pkgconfig/foonathan-lexy.pc <<EOF
    Name: foonathan-lexy
    Description: Header-only C++ parsing DSL
    Version: 2025.05.0
    Cflags: -I$out/include
    EOF
    runHook postInstall
  '';

  meta = {
    description = "C++ parsing DSL by Jonathan Müller";
    homepage = "https://github.com/foonathan/lexy";
    license = lib.licenses.boost;
    platforms = lib.platforms.all;
  };
}
