{ pkgs ? import <nixpkgs> {},
  stdenv ? pkgs.clangStdenv,
}:

let
  customPython = pkgs.python38.buildEnv.override {
  };
in

pkgs.mkShell {
  buildInputs = [ customPython pkgs.clang ];
}
