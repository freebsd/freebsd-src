/*
 * linux/fs/hfs/mdb.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains functions for reading/writing the MDB.
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
 * The HFS Master Directory Block (MDB).
 *
 * Also known as the Volume Information Block (VIB), this structure is
 * the HFS equivalent of a superblock.
 *
 * Reference: _Inside Macintosh: Files_ pages 2-59 through 2-62
 *
 * modified for HFS Extended
 */
struct raw_mdb {
	hfs_word_t	drSigWord;	/* Signature word indicating fs type */
	hfs_lword_t	drCrDate;	/* fs creation date/time */
	hfs_lword_t	drLsMod;	/* fs modification date/time */
	hfs_word_t	drAtrb;		/* fs attributes */
	hfs_word_t	drNmFls;	/* number of files in root directory */
	hfs_word_t	drVBMSt;	/* location (in 512-byte blocks)
					   of the volume bitmap */
	hfs_word_t	drAllocPtr;	/* location (in allocation blocks)
					   to begin next allocation search */
	hfs_word_t	drNmAlBlks;	/* number of allocation blocks */
	hfs_lword_t	drAlBlkSiz;	/* bytes in an allocation block */
	hfs_lword_t	drClpSiz;	/* clumpsize, the number of bytes to
					   allocate when extending a file */
	hfs_word_t	drAlBlSt;	/* location (in 512-byte blocks)
					   of the first allocation block */
	hfs_lword_t	drNxtCNID;	/* CNID to assign to the next
					   file or directory created */
	hfs_word_t	drFreeBks;	/* number of free allocation blocks */
	hfs_byte_t	drVN[28];	/* the volume label */
	hfs_lword_t	drVolBkUp;	/* fs backup date/time */
	hfs_word_t	drVSeqNum;	/* backup sequence number */
	hfs_lword_t	drWrCnt;	/* fs write count */
	hfs_lword_t	drXTClpSiz;	/* clumpsize for the extents B-tree */
	hfs_lword_t	drCTClpSiz;	/* clumpsize for the catalog B-tree */
	hfs_word_t	drNmRtDirs;	/* number of directories in
					   the root directory */
	hfs_lword_t	drFilCnt;	/* number of files in the fs */
	hfs_lword_t	drDirCnt;	/* number of directories in the fs */
	hfs_byte_t	drFndrInfo[32];	/* data used by the Finder */
	hfs_word_t	drEmbedSigWord;	/* embedded volume signature */
	hfs_lword_t     drEmbedExtent;  /* starting block number (xdrStABN) 
					   and number of allocation blocks 
					   (xdrNumABlks) occupied by embedded
					   volume */
	hfs_lword_t	drXTFlSize;	/* bytes in the extents B-tree */
	hfs_byte_t	drXTExtRec[12];	/* extents B-tree's first 3 extents */
	hfs_lword_t	drCTFlSize;	/* bytes in the catalog B-tree */
	hfs_byte_t	drCTExtRec[12];	/* catalog B-tree's first 3 extents */
} __attribute__((packed));

/*================ Global functions ================*/

/*
 * hfs_mdb_get()
 *
 * Build the in-core MDB for a filesystem, including
 * the B-trees and the volume bitmap.
 */
