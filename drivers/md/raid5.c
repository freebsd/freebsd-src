/*
 * raid5.c : Multiple Devices driver for Linux
 *	   Copyright (C) 1996, 1997 Ingo Molnar, Miguel de Icaza, Gadi Oxman
 *	   Copyright (C) 1999, 2000 Ingo Molnar
 *
 * RAID-5 management functions.
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


#include <linux/config.h>
#include <linux/module.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <linux/raid/raid5.h>
#include <asm/bitops.h>
#include <asm/atomic.h>

static mdk_personality_t raid5_personality;

/*
 * Stripe cache
 */

#define NR_STRIPES		256
#define	IO_THRESHOLD		1
#define HASH_PAGES		1
#define HASH_PAGES_ORDER	0
#define NR_HASH			(HASH_PAGES * PAGE_SIZE / sizeof(struct stripe_head *))
#define HASH_MASK		(NR_HASH - 1)
#define stripe_hash(conf, sect)	((conf)->stripe_hashtbl[((sect) / ((conf)->buffer_size >> 9)) & HASH_MASK])

/*
 * The following can be used to debug the driver
 */
#define RAID5_DEBUG	0
#define RAID5_PARANOIA	1
#if RAID5_PARANOIA && CONFIG_SMP
# define CHECK_DEVLOCK() if (!spin_is_locked(&conf->device_lock)) BUG()
#else
# define CHECK_DEVLOCK()
#endif

#if RAID5_DEBUG
#define PRINTK(x...) printk(x)
#define inline
#define __inline__
#else
#define PRINTK(x...) do { } while (0)
#endif

static void print_raid5_conf (raid5_conf_t *conf);

static inline void __release_stripe(raid5_conf_t *conf, struct stripe_head *sh)
{
	if (atomic_dec_and_test(&sh->count)) {
		if (!list_empty(&sh->lru))
			BUG();
		if (atomic_read(&conf->active_stripes)==0)
			BUG();
		if (test_bit(STRIPE_HANDLE, &sh->state)) {
			if (test_bit(STRIPE_DELAYED, &sh->state))
				list_add_tail(&sh->lru, &conf->delayed_list);
			else
				list_add_tail(&sh->lru, &conf->handle_list);
			md_wakeup_thread(conf->thread);
		} else {
			if (test_and_clear_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
				atomic_dec(&conf->preread_active_stripes);
				if (atomic_read(&conf->preread_active_stripes) < IO_THRESHOLD)
					md_wakeup_thread(conf->thread);
			}
			list_add_tail(&sh->lru, &conf->inactive_list);
			atomic_dec(&conf->active_stripes);
			if (!conf->inactive_blocked ||
			    atomic_read(&conf->active_stripes) < (NR_STRIPES*3/4))
				wake_up(&conf->wait_for_stripe);
		}
	}
}
static void release_stripe(struct stripe_head *sh)
{
	raid5_conf_t *conf = sh->raid_conf;
	unsigned long flags;
	
	spin_lock_irqsave(&conf->device_lock, flags);
	__release_stripe(conf, sh);
	spin_unlock_irqrestore(&conf->device_lock, flags);
}

static void remove_hash(struct stripe_head *sh)
{
	PRINTK("remove_hash(), stripe %lu\n", sh->sector);

	if (sh->hash_pprev) {
		if (sh->hash_next)
			sh->hash_next->hash_pprev = sh->hash_pprev;
		*sh->hash_pprev = sh->hash_next;
		sh->hash_pprev = NULL;
	}
}

static __inline__ void insert_hash(raid5_conf_t *conf, struct stripe_head *sh)
{
	struct stripe_head **shp = &stripe_hash(conf, sh->sector);

	PRINTK("insert_hash(), stripe %lu\n",sh->sector);

	CHECK_DEVLOCK();
	if ((sh->hash_next = *shp) != NULL)
		(*shp)->hash_pprev = &sh->hash_next;
	*shp = sh;
	sh->hash_pprev = shp;
}


/* find an idle stripe, make sure it is unhashed, and return it. */
static struct stripe_head *get_free_stripe(raid5_conf_t *conf)
{
	struct stripe_head *sh = NULL;
	struct list_head *first;

	CHECK_DEVLOCK();
	if (list_empty(&conf->inactive_list))
		goto out;
	first = conf->inactive_list.next;
	sh = list_entry(first, struct stripe_head, lru);
	list_del_init(first);
	remove_hash(sh);
	atomic_inc(&conf->active_stripes);
out:
	return sh;
}

static void shrink_buffers(struct stripe_head *sh, int num)
{
	struct buffer_head *bh;
	int i;

	for (i=0; i<num ; i++) {
		bh = sh->bh_cache[i];
		if (!bh)
			return;
		sh->bh_cache[i] = NULL;
		free_page((unsigned long) bh->b_data);
		kfree(bh);
	}
}

static int grow_buffers(struct stripe_head *sh, int num, int b_size, int priority)
{
	struct buffer_head *bh;
	int i;

	for (i=0; i<num; i++) {
		struct page *page;
		bh = kmalloc(sizeof(struct buffer_head), priority);
		if (!bh)
			return 1;
		memset(bh, 0, sizeof (struct buffer_head));
		init_waitqueue_head(&bh->b_wait);
		if ((page = alloc_page(priority)))
			bh->b_data = page_address(page);
		else {
			kfree(bh);
			return 1;
		}
		atomic_set(&bh->b_count, 0);
		bh->b_page = page;
		sh->bh_cache[i] = bh;

	}
	return 0;
}

static struct buffer_head *raid5_build_block (struct stripe_head *sh, int i);

static inline void init_stripe(struct stripe_head *sh, unsigned long sector)
{
	raid5_conf_t *conf = sh->raid_conf;
	int disks = conf->raid_disks, i;

	if (atomic_read(&sh->count) != 0)
		BUG();
	if (test_bit(STRIPE_HANDLE, &sh->state))
		BUG();
	
	CHECK_DEVLOCK();
	PRINTK("init_stripe called, stripe %lu\n", sh->sector);

	remove_hash(sh);
	
	sh->sector = sector;
	sh->size = conf->buffer_size;
	sh->state = 0;

	for (i=disks; i--; ) {
		if (sh->bh_read[i] || sh->bh_write[i] || sh->bh_written[i] ||
		    buffer_locked(sh->bh_cache[i])) {
			printk("sector=%lx i=%d %p %p %p %d\n",
			       sh->sector, i, sh->bh_read[i],
			       sh->bh_write[i], sh->bh_written[i],
			       buffer_locked(sh->bh_cache[i]));
			BUG();
		}
		clear_bit(BH_Uptodate, &sh->bh_cache[i]->b_state);
		raid5_build_block(sh, i);
	}
	insert_hash(conf, sh);
}

/* the buffer size has changed, so unhash all stripes
 * as active stripes complete, they will go onto inactive list
 */
static void shrink_stripe_cache(raid5_conf_t *conf)
{
	int i;
	CHECK_DEVLOCK();
	if (atomic_read(&conf->active_stripes))
		BUG();
	for (i=0; i < NR_HASH; i++) {
		struct stripe_head *sh;
		while ((sh = conf->stripe_hashtbl[i])) 
			remove_hash(sh);
	}
}

static struct stripe_head *__find_stripe(raid5_conf_t *conf, unsigned long sector)
{
	struct stripe_head *sh;

	CHECK_DEVLOCK();
	PRINTK("__find_stripe, sector %lu\n", sector);
	for (sh = stripe_hash(conf, sector); sh; sh = sh->hash_next)
		if (sh->sector == sector)
			return sh;
	PRINTK("__stripe %lu not in cache\n", sector);
	return NULL;
}

static struct stripe_head *get_active_stripe(raid5_conf_t *conf, unsigned long sector, int size, int noblock) 
{
	struct stripe_head *sh;

	PRINTK("get_stripe, sector %lu\n", sector);

	md_spin_lock_irq(&conf->device_lock);

