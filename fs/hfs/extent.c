/*
 * linux/fs/hfs/extent.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the functions related to the extents B-tree.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs.h"

/*================ File-local data type ================*/

/* An extent record on disk*/
struct hfs_raw_extent {
	hfs_word_t	block1;
	hfs_word_t	length1;
	hfs_word_t	block2;
	hfs_word_t	length2;
	hfs_word_t	block3;
	hfs_word_t	length3;
};

/*================ File-local functions ================*/

/*
 * build_key
 */
static inline void build_key(struct hfs_ext_key *key,
			     const struct hfs_fork *fork, hfs_u16 block)
{
	key->KeyLen = 7;
	key->FkType = fork->fork;
	hfs_put_nl(fork->entry->cnid, key->FNum);
	hfs_put_hs(block,             key->FABN);
}


/*
 * lock_bitmap()
 *
 * Get an exclusive lock on the B-tree bitmap.
 */
static inline void lock_bitmap(struct hfs_mdb *mdb) {
	while (mdb->bitmap_lock) {
		hfs_sleep_on(&mdb->bitmap_wait);
	}
	mdb->bitmap_lock = 1;
}

/*
 * unlock_bitmap()
 *
 * Relinquish an exclusive lock on the B-tree bitmap.
 */
static inline void unlock_bitmap(struct hfs_mdb *mdb) {
	mdb->bitmap_lock = 0;
	hfs_wake_up(&mdb->bitmap_wait);
}

/*
 * dump_ext()
 *
 * prints the content of a extent for debugging purposes.
 */
#if defined(DEBUG_EXTENTS) || defined(DEBUG_ALL)
static void dump_ext(const char *msg, const struct hfs_extent *e) {
	if (e) {
		hfs_warn("%s (%d-%d) (%d-%d) (%d-%d)\n", msg,
			 e->start,
			 e->start + e->length[0] - 1,
			 e->start + e->length[0],
			 e->start + e->length[0] + e->length[1] - 1,
			 e->start + e->length[0] + e->length[1],
			 e->end);
	} else {
		hfs_warn("%s NULL\n", msg);
	}
}
#else
#define dump_ext(A,B) {}
#endif

/*
 * read_extent()
 * 
 * Initializes a (struct hfs_extent) from a (struct hfs_raw_extent) and
 * the number of the starting block for the extent.
 *
 * Note that the callers must check that to,from != NULL
 */
static void read_extent(struct hfs_extent *to,
			const struct hfs_raw_extent *from,
			hfs_u16 start)
{
	to->start = start;
	to->block[0]  = hfs_get_hs(from->block1);
	to->length[0] = hfs_get_hs(from->length1);
	to->block[1]  = hfs_get_hs(from->block2);
	to->length[1] = hfs_get_hs(from->length2);
	to->block[2]  = hfs_get_hs(from->block3);
	to->length[2] = hfs_get_hs(from->length3);
	to->end = start + to->length[0] + to->length[1] + to->length[2] - 1;
	to->next = to->prev = NULL;
	to->count = 0;
}

/*
 * write_extent()
 * 
 * Initializes a (struct hfs_raw_extent) from a (struct hfs_extent).
 *
 * Note that the callers must check that to,from != NULL
 */
static void write_extent(struct hfs_raw_extent *to,
			 const struct hfs_extent *from)
{
	hfs_put_hs(from->block[0], to->block1);
	hfs_put_hs(from->length[0], to->length1);
	hfs_put_hs(from->block[1], to->block2);
	hfs_put_hs(from->length[1], to->length2);
	hfs_put_hs(from->block[2], to->block3);
	hfs_put_hs(from->length[2], to->length3);
}

/*
 * decode_extent()
 *
 * Given an extent record and allocation block offset into the file,
 * return the number of the corresponding allocation block on disk,
 * or -1 if the desired block is not mapped by the given extent.
 *
 * Note that callers must check that extent != NULL
 */
