obj-m += heartbeat.o

MOD=heartbeat.ko
all: $(MOD)


clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean



$(MOD): heartbeat.c
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules


# insert the module into the kernel
ins: $(MOD)
	sudo insmod $(MOD)

# remove the module from the kernel
rm:
	sudo rmmod $(MOD)
