/*
 *  fs/partitions/msdos.c
 *
 *  Code extracted from drivers/block/genhd.c
 *  Copyright (C) 1991-1998  Linus Torvalds
 *
 *  Thanks to Branko Lankester, lankeste@fwi.uva.nl, who found a bug
 *  in the early extended-partition checks and added DM partitions
 *
 *  Support for DiskManager v6.0x added by Mark Lord,
 *  with information provided by OnTrack.  This now works for linux fdisk
 *  and LILO, as well as loadlin and bootln.  Note that disks other than
 *  /dev/hda *must* have a "DOS" type 0x51 partition in the first slot (hda1).
 *
 *  More flexible handling of extended partitions - aeb, 950831
 *
 *  Check partition table on IDE disks for common CHS translations
 *
 *  Re-organised Feb 1998 Russell King
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/blk.h>

#ifdef CONFIG_BLK_DEV_IDE
#include <linux/ide.h>	/* IDE xlate */
#elif defined(CONFIG_BLK_DEV_IDE_MODULE)
#include <linux/module.h>

int (*ide_xlate_1024_hook)(kdev_t, int, int, const char *);
EXPORT_SYMBOL(ide_xlate_1024_hook);
#define ide_xlate_1024 ide_xlate_1024_hook
#endif

#include <asm/system.h>

#include "check.h"
#include "msdos.h"

#if CONFIG_BLK_DEV_MD
extern void md_autodetect_dev(kdev_t dev);
#endif

/*
 * Many architectures don't like unaligned accesses, which is
 * frequently the case with the nr_sects and start_sect partition
 * table entries.
 */
#include <asm/unaligned.h>

#define SYS_IND(p)	(get_unaligned(&p->sys_ind))
#define NR_SECTS(p)	({ __typeof__(p->nr_sects) __a =	\
				get_unaligned(&p->nr_sects);	\
				le32_to_cpu(__a); \
			})

#define START_SECT(p)	({ __typeof__(p->start_sect) __a =	\
				get_unaligned(&p->start_sect);	\
				le32_to_cpu(__a); \
			})

static inline int is_extended_partition(struct partition *p)
{
	return (SYS_IND(p) == DOS_EXTENDED_PARTITION ||
		SYS_IND(p) == WIN98_EXTENDED_PARTITION ||
		SYS_IND(p) == LINUX_EXTENDED_PARTITION);
}

/*
 * msdos_partition_name() formats the short partition name into the supplied
 * buffer, and returns a pointer to that buffer.
 * Used by several partition types which makes conditional inclusion messy,
 * use __attribute__ ((unused)) instead.
 */
static char __attribute__ ((unused))
	*msdos_partition_name (struct gendisk *hd, int minor, char *buf)
{
#ifdef CONFIG_DEVFS_FS
	sprintf(buf, "p%d", (minor & ((1 << hd->minor_shift) - 1)));
	return buf;
#else
	return disk_name(hd, minor, buf);
#endif
}

#define MSDOS_LABEL_MAGIC1	0x55
#define MSDOS_LABEL_MAGIC2	0xAA

static inline int
msdos_magic_present(unsigned char *p)
{
	return (p[0] == MSDOS_LABEL_MAGIC1 && p[1] == MSDOS_LABEL_MAGIC2);
}

/*
 * Create devices for each logical partition in an extended partition.
 * The logical partitions form a linked list, with each entry being
 * a partition table with two entries.  The first entry
 * is the real data partition (with a start relative to the partition
 * table start).  The second is a pointer to the next logical partition
 * (with a start relative to the entire extended partition).
 * We do not create a Linux partition for the partition tables, but
 * only for the actual data partitions.
 */

