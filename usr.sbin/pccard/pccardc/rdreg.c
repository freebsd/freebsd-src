#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
void
dumpslot(sl)
	int     sl;
{
	char    name[64];
	int     fd;
	struct pcic_reg r;

	sprintf(name, "/dev/card%d", sl);
	fd = open(name, 2);
	if (fd < 0) {
		perror(name);
		return;
	}
	printf("Registers for slot %d\n", sl);
	for (r.reg = 0; r.reg < 0x40; r.reg++) {
		if (ioctl(fd, PIOCGREG, &r)) {
			perror("ioctl");
			break;
		}
		if ((r.reg % 16) == 0)
			printf("%02x:", r.reg);
		printf(" %02x", r.value);
		if ((r.reg % 16) == 15)
			printf("\n");
	}
	close(fd);
}

int
rdreg_main(argc, argv)
	int     argc;
	char   *argv[];
{
	if (argc != 2) {
		dumpslot(0);
		dumpslot(1);
	} else
		dumpslot(atoi(argv[1]));
	return 0;
}
