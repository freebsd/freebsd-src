/*
 * raid1.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 1999, 2000 Ingo Molnar, Red Hat
 *
 * Copyright (C) 1996, 1997, 1998 Ingo Molnar, Miguel de Icaza, Gadi Oxman
 *
 * RAID-1 management functions.
 *
 * Better read-balancing code written by Mika Kuoppala <miku@iki.fi>, 2000
 *
 * Fixes to reconstruction by Jakob Østergaard" <jakob@ostenfeld.dk>
 * Various fixes by Neil Brown <neilb@cse.unsw.edu.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/slab.h>
#include <linux/raid/raid1.h>
#include <asm/atomic.h>

#define MAJOR_NR MD_MAJOR
#define MD_DRIVER
#define MD_PERSONALITY

#define MAX_WORK_PER_DISK 128

#define	NR_RESERVED_BUFS	32


/*
 * The following can be used to debug the driver
 */
#define RAID1_DEBUG	0

#if RAID1_DEBUG
#define PRINTK(x...)   printk(x)
#define inline
#define __inline__
#else
#define PRINTK(x...)  do { } while (0)
#endif


static mdk_personality_t raid1_personality;
static md_spinlock_t retry_list_lock = MD_SPIN_LOCK_UNLOCKED;
struct raid1_bh *raid1_retry_list = NULL, **raid1_retry_tail;

static struct buffer_head *raid1_alloc_bh(raid1_conf_t *conf, int cnt)
{
	/* return a linked list of "cnt" struct buffer_heads.
	 * don't take any off the free list unless we know we can
	 * get all we need, otherwise we could deadlock
	 */
	struct buffer_head *bh=NULL;

	while(cnt) {
		struct buffer_head *t;
		md_spin_lock_irq(&conf->device_lock);
		if (!conf->freebh_blocked && conf->freebh_cnt >= cnt)
			while (cnt) {
				t = conf->freebh;
				conf->freebh = t->b_next;
				t->b_next = bh;
				bh = t;
				t->b_state = 0;
				conf->freebh_cnt--;
				cnt--;
			}
		md_spin_unlock_irq(&conf->device_lock);
		if (cnt == 0)
			break;
		t = kmem_cache_alloc(bh_cachep, SLAB_NOIO);
		if (t) {
			t->b_next = bh;
			bh = t;
			cnt--;
		} else {
			PRINTK("raid1: waiting for %d bh\n", cnt);
			conf->freebh_blocked = 1;
			wait_disk_event(conf->wait_buffer,
					!conf->freebh_blocked ||
					conf->freebh_cnt > conf->raid_disks * NR_RESERVED_BUFS/2);
			conf->freebh_blocked = 0;
		}
	}
	return bh;
}

static inline void raid1_free_bh(raid1_conf_t *conf, struct buffer_head *bh)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->device_lock, flags);
	while (bh) {
		struct buffer_head *t = bh;
		bh=bh->b_next;
		if (t->b_pprev == NULL)
			kmem_cache_free(bh_cachep, t);
		else {
			t->b_next= conf->freebh;
			conf->freebh = t;
			conf->freebh_cnt++;
		}
	}
	spin_unlock_irqrestore(&conf->device_lock, flags);
	wake_up(&conf->wait_buffer);
}

static int raid1_grow_bh(raid1_conf_t *conf, int cnt)
{
	/* allocate cnt buffer_heads, possibly less if kmalloc fails */
	int i = 0;

	while (i < cnt) {
		struct buffer_head *bh;
		bh = kmem_cache_alloc(bh_cachep, SLAB_KERNEL);
		if (!bh) break;

		md_spin_lock_irq(&conf->device_lock);
		bh->b_pprev = &conf->freebh;
		bh->b_next = conf->freebh;
		conf->freebh = bh;
		conf->freebh_cnt++;
		md_spin_unlock_irq(&conf->device_lock);

		i++;
	}
	return i;
}

static void raid1_shrink_bh(raid1_conf_t *conf)
{
	/* discard all buffer_heads */

	md_spin_lock_irq(&conf->device_lock);
	while (conf->freebh) {
		struct buffer_head *bh = conf->freebh;
		conf->freebh = bh->b_next;
		kmem_cache_free(bh_cachep, bh);
		conf->freebh_cnt--;
	}
	md_spin_unlock_irq(&conf->device_lock);
}
		

static struct raid1_bh *raid1_alloc_r1bh(raid1_conf_t *conf)
{
	struct raid1_bh *r1_bh = NULL;

	do {
		md_spin_lock_irq(&conf->device_lock);
		if (!conf->freer1_blocked && conf->freer1) {
			r1_bh = conf->freer1;
			conf->freer1 = r1_bh->next_r1;
			conf->freer1_cnt--;
			r1_bh->next_r1 = NULL;
			r1_bh->state = (1 << R1BH_PreAlloc);
			r1_bh->bh_req.b_state = 0;
		}
		md_spin_unlock_irq(&conf->device_lock);
		if (r1_bh)
			return r1_bh;
		r1_bh = (struct raid1_bh *) kmalloc(sizeof(struct raid1_bh), GFP_NOIO);
		if (r1_bh) {
			memset(r1_bh, 0, sizeof(*r1_bh));
			return r1_bh;
		}
		conf->freer1_blocked = 1;
		wait_disk_event(conf->wait_buffer,
				!conf->freer1_blocked ||
				conf->freer1_cnt > NR_RESERVED_BUFS/2
			);
		conf->freer1_blocked = 0;
	} while (1);
}

static inline void raid1_free_r1bh(struct raid1_bh *r1_bh)
{
	struct buffer_head *bh = r1_bh->mirror_bh_list;
	raid1_conf_t *conf = mddev_to_conf(r1_bh->mddev);

	r1_bh->mirror_bh_list = NULL;

	if (test_bit(R1BH_PreAlloc, &r1_bh->state)) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		r1_bh->next_r1 = conf->freer1;
		conf->freer1 = r1_bh;
		conf->freer1_cnt++;
		spin_unlock_irqrestore(&conf->device_lock, flags);
		/* don't need to wakeup wait_buffer because
		 *  raid1_free_bh below will do that
		 */
	} else {
		kfree(r1_bh);
	}
	raid1_free_bh(conf, bh);
}

static int raid1_grow_r1bh (raid1_conf_t *conf, int cnt)
{
	int i = 0;

	while (i < cnt) {
		struct raid1_bh *r1_bh;
		r1_bh = (struct raid1_bh*)kmalloc(sizeof(*r1_bh), GFP_KERNEL);
		if (!r1_bh)
			break;
		memset(r1_bh, 0, sizeof(*r1_bh));
		set_bit(R1BH_PreAlloc, &r1_bh->state);
		r1_bh->mddev = conf->mddev;

		raid1_free_r1bh(r1_bh);
		i++;
	}
	return i;
}

static void raid1_shrink_r1bh(raid1_conf_t *conf)
{
	md_spin_lock_irq(&conf->device_lock);
	while (conf->freer1) {
		struct raid1_bh *r1_bh = conf->freer1;
		conf->freer1 = r1_bh->next_r1;
		conf->freer1_cnt--;
		kfree(r1_bh);
	}
	md_spin_unlock_irq(&conf->device_lock);
}



