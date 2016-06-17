#ifndef _RAID1_H
#define _RAID1_H

#include <linux/raid/md.h>

struct mirror_info {
	int		number;
	int		raid_disk;
	kdev_t		dev;
	int		sect_limit;
	int		head_position;

	/*
	 * State bits:
	 */
	int		operational;
	int		write_only;
	int		spare;

	int		used_slot;
};

struct raid1_private_data {
	mddev_t			*mddev;
	struct mirror_info	mirrors[MD_SB_DISKS];
	int			nr_disks;
	int			raid_disks;
	int			working_disks;
	int			last_used;
	unsigned long		next_sect;
	int			sect_count;
	mdk_thread_t		*thread, *resync_thread;
	int			resync_mirrors;
	struct mirror_info	*spare;
	md_spinlock_t		device_lock;

	/* buffer pool */
	/* buffer_heads that we have pre-allocated have b_pprev -> &freebh
	 * and are linked into a stack using b_next
	 * raid1_bh that are pre-allocated have R1BH_PreAlloc set.
	 * All these variable are protected by device_lock
	 */
	struct buffer_head	*freebh;
	int			freebh_cnt;	/* how many are on the list */
	int			freebh_blocked;
	struct raid1_bh		*freer1;
	int			freer1_blocked;
	int			freer1_cnt;
	struct raid1_bh		*freebuf; 	/* each bh_req has a page allocated */
	md_wait_queue_head_t	wait_buffer;

	/* for use when syncing mirrors: */
	unsigned long	start_active, start_ready,
		start_pending, start_future;
	int	cnt_done, cnt_active, cnt_ready,
		cnt_pending, cnt_future;
	int	phase;
	int	window;
	md_wait_queue_head_t	wait_done;
	md_wait_queue_head_t	wait_ready;
	md_spinlock_t		segment_lock;
};

typedef struct raid1_private_data raid1_conf_t;

/*
 * this is the only point in the RAID code where we violate
 * C type safety. mddev->private is an 'opaque' pointer.
 */
#define mddev_to_conf(mddev) ((raid1_conf_t *) mddev->private)

/*
 * this is our 'private' 'collective' RAID1 buffer head.
 * it contains information about what kind of IO operations were started
 * for this RAID1 operation, and about their status:
 */

struct raid1_bh {
	atomic_t		remaining; /* 'have we finished' count,
					    * used from IRQ handlers
					    */
	int			cmd;
	unsigned long		state;
	mddev_t			*mddev;
	struct buffer_head	*master_bh;
	struct buffer_head	*mirror_bh_list;
	struct buffer_head	bh_req;
	struct raid1_bh		*next_r1;	/* next for retry or in free list */
};
/* bits for raid1_bh.state */
#define	R1BH_Uptodate	1
#define	R1BH_SyncPhase	2
#define	R1BH_PreAlloc	3	/* this was pre-allocated, add to free list */
#endif
