/*
 * linux/fs/hfs/bitmap.c
 *
 * Copyright (C) 1996-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * Based on GPLed code Copyright (C) 1995  Michael Dreher
 *
 * This file contains the code to modify the volume bitmap:
 * search/set/clear bits.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs.h"

/*================ Global functions ================*/

/*
 * hfs_vbm_count_free()
 *
 * Description:
 *   Count the number of consecutive cleared bits in the bitmap blocks of
 *   the hfs MDB starting at bit number 'start'.  'mdb' had better
 *   be locked or the indicated number of blocks may be no longer free,
 *   when this functions returns!
 * Input Variable(s):
 *   struct hfs_mdb *mdb: Pointer to the hfs MDB
 *   hfs_u16 start: bit number to start at
 * Output Variable(s):
 *   NONE
 * Returns:
 *   The number of consecutive cleared bits starting at bit 'start'
 * Preconditions:
 *   'mdb' points to a "valid" (struct hfs_mdb).
 * Postconditions:
 *   NONE
 */
hfs_u16 hfs_vbm_count_free(const struct hfs_mdb *mdb, hfs_u16 start)
{
	hfs_u16 block_nr;	/* index of the current bitmap block */
	hfs_u16 bit_nr;		/* index of the current bit in block */
	hfs_u16 count;		/* number of bits found so far */
	hfs_u16 len;		/* number of bits found in this block */
	hfs_u16 max_block;	/* index of last bitmap block */
	hfs_u16 max_bits;	/* index of last bit in block */

	/* is this a valid HFS MDB? */
	if (!mdb) {
		return 0;
	}

	block_nr = start / HFS_BM_BPB;
	bit_nr	 = start % HFS_BM_BPB;
	max_block = (mdb->fs_ablocks + HFS_BM_BPB - 1) / HFS_BM_BPB - 1;

	count = 0;
	while (block_nr <= max_block) {
		if (block_nr != max_block) {
			max_bits = HFS_BM_BPB;
		} else {
			max_bits = mdb->fs_ablocks % HFS_BM_BPB;
		}

		len=hfs_count_zero_bits(hfs_buffer_data(mdb->bitmap[block_nr]),
					max_bits, bit_nr);
		count += len;

		/* see if we fell short of the end of this block */
		if ((len + bit_nr) < max_bits) {
			break;
		}

		++block_nr;
		bit_nr = 0;
	}
	return count;
}

/*
 * hfs_vbm_search_free()
 *
 * Description:
 *   Search for 'num_bits' consecutive cleared bits in the bitmap blocks of
 *   the hfs MDB. 'mdb' had better be locked or the returned range
 *   may be no longer free, when this functions returns!
 *   XXX Currently the search starts from bit 0, but it should start with
 *   the bit number stored in 's_alloc_ptr' of the MDB.
 * Input Variable(s):
 *   struct hfs_mdb *mdb: Pointer to the hfs MDB
 *   hfs_u16 *num_bits: Pointer to the number of cleared bits
 *     to search for
 * Output Variable(s):
 *   hfs_u16 *num_bits: The number of consecutive clear bits of the
 *     returned range. If the bitmap is fragmented, this will be less than
 *     requested and it will be zero, when the disk is full.
 * Returns:
 *   The number of the first bit of the range of cleared bits which has been
 *   found. When 'num_bits' is zero, this is invalid!
 * Preconditions:
 *   'mdb' points to a "valid" (struct hfs_mdb).
 *   'num_bits' points to a variable of type (hfs_u16), which contains
 *	the number of cleared bits to find.
 * Postconditions:
 *   'num_bits' is set to the length of the found sequence.
 */
