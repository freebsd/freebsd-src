/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"

static int force;
static int primary_only;

static void
usage_create(void)
{

	fprintf(stderr,
	    "usage: %s [-fp] device ...\n", getprogname());
	exit(1);
}

static void
create(int fd)
{
	uuid_t uuid;
	off_t blocks, last;
	map_t *gpt, *tpg;
	map_t *tbl, *lbt;
	map_t *map;
	struct mbr *mbr;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;

	last = mediasz / secsz - 1LL;

	if (map_find(MAP_TYPE_PRI_GPT_HDR) != NULL ||
	    map_find(MAP_TYPE_SEC_GPT_HDR) != NULL) {
		warnx("%s: error: device already contains a GPT", device_name);
		return;
	}
	map = map_find(MAP_TYPE_MBR);
	if (map != NULL) {
		if (!force) {
			warnx("%s: error: device contains a MBR", device_name);
			return;
		}

		/* Nuke the MBR in our internal map. */
		map->map_type = MAP_TYPE_UNUSED;
	}

	/*
	 * Create PMBR.
	 */
	if (map_find(MAP_TYPE_PMBR) == NULL) {
		if (map_free(0LL, 1LL) == 0) {
			warnx("%s: error: no room for the PMBR", device_name);
			return;
		}
		mbr = gpt_read(fd, 0LL, 1);
		bzero(mbr, sizeof(*mbr));
		mbr->mbr_sig = htole16(MBR_SIG);
		mbr->mbr_part[0].part_shd = 0xff;
		mbr->mbr_part[0].part_ssect = 0xff;
		mbr->mbr_part[0].part_scyl = 0xff;
		mbr->mbr_part[0].part_typ = 0xee;
		mbr->mbr_part[0].part_ehd = 0xff;
		mbr->mbr_part[0].part_esect = 0xff;
		mbr->mbr_part[0].part_ecyl = 0xff;
		mbr->mbr_part[0].part_start_lo = htole16(1);
		if (last > 0xffffffff) {
			mbr->mbr_part[0].part_size_lo = htole16(0xffff);
			mbr->mbr_part[0].part_size_hi = htole16(0xffff);
		} else {
			mbr->mbr_part[0].part_size_lo = htole16(last);
			mbr->mbr_part[0].part_size_hi = htole16(last >> 16);
		}
		map = map_add(0LL, 1LL, MAP_TYPE_PMBR, mbr);
		gpt_write(fd, map);
	}

	/* Get the amount of free space after the MBR */
	blocks = map_free(1LL, 0LL);
	if (blocks == 0LL) {
		warnx("%s: error: no room for the GPT header", device_name);
		return;
	}

	/* Don't create more than parts entries. */
	if ((uint64_t)(blocks - 1) * secsz > parts * sizeof(struct gpt_ent)) {
		blocks = (parts * sizeof(struct gpt_ent)) / secsz;
		if ((parts * sizeof(struct gpt_ent)) % secsz)
			blocks++;
		blocks++;		/* Don't forget the header itself */
	}

	/* Never cross the median of the device. */
	if ((blocks + 1LL) > ((last + 1LL) >> 1))
		blocks = ((last + 1LL) >> 1) - 1LL;

	/*
	 * Get the amount of free space at the end of the device and
	 * calculate the size for the GPT structures.
	 */
	map = map_last();
	if (map->map_type != MAP_TYPE_UNUSED) {
		warnx("%s: error: no room for the backup header", device_name);
		return;
	}

	if (map->map_size < blocks)
		blocks = map->map_size;
	if (blocks == 1LL) {
		warnx("%s: error: no room for the GPT table", device_name);
		return;
	}

	blocks--;		/* Number of blocks in the GPT table. */
	gpt = map_add(1LL, 1LL, MAP_TYPE_PRI_GPT_HDR, calloc(1, secsz));
	tbl = map_add(2LL, blocks, MAP_TYPE_PRI_GPT_TBL,
	    calloc(blocks, secsz));
	if (gpt == NULL || tbl == NULL)
		return;

	hdr = gpt->map_data;
	memcpy(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig));
	hdr->hdr_revision = htole32(GPT_HDR_REVISION);
	/*
	 * XXX struct gpt_hdr is not a multiple of 8 bytes in size and thus
	 * contains padding we must not include in the size.
	 */
	hdr->hdr_size = htole32(offsetof(struct gpt_hdr, padding));
	hdr->hdr_lba_self = htole64(gpt->map_start);
	hdr->hdr_lba_alt = htole64(last);
	hdr->hdr_lba_start = htole64(tbl->map_start + blocks);
	hdr->hdr_lba_end = htole64(last - blocks - 1LL);
	uuid_create(&uuid, NULL);
	le_uuid_enc(&hdr->hdr_uuid, &uuid);
	hdr->hdr_lba_table = htole64(tbl->map_start);
	hdr->hdr_entries = htole32((blocks * secsz) / sizeof(struct gpt_ent));
	if (le32toh(hdr->hdr_entries) > parts)
		hdr->hdr_entries = htole32(parts);
	hdr->hdr_entsz = htole32(sizeof(struct gpt_ent));

	ent = tbl->map_data;
	for (i = 0; i < le32toh(hdr->hdr_entries); i++) {
		uuid_create(&uuid, NULL);
		le_uuid_enc(&ent[i].ent_uuid, &uuid);
	}

	hdr->hdr_crc_table = htole32(crc32(ent, le32toh(hdr->hdr_entries) *
	    le32toh(hdr->hdr_entsz)));
	hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));

	gpt_write(fd, gpt);
	gpt_write(fd, tbl);

	/*
	 * Create backup GPT if the user didn't suppress it.
	 */
	if (!primary_only) {
		tpg = map_add(last, 1LL, MAP_TYPE_SEC_GPT_HDR,
		    calloc(1, secsz));
		lbt = map_add(last - blocks, blocks, MAP_TYPE_SEC_GPT_TBL,
		    tbl->map_data);
		memcpy(tpg->map_data, gpt->map_data, secsz);
		hdr = tpg->map_data;
		hdr->hdr_lba_self = htole64(tpg->map_start);
		hdr->hdr_lba_alt = htole64(gpt->map_start);
		hdr->hdr_lba_table = htole64(lbt->map_start);
		hdr->hdr_crc_self = 0;		/* Don't ever forget this! */
		hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));
		gpt_write(fd, lbt);
		gpt_write(fd, tpg);
	}
}

int
cmd_create(int argc, char *argv[])
{
	int ch, fd;

	while ((ch = getopt(argc, argv, "fp")) != -1) {
		switch(ch) {
		case 'f':
			force = 1;
			break;
		case 'p':
			primary_only = 1;
			break;
		default:
			usage_create();
		}
	}

	if (argc == optind)
		usage_create();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd == -1) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		create(fd);

		gpt_close(fd);
	}

	return (0);
}
