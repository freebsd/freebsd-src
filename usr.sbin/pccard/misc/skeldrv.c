#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
#include <pccard/cis.h>


main(argc, argv)
int	argc;
char	*argv[];
{
struct drv_desc drv;
int fd, err;

	fd = open("/dev/card0", 0);
	if (fd < 0)
		{
		perror("/dev/card0");
		exit(1);
		}
	strcpy(drv.name, "skel");
	if (argc == 2)
		drv.unit = atoi(argv[1]);
	else
		drv.unit = 0;
	drv.iobase = 0x300;
	drv.mem = 0xD4000;
	drv.memsize = 16*1024;
	drv.irq = 0xFFFF;
	drv.flags = 0x1234;
	if (ioctl(fd, PIOCSDRV, &drv))
		perror("set driver");
	close(fd);
}
