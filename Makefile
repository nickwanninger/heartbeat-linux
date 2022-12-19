CC=gcc
ARCH=x86_64

# configure your linux!
LINUX_VERSION=5.17.5
LINUX_DIR=linux-$(LINUX_VERSION)
LINUX_URL=https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.17.5.tar.xz

.PHONY: linux run

# arguments to make
MARGS=CC=$(CC) ARCH=$(ARCH) O=$(PWD)/build --no-print-directory

build/linux_tag:
	@mkdir -p build
	@[ ! -d $(LINUX_DIR) ] && wget -c $(LINUX_URL) -O - | tar -Jx || true
	@$(MAKE) $(MARGS) -C $(LINUX_DIR) defconfig
	touch build/linux_tag


root/main: user/main.c
	clang -O3 -g3 -S -o build/main.S $^
	perl user/transform.pl build/main.S build/main.transform.S
	clang -O3 -g3 -pthread -lm -static -o $@ build/main.transform.S

menuconfig: build/linux_tag
	@$(MAKE) $(MARGS) -C $(LINUX_DIR) menuconfig



linux: build/linux_tag
	@$(MAKE) $(MARGS) -C $(LINUX_DIR)


PWD := $(CURDIR)

kmod: linux
	@mkdir -p $(PWD)/build/kmod
	$(MAKE) $(MARGS) -C $(PWD)/build M=$(PWD)/module modules 
	@mkdir -p root/etc/
	@cp module/mod.ko root/etc/
 
clean: 
	$(MAKE) $(MARGS) -C $(PWD)/build M=$(PWD)/module clean

run:
	@tools/run.sh
