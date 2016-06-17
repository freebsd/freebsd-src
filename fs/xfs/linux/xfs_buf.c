/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

/*
 *	page_buf.c
 *
 *	The page_buf module provides an abstract buffer cache model on top of
 *	the Linux page cache.  Cached metadata blocks for a file system are
 *	hashed to the inode for the block device.  The page_buf module
 *	assembles buffer (page_buf_t) objects on demand to aggregate such
 *	cached pages for I/O.
 *
 *
 *      Written by Steve Lord, Jim Mostek, Russell Cattelan
 *		    and Rajagopal Ananthanarayanan ("ananth") at SGI.
 *
 */

#include <linux/stddef.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/locks.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>

#include "xfs_linux.h"

#define BN_ALIGN_MASK	((1 << (PAGE_CACHE_SHIFT - BBSHIFT)) - 1)

#ifndef GFP_READAHEAD
#define GFP_READAHEAD	0
#endif

/*
 * A backport of the 2.5 scheduler is used by many vendors of 2.4-based
 * distributions.
 * We can only guess it's presences by the lack of the SCHED_YIELD flag.
 * If the heuristic doesn't work, change this define by hand.
 */
#ifndef SCHED_YIELD
#define __HAVE_NEW_SCHEDULER	1
#endif

/*
 * cpumask_t is used for supporting NR_CPUS > BITS_PER_LONG.
 * If support for this is present, migrate_to_cpu exists and provides
 * a wrapper around the set_cpus_allowed routine.
 */
#ifdef copy_cpumask
#define __HAVE_CPUMASK_T	1
#endif

#ifndef __HAVE_CPUMASK_T
# ifndef __HAVE_NEW_SCHEDULER
#  define migrate_to_cpu(cpu)	\
	do { current->cpus_allowed = 1UL << (cpu); } while (0)
# else
#  define migrate_to_cpu(cpu)	\
	set_cpus_allowed(current, 1UL << (cpu))
# endif
#endif

/*
 * File wide globals
 */

STATIC kmem_cache_t *pagebuf_cache;

#define MAX_IO_DAEMONS		NR_CPUS
#define CPU_TO_DAEMON(cpu)	(cpu)
STATIC int pb_logio_daemons[MAX_IO_DAEMONS];
STATIC struct list_head pagebuf_logiodone_tq[MAX_IO_DAEMONS];
STATIC wait_queue_head_t pagebuf_logiodone_wait[MAX_IO_DAEMONS];
STATIC int pb_dataio_daemons[MAX_IO_DAEMONS];
STATIC struct list_head pagebuf_dataiodone_tq[MAX_IO_DAEMONS];
STATIC wait_queue_head_t pagebuf_dataiodone_wait[MAX_IO_DAEMONS];

/*
 * For pre-allocated buffer head pool
 */

#define NR_RESERVED_BH	64
static wait_queue_head_t	pb_resv_bh_wait;
static spinlock_t		pb_resv_bh_lock = SPIN_LOCK_UNLOCKED;
struct buffer_head		*pb_resv_bh = NULL;	/* list of bh */
int				pb_resv_bh_cnt = 0;	/* # of bh available */

STATIC void pagebuf_daemon_wakeup(void);
STATIC void _pagebuf_ioapply(page_buf_t *);
STATIC void pagebuf_delwri_queue(page_buf_t *, int);
STATIC void pagebuf_runall_queues(struct list_head[]);

/*
 * Pagebuf debugging
 */

#ifdef PAGEBUF_TRACE
void
pagebuf_trace(
	page_buf_t	*pb,
	char		*id,
	void		*data,
	void		*ra)
{
	ktrace_enter(pagebuf_trace_buf,
		pb, id,
		(void *)(unsigned long)pb->pb_flags,
		(void *)(unsigned long)pb->pb_hold.counter,
		(void *)(unsigned long)pb->pb_sema.count.counter,
		(void *)current,
		data, ra,
		(void *)(unsigned long)((pb->pb_file_offset>>32) & 0xffffffff),
		(void *)(unsigned long)(pb->pb_file_offset & 0xffffffff),
		(void *)(unsigned long)pb->pb_buffer_length,
		NULL, NULL, NULL, NULL, NULL);
}
ktrace_t *pagebuf_trace_buf;
#define PAGEBUF_TRACE_SIZE	4096
#define PB_TRACE(pb, id, data)	\
	pagebuf_trace(pb, id, (void *)data, (void *)__builtin_return_address(0))
#else
#define PB_TRACE(pb, id, data)	do { } while (0)
#endif

#ifdef PAGEBUF_LOCK_TRACKING
# define PB_SET_OWNER(pb)	((pb)->pb_last_holder = current->pid)
# define PB_CLEAR_OWNER(pb)	((pb)->pb_last_holder = -1)
# define PB_GET_OWNER(pb)	((pb)->pb_last_holder)
#else
# define PB_SET_OWNER(pb)	do { } while (0)
# define PB_CLEAR_OWNER(pb)	do { } while (0)
# define PB_GET_OWNER(pb)	do { } while (0)
#endif

/*
 * Pagebuf allocation / freeing.
 */

#define pb_to_gfp(flags) \
	(((flags) & PBF_READ_AHEAD) ? GFP_READAHEAD : \
	 ((flags) & PBF_DONT_BLOCK) ? GFP_NOFS : GFP_KERNEL)

#define pb_to_km(flags) \
	 (((flags) & PBF_DONT_BLOCK) ? KM_NOFS : KM_SLEEP)


#define pagebuf_allocate(flags) \
	kmem_zone_alloc(pagebuf_cache, pb_to_km(flags))
#define pagebuf_deallocate(pb) \
	kmem_zone_free(pagebuf_cache, (pb));

/*
 * Pagebuf hashing
 */

#define NBITS	8
#define NHASH	(1<<NBITS)

typedef struct {
	struct list_head	pb_hash;
	spinlock_t		pb_hash_lock;
} pb_hash_t;

STATIC pb_hash_t	pbhash[NHASH];
#define pb_hash(pb)	&pbhash[pb->pb_hash_index]

STATIC int
_bhash(
	struct block_device *bdev,
	loff_t		base)
{
	int		bit, hval;

	base >>= 9;
	base ^= (unsigned long)bdev / L1_CACHE_BYTES;
	for (bit = hval = 0; base && bit < sizeof(base) * 8; bit += NBITS) {
		hval ^= (int)base & (NHASH-1);
		base >>= NBITS;
	}
	return hval;
}

/*
 * Mapping of multi-page buffers into contiguous virtual space
 */

STATIC void *pagebuf_mapout_locked(page_buf_t *);

typedef struct a_list {
	void		*vm_addr;
	struct a_list	*next;
} a_list_t;

STATIC a_list_t		*as_free_head;
STATIC int		as_list_len;
STATIC spinlock_t	as_lock = SPIN_LOCK_UNLOCKED;

/*
 * Try to batch vunmaps because they are costly.
 */
STATIC void
free_address(
	void		*addr)
{
	a_list_t	*aentry;

	aentry = kmalloc(sizeof(a_list_t), GFP_ATOMIC);
	if (aentry) {
		spin_lock(&as_lock);
		aentry->next = as_free_head;
		aentry->vm_addr = addr;
		as_free_head = aentry;
		as_list_len++;
		spin_unlock(&as_lock);
	} else {
		vunmap(addr);
	}
}

STATIC void
purge_addresses(void)
{
	a_list_t	*aentry, *old;

	if (as_free_head == NULL)
		return;

	spin_lock(&as_lock);
	aentry = as_free_head;
	as_free_head = NULL;
	as_list_len = 0;
	spin_unlock(&as_lock);

	while ((old = aentry) != NULL) {
		vunmap(aentry->vm_addr);
		aentry = aentry->next;
		kfree(old);
	}
}

/*
 *	Internal pagebuf object manipulation
 */

STATIC void
_pagebuf_initialize(
	page_buf_t		*pb,
	pb_target_t		*target,
	loff_t			range_base,
	size_t			range_length,
	page_buf_flags_t	flags)
{
	/*
	 * We don't want certain flags to appear in pb->pb_flags.
	 */
	flags &= ~(PBF_LOCK|PBF_MAPPED|PBF_DONT_BLOCK|PBF_READ_AHEAD);

	memset(pb, 0, sizeof(page_buf_t));
	atomic_set(&pb->pb_hold, 1);
	init_MUTEX_LOCKED(&pb->pb_iodonesema);
	INIT_LIST_HEAD(&pb->pb_list);
	INIT_LIST_HEAD(&pb->pb_hash_list);
	init_MUTEX_LOCKED(&pb->pb_sema); /* held, no waiters */
	PB_SET_OWNER(pb);
	pb->pb_target = target;
	pb->pb_file_offset = range_base;
	/*
	 * Set buffer_length and count_desired to the same value initially.
	 * IO routines should use count_desired, which will be the same in
	 * most cases but may be reset (e.g. XFS recovery).
	 */
	pb->pb_buffer_length = pb->pb_count_desired = range_length;
	pb->pb_flags = flags | PBF_NONE;
	pb->pb_bn = PAGE_BUF_DADDR_NULL;
	atomic_set(&pb->pb_pin_count, 0);
	init_waitqueue_head(&pb->pb_waiters);

	XFS_STATS_INC(pb_create);
	PB_TRACE(pb, "initialize", target);
}

