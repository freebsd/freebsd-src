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
 * Written by Steve Lord, Jim Mostek, Russell Cattelan at SGI
 */

#ifndef __XFS_BUF_H__
#define __XFS_BUF_H__

#include <linux/version.h>
#include <linux/config.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm/system.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uio.h>

/* nptl patch changes where the sigmask_lock is defined */
#ifdef CLONE_SIGNAL /* stock */
#define sigmask_lock()		spin_lock_irq(&current->sigmask_lock);
#define sigmask_unlock()	spin_unlock_irq(&current->sigmask_lock);
#define __recalc_sigpending(x)	recalc_sigpending(x)
#else /* nptl */
#define sigmask_lock()		spin_lock_irq(&current->sighand->siglock);
#define sigmask_unlock()	spin_unlock_irq(&current->sighand->siglock);
#define __recalc_sigpending(x)	recalc_sigpending()
#endif
/*
 *	Base types
 */

/* daddr must be signed since -1 is used for bmaps that are not yet allocated */
typedef loff_t page_buf_daddr_t;

#define PAGE_BUF_DADDR_NULL ((page_buf_daddr_t) (-1LL))

#define page_buf_ctob(pp)	((pp) * PAGE_CACHE_SIZE)
#define page_buf_btoc(dd)	(((dd) + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT)
#define page_buf_btoct(dd)	((dd) >> PAGE_CACHE_SHIFT)
#define page_buf_poff(aa)	((aa) & ~PAGE_CACHE_MASK)

typedef enum page_buf_rw_e {
	PBRW_READ = 1,			/* transfer into target memory */
	PBRW_WRITE = 2,			/* transfer from target memory */
	PBRW_ZERO = 3			/* Zero target memory */
} page_buf_rw_t;


typedef enum page_buf_flags_e {		/* pb_flags values */
	PBF_READ = (1 << 0),	/* buffer intended for reading from device */
	PBF_WRITE = (1 << 1),	/* buffer intended for writing to device   */
	PBF_MAPPED = (1 << 2),  /* buffer mapped (pb_addr valid)           */
	PBF_PARTIAL = (1 << 3), /* buffer partially read                   */
	PBF_ASYNC = (1 << 4),   /* initiator will not wait for completion  */
	PBF_NONE = (1 << 5),    /* buffer not read at all                  */
	PBF_DELWRI = (1 << 6),  /* buffer has dirty pages                  */
	PBF_STALE = (1 << 10),	/* buffer has been staled, do not find it  */
	PBF_FS_MANAGED = (1 << 11), /* filesystem controls freeing memory  */
	PBF_FS_DATAIOD = (1 << 12), /* schedule IO completion on fs datad  */

	/* flags used only as arguments to access routines */
	PBF_LOCK = (1 << 13),	/* lock requested			   */
	PBF_TRYLOCK = (1 << 14), /* lock requested, but do not wait	   */
	PBF_DONT_BLOCK = (1 << 15), /* do not block in current thread	   */

	/* flags used only internally */
	_PBF_PAGECACHE = (1 << 16),	/* backed by pagecache		   */
	_PBF_PRIVATE_BH = (1 << 17), /* do not use public buffer heads	   */
	_PBF_ALL_PAGES_MAPPED = (1 << 18), /* all pages in range mapped	   */
	_PBF_ADDR_ALLOCATED = (1 << 19), /* pb_addr space was allocated	   */
	_PBF_MEM_ALLOCATED = (1 << 20), /* underlying pages are allocated  */
	_PBF_MEM_SLAB = (1 << 21), /* underlying pages are slab allocated  */

	PBF_FORCEIO = (1 << 22), /* ignore any cache state		   */
	PBF_FLUSH = (1 << 23),	/* flush disk write cache		   */
	PBF_READ_AHEAD = (1 << 24), /* asynchronous read-ahead		   */
	PBF_RUN_QUEUES = (1 << 25), /* run block device task queue	   */
	PBF_DIRECTIO = (1 << 26), /* used for a direct IO mapping	   */

} page_buf_flags_t;

