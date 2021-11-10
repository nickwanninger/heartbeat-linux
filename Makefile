obj-m += heartbeat.o

heartbeat-objs := src/kmod.o
EXTRA_CFLAGS:=-I$(PWD)/include
MOD=heartbeat.ko

default:
	mkdir -p build
	cd build && cmake ../ && make -j
	cp build/compile_commands.json .

clean:
	rm -rf build
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean


build/user: src/user.c
	@clang $(EXTRA_CFLAGS) -o build/user src/user.c



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
