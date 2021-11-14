obj-m += heartbeat.o

heartbeat-objs := src/kmod.o
EXTRA_CFLAGS:=-I$(PWD)/include
MOD=heartbeat.ko

default: build/libhb.so build/ex

clean:
	rm -rf build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean



build/libhb.so: src/heartbeat.c src/entry.S
	@mkdir -p build
	@gcc -Iinclude -shared -o $@ -fPIC $^


build/ex: build/libhb.so example/example.c
	gcc -pthread -o $@ -Iinclude $^


# run `make kmod` to build the kernel module
kmod: $(MOD)

$(MOD): src/kmod.c
	mkdir -p build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules


# insert the module into the kernel
ins: $(MOD)
	sudo insmod $(MOD)
	sudo chmod +rw /dev/heartbeat

# remove the module from the kernel
rm:
	sudo rmmod $(MOD)
