/*
   md.c : Multiple Devices driver for Linux
	  Copyright (C) 1998, 1999, 2000 Ingo Molnar

     completely rewritten, based on the MD driver code from Marc Zyngier

   Changes:

   - RAID-1/RAID-5 extensions by Miguel de Icaza, Gadi Oxman, Ingo Molnar
   - boot support for linear and striped mode by Harald Hoyer <HarryH@Royal.Net>
   - kerneld support by Boris Tobotras <boris@xtalk.msk.su>
   - kmod support by: Cyrus Durgin
   - RAID0 bugfixes: Mark Anthony Lisher <markal@iname.com>
   - Devfs support by Richard Gooch <rgooch@atnf.csiro.au>

   - lots of fixes and improvements to the RAID1/RAID5 and generic
     RAID code (such as request based resynchronization):

     Neil Brown <neilb@cse.unsw.edu.au>.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/raid/md.h>
#include <linux/sysctl.h>
#include <linux/raid/xor.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/init.h>

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#include <asm/unaligned.h>

#define MAJOR_NR MD_MAJOR
#define MD_DRIVER

#include <linux/blk.h>

#define DEBUG 0
#if DEBUG
# define dprintk(x...) printk(x)
#else
# define dprintk(x...) do { } while(0)
#endif

#ifndef MODULE
static void autostart_arrays (void);
#endif

static mdk_personality_t *pers[MAX_PERSONALITY];

/*
 * Current RAID-1,4,5 parallel reconstruction 'guaranteed speed limit'
 * is 100 KB/sec, so the extra system load does not show up that much.
 * Increase it if you want to have more _guaranteed_ speed. Note that
 * the RAID driver will use the maximum available bandwith if the IO
 * subsystem is idle. There is also an 'absolute maximum' reconstruction
 * speed limit - in case reconstruction slows down your system despite
 * idle IO detection.
 *
 * you can change it via /proc/sys/dev/raid/speed_limit_min and _max.
 */

static int sysctl_speed_limit_min = 100;
static int sysctl_speed_limit_max = 100000;

static struct ctl_table_header *raid_table_header;

static ctl_table raid_table[] = {
	{DEV_RAID_SPEED_LIMIT_MIN, "speed_limit_min",
	 &sysctl_speed_limit_min, sizeof(int), 0644, NULL, &proc_dointvec},
	{DEV_RAID_SPEED_LIMIT_MAX, "speed_limit_max",
	 &sysctl_speed_limit_max, sizeof(int), 0644, NULL, &proc_dointvec},
	{0}
};

static ctl_table raid_dir_table[] = {
	{DEV_RAID, "raid", NULL, 0, 0555, raid_table},
	{0}
};

static ctl_table raid_root_table[] = {
	{CTL_DEV, "dev", NULL, 0, 0555, raid_dir_table},
	{0}
};

/*
 * these have to be allocated separately because external
 * subsystems want to have a pre-defined structure
 */
struct hd_struct md_hd_struct[MAX_MD_DEVS];
static int md_blocksizes[MAX_MD_DEVS];
static int md_hardsect_sizes[MAX_MD_DEVS];
static int md_maxreadahead[MAX_MD_DEVS];
static mdk_thread_t *md_recovery_thread;

int md_size[MAX_MD_DEVS];

static struct block_device_operations md_fops;
static devfs_handle_t devfs_handle;

static struct gendisk md_gendisk=
{
	major: MD_MAJOR,
	major_name: "md",
	minor_shift: 0,
	max_p: 1,
	part: md_hd_struct,
	sizes: md_size,
	nr_real: MAX_MD_DEVS,
	real_devices: NULL,
	next: NULL,
	fops: &md_fops,
};

/*
 * Enables to iterate over all existing md arrays
 */
static MD_LIST_HEAD(all_mddevs);

/*
 * The mapping between kdev and mddev is not necessary a simple
 * one! Eg. HSM uses several sub-devices to implement Logical
 * Volumes. All these sub-devices map to the same mddev.
 */
dev_mapping_t mddev_map[MAX_MD_DEVS];

void add_mddev_mapping(mddev_t * mddev, kdev_t dev, void *data)
{
	unsigned int minor = MINOR(dev);

	if (MAJOR(dev) != MD_MAJOR) {
		MD_BUG();
		return;
	}
	if (mddev_map[minor].mddev) {
		MD_BUG();
		return;
	}
	mddev_map[minor].mddev = mddev;
	mddev_map[minor].data = data;
}

void del_mddev_mapping(mddev_t * mddev, kdev_t dev)
{
	unsigned int minor = MINOR(dev);

	if (MAJOR(dev) != MD_MAJOR) {
		MD_BUG();
		return;
	}
	if (mddev_map[minor].mddev != mddev) {
		MD_BUG();
		return;
	}
	mddev_map[minor].mddev = NULL;
	mddev_map[minor].data = NULL;
}

static int md_make_request(request_queue_t *q, int rw, struct buffer_head * bh)
{
	mddev_t *mddev = kdev_to_mddev(bh->b_rdev);

	if (mddev && mddev->pers)
		return mddev->pers->make_request(mddev, rw, bh);
	else {
		buffer_IO_error(bh);
		return 0;
	}
}

static mddev_t * alloc_mddev(kdev_t dev)
{
	mddev_t *mddev;

	if (MAJOR(dev) != MD_MAJOR) {
		MD_BUG();
		return 0;
	}
	mddev = (mddev_t *) kmalloc(sizeof(*mddev), GFP_KERNEL);
	if (!mddev)
		return NULL;

	memset(mddev, 0, sizeof(*mddev));

	mddev->__minor = MINOR(dev);
	init_MUTEX(&mddev->reconfig_sem);
	init_MUTEX(&mddev->recovery_sem);
	init_MUTEX(&mddev->resync_sem);
	MD_INIT_LIST_HEAD(&mddev->disks);
	MD_INIT_LIST_HEAD(&mddev->all_mddevs);
	atomic_set(&mddev->active, 0);

	/*
	 * The 'base' mddev is the one with data NULL.
	 * personalities can create additional mddevs
	 * if necessary.
	 */
	add_mddev_mapping(mddev, dev, 0);
	md_list_add(&mddev->all_mddevs, &all_mddevs);

	MOD_INC_USE_COUNT;

	return mddev;
}

mdk_rdev_t * find_rdev_nr(mddev_t *mddev, int nr)
{
	mdk_rdev_t * rdev;
	struct md_list_head *tmp;

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->desc_nr == nr)
			return rdev;
	}
	return NULL;
}

mdk_rdev_t * find_rdev(mddev_t * mddev, kdev_t dev)
{
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->dev == dev)
			return rdev;
	}
	return NULL;
}

static MD_LIST_HEAD(device_names);

char * partition_name(kdev_t dev)
{
	struct gendisk *hd;
	static char nomem [] = "<nomem>";
	dev_name_t *dname;
	struct md_list_head *tmp = device_names.next;

	while (tmp != &device_names) {
		dname = md_list_entry(tmp, dev_name_t, list);
		if (dname->dev == dev)
			return dname->name;
		tmp = tmp->next;
	}

	dname = (dev_name_t *) kmalloc(sizeof(*dname), GFP_KERNEL);

	if (!dname)
		return nomem;
	/*
	 * ok, add this new device name to the list
	 */
	hd = get_gendisk (dev);
	dname->name = NULL;
	if (hd)
		dname->name = disk_name (hd, MINOR(dev), dname->namebuf);
	if (!dname->name) {
		sprintf (dname->namebuf, "[dev %s]", kdevname(dev));
		dname->name = dname->namebuf;
	}

	dname->dev = dev;
	MD_INIT_LIST_HEAD(&dname->list);
	md_list_add(&dname->list, &device_names);

	return dname->name;
}

static unsigned int calc_dev_sboffset(kdev_t dev, mddev_t *mddev,
						int persistent)
{
	unsigned int size = 0;

	if (blk_size[MAJOR(dev)])
		size = blk_size[MAJOR(dev)][MINOR(dev)];
	if (persistent)
		size = MD_NEW_SIZE_BLOCKS(size);
	return size;
}

static unsigned int calc_dev_size(kdev_t dev, mddev_t *mddev, int persistent)
{
	unsigned int size;

	size = calc_dev_sboffset(dev, mddev, persistent);
	if (!mddev->sb) {
		MD_BUG();
		return size;
	}
	if (mddev->sb->chunk_size)
		size &= ~(mddev->sb->chunk_size/1024 - 1);
	return size;
}

static unsigned int zoned_raid_size(mddev_t *mddev)
{
	unsigned int mask;
	mdk_rdev_t * rdev;
	struct md_list_head *tmp;

	if (!mddev->sb) {
		MD_BUG();
		return -EINVAL;
	}
	/*
	 * do size and offset calculations.
	 */
	mask = ~(mddev->sb->chunk_size/1024 - 1);

	ITERATE_RDEV(mddev,rdev,tmp) {
		rdev->size &= mask;
		md_size[mdidx(mddev)] += rdev->size;
	}
	return 0;
}

/*
 * We check wether all devices are numbered from 0 to nb_dev-1. The
 * order is guaranteed even after device name changes.
 *
 * Some personalities (raid0, linear) use this. Personalities that
 * provide data have to be able to deal with loss of individual
 * disks, so they do their checking themselves.
 */
int md_check_ordering(mddev_t *mddev)
{
	int i, c;
	mdk_rdev_t *rdev;
	struct md_list_head *tmp;

	/*
	 * First, all devices must be fully functional
	 */
	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty) {
			printk(KERN_ERR "md: md%d's device %s faulty, aborting.\n",
			       mdidx(mddev), partition_name(rdev->dev));
			goto abort;
		}
	}

	c = 0;
	ITERATE_RDEV(mddev,rdev,tmp) {
		c++;
	}
	if (c != mddev->nb_dev) {
		MD_BUG();
		goto abort;
	}
	if (mddev->nb_dev != mddev->sb->raid_disks) {
		printk(KERN_ERR "md: md%d, array needs %d disks, has %d, aborting.\n",
			mdidx(mddev), mddev->sb->raid_disks, mddev->nb_dev);
		goto abort;
	}
	/*
	 * Now the numbering check
	 */
	for (i = 0; i < mddev->nb_dev; i++) {
		c = 0;
		ITERATE_RDEV(mddev,rdev,tmp) {
			if (rdev->desc_nr == i)
				c++;
		}
		if (!c) {
			printk(KERN_ERR "md: md%d, missing disk #%d, aborting.\n",
			       mdidx(mddev), i);
			goto abort;
		}
		if (c > 1) {
			printk(KERN_ERR "md: md%d, too many disks #%d, aborting.\n",
			       mdidx(mddev), i);
			goto abort;
		}
	}
	return 0;
abort:
	return 1;
}

static void remove_descriptor(mdp_disk_t *disk, mdp_super_t *sb)
{
	if (disk_active(disk)) {
		sb->working_disks--;
	} else {
		if (disk_spare(disk)) {
			sb->spare_disks--;
			sb->working_disks--;
		} else	{
			sb->failed_disks--;
		}
	}
	sb->nr_disks--;
	disk->major = 0;
	disk->minor = 0;
	mark_disk_removed(disk);
}

#define BAD_MAGIC KERN_ERR \
"md: invalid raid superblock magic on %s\n"

#define BAD_MINOR KERN_ERR \
"md: %s: invalid raid minor (%x)\n"

#define OUT_OF_MEM KERN_ALERT \
"md: out of memory.\n"

#define NO_SB KERN_ERR \
"md: disabled device %s, could not read superblock.\n"

#define BAD_CSUM KERN_WARNING \
"md: invalid superblock checksum on %s\n"

static int alloc_array_sb(mddev_t * mddev)
{
	if (mddev->sb) {
		MD_BUG();
		return 0;
	}

	mddev->sb = (mdp_super_t *) __get_free_page (GFP_KERNEL);
	if (!mddev->sb)
		return -ENOMEM;
	md_clear_page(mddev->sb);
	return 0;
}

static int alloc_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev->sb)
		MD_BUG();

	rdev->sb_page = alloc_page(GFP_KERNEL);
	if (!rdev->sb_page) {
		printk(OUT_OF_MEM);
		return -EINVAL;
	}
	rdev->sb = (mdp_super_t *) page_address(rdev->sb_page);

	return 0;
}

static void free_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev->sb_page) {
		page_cache_release(rdev->sb_page);
		rdev->sb = NULL;
		rdev->sb_page = NULL;
		rdev->sb_offset = 0;
		rdev->size = 0;
	} else {
		if (!rdev->faulty)
			MD_BUG();
	}
}


static void bh_complete(struct buffer_head *bh, int uptodate)
{

	if (uptodate)
		set_bit(BH_Uptodate, &bh->b_state);

	complete((struct completion*)bh->b_private);
}

static int sync_page_io(kdev_t dev, unsigned long sector, int size,
			struct page *page, int rw)
{
	struct buffer_head bh;
	struct completion event;

	init_completion(&event);
	init_buffer(&bh, bh_complete, &event);
	bh.b_rdev = dev;
	bh.b_rsector = sector;
	bh.b_state	= (1 << BH_Req) | (1 << BH_Mapped) | (1 << BH_Lock);
	bh.b_size = size;
	bh.b_page = page;
	bh.b_reqnext = NULL;
	bh.b_data = page_address(page);
	generic_make_request(rw, &bh);

	run_task_queue(&tq_disk);
	wait_for_completion(&event);

	return test_bit(BH_Uptodate, &bh.b_state);
}

static int read_disk_sb(mdk_rdev_t * rdev)
{
	int ret = -EINVAL;
	kdev_t dev = rdev->dev;
	unsigned long sb_offset;

	if (!rdev->sb) {
		MD_BUG();
		goto abort;
	}

	/*
	 * Calculate the position of the superblock,
	 * it's at the end of the disk
	 */
	sb_offset = calc_dev_sboffset(rdev->dev, rdev->mddev, 1);
	rdev->sb_offset = sb_offset;

	if (!sync_page_io(dev, sb_offset<<1, MD_SB_BYTES, rdev->sb_page, READ)) {
		printk(NO_SB,partition_name(dev));
		return -EINVAL;
	}
	printk(KERN_INFO " [events: %08lx]\n", (unsigned long)rdev->sb->events_lo);
	ret = 0;
abort:
	return ret;
}

static unsigned int calc_sb_csum(mdp_super_t * sb)
{
	unsigned int disk_csum, csum;

	disk_csum = sb->sb_csum;
	sb->sb_csum = 0;
	csum = csum_partial((void *)sb, MD_SB_BYTES, 0);
	sb->sb_csum = disk_csum;
	return csum;
}

/*
 * Check one RAID superblock for generic plausibility
 */

