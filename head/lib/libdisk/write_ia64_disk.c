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
 *
 * CRC32 code derived from work by Gary S. Brown.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/diskmbr.h>
#include <sys/gpt.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "libdisk.h"

static uuid_t _efi = GPT_ENT_TYPE_EFI;
static uuid_t _fbsd = GPT_ENT_TYPE_FREEBSD;
static uuid_t _swap = GPT_ENT_TYPE_FREEBSD_SWAP;
static uuid_t _ufs = GPT_ENT_TYPE_FREEBSD_UFS;

static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t
crc32(const void *buf, size_t size)
{
	const uint8_t *p;
	uint32_t crc;

	p = buf;
	crc = ~0U;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return (crc ^ ~0U);
}

static int
write_pmbr(int fd, const struct disk *disk)
{
	struct dos_partition dp;
	char *buffer;
	u_long nsects;
	int error;

	error = 0;
	nsects = disk->media_size / disk->sector_size;
	nsects--;	/* The GPT starts at LBA 1 */

	buffer = calloc(disk->sector_size, 1);
	if (buffer == NULL)
		return (ENOMEM);
	buffer[DOSMAGICOFFSET] = DOSMAGIC & 0xff;
	buffer[DOSMAGICOFFSET + 1] = DOSMAGIC >> 8;

	dp.dp_flag = 0;
	dp.dp_shd = dp.dp_ssect = dp.dp_scyl = 0xff;
	dp.dp_typ = DOSPTYP_PMBR;
	dp.dp_ehd = dp.dp_esect = dp.dp_ecyl = 0xff;
	dp.dp_start = 1;
	dp.dp_size = (nsects > 0xffffffffu) ? ~0u : nsects;
	memcpy(buffer + DOSPARTOFF, &dp, DOSPARTSIZE);

	if (lseek(fd, 0L, SEEK_SET) != 0L ||
	    write(fd, buffer, disk->sector_size) != disk->sector_size)
		error = (errno) ? errno : EAGAIN;

	free(buffer);
	return (error);
}

static int
read_gpt(int fd, const struct disk *disk, struct gpt_hdr *hdr,
    struct gpt_ent *tbl)
{
	char *buffer;
	off_t off;
	size_t nsects, sz;
	int error, i;

	error = 0;
	nsects = disk->gpt_size * sizeof(struct gpt_ent) / disk->sector_size;
	nsects++;
	sz = nsects * disk->sector_size;
	buffer = malloc(sz);
	if (buffer == NULL)
		return (ENOMEM);

	if (lseek(fd, disk->sector_size, SEEK_SET) != disk->sector_size ||
	    read(fd, buffer, disk->sector_size) != disk->sector_size) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}
	if (memcmp(buffer, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) != 0) {
		/*
		 * No GPT on disk. Create one out of thin air.
		 */
		bzero(&hdr[0], sizeof(struct gpt_hdr));
		memcpy(hdr[0].hdr_sig, GPT_HDR_SIG, sizeof(hdr[0].hdr_sig));
		hdr[0].hdr_revision = GPT_HDR_REVISION;
		hdr[0].hdr_size = offsetof(struct gpt_hdr, padding);
		hdr[0].hdr_lba_self = 1;
		hdr[0].hdr_lba_alt = disk->media_size / disk->sector_size - 1L;
		hdr[0].hdr_lba_start = disk->lba_start;
		hdr[0].hdr_lba_end = disk->lba_end;
		uuid_create(&hdr[0].hdr_uuid, NULL);
		hdr[0].hdr_lba_table = 2;
		hdr[0].hdr_entries = disk->gpt_size;
		hdr[0].hdr_entsz = sizeof(struct gpt_ent);
		hdr[1] = hdr[0];
		hdr[1].hdr_lba_self = hdr[0].hdr_lba_alt;
		hdr[1].hdr_lba_alt = hdr[0].hdr_lba_self;
		hdr[1].hdr_lba_table = disk->lba_end + 1;

		for (i = 0; i < disk->gpt_size; i++) {
			bzero(&tbl[i], sizeof(struct gpt_ent));
			uuid_create(&tbl[i].ent_uuid, NULL);
		}

		goto bail;
	}

	/*
	 * We have a GPT on disk. Read it.
	 */
	memcpy(&hdr[0], buffer, sizeof(struct gpt_hdr));
	off = hdr->hdr_lba_table * disk->sector_size;
	if (lseek(fd, off, SEEK_SET) != off ||
	    read(fd, buffer, sz) != sz) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}
	memcpy(tbl, buffer, sizeof(struct gpt_ent) * disk->gpt_size);
	off = hdr->hdr_lba_alt * disk->sector_size;
	if (lseek(fd, off, SEEK_SET) != off ||
	    read(fd, buffer, disk->sector_size) != disk->sector_size) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}
	memcpy(&hdr[1], buffer, sizeof(struct gpt_hdr));

bail:
	free(buffer);
	return (error);
}

static int
update_gpt(int fd, const struct disk *disk, struct gpt_hdr *hdr,
    struct gpt_ent *tbl)
{
	struct gpt_ent *save;
	char *buffer;
	struct chunk *c;
	off_t off;
	size_t bufsz;
	int error, idx, sav;

	error = 0;