#define PBF_UPDATE (PBF_READ | PBF_WRITE)
#define PBF_NOT_DONE(pb) (((pb)->pb_flags & (PBF_PARTIAL|PBF_NONE)) != 0)
#define PBF_DONE(pb) (((pb)->pb_flags & (PBF_PARTIAL|PBF_NONE)) == 0)

#define PBR_SECTOR_ONLY	1	/* only use sector size buffer heads */
#define PBR_ALIGNED_ONLY 2	/* only use aligned I/O */

typedef struct pb_target {
	int			pbr_flags;
	dev_t			pbr_dev;
	kdev_t			pbr_kdev;
	struct block_device	*pbr_bdev;
	struct address_space	*pbr_mapping;
	unsigned int		pbr_bsize;
	unsigned int		pbr_sshift;
	size_t			pbr_smask;
} pb_target_t;

/*
 *	page_buf_t:  Buffer structure for page cache-based buffers
 *
 * This buffer structure is used by the page cache buffer management routines
 * to refer to an assembly of pages forming a logical buffer.  The actual
 * I/O is performed with buffer_head or bio structures, as required by drivers,
 * for drivers which do not understand this structure.  The buffer structure is
 * used on temporary basis only, and discarded when released.
 *
 * The real data storage is recorded in the page cache.  Metadata is
 * hashed to the inode for the block device on which the file system resides.
 * File data is hashed to the inode for the file.  Pages which are only
 * partially filled with data have bits set in their block_map entry
 * to indicate which disk blocks in the page are not valid.
 */

struct page_buf_s;
typedef void (*page_buf_iodone_t)(struct page_buf_s *);
			/* call-back function on I/O completion */
typedef void (*page_buf_relse_t)(struct page_buf_s *);
			/* call-back function on I/O completion */
typedef int (*page_buf_bdstrat_t)(struct page_buf_s *);

#define PB_PAGES	4

typedef struct page_buf_s {
	struct semaphore	pb_sema;	/* semaphore for lockables  */
	unsigned long		pb_flushtime;	/* time to flush pagebuf    */
	atomic_t		pb_pin_count;	/* pin count		    */
	wait_queue_head_t	pb_waiters;	/* unpin waiters	    */
	struct list_head	pb_list;
	page_buf_flags_t	pb_flags;	/* status flags */
	struct list_head	pb_hash_list;
	struct pb_target	*pb_target;	/* logical object */
	atomic_t		pb_hold;	/* reference count */
	page_buf_daddr_t	pb_bn;		/* block number for I/O */
	loff_t			pb_file_offset;	/* offset in file */
	size_t			pb_buffer_length; /* size of buffer in bytes */
	size_t			pb_count_desired; /* desired transfer size */
	void			*pb_addr;	/* virtual address of buffer */
	struct tq_struct	pb_iodone_sched;
	atomic_t		pb_io_remaining;/* #outstanding I/O requests */
	page_buf_iodone_t	pb_iodone;	/* I/O completion function */
	page_buf_relse_t	pb_relse;	/* releasing function */
	page_buf_bdstrat_t	pb_strat;	/* pre-write function */
	struct semaphore	pb_iodonesema;	/* Semaphore for I/O waiters */
	void			*pb_fspriv;
	void			*pb_fspriv2;
	void			*pb_fspriv3;
	unsigned short		pb_error;	/* error code on I/O */
	unsigned short		pb_page_count;	/* size of page array */
	unsigned short		pb_offset;	/* page offset in first page */
	unsigned char		pb_locked;	/* page array is locked */
	unsigned char		pb_hash_index;	/* hash table index	*/
	struct page		**pb_pages;	/* array of page pointers */
	struct page		*pb_page_array[PB_PAGES]; /* inline pages */
#ifdef PAGEBUF_LOCK_TRACKING
	int			pb_last_holder;
#endif
} page_buf_t;


/* Finding and Reading Buffers */

extern page_buf_t *pagebuf_find(	/* find buffer for block if	*/
					/* the block is in memory	*/
		struct pb_target *,	/* inode for block		*/
		loff_t,			/* starting offset of range	*/
		size_t,			/* length of range		*/
		page_buf_flags_t);	/* PBF_LOCK			*/

