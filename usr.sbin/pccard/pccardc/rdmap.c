#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
#include <pccard/cis.h>

int
rdmap_main(argc, argv)
int	argc;
char	*argv[];
{
int	node, mask;
struct card *cp;

	for (node = 0; node < 8; node++)
	    scan(node);
	exit(0);
}
static scan(slot)
int slot;
{
int	fd, mask;
char	blk[1024];
char	name[64];
struct slotstate st;

	sprintf(name, "/dev/card%d", slot);
	fd = open(name, 0);
	if (fd < 0)
		return;
	ioctl(fd, PIOCGSTATE, &st);
/*
	if (st.state == filled)
 */
		{
		dump_mem(fd, st.maxmem);
		dump_io(fd, st.maxio);
		}
	close(fd);
}
dump_mem(fd, nmem)
int fd, nmem;
{
struct mem_desc mem;
int	i;

	for (i = 0; i < nmem; i++)
		{
		mem.window = i;
		ioctl(fd, PIOCGMEM, &mem);
printf("Mem %d: flags 0x%03x host 0x%6x card %04x size %d bytes\n",
	mem.window, mem.flags, mem.start, mem.card, mem.size);
		}
}
dump_io(fd, nio)
int fd, nio;
{
struct io_desc io;
int	i;

	for (i = 0; i < nio; i++)
		{
		io.window = i;
		ioctl(fd, PIOCGIO, &io);
printf("I/O %d: flags 0x%03x port 0x%3x size %d bytes\n",
	io.window, io.flags, io.start, io.size);
		}
}
