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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/gpt.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "map.h"
#include "gpt.h"

static uuid_t type;
static off_t block, size;

static void
usage_add(void)
{

	fprintf(stderr,
	    "usage: %s [-b lba] [-s lba] [-t uuid] device\n", getprogname());
	exit(1);
}

static void
add(int fd)
{
	map_t *gpt, *tpg;
	map_t *tbl, *lbt;
	map_t *map;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;

	gpt = map_find(MAP_TYPE_PRI_GPT_HDR);
	if (gpt == NULL) {
		warnx("%s: error: no primary GPT header; run create or recover",
		    device_name);
		return;
	}

	tpg = map_find(MAP_TYPE_SEC_GPT_HDR);
	if (tpg == NULL) {
		warnx("%s: error: no secondary GPT header; run recover",
		    device_name);
		return;
	}

	tbl = map_find(MAP_TYPE_PRI_GPT_TBL);
	lbt = map_find(MAP_TYPE_SEC_GPT_TBL);
	if (tbl == NULL || lbt == NULL) {
		warnx("%s: error: run recover -- trust me", device_name);
		return;
	}

	/* Create UFS partitions by default. */
	if (uuid_is_nil(&type, NULL)) {
		uuid_t ufs = GPT_ENT_TYPE_FREEBSD_UFS;
		type = ufs;
	}

	map = map_alloc(block, size);
	if (map == NULL) {
		warnx("%s: error: no space available on device", device_name);
		return;
	}

	/* Find empty slot in GPT table. */
	hdr = gpt->map_data;
	for (i = 0; i < hdr->hdr_entries; i++) {
		ent = (void*)((char*)tbl->map_data + i * hdr->hdr_entsz);
		if (uuid_is_nil(&ent->ent_type, NULL))
			break;
	}
	if (i == hdr->hdr_entries) {
		warnx("%s: error: no available table entries", device_name);
		return;
	}

	ent->ent_type = type;
	ent->ent_lba_start = map->map_start;
	ent->ent_lba_end = map->map_start + map->map_size - 1LL;

	hdr->hdr_crc_table = crc32(tbl->map_data,
	    hdr->hdr_entries * hdr->hdr_entsz);
	hdr->hdr_crc_self = 0;
	hdr->hdr_crc_self = crc32(hdr, hdr->hdr_size);

	gpt_write(fd, gpt);
	gpt_write(fd, tbl);

	hdr = tpg->map_data;
	ent = (void*)((char*)lbt->map_data + i * hdr->hdr_entsz);

	ent->ent_type = type;
	ent->ent_lba_start = map->map_start;
	ent->ent_lba_end = map->map_start + map->map_size - 1LL;

	hdr->hdr_crc_table = crc32(lbt->map_data,
	    hdr->hdr_entries * hdr->hdr_entsz);
	hdr->hdr_crc_self = 0;
	hdr->hdr_crc_self = crc32(hdr, hdr->hdr_size);

	gpt_write(fd, lbt);
	gpt_write(fd, tpg);
}

int
cmd_add(int argc, char *argv[])
{
	char *p;
	int ch, fd;
	uint32_t status;

	/* Get the migrate options */
	while ((ch = getopt(argc, argv, "b:s:t:")) != -1) {
		switch(ch) {
		case 'b':
			if (block > 0)
				usage_add();
			block = strtol(optarg, &p, 10);
			if (*p != 0 || block < 1)
				usage_add();
			break;
		case 's':
			if (size > 0)
				usage_add();
			size = strtol(optarg, &p, 10);
			if (*p != 0 || size < 1)
				usage_add();
			break;
		case 't':
			if (!uuid_is_nil(&type, NULL))
				usage_add();
			uuid_from_string(optarg, &type, &status);
			if (status != uuid_s_ok) {
				if (strcmp(optarg, "efi") == 0) {
					uuid_t efi = GPT_ENT_TYPE_EFI;
					type = efi;
				} else if (strcmp(optarg, "swap") == 0) {
					uuid_t sw = GPT_ENT_TYPE_FREEBSD_SWAP;
					type = sw;
				} else if (strcmp(optarg, "ufs") == 0) {
					uuid_t ufs = GPT_ENT_TYPE_FREEBSD_UFS;
					type = ufs;
				} else
					usage_add();
			}
			break;
		default:
			usage_add();
		}
	}

	if (argc == optind)
		usage_add();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd == -1) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		add(fd);

		gpt_close(fd);
	}

	return (0);
}