extern page_buf_t *pagebuf_get(		/* allocate a buffer		*/
		struct pb_target *,	/* inode for buffer		*/
		loff_t,			/* starting offset of range     */
		size_t,			/* length of range              */
		page_buf_flags_t);	/* PBF_LOCK, PBF_READ,		*/
					/* PBF_ASYNC			*/

extern page_buf_t *pagebuf_lookup(
		struct pb_target *,
		loff_t,			/* starting offset of range	*/
		size_t,			/* length of range		*/
		page_buf_flags_t);	/* PBF_READ, PBF_WRITE,		*/
					/* PBF_FORCEIO, 		*/

extern page_buf_t *pagebuf_get_empty(	/* allocate pagebuf struct with	*/
					/*  no memory or disk address	*/
		size_t len,
		struct pb_target *);	/* mount point "fake" inode	*/

extern page_buf_t *pagebuf_get_no_daddr(/* allocate pagebuf struct	*/
					/* without disk address		*/
		size_t len,
		struct pb_target *);	/* mount point "fake" inode	*/

extern int pagebuf_associate_memory(
		page_buf_t *,
		void *,
		size_t);

extern void pagebuf_hold(		/* increment reference count	*/
		page_buf_t *);		/* buffer to hold		*/

extern void pagebuf_readahead(		/* read ahead into cache	*/
		struct pb_target  *,	/* target for buffer (or NULL)	*/
		loff_t,			/* starting offset of range     */
		size_t,			/* length of range              */
		page_buf_flags_t);	/* additional read flags	*/

/* Releasing Buffers */

extern void pagebuf_free(		/* deallocate a buffer		*/
		page_buf_t *);		/* buffer to deallocate		*/

extern void pagebuf_rele(		/* release hold on a buffer	*/
		page_buf_t *);		/* buffer to release		*/

/* Locking and Unlocking Buffers */

extern int pagebuf_cond_lock(		/* lock buffer, if not locked	*/
					/* (returns -EBUSY if locked)	*/
		page_buf_t *);		/* buffer to lock		*/

extern int pagebuf_lock_value(		/* return count on lock		*/
		page_buf_t *);          /* buffer to check              */

extern int pagebuf_lock(		/* lock buffer                  */
		page_buf_t *);          /* buffer to lock               */

extern void pagebuf_unlock(		/* unlock buffer		*/
		page_buf_t *);		/* buffer to unlock		*/

/* Buffer Read and Write Routines */

extern void pagebuf_iodone(		/* mark buffer I/O complete	*/
		page_buf_t *,		/* buffer to mark		*/
		int,			/* use data/log helper thread.	*/
		int);			/* run completion locally, or in
					 * a helper thread.		*/

extern void pagebuf_ioerror(		/* mark buffer in error	(or not) */
		page_buf_t *,		/* buffer to mark		*/
		unsigned int);		/* error to store (0 if none)	*/

extern int pagebuf_iostart(		/* start I/O on a buffer	*/
		page_buf_t *,		/* buffer to start		*/
		page_buf_flags_t);	/* PBF_LOCK, PBF_ASYNC,		*/
					/* PBF_READ, PBF_WRITE,		*/
					/* PBF_DELWRI			*/

extern int pagebuf_iorequest(		/* start real I/O		*/
		page_buf_t *);		/* buffer to convey to device	*/

extern int pagebuf_iowait(		/* wait for buffer I/O done	*/
		page_buf_t *);		/* buffer to wait on		*/

extern void pagebuf_iomove(		/* move data in/out of pagebuf	*/
		page_buf_t *,		/* buffer to manipulate		*/
		size_t,			/* starting buffer offset	*/
		size_t,			/* length in buffer		*/
		caddr_t,		/* data pointer			*/
		page_buf_rw_t);		/* direction			*/

static inline int pagebuf_iostrategy(page_buf_t *pb)
{
	return pb->pb_strat ? pb->pb_strat(pb) : pagebuf_iorequest(pb);
}

static inline int pagebuf_geterror(page_buf_t *pb)
{
	return pb ? pb->pb_error : ENOMEM;
}

/* Buffer Utility Routines */

extern caddr_t pagebuf_offset(		/* pointer at offset in buffer	*/
		page_buf_t *,		/* buffer to offset into	*/
		size_t);		/* offset			*/

