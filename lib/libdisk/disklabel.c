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
#include <sys/disklabel.h>
#include "libdisk.h"

struct disklabel *
read_disklabel(int fd, daddr_t block)
{
	struct disklabel *dp;

	dp = (struct disklabel *) read_block(fd,block);
	if (dp->d_magic != DISKMAGIC)
		return 0;
	if (dp->d_magic2 != DISKMAGIC)
		return 0;
	if (dkcksum(dp) != 0)
		return 0;
	return dp;
}