static int check_disk_sb(mdk_rdev_t * rdev)
{
	mdp_super_t *sb;
	int ret = -EINVAL;

	sb = rdev->sb;
	if (!sb) {
		MD_BUG();
		goto abort;
	}

	if (sb->md_magic != MD_SB_MAGIC) {
		printk(BAD_MAGIC, partition_name(rdev->dev));
		goto abort;
	}

	if (sb->md_minor >= MAX_MD_DEVS) {
		printk(BAD_MINOR, partition_name(rdev->dev), sb->md_minor);
		goto abort;
	}

	if (calc_sb_csum(sb) != sb->sb_csum) {
		printk(BAD_CSUM, partition_name(rdev->dev));
		goto abort;
	}
	ret = 0;
abort:
	return ret;
}

static kdev_t dev_unit(kdev_t dev)
{
	unsigned int mask;
	struct gendisk *hd = get_gendisk(dev);

	if (!hd)
		return 0;
	mask = ~((1 << hd->minor_shift) - 1);

	return MKDEV(MAJOR(dev), MINOR(dev) & mask);
}

static mdk_rdev_t * match_dev_unit(mddev_t *mddev, kdev_t dev)
{
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;

	ITERATE_RDEV(mddev,rdev,tmp)
		if (dev_unit(rdev->dev) == dev_unit(dev))
			return rdev;

	return NULL;
}

static int match_mddev_units(mddev_t *mddev1, mddev_t *mddev2)
{
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;

	ITERATE_RDEV(mddev1,rdev,tmp)
		if (match_dev_unit(mddev2, rdev->dev))
			return 1;

	return 0;
}

static MD_LIST_HEAD(all_raid_disks);
static MD_LIST_HEAD(pending_raid_disks);

static void bind_rdev_to_array(mdk_rdev_t * rdev, mddev_t * mddev)
{
	mdk_rdev_t *same_pdev;

	if (rdev->mddev) {
		MD_BUG();
		return;
	}
	same_pdev = match_dev_unit(mddev, rdev->dev);
	if (same_pdev)
		printk( KERN_WARNING
"md%d: WARNING: %s appears to be on the same physical disk as %s. True\n"
"     protection against single-disk failure might be compromised.\n",
			mdidx(mddev), partition_name(rdev->dev),
				partition_name(same_pdev->dev));

	md_list_add(&rdev->same_set, &mddev->disks);
	rdev->mddev = mddev;
	mddev->nb_dev++;
	printk(KERN_INFO "md: bind<%s,%d>\n", partition_name(rdev->dev), mddev->nb_dev);
}

static void unbind_rdev_from_array(mdk_rdev_t * rdev)
{
	if (!rdev->mddev) {
		MD_BUG();
		return;
	}
	md_list_del(&rdev->same_set);
	MD_INIT_LIST_HEAD(&rdev->same_set);
	rdev->mddev->nb_dev--;
	printk(KERN_INFO "md: unbind<%s,%d>\n", partition_name(rdev->dev),
						 rdev->mddev->nb_dev);
	rdev->mddev = NULL;
}

/*
 * prevent the device from being mounted, repartitioned or
 * otherwise reused by a RAID array (or any other kernel
 * subsystem), by opening the device. [simply getting an
 * inode is not enough, the SCSI module usage code needs
 * an explicit open() on the device]
 */
static int lock_rdev(mdk_rdev_t *rdev)
{
	int err = 0;
	struct block_device *bdev;

	bdev = bdget(rdev->dev);
	if (!bdev)
		return -ENOMEM;
	err = blkdev_get(bdev, FMODE_READ|FMODE_WRITE, 0, BDEV_RAW);
	if (!err)
		rdev->bdev = bdev;
	return err;
}

static void unlock_rdev(mdk_rdev_t *rdev)
{
	struct block_device *bdev = rdev->bdev;
	rdev->bdev = NULL;
	if (!bdev)
		MD_BUG();
	blkdev_put(bdev, BDEV_RAW);
}

void md_autodetect_dev(kdev_t dev);

static void export_rdev(mdk_rdev_t * rdev)
{
	printk(KERN_INFO "md: export_rdev(%s)\n",partition_name(rdev->dev));
	if (rdev->mddev)
		MD_BUG();
	unlock_rdev(rdev);
	free_disk_sb(rdev);
	md_list_del(&rdev->all);
	MD_INIT_LIST_HEAD(&rdev->all);
	if (rdev->pending.next != &rdev->pending) {
		printk(KERN_INFO "md: (%s was pending)\n",
			partition_name(rdev->dev));
		md_list_del(&rdev->pending);
		MD_INIT_LIST_HEAD(&rdev->pending);
	}
#ifndef MODULE
	md_autodetect_dev(rdev->dev);
#endif
	rdev->dev = 0;
	rdev->faulty = 0;
	kfree(rdev);
}

static void kick_rdev_from_array(mdk_rdev_t * rdev)
{
	unbind_rdev_from_array(rdev);
	export_rdev(rdev);
}

static void export_array(mddev_t *mddev)
{
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;
	mdp_super_t *sb = mddev->sb;

	if (mddev->sb) {
		mddev->sb = NULL;
		free_page((unsigned long) sb);
	}

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (!rdev->mddev) {
			MD_BUG();
			continue;
		}
		kick_rdev_from_array(rdev);
	}
	if (mddev->nb_dev)
		MD_BUG();
}

static void free_mddev(mddev_t *mddev)
{
	if (!mddev) {
		MD_BUG();
		return;
	}

	export_array(mddev);
	md_size[mdidx(mddev)] = 0;
	md_hd_struct[mdidx(mddev)].nr_sects = 0;

	/*
	 * Make sure nobody else is using this mddev
	 * (careful, we rely on the global kernel lock here)
	 */
	while (sem_getcount(&mddev->resync_sem) != 1)
		schedule();
	while (sem_getcount(&mddev->recovery_sem) != 1)
		schedule();

	del_mddev_mapping(mddev, MKDEV(MD_MAJOR, mdidx(mddev)));
	md_list_del(&mddev->all_mddevs);
	MD_INIT_LIST_HEAD(&mddev->all_mddevs);
	kfree(mddev);
	MOD_DEC_USE_COUNT;
}

#undef BAD_CSUM
#undef BAD_MAGIC
#undef OUT_OF_MEM
#undef NO_SB

static void print_desc(mdp_disk_t *desc)
{
	printk(" DISK<N:%d,%s(%d,%d),R:%d,S:%d>\n", desc->number,
		partition_name(MKDEV(desc->major,desc->minor)),
		desc->major,desc->minor,desc->raid_disk,desc->state);
}

static void print_sb(mdp_super_t *sb)
{
	int i;

	printk(KERN_INFO "md:  SB: (V:%d.%d.%d) ID:<%08x.%08x.%08x.%08x> CT:%08x\n",
		sb->major_version, sb->minor_version, sb->patch_version,
		sb->set_uuid0, sb->set_uuid1, sb->set_uuid2, sb->set_uuid3,
		sb->ctime);
	printk(KERN_INFO "md:     L%d S%08d ND:%d RD:%d md%d LO:%d CS:%d\n", sb->level,
		sb->size, sb->nr_disks, sb->raid_disks, sb->md_minor,
		sb->layout, sb->chunk_size);
	printk(KERN_INFO "md:     UT:%08x ST:%d AD:%d WD:%d FD:%d SD:%d CSUM:%08x E:%08lx\n",
		sb->utime, sb->state, sb->active_disks, sb->working_disks,
		sb->failed_disks, sb->spare_disks,
		sb->sb_csum, (unsigned long)sb->events_lo);

	printk(KERN_INFO);
	for (i = 0; i < MD_SB_DISKS; i++) {
		mdp_disk_t *desc;

		desc = sb->disks + i;
		if (desc->number || desc->major || desc->minor ||
		    desc->raid_disk || (desc->state && (desc->state != 4))) {
			printk("     D %2d: ", i);
			print_desc(desc);
		}
	}
	printk(KERN_INFO "md:     THIS: ");
	print_desc(&sb->this_disk);

}

static void print_rdev(mdk_rdev_t *rdev)
{
	printk(KERN_INFO "md: rdev %s: O:%s, SZ:%08ld F:%d DN:%d ",
		partition_name(rdev->dev), partition_name(rdev->old_dev),
		rdev->size, rdev->faulty, rdev->desc_nr);
	if (rdev->sb) {
		printk(KERN_INFO "md: rdev superblock:\n");
		print_sb(rdev->sb);
	} else
		printk(KERN_INFO "md: no rdev superblock!\n");
}

void md_print_devices(void)
{
	struct md_list_head *tmp, *tmp2;
	mdk_rdev_t *rdev;
	mddev_t *mddev;

	printk("\n");
	printk("md:	**********************************\n");
	printk("md:	* <COMPLETE RAID STATE PRINTOUT> *\n");
	printk("md:	**********************************\n");
	ITERATE_MDDEV(mddev,tmp) {
		printk("md%d: ", mdidx(mddev));

		ITERATE_RDEV(mddev,rdev,tmp2)
			printk("<%s>", partition_name(rdev->dev));

		if (mddev->sb) {
			printk(" array superblock:\n");
			print_sb(mddev->sb);
		} else
			printk(" no array superblock.\n");

		ITERATE_RDEV(mddev,rdev,tmp2)
			print_rdev(rdev);
	}
	printk("md:	**********************************\n");
	printk("\n");
}

static int sb_equal(mdp_super_t *sb1, mdp_super_t *sb2)
{
	int ret;
	mdp_super_t *tmp1, *tmp2;

	tmp1 = kmalloc(sizeof(*tmp1),GFP_KERNEL);
	tmp2 = kmalloc(sizeof(*tmp2),GFP_KERNEL);

	if (!tmp1 || !tmp2) {
		ret = 0;
		printk(KERN_INFO "md.c: sb1 is not equal to sb2!\n");
		goto abort;
	}

	*tmp1 = *sb1;
	*tmp2 = *sb2;

	/*
	 * nr_disks is not constant
	 */
	tmp1->nr_disks = 0;
	tmp2->nr_disks = 0;

	if (memcmp(tmp1, tmp2, MD_SB_GENERIC_CONSTANT_WORDS * 4))
		ret = 0;
	else
		ret = 1;

abort:
	if (tmp1)
		kfree(tmp1);
	if (tmp2)
		kfree(tmp2);

	return ret;
}

static int uuid_equal(mdk_rdev_t *rdev1, mdk_rdev_t *rdev2)
{
	if (	(rdev1->sb->set_uuid0 == rdev2->sb->set_uuid0) &&
		(rdev1->sb->set_uuid1 == rdev2->sb->set_uuid1) &&
		(rdev1->sb->set_uuid2 == rdev2->sb->set_uuid2) &&
		(rdev1->sb->set_uuid3 == rdev2->sb->set_uuid3))

		return 1;

	return 0;
}

static mdk_rdev_t * find_rdev_all(kdev_t dev)
{
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;

	tmp = all_raid_disks.next;
	while (tmp != &all_raid_disks) {
		rdev = md_list_entry(tmp, mdk_rdev_t, all);
		if (rdev->dev == dev)
			return rdev;
		tmp = tmp->next;
	}
	return NULL;
}

#define GETBLK_FAILED KERN_ERR \
"md: getblk failed for device %s\n"

static int write_disk_sb(mdk_rdev_t * rdev)
{
	kdev_t dev;
	unsigned long sb_offset, size;

	if (!rdev->sb) {
		MD_BUG();
		return 1;
	}
	if (rdev->faulty) {
		MD_BUG();
		return 1;
	}
	if (rdev->sb->md_magic != MD_SB_MAGIC) {
		MD_BUG();
		return 1;
	}

	dev = rdev->dev;
	sb_offset = calc_dev_sboffset(dev, rdev->mddev, 1);
	if (rdev->sb_offset != sb_offset) {
		printk(KERN_INFO "%s's sb offset has changed from %ld to %ld, skipping\n",
		       partition_name(dev), rdev->sb_offset, sb_offset);
		goto skip;
	}
	/*
	 * If the disk went offline meanwhile and it's just a spare, then
	 * its size has changed to zero silently, and the MD code does
	 * not yet know that it's faulty.
	 */
	size = calc_dev_size(dev, rdev->mddev, 1);
	if (size != rdev->size) {
		printk(KERN_INFO "%s's size has changed from %ld to %ld since import, skipping\n",
		       partition_name(dev), rdev->size, size);
		goto skip;
	}

	printk(KERN_INFO "(write) %s's sb offset: %ld\n", partition_name(dev), sb_offset);

	if (!sync_page_io(dev, sb_offset<<1, MD_SB_BYTES, rdev->sb_page, WRITE)) {
		printk("md: write_disk_sb failed for device %s\n", partition_name(dev));
		return 1;
	}
skip:
	return 0;
}
#undef GETBLK_FAILED

static void set_this_disk(mddev_t *mddev, mdk_rdev_t *rdev)
{
	int i, ok = 0;
	mdp_disk_t *desc;

	for (i = 0; i < MD_SB_DISKS; i++) {
		desc = mddev->sb->disks + i;
#if 0
		if (disk_faulty(desc)) {
			if (MKDEV(desc->major,desc->minor) == rdev->dev)
				ok = 1;
			continue;
		}
#endif
		if (MKDEV(desc->major,desc->minor) == rdev->dev) {
			rdev->sb->this_disk = *desc;
			rdev->desc_nr = desc->number;
			ok = 1;
			break;
		}
	}

	if (!ok) {
		MD_BUG();
	}
}

static int sync_sbs(mddev_t * mddev)
{
	mdk_rdev_t *rdev;
	mdp_super_t *sb;
	struct md_list_head *tmp;

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty || rdev->alias_device)
			continue;
		sb = rdev->sb;
		*sb = *mddev->sb;
		set_this_disk(mddev, rdev);
		sb->sb_csum = calc_sb_csum(sb);
	}
	return 0;
}

