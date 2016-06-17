/*
 * kernel/lvm-snap.c
 *
 * Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 *               2000 - 2002 Heinz Mauelshagen, Sistina Software
 *
 * LVM snapshot driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * LVM snapshot driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/*
 * Changelog
 *
 *    05/07/2000 - implemented persistent snapshot support
 *    23/11/2000 - used cpu_to_le64 rather than my own macro
 *    25/01/2001 - Put LockPage back in
 *    01/02/2001 - A dropped snapshot is now set as inactive
 *    14/02/2001 - tidied debug statements
 *    19/02/2001 - changed rawio calls to pass in preallocated buffer_heads
 *    26/02/2001 - introduced __brw_kiovec to remove a lot of conditional
 *                 compiles.
 *    07/03/2001 - fixed COW exception table not persistent on 2.2 (HM)
 *    12/03/2001 - lvm_pv_get_number changes:
 *                 o made it static
 *                 o renamed it to _pv_get_number
 *                 o pv number is returned in new uint * arg
 *                 o -1 returned on error
 *                 lvm_snapshot_fill_COW_table has a return value too.
 *    15/10/2001 - fix snapshot alignment problem [CM]
 *               - fix snapshot full oops (always check lv_block_exception) [CM]
 *    26/06/2002 - support for new list_move macro [patch@luckynet.dynu.com]
 *    26/07/2002 - removed conditional list_move macro because we will
 *                 discontinue LVM1 before 2.6 anyway
 *    27/08/2003 - fixed unsafe list handling in lvm_find_exception_table() [HM]
 *
 */

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/smp_lock.h>
#include <linux/types.h>
#include <linux/iobuf.h>
#include <linux/lvm.h>
#include <linux/devfs_fs_kernel.h>


#include "lvm-internal.h"

static char *lvm_snap_version __attribute__ ((unused)) =
    "LVM " LVM_RELEASE_NAME " snapshot code (" LVM_RELEASE_DATE ")\n";


extern const char *const lvm_name;
extern int lvm_blocksizes[];

void lvm_snapshot_release(lv_t *);

static int _write_COW_table_block(vg_t * vg, lv_t * lv, int idx,
				  const char **reason);
static void _disable_snapshot(vg_t * vg, lv_t * lv);


static inline int __brw_kiovec(int rw, int nr, struct kiobuf *iovec[],
			       kdev_t dev, unsigned long b[], int size,
			       lv_t * lv)
{
	return brw_kiovec(rw, nr, iovec, dev, b, size);
}


static int _pv_get_number(vg_t * vg, kdev_t rdev, uint * pvn)
{
	uint p;
	for (p = 0; p < vg->pv_max; p++) {
		if (vg->pv[p] == NULL)
			continue;

		if (vg->pv[p]->pv_dev == rdev)
			break;
	}

	if (p >= vg->pv_max) {
		/* bad news, the snapshot COW table is probably corrupt */
		printk(KERN_ERR
		       "%s -- _pv_get_number failed for rdev = %u\n",
		       lvm_name, rdev);
		return -1;
	}

	*pvn = vg->pv[p]->pv_number;
	return 0;
}


#define hashfn(dev,block,mask,chunk_size) \
	((HASHDEV(dev)^((block)/(chunk_size))) & (mask))

static inline lv_block_exception_t *lvm_find_exception_table(kdev_t
							     org_dev,
							     unsigned long
							     org_start,
							     lv_t * lv)
{
	struct list_head *hash_table = lv->lv_snapshot_hash_table, *next, *n;
	unsigned long mask = lv->lv_snapshot_hash_mask;
	int chunk_size = lv->lv_chunk_size;
	lv_block_exception_t *ret;
	int i = 0;

	hash_table =
	    &hash_table[hashfn(org_dev, org_start, mask, chunk_size)];
	ret = NULL;

	for (next = hash_table->next, n = next->next; next != hash_table;
             next = n, n = next->next) {
		lv_block_exception_t *exception;

		exception = list_entry(next, lv_block_exception_t, hash);
		if (exception->rsector_org == org_start &&
		    exception->rdev_org == org_dev) {
			if (i) {
				/* fun, isn't it? :) */
				list_del(next);
				list_add(next, hash_table);
			}
			ret = exception;
			break;
		}
		i++;
	}
	return ret;
}