static void extended_partition(struct gendisk *hd, struct block_device *bdev,
			int minor, unsigned long first_size, int *current_minor)
{
	struct partition *p;
	Sector sect;
	unsigned char *data;
	unsigned long first_sector, this_sector, this_size;
	int mask = (1 << hd->minor_shift) - 1;
	int sector_size = get_hardsect_size(to_kdev_t(bdev->bd_dev)) / 512;
	int loopct = 0;		/* number of links followed
				   without finding a data partition */
	int i;

	this_sector = first_sector = hd->part[minor].start_sect;
	this_size = first_size;

	while (1) {
		if (++loopct > 100)
			return;
		if ((*current_minor & mask) == 0)
			return;
		data = read_dev_sector(bdev, this_sector, &sect);
		if (!data)
			return;

		if (!msdos_magic_present(data + 510))
			goto done; 

		p = (struct partition *) (data + 0x1be);

		/*
		 * Usually, the first entry is the real data partition,
		 * the 2nd entry is the next extended partition, or empty,
		 * and the 3rd and 4th entries are unused.
		 * However, DRDOS sometimes has the extended partition as
		 * the first entry (when the data partition is empty),
		 * and OS/2 seems to use all four entries.
		 */

		/* 
		 * First process the data partition(s)
		 */
		for (i=0; i<4; i++, p++) {
			unsigned long offs, size, next;
			if (!NR_SECTS(p) || is_extended_partition(p))
				continue;

			/* Check the 3rd and 4th entries -
			   these sometimes contain random garbage */
			offs = START_SECT(p)*sector_size;
			size = NR_SECTS(p)*sector_size;
			next = this_sector + offs;
			if (i >= 2) {
				if (offs + size > this_size)
					continue;
				if (next < first_sector)
					continue;
				if (next + size > first_sector + first_size)
					continue;
			}

			add_gd_partition(hd, *current_minor, next, size);
#if CONFIG_BLK_DEV_MD
			if (SYS_IND(p) == LINUX_RAID_PARTITION) {
			    md_autodetect_dev(MKDEV(hd->major,*current_minor));
			}
#endif

			(*current_minor)++;
			loopct = 0;
			if ((*current_minor & mask) == 0)
				goto done;
		}
		/*
		 * Next, process the (first) extended partition, if present.
		 * (So far, there seems to be no reason to make
		 *  extended_partition()  recursive and allow a tree
		 *  of extended partitions.)
		 * It should be a link to the next logical partition.
		 * Create a minor for this just long enough to get the next
		 * partition table.  The minor will be reused for the next
		 * data partition.
		 */
		p -= 4;
		for (i=0; i<4; i++, p++)
			if (NR_SECTS(p) && is_extended_partition(p))
				break;
		if (i == 4)
			goto done;	 /* nothing left to do */

		this_sector = first_sector + START_SECT(p) * sector_size;
		this_size = NR_SECTS(p) * sector_size;
		minor = *current_minor;
		put_dev_sector(sect);
	}
done:
	put_dev_sector(sect);
}

/* james@bpgc.com: Solaris has a nasty indicator: 0x82 which also
   indicates linux swap.  Be careful before believing this is Solaris. */

static void
solaris_x86_partition(struct gendisk *hd, struct block_device *bdev,
		int minor, int *current_minor)
{

#ifdef CONFIG_SOLARIS_X86_PARTITION
	long offset = hd->part[minor].start_sect;
	Sector sect;
	struct solaris_x86_vtoc *v;
	struct solaris_x86_slice *s;
	int mask = (1 << hd->minor_shift) - 1;
	int i;
	char buf[40];

	v = (struct solaris_x86_vtoc *)read_dev_sector(bdev, offset+1, &sect);
	if (!v)
		return;
	if (le32_to_cpu(v->v_sanity) != SOLARIS_X86_VTOC_SANE) {
		put_dev_sector(sect);
		return;
	}
	printk(" %s: <solaris:", msdos_partition_name(hd, minor, buf));
	if (le32_to_cpu(v->v_version) != 1) {
		printk("  cannot handle version %d vtoc>\n",
			le32_to_cpu(v->v_version));
		put_dev_sector(sect);
		return;
	}
	for (i=0; i<SOLARIS_X86_NUMSLICE; i++) {
		if ((*current_minor & mask) == 0)
			break;
		s = &v->v_slice[i];

		if (s->s_size == 0)
			continue;
		printk(" [s%d]", i);
		/* solaris partitions are relative to current MS-DOS
		 * one but add_gd_partition starts relative to sector
		 * zero of the disk.  Therefore, must add the offset
		 * of the current partition */
		add_gd_partition(hd, *current_minor,
				 le32_to_cpu(s->s_start)+offset,
				 le32_to_cpu(s->s_size));
		(*current_minor)++;
	}
	put_dev_sector(sect);
	printk(" >\n");
#endif
}

