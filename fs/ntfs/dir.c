/*
 * dir.c
 *
 * Copyright (C) 1995-1997, 1999 Martin von Löwis
 * Copyright (C) 1999 Steve Dodd
 * Copyright (C) 1999 Joseph Malicki
 * Copyright (C) 2001 Anton Altaparmakov (AIA)
 */

#include "ntfstypes.h"
#include "struct.h"
#include "dir.h"
#include "macros.h"

#include <linux/errno.h>
#include "super.h"
#include "inode.h"
#include "attr.h"
#include "support.h"
#include "util.h"
#include <linux/smp_lock.h>
#include <linux/bitops.h>

static char I30[] = "$I30";

/* An index record should start with INDX, and the last word in each block
 * should contain the check value. If it passes, the original values need to
 * be restored. */
int ntfs_check_index_record(ntfs_inode *ino, char *record)
{
	return ntfs_fixup_record(record, "INDX", ino->u.index.recordsize);
}

static inline int ntfs_is_top(ntfs_u64 stack)
{
	return stack == 14;
}

static int ntfs_pop(ntfs_u64 *stack)
{
	static int width[16] = {1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,-1};
	int res = -1;
	
	switch (width[*stack & 15]) {
	case 1:
		res = (int)((*stack & 15) >> 1);
		*stack >>= 4;
		break;
	case 2:
		res = (int)(((*stack & 63) >> 2) + 7);
		*stack >>= 6;
		break;
	case 3:
		res = (int)(((*stack & 255) >> 3) + 23);
		*stack >>= 8;
		break;
	case 4:
		res = (int)(((*stack & 1023) >> 4) + 55);
		*stack >>= 10;
		break;
	default:
		ntfs_error("Unknown encoding\n");
	}
	return res;
}

static inline unsigned int ntfs_top(void)
{
	return 14;
}

static ntfs_u64 ntfs_push(ntfs_u64 stack, int i)
{
	if (i < 7)
		return (stack << 4) | (i << 1);
	if (i < 23)
		return (stack << 6) | ((i - 7) << 2) | 1;
	if (i < 55)
		return (stack << 8) | ((i - 23) << 3) | 3;
	if (i < 120)
		return (stack << 10) | ((i - 55) << 4) | 7;
	ntfs_error("Too many entries\n");
	return ~((ntfs_u64)0);
}

#if 0
static void ntfs_display_stack(ntfs_u64 stack)
{
	while(!ntfs_is_top(stack))
	{
		printf("%d ", ntfs_pop(&stack));
	}
	printf("\n");
}
#endif

/* True if the entry points to another block of entries. */
static inline int ntfs_entry_has_subnodes(char *entry)
{
	return (NTFS_GETU16(entry + 0xc) & 1);
}

/* True if it is not the 'end of dir' entry. */
static inline int ntfs_entry_is_used(char *entry)
{
	return !(NTFS_GETU16(entry + 0xc) & 2);
}

/*
 * Removed RACE for allocating index blocks. But stil not too happy.
 * There might be more races afterwards. (AIA)
 */