inline void lvm_hash_link(lv_block_exception_t * exception,
			  kdev_t org_dev, unsigned long org_start,
			  lv_t * lv)
{
	struct list_head *hash_table = lv->lv_snapshot_hash_table;
	unsigned long mask = lv->lv_snapshot_hash_mask;
	int chunk_size = lv->lv_chunk_size;

	if (!hash_table)
		BUG();
	hash_table =
	    &hash_table[hashfn(org_dev, org_start, mask, chunk_size)];
	list_add(&exception->hash, hash_table);
}

/*
 * Determine if we already have a snapshot chunk for this block.
 * Return: 1 if it the chunk already exists
 *         0 if we need to COW this block and allocate a new chunk
 *        -1 if the snapshot was disabled because it ran out of space
 *
 * We need to be holding at least a read lock on lv->lv_lock.
 */
int lvm_snapshot_remap_block(kdev_t * org_dev, unsigned long *org_sector,
			     unsigned long pe_start, lv_t * lv)
{
	int ret;
	unsigned long pe_off, pe_adjustment, __org_start;
	kdev_t __org_dev;
	int chunk_size = lv->lv_chunk_size;
	lv_block_exception_t *exception;

	if (!lv->lv_block_exception)
		return -1;

	pe_off = pe_start % chunk_size;
	pe_adjustment = (*org_sector - pe_off) % chunk_size;
	__org_start = *org_sector - pe_adjustment;
	__org_dev = *org_dev;
	ret = 0;
	exception = lvm_find_exception_table(__org_dev, __org_start, lv);
	if (exception) {
		*org_dev = exception->rdev_new;
		*org_sector = exception->rsector_new + pe_adjustment;
		ret = 1;
	}
	return ret;
}

void lvm_drop_snapshot(vg_t * vg, lv_t * lv_snap, const char *reason)
{
	kdev_t last_dev;
	int i;

	/* no exception storage space available for this snapshot
	   or error on this snapshot --> release it */
	invalidate_buffers(lv_snap->lv_dev);

	/* wipe the snapshot since it's inconsistent now */
	_disable_snapshot(vg, lv_snap);

	for (i = last_dev = 0; i < lv_snap->lv_remap_ptr; i++) {
		if (lv_snap->lv_block_exception[i].rdev_new != last_dev) {
			last_dev = lv_snap->lv_block_exception[i].rdev_new;
			invalidate_buffers(last_dev);
		}
	}

	lvm_snapshot_release(lv_snap);
	lv_snap->lv_status &= ~LV_ACTIVE;

	printk(KERN_INFO
	       "%s -- giving up to snapshot %s on %s: %s\n",
	       lvm_name, lv_snap->lv_snapshot_org->lv_name,
	       lv_snap->lv_name, reason);
}

static inline int lvm_snapshot_prepare_blocks(unsigned long *blocks,
					      unsigned long start,
					      int nr_sectors,
					      int blocksize)
{
	int i, sectors_per_block, nr_blocks;

	sectors_per_block = blocksize / SECTOR_SIZE;

	if (start & (sectors_per_block - 1))
		return 0;

	nr_blocks = nr_sectors / sectors_per_block;
	start /= sectors_per_block;

	for (i = 0; i < nr_blocks; i++)
		blocks[i] = start++;

	return 1;
}

inline int lvm_get_blksize(kdev_t dev)
{
	int correct_size = BLOCK_SIZE, i, major;

	major = MAJOR(dev);
	if (blksize_size[major]) {
		i = blksize_size[major][MINOR(dev)];
		if (i)
			correct_size = i;
	}
	return correct_size;
}

#ifdef DEBUG_SNAPSHOT
static inline void invalidate_snap_cache(unsigned long start,
					 unsigned long nr, kdev_t dev)
{
	struct buffer_head *bh;
	int sectors_per_block, i, blksize, minor;

	minor = MINOR(dev);
	blksize = lvm_blocksizes[minor];
	sectors_per_block = blksize >> 9;
	nr /= sectors_per_block;
	start /= sectors_per_block;

	for (i = 0; i < nr; i++) {
		bh = get_hash_table(dev, start++, blksize);
		if (bh)
			bforget(bh);
	}
}
#endif


