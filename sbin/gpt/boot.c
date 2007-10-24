/*-
 * Copyright (c) 2007 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "gpt.h"

static uuid_t boot_uuid = GPT_ENT_TYPE_FREEBSD_BOOT;
static const char *pmbr_path = "/boot/pmbr";
static const char *gptboot_path = "/boot/gptboot";
static u_long boot_size;

static void
usage_boot(void)
{

	fprintf(stderr,
	    "usage: %s [-b pmbr] [-g gptboot] [-s count] device ...\n",
	    getprogname());
	exit(1);
}

static int
gpt_find(uuid_t *type, map_t **mapp)
{
	map_t *gpt, *tbl, *map;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;

	/* Find a GPT partition with the requested UUID type. */
	gpt = map_find(MAP_TYPE_PRI_GPT_HDR);
	if (gpt == NULL) {
		warnx("%s: error: no primary GPT header", device_name);
		return (ENXIO);
	}

	tbl = map_find(MAP_TYPE_PRI_GPT_TBL);
	if (tbl == NULL) {
		warnx("%s: error: no primary partition table", device_name);
		return (ENXIO);
	}

	hdr = gpt->map_data;
	for (i = 0; i < le32toh(hdr->hdr_entries); i++) {
		ent = (void *)((char *)tbl->map_data + i *
		    le32toh(hdr->hdr_entsz));
		if (uuid_equal(&ent->ent_type, type, NULL))
			break;
	}
	if (i == le32toh(hdr->hdr_entries)) {
		*mapp = NULL;
		return (0);
	}

	/* Lookup the map corresponding to this partition. */
	for (map = map_find(MAP_TYPE_GPT_PART); map != NULL;
	     map = map->map_next) {
		if (map->map_type != MAP_TYPE_GPT_PART)
			continue;
		if (map->map_start == (off_t)le64toh(ent->ent_lba_start)) {
			assert(map->map_start + map->map_size - 1LL ==
			    (off_t)le64toh(ent->ent_lba_end));
			*mapp = map;
			return (0);
		}
	}

	/* Hmm, the map list is not in sync with the GPT table. */
	errx(1, "internal map list is corrupted");
}

static void
boot(int fd)
{
	struct stat sb;
	off_t bsize, ofs;
	map_t *pmbr, *gptboot;
	struct mbr *mbr;
	char *buf;
	ssize_t nbytes;
	unsigned int entry;
	int bfd;

	/* First step: verify boot partition size. */
	if (boot_size == 0)
		/* Default to 64k. */
		bsize = 65536 / secsz;
	else {
		if (boot_size * secsz < 16384) {
			warnx("invalid boot partition size %lu", boot_size);
			return;
		}
		bsize = boot_size;
	}
	
	/* Second step: write the PMBR boot loader into the PMBR. */
	pmbr = map_find(MAP_TYPE_PMBR);
	if (pmbr == NULL) {
		warnx("%s: error: PMBR not found", device_name);
		return;
	}
	bfd = open(pmbr_path, O_RDONLY);
	if (bfd < 0 || fstat(bfd, &sb) < 0) {
		warn("unable to open PMBR boot loader");
		return;
	}
	if (sb.st_size != secsz) {
		warnx("invalid PMBR boot loader");
		return;
	}
	mbr = pmbr->map_data;
	nbytes = read(bfd, mbr->mbr_code, sizeof(mbr->mbr_code));
	if (nbytes < 0) {
		warn("unable to read PMBR boot loader");
		return;
	}
	if (nbytes != sizeof(mbr->mbr_code)) {
		warnx("short read of PMBR boot loader");
		return;
	}
	close(bfd);
	gpt_write(fd, pmbr);

	/* Third step: open gptboot and obtain its size. */
	bfd = open(gptboot_path, O_RDONLY);
	if (bfd < 0 || fstat(bfd, &sb) < 0) {
		warn("unable to open GPT boot loader");
		return;
	}
	

	/* Fourth step: find an existing boot partition or create one. */
	if (gpt_find(&boot_uuid, &gptboot) != 0)
		return;
	if (gptboot != NULL) {
		if (gptboot->map_size * secsz < sb.st_size) {
			warnx("%s: error: boot partition is too small",
			    device_name);
			return;
		}
	} else if (bsize * secsz < sb.st_size) {
		warnx(
		    "%s: error: proposed size for boot partition is too small",
		    device_name);
		return;
	} else {
		entry = 0;
		gptboot = gpt_add_part(fd, boot_uuid, 0, bsize, &entry);
		if (gptboot == NULL)
			return;
	}

	/* Fourth step, write out the gptboot binary to the boot partition. */	
	buf = malloc(sb.st_size);
	nbytes = read(bfd, buf, sb.st_size);
	if (nbytes < 0) {
		warn("unable to read GPT boot loader");
		return;
	}
	if (nbytes != sb.st_size) {
		warnx("short read of GPT boot loader");
		return;
	}
	close(bfd);
	ofs = gptboot->map_start * secsz;
	if (lseek(fd, ofs, SEEK_SET) != ofs) {
		warn("%s: error: unable to seek to boot partition",
		    device_name);
		return;
	}
	nbytes = write(fd, buf, sb.st_size);
	if (nbytes < 0) {
		warn("unable to write GPT boot loader");
		return;
	}
	if (nbytes != sb.st_size) {
		warnx("short write of GPT boot loader");
		return;
	}
	free(buf);
}

int
cmd_boot(int argc, char *argv[])
{
	char *p;
	int ch, fd;

	while ((ch = getopt(argc, argv, "b:g:s:")) != -1) {
		switch (ch) {
		case 'b':
			pmbr_path = optarg;
			break;
		case 'g':
			gptboot_path = optarg;
			break;
		case 's':
			if (boot_size > 0)
				usage_boot();
			boot_size = strtol(optarg, &p, 10);
			if (*p != '\0' || boot_size < 1)
				usage_boot();
			break;
		default:
			usage_boot();
		}
	}

	if (argc == optind)
		usage_boot();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd < 0) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		boot(fd);

		gpt_close(fd);
	}

	return (0);
}