/*
 * Allocate a page array capable of holding a specified number
 * of pages, and point the page buf at it.
 */
STATIC int
_pagebuf_get_pages(
	page_buf_t		*pb,
	int			page_count,
	page_buf_flags_t	flags)
{
	/* Make sure that we have a page list */
	if (pb->pb_pages == NULL) {
		pb->pb_offset = page_buf_poff(pb->pb_file_offset);
		pb->pb_page_count = page_count;
		if (page_count <= PB_PAGES) {
			pb->pb_pages = pb->pb_page_array;
		} else {
			pb->pb_pages = kmem_alloc(sizeof(struct page *) *
					page_count, pb_to_km(flags));
			if (pb->pb_pages == NULL)
				return -ENOMEM;
		}
		memset(pb->pb_pages, 0, sizeof(struct page *) * page_count);
	}
	return 0;
}

/*
 * Walk a pagebuf releasing all the pages contained within it.
 */
STATIC inline void
_pagebuf_freepages(
	page_buf_t		*pb)
{
	int			buf_index;

	for (buf_index = 0; buf_index < pb->pb_page_count; buf_index++) {
		struct page	*page = pb->pb_pages[buf_index];

		if (page) {
			pb->pb_pages[buf_index] = NULL;
			page_cache_release(page);
		}
	}
}

/*
 *	pagebuf_free
 *
 *	pagebuf_free releases the specified buffer.  The modification
 *	state of any associated pages is left unchanged.
 */
void
pagebuf_free(
	page_buf_t		*pb)
{
	PB_TRACE(pb, "free", 0);
	
	ASSERT(list_empty(&pb->pb_hash_list));

	/* release any virtual mapping */ ;
	if (pb->pb_flags & _PBF_ADDR_ALLOCATED) {
		void *vaddr = pagebuf_mapout_locked(pb);
		if (vaddr) {
			free_address(vaddr);
		}
	}

	if (pb->pb_flags & _PBF_MEM_ALLOCATED) {
		if (pb->pb_pages) {
			/* release the pages in the address list */
			if ((pb->pb_pages[0]) &&
			    (pb->pb_flags & _PBF_MEM_SLAB)) {
				kfree(pb->pb_addr);
			} else {
				_pagebuf_freepages(pb);
			}
			if (pb->pb_pages != pb->pb_page_array)
				kfree(pb->pb_pages);
			pb->pb_pages = NULL;
		}
		pb->pb_flags &= ~(_PBF_MEM_ALLOCATED|_PBF_MEM_SLAB);
	}

	pagebuf_deallocate(pb);
}

/*
 *	_pagebuf_lookup_pages
 *
 *	_pagebuf_lookup_pages finds all pages which match the buffer
 *	in question and the range of file offsets supplied,
 *	and builds the page list for the buffer, if the
 *	page list is not already formed or if not all of the pages are
 *	already in the list. Invalid pages (pages which have not yet been
 *	read in from disk) are assigned for any pages which are not found.
 */
STATIC int
_pagebuf_lookup_pages(
	page_buf_t		*pb,
	struct address_space	*aspace,
	page_buf_flags_t	flags)
{
	loff_t			next_buffer_offset;
	unsigned long		page_count, pi, index;
	struct page		*page;
	int			gfp_mask = pb_to_gfp(flags);
	int			all_mapped, good_pages, rval, retries;
	size_t			blocksize;

	next_buffer_offset = pb->pb_file_offset + pb->pb_buffer_length;
	good_pages = page_count = (page_buf_btoc(next_buffer_offset) -
				   page_buf_btoct(pb->pb_file_offset));

	if (pb->pb_flags & _PBF_ALL_PAGES_MAPPED) {
		/* Bring pages forward in cache */
		for (pi = 0; pi < page_count; pi++) {
			mark_page_accessed(pb->pb_pages[pi]);
		}
		if ((flags & PBF_MAPPED) && !(pb->pb_flags & PBF_MAPPED)) {
			all_mapped = 1;
			rval = 0;
			goto mapit;
		}
		return 0;
	}

	/* Ensure pb_pages field has been initialised */
	rval = _pagebuf_get_pages(pb, page_count, flags);
	if (rval)
		return rval;

	all_mapped = 1;
	blocksize = pb->pb_target->pbr_bsize;

	/* Enter the pages in the page list */
	index = (pb->pb_file_offset - pb->pb_offset) >> PAGE_CACHE_SHIFT;
	for (pi = 0; pi < page_count; pi++, index++) {
		if (pb->pb_pages[pi] == 0) {
			retries = 0;
		      retry:
			page = find_or_create_page(aspace, index, gfp_mask);
			if (!page) {
				if (flags & PBF_READ_AHEAD)
					return -ENOMEM;
				/*
				 * This could deadlock.  But until all the
				 * XFS lowlevel code is revamped to handle
				 * buffer allocation failures we can't do
				 * much.
				 */
				if (!(++retries % 100)) {
					printk(KERN_ERR
					       "possibly deadlocking in %s\n",
					       __FUNCTION__);
				}
				XFS_STATS_INC(pb_page_retries);
				pagebuf_daemon_wakeup();
				current->state = TASK_UNINTERRUPTIBLE;
				schedule_timeout(10);
				goto retry;
			}
			XFS_STATS_INC(pb_page_found);
			mark_page_accessed(page);
			pb->pb_pages[pi] = page;
		} else {
			page = pb->pb_pages[pi];
			lock_page(page);
		}

		/* If we need to do I/O on a page record the fact */
		if (!Page_Uptodate(page)) {
			good_pages--;
			if ((blocksize == PAGE_CACHE_SIZE) &&
			    (flags & PBF_READ))
				pb->pb_locked = 1;
		}
	}

	if (!pb->pb_locked) {
		for (pi = 0; pi < page_count; pi++) {
			if (pb->pb_pages[pi])
				unlock_page(pb->pb_pages[pi]);
		}
	}

	pb->pb_flags |= _PBF_PAGECACHE;
mapit:
	pb->pb_flags |= _PBF_MEM_ALLOCATED;
	if (all_mapped) {
		pb->pb_flags |= _PBF_ALL_PAGES_MAPPED;

		/* A single page buffer is always mappable */
		if (page_count == 1) {
			pb->pb_addr = (caddr_t)
				page_address(pb->pb_pages[0]) + pb->pb_offset;
			pb->pb_flags |= PBF_MAPPED;
		} else if (flags & PBF_MAPPED) {
			if (as_list_len > 64)
				purge_addresses();
			pb->pb_addr = vmap(pb->pb_pages, page_count,
					VM_ALLOC, PAGE_KERNEL);
			if (pb->pb_addr == NULL)
				return -ENOMEM;
			pb->pb_addr += pb->pb_offset;
			pb->pb_flags |= PBF_MAPPED | _PBF_ADDR_ALLOCATED;
		}
	}
	/* If some pages were found with data in them
	 * we are not in PBF_NONE state.
	 */
	if (good_pages != 0) {
		pb->pb_flags &= ~(PBF_NONE);
		if (good_pages != page_count) {
			pb->pb_flags |= PBF_PARTIAL;
		}
	}

	PB_TRACE(pb, "lookup_pages", (long)good_pages);

	return rval;
}


/*
 *	Pre-allocation of a pool of buffer heads for use in
 *	low-memory situations.
 */

/*
 *	_pagebuf_prealloc_bh
 *
 *	Pre-allocate a pool of "count" buffer heads at startup.
 *	Puts them on a list at "pb_resv_bh"
 *	Returns number of bh actually allocated to pool.
 */
STATIC int
_pagebuf_prealloc_bh(
	int			count)
{
	struct buffer_head	*bh;
	int			i;

	for (i = 0; i < count; i++) {
		bh = kmem_cache_alloc(bh_cachep, SLAB_KERNEL);
		if (!bh)
			break;
		bh->b_pprev = &pb_resv_bh;
		bh->b_next = pb_resv_bh;
		pb_resv_bh = bh;
		pb_resv_bh_cnt++;
	}
	return i;
}

/*
 *	_pagebuf_get_prealloc_bh
 *
 *	Get one buffer head from our pre-allocated pool.
 *	If pool is empty, sleep 'til one comes back in.
 *	Returns aforementioned buffer head.
 */
