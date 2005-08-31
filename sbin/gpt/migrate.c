/*-
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
#include <sys/disklabel.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"

/*
 * Allow compilation on platforms that do not have a BSD label.
 * The values are valid for amd64, i386 and ia64 disklabels.
 */
#ifndef LABELOFFSET
#define	LABELOFFSET	0
#endif
#ifndef LABELSECTOR
#define	LABELSECTOR	1
#endif

static int force;
static int slice;

static void
usage_migrate(void)
{

	fprintf(stderr,
	    "usage: %s [-fs] device\n", getprogname());
	exit(1);
}

static struct gpt_ent*
migrate_disklabel(int fd, off_t start, struct gpt_ent *ent)
{
	char *buf;
	struct disklabel *dl;
	off_t ofs, rawofs;
	int i;

	buf = gpt_read(fd, start + LABELSECTOR, 1);
	dl = (void*)(buf + LABELOFFSET);

	if (le32toh(dl->d_magic) != DISKMAGIC ||
	    le32toh(dl->d_magic2) != DISKMAGIC) {
		warnx("%s: warning: FreeBSD slice without disklabel",
		    device_name);
		return (ent);
	}

	rawofs = le32toh(dl->d_partitions[RAW_PART].p_offset) *
	    le32toh(dl->d_secsize);
	for (i = 0; i < le16toh(dl->d_npartitions); i++) {
		if (dl->d_partitions[i].p_fstype == FS_UNUSED)
			continue;
		ofs = le32toh(dl->d_partitions[i].p_offset) *
		    le32toh(dl->d_secsize);
		if (ofs < rawofs)
			rawofs = 0;
	}
	rawofs /= secsz;

	for (i = 0; i < le16toh(dl->d_npartitions); i++) {
		switch (dl->d_partitions[i].p_fstype) {
		case FS_UNUSED:
			continue;
		case FS_SWAP: {
			uuid_t swap = GPT_ENT_TYPE_FREEBSD_SWAP;
			le_uuid_enc(&ent->ent_type, &swap);
			unicode16(ent->ent_name,
			    L"FreeBSD swap partition", 36);
			break;
		}
		case FS_BSDFFS: {
			uuid_t ufs = GPT_ENT_TYPE_FREEBSD_UFS;
			le_uuid_enc(&ent->ent_type, &ufs);
			unicode16(ent->ent_name,
			    L"FreeBSD UFS partition", 36);
			break;
		}
		case FS_VINUM: {
			uuid_t vinum = GPT_ENT_TYPE_FREEBSD_VINUM;
			le_uuid_enc(&ent->ent_type, &vinum);
			unicode16(ent->ent_name,
			    L"FreeBSD vinum partition", 36);
			break;
		}
		default:
			warnx("%s: warning: unknown FreeBSD partition (%d)",
			    device_name, dl->d_partitions[i].p_fstype);
			continue;
		}

		ofs = (le32toh(dl->d_partitions[i].p_offset) *
		    le32toh(dl->d_secsize)) / secsz;
		ofs = (ofs > 0) ? ofs - rawofs : 0;
		ent->ent_lba_start = htole64(start + ofs);
		ent->ent_lba_end = htole64(start + ofs +
		    le32toh(dl->d_partitions[i].p_size) - 1LL);
		ent++;
	}

	return (ent);
}