int md_update_sb(mddev_t * mddev)
{
	int err, count = 100;
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;

	if (!mddev->sb_dirty) {
		printk("hm, md_update_sb() called without ->sb_dirty == 1, from %p.\n", __builtin_return_address(0));
		return 0;
	}
	mddev->sb_dirty = 0;
repeat:
	mddev->sb->utime = CURRENT_TIME;
	if ((++mddev->sb->events_lo)==0)
		++mddev->sb->events_hi;

	if ((mddev->sb->events_lo|mddev->sb->events_hi)==0) {
		/*
		 * oops, this 64-bit counter should never wrap.
		 * Either we are in around ~1 trillion A.C., assuming
		 * 1 reboot per second, or we have a bug:
		 */
		MD_BUG();
		mddev->sb->events_lo = mddev->sb->events_hi = 0xffffffff;
	}
	sync_sbs(mddev);

	/*
	 * do not write anything to disk if using
	 * nonpersistent superblocks
	 */
	if (mddev->sb->not_persistent)
		return 0;

	printk(KERN_INFO "md: updating md%d RAID superblock on device\n",
					mdidx(mddev));

	err = 0;
	ITERATE_RDEV(mddev,rdev,tmp) {
		printk(KERN_INFO "md: ");
		if (rdev->faulty)
			printk("(skipping faulty ");
		if (rdev->alias_device)
			printk("(skipping alias ");
		if (!rdev->faulty && disk_faulty(&rdev->sb->this_disk)) {
			printk("(skipping new-faulty %s )\n",
			       partition_name(rdev->dev));
			continue;
		}
		printk("%s ", partition_name(rdev->dev));
		if (!rdev->faulty && !rdev->alias_device) {
			printk("[events: %08lx]",
				(unsigned long)rdev->sb->events_lo);
			err += write_disk_sb(rdev);
		} else
			printk(")\n");
	}
	if (err) {
		if (--count) {
			printk(KERN_ERR "md: errors occurred during superblock update, repeating\n");
			goto repeat;
		}
		printk(KERN_ERR "md: excessive errors occurred during superblock update, exiting\n");
	}
	return 0;
}

/*
 * Import a device. If 'on_disk', then sanity check the superblock
 *
 * mark the device faulty if:
 *
 *   - the device is nonexistent (zero size)
 *   - the device has no valid superblock
 *
 */
static int md_import_device(kdev_t newdev, int on_disk)
{
	int err;
	mdk_rdev_t *rdev;
	unsigned int size;

	if (find_rdev_all(newdev))
		return -EEXIST;

	rdev = (mdk_rdev_t *) kmalloc(sizeof(*rdev), GFP_KERNEL);
	if (!rdev) {
		printk(KERN_ERR "md: could not alloc mem for %s!\n", partition_name(newdev));
		return -ENOMEM;
	}
	memset(rdev, 0, sizeof(*rdev));

	if (is_mounted(newdev)) {
		printk(KERN_WARNING "md: can not import %s, has active inodes!\n",
			partition_name(newdev));
		err = -EBUSY;
		goto abort_free;
	}

	if ((err = alloc_disk_sb(rdev)))
		goto abort_free;

	rdev->dev = newdev;
	if (lock_rdev(rdev)) {
		printk(KERN_ERR "md: could not lock %s, zero-size? Marking faulty.\n",
			partition_name(newdev));
		err = -EINVAL;
		goto abort_free;
	}
	rdev->desc_nr = -1;
	rdev->faulty = 0;

	size = 0;
	if (blk_size[MAJOR(newdev)])
		size = blk_size[MAJOR(newdev)][MINOR(newdev)];
	if (!size) {
		printk(KERN_WARNING "md: %s has zero size, marking faulty!\n",
				partition_name(newdev));
		err = -EINVAL;
		goto abort_free;
	}

	if (on_disk) {
		if ((err = read_disk_sb(rdev))) {
			printk(KERN_WARNING "md: could not read %s's sb, not importing!\n",
			       partition_name(newdev));
			goto abort_free;
		}
		if ((err = check_disk_sb(rdev))) {
			printk(KERN_WARNING "md: %s has invalid sb, not importing!\n",
			       partition_name(newdev));
			goto abort_free;
		}

		if (rdev->sb->level != -4) {
			rdev->old_dev = MKDEV(rdev->sb->this_disk.major,
						rdev->sb->this_disk.minor);
			rdev->desc_nr = rdev->sb->this_disk.number;
		} else {
			rdev->old_dev = MKDEV(0, 0);
			rdev->desc_nr = -1;
		}
	}
	md_list_add(&rdev->all, &all_raid_disks);
	MD_INIT_LIST_HEAD(&rdev->pending);

	return 0;

abort_free:
	if (rdev->sb) {
		if (rdev->bdev)
			unlock_rdev(rdev);
		free_disk_sb(rdev);
	}
	kfree(rdev);
	return err;
}

/*
 * Check a full RAID array for plausibility
 */

#define INCONSISTENT KERN_ERR \
"md: fatal superblock inconsistency in %s -- removing from array\n"

#define OUT_OF_DATE KERN_ERR \
"md: superblock update time inconsistency -- using the most recent one\n"

#define OLD_VERSION KERN_ALERT \
"md: md%d: unsupported raid array version %d.%d.%d\n"

#define NOT_CLEAN_IGNORE KERN_ERR \
"md: md%d: raid array is not clean -- starting background reconstruction\n"

#define UNKNOWN_LEVEL KERN_ERR \
"md: md%d: unsupported raid level %d\n"

static int analyze_sbs(mddev_t * mddev)
{
	int out_of_date = 0, i, first;
	struct md_list_head *tmp, *tmp2;
	mdk_rdev_t *rdev, *rdev2, *freshest;
	mdp_super_t *sb;

	/*
	 * Verify the RAID superblock on each real device
	 */
	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty) {
			MD_BUG();
			goto abort;
		}
		if (!rdev->sb) {
			MD_BUG();
			goto abort;
		}
		if (check_disk_sb(rdev))
			goto abort;
	}

	/*
	 * The superblock constant part has to be the same
	 * for all disks in the array.
	 */
	sb = NULL;

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (!sb) {
			sb = rdev->sb;
			continue;
		}
		if (!sb_equal(sb, rdev->sb)) {
			printk(INCONSISTENT, partition_name(rdev->dev));
			kick_rdev_from_array(rdev);
			continue;
		}
	}

	/*
	 * OK, we have all disks and the array is ready to run. Let's
	 * find the freshest superblock, that one will be the superblock
	 * that represents the whole array.
	 */
	if (!mddev->sb)
		if (alloc_array_sb(mddev))
			goto abort;
	sb = mddev->sb;
	freshest = NULL;

	ITERATE_RDEV(mddev,rdev,tmp) {
		__u64 ev1, ev2;
		/*
		 * if the checksum is invalid, use the superblock
		 * only as a last resort. (decrease it's age by
		 * one event)
		 */
		if (calc_sb_csum(rdev->sb) != rdev->sb->sb_csum) {
			if (rdev->sb->events_lo || rdev->sb->events_hi)
				if ((rdev->sb->events_lo--)==0)
					rdev->sb->events_hi--;
		}

		printk(KERN_INFO "md: %s's event counter: %08lx\n",
		       partition_name(rdev->dev),
			(unsigned long)rdev->sb->events_lo);
		if (!freshest) {
			freshest = rdev;
			continue;
		}
		/*
		 * Find the newest superblock version
		 */
		ev1 = md_event(rdev->sb);
		ev2 = md_event(freshest->sb);
		if (ev1 != ev2) {
			out_of_date = 1;
			if (ev1 > ev2)
				freshest = rdev;
		}
	}
	if (out_of_date) {
		printk(OUT_OF_DATE);
		printk(KERN_INFO "md: freshest: %s\n", partition_name(freshest->dev));
	}
	memcpy (sb, freshest->sb, sizeof(*sb));

	/*
	 * at this point we have picked the 'best' superblock
	 * from all available superblocks.
	 * now we validate this superblock and kick out possibly
	 * failed disks.
	 */
	ITERATE_RDEV(mddev,rdev,tmp) {
		/*
		 * Kick all non-fresh devices
		 */
		__u64 ev1, ev2;
		ev1 = md_event(rdev->sb);
		ev2 = md_event(sb);
		++ev1;
		if (ev1 < ev2) {
			printk(KERN_WARNING "md: kicking non-fresh %s from array!\n",
						partition_name(rdev->dev));
			kick_rdev_from_array(rdev);
			continue;
		}
	}

	/*
	 * Fix up changed device names ... but only if this disk has a
	 * recent update time. Use faulty checksum ones too.
	 */
	if (mddev->sb->level != -4)
	ITERATE_RDEV(mddev,rdev,tmp) {
		__u64 ev1, ev2, ev3;
		if (rdev->faulty || rdev->alias_device) {
			MD_BUG();
			goto abort;
		}
		ev1 = md_event(rdev->sb);
		ev2 = md_event(sb);
		ev3 = ev2;
		--ev3;
		if ((rdev->dev != rdev->old_dev) &&
			((ev1 == ev2) || (ev1 == ev3))) {
			mdp_disk_t *desc;

			printk(KERN_WARNING "md: device name has changed from %s to %s since last import!\n",
			       partition_name(rdev->old_dev), partition_name(rdev->dev));
			if (rdev->desc_nr == -1) {
				MD_BUG();
				goto abort;
			}
			desc = &sb->disks[rdev->desc_nr];
			if (rdev->old_dev != MKDEV(desc->major, desc->minor)) {
				MD_BUG();
				goto abort;
			}
			desc->major = MAJOR(rdev->dev);
			desc->minor = MINOR(rdev->dev);
			desc = &rdev->sb->this_disk;
			desc->major = MAJOR(rdev->dev);
			desc->minor = MINOR(rdev->dev);
		}
	}

	/*
	 * Remove unavailable and faulty devices ...
	 *
	 * note that if an array becomes completely unrunnable due to
	 * missing devices, we do not write the superblock back, so the
	 * administrator has a chance to fix things up. The removal thus
	 * only happens if it's nonfatal to the contents of the array.
	 */
	for (i = 0; i < MD_SB_DISKS; i++) {
		int found;
		mdp_disk_t *desc;
		kdev_t dev;

		desc = sb->disks + i;
		dev = MKDEV(desc->major, desc->minor);

		/*
		 * We kick faulty devices/descriptors immediately.
		 *
		 * Note: multipath devices are a special case.  Since we
		 * were able to read the superblock on the path, we don't
		 * care if it was previously marked as faulty, it's up now
		 * so enable it.
		 */
		if (disk_faulty(desc) && mddev->sb->level != -4) {
			found = 0;
			ITERATE_RDEV(mddev,rdev,tmp) {
				if (rdev->desc_nr != desc->number)
					continue;
				printk(KERN_WARNING "md%d: kicking faulty %s!\n",
					mdidx(mddev),partition_name(rdev->dev));
				kick_rdev_from_array(rdev);
				found = 1;
				break;
			}
			if (!found) {
				if (dev == MKDEV(0,0))
					continue;
				printk(KERN_WARNING "md%d: removing former faulty %s!\n",
					mdidx(mddev), partition_name(dev));
			}
			remove_descriptor(desc, sb);
			continue;
		} else if (disk_faulty(desc)) {
			/*
			 * multipath entry marked as faulty, unfaulty it
			 */
			rdev = find_rdev(mddev, dev);
			if(rdev)
				mark_disk_spare(desc);
			else
				remove_descriptor(desc, sb);
		}

		if (dev == MKDEV(0,0))
			continue;
		/*
		 * Is this device present in the rdev ring?
		 */
		found = 0;
		ITERATE_RDEV(mddev,rdev,tmp) {
			/*
			 * Multi-path IO special-case: since we have no
			 * this_disk descriptor at auto-detect time,
			 * we cannot check rdev->number.
			 * We can check the device though.
			 */
			if ((sb->level == -4) && (rdev->dev ==
					MKDEV(desc->major,desc->minor))) {
				found = 1;
				break;
			}
			if (rdev->desc_nr == desc->number) {
				found = 1;
				break;
			}
		}
		if (found)
			continue;

		printk(KERN_WARNING "md%d: former device %s is unavailable, removing from array!\n",
		       mdidx(mddev), partition_name(dev));
		remove_descriptor(desc, sb);
	}

	/*
	 * Double check wether all devices mentioned in the
	 * superblock are in the rdev ring.
	 */
	first = 1;
	for (i = 0; i < MD_SB_DISKS; i++) {
		mdp_disk_t *desc;
		kdev_t dev;

		desc = sb->disks + i;
		dev = MKDEV(desc->major, desc->minor);

		if (dev == MKDEV(0,0))
			continue;

		if (disk_faulty(desc)) {
			MD_BUG();
			goto abort;
		}

		rdev = find_rdev(mddev, dev);
		if (!rdev) {
			MD_BUG();
			goto abort;
		}
		/*
		 * In the case of Multipath-IO, we have no
		 * other information source to find out which
		 * disk is which, only the position of the device
		 * in the superblock:
		 */
		if (mddev->sb->level == -4) {
			if ((rdev->desc_nr != -1) && (rdev->desc_nr != i)) {
				MD_BUG();
				goto abort;
			}
			rdev->desc_nr = i;
			if (!first)
				rdev->alias_device = 1;
			else
				first = 0;
		}
	}

	/*
	 * Kick all rdevs that are not in the
	 * descriptor array:
	 */
	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->desc_nr == -1)
			kick_rdev_from_array(rdev);
	}

	/*
	 * Do a final reality check.
	 */
	if (mddev->sb->level != -4) {
		ITERATE_RDEV(mddev,rdev,tmp) {
			if (rdev->desc_nr == -1) {
				MD_BUG();
				goto abort;
			}
			/*
			 * is the desc_nr unique?
			 */
			ITERATE_RDEV(mddev,rdev2,tmp2) {
				if ((rdev2 != rdev) &&
						(rdev2->desc_nr == rdev->desc_nr)) {
					MD_BUG();
					goto abort;
				}
			}
			/*
			 * is the device unique?
			 */
			ITERATE_RDEV(mddev,rdev2,tmp2) {
				if ((rdev2 != rdev) &&
						(rdev2->dev == rdev->dev)) {
					MD_BUG();
					goto abort;
				}
			}
		}
	}

	/*
	 * Check if we can support this RAID array
	 */
	if (sb->major_version != MD_MAJOR_VERSION ||
			sb->minor_version > MD_MINOR_VERSION) {

		printk(OLD_VERSION, mdidx(mddev), sb->major_version,
				sb->minor_version, sb->patch_version);
		goto abort;
	}

	if ((sb->state != (1 << MD_SB_CLEAN)) && ((sb->level == 1) ||
			(sb->level == 4) || (sb->level == 5)))
		printk(NOT_CLEAN_IGNORE, mdidx(mddev));

	return 0;
abort:
	return 1;
}

#undef INCONSISTENT
#undef OUT_OF_DATE
#undef OLD_VERSION
#undef OLD_LEVEL