int lvm_snapshot_fill_COW_page(vg_t * vg, lv_t * lv_snap)
{
	int id = 0, is = lv_snap->lv_remap_ptr;
	ulong blksize_snap;
	lv_COW_table_disk_t *lv_COW_table = (lv_COW_table_disk_t *)
	    page_address(lv_snap->lv_COW_table_iobuf->maplist[0]);

	if (is == 0)
		return 0;

	is--;
	blksize_snap =
	    lvm_get_blksize(lv_snap->lv_block_exception[is].rdev_new);
	is -= is % (blksize_snap / sizeof(lv_COW_table_disk_t));

	memset(lv_COW_table, 0, blksize_snap);
	for (; is < lv_snap->lv_remap_ptr; is++, id++) {
		/* store new COW_table entry */
		lv_block_exception_t *be =
		    lv_snap->lv_block_exception + is;
		uint pvn;

		if (_pv_get_number(vg, be->rdev_org, &pvn))
			goto bad;

		lv_COW_table[id].pv_org_number = cpu_to_le64(pvn);
		lv_COW_table[id].pv_org_rsector =
		    cpu_to_le64(be->rsector_org);

		if (_pv_get_number(vg, be->rdev_new, &pvn))
			goto bad;

		lv_COW_table[id].pv_snap_number = cpu_to_le64(pvn);
		lv_COW_table[id].pv_snap_rsector =
		    cpu_to_le64(be->rsector_new);
	}

	return 0;

      bad:
	printk(KERN_ERR "%s -- lvm_snapshot_fill_COW_page failed",
	       lvm_name);
	return -1;
}


/*
 * writes a COW exception table sector to disk (HM)
 *
 * We need to hold a write lock on lv_snap->lv_lock.
 */
int lvm_write_COW_table_block(vg_t * vg, lv_t * lv_snap)
{
	int r;
	const char *err;
	if ((r = _write_COW_table_block(vg, lv_snap,
					lv_snap->lv_remap_ptr - 1, &err)))
		lvm_drop_snapshot(vg, lv_snap, err);
	return r;
}

/*
 * copy on write handler for one snapshot logical volume
 *
 * read the original blocks and store it/them on the new one(s).
 * if there is no exception storage space free any longer --> release snapshot.
 *
 * this routine gets called for each _first_ write to a physical chunk.
 *
 * We need to hold a write lock on lv_snap->lv_lock.  It is assumed that
 * lv->lv_block_exception is non-NULL (checked by lvm_snapshot_remap_block())
 * when this function is called.
 */
