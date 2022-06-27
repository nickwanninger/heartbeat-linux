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
    mkdir -p $out/include
    cp include/*.h $out/include
    mkdir $out/rf_compiler
    cp rf_compiler/transform.py $out/rf_compiler
    cp rf_compiler/transform.pl $out/rf_compiler
    cp src/entry.S $out/rf_compiler
  '';

  meta = {
    description = "Library support for the heartbeat-interrupt kernel module.";
  };
}
