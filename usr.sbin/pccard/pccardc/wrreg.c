#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/card.h>

int
wrreg_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     reg, value;
	char    name[64];
	int     fd;
	struct pcic_reg r;

	if (argc != 4) {
		fprintf(stderr, "usage: wrreg slot reg value\n");
		exit(1);
	}
	sprintf(name, "/dev/card%d", atoi(argv[1]));
	fd = open(name, 2);
	if (fd < 0) {
		perror(name);
		exit(1);
	}
	if (sscanf(argv[2], "%x", &reg) != 1 ||
	    sscanf(argv[3], "%x", &value) != 1) {
		fprintf(stderr, "arg error\n");
		exit(1);
	}
	r.reg = reg;
	r.value = value;
	if (ioctl(fd, PIOCSREG, &r))
		perror("ioctl");
	return 0;
}