static inline void raid1_free_buf(struct raid1_bh *r1_bh)
{
	unsigned long flags;
	struct buffer_head *bh = r1_bh->mirror_bh_list;
	raid1_conf_t *conf = mddev_to_conf(r1_bh->mddev);
	r1_bh->mirror_bh_list = NULL;
	
	spin_lock_irqsave(&conf->device_lock, flags);
	r1_bh->next_r1 = conf->freebuf;
	conf->freebuf = r1_bh;
	spin_unlock_irqrestore(&conf->device_lock, flags);
	raid1_free_bh(conf, bh);
}

static struct raid1_bh *raid1_alloc_buf(raid1_conf_t *conf)
{
	struct raid1_bh *r1_bh;

	md_spin_lock_irq(&conf->device_lock);
	wait_event_lock_irq(conf->wait_buffer, conf->freebuf, conf->device_lock);
	r1_bh = conf->freebuf;
	conf->freebuf = r1_bh->next_r1;
	r1_bh->next_r1= NULL;
	md_spin_unlock_irq(&conf->device_lock);

	return r1_bh;
}

static int raid1_grow_buffers (raid1_conf_t *conf, int cnt)
{
	int i = 0;
	struct raid1_bh *head = NULL, **tail;
	tail = &head;

	while (i < cnt) {
		struct raid1_bh *r1_bh;
		struct page *page;

		page = alloc_page(GFP_KERNEL);
		if (!page)
			break;

		r1_bh = (struct raid1_bh *) kmalloc(sizeof(*r1_bh), GFP_KERNEL);
		if (!r1_bh) {
			__free_page(page);
			break;
		}
		memset(r1_bh, 0, sizeof(*r1_bh));
		r1_bh->bh_req.b_page = page;
		r1_bh->bh_req.b_data = page_address(page);
		*tail = r1_bh;
		r1_bh->next_r1 = NULL;
		tail = & r1_bh->next_r1;
		i++;
	}
	/* this lock probably isn't needed, as at the time when
	 * we are allocating buffers, nobody else will be touching the
	 * freebuf list.  But it doesn't hurt....
	 */
	md_spin_lock_irq(&conf->device_lock);
	*tail = conf->freebuf;
	conf->freebuf = head;
	md_spin_unlock_irq(&conf->device_lock);
	return i;
}

static void raid1_shrink_buffers (raid1_conf_t *conf)
{
	struct raid1_bh *head;
	md_spin_lock_irq(&conf->device_lock);
	head = conf->freebuf;
	conf->freebuf = NULL;
	md_spin_unlock_irq(&conf->device_lock);

	while (head) {
		struct raid1_bh *r1_bh = head;
		head = r1_bh->next_r1;
		__free_page(r1_bh->bh_req.b_page);
		kfree(r1_bh);
	}
}

static int raid1_map (mddev_t *mddev, kdev_t *rdev)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);
	int i, disks = MD_SB_DISKS;

	/*
	 * Later we do read balancing on the read side 
	 * now we use the first available disk.
	 */

	for (i = 0; i < disks; i++) {
		if (conf->mirrors[i].operational) {
			*rdev = conf->mirrors[i].dev;
			return (0);
		}
	}

	printk (KERN_ERR "raid1_map(): huh, no more operational devices?\n");
	return (-1);
}

static void raid1_reschedule_retry (struct raid1_bh *r1_bh)
{
	unsigned long flags;
	mddev_t *mddev = r1_bh->mddev;
	raid1_conf_t *conf = mddev_to_conf(mddev);

	md_spin_lock_irqsave(&retry_list_lock, flags);
	if (raid1_retry_list == NULL)
		raid1_retry_tail = &raid1_retry_list;
	*raid1_retry_tail = r1_bh;
	raid1_retry_tail = &r1_bh->next_r1;
	r1_bh->next_r1 = NULL;
	md_spin_unlock_irqrestore(&retry_list_lock, flags);
	md_wakeup_thread(conf->thread);
}


static void inline io_request_done(unsigned long sector, raid1_conf_t *conf, int phase)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->segment_lock, flags);
	if (sector < conf->start_active)
		conf->cnt_done--;
	else if (sector >= conf->start_future && conf->phase == phase)
		conf->cnt_future--;
	else if (!--conf->cnt_pending)
		wake_up(&conf->wait_ready);

	spin_unlock_irqrestore(&conf->segment_lock, flags);
}

static void inline sync_request_done (unsigned long sector, raid1_conf_t *conf)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->segment_lock, flags);
	if (sector >= conf->start_ready)
		--conf->cnt_ready;
	else if (sector >= conf->start_active) {
		if (!--conf->cnt_active) {
			conf->start_active = conf->start_ready;
			wake_up(&conf->wait_done);
		}
	}
	spin_unlock_irqrestore(&conf->segment_lock, flags);
}

/*
 * raid1_end_bh_io() is called when we have finished servicing a mirrored
 * operation and are ready to return a success/failure code to the buffer
 * cache layer.
 */
static void raid1_end_bh_io (struct raid1_bh *r1_bh, int uptodate)
{
	struct buffer_head *bh = r1_bh->master_bh;

	io_request_done(bh->b_rsector, mddev_to_conf(r1_bh->mddev),
			test_bit(R1BH_SyncPhase, &r1_bh->state));

	bh->b_end_io(bh, uptodate);
	raid1_free_r1bh(r1_bh);
}
void raid1_end_request (struct buffer_head *bh, int uptodate)
{
	struct raid1_bh * r1_bh = (struct raid1_bh *)(bh->b_private);

	/*
	 * this branch is our 'one mirror IO has finished' event handler:
	 */
	if (!uptodate)
		md_error (r1_bh->mddev, bh->b_dev);
	else
		/*
		 * Set R1BH_Uptodate in our master buffer_head, so that
		 * we will return a good error code for to the higher
		 * levels even if IO on some other mirrored buffer fails.
		 *
		 * The 'master' represents the complex operation to 
		 * user-side. So if something waits for IO, then it will
		 * wait for the 'master' buffer_head.
		 */
		set_bit (R1BH_Uptodate, &r1_bh->state);

	/*
	 * We split up the read and write side, imho they are 
	 * conceptually different.
	 */

	if ( (r1_bh->cmd == READ) || (r1_bh->cmd == READA) ) {
		/*
		 * we have only one buffer_head on the read side
		 */
		
		if (uptodate) {
			raid1_end_bh_io(r1_bh, uptodate);
			return;
		}
		/*
		 * oops, read error:
		 */
		printk(KERN_ERR "raid1: %s: rescheduling block %lu\n", 
			 partition_name(bh->b_dev), bh->b_blocknr);
		raid1_reschedule_retry(r1_bh);
		return;
	}

	/*
	 * WRITE:
	 *
	 * Let's see if all mirrored write operations have finished 
	 * already.
	 */

	if (atomic_dec_and_test(&r1_bh->remaining))
		raid1_end_bh_io(r1_bh, test_bit(R1BH_Uptodate, &r1_bh->state));
}

/*
 * This routine returns the disk from which the requested read should
 * be done. It bookkeeps the last read position for every disk
 * in array and when new read requests come, the disk which last
 * position is nearest to the request, is chosen.
 *
 * TODO: now if there are 2 mirrors in the same 2 devices, performance
 * degrades dramatically because position is mirror, not device based.
 * This should be changed to be device based. Also atomic sequential
 * reads should be somehow balanced.
 */