#ifdef CONFIG_BSD_DISKLABEL
static void
check_and_add_bsd_partition(struct gendisk *hd, struct bsd_partition *bsd_p,
	int baseminor, int *current_minor)
{
	int i, bsd_start, bsd_size;

	bsd_start = le32_to_cpu(bsd_p->p_offset);
	bsd_size = le32_to_cpu(bsd_p->p_size);

	/* check relative position of already allocated partitions */
	for (i = baseminor+1; i < *current_minor; i++) {
		int start = hd->part[i].start_sect;
		int size = hd->part[i].nr_sects;

		if (start+size <= bsd_start || start >= bsd_start+bsd_size)
			continue;	/* no overlap */

		if (start == bsd_start && size == bsd_size)
			return;		/* equal -> no need to add */

		if (start <= bsd_start && start+size >= bsd_start+bsd_size) {
			/* bsd living within dos partition */
#ifdef DEBUG_BSD_DISKLABEL
			printk("w: %d %ld+%ld,%d+%d", 
			       i, start, size, bsd_start, bsd_size);
#endif
			break;		/* ok */
		}

		/* ouch: bsd and linux overlap */
#ifdef DEBUG_BSD_DISKLABEL
		printk("???: %d %ld+%ld,%d+%d",
		       i, start, size, bsd_start, bsd_size);
#endif
		printk("???");
		return;
	}

	add_gd_partition(hd, *current_minor, bsd_start, bsd_size);
	(*current_minor)++;
}

/* 
 * Create devices for BSD partitions listed in a disklabel, under a
 * dos-like partition. See extended_partition() for more information.
 */
static void do_bsd_partition(struct gendisk *hd, struct block_device *bdev,
	int minor, int *current_minor, char *name, int max_partitions)
{
	long offset = hd->part[minor].start_sect;
	Sector sect;
	struct bsd_disklabel *l;
	struct bsd_partition *p;
	int mask = (1 << hd->minor_shift) - 1;
	int baseminor = (minor & ~mask);
	char buf[40];

	l = (struct bsd_disklabel *)read_dev_sector(bdev, offset+1, &sect);
	if (!l)
		return;
	if (le32_to_cpu(l->d_magic) != BSD_DISKMAGIC) {
		put_dev_sector(sect);
		return;
	}
	printk(" %s: <%s:", msdos_partition_name(hd, minor, buf), name);

	if (le16_to_cpu(l->d_npartitions) < max_partitions)
		max_partitions = le16_to_cpu(l->d_npartitions);
	for (p = l->d_partitions; p - l->d_partitions <  max_partitions; p++) {
		if ((*current_minor & mask) == 0)
			break;
		if (p->p_fstype == BSD_FS_UNUSED) 
			continue;
		check_and_add_bsd_partition(hd, p, baseminor, current_minor);
	}
	put_dev_sector(sect);
	printk(" >\n");
}
#endif

static void bsd_partition(struct gendisk *hd, struct block_device *bdev,
	int minor, int *current_minor)
{
#ifdef CONFIG_BSD_DISKLABEL
	do_bsd_partition(hd, bdev, minor, current_minor, "bsd",
		BSD_MAXPARTITIONS);
#endif
}

static void netbsd_partition(struct gendisk *hd, struct block_device *bdev,
		int minor, int *current_minor)
{
#ifdef CONFIG_BSD_DISKLABEL
	do_bsd_partition(hd, bdev, minor, current_minor, "netbsd",
			BSD_MAXPARTITIONS);
#endif
}

static void openbsd_partition(struct gendisk *hd, struct block_device *bdev,
		int minor, int *current_minor)
{
#ifdef CONFIG_BSD_DISKLABEL
	do_bsd_partition(hd, bdev, minor, current_minor,
			"openbsd", OPENBSD_MAXPARTITIONS);
#endif
}

/*
 * Create devices for Unixware partitions listed in a disklabel, under a
 * dos-like partition. See extended_partition() for more information.
 */
