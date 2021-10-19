obj-m += timer_module.o

all: timer_module.ko


clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean



timer_module.ko: timer_module.c
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules


# insert the module into the kernel
ins: timer_module.ko
	sudo insmod timer_module.ko

# remove the module from the kernel
rm:
	sudo rmmod timer_module.ko
