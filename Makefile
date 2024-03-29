obj-m += heartbeat.o

INSTALL_MOD_PATH?=/usr/local/

heartbeat-objs := src/kmod.o
EXTRA_CFLAGS:=-I$(PWD)/include
MOD=heartbeat.ko

default: build/ex build/libhb.so

clean:
	rm -rf build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean



build/libhb.so: src/heartbeat.c src/entry.S
	@mkdir -p build
	gcc -g -Iinclude -shared -o $@ -fPIC $^


build/ex:  example/example.c src/heartbeat.c src/entry.S
	@mkdir -p build
	gcc -g -pthread -o $@ -Iinclude $^

build/sum_array:  rf_compiler/sum_array.c src/heartbeat.c src/entry.S
	@mkdir -p build
	gcc -g -pthread -o $@ -Iinclude $^

install:
	@install -m 755 build/libhb.so $(INSTALL_MOD_PATH)/lib/libhb.so
	@install -m 644 include/heartbeat.h $(INSTALL_MOD_PATH)/include/heartbeat.h
	@install -m 644 include/heartbeat_kmod.h $(INSTALL_MOD_PATH)/include/heartbeat_kmod.h

# run `make kmod` to build the kernel module
kmod: $(MOD)

$(MOD): src/kmod.c
	mkdir -p build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
# To build on Rainey's desktop nixos machine, use the command below.
#	make -C $(nix-build -E '(import <nixpkgs> {}).linuxKernel.kernels.linux_5_18.dev' --no-out-link)/lib/modules/*/build M=$(pwd) modules


# insert the module into the kernel
ins: $(MOD)
	sudo insmod $(MOD)
	sudo chmod 0777 /dev/heartbeat

# remove the module from the kernel
rm:
	sudo rmmod $(MOD)

test:
	make
	sudo sync
	make ins
	sudo build/ex
	make rm



.PHONY: test install