static int raid1_read_balance (raid1_conf_t *conf, struct buffer_head *bh)
{
	int new_disk = conf->last_used;
	const int sectors = bh->b_size >> 9;
	const unsigned long this_sector = bh->b_rsector;
	int disk = new_disk;
	unsigned long new_distance;
	unsigned long current_distance;
	
	/*
	 * Check if it is sane at all to balance
	 */
	
	if (conf->resync_mirrors)
		goto rb_out;
	

#if defined(CONFIG_ALPHA) && ((__GNUC__ < 3) || \
			      ((__GNUC__ == 3) && (__GNUC_MINOR__ < 3)))
	/* Work around a compiler bug in older gcc */
	new_disk = *(volatile int *)&new_disk;
#endif

	/* make sure that disk is operational */
	while( !conf->mirrors[new_disk].operational) {
		if (new_disk <= 0) new_disk = conf->raid_disks;
		new_disk--;
		if (new_disk == disk) {
			/*
			 * This means no working disk was found
			 * Nothing much to do, lets not change anything
			 * and hope for the best...
			 */
			
			new_disk = conf->last_used;

			goto rb_out;
		}
	}
	disk = new_disk;
	/* now disk == new_disk == starting point for search */
	
	/*
	 * Don't touch anything for sequential reads.
	 */

	if (this_sector == conf->mirrors[new_disk].head_position)
		goto rb_out;
	
	/*
	 * If reads have been done only on a single disk
	 * for a time, lets give another disk a change.
	 * This is for kicking those idling disks so that
	 * they would find work near some hotspot.
	 */
	
	if (conf->sect_count >= conf->mirrors[new_disk].sect_limit) {
		conf->sect_count = 0;

#if defined(CONFIG_SPARC64) && (__GNUC__ == 2) && (__GNUC_MINOR__ == 92)
		/* Work around a compiler bug in egcs-2.92.11 19980921 */
		new_disk = *(volatile int *)&new_disk;
#endif
		do {
			if (new_disk<=0)
				new_disk = conf->raid_disks;
			new_disk--;
			if (new_disk == disk)
				break;
		} while ((conf->mirrors[new_disk].write_only) ||
			 (!conf->mirrors[new_disk].operational));

		goto rb_out;
	}
	
	current_distance = abs(this_sector -
				conf->mirrors[disk].head_position);
	
	/* Find the disk which is closest */
	
#if defined(CONFIG_ALPHA) && ((__GNUC__ < 3) || \
			      ((__GNUC__ == 3) && (__GNUC_MINOR__ < 3)))
	/* Work around a compiler bug in older gcc */
	disk = *(volatile int *)&disk;
#endif
	do {
		if (disk <= 0)
			disk = conf->raid_disks;
		disk--;
		
		if ((conf->mirrors[disk].write_only) ||
				(!conf->mirrors[disk].operational))
			continue;
		
		new_distance = abs(this_sector -
					conf->mirrors[disk].head_position);
		
		if (new_distance < current_distance) {
			conf->sect_count = 0;
			current_distance = new_distance;
			new_disk = disk;
		}
	} while (disk != conf->last_used);

rb_out:
	conf->mirrors[new_disk].head_position = this_sector + sectors;

	conf->last_used = new_disk;
	conf->sect_count += sectors;

	return new_disk;
}

static int raid1_make_request (mddev_t *mddev, int rw,
			       struct buffer_head * bh)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);
	struct buffer_head *bh_req, *bhl;
	struct raid1_bh * r1_bh;
	int disks = MD_SB_DISKS;
	int i, sum_bhs = 0;
	struct mirror_info *mirror;

	if (!buffer_locked(bh))
		BUG();
	
/*
 * make_request() can abort the operation when READA is being
 * used and no empty request is available.
 *
 * Currently, just replace the command with READ/WRITE.
 */
	if (rw == READA)
		rw = READ;

	r1_bh = raid1_alloc_r1bh (conf);

	spin_lock_irq(&conf->segment_lock);
	wait_event_lock_irq(conf->wait_done,
			bh->b_rsector < conf->start_active ||
			bh->b_rsector >= conf->start_future,
			conf->segment_lock);
	if (bh->b_rsector < conf->start_active) 
		conf->cnt_done++;
	else {
		conf->cnt_future++;
		if (conf->phase)
			set_bit(R1BH_SyncPhase, &r1_bh->state);
	}
	spin_unlock_irq(&conf->segment_lock);
	
	/*
	 * i think the read and write branch should be separated completely,
	 * since we want to do read balancing on the read side for example.
	 * Alternative implementations? :) --mingo
	 */

	r1_bh->master_bh = bh;
	r1_bh->mddev = mddev;
	r1_bh->cmd = rw;

	if (rw == READ) {
		/*
		 * read balancing logic:
		 */
		mirror = conf->mirrors + raid1_read_balance(conf, bh);

		bh_req = &r1_bh->bh_req;
		memcpy(bh_req, bh, sizeof(*bh));
		bh_req->b_blocknr = bh->b_rsector;
		bh_req->b_dev = mirror->dev;
		bh_req->b_rdev = mirror->dev;
	/*	bh_req->b_rsector = bh->n_rsector; */
		bh_req->b_end_io = raid1_end_request;
		bh_req->b_private = r1_bh;
		generic_make_request (rw, bh_req);
		return 0;
	}

	/*
	 * WRITE:
	 */

	bhl = raid1_alloc_bh(conf, conf->raid_disks);
	for (i = 0; i < disks; i++) {
		struct buffer_head *mbh;
		if (!conf->mirrors[i].operational) 
			continue;
 
	/*
	 * We should use a private pool (size depending on NR_REQUEST),
	 * to avoid writes filling up the memory with bhs
	 *
 	 * Such pools are much faster than kmalloc anyways (so we waste
 	 * almost nothing by not using the master bh when writing and
 	 * win alot of cleanness) but for now we are cool enough. --mingo
 	 *
	 * It's safe to sleep here, buffer heads cannot be used in a shared
 	 * manner in the write branch. Look how we lock the buffer at the
 	 * beginning of this function to grok the difference ;)
	 */
 		mbh = bhl;
		if (mbh == NULL) {
			MD_BUG();
			break;
		}
		bhl = mbh->b_next;
		mbh->b_next = NULL;
		mbh->b_this_page = (struct buffer_head *)1;
		
 	/*
 	 * prepare mirrored mbh (fields ordered for max mem throughput):
 	 */
		mbh->b_blocknr    = bh->b_rsector;
		mbh->b_dev        = conf->mirrors[i].dev;
		mbh->b_rdev	  = conf->mirrors[i].dev;
		mbh->b_rsector	  = bh->b_rsector;
		mbh->b_state      = (1<<BH_Req) | (1<<BH_Dirty) |
						(1<<BH_Mapped) | (1<<BH_Lock);

		atomic_set(&mbh->b_count, 1);
 		mbh->b_size       = bh->b_size;
 		mbh->b_page	  = bh->b_page;
 		mbh->b_data	  = bh->b_data;
 		mbh->b_list       = BUF_LOCKED;
 		mbh->b_end_io     = raid1_end_request;
 		mbh->b_private    = r1_bh;

		mbh->b_next = r1_bh->mirror_bh_list;
		r1_bh->mirror_bh_list = mbh;
		sum_bhs++;
	}
	if (bhl) raid1_free_bh(conf,bhl);
	if (!sum_bhs) {
		/* Gag - all mirrors non-operational.. */
		raid1_end_bh_io(r1_bh, 0);
		return 0;
	}
	md_atomic_set(&r1_bh->remaining, sum_bhs);

	/*
	 * We have to be a bit careful about the semaphore above, thats
	 * why we start the requests separately. Since kmalloc() could
	 * fail, sleep and make_request() can sleep too, this is the
	 * safer solution. Imagine, end_request decreasing the semaphore
	 * before we could have set it up ... We could play tricks with
	 * the semaphore (presetting it and correcting at the end if
	 * sum_bhs is not 'n' but we have to do end_request by hand if
	 * all requests finish until we had a chance to set up the
	 * semaphore correctly ... lots of races).
	 */
	bh = r1_bh->mirror_bh_list;
	while(bh) {
		struct buffer_head *bh2 = bh;
		bh = bh->b_next;
		generic_make_request(rw, bh2);
	}
	return (0);
}

