/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libdisk.h"

void *
read_block(int fd, daddr_t block, u_long sector_size)
{
	void *foo;
	int i;

	foo = malloc(sector_size);
	if (foo == NULL)
		return (NULL);
	if (-1 == lseek(fd, (off_t)block * sector_size, SEEK_SET)) {
		free (foo);
		return (NULL);
	}
	i = read(fd, foo, sector_size);
	if ((int)sector_size != i) {
		free (foo);
		return (NULL);
	}
	return foo;
}

int
write_block(int fd, daddr_t block, const void *foo, u_long sector_size)
{
	int i;

	if (-1 == lseek(fd, (off_t)block * sector_size, SEEK_SET))
		return (-1);
	i = write(fd, foo, sector_size);
	if ((int)sector_size != i)
		return (-1);
	return 0;
}
