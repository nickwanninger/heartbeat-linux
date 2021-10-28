#include <heartbeat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

void handler(void) {
	printf("handler!\n");
}

int main(int argc, char **argv) {

	int fd = open("/dev/heartbeat", O_RDWR);
	printf("fd = %d\n", fd);

	struct hb_configuration config;
	config.handler_address = (unsigned long)handler;
	int result = ioctl(fd, HB_INIT, &config);
	printf("ioctl result = %d\n", result);
	close(fd);

}
