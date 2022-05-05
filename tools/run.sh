#!/bin/bash

set -e


# compile linux and the kernel module
make kmod -j$(nproc)


WORKSPACE=$(pwd)

# git doesn't like empty files...
mkdir -p root/{bin,sbin,etc,proc,sys,usr/{bin,sbin}}

pushd root
	pwd
	find . -print0 \
		| cpio --quiet --null -ov --format=newc \
		| gzip --quiet -9 > $WORKSPACE/build/initramfs.cpio.gz
popd


QEMU_FLAGS="-smp 1 -m 8G "
QEMU_FLAGS+="-global kvm-pit.lost_tick_policy=discard "
QEMU_FLAGS+="-kernel $WORKSPACE/build/arch/x86_64/boot/bzImage "
QEMU_FLAGS+="-initrd $WORKSPACE/build/initramfs.cpio.gz "
QEMU_FLAGS+="-nographic -append \"console=ttyS0\" "
QEMU_FLAGS+="-netdev user,id=u1  -device e1000,netdev=u1 "
QEMU_FLAGS+="-rtc base=localtime "

echo $QEMU_FLAGS
qemu-system-x86_64 $QEMU_FLAGS $@
