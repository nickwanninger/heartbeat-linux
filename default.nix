{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  cc ? pkgs.gcc
}:

stdenv.mkDerivation rec {
  name = "heartbeat-linux";

  src = ./.;

  buildInputs = [ cc ];

  buildPhase = ''
    make libhb.so
  '';

  installPhase = ''
    mkdir -p $out
    cp libhb.so $out
    mkdir -p $out/user
    cp user/*.h user/entry.S $out/user
  '';

  meta = {
    description = "Library support for the heartbeat-interrupt kernel module.";
  };
}