STATIC struct buffer_head *
_pagebuf_get_prealloc_bh(void)
{
	unsigned long		flags;
	struct buffer_head	*bh;
	DECLARE_WAITQUEUE	(wait, current);

	spin_lock_irqsave(&pb_resv_bh_lock, flags);

	if (pb_resv_bh_cnt < 1) {
		add_wait_queue(&pb_resv_bh_wait, &wait);
		do {
			set_current_state(TASK_UNINTERRUPTIBLE);
			spin_unlock_irqrestore(&pb_resv_bh_lock, flags);
			blk_run_queues();
			schedule();
			spin_lock_irqsave(&pb_resv_bh_lock, flags);
		} while (pb_resv_bh_cnt < 1);
		__set_current_state(TASK_RUNNING);
		remove_wait_queue(&pb_resv_bh_wait, &wait);
	}

	BUG_ON(pb_resv_bh_cnt < 1);
	BUG_ON(!pb_resv_bh);

	bh = pb_resv_bh;
	pb_resv_bh = bh->b_next;
	pb_resv_bh_cnt--;

	spin_unlock_irqrestore(&pb_resv_bh_lock, flags);
	return bh;
}

/*
 *	_pagebuf_free_bh
 *
 *	Take care of buffer heads that we're finished with.
 *	Call this instead of just kmem_cache_free(bh_cachep, bh)
 *	when you're done with a bh.
 *
 *	If our pre-allocated pool is full, just free the buffer head.
 *	Otherwise, put it back in the pool, and wake up anybody
 *	waiting for one.
 */
STATIC inline void
_pagebuf_free_bh(
	struct buffer_head	*bh)
{
	unsigned long		flags;
	int			free;

	if (! (free = pb_resv_bh_cnt >= NR_RESERVED_BH)) {
		spin_lock_irqsave(&pb_resv_bh_lock, flags);

		if (! (free = pb_resv_bh_cnt >= NR_RESERVED_BH)) {
			bh->b_pprev = &pb_resv_bh;
			bh->b_next = pb_resv_bh;
			pb_resv_bh = bh;
			pb_resv_bh_cnt++;

			if (waitqueue_active(&pb_resv_bh_wait)) {
				wake_up(&pb_resv_bh_wait);
			}
		}

		spin_unlock_irqrestore(&pb_resv_bh_lock, flags);
	}
	if (free) {
		kmem_cache_free(bh_cachep, bh);
	}
}

/*
 *	Finding and Reading Buffers
 */

/*
 *	_pagebuf_find
 *
 *	Looks up, and creates if absent, a lockable buffer for
 *	a given range of an inode.  The buffer is returned
 *	locked.	 If other overlapping buffers exist, they are
 *	released before the new buffer is created and locked,
 *	which may imply that this call will block until those buffers
 *	are unlocked.  No I/O is implied by this call.
 */
STATIC page_buf_t *
_pagebuf_find(				/* find buffer for block	*/
	pb_target_t		*target,/* target for block		*/
	loff_t			ioff,	/* starting offset of range	*/
	size_t			isize,	/* length of range		*/
	page_buf_flags_t	flags,	/* PBF_TRYLOCK			*/
	page_buf_t		*new_pb)/* newly allocated buffer	*/
{
	loff_t			range_base;
	size_t			range_length;
	int			hval;
	pb_hash_t		*h;
	struct list_head	*p;
	page_buf_t		*pb;
	int			not_locked;

	range_base = (ioff << BBSHIFT);
	range_length = (isize << BBSHIFT);

	/* Ensure we never do IOs smaller than the sector size */
	BUG_ON(range_length < (1 << target->pbr_sshift));

	/* Ensure we never do IOs that are not sector aligned */
	BUG_ON(range_base & (loff_t)target->pbr_smask);

	hval = _bhash(target->pbr_bdev, range_base);
	h = &pbhash[hval];

	spin_lock(&h->pb_hash_lock);
	list_for_each(p, &h->pb_hash) {
		pb = list_entry(p, page_buf_t, pb_hash_list);

		if (pb->pb_target == target &&
		    pb->pb_file_offset == range_base &&
		    pb->pb_buffer_length == range_length) {
			/* If we look at something bring it to the
			 * front of the list for next time
			 */
			atomic_inc(&pb->pb_hold);
			list_move(&pb->pb_hash_list, &h->pb_hash);
			goto found;
		}
	}

	/* No match found */
	if (new_pb) {
		_pagebuf_initialize(new_pb, target, range_base,
				range_length, flags);
		new_pb->pb_hash_index = hval;
		list_add(&new_pb->pb_hash_list, &h->pb_hash);
	} else {
		XFS_STATS_INC(pb_miss_locked);
	}

	spin_unlock(&h->pb_hash_lock);
	return (new_pb);

found:
	spin_unlock(&h->pb_hash_lock);

	/* Attempt to get the semaphore without sleeping,
	 * if this does not work then we need to drop the
	 * spinlock and do a hard attempt on the semaphore.
	 */
	not_locked = down_trylock(&pb->pb_sema);
	if (not_locked) {
		if (!(flags & PBF_TRYLOCK)) {
			/* wait for buffer ownership */
			PB_TRACE(pb, "get_lock", 0);
			pagebuf_lock(pb);
			XFS_STATS_INC(pb_get_locked_waited);
		} else {
			/* We asked for a trylock and failed, no need
			 * to look at file offset and length here, we
			 * know that this pagebuf at least overlaps our
			 * pagebuf and is locked, therefore our buffer
			 * either does not exist, or is this buffer
			 */

			pagebuf_rele(pb);
			XFS_STATS_INC(pb_busy_locked);
			return (NULL);
		}
	} else {
		/* trylock worked */
		PB_SET_OWNER(pb);
	}

	if (pb->pb_flags & PBF_STALE)
		pb->pb_flags &= PBF_MAPPED | \
				_PBF_ALL_PAGES_MAPPED | \
				_PBF_ADDR_ALLOCATED | \
				_PBF_MEM_ALLOCATED | \
				_PBF_MEM_SLAB;
	PB_TRACE(pb, "got_lock", 0);
	XFS_STATS_INC(pb_get_locked);
	return (pb);
}


/*
 *	pagebuf_find
 *
 *	pagebuf_find returns a buffer matching the specified range of
 *	data for the specified target, if any of the relevant blocks
 *	are in memory.  The buffer may have unallocated holes, if
 *	some, but not all, of the blocks are in memory.  Even where
 *	pages are present in the buffer, not all of every page may be
 *	valid.
 */
page_buf_t *
pagebuf_find(				/* find buffer for block	*/
					/* if the block is in memory	*/
	pb_target_t		*target,/* target for block		*/
	loff_t			ioff,	/* starting offset of range	*/
	size_t			isize,	/* length of range		*/
	page_buf_flags_t	flags)	/* PBF_TRYLOCK			*/
{
	return _pagebuf_find(target, ioff, isize, flags, NULL);
}

/*
 *	pagebuf_get
 *
 *	pagebuf_get assembles a buffer covering the specified range.
 *	Some or all of the blocks in the range may be valid.  Storage
 *	in memory for all portions of the buffer will be allocated,
 *	although backing storage may not be.  If PBF_READ is set in
 *	flags, pagebuf_iostart is called also.
 */
page_buf_t *
pagebuf_get(				/* allocate a buffer		*/
	pb_target_t		*target,/* target for buffer		*/
	loff_t			ioff,	/* starting offset of range	*/
	size_t			isize,	/* length of range		*/
	page_buf_flags_t	flags)	/* PBF_TRYLOCK			*/
{
	page_buf_t		*pb, *new_pb;
	int			error;

	new_pb = pagebuf_allocate(flags);
	if (unlikely(!new_pb))
		return (NULL);

	pb = _pagebuf_find(target, ioff, isize, flags, new_pb);
	if (pb != new_pb) {
		pagebuf_deallocate(new_pb);
		if (unlikely(!pb))
			return (NULL);
	}

	XFS_STATS_INC(pb_get);

	/* fill in any missing pages */
	error = _pagebuf_lookup_pages(pb, pb->pb_target->pbr_mapping, flags);
	if (unlikely(error)) {
		printk(KERN_WARNING
			"pagebuf_get: warning, failed to lookup pages\n");
		goto no_buffer;
	}

	/*
	 * Always fill in the block number now, the mapped cases can do
	 * their own overlay of this later.
	 */
	pb->pb_bn = ioff;
	pb->pb_count_desired = pb->pb_buffer_length;

	if (flags & PBF_READ) {
		if (PBF_NOT_DONE(pb)) {
			PB_TRACE(pb, "get_read", (unsigned long)flags);
			XFS_STATS_INC(pb_get_read);
			pagebuf_iostart(pb, flags);
		} else if (flags & PBF_ASYNC) {
			PB_TRACE(pb, "get_read_async", (unsigned long)flags);
			/*
			 * Read ahead call which is already satisfied,
			 * drop the buffer
			 */
			goto no_buffer;
		} else {
			PB_TRACE(pb, "get_read_done", (unsigned long)flags);
			/* We do not want read in the flags */
			pb->pb_flags &= ~PBF_READ;
		}
	} else {
		PB_TRACE(pb, "get_write", (unsigned long)flags);
	}

	return pb;

no_buffer:
	if (flags & (PBF_LOCK | PBF_TRYLOCK))
		pagebuf_unlock(pb);
	pagebuf_rele(pb);
	return NULL;
}

