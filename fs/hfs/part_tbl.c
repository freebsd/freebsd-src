/*
 * linux/fs/hfs/part_tbl.c
 *
 * Copyright (C) 1996-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * Original code to handle the new style Mac partition table based on
 * a patch contributed by Holger Schemel (aeglos@valinor.owl.de).
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 *
 * The code in this file initializes some structures which contain
 * pointers by calling memset(&foo, 0, sizeof(foo)).
 * This produces the desired behavior only due to the non-ANSI
 * assumption that the machine representation of NULL is all zeros.
 */

#include "hfs.h"

/*================ File-local data types ================*/

/*
 * The Macintosh Driver Descriptor Block
 *
 * On partitioned Macintosh media this is block 0.
 * We really only need the "magic number" to check for partitioned media.
 */
struct hfs_drvr_desc {
	hfs_word_t	ddSig;		/* The signature word */
	/* a bunch more stuff we don't need */
};

/* 
 * The new style Mac partition map
 *
 * For each partition on the media there is a physical block (512-byte
 * block) containing one of these structures.  These blocks are
 * contiguous starting at block 1.
 */
struct new_pmap {
	hfs_word_t	pmSig;		/* Signature bytes to verify
					   that this is a partition
					   map block */
	hfs_word_t	reSigPad;	/* padding */
	hfs_lword_t	pmMapBlkCnt;	/* (At least in block 1) this
					   is the number of partition
					   map blocks */
	hfs_lword_t	pmPyPartStart;	/* The physical block number
					   of the first block in this
					   partition */
	hfs_lword_t	pmPartBlkCnt;	/* The number of physical
					   blocks in this partition */
	hfs_byte_t	pmPartName[32];	/* (null terminated?) string
					   giving the name of this
					   partition */
	hfs_byte_t	pmPartType[32];	/* (null terminated?) string
					   giving the type of this
					   partition */
	/* a bunch more stuff we don't need */
};

/* 
 * The old style Mac partition map
 *
 * The partition map consists for a 2-byte signature followed by an
 * array of these structures.  The map is terminated with an all-zero
 * one of these.
 */
struct old_pmap {
	hfs_word_t		pdSig;	/* Signature bytes */
	struct 	old_pmap_entry {
		hfs_lword_t	pdStart;
		hfs_lword_t	pdSize;
		hfs_lword_t	pdFSID;
	}	pdEntry[42];
} __attribute__((packed));

/*================ File-local functions ================*/

/*
 * parse_new_part_table()
 *
 * Parse a new style partition map looking for the
 * start and length of the 'part'th HFS partition.
 */
static int parse_new_part_table(hfs_sysmdb sys_mdb, hfs_buffer buf,
				int part, hfs_s32 *size, hfs_s32 *start)
{
	struct new_pmap *pm = (struct new_pmap *)hfs_buffer_data(buf);
	hfs_u32 pmap_entries = hfs_get_hl(pm->pmMapBlkCnt);
	int hfs_part = 0;
	int entry;

	for (entry = 0; (entry < pmap_entries) && !(*start); ++entry) {
		if (entry) {
			/* read the next partition map entry */
			buf = hfs_buffer_get(sys_mdb, HFS_PMAP_BLK + entry, 1);
			if (!hfs_buffer_ok(buf)) {
				hfs_warn("hfs_fs: unable to "
				         "read partition map.\n");
				goto bail;
			}
			pm = (struct new_pmap *)hfs_buffer_data(buf);
			if (hfs_get_ns(pm->pmSig) !=
						htons(HFS_NEW_PMAP_MAGIC)) {
				hfs_warn("hfs_fs: invalid "
				         "entry in partition map\n");
				hfs_buffer_put(buf);
				goto bail;
			}
		}

		/* look for an HFS partition */
		if (!memcmp(pm->pmPartType,"Apple_HFS",9) && 
		    ((hfs_part++) == part)) {
			/* Found it! */
			*start = hfs_get_hl(pm->pmPyPartStart);
			*size = hfs_get_hl(pm->pmPartBlkCnt);
		}

		hfs_buffer_put(buf);
	}

	return 0;

bail:
	return 1;
}