static int ntfs_allocate_index_block(ntfs_iterate_s *walk)
{
	ntfs_attribute *allocation, *bitmap = 0;
	int error, size, i, bit;
	ntfs_u8 *bmap;
	ntfs_io io;
	ntfs_volume *vol = walk->dir->vol;

	/* Check for allocation attribute. */
	allocation = ntfs_find_attr(walk->dir, vol->at_index_allocation, I30);
	if (!allocation) {
		ntfs_u8 bmp[8];
		/* Create index allocation attribute. */
		error = ntfs_create_attr(walk->dir, vol->at_index_allocation,
					 I30, 0, 0, &allocation);
		if (error)
			goto err_ret;
		ntfs_bzero(bmp, sizeof(bmp));
		error = ntfs_create_attr(walk->dir, vol->at_bitmap, I30, bmp,
					 sizeof(bmp), &bitmap);
		if (error)
			goto err_ret;
	} else
		bitmap = ntfs_find_attr(walk->dir, vol->at_bitmap, I30);
	if (!bitmap) {
		ntfs_error("Directory w/o bitmap\n");
		error = -EINVAL;
		goto err_ret;
	}
	size = bitmap->size;
	bmap = ntfs_malloc(size);
	if (!bmap) {
		error = -ENOMEM;
		goto err_ret;
	}
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
try_again:
	io.param = bmap;
	io.size = size;
	error = ntfs_read_attr(walk->dir, vol->at_bitmap, I30, 0, &io);
	if (error || (io.size != size && (error = -EIO, 1)))
		goto err_fb_out;
	/* Allocate a bit. */
	for (bit = i = 0; i < size; i++) {
		if (bmap[i] == 0xFF)
			continue;
		bit = ffz(bmap[i]);
		if (bit < 8)
			break;
	}
	if (i >= size) {
		/* FIXME: Extend bitmap. */
		error = -EOPNOTSUPP;
		goto err_fb_out;
	}
	/* Get the byte containing our bit again, now taking the BKL. */
	io.param = bmap;
	io.size = 1;
	lock_kernel();
	error = ntfs_read_attr(walk->dir, vol->at_bitmap, I30, i, &io);
	if (error || (io.size != 1 && (error = -EIO, 1)))
		goto err_unl_out;
	if (ntfs_test_and_set_bit(bmap, bit)) {
		unlock_kernel();
		/* Give other process(es) a chance to finish. */
		schedule();
		goto try_again;
	}
	walk->newblock = (i * 8 + bit) * walk->dir->u.index.clusters_per_record;
	io.param = bmap;
	error = ntfs_write_attr(walk->dir, vol->at_bitmap, I30, i, &io);
	if (error || (io.size != size && (error = -EIO, 1)))
		goto err_unl_out;
	/* Change inode on disk, required when bitmap is resident. */
	error = ntfs_update_inode(walk->dir);
	if (error)
		goto err_unl_out;
	unlock_kernel();
	ntfs_free(bmap);
	/* Check whether record is out of allocated range. */
	size = allocation->size;
	if (walk->newblock * vol->cluster_size >= size) {
		/* Build index record. */
		int hsize;
		int s1 = walk->dir->u.index.recordsize;
		int nr_fix = (s1 >> vol->sector_size) + 1;
		char *record = ntfs_malloc(s1);
		if (!record) {
			error = -ENOMEM;
			goto err_ret;
		}
		ntfs_bzero(record, s1);
		/* Magic */
		ntfs_memcpy(record, "INDX", 4);
		/* Offset to fixups */
		NTFS_PUTU16(record + 4, 0x28);
		/* Number of fixups. */
		NTFS_PUTU16(record + 6, nr_fix);
		/* Log file sequence number - We don't do journalling so we
		 * just set it to zero which should be the Right Thing. (AIA) */
		NTFS_PUTU64(record + 8, 0);
		/* VCN of buffer */
		NTFS_PUTU64(record + 0x10, walk->newblock);
		/* Header size. */
		hsize = 0x10 + 2 * nr_fix;
		hsize = (hsize + 7) & ~7; /* Align. */
		NTFS_PUTU16(record + 0x18, hsize);
		/* Total size of record. */
		NTFS_PUTU32(record + 0x20, s1 - 0x18);
		/* Writing the data will extend the attribute. */
		io.param = record;
		io.size = s1;
		io.do_read = 0;
		error = ntfs_readwrite_attr(walk->dir, allocation, size, &io);
		ntfs_free(record);
		if (error || (io.size != s1 && (error = -EIO, 1)))
			goto err_ret;
		error = ntfs_update_inode(walk->dir);
		if (error)
			goto err_ret;
	}
	return 0;
err_unl_out:
	unlock_kernel();
err_fb_out:
	ntfs_free(bmap);
err_ret:
	return error;
}

/* Write an index block (root or allocation) back to storage.
 * Used is the total number of bytes in buf, including all headers. */
static int ntfs_index_writeback(ntfs_iterate_s *walk, ntfs_u8 *buf, int block,
				int used)
{
	ntfs_io io;
	int error;
	ntfs_attribute *a;
	ntfs_volume *vol = walk->dir->vol;
	
	io.fn_put = 0;
	io.fn_get = ntfs_get;
	io.param = buf;
	if (block == -1) {	/* Index root. */
		NTFS_PUTU16(buf + 0x14, used - 0x10);
		/* 0x18 is a copy thereof. */
		NTFS_PUTU16(buf + 0x18, used - 0x10);
		io.size = used;
		error = ntfs_write_attr(walk->dir, vol->at_index_root, I30, 0,
					&io);
		if (error || (io.size != used && (error = -EIO, 1)))
			return error;
		/* Shrink if necessary. */
		a = ntfs_find_attr(walk->dir, vol->at_index_root, I30);
		ntfs_resize_attr(walk->dir, a, used);
	} else {
		NTFS_PUTU16(buf + 0x1C, used - 0x18);
		io.size = walk->dir->u.index.recordsize;
		error = ntfs_insert_fixups(buf, io.size);
		if (error) {
			printk(KERN_ALERT "NTFS: ntfs_index_writeback() caught "
					"corrupt index record ntfs record "
					"header. Refusing to write corrupt "
					"data to disk. Unmount and run chkdsk "
					"immediately!\n");
			return -EIO;
		}
		error = ntfs_write_attr(walk->dir, vol->at_index_allocation,
				I30, (__s64)block << vol->cluster_size_bits,
				&io);
		if (error || (io.size != walk->dir->u.index.recordsize &&
				(error = -EIO, 1)))
			return error;
	}
	return 0;
}

