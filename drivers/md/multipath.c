/*
 * multipath.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 1999, 2000, 2001 Ingo Molnar, Red Hat
 *
 * Copyright (C) 1996, 1997, 1998 Ingo Molnar, Miguel de Icaza, Gadi Oxman
 *
 * MULTIPATH management functions.
 *
 * derived from raid1.c.
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
#include <linux/slab.h>
#include <linux/raid/multipath.h>
#include <asm/atomic.h>

#define MAJOR_NR MD_MAJOR
#define MD_DRIVER
#define MD_PERSONALITY

#define MAX_WORK_PER_DISK 128

#define	NR_RESERVED_BUFS	32


/*
 * The following can be used to debug the driver
 */
#define MULTIPATH_DEBUG	0

#if MULTIPATH_DEBUG
#define PRINTK(x...)   printk(x)
#define inline
#define __inline__
#else
#define PRINTK(x...)  do { } while (0)
#endif


static mdk_personality_t multipath_personality;
static md_spinlock_t retry_list_lock = MD_SPIN_LOCK_UNLOCKED;
struct multipath_bh *multipath_retry_list = NULL, **multipath_retry_tail;

static int multipath_diskop(mddev_t *mddev, mdp_disk_t **d, int state);



static struct multipath_bh *multipath_alloc_mpbh(multipath_conf_t *conf)
{
	struct multipath_bh *mp_bh = NULL;

	do {
		md_spin_lock_irq(&conf->device_lock);
		if (!conf->freer1_blocked && conf->freer1) {
			mp_bh = conf->freer1;
			conf->freer1 = mp_bh->next_mp;
			conf->freer1_cnt--;
			mp_bh->next_mp = NULL;
			mp_bh->state = (1 << MPBH_PreAlloc);
			mp_bh->bh_req.b_state = 0;
		}
		md_spin_unlock_irq(&conf->device_lock);
		if (mp_bh)
			return mp_bh;
		mp_bh = (struct multipath_bh *) kmalloc(sizeof(struct multipath_bh),
					GFP_NOIO);
		if (mp_bh) {
			memset(mp_bh, 0, sizeof(*mp_bh));
			return mp_bh;
		}
		conf->freer1_blocked = 1;
		wait_disk_event(conf->wait_buffer,
				!conf->freer1_blocked ||
				conf->freer1_cnt > NR_RESERVED_BUFS/2
		    );
		conf->freer1_blocked = 0;
	} while (1);
}

static inline void multipath_free_mpbh(struct multipath_bh *mp_bh)
{
	multipath_conf_t *conf = mddev_to_conf(mp_bh->mddev);

	if (test_bit(MPBH_PreAlloc, &mp_bh->state)) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		mp_bh->next_mp = conf->freer1;
		conf->freer1 = mp_bh;
		conf->freer1_cnt++;
		spin_unlock_irqrestore(&conf->device_lock, flags);
		wake_up(&conf->wait_buffer);
	} else {
		kfree(mp_bh);
	}
}

static int multipath_grow_mpbh (multipath_conf_t *conf, int cnt)
{
	int i = 0;

	while (i < cnt) {
		struct multipath_bh *mp_bh;
		mp_bh = (struct multipath_bh*)kmalloc(sizeof(*mp_bh), GFP_KERNEL);
		if (!mp_bh)
			break;
		memset(mp_bh, 0, sizeof(*mp_bh));
		set_bit(MPBH_PreAlloc, &mp_bh->state);
		mp_bh->mddev = conf->mddev;	       

		multipath_free_mpbh(mp_bh);
		i++;
	}
	return i;
}

static void multipath_shrink_mpbh(multipath_conf_t *conf)
{
	md_spin_lock_irq(&conf->device_lock);
	while (conf->freer1) {
		struct multipath_bh *mp_bh = conf->freer1;
		conf->freer1 = mp_bh->next_mp;
		conf->freer1_cnt--;
		kfree(mp_bh);
	}
	md_spin_unlock_irq(&conf->device_lock);
}