/*
 * Create a skeletal pagebuf (no pages associated with it).
 */
page_buf_t *
pagebuf_lookup(
	struct pb_target	*target,
	loff_t			ioff,
	size_t			isize,
	page_buf_flags_t	flags)
{
	page_buf_t		*pb;

	flags |= _PBF_PRIVATE_BH;
	pb = pagebuf_allocate(flags);
	if (pb) {
		_pagebuf_initialize(pb, target, ioff, isize, flags);
	}
	return pb;
}

/*
 * If we are not low on memory then do the readahead in a deadlock
 * safe manner.
 */
void
pagebuf_readahead(
	pb_target_t		*target,
	loff_t			ioff,
	size_t			isize,
	page_buf_flags_t	flags)
{
	flags |= (PBF_TRYLOCK|PBF_READ|PBF_ASYNC|PBF_READ_AHEAD);
	pagebuf_get(target, ioff, isize, flags);
}

page_buf_t *
pagebuf_get_empty(
	size_t			len,
	pb_target_t		*target)
{
	page_buf_t		*pb;

	pb = pagebuf_allocate(0);
	if (pb)
		_pagebuf_initialize(pb, target, 0, len, 0);
	return pb;
}

static inline struct page *
mem_to_page(
	void			*addr)
{
	if (((unsigned long)addr < VMALLOC_START) ||
	    ((unsigned long)addr >= VMALLOC_END)) {
		return virt_to_page(addr);
	} else {
		return vmalloc_to_page(addr);
	}
}

int
pagebuf_associate_memory(
	page_buf_t		*pb,
	void			*mem,
	size_t			len)
{
	int			rval;
	int			i = 0;
	size_t			ptr;
	size_t			end, end_cur;
	off_t			offset;
	int			page_count;

	page_count = PAGE_CACHE_ALIGN(len) >> PAGE_CACHE_SHIFT;
	offset = (off_t) mem - ((off_t)mem & PAGE_CACHE_MASK);
	if (offset && (len > PAGE_CACHE_SIZE))
		page_count++;

	/* Free any previous set of page pointers */
	if (pb->pb_pages && (pb->pb_pages != pb->pb_page_array)) {
		kfree(pb->pb_pages);
	}
	pb->pb_pages = NULL;
	pb->pb_addr = mem;

	rval = _pagebuf_get_pages(pb, page_count, 0);
	if (rval)
		return rval;

	pb->pb_offset = offset;
	ptr = (size_t) mem & PAGE_CACHE_MASK;
	end = PAGE_CACHE_ALIGN((size_t) mem + len);
	end_cur = end;
	/* set up first page */
	pb->pb_pages[0] = mem_to_page(mem);

	ptr += PAGE_CACHE_SIZE;
	pb->pb_page_count = ++i;
	while (ptr < end) {
		pb->pb_pages[i] = mem_to_page((void *)ptr);
		pb->pb_page_count = ++i;
		ptr += PAGE_CACHE_SIZE;
	}
	pb->pb_locked = 0;

	pb->pb_count_desired = pb->pb_buffer_length = len;
	pb->pb_flags |= PBF_MAPPED | _PBF_PRIVATE_BH;

	return 0;
}

page_buf_t *
pagebuf_get_no_daddr(
	size_t			len,
	pb_target_t		*target)
{
	int			rval;
	void			*rmem = NULL;
	page_buf_flags_t	flags = PBF_FORCEIO;
	page_buf_t		*pb;
	size_t			tlen = 0;

	if (unlikely(len > 0x20000))
		return NULL;

	pb = pagebuf_allocate(flags);
	if (!pb)
		return NULL;

	_pagebuf_initialize(pb, target, 0, len, flags);

	do {
		if (tlen == 0) {
			tlen = len; /* first time */
		} else {
			kfree(rmem); /* free the mem from the previous try */
			tlen <<= 1; /* double the size and try again */
		}
		if ((rmem = kmalloc(tlen, GFP_KERNEL)) == 0) {
			pagebuf_free(pb);
			return NULL;
		}
	} while ((size_t)rmem != ((size_t)rmem & ~target->pbr_smask));

	if ((rval = pagebuf_associate_memory(pb, rmem, len)) != 0) {
		kfree(rmem);
		pagebuf_free(pb);
		return NULL;
	}
	/* otherwise pagebuf_free just ignores it */
	pb->pb_flags |= (_PBF_MEM_ALLOCATED | _PBF_MEM_SLAB);
	PB_CLEAR_OWNER(pb);
	up(&pb->pb_sema);	/* Return unlocked pagebuf */

	PB_TRACE(pb, "no_daddr", rmem);

	return pb;
}


/*
 *	pagebuf_hold
 *
 *	Increment reference count on buffer, to hold the buffer concurrently
 *	with another thread which may release (free) the buffer asynchronously.
 *
 *	Must hold the buffer already to call this function.
 */
void
pagebuf_hold(
	page_buf_t		*pb)
{
	atomic_inc(&pb->pb_hold);
	PB_TRACE(pb, "hold", 0);
}

/*
 *	pagebuf_rele
 *
 *	pagebuf_rele releases a hold on the specified buffer.  If the
 *	the hold count is 1, pagebuf_rele calls pagebuf_free.
 */
void
pagebuf_rele(
	page_buf_t		*pb)
{
	pb_hash_t		*hash = pb_hash(pb);

	PB_TRACE(pb, "rele", pb->pb_relse);

	if (atomic_dec_and_lock(&pb->pb_hold, &hash->pb_hash_lock)) {
		int		do_free = 1;

		if (pb->pb_relse) {
			atomic_inc(&pb->pb_hold);
			spin_unlock(&hash->pb_hash_lock);
			(*(pb->pb_relse)) (pb);
			spin_lock(&hash->pb_hash_lock);
			do_free = 0;
		}

		if (pb->pb_flags & PBF_DELWRI) {
			pb->pb_flags |= PBF_ASYNC;
			atomic_inc(&pb->pb_hold);
			pagebuf_delwri_queue(pb, 0);
			do_free = 0;
		} else if (pb->pb_flags & PBF_FS_MANAGED) {
			do_free = 0;
		}

		if (do_free) {
			list_del_init(&pb->pb_hash_list);
			spin_unlock(&hash->pb_hash_lock);
			pagebuf_free(pb);
		} else {
			spin_unlock(&hash->pb_hash_lock);
		}
	}
}


/*
 *	Mutual exclusion on buffers.  Locking model:
 *
 *	Buffers associated with inodes for which buffer locking
 *	is not enabled are not protected by semaphores, and are
 *	assumed to be exclusively owned by the caller.  There is a
 *	spinlock in the buffer, used by the caller when concurrent
 *	access is possible.
 */

/*
 *	pagebuf_cond_lock
 *
 *	pagebuf_cond_lock locks a buffer object, if it is not already locked.
 *	Note that this in no way
 *	locks the underlying pages, so it is only useful for synchronizing
 *	concurrent use of page buffer objects, not for synchronizing independent
 *	access to the underlying pages.
 */
int
pagebuf_cond_lock(			/* lock buffer, if not locked	*/
					/* returns -EBUSY if locked)	*/
	page_buf_t		*pb)
{
	int			locked;

	locked = down_trylock(&pb->pb_sema) == 0;
	if (locked) {
		PB_SET_OWNER(pb);
	}
	PB_TRACE(pb, "cond_lock", (long)locked);
	return(locked ? 0 : -EBUSY);
}

/*
 *	pagebuf_lock_value
 *
 *	Return lock value for a pagebuf
 */
int
pagebuf_lock_value(
	page_buf_t		*pb)
{
	return(atomic_read(&pb->pb_sema.count));
}

/*
 *	pagebuf_lock
 *
 *	pagebuf_lock locks a buffer object.  Note that this in no way
 *	locks the underlying pages, so it is only useful for synchronizing
 *	concurrent use of page buffer objects, not for synchronizing independent
 *	access to the underlying pages.
 */
int
pagebuf_lock(
	page_buf_t		*pb)
{
	PB_TRACE(pb, "lock", 0);
	if (atomic_read(&pb->pb_io_remaining))
		blk_run_queues();
	down(&pb->pb_sema);
	PB_SET_OWNER(pb);
	PB_TRACE(pb, "locked", 0);
	return 0;
}

/*
 *	pagebuf_unlock
 *
 *	pagebuf_unlock releases the lock on the buffer object created by
 *	pagebuf_lock or pagebuf_cond_lock (not any
 *	pinning of underlying pages created by pagebuf_pin).
 */
void
pagebuf_unlock(				/* unlock buffer		*/
	page_buf_t		*pb)	/* buffer to unlock		*/
{
	PB_CLEAR_OWNER(pb);
	up(&pb->pb_sema);
	PB_TRACE(pb, "unlock", 0);
}


/*
 *	Pinning Buffer Storage in Memory
 */

