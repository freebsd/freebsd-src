/*
 * Copyright (c) 2003 Marcel Moolenaar
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
#include <sys/diskmbr.h>
#include <sys/gpt.h>
#include <sys/uuid.h>

#include <fcntl.h>
#include <inttypes.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "libdisk.h"

static uuid_t _efi = GPT_ENT_TYPE_EFI;
static uuid_t _mbr = GPT_ENT_TYPE_MBR;
static uuid_t _fbsd = GPT_ENT_TYPE_FREEBSD;
static uuid_t _swap = GPT_ENT_TYPE_FREEBSD_SWAP;
static uuid_t _ufs = GPT_ENT_TYPE_FREEBSD_UFS;

static struct disk *
parse_disk(char *conftxt, const char *name)
{
	char devname[64];
	struct disk *disk;
	struct dos_partition *part;
	struct gpt_hdr *gpt;
	char *buffer, *p, *q;
	int fd, i;

	disk = (struct disk *)calloc(sizeof *disk, 1);
	if (disk == NULL)
		return (NULL);

	disk->name = strdup(name);
	p = strsep(&conftxt, " ");			/* media size */
	disk->media_size = strtoimax(p, &q, 0);
	if (*q)
		goto fail;

	p = strsep(&conftxt, " ");			/* sector size */
	disk->sector_size = strtoul(p, &q, 0);
	if (*q)
		goto fail;

	if (disk->sector_size == 0)
		disk->sector_size = 512;

	if (disk->media_size % disk->sector_size)
		goto fail;

	/*
	 * We need to read the disk to get GPT specific information.
	 */

	snprintf(devname, sizeof(devname), "%s%s", _PATH_DEV, name);
	fd = open(devname, O_RDONLY);
	if (fd == -1)
		goto fail;
	buffer = malloc(2 * disk->sector_size);
	if (buffer == NULL) {
		close (fd);
		goto fail;
	}
	if (read(fd, buffer, 2 * disk->sector_size) == -1) {
		free(buffer);
		close(fd);
		goto fail;
	}
	close(fd);

	gpt = (struct gpt_hdr *)(buffer + disk->sector_size);
	if (memcmp(gpt->hdr_sig, GPT_HDR_SIG, sizeof(gpt->hdr_sig))) {
		/*
		 * No GPT present. Check if the MBR is empty (if present)
		 * or is a PMBR before declaring this disk as empty. If
		 * the MBR isn't empty, bail out. Let's not risk nuking a
		 * disk.
		 */
		if (*(u_short *)(buffer + DOSMAGICOFFSET) == DOSMAGIC) {
			for (i = 0; i < 4; i++) {
				part = (struct dos_partition *)
				    (buffer + DOSPARTOFF + i * DOSPARTSIZE);
				if (part->dp_typ != 0 &&
				    part->dp_typ != DOSPTYP_PMBR)
					break;
			}
			if (i < 4) {
				free(buffer);
				goto fail;
			}
		}
		disk->gpt_size = 128;
		disk->lba_start = (disk->gpt_size * sizeof(struct gpt_ent)) /
		    disk->sector_size + 2;
		disk->lba_end = (disk->media_size / disk->sector_size) -
		    disk->lba_start;
	} else {
		disk->lba_start = gpt->hdr_lba_start;
		disk->lba_end = gpt->hdr_lba_end;
		disk->gpt_size = gpt->hdr_entries;
	}
	free(buffer);
	Add_Chunk(disk, disk->lba_start, disk->lba_end - disk->lba_start + 1,
	    name, whole, 0, 0, "-");
	return (disk);

fail:
	free(disk->name);
	free(disk);
	return (NULL);
}

struct disk *
Int_Open_Disk(const char *name, char *conftxt)
{
	struct chunk chunk;
	uuid_t uuid;
	struct disk *disk;
	char *p, *q, *r, *s, *sd, *type;
	u_long i;
	uint32_t status;

	p = conftxt;
	while (p != NULL && *p != 0) {
		q = strsep(&p, " ");
		if (strcmp(q, "0") == 0) {
			q = strsep(&p, " ");
			if (strcmp(q, "DISK") == 0) {
				q = strsep(&p, " ");
				if (strcmp(q, name) == 0)
					break;
			}
		}
		p = strchr(p, '\n');
		if (p != NULL && *p == '\n')
			p++;
		conftxt = p;
	}
	if (p == NULL || *p == 0)
		return (NULL);

	conftxt = strchr(p, '\n');
	if (conftxt != NULL)
		*conftxt++ = '\0';

	disk = parse_disk(p, name);
	if (disk == NULL)
		return (NULL);

	while (conftxt != NULL && *conftxt != 0) {
		p = conftxt;
		conftxt = strchr(p, '\n');
		if (conftxt != NULL)
			*conftxt++ = '\0';

		sd = strsep(&p, " ");			/* depth */
		if (strcmp(sd, "0") == 0)
			break;

		type = strsep(&p, " ");			/* type */
		chunk.name = strsep(&p, " ");		/* name */
		q = strsep(&p, " ");			/* length */
		i = strtoimax(q, &r, 0);
		if (*r)
			abort();
		chunk.end = i / disk->sector_size;
		q = strsep(&p, " ");			/* sector size */

		for (;;) {
			q = strsep(&p, " ");
			if (q == NULL)
				break;
			r = strsep(&p, " ");
			i = strtoimax(r, &s, 0);
			if (*s) {
				uuid_from_string(r, &uuid, &status);
				if (status != uuid_s_ok)
					abort();
			} else
				status = uuid_s_invalid_string_uuid;
			if (!strcmp(q, "o"))
				chunk.offset = i / disk->sector_size;
			else if (!strcmp(q, "i"))
				chunk.flags = CHUNK_ITOF(i) | CHUNK_HAS_INDEX;
			else if (!strcmp(q, "ty"))
				chunk.subtype = i;
		}

		if (strncmp(type, "MBR", 3) == 0) {
			switch (chunk.subtype) {
			case 0xa5:
				chunk.type = freebsd;
				break;
			case 0x01:
			case 0x04:
			case 0x06:
			case 0x0b:
			case 0x0c:
			case 0x0e:
				chunk.type = fat;
				break;
			case 0xef:	/* EFI */
				chunk.type = efi;
				break;
			default:
				chunk.type = mbr;
				break;
			}
		} else if (strcmp(type, "BSD") == 0) {
			chunk.type = part;
		} else if (strcmp(type, "GPT") == 0) {
			chunk.subtype = 0;
			if (status != uuid_s_ok)
				abort();
			if (uuid_is_nil(&uuid, NULL))
				chunk.type = unused;
			else if (uuid_equal(&uuid, &_efi, NULL))
				chunk.type = efi;
			else if (uuid_equal(&uuid, &_mbr, NULL))
				chunk.type = unknown;
			else if (uuid_equal(&uuid, &_fbsd, NULL)) {
				chunk.type = freebsd;
				chunk.subtype = 0xa5;
			} else if (uuid_equal(&uuid, &_swap, NULL)) {
				chunk.type = part;
				chunk.subtype = FS_SWAP;
			} else if (uuid_equal(&uuid, &_ufs, NULL)) {
				chunk.type = part;
				chunk.subtype = FS_BSDFFS;
			} else
				chunk.type = unknown;
		} else
			abort();

		Add_Chunk(disk, chunk.offset, chunk.end, chunk.name,
		    chunk.type, chunk.subtype, chunk.flags, 0);
	}

	return (disk);
}