static void raid1_status(struct seq_file *seq, mddev_t *mddev)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);
	int i;
	
	seq_printf(seq, " [%d/%d] [", conf->raid_disks,
						 conf->working_disks);
	for (i = 0; i < conf->raid_disks; i++)
		seq_printf(seq, "%s",
			conf->mirrors[i].operational ? "U" : "_");
	seq_printf(seq, "]");
}

#define LAST_DISK KERN_ALERT \
"raid1: only one disk left and IO error.\n"

#define NO_SPARE_DISK KERN_ALERT \
"raid1: no spare disk left, degrading mirror level by one.\n"

#define DISK_FAILED KERN_ALERT \
"raid1: Disk failure on %s, disabling device. \n" \
"	Operation continuing on %d devices\n"

#define START_SYNCING KERN_ALERT \
"raid1: start syncing spare disk.\n"

#define ALREADY_SYNCING KERN_INFO \
"raid1: syncing already in progress.\n"

static void mark_disk_bad (mddev_t *mddev, int failed)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);
	struct mirror_info *mirror = conf->mirrors+failed;
	mdp_super_t *sb = mddev->sb;

	mirror->operational = 0;
	mark_disk_faulty(sb->disks+mirror->number);
	mark_disk_nonsync(sb->disks+mirror->number);
	mark_disk_inactive(sb->disks+mirror->number);
	if (!mirror->write_only)
		sb->active_disks--;
	sb->working_disks--;
	sb->failed_disks++;
	mddev->sb_dirty = 1;
	md_wakeup_thread(conf->thread);
	if (!mirror->write_only)
		conf->working_disks--;
	printk (DISK_FAILED, partition_name (mirror->dev),
				 conf->working_disks);
}

static int raid1_error (mddev_t *mddev, kdev_t dev)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);
	struct mirror_info * mirrors = conf->mirrors;
	int disks = MD_SB_DISKS;
	int i;

	/* Find the drive.
	 * If it is not operational, then we have already marked it as dead
	 * else if it is the last working disks, ignore the error, let the
	 * next level up know.
	 * else mark the drive as failed
	 */

	for (i = 0; i < disks; i++)
		if (mirrors[i].dev==dev && mirrors[i].operational)
			break;
	if (i == disks)
		return 0;

	if (i < conf->raid_disks && conf->working_disks == 1) {
		/* Don't fail the drive, act as though we were just a
		 * normal single drive
		 */

		return 1;
	}
	mark_disk_bad(mddev, i);
	return 0;
}

#undef LAST_DISK
#undef NO_SPARE_DISK
#undef DISK_FAILED
#undef START_SYNCING


static void print_raid1_conf (raid1_conf_t *conf)
{
	int i;
	struct mirror_info *tmp;

	printk("RAID1 conf printout:\n");
	if (!conf) {
		printk("(conf==NULL)\n");
		return;
	}
	printk(" --- wd:%d rd:%d nd:%d\n", conf->working_disks,
			 conf->raid_disks, conf->nr_disks);

	for (i = 0; i < MD_SB_DISKS; i++) {
		tmp = conf->mirrors + i;
		printk(" disk %d, s:%d, o:%d, n:%d rd:%d us:%d dev:%s\n",
			i, tmp->spare,tmp->operational,
			tmp->number,tmp->raid_disk,tmp->used_slot,
			partition_name(tmp->dev));
	}
}

static void close_sync(raid1_conf_t *conf)
{
	mddev_t *mddev = conf->mddev;
	/* If reconstruction was interrupted, we need to close the "active" and "pending"
	 * holes.
	 * we know that there are no active rebuild requests, os cnt_active == cnt_ready ==0
	 */
	/* this is really needed when recovery stops too... */
	spin_lock_irq(&conf->segment_lock);
	conf->start_active = conf->start_pending;
	conf->start_ready = conf->start_pending;
	wait_event_lock_irq(conf->wait_ready, !conf->cnt_pending, conf->segment_lock);
	conf->start_active =conf->start_ready = conf->start_pending = conf->start_future;
	conf->start_future = (mddev->sb->size<<1)+1;
	conf->cnt_pending = conf->cnt_future;
	conf->cnt_future = 0;
	conf->phase = conf->phase ^1;
	wait_event_lock_irq(conf->wait_ready, !conf->cnt_pending, conf->segment_lock);
	conf->start_active = conf->start_ready = conf->start_pending = conf->start_future = 0;
	conf->phase = 0;
	conf->cnt_future = conf->cnt_done;;
	conf->cnt_done = 0;
	spin_unlock_irq(&conf->segment_lock);
	wake_up(&conf->wait_done);
}