hfs_u16 hfs_vbm_search_free(const struct hfs_mdb *mdb, hfs_u16 *num_bits)
{
	hfs_u16 block_nr; /* index of the current bitmap block */

	/* position and length of current portion of a run */
	hfs_u16 cur_pos, cur_len;

	/* position and length of current complete run */
	hfs_u16 pos=0, len=0;
	
	/* position and length of longest complete run */
	hfs_u16 longest_pos=0, longest_len=0;

	void *bitmap; /* contents of the current bitmap block */
	hfs_u16 max_block; /* upper limit of outer loop */
	hfs_u16 max_bits; /* upper limit of inner loop */

	/* is this a valid HFS MDB? */
	if (!mdb) {
		*num_bits = 0;
		hfs_warn("hfs_vbm_search_free: not a valid MDB\n");
		return 0;
	}
	
	/* make sure we have actual work to perform */
	if (!(*num_bits)) {
		return 0;
	}

	max_block = (mdb->fs_ablocks+HFS_BM_BPB-1) / HFS_BM_BPB - 1;
	
	/* search all bitmap blocks */
	for (block_nr = 0; block_nr <= max_block; block_nr++) {
		bitmap = hfs_buffer_data(mdb->bitmap[block_nr]);

		if (block_nr != max_block) {
			max_bits = HFS_BM_BPB;
		} else {
			max_bits = mdb->fs_ablocks % HFS_BM_BPB;
		}

		cur_pos = 0;
		do {
			cur_len = hfs_count_zero_bits(bitmap, max_bits,
						      cur_pos);
			len += cur_len;
			if (len > longest_len) {
				longest_pos = pos;
				longest_len = len;
				if (len >= *num_bits) {
					goto search_end;
				}
			}
			if ((cur_pos + cur_len) == max_bits) {
				break; /* zeros may continue into next block */
			}

			/* find start of next run of zeros */
			cur_pos = hfs_find_zero_bit(bitmap, max_bits,
						    cur_pos + cur_len);
			pos = cur_pos + HFS_BM_BPB*block_nr;
			len = 0;
		} while (cur_pos < max_bits);
	}

search_end:
	*num_bits = longest_len;
	return longest_pos;
}


/*
 * hfs_set_vbm_bits()
 *
 * Description:
 *   Set the requested bits in the volume bitmap of the hfs filesystem
 * Input Variable(s):
 *   struct hfs_mdb *mdb: Pointer to the hfs MDB
 *   hfs_u16 start: The offset of the first bit
 *   hfs_u16 count: The number of bits
 * Output Variable(s):
 *   None
 * Returns:
 *    0: no error
 *   -1: One of the bits was already set.  This is a strange
 *	 error and when it happens, the filesystem must be repaired!
 *   -2: One or more of the bits are out of range of the bitmap.
 *   -3: The 's_magic' field of the MDB does not match
 * Preconditions:
 *   'mdb' points to a "valid" (struct hfs_mdb).
 * Postconditions:
 *   Starting with bit number 'start', 'count' bits in the volume bitmap
 *   are set. The affected bitmap blocks are marked "dirty", the free
 *   block count of the MDB is updated and the MDB is marked dirty.
 */
int hfs_set_vbm_bits(struct hfs_mdb *mdb, hfs_u16 start, hfs_u16 count)
{
	hfs_u16 block_nr;	/* index of the current bitmap block */
	hfs_u16 u32_nr;		/* index of the current hfs_u32 in block */
	hfs_u16 bit_nr;		/* index of the current bit in hfs_u32 */
	hfs_u16 left = count;	/* number of bits left to be set */
	hfs_u32 *bitmap;	/* the current bitmap block's contents */

	/* is this a valid HFS MDB? */
	if (!mdb) {
		return -3;
	}

	/* is there any actual work to be done? */
	if (!count) {
		return 0;
	}

	/* are all of the bits in range? */
	if ((start + count) > mdb->fs_ablocks) {
		return -2;
	}

	block_nr = start / HFS_BM_BPB;
	u32_nr = (start % HFS_BM_BPB) / 32;
	bit_nr = start % 32;

	/* bitmap is always on a 32-bit boundary */
	bitmap = (hfs_u32 *)hfs_buffer_data(mdb->bitmap[block_nr]);

	/* do any partial hfs_u32 at the start */
	if (bit_nr != 0) {
		while ((bit_nr < 32) && left) {
			if (hfs_set_bit(bit_nr, bitmap + u32_nr)) {
				hfs_buffer_dirty(mdb->bitmap[block_nr]);
				return -1;
			}
			++bit_nr;
			--left;
		}
		bit_nr=0;

		/* advance u32_nr and check for end of this block */
		if (++u32_nr > 127) {
			u32_nr = 0;
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			++block_nr;
			/* bitmap is always on a 32-bit boundary */
			bitmap = (hfs_u32 *)
					hfs_buffer_data(mdb->bitmap[block_nr]);
		}
	}

	/* do full hfs_u32s */
	while (left > 31) {
		if (bitmap[u32_nr] != ((hfs_u32)0)) {
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			return -1;
		}
		bitmap[u32_nr] = ~((hfs_u32)0);
		left -= 32;

		/* advance u32_nr and check for end of this block */
		if (++u32_nr > 127) {
			u32_nr = 0;
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			++block_nr;
			/* bitmap is always on a 32-bit boundary */
			bitmap = (hfs_u32 *)
					hfs_buffer_data(mdb->bitmap[block_nr]);
		}
	}

			
	/* do any partial hfs_u32 at end */
	while (left) {
		if (hfs_set_bit(bit_nr, bitmap + u32_nr)) {
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			return -1;
		}
		++bit_nr;
		--left;
	}

	hfs_buffer_dirty(mdb->bitmap[block_nr]);
	mdb->free_ablocks -= count;

	/* successful completion */
	hfs_mdb_dirty(mdb->sys_mdb);
	return 0;
}