static void unixware_partition(struct gendisk *hd, struct block_device *bdev,
		int minor, int *current_minor)
{
#ifdef CONFIG_UNIXWARE_DISKLABEL
	long offset = hd->part[minor].start_sect;
	Sector sect;
	struct unixware_disklabel *l;
	struct unixware_slice *p;
	int mask = (1 << hd->minor_shift) - 1;
	char buf[40];

	l = (struct unixware_disklabel *)read_dev_sector(bdev, offset+29, &sect);
	if (!l)
		return;
	if (le32_to_cpu(l->d_magic) != UNIXWARE_DISKMAGIC ||
	    le32_to_cpu(l->vtoc.v_magic) != UNIXWARE_DISKMAGIC2) {
		put_dev_sector(sect);
		return;
	}
	printk(" %s: <unixware:", msdos_partition_name(hd, minor, buf));
	p = &l->vtoc.v_slice[1];
	/* I omit the 0th slice as it is the same as whole disk. */
	while (p - &l->vtoc.v_slice[0] < UNIXWARE_NUMSLICE) {
		if ((*current_minor & mask) == 0)
			break;

		if (p->s_label != UNIXWARE_FS_UNUSED) {
			add_gd_partition(hd, *current_minor, START_SECT(p),
					 NR_SECTS(p));
			(*current_minor)++;
		}
		p++;
	}
	put_dev_sector(sect);
	printk(" >\n");
#endif
}

/*
 * Minix 2.0.0/2.0.2 subpartition support.
 * Anand Krishnamurthy <anandk@wiproge.med.ge.com>
 * Rajeev V. Pillai    <rajeevvp@yahoo.com>
 */
static void minix_partition(struct gendisk *hd, struct block_device *bdev,
		int minor, int *current_minor)
{
#ifdef CONFIG_MINIX_SUBPARTITION
	long offset = hd->part[minor].start_sect;
	Sector sect;
	unsigned char *data;
	struct partition *p;
	int mask = (1 << hd->minor_shift) - 1;
	int i;
	char buf[40];

	data = read_dev_sector(bdev, offset, &sect);
	if (!data)
		return;

	p = (struct partition *)(data + 0x1be);

	/* The first sector of a Minix partition can have either
	 * a secondary MBR describing its subpartitions, or
	 * the normal boot sector. */
	if (msdos_magic_present (data + 510) &&
	    SYS_IND(p) == MINIX_PARTITION) { /* subpartition table present */

		printk(" %s: <minix:", msdos_partition_name(hd, minor, buf));
		for (i = 0; i < MINIX_NR_SUBPARTITIONS; i++, p++) {
			if ((*current_minor & mask) == 0)
				break;
			/* add each partition in use */
			if (SYS_IND(p) == MINIX_PARTITION) {
				add_gd_partition(hd, *current_minor,
					      START_SECT(p), NR_SECTS(p));
				(*current_minor)++;
			}
		}
		printk(" >\n");
	}
	put_dev_sector(sect);
#endif /* CONFIG_MINIX_SUBPARTITION */
}

static struct {
	unsigned char id;
	void (*parse)(struct gendisk *, struct block_device *, int, int *);
} subtypes[] = {
	{BSD_PARTITION, bsd_partition},
	{NETBSD_PARTITION, netbsd_partition},
	{OPENBSD_PARTITION, openbsd_partition},
	{MINIX_PARTITION, minix_partition},
	{UNIXWARE_PARTITION, unixware_partition},
	{SOLARIS_X86_PARTITION, solaris_x86_partition},
	{0, NULL},
};
/*
 * Look for various forms of IDE disk geometry translation
 */
