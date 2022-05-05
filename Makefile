CC=clang
ARCH=x86_64

.PHONY: linux

# arguments to make
MARGS=CC=$(CC) ARCH=$(ARCH) O=$(PWD)/build --no-print-directory

build/linux_tag:
	@mkdir -p build
	@[ ! -d linux ] && git clone git@github.com:torvalds/linux.git --depth 1 linux || true
	@$(MAKE) $(MARGS) -C linux defconfig
	touch build/linux_tag


linux: build/linux_tag
	@$(MAKE) $(MARGS) -C linux