static int ntfs_split_record(ntfs_iterate_s *walk, char *start, int bsize,
			     int usize)
{
	char *entry, *prev;
	ntfs_u8 *newbuf = 0, *middle = 0;
	int error, othersize, mlen;
	ntfs_io io;
	ntfs_volume *vol = walk->dir->vol;
	int oldblock;

	error = ntfs_allocate_index_block(walk);
	if (error)
		return error;
	/* This should not happen. */
	if (walk->block == -1) {
		ntfs_error("Trying to split root");
		return -EOPNOTSUPP;
	}
	entry = start + NTFS_GETU16(start + 0x18) + 0x18; 
	for (prev = entry; entry - start < usize / 2; 
					       entry += NTFS_GETU16(entry + 8))
		prev = entry;
	newbuf = ntfs_malloc(vol->index_record_size);
	if (!newbuf)
		return -ENOMEM;
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	io.param = newbuf;
	io.size = vol->index_record_size;
	/* Read in old header. FIXME: Reading everything is overkill. */
	error = ntfs_read_attr(walk->dir, vol->at_index_allocation, I30,
			(__s64)walk->newblock << vol->cluster_size_bits, &io);
	if (error)
		goto out;
	if (io.size != vol->index_record_size) {
		error = -EIO;
		goto out;
	}
	/* FIXME: Adjust header. */
	/* Copy everything from entry to new block. */
	othersize = usize - (entry - start);
	ntfs_memcpy(newbuf + NTFS_GETU16(newbuf + 0x18) + 0x18, entry,
								    othersize);
	/* Copy flags. */
	NTFS_PUTU32(newbuf + 0x24, NTFS_GETU32(start + 0x24));
	error = ntfs_index_writeback(walk, newbuf, walk->newblock,
				othersize + NTFS_GETU16(newbuf + 0x18) + 0x18);
	if (error)
		goto out;
	/* Move prev to walk. */
	mlen = NTFS_GETU16(prev + 0x8);
	/* Remember old child node. */
	if (ntfs_entry_has_subnodes(prev))
		oldblock = NTFS_GETU32(prev + mlen - 8);
	else
		oldblock = -1;
	/* Allow for pointer to subnode. */
	middle = ntfs_malloc(ntfs_entry_has_subnodes(prev) ? mlen : mlen + 8);
	if (!middle){
		error = -ENOMEM;
		goto out;
	}
	ntfs_memcpy(middle, prev, mlen);
	/* Set has_subnodes flag. */
	NTFS_PUTU8(middle + 0xC, NTFS_GETU8(middle + 0xC) | 1);
	/* Middle entry points to block, parent entry will point to newblock. */
	NTFS_PUTU64(middle + mlen - 8, walk->block);
	if (walk->new_entry)
		ntfs_error("Entry not reset");
	walk->new_entry = middle;
	walk->u.flags |= ITERATE_SPLIT_DONE;
	/* Terminate old block. */
	othersize = usize - (prev-start);
	NTFS_PUTU64(prev, 0);
	if (oldblock == -1) {
		NTFS_PUTU32(prev + 8, 0x10);
		NTFS_PUTU32(prev + 0xC, 2);
		othersize += 0x10;
	} else {
		NTFS_PUTU32(prev + 8, 0x18);
		NTFS_PUTU32(prev + 0xC, 3);
		NTFS_PUTU64(prev + 0x10, oldblock);
		othersize += 0x18;
	}
	/* Write back original block. */
	error = ntfs_index_writeback(walk, start, walk->block, othersize);
 out:
	if (newbuf)
		ntfs_free(newbuf);
	if (middle)
		ntfs_free(middle);
	return error;
}

static int ntfs_dir_insert(ntfs_iterate_s *walk, char *start, char* entry)
{
	int blocksize, usedsize, error, offset;
	int do_split = 0;
	offset = entry - start;
	if (walk->block == -1) { /* index root */
		blocksize = walk->dir->vol->mft_record_size;
		usedsize = NTFS_GETU16(start + 0x14) + 0x10;
	} else {
		blocksize = walk->dir->u.index.recordsize;
		usedsize = NTFS_GETU16(start + 0x1C) + 0x18;
	}
	if (usedsize + walk->new_entry_size > blocksize) {
		char* s1 = ntfs_malloc(blocksize + walk->new_entry_size);
		if (!s1)
			return -ENOMEM;
		ntfs_memcpy(s1, start, usedsize);
		do_split = 1;
		/* Adjust entry to s1. */
		entry = s1 + (entry - start);
		start = s1;
	}
	ntfs_memmove(entry + walk->new_entry_size, entry, usedsize - offset);
	ntfs_memcpy(entry, walk->new_entry, walk->new_entry_size);
	usedsize += walk->new_entry_size;
	ntfs_free(walk->new_entry);
	walk->new_entry = 0;
	if (do_split) {
		error = ntfs_split_record(walk, start, blocksize, usedsize);
		ntfs_free(start);
	} else {
		error = ntfs_index_writeback(walk, start, walk->block,usedsize);
		if (error)
			return error;
	}
	return 0;
}