static int raid1_diskop(mddev_t *mddev, mdp_disk_t **d, int state)
{
	int err = 0;
	int i, failed_disk=-1, spare_disk=-1, removed_disk=-1, added_disk=-1;
	raid1_conf_t *conf = mddev->private;
	struct mirror_info *tmp, *sdisk, *fdisk, *rdisk, *adisk;
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *failed_desc, *spare_desc, *added_desc;
	mdk_rdev_t *spare_rdev, *failed_rdev;

	print_raid1_conf(conf);

	switch (state) {
	case DISKOP_SPARE_ACTIVE:
	case DISKOP_SPARE_INACTIVE:
		/* need to wait for pending sync io before locking device */
		close_sync(conf);
	}

	md_spin_lock_irq(&conf->device_lock);
	/*
	 * find the disk ...
	 */
	switch (state) {

	case DISKOP_SPARE_ACTIVE:

		/*
		 * Find the failed disk within the RAID1 configuration ...
		 * (this can only be in the first conf->working_disks part)
		 */
		for (i = 0; i < conf->raid_disks; i++) {
			tmp = conf->mirrors + i;
			if ((!tmp->operational && !tmp->spare) ||
					!tmp->used_slot) {
				failed_disk = i;
				break;
			}
		}
		/*
		 * When we activate a spare disk we _must_ have a disk in
		 * the lower (active) part of the array to replace. 
		 */
		if ((failed_disk == -1) || (failed_disk >= conf->raid_disks)) {
			MD_BUG();
			err = 1;
			goto abort;
		}
		/* fall through */

	case DISKOP_SPARE_WRITE:
	case DISKOP_SPARE_INACTIVE:

		/*
		 * Find the spare disk ... (can only be in the 'high'
		 * area of the array)
		 */
		for (i = conf->raid_disks; i < MD_SB_DISKS; i++) {
			tmp = conf->mirrors + i;
			if (tmp->spare && tmp->number == (*d)->number) {
				spare_disk = i;
				break;
			}
		}
		if (spare_disk == -1) {
			MD_BUG();
			err = 1;
			goto abort;
		}
		break;

	case DISKOP_HOT_REMOVE_DISK:

		for (i = 0; i < MD_SB_DISKS; i++) {
			tmp = conf->mirrors + i;
			if (tmp->used_slot && (tmp->number == (*d)->number)) {
				if (tmp->operational) {
					err = -EBUSY;
					goto abort;
				}
				removed_disk = i;
				break;
			}
		}
		if (removed_disk == -1) {
			MD_BUG();
			err = 1;
			goto abort;
		}
		break;

	case DISKOP_HOT_ADD_DISK:

		for (i = conf->raid_disks; i < MD_SB_DISKS; i++) {
			tmp = conf->mirrors + i;
			if (!tmp->used_slot) {
				added_disk = i;
				break;
			}
		}
		if (added_disk == -1) {
			MD_BUG();
			err = 1;
			goto abort;
		}
		break;
	}

	switch (state) {
	/*
	 * Switch the spare disk to write-only mode:
	 */
	case DISKOP_SPARE_WRITE:
		sdisk = conf->mirrors + spare_disk;
		sdisk->operational = 1;
		sdisk->write_only = 1;
		break;
	/*
	 * Deactivate a spare disk:
	 */
	case DISKOP_SPARE_INACTIVE:
		if (conf->start_future > 0) {
			MD_BUG();
			err = -EBUSY;
			break;
		}
		sdisk = conf->mirrors + spare_disk;
		sdisk->operational = 0;
		sdisk->write_only = 0;
		break;
	/*
	 * Activate (mark read-write) the (now sync) spare disk,
	 * which means we switch it's 'raid position' (->raid_disk)
	 * with the failed disk. (only the first 'conf->nr_disks'
	 * slots are used for 'real' disks and we must preserve this
	 * property)
	 */
	case DISKOP_SPARE_ACTIVE:
		if (conf->start_future > 0) {
			MD_BUG();
			err = -EBUSY;
			break;
		}
		sdisk = conf->mirrors + spare_disk;
		fdisk = conf->mirrors + failed_disk;

		spare_desc = &sb->disks[sdisk->number];
		failed_desc = &sb->disks[fdisk->number];

		if (spare_desc != *d) {
			MD_BUG();
			err = 1;
			goto abort;
		}

		if (spare_desc->raid_disk != sdisk->raid_disk) {
			MD_BUG();
			err = 1;
			goto abort;
		}
			
		if (sdisk->raid_disk != spare_disk) {
			MD_BUG();
			err = 1;
			goto abort;
		}

		if (failed_desc->raid_disk != fdisk->raid_disk) {
			MD_BUG();
			err = 1;
			goto abort;
		}

		if (fdisk->raid_disk != failed_disk) {
			MD_BUG();
			err = 1;
			goto abort;
		}

		/*
		 * do the switch finally
		 */
		spare_rdev = find_rdev_nr(mddev, spare_desc->number);
		failed_rdev = find_rdev_nr(mddev, failed_desc->number);

		/* There must be a spare_rdev, but there may not be a
		 * failed_rdev.  That slot might be empty...
		 */
		spare_rdev->desc_nr = failed_desc->number;
		if (failed_rdev)
			failed_rdev->desc_nr = spare_desc->number;
		
		xchg_values(*spare_desc, *failed_desc);
		xchg_values(*fdisk, *sdisk);

		/*
		 * (careful, 'failed' and 'spare' are switched from now on)
		 *
		 * we want to preserve linear numbering and we want to
		 * give the proper raid_disk number to the now activated
		 * disk. (this means we switch back these values)
		 */
	
		xchg_values(spare_desc->raid_disk, failed_desc->raid_disk);
		xchg_values(sdisk->raid_disk, fdisk->raid_disk);
		xchg_values(spare_desc->number, failed_desc->number);
		xchg_values(sdisk->number, fdisk->number);

		*d = failed_desc;

		if (sdisk->dev == MKDEV(0,0))
			sdisk->used_slot = 0;
		/*
		 * this really activates the spare.
		 */
		fdisk->spare = 0;
		fdisk->write_only = 0;

		/*
		 * if we activate a spare, we definitely replace a
		 * non-operational disk slot in the 'low' area of
		 * the disk array.
		 */

		conf->working_disks++;

		break;

	case DISKOP_HOT_REMOVE_DISK:
		rdisk = conf->mirrors + removed_disk;

		if (rdisk->spare && (removed_disk < conf->raid_disks)) {
			MD_BUG();	
			err = 1;
			goto abort;
		}
		rdisk->dev = MKDEV(0,0);
		rdisk->used_slot = 0;
		conf->nr_disks--;
		break;

	case DISKOP_HOT_ADD_DISK:
		adisk = conf->mirrors + added_disk;
		added_desc = *d;

		if (added_disk != added_desc->number) {
			MD_BUG();	
			err = 1;
			goto abort;
		}

		adisk->number = added_desc->number;
		adisk->raid_disk = added_desc->raid_disk;
		adisk->dev = MKDEV(added_desc->major,added_desc->minor);

		adisk->operational = 0;
		adisk->write_only = 0;
		adisk->spare = 1;
		adisk->used_slot = 1;
		adisk->head_position = 0;
		conf->nr_disks++;

		break;

	default:
		MD_BUG();	
		err = 1;
		goto abort;
	}
abort:
	md_spin_unlock_irq(&conf->device_lock);
	if (state == DISKOP_SPARE_ACTIVE || state == DISKOP_SPARE_INACTIVE)
		/* should move to "END_REBUILD" when such exists */
		raid1_shrink_buffers(conf);

	print_raid1_conf(conf);
	return err;
}


#define IO_ERROR KERN_ALERT \
"raid1: %s: unrecoverable I/O read error for block %lu\n"

#define REDIRECT_SECTOR KERN_ERR \
"raid1: %s: redirecting sector %lu to another mirror\n"

/*
 * This is a kernel thread which:
 *
 *	1.	Retries failed read operations on working mirrors.
 *	2.	Updates the raid superblock when problems encounter.
 *	3.	Performs writes following reads for array syncronising.
 */
static void end_sync_write(struct buffer_head *bh, int uptodate);
static void end_sync_read(struct buffer_head *bh, int uptodate);