int lvm_snapshot_COW(kdev_t org_phys_dev,
		     unsigned long org_phys_sector,
		     unsigned long org_pe_start,
		     unsigned long org_virt_sector,
		     vg_t * vg, lv_t * lv_snap)
{
	const char *reason;
	unsigned long org_start, snap_start, snap_phys_dev, virt_start,
	    pe_off;
	unsigned long phys_start;
	int idx = lv_snap->lv_remap_ptr, chunk_size =
	    lv_snap->lv_chunk_size;
	struct kiobuf *iobuf = lv_snap->lv_iobuf;
	unsigned long *blocks = iobuf->blocks;
	int blksize_snap, blksize_org, min_blksize, max_blksize;
	int max_sectors, nr_sectors;

	/* check if we are out of snapshot space */
	if (idx >= lv_snap->lv_remap_end)
		goto fail_out_of_space;

	/* calculate physical boundaries of source chunk */
	pe_off = org_pe_start % chunk_size;
	org_start =
	    org_phys_sector - ((org_phys_sector - pe_off) % chunk_size);
	virt_start = org_virt_sector - (org_phys_sector - org_start);

	/* calculate physical boundaries of destination chunk */
	snap_phys_dev = lv_snap->lv_block_exception[idx].rdev_new;
	snap_start = lv_snap->lv_block_exception[idx].rsector_new;

#ifdef DEBUG_SNAPSHOT
	printk(KERN_INFO
	       "%s -- COW: "
	       "org %s faulting %lu start %lu, snap %s start %lu, "
	       "size %d, pe_start %lu pe_off %lu, virt_sec %lu\n",
	       lvm_name,
	       kdevname(org_phys_dev), org_phys_sector, org_start,
	       kdevname(snap_phys_dev), snap_start,
	       chunk_size, org_pe_start, pe_off, org_virt_sector);
#endif

	blksize_org = lvm_sectsize(org_phys_dev);
	blksize_snap = lvm_sectsize(snap_phys_dev);
	max_blksize = max(blksize_org, blksize_snap);
	min_blksize = min(blksize_org, blksize_snap);
	max_sectors = KIO_MAX_SECTORS * (min_blksize >> 9);

	if (chunk_size % (max_blksize >> 9))
		goto fail_blksize;

	/* Don't change org_start, we need it to fill in the exception table */
	phys_start = org_start;

	while (chunk_size) {
		nr_sectors = min(chunk_size, max_sectors);
		chunk_size -= nr_sectors;

		iobuf->length = nr_sectors << 9;

		if (!lvm_snapshot_prepare_blocks(blocks, phys_start,
						 nr_sectors, blksize_org))
			goto fail_prepare;

		if (__brw_kiovec(READ, 1, &iobuf, org_phys_dev, blocks,
				 blksize_org,
				 lv_snap) != (nr_sectors << 9))
			goto fail_raw_read;

		if (!lvm_snapshot_prepare_blocks(blocks, snap_start,
						 nr_sectors, blksize_snap))
			goto fail_prepare;

		if (__brw_kiovec(WRITE, 1, &iobuf, snap_phys_dev, blocks,
				 blksize_snap,
				 lv_snap) != (nr_sectors << 9))
			goto fail_raw_write;

		phys_start += nr_sectors;
		snap_start += nr_sectors;
	}

#ifdef DEBUG_SNAPSHOT
	/* invalidate the logical snapshot buffer cache */
	invalidate_snap_cache(virt_start, lv_snap->lv_chunk_size,
			      lv_snap->lv_dev);
#endif

	/* the original chunk is now stored on the snapshot volume
	   so update the execption table */
	lv_snap->lv_block_exception[idx].rdev_org = org_phys_dev;
	lv_snap->lv_block_exception[idx].rsector_org = org_start;

	lvm_hash_link(lv_snap->lv_block_exception + idx,
		      org_phys_dev, org_start, lv_snap);
	lv_snap->lv_remap_ptr = idx + 1;
	if (lv_snap->lv_snapshot_use_rate > 0) {
		if (lv_snap->lv_remap_ptr * 100 / lv_snap->lv_remap_end >=
		    lv_snap->lv_snapshot_use_rate)
			wake_up_interruptible(&lv_snap->lv_snapshot_wait);
	}
	return 0;

	/* slow path */
      out:
	lvm_drop_snapshot(vg, lv_snap, reason);
	return 1;

      fail_out_of_space:
	reason = "out of space";
	goto out;
      fail_raw_read:
	reason = "read error";
	goto out;
      fail_raw_write:
	reason = "write error";
	goto out;
      fail_blksize:
	reason = "blocksize error";
	goto out;

      fail_prepare:
	reason = "couldn't prepare kiovec blocks "
	    "(start probably isn't block aligned)";
	goto out;
}

int lvm_snapshot_alloc_iobuf_pages(struct kiobuf *iobuf, int sectors)
{
	int bytes, nr_pages, err, i;

	bytes = sectors * SECTOR_SIZE;
	nr_pages = (bytes + ~PAGE_MASK) >> PAGE_SHIFT;
	err = expand_kiobuf(iobuf, nr_pages);
	if (err)
		goto out;

	err = -ENOMEM;
	iobuf->locked = 1;
	iobuf->nr_pages = 0;
	for (i = 0; i < nr_pages; i++) {
		struct page *page;

		page = alloc_page(GFP_KERNEL);
		if (!page)
			goto out;

		iobuf->maplist[i] = page;
		LockPage(page);
		iobuf->nr_pages++;
	}
	iobuf->offset = 0;

	err = 0;

      out:
	return err;
}

static int calc_max_buckets(void)
{
	unsigned long mem;

	mem = num_physpages << PAGE_SHIFT;
	mem /= 100;
	mem *= 2;
	mem /= sizeof(struct list_head);

	return mem;
}

