#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/card.h>

int
pccardmem_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     addr = 0;
	int     fd;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [ memory-address ]\n", argv[0]);
		exit(1);
	}
	fd = open("/dev/card0", 0);
	if (fd < 0) {
		perror("/dev/card0");
		exit(1);
	}
	if (argc == 2) {
		if (sscanf(argv[1], "%x", &addr) != 1) {
			fprintf(stderr, "arg error\n");
			exit(1);
		}
	}
	if (ioctl(fd, PIOCRWMEM, &addr))
		perror("ioctl");
	else
		printf("PCCARD Memory address set to 0x%x\n", addr);
	exit(0);
}