struct hfs_mdb *hfs_mdb_get(hfs_sysmdb sys_mdb, int readonly,
			    hfs_s32 part_start)
{
	struct hfs_mdb *mdb;
	hfs_buffer buf;
	struct raw_mdb *raw;
	unsigned int bs, block;
	int lcv, limit;
	hfs_buffer *bmbuf;

	if (!HFS_NEW(mdb)) {
		hfs_warn("hfs_fs: out of memory\n");
		return NULL;
	}

	memset(mdb, 0, sizeof(*mdb));
	mdb->magic = HFS_MDB_MAGIC;
	mdb->sys_mdb = sys_mdb;
	INIT_LIST_HEAD(&mdb->entry_dirty);
	hfs_init_waitqueue(&mdb->rename_wait);
	hfs_init_waitqueue(&mdb->bitmap_wait);

	/* See if this is an HFS filesystem */
	buf = hfs_buffer_get(sys_mdb, part_start + HFS_MDB_BLK, 1);
	if (!hfs_buffer_ok(buf)) {
		hfs_warn("hfs_fs: Unable to read superblock\n");
		HFS_DELETE(mdb);
		goto bail2;
	}

	raw = (struct raw_mdb *)hfs_buffer_data(buf);
	if (hfs_get_ns(raw->drSigWord) != htons(HFS_SUPER_MAGIC)) {
		hfs_buffer_put(buf);
		HFS_DELETE(mdb);
		goto bail2;
	}
	mdb->buf = buf;
	
	bs = hfs_get_hl(raw->drAlBlkSiz);
	if (!bs || (bs & (HFS_SECTOR_SIZE-1))) {
		hfs_warn("hfs_fs: bad allocation block size %d != 512\n", bs);
		hfs_buffer_put(buf);
		HFS_DELETE(mdb);
		goto bail2;
	}
	mdb->alloc_blksz = bs >> HFS_SECTOR_SIZE_BITS;

	/* These parameters are read from the MDB, and never written */
	mdb->create_date = hfs_get_hl(raw->drCrDate);
	mdb->fs_ablocks  = hfs_get_hs(raw->drNmAlBlks);
	mdb->fs_start    = hfs_get_hs(raw->drAlBlSt) + part_start;
	mdb->backup_date = hfs_get_hl(raw->drVolBkUp);
	mdb->clumpablks  = (hfs_get_hl(raw->drClpSiz) / mdb->alloc_blksz)
						 >> HFS_SECTOR_SIZE_BITS;
	memcpy(mdb->vname, raw->drVN, sizeof(raw->drVN));

	/* These parameters are read from and written to the MDB */
	mdb->modify_date  = hfs_get_nl(raw->drLsMod);
	mdb->attrib       = hfs_get_ns(raw->drAtrb);
	mdb->free_ablocks = hfs_get_hs(raw->drFreeBks);
	mdb->next_id      = hfs_get_hl(raw->drNxtCNID);
	mdb->write_count  = hfs_get_hl(raw->drWrCnt);
	mdb->root_files   = hfs_get_hs(raw->drNmFls);
	mdb->root_dirs    = hfs_get_hs(raw->drNmRtDirs);
	mdb->file_count   = hfs_get_hl(raw->drFilCnt);
	mdb->dir_count    = hfs_get_hl(raw->drDirCnt);

	/* TRY to get the alternate (backup) MDB. */
	lcv = mdb->fs_start + mdb->fs_ablocks * mdb->alloc_blksz;
	limit = lcv + mdb->alloc_blksz;
	for (; lcv < limit; ++lcv) {
		buf = hfs_buffer_get(sys_mdb, lcv, 1);
		if (hfs_buffer_ok(buf)) {
			struct raw_mdb *tmp =
				(struct raw_mdb *)hfs_buffer_data(buf);
			
			if (hfs_get_ns(tmp->drSigWord) ==
			    htons(HFS_SUPER_MAGIC)) {
				mdb->alt_buf = buf;
				break;
			}
		}
		hfs_buffer_put(buf);
	}
	
	if (mdb->alt_buf == NULL) {
		hfs_warn("hfs_fs: unable to locate alternate MDB\n");
		hfs_warn("hfs_fs: continuing without an alternate MDB\n");
	}
	
	/* read in the bitmap */
	block = hfs_get_hs(raw->drVBMSt) + part_start;
	bmbuf = mdb->bitmap;
	lcv = (mdb->fs_ablocks + 4095) / 4096;
	for ( ; lcv; --lcv, ++bmbuf, ++block) {
		if (!hfs_buffer_ok(*bmbuf =
				   hfs_buffer_get(sys_mdb, block, 1))) {
			hfs_warn("hfs_fs: unable to read volume bitmap\n");
			goto bail1;
		}
	}

	if (!(mdb->ext_tree = hfs_btree_init(mdb, htonl(HFS_EXT_CNID),
					     raw->drXTExtRec,
					     hfs_get_hl(raw->drXTFlSize),
					     hfs_get_hl(raw->drXTClpSiz))) ||
	    !(mdb->cat_tree = hfs_btree_init(mdb, htonl(HFS_CAT_CNID),
					     raw->drCTExtRec,
					     hfs_get_hl(raw->drCTFlSize),
					     hfs_get_hl(raw->drCTClpSiz)))) {
		hfs_warn("hfs_fs: unable to initialize data structures\n");
		goto bail1;
	}

	if (!(mdb->attrib & htons(HFS_SB_ATTRIB_CLEAN))) {
		hfs_warn("hfs_fs: WARNING: mounting unclean filesystem.\n");
	} else if (!readonly && !(mdb->attrib & (HFS_SB_ATTRIB_HLOCK | HFS_SB_ATTRIB_SLOCK))) {
		/* Mark the volume uncleanly unmounted in case we crash */
		hfs_put_ns(mdb->attrib & htons(~HFS_SB_ATTRIB_CLEAN),
			   raw->drAtrb);
		hfs_buffer_dirty(mdb->buf);
		hfs_buffer_sync(mdb->buf);
	}

	return mdb;

bail1:
	hfs_mdb_put(mdb, readonly);
bail2:
	return NULL;
}