	do {
		if (conf->buffer_size == 0 ||
		    (size && size != conf->buffer_size)) {
			/* either the size is being changed (buffer_size==0) or
			 * we need to change it.
			 * If size==0, we can proceed as soon as buffer_size gets set.
			 * If size>0, we can proceed when active_stripes reaches 0, or
			 * when someone else sets the buffer_size to size.
			 * If someone sets the buffer size to something else, we will need to
			 * assert that we want to change it again
			 */
			int oldsize = conf->buffer_size;
			PRINTK("get_stripe %ld/%d buffer_size is %d, %d active\n", sector, size, conf->buffer_size, atomic_read(&conf->active_stripes));
			if (size==0)
				wait_event_lock_irq(conf->wait_for_stripe,
						    conf->buffer_size,
						    conf->device_lock);
			else {
				while (conf->buffer_size != size && atomic_read(&conf->active_stripes)) {
					conf->buffer_size = 0;
					wait_event_lock_irq(conf->wait_for_stripe,
							    atomic_read(&conf->active_stripes)==0 || conf->buffer_size,
							    conf->device_lock);
					PRINTK("waited and now  %ld/%d buffer_size is %d - %d active\n", sector, size,
					       conf->buffer_size, atomic_read(&conf->active_stripes));
				}

				if (conf->buffer_size != size) {
					printk("raid5: switching cache buffer size, %d --> %d\n", oldsize, size);
					shrink_stripe_cache(conf);
					if (size==0) BUG();
					conf->buffer_size = size;
					PRINTK("size now %d\n", conf->buffer_size);
				}
			}
		}
		if (size == 0)
			sector -= sector & ((conf->buffer_size>>9)-1);

		sh = __find_stripe(conf, sector);
		if (!sh) {
			if (!conf->inactive_blocked)
				sh = get_free_stripe(conf);
			if (noblock && sh == NULL)
				break;
			if (!sh) {
				conf->inactive_blocked = 1;
				wait_event_lock_irq(conf->wait_for_stripe,
						    !list_empty(&conf->inactive_list) &&
						    (atomic_read(&conf->active_stripes) < (NR_STRIPES *3/4)
						     || !conf->inactive_blocked),
						    conf->device_lock);
				conf->inactive_blocked = 0;
			} else
				init_stripe(sh, sector);
		} else {
			if (atomic_read(&sh->count)) {
				if (!list_empty(&sh->lru))
					BUG();
			} else {
				if (!test_bit(STRIPE_HANDLE, &sh->state))
					atomic_inc(&conf->active_stripes);
				if (list_empty(&sh->lru))
					BUG();
				list_del_init(&sh->lru);
			}
		}
	} while (sh == NULL);

	if (sh)
		atomic_inc(&sh->count);

	md_spin_unlock_irq(&conf->device_lock);
	return sh;
}

static int grow_stripes(raid5_conf_t *conf, int num, int priority)
{
	struct stripe_head *sh;

	while (num--) {
		sh = kmalloc(sizeof(struct stripe_head), priority);
		if (!sh)
			return 1;
		memset(sh, 0, sizeof(*sh));
		sh->raid_conf = conf;
		sh->lock = SPIN_LOCK_UNLOCKED;

		if (grow_buffers(sh, conf->raid_disks, PAGE_SIZE, priority)) {
			shrink_buffers(sh, conf->raid_disks);
			kfree(sh);
			return 1;
		}
		/* we just created an active stripe so... */
		atomic_set(&sh->count, 1);
		atomic_inc(&conf->active_stripes);
		INIT_LIST_HEAD(&sh->lru);
		release_stripe(sh);
	}
	return 0;
}

static void shrink_stripes(raid5_conf_t *conf, int num)
{
	struct stripe_head *sh;

	while (num--) {
		spin_lock_irq(&conf->device_lock);
		sh = get_free_stripe(conf);
		spin_unlock_irq(&conf->device_lock);
		if (!sh)
			break;
		if (atomic_read(&sh->count))
			BUG();
		shrink_buffers(sh, conf->raid_disks);
		kfree(sh);
		atomic_dec(&conf->active_stripes);
	}
}


static void raid5_end_read_request (struct buffer_head * bh, int uptodate)
{
 	struct stripe_head *sh = bh->b_private;
	raid5_conf_t *conf = sh->raid_conf;
	int disks = conf->raid_disks, i;
	unsigned long flags;

	for (i=0 ; i<disks; i++)
		if (bh == sh->bh_cache[i])
			break;

	PRINTK("end_read_request %lu/%d, count: %d, uptodate %d.\n", sh->sector, i, atomic_read(&sh->count), uptodate);
	if (i == disks) {
		BUG();
		return;
	}

	if (uptodate) {
		struct buffer_head *buffer;
		spin_lock_irqsave(&conf->device_lock, flags);
		/* we can return a buffer if we bypassed the cache or
		 * if the top buffer is not in highmem.  If there are
		 * multiple buffers, leave the extra work to
		 * handle_stripe
		 */
		buffer = sh->bh_read[i];
		if (buffer &&
		    (!PageHighMem(buffer->b_page)
		     || buffer->b_page == bh->b_page )
			) {
			sh->bh_read[i] = buffer->b_reqnext;
			buffer->b_reqnext = NULL;
		} else
			buffer = NULL;
		spin_unlock_irqrestore(&conf->device_lock, flags);
		if (sh->bh_page[i]==NULL)
			set_bit(BH_Uptodate, &bh->b_state);
		if (buffer) {
			if (buffer->b_page != bh->b_page)
				memcpy(buffer->b_data, bh->b_data, bh->b_size);
			buffer->b_end_io(buffer, 1);
		}
	} else {
		md_error(conf->mddev, bh->b_dev);
		clear_bit(BH_Uptodate, &bh->b_state);
	}
	/* must restore b_page before unlocking buffer... */
	if (sh->bh_page[i]) {
		bh->b_page = sh->bh_page[i];
		bh->b_data = page_address(bh->b_page);
		sh->bh_page[i] = NULL;
		clear_bit(BH_Uptodate, &bh->b_state);
	}
	clear_bit(BH_Lock, &bh->b_state);
	set_bit(STRIPE_HANDLE, &sh->state);
	release_stripe(sh);
}

static void raid5_end_write_request (struct buffer_head *bh, int uptodate)
{
 	struct stripe_head *sh = bh->b_private;
	raid5_conf_t *conf = sh->raid_conf;
	int disks = conf->raid_disks, i;
	unsigned long flags;

	for (i=0 ; i<disks; i++)
		if (bh == sh->bh_cache[i])
			break;

	PRINTK("end_write_request %lu/%d, count %d, uptodate: %d.\n", sh->sector, i, atomic_read(&sh->count), uptodate);
	if (i == disks) {
		BUG();
		return;
	}

	md_spin_lock_irqsave(&conf->device_lock, flags);
	if (!uptodate)
		md_error(conf->mddev, bh->b_dev);
	clear_bit(BH_Lock, &bh->b_state);
	set_bit(STRIPE_HANDLE, &sh->state);
	__release_stripe(conf, sh);
	md_spin_unlock_irqrestore(&conf->device_lock, flags);
}
	


static struct buffer_head *raid5_build_block (struct stripe_head *sh, int i)
{
	raid5_conf_t *conf = sh->raid_conf;
	struct buffer_head *bh = sh->bh_cache[i];
	unsigned long block = sh->sector / (sh->size >> 9);

	init_buffer(bh, raid5_end_read_request, sh);
	bh->b_dev       = conf->disks[i].dev;
	bh->b_blocknr   = block;

	bh->b_state	= (1 << BH_Req) | (1 << BH_Mapped);
	bh->b_size	= sh->size;
	bh->b_list	= BUF_LOCKED;
	return bh;
}

