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
#include <paths.h>
#include "libdisk.h"

/*
 * XXX: A lot of hardcoded 512s probably should be foo->sector_size;
 *	I'm not sure which, so I leave it like it worked before. --schweikh
 */
int
Write_Disk(const struct disk *d1)
{
	u_char buf[BBSIZE];
	char device[64];
	struct chunk *c1;
	struct disklabel *dl;
	void *p;
	uint64_t *lp, sum;
	int fd, i;

	strcpy(device, _PATH_DEV);
	strcat(device, d1->name);

	fd = open(device, O_RDWR);
	if (fd < 0)
                return (1);

	c1 = d1->chunks->part;
	if (!strcmp(c1->name, "X") || c1->type != freebsd) {
		close (fd);
		return (0);
	}

	for (i = 0; i < BBSIZE/512; i++) {
		if (!(p = read_block(fd, i + c1->offset, 512))) {
			close (fd);
			return (1);
		}
		memcpy(buf + 512 * i, p, 512);
		free(p);
	}
	if (d1->boot1)
		memcpy(buf + 512, d1->boot1, BBSIZE - 512);

	dl = (struct disklabel *)(buf + 512 * LABELSECTOR + LABELOFFSET);
	Fill_Disklabel(dl, d1, c1);

	/*
	 * Tell SRM where the bootstrap is.
	 */
	lp = (uint64_t *)buf;
	lp[60] = (BBSIZE - 512) / 512;	/* Length */
	lp[61] = 1;			/* Start */
	lp[62] = 0;			/* Flags */

	/*
	 * Generate the bootblock checksum for the SRM console.
	 */
	sum = 0;
	for (i = 0; i < 63; i++)
	    sum += lp[i];
	lp[63] = sum;

	for (i = 0; i < BBSIZE / 512; i++)
		write_block(fd, i + c1->offset, buf + 512 * i, 512);
	close(fd);

	return (0);
}
