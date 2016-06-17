#ifndef _MULTIPATH_H
#define _MULTIPATH_H

#include <linux/raid/md.h>

struct multipath_info {
	int		number;
	int		raid_disk;
	kdev_t		dev;

	/*
	 * State bits:
	 */
	int		operational;
	int		spare;

	int		used_slot;
};

struct multipath_private_data {
	mddev_t			*mddev;
	struct multipath_info	multipaths[MD_SB_DISKS];
	int			nr_disks;
	int			raid_disks;
	int			working_disks;
	mdk_thread_t		*thread;
	struct multipath_info	*spare;
	md_spinlock_t		device_lock;

	/* buffer pool */
	/* buffer_heads that we have pre-allocated have b_pprev -> &freebh
	 * and are linked into a stack using b_next
	 * multipath_bh that are pre-allocated have MPBH_PreAlloc set.
	 * All these variable are protected by device_lock
	 */
	struct multipath_bh	*freer1;
	int			freer1_blocked;
	int			freer1_cnt;
	md_wait_queue_head_t	wait_buffer;
};

typedef struct multipath_private_data multipath_conf_t;

/*
 * this is the only point in the RAID code where we violate
 * C type safety. mddev->private is an 'opaque' pointer.
 */
#define mddev_to_conf(mddev) ((multipath_conf_t *) mddev->private)

/*
 * this is our 'private' 'collective' MULTIPATH buffer head.
 * it contains information about what kind of IO operations were started
 * for this MULTIPATH operation, and about their status:
 */

struct multipath_bh {
	atomic_t		remaining; /* 'have we finished' count,
					    * used from IRQ handlers
					    */
	int			cmd;
	unsigned long		state;
	mddev_t			*mddev;
	struct buffer_head	*master_bh;
	struct buffer_head	bh_req;
	struct multipath_bh	*next_mp; /* next for retry or in free list */
};
/* bits for multipath_bh.state */
#define	MPBH_Uptodate	1
#define	MPBH_SyncPhase	2
#define	MPBH_PreAlloc	3	/* this was pre-allocated, add to free list */
#endif