static int raid5_error (mddev_t *mddev, kdev_t dev)
{
	raid5_conf_t *conf = (raid5_conf_t *) mddev->private;
	mdp_super_t *sb = mddev->sb;
	struct disk_info *disk;
	int i;

	PRINTK("raid5_error called\n");

	for (i = 0, disk = conf->disks; i < conf->raid_disks; i++, disk++) {
		if (disk->dev == dev) {
			if (disk->operational) {
				disk->operational = 0;
				mark_disk_faulty(sb->disks+disk->number);
				mark_disk_nonsync(sb->disks+disk->number);
				mark_disk_inactive(sb->disks+disk->number);
				sb->active_disks--;
				sb->working_disks--;
				sb->failed_disks++;
				mddev->sb_dirty = 1;
				conf->working_disks--;
				conf->failed_disks++;
				md_wakeup_thread(conf->thread);
				printk (KERN_ALERT
					"raid5: Disk failure on %s, disabling device."
					" Operation continuing on %d devices\n",
					partition_name (dev), conf->working_disks);
			}
			return 0;
		}
	}
	/*
	 * handle errors in spares (during reconstruction)
	 */
	if (conf->spare) {
		disk = conf->spare;
		if (disk->dev == dev) {
			printk (KERN_ALERT
				"raid5: Disk failure on spare %s\n",
				partition_name (dev));
			if (!conf->spare->operational) {
				/* probably a SET_DISK_FAULTY ioctl */
				return -EIO;
			}
			disk->operational = 0;
			disk->write_only = 0;
			conf->spare = NULL;
			mark_disk_faulty(sb->disks+disk->number);
			mark_disk_nonsync(sb->disks+disk->number);
			mark_disk_inactive(sb->disks+disk->number);
			sb->spare_disks--;
			sb->working_disks--;
			sb->failed_disks++;

			mddev->sb_dirty = 1;
			md_wakeup_thread(conf->thread);

			return 0;
		}
	}
	MD_BUG();
	return -EIO;
}	

/*
 * Input: a 'big' sector number,
 * Output: index of the data and parity disk, and the sector # in them.
 */
static unsigned long raid5_compute_sector(unsigned long r_sector, unsigned int raid_disks,
			unsigned int data_disks, unsigned int * dd_idx,
			unsigned int * pd_idx, raid5_conf_t *conf)
{
	unsigned long stripe;
	unsigned long chunk_number;
	unsigned int chunk_offset;
	unsigned long new_sector;
	int sectors_per_chunk = conf->chunk_size >> 9;

	/* First compute the information on this sector */

	/*
	 * Compute the chunk number and the sector offset inside the chunk
	 */
	chunk_number = r_sector / sectors_per_chunk;
	chunk_offset = r_sector % sectors_per_chunk;

	/*
	 * Compute the stripe number
	 */
	stripe = chunk_number / data_disks;

	/*
	 * Compute the data disk and parity disk indexes inside the stripe
	 */
	*dd_idx = chunk_number % data_disks;

	/*
	 * Select the parity disk based on the user selected algorithm.
	 */
	if (conf->level == 4)
		*pd_idx = data_disks;
	else switch (conf->algorithm) {
		case ALGORITHM_LEFT_ASYMMETRIC:
			*pd_idx = data_disks - stripe % raid_disks;
			if (*dd_idx >= *pd_idx)
				(*dd_idx)++;
			break;
		case ALGORITHM_RIGHT_ASYMMETRIC:
			*pd_idx = stripe % raid_disks;
			if (*dd_idx >= *pd_idx)
				(*dd_idx)++;
			break;
		case ALGORITHM_LEFT_SYMMETRIC:
			*pd_idx = data_disks - stripe % raid_disks;
			*dd_idx = (*pd_idx + 1 + *dd_idx) % raid_disks;
			break;
		case ALGORITHM_RIGHT_SYMMETRIC:
			*pd_idx = stripe % raid_disks;
			*dd_idx = (*pd_idx + 1 + *dd_idx) % raid_disks;
			break;
		default:
			printk ("raid5: unsupported algorithm %d\n", conf->algorithm);
	}

	/*
	 * Finally, compute the new sector number
	 */
	new_sector = stripe * sectors_per_chunk + chunk_offset;
	return new_sector;
}

#if 0
static unsigned long compute_blocknr(struct stripe_head *sh, int i)
{
	raid5_conf_t *conf = sh->raid_conf;
	int raid_disks = conf->raid_disks, data_disks = raid_disks - 1;
	unsigned long new_sector = sh->sector, check;
	int sectors_per_chunk = conf->chunk_size >> 9;
	unsigned long stripe = new_sector / sectors_per_chunk;
	int chunk_offset = new_sector % sectors_per_chunk;
	int chunk_number, dummy1, dummy2, dd_idx = i;
	unsigned long r_sector, blocknr;

	switch (conf->algorithm) {
		case ALGORITHM_LEFT_ASYMMETRIC:
		case ALGORITHM_RIGHT_ASYMMETRIC:
			if (i > sh->pd_idx)
				i--;
			break;
		case ALGORITHM_LEFT_SYMMETRIC:
		case ALGORITHM_RIGHT_SYMMETRIC:
			if (i < sh->pd_idx)
				i += raid_disks;
			i -= (sh->pd_idx + 1);
			break;
		default:
			printk ("raid5: unsupported algorithm %d\n", conf->algorithm);
	}

	chunk_number = stripe * data_disks + i;
	r_sector = chunk_number * sectors_per_chunk + chunk_offset;
	blocknr = r_sector / (sh->size >> 9);

	check = raid5_compute_sector (r_sector, raid_disks, data_disks, &dummy1, &dummy2, conf);
	if (check != sh->sector || dummy1 != dd_idx || dummy2 != sh->pd_idx) {
		printk("compute_blocknr: map not correct\n");
		return 0;
	}
	return blocknr;
}
#endif

#define check_xor() 	do { 					\
			   if (count == MAX_XOR_BLOCKS) {	\
				xor_block(count, bh_ptr);	\
				count = 1;			\
			   }					\
			} while(0)


static void compute_block(struct stripe_head *sh, int dd_idx)
{
	raid5_conf_t *conf = sh->raid_conf;
	int i, count, disks = conf->raid_disks;
	struct buffer_head *bh_ptr[MAX_XOR_BLOCKS], *bh;

	PRINTK("compute_block, stripe %lu, idx %d\n", sh->sector, dd_idx);


	memset(sh->bh_cache[dd_idx]->b_data, 0, sh->size);
	bh_ptr[0] = sh->bh_cache[dd_idx];
	count = 1;
	for (i = disks ; i--; ) {
		if (i == dd_idx)
			continue;
		bh = sh->bh_cache[i];
		if (buffer_uptodate(bh))
			bh_ptr[count++] = bh;
		else
			printk("compute_block() %d, stripe %lu, %d not present\n", dd_idx, sh->sector, i);

		check_xor();
	}
	if (count != 1)
		xor_block(count, bh_ptr);
	set_bit(BH_Uptodate, &sh->bh_cache[dd_idx]->b_state);
}

