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
#include <err.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sun_disklabel.h>
#include <paths.h>
#include <errno.h>
#include "libdisk.h"

int
Write_Disk(const struct disk *d1)
{
	struct sun_disklabel *sl;
	struct chunk *c, *c1, *c2;
	int i;
	char *p;
	u_long secpercyl;
	u_short *sp1, *sp2, cksum;
	char device[64];
	int fd;

	sl = calloc(sizeof *sl, 1);
	c = d1->chunks;
	c2 = c->part;
	secpercyl = d1->bios_sect * d1->bios_hd;
	sl->sl_pcylinders = c->size / secpercyl;
	sl->sl_ncylinders = c2->size / secpercyl;
	sl->sl_acylinders = sl->sl_pcylinders - sl->sl_ncylinders;
	sl->sl_magic = SUN_DKMAGIC;
	sl->sl_nsectors = d1->bios_sect;
	sl->sl_ntracks = d1->bios_hd;
	if (c->size > 4999 * 1024 * 2) {
		sprintf(sl->sl_text, "FreeBSD%luG cyl %u alt %u hd %u sec %u",
		    (c->size + 1024 * 1024) / (2 * 1024 * 1024),
		    sl->sl_ncylinders, sl->sl_acylinders,
		    sl->sl_ntracks, sl->sl_nsectors);
	} else {
		sprintf(sl->sl_text, "FreeBSD%luM cyl %u alt %u hd %u sec %u",
		    (c->size + 1024) / (2 * 1024),
		    sl->sl_ncylinders, sl->sl_acylinders,
		    sl->sl_ntracks, sl->sl_nsectors);
	}
	sl->sl_interleave = 1;
	sl->sl_sparespercyl = 0;
	sl->sl_rpm = 3600;

	for (c1 = c2->part; c1 != NULL; c1 = c1->next) {
		p = c1->name;
		p += strlen(p);
		p--;
		if (*p < 'a' || *p > 'h')
			continue;
		i = *p - 'a';
		sl->sl_part[i].sdkp_cyloffset = c1->offset / secpercyl;
		sl->sl_part[i].sdkp_nsectors = c1->size;
	}

	sp1 = (u_short *)sl;
	sp2 = (u_short *)(sl + 1);
	sl->sl_cksum = cksum = 0;
	while (sp1 < sp2)
		cksum ^= *sp1++;
	sl->sl_cksum = cksum;

	strcpy(device,_PATH_DEV);
        strcat(device,d1->name);

        fd = open(device,O_RDWR);
        if (fd < 0) {
                warn("open(%s) failed", device);
                return (1);
        }

	write_block(fd, 0, sl, sizeof *sl);
	close(fd);
	return 0;
}