static int device_size_calculation(mddev_t * mddev)
{
	int data_disks = 0, persistent;
	unsigned int readahead;
	mdp_super_t *sb = mddev->sb;
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;

	/*
	 * Do device size calculation. Bail out if too small.
	 * (we have to do this after having validated chunk_size,
	 * because device size has to be modulo chunk_size)
	 */
	persistent = !mddev->sb->not_persistent;
	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty)
			continue;
		if (rdev->size) {
			MD_BUG();
			continue;
		}
		rdev->size = calc_dev_size(rdev->dev, mddev, persistent);
		if (rdev->size < sb->chunk_size / 1024) {
			printk(KERN_WARNING
				"md: Dev %s smaller than chunk_size: %ldk < %dk\n",
				partition_name(rdev->dev),
				rdev->size, sb->chunk_size / 1024);
			return -EINVAL;
		}
	}

	switch (sb->level) {
		case -4:
			data_disks = 1;
			break;
		case -3:
			data_disks = 1;
			break;
		case -2:
			data_disks = 1;
			break;
		case -1:
			zoned_raid_size(mddev);
			data_disks = 1;
			break;
		case 0:
			zoned_raid_size(mddev);
			data_disks = sb->raid_disks;
			break;
		case 1:
			data_disks = 1;
			break;
		case 4:
		case 5:
			data_disks = sb->raid_disks-1;
			break;
		default:
			printk(UNKNOWN_LEVEL, mdidx(mddev), sb->level);
			goto abort;
	}
	if (!md_size[mdidx(mddev)])
		md_size[mdidx(mddev)] = sb->size * data_disks;

	readahead = MD_READAHEAD;
	if ((sb->level == 0) || (sb->level == 4) || (sb->level == 5)) {
		readahead = (mddev->sb->chunk_size>>PAGE_SHIFT) * 4 * data_disks;
		if (readahead < data_disks * (MAX_SECTORS>>(PAGE_SHIFT-9))*2)
			readahead = data_disks * (MAX_SECTORS>>(PAGE_SHIFT-9))*2;
	} else {
		// (no multipath branch - it uses the default setting)
		if (sb->level == -3)
			readahead = 0;
	}
	md_maxreadahead[mdidx(mddev)] = readahead;

	printk(KERN_INFO "md%d: max total readahead window set to %ldk\n",
		mdidx(mddev), readahead*(PAGE_SIZE/1024));

	printk(KERN_INFO
		"md%d: %d data-disks, max readahead per data-disk: %ldk\n",
			mdidx(mddev), data_disks, readahead/data_disks*(PAGE_SIZE/1024));
	return 0;
abort:
	return 1;
}


#define TOO_BIG_CHUNKSIZE KERN_ERR \
"too big chunk_size: %d > %d\n"

#define TOO_SMALL_CHUNKSIZE KERN_ERR \
"too small chunk_size: %d < %ld\n"

#define BAD_CHUNKSIZE KERN_ERR \
"no chunksize specified, see 'man raidtab'\n"

static int do_md_run(mddev_t * mddev)
{
	int pnum, err;
	int chunk_size;
	struct md_list_head *tmp;
	mdk_rdev_t *rdev;


	if (!mddev->nb_dev) {
		MD_BUG();
		return -EINVAL;
	}

	if (mddev->pers)
		return -EBUSY;

	/*
	 * Resize disks to align partitions size on a given
	 * chunk size.
	 */
	md_size[mdidx(mddev)] = 0;

	/*
	 * Analyze all RAID superblock(s)
	 */
	if (analyze_sbs(mddev)) {
		MD_BUG();
		return -EINVAL;
	}

	chunk_size = mddev->sb->chunk_size;
	pnum = level_to_pers(mddev->sb->level);

	mddev->param.chunk_size = chunk_size;
	mddev->param.personality = pnum;

	if ((pnum != MULTIPATH) && (pnum != RAID1)) {
		if (!chunk_size) {
			/*
			 * 'default chunksize' in the old md code used to
			 * be PAGE_SIZE, baaad.
			 * we abort here to be on the safe side. We dont
			 * want to continue the bad practice.
			 */
			printk(BAD_CHUNKSIZE);
			return -EINVAL;
		}
		if (chunk_size > MAX_CHUNK_SIZE) {
			printk(TOO_BIG_CHUNKSIZE, chunk_size, MAX_CHUNK_SIZE);
			return -EINVAL;
		}
		/*
		 * chunk-size has to be a power of 2 and multiples of PAGE_SIZE
		 */
		if ( (1 << ffz(~chunk_size)) != chunk_size) {
			MD_BUG();
			return -EINVAL;
		}
		if (chunk_size < PAGE_SIZE) {
			printk(TOO_SMALL_CHUNKSIZE, chunk_size, PAGE_SIZE);
			return -EINVAL;
		}
	} else
		if (chunk_size)
			printk(KERN_INFO "md: RAID level %d does not need chunksize! Continuing anyway.\n",
			       mddev->sb->level);

	if (pnum >= MAX_PERSONALITY) {
		MD_BUG();
		return -EINVAL;
	}

	if (!pers[pnum])
	{
#ifdef CONFIG_KMOD
		char module_name[80];
		sprintf (module_name, "md-personality-%d", pnum);
		request_module (module_name);
		if (!pers[pnum])
#endif
		{
			printk(KERN_ERR "md: personality %d is not loaded!\n",
				pnum);
			return -EINVAL;
		}
	}

	if (device_size_calculation(mddev))
		return -EINVAL;

	/*
	 * Drop all container device buffers, from now on
	 * the only valid external interface is through the md
	 * device.
	 * Also find largest hardsector size
	 */
	md_hardsect_sizes[mdidx(mddev)] = 512;
	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty)
			continue;
		invalidate_device(rdev->dev, 1);
		if (get_hardsect_size(rdev->dev)
			> md_hardsect_sizes[mdidx(mddev)])
			md_hardsect_sizes[mdidx(mddev)] =
				get_hardsect_size(rdev->dev);
	}
	md_blocksizes[mdidx(mddev)] = 1024;
	if (md_blocksizes[mdidx(mddev)] < md_hardsect_sizes[mdidx(mddev)])
		md_blocksizes[mdidx(mddev)] = md_hardsect_sizes[mdidx(mddev)];
	mddev->pers = pers[pnum];

	err = mddev->pers->run(mddev);
	if (err) {
		printk(KERN_ERR "md: pers->run() failed ...\n");
		mddev->pers = NULL;
		return -EINVAL;
	}

	mddev->sb->state &= ~(1 << MD_SB_CLEAN);
	mddev->sb_dirty = 1;
	md_update_sb(mddev);

	/*
	 * md_size has units of 1K blocks, which are
	 * twice as large as sectors.
	 */
	md_hd_struct[mdidx(mddev)].start_sect = 0;
	register_disk(&md_gendisk, MKDEV(MAJOR_NR,mdidx(mddev)),
			1, &md_fops, md_size[mdidx(mddev)]<<1);

	read_ahead[MD_MAJOR] = 1024;
	return (0);
}

#undef TOO_BIG_CHUNKSIZE
#undef BAD_CHUNKSIZE

#define OUT(x) do { err = (x); goto out; } while (0)

static int restart_array(mddev_t *mddev)
{
	int err = 0;

	/*
	 * Complain if it has no devices
	 */
	if (!mddev->nb_dev)
		OUT(-ENXIO);

	if (mddev->pers) {
		if (!mddev->ro)
			OUT(-EBUSY);

		mddev->ro = 0;
		set_device_ro(mddev_to_kdev(mddev), 0);

		printk(KERN_INFO
			"md: md%d switched to read-write mode.\n", mdidx(mddev));
		/*
		 * Kick recovery or resync if necessary
		 */
		md_recover_arrays();
		if (mddev->pers->restart_resync)
			mddev->pers->restart_resync(mddev);
	} else {
		printk(KERN_ERR "md: md%d has no personality assigned.\n",
			mdidx(mddev));
		err = -EINVAL;
	}

out:
	return err;
}

#define STILL_MOUNTED KERN_WARNING \
"md: md%d still mounted.\n"
#define	STILL_IN_USE \
"md: md%d still in use.\n"

static int do_md_stop(mddev_t * mddev, int ro)
{
	int err = 0, resync_interrupted = 0;
	kdev_t dev = mddev_to_kdev(mddev);

#if 0 /* ->active is not currently reliable */
	if (atomic_read(&mddev->active)>1) {
		printk(STILL_IN_USE, mdidx(mddev));
		OUT(-EBUSY);
	}
#endif

	if (mddev->pers) {
		/*
		 * It is safe to call stop here, it only frees private
		 * data. Also, it tells us if a device is unstoppable
		 * (eg. resyncing is in progress)
		 */
		if (mddev->pers->stop_resync)
			if (mddev->pers->stop_resync(mddev))
				resync_interrupted = 1;

		if (mddev->recovery_running)
			md_interrupt_thread(md_recovery_thread);

		/*
		 * This synchronizes with signal delivery to the
		 * resync or reconstruction thread. It also nicely
		 * hangs the process if some reconstruction has not
		 * finished.
		 */
		down(&mddev->recovery_sem);
		up(&mddev->recovery_sem);

		invalidate_device(dev, 1);

		if (ro) {
			if (mddev->ro)
				OUT(-ENXIO);
			mddev->ro = 1;
		} else {
			if (mddev->ro)
				set_device_ro(dev, 0);
			if (mddev->pers->stop(mddev)) {
				if (mddev->ro)
					set_device_ro(dev, 1);
				OUT(-EBUSY);
			}
			if (mddev->ro)
				mddev->ro = 0;
		}
		if (mddev->sb) {
			/*
			 * mark it clean only if there was no resync
			 * interrupted.
			 */
			if (!mddev->recovery_running && !resync_interrupted) {
				printk(KERN_INFO "md: marking sb clean...\n");
				mddev->sb->state |= 1 << MD_SB_CLEAN;
			}
			mddev->sb_dirty = 1;
			md_update_sb(mddev);
		}
		if (ro)
			set_device_ro(dev, 1);
	}

	/*
	 * Free resources if final stop
	 */
	if (!ro) {
		printk(KERN_INFO "md: md%d stopped.\n", mdidx(mddev));
		free_mddev(mddev);

	} else
		printk(KERN_INFO "md: md%d switched to read-only mode.\n", mdidx(mddev));
out:
	return err;
}

#undef OUT

/*
 * We have to safely support old arrays too.
 */
int detect_old_array(mdp_super_t *sb)
{
	if (sb->major_version > 0)
		return 0;
	if (sb->minor_version >= 90)
		return 0;

	return -EINVAL;
}


static void autorun_array(mddev_t *mddev)
{
	mdk_rdev_t *rdev;
	struct md_list_head *tmp;
	int err;

	if (mddev->disks.prev == &mddev->disks) {
		MD_BUG();
		return;
	}

	printk(KERN_INFO "md: running: ");

	ITERATE_RDEV(mddev,rdev,tmp) {
		printk("<%s>", partition_name(rdev->dev));
	}
	printk("\n");

	err = do_md_run (mddev);
	if (err) {
		printk(KERN_WARNING "md :do_md_run() returned %d\n", err);
		/*
		 * prevent the writeback of an unrunnable array
		 */
		mddev->sb_dirty = 0;
		do_md_stop (mddev, 0);
	}
}

/*
 * lets try to run arrays based on all disks that have arrived
 * until now. (those are in the ->pending list)
 *
 * the method: pick the first pending disk, collect all disks with
 * the same UUID, remove all from the pending list and put them into
 * the 'same_array' list. Then order this list based on superblock
 * update time (freshest comes first), kick out 'old' disks and
 * compare superblocks. If everything's fine then run it.
 *
 * If "unit" is allocated, then bump its reference count
 */
static void autorun_devices(kdev_t countdev)
{
	struct md_list_head candidates;
	struct md_list_head *tmp;
	mdk_rdev_t *rdev0, *rdev;
	mddev_t *mddev;
	kdev_t md_kdev;


	printk(KERN_INFO "md: autorun ...\n");
	while (pending_raid_disks.next != &pending_raid_disks) {
		rdev0 = md_list_entry(pending_raid_disks.next,
					 mdk_rdev_t, pending);

		printk(KERN_INFO "md: considering %s ...\n", partition_name(rdev0->dev));
		MD_INIT_LIST_HEAD(&candidates);
		ITERATE_RDEV_PENDING(rdev,tmp) {
			if (uuid_equal(rdev0, rdev)) {
				if (!sb_equal(rdev0->sb, rdev->sb)) {
					printk(KERN_WARNING
					       "md: %s has same UUID as %s, but superblocks differ ...\n",
					       partition_name(rdev->dev), partition_name(rdev0->dev));
					continue;
				}
				printk(KERN_INFO "md:  adding %s ...\n", partition_name(rdev->dev));
				md_list_del(&rdev->pending);
				md_list_add(&rdev->pending, &candidates);
			}
		}
		/*
		 * now we have a set of devices, with all of them having
		 * mostly sane superblocks. It's time to allocate the
		 * mddev.
		 */
		md_kdev = MKDEV(MD_MAJOR, rdev0->sb->md_minor);
		mddev = kdev_to_mddev(md_kdev);
		if (mddev) {
			printk(KERN_WARNING "md: md%d already running, cannot run %s\n",
			       mdidx(mddev), partition_name(rdev0->dev));
			ITERATE_RDEV_GENERIC(candidates,pending,rdev,tmp)
				export_rdev(rdev);
			continue;
		}
		mddev = alloc_mddev(md_kdev);
		if (!mddev) {
			printk(KERN_ERR "md: cannot allocate memory for md drive.\n");
			break;
		}
		if (md_kdev == countdev)
			atomic_inc(&mddev->active);
		printk(KERN_INFO "md: created md%d\n", mdidx(mddev));
		ITERATE_RDEV_GENERIC(candidates,pending,rdev,tmp) {
			bind_rdev_to_array(rdev, mddev);
			md_list_del(&rdev->pending);
			MD_INIT_LIST_HEAD(&rdev->pending);
		}
		autorun_array(mddev);
	}
	printk(KERN_INFO "md: ... autorun DONE.\n");
}

/*
 * import RAID devices based on one partition
 * if possible, the array gets run as well.
 */

#define BAD_VERSION KERN_ERR \
"md: %s has RAID superblock version 0.%d, autodetect needs v0.90 or higher\n"

#define OUT_OF_MEM KERN_ALERT \
"md: out of memory.\n"

#define NO_DEVICE KERN_ERR \
"md: disabled device %s\n"

#define AUTOADD_FAILED KERN_ERR \
"md: auto-adding devices to md%d FAILED (error %d).\n"

#define AUTOADD_FAILED_USED KERN_ERR \
"md: cannot auto-add device %s to md%d, already used.\n"

#define AUTORUN_FAILED KERN_ERR \
"md: auto-running md%d FAILED (error %d).\n"

#define MDDEV_BUSY KERN_ERR \
"md: cannot auto-add to md%d, already running.\n"

#define AUTOADDING KERN_INFO \
"md: auto-adding devices to md%d, based on %s's superblock.\n"

#define AUTORUNNING KERN_INFO \
"md: auto-running md%d.\n"