static void compute_parity(struct stripe_head *sh, int method)
{
	raid5_conf_t *conf = sh->raid_conf;
	int i, pd_idx = sh->pd_idx, disks = conf->raid_disks, count;
	struct buffer_head *bh_ptr[MAX_XOR_BLOCKS];
	struct buffer_head *chosen[MD_SB_DISKS];

	PRINTK("compute_parity, stripe %lu, method %d\n", sh->sector, method);
	memset(chosen, 0, sizeof(chosen));

	count = 1;
	bh_ptr[0] = sh->bh_cache[pd_idx];
	switch(method) {
	case READ_MODIFY_WRITE:
		if (!buffer_uptodate(sh->bh_cache[pd_idx]))
			BUG();
		for (i=disks ; i-- ;) {
			if (i==pd_idx)
				continue;
			if (sh->bh_write[i] &&
			    buffer_uptodate(sh->bh_cache[i])) {
				bh_ptr[count++] = sh->bh_cache[i];
				chosen[i] = sh->bh_write[i];
				sh->bh_write[i] = sh->bh_write[i]->b_reqnext;
				chosen[i]->b_reqnext = sh->bh_written[i];
				sh->bh_written[i] = chosen[i];
				check_xor();
			}
		}
		break;
	case RECONSTRUCT_WRITE:
		memset(sh->bh_cache[pd_idx]->b_data, 0, sh->size);
		for (i= disks; i-- ;)
			if (i!=pd_idx && sh->bh_write[i]) {
				chosen[i] = sh->bh_write[i];
				sh->bh_write[i] = sh->bh_write[i]->b_reqnext;
				chosen[i]->b_reqnext = sh->bh_written[i];
				sh->bh_written[i] = chosen[i];
			}
		break;
	case CHECK_PARITY:
		break;
	}
	if (count>1) {
		xor_block(count, bh_ptr);
		count = 1;
	}
	
	for (i = disks; i--;)
		if (chosen[i]) {
			struct buffer_head *bh = sh->bh_cache[i];
			char *bdata;
			bdata = bh_kmap(chosen[i]);
			memcpy(bh->b_data,
			       bdata,sh->size);
			bh_kunmap(chosen[i]);
			set_bit(BH_Lock, &bh->b_state);
			mark_buffer_uptodate(bh, 1);
		}

	switch(method) {
	case RECONSTRUCT_WRITE:
	case CHECK_PARITY:
		for (i=disks; i--;)
			if (i != pd_idx) {
				bh_ptr[count++] = sh->bh_cache[i];
				check_xor();
			}
		break;
	case READ_MODIFY_WRITE:
		for (i = disks; i--;)
			if (chosen[i]) {
				bh_ptr[count++] = sh->bh_cache[i];
				check_xor();
			}
	}
	if (count != 1)
		xor_block(count, bh_ptr);
	
	if (method != CHECK_PARITY) {
		mark_buffer_uptodate(sh->bh_cache[pd_idx], 1);
		set_bit(BH_Lock, &sh->bh_cache[pd_idx]->b_state);
	} else
		mark_buffer_uptodate(sh->bh_cache[pd_idx], 0);
}

static void add_stripe_bh (struct stripe_head *sh, struct buffer_head *bh, int dd_idx, int rw)
{
	struct buffer_head **bhp;
	raid5_conf_t *conf = sh->raid_conf;

	PRINTK("adding bh b#%lu to stripe s#%lu\n", bh->b_blocknr, sh->sector);


	spin_lock(&sh->lock);
	spin_lock_irq(&conf->device_lock);
	bh->b_reqnext = NULL;
	if (rw == READ)
		bhp = &sh->bh_read[dd_idx];
	else
		bhp = &sh->bh_write[dd_idx];
	while (*bhp) {
		printk(KERN_NOTICE "raid5: multiple %d requests for sector %ld\n", rw, sh->sector);
		bhp = & (*bhp)->b_reqnext;
	}
	*bhp = bh;
	spin_unlock_irq(&conf->device_lock);
	spin_unlock(&sh->lock);

	PRINTK("added bh b#%lu to stripe s#%lu, disk %d.\n", bh->b_blocknr, sh->sector, dd_idx);
}





/*
 * handle_stripe - do things to a stripe.
 *
 * We lock the stripe and then examine the state of various bits
 * to see what needs to be done.
 * Possible results:
 *    return some read request which now have data
 *    return some write requests which are safely on disc
 *    schedule a read on some buffers
 *    schedule a write of some buffers
 *    return confirmation of parity correctness
 *
 * Parity calculations are done inside the stripe lock
 * buffers are taken off read_list or write_list, and bh_cache buffers
 * get BH_Lock set before the stripe lock is released.
 *
 */
 
