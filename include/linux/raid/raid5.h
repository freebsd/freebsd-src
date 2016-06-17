#ifndef _RAID5_H
#define _RAID5_H

#include <linux/raid/md.h>
#include <linux/raid/xor.h>

/*
 *
 * Each stripe contains one buffer per disc.  Each buffer can be in
 * one of a number of states determined by bh_state.  Changes between
 * these states happen *almost* exclusively under a per-stripe
 * spinlock.  Some very specific changes can happen in b_end_io, and
 * these are not protected by the spin lock.
 *
 * The bh_state bits that are used to represent these states are:
 *   BH_Uptodate, BH_Lock
 *
 * State Empty == !Uptodate, !Lock
 *        We have no data, and there is no active request
 * State Want == !Uptodate, Lock
 *        A read request is being submitted for this block
 * State Dirty == Uptodate, Lock
 *        Some new data is in this buffer, and it is being written out
 * State Clean == Uptodate, !Lock
 *        We have valid data which is the same as on disc
 *
 * The possible state transitions are:
 *
 *  Empty -> Want   - on read or write to get old data for  parity calc
 *  Empty -> Dirty  - on compute_parity to satisfy write/sync request.(RECONSTRUCT_WRITE)
 *  Empty -> Clean  - on compute_block when computing a block for failed drive
 *  Want  -> Empty  - on failed read
 *  Want  -> Clean  - on successful completion of read request
 *  Dirty -> Clean  - on successful completion of write request
 *  Dirty -> Clean  - on failed write
 *  Clean -> Dirty  - on compute_parity to satisfy write/sync (RECONSTRUCT or RMW)
 *
 * The Want->Empty, Want->Clean, Dirty->Clean, transitions
 * all happen in b_end_io at interrupt time.
 * Each sets the Uptodate bit before releasing the Lock bit.
 * This leaves one multi-stage transition:
 *    Want->Dirty->Clean
 * This is safe because thinking that a Clean buffer is actually dirty
 * will at worst delay some action, and the stripe will be scheduled
 * for attention after the transition is complete.
 *
 * There is one possibility that is not covered by these states.  That
 * is if one drive has failed and there is a spare being rebuilt.  We
 * can't distinguish between a clean block that has been generated
 * from parity calculations, and a clean block that has been
 * successfully written to the spare ( or to parity when resyncing).
 * To distingush these states we have a stripe bit STRIPE_INSYNC that
 * is set whenever a write is scheduled to the spare, or to the parity
 * disc if there is no spare.  A sync request clears this bit, and
 * when we find it set with no buffers locked, we know the sync is
 * complete.
 *
 * Buffers for the md device that arrive via make_request are attached
 * to the appropriate stripe in one of two lists linked on b_reqnext.
 * One list (bh_read) for read requests, one (bh_write) for write.
 * There should never be more than one buffer on the two lists
 * together, but we are not guaranteed of that so we allow for more.
 *
 * If a buffer is on the read list when the associated cache buffer is
 * Uptodate, the data is copied into the read buffer and it's b_end_io
 * routine is called.  This may happen in the end_request routine only
 * if the buffer has just successfully been read.  end_request should
 * remove the buffers from the list and then set the Uptodate bit on
 * the buffer.  Other threads may do this only if they first check
 * that the Uptodate bit is set.  Once they have checked that they may
 * take buffers off the read queue.
 *
 * When a buffer on the write list is committed for write is it copied
 * into the cache buffer, which is then marked dirty, and moved onto a
 * third list, the written list (bh_written).  Once both the parity
 * block and the cached buffer are successfully written, any buffer on
 * a written list can be returned with b_end_io.
 *
 * The write list and read list both act as fifos.  The read list is
 * protected by the device_lock.  The write and written lists are
 * protected by the stripe lock.  The device_lock, which can be
 * claimed while the stipe lock is held, is only for list
 * manipulations and will only be held for a very short time.  It can
 * be claimed from interrupts.
 *
 *
 * Stripes in the stripe cache can be on one of two lists (or on
 * neither).  The "inactive_list" contains stripes which are not
 * currently being used for any request.  They can freely be reused
 * for another stripe.  The "handle_list" contains stripes that need
 * to be handled in some way.  Both of these are fifo queues.  Each
 * stripe is also (potentially) linked to a hash bucket in the hash
 * table so that it can be found by sector number.  Stripes that are
 * not hashed must be on the inactive_list, and will normally be at
 * the front.  All stripes start life this way.
 *
 * The inactive_list, handle_list and hash bucket lists are all protected by the
 * device_lock.
 *  - stripes on the inactive_list never have their stripe_lock held.
 *  - stripes have a reference counter. If count==0, they are on a list.
 *  - If a stripe might need handling, STRIPE_HANDLE is set.
 *  - When refcount reaches zero, then if STRIPE_HANDLE it is put on
 *    handle_list else inactive_list
 *
 * This, combined with the fact that STRIPE_HANDLE is only ever
 * cleared while a stripe has a non-zero count means that if the
 * refcount is 0 and STRIPE_HANDLE is set, then it is on the
 * handle_list and if recount is 0 and STRIPE_HANDLE is not set, then
 * the stripe is on inactive_list.
 *
 * The possible transitions are:
 *  activate an unhashed/inactive stripe (get_active_stripe())
 *     lockdev check-hash unlink-stripe cnt++ clean-stripe hash-stripe unlockdev
 *  activate a hashed, possibly active stripe (get_active_stripe())
 *     lockdev check-hash if(!cnt++)unlink-stripe unlockdev
 *  attach a request to an active stripe (add_stripe_bh())
 *     lockdev attach-buffer unlockdev
 *  handle a stripe (handle_stripe())
 *     lockstripe clrSTRIPE_HANDLE ... (lockdev check-buffers unlockdev) .. change-state .. record io needed unlockstripe schedule io
 *  release an active stripe (release_stripe())
 *     lockdev if (!--cnt) { if  STRIPE_HANDLE, add to handle_list else add to inactive-list } unlockdev
 *
 * The refcount counts each thread that have activated the stripe,
 * plus raid5d if it is handling it, plus one for each active request
 * on a cached buffer.
 */
