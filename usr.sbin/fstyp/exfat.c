/*
 * Copyright (c) 2017 Conrad Meyer <cem@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/endian.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#ifdef WITH_ICONV
#include <iconv.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fstyp.h"

/*
 * https://docs.microsoft.com/en-us/windows/win32/fileio/exfat-specification
 */

struct exfat_vbr {
	char		ev_jmp[3];
	char		ev_fsname[8];
	char		ev_zeros[53];
	uint64_t	ev_part_offset;
	uint64_t	ev_vol_length;
	uint32_t	ev_fat_offset;
	uint32_t	ev_fat_length;
	uint32_t	ev_cluster_offset;
	uint32_t	ev_cluster_count;
	uint32_t	ev_rootdir_cluster;
	uint32_t	ev_vol_serial;
	uint16_t	ev_fs_revision;
	uint16_t	ev_vol_flags;
	uint8_t		ev_log_bytes_per_sect;
	uint8_t		ev_log_sect_per_clust;
	uint8_t		ev_num_fats;
	uint8_t		ev_drive_sel;
	uint8_t		ev_percent_used;
} __packed;

struct exfat_dirent {
	uint8_t		xde_type;
#define	XDE_TYPE_INUSE_MASK	0x80	/* 1=in use */
#define	XDE_TYPE_INUSE_SHIFT	7
#define	XDE_TYPE_CATEGORY_MASK	0x40	/* 0=primary */
#define	XDE_TYPE_CATEGORY_SHIFT	6
#define	XDE_TYPE_IMPORTNC_MASK	0x20	/* 0=critical */
#define	XDE_TYPE_IMPORTNC_SHIFT	5
#define	XDE_TYPE_CODE_MASK	0x1f
/* InUse=0, ..., TypeCode=0: EOD. */
#define	XDE_TYPE_EOD		0x00
#define	XDE_TYPE_ALLOC_BITMAP	(XDE_TYPE_INUSE_MASK | 0x01)
#define	XDE_TYPE_UPCASE_TABLE	(XDE_TYPE_INUSE_MASK | 0x02)
#define	XDE_TYPE_VOL_LABEL	(XDE_TYPE_INUSE_MASK | 0x03)
#define	XDE_TYPE_FILE		(XDE_TYPE_INUSE_MASK | 0x05)
#define	XDE_TYPE_VOL_GUID	(XDE_TYPE_INUSE_MASK | XDE_TYPE_IMPORTNC_MASK)
#define	XDE_TYPE_STREAM_EXT	(XDE_TYPE_INUSE_MASK | XDE_TYPE_CATEGORY_MASK)
#define	XDE_TYPE_FILE_NAME	(XDE_TYPE_INUSE_MASK | XDE_TYPE_CATEGORY_MASK | 0x01)
#define	XDE_TYPE_VENDOR		(XDE_TYPE_INUSE_MASK | XDE_TYPE_CATEGORY_MASK | XDE_TYPE_IMPORTNC_MASK)
#define	XDE_TYPE_VENDOR_ALLOC	(XDE_TYPE_INUSE_MASK | XDE_TYPE_CATEGORY_MASK | XDE_TYPE_IMPORTNC_MASK | 0x01)
	union {
		uint8_t	xde_generic_[19];
		struct exde_primary {
			/*
			 * Count of "secondary" dirents following this one.
			 *
			 * A single logical entity may be composed of a
			 * sequence of several dirents, starting with a primary
			 * one; the rest are secondary dirents.
			 */
			uint8_t		xde_secondary_count_;
			uint16_t	xde_set_chksum_;
			uint16_t	xde_prim_flags_;
			uint8_t		xde_prim_generic_[14];
		} __packed xde_primary_;
		struct exde_secondary {
			uint8_t		xde_sec_flags_;
			uint8_t		xde_sec_generic_[18];
		} __packed xde_secondary_;
	} u;
	uint32_t	xde_first_cluster;
	uint64_t	xde_data_len;
};
#define	xde_generic		u.xde_generic_
#define	xde_secondary_count	u.xde_primary_.xde_secondary_count
#define	xde_set_chksum		u.xde_primary_.xde_set_chksum_
#define	xde_prim_flags		u.xde_primary_.xde_prim_flags_
#define	xde_sec_flags		u.xde_secondary_.xde_sec_flags_
_Static_assert(sizeof(struct exfat_dirent) == 32, "spec");

