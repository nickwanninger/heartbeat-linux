CC=clang
ARCH=x86_64

.PHONY: linux run

# arguments to make
MARGS=CC=$(CC) ARCH=$(ARCH) O=$(PWD)/build --no-print-directory

build/linux_tag:
	@mkdir -p build
	@[ ! -d linux ] && git clone git@github.com:torvalds/linux.git --depth 1 linux || true
	@$(MAKE) $(MARGS) -C linux defconfig
	touch build/linux_tag


linux: build/linux_tag
	@$(MAKE) $(MARGS) -C linux


PWD := $(CURDIR)

kmod: linux
	$(MAKE) $(MARGS) -C $(PWD)/build M=$(PWD)/module modules 
	@mkdir -p root/etc/modules/
	@cp module/mod.ko root/etc/modules/
 
clean: 
	$(MAKE) $(MARGS) -C $(PWD)/build M=$(PWD)/module clean

run:
	@tools/run.sh