int lvm_snapshot_alloc_hash_table(lv_t * lv)
{
	int err;
	unsigned long buckets, max_buckets, size;
	struct list_head *hash;

	buckets = lv->lv_remap_end;
	max_buckets = calc_max_buckets();
	buckets = min(buckets, max_buckets);
	while (buckets & (buckets - 1))
		buckets &= (buckets - 1);

	size = buckets * sizeof(struct list_head);

	err = -ENOMEM;
	hash = vmalloc(size);
	lv->lv_snapshot_hash_table = hash;

	if (!hash)
		goto out;
	lv->lv_snapshot_hash_table_size = size;

	lv->lv_snapshot_hash_mask = buckets - 1;
	while (buckets--)
		INIT_LIST_HEAD(hash + buckets);
	err = 0;
      out:
	return err;
}

int lvm_snapshot_alloc(lv_t * lv_snap)
{
	int ret;

	/* allocate kiovec to do chunk io */
	ret = alloc_kiovec(1, &lv_snap->lv_iobuf);
	if (ret)
		goto out;

	ret = lvm_snapshot_alloc_iobuf_pages(lv_snap->lv_iobuf,
					     KIO_MAX_SECTORS);
	if (ret)
		goto out_free_kiovec;

	/* allocate kiovec to do exception table io */
	ret = alloc_kiovec(1, &lv_snap->lv_COW_table_iobuf);
	if (ret)
		goto out_free_kiovec;

	ret = lvm_snapshot_alloc_iobuf_pages(lv_snap->lv_COW_table_iobuf,
					     PAGE_SIZE / SECTOR_SIZE);
	if (ret)
		goto out_free_both_kiovecs;

	ret = lvm_snapshot_alloc_hash_table(lv_snap);
	if (ret)
		goto out_free_both_kiovecs;

      out:
	return ret;

      out_free_both_kiovecs:
	unmap_kiobuf(lv_snap->lv_COW_table_iobuf);
	free_kiovec(1, &lv_snap->lv_COW_table_iobuf);
	lv_snap->lv_COW_table_iobuf = NULL;

      out_free_kiovec:
	unmap_kiobuf(lv_snap->lv_iobuf);
	free_kiovec(1, &lv_snap->lv_iobuf);
	lv_snap->lv_iobuf = NULL;
	vfree(lv_snap->lv_snapshot_hash_table);
	lv_snap->lv_snapshot_hash_table = NULL;
	goto out;
}

void lvm_snapshot_release(lv_t * lv)
{
	if (lv->lv_block_exception) {
		vfree(lv->lv_block_exception);
		lv->lv_block_exception = NULL;
	}
	if (lv->lv_snapshot_hash_table) {
		vfree(lv->lv_snapshot_hash_table);
		lv->lv_snapshot_hash_table = NULL;
		lv->lv_snapshot_hash_table_size = 0;
	}
	if (lv->lv_iobuf) {
		kiobuf_wait_for_io(lv->lv_iobuf);
		unmap_kiobuf(lv->lv_iobuf);
		free_kiovec(1, &lv->lv_iobuf);
		lv->lv_iobuf = NULL;
	}
	if (lv->lv_COW_table_iobuf) {
		kiobuf_wait_for_io(lv->lv_COW_table_iobuf);
		unmap_kiobuf(lv->lv_COW_table_iobuf);
		free_kiovec(1, &lv->lv_COW_table_iobuf);
		lv->lv_COW_table_iobuf = NULL;
	}
}


