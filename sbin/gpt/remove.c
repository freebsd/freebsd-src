/*-
 * Copyright (c) 2004 Marcel Moolenaar
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

static int all;
static uuid_t type;
static off_t block, size;
static unsigned int entry;

static void
usage_remove(void)
{

	fprintf(stderr,
	    "usage: %s -a device ...\n"
	    "       %s [-b lba] [-i index] [-s lba] [-t uuid] device ...\n",
	    getprogname(), getprogname());
	exit(1);
}

static void
rem(int fd)
{
	uuid_t uuid;
	map_t *gpt, *tpg;
	map_t *tbl, *lbt;
	map_t *m;
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

	/* Remove all matching entries in the map. */
	for (m = map_first(); m != NULL; m = m->map_next) {
		if (m->map_type != MAP_TYPE_GPT_PART || m->map_index < 1)
			continue;
		if (entry > 0 && entry != m->map_index)
			continue;
		if (block > 0 && block != m->map_start)
			continue;
		if (size > 0 && size != m->map_size)
			continue;

		i = m->map_index - 1;

		hdr = gpt->map_data;
		ent = (void*)((char*)tbl->map_data + i *
		    le32toh(hdr->hdr_entsz));
		le_uuid_dec(&ent->ent_type, &uuid);
		if (!uuid_is_nil(&type, NULL) &&
		    !uuid_equal(&type, &uuid, NULL))
			continue;

		/* Remove the primary entry by clearing the partition type. */
		uuid_create_nil(&ent->ent_type, NULL);

		hdr->hdr_crc_table = htole32(crc32(tbl->map_data,
		    le32toh(hdr->hdr_entries) * le32toh(hdr->hdr_entsz)));
		hdr->hdr_crc_self = 0;
		hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));

		gpt_write(fd, gpt);
		gpt_write(fd, tbl);

		hdr = tpg->map_data;
		ent = (void*)((char*)lbt->map_data + i *
		    le32toh(hdr->hdr_entsz));

		/* Remove the secundary entry. */
		uuid_create_nil(&ent->ent_type, NULL);

		hdr->hdr_crc_table = htole32(crc32(lbt->map_data,
		    le32toh(hdr->hdr_entries) * le32toh(hdr->hdr_entsz)));
		hdr->hdr_crc_self = 0;
		hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));

		gpt_write(fd, lbt);
		gpt_write(fd, tpg);

		printf("%sp%u removed\n", device_name, m->map_index);
	}
}

int
cmd_remove(int argc, char *argv[])
{
	char *p;
	int ch, fd;
	uint32_t status;

	/* Get the remove options */
	while ((ch = getopt(argc, argv, "ab:i:s:t:")) != -1) {
		switch(ch) {
		case 'a':
			if (all > 0)
				usage_remove();
			all = 1;
			break;
		case 'b':
			if (block > 0)
				usage_remove();
			block = strtol(optarg, &p, 10);
			if (*p != 0 || block < 1)
				usage_remove();
			break;
		case 'i':
			if (entry > 0)
				usage_remove();
			entry = strtol(optarg, &p, 10);
			if (*p != 0 || entry < 1)
				usage_remove();
			break;
		case 's':
			if (size > 0)
				usage_remove();
			size = strtol(optarg, &p, 10);
			if (*p != 0 || size < 1)
				usage_remove();
			break;
		case 't':
			if (!uuid_is_nil(&type, NULL))
				usage_remove();
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
				} else if (strcmp(optarg, "linux") == 0 ||
				    strcmp(optarg, "windows") == 0) {
					uuid_t m = GPT_ENT_TYPE_MS_BASIC_DATA;
					type = m;
				} else
					usage_remove();
			}
			break;
		default:
			usage_remove();
		}
	}

	if (!all ^
	    (block > 0 || entry > 0 || size > 0 || !uuid_is_nil(&type, NULL)))
		usage_remove();

	if (argc == optind)
		usage_remove();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd == -1) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		rem(fd);

		gpt_close(fd);
	}

	return (0);
}