/*
 * parse_old_part_table()
 *
 * Parse a old style partition map looking for the
 * start and length of the 'part'th HFS partition.
 */
static int parse_old_part_table(hfs_sysmdb sys_mdb, hfs_buffer buf,
				int part, hfs_s32 *size, hfs_s32 *start)
{
	struct old_pmap *pm = (struct old_pmap *)hfs_buffer_data(buf);
	struct old_pmap_entry *p = &pm->pdEntry[0];
	int hfs_part = 0;

	while ((p->pdStart || p->pdSize || p->pdFSID) && !(*start)) {
		/* look for an HFS partition */
		if ((hfs_get_nl(p->pdFSID) == htonl(0x54465331)/*"TFS1"*/) &&
		    ((hfs_part++) == part)) {
			/* Found it! */
			*start = hfs_get_hl(p->pdStart);
			*size = hfs_get_hl(p->pdSize);
		}
		++p;
	}
	hfs_buffer_put(buf);

	return 0;
}

/*================ Global functions ================*/

/*
 * hfs_part_find()
 *
 * Parse the partition map looking for the
 * start and length of the 'part'th HFS partition.
 */
int hfs_part_find(hfs_sysmdb sys_mdb, int part, int silent,
		  hfs_s32 *size, hfs_s32 *start)
{
	hfs_buffer buf;
	hfs_u16 sig;
	int dd_found = 0;
	int retval = 1;

	/* Read block 0 to see if this media is partitioned */
	buf = hfs_buffer_get(sys_mdb, HFS_DD_BLK, 1);
	if (!hfs_buffer_ok(buf)) {
		hfs_warn("hfs_fs: Unable to read block 0.\n");
		goto done;
	}
	sig = hfs_get_ns(((struct hfs_drvr_desc *)hfs_buffer_data(buf))->ddSig);
	hfs_buffer_put(buf);

        if (sig == htons(HFS_DRVR_DESC_MAGIC)) {
		/* We are definitely on partitioned media. */
		dd_found = 1;
	}

	buf = hfs_buffer_get(sys_mdb, HFS_PMAP_BLK, 1);
	if (!hfs_buffer_ok(buf)) {
		hfs_warn("hfs_fs: Unable to read block 1.\n");
		goto done;
	}

	*size = *start = 0;

	switch (hfs_get_ns(hfs_buffer_data(buf))) {
	case __constant_htons(HFS_OLD_PMAP_MAGIC):
		retval = parse_old_part_table(sys_mdb, buf, part, size, start);
		break;

	case __constant_htons(HFS_NEW_PMAP_MAGIC):
		retval = parse_new_part_table(sys_mdb, buf, part, size, start);
		break;

	default:
		if (dd_found) {
			/* The media claimed to have a partition map */
			if (!silent) {
				hfs_warn("hfs_fs: This disk has an "
					 "unrecognized partition map type.\n");
			}
		} else {
			/* Conclude that the media is not partitioned */
			retval = 0;
		}
		goto done;
	}

	if (!retval) {
		if (*start == 0) {
			if (part) {
				hfs_warn("hfs_fs: unable to locate "
				         "HFS partition number %d.\n", part);
			} else {
				hfs_warn("hfs_fs: unable to locate any "
					 "HFS partitions.\n");
			}
			retval = 1;
		} else if (*size < 0) {
			hfs_warn("hfs_fs: Partition size > 1 Terabyte.\n");
			retval = 1;
		} else if (*start < 0) {
			hfs_warn("hfs_fs: Partition begins beyond 1 "
				 "Terabyte.\n");
			retval = 1;
		}
	}
done:
	return retval;
}