/*
 *	pagebuf_pin
 *
 *	pagebuf_pin locks all of the memory represented by a buffer in
 *	memory.  Multiple calls to pagebuf_pin and pagebuf_unpin, for
 *	the same or different buffers affecting a given page, will
 *	properly count the number of outstanding "pin" requests.  The
 *	buffer may be released after the pagebuf_pin and a different
 *	buffer used when calling pagebuf_unpin, if desired.
 *	pagebuf_pin should be used by the file system when it wants be
 *	assured that no attempt will be made to force the affected
 *	memory to disk.	 It does not assure that a given logical page
 *	will not be moved to a different physical page.
 */
void
pagebuf_pin(
	page_buf_t		*pb)
{
	atomic_inc(&pb->pb_pin_count);
	PB_TRACE(pb, "pin", (long)pb->pb_pin_count.counter);
}

/*
 *	pagebuf_unpin
 *
 *	pagebuf_unpin reverses the locking of memory performed by
 *	pagebuf_pin.  Note that both functions affected the logical
 *	pages associated with the buffer, not the buffer itself.
 */
void
pagebuf_unpin(
	page_buf_t		*pb)
{
	if (atomic_dec_and_test(&pb->pb_pin_count)) {
		wake_up_all(&pb->pb_waiters);
	}
	PB_TRACE(pb, "unpin", (long)pb->pb_pin_count.counter);
}

int
pagebuf_ispin(
	page_buf_t		*pb)
{
	return atomic_read(&pb->pb_pin_count);
}

/*
 *	pagebuf_wait_unpin
 *
 *	pagebuf_wait_unpin waits until all of the memory associated
 *	with the buffer is not longer locked in memory.  It returns
 *	immediately if none of the affected pages are locked.
 */
static inline void
_pagebuf_wait_unpin(
	page_buf_t		*pb)
{
	DECLARE_WAITQUEUE	(wait, current);

	if (atomic_read(&pb->pb_pin_count) == 0)
		return;

	add_wait_queue(&pb->pb_waiters, &wait);
	for (;;) {
		current->state = TASK_UNINTERRUPTIBLE;
		if (atomic_read(&pb->pb_pin_count) == 0)
			break;
		if (atomic_read(&pb->pb_io_remaining))
			blk_run_queues();
		schedule();
	}
	remove_wait_queue(&pb->pb_waiters, &wait);
	current->state = TASK_RUNNING;
}


/*
 *	Buffer Utility Routines
 */

/*
 *	pagebuf_iodone
 *
 *	pagebuf_iodone marks a buffer for which I/O is in progress
 *	done with respect to that I/O.	The pb_iodone routine, if
 *	present, will be called as a side-effect.
 */
void
pagebuf_iodone_sched(
	void			*v)
{
	page_buf_t		*pb = (page_buf_t *)v;

	if (pb->pb_iodone) {
		(*(pb->pb_iodone)) (pb);
		return;
	}

	if (pb->pb_flags & PBF_ASYNC) {
		if (!pb->pb_relse)
			pagebuf_unlock(pb);
		pagebuf_rele(pb);
	}
}

void
pagebuf_iodone(
	page_buf_t		*pb,
	int			dataio,
	int			schedule)
{
	pb->pb_flags &= ~(PBF_READ | PBF_WRITE);
	if (pb->pb_error == 0) {
		pb->pb_flags &= ~(PBF_PARTIAL | PBF_NONE);
	}

	PB_TRACE(pb, "iodone", pb->pb_iodone);

	if ((pb->pb_iodone) || (pb->pb_flags & PBF_ASYNC)) {
		if (schedule) {
			int	daemon = CPU_TO_DAEMON(smp_processor_id());

			INIT_TQUEUE(&pb->pb_iodone_sched,
				pagebuf_iodone_sched, (void *)pb);
			queue_task(&pb->pb_iodone_sched, dataio ?
				&pagebuf_dataiodone_tq[daemon] :
				&pagebuf_logiodone_tq[daemon]);
			wake_up(dataio ?
				&pagebuf_dataiodone_wait[daemon] :
				&pagebuf_logiodone_wait[daemon]);
		} else {
			pagebuf_iodone_sched(pb);
		}
	} else {
		up(&pb->pb_iodonesema);
	}
}

/*
 *	pagebuf_ioerror
 *
 *	pagebuf_ioerror sets the error code for a buffer.
 */
void
pagebuf_ioerror(			/* mark/clear buffer error flag */
	page_buf_t		*pb,	/* buffer to mark		*/
	unsigned int		error)	/* error to store (0 if none)	*/
{
	pb->pb_error = error;
	PB_TRACE(pb, "ioerror", (unsigned long)error);
}

/*
 *	pagebuf_iostart
 *
 *	pagebuf_iostart initiates I/O on a buffer, based on the flags supplied.
 *	If necessary, it will arrange for any disk space allocation required,
 *	and it will break up the request if the block mappings require it.
 *	The pb_iodone routine in the buffer supplied will only be called
 *	when all of the subsidiary I/O requests, if any, have been completed.
 *	pagebuf_iostart calls the pagebuf_ioinitiate routine or
 *	pagebuf_iorequest, if the former routine is not defined, to start
 *	the I/O on a given low-level request.
 */
int
pagebuf_iostart(			/* start I/O on a buffer	  */
	page_buf_t		*pb,	/* buffer to start		  */
	page_buf_flags_t	flags)	/* PBF_LOCK, PBF_ASYNC, PBF_READ, */
					/* PBF_WRITE, PBF_DELWRI,	  */
					/* PBF_DONT_BLOCK		  */
{
	int			status = 0;

	PB_TRACE(pb, "iostart", (unsigned long)flags);

	if (flags & PBF_DELWRI) {
		pb->pb_flags &= ~(PBF_READ | PBF_WRITE | PBF_ASYNC);
		pb->pb_flags |= flags & (PBF_DELWRI | PBF_ASYNC);
		pagebuf_delwri_queue(pb, 1);
		return status;
	}

	pb->pb_flags &= ~(PBF_READ | PBF_WRITE | PBF_ASYNC | PBF_DELWRI | \
			PBF_READ_AHEAD | PBF_RUN_QUEUES);
	pb->pb_flags |= flags & (PBF_READ | PBF_WRITE | PBF_ASYNC | \
			PBF_READ_AHEAD | PBF_RUN_QUEUES);

	BUG_ON(pb->pb_bn == PAGE_BUF_DADDR_NULL);

	/* For writes allow an alternate strategy routine to precede
	 * the actual I/O request (which may not be issued at all in
	 * a shutdown situation, for example).
	 */
	status = (flags & PBF_WRITE) ?
		pagebuf_iostrategy(pb) : pagebuf_iorequest(pb);

	/* Wait for I/O if we are not an async request.
	 * Note: async I/O request completion will release the buffer,
	 * and that can already be done by this point.  So using the
	 * buffer pointer from here on, after async I/O, is invalid.
	 */
	if (!status && !(flags & PBF_ASYNC))
		status = pagebuf_iowait(pb);

	return status;
}


/*
 * Helper routines for pagebuf_iorequest (pagebuf I/O completion)
 */

STATIC __inline__ int
_pagebuf_iolocked(
	page_buf_t		*pb)
{
	ASSERT(pb->pb_flags & (PBF_READ|PBF_WRITE));
	if (pb->pb_target->pbr_bsize < PAGE_CACHE_SIZE)
		return pb->pb_locked;
	if (pb->pb_flags & PBF_READ)
		return pb->pb_locked;
	return (pb->pb_flags & _PBF_PAGECACHE);
}

STATIC void
_pagebuf_iodone(
	page_buf_t		*pb,
	int			schedule)
{
	int			i;

	if (atomic_dec_and_test(&pb->pb_io_remaining) != 1)
		return;

	if (_pagebuf_iolocked(pb))
		for (i = 0; i < pb->pb_page_count; i++)
			unlock_page(pb->pb_pages[i]);
	pb->pb_locked = 0;
	pagebuf_iodone(pb, (pb->pb_flags & PBF_FS_DATAIOD), schedule);
}

STATIC void
_end_io_pagebuf(
	struct buffer_head	*bh,
	int			uptodate,
	int			fullpage)
{
	struct page		*page = bh->b_page;
	page_buf_t		*pb = (page_buf_t *)bh->b_private;

	mark_buffer_uptodate(bh, uptodate);
	put_bh(bh);

	if (!uptodate) {
		SetPageError(page);
		pb->pb_error = EIO;
	}

	if (fullpage) {
		unlock_buffer(bh);
		_pagebuf_free_bh(bh);
		if (!PageError(page))
			SetPageUptodate(page);
	} else {
		static spinlock_t page_uptodate_lock = SPIN_LOCK_UNLOCKED;
		struct buffer_head *bp;
		unsigned long flags;

		ASSERT(PageLocked(page));
		spin_lock_irqsave(&page_uptodate_lock, flags);
		clear_buffer_async(bh);
		unlock_buffer(bh);
		for (bp = bh->b_this_page; bp != bh; bp = bp->b_this_page) {
			if (buffer_locked(bp)) {
				if (buffer_async(bp))
					break;
			} else if (!buffer_uptodate(bp))
				break;
		}
		spin_unlock_irqrestore(&page_uptodate_lock, flags);
		if (bp == bh && !PageError(page))
			SetPageUptodate(page);
	}

	_pagebuf_iodone(pb, 1);
}

