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
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/diskmbr.h>
#include <paths.h>
#include "libdisk.h"

/* XXX: A lot of hardcoded 512s probably should be foo->sector_size;
        I'm not sure which, so I leave it like it worked before. --schweikh */
static int
Write_FreeBSD(int fd, const struct disk *new, const struct chunk *c1)
{
	struct disklabel *dl;
	int i;
	void *p;
	u_char buf[BBSIZE];

	for (i = 0; i < BBSIZE/512; i++) {
		p = read_block(fd, i + c1->offset, 512);
		if (p == NULL)
			return (1);
		memcpy(buf + 512 * i, p, 512);
		free(p);
	}

	dl = (struct disklabel *)(buf + 512 * LABELSECTOR + LABELOFFSET);
	Fill_Disklabel(dl, new, NULL, c1);

	for (i = 0; i < BBSIZE/512; i++) {
		if (write_block(fd, i + c1->offset, buf + 512 * i, 512) != 0)
			return (1);
	}

	return (0);
}

static void
Write_Int32(u_int32_t *p, u_int32_t v)
{
	u_int8_t *bp = (u_int8_t *)p;
	bp[0] = (v >> 0) & 0xff;
	bp[1] = (v >> 8) & 0xff;
	bp[2] = (v >> 16) & 0xff;
	bp[3] = (v >> 24) & 0xff;
}

int
Write_Disk(const struct disk *d1)
{
	struct chunk *c1;
	char device[64];
	u_char *mbr;
	struct dos_partition *dp, work[NDOSPART];
	int s[4];
	int fd, i, j, ret;

	strcpy(device, _PATH_DEV);
        strcat(device, d1->name);

        fd = open(device,O_RDWR);
        if (fd < 0)
                return (1);

	memset(s, 0, sizeof(s));
	mbr = read_block(fd, 0, 512);
	if (mbr == NULL)
		goto fail;
	dp = (struct dos_partition*)(mbr + DOSPARTOFF);
	memcpy(work, dp, sizeof(work));
	dp = work;
	free(mbr);

	ret = 0;
	for (c1 = d1->chunks->part; c1; c1 = c1->next) {
		if (c1->type == unused || !strcmp(c1->name, "X"))
			continue;
		j = c1->name[strlen(d1->name) + 1] - '1';
		if (j < 0 || j > 3)
			continue;
		s[j]++;
		if (c1->type == freebsd)
			ret += Write_FreeBSD(fd, d1, c1);

		Write_Int32(&dp[j].dp_start, c1->offset);
		Write_Int32(&dp[j].dp_size, c1->size);

		i = 1024*d1->bios_sect*d1->bios_hd;
		if (i == 0 || c1->offset >= i) {
			/* Start */
			dp[j].dp_ssect = 0xff;
			dp[j].dp_shd = 0xff;
			dp[j].dp_scyl = 0xff;

			/* End */
			dp[j].dp_esect = 0xff;
			dp[j].dp_ehd = 0xff;
			dp[j].dp_ecyl = 0xff;
		} else {
			/* Start */
			i = c1->offset;
			dp[j].dp_ssect = i % d1->bios_sect;
			i -= dp[j].dp_ssect++;
			i /= d1->bios_sect;
			dp[j].dp_shd =  i % d1->bios_hd;
			i -= dp[j].dp_shd;
			i /= d1->bios_hd;
			dp[j].dp_scyl = i;
			i -= dp[j].dp_scyl;
			dp[j].dp_ssect |= i >> 2;

			/* End */
			i = c1->end;
			dp[j].dp_esect = i % d1->bios_sect;
			i -= dp[j].dp_esect++;
			i /= d1->bios_sect;
			dp[j].dp_ehd =  i % d1->bios_hd;
			i -= dp[j].dp_ehd;
			i /= d1->bios_hd;
			dp[j].dp_ecyl = i;
			i -= dp[j].dp_ecyl;
			dp[j].dp_esect |= i >> 2;
		}

#ifdef DEBUG
		printf("S:%lu = (%x/%x/%x)  E:%lu = (%x/%x/%x)\n", c1->offset,
		    dp[j].dp_scyl, dp[j].dp_shd, dp[j].dp_ssect, c1->end,
		    dp[j].dp_ecyl, dp[j].dp_ehd, dp[j].dp_esect);
#endif

		dp[j].dp_typ = c1->subtype;
		if (c1->flags & CHUNK_ACTIVE)
			dp[j].dp_flag = 0x80;
		else
			dp[j].dp_flag = 0;
	}

	for(i = 0; i < NDOSPART; i++) {
		if (!s[i])
			memset(dp + i, 0, sizeof(*dp));
	}

	mbr = read_block(fd, 0, d1->sector_size);
	if (mbr == NULL)
		goto fail;
	memcpy(mbr + DOSPARTOFF, dp, sizeof(*dp) * NDOSPART);
	mbr[512-2] = 0x55;
	mbr[512-1] = 0xaa;
	ret += write_block(fd, 0, mbr, 512);
	free(mbr);
	if (ret != 0)
		goto fail;

	close(fd);
	return (0);

 fail:
	close(fd);
	return (1);
}