static int autostart_array(kdev_t startdev, kdev_t countdev)
{
	int err = -EINVAL, i;
	mdp_super_t *sb = NULL;
	mdk_rdev_t *start_rdev = NULL, *rdev;

	if (md_import_device(startdev, 1)) {
		printk(KERN_WARNING "md: could not import %s!\n", partition_name(startdev));
		goto abort;
	}

	start_rdev = find_rdev_all(startdev);
	if (!start_rdev) {
		MD_BUG();
		goto abort;
	}
	if (start_rdev->faulty) {
		printk(KERN_WARNING "md: can not autostart based on faulty %s!\n",
						partition_name(startdev));
		goto abort;
	}
	md_list_add(&start_rdev->pending, &pending_raid_disks);

	sb = start_rdev->sb;

	err = detect_old_array(sb);
	if (err) {
		printk(KERN_WARNING "md: array version is too old to be autostarted ,"
		       "use raidtools 0.90 mkraid --upgrade to upgrade the array "
		       "without data loss!\n");
		goto abort;
	}

	for (i = 0; i < MD_SB_DISKS; i++) {
		mdp_disk_t *desc;
		kdev_t dev;

		desc = sb->disks + i;
		dev = MKDEV(desc->major, desc->minor);

		if (dev == MKDEV(0,0))
			continue;
		if (dev == startdev)
			continue;
		if (md_import_device(dev, 1)) {
			printk(KERN_WARNING "md: could not import %s, trying to run array nevertheless.\n",
			       partition_name(dev));
			continue;
		}
		rdev = find_rdev_all(dev);
		if (!rdev) {
			MD_BUG();
			goto abort;
		}
		md_list_add(&rdev->pending, &pending_raid_disks);
	}

	/*
	 * possibly return codes
	 */
	autorun_devices(countdev);
	return 0;

abort:
	if (start_rdev)
		export_rdev(start_rdev);
	return err;
}

#undef BAD_VERSION
#undef OUT_OF_MEM
#undef NO_DEVICE
#undef AUTOADD_FAILED_USED
#undef AUTOADD_FAILED
#undef AUTORUN_FAILED
#undef AUTOADDING
#undef AUTORUNNING


static int get_version(void * arg)
{
	mdu_version_t ver;

	ver.major = MD_MAJOR_VERSION;
	ver.minor = MD_MINOR_VERSION;
	ver.patchlevel = MD_PATCHLEVEL_VERSION;

	if (md_copy_to_user(arg, &ver, sizeof(ver)))
		return -EFAULT;

	return 0;
}

