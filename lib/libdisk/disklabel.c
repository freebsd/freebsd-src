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
#include <sys/disklabel.h>
#include "libdisk.h"

struct disklabel *
read_disklabel(int fd, daddr_t block, u_long sector_size)
{
	struct disklabel *dp;

	if ((dp = (struct disklabel *) read_block(fd, block, sector_size))) {
		if (dp->d_magic != DISKMAGIC || dp->d_magic2 != DISKMAGIC ||
		    dkcksum(dp) != 0) {
			free(dp);
			dp = 0;
		}
	}
	return dp;
}