/*
 * hfs_mdb_commit()
 *
 * Description:
 *   This updates the MDB on disk (look also at hfs_write_super()).
 *   It does not check, if the superblock has been modified, or
 *   if the filesystem has been mounted read-only. It is mainly
 *   called by hfs_write_super() and hfs_btree_extend().
 * Input Variable(s):
 *   struct hfs_mdb *mdb: Pointer to the hfs MDB
 *   int backup;
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'mdb' points to a "valid" (struct hfs_mdb).
 * Postconditions:
 *   The HFS MDB and on disk will be updated, by copying the possibly
 *   modified fields from the in memory MDB (in native byte order) to
 *   the disk block buffer.
 *   If 'backup' is non-zero then the alternate MDB is also written
 *   and the function doesn't return until it is actually on disk.
 */
void hfs_mdb_commit(struct hfs_mdb *mdb, int backup)
{
	struct raw_mdb *raw = (struct raw_mdb *)hfs_buffer_data(mdb->buf);

	/* Commit catalog entries to buffers */
	hfs_cat_commit(mdb);

	/* Commit B-tree data to buffers */
	hfs_btree_commit(mdb->cat_tree, raw->drCTExtRec, raw->drCTFlSize);
	hfs_btree_commit(mdb->ext_tree, raw->drXTExtRec, raw->drXTFlSize);

	/* Update write_count and modify_date */
	++mdb->write_count;
	mdb->modify_date = hfs_time();

	/* These parameters may have been modified, so write them back */
	hfs_put_nl(mdb->modify_date,   raw->drLsMod);
	hfs_put_hs(mdb->free_ablocks,  raw->drFreeBks);
	hfs_put_hl(mdb->next_id,       raw->drNxtCNID);
	hfs_put_hl(mdb->write_count,   raw->drWrCnt);
	hfs_put_hs(mdb->root_files,    raw->drNmFls);
	hfs_put_hs(mdb->root_dirs,     raw->drNmRtDirs);
	hfs_put_hl(mdb->file_count,    raw->drFilCnt);
	hfs_put_hl(mdb->dir_count,     raw->drDirCnt);

	/* write MDB to disk */
	hfs_buffer_dirty(mdb->buf);

       	/* write the backup MDB, not returning until it is written. 
         * we only do this when either the catalog or extents overflow
         * files grow. */
        if (backup && hfs_buffer_ok(mdb->alt_buf)) {
		struct raw_mdb *tmp = (struct raw_mdb *)
			hfs_buffer_data(mdb->alt_buf);
		
		if ((hfs_get_hl(tmp->drCTFlSize) < 
		     hfs_get_hl(raw->drCTFlSize)) ||
		    (hfs_get_hl(tmp->drXTFlSize) <
		     hfs_get_hl(raw->drXTFlSize))) {
			memcpy(hfs_buffer_data(mdb->alt_buf), 
			       hfs_buffer_data(mdb->buf), HFS_SECTOR_SIZE); 
			hfs_buffer_dirty(mdb->alt_buf);
			hfs_buffer_sync(mdb->alt_buf);
		}
        }
}

/*
 * hfs_mdb_put()
 *
 * Release the resources associated with the in-core MDB.  */
void hfs_mdb_put(struct hfs_mdb *mdb, int readonly) {
	int lcv;

	/* invalidate cached catalog entries */
	hfs_cat_invalidate(mdb);

	/* free the B-trees */
	hfs_btree_free(mdb->ext_tree);
	hfs_btree_free(mdb->cat_tree);

	/* free the volume bitmap */
	for (lcv = 0; lcv < HFS_BM_MAXBLOCKS; ++lcv) {
		hfs_buffer_put(mdb->bitmap[lcv]);
	}

	/* update volume attributes */
	if (!readonly) {
		struct raw_mdb *raw = 
				(struct raw_mdb *)hfs_buffer_data(mdb->buf);
		hfs_put_ns(mdb->attrib, raw->drAtrb);
		hfs_buffer_dirty(mdb->buf);
	}

	/* free the buffers holding the primary and alternate MDBs */
	hfs_buffer_put(mdb->buf);
	hfs_buffer_put(mdb->alt_buf);

	/* free the MDB */
	HFS_DELETE(mdb);
}
