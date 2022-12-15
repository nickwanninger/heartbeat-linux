{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  cc ? pkgs.gcc,
  perlPackages ? pkgs.perl534Packages,
  mkWrapper ? pkgs.makeWrapper
}:

stdenv.mkDerivation rec {
  name = "heartbeat-linux";

  src = ./.;

  buildInputs = [ cc mkWrapper ];

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
    mkdir -p $out/src
    cp src/entry.S $out/src
  '';

  postFixup = ''
    wrapProgram $out/rf_compiler/transform.pl \
      --prefix PERL5LIB : "${with perlPackages; makePerlPath [  ]}"
  '';

  meta = {
    description = "Library support for the heartbeat-interrupt kernel module.";
  };
}