static void handle_stripe(struct stripe_head *sh)
{
	raid5_conf_t *conf = sh->raid_conf;
	int disks = conf->raid_disks;
	struct buffer_head *return_ok= NULL, *return_fail = NULL;
	int action[MD_SB_DISKS];
	int i;
	int syncing;
	int locked=0, uptodate=0, to_read=0, to_write=0, failed=0, written=0;
	int failed_num=0;
	struct buffer_head *bh;

	PRINTK("handling stripe %ld, cnt=%d, pd_idx=%d\n", sh->sector, atomic_read(&sh->count), sh->pd_idx);
	memset(action, 0, sizeof(action));

	spin_lock(&sh->lock);
	clear_bit(STRIPE_HANDLE, &sh->state);
	clear_bit(STRIPE_DELAYED, &sh->state);

	syncing = test_bit(STRIPE_SYNCING, &sh->state);
	/* Now to look around and see what can be done */

	for (i=disks; i--; ) {
		bh = sh->bh_cache[i];
		PRINTK("check %d: state 0x%lx read %p write %p written %p\n", i, bh->b_state, sh->bh_read[i], sh->bh_write[i], sh->bh_written[i]);
		/* maybe we can reply to a read */
		if (buffer_uptodate(bh) && sh->bh_read[i]) {
			struct buffer_head *rbh, *rbh2;
			PRINTK("Return read for disc %d\n", i);
			spin_lock_irq(&conf->device_lock);
			rbh = sh->bh_read[i];
			sh->bh_read[i] = NULL;
			spin_unlock_irq(&conf->device_lock);
			while (rbh) {
				char *bdata;
				bdata = bh_kmap(rbh);
				memcpy(bdata, bh->b_data, bh->b_size);
				bh_kunmap(rbh);
				rbh2 = rbh->b_reqnext;
				rbh->b_reqnext = return_ok;
				return_ok = rbh;
				rbh = rbh2;
			}
		}

		/* now count some things */
		if (buffer_locked(bh)) locked++;
		if (buffer_uptodate(bh)) uptodate++;

		
		if (sh->bh_read[i]) to_read++;
		if (sh->bh_write[i]) to_write++;
		if (sh->bh_written[i]) written++;
		if (!conf->disks[i].operational) {
			failed++;
			failed_num = i;
		}
	}
	PRINTK("locked=%d uptodate=%d to_read=%d to_write=%d failed=%d failed_num=%d\n",
	       locked, uptodate, to_read, to_write, failed, failed_num);
	/* check if the array has lost two devices and, if so, some requests might
	 * need to be failed
	 */
	if (failed > 1 && to_read+to_write+written) {
		for (i=disks; i--; ) {
			/* fail all writes first */
			if (sh->bh_write[i]) to_write--;
			while ((bh = sh->bh_write[i])) {
				sh->bh_write[i] = bh->b_reqnext;
				bh->b_reqnext = return_fail;
				return_fail = bh;
			}
			/* and fail all 'written' */
			if (sh->bh_written[i]) written--;
			while ((bh = sh->bh_written[i])) {
				sh->bh_written[i] = bh->b_reqnext;
				bh->b_reqnext = return_fail;
				return_fail = bh;
			}

			/* fail any reads if this device is non-operational */
			if (!conf->disks[i].operational) {
				spin_lock_irq(&conf->device_lock);
				if (sh->bh_read[i]) to_read--;
				while ((bh = sh->bh_read[i])) {
					sh->bh_read[i] = bh->b_reqnext;
					bh->b_reqnext = return_fail;
					return_fail = bh;
				}
				spin_unlock_irq(&conf->device_lock);
			}
		}
	}
	if (failed > 1 && syncing) {
		md_done_sync(conf->mddev, (sh->size>>9) - sh->sync_redone,0);
		clear_bit(STRIPE_SYNCING, &sh->state);
		syncing = 0;
	}

	/* might be able to return some write requests if the parity block
	 * is safe, or on a failed drive
	 */
	bh = sh->bh_cache[sh->pd_idx];
	if ( written &&
	     ( (conf->disks[sh->pd_idx].operational && !buffer_locked(bh) && buffer_uptodate(bh))
	       || (failed == 1 && failed_num == sh->pd_idx))
	    ) {
	    /* any written block on a uptodate or failed drive can be returned */
	    for (i=disks; i--; )
		if (sh->bh_written[i]) {
		    bh = sh->bh_cache[i];
		    if (!conf->disks[sh->pd_idx].operational ||
			(!buffer_locked(bh) && buffer_uptodate(bh)) ) {
			/* maybe we can return some write requests */
			struct buffer_head *wbh, *wbh2;
			PRINTK("Return write for disc %d\n", i);
			wbh = sh->bh_written[i];
			sh->bh_written[i] = NULL;
			while (wbh) {
			    wbh2 = wbh->b_reqnext;
			    wbh->b_reqnext = return_ok;
			    return_ok = wbh;
			    wbh = wbh2;
			}
		    }
		}
	}
		
	/* Now we might consider reading some blocks, either to check/generate
	 * parity, or to satisfy requests
	 */
	if (to_read || (syncing && (uptodate+failed < disks))) {
		for (i=disks; i--;) {
			bh = sh->bh_cache[i];
			if (!buffer_locked(bh) && !buffer_uptodate(bh) &&
			    (sh->bh_read[i] || syncing || (failed && sh->bh_read[failed_num]))) {
				/* we would like to get this block, possibly
				 * by computing it, but we might not be able to
				 */
				if (uptodate == disks-1) {
					PRINTK("Computing block %d\n", i);
					compute_block(sh, i);
					uptodate++;
				} else if (conf->disks[i].operational) {
					set_bit(BH_Lock, &bh->b_state);
					action[i] = READ+1;
					/* if I am just reading this block and we don't have
					   a failed drive, or any pending writes then sidestep the cache */
					if (sh->bh_page[i]) BUG();
					if (sh->bh_read[i] && !sh->bh_read[i]->b_reqnext &&
					    ! syncing && !failed && !to_write) {
						sh->bh_page[i] = sh->bh_cache[i]->b_page;
						sh->bh_cache[i]->b_page =  sh->bh_read[i]->b_page;
						sh->bh_cache[i]->b_data =  sh->bh_read[i]->b_data;
					}
					locked++;
					PRINTK("Reading block %d (sync=%d)\n", i, syncing);
					if (syncing)
						md_sync_acct(conf->disks[i].dev, bh->b_size>>9);
				}
			}
		}
		set_bit(STRIPE_HANDLE, &sh->state);
	}

	/* now to consider writing and what else, if anything should be read */
	if (to_write) {
		int rmw=0, rcw=0;
		for (i=disks ; i--;) {
			/* would I have to read this buffer for read_modify_write */
			bh = sh->bh_cache[i];
			if ((sh->bh_write[i] || i == sh->pd_idx) &&
			    (!buffer_locked(bh) || sh->bh_page[i]) &&
			    !buffer_uptodate(bh)) {
				if (conf->disks[i].operational 
/*				    && !(conf->resync_parity && i == sh->pd_idx) */
					)
					rmw++;
				else rmw += 2*disks;  /* cannot read it */
			}
			/* Would I have to read this buffer for reconstruct_write */
			if (!sh->bh_write[i] && i != sh->pd_idx &&
			    (!buffer_locked(bh) || sh->bh_page[i]) &&
			    !buffer_uptodate(bh)) {
				if (conf->disks[i].operational) rcw++;
				else rcw += 2*disks;
			}
		}
		PRINTK("for sector %ld, rmw=%d rcw=%d\n", sh->sector, rmw, rcw);
		set_bit(STRIPE_HANDLE, &sh->state);
		if (rmw < rcw && rmw > 0)
			/* prefer read-modify-write, but need to get some data */
			for (i=disks; i--;) {
				bh = sh->bh_cache[i];
				if ((sh->bh_write[i] || i == sh->pd_idx) &&
				    !buffer_locked(bh) && !buffer_uptodate(bh) &&
				    conf->disks[i].operational) {
					if (test_bit(STRIPE_PREREAD_ACTIVE, &sh->state))
					{
						PRINTK("Read_old block %d for r-m-w\n", i);
						set_bit(BH_Lock, &bh->b_state);
						action[i] = READ+1;
						locked++;
					} else {
						set_bit(STRIPE_DELAYED, &sh->state);
						set_bit(STRIPE_HANDLE, &sh->state);
					}
				}
			}
		if (rcw <= rmw && rcw > 0)
			/* want reconstruct write, but need to get some data */
			for (i=disks; i--;) {
				bh = sh->bh_cache[i];
				if (!sh->bh_write[i]  && i != sh->pd_idx &&
				    !buffer_locked(bh) && !buffer_uptodate(bh) &&
				    conf->disks[i].operational) {
					if (test_bit(STRIPE_PREREAD_ACTIVE, &sh->state))
					{
						PRINTK("Read_old block %d for Reconstruct\n", i);
						set_bit(BH_Lock, &bh->b_state);
						action[i] = READ+1;
						locked++;
					} else {
						set_bit(STRIPE_DELAYED, &sh->state);
						set_bit(STRIPE_HANDLE, &sh->state);
					}
				}
			}
		/* now if nothing is locked, and if we have enough data, we can start a write request */
		if (locked == 0 && (rcw == 0 ||rmw == 0)) {
			PRINTK("Computing parity...\n");
			compute_parity(sh, rcw==0 ? RECONSTRUCT_WRITE : READ_MODIFY_WRITE);
			/* now every locked buffer is ready to be written */
			for (i=disks; i--;)
				if (buffer_locked(sh->bh_cache[i])) {
					PRINTK("Writing block %d\n", i);
					locked++;
					action[i] = WRITE+1;
					if (!conf->disks[i].operational
					    || (i==sh->pd_idx && failed == 0))
						set_bit(STRIPE_INSYNC, &sh->state);
				}
			if (test_and_clear_bit(STRIPE_PREREAD_ACTIVE, &sh->state)) {
				atomic_dec(&conf->preread_active_stripes);
				if (atomic_read(&conf->preread_active_stripes) < IO_THRESHOLD)
					md_wakeup_thread(conf->thread);
			}
		}
	}

	/* maybe we need to check and possibly fix the parity for this stripe
	 * Any reads will already have been scheduled, so we just see if enough data
	 * is available
	 */
	if (syncing && locked == 0 &&
	    !test_bit(STRIPE_INSYNC, &sh->state) && failed <= 1) {
		set_bit(STRIPE_HANDLE, &sh->state);
		if (failed == 0) {
			if (uptodate != disks)
				BUG();
			compute_parity(sh, CHECK_PARITY);
			uptodate--;
			bh = sh->bh_cache[sh->pd_idx];
			if ((*(u32*)bh->b_data) == 0 &&
			    !memcmp(bh->b_data, bh->b_data+4, bh->b_size-4)) {
				/* parity is correct (on disc, not in buffer any more) */
				set_bit(STRIPE_INSYNC, &sh->state);
			}
		}
		if (!test_bit(STRIPE_INSYNC, &sh->state)) {
			struct disk_info *spare;
			if (failed==0)
				failed_num = sh->pd_idx;
			/* should be able to compute the missing block and write it to spare */
			if (!buffer_uptodate(sh->bh_cache[failed_num])) {
				if (uptodate+1 != disks)
					BUG();
				compute_block(sh, failed_num);
				uptodate++;
			}
			if (uptodate != disks)
				BUG();
			bh = sh->bh_cache[failed_num];
			set_bit(BH_Lock, &bh->b_state);
			action[failed_num] = WRITE+1;
			locked++;
			set_bit(STRIPE_INSYNC, &sh->state);
			if (conf->disks[failed_num].operational)
				md_sync_acct(conf->disks[failed_num].dev, bh->b_size>>9);
			else if ((spare=conf->spare))
				md_sync_acct(spare->dev, bh->b_size>>9);

		}
	}
	if (syncing && locked == 0 && test_bit(STRIPE_INSYNC, &sh->state)) {
		md_done_sync(conf->mddev, (sh->size>>9) - sh->sync_redone,1);
		clear_bit(STRIPE_SYNCING, &sh->state);
	}
	
	
	spin_unlock(&sh->lock);

	while ((bh=return_ok)) {
		return_ok = bh->b_reqnext;
		bh->b_reqnext = NULL;
		bh->b_end_io(bh, 1);
	}
	while ((bh=return_fail)) {
		return_fail = bh->b_reqnext;
		bh->b_reqnext = NULL;
		bh->b_end_io(bh, 0);
	}
	for (i=disks; i-- ;) 
		if (action[i]) {
			struct buffer_head *bh = sh->bh_cache[i];
			struct disk_info *spare = conf->spare;
			int skip = 0;
			if (action[i] == READ+1)
				bh->b_end_io = raid5_end_read_request;
			else
				bh->b_end_io = raid5_end_write_request;
			if (conf->disks[i].operational)
				bh->b_dev = conf->disks[i].dev;
			else if (spare && action[i] == WRITE+1)
				bh->b_dev = spare->dev;
			else skip=1;
			if (!skip) {
				PRINTK("for %ld schedule op %d on disc %d\n", sh->sector, action[i]-1, i);
				atomic_inc(&sh->count);
				bh->b_rdev = bh->b_dev;
				bh->b_rsector = bh->b_blocknr * (bh->b_size>>9);
				generic_make_request(action[i]-1, bh);
			} else {
				PRINTK("skip op %d on disc %d for sector %ld\n", action[i]-1, i, sh->sector);
				clear_bit(BH_Lock, &bh->b_state);
				set_bit(STRIPE_HANDLE, &sh->state);
			}
		}
}