/* Try to split INDEX_ROOT attributes. Return -E2BIG if nothing changed. */
int ntfs_split_indexroot(ntfs_inode *ino)
{
	ntfs_attribute *ra;
	ntfs_u8 *root = 0, *index = 0;
	ntfs_io io;
	int error, off, i, bsize, isize;
	ntfs_iterate_s walk;

	ra = ntfs_find_attr(ino, ino->vol->at_index_root, I30);
	if (!ra)
		return -ENOTDIR;
	bsize = ino->vol->mft_record_size;
	root = ntfs_malloc(bsize);
	if (!root)
		return -E2BIG;
	io.fn_put = ntfs_put;
	io.param = root;
	io.size = bsize;
	error = ntfs_read_attr(ino, ino->vol->at_index_root, I30, 0, &io);
	if (error)
		goto out;
	off = 0x20;
	/* Count number of entries. */
	for (i = 0; ntfs_entry_is_used(root + off); i++)
		off += NTFS_GETU16(root + off + 8);
	if (i <= 2) {
		/* We don't split small index roots. */
		error = -E2BIG;
		goto out;
	}
	index = ntfs_malloc(ino->vol->index_record_size);
	if (!index) {
		error = -ENOMEM;
		goto out;
	}
	walk.dir = ino;
	walk.block = -1;
	walk.result = walk.new_entry = 0;
	walk.name = 0;
	error = ntfs_allocate_index_block(&walk);
	if (error)
		goto out;
	/* Write old root to new index block. */
	io.param = index;
	io.size = ino->vol->index_record_size;
	error = ntfs_read_attr(ino, ino->vol->at_index_allocation, I30,
		(__s64)walk.newblock << ino->vol->cluster_size_bits, &io);
	if (error)
		goto out;
	isize = NTFS_GETU16(root + 0x18) - 0x10;
	ntfs_memcpy(index + NTFS_GETU16(index + 0x18) + 0x18, root+0x20, isize);
	/* Copy flags. */
	NTFS_PUTU32(index + 0x24, NTFS_GETU32(root + 0x1C));
	error = ntfs_index_writeback(&walk, index, walk.newblock, 
				     isize + NTFS_GETU16(index + 0x18) + 0x18);
	if (error)
		goto out;
	/* Mark root as split. */
	NTFS_PUTU32(root + 0x1C, 1);
	/* Truncate index root. */
	NTFS_PUTU64(root + 0x20, 0);
	NTFS_PUTU32(root + 0x28, 0x18);
	NTFS_PUTU32(root + 0x2C, 3);
	NTFS_PUTU64(root + 0x30, walk.newblock);
	error = ntfs_index_writeback(&walk, root, -1, 0x38);
 out:
	ntfs_free(root);
	ntfs_free(index);
	return error;
}

/* The entry has been found. Copy the result in the caller's buffer */
static int ntfs_copyresult(char *dest, char *source)
{
	int length = NTFS_GETU16(source + 8);
	ntfs_memcpy(dest, source, length);
	return 1;
}

/* Use $UpCase some day. */
static inline unsigned short ntfs_my_toupper(ntfs_volume *vol, ntfs_u16 x)
{
	/* We should read any pending rest of $UpCase here. */
	if (x >= vol->upcase_length)
		return x;
	return vol->upcase[x];
}

/* Everything passed in walk and entry. */
static int ntfs_my_strcmp(ntfs_iterate_s *walk, const unsigned char *entry)
{
	int lu = *(entry + 0x50);
	int i;

	ntfs_u16* name = (ntfs_u16*)(entry + 0x52);
	ntfs_volume *vol = walk->dir->vol;
	for (i = 0; i < lu && i < walk->namelen; i++)
		if (ntfs_my_toupper(vol, NTFS_GETU16(name + i)) != 
			     ntfs_my_toupper(vol, NTFS_GETU16(walk->name + i)))
			break;
	if (i == lu && i == walk->namelen)
		return 0;
	if (i == lu)
		return 1;
	if (i == walk->namelen)
		return -1;
	if (ntfs_my_toupper(vol, NTFS_GETU16(name + i)) < 
			    ntfs_my_toupper(vol, NTFS_GETU16(walk->name + i)))
		return 1;
	return -1;
}

/* Necessary forward declaration. */
static int ntfs_getdir_iterate(ntfs_iterate_s *walk, char *start, char *entry);

/* Parse a block of entries. Load the block, fix it up, and iterate over the
 * entries. The block is given as virtual cluster number. */
static int ntfs_getdir_record(ntfs_iterate_s *walk, int block)
{
	int length = walk->dir->u.index.recordsize;
	char *record = (char*)ntfs_malloc(length);
	char *offset;
	int retval,error;
	int oldblock;
	ntfs_io io;

	if (!record)
		return -ENOMEM;
	io.fn_put = ntfs_put;
	io.param = record;
	io.size = length;
	/* Read the block from the index allocation attribute. */
	error = ntfs_read_attr(walk->dir, walk->dir->vol->at_index_allocation,
		I30, (__s64)block << walk->dir->vol->cluster_size_bits, &io);
	if (error || io.size != length) {
		ntfs_error("read failed\n");
		ntfs_free(record);
		return 0;
	}
	if (!ntfs_check_index_record(walk->dir, record)) {
		ntfs_error("%x is not an index record\n", block);
		ntfs_free(record);
		return 0;
	}
	offset = record + NTFS_GETU16(record + 0x18) + 0x18;
	oldblock = walk->block;
	walk->block = block;
	retval = ntfs_getdir_iterate(walk, record, offset);
	walk->block = oldblock;
	ntfs_free(record);
	return retval;
}

