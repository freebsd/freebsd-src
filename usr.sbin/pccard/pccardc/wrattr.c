#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/card.h>

int
wrattr_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     reg, value;
	char    name[64], c;
	int     fd;
	off_t   offs;

	if (argc != 4) {
		fprintf(stderr, "usage: wrmem slot offs value\n");
		exit(1);
	}
	sprintf(name, "/dev/card%d", atoi(argv[1]));
	fd = open(name, 2);
	if (fd < 0) {
		perror(name);
		exit(1);
	}
	reg = MDF_ATTR;
	if (ioctl(fd, PIOCRWFLAG, &reg)) {
		perror("ioctl (PIOCRWFLAG)");
		exit(1);
	}
	if (sscanf(argv[2], "%x", &reg) != 1 ||
	    sscanf(argv[3], "%x", &value) != 1) {
		fprintf(stderr, "arg error\n");
		exit(1);
	}
	offs = reg;
	c = value;
	lseek(fd, offs, SEEK_SET);
	if (write(fd, &c, 1) != 1)
		perror(name);
	return 0;
}