static void raid1d (void *data)
{
	struct raid1_bh *r1_bh;
	struct buffer_head *bh;
	unsigned long flags;
	raid1_conf_t *conf = data;
	mddev_t *mddev = conf->mddev;
	kdev_t dev;

	if (mddev->sb_dirty)
		md_update_sb(mddev);

	for (;;) {
		md_spin_lock_irqsave(&retry_list_lock, flags);
		r1_bh = raid1_retry_list;
		if (!r1_bh)
			break;
		raid1_retry_list = r1_bh->next_r1;
		md_spin_unlock_irqrestore(&retry_list_lock, flags);

		mddev = r1_bh->mddev;
		bh = &r1_bh->bh_req;
		switch(r1_bh->cmd) {
		case SPECIAL:
			/* have to allocate lots of bh structures and
			 * schedule writes
			 */
			if (test_bit(R1BH_Uptodate, &r1_bh->state)) {
				int i, sum_bhs = 0;
				int disks = MD_SB_DISKS;
				struct buffer_head *bhl, *mbh;
				
				conf = mddev_to_conf(mddev);
				bhl = raid1_alloc_bh(conf, conf->raid_disks); /* don't really need this many */
				for (i = 0; i < disks ; i++) {
					if (!conf->mirrors[i].operational)
						continue;
					if (i==conf->last_used)
						/* we read from here, no need to write */
						continue;
					if (i < conf->raid_disks
					    && !conf->resync_mirrors)
						/* don't need to write this,
						 * we are just rebuilding */
						continue;
					mbh = bhl;
					if (!mbh) {
						MD_BUG();
						break;
					}
					bhl = mbh->b_next;
					mbh->b_this_page = (struct buffer_head *)1;

						
				/*
				 * prepare mirrored bh (fields ordered for max mem throughput):
				 */
					mbh->b_blocknr    = bh->b_blocknr;
					mbh->b_dev        = conf->mirrors[i].dev;
					mbh->b_rdev	  = conf->mirrors[i].dev;
					mbh->b_rsector	  = bh->b_blocknr;
					mbh->b_state      = (1<<BH_Req) | (1<<BH_Dirty) |
						(1<<BH_Mapped) | (1<<BH_Lock);
					atomic_set(&mbh->b_count, 1);
					mbh->b_size       = bh->b_size;
					mbh->b_page	  = bh->b_page;
					mbh->b_data	  = bh->b_data;
					mbh->b_list       = BUF_LOCKED;
					mbh->b_end_io     = end_sync_write;
					mbh->b_private    = r1_bh;

					mbh->b_next = r1_bh->mirror_bh_list;
					r1_bh->mirror_bh_list = mbh;

					sum_bhs++;
				}
				md_atomic_set(&r1_bh->remaining, sum_bhs);
				if (bhl) raid1_free_bh(conf, bhl);
				mbh = r1_bh->mirror_bh_list;

				if (!sum_bhs) {
					/* nowhere to write this too... I guess we
					 * must be done
					 */
					sync_request_done(bh->b_blocknr, conf);
					md_done_sync(mddev, bh->b_size>>9, 0);
					raid1_free_buf(r1_bh);
				} else
				while (mbh) {
					struct buffer_head *bh1 = mbh;
					mbh = mbh->b_next;
					generic_make_request(WRITE, bh1);
					md_sync_acct(bh1->b_dev, bh1->b_size/512);
				}
			} else {
				/* There is no point trying a read-for-reconstruct
				 * as reconstruct is about to be aborted
				 */

				printk (IO_ERROR, partition_name(bh->b_dev), bh->b_blocknr);
				md_done_sync(mddev, bh->b_size>>9, 0);
			}

			break;
		case READ:
		case READA:
			dev = bh->b_dev;
			raid1_map (mddev, &bh->b_dev);
			if (bh->b_dev == dev) {
				printk (IO_ERROR, partition_name(bh->b_dev), bh->b_blocknr);
				raid1_end_bh_io(r1_bh, 0);
			} else {
				printk (REDIRECT_SECTOR,
					partition_name(bh->b_dev), bh->b_blocknr);
				bh->b_rdev = bh->b_dev;
				bh->b_rsector = bh->b_blocknr;
				generic_make_request (r1_bh->cmd, bh);
			}
			break;
		}
	}
	md_spin_unlock_irqrestore(&retry_list_lock, flags);
}
#undef IO_ERROR
#undef REDIRECT_SECTOR

/*
 * Private kernel thread to reconstruct mirrors after an unclean
 * shutdown.
 */
static void raid1syncd (void *data)
{
	raid1_conf_t *conf = data;
	mddev_t *mddev = conf->mddev;

	if (!conf->resync_mirrors)
		return;
	if (conf->resync_mirrors == 2)
		return;
	down(&mddev->recovery_sem);
	if (!md_do_sync(mddev, NULL)) {
		/*
		 * Only if everything went Ok.
		 */
		conf->resync_mirrors = 0;
	}

	close_sync(conf);

	up(&mddev->recovery_sem);
	raid1_shrink_buffers(conf);
}

/*
 * perform a "sync" on one "block"
 *
 * We need to make sure that no normal I/O request - particularly write
 * requests - conflict with active sync requests.
 * This is achieved by conceptually dividing the device space into a
 * number of sections:
 *  DONE: 0 .. a-1     These blocks are in-sync
 *  ACTIVE: a.. b-1    These blocks may have active sync requests, but
 *                     no normal IO requests
 *  READY: b .. c-1    These blocks have no normal IO requests - sync
 *                     request may be happening
 *  PENDING: c .. d-1  These blocks may have IO requests, but no new
 *                     ones will be added
 *  FUTURE:  d .. end  These blocks are not to be considered yet. IO may
 *                     be happening, but not sync
 *
 * We keep a
 *   phase    which flips (0 or 1) each time d moves and
 * a count of:
 *   z =  active io requests in FUTURE since d moved - marked with
 *        current phase
 *   y =  active io requests in FUTURE before d moved, or PENDING -
 *        marked with previous phase
 *   x =  active sync requests in READY
 *   w =  active sync requests in ACTIVE
 *   v =  active io requests in DONE
 *
 * Normally, a=b=c=d=0 and z= active io requests
 *   or a=b=c=d=END and v= active io requests
 * Allowed changes to a,b,c,d:
 * A:  c==d &&  y==0 -> d+=window, y=z, z=0, phase=!phase
 * B:  y==0 -> c=d
 * C:   b=c, w+=x, x=0
 * D:  w==0 -> a=b
 * E: a==b==c==d==end -> a=b=c=d=0, z=v, v=0
 *
 * At start of sync we apply A.
 * When y reaches 0, we apply B then A then being sync requests
 * When sync point reaches c-1, we wait for y==0, and W==0, and
 * then apply apply B then A then D then C.
 * Finally, we apply E
 *
 * The sync request simply issues a "read" against a working drive
 * This is marked so that on completion the raid1d thread is woken to
 * issue suitable write requests
 */