/* Go down to the next block of entries. These collate before the current
 * entry. */
static int ntfs_descend(ntfs_iterate_s *walk, ntfs_u8 *start, ntfs_u8 *entry)
{
	int length = NTFS_GETU16(entry + 8);
	int nextblock = NTFS_GETU32(entry + length - 8);
	int error;

	if (!ntfs_entry_has_subnodes(entry)) {
		ntfs_error("illegal ntfs_descend call\n");
		return 0;
	}
	error = ntfs_getdir_record(walk, nextblock);
	if (!error && walk->type == DIR_INSERT && 
	    (walk->u.flags & ITERATE_SPLIT_DONE)) {
		/* Split has occurred. Adjust entry, insert new_entry. */
		NTFS_PUTU32(entry + length - 8, walk->newblock);
		/* Reset flags, as the current block might be split again. */
		walk->u.flags &= ~ITERATE_SPLIT_DONE;
		error = ntfs_dir_insert(walk, start, entry);
	}
	return error;
}

static int ntfs_getdir_iterate_byposition(ntfs_iterate_s *walk, char* start,
					  char *entry)
{
	int retval = 0;
	int curpos = 0, destpos = 0;
	int length;
	if (walk->u.pos != 0) {
		if (ntfs_is_top(walk->u.pos))
			return 0;
		destpos = ntfs_pop(&walk->u.pos);
	}
	while (1) {
		if (walk->u.pos == 0) {
			if (ntfs_entry_has_subnodes(entry))
				ntfs_descend(walk, start, entry);
			else
				walk->u.pos = ntfs_top();
			if (ntfs_is_top(walk->u.pos) && 
			    !ntfs_entry_is_used(entry))
				return 1;
			walk->u.pos = ntfs_push(walk->u.pos, curpos);
			return 1;
		}
		if (curpos == destpos) {
			if (!ntfs_is_top(walk->u.pos) && 
			    ntfs_entry_has_subnodes(entry)) {
				retval = ntfs_descend(walk, start, entry);
				if (retval) {
					walk->u.pos = ntfs_push(walk->u.pos,
								curpos);
					return retval;
				}
				if (!ntfs_entry_is_used(entry))
					return 0;
				walk->u.pos = 0;
			}
			if (ntfs_entry_is_used(entry)) {
				retval = ntfs_copyresult(walk->result, entry);
				walk->u.pos = 0;
			} else {
				walk->u.pos = ntfs_top();
				return 0;
			}
		}
		curpos++;
		if (!ntfs_entry_is_used(entry))
			break;
		length = NTFS_GETU16(entry + 8);
		if (!length) {
			ntfs_error("infinite loop\n");
			break;
		}
		entry += length;
	}
	return -1;
}
	
/* Iterate over a list of entries, either from an index block, or from the
 * index root.
 * If searching BY_POSITION, pop the top index from the position. If the
 * position stack is empty then, return the item at the index and set the
 * position to the next entry. If the position stack is not empty, 
 * recursively proceed for subnodes. If the entry at the position is the
 * 'end of dir' entry, return 'not found' and the empty stack.
 * If searching BY_NAME, walk through the items until found or until
 * one item is collated after the requested item. In the former case, return
 * the result. In the latter case, recursively proceed to the subnodes.
 * If 'end of dir' is reached, the name is not in the directory */
static int ntfs_getdir_iterate(ntfs_iterate_s *walk, char *start, char *entry)
{
	int length;
	int cmp;

	if (walk->type == BY_POSITION)
		return ntfs_getdir_iterate_byposition(walk, start, entry);
	do {
		/* If the current entry is a real one, compare with the
		 * requested item. If the current entry is the last item, it
		 * is always larger than the requested item. */
		cmp = ntfs_entry_is_used(entry) ? 
						ntfs_my_strcmp(walk,entry) : -1;
		switch (walk->type) {
		case BY_NAME:
			switch (cmp) {
			case -1:
				return ntfs_entry_has_subnodes(entry) ?
					ntfs_descend(walk, start, entry) : 0;
			case  0:
				return ntfs_copyresult(walk->result, entry);
			case  1:
				break;
			}
			break;
		case DIR_INSERT:
			switch (cmp) {
			case -1:
				return ntfs_entry_has_subnodes(entry) ?
					ntfs_descend(walk, start, entry) :
					ntfs_dir_insert(walk, start, entry);
			case  0:
				return -EEXIST;
			case  1:
				break;
			}
			break;
		default:
			ntfs_error("TODO\n"); /* FIXME: ? */
		}
		if (!ntfs_entry_is_used(entry))
			break;
		length = NTFS_GETU16(entry + 8);
		if (!length) {
			ntfs_error("infinite loop\n");
			break;
		}
		entry += length;
	} while (1);
	return 0;
}

