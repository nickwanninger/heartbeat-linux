{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  kernel1 ? pkgs.linuxKernel.kernels.linux_5_18,
#  kernel1 ? pkgs.linuxKernel.kernels.linux_zen,
  nukeReferences ? pkgs.nukeReferences
}:

stdenv.mkDerivation rec {
  name = "heartbeat-timer-${version}-${kernel.version}";
  version = "1.0";

  src = ./.;

  buildInputs = [ nukeReferences ];
  hardeningDisable = [ "pic" ];
  kernel = kernel1.dev;
  kernelVersion = kernel.modDirVersion;

  buildPhase = ''
    # missing some "touch" commands, make sure they exist for build.
    touch .bmd-support.o.cmd
    make -C $kernel/lib/modules/$kernelVersion/build modules "M=$(pwd -P)"
  '';

  installPhase = ''
    mkdir -p $out/lib/modules/$kernelVersion/misc
    for x in $(find . -name '*.ko'); do
      nuke-refs $x
      cp $x $out/lib/modules/$kernelVersion/misc/
    done
  '';

  meta.platforms = [ "x86_64-linux" ];
}