static int raid1_sync_request (mddev_t *mddev, unsigned long sector_nr)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);
	struct mirror_info *mirror;
	struct raid1_bh *r1_bh;
	struct buffer_head *bh;
	int bsize;
	int disk;
	int block_nr;
	int buffs;

	if (!sector_nr) {
		/* we want enough buffers to hold twice the window of 128*/
		buffs = 128 *2 / (PAGE_SIZE>>9);
		buffs = raid1_grow_buffers(conf, buffs);
		if (buffs < 2)
			goto nomem;
		conf->window = buffs*(PAGE_SIZE>>9)/2;
	}
	spin_lock_irq(&conf->segment_lock);
	if (!sector_nr) {
		/* initialize ...*/
		conf->start_active = 0;
		conf->start_ready = 0;
		conf->start_pending = 0;
		conf->start_future = 0;
		conf->phase = 0;
		
		conf->cnt_future += conf->cnt_done+conf->cnt_pending;
		conf->cnt_done = conf->cnt_pending = 0;
		if (conf->cnt_ready || conf->cnt_active)
			MD_BUG();
	}
	while (sector_nr >= conf->start_pending) {
		PRINTK("wait .. sect=%lu start_active=%d ready=%d pending=%d future=%d, cnt_done=%d active=%d ready=%d pending=%d future=%d\n",
			sector_nr, conf->start_active, conf->start_ready, conf->start_pending, conf->start_future,
			conf->cnt_done, conf->cnt_active, conf->cnt_ready, conf->cnt_pending, conf->cnt_future);
		wait_event_lock_irq(conf->wait_done,
					!conf->cnt_active,
					conf->segment_lock);
		wait_event_lock_irq(conf->wait_ready,
					!conf->cnt_pending,
					conf->segment_lock);
		conf->start_active = conf->start_ready;
		conf->start_ready = conf->start_pending;
		conf->start_pending = conf->start_future;
		conf->start_future = conf->start_future+conf->window;
		// Note: falling off the end is not a problem
		conf->phase = conf->phase ^1;
		conf->cnt_active = conf->cnt_ready;
		conf->cnt_ready = 0;
		conf->cnt_pending = conf->cnt_future;
		conf->cnt_future = 0;
		wake_up(&conf->wait_done);
	}
	conf->cnt_ready++;
	spin_unlock_irq(&conf->segment_lock);
		

	/* If reconstructing, and >1 working disc,
	 * could dedicate one to rebuild and others to
	 * service read requests ..
	 */
	disk = conf->last_used;
	/* make sure disk is operational */
	while (!conf->mirrors[disk].operational) {
		if (disk <= 0) disk = conf->raid_disks;
		disk--;
		if (disk == conf->last_used)
			break;
	}
	conf->last_used = disk;
	
	mirror = conf->mirrors+conf->last_used;
	
	r1_bh = raid1_alloc_buf (conf);
	r1_bh->master_bh = NULL;
	r1_bh->mddev = mddev;
	r1_bh->cmd = SPECIAL;
	bh = &r1_bh->bh_req;

	block_nr = sector_nr;
	bsize = 512;
	while (!(block_nr & 1) && bsize < PAGE_SIZE
			&& (block_nr+2)*(bsize>>9) <= (mddev->sb->size *2)) {
		block_nr >>= 1;
		bsize <<= 1;
	}
	bh->b_size = bsize;
	bh->b_list = BUF_LOCKED;
	bh->b_dev = mirror->dev;
	bh->b_rdev = mirror->dev;
	bh->b_state = (1<<BH_Req) | (1<<BH_Mapped) | (1<<BH_Lock);
	if (!bh->b_page)
		BUG();
	if (!bh->b_data)
		BUG();
	if (bh->b_data != page_address(bh->b_page))
		BUG();
	bh->b_end_io = end_sync_read;
	bh->b_private = r1_bh;
	bh->b_blocknr = sector_nr;
	bh->b_rsector = sector_nr;
	init_waitqueue_head(&bh->b_wait);

	generic_make_request(READ, bh);
	md_sync_acct(bh->b_dev, bh->b_size/512);

	return (bsize >> 9);

nomem:
	raid1_shrink_buffers(conf);
	return -ENOMEM;
}

static void end_sync_read(struct buffer_head *bh, int uptodate)
{
	struct raid1_bh * r1_bh = (struct raid1_bh *)(bh->b_private);

	/* we have read a block, now it needs to be re-written,
	 * or re-read if the read failed.
	 * We don't do much here, just schedule handling by raid1d
	 */
	if (!uptodate)
		md_error (r1_bh->mddev, bh->b_dev);
	else
		set_bit(R1BH_Uptodate, &r1_bh->state);
	raid1_reschedule_retry(r1_bh);
}

static void end_sync_write(struct buffer_head *bh, int uptodate)
{
 	struct raid1_bh * r1_bh = (struct raid1_bh *)(bh->b_private);
	
	if (!uptodate)
 		md_error (r1_bh->mddev, bh->b_dev);
	if (atomic_dec_and_test(&r1_bh->remaining)) {
		mddev_t *mddev = r1_bh->mddev;
 		unsigned long sect = bh->b_blocknr;
		int size = bh->b_size;
		raid1_free_buf(r1_bh);
		sync_request_done(sect, mddev_to_conf(mddev));
		md_done_sync(mddev,size>>9, uptodate);
	}
}

#define INVALID_LEVEL KERN_WARNING \
"raid1: md%d: raid level not set to mirroring (%d)\n"

#define NO_SB KERN_ERR \
"raid1: disabled mirror %s (couldn't access raid superblock)\n"

#define ERRORS KERN_ERR \
"raid1: disabled mirror %s (errors detected)\n"

#define NOT_IN_SYNC KERN_ERR \
"raid1: disabled mirror %s (not in sync)\n"

#define INCONSISTENT KERN_ERR \
"raid1: disabled mirror %s (inconsistent descriptor)\n"

#define ALREADY_RUNNING KERN_ERR \
"raid1: disabled mirror %s (mirror %d already operational)\n"

#define OPERATIONAL KERN_INFO \
"raid1: device %s operational as mirror %d\n"

#define MEM_ERROR KERN_ERR \
"raid1: couldn't allocate memory for md%d\n"

#define SPARE KERN_INFO \
"raid1: spare disk %s\n"

#define NONE_OPERATIONAL KERN_ERR \
"raid1: no operational mirrors for md%d\n"

#define ARRAY_IS_ACTIVE KERN_INFO \
"raid1: raid set md%d active with %d out of %d mirrors\n"

#define THREAD_ERROR KERN_ERR \
"raid1: couldn't allocate thread for md%d\n"

#define START_RESYNC KERN_WARNING \
"raid1: raid set md%d not clean; reconstructing mirrors\n"