struct exfat_de_label {
	uint8_t		xdel_type;	/* XDE_TYPE_VOL_LABEL */
	uint8_t		xdel_char_cnt;	/* Length of UCS-2 label */
	uint16_t	xdel_vol_lbl[11];
	uint8_t		xdel_reserved[8];
};
_Static_assert(sizeof(struct exfat_de_label) == 32, "spec");

#define	MAIN_BOOT_REGION_SECT	0
#define	BACKUP_BOOT_REGION_SECT	12

#define	SUBREGION_CHKSUM_SECT	11

#define	FIRST_CLUSTER		2
#define	BAD_BLOCK_SENTINEL	0xfffffff7u
#define	END_CLUSTER_SENTINEL	0xffffffffu

static inline void *
read_sectn(FILE *fp, off_t sect, unsigned count, unsigned bytespersec)
{
	return (read_buf(fp, sect * bytespersec, bytespersec * count));
}

static inline void *
read_sect(FILE *fp, off_t sect, unsigned bytespersec)
{
	return (read_sectn(fp, sect, 1, bytespersec));
}

/*
 * Compute the byte-by-byte multi-sector checksum of the given boot region
 * (MAIN or BACKUP), for a given bytespersec (typically 512 or 4096).
 *
 * Endian-safe; result is host endian.
 */
static int
exfat_compute_boot_chksum(FILE *fp, unsigned region, unsigned bytespersec,
    uint32_t *result)
{
	unsigned char *sector;
	unsigned n, sect;
	uint32_t checksum;

	checksum = 0;
	for (sect = 0; sect < 11; sect++) {
		sector = read_sect(fp, region + sect, bytespersec);
		if (sector == NULL)
			return (ENXIO);
		for (n = 0; n < bytespersec; n++) {
			if (sect == 0) {
				switch (n) {
				case 106:
				case 107:
				case 112:
					continue;
				}
			}
			checksum = ((checksum & 1) ? 0x80000000u : 0u) +
			    (checksum >> 1) + (uint32_t)sector[n];
		}
		free(sector);
	}

	*result = checksum;
	return (0);
}

#ifdef WITH_ICONV
static void
convert_label(const uint16_t *ucs2label /* LE */, unsigned ucs2len, char
    *label_out, size_t label_sz)
{
	const char *label;
	char *label_out_orig;
	iconv_t cd;
	size_t srcleft, rc;

	/* Currently hardcoded in fstyp.c as 256 or so. */
	assert(label_sz > 1);

	if (ucs2len == 0) {
		/*
		 * Kind of seems bogus, but the spec allows an empty label
		 * entry with the same meaning as no label.
		 */
		return;
	}

	if (ucs2len > 11) {
		warnx("exfat: Bogus volume label length: %u", ucs2len);
		return;
	}

	/* dstname="" means convert to the current locale. */
	cd = iconv_open("", EXFAT_ENC);
	if (cd == (iconv_t)-1) {
		warn("exfat: Could not open iconv");
		return;
	}

	label_out_orig = label_out;

	/* Dummy up the byte pointer and byte length iconv's API wants. */
	label = (const void *)ucs2label;
	srcleft = ucs2len * sizeof(*ucs2label);

	rc = iconv(cd, __DECONST(char **, &label), &srcleft, &label_out,
	    &label_sz);
	if (rc == (size_t)-1) {
		warn("exfat: iconv()");
		*label_out_orig = '\0';
	} else {
		/* NUL-terminate result (iconv advances label_out). */
		if (label_sz == 0)
			label_out--;
		*label_out = '\0';
	}

	iconv_close(cd);
}

/*
 * Using the FAT table, look up the next cluster in this chain.
 */