static int decode_extent(const struct hfs_extent * extent, int block)
{
	if (!extent || (block < extent->start) || (block > extent->end) ||
	    (extent->end == (hfs_u16)(extent->start - 1))) {
		return -1;
	}
	block -= extent->start;
	if (block < extent->length[0]) {
		return block + extent->block[0];
	}
	block -= extent->length[0];
	if (block < extent->length[1]) {
		return block + extent->block[1];
	}
	return block + extent->block[2] - extent->length[1];
}

/*
 * relse_ext()
 *
 * Reduce the reference count of an in-core extent record by one,
 * removing it from memory if the count falls to zero.
 */
static void relse_ext(struct hfs_extent *ext)
{
	if (--ext->count || !ext->start) {
		return;
	}
	ext->prev->next = ext->next;
	if (ext->next) {
		ext->next->prev = ext->prev;
	}
	HFS_DELETE(ext);
}

/*
 * set_cache()
 * 
 * Changes the 'cache' field of the fork.
 */
static inline void set_cache(struct hfs_fork *fork, struct hfs_extent *ext)
{
	struct hfs_extent *tmp = fork->cache;

	++ext->count;
	fork->cache = ext;
	relse_ext(tmp);
}

/*
 * find_ext()
 *
 * Given a pointer to a (struct hfs_file) and an allocation block
 * number in the file, find the extent record containing that block.
 * Returns a pointer to the extent record on success or NULL on failure.
 * The 'cache' field of 'fil' also points to the extent so it has a
 * reference count of at least 2.
 *
 * Callers must check that fil != NULL
 */
static struct hfs_extent * find_ext(struct hfs_fork *fork, int alloc_block)
{
        struct hfs_cat_entry *entry = fork->entry;
	struct hfs_btree *tr= entry->mdb->ext_tree;
	struct hfs_ext_key target, *key;
	struct hfs_brec brec;
	struct hfs_extent *ext, *ptr;
	int tmp;

	if (alloc_block < 0) {
		ext = &fork->first;
		goto found;
	}

	ext = fork->cache;
	if (!ext || (alloc_block < ext->start)) {
		ext = &fork->first;
	}
	while (ext->next && (alloc_block > ext->end)) {
		ext = ext->next;
	}
	if ((alloc_block <= ext->end) && (alloc_block >= ext->start)) {
		goto found;
	}

	/* time to read more extents */
	if (!HFS_NEW(ext)) {
		goto bail3;
	}

	build_key(&target, fork, alloc_block);

	tmp = hfs_bfind(&brec, tr, HFS_BKEY(&target), HFS_BFIND_READ_LE);
	if (tmp < 0) {
		goto bail2;
	}

	key = (struct hfs_ext_key *)brec.key;
	if ((hfs_get_nl(key->FNum) != hfs_get_nl(target.FNum)) ||
	    (key->FkType != fork->fork)) {
		goto bail1;
	}
		
	read_extent(ext, brec.data, hfs_get_hs(key->FABN));
	hfs_brec_relse(&brec, NULL);

	if ((alloc_block > ext->end) && (alloc_block < ext->start)) {
		/* something strange happened */
		goto bail2;
	}

	ptr = fork->cache;
	if (!ptr || (alloc_block < ptr->start)) {
		ptr = &fork->first;
	}
	while (ptr->next && (alloc_block > ptr->end)) {
		ptr = ptr->next;
	}
	if (ext->start == ptr->start) {
		/* somebody beat us to it. */
		HFS_DELETE(ext);
		ext = ptr;
	} else if (ext->start < ptr->start) {
		/* insert just before ptr */
		ptr->prev->next = ext;
		ext->prev = ptr->prev;
		ext->next = ptr;
		ptr->prev = ext;
	} else {
		/* insert at end */
		ptr->next = ext;
		ext->prev = ptr;
	}
 found:
	++ext->count; /* for return value */
	set_cache(fork, ext);
	return ext;

 bail1:
	hfs_brec_relse(&brec, NULL);
 bail2:
	HFS_DELETE(ext);
 bail3:
	return NULL;
}