static inline void raid5_activate_delayed(raid5_conf_t *conf)
{
	if (atomic_read(&conf->preread_active_stripes) < IO_THRESHOLD) {
		while (!list_empty(&conf->delayed_list)) {
			struct list_head *l = conf->delayed_list.next;
			struct stripe_head *sh;
			sh = list_entry(l, struct stripe_head, lru);
			list_del_init(l);
			clear_bit(STRIPE_DELAYED, &sh->state);
			if (!test_and_set_bit(STRIPE_PREREAD_ACTIVE, &sh->state))
				atomic_inc(&conf->preread_active_stripes);
			list_add_tail(&sh->lru, &conf->handle_list);
		}
	}
}
static void raid5_unplug_device(void *data)
{
	raid5_conf_t *conf = (raid5_conf_t *)data;
	unsigned long flags;

	spin_lock_irqsave(&conf->device_lock, flags);

	raid5_activate_delayed(conf);
	
	conf->plugged = 0;
	md_wakeup_thread(conf->thread);

	spin_unlock_irqrestore(&conf->device_lock, flags);
}

static inline void raid5_plug_device(raid5_conf_t *conf)
{
	spin_lock_irq(&conf->device_lock);
	if (list_empty(&conf->delayed_list))
		if (!conf->plugged) {
			conf->plugged = 1;
			queue_task(&conf->plug_tq, &tq_disk);
		}
	spin_unlock_irq(&conf->device_lock);
}

static int raid5_make_request (mddev_t *mddev, int rw, struct buffer_head * bh)
{
	raid5_conf_t *conf = (raid5_conf_t *) mddev->private;
	const unsigned int raid_disks = conf->raid_disks;
	const unsigned int data_disks = raid_disks - 1;
	unsigned int dd_idx, pd_idx;
	unsigned long new_sector;
	int read_ahead = 0;

	struct stripe_head *sh;

	if (rw == READA) {
		rw = READ;
		read_ahead=1;
	}

	new_sector = raid5_compute_sector(bh->b_rsector,
			raid_disks, data_disks, &dd_idx, &pd_idx, conf);

	PRINTK("raid5_make_request, sector %lu\n", new_sector);
	sh = get_active_stripe(conf, new_sector, bh->b_size, read_ahead);
	if (sh) {
		sh->pd_idx = pd_idx;

		add_stripe_bh(sh, bh, dd_idx, rw);

		raid5_plug_device(conf);
		handle_stripe(sh);
		release_stripe(sh);
	} else
		bh->b_end_io(bh, test_bit(BH_Uptodate, &bh->b_state));
	return 0;
}

/*
 * Determine correct block size for this device.
 */
unsigned int device_bsize (kdev_t dev)
{
	unsigned int i, correct_size;

	correct_size = BLOCK_SIZE;
	if (blksize_size[MAJOR(dev)]) {
		i = blksize_size[MAJOR(dev)][MINOR(dev)];
		if (i)
			correct_size = i;
	}

	return correct_size;
}

static int raid5_sync_request (mddev_t *mddev, unsigned long sector_nr)
{
	raid5_conf_t *conf = (raid5_conf_t *) mddev->private;
	struct stripe_head *sh;
	int sectors_per_chunk = conf->chunk_size >> 9;
	unsigned long stripe = sector_nr/sectors_per_chunk;
	int chunk_offset = sector_nr % sectors_per_chunk;
	int dd_idx, pd_idx;
	unsigned long first_sector;
	int raid_disks = conf->raid_disks;
	int data_disks = raid_disks-1;
	int redone = 0;
	int bufsize;

	sh = get_active_stripe(conf, sector_nr, 0, 0);
	bufsize = sh->size;
	redone = sector_nr - sh->sector;
	first_sector = raid5_compute_sector(stripe*data_disks*sectors_per_chunk
		+ chunk_offset, raid_disks, data_disks, &dd_idx, &pd_idx, conf);
	sh->pd_idx = pd_idx;
	spin_lock(&sh->lock);	
	set_bit(STRIPE_SYNCING, &sh->state);
	clear_bit(STRIPE_INSYNC, &sh->state);
	sh->sync_redone = redone;
	spin_unlock(&sh->lock);

	handle_stripe(sh);
	release_stripe(sh);

	return (bufsize>>9)-redone;
}

/*
 * This is our raid5 kernel thread.
 *
 * We scan the hash table for stripes which can be handled now.
 * During the scan, completed stripes are saved for us by the interrupt
 * handler, so that they will not have to wait for our next wakeup.
 */
static void raid5d (void *data)
{
	struct stripe_head *sh;
	raid5_conf_t *conf = data;
	mddev_t *mddev = conf->mddev;
	int handled;

	PRINTK("+++ raid5d active\n");

	handled = 0;

	if (mddev->sb_dirty)
		md_update_sb(mddev);
	md_spin_lock_irq(&conf->device_lock);
	while (1) {
		struct list_head *first;

		if (list_empty(&conf->handle_list) &&
		    atomic_read(&conf->preread_active_stripes) < IO_THRESHOLD &&
		    !conf->plugged &&
		    !list_empty(&conf->delayed_list))
			raid5_activate_delayed(conf);

		if (list_empty(&conf->handle_list))
			break;

		first = conf->handle_list.next;
		sh = list_entry(first, struct stripe_head, lru);

		list_del_init(first);
		atomic_inc(&sh->count);
		if (atomic_read(&sh->count)!= 1)
			BUG();
		md_spin_unlock_irq(&conf->device_lock);
		
		handled++;
		handle_stripe(sh);
		release_stripe(sh);

		md_spin_lock_irq(&conf->device_lock);
	}
	PRINTK("%d stripes handled\n", handled);

	md_spin_unlock_irq(&conf->device_lock);

	PRINTK("--- raid5d inactive\n");
}

/*
 * Private kernel thread for parity reconstruction after an unclean
 * shutdown. Reconstruction on spare drives in case of a failed drive
 * is done by the generic mdsyncd.
 */
static void raid5syncd (void *data)
{
	raid5_conf_t *conf = data;
	mddev_t *mddev = conf->mddev;

	if (!conf->resync_parity)
		return;
	if (conf->resync_parity == 2)
		return;
	down(&mddev->recovery_sem);
	if (md_do_sync(mddev,NULL)) {
		up(&mddev->recovery_sem);
		printk("raid5: resync aborted!\n");
		return;
	}
	conf->resync_parity = 0;
	up(&mddev->recovery_sem);
	printk("raid5: resync finished.\n");
}