static uint32_t
exfat_fat_next(FILE *fp, const struct exfat_vbr *ev, unsigned BPS,
    uint32_t cluster)
{
	uint32_t fat_offset_sect, clsect, clsectoff;
	uint32_t *fatsect, nextclust;

	fat_offset_sect = le32toh(ev->ev_fat_offset);
	clsect = fat_offset_sect + (cluster / (BPS / sizeof(cluster)));
	clsectoff = (cluster % (BPS / sizeof(cluster)));

	/* XXX This is pretty wasteful without a block cache for the FAT. */
	fatsect = read_sect(fp, clsect, BPS);
	nextclust = le32toh(fatsect[clsectoff]);
	free(fatsect);

	return (nextclust);
}

static void
exfat_find_label(FILE *fp, const struct exfat_vbr *ev, unsigned BPS,
    char *label_out, size_t label_sz)
{
	uint32_t rootdir_cluster, sects_per_clust, cluster_offset_sect;
	off_t rootdir_sect;
	struct exfat_dirent *declust, *it;

	cluster_offset_sect = le32toh(ev->ev_cluster_offset);
	rootdir_cluster = le32toh(ev->ev_rootdir_cluster);
	sects_per_clust = (1u << ev->ev_log_sect_per_clust);

	if (rootdir_cluster < FIRST_CLUSTER) {
		warnx("%s: invalid rootdir cluster %u < %d", __func__,
		    rootdir_cluster, FIRST_CLUSTER);
		return;
	}


	for (; rootdir_cluster != END_CLUSTER_SENTINEL;
	    rootdir_cluster = exfat_fat_next(fp, ev, BPS, rootdir_cluster)) {
		if (rootdir_cluster == BAD_BLOCK_SENTINEL) {
			warnx("%s: Bogus bad block in root directory chain",
			    __func__);
			return;
		}

		rootdir_sect = (rootdir_cluster - FIRST_CLUSTER) *
		    sects_per_clust + cluster_offset_sect;
		declust = read_sectn(fp, rootdir_sect, sects_per_clust, BPS);
		for (it = declust;
		    it < declust + (sects_per_clust * BPS / sizeof(*it)); it++) {
			bool eod = false;

			/*
			 * Simplistic directory traversal; doesn't do any
			 * validation of "MUST" requirements in spec.
			 */
			switch (it->xde_type) {
			case XDE_TYPE_EOD:
				eod = true;
				break;
			case XDE_TYPE_VOL_LABEL: {
				struct exfat_de_label *lde = (void*)it;
				convert_label(lde->xdel_vol_lbl,
				    lde->xdel_char_cnt, label_out, label_sz);
				free(declust);
				return;
				}
			}

			if (eod)
				break;
		}
		free(declust);
	}
}
#endif /* WITH_ICONV */

int
fstyp_exfat(FILE *fp, char *label, size_t size)
{
	struct exfat_vbr *ev;
	uint32_t *cksect;
	unsigned bytespersec;
	uint32_t chksum;
	int error;

	error = 1;
	cksect = NULL;
	ev = (struct exfat_vbr *)read_buf(fp, 0, 512);
	if (ev == NULL || strncmp(ev->ev_fsname, "EXFAT   ", 8) != 0)
		goto out;

	if (ev->ev_log_bytes_per_sect < 9 || ev->ev_log_bytes_per_sect > 12) {
		warnx("exfat: Invalid BytesPerSectorShift");
		goto out;
	}

	bytespersec = (1u << ev->ev_log_bytes_per_sect);

	error = exfat_compute_boot_chksum(fp, MAIN_BOOT_REGION_SECT,
	    bytespersec, &chksum);
	if (error != 0)
		goto out;

	cksect = read_sect(fp, MAIN_BOOT_REGION_SECT + SUBREGION_CHKSUM_SECT,
	    bytespersec);

	/*
	 * Technically the entire sector should be full of repeating 4-byte
	 * checksum pattern, but we only verify the first.
	 */
	if (chksum != le32toh(cksect[0])) {
		warnx("exfat: Found checksum 0x%08x != computed 0x%08x",
		    le32toh(cksect[0]), chksum);
		error = 1;
		goto out;
	}

#ifdef WITH_ICONV
	if (show_label)
		exfat_find_label(fp, ev, bytespersec, label, size);
#else
	if (show_label) {
		warnx("label not available without iconv support");
		memset(label, 0, size);
	}
#endif

out:
	free(cksect);
	free(ev);
	return (error != 0);
}