	/*
	 * Save the entries of those chunks that have an index. They are
	 * the ones that exist on disk already.
	 */
	sav = 0;
	for (c = disk->chunks->part; c != NULL; c = c->next) {
		if ((c->flags & CHUNK_HAS_INDEX))
			sav++;
	}
	if (sav > 0) {
		save = malloc(sav * sizeof(struct gpt_ent));
		if (save == NULL)
			abort();
		sav = 0;
		for (c = disk->chunks->part; c != NULL; c = c->next) {
			if ((c->flags & CHUNK_HAS_INDEX)) {
				idx = CHUNK_FTOI(c->flags);
				save[sav] = tbl[idx];
				c->flags ^= CHUNK_ITOF(idx);
				c->flags |= CHUNK_ITOF(sav);
				sav++;
			}
		}
	} else
		save = NULL;

	/*
	 * Clear the table entries.
	 */
	for (idx = 0; idx < disk->gpt_size; idx++) {
		uuid_create_nil(&tbl[idx].ent_type, NULL);
		tbl[idx].ent_lba_start = 0;
		tbl[idx].ent_lba_end = 0;
		tbl[idx].ent_attr = 0;
		bzero(tbl[idx].ent_name, sizeof(tbl[idx].ent_name));
	}

	/*
	 * Repopulate the table from the chunks, possibly using saved
	 * information.
	 */
	idx = 0;
	for (c = disk->chunks->part; c != NULL; c = c->next) {
		if (!(c->flags & CHUNK_HAS_INDEX)) {
			switch (c->type) {
			case freebsd:
				tbl[idx].ent_type = _fbsd;
				break;
			case efi:
				tbl[idx].ent_type = _efi;
				break;
			case part:
				switch (c->subtype) {
				case FS_SWAP:
					tbl[idx].ent_type = _swap;
					break;
				case FS_BSDFFS:
					tbl[idx].ent_type = _ufs;
					break;
				default:
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}
		} else {
			sav = CHUNK_FTOI(c->flags);
			tbl[idx].ent_type = save[sav].ent_type;
			memcpy(tbl[idx].ent_name, save[sav].ent_name,
			    sizeof(tbl[idx].ent_name));
		}
		tbl[idx].ent_lba_start = c->offset;
		tbl[idx].ent_lba_end = c->end;

		idx++;
		if (idx == disk->gpt_size)
			return (ENOSPC);
 	}
	if (save != NULL)
		free(save);

	hdr[0].hdr_crc_table = crc32(tbl,
	    disk->gpt_size * sizeof(struct gpt_ent));
	hdr[0].hdr_crc_self = 0;
	hdr[0].hdr_crc_self = crc32(&hdr[0], hdr[0].hdr_size);

	hdr[1].hdr_crc_table = hdr[0].hdr_crc_table;
	hdr[1].hdr_crc_self = 0;
	hdr[1].hdr_crc_self = crc32(&hdr[1], hdr[1].hdr_size);

	/*
	 * Write the new GPT back to the disk.
	 */
	bufsz = disk->gpt_size * sizeof(struct gpt_ent);
	if (bufsz == 0 || bufsz % disk->sector_size)
		bufsz += disk->sector_size;
	bufsz = (bufsz / disk->sector_size) * disk->sector_size;
	buffer = calloc(1, bufsz);

	memcpy(buffer, &hdr[0], sizeof(struct gpt_hdr));
	off = hdr[0].hdr_lba_self * disk->sector_size;
	if (lseek(fd, off, SEEK_SET) != off ||
	    write(fd, buffer, disk->sector_size) != disk->sector_size) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}
	memcpy(buffer, &hdr[1], sizeof(struct gpt_hdr));
	off = hdr[1].hdr_lba_self * disk->sector_size;
	if (lseek(fd, off, SEEK_SET) != off ||
	    write(fd, buffer, disk->sector_size) != disk->sector_size) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}
	memcpy(buffer, tbl, disk->gpt_size * sizeof(struct gpt_ent));
	off = hdr[0].hdr_lba_table * disk->sector_size;
	if (lseek(fd, off, SEEK_SET) != off ||
	    write(fd, buffer, bufsz) != bufsz) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}
	off = hdr[1].hdr_lba_table * disk->sector_size;
	if (lseek(fd, off, SEEK_SET) != off ||
	    write(fd, buffer, bufsz) != bufsz) {
		error = (errno) ? errno : EAGAIN;
		goto bail;
	}

bail:
	free(buffer);
	return (error);
}

int
Write_Disk(const struct disk *disk)
{
	char devname[64];
	struct gpt_hdr *hdr;
	struct gpt_ent *tbl;
	int error, fd;

	hdr = malloc(sizeof(struct gpt_hdr) * 2);
	if (hdr == NULL)
		return (ENOMEM);
	tbl = malloc(sizeof(struct gpt_ent) * disk->gpt_size);
	if (tbl == NULL) {
		free(hdr);
		return (ENOMEM);
	}

	snprintf(devname, sizeof(devname), "%s%s", _PATH_DEV, disk->name);
	fd = open(devname, O_RDWR);
	if (fd == -1) {
		free(tbl);
		free(hdr);
		return (errno);
	}

	/*
	 * We can always write the PMBR, because we reject disks that do not
	 * have a PMBR and are not virgin.
	 */
	error = write_pmbr(fd, disk);
	if (error)
		goto bail;

	/*
	 * Read the existing GPT from disk or otherwise create one out of
	 * thin air. This way we can preserve the UUIDs and the entry names
	 * when updating it.
	 */
	error = read_gpt(fd, disk, hdr, tbl);
	if (error)
		goto bail;

	/*
	 * Update and write the in-memory copy of the GPT.
	 */
	error = update_gpt(fd, disk, hdr, tbl);

bail:
	close(fd);
	free(tbl);
	free(hdr);
	return (error);
}