/*  Tree walking is done using position numbers. The following numbers have a
 *  special meaning:
 *       0   start (.)
 *      -1   no more entries
 *      -2   ..
 *  All other numbers encode sequences of indices. The sequence a, b, c is 
 *  encoded as <stop><c><b><a>, where <foo> is the encoding of foo. The
 *  first few integers are encoded as follows:
 *      0:    0000    1:    0010    2:    0100    3:    0110
 *      4:    1000    5:    1010    6:    1100 stop:    1110
 *      7:  000001    8:  000101    9:  001001   10:  001101
 *  The least significant bits give the width of this encoding, the other bits
 *  encode the value, starting from the first value of the interval.
 *   tag     width  first value  last value
 *   0       3      0            6
 *   01      4      7            22
 *   011     5      23           54
 *   0111    6      55           119
 *   More values are hopefully not needed, as the file position has currently
 *   64 bits in total. */

/* Find an entry in the directory. Return 0 if not found, otherwise copy the
 * entry to the result buffer. */
int ntfs_getdir(ntfs_iterate_s *walk)
{
	int length = walk->dir->vol->mft_record_size;
	int retval, error;
	/* Start at the index root. */
	char *root = ntfs_malloc(length);
	ntfs_io io;

	if (!root)
		return -ENOMEM;
	io.fn_put = ntfs_put;
	io.param = root;
	io.size = length;
	error = ntfs_read_attr(walk->dir, walk->dir->vol->at_index_root, I30,
			       0, &io);
	if (error) {
		ntfs_error("Not a directory\n");
		return 0;
	}
	walk->block = -1;
	/* FIXME: Move these to walk. */
	walk->dir->u.index.recordsize = NTFS_GETU32(root + 0x8);
	walk->dir->u.index.clusters_per_record = NTFS_GETU32(root + 0xC);
	/* FIXME: Consistency check. */
	/* Skip header. */
	retval = ntfs_getdir_iterate(walk, root, root + 0x20);
	ntfs_free(root);
	return retval;
}

/* Find an entry in the directory by its position stack. Iteration starts
 * if the stack is 0, in which case the position is set to the first item
 * in the directory. If the position is nonzero, return the item at the
 * position and change the position to the next item. The position is -1
 * if there are no more items. */
int ntfs_getdir_byposition(ntfs_iterate_s *walk)
{
	walk->type = BY_POSITION;
	return ntfs_getdir(walk);
}

/* Find an entry in the directory by its name. Return 0 if not found. */
int ntfs_getdir_byname(ntfs_iterate_s *walk)
{
	walk->type = BY_NAME;
	return ntfs_getdir(walk);
}