/* Pinning Buffer Storage in Memory */

extern void pagebuf_pin(		/* pin buffer in memory		*/
		page_buf_t *);		/* buffer to pin		*/

extern void pagebuf_unpin(		/* unpin buffered data		*/
		page_buf_t *);		/* buffer to unpin		*/

extern int pagebuf_ispin(		/* check if buffer is pinned	*/
		page_buf_t *);		/* buffer to check		*/

/* Delayed Write Buffer Routines */

#define PBDF_WAIT    0x01
extern void pagebuf_delwri_flush(
		pb_target_t *,
		unsigned long,
		int *);

extern void pagebuf_delwri_dequeue(
		page_buf_t *);

/* Buffer Daemon Setup Routines */

extern int pagebuf_init(void);
extern void pagebuf_terminate(void);


#ifdef PAGEBUF_TRACE
extern ktrace_t *pagebuf_trace_buf;
extern void pagebuf_trace(
		page_buf_t *,		/* buffer being traced		*/
		char *,			/* description of operation	*/
		void *,			/* arbitrary diagnostic value	*/
		void *);		/* return address		*/
#else
# define pagebuf_trace(pb, id, ptr, ra)	do { } while (0)
#endif

#define pagebuf_target_name(target)	bdevname((target)->pbr_kdev)

/*
 * Kernel version compatibility macros
 */

#define page_buffers(page)	((page)->buffers)
#define page_has_buffers(page)	((page)->buffers)
#define PageUptodate(x)		Page_Uptodate(x)
/*
 * macro tricks to expand the set_buffer_foo() and clear_buffer_foo()
 * functions.
 */