static int raid5_run (mddev_t *mddev)
{
	raid5_conf_t *conf;
	int i, j, raid_disk, memory;
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *desc;
	mdk_rdev_t *rdev;
	struct disk_info *disk;
	struct md_list_head *tmp;
	int start_recovery = 0;

	MOD_INC_USE_COUNT;

	if (sb->level != 5 && sb->level != 4) {
		printk("raid5: md%d: raid level not set to 4/5 (%d)\n", mdidx(mddev), sb->level);
		MOD_DEC_USE_COUNT;
		return -EIO;
	}

	mddev->private = kmalloc (sizeof (raid5_conf_t), GFP_KERNEL);
	if ((conf = mddev->private) == NULL)
		goto abort;
	memset (conf, 0, sizeof (*conf));
	conf->mddev = mddev;

	if ((conf->stripe_hashtbl = (struct stripe_head **) md__get_free_pages(GFP_ATOMIC, HASH_PAGES_ORDER)) == NULL)
		goto abort;
	memset(conf->stripe_hashtbl, 0, HASH_PAGES * PAGE_SIZE);

	conf->device_lock = MD_SPIN_LOCK_UNLOCKED;
	md_init_waitqueue_head(&conf->wait_for_stripe);
	INIT_LIST_HEAD(&conf->handle_list);
	INIT_LIST_HEAD(&conf->delayed_list);
	INIT_LIST_HEAD(&conf->inactive_list);
	atomic_set(&conf->active_stripes, 0);
	atomic_set(&conf->preread_active_stripes, 0);
	conf->buffer_size = PAGE_SIZE; /* good default for rebuild */

	conf->plugged = 0;
	conf->plug_tq.sync = 0;
	conf->plug_tq.routine = &raid5_unplug_device;
	conf->plug_tq.data = conf;

	PRINTK("raid5_run(md%d) called.\n", mdidx(mddev));

	ITERATE_RDEV(mddev,rdev,tmp) {
		/*
		 * This is important -- we are using the descriptor on
		 * the disk only to get a pointer to the descriptor on
		 * the main superblock, which might be more recent.
		 */
		desc = sb->disks + rdev->desc_nr;
		raid_disk = desc->raid_disk;
		disk = conf->disks + raid_disk;

		if (disk_faulty(desc)) {
			printk(KERN_ERR "raid5: disabled device %s (errors detected)\n", partition_name(rdev->dev));
			if (!rdev->faulty) {
				MD_BUG();
				goto abort;
			}
			disk->number = desc->number;
			disk->raid_disk = raid_disk;
			disk->dev = rdev->dev;

			disk->operational = 0;
			disk->write_only = 0;
			disk->spare = 0;
			disk->used_slot = 1;
			continue;
		}
		if (disk_active(desc)) {
			if (!disk_sync(desc)) {
				printk(KERN_ERR "raid5: disabled device %s (not in sync)\n", partition_name(rdev->dev));
				MD_BUG();
				goto abort;
			}
			if (raid_disk > sb->raid_disks) {
				printk(KERN_ERR "raid5: disabled device %s (inconsistent descriptor)\n", partition_name(rdev->dev));
				continue;
			}
			if (disk->operational) {
				printk(KERN_ERR "raid5: disabled device %s (device %d already operational)\n", partition_name(rdev->dev), raid_disk);
				continue;
			}
			printk(KERN_INFO "raid5: device %s operational as raid disk %d\n", partition_name(rdev->dev), raid_disk);
	
			disk->number = desc->number;
			disk->raid_disk = raid_disk;
			disk->dev = rdev->dev;
			disk->operational = 1;
			disk->used_slot = 1;

			conf->working_disks++;
		} else {
			/*
			 * Must be a spare disk ..
			 */
			printk(KERN_INFO "raid5: spare disk %s\n", partition_name(rdev->dev));
			disk->number = desc->number;
			disk->raid_disk = raid_disk;
			disk->dev = rdev->dev;

			disk->operational = 0;
			disk->write_only = 0;
			disk->spare = 1;
			disk->used_slot = 1;
		}
	}

	for (i = 0; i < MD_SB_DISKS; i++) {
		desc = sb->disks + i;
		raid_disk = desc->raid_disk;
		disk = conf->disks + raid_disk;

		if (disk_faulty(desc) && (raid_disk < sb->raid_disks) &&
			!conf->disks[raid_disk].used_slot) {

			disk->number = desc->number;
			disk->raid_disk = raid_disk;
			disk->dev = MKDEV(0,0);

			disk->operational = 0;
			disk->write_only = 0;
			disk->spare = 0;
			disk->used_slot = 1;
		}
	}

	conf->raid_disks = sb->raid_disks;
	/*
	 * 0 for a fully functional array, 1 for a degraded array.
	 */
	conf->failed_disks = conf->raid_disks - conf->working_disks;
	conf->mddev = mddev;
	conf->chunk_size = sb->chunk_size;
	conf->level = sb->level;
	conf->algorithm = sb->layout;
	conf->max_nr_stripes = NR_STRIPES;

#if 0
	for (i = 0; i < conf->raid_disks; i++) {
		if (!conf->disks[i].used_slot) {
			MD_BUG();
			goto abort;
		}
	}
#endif
	if (!conf->chunk_size || conf->chunk_size % 4) {
		printk(KERN_ERR "raid5: invalid chunk size %d for md%d\n", conf->chunk_size, mdidx(mddev));
		goto abort;
	}
	if (conf->algorithm > ALGORITHM_RIGHT_SYMMETRIC) {
		printk(KERN_ERR "raid5: unsupported parity algorithm %d for md%d\n", conf->algorithm, mdidx(mddev));
		goto abort;
	}
	if (conf->failed_disks > 1) {
		printk(KERN_ERR "raid5: not enough operational devices for md%d (%d/%d failed)\n", mdidx(mddev), conf->failed_disks, conf->raid_disks);
		goto abort;
	}

	if (conf->working_disks != sb->raid_disks) {
		printk(KERN_ALERT "raid5: md%d, not all disks are operational -- trying to recover array\n", mdidx(mddev));
		start_recovery = 1;
	}

	{
		const char * name = "raid5d";

		conf->thread = md_register_thread(raid5d, conf, name);
		if (!conf->thread) {
			printk(KERN_ERR "raid5: couldn't allocate thread for md%d\n", mdidx(mddev));
			goto abort;
		}
	}

	memory = conf->max_nr_stripes * (sizeof(struct stripe_head) +
		 conf->raid_disks * ((sizeof(struct buffer_head) + PAGE_SIZE))) / 1024;
	if (grow_stripes(conf, conf->max_nr_stripes, GFP_KERNEL)) {
		printk(KERN_ERR "raid5: couldn't allocate %dkB for buffers\n", memory);
		shrink_stripes(conf, conf->max_nr_stripes);
		goto abort;
	} else
		printk(KERN_INFO "raid5: allocated %dkB for md%d\n", memory, mdidx(mddev));

	/*
	 * Regenerate the "device is in sync with the raid set" bit for
	 * each device.
	 */
	for (i = 0; i < MD_SB_DISKS ; i++) {
		mark_disk_nonsync(sb->disks + i);
		for (j = 0; j < sb->raid_disks; j++) {
			if (!conf->disks[j].operational)
				continue;
			if (sb->disks[i].number == conf->disks[j].number)
				mark_disk_sync(sb->disks + i);
		}
	}
	sb->active_disks = conf->working_disks;

	if (sb->active_disks == sb->raid_disks)
		printk("raid5: raid level %d set md%d active with %d out of %d devices, algorithm %d\n", conf->level, mdidx(mddev), sb->active_disks, sb->raid_disks, conf->algorithm);
	else
		printk(KERN_ALERT "raid5: raid level %d set md%d active with %d out of %d devices, algorithm %d\n", conf->level, mdidx(mddev), sb->active_disks, sb->raid_disks, conf->algorithm);

	if (!start_recovery && !(sb->state & (1 << MD_SB_CLEAN))) {
		const char * name = "raid5syncd";

		conf->resync_thread = md_register_thread(raid5syncd, conf,name);
		if (!conf->resync_thread) {
			printk(KERN_ERR "raid5: couldn't allocate thread for md%d\n", mdidx(mddev));
			goto abort;
		}

		printk("raid5: raid set md%d not clean; reconstructing parity\n", mdidx(mddev));
		conf->resync_parity = 1;
		md_wakeup_thread(conf->resync_thread);
	}

	print_raid5_conf(conf);
	if (start_recovery)
		md_recover_arrays();
	print_raid5_conf(conf);

	/* Ok, everything is just fine now */
	return (0);
abort:
	if (conf) {
		print_raid5_conf(conf);
		if (conf->stripe_hashtbl)
			free_pages((unsigned long) conf->stripe_hashtbl,
							HASH_PAGES_ORDER);
		kfree(conf);
	}
	mddev->private = NULL;
	printk(KERN_ALERT "raid5: failed to run raid set md%d\n", mdidx(mddev));
	MOD_DEC_USE_COUNT;
	return -EIO;
}