static int _write_COW_table_block(vg_t * vg, lv_t * lv_snap,
				  int idx, const char **reason)
{
	int blksize_snap;
	int end_of_table;
	int idx_COW_table;
	uint pvn;
	ulong snap_pe_start, COW_table_sector_offset,
	    COW_entries_per_pe, COW_chunks_per_pe, COW_entries_per_block;
	ulong blocks[1];
	kdev_t snap_phys_dev;
	lv_block_exception_t *be;
	struct kiobuf *COW_table_iobuf = lv_snap->lv_COW_table_iobuf;
	lv_COW_table_disk_t *lv_COW_table =
	    (lv_COW_table_disk_t *) page_address(lv_snap->
						 lv_COW_table_iobuf->
						 maplist[0]);

	COW_chunks_per_pe = LVM_GET_COW_TABLE_CHUNKS_PER_PE(vg, lv_snap);
	COW_entries_per_pe = LVM_GET_COW_TABLE_ENTRIES_PER_PE(vg, lv_snap);

	/* get physical addresse of destination chunk */
	snap_phys_dev = lv_snap->lv_block_exception[idx].rdev_new;
	snap_pe_start =
	    lv_snap->lv_block_exception[idx -
					(idx %
					 COW_entries_per_pe)].rsector_new -
	    lv_snap->lv_chunk_size;

	blksize_snap = lvm_sectsize(snap_phys_dev);

	COW_entries_per_block = blksize_snap / sizeof(lv_COW_table_disk_t);
	idx_COW_table = idx % COW_entries_per_pe % COW_entries_per_block;

	if (idx_COW_table == 0)
		memset(lv_COW_table, 0, blksize_snap);

	/* sector offset into the on disk COW table */
	COW_table_sector_offset =
	    (idx % COW_entries_per_pe) / (SECTOR_SIZE /
					  sizeof(lv_COW_table_disk_t));

	/* COW table block to write next */
	blocks[0] =
	    (snap_pe_start +
	     COW_table_sector_offset) >> (blksize_snap >> 10);

	/* store new COW_table entry */
	be = lv_snap->lv_block_exception + idx;
	if (_pv_get_number(vg, be->rdev_org, &pvn))
		goto fail_pv_get_number;

	lv_COW_table[idx_COW_table].pv_org_number = cpu_to_le64(pvn);
	lv_COW_table[idx_COW_table].pv_org_rsector =
	    cpu_to_le64(be->rsector_org);
	if (_pv_get_number(vg, snap_phys_dev, &pvn))
		goto fail_pv_get_number;

	lv_COW_table[idx_COW_table].pv_snap_number = cpu_to_le64(pvn);
	lv_COW_table[idx_COW_table].pv_snap_rsector =
	    cpu_to_le64(be->rsector_new);

	COW_table_iobuf->length = blksize_snap;
	/* COW_table_iobuf->nr_pages = 1; */

	if (__brw_kiovec(WRITE, 1, &COW_table_iobuf, snap_phys_dev,
			 blocks, blksize_snap, lv_snap) != blksize_snap)
		goto fail_raw_write;

	/* initialization of next COW exception table block with zeroes */
	end_of_table = idx % COW_entries_per_pe == COW_entries_per_pe - 1;
	if (idx_COW_table % COW_entries_per_block ==
	    COW_entries_per_block - 1 || end_of_table) {
		/* don't go beyond the end */
		if (idx + 1 >= lv_snap->lv_remap_end)
			goto out;

		memset(lv_COW_table, 0, blksize_snap);

		if (end_of_table) {
			idx++;
			snap_phys_dev =
			    lv_snap->lv_block_exception[idx].rdev_new;
			snap_pe_start =
			    lv_snap->lv_block_exception[idx -
							(idx %
							 COW_entries_per_pe)].
			    rsector_new - lv_snap->lv_chunk_size;
			blksize_snap = lvm_sectsize(snap_phys_dev);
			blocks[0] = snap_pe_start >> (blksize_snap >> 10);
		} else
			blocks[0]++;

		if (__brw_kiovec(WRITE, 1, &COW_table_iobuf, snap_phys_dev,
				 blocks, blksize_snap, lv_snap) !=
		    blksize_snap)
			goto fail_raw_write;
	}

      out:
	return 0;

      fail_raw_write:
	*reason = "write error";
	return 1;

      fail_pv_get_number:
	*reason = "_pv_get_number failed";
	return 1;
}

/*
 * FIXME_1.2
 * This function is a bit of a hack; we need to ensure that the
 * snapshot is never made active again, because it will surely be
 * corrupt.  At the moment we do not have access to the LVM metadata
 * from within the kernel.  So we set the first exception to point to
 * sector 1 (which will always be within the metadata, and as such
 * invalid).  User land tools will check for this when they are asked
 * to activate the snapshot and prevent this from happening.
 */

static void _disable_snapshot(vg_t * vg, lv_t * lv)
{
	const char *err;
	lv->lv_block_exception[0].rsector_org =
	    LVM_SNAPSHOT_DROPPED_SECTOR;
	if (_write_COW_table_block(vg, lv, 0, &err) < 0) {
		printk(KERN_ERR "%s -- couldn't disable snapshot: %s\n",
		       lvm_name, err);
	}
}