#define BUFFER_FNS(bit, name)						\
static inline void set_buffer_##name(struct buffer_head *bh)		\
{									\
	set_bit(BH_##bit, &(bh)->b_state);				\
}									\
static inline void clear_buffer_##name(struct buffer_head *bh)		\
{									\
	clear_bit(BH_##bit, &(bh)->b_state);				\
}									\

/*
 * Emit the buffer bitops functions.   Note that there are also functions
 * of the form "mark_buffer_foo()".  These are higher-level functions which
 * do something in addition to setting a b_state bit.
 */
BUFFER_FNS(Uptodate, uptodate)
BUFFER_FNS(Dirty, dirty)
BUFFER_FNS(Lock, locked)
BUFFER_FNS(Req, req)
BUFFER_FNS(Mapped, mapped)
BUFFER_FNS(New, new)
BUFFER_FNS(Async, async)
BUFFER_FNS(Wait_IO, wait_io)
BUFFER_FNS(Launder, launder)
BUFFER_FNS(Sync, sync)
BUFFER_FNS(Delay, delay)

#define get_seconds()		CURRENT_TIME
#define blk_run_queues()	run_task_queue(&tq_disk)
#define i_size_read(inode)	((inode)->i_size)
#define i_size_write(inode, sz)	((inode)->i_size = (sz))

/* These are just for xfs_syncsub... it sets an internal variable
 * then passes it to VOP_FLUSH_PAGES or adds the flags to a newly gotten buf_t
 */
#define XFS_B_ASYNC		PBF_ASYNC
#define XFS_B_DELWRI		PBF_DELWRI
#define XFS_B_READ		PBF_READ
#define XFS_B_WRITE		PBF_WRITE
#define XFS_B_STALE		PBF_STALE

#define XFS_BUF_TRYLOCK		PBF_TRYLOCK
#define XFS_INCORE_TRYLOCK	PBF_TRYLOCK
#define XFS_BUF_LOCK		PBF_LOCK
#define XFS_BUF_MAPPED		PBF_MAPPED

#define BUF_BUSY		PBF_DONT_BLOCK

#define XFS_BUF_BFLAGS(x)	((x)->pb_flags)
#define XFS_BUF_ZEROFLAGS(x)	\
	((x)->pb_flags &= ~(PBF_READ|PBF_WRITE|PBF_ASYNC|PBF_DELWRI))

#define XFS_BUF_STALE(x)	((x)->pb_flags |= XFS_B_STALE)
#define XFS_BUF_UNSTALE(x)	((x)->pb_flags &= ~XFS_B_STALE)
#define XFS_BUF_ISSTALE(x)	((x)->pb_flags & XFS_B_STALE)
#define XFS_BUF_SUPER_STALE(x)	do {				\
					XFS_BUF_STALE(x);	\
					xfs_buf_undelay(x);	\
					XFS_BUF_DONE(x);	\
				} while (0)

#define XFS_BUF_MANAGE		PBF_FS_MANAGED
#define XFS_BUF_UNMANAGE(x)	((x)->pb_flags &= ~PBF_FS_MANAGED)

static inline void xfs_buf_undelay(page_buf_t *pb)
{
	if (pb->pb_flags & PBF_DELWRI) {
		if (pb->pb_list.next != &pb->pb_list) {
			pagebuf_delwri_dequeue(pb);
			pagebuf_rele(pb);
		} else {
			pb->pb_flags &= ~PBF_DELWRI;
		}
	}
}

#define XFS_BUF_DELAYWRITE(x)	 ((x)->pb_flags |= PBF_DELWRI)
#define XFS_BUF_UNDELAYWRITE(x)	 xfs_buf_undelay(x)
#define XFS_BUF_ISDELAYWRITE(x)	 ((x)->pb_flags & PBF_DELWRI)

#define XFS_BUF_ERROR(x,no)	 pagebuf_ioerror(x,no)
#define XFS_BUF_GETERROR(x)	 pagebuf_geterror(x)
#define XFS_BUF_ISERROR(x)	 (pagebuf_geterror(x)?1:0)

#define XFS_BUF_DONE(x)		 ((x)->pb_flags &= ~(PBF_PARTIAL|PBF_NONE))
#define XFS_BUF_UNDONE(x)	 ((x)->pb_flags |= PBF_PARTIAL|PBF_NONE)
#define XFS_BUF_ISDONE(x)	 (!(PBF_NOT_DONE(x)))

#define XFS_BUF_BUSY(x)		 ((x)->pb_flags |= PBF_FORCEIO)
#define XFS_BUF_UNBUSY(x)	 ((x)->pb_flags &= ~PBF_FORCEIO)
#define XFS_BUF_ISBUSY(x)	 (1)

#define XFS_BUF_ASYNC(x)	 ((x)->pb_flags |= PBF_ASYNC)
#define XFS_BUF_UNASYNC(x)	 ((x)->pb_flags &= ~PBF_ASYNC)
#define XFS_BUF_ISASYNC(x)	 ((x)->pb_flags & PBF_ASYNC)

#define XFS_BUF_FLUSH(x)	 ((x)->pb_flags |= PBF_FLUSH)
#define XFS_BUF_UNFLUSH(x)	 ((x)->pb_flags &= ~PBF_FLUSH)
#define XFS_BUF_ISFLUSH(x)	 ((x)->pb_flags & PBF_FLUSH)

#define XFS_BUF_SHUT(x)		 printk("XFS_BUF_SHUT not implemented yet\n")
#define XFS_BUF_UNSHUT(x)	 printk("XFS_BUF_UNSHUT not implemented yet\n")
#define XFS_BUF_ISSHUT(x)	 (0)

#define XFS_BUF_HOLD(x)		pagebuf_hold(x)
#define XFS_BUF_READ(x)		((x)->pb_flags |= PBF_READ)
#define XFS_BUF_UNREAD(x)	((x)->pb_flags &= ~PBF_READ)
#define XFS_BUF_ISREAD(x)	((x)->pb_flags & PBF_READ)

#define XFS_BUF_WRITE(x)	((x)->pb_flags |= PBF_WRITE)
#define XFS_BUF_UNWRITE(x)	((x)->pb_flags &= ~PBF_WRITE)
#define XFS_BUF_ISWRITE(x)	((x)->pb_flags & PBF_WRITE)

#define XFS_BUF_ISUNINITIAL(x)	 (0)
#define XFS_BUF_UNUNINITIAL(x)	 (0)

#define XFS_BUF_BP_ISMAPPED(bp)	 1

typedef struct page_buf_s xfs_buf_t;
#define xfs_buf page_buf_s

typedef struct pb_target xfs_buftarg_t;
#define xfs_buftarg pb_target

#define XFS_BUF_DATAIO(x)	((x)->pb_flags |= PBF_FS_DATAIOD)
#define XFS_BUF_UNDATAIO(x)	((x)->pb_flags &= ~PBF_FS_DATAIOD)

#define XFS_BUF_IODONE_FUNC(buf)	(buf)->pb_iodone
#define XFS_BUF_SET_IODONE_FUNC(buf, func)	\
			(buf)->pb_iodone = (func)
#define XFS_BUF_CLR_IODONE_FUNC(buf)		\
			(buf)->pb_iodone = NULL
#define XFS_BUF_SET_BDSTRAT_FUNC(buf, func)	\
			(buf)->pb_strat = (func)
#define XFS_BUF_CLR_BDSTRAT_FUNC(buf)		\
			(buf)->pb_strat = NULL

#define XFS_BUF_FSPRIVATE(buf, type)		\
			((type)(buf)->pb_fspriv)
#define XFS_BUF_SET_FSPRIVATE(buf, value)	\
			(buf)->pb_fspriv = (void *)(value)
#define XFS_BUF_FSPRIVATE2(buf, type)		\
			((type)(buf)->pb_fspriv2)
#define XFS_BUF_SET_FSPRIVATE2(buf, value)	\
			(buf)->pb_fspriv2 = (void *)(value)
#define XFS_BUF_FSPRIVATE3(buf, type)		\
			((type)(buf)->pb_fspriv3)
#define XFS_BUF_SET_FSPRIVATE3(buf, value)	\
			(buf)->pb_fspriv3  = (void *)(value)
#define XFS_BUF_SET_START(buf)

#define XFS_BUF_SET_BRELSE_FUNC(buf, value) \
			(buf)->pb_relse = (value)

#define XFS_BUF_PTR(bp)		(xfs_caddr_t)((bp)->pb_addr)

extern inline xfs_caddr_t xfs_buf_offset(page_buf_t *bp, size_t offset)
{
	if (bp->pb_flags & PBF_MAPPED)
		return XFS_BUF_PTR(bp) + offset;
	return (xfs_caddr_t) pagebuf_offset(bp, offset);
}

#define XFS_BUF_SET_PTR(bp, val, count)		\
				pagebuf_associate_memory(bp, val, count)
#define XFS_BUF_ADDR(bp)	((bp)->pb_bn)
#define XFS_BUF_SET_ADDR(bp, blk)		\
			((bp)->pb_bn = (page_buf_daddr_t)(blk))
#define XFS_BUF_OFFSET(bp)	((bp)->pb_file_offset)
#define XFS_BUF_SET_OFFSET(bp, off)		\
			((bp)->pb_file_offset = (off))
#define XFS_BUF_COUNT(bp)	((bp)->pb_count_desired)
#define XFS_BUF_SET_COUNT(bp, cnt)		\
			((bp)->pb_count_desired = (cnt))
#define XFS_BUF_SIZE(bp)	((bp)->pb_buffer_length)
#define XFS_BUF_SET_SIZE(bp, cnt)		\
			((bp)->pb_buffer_length = (cnt))
#define XFS_BUF_SET_VTYPE_REF(bp, type, ref)
#define XFS_BUF_SET_VTYPE(bp, type)
#define XFS_BUF_SET_REF(bp, ref)

#define XFS_BUF_ISPINNED(bp)	pagebuf_ispin(bp)

#define XFS_BUF_VALUSEMA(bp)	pagebuf_lock_value(bp)
#define XFS_BUF_CPSEMA(bp)	(pagebuf_cond_lock(bp) == 0)
#define XFS_BUF_VSEMA(bp)	pagebuf_unlock(bp)
#define XFS_BUF_PSEMA(bp,x)	pagebuf_lock(bp)
#define XFS_BUF_V_IODONESEMA(bp) up(&bp->pb_iodonesema);

/* setup the buffer target from a buftarg structure */
#define XFS_BUF_SET_TARGET(bp, target)	\
		(bp)->pb_target = (target)
#define XFS_BUF_TARGET(bp)	((bp)->pb_target)
#define XFS_BUFTARG_NAME(target)	\
		pagebuf_target_name(target)

#define XFS_BUF_SET_VTYPE_REF(bp, type, ref)
#define XFS_BUF_SET_VTYPE(bp, type)
#define XFS_BUF_SET_REF(bp, ref)

#define xfs_buf_read(target, blkno, len, flags) \
		pagebuf_get((target), (blkno), (len), \
			PBF_LOCK | PBF_READ | PBF_MAPPED)
#define xfs_buf_get(target, blkno, len, flags) \
		pagebuf_get((target), (blkno), (len), \
			PBF_LOCK | PBF_MAPPED)

#define xfs_buf_read_flags(target, blkno, len, flags) \
		pagebuf_get((target), (blkno), (len), PBF_READ | (flags))
#define xfs_buf_get_flags(target, blkno, len, flags) \
		pagebuf_get((target), (blkno), (len), (flags))

static inline int	xfs_bawrite(void *mp, page_buf_t *bp)
{
	bp->pb_fspriv3 = mp;
	bp->pb_strat = xfs_bdstrat_cb;
	xfs_buf_undelay(bp);
	return pagebuf_iostart(bp, PBF_WRITE | PBF_ASYNC | PBF_RUN_QUEUES);
}

static inline void	xfs_buf_relse(page_buf_t *bp)
{
	if (!bp->pb_relse)
		pagebuf_unlock(bp);
	pagebuf_rele(bp);
}

#define xfs_bpin(bp)		pagebuf_pin(bp)
#define xfs_bunpin(bp)		pagebuf_unpin(bp)

#define xfs_buftrace(id, bp)	\
	    pagebuf_trace(bp, id, NULL, (void *)__builtin_return_address(0))

#define xfs_biodone(pb)		    \
	    pagebuf_iodone(pb, (pb->pb_flags & PBF_FS_DATAIOD), 0)

#define xfs_incore(buftarg,blkno,len,lockit) \
	    pagebuf_find(buftarg, blkno ,len, lockit)


#define xfs_biomove(pb, off, len, data, rw) \
	    pagebuf_iomove((pb), (off), (len), (data), \
		((rw) == XFS_B_WRITE) ? PBRW_WRITE : PBRW_READ)

#define xfs_biozero(pb, off, len) \
	    pagebuf_iomove((pb), (off), (len), NULL, PBRW_ZERO)


static inline int	XFS_bwrite(page_buf_t *pb)
{
	int	iowait = (pb->pb_flags & PBF_ASYNC) == 0;
	int	error = 0;

	if (!iowait)
		pb->pb_flags |= PBF_RUN_QUEUES;

	xfs_buf_undelay(pb);
	pagebuf_iostrategy(pb);
	if (iowait) {
		error = pagebuf_iowait(pb);
		xfs_buf_relse(pb);
	}
	return error;
}

#define XFS_bdwrite(pb)		     \
	    pagebuf_iostart(pb, PBF_DELWRI | PBF_ASYNC)

static inline int xfs_bdwrite(void *mp, page_buf_t *bp)
{
	bp->pb_strat = xfs_bdstrat_cb;
	bp->pb_fspriv3 = mp;

	return pagebuf_iostart(bp, PBF_DELWRI | PBF_ASYNC);
}

#define XFS_bdstrat(bp) pagebuf_iorequest(bp)

#define xfs_iowait(pb)	pagebuf_iowait(pb)


/*
 * Go through all incore buffers, and release buffers
 * if they belong to the given device. This is used in
 * filesystem error handling to preserve the consistency
 * of its metadata.
 */

#define xfs_binval(buftarg)	xfs_flush_buftarg(buftarg)

#define XFS_bflush(buftarg)	xfs_flush_buftarg(buftarg)

#define xfs_incore_relse(buftarg,delwri_only,wait)	\
	xfs_relse_buftarg(buftarg)

#define xfs_baread(target, rablkno, ralen)  \
	pagebuf_readahead((target), (rablkno), (ralen), PBF_DONT_BLOCK)

#define xfs_buf_get_empty(len, target)	pagebuf_get_empty((len), (target))
#define xfs_buf_get_noaddr(len, target)	pagebuf_get_no_daddr((len), (target))
#define xfs_buf_free(bp)		pagebuf_free(bp)

#endif	/* __XFS_BUF_H__ */