/*
 * hfs_clear_vbm_bits()
 *
 * Description:
 *   Clear the requested bits in the volume bitmap of the hfs filesystem
 * Input Variable(s):
 *   struct hfs_mdb *mdb: Pointer to the hfs MDB
 *   hfs_u16 start: The offset of the first bit
 *   hfs_u16 count: The number of bits
 * Output Variable(s):
 *   None
 * Returns:
 *    0: no error
 *   -1: One of the bits was already clear.  This is a strange
 *	 error and when it happens, the filesystem must be repaired!
 *   -2: One or more of the bits are out of range of the bitmap.
 *   -3: The 's_magic' field of the MDB does not match
 * Preconditions:
 *   'mdb' points to a "valid" (struct hfs_mdb).
 * Postconditions:
 *   Starting with bit number 'start', 'count' bits in the volume bitmap
 *   are cleared. The affected bitmap blocks are marked "dirty", the free
 *   block count of the MDB is updated and the MDB is marked dirty.
 */
int hfs_clear_vbm_bits(struct hfs_mdb *mdb, hfs_u16 start, hfs_u16 count)
{
	hfs_u16 block_nr;	/* index of the current bitmap block */
	hfs_u16 u32_nr;		/* index of the current hfs_u32 in block */
	hfs_u16 bit_nr;		/* index of the current bit in hfs_u32 */
	hfs_u16 left = count;	/* number of bits left to be set */
	hfs_u32 *bitmap;	/* the current bitmap block's contents */

	/* is this a valid HFS MDB? */
	if (!mdb) {
		return -3;
	}

	/* is there any actual work to be done? */
	if (!count) {
		return 0;
	}

	/* are all of the bits in range? */
	if ((start + count) > mdb->fs_ablocks) {
		return -2;
	}

	block_nr = start / HFS_BM_BPB;
	u32_nr = (start % HFS_BM_BPB) / 32;
	bit_nr = start % 32;

	/* bitmap is always on a 32-bit boundary */
	bitmap = (hfs_u32 *)hfs_buffer_data(mdb->bitmap[block_nr]);

	/* do any partial hfs_u32 at the start */
	if (bit_nr != 0) {
		while ((bit_nr < 32) && left) {
			if (!hfs_clear_bit(bit_nr, bitmap + u32_nr)) {
				hfs_buffer_dirty(mdb->bitmap[block_nr]);
				return -1;
			}
			++bit_nr;
			--left;
		}
		bit_nr=0;

		/* advance u32_nr and check for end of this block */
		if (++u32_nr > 127) {
			u32_nr = 0;
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			++block_nr;
			/* bitmap is always on a 32-bit boundary */
			bitmap = (hfs_u32 *)
					hfs_buffer_data(mdb->bitmap[block_nr]);
		}
	}

	/* do full hfs_u32s */
	while (left > 31) {
		if (bitmap[u32_nr] != ~((hfs_u32)0)) {
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			return -1;
		}
		bitmap[u32_nr] = ((hfs_u32)0);
		left -= 32;

		/* advance u32_nr and check for end of this block */
		if (++u32_nr > 127) {
			u32_nr = 0;
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			++block_nr;
			/* bitmap is always on a 32-bit boundary */
			bitmap = (hfs_u32 *)
					hfs_buffer_data(mdb->bitmap[block_nr]);
		}
	}

			
	/* do any partial hfs_u32 at end */
	while (left) {
		if (!hfs_clear_bit(bit_nr, bitmap + u32_nr)) {
			hfs_buffer_dirty(mdb->bitmap[block_nr]);
			return -1;
		}
		++bit_nr;
		--left;
	}

	hfs_buffer_dirty(mdb->bitmap[block_nr]);
	mdb->free_ablocks += count;

	/* successful completion */
	hfs_mdb_dirty(mdb->sys_mdb);
	return 0;
}
