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
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/diskpc98.h>
#include <paths.h>
#include "libdisk.h"

/*
 * XXX: A lot of hardcoded 512s probably should be foo->sector_size;
 *	I'm not sure which, so I leave it like it worked before. --schweikh
 */
static int
Write_FreeBSD(int fd, const struct disk *new, const struct disk *old,
	      const struct chunk *c1)
{
	struct disklabel *dl;
	int i;
	void *p;
	u_char buf[BBSIZE];

	for (i = 0; i < BBSIZE / 512; i++) {
		p = read_block(fd, i + c1->offset, 512);
		memcpy(buf + 512 * i, p, 512);
		free(p);
	}
	if (new->boot1)
		memcpy(buf, new->boot1, 512);

	if (new->boot2)
		memcpy(buf + 512, new->boot2, BBSIZE - 512);

	dl = (struct disklabel *)(buf + 512 * LABELSECTOR + LABELOFFSET);
	Fill_Disklabel(dl, new, old, c1);

	for (i = 0; i < BBSIZE / 512; i++)
		write_block(fd, i + c1->offset, buf + 512 * i, 512);

	return 0;
}


int
Write_Disk(const struct disk *d1)
{
	int fd, i, j;
	struct disk *old = NULL;
	struct chunk *c1;
	int ret = 0;
	char device[64];
	u_char *mbr;
	struct pc98_partition *dp, work[NDOSPART];
	int s[7];
	int PC98_EntireDisk = 0;

	strcpy(device, _PATH_DEV);
        strcat(device, d1->name);

	/* XXX - for entire FreeBSD(98) */
	for (c1 = d1->chunks->part; c1; c1 = c1->next) {
	    if ((c1->type == freebsd) || (c1->offset == 0))
		device[9] = 0;
	}

	fd = open(device, O_RDWR);
	if (fd < 0) {
#ifdef DEBUG
		warn("open(%s) failed", device);
#endif
		return 1;
	}

	memset(s, 0, sizeof s);
	mbr = read_block(fd, 1, d1->sector_size);
	dp = (struct pc98_partition *)(mbr + DOSPARTOFF);
	memcpy(work, dp, sizeof work);
	dp = work;
	free(mbr);
	for (c1 = d1->chunks->part; c1; c1 = c1->next) {
		if (c1->type == unused)
			continue;
		if (!strcmp(c1->name, "X"))
			continue;
		j = c1->name[strlen(d1->name) + 1] - '1';
		if (j < 0 || j > 7)
			continue;
		s[j]++;
		if (c1->type == freebsd)
			ret += Write_FreeBSD(fd, d1, old, c1);

		i = c1->offset;
		dp[j].dp_ssect = dp[j].dp_ipl_sct = i % d1->bios_sect;
		i -= dp[j].dp_ssect;
		i /= d1->bios_sect;
		dp[j].dp_shd = dp[j].dp_ipl_head = i % d1->bios_hd;
		i -= dp[j].dp_shd;
		i /= d1->bios_hd;
		dp[j].dp_scyl = dp[j].dp_ipl_cyl = i;
#ifdef DEBUG
		printf("S:%lu = (%x/%x/%x)", c1->offset,
		       dp[j].dp_scyl, dp[j].dp_shd, dp[j].dp_ssect);
#endif

		i = c1->end;
#if 1
		dp[j].dp_esect = dp[j].dp_ehd = 0;
		dp[j].dp_ecyl = i / (d1->bios_sect * d1->bios_hd);
#else
		dp[j].dp_esect = i % d1->bios_sect;
		i -= dp[j].dp_esect;
		i /= d1->bios_sect;
		dp[j].dp_ehd = i % d1->bios_hd;
		i -= dp[j].dp_ehd;
		i /= d1->bios_hd;
		dp[j].dp_ecyl = i;
#endif
#ifdef DEBUG
		printf("  E:%lu = (%x/%x/%x)\n", c1->end,
		       dp[j].dp_ecyl, dp[j].dp_ehd, dp[j].dp_esect);
#endif

		dp[j].dp_mid = c1->subtype & 0xff;
		dp[j].dp_sid = c1->subtype >> 8;
		if (c1->flags & CHUNK_ACTIVE)
			dp[j].dp_mid |= 0x80;

		strncpy(dp[j].dp_name, c1->sname, 16);
	}
	j = 0;
	for (i = 0; i < NDOSPART; i++) {
		if (!s[i])
			memset(dp + i, 0, sizeof *dp);
	}

	if (d1->bootipl)
		write_block(fd, 0, d1->bootipl, d1->sector_size);

	mbr = read_block(fd, 1, d1->sector_size);
	memcpy(mbr + DOSPARTOFF, dp, sizeof *dp * NDOSPART);
	/* XXX - for entire FreeBSD(98) */
	for (c1 = d1->chunks->part; c1; c1 = c1->next)
		if (((c1->type == freebsd) || (c1->type == fat))
			 && (c1->offset == 0))
			PC98_EntireDisk = 1;
	if (PC98_EntireDisk == 0)
		write_block(fd, 1, mbr, d1->sector_size);

	if (d1->bootmenu)
		for (i = 0; i * d1->sector_size < d1->bootmenu_size; i++)
			write_block(fd, 2 + i,
				    &d1->bootmenu[i * d1->sector_size],
				    d1->sector_size);

	close(fd);
	return 0;
}