static int multipath_map (mddev_t *mddev, kdev_t *rdev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	int i, disks = MD_SB_DISKS;

	/*
	 * Later we do read balancing on the read side 
	 * now we use the first available disk.
	 */

	for (i = 0; i < disks; i++) {
		if (conf->multipaths[i].operational) {
			*rdev = conf->multipaths[i].dev;
			return (0);
		}
	}

	printk (KERN_ERR "multipath_map(): no more operational IO paths?\n");
	return (-1);
}

static void multipath_reschedule_retry (struct multipath_bh *mp_bh)
{
	unsigned long flags;
	mddev_t *mddev = mp_bh->mddev;
	multipath_conf_t *conf = mddev_to_conf(mddev);

	md_spin_lock_irqsave(&retry_list_lock, flags);
	if (multipath_retry_list == NULL)
		multipath_retry_tail = &multipath_retry_list;
	*multipath_retry_tail = mp_bh;
	multipath_retry_tail = &mp_bh->next_mp;
	mp_bh->next_mp = NULL;
	md_spin_unlock_irqrestore(&retry_list_lock, flags);
	md_wakeup_thread(conf->thread);
}


/*
 * multipath_end_bh_io() is called when we have finished servicing a multipathed
 * operation and are ready to return a success/failure code to the buffer
 * cache layer.
 */
static void multipath_end_bh_io (struct multipath_bh *mp_bh, int uptodate)
{
	struct buffer_head *bh = mp_bh->master_bh;

	bh->b_end_io(bh, uptodate);
	multipath_free_mpbh(mp_bh);
}

void multipath_end_request (struct buffer_head *bh, int uptodate)
{
	struct multipath_bh * mp_bh = (struct multipath_bh *)(bh->b_private);

	/*
	 * this branch is our 'one multipath IO has finished' event handler:
	 */
	if (!uptodate)
		md_error (mp_bh->mddev, bh->b_dev);
	else
		/*
		 * Set MPBH_Uptodate in our master buffer_head, so that
		 * we will return a good error code for to the higher
		 * levels even if IO on some other multipathed buffer fails.
		 *
		 * The 'master' represents the complex operation to 
		 * user-side. So if something waits for IO, then it will
		 * wait for the 'master' buffer_head.
		 */
		set_bit (MPBH_Uptodate, &mp_bh->state);

		
	if (uptodate) {
		multipath_end_bh_io(mp_bh, uptodate);
		return;
	}
	/*
	 * oops, IO error:
	 */
	printk(KERN_ERR "multipath: %s: rescheduling block %lu\n", 
		 partition_name(bh->b_dev), bh->b_blocknr);
	multipath_reschedule_retry(mp_bh);
	return;
}

/*
 * This routine returns the disk from which the requested read should
 * be done.
 */

static int multipath_read_balance (multipath_conf_t *conf)
{
	int disk;

	for (disk = 0; disk < conf->raid_disks; disk++)	
		if (conf->multipaths[disk].operational)
			return disk;
	BUG();
	return 0;
}

static int multipath_make_request (mddev_t *mddev, int rw,
			       struct buffer_head * bh)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	struct buffer_head *bh_req;
	struct multipath_bh * mp_bh;
	struct multipath_info *multipath;

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

	mp_bh = multipath_alloc_mpbh (conf);

	mp_bh->master_bh = bh;
	mp_bh->mddev = mddev;
	mp_bh->cmd = rw;

	/*
	 * read balancing logic:
	 */
	multipath = conf->multipaths + multipath_read_balance(conf);

	bh_req = &mp_bh->bh_req;
	memcpy(bh_req, bh, sizeof(*bh));
	bh_req->b_blocknr = bh->b_rsector;
	bh_req->b_dev = multipath->dev;
	bh_req->b_rdev = multipath->dev;