/*
 * delete_extent()
 *
 * Description:
 *   Deletes an extent record from a fork, reducing its physical length.
 * Input Variable(s):
 *   struct hfs_fork *fork: the fork
 *   struct hfs_extent *ext: the current last extent for 'fork'
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'fork' points to a valid (struct hfs_fork)
 *   'ext' point to a valid (struct hfs_extent) which is the last in 'fork'
 *    and which is not also the first extent in 'fork'.
 * Postconditions:
 *   The extent record has been removed if possible, and a warning has been
 *   printed otherwise.
 */
static void delete_extent(struct hfs_fork *fork, struct hfs_extent *ext)
{
	struct hfs_mdb *mdb = fork->entry->mdb;
	struct hfs_ext_key key;
	int error;

	if (fork->cache == ext) {
		set_cache(fork, ext->prev);
	}
	ext->prev->next = NULL;
	if (ext->count != 1) {
		hfs_warn("hfs_truncate: extent has count %d.\n", ext->count);
	}

	lock_bitmap(mdb);
	error = hfs_clear_vbm_bits(mdb, ext->block[2], ext->length[2]);
	if (error) {
		hfs_warn("hfs_truncate: error %d freeing blocks.\n", error);
	}
	error = hfs_clear_vbm_bits(mdb, ext->block[1], ext->length[1]);
	if (error) {
		hfs_warn("hfs_truncate: error %d freeing blocks.\n", error);
	}
	error = hfs_clear_vbm_bits(mdb, ext->block[0], ext->length[0]);
	if (error) {
		hfs_warn("hfs_truncate: error %d freeing blocks.\n", error);
	}
	unlock_bitmap(mdb);

	build_key(&key, fork, ext->start);

	error = hfs_bdelete(mdb->ext_tree, HFS_BKEY(&key));
	if (error) {
		hfs_warn("hfs_truncate: error %d deleting an extent.\n", error);
	}

	HFS_DELETE(ext);
}

/*
 * new_extent()
 *
 * Description:
 *   Adds a new extent record to a fork, extending its physical length.
 * Input Variable(s):
 *   struct hfs_fork *fork: the fork to extend
 *   struct hfs_extent *ext: the current last extent for 'fork'
 *   hfs_u16 ablock: the number of allocation blocks in 'fork'.
 *   hfs_u16 start: first allocation block to add to 'fork'.
 *   hfs_u16 len: the number of allocation blocks to add to 'fork'.
 *   hfs_u32 ablksz: number of sectors in an allocation block.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   (struct hfs_extent *) the new extent or NULL
 * Preconditions:
 *   'fork' points to a valid (struct hfs_fork)
 *   'ext' point to a valid (struct hfs_extent) which is the last in 'fork'
 *   'ablock', 'start', 'len' and 'ablksz' are what they claim to be.
 * Postconditions:
 *   If NULL is returned then no changes have been made to 'fork'.
 *   If the return value is non-NULL that it is the extent that has been
 *   added to 'fork' both in memory and on disk.  The 'psize' field of
 *   'fork' has been updated to reflect the new physical size.
 */
static struct hfs_extent *new_extent(struct hfs_fork *fork,
				     struct hfs_extent *ext,
				     hfs_u16 ablock, hfs_u16 start,
				     hfs_u16 len, hfs_u16 ablksz)
{
	struct hfs_raw_extent raw;
	struct hfs_ext_key key;
	int error;

	if (fork->entry->cnid == htonl(HFS_EXT_CNID)) {
		/* Limit extents tree to the record in the MDB */
		return NULL;
	}

	if (!HFS_NEW(ext->next)) {
		return NULL;
	}
	ext->next->prev = ext;
	ext->next->next = NULL;
	ext = ext->next;
	relse_ext(ext->prev);

	ext->start = ablock;
	ext->block[0] = start;
	ext->length[0] = len;
	ext->block[1] = 0;
	ext->length[1] = 0;
	ext->block[2] = 0;
	ext->length[2] = 0;
	ext->end = ablock + len - 1;
	ext->count = 1;

	write_extent(&raw, ext);
	
	build_key(&key, fork, ablock);

	error = hfs_binsert(fork->entry->mdb->ext_tree, 
			    HFS_BKEY(&key), &raw, sizeof(raw));
	if (error) {
		ext->prev->next = NULL;
		HFS_DELETE(ext);
		return NULL;
	}
	set_cache(fork, ext);
	return ext;
}

