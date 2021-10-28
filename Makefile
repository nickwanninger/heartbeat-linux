obj-m += heartbeat.o

heartbeat-objs := src/kmod.o
EXTRA_CFLAGS:=-I$(PWD)/include
MOD=heartbeat.ko
all: $(MOD) build/user


clean:
	rm -rf build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean


build/user: src/user.c
	@clang $(EXTRA_CFLAGS) -o build/user src/user.c


$(MOD): src/kmod.c
	mkdir -p build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules


# insert the module into the kernel
ins: $(MOD)
	sudo insmod $(MOD)

# remove the module from the kernel
rm:
	sudo rmmod $(MOD)