int ntfs_getdir_unsorted(ntfs_inode *ino, u32 *p_high, u32 *p_low,
		int (*cb)(ntfs_u8 *, void *), void *param)
{
	s64 ib_ofs;
	char *buf = 0, *entry = 0;
	ntfs_attribute *attr;
	ntfs_volume *vol;
	int byte, bit, err = 0;
	u32 start, finish, ibs, max_size;
	ntfs_io io;
	u8 ibs_bits;

	if (!ino) {
		ntfs_error("%s(): No inode! Returning -EINVAL.\n",__FUNCTION__);
		return -EINVAL;
	}
	vol = ino->vol;
	if (!vol) {
		ntfs_error("%s(): Inode 0x%lx has no volume. Returning "
			    "-EINVAL.\n", __FUNCTION__, ino->i_number);
		return -EINVAL;
	}
	ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 1: Entering for inode 0x%lx, "
			"p_high = 0x%x, p_low = 0x%x.\n", __FUNCTION__,
			ino->i_number, *p_high, *p_low);
	if (!*p_high) {
		/* We are still in the index root. */
		buf = ntfs_malloc(io.size = vol->mft_record_size);
		if (!buf)
			return -ENOMEM;
		io.fn_put = ntfs_put;
		io.param = buf;
		err = ntfs_read_attr(ino, vol->at_index_root, I30, 0, &io);
		if (err || !io.size)
			goto read_err_ret;
		ino->u.index.recordsize = ibs = NTFS_GETU32(buf + 0x8);
		ino->u.index.clusters_per_record = NTFS_GETU32(buf + 0xC);
		entry = buf + 0x20;
		ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 2: In index root.\n",
				__FUNCTION__);
		ibs_bits = ffs(ibs) - 1;
		/* Compensate for faked "." and "..". */
		start = 2;
	} else { /* We are in an index record. */
		io.size = ibs = ino->u.index.recordsize;
		buf = ntfs_malloc(ibs);
		if (!buf)
			return -ENOMEM;
		ibs_bits = ffs(ibs) - 1;
		io.fn_put = ntfs_put;
		io.param = buf;
		/*
		 * 0 is index root, index allocation starts at 1 and works in
		 * units of index block size (ibs).
		 */
		ib_ofs = (s64)(*p_high - 1) << ibs_bits;
		err = ntfs_read_attr(ino, vol->at_index_allocation, I30, ib_ofs,
				&io);
		if (err || io.size != ibs)
			goto read_err_ret;
		if (!ntfs_check_index_record(ino, buf)) {
			ntfs_error("%s(): Index block 0x%x is not an index "
					"record. Returning -ENOTDIR.\n",
					 __FUNCTION__, *p_high - 1);
			ntfs_free(buf);
			return -ENOTDIR;
		}
		entry = buf + 0x18 + NTFS_GETU16(buf + 0x18);
		ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 3: In index "
				"allocation.\n", __FUNCTION__);
		start = 0;
	}
	/* Process the entries. */
	finish = *p_low;
	for (; entry < (buf + ibs) && ntfs_entry_is_used(entry);
			entry += NTFS_GETU16(entry + 8)) {
		if (start < finish) {
			/* Skip entries that were already processed. */
			ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 4: Skipping "
					"already processed entry p_high 0x%x, "
					"p_low 0x%x.\n", __FUNCTION__, *p_high,
					start);
			start++;
			continue;
		}
		ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 5: Processing entry "
				"p_high 0x%x, p_low 0x%x.\n", __FUNCTION__,
				*p_high, *p_low);
		if ((err = cb(entry, param))) {
			/* filldir signalled us to stop. */
			ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 6: cb returned "
					"%i, returning 0, p_high 0x%x, "
					"p_low 0x%x.\n", __FUNCTION__, err,
					*p_high, *p_low);
			ntfs_free(buf);
			return 0;
		}
		++*p_low;
	}
	ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 7: After processing entries, "
			"p_high 0x%x, p_low 0x%x.\n", __FUNCTION__, *p_high,
			*p_low);
	/* We have to locate the next record. */
	ntfs_free(buf);
	buf = 0;
	*p_low = 0;
	attr = ntfs_find_attr(ino, vol->at_bitmap, I30);
	if (!attr) {
		/* Directory does not have index bitmap and index allocation. */
		*p_high = 0x7fff;
		ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 8: No index allocation. "
				"Returning 0, p_high 0x7fff, p_low 0x0.\n",
				__FUNCTION__);
		return 0;
	}
	max_size = attr->size;
	if (max_size > 0x7fff >> 3) {
		ntfs_error("%s(): Directory too large. Visible "
				"length is truncated.\n", __FUNCTION__);
		max_size = 0x7fff >> 3;
	}
	buf = ntfs_malloc(max_size);
	if (!buf)
		return -ENOMEM;
	io.param = buf;
	io.size = max_size;
	err = ntfs_read_attr(ino, vol->at_bitmap, I30, 0, &io);
	if (err || io.size != max_size)
		goto read_err_ret;
	attr = ntfs_find_attr(ino, vol->at_index_allocation, I30);
	if (!attr) {
		ntfs_free(buf);
		ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 9: Find attr failed. "
				"Returning -EIO.\n", __FUNCTION__);
		return -EIO;
	}
	if (attr->resident) {
		ntfs_free(buf);
		ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 9.5: IA is resident. Not"
				" allowed. Returning EINVAL.\n", __FUNCTION__);
		return -EINVAL;
	}
	/* Loop while going through non-allocated index records. */
	max_size <<= 3;
	while (1) {
		if (++*p_high >= 0x7fff) {
			ntfs_error("%s(): Unsorted 10: Directory "
					"inode 0x%lx overflowed the maximum "
					"number of index allocation buffers "
					"the driver can cope with. Pretending "
					"to be at end of directory.\n",
					__FUNCTION__, ino->i_number);
			goto fake_eod;
		}
		if (*p_high > max_size || (s64)*p_high << ibs_bits >
				attr->initialized) {
fake_eod:
			/* No more index records. */
			*p_high = 0x7fff;
			*p_low = 0;
			ntfs_free(buf);
			ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 10.5: No more "
					"index records. Returning 0, p_high "
					"0x7fff, p_low 0.\n", __FUNCTION__);
			return 0;
		}
		byte = (ntfs_cluster_t)(*p_high - 1);
		bit = 1 << (byte & 7);
		byte >>= 3;
		if ((buf[byte] & bit))
			break;
	};
	ntfs_debug(DEBUG_DIR3, "%s(): Unsorted 11: Done. Returning 0, p_high "
			"0x%x, p_low 0x%x.\n", __FUNCTION__, *p_high, *p_low);
	ntfs_free(buf);
	return 0;
read_err_ret:
	if (!err)
		err = -EIO;
	ntfs_error("%s(): Read failed. Returning error code %i.\n",
			__FUNCTION__, err);
	ntfs_free(buf);
	return err;
}