/*	bh_req->b_rsector = bh->n_rsector; */
	bh_req->b_end_io = multipath_end_request;
	bh_req->b_private = mp_bh;
	generic_make_request (rw, bh_req);
	return 0;
}

static void multipath_status (struct seq_file *seq, mddev_t *mddev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	int i;
	
	seq_printf (seq, " [%d/%d] [", conf->raid_disks,
						 conf->working_disks);
	for (i = 0; i < conf->raid_disks; i++)
		seq_printf (seq, "%s",
			conf->multipaths[i].operational ? "U" : "_");
	seq_printf (seq, "]");
}

#define LAST_DISK KERN_ALERT \
"multipath: only one IO path left and IO error.\n"

#define NO_SPARE_DISK KERN_ALERT \
"multipath: no spare IO path left!\n"

#define DISK_FAILED KERN_ALERT \
"multipath: IO failure on %s, disabling IO path. \n" \
"	Operation continuing on %d IO paths.\n"

static void mark_disk_bad (mddev_t *mddev, int failed)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	struct multipath_info *multipath = conf->multipaths+failed;
	mdp_super_t *sb = mddev->sb;

	multipath->operational = 0;
	mark_disk_faulty(sb->disks+multipath->number);
	mark_disk_nonsync(sb->disks+multipath->number);
	mark_disk_inactive(sb->disks+multipath->number);
	sb->active_disks--;
	sb->working_disks--;
	sb->failed_disks++;
	mddev->sb_dirty = 1;
	md_wakeup_thread(conf->thread);
	conf->working_disks--;
	printk (DISK_FAILED, partition_name (multipath->dev),
				 conf->working_disks);
}

/*
 * Careful, this can execute in IRQ contexts as well!
 */
static int multipath_error (mddev_t *mddev, kdev_t dev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	struct multipath_info * multipaths = conf->multipaths;
	int disks = MD_SB_DISKS;
	int other_paths = 1;
	int i;

	if (conf->working_disks == 1) {
		other_paths = 0;
		for (i = 0; i < disks; i++) {
			if (multipaths[i].spare) {
				other_paths = 1;
				break;
			}
		}
	}

	if (!other_paths) {
		/*
		 * Uh oh, we can do nothing if this is our last path, but
		 * first check if this is a queued request for a device
		 * which has just failed.
		 */
		for (i = 0; i < disks; i++) {
			if (multipaths[i].dev==dev && !multipaths[i].operational)
				return 0;
		}
		printk (LAST_DISK);
	} else {
		/*
		 * Mark disk as unusable
		 */
		for (i = 0; i < disks; i++) {
			if (multipaths[i].dev==dev && multipaths[i].operational) {
				mark_disk_bad(mddev, i);
				break;
			}
		}
		if (!conf->working_disks) {
			int err = 1;
			mdp_disk_t *spare;
			mdp_super_t *sb = mddev->sb;

			spare = get_spare(mddev);
			if (spare) {
				err = multipath_diskop(mddev, &spare, DISKOP_SPARE_WRITE);
				printk("got DISKOP_SPARE_WRITE err: %d. (spare_faulty(): %d)\n", err, disk_faulty(spare));
			}
			if (!err && !disk_faulty(spare)) {
				multipath_diskop(mddev, &spare, DISKOP_SPARE_ACTIVE);
				mark_disk_sync(spare);
				mark_disk_active(spare);
				sb->active_disks++;
				sb->spare_disks--;
			}
		}
	}
	return 0;
}

#undef LAST_DISK
#undef NO_SPARE_DISK
#undef DISK_FAILED