#define SET_FROM_SB(x) info.x = mddev->sb->x
static int get_array_info(mddev_t * mddev, void * arg)
{
	mdu_array_info_t info;

	if (!mddev->sb) {
		MD_BUG();
		return -EINVAL;
	}

	SET_FROM_SB(major_version);
	SET_FROM_SB(minor_version);
	SET_FROM_SB(patch_version);
	SET_FROM_SB(ctime);
	SET_FROM_SB(level);
	SET_FROM_SB(size);
	SET_FROM_SB(nr_disks);
	SET_FROM_SB(raid_disks);
	SET_FROM_SB(md_minor);
	SET_FROM_SB(not_persistent);

	SET_FROM_SB(utime);
	SET_FROM_SB(state);
	SET_FROM_SB(active_disks);
	SET_FROM_SB(working_disks);
	SET_FROM_SB(failed_disks);
	SET_FROM_SB(spare_disks);

	SET_FROM_SB(layout);
	SET_FROM_SB(chunk_size);

	if (md_copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}
#undef SET_FROM_SB

#define SET_FROM_SB(x) info.x = mddev->sb->disks[nr].x
static int get_disk_info(mddev_t * mddev, void * arg)
{
	mdu_disk_info_t info;
	unsigned int nr;

	if (!mddev->sb)
		return -EINVAL;

	if (md_copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	nr = info.number;
	if (nr >= MD_SB_DISKS)
		return -EINVAL;

	SET_FROM_SB(major);
	SET_FROM_SB(minor);
	SET_FROM_SB(raid_disk);
	SET_FROM_SB(state);

	if (md_copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}
#undef SET_FROM_SB

#define SET_SB(x) mddev->sb->disks[nr].x = info->x

static int add_new_disk(mddev_t * mddev, mdu_disk_info_t *info)
{
	int err, size, persistent;
	mdk_rdev_t *rdev;
	unsigned int nr;
	kdev_t dev;
	dev = MKDEV(info->major,info->minor);

	if (find_rdev_all(dev)) {
		printk(KERN_WARNING "md: device %s already used in a RAID array!\n",
		       partition_name(dev));
		return -EBUSY;
	}
	if (!mddev->sb) {
		/* expecting a device which has a superblock */
		err = md_import_device(dev, 1);
		if (err) {
			printk(KERN_WARNING "md: md_import_device returned %d\n", err);
			return -EINVAL;
		}
		rdev = find_rdev_all(dev);
		if (!rdev) {
			MD_BUG();
			return -EINVAL;
		}
		if (mddev->nb_dev) {
			mdk_rdev_t *rdev0 = md_list_entry(mddev->disks.next,
							mdk_rdev_t, same_set);
			if (!uuid_equal(rdev0, rdev)) {
				printk(KERN_WARNING "md: %s has different UUID to %s\n",
				       partition_name(rdev->dev), partition_name(rdev0->dev));
				export_rdev(rdev);
				return -EINVAL;
			}
			if (!sb_equal(rdev0->sb, rdev->sb)) {
				printk(KERN_WARNING "md: %s has same UUID but different superblock to %s\n",
				       partition_name(rdev->dev), partition_name(rdev0->dev));
				export_rdev(rdev);
				return -EINVAL;
			}
		}
		bind_rdev_to_array(rdev, mddev);
		return 0;
	}

	nr = info->number;
	if (nr >= mddev->sb->nr_disks) {
		MD_BUG();
		return -EINVAL;
	}


	SET_SB(number);
	SET_SB(major);
	SET_SB(minor);
	SET_SB(raid_disk);
	SET_SB(state);

	if ((info->state & (1<<MD_DISK_FAULTY))==0) {
		err = md_import_device (dev, 0);
		if (err) {
			printk(KERN_WARNING "md: error, md_import_device() returned %d\n", err);
			return -EINVAL;
		}
		rdev = find_rdev_all(dev);
		if (!rdev) {
			MD_BUG();
			return -EINVAL;
		}

		rdev->old_dev = dev;
		rdev->desc_nr = info->number;

		bind_rdev_to_array(rdev, mddev);

		persistent = !mddev->sb->not_persistent;
		if (!persistent)
			printk(KERN_INFO "md: nonpersistent superblock ...\n");

		size = calc_dev_size(dev, mddev, persistent);
		rdev->sb_offset = calc_dev_sboffset(dev, mddev, persistent);

		if (!mddev->sb->size || (mddev->sb->size > size))
			mddev->sb->size = size;
	}

	/*
	 * sync all other superblocks with the main superblock
	 */
	sync_sbs(mddev);

	return 0;
}
#undef SET_SB

static int hot_generate_error(mddev_t * mddev, kdev_t dev)
{
	struct request_queue *q;
	mdk_rdev_t *rdev;
	mdp_disk_t *disk;

	if (!mddev->pers)
		return -ENODEV;

	printk(KERN_INFO "md: trying to generate %s error in md%d ... \n",
		partition_name(dev), mdidx(mddev));

	rdev = find_rdev(mddev, dev);
	if (!rdev) {
		MD_BUG();
		return -ENXIO;
	}

	if (rdev->desc_nr == -1) {
		MD_BUG();
		return -EINVAL;
	}
	disk = &mddev->sb->disks[rdev->desc_nr];
	if (!disk_active(disk))
		return -ENODEV;

	q = blk_get_queue(rdev->dev);
	if (!q) {
		MD_BUG();
		return -ENODEV;
	}
	printk(KERN_INFO "md: okay, generating error!\n");
//	q->oneshot_error = 1; // disabled for now

	return 0;
}

static int hot_remove_disk(mddev_t * mddev, kdev_t dev)
{
	int err;
	mdk_rdev_t *rdev;
	mdp_disk_t *disk;

	if (!mddev->pers)
		return -ENODEV;

	printk(KERN_INFO "md: trying to remove %s from md%d ... \n",
		partition_name(dev), mdidx(mddev));

	if (!mddev->pers->diskop) {
		printk(KERN_WARNING "md%d: personality does not support diskops!\n",
		       mdidx(mddev));
		return -EINVAL;
	}

	rdev = find_rdev(mddev, dev);
	if (!rdev)
		return -ENXIO;

	if (rdev->desc_nr == -1) {
		MD_BUG();
		return -EINVAL;
	}
	disk = &mddev->sb->disks[rdev->desc_nr];
	if (disk_active(disk))
		goto busy;

	if (disk_removed(disk))
		return -EINVAL;

	err = mddev->pers->diskop(mddev, &disk, DISKOP_HOT_REMOVE_DISK);
	if (err == -EBUSY)
		goto busy;

	if (err) {
		MD_BUG();
		return -EINVAL;
	}

	remove_descriptor(disk, mddev->sb);
	kick_rdev_from_array(rdev);
	mddev->sb_dirty = 1;
	md_update_sb(mddev);

	return 0;
busy:
	printk(KERN_WARNING "md: cannot remove active disk %s from md%d ... \n",
		partition_name(dev), mdidx(mddev));
	return -EBUSY;
}

static int hot_add_disk(mddev_t * mddev, kdev_t dev)
{
	int i, err, persistent;
	unsigned int size;
	mdk_rdev_t *rdev;
	mdp_disk_t *disk;

	if (!mddev->pers)
		return -ENODEV;

	printk(KERN_INFO "md: trying to hot-add %s to md%d ... \n",
		partition_name(dev), mdidx(mddev));

	if (!mddev->pers->diskop) {
		printk(KERN_WARNING "md%d: personality does not support diskops!\n",
		       mdidx(mddev));
		return -EINVAL;
	}

	persistent = !mddev->sb->not_persistent;

	rdev = find_rdev(mddev, dev);
	if (rdev)
		return -EBUSY;

	err = md_import_device (dev, 0);
	if (err) {
		printk(KERN_WARNING "md: error, md_import_device() returned %d\n", err);
		return -EINVAL;
	}
	rdev = find_rdev_all(dev);
	if (!rdev) {
		MD_BUG();
		return -EINVAL;
	}
	if (rdev->faulty) {
		printk(KERN_WARNING "md: can not hot-add faulty %s disk to md%d!\n",
				partition_name(dev), mdidx(mddev));
		err = -EINVAL;
		goto abort_export;
	}
	size = calc_dev_size(dev, mddev, persistent);

	if (size < mddev->sb->size) {
		printk(KERN_WARNING "md%d: disk size %d blocks < array size %d\n",
				mdidx(mddev), size, mddev->sb->size);
		err = -ENOSPC;
		goto abort_export;
	}
	bind_rdev_to_array(rdev, mddev);

	/*
	 * The rest should better be atomic, we can have disk failures
	 * noticed in interrupt contexts ...
	 */
	rdev->old_dev = dev;
	rdev->size = size;
	rdev->sb_offset = calc_dev_sboffset(dev, mddev, persistent);

	disk = mddev->sb->disks + mddev->sb->raid_disks;
	for (i = mddev->sb->raid_disks; i < MD_SB_DISKS; i++) {
		disk = mddev->sb->disks + i;

		if (!disk->major && !disk->minor)
			break;
		if (disk_removed(disk))
			break;
	}
	if (i == MD_SB_DISKS) {
		printk(KERN_WARNING "md%d: can not hot-add to full array!\n",
		       mdidx(mddev));
		err = -EBUSY;
		goto abort_unbind_export;
	}

	if (disk_removed(disk)) {
		/*
		 * reuse slot
		 */
		if (disk->number != i) {
			MD_BUG();
			err = -EINVAL;
			goto abort_unbind_export;
		}
	} else {
		disk->number = i;
	}

	disk->raid_disk = disk->number;
	disk->major = MAJOR(dev);
	disk->minor = MINOR(dev);

	if (mddev->pers->diskop(mddev, &disk, DISKOP_HOT_ADD_DISK)) {
		MD_BUG();
		err = -EINVAL;
		goto abort_unbind_export;
	}

	mark_disk_spare(disk);
	mddev->sb->nr_disks++;
	mddev->sb->spare_disks++;
	mddev->sb->working_disks++;

	mddev->sb_dirty = 1;
	md_update_sb(mddev);

	/*
	 * Kick recovery, maybe this spare has to be added to the
	 * array immediately.
	 */
	md_recover_arrays();

	return 0;

abort_unbind_export:
	unbind_rdev_from_array(rdev);

abort_export:
	export_rdev(rdev);
	return err;
}

#define SET_SB(x) mddev->sb->x = info->x
static int set_array_info(mddev_t * mddev, mdu_array_info_t *info)
{

	if (alloc_array_sb(mddev))
		return -ENOMEM;

	mddev->sb->major_version = MD_MAJOR_VERSION;
	mddev->sb->minor_version = MD_MINOR_VERSION;
	mddev->sb->patch_version = MD_PATCHLEVEL_VERSION;
	mddev->sb->ctime = CURRENT_TIME;

	SET_SB(level);
	SET_SB(size);
	SET_SB(nr_disks);
	SET_SB(raid_disks);
	SET_SB(md_minor);
	SET_SB(not_persistent);

	SET_SB(state);
	SET_SB(active_disks);
	SET_SB(working_disks);
	SET_SB(failed_disks);
	SET_SB(spare_disks);

	SET_SB(layout);
	SET_SB(chunk_size);

	mddev->sb->md_magic = MD_SB_MAGIC;

	/*
	 * Generate a 128 bit UUID
	 */
	get_random_bytes(&mddev->sb->set_uuid0, 4);
	get_random_bytes(&mddev->sb->set_uuid1, 4);
	get_random_bytes(&mddev->sb->set_uuid2, 4);
	get_random_bytes(&mddev->sb->set_uuid3, 4);

	return 0;
}
#undef SET_SB

static int set_disk_info(mddev_t * mddev, void * arg)
{
	printk(KERN_INFO "md: not yet");
	return -EINVAL;
}

static int clear_array(mddev_t * mddev)
{
	printk(KERN_INFO "md: not yet");
	return -EINVAL;
}

static int write_raid_info(mddev_t * mddev)
{
	printk(KERN_INFO "md: not yet");
	return -EINVAL;
}

static int protect_array(mddev_t * mddev)
{
	printk(KERN_INFO "md: not yet");
	return -EINVAL;
}

static int unprotect_array(mddev_t * mddev)
{
	printk(KERN_INFO "md: not yet");
	return -EINVAL;
}

static int set_disk_faulty(mddev_t *mddev, kdev_t dev)
{
	int ret;

	ret = md_error(mddev, dev);
	return ret;
}

static int md_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	int err = 0;
	struct hd_geometry *loc = (struct hd_geometry *) arg;
	mddev_t *mddev = NULL;
	kdev_t dev;

	if (!md_capable_admin())
		return -EACCES;

	dev = inode->i_rdev;
	minor = MINOR(dev);
	if (minor >= MAX_MD_DEVS) {
		MD_BUG();
		return -EINVAL;
	}

	/*
	 * Commands dealing with the RAID driver but not any
	 * particular array:
	 */
	switch (cmd)
	{
		case RAID_VERSION:
			err = get_version((void *)arg);
			goto done;

		case PRINT_RAID_DEBUG:
			err = 0;
			md_print_devices();
			goto done_unlock;

#ifndef MODULE
		case RAID_AUTORUN:
			err = 0;
			autostart_arrays();
			goto done;
#endif

		case BLKGETSIZE:
		case BLKGETSIZE64:
		case BLKRAGET:
		case BLKRASET:
		case BLKFLSBUF:
		case BLKSSZGET:
		case BLKBSZGET:
		case BLKBSZSET:
			err = blk_ioctl (dev, cmd, arg);
			goto abort;

		default:;
	}

	/*
	 * Commands creating/starting a new array:
	 */

	mddev = kdev_to_mddev(dev);

	switch (cmd)
	{
		case SET_ARRAY_INFO:
		case START_ARRAY:
			if (mddev) {
				printk(KERN_WARNING "md: array md%d already exists!\n",
								mdidx(mddev));
				err = -EEXIST;
				goto abort;
			}
		default:;
	}
	switch (cmd)
	{
		case SET_ARRAY_INFO:
			mddev = alloc_mddev(dev);
			if (!mddev) {
				err = -ENOMEM;
				goto abort;
			}
			atomic_inc(&mddev->active);

			/*
			 * alloc_mddev() should possibly self-lock.
			 */
			err = lock_mddev(mddev);
			if (err) {
				printk(KERN_WARNING "md: ioctl, reason %d, cmd %d\n",
				       err, cmd);
				goto abort;
			}

			if (mddev->sb) {
				printk(KERN_WARNING "md: array md%d already has a superblock!\n",
					mdidx(mddev));
				err = -EBUSY;
				goto abort_unlock;
			}
			if (arg) {
				mdu_array_info_t info;
				if (md_copy_from_user(&info, (void*)arg, sizeof(info))) {
					err = -EFAULT;
					goto abort_unlock;
				}
				err = set_array_info(mddev, &info);
				if (err) {
					printk(KERN_WARNING "md: couldnt set array info. %d\n", err);
					goto abort_unlock;
				}
			}
			goto done_unlock;

		case START_ARRAY:
			/*
			 * possibly make it lock the array ...
			 */
			err = autostart_array((kdev_t)arg, dev);
			if (err) {
				printk(KERN_WARNING "md: autostart %s failed!\n",
					partition_name((kdev_t)arg));
				goto abort;
			}
			goto done;

		default:;
	}

	/*
	 * Commands querying/configuring an existing array:
	 */

	if (!mddev) {
		err = -ENODEV;
		goto abort;
	}
	err = lock_mddev(mddev);
	if (err) {
		printk(KERN_INFO "md: ioctl lock interrupted, reason %d, cmd %d\n",err, cmd);
		goto abort;
	}
	/* if we don't have a superblock yet, only ADD_NEW_DISK or STOP_ARRAY is allowed */
	if (!mddev->sb && cmd != ADD_NEW_DISK && cmd != STOP_ARRAY && cmd != RUN_ARRAY) {
		err = -ENODEV;
		goto abort_unlock;
	}

	/*
	 * Commands even a read-only array can execute:
	 */
	switch (cmd)
	{
		case GET_ARRAY_INFO:
			err = get_array_info(mddev, (void *)arg);
			goto done_unlock;

		case GET_DISK_INFO:
			err = get_disk_info(mddev, (void *)arg);
			goto done_unlock;

		case RESTART_ARRAY_RW:
			err = restart_array(mddev);
			goto done_unlock;

		case STOP_ARRAY:
			if (inode->i_bdev->bd_openers > 1)
				err = -EBUSY;
			else if (!(err = do_md_stop (mddev, 0)))
				mddev = NULL;
			goto done_unlock;

		case STOP_ARRAY_RO:
			if (inode->i_bdev->bd_openers > 1)
				err = -EBUSY;
			else 
				err = do_md_stop (mddev, 1);
			goto done_unlock;

	/*
	 * We have a problem here : there is no easy way to give a CHS
	 * virtual geometry. We currently pretend that we have a 2 heads
	 * 4 sectors (with a BIG number of cylinders...). This drives
	 * dosfs just mad... ;-)
	 */
		case HDIO_GETGEO:
			if (!loc) {
				err = -EINVAL;
				goto abort_unlock;
			}
			err = md_put_user (2, (char *) &loc->heads);
			if (err)
				goto abort_unlock;
			err = md_put_user (4, (char *) &loc->sectors);
			if (err)
				goto abort_unlock;
			err = md_put_user (md_hd_struct[mdidx(mddev)].nr_sects/8,
						(short *) &loc->cylinders);
			if (err)
				goto abort_unlock;
			err = md_put_user (md_hd_struct[minor].start_sect,
						(long *) &loc->start);
			goto done_unlock;
	}

	/*
	 * The remaining ioctls are changing the state of the
	 * superblock, so we do not allow read-only arrays
	 * here:
	 */
	if (mddev->ro) {
		err = -EROFS;
		goto abort_unlock;
	}

	switch (cmd)
	{
		case CLEAR_ARRAY:
			err = clear_array(mddev);
			goto done_unlock;

		case ADD_NEW_DISK:
		{
			mdu_disk_info_t info;
			if (md_copy_from_user(&info, (void*)arg, sizeof(info)))
				err = -EFAULT;
			else
				err = add_new_disk(mddev, &info);
			goto done_unlock;
		}
		case HOT_GENERATE_ERROR:
			err = hot_generate_error(mddev, (kdev_t)arg);
			goto done_unlock;
		case HOT_REMOVE_DISK:
			err = hot_remove_disk(mddev, (kdev_t)arg);
			goto done_unlock;

		case HOT_ADD_DISK:
			err = hot_add_disk(mddev, (kdev_t)arg);
			goto done_unlock;

		case SET_DISK_INFO:
			err = set_disk_info(mddev, (void *)arg);
			goto done_unlock;

		case WRITE_RAID_INFO:
			err = write_raid_info(mddev);
			goto done_unlock;

		case UNPROTECT_ARRAY:
			err = unprotect_array(mddev);
			goto done_unlock;

		case PROTECT_ARRAY:
			err = protect_array(mddev);
			goto done_unlock;

		case SET_DISK_FAULTY:
			err = set_disk_faulty(mddev, (kdev_t)arg);
			goto done_unlock;

		case RUN_ARRAY:
		{
/* The data is never used....
			mdu_param_t param;
			err = md_copy_from_user(&param, (mdu_param_t *)arg,
							 sizeof(param));
			if (err)
				goto abort_unlock;
*/
			err = do_md_run (mddev);
			/*
			 * we have to clean up the mess if
			 * the array cannot be run for some
			 * reason ...
			 */
			if (err) {
				mddev->sb_dirty = 0;
				if (!do_md_stop (mddev, 0))
					mddev = NULL;
			}
			goto done_unlock;
		}

		default:
			printk(KERN_WARNING "md: %s(pid %d) used obsolete MD ioctl, "
			       "upgrade your software to use new ictls.\n",
			       current->comm, current->pid);
			err = -EINVAL;
			goto abort_unlock;
	}

done_unlock:
abort_unlock:
	if (mddev)
		unlock_mddev(mddev);

	return err;
done:
	if (err)
		MD_BUG();
abort:
	return err;
}

static int md_open(struct inode *inode, struct file *file)
{
	/*
	 * Always succeed, but increment the usage count
	 */
	mddev_t *mddev = kdev_to_mddev(inode->i_rdev);
	if (mddev)
		atomic_inc(&mddev->active);
	return (0);
}

static int md_release(struct inode *inode, struct file * file)
{
	mddev_t *mddev = kdev_to_mddev(inode->i_rdev);
	if (mddev)
		atomic_dec(&mddev->active);
	return 0;
}

static struct block_device_operations md_fops=
{
	owner:		THIS_MODULE,
	open:		md_open,
	release:	md_release,
	ioctl:		md_ioctl,
};


int md_thread(void * arg)
{
	mdk_thread_t *thread = arg;

	md_lock_kernel();

	/*
	 * Detach thread
	 */

	daemonize();

	sprintf(current->comm, thread->name);
	md_init_signals();
	md_flush_signals();
	thread->tsk = current;

	/*
	 * md_thread is a 'system-thread', it's priority should be very
	 * high. We avoid resource deadlocks individually in each
	 * raid personality. (RAID5 does preallocation) We also use RR and
	 * the very same RT priority as kswapd, thus we will never get
	 * into a priority inversion deadlock.
	 *
	 * we definitely have to have equal or higher priority than
	 * bdflush, otherwise bdflush will deadlock if there are too
	 * many dirty RAID5 blocks.
	 */
	current->policy = SCHED_OTHER;
	current->nice = -20;
	md_unlock_kernel();

	complete(thread->event);
	while (thread->run) {
		void (*run)(void *data);
		DECLARE_WAITQUEUE(wait, current);

		add_wait_queue(&thread->wqueue, &wait);
		set_task_state(current, TASK_INTERRUPTIBLE);
		if (!test_bit(THREAD_WAKEUP, &thread->flags)) {
			dprintk("md: thread %p went to sleep.\n", thread);
			schedule();
			dprintk("md: thread %p woke up.\n", thread);
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&thread->wqueue, &wait);
		clear_bit(THREAD_WAKEUP, &thread->flags);

		run = thread->run;
		if (run) {
			run(thread->data);
			run_task_queue(&tq_disk);
		}
		if (md_signal_pending(current))
			md_flush_signals();
	}
	complete(thread->event);
	return 0;
}

void md_wakeup_thread(mdk_thread_t *thread)
{
	dprintk("md: waking up MD thread %p.\n", thread);
	set_bit(THREAD_WAKEUP, &thread->flags);
	wake_up(&thread->wqueue);
}

mdk_thread_t *md_register_thread(void (*run) (void *),
						void *data, const char *name)
{
	mdk_thread_t *thread;
	int ret;
	struct completion event;

	thread = (mdk_thread_t *) kmalloc
				(sizeof(mdk_thread_t), GFP_KERNEL);
	if (!thread)
		return NULL;

	memset(thread, 0, sizeof(mdk_thread_t));
	md_init_waitqueue_head(&thread->wqueue);

	init_completion(&event);
	thread->event = &event;
	thread->run = run;
	thread->data = data;
	thread->name = name;
	ret = kernel_thread(md_thread, thread, 0);
	if (ret < 0) {
		kfree(thread);
		return NULL;
	}
	wait_for_completion(&event);
	return thread;
}

void md_interrupt_thread(mdk_thread_t *thread)
{
	if (!thread->tsk) {
		MD_BUG();
		return;
	}
	dprintk("interrupting MD-thread pid %d\n", thread->tsk->pid);
	send_sig(SIGKILL, thread->tsk, 1);
}

void md_unregister_thread(mdk_thread_t *thread)
{
	struct completion event;

	init_completion(&event);

	thread->event = &event;
	thread->run = NULL;
	thread->name = NULL;
	md_interrupt_thread(thread);
	wait_for_completion(&event);
	kfree(thread);
}

void md_recover_arrays(void)
{
	if (!md_recovery_thread) {
		MD_BUG();
		return;
	}
	md_wakeup_thread(md_recovery_thread);
}


int md_error(mddev_t *mddev, kdev_t rdev)
{
	mdk_rdev_t * rrdev;

	dprintk("md_error dev:(%d:%d), rdev:(%d:%d), (caller: %p,%p,%p,%p).\n",
		MAJOR(dev),MINOR(dev),MAJOR(rdev),MINOR(rdev),
		__builtin_return_address(0),__builtin_return_address(1),
		__builtin_return_address(2),__builtin_return_address(3));

	if (!mddev) {
		MD_BUG();
		return 0;
	}
	rrdev = find_rdev(mddev, rdev);
	if (!rrdev || rrdev->faulty)
		return 0;
	if (!mddev->pers->error_handler
			|| mddev->pers->error_handler(mddev,rdev) <= 0) {
		rrdev->faulty = 1;
	} else
		return 1;
	/*
	 * if recovery was running, stop it now.
	 */
	if (mddev->pers->stop_resync)
		mddev->pers->stop_resync(mddev);
	if (mddev->recovery_running)
		md_interrupt_thread(md_recovery_thread);
	md_recover_arrays();

	return 0;
}

static void status_unused(struct seq_file *seq)
{
	int i = 0;
	mdk_rdev_t *rdev;
	struct md_list_head *tmp;

	seq_printf(seq, "unused devices: ");

	ITERATE_RDEV_ALL(rdev,tmp) {
		if (!rdev->same_set.next && !rdev->same_set.prev) {
			/*
			 * The device is not yet used by any array.
			 */
			i++;
			seq_printf(seq, "%s ",
				partition_name(rdev->dev));
		}
	}
	if (!i)
		seq_printf(seq, "<none>");

	seq_printf(seq, "\n");
}


static void status_resync(struct seq_file *seq, mddev_t * mddev)
{
	unsigned long max_blocks, resync, res, dt, db, rt;

	resync = (mddev->curr_resync - atomic_read(&mddev->recovery_active))/2;
	max_blocks = mddev->sb->size;

	/*
	 * Should not happen.
	 */
	if (!max_blocks)
		MD_BUG();

	res = (resync/1024)*1000/(max_blocks/1024 + 1);
	{
		int i, x = res/50, y = 20-x;
		seq_printf(seq, "[");
		for (i = 0; i < x; i++)
			seq_printf(seq, "=");
		seq_printf(seq, ">");
		for (i = 0; i < y; i++)
			seq_printf(seq, ".");
		seq_printf(seq, "] ");
	}
	if (!mddev->recovery_running)
		/*
		 * true resync
		 */
		seq_printf(seq, " resync =%3lu.%lu%% (%lu/%lu)",
				res/10, res % 10, resync, max_blocks);
	else
		/*
		 * recovery ...
		 */
		seq_printf(seq, " recovery =%3lu.%lu%% (%lu/%lu)",
				res/10, res % 10, resync, max_blocks);

	/*
	 * We do not want to overflow, so the order of operands and
	 * the * 100 / 100 trick are important. We do a +1 to be
	 * safe against division by zero. We only estimate anyway.
	 *
	 * dt: time from mark until now
	 * db: blocks written from mark until now
	 * rt: remaining time
	 */
	dt = ((jiffies - mddev->resync_mark) / HZ);
	if (!dt) dt++;
	db = resync - (mddev->resync_mark_cnt/2);
	rt = (dt * ((max_blocks-resync) / (db/100+1)))/100;

	seq_printf(seq, " finish=%lu.%lumin", rt / 60, (rt % 60)/6);

	seq_printf(seq, " speed=%ldK/sec", db/dt);

}


static void *md_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct list_head *tmp;
	loff_t l = *pos;
	mddev_t *mddev;

	if (l >= 0x10000)
		return NULL;
	if (!l--)
		/* header */
		return (void*)1;

	list_for_each(tmp,&all_mddevs)
		if (!l--) {
			mddev = list_entry(tmp, mddev_t, all_mddevs);
			return mddev;
		}
	if (!l--)	
		return (void*)2;/* tail */
	return NULL;
}

static void *md_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct list_head *tmp;
	mddev_t *next_mddev, *mddev = v;
	
	++*pos;
	if (v == (void*)2)
		return NULL;

	if (v == (void*)1)
		tmp = all_mddevs.next;
	else
		tmp = mddev->all_mddevs.next;
	if (tmp != &all_mddevs)
		next_mddev = list_entry(tmp,mddev_t,all_mddevs);
	else {
		next_mddev = (void*)2;
		*pos = 0x10000;
	}		

	return next_mddev;

}

