/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include "libdisk.h"

void *
read_block(int fd, daddr_t block)
{
	void *foo;

	foo = malloc(512);
	if (!foo)
		err(1,"malloc");
	if (-1 == lseek(fd, (off_t)block * 512, SEEK_SET))
		err(1, "lseek");
	if (512 != read(fd,foo, 512))
		err(1,"read");
	return foo;
}

void
write_block(int fd, daddr_t block, void *foo)
{
	if (-1 == lseek(fd, (off_t)block * 512, SEEK_SET))
		err(1, "lseek");
	if (512 != write(fd,foo, 512))
		err(1,"write");
}