STATIC void
_pagebuf_end_io_complete_pages(
	struct buffer_head	*bh,
	int			uptodate)
{
	_end_io_pagebuf(bh, uptodate, 1);
}

STATIC void
_pagebuf_end_io_partial_pages(
	struct buffer_head	*bh,
	int			uptodate)
{
	_end_io_pagebuf(bh, uptodate, 0);
}


/*
 * Initiate I/O on part of a page we are interested in
 */
STATIC int
_pagebuf_page_io(
	struct page		*page,	/* Page structure we are dealing with */
	pb_target_t		*pbr,	/* device parameters (bsz, ssz, dev) */
	page_buf_t		*pb,	/* pagebuf holding it, can be NULL */
	page_buf_daddr_t	bn,	/* starting block number */
	size_t			pg_offset,	/* starting offset in page */
	size_t			pg_length,	/* count of data to process */
	int			rw,	/* read/write operation */
	int			flush)
{
	size_t			sector;
	size_t			blk_length = 0;
	struct buffer_head	*bh, *head, *bufferlist[MAX_BUF_PER_PAGE];
	int			sector_shift = pbr->pbr_sshift;
	int			i = 0, cnt = 0;
	int			public_bh = 0;
	int			multi_ok;

	if ((pbr->pbr_bsize < PAGE_CACHE_SIZE) &&
	    !(pb->pb_flags & _PBF_PRIVATE_BH)) {
		int		cache_ok;

		cache_ok = !((pb->pb_flags & PBF_FORCEIO) || (rw == WRITE));
		public_bh = multi_ok = 1;
		sector = 1 << sector_shift;

		ASSERT(PageLocked(page));
		if (!page_has_buffers(page))
			create_empty_buffers(page, pbr->pbr_kdev, sector);

		i = sector >> BBSHIFT;
		bn -= (pg_offset >> BBSHIFT);

		/* Find buffer_heads belonging to just this pagebuf */
		bh = head = page_buffers(page);
		do {
			if (buffer_uptodate(bh) && cache_ok)
				continue;
			if (blk_length < pg_offset)
				continue;
			if (blk_length >= pg_offset + pg_length)
				break;

			lock_buffer(bh);
			get_bh(bh);
			bh->b_size = sector;
			bh->b_blocknr = bn;
			bufferlist[cnt++] = bh;

		} while ((bn += i),
			 (blk_length += sector),
			  (bh = bh->b_this_page) != head);

		goto request;
	}

	/* Calculate the block offsets and length we will be using */
	if (pg_offset) {
		size_t		block_offset;

		block_offset = pg_offset >> sector_shift;
		block_offset = pg_offset - (block_offset << sector_shift);
		blk_length = (pg_length + block_offset + pbr->pbr_smask) >>
								sector_shift;
	} else {
		blk_length = (pg_length + pbr->pbr_smask) >> sector_shift;
	}

	/* This will attempt to make a request bigger than the sector
	 * size if we are well aligned.
	 */
	switch (pb->pb_target->pbr_flags) {
	case 0:
		sector = blk_length << sector_shift;
		blk_length = 1;
		break;
	case PBR_ALIGNED_ONLY:
		if ((pg_offset == 0) && (pg_length == PAGE_CACHE_SIZE) &&
		    (((unsigned int) bn) & BN_ALIGN_MASK) == 0) {
			sector = blk_length << sector_shift;
			blk_length = 1;
			break;
		}
	case PBR_SECTOR_ONLY:
		/* Fallthrough, same as default */
	default:
		sector = 1 << sector_shift;
	}

	/* If we are doing I/O larger than the bh->b_size field then
	 * we need to split this request up.
	 */
	while (sector > ((1ULL << NBBY * sizeof(bh->b_size)) - 1)) {
		sector >>= 1;
		blk_length++;
	}

	multi_ok = (blk_length != 1);
	i = sector >> BBSHIFT;

	for (; blk_length > 0; bn += i, blk_length--, pg_offset += sector) {
		bh = kmem_cache_alloc(bh_cachep, SLAB_NOFS);
		if (!bh)
			bh = _pagebuf_get_prealloc_bh();
		memset(bh, 0, sizeof(*bh));
		bh->b_blocknr = bn;
		bh->b_size = sector;
		bh->b_dev = pbr->pbr_kdev;
		set_buffer_locked(bh);
		set_bh_page(bh, page, pg_offset);
		init_waitqueue_head(&bh->b_wait);
		atomic_set(&bh->b_count, 1);
		bufferlist[cnt++] = bh;
	}

request:
	if (cnt) {
		void	(*callback)(struct buffer_head *, int);

		callback = (multi_ok && public_bh) ?
				_pagebuf_end_io_partial_pages :
				_pagebuf_end_io_complete_pages;

		/* Account for additional buffers in progress */
		atomic_add(cnt, &pb->pb_io_remaining);

#ifdef RQ_WRITE_ORDERED
		if (flush)
			set_bit(BH_Ordered_Flush, &bufferlist[cnt-1]->b_state);
#endif

		for (i = 0; i < cnt; i++) {
			bh = bufferlist[i];
			init_buffer(bh, callback, pb);
			bh->b_rdev = bh->b_dev;
			bh->b_rsector = bh->b_blocknr;
			set_buffer_mapped(bh);
			set_buffer_async(bh);
			set_buffer_req(bh);
			if (rw == WRITE)
				set_buffer_uptodate(bh);
			generic_make_request(rw, bh);
		}
		return 0;
	}

	/*
	 * We have no I/O to submit, let the caller know that
	 * we have skipped over this page entirely.
	 */
	return 1;
}

STATIC void
_pagebuf_page_apply(
	page_buf_t		*pb,
	loff_t			offset,
	struct page		*page,
	size_t			pg_offset,
	size_t			pg_length,
	int			last)
{
	page_buf_daddr_t	bn = pb->pb_bn;
	pb_target_t		*pbr = pb->pb_target;
	loff_t			pb_offset;
	int			status, locking;

	ASSERT(page);
	ASSERT(pb->pb_flags & (PBF_READ|PBF_WRITE));

	if ((pbr->pbr_bsize == PAGE_CACHE_SIZE) &&
	    (pb->pb_buffer_length < PAGE_CACHE_SIZE) &&
	    (pb->pb_flags & PBF_READ) && pb->pb_locked) {
		bn -= (pb->pb_offset >> BBSHIFT);
		pg_offset = 0;
		pg_length = PAGE_CACHE_SIZE;
	} else {
		pb_offset = offset - pb->pb_file_offset;
		if (pb_offset) {
			bn += (pb_offset + BBMASK) >> BBSHIFT;
		}
	}

	locking = _pagebuf_iolocked(pb);
	if (pb->pb_flags & PBF_WRITE) {
		if (locking && !pb->pb_locked)
			lock_page(page);
		status = _pagebuf_page_io(page, pbr, pb, bn,
				pg_offset, pg_length, WRITE,
				last && (pb->pb_flags & PBF_FLUSH));
	} else {
		status = _pagebuf_page_io(page, pbr, pb, bn,
				pg_offset, pg_length, READ, 0);
	}
	if (status && locking && !(pb->pb_target->pbr_bsize < PAGE_CACHE_SIZE))
		unlock_page(page);
}

/*
 *	pagebuf_iorequest
 *
 *	pagebuf_iorequest is the core I/O request routine.
 *	It assumes that the buffer is well-formed and
 *	mapped and ready for physical I/O, unlike
 *	pagebuf_iostart() and pagebuf_iophysio().  Those
 *	routines call the pagebuf_ioinitiate routine to start I/O,
 *	if it is present, or else call pagebuf_iorequest()
 *	directly if the pagebuf_ioinitiate routine is not present.
 *
 *	This function will be responsible for ensuring access to the
 *	pages is restricted whilst I/O is in progress - for locking
 *	pagebufs the pagebuf lock is the mediator, for non-locking
 *	pagebufs the pages will be locked. In the locking case we
 *	need to use the pagebuf lock as multiple meta-data buffers
 *	will reference the same page.
 */
int
pagebuf_iorequest(			/* start real I/O		*/
	page_buf_t		*pb)	/* buffer to convey to device	*/
{
	PB_TRACE(pb, "iorequest", 0);

	if (pb->pb_flags & PBF_DELWRI) {
		pagebuf_delwri_queue(pb, 1);
		return 0;
	}

	if (pb->pb_flags & PBF_WRITE) {
		_pagebuf_wait_unpin(pb);
	}

	pagebuf_hold(pb);

	/* Set the count to 1 initially, this will stop an I/O
	 * completion callout which happens before we have started
	 * all the I/O from calling pagebuf_iodone too early.
	 */
	atomic_set(&pb->pb_io_remaining, 1);
	_pagebuf_ioapply(pb);
	_pagebuf_iodone(pb, 0);

	pagebuf_rele(pb);
	return 0;
}