static void print_multipath_conf (multipath_conf_t *conf)
{
	int i;
	struct multipath_info *tmp;

	printk("MULTIPATH conf printout:\n");
	if (!conf) {
		printk("(conf==NULL)\n");
		return;
	}
	printk(" --- wd:%d rd:%d nd:%d\n", conf->working_disks,
			 conf->raid_disks, conf->nr_disks);

	for (i = 0; i < MD_SB_DISKS; i++) {
		tmp = conf->multipaths + i;
		if (tmp->spare || tmp->operational || tmp->number ||
				tmp->raid_disk || tmp->used_slot)
			printk(" disk%d, s:%d, o:%d, n:%d rd:%d us:%d dev:%s\n",
				i, tmp->spare,tmp->operational,
				tmp->number,tmp->raid_disk,tmp->used_slot,
				partition_name(tmp->dev));
	}
}

static int multipath_diskop(mddev_t *mddev, mdp_disk_t **d, int state)
{
	int err = 0;
	int i, failed_disk=-1, spare_disk=-1, removed_disk=-1, added_disk=-1;
	multipath_conf_t *conf = mddev->private;
	struct multipath_info *tmp, *sdisk, *fdisk, *rdisk, *adisk;
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *failed_desc, *spare_desc, *added_desc;
	mdk_rdev_t *spare_rdev, *failed_rdev;

	print_multipath_conf(conf);
	md_spin_lock_irq(&conf->device_lock);
	/*
	 * find the disk ...
	 */
	switch (state) {

	case DISKOP_SPARE_ACTIVE:

		/*
		 * Find the failed disk within the MULTIPATH configuration ...
		 * (this can only be in the first conf->working_disks part)
		 */
		for (i = 0; i < conf->raid_disks; i++) {
			tmp = conf->multipaths + i;
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
			tmp = conf->multipaths + i;
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
			tmp = conf->multipaths + i;
			if (tmp->used_slot && (tmp->number == (*d)->number)) {
				if (tmp->operational) {
					printk(KERN_ERR "hot-remove-disk, slot %d is identified to be the requested disk (number %d), but is still operational!\n", i, (*d)->number);
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
			tmp = conf->multipaths + i;
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
		sdisk = conf->multipaths + spare_disk;
		sdisk->operational = 1;
		break;
	/*
	 * Deactivate a spare disk:
	 */
	case DISKOP_SPARE_INACTIVE:
		sdisk = conf->multipaths + spare_disk;
		sdisk->operational = 0;
		break;
	/*
	 * Activate (mark read-write) the (now sync) spare disk,
	 * which means we switch it's 'raid position' (->raid_disk)
	 * with the failed disk. (only the first 'conf->nr_disks'
	 * slots are used for 'real' disks and we must preserve this
	 * property)
	 */
	case DISKOP_SPARE_ACTIVE:
		sdisk = conf->multipaths + spare_disk;
		fdisk = conf->multipaths + failed_disk;

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
		xchg_values(spare_rdev->desc_nr, failed_rdev->desc_nr);
		spare_rdev->alias_device = 0;
		failed_rdev->alias_device = 1;

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

		/*
		 * if we activate a spare, we definitely replace a
		 * non-operational disk slot in the 'low' area of
		 * the disk array.
		 */

		conf->working_disks++;

		break;

	case DISKOP_HOT_REMOVE_DISK:
		rdisk = conf->multipaths + removed_disk;

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
		adisk = conf->multipaths + added_disk;
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
		adisk->spare = 1;
		adisk->used_slot = 1;
		conf->nr_disks++;

		break;

	default:
		MD_BUG();	
		err = 1;
		goto abort;
	}
abort:
	md_spin_unlock_irq(&conf->device_lock);

	print_multipath_conf(conf);
	return err;
}


#define IO_ERROR KERN_ALERT \
"multipath: %s: unrecoverable IO read error for block %lu\n"

#define REDIRECT_SECTOR KERN_ERR \
"multipath: %s: redirecting sector %lu to another IO path\n"

/*
 * This is a kernel thread which:
 *
 *	1.	Retries failed read operations on working multipaths.
 *	2.	Updates the raid superblock when problems encounter.
 *	3.	Performs writes following reads for array syncronising.
 */

static void multipathd (void *data)
{
	struct multipath_bh *mp_bh;
	struct buffer_head *bh;
	unsigned long flags;
	mddev_t *mddev;
	kdev_t dev;


	for (;;) {
		md_spin_lock_irqsave(&retry_list_lock, flags);
		mp_bh = multipath_retry_list;
		if (!mp_bh)
			break;
		multipath_retry_list = mp_bh->next_mp;
		md_spin_unlock_irqrestore(&retry_list_lock, flags);

		mddev = mp_bh->mddev;
		if (mddev->sb_dirty)
			md_update_sb(mddev);
		bh = &mp_bh->bh_req;
		dev = bh->b_dev;
		
		multipath_map (mddev, &bh->b_dev);
		if (bh->b_dev == dev) {
			printk (IO_ERROR, partition_name(bh->b_dev), bh->b_blocknr);
			multipath_end_bh_io(mp_bh, 0);
		} else {
			printk (REDIRECT_SECTOR,
				partition_name(bh->b_dev), bh->b_blocknr);
			bh->b_rdev = bh->b_dev;
			bh->b_rsector = bh->b_blocknr;
			generic_make_request (mp_bh->cmd, bh);
		}
	}
	md_spin_unlock_irqrestore(&retry_list_lock, flags);
}
#undef IO_ERROR
#undef REDIRECT_SECTOR

/*
 * This will catch the scenario in which one of the multipaths was
 * mounted as a normal device rather than as a part of a raid set.
 *
 * check_consistency is very personality-dependent, eg. RAID5 cannot
 * do this check, it uses another method.
 */
static int __check_consistency (mddev_t *mddev, int row)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);
	int disks = MD_SB_DISKS;
	kdev_t dev;
	struct buffer_head *bh = NULL;
	int i, rc = 0;
	char *buffer = NULL;

	for (i = 0; i < disks; i++) {
		if (!conf->multipaths[i].operational)
			continue;
		printk("(checking disk %d)\n",i);
		dev = conf->multipaths[i].dev;
		set_blocksize(dev, 4096);
		if ((bh = bread(dev, row / 4, 4096)) == NULL)
			break;
		if (!buffer) {
			buffer = (char *) __get_free_page(GFP_KERNEL);
			if (!buffer)
				break;
			memcpy(buffer, bh->b_data, 4096);
		} else if (memcmp(buffer, bh->b_data, 4096)) {
			rc = 1;
			break;
		}
		bforget(bh);
		fsync_dev(dev);
		invalidate_buffers(dev);
		bh = NULL;
	}
	if (buffer)
		free_page((unsigned long) buffer);
	if (bh) {
		dev = bh->b_dev;
		bforget(bh);
		fsync_dev(dev);
		invalidate_buffers(dev);
	}
	return rc;
}

static int check_consistency (mddev_t *mddev)
{
	if (__check_consistency(mddev, 0))
/*
 * we do not do this currently, as it's perfectly possible to
 * have an inconsistent array when it's freshly created. Only
 * newly written data has to be consistent.
 */
		return 0;

	return 0;
}

#define INVALID_LEVEL KERN_WARNING \
"multipath: md%d: raid level not set to multipath IO (%d)\n"

#define NO_SB KERN_ERR \
"multipath: disabled IO path %s (couldn't access raid superblock)\n"

#define ERRORS KERN_ERR \
"multipath: disabled IO path %s (errors detected)\n"

#define NOT_IN_SYNC KERN_ERR \
"multipath: making IO path %s a spare path (not in sync)\n"

#define INCONSISTENT KERN_ERR \
"multipath: disabled IO path %s (inconsistent descriptor)\n"

#define ALREADY_RUNNING KERN_ERR \
"multipath: disabled IO path %s (multipath %d already operational)\n"

#define OPERATIONAL KERN_INFO \
"multipath: device %s operational as IO path %d\n"

#define MEM_ERROR KERN_ERR \
"multipath: couldn't allocate memory for md%d\n"

#define SPARE KERN_INFO \
"multipath: spare IO path %s\n"

#define NONE_OPERATIONAL KERN_ERR \
"multipath: no operational IO paths for md%d\n"

#define SB_DIFFERENCES KERN_ERR \
"multipath: detected IO path differences!\n"

#define ARRAY_IS_ACTIVE KERN_INFO \
"multipath: array md%d active with %d out of %d IO paths (%d spare IO paths)\n"

#define THREAD_ERROR KERN_ERR \
"multipath: couldn't allocate thread for md%d\n"

static int multipath_run (mddev_t *mddev)
{
	multipath_conf_t *conf;
	int i, j, disk_idx;
	struct multipath_info *disk, *disk2;
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *desc, *desc2;
	mdk_rdev_t *rdev, *def_rdev = NULL;
	struct md_list_head *tmp;
	int num_rdevs = 0;

	MOD_INC_USE_COUNT;

	if (sb->level != -4) {
		printk(INVALID_LEVEL, mdidx(mddev), sb->level);
		goto out;
	}
	/*
	 * copy the already verified devices into our private MULTIPATH
	 * bookkeeping area. [whatever we allocate in multipath_run(),
	 * should be freed in multipath_stop()]
	 */

	conf = kmalloc(sizeof(multipath_conf_t), GFP_KERNEL);
	mddev->private = conf;
	if (!conf) {
		printk(MEM_ERROR, mdidx(mddev));
		goto out;
	}
	memset(conf, 0, sizeof(*conf));

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty) {
			/* this is a "should never happen" case and if it */
			/* ever does happen, a continue; won't help */
			printk(ERRORS, partition_name(rdev->dev));
			continue;
		} else {
			/* this is a "should never happen" case and if it */
			/* ever does happen, a continue; won't help */
			if (!rdev->sb) {
				MD_BUG();
				continue;
			}
		}
		if (rdev->desc_nr == -1) {
			MD_BUG();
			continue;
		}

		desc = &sb->disks[rdev->desc_nr];
		disk_idx = desc->raid_disk;
		disk = conf->multipaths + disk_idx;

		if (!disk_sync(desc))
			printk(NOT_IN_SYNC, partition_name(rdev->dev));

		/*
		 * Mark all disks as spare to start with, then pick our
		 * active disk.  If we have a disk that is marked active
		 * in the sb, then use it, else use the first rdev.
		 */
		disk->number = desc->number;
		disk->raid_disk = desc->raid_disk;
		disk->dev = rdev->dev;
		disk->operational = 0;
		disk->spare = 1;
		disk->used_slot = 1;
		mark_disk_sync(desc);

		if (disk_active(desc)) {
			if(!conf->working_disks) {
				printk(OPERATIONAL, partition_name(rdev->dev),
 					desc->raid_disk);
				disk->operational = 1;
				disk->spare = 0;
				conf->working_disks++;
				def_rdev = rdev;
			} else {
				mark_disk_spare(desc);
			}
		} else
			mark_disk_spare(desc);

		if(!num_rdevs++) def_rdev = rdev;
	}
	if(!conf->working_disks && num_rdevs) {
		desc = &sb->disks[def_rdev->desc_nr];
		disk = conf->multipaths + desc->raid_disk;
		printk(OPERATIONAL, partition_name(def_rdev->dev),
			disk->raid_disk);
		disk->operational = 1;
		disk->spare = 0;
		conf->working_disks++;
		mark_disk_active(desc);
	}
	/*
	 * Make sure our active path is in desc spot 0
	 */
	if(def_rdev->desc_nr != 0) {
		rdev = find_rdev_nr(mddev, 0);
		desc = &sb->disks[def_rdev->desc_nr];
		desc2 = sb->disks;
		disk = conf->multipaths + desc->raid_disk;
		disk2 = conf->multipaths + desc2->raid_disk;
		xchg_values(*desc2,*desc);
		xchg_values(*disk2,*disk);
		xchg_values(desc2->number, desc->number);
		xchg_values(disk2->number, disk->number);
		xchg_values(desc2->raid_disk, desc->raid_disk);
		xchg_values(disk2->raid_disk, disk->raid_disk);
		if(rdev) {
			xchg_values(def_rdev->desc_nr,rdev->desc_nr);
		} else {
			def_rdev->desc_nr = 0;
		}
	}
	conf->raid_disks = sb->raid_disks = sb->active_disks = 1;
	conf->nr_disks = sb->nr_disks = sb->working_disks = num_rdevs;
	sb->failed_disks = 0;
	sb->spare_disks = num_rdevs - 1;
	mddev->sb_dirty = 1;
	conf->mddev = mddev;
	conf->device_lock = MD_SPIN_LOCK_UNLOCKED;

	init_waitqueue_head(&conf->wait_buffer);

	if (!conf->working_disks) {
		printk(NONE_OPERATIONAL, mdidx(mddev));
		goto out_free_conf;
	}


	/* pre-allocate some buffer_head structures.
	 * As a minimum, 1 mpbh and raid_disks buffer_heads
	 * would probably get us by in tight memory situations,
	 * but a few more is probably a good idea.
	 * For now, try NR_RESERVED_BUFS mpbh and
	 * NR_RESERVED_BUFS*raid_disks bufferheads
	 * This will allow at least NR_RESERVED_BUFS concurrent
	 * reads or writes even if kmalloc starts failing
	 */
	if (multipath_grow_mpbh(conf, NR_RESERVED_BUFS) < NR_RESERVED_BUFS) {
		printk(MEM_ERROR, mdidx(mddev));
		goto out_free_conf;
	}

	if ((sb->state & (1 << MD_SB_CLEAN))) {
		/*
		 * we do sanity checks even if the device says
		 * it's clean ...
		 */
		if (check_consistency(mddev)) {
			printk(SB_DIFFERENCES);
			sb->state &= ~(1 << MD_SB_CLEAN);
		}
	}

	{
		const char * name = "multipathd";

		conf->thread = md_register_thread(multipathd, conf, name);
		if (!conf->thread) {
			printk(THREAD_ERROR, mdidx(mddev));
			goto out_free_conf;
		}
	}

	/*
	 * Regenerate the "device is in sync with the raid set" bit for
	 * each device.
	 */
	for (i = 0; i < MD_SB_DISKS; i++) {
		mark_disk_nonsync(sb->disks+i);
		for (j = 0; j < sb->raid_disks; j++) {
			if (sb->disks[i].number == conf->multipaths[j].number)
				mark_disk_sync(sb->disks+i);
		}
	}

	printk(ARRAY_IS_ACTIVE, mdidx(mddev), sb->active_disks,
			sb->raid_disks, sb->spare_disks);
	/*
	 * Ok, everything is just fine now
	 */
	return 0;

out_free_conf:
	multipath_shrink_mpbh(conf);
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
#undef SB_DIFFERENCES
#undef ARRAY_IS_ACTIVE

static int multipath_stop (mddev_t *mddev)
{
	multipath_conf_t *conf = mddev_to_conf(mddev);

	md_unregister_thread(conf->thread);
	multipath_shrink_mpbh(conf);
	kfree(conf);
	mddev->private = NULL;
	MOD_DEC_USE_COUNT;
	return 0;
}

static mdk_personality_t multipath_personality=
{
	name:		"multipath",
	make_request:	multipath_make_request,
	run:		multipath_run,
	stop:		multipath_stop,
	status:		multipath_status,
	error_handler:	multipath_error,
	diskop:		multipath_diskop,
};

static int md__init multipath_init (void)
{
	return register_md_personality (MULTIPATH, &multipath_personality);
}

static void multipath_exit (void)
{
	unregister_md_personality (MULTIPATH);
}

module_init(multipath_init);
module_exit(multipath_exit);
MODULE_LICENSE("GPL");