int ntfs_dir_add(ntfs_inode *dir, ntfs_inode *new, ntfs_attribute *name)
{
	ntfs_iterate_s walk;
	int nsize, esize;
	ntfs_u8* entry, *ndata;
	int error;

	walk.type = DIR_INSERT;
	walk.dir = dir;
	walk.u.flags = 0;
	nsize = name->size;
	ndata = name->d.data;
	walk.name = (ntfs_u16*)(ndata + 0x42);
	walk.namelen = NTFS_GETU8(ndata + 0x40);
	walk.new_entry_size = esize = (nsize + 0x10 + 7) & ~7;
	walk.new_entry = entry = ntfs_malloc(esize);
	if (!entry)
		return -ENOMEM;
	NTFS_PUTINUM(entry, new);
	NTFS_PUTU16(entry + 0x8, esize); /* Size of entry. */
	NTFS_PUTU16(entry + 0xA, nsize); /* Size of original name attribute. */
	NTFS_PUTU16(entry + 0xC, 0);     /* Flags. */
	NTFS_PUTU16(entry + 0xE, 0);	 /* Reserved. */
	ntfs_memcpy(entry + 0x10, ndata, nsize);
	ntfs_bzero(entry + 0x10 + nsize, esize - 0x10 - nsize);
	error = ntfs_getdir(&walk);
	if (walk.new_entry)
		ntfs_free(walk.new_entry);
	return error;
}

#if 0
int ntfs_dir_add1(ntfs_inode *dir, const char* name, int namelen,
		  ntfs_inode *ino)
{
	ntfs_iterate_s walk;
	int error;
	int nsize;
	char *entry;
	ntfs_attribute *name_attr;
	error = ntfs_decodeuni(dir->vol, name, namelen, &walk.name,
			       &walk.namelen);
	if (error)
		return error;
	/* FIXME: Set flags. */
	walk.type = DIR_INSERT;
	walk.dir = dir;
	/* walk.new = ino; */
	/* Prepare new entry. */
	/* Round up to a multiple of 8. */
	walk.new_entry_size = nsize = ((0x52 + 2 * walk.namelen + 7) / 8) * 8;
	walk.new_entry = entry = ntfs_malloc(nsize);
	if (!entry)
		return -ENOMEM;
	ntfs_bzero(entry, nsize);
	NTFS_PUTINUM(entry, ino);
	NTFS_PUTU16(entry + 8, nsize);
	NTFS_PUTU16(entry + 0xA, 0x42 + 2 * namelen); /* FIXME: Size of name 
						       * attribute. */
	NTFS_PUTU32(entry + 0xC, 0); /* FIXME: D-F? */
	name_attr = ntfs_find_attr(ino, vol->at_file_name, 0);
						    /* FIXME: multiple names */
	if (!name_attr || !name_attr->resident)
		return -EIDRM;
	/* Directory, file stamps, sizes, filename. */
	ntfs_memcpy(entry + 0x10, name_attr->d.data, 0x42 + 2 * namelen);
	error = ntfs_getdir(&walk);
	ntfs_free(walk.name);
	return error;
}
#endif

/* Fills out and creates an INDEX_ROOT attribute. */
int ntfs_add_index_root(ntfs_inode *ino, int type)
{
	ntfs_attribute *da;
	ntfs_u8 data[0x30]; /* 0x20 header, 0x10 last entry. */
	char name[10];

	NTFS_PUTU32(data, type);
	/* Collation rule. 1 == COLLATION_FILENAME */
	NTFS_PUTU32(data + 4, 1);
	NTFS_PUTU32(data + 8, ino->vol->index_record_size);
	NTFS_PUTU32(data + 0xC, ino->vol->index_clusters_per_record);
	/* Byte offset to first INDEX_ENTRY. */
	NTFS_PUTU32(data + 0x10, 0x10);
	/* Size of entries, including header. */
	NTFS_PUTU32(data + 0x14, 0x20);
	NTFS_PUTU32(data + 0x18, 0x20);
	/* No index allocation, yet. */
	NTFS_PUTU32(data + 0x1C, 0);
	/* Add last entry. */
	/* Indexed MFT record. */
	NTFS_PUTU64(data + 0x20, 0);
	/* Size of entry. */
	NTFS_PUTU32(data + 0x28, 0x10);
	/* Flags: Last entry, no child nodes. */
	NTFS_PUTU32(data + 0x2C, 2);
	/* Compute name. */
	ntfs_indexname(name, type);
	return ntfs_create_attr(ino, ino->vol->at_index_root, name,
				data, sizeof(data), &da);
}

int ntfs_mkdir(ntfs_inode *dir, const char *name, int namelen,
	       ntfs_inode *result)
{
	int error;
	
	error = ntfs_alloc_inode(dir, result, name, namelen, NTFS_AFLAG_DIR);
	if (error)
		goto out;
	error = ntfs_add_index_root(result, 0x30);
	if (error)
		goto out;
	/* Set directory bit. */
	result->attr[0x16] |= 2;
	error = ntfs_update_inode(dir);
	if (error)
		goto out;
	error = ntfs_update_inode(result);
	if (error)
		goto out;
 out:
	return error;
}