static void
migrate(int fd)
{
	uuid_t uuid;
	off_t blocks, last;
	map_t *gpt, *tpg;
	map_t *tbl, *lbt;
	map_t *map;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	struct mbr *mbr;
	uint32_t start, size;
	unsigned int i;

	last = mediasz / secsz - 1LL;

	map = map_find(MAP_TYPE_MBR);
	if (map == NULL || map->map_start != 0) {
		warnx("%s: error: no partitions to convert", device_name);
		return;
	}

	mbr = map->map_data;

	if (map_find(MAP_TYPE_PRI_GPT_HDR) != NULL ||
	    map_find(MAP_TYPE_SEC_GPT_HDR) != NULL) {
		warnx("%s: error: device already contains a GPT", device_name);
		return;
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

	lbt = map_add(last - blocks, blocks, MAP_TYPE_SEC_GPT_TBL,
	    tbl->map_data);
	tpg = map_add(last, 1LL, MAP_TYPE_SEC_GPT_HDR, calloc(1, secsz));

	hdr = gpt->map_data;
	memcpy(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig));
	hdr->hdr_revision = htole32(GPT_HDR_REVISION);
	/*
	 * XXX struct gpt_hdr is not a multiple of 8 bytes in size and thus
	 * contains padding we must not include in the size.
	 */
	hdr->hdr_size = htole32(offsetof(struct gpt_hdr, padding));
	hdr->hdr_lba_self = htole64(gpt->map_start);
	hdr->hdr_lba_alt = htole64(tpg->map_start);
	hdr->hdr_lba_start = htole64(tbl->map_start + blocks);
	hdr->hdr_lba_end = htole64(lbt->map_start - 1LL);
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

	/* Mirror partitions. */
	for (i = 0; i < 4; i++) {
		start = le16toh(mbr->mbr_part[i].part_start_hi);
		start = (start << 16) + le16toh(mbr->mbr_part[i].part_start_lo);
		size = le16toh(mbr->mbr_part[i].part_size_hi);
		size = (size << 16) + le16toh(mbr->mbr_part[i].part_size_lo);

		switch (mbr->mbr_part[i].part_typ) {
		case 0:
			continue;
		case 165: {	/* FreeBSD */
			if (slice) {
				uuid_t freebsd = GPT_ENT_TYPE_FREEBSD;
				le_uuid_enc(&ent->ent_type, &freebsd);
				ent->ent_lba_start = htole64((uint64_t)start);
				ent->ent_lba_end = htole64(start + size - 1LL);
				unicode16(ent->ent_name,
				    L"FreeBSD disklabel partition", 36);
				ent++;
			} else
				ent = migrate_disklabel(fd, start, ent);
			break;
		}
		case 239: {	/* EFI */
			uuid_t efi_slice = GPT_ENT_TYPE_EFI;
			le_uuid_enc(&ent->ent_type, &efi_slice);
			ent->ent_lba_start = htole64((uint64_t)start);
			ent->ent_lba_end = htole64(start + size - 1LL);
			unicode16(ent->ent_name, L"EFI system partition", 36);
			ent++;
			break;
		}
		default:
			if (!force) {
				warnx("%s: error: unknown partition type (%d)",
				    device_name, mbr->mbr_part[i].part_typ);
				return;
			}
		}
	}
	ent = tbl->map_data;

	hdr->hdr_crc_table = htole32(crc32(ent, le32toh(hdr->hdr_entries) *
	    le32toh(hdr->hdr_entsz)));
	hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));

	gpt_write(fd, gpt);
	gpt_write(fd, tbl);

	/*
	 * Create backup GPT.
	 */
	memcpy(tpg->map_data, gpt->map_data, secsz);
	hdr = tpg->map_data;
	hdr->hdr_lba_self = htole64(tpg->map_start);
	hdr->hdr_lba_alt = htole64(gpt->map_start);
	hdr->hdr_lba_table = htole64(lbt->map_start);
	hdr->hdr_crc_self = 0;			/* Don't ever forget this! */
	hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));

	gpt_write(fd, lbt);
	gpt_write(fd, tpg);

	map = map_find(MAP_TYPE_MBR);
	mbr = map->map_data;
	/*
	 * Turn the MBR into a Protective MBR.
	 */
	bzero(mbr->mbr_part, sizeof(mbr->mbr_part));
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
	gpt_write(fd, map);
}

int
cmd_migrate(int argc, char *argv[])
{
	int ch, fd;

	/* Get the migrate options */
	while ((ch = getopt(argc, argv, "fs")) != -1) {
		switch(ch) {
		case 'f':
			force = 1;
			break;
		case 's':
			slice = 1;
			break;
		default:
			usage_migrate();
		}
	}

	if (argc == optind)
		usage_migrate();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd == -1) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		migrate(fd);

		gpt_close(fd);
	}

	return (0);
}