static int raid5_stop_resync (mddev_t *mddev)
{
	raid5_conf_t *conf = mddev_to_conf(mddev);
	mdk_thread_t *thread = conf->resync_thread;

	if (thread) {
		if (conf->resync_parity) {
			conf->resync_parity = 2;
			md_interrupt_thread(thread);
			printk(KERN_INFO "raid5: parity resync was not fully finished, restarting next time.\n");
			return 1;
		}
		return 0;
	}
	return 0;
}

static int raid5_restart_resync (mddev_t *mddev)
{
	raid5_conf_t *conf = mddev_to_conf(mddev);

	if (conf->resync_parity) {
		if (!conf->resync_thread) {
			MD_BUG();
			return 0;
		}
		printk("raid5: waking up raid5resync.\n");
		conf->resync_parity = 1;
		md_wakeup_thread(conf->resync_thread);
		return 1;
	} else
		printk("raid5: no restart-resync needed.\n");
	return 0;
}


static int raid5_stop (mddev_t *mddev)
{
	raid5_conf_t *conf = (raid5_conf_t *) mddev->private;

	if (conf->resync_thread)
		md_unregister_thread(conf->resync_thread);
	md_unregister_thread(conf->thread);
	shrink_stripes(conf, conf->max_nr_stripes);
	free_pages((unsigned long) conf->stripe_hashtbl, HASH_PAGES_ORDER);
	kfree(conf);
	mddev->private = NULL;
	MOD_DEC_USE_COUNT;
	return 0;
}

#if RAID5_DEBUG
static void print_sh (struct stripe_head *sh)
{
	int i;

	printk("sh %lu, size %d, pd_idx %d, state %ld.\n", sh->sector, sh->size, sh->pd_idx, sh->state);
	printk("sh %lu,  count %d.\n", sh->sector, atomic_read(&sh->count));
	printk("sh %lu, ", sh->sector);
	for (i = 0; i < MD_SB_DISKS; i++) {
		if (sh->bh_cache[i])
			printk("(cache%d: %p %ld) ", i, sh->bh_cache[i], sh->bh_cache[i]->b_state);
	}
	printk("\n");
}

static void printall (raid5_conf_t *conf)
{
	struct stripe_head *sh;
	int i;

	md_spin_lock_irq(&conf->device_lock);
	for (i = 0; i < NR_HASH; i++) {
		sh = conf->stripe_hashtbl[i];
		for (; sh; sh = sh->hash_next) {
			if (sh->raid_conf != conf)
				continue;
			print_sh(sh);
		}
	}
	md_spin_unlock_irq(&conf->device_lock);

	PRINTK("--- raid5d inactive\n");
}
#endif

static void raid5_status (struct seq_file *seq, mddev_t *mddev)
{
	raid5_conf_t *conf = (raid5_conf_t *) mddev->private;
	mdp_super_t *sb = mddev->sb;
	int i;

	seq_printf (seq, " level %d, %dk chunk, algorithm %d", sb->level, sb->chunk_size >> 10, sb->layout);
	seq_printf (seq, " [%d/%d] [", conf->raid_disks, conf->working_disks);
	for (i = 0; i < conf->raid_disks; i++)
		seq_printf (seq, "%s", conf->disks[i].operational ? "U" : "_");
	seq_printf (seq, "]");
#if RAID5_DEBUG
#define D(x) \
	seq_printf (seq, "<"#x":%d>", atomic_read(&conf->x))
	printall(conf);
#endif

}

static void print_raid5_conf (raid5_conf_t *conf)
{
	int i;
	struct disk_info *tmp;

	printk("RAID5 conf printout:\n");
	if (!conf) {
		printk("(conf==NULL)\n");
		return;
	}
	printk(" --- rd:%d wd:%d fd:%d\n", conf->raid_disks,
		 conf->working_disks, conf->failed_disks);

#if RAID5_DEBUG
	for (i = 0; i < MD_SB_DISKS; i++) {
#else
	for (i = 0; i < conf->working_disks+conf->failed_disks; i++) {
#endif
		tmp = conf->disks + i;
		printk(" disk %d, s:%d, o:%d, n:%d rd:%d us:%d dev:%s\n",
			i, tmp->spare,tmp->operational,
			tmp->number,tmp->raid_disk,tmp->used_slot,
			partition_name(tmp->dev));
	}
}

static int raid5_diskop(mddev_t *mddev, mdp_disk_t **d, int state)
{
	int err = 0;
	int i, failed_disk=-1, spare_disk=-1, removed_disk=-1, added_disk=-1;
	raid5_conf_t *conf = mddev->private;
	struct disk_info *tmp, *sdisk, *fdisk, *rdisk, *adisk;
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *failed_desc, *spare_desc, *added_desc;
	mdk_rdev_t *spare_rdev, *failed_rdev;

	print_raid5_conf(conf);
	md_spin_lock_irq(&conf->device_lock);
	/*
	 * find the disk ...
	 */
	switch (state) {

	case DISKOP_SPARE_ACTIVE:

		/*
		 * Find the failed disk within the RAID5 configuration ...
		 * (this can only be in the first conf->raid_disks part)
		 */
		for (i = 0; i < conf->raid_disks; i++) {
			tmp = conf->disks + i;
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
			tmp = conf->disks + i;
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
			tmp = conf->disks + i;
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
			tmp = conf->disks + i;
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
		if (conf->spare) {
			MD_BUG();
			err = 1;
			goto abort;
		}
		sdisk = conf->disks + spare_disk;
		sdisk->operational = 1;
		sdisk->write_only = 1;
		conf->spare = sdisk;
		break;
	/*
	 * Deactivate a spare disk:
	 */
	case DISKOP_SPARE_INACTIVE:
		sdisk = conf->disks + spare_disk;
		sdisk->operational = 0;
		sdisk->write_only = 0;
		/*
		 * Was the spare being resynced?
		 */
		if (conf->spare == sdisk)
			conf->spare = NULL;
		break;
	/*
	 * Activate (mark read-write) the (now sync) spare disk,
	 * which means we switch it's 'raid position' (->raid_disk)
	 * with the failed disk. (only the first 'conf->raid_disks'
	 * slots are used for 'real' disks and we must preserve this
	 * property)
	 */
	case DISKOP_SPARE_ACTIVE:
		if (!conf->spare) {
			MD_BUG();
			err = 1;
			goto abort;
		}
		sdisk = conf->disks + spare_disk;
		fdisk = conf->disks + failed_disk;

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
		conf->failed_disks--;
		conf->working_disks++;
		conf->spare = NULL;

		break;

	case DISKOP_HOT_REMOVE_DISK:
		rdisk = conf->disks + removed_disk;

		if (rdisk->spare && (removed_disk < conf->raid_disks)) {
			MD_BUG();	
			err = 1;
			goto abort;
		}
		rdisk->dev = MKDEV(0,0);
		rdisk->used_slot = 0;

		break;

	case DISKOP_HOT_ADD_DISK:
		adisk = conf->disks + added_disk;
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


		break;

	default:
		MD_BUG();	
		err = 1;
		goto abort;
	}
abort:
	md_spin_unlock_irq(&conf->device_lock);
	print_raid5_conf(conf);
	return err;
}

static mdk_personality_t raid5_personality=
{
	name:		"raid5",
	make_request:	raid5_make_request,
	run:		raid5_run,
	stop:		raid5_stop,
	status:		raid5_status,
	error_handler:	raid5_error,
	diskop:		raid5_diskop,
	stop_resync:	raid5_stop_resync,
	restart_resync:	raid5_restart_resync,
	sync_request:	raid5_sync_request
};

static int md__init raid5_init (void)
{
	return register_md_personality (RAID5, &raid5_personality);
}

static void raid5_exit (void)
{
	unregister_md_personality (RAID5);
}

module_init(raid5_init);
module_exit(raid5_exit);
MODULE_LICENSE("GPL");
