#!/bin/bash

set -e

make kmod -j$(nproc)
make root/main

WORKSPACE=$(pwd)

pushd root
	pwd
	find . -print0 \
		| cpio --quiet --null -ov --format=newc \
		| gzip --quiet -9 > $WORKSPACE/build/initramfs.cpio.gz 2>/dev/null
popd


QEMU_FLAGS="-smp cores=1,sockets=1 -m 8G -machine q35 "
QEMU_FLAGS+="-s "
QEMU_FLAGS+="-enable-kvm "
QEMU_FLAGS+="-kernel $WORKSPACE/build/arch/x86_64/boot/bzImage "
QEMU_FLAGS+="-initrd $WORKSPACE/build/initramfs.cpio.gz "
QEMU_FLAGS+="-nographic "
QEMU_FLAGS+="-netdev user,id=u1 -device e1000,netdev=u1 "
QEMU_FLAGS+="-rtc base=localtime "
# QEMU_FLAGS+="-curses "
QEMU_FLAGS+="-serial mon:stdio "

echo $QEMU_FLAGS
qemu-system-x86_64 $QEMU_FLAGS -append "nopti nokaslr console=ttyS0 root=/dev/sda" $@
