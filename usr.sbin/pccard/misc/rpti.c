/*
 *	Enabler for PCCARD. Used for testing drivers etc.
 *	Options:
 *	enabler slot driver [ -m card addr size ] [ -i iobase ] [ -q irq ]
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
#include <pccard/cis.h>

void usage();


main(argc, argv)
int	argc;
char	*argv[];
{
struct drv_desc drv;
struct mem_desc mem;
struct io_desc io;
int fd, err, slot, i, card_addr;
char name[32], cmd[256];
char	*p;

	if (argc == 2)
		slot = atoi(argv[1]);
	else
		slot = 0;
	if (slot < 0 || slot >= MAXSLOT)
		usage("Illegal slot number");
	sprintf(cmd, "util/wrattr %d 100 80", slot);
	printf("%s\n", cmd);
	system(cmd);
	usleep(200*1000);
	sprintf(cmd, "util/wrattr %d 100 0", slot);
	printf("%s\n", cmd);
	system(cmd);
	usleep(200*1000);
	sprintf(cmd, "util/wrattr %d 100 30", slot);
	printf("%s\n", cmd);
	system(cmd);
	usleep(200*1000);
	bzero(&drv, sizeof(drv));
	drv.unit = 0;
	strcpy(drv.name, "ed");
	drv.irqmask = 1 << 5;
	sprintf(name, "/dev/card%d", slot);
	fd = open(name, 2);
	if (fd < 0)
		{
		perror(name);
		exit(1);
		}
/*
 *	Map the memory and I/O contexts.
 */
	drv.mem = 0xD4000;
	if (drv.mem)
		{
		mem.window = 0;
		mem.flags = MDF_ACTIVE;
		mem.start = (caddr_t)drv.mem;
		mem.size = 16*1024;
		mem.card = 0x4000;
		if (ioctl(fd, PIOCSMEM, &mem))
			{
			perror("Set memory context");
			exit(1);
			}
		}
	drv.iobase = 0x300;
	if (drv.iobase)
		{
		io.window = 0;
		io.flags = IODF_ACTIVE|IODF_CS16|IODF_WS;
		io.start = drv.iobase;
		io.size = 32;	/* Blah... */
		if (ioctl(fd, PIOCSIO, &io))
			{
			perror("Set I/O context");
			exit(1);
			}
#ifdef 0
		io.window = 1;
		io.flags = IODF_ACTIVE|IODF_16BIT;
		io.start = drv.iobase+16;
		io.size = 16;	/* Blah... */
		if (ioctl(fd, PIOCSIO, &io))
			{
			perror("Set I/O context");
			exit(1);
			}
#endif
		}
	if (ioctl(fd, PIOCSDRV, &drv))
		perror("set driver");
	close(fd);
}
/*
 *	usage - print usage and exit
 */
void
usage(msg)
char *msg;
{
	fprintf(stderr, "rpti: %s\n", msg);
	fprintf(stderr, "Usage: rpti slot driver\n");
	exit(1);
}