static int raid1_run (mddev_t *mddev)
{
	raid1_conf_t *conf;
	int i, j, disk_idx;
	struct mirror_info *disk;
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *descriptor;
	mdk_rdev_t *rdev;
	struct md_list_head *tmp;
	int start_recovery = 0;

	MOD_INC_USE_COUNT;

	if (sb->level != 1) {
		printk(INVALID_LEVEL, mdidx(mddev), sb->level);
		goto out;
	}
	/*
	 * copy the already verified devices into our private RAID1
	 * bookkeeping area. [whatever we allocate in raid1_run(),
	 * should be freed in raid1_stop()]
	 */

	conf = kmalloc(sizeof(raid1_conf_t), GFP_KERNEL);
	mddev->private = conf;
	if (!conf) {
		printk(MEM_ERROR, mdidx(mddev));
		goto out;
	}
	memset(conf, 0, sizeof(*conf));

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty) {
			printk(ERRORS, partition_name(rdev->dev));
		} else {
			if (!rdev->sb) {
				MD_BUG();
				continue;
			}
		}
		if (rdev->desc_nr == -1) {
			MD_BUG();
			continue;
		}
		descriptor = &sb->disks[rdev->desc_nr];
		disk_idx = descriptor->raid_disk;
		disk = conf->mirrors + disk_idx;

		if (disk_faulty(descriptor)) {
			disk->number = descriptor->number;
			disk->raid_disk = disk_idx;
			disk->dev = rdev->dev;
			disk->sect_limit = MAX_WORK_PER_DISK;
			disk->operational = 0;
			disk->write_only = 0;
			disk->spare = 0;
			disk->used_slot = 1;
			disk->head_position = 0;
			continue;
		}
		if (disk_active(descriptor)) {
			if (!disk_sync(descriptor)) {
				printk(NOT_IN_SYNC,
					partition_name(rdev->dev));
				continue;
			}
			if ((descriptor->number > MD_SB_DISKS) ||
					 (disk_idx > sb->raid_disks)) {

				printk(INCONSISTENT,
					partition_name(rdev->dev));
				continue;
			}
			if (disk->operational) {
				printk(ALREADY_RUNNING,
					partition_name(rdev->dev),
					disk_idx);
				continue;
			}
			printk(OPERATIONAL, partition_name(rdev->dev),
 					disk_idx);
			disk->number = descriptor->number;
			disk->raid_disk = disk_idx;
			disk->dev = rdev->dev;
			disk->sect_limit = MAX_WORK_PER_DISK;
			disk->operational = 1;
			disk->write_only = 0;
			disk->spare = 0;
			disk->used_slot = 1;
			disk->head_position = 0;
			conf->working_disks++;
		} else {
		/*
		 * Must be a spare disk ..
		 */
			printk(SPARE, partition_name(rdev->dev));
			disk->number = descriptor->number;
			disk->raid_disk = disk_idx;
			disk->dev = rdev->dev;
			disk->sect_limit = MAX_WORK_PER_DISK;
			disk->operational = 0;
			disk->write_only = 0;
			disk->spare = 1;
			disk->used_slot = 1;
			disk->head_position = 0;
		}
	}
	conf->raid_disks = sb->raid_disks;
	conf->nr_disks = sb->nr_disks;
	conf->mddev = mddev;
	conf->device_lock = MD_SPIN_LOCK_UNLOCKED;

	conf->segment_lock = MD_SPIN_LOCK_UNLOCKED;
	init_waitqueue_head(&conf->wait_buffer);
	init_waitqueue_head(&conf->wait_done);
	init_waitqueue_head(&conf->wait_ready);

	if (!conf->working_disks) {
		printk(NONE_OPERATIONAL, mdidx(mddev));
		goto out_free_conf;
	}


	/* pre-allocate some buffer_head structures.
	 * As a minimum, 1 r1bh and raid_disks buffer_heads
	 * would probably get us by in tight memory situations,
	 * but a few more is probably a good idea.
	 * For now, try NR_RESERVED_BUFS r1bh and
	 * NR_RESERVED_BUFS*raid_disks bufferheads
	 * This will allow at least NR_RESERVED_BUFS concurrent
	 * reads or writes even if kmalloc starts failing
	 */
	if (raid1_grow_r1bh(conf, NR_RESERVED_BUFS) < NR_RESERVED_BUFS ||
	    raid1_grow_bh(conf, NR_RESERVED_BUFS*conf->raid_disks)
	                      < NR_RESERVED_BUFS*conf->raid_disks) {
		printk(MEM_ERROR, mdidx(mddev));
		goto out_free_conf;
	}

	for (i = 0; i < MD_SB_DISKS; i++) {
		
		descriptor = sb->disks+i;
		disk_idx = descriptor->raid_disk;
		disk = conf->mirrors + disk_idx;

		if (disk_faulty(descriptor) && (disk_idx < conf->raid_disks) &&
				!disk->used_slot) {

			disk->number = descriptor->number;
			disk->raid_disk = disk_idx;
			disk->dev = MKDEV(0,0);

			disk->operational = 0;
			disk->write_only = 0;
			disk->spare = 0;
			disk->used_slot = 1;
			disk->head_position = 0;
		}
	}

	/*
	 * find the first working one and use it as a starting point
	 * to read balancing.
	 */
	for (j = 0; !conf->mirrors[j].operational && j < MD_SB_DISKS; j++)
		/* nothing */;
	conf->last_used = j;


	if (conf->working_disks != sb->raid_disks) {
		printk(KERN_ALERT "raid1: md%d, not all disks are operational -- trying to recover array\n", mdidx(mddev));
		start_recovery = 1;
	}

	{
		const char * name = "raid1d";

		conf->thread = md_register_thread(raid1d, conf, name);
		if (!conf->thread) {
			printk(THREAD_ERROR, mdidx(mddev));
			goto out_free_conf;
		}
	}

	if (!start_recovery && !(sb->state & (1 << MD_SB_CLEAN)) &&
	    (conf->working_disks > 1)) {
		const char * name = "raid1syncd";

		conf->resync_thread = md_register_thread(raid1syncd, conf,name);
		if (!conf->resync_thread) {
			printk(THREAD_ERROR, mdidx(mddev));
			goto out_free_conf;
		}

		printk(START_RESYNC, mdidx(mddev));
		conf->resync_mirrors = 1;
		md_wakeup_thread(conf->resync_thread);
	}

	/*
	 * Regenerate the "device is in sync with the raid set" bit for
	 * each device.
	 */
	for (i = 0; i < MD_SB_DISKS; i++) {
		mark_disk_nonsync(sb->disks+i);
		for (j = 0; j < sb->raid_disks; j++) {
			if (!conf->mirrors[j].operational)
				continue;
			if (sb->disks[i].number == conf->mirrors[j].number)
				mark_disk_sync(sb->disks+i);
		}
	}
	sb->active_disks = conf->working_disks;

	if (start_recovery)
		md_recover_arrays();


	printk(ARRAY_IS_ACTIVE, mdidx(mddev), sb->active_disks, sb->raid_disks);
	/*
	 * Ok, everything is just fine now
	 */
	return 0;

out_free_conf:
	raid1_shrink_r1bh(conf);
	raid1_shrink_bh(conf);
	raid1_shrink_buffers(conf);
	kfree(conf);
	mddev->private = NULL;
out:
	MOD_DEC_USE_COUNT;
	return -EIO;
}

#undef INVALID_LEVEL
#undef NO_SB
#undef ERRORS
#undef NOT_IN_SYNC
#undef INCONSISTENT
#undef ALREADY_RUNNING
#undef OPERATIONAL
#undef SPARE
#undef NONE_OPERATIONAL
#undef ARRAY_IS_ACTIVE

static int raid1_stop_resync (mddev_t *mddev)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);

	if (conf->resync_thread) {
		if (conf->resync_mirrors) {
			conf->resync_mirrors = 2;
			md_interrupt_thread(conf->resync_thread);

			printk(KERN_INFO "raid1: mirror resync was not fully finished, restarting next time.\n");
			return 1;
		}
		return 0;
	}
	return 0;
}

static int raid1_restart_resync (mddev_t *mddev)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);

	if (conf->resync_mirrors) {
		if (!conf->resync_thread) {
			MD_BUG();
			return 0;
		}
		conf->resync_mirrors = 1;
		md_wakeup_thread(conf->resync_thread);
		return 1;
	}
	return 0;
}

static int raid1_stop (mddev_t *mddev)
{
	raid1_conf_t *conf = mddev_to_conf(mddev);

	md_unregister_thread(conf->thread);
	if (conf->resync_thread)
		md_unregister_thread(conf->resync_thread);
	raid1_shrink_r1bh(conf);
	raid1_shrink_bh(conf);
	raid1_shrink_buffers(conf);
	kfree(conf);
	mddev->private = NULL;
	MOD_DEC_USE_COUNT;
	return 0;
}

static mdk_personality_t raid1_personality=
{
	name:		"raid1",
	make_request:	raid1_make_request,
	run:		raid1_run,
	stop:		raid1_stop,
	status:		raid1_status,
	error_handler:	raid1_error,
	diskop:		raid1_diskop,
	stop_resync:	raid1_stop_resync,
	restart_resync:	raid1_restart_resync,
	sync_request:	raid1_sync_request
};

static int md__init raid1_init (void)
{
	return register_md_personality (RAID1, &raid1_personality);
}

static void raid1_exit (void)
{
	unregister_md_personality (RAID1);
}

module_init(raid1_init);
module_exit(raid1_exit);
MODULE_LICENSE("GPL");
