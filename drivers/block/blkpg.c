/*
 * Partition table and disk geometry handling
 *
 * This obsoletes the partition-handling code in genhd.c:
 * Userspace can look at a disk in arbitrary format and tell
 * the kernel what partitions there are on the disk, and how
 * these should be numbered.
 * It also allows one to repartition a disk that is being used.
 *
 * A single ioctl with lots of subfunctions:
 *
 * Device number stuff:
 *    get_whole_disk()          (given the device number of a partition, find
 *                               the device number of the encompassing disk)
 *    get_all_partitions()      (given the device number of a disk, return the
 *                               device numbers of all its known partitions)
 *
 * Partition stuff:
 *    add_partition()
 *    delete_partition()
 *    test_partition_in_use()   (also for test_disk_in_use)
 *
 * Geometry stuff:
 *    get_geometry()
 *    set_geometry()
 *    get_bios_drivedata()
 *
 * For today, only the partition stuff - aeb, 990515
 */

#include <linux/errno.h>
#include <linux/fs.h>			/* for BLKRASET, ... */
#include <linux/sched.h>		/* for capable() */
#include <linux/blk.h>			/* for set_device_ro() */
#include <linux/blkpg.h>
#include <linux/genhd.h>
#include <linux/swap.h>			/* for is_swap_partition() */
#include <linux/module.h>               /* for EXPORT_SYMBOL */

#include <asm/uaccess.h>

/*
 * What is the data describing a partition?
 *
 * 1. a device number (kdev_t)
 * 2. a starting sector and number of sectors (hd_struct)
 *    given in the part[] array of the gendisk structure for the drive.
 *
 * The number of sectors is replicated in the sizes[] array of
 * the gendisk structure for the major, which again is copied to
 * the blk_size[][] array.
 * (However, hd_struct has the number of 512-byte sectors,
 *  g->sizes[] and blk_size[][] have the number of 1024-byte blocks.)
 * Note that several drives may have the same major.
 */

/*
 * Add a partition.
 *
 * returns: EINVAL: bad parameters
 *          ENXIO: cannot find drive
 *          EBUSY: proposed partition overlaps an existing one
 *                 or has the same number as an existing one
 *          0: all OK.
 */
int add_partition(kdev_t dev, struct blkpg_partition *p) {
	struct gendisk *g;
	long long ppstart, pplength;
	long pstart, plength;
	int i, drive, first_minor, end_minor, minor;

	/* convert bytes to sectors, check for fit in a hd_struct */
	ppstart = (p->start >> 9);
	pplength = (p->length >> 9);
	pstart = ppstart;
	plength = pplength;
	if (pstart != ppstart || plength != pplength
	    || pstart < 0 || plength < 0)
		return -EINVAL;

	/* find the drive major */
	g = get_gendisk(dev);
	if (!g)
		return -ENXIO;

	/* existing drive? */
	drive = (MINOR(dev) >> g->minor_shift);
	first_minor = (drive << g->minor_shift);
	end_minor   = first_minor + g->max_p;
	if (drive >= g->nr_real)
		return -ENXIO;

	/* drive and partition number OK? */
	if (first_minor != MINOR(dev) || p->pno <= 0 || p->pno >= g->max_p)
		return -EINVAL;

	/* partition number in use? */
	minor = first_minor + p->pno;
	if (g->part[minor].nr_sects != 0)
		return -EBUSY;

	/* overlap? */
	for (i=first_minor+1; i<end_minor; i++)
		if (!(pstart+plength <= g->part[i].start_sect ||
		      pstart >= g->part[i].start_sect + g->part[i].nr_sects))
			return -EBUSY;

	/* all seems OK */
	g->part[minor].start_sect = pstart;
	g->part[minor].nr_sects = plength;
	if (g->sizes)
		g->sizes[minor] = (plength >> (BLOCK_SIZE_BITS - 9));
	devfs_register_partitions (g, first_minor, 0);
	return 0;
}

/*
 * Delete a partition given by partition number
 *
 * returns: EINVAL: bad parameters
 *          ENXIO: cannot find partition
 *          EBUSY: partition is busy
 *          0: all OK.
 *
 * Note that the dev argument refers to the entire disk, not the partition.
 */
int del_partition(kdev_t dev, struct blkpg_partition *p) {
	struct gendisk *g;
	kdev_t devp;
	int drive, first_minor, minor;

	/* find the drive major */
	g = get_gendisk(dev);
	if (!g)
		return -ENXIO;

	/* drive and partition number OK? */
	drive = (MINOR(dev) >> g->minor_shift);
	first_minor = (drive << g->minor_shift);
	if (first_minor != MINOR(dev) || p->pno <= 0 || p->pno >= g->max_p)
		return -EINVAL;

	/* existing drive and partition? */
	minor = first_minor + p->pno;
	if (drive >= g->nr_real || g->part[minor].nr_sects == 0)
		return -ENXIO;

	/* partition in use? Incomplete check for now. */
	devp = MKDEV(MAJOR(dev), minor);
	if (is_mounted(devp) || is_swap_partition(devp))
		return -EBUSY;

	/* all seems OK */
	fsync_dev(devp);
	invalidate_buffers(devp);

	g->part[minor].start_sect = 0;
	g->part[minor].nr_sects = 0;
	if (g->sizes)
		g->sizes[minor] = 0;
	devfs_register_partitions (g, first_minor, 0);

	return 0;
}

