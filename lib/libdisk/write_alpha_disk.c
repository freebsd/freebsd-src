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
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/diskmbr.h>
#include <paths.h>
#include "libdisk.h"

/* XXX: A lot of hardcoded 512s probably should be foo->sector_size;
        I'm not sure which, so I leave it like it worked before. --schweikh */
static int
Write_FreeBSD(int fd, const struct disk *new, const struct disk *old, const struct chunk *c1)
{
	struct disklabel *dl;
	int i;
	void *p;
	u_char buf[BBSIZE];
	u_long *lp, sum;

	for(i = 0; i < BBSIZE/512; i++) {
		p = read_block(fd, i + c1->offset, 512);
		memcpy(buf + 512 * i, p, 512);
		free(p);
	}
	if(new->boot1)
		memcpy(buf + 512, new->boot1, BBSIZE-512);

	dl = (struct disklabel *)(buf + 512 * LABELSECTOR + LABELOFFSET);
	Fill_Disklabel(dl, new, old, c1);

	/*
	 * Tell SRM where the bootstrap is.
	 */
	lp = (u_long *)buf;
	lp[60] = 15;
	lp[61] = 1;
	lp[62] = 0;

	/*
	 * Generate the bootblock checksum for the SRM console.
	 */
	for (lp = (u_long *)buf, i = 0, sum = 0; i < 63; i++)
	    sum += lp[i];
	lp[63] = sum;

	for(i=0;i<BBSIZE/512;i++) {
		write_block(fd, i + c1->offset, buf + 512 * i, 512);
	}

	return 0;
}



int
Write_Disk(const struct disk *d1)
{
	int fd,i;
	struct disk *old = 0;
	struct chunk *c1;
	int ret = 0;
	char device[64];
	u_char *mbr;
	struct dos_partition *dp,work[NDOSPART];
	int s[4];
	int one = 1;
	int zero = 0;

	strcpy(device,_PATH_DEV);
        strcat(device,d1->name);


        fd = open(device,O_RDWR);
        if (fd < 0) {
#ifdef DEBUG
                warn("open(%s) failed", device);
#endif
                return 1;
        }
	ioctl(fd, DIOCWLABEL, &one);

	memset(s,0,sizeof s);
	mbr = read_block(fd, 0, d1->sector_size);
	dp = (struct dos_partition*)(mbr + DOSPARTOFF);
	memcpy(work, dp, sizeof work);
	dp = work;
	free(mbr);
	for (c1 = d1->chunks->part; c1; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (!strcmp(c1->name, "X")) continue;
		if (c1->type == freebsd)
			ret += Write_FreeBSD(fd, d1, old, c1);

	}

	i = 1;
	i = ioctl(fd, DIOCSYNCSLICEINFO, &i);
#ifdef DEBUG
	if (i != 0)
		warn("ioctl(DIOCSYNCSLICEINFO)");
#endif
	ioctl(fd, DIOCWLABEL, &zero);
	close(fd);
	return 0;
}