static int handle_ide_mess(struct block_device *bdev)
{
#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
	Sector sect;
	unsigned char *data;
	kdev_t dev = to_kdev_t(bdev->bd_dev);
	unsigned int sig;
	int heads = 0;
	struct partition *p;
	int i;
#ifdef CONFIG_BLK_DEV_IDE_MODULE
	if (!ide_xlate_1024)
		return 1;
#endif
	/*
	 * The i386 partition handling programs very often
	 * make partitions end on cylinder boundaries.
	 * There is no need to do so, and Linux fdisk doesn't always
	 * do this, and Windows NT on Alpha doesn't do this either,
	 * but still, this helps to guess #heads.
	 */
	data = read_dev_sector(bdev, 0, &sect);
	if (!data)
		return -1;
	if (!msdos_magic_present(data + 510)) {
		put_dev_sector(sect);
		return 0;
	}
	sig = le16_to_cpu(*(unsigned short *)(data + 2));
	p = (struct partition *) (data + 0x1be);
	for (i = 0; i < 4; i++) {
		struct partition *q = &p[i];
		if (NR_SECTS(q)) {
			if ((q->sector & 63) == 1 &&
			    (q->end_sector & 63) == 63)
				heads = q->end_head + 1;
			break;
		}
	}
	if (SYS_IND(p) == EZD_PARTITION) {
		/*
		 * Accesses to sector 0 must go to sector 1 instead.
		 */
		if (ide_xlate_1024(dev, -1, heads, " [EZD]"))
			goto reread;
	} else if (SYS_IND(p) == DM6_PARTITION) {

		/*
		 * Everything on the disk is offset by 63 sectors,
		 * including a "new" MBR with its own partition table.
		 */
		if (ide_xlate_1024(dev, 1, heads, " [DM6:DDO]"))
			goto reread;
	} else if (sig <= 0x1ae &&
		   data[sig] == 0xAA && data[sig+1] == 0x55 &&
		   (data[sig+2] & 1)) {
		/* DM6 signature in MBR, courtesy of OnTrack */
		(void) ide_xlate_1024 (dev, 0, heads, " [DM6:MBR]");
	} else if (SYS_IND(p) == DM6_AUX1PARTITION ||
		   SYS_IND(p) == DM6_AUX3PARTITION) {
		/*
		 * DM6 on other than the first (boot) drive
		 */
		(void) ide_xlate_1024(dev, 0, heads, " [DM6:AUX]");
	} else {
		(void) ide_xlate_1024(dev, 2, heads, " [PTBL]");
	}
	put_dev_sector(sect);
	return 1;

reread:
	put_dev_sector(sect);
	/* Flush the cache */
	invalidate_bdev(bdev, 1);
	truncate_inode_pages(bdev->bd_inode->i_mapping, 0);
#endif /* defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE) */
	return 1;
}
 
int msdos_partition(struct gendisk *hd, struct block_device *bdev,
		    unsigned long first_sector, int first_part_minor)
{
	int i, minor = first_part_minor;
	Sector sect;
	struct partition *p;
	unsigned char *data;
	int mask = (1 << hd->minor_shift) - 1;
	int sector_size = get_hardsect_size(to_kdev_t(bdev->bd_dev)) / 512;
	int current_minor = first_part_minor;
	int err;

	err = handle_ide_mess(bdev);
	if (err <= 0)
		return err;
	data = read_dev_sector(bdev, 0, &sect);
	if (!data)
		return -1;
	if (!msdos_magic_present(data + 510)) {
		put_dev_sector(sect);
		return 0;
	}
	p = (struct partition *) (data + 0x1be);

	/*
	 * Look for partitions in two passes:
	 * First find the primary and DOS-type extended partitions.
	 * On the second pass look inside *BSD, Unixware and Solaris partitions.
	 */

	current_minor += 4;
	for (i=1 ; i<=4 ; minor++,i++,p++) {
		if (!NR_SECTS(p))
			continue;
		add_gd_partition(hd, minor,
				first_sector+START_SECT(p)*sector_size,
				NR_SECTS(p)*sector_size);
#if CONFIG_BLK_DEV_MD
		if (SYS_IND(p) == LINUX_RAID_PARTITION) {
			md_autodetect_dev(MKDEV(hd->major,minor));
		}
#endif
		if (is_extended_partition(p)) {
			unsigned long size = hd->part[minor].nr_sects;
			printk(" <");
			/* prevent someone doing mkfs or mkswap on an
			   extended partition, but leave room for LILO */
			if (size > 2)
				hd->part[minor].nr_sects = 2;
			extended_partition(hd, bdev, minor, size, &current_minor);
			printk(" >");
		}
	}

	/*
	 *  Check for old-style Disk Manager partition table
	 */
	if (msdos_magic_present(data + 0xfc)) {
		p = (struct partition *) (0x1be + data);
		for (i = 4 ; i < 16 ; i++, current_minor++) {
			p--;
			if ((current_minor & mask) == 0)
				break;
			if (!(START_SECT(p) && NR_SECTS(p)))
				continue;
			add_gd_partition(hd, current_minor, START_SECT(p), NR_SECTS(p));
		}
	}
	printk("\n");

	/* second pass - output for each on a separate line */
	minor -= 4;
	p = (struct partition *) (0x1be + data);
	for (i=1 ; i<=4 ; minor++,i++,p++) {
		unsigned char id = SYS_IND(p);
		int n;

		if (!NR_SECTS(p))
			continue;

		for (n = 0; subtypes[n].parse && id != subtypes[n].id; n++)
			;

		if (subtypes[n].parse)
			subtypes[n].parse(hd, bdev, minor, &current_minor);
	}
	put_dev_sector(sect);
	return 1;
}