/*
 * update_ext()
 *
 * Given a (struct hfs_fork) write an extent record back to disk.
 */
static void update_ext(struct hfs_fork *fork, struct hfs_extent *ext)
{
	struct hfs_ext_key target;
	struct hfs_brec brec;

	if (ext->start) {
		build_key(&target, fork, ext->start);

		if (!hfs_bfind(&brec, fork->entry->mdb->ext_tree,
			       HFS_BKEY(&target), HFS_BFIND_WRITE)) {
			write_extent(brec.data, ext);
			hfs_brec_relse(&brec, NULL);
		}
	}
}

/*
 * zero_blocks()
 * 
 * Zeros-out 'num' allocation blocks beginning with 'start'.
 */
static int zero_blocks(struct hfs_mdb *mdb, int start, int num) {
	hfs_buffer buf;
	int end;
	int j;

	start = mdb->fs_start + start * mdb->alloc_blksz;
	end = start + num * mdb->alloc_blksz;

	for (j=start; j<end; ++j) {
		if (hfs_buffer_ok(buf = hfs_buffer_get(mdb->sys_mdb, j, 0))) {
			memset(hfs_buffer_data(buf), 0, HFS_SECTOR_SIZE);
			hfs_buffer_dirty(buf);
			hfs_buffer_put(buf);
		}
	}
	return 0;
}

/*
 * shrink_fork()
 *
 * Try to remove enough allocation blocks from 'fork'
 * so that it is 'ablocks' allocation blocks long. 
 */
static void shrink_fork(struct hfs_fork *fork, int ablocks)
{
	struct hfs_mdb *mdb = fork->entry->mdb;
	struct hfs_extent *ext;
	int i, error, next, count;
	hfs_u32 ablksz = mdb->alloc_blksz;

	next =  (fork->psize / ablksz) - 1;
	ext = find_ext(fork, next);
	while (ext && ext->start && (ext->start >= ablocks)) {
		next = ext->start - 1;
		delete_extent(fork, ext);
		ext = find_ext(fork, next);
	}
	if (!ext) {
		fork->psize = (next + 1) * ablksz;
		return;
	}

	if ((count = next + 1 - ablocks) > 0) {
		for (i=2; (i>=0) && !ext->length[i]; --i) {};
		lock_bitmap(mdb);
		while (count && (ext->length[i] <= count)) {
			ext->end -= ext->length[i];
			count -= ext->length[i];
			error = hfs_clear_vbm_bits(mdb, ext->block[i],
						   ext->length[i]);
			if (error) {
				hfs_warn("hfs_truncate: error %d freeing "
				       "blocks.\n", error);
			}
			ext->block[i] = ext->length[i] = 0;
			--i;
		}
		if (count) {
			ext->end -= count;
			ext->length[i] -= count;
			error = hfs_clear_vbm_bits(mdb, ext->block[i] +
						       ext->length[i], count);
			if (error) {
				hfs_warn("hfs_truncate: error %d freeing "
				       "blocks.\n", error);
			}
		}
		unlock_bitmap(mdb);
		update_ext(fork, ext);
	}

	fork->psize = ablocks * ablksz;
}

/*
 * grow_fork()
 *
 * Try to add enough allocation blocks to 'fork'
 * so that it is 'ablock' allocation blocks long. 
 */