int blkpg_ioctl(kdev_t dev, struct blkpg_ioctl_arg *arg)
{
	struct blkpg_ioctl_arg a;
	struct blkpg_partition p;
	int len;

	if (copy_from_user(&a, arg, sizeof(struct blkpg_ioctl_arg)))
		return -EFAULT;

	switch (a.op) {
		case BLKPG_ADD_PARTITION:
		case BLKPG_DEL_PARTITION:
			len = a.datalen;
			if (len < sizeof(struct blkpg_partition))
				return -EINVAL;
			if (copy_from_user(&p, a.data, sizeof(struct blkpg_partition)))
				return -EFAULT;
			if (!capable(CAP_SYS_ADMIN))
				return -EACCES;
			if (a.op == BLKPG_ADD_PARTITION)
				return add_partition(dev, &p);
			else
				return del_partition(dev, &p);
		default:
			return -EINVAL;
	}
}

/*
 * Common ioctl's for block devices
 */

int blk_ioctl(kdev_t dev, unsigned int cmd, unsigned long arg)
{
	struct gendisk *g;
	u64 ullval = 0;
	int intval;

	if (!dev)
		return -EINVAL;

	switch (cmd) {
		case BLKROSET:
			if (!capable(CAP_SYS_ADMIN))
				return -EACCES;
			if (get_user(intval, (int *)(arg)))
				return -EFAULT;
			set_device_ro(dev, intval);
			return 0;
		case BLKROGET:
			intval = (is_read_only(dev) != 0);
			return put_user(intval, (int *)(arg));

		case BLKRASET:
			if(!capable(CAP_SYS_ADMIN))
				return -EACCES;
			if(arg > 0xff)
				return -EINVAL;
			read_ahead[MAJOR(dev)] = arg;
			return 0;
		case BLKRAGET:
			if (!arg)
				return -EINVAL;
			return put_user(read_ahead[MAJOR(dev)], (long *) arg);

		case BLKFLSBUF:
			if(!capable(CAP_SYS_ADMIN))
				return -EACCES;
			fsync_dev(dev);
			invalidate_buffers(dev);
			return 0;

		case BLKSSZGET:
			/* get block device sector size as needed e.g. by fdisk */
			intval = get_hardsect_size(dev);
			return put_user(intval, (int *) arg);

		case BLKGETSIZE:
		case BLKGETSIZE64:
			g = get_gendisk(dev);
			if (g)
				ullval = g->part[MINOR(dev)].nr_sects;

			if (cmd == BLKGETSIZE)
				return put_user((unsigned long)ullval, (unsigned long *)arg);
			else
				return put_user(ullval << 9, (u64 *)arg);
#if 0
		case BLKRRPART: /* Re-read partition tables */
			if (!capable(CAP_SYS_ADMIN)) 
				return -EACCES;
			return reread_partitions(dev, 1);
#endif

		case BLKPG:
			return blkpg_ioctl(dev, (struct blkpg_ioctl_arg *) arg);
			
		case BLKELVGET:
			return blkelvget_ioctl(&blk_get_queue(dev)->elevator,
					       (blkelv_ioctl_arg_t *) arg);
		case BLKELVSET:
			return blkelvset_ioctl(&blk_get_queue(dev)->elevator,
					       (blkelv_ioctl_arg_t *) arg);

		case BLKBSZGET:
			/* get the logical block size (cf. BLKSSZGET) */
			intval = BLOCK_SIZE;
			if (blksize_size[MAJOR(dev)])
				intval = blksize_size[MAJOR(dev)][MINOR(dev)];
			return put_user (intval, (int *) arg);

		case BLKBSZSET:
			/* set the logical block size */
			if (!capable (CAP_SYS_ADMIN))
				return -EACCES;
			if (!dev || !arg)
				return -EINVAL;
			if (get_user (intval, (int *) arg))
				return -EFAULT;
			if (intval > PAGE_SIZE || intval < 512 ||
			    (intval & (intval - 1)))
				return -EINVAL;
			if (is_mounted (dev) || is_swap_partition (dev))
				return -EBUSY;
			set_blocksize (dev, intval);
			return 0;

		default:
			return -EINVAL;
	}
}

EXPORT_SYMBOL(blk_ioctl);
