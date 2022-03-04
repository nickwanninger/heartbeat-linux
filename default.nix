{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  cc ? pkgs.gcc
}:

stdenv.mkDerivation rec {
  name = "heartbeat-linux";

  src = ./.;

  buildInputs = [ cc ];

  buildPhase = ''
    make build/libhb.so
    make build/ex
  '';

  installPhase = ''
    mkdir -p $out
    cp build/libhb.so $out
    cp build/ex $out
  '';

  meta = {
    description = "Library support for the heartbeat-interrupt kernel module.";
  };
}