struct stripe_head {
	struct stripe_head	*hash_next, **hash_pprev; /* hash pointers */
	struct list_head	lru;			/* inactive_list or handle_list */
	struct raid5_private_data	*raid_conf;
	struct buffer_head	*bh_cache[MD_SB_DISKS];	/* buffered copy */
	struct buffer_head	*bh_read[MD_SB_DISKS];	/* read request buffers of the MD device */
	struct buffer_head	*bh_write[MD_SB_DISKS];	/* write request buffers of the MD device */
	struct buffer_head	*bh_written[MD_SB_DISKS]; /* write request buffers of the MD device that have been scheduled for write */
	struct page		*bh_page[MD_SB_DISKS];	/* saved bh_cache[n]->b_page when reading around the cache */
	unsigned long		sector;			/* sector of this row */
	int			size;			/* buffers size */
	int			pd_idx;			/* parity disk index */
	unsigned long		state;			/* state flags */
	atomic_t		count;			/* nr of active thread/requests */
	spinlock_t		lock;
	int			sync_redone;
};


/*
 * Write method
 */
#define RECONSTRUCT_WRITE	1
#define READ_MODIFY_WRITE	2
/* not a write method, but a compute_parity mode */
#define	CHECK_PARITY		3

/*
 * Stripe state
 */
#define STRIPE_ERROR		1
#define STRIPE_HANDLE		2
#define	STRIPE_SYNCING		3
#define	STRIPE_INSYNC		4
#define	STRIPE_PREREAD_ACTIVE	5
#define	STRIPE_DELAYED		6

/*
 * Plugging:
 *
 * To improve write throughput, we need to delay the handling of some
 * stripes until there has been a chance that several write requests
 * for the one stripe have all been collected.
 * In particular, any write request that would require pre-reading
 * is put on a "delayed" queue until there are no stripes currently
 * in a pre-read phase.  Further, if the "delayed" queue is empty when
 * a stripe is put on it then we "plug" the queue and do not process it
 * until an unplg call is made. (the tq_disk list is run).
 *
 * When preread is initiated on a stripe, we set PREREAD_ACTIVE and add
 * it to the count of prereading stripes.
 * When write is initiated, or the stripe refcnt == 0 (just in case) we
 * clear the PREREAD_ACTIVE flag and decrement the count
 * Whenever the delayed queue is empty and the device is not plugged, we
 * move any strips from delayed to handle and clear the DELAYED flag and set PREREAD_ACTIVE.
 * In stripe_handle, if we find pre-reading is necessary, we do it if
 * PREREAD_ACTIVE is set, else we set DELAYED which will send it to the delayed queue.
 * HANDLE gets cleared if stripe_handle leave nothing locked.
 */
 

struct disk_info {
	kdev_t	dev;
	int	operational;
	int	number;
	int	raid_disk;
	int	write_only;
	int	spare;
	int	used_slot;
};

struct raid5_private_data {
	struct stripe_head	**stripe_hashtbl;
	mddev_t			*mddev;
	mdk_thread_t		*thread, *resync_thread;
	struct disk_info	disks[MD_SB_DISKS];
	struct disk_info	*spare;
	int			buffer_size;
	int			chunk_size, level, algorithm;
	int			raid_disks, working_disks, failed_disks;
	int			resync_parity;
	int			max_nr_stripes;

	struct list_head	handle_list; /* stripes needing handling */
	struct list_head	delayed_list; /* stripes that have plugged requests */
	atomic_t		preread_active_stripes; /* stripes with scheduled io */
	/*
	 * Free stripes pool
	 */
	atomic_t		active_stripes;
	struct list_head	inactive_list;
	md_wait_queue_head_t	wait_for_stripe;
	int			inactive_blocked;	/* release of inactive stripes blocked,
							 * waiting for 25% to be free
							 */        
	md_spinlock_t		device_lock;

	int			plugged;
	struct tq_struct	plug_tq;
};

typedef struct raid5_private_data raid5_conf_t;

#define mddev_to_conf(mddev) ((raid5_conf_t *) mddev->private)

/*
 * Our supported algorithms
 */
#define ALGORITHM_LEFT_ASYMMETRIC	0
#define ALGORITHM_RIGHT_ASYMMETRIC	1
#define ALGORITHM_LEFT_SYMMETRIC	2
#define ALGORITHM_RIGHT_SYMMETRIC	3

#endif
