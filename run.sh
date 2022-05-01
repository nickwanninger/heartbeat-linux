#!/bin/bash

set -x

WORKSPACE=$(pwd)


BUSYBOX_VERSION=1.32.0


mkdir -p $WORKSPACE/build/busybox-x86

if [ ! -d "busybox-${BUSYBOX_VERSION}" ]
then
	curl https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2 | tar xjf -
	pushd "busybox-${BUSYBOX_VERSION}" >/dev/null
		# create the destination and make the config
		mkdir -p $WORKSPACE/build/busybox-x86
		make O=$WORKSPACE/build/busybox-x86 defconfig

		# compile the code producing a static binary
		LDFLAGS="--static" make O=$WORKSPACE/build/busybox-x86 -j$(nproc)
		make O=$WORKSPACE/build/busybox-x86 install

	popd
fi


ROOT=$WORKSPACE/build/initramfs/busybox-x86
rm -rf $ROOT
mkdir -p $ROOT

cat << EOT > $ROOT/init
#!/bin/sh
echo "hello"

mount -t proc none /proc
mount -t sysfs none /sys

cat <<!


Boot took $(cut -d' ' -f1 /proc/uptime) seconds

        _       _     __ _                  
  /\/\ (_)_ __ (_)   / /(_)_ __  _   ___  __
 /    \| | '_ \| |  / / | | '_ \| | | \ \/ /
/ /\/\ \ | | | | | / /__| | | | | |_| |>  < 
\/    \/_|_| |_|_| \____/_|_| |_|\__,_/_/\_\ 


Welcome to mini_linux


!
exec /bin/sh
EOT

# create the target directory for our simplistic version of initramfs, and then
# copy the files over to it.
mkdir -p $ROOT/{bin,sbin,etc,proc,sys,usr/{bin,sbin}}
rsync -a $WORKSPACE/build/busybox-x86/_install/* $ROOT
chmod +x $ROOT/init

pushd $WORKSPACE/build/initramfs/busybox-x86
	find . -print0 \
		| cpio --quiet --null -ov --format=newc \
		| gzip -9 > $WORKSPACE/build/initramfs-busybox-x86.cpio.gz
popd


make CC=clang -C linux -j$(nproc)

qemu-system-x86_64 -m 8G                                 \
	-kernel $WORKSPACE/linux/arch/x86_64/boot/bzImage      \
	-initrd $WORKSPACE/build/initramfs-busybox-x86.cpio.gz \
	-nographic -append "console=ttyS0 rdinit=/init"