static void md_seq_stop(struct seq_file *seq, void *v)
{

}

static int md_seq_show(struct seq_file *seq, void *v)
{
	int j, size;
	struct md_list_head *tmp2;
	mdk_rdev_t *rdev;
	mddev_t *mddev = v;

	if (v == (void*)1) {
		seq_printf(seq, "Personalities : ");
		for (j = 0; j < MAX_PERSONALITY; j++)
			if (pers[j])
				seq_printf(seq, "[%s] ", pers[j]->name);

		seq_printf(seq, "\n");
		seq_printf(seq, "read_ahead ");
		if (read_ahead[MD_MAJOR] == INT_MAX)
			seq_printf(seq, "not set\n");
		else
			seq_printf(seq, "%d sectors\n", read_ahead[MD_MAJOR]);
		return 0;
	}
	if (v == (void*)2) {
		status_unused(seq);
		return 0;
	}

	seq_printf(seq, "md%d : %sactive", mdidx(mddev),
		   mddev->pers ? "" : "in");
	if (mddev->pers) {
		if (mddev->ro)
			seq_printf(seq, " (read-only)");
		seq_printf(seq, " %s", mddev->pers->name);
	}
	
	size = 0;
	ITERATE_RDEV(mddev,rdev,tmp2) {
		seq_printf(seq, " %s[%d]",
			   partition_name(rdev->dev), rdev->desc_nr);
		if (rdev->faulty) {
			seq_printf(seq, "(F)");
			continue;
		}
		size += rdev->size;
	}

	if (mddev->nb_dev) {
		if (mddev->pers)
			seq_printf(seq, "\n      %d blocks",
				   md_size[mdidx(mddev)]);
		else
			seq_printf(seq, "\n      %d blocks", size);
	}

	if (mddev->pers) {

		mddev->pers->status (seq, mddev);

		seq_printf(seq, "\n      ");
		if (mddev->curr_resync) {
			status_resync (seq, mddev);
		} else {
			if (sem_getcount(&mddev->resync_sem) != 1)
				seq_printf(seq, "	resync=DELAYED");
		}
	}
	seq_printf(seq, "\n");

	return 0;
}

  
static struct seq_operations md_seq_ops = {
	.start  = md_seq_start,
	.next   = md_seq_next,
	.stop   = md_seq_stop,
	.show   = md_seq_show,
};

static int md_seq_open(struct inode *inode, struct file *file)
{
	int error;

	error = seq_open(file, &md_seq_ops);
	return error;
}

static struct file_operations md_seq_fops = {
	.open           = md_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= seq_release,
};


int register_md_personality(int pnum, mdk_personality_t *p)
{
	if (pnum >= MAX_PERSONALITY) {
		MD_BUG();
		return -EINVAL;
	}

	if (pers[pnum]) {
		MD_BUG();
		return -EBUSY;
	}

	pers[pnum] = p;
	printk(KERN_INFO "md: %s personality registered as nr %d\n", p->name, pnum);
	return 0;
}

int unregister_md_personality(int pnum)
{
	if (pnum >= MAX_PERSONALITY) {
		MD_BUG();
		return -EINVAL;
	}

	printk(KERN_INFO "md: %s personality unregistered\n", pers[pnum]->name);
	pers[pnum] = NULL;
	return 0;
}

mdp_disk_t *get_spare(mddev_t *mddev)
{
	mdp_super_t *sb = mddev->sb;
	mdp_disk_t *disk;
	mdk_rdev_t *rdev;
	struct md_list_head *tmp;

	ITERATE_RDEV(mddev,rdev,tmp) {
		if (rdev->faulty)
			continue;
		if (!rdev->sb) {
			MD_BUG();
			continue;
		}
		disk = &sb->disks[rdev->desc_nr];
		if (disk_faulty(disk)) {
			MD_BUG();
			continue;
		}
		if (disk_active(disk))
			continue;
		return disk;
	}
	return NULL;
}

static unsigned int sync_io[DK_MAX_MAJOR][DK_MAX_DISK];
void md_sync_acct(kdev_t dev, unsigned long nr_sectors)
{
	unsigned int major = MAJOR(dev);
	unsigned int index;

	index = disk_index(dev);
	if ((index >= DK_MAX_DISK) || (major >= DK_MAX_MAJOR))
		return;

	sync_io[major][index] += nr_sectors;
}

static int is_mddev_idle(mddev_t *mddev)
{
	mdk_rdev_t * rdev;
	struct md_list_head *tmp;
	int idle;
	unsigned long curr_events;

	idle = 1;
	ITERATE_RDEV(mddev,rdev,tmp) {
		int major = MAJOR(rdev->dev);
		int idx = disk_index(rdev->dev);

		if ((idx >= DK_MAX_DISK) || (major >= DK_MAX_MAJOR))
			continue;

		curr_events = kstat.dk_drive_rblk[major][idx] +
						kstat.dk_drive_wblk[major][idx] ;
		curr_events -= sync_io[major][idx];
		if ((curr_events - rdev->last_events) > 32) {
			rdev->last_events = curr_events;
			idle = 0;
		}
	}
	return idle;
}

MD_DECLARE_WAIT_QUEUE_HEAD(resync_wait);

void md_done_sync(mddev_t *mddev, int blocks, int ok)
{
	/* another "blocks" (512byte) blocks have been synced */
	atomic_sub(blocks, &mddev->recovery_active);
	wake_up(&mddev->recovery_wait);
	if (!ok) {
		// stop recovery, signal do_sync ....
		if (mddev->pers->stop_resync)
			mddev->pers->stop_resync(mddev);
		if (mddev->recovery_running)
			md_interrupt_thread(md_recovery_thread);
	}
}

#define SYNC_MARKS	10
#define	SYNC_MARK_STEP	(3*HZ)
int md_do_sync(mddev_t *mddev, mdp_disk_t *spare)
{
	mddev_t *mddev2;
	unsigned int max_sectors, currspeed,
		j, window, err, serialize;
	unsigned long mark[SYNC_MARKS];
	unsigned long mark_cnt[SYNC_MARKS];
	int last_mark,m;
	struct md_list_head *tmp;
	unsigned long last_check;


	err = down_interruptible(&mddev->resync_sem);
	if (err)
		goto out_nolock;

recheck:
	serialize = 0;
	ITERATE_MDDEV(mddev2,tmp) {
		if (mddev2 == mddev)
			continue;
		if (mddev2->curr_resync && match_mddev_units(mddev,mddev2)) {
			printk(KERN_INFO "md: delaying resync of md%d until md%d "
			       "has finished resync (they share one or more physical units)\n",
			       mdidx(mddev), mdidx(mddev2));
			serialize = 1;
			break;
		}
	}
	if (serialize) {
		interruptible_sleep_on(&resync_wait);
		if (md_signal_pending(current)) {
			md_flush_signals();
			err = -EINTR;
			goto out;
		}
		goto recheck;
	}

	mddev->curr_resync = 1;

	max_sectors = mddev->sb->size<<1;

	printk(KERN_INFO "md: syncing RAID array md%d\n", mdidx(mddev));
	printk(KERN_INFO "md: minimum _guaranteed_ reconstruction speed: %d KB/sec/disc.\n",
						sysctl_speed_limit_min);
	printk(KERN_INFO "md: using maximum available idle IO bandwith "
	       "(but not more than %d KB/sec) for reconstruction.\n",
	       sysctl_speed_limit_max);

	/*
	 * Resync has low priority.
	 */
	current->nice = 19;

	is_mddev_idle(mddev); /* this also initializes IO event counters */
	for (m = 0; m < SYNC_MARKS; m++) {
		mark[m] = jiffies;
		mark_cnt[m] = 0;
	}
	last_mark = 0;
	mddev->resync_mark = mark[last_mark];
	mddev->resync_mark_cnt = mark_cnt[last_mark];

	/*
	 * Tune reconstruction:
	 */
	window = vm_max_readahead*(PAGE_SIZE/512);
	printk(KERN_INFO "md: using %dk window, over a total of %d blocks.\n",
	       window/2,max_sectors/2);

	atomic_set(&mddev->recovery_active, 0);
	init_waitqueue_head(&mddev->recovery_wait);
	last_check = 0;
	for (j = 0; j < max_sectors;) {
		int sectors;

		sectors = mddev->pers->sync_request(mddev, j);

		if (sectors < 0) {
			err = sectors;
			goto out;
		}
		atomic_add(sectors, &mddev->recovery_active);
		j += sectors;
		mddev->curr_resync = j;

		if (last_check + window > j)
			continue;

		last_check = j;

		run_task_queue(&tq_disk);

	repeat:
		if (jiffies >= mark[last_mark] + SYNC_MARK_STEP ) {
			/* step marks */
			int next = (last_mark+1) % SYNC_MARKS;

			mddev->resync_mark = mark[next];
			mddev->resync_mark_cnt = mark_cnt[next];
			mark[next] = jiffies;
			mark_cnt[next] = j - atomic_read(&mddev->recovery_active);
			last_mark = next;
		}


		if (md_signal_pending(current)) {
			/*
			 * got a signal, exit.
			 */
			mddev->curr_resync = 0;
			printk(KERN_INFO "md: md_do_sync() got signal ... exiting\n");
			md_flush_signals();
			err = -EINTR;
			goto out;
		}

		/*
		 * this loop exits only if either when we are slower than
		 * the 'hard' speed limit, or the system was IO-idle for
		 * a jiffy.
		 * the system might be non-idle CPU-wise, but we only care
		 * about not overloading the IO subsystem. (things like an
		 * e2fsck being done on the RAID array should execute fast)
		 */
		if (md_need_resched(current))
			schedule();

		currspeed = (j-mddev->resync_mark_cnt)/2/((jiffies-mddev->resync_mark)/HZ +1) +1;

		if (currspeed > sysctl_speed_limit_min) {
			current->nice = 19;

			if ((currspeed > sysctl_speed_limit_max) ||
					!is_mddev_idle(mddev)) {
				current->state = TASK_INTERRUPTIBLE;
				md_schedule_timeout(HZ/4);
				goto repeat;
			}
		} else
			current->nice = -20;
	}
	printk(KERN_INFO "md: md%d: sync done.\n",mdidx(mddev));
	err = 0;
	/*
	 * this also signals 'finished resyncing' to md_stop
	 */
out:
	wait_disk_event(mddev->recovery_wait, atomic_read(&mddev->recovery_active)==0);
	up(&mddev->resync_sem);
out_nolock:
	mddev->curr_resync = 0;
	wake_up(&resync_wait);
	return err;
}


/*
 * This is a kernel thread which syncs a spare disk with the active array
 *
 * the amount of foolproofing might seem to be a tad excessive, but an
 * early (not so error-safe) version of raid1syncd synced the first 0.5 gigs
 * of my root partition with the first 0.5 gigs of my /home partition ... so
 * i'm a bit nervous ;)
 */
void md_do_recovery(void *data)
{
	int err;
	mddev_t *mddev;
	mdp_super_t *sb;
	mdp_disk_t *spare;
	struct md_list_head *tmp;

	printk(KERN_INFO "md: recovery thread got woken up ...\n");
restart:
	ITERATE_MDDEV(mddev,tmp) {
		sb = mddev->sb;
		if (!sb)
			continue;
		if (mddev->recovery_running)
			continue;
		if (sb->active_disks == sb->raid_disks)
			continue;
		if (mddev->sb_dirty)
			md_update_sb(mddev);
		if (!sb->spare_disks) {
			printk(KERN_ERR "md%d: no spare disk to reconstruct array! "
			       "-- continuing in degraded mode\n", mdidx(mddev));
			continue;
		}
		/*
		 * now here we get the spare and resync it.
		 */
		spare = get_spare(mddev);
		if (!spare)
			continue;
		printk(KERN_INFO "md%d: resyncing spare disk %s to replace failed disk\n",
		       mdidx(mddev), partition_name(MKDEV(spare->major,spare->minor)));
		if (!mddev->pers->diskop)
			continue;
		if (mddev->pers->diskop(mddev, &spare, DISKOP_SPARE_WRITE))
			continue;
		down(&mddev->recovery_sem);
		mddev->recovery_running = 1;
		err = md_do_sync(mddev, spare);
		if (err == -EIO) {
			printk(KERN_INFO "md%d: spare disk %s failed, skipping to next spare.\n",
			       mdidx(mddev), partition_name(MKDEV(spare->major,spare->minor)));
			if (!disk_faulty(spare)) {
				mddev->pers->diskop(mddev,&spare,DISKOP_SPARE_INACTIVE);
				mark_disk_faulty(spare);
				mark_disk_nonsync(spare);
				mark_disk_inactive(spare);
				sb->spare_disks--;
				sb->working_disks--;
				sb->failed_disks++;
			}
		} else
			if (disk_faulty(spare))
				mddev->pers->diskop(mddev, &spare,
						DISKOP_SPARE_INACTIVE);
		if (err == -EINTR || err == -ENOMEM) {
			/*
			 * Recovery got interrupted, or ran out of mem ...
			 * signal back that we have finished using the array.
			 */
			mddev->pers->diskop(mddev, &spare,
							 DISKOP_SPARE_INACTIVE);
			up(&mddev->recovery_sem);
			mddev->recovery_running = 0;
			continue;
		} else {
			mddev->recovery_running = 0;
			up(&mddev->recovery_sem);
		}
		if (!disk_faulty(spare)) {
			/*
			 * the SPARE_ACTIVE diskop possibly changes the
			 * pointer too
			 */
			mddev->pers->diskop(mddev, &spare, DISKOP_SPARE_ACTIVE);
			mark_disk_sync(spare);
			mark_disk_active(spare);
			sb->active_disks++;
			sb->spare_disks--;
		}
		mddev->sb_dirty = 1;
		md_update_sb(mddev);
		goto restart;
	}
	printk(KERN_INFO "md: recovery thread finished ...\n");

}

