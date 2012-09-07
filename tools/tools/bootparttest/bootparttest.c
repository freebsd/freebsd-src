/*-
 * Copyright (c) 2012 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <libgeom.h>
#include <libutil.h>
#include <part.h>
#include <stdio.h>
#include <unistd.h>

struct disk {
	const char	*name;
	uint64_t	mediasize;
	uint16_t	sectorsize;

	int		fd;
	int		file;
	off_t		offset;
};

static int
diskread(void *arg, void *buf, size_t blocks, off_t offset)
{
	struct disk *dp;

	dp = (struct disk *)arg;
	printf("%s: read %d blocks from the offset %jd [+%jd]\n", dp->name,
	    blocks, offset, dp->offset);
	if (offset >= dp->mediasize / dp->sectorsize)
		return (-1);

	return (pread(dp->fd, buf, blocks * dp->sectorsize,
	    (offset + dp->offset) * dp->sectorsize) != blocks * dp->sectorsize);
}

static const char*
ptable_type2str(const struct ptable *table)
{

	switch (ptable_gettype(table)) {
	case PTABLE_NONE:
		return ("None");
	case PTABLE_BSD:
		return ("BSD");
	case PTABLE_MBR:
		return ("MBR");
	case PTABLE_GPT:
		return ("GPT");
	case PTABLE_VTOC8:
		return ("VTOC8");
	};
	return ("Unknown");
}

#define	PWIDTH	35
static void
ptable_print(void *arg, const char *pname, const struct ptable_entry *part)
{
	struct ptable *table;
	struct disk *dp, bsd;
	char line[80], size[6];

	dp = (struct disk *)arg;
	sprintf(line, "  %s%s: %s", dp->file ? "disk0": dp->name, pname,
	    parttype2str(part->type));
	humanize_number(size, sizeof(size),
	    (part->end - part->start + 1) * dp->sectorsize, "",
	    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
	printf("%-*s%s\n", PWIDTH, line, size);
	if (part->type == PART_FREEBSD) {
		sprintf(line, "%s%s", dp->file ? "disk0": dp->name, pname);
		bsd.name = line;
		bsd.fd = dp->fd;
		bsd.file = 0;	/* to use dp->name in the next sprintf */
		bsd.offset = dp->offset + part->start;
		bsd.sectorsize = dp->sectorsize;
		bsd.mediasize = (part->end - part->start + 1) * dp->sectorsize;
		table = ptable_open(&bsd, bsd.mediasize / bsd.sectorsize,
		    bsd.sectorsize, diskread);
		if (table == NULL)
			return;
		ptable_iterate(table, &bsd, ptable_print);
		ptable_close(table);
	}
}
#undef PWIDTH

static void
inspect_disk(struct disk *dp)
{
	struct ptable *table;

	table = ptable_open(dp, dp->mediasize / dp->sectorsize,
	    dp->sectorsize, diskread);
	if (table == NULL) {
		printf("ptable_open failed\n");
		return;
	}
	printf("Partition table detected: %s\n", ptable_type2str(table));
	ptable_iterate(table, dp, ptable_print);
	ptable_close(table);
}

int
main(int argc, char **argv)
{
	struct stat sb;
	struct disk d;

	if (argc < 2)
		errx(1, "Usage: %s <GEOM provider name> | "
		    "<disk image file name>", argv[0]);
	d.name = argv[1];
	if (stat(d.name, &sb) == 0 && S_ISREG(sb.st_mode)) {
		d.fd = open(d.name, O_RDONLY);
		if (d.fd < 0)
			err(1, "open %s", d.name);
		d.mediasize = sb.st_size;
		d.sectorsize = 512;
		d.file = 1;
	} else {
		d.fd = g_open(d.name, 0);
		if (d.fd < 0)
			err(1, "g_open %s", d.name);
		d.mediasize = g_mediasize(d.fd);
		d.sectorsize = g_sectorsize(d.fd);
		d.file = 0;
	}
	d.offset = 0;
	printf("%s \"%s\" opened\n", d.file ? "Disk image": "GEOM provider",
	    d.name);
	printf("Mediasize: %ju Bytes (%ju sectors)\nSectorsize: %u Bytes\n",
	    d.mediasize, d.mediasize / d.sectorsize, d.sectorsize);

	inspect_disk(&d);

	if (d.file)
		close(d.fd);
	else
		g_close(d.fd);
	return (0);
}