static int grow_fork(struct hfs_fork *fork, int ablocks)
{
	struct hfs_cat_entry *entry = fork->entry;
	struct hfs_mdb *mdb = entry->mdb;
	struct hfs_extent *ext;
	int i, start, err;
	hfs_u16 need, len=0;
	hfs_u32 ablksz = mdb->alloc_blksz;
	hfs_u32 blocks, clumpablks;

	blocks = fork->psize;
	need = ablocks - blocks/ablksz;
	if (need < 1) { /* no need to grow the fork */
		return 0;
	}

	/* round up to clumpsize */
	if (entry->u.file.clumpablks) {
		clumpablks = entry->u.file.clumpablks;
	} else {
		clumpablks = mdb->clumpablks;
	}
	need = ((need + clumpablks - 1) / clumpablks) * clumpablks;

	/* find last extent record and try to extend it */
	if (!(ext = find_ext(fork, blocks/ablksz - 1))) {
		/* somehow we couldn't find the end of the file! */
		return -1;
	}

	/* determine which is the last used extent in the record */
	/* then try to allocate the blocks immediately following it */
	for (i=2; (i>=0) && !ext->length[i]; --i) {};
	if (i>=0) {
		/* try to extend the last extent */
		start = ext->block[i] + ext->length[i];

		err = 0;
		lock_bitmap(mdb);
		len = hfs_vbm_count_free(mdb, start);
		if (!len) {
			unlock_bitmap(mdb);
			goto more_extents;
		}
		if (need < len) {
			len = need;
		}
		err = hfs_set_vbm_bits(mdb, start, len);
		unlock_bitmap(mdb);
		if (err) {
			relse_ext(ext);
			return -1;
		}
	
		zero_blocks(mdb, start, len);
	
		ext->length[i] += len;
		ext->end += len;
		blocks = (fork->psize += len * ablksz);
		need -= len;
		update_ext(fork, ext);
	}

more_extents:
	/* add some more extents */
	while (need) {
		len = need;
		err = 0;
		lock_bitmap(mdb);
		start = hfs_vbm_search_free(mdb, &len);
		if (need < len) {
			len = need;
		}
		err = hfs_set_vbm_bits(mdb, start, len);
		unlock_bitmap(mdb);
		if (!len || err) {
			relse_ext(ext);
			return -1;
		}
		zero_blocks(mdb, start, len);

		/* determine which is the first free extent in the record */
		for (i=0; (i<3) && ext->length[i]; ++i) {};
		if (i < 3) {
			ext->block[i] = start;
			ext->length[i] = len;
			ext->end += len;
			update_ext(fork, ext);
		} else {
			if (!(ext = new_extent(fork, ext, blocks/ablksz,
					       start, len, ablksz))) {
				lock_bitmap(mdb);
				hfs_clear_vbm_bits(mdb, start, len);
				unlock_bitmap(mdb);
				return -1;
			}
		}
		blocks = (fork->psize += len * ablksz);
		need -= len;
	}
	set_cache(fork, ext);
	relse_ext(ext);
	return 0;
}

/*================ Global functions ================*/

/*
 * hfs_ext_compare()
 *
 * Description:
 *   This is the comparison function used for the extents B-tree.  In
 *   comparing extent B-tree entries, the file id is the most
 *   significant field (compared as unsigned ints); the fork type is
 *   the second most significant field (compared as unsigned chars);
 *   and the allocation block number field is the least significant
 *   (compared as unsigned ints).
 * Input Variable(s):
 *   struct hfs_ext_key *key1: pointer to the first key to compare
 *   struct hfs_ext_key *key2: pointer to the second key to compare
 * Output Variable(s):
 *   NONE
 * Returns:
 *   int: negative if key1<key2, positive if key1>key2, and 0 if key1==key2
 * Preconditions:
 *   key1 and key2 point to "valid" (struct hfs_ext_key)s.
 * Postconditions:
 *   This function has no side-effects */
int hfs_ext_compare(const struct hfs_ext_key *key1,
		    const struct hfs_ext_key *key2)
{
	unsigned int tmp;
	int retval;

	tmp = hfs_get_hl(key1->FNum) - hfs_get_hl(key2->FNum);
	if (tmp != 0) {
		retval = (int)tmp;
	} else {
		tmp = (unsigned char)key1->FkType - (unsigned char)key2->FkType;
		if (tmp != 0) {
			retval = (int)tmp;
		} else {
			retval = (int)(hfs_get_hs(key1->FABN)
				       - hfs_get_hs(key2->FABN));
		}
	}
	return retval;
}

/*
 * hfs_extent_adj()
 *
 * Given an hfs_fork shrink or grow the fork to hold the
 * forks logical size.
 */