int md_notify_reboot(struct notifier_block *this,
					unsigned long code, void *x)
{
	struct md_list_head *tmp;
	mddev_t *mddev;

	if ((code == MD_SYS_DOWN) || (code == MD_SYS_HALT)
					|| (code == MD_SYS_POWER_OFF)) {

		printk(KERN_INFO "md: stopping all md devices.\n");

		ITERATE_MDDEV(mddev,tmp)
			do_md_stop (mddev, 1);
		/*
		 * certain more exotic SCSI devices are known to be
		 * volatile wrt too early system reboots. While the
		 * right place to handle this issue is the given
		 * driver, we do want to have a safe RAID driver ...
		 */
		md_mdelay(1000*1);
	}
	return NOTIFY_DONE;
}

struct notifier_block md_notifier = {
	notifier_call:	md_notify_reboot,
	next:		NULL,
	priority:	INT_MAX, /* before any real devices */
};

static void md_geninit(void)
{
	struct proc_dir_entry *p;
	int i;

	for(i = 0; i < MAX_MD_DEVS; i++) {
		md_blocksizes[i] = 1024;
		md_size[i] = 0;
		md_hardsect_sizes[i] = 512;
		md_maxreadahead[i] = MD_READAHEAD;
	}
	blksize_size[MAJOR_NR] = md_blocksizes;
	blk_size[MAJOR_NR] = md_size;
	max_readahead[MAJOR_NR] = md_maxreadahead;
	hardsect_size[MAJOR_NR] = md_hardsect_sizes;

	dprintk("md: sizeof(mdp_super_t) = %d\n", (int)sizeof(mdp_super_t));

#ifdef CONFIG_PROC_FS
	p = create_proc_entry("mdstat", S_IRUGO, NULL);
	if (p)
		p->proc_fops = &md_seq_fops;
#endif
}

int md__init md_init(void)
{
	static char * name = "mdrecoveryd";
	int minor;

	printk(KERN_INFO "md: md driver %d.%d.%d MAX_MD_DEVS=%d, MD_SB_DISKS=%d\n",
			MD_MAJOR_VERSION, MD_MINOR_VERSION,
			MD_PATCHLEVEL_VERSION, MAX_MD_DEVS, MD_SB_DISKS);

	if (devfs_register_blkdev (MAJOR_NR, "md", &md_fops))
	{
		printk(KERN_ALERT "md: Unable to get major %d for md\n", MAJOR_NR);
		return (-1);
	}
	devfs_handle = devfs_mk_dir (NULL, "md", NULL);
	/* we don't use devfs_register_series because we want to fill md_hd_struct */
	for (minor=0; minor < MAX_MD_DEVS; ++minor) {
		char devname[128];
		sprintf (devname, "%u", minor);
		md_hd_struct[minor].de = devfs_register (devfs_handle,
			devname, DEVFS_FL_DEFAULT, MAJOR_NR, minor,
			S_IFBLK | S_IRUSR | S_IWUSR, &md_fops, NULL);
	}

	/* forward all md request to md_make_request */
	blk_queue_make_request(BLK_DEFAULT_QUEUE(MAJOR_NR), md_make_request);


	read_ahead[MAJOR_NR] = INT_MAX;

	add_gendisk(&md_gendisk);

	md_recovery_thread = md_register_thread(md_do_recovery, NULL, name);
	if (!md_recovery_thread)
		printk(KERN_ALERT "md: bug: couldn't allocate md_recovery_thread\n");

	md_register_reboot_notifier(&md_notifier);
	raid_table_header = register_sysctl_table(raid_root_table, 1);

	md_geninit();
	return (0);
}


#ifndef MODULE

/*
 * When md (and any require personalities) are compiled into the kernel
 * (not a module), arrays can be assembles are boot time using with AUTODETECT
 * where specially marked partitions are registered with md_autodetect_dev(),
 * and with MD_BOOT where devices to be collected are given on the boot line
 * with md=.....
 * The code for that is here.
 */

struct {
	int set;
	int noautodetect;
} raid_setup_args md__initdata;

/*
 * Searches all registered partitions for autorun RAID arrays
 * at boot time.
 */
static kdev_t detected_devices[128];
static int dev_cnt;

void md_autodetect_dev(kdev_t dev)
{
	if (dev_cnt >= 0 && dev_cnt < 127)
		detected_devices[dev_cnt++] = dev;
}


static void autostart_arrays(void)
{
	mdk_rdev_t *rdev;
	int i;

	printk(KERN_INFO "md: Autodetecting RAID arrays.\n");

	for (i = 0; i < dev_cnt; i++) {
		kdev_t dev = detected_devices[i];

		if (md_import_device(dev,1)) {
			printk(KERN_ALERT "md: could not import %s!\n",
				partition_name(dev));
			continue;
		}
		/*
		 * Sanity checks:
		 */
		rdev = find_rdev_all(dev);
		if (!rdev) {
			MD_BUG();
			continue;
		}
		if (rdev->faulty) {
			MD_BUG();
			continue;
		}
		md_list_add(&rdev->pending, &pending_raid_disks);
	}
	dev_cnt = 0;

	autorun_devices(-1);
}

static struct {
	char device_set [MAX_MD_DEVS];
	int pers[MAX_MD_DEVS];
	int chunk[MAX_MD_DEVS];
	char *device_names[MAX_MD_DEVS];
} md_setup_args md__initdata;

/*
 * Parse the command-line parameters given our kernel, but do not
 * actually try to invoke the MD device now; that is handled by
 * md_setup_drive after the low-level disk drivers have initialised.
 *
 * 27/11/1999: Fixed to work correctly with the 2.3 kernel (which
 *             assigns the task of parsing integer arguments to the
 *             invoked program now).  Added ability to initialise all
 *             the MD devices (by specifying multiple "md=" lines)
 *             instead of just one.  -- KTK
 * 18May2000: Added support for persistant-superblock arrays:
 *             md=n,0,factor,fault,device-list   uses RAID0 for device n
 *             md=n,-1,factor,fault,device-list  uses LINEAR for device n
 *             md=n,device-list      reads a RAID superblock from the devices
 *             elements in device-list are read by name_to_kdev_t so can be
 *             a hex number or something like /dev/hda1 /dev/sdb
 * 2001-06-03: Dave Cinege <dcinege@psychosis.com>
 *		Shifted name_to_kdev_t() and related operations to md_set_drive()
 *		for later execution. Rewrote section to make devfs compatible.
 */
static int md__init md_setup(char *str)
{
	int minor, level, factor, fault;
	char *pername = "";
	char *str1 = str;

	if (get_option(&str, &minor) != 2) {	/* MD Number */
		printk(KERN_WARNING "md: Too few arguments supplied to md=.\n");
		return 0;
	}
	if (minor >= MAX_MD_DEVS) {
		printk(KERN_WARNING "md: md=%d, Minor device number too high.\n", minor);
		return 0;
	} else if (md_setup_args.device_names[minor]) {
		printk(KERN_WARNING "md: md=%d, Specified more then once. "
		       "Replacing previous definition.\n", minor);
	}
	switch (get_option(&str, &level)) {	/* RAID Personality */
	case 2: /* could be 0 or -1.. */
		if (level == 0 || level == -1) {
			if (get_option(&str, &factor) != 2 ||	/* Chunk Size */
					get_option(&str, &fault) != 2) {
				printk(KERN_WARNING "md: Too few arguments supplied to md=.\n");
				return 0;
			}
			md_setup_args.pers[minor] = level;
			md_setup_args.chunk[minor] = 1 << (factor+12);
			switch(level) {
			case -1:
				level = LINEAR;
				pername = "linear";
				break;
			case 0:
				level = RAID0;
				pername = "raid0";
				break;
			default:
				printk(KERN_WARNING
				       "md: The kernel has not been configured for raid%d support!\n",
				       level);
				return 0;
			}
			md_setup_args.pers[minor] = level;
			break;
		}
		/* FALL THROUGH */
	case 1: /* the first device is numeric */
		str = str1;
		/* FALL THROUGH */
	case 0:
		md_setup_args.pers[minor] = 0;
		pername="super-block";
	}

	printk(KERN_INFO "md: Will configure md%d (%s) from %s, below.\n",
		minor, pername, str);
	md_setup_args.device_names[minor] = str;

	return 1;
}

extern kdev_t name_to_kdev_t(char *line) md__init;
void md__init md_setup_drive(void)
{
	int minor, i;
	kdev_t dev;
	mddev_t*mddev;
	kdev_t devices[MD_SB_DISKS+1];

	for (minor = 0; minor < MAX_MD_DEVS; minor++) {
		int err = 0;
		char *devname;
		mdu_disk_info_t dinfo;

		if ((devname = md_setup_args.device_names[minor]) == 0)	continue;

		for (i = 0; i < MD_SB_DISKS && devname != 0; i++) {

			char *p;
			void *handle;

			p = strchr(devname, ',');
			if (p)
				*p++ = 0;

			dev = name_to_kdev_t(devname);
			handle = devfs_find_handle(NULL, devname, MAJOR (dev), MINOR (dev),
							DEVFS_SPECIAL_BLK, 1);
			if (handle != 0) {
				unsigned major, minor;
				devfs_get_maj_min(handle, &major, &minor);
				dev = MKDEV(major, minor);
			}
			if (dev == 0) {
				printk(KERN_WARNING "md: Unknown device name: %s\n", devname);
				break;
			}

			devices[i] = dev;
			md_setup_args.device_set[minor] = 1;

			devname = p;
		}
		devices[i] = 0;

		if (md_setup_args.device_set[minor] == 0)
			continue;

		if (mddev_map[minor].mddev) {
			printk(KERN_WARNING
			       "md: Ignoring md=%d, already autodetected. (Use raid=noautodetect)\n",
			       minor);
			continue;
		}
		printk(KERN_INFO "md: Loading md%d: %s\n", minor, md_setup_args.device_names[minor]);

		mddev = alloc_mddev(MKDEV(MD_MAJOR,minor));
		if (!mddev) {
			printk(KERN_ERR "md: kmalloc failed - cannot start array %d\n", minor);
			continue;
		}
		if (md_setup_args.pers[minor]) {
			/* non-persistent */
			mdu_array_info_t ainfo;
			ainfo.level = pers_to_level(md_setup_args.pers[minor]);
			ainfo.size = 0;
			ainfo.nr_disks =0;
			ainfo.raid_disks =0;
			ainfo.md_minor =minor;
			ainfo.not_persistent = 1;

			ainfo.state = (1 << MD_SB_CLEAN);
			ainfo.active_disks = 0;
			ainfo.working_disks = 0;
			ainfo.failed_disks = 0;
			ainfo.spare_disks = 0;
			ainfo.layout = 0;
			ainfo.chunk_size = md_setup_args.chunk[minor];
			err = set_array_info(mddev, &ainfo);
			for (i = 0; !err && (dev = devices[i]); i++) {
				dinfo.number = i;
				dinfo.raid_disk = i;
				dinfo.state = (1<<MD_DISK_ACTIVE)|(1<<MD_DISK_SYNC);
				dinfo.major = MAJOR(dev);
				dinfo.minor = MINOR(dev);
				mddev->sb->nr_disks++;
				mddev->sb->raid_disks++;
				mddev->sb->active_disks++;
				mddev->sb->working_disks++;
				err = add_new_disk (mddev, &dinfo);
			}
		} else {
			/* persistent */
			for (i = 0; (dev = devices[i]); i++) {
				dinfo.major = MAJOR(dev);
				dinfo.minor = MINOR(dev);
				add_new_disk (mddev, &dinfo);
			}
		}
		if (!err)
			err = do_md_run(mddev);
		if (err) {
			mddev->sb_dirty = 0;
			do_md_stop(mddev, 0);
			printk(KERN_WARNING "md: starting md%d failed\n", minor);
		}
	}
}

static int md__init raid_setup(char *str)
{
	int len, pos;

	len = strlen(str) + 1;
	pos = 0;

	while (pos < len) {
		char *comma = strchr(str+pos, ',');
		int wlen;
		if (comma)
			wlen = (comma-str)-pos;
		else	wlen = (len-1)-pos;

		if (strncmp(str, "noautodetect", wlen) == 0)
			raid_setup_args.noautodetect = 1;
		pos += wlen+1;
	}
	raid_setup_args.set = 1;
	return 1;
}

int md__init md_run_setup(void)
{
	if (raid_setup_args.noautodetect)
		printk(KERN_INFO "md: Skipping autodetection of RAID arrays. (raid=noautodetect)\n");
	else
		autostart_arrays();
	md_setup_drive();
	return 0;
}

__setup("raid=", raid_setup);
__setup("md=", md_setup);

__initcall(md_init);
__initcall(md_run_setup);

#else /* It is a MODULE */

int init_module(void)
{
	return md_init();
}

static void free_device_names(void)
{
	while (device_names.next != &device_names) {
		struct list_head *tmp = device_names.next;
		list_del(tmp);
		kfree(tmp);
	}
}


void cleanup_module(void)
{
	md_unregister_thread(md_recovery_thread);
	devfs_unregister(devfs_handle);

	devfs_unregister_blkdev(MAJOR_NR,"md");
	unregister_reboot_notifier(&md_notifier);
	unregister_sysctl_table(raid_table_header);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("mdstat", NULL);
#endif

	del_gendisk(&md_gendisk);

	blk_dev[MAJOR_NR].queue = NULL;
	blksize_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = NULL;
	max_readahead[MAJOR_NR] = NULL;
	hardsect_size[MAJOR_NR] = NULL;

	free_device_names();

}
#endif

MD_EXPORT_SYMBOL(md_size);
MD_EXPORT_SYMBOL(register_md_personality);
MD_EXPORT_SYMBOL(unregister_md_personality);
MD_EXPORT_SYMBOL(partition_name);
MD_EXPORT_SYMBOL(md_error);
MD_EXPORT_SYMBOL(md_do_sync);
MD_EXPORT_SYMBOL(md_sync_acct);
MD_EXPORT_SYMBOL(md_done_sync);
MD_EXPORT_SYMBOL(md_recover_arrays);
MD_EXPORT_SYMBOL(md_register_thread);
MD_EXPORT_SYMBOL(md_unregister_thread);
MD_EXPORT_SYMBOL(md_update_sb);
MD_EXPORT_SYMBOL(md_wakeup_thread);
MD_EXPORT_SYMBOL(md_print_devices);
MD_EXPORT_SYMBOL(find_rdev_nr);
MD_EXPORT_SYMBOL(md_interrupt_thread);
MD_EXPORT_SYMBOL(mddev_map);
MD_EXPORT_SYMBOL(md_check_ordering);
MD_EXPORT_SYMBOL(get_spare);
MODULE_LICENSE("GPL");