/*
 *	pagebuf_iowait
 *
 *	pagebuf_iowait waits for I/O to complete on the buffer supplied.
 *	It returns immediately if no I/O is pending.  In any case, it returns
 *	the error code, if any, or 0 if there is no error.
 */
int
pagebuf_iowait(
	page_buf_t		*pb)
{
	PB_TRACE(pb, "iowait", 0);
	if (atomic_read(&pb->pb_io_remaining))
		blk_run_queues();
	if ((pb->pb_flags & PBF_FS_DATAIOD))
		pagebuf_runall_queues(pagebuf_dataiodone_tq);
	down(&pb->pb_iodonesema);
	PB_TRACE(pb, "iowaited", (long)pb->pb_error);
	return pb->pb_error;
}

STATIC void *
pagebuf_mapout_locked(
	page_buf_t		*pb)
{
	void			*old_addr = NULL;

	if (pb->pb_flags & PBF_MAPPED) {
		if (pb->pb_flags & _PBF_ADDR_ALLOCATED)
			old_addr = pb->pb_addr - pb->pb_offset;
		pb->pb_addr = NULL;
		pb->pb_flags &= ~(PBF_MAPPED | _PBF_ADDR_ALLOCATED);
	}

	return old_addr;	/* Caller must free the address space,
				 * we are under a spin lock, probably
				 * not safe to do vfree here
				 */
}

caddr_t
pagebuf_offset(
	page_buf_t		*pb,
	size_t			offset)
{
	struct page		*page;

	offset += pb->pb_offset;

	page = pb->pb_pages[offset >> PAGE_CACHE_SHIFT];
	return (caddr_t) page_address(page) + (offset & (PAGE_CACHE_SIZE - 1));
}

/*
 *	pagebuf_iomove
 *
 *	Move data into or out of a buffer.
 */
void
pagebuf_iomove(
	page_buf_t		*pb,	/* buffer to process		*/
	size_t			boff,	/* starting buffer offset	*/
	size_t			bsize,	/* length to copy		*/
	caddr_t			data,	/* data address			*/
	page_buf_rw_t		mode)	/* read/write flag		*/
{
	size_t			bend, cpoff, csize;
	struct page		*page;

	bend = boff + bsize;
	while (boff < bend) {
		page = pb->pb_pages[page_buf_btoct(boff + pb->pb_offset)];
		cpoff = page_buf_poff(boff + pb->pb_offset);
		csize = min_t(size_t,
			      PAGE_CACHE_SIZE-cpoff, pb->pb_count_desired-boff);

		ASSERT(((csize + cpoff) <= PAGE_CACHE_SIZE));

		switch (mode) {
		case PBRW_ZERO:
			memset(page_address(page) + cpoff, 0, csize);
			break;
		case PBRW_READ:
			memcpy(data, page_address(page) + cpoff, csize);
			break;
		case PBRW_WRITE:
			memcpy(page_address(page) + cpoff, data, csize);
		}

		boff += csize;
		data += csize;
	}
}

/*
 *	_pagebuf_ioapply
 *
 *	Applies _pagebuf_page_apply to each page of the page_buf_t.
 */
STATIC void
_pagebuf_ioapply(			/* apply function to pages	*/
	page_buf_t		*pb)	/* buffer to examine		*/
{
	int			index;
	loff_t			buffer_offset = pb->pb_file_offset;
	size_t			buffer_len = pb->pb_count_desired;
	size_t			page_offset, len;
	size_t			cur_offset, cur_len;

	cur_offset = pb->pb_offset;
	cur_len = buffer_len;

	if (!pb->pb_locked &&
	    (pb->pb_target->pbr_bsize < PAGE_CACHE_SIZE)) {
		for (index = 0; index < pb->pb_page_count; index++)
			lock_page(pb->pb_pages[index]);
		pb->pb_locked = 1;
	}

	for (index = 0; index < pb->pb_page_count; index++) {
		if (cur_len == 0)
			break;
		if (cur_offset >= PAGE_CACHE_SIZE) {
			cur_offset -= PAGE_CACHE_SIZE;
			continue;
		}

		page_offset = cur_offset;
		cur_offset = 0;

		len = PAGE_CACHE_SIZE - page_offset;
		if (len > cur_len)
			len = cur_len;
		cur_len -= len;

		_pagebuf_page_apply(pb, buffer_offset,
				pb->pb_pages[index], page_offset, len,
				index + 1 == pb->pb_page_count);
		buffer_offset += len;
		buffer_len -= len;
	}

	/*
	 * Run the block device task queue here, while we have
	 * a hold on the pagebuf (important to have that hold).
	 */
	if (pb->pb_flags & PBF_RUN_QUEUES) {
		pb->pb_flags &= ~PBF_RUN_QUEUES;
		if (atomic_read(&pb->pb_io_remaining) > 1)
			blk_run_queues();
	}
}


/*
 * Pagebuf delayed write buffer handling
 */

STATIC LIST_HEAD(pbd_delwrite_queue);
STATIC spinlock_t pbd_delwrite_lock = SPIN_LOCK_UNLOCKED;

STATIC void
pagebuf_delwri_queue(
	page_buf_t		*pb,
	int			unlock)
{
	PB_TRACE(pb, "delwri_q", (long)unlock);
	spin_lock(&pbd_delwrite_lock);
	/* If already in the queue, dequeue and place at tail */
	if (!list_empty(&pb->pb_list)) {
		if (unlock) {
			atomic_dec(&pb->pb_hold);
		}
		list_del(&pb->pb_list);
	}

	list_add_tail(&pb->pb_list, &pbd_delwrite_queue);
	pb->pb_flushtime = jiffies + xfs_age_buffer;
	spin_unlock(&pbd_delwrite_lock);

	if (unlock)
		pagebuf_unlock(pb);
}

void
pagebuf_delwri_dequeue(
	page_buf_t		*pb)
{
	PB_TRACE(pb, "delwri_uq", 0);
	spin_lock(&pbd_delwrite_lock);
	list_del_init(&pb->pb_list);
	pb->pb_flags &= ~PBF_DELWRI;
	spin_unlock(&pbd_delwrite_lock);
}


/*
 * The pagebuf iodone daemons
 */

STATIC int
pagebuf_iodone_daemon(
	void			*__bind_cpu,
	const char		*name,
	int			pagebuf_daemons[],
	struct list_head	pagebuf_iodone_tq[],
	wait_queue_head_t	pagebuf_iodone_wait[])
{
	int			bind_cpu, cpu;
	DECLARE_WAITQUEUE	(wait, current);

	bind_cpu = (int) (long)__bind_cpu;
	cpu = CPU_TO_DAEMON(cpu_logical_map(bind_cpu));

	/*  Set up the thread  */
	daemonize();

	/* Avoid signals */
	sigmask_lock();
	sigfillset(&current->blocked);
	__recalc_sigpending(current);
	sigmask_unlock();

	/* Migrate to the right CPU */
	migrate_to_cpu(cpu);
#ifdef __HAVE_NEW_SCHEDULER
	if (smp_processor_id() != cpu)
		BUG();
#else
	while (smp_processor_id() != cpu)
		schedule();
#endif

	sprintf(current->comm, "%s/%d", name, bind_cpu);
	INIT_LIST_HEAD(&pagebuf_iodone_tq[cpu]);
	init_waitqueue_head(&pagebuf_iodone_wait[cpu]);
	__set_current_state(TASK_INTERRUPTIBLE);
	mb();

	pagebuf_daemons[cpu] = 1;

	for (;;) {
		add_wait_queue(&pagebuf_iodone_wait[cpu], &wait);

		if (TQ_ACTIVE(pagebuf_iodone_tq[cpu]))
			__set_task_state(current, TASK_RUNNING);
		schedule();
		remove_wait_queue(&pagebuf_iodone_wait[cpu], &wait);
		run_task_queue(&pagebuf_iodone_tq[cpu]);
		if (pagebuf_daemons[cpu] == 0)
			break;
		__set_current_state(TASK_INTERRUPTIBLE);
	}

	pagebuf_daemons[cpu] = -1;
	wake_up_interruptible(&pagebuf_iodone_wait[cpu]);
	return 0;
}

STATIC void
pagebuf_runall_queues(
	struct list_head	pagebuf_iodone_tq[])
{
	int	pcpu, cpu;

	for (cpu = 0; cpu < min(smp_num_cpus, MAX_IO_DAEMONS); cpu++) {
		pcpu = CPU_TO_DAEMON(cpu_logical_map(cpu));

		run_task_queue(&pagebuf_iodone_tq[pcpu]);
	}
}

STATIC int
pagebuf_logiodone_daemon(
	void			*__bind_cpu)
{
	return pagebuf_iodone_daemon(__bind_cpu, "xfslogd", pb_logio_daemons,
			pagebuf_logiodone_tq, pagebuf_logiodone_wait);
}