void hfs_extent_adj(struct hfs_fork *fork)
{
	if (fork) {
		hfs_u32 blks, ablocks, ablksz;

		if (fork->lsize > HFS_FORK_MAX) {
			fork->lsize = HFS_FORK_MAX;
		}

		blks = (fork->lsize+HFS_SECTOR_SIZE-1) >> HFS_SECTOR_SIZE_BITS;
		ablksz = fork->entry->mdb->alloc_blksz;
		ablocks = (blks + ablksz - 1) / ablksz;

		if (blks > fork->psize) {
			grow_fork(fork, ablocks);
			if (blks > fork->psize) {
				fork->lsize =
					fork->psize >> HFS_SECTOR_SIZE_BITS;
			}
		} else if (blks < fork->psize) {
			shrink_fork(fork, ablocks);
		}
	}
}

/*
 * hfs_extent_map()
 *
 * Given an hfs_fork and a block number within the fork, return the
 * number of the corresponding physical block on disk, or zero on
 * error.
 */
int hfs_extent_map(struct hfs_fork *fork, int block, int create) 
{
	int ablksz, ablock, offset, tmp;
	struct hfs_extent *ext;

	if (!fork || !fork->entry || !fork->entry->mdb) {
		return 0;
	}

#if defined(DEBUG_EXTENTS) || defined(DEBUG_ALL)
	hfs_warn("hfs_extent_map: ablock %d of file %d, fork %d\n",
		 block, fork->entry->cnid, fork->fork);
#endif

	if (block < 0) {
		hfs_warn("hfs_extent_map: block < 0\n");
		return 0;
	}
	if (block > (HFS_FORK_MAX >> HFS_SECTOR_SIZE_BITS)) {
		hfs_warn("hfs_extent_map: block(0x%08x) > big; cnid=%d "
			 "fork=%d\n", block, fork->entry->cnid, fork->fork);
		return 0;
	}
	ablksz = fork->entry->mdb->alloc_blksz;
	offset = fork->entry->mdb->fs_start + (block % ablksz);
	ablock = block / ablksz;
	
	if (block >= fork->psize) {
		if (!create || (grow_fork(fork, ablock + 1) < 0))
			return 0;
	}

#if defined(DEBUG_EXTENTS) || defined(DEBUG_ALL)
	hfs_warn("(lblock %d offset %d)\n", ablock, offset);
#endif

	if ((ext = find_ext(fork, ablock))) {
		dump_ext("trying new: ", ext);
		tmp = decode_extent(ext, ablock);
		relse_ext(ext);
		if (tmp >= 0) {
			return tmp*ablksz + offset;
		}
	} 

	return 0;
}

/*
 * hfs_extent_out()
 *
 * Copy the first extent record from a (struct hfs_fork) to a (struct
 * raw_extent), record (normally the one in the catalog entry).
 */
void hfs_extent_out(const struct hfs_fork *fork, hfs_byte_t dummy[12])
{
	struct hfs_raw_extent *ext = (struct hfs_raw_extent *)dummy;

	if (fork && ext) {
		write_extent(ext, &fork->first);
		dump_ext("extent out: ", &fork->first);
	}
}

/*
 * hfs_extent_in()
 *
 * Copy an raw_extent to the 'first' and 'cache' fields of an hfs_fork.
 */
void hfs_extent_in(struct hfs_fork *fork, const hfs_byte_t dummy[12])
{
	const struct hfs_raw_extent *ext =
		(const struct hfs_raw_extent *)dummy;

	if (fork && ext) {
		read_extent(&fork->first, ext, 0);
		fork->cache = &fork->first;
		fork->first.count = 2;
		dump_ext("extent in: ", &fork->first);
	}
}

/* 
 * hfs_extent_free()
 *
 * Removes from memory all extents associated with 'fil'.
 */
void hfs_extent_free(struct hfs_fork *fork)
{
	if (fork) {
		set_cache(fork, &fork->first);

	        if (fork->first.next) {
		        hfs_warn("hfs_extent_free: extents in use!\n");
		}
	}
}