STATIC int
pagebuf_dataiodone_daemon(
	void			*__bind_cpu)
{
	return pagebuf_iodone_daemon(__bind_cpu, "xfsdatad", pb_dataio_daemons,
			pagebuf_dataiodone_tq, pagebuf_dataiodone_wait);
}


/* Defines for pagebuf daemon */
STATIC DECLARE_COMPLETION(pagebuf_daemon_done);
STATIC struct task_struct *pagebuf_daemon_task;
STATIC int pagebuf_daemon_active;
STATIC int force_flush;

STATIC void
pagebuf_daemon_wakeup(void)
{
	force_flush = 1;
	barrier();
	wake_up_process(pagebuf_daemon_task);
}

STATIC int
pagebuf_daemon(
	void			*data)
{
	int			count;
	page_buf_t		*pb;
	struct list_head	*curr, *next, tmp;

	/*  Set up the thread  */
	daemonize();

	/* Mark it active */
	pagebuf_daemon_task = current;
	pagebuf_daemon_active = 1;
	barrier();

	/* Avoid signals */
	sigmask_lock();
	sigfillset(&current->blocked);
	__recalc_sigpending(current);
	sigmask_unlock();

	strcpy(current->comm, "xfsbufd");
	current->flags |= PF_MEMALLOC;

	INIT_LIST_HEAD(&tmp);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(xfs_flush_interval);

		spin_lock(&pbd_delwrite_lock);

		count = 0;
		list_for_each_safe(curr, next, &pbd_delwrite_queue) {
			pb = list_entry(curr, page_buf_t, pb_list);

			PB_TRACE(pb, "walkq1", (long)pagebuf_ispin(pb));

			if ((pb->pb_flags & PBF_DELWRI) &&
			     !pagebuf_ispin(pb) && !pagebuf_cond_lock(pb)) {
				if (!force_flush &&
				    time_before(jiffies, pb->pb_flushtime)) {
					pagebuf_unlock(pb);
					break;
				}

				pb->pb_flags &= ~PBF_DELWRI;
				pb->pb_flags |= PBF_WRITE;
				list_move(&pb->pb_list, &tmp);
				count++;
			}
		}

		spin_unlock(&pbd_delwrite_lock);
		while (!list_empty(&tmp)) {
			pb = list_entry(tmp.next, page_buf_t, pb_list);
			list_del_init(&pb->pb_list);

			pagebuf_iostrategy(pb);
		}

		if (as_list_len > 0)
			purge_addresses();
		if (count)
			blk_run_queues();

		force_flush = 0;
	} while (pagebuf_daemon_active);

	complete_and_exit(&pagebuf_daemon_done, 0);
}

void
pagebuf_delwri_flush(
	pb_target_t		*target,
	u_long			flags,
	int			*pinptr)
{
	page_buf_t		*pb;
	struct list_head	*curr, *next, tmp;
	int			pincount = 0;
	int			flush_cnt = 0;

	pagebuf_runall_queues(pagebuf_dataiodone_tq);
	pagebuf_runall_queues(pagebuf_logiodone_tq);

	spin_lock(&pbd_delwrite_lock);
	INIT_LIST_HEAD(&tmp);

	list_for_each_safe(curr, next, &pbd_delwrite_queue) {
		pb = list_entry(curr, page_buf_t, pb_list);

		/*
		 * Skip other targets, markers and in progress buffers
		 */

		if ((pb->pb_flags == 0) || (pb->pb_target != target) ||
		    !(pb->pb_flags & PBF_DELWRI)) {
			continue;
		}

		PB_TRACE(pb, "walkq2", (long)pagebuf_ispin(pb));
		if (pagebuf_ispin(pb)) {
			pincount++;
			continue;
		}

		pb->pb_flags &= ~PBF_DELWRI;
		pb->pb_flags |= PBF_WRITE;
		list_move(&pb->pb_list, &tmp);

	}
	
	/* ok found all the items that can be worked on 
	 * drop the lock and process the private list */
	spin_unlock(&pbd_delwrite_lock);
	
	list_for_each_safe(curr, next, &tmp) {
		pb = list_entry(curr, page_buf_t, pb_list);
		
		if (flags & PBDF_WAIT)
			pb->pb_flags &= ~PBF_ASYNC;
		else
			list_del_init(curr);

		pagebuf_lock(pb);
		pagebuf_iostrategy(pb);

		if (++flush_cnt > 32) {
			blk_run_queues();
			flush_cnt = 0;
		}
	}

	blk_run_queues();

	/* must run list the second time even if PBDF_WAIT isn't
	 * to reset all the pb_list pointers 
	 */
	while (!list_empty(&tmp)) {
		pb = list_entry(tmp.next, page_buf_t, pb_list);

		list_del_init(&pb->pb_list);
		
		pagebuf_iowait(pb);
		if (!pb->pb_relse)
			pagebuf_unlock(pb);
		pagebuf_rele(pb);
	}

	if (pinptr)
		*pinptr = pincount;
}

STATIC int
pagebuf_daemon_start(void)
{
	int		cpu, pcpu;

	kernel_thread(pagebuf_daemon, NULL, CLONE_FS|CLONE_FILES|CLONE_VM);

	for (cpu = 0; cpu < min(smp_num_cpus, MAX_IO_DAEMONS); cpu++) {
		pcpu = CPU_TO_DAEMON(cpu_logical_map(cpu));

		if (kernel_thread(pagebuf_logiodone_daemon,
				(void *)(long) cpu,
				CLONE_FS|CLONE_FILES|CLONE_VM) < 0) {
			printk("pagebuf_logiodone daemon failed to start\n");
		} else {
			while (!pb_logio_daemons[pcpu])
				yield();
		}
	}
	for (cpu = 0; cpu < min(smp_num_cpus, MAX_IO_DAEMONS); cpu++) {
		pcpu = CPU_TO_DAEMON(cpu_logical_map(cpu));

		if (kernel_thread(pagebuf_dataiodone_daemon,
				(void *)(long) cpu,
				CLONE_FS|CLONE_FILES|CLONE_VM) < 0) {
			printk("pagebuf_dataiodone daemon failed to start\n");
		} else {
			while (!pb_dataio_daemons[pcpu])
				yield();
		}
	}
	return 0;
}

/*
 * pagebuf_daemon_stop
 *
 * Note: do not mark as __exit, it is called from pagebuf_terminate.
 */
STATIC void
pagebuf_daemon_stop(void)
{
	int		cpu, pcpu;

	pagebuf_daemon_active = 0;
	barrier();
	wait_for_completion(&pagebuf_daemon_done);

	for (pcpu = 0; pcpu < min(smp_num_cpus, MAX_IO_DAEMONS); pcpu++) {
		cpu = CPU_TO_DAEMON(cpu_logical_map(pcpu));

		pb_logio_daemons[cpu] = 0;
		wake_up(&pagebuf_logiodone_wait[cpu]);
		wait_event_interruptible(pagebuf_logiodone_wait[cpu],
				pb_logio_daemons[cpu] == -1);

		pb_dataio_daemons[cpu] = 0;
		wake_up(&pagebuf_dataiodone_wait[cpu]);
		wait_event_interruptible(pagebuf_dataiodone_wait[cpu],
				pb_dataio_daemons[cpu] == -1);
	}
}


STATIC int
pagebuf_shaker(int number, unsigned int mask)
{
	pagebuf_daemon_wakeup();
	return 0;
}


/*
 *	Initialization and Termination
 */

int __init
pagebuf_init(void)
{
	int			i;

	pagebuf_cache = kmem_cache_create("page_buf_t", sizeof(page_buf_t), 0,
			SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (pagebuf_cache == NULL) {
		printk("pagebuf: couldn't init pagebuf cache\n");
		pagebuf_terminate();
		return -ENOMEM;
	}

	if (_pagebuf_prealloc_bh(NR_RESERVED_BH) < NR_RESERVED_BH) {
		printk("pagebuf: couldn't pre-allocate %d buffer heads\n",
			NR_RESERVED_BH);
		pagebuf_terminate();
		return -ENOMEM;
	}

	init_waitqueue_head(&pb_resv_bh_wait);

	for (i = 0; i < NHASH; i++) {
		spin_lock_init(&pbhash[i].pb_hash_lock);
		INIT_LIST_HEAD(&pbhash[i].pb_hash);
	}

#ifdef PAGEBUF_TRACE
	pagebuf_trace_buf = ktrace_alloc(PAGEBUF_TRACE_SIZE, KM_SLEEP);
#endif

	pagebuf_daemon_start();
	kmem_shake_register(pagebuf_shaker);
	return 0;
}

/*
 *	pagebuf_terminate.
 *
 *	Note: do not mark as __exit, this is also called from the __init code.
 */
void
pagebuf_terminate(void)
{
	pagebuf_daemon_stop();

#ifdef PAGEBUF_TRACE
	ktrace_free(pagebuf_trace_buf);
#endif

	kmem_cache_destroy(pagebuf_cache);
	kmem_shake_deregister(pagebuf_shaker);
}
