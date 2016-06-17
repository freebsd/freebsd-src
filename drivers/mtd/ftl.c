/* This version ported to the Linux-MTD system by dwmw2@infradead.org
 * $Id: ftl.c,v 1.45 2003/01/24 23:31:27 dwmw2 Exp $
 *
 * Fixes: Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - fixes some leaks on failure in build_maps and ftl_notify_add, cleanups
 *
 * Based on:
 */
/*======================================================================

    A Flash Translation Layer memory card driver

    This driver implements a disk-like block device driver with an
    apparent block size of 512 bytes for flash memory cards.

    ftl_cs.c 1.62 2000/02/01 00:59:04

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in
    which case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.

    LEGAL NOTE: The FTL format is patented by M-Systems.  They have
    granted a license for its use with PCMCIA devices:

     "M-Systems grants a royalty-free, non-exclusive license under
      any presently existing M-Systems intellectual property rights
      necessary for the design and development of FTL-compatible
      drivers, file systems and utilities using the data formats with
      PCMCIA PC Cards as described in the PCMCIA Flash Translation
      Layer (FTL) Specification."

    Use of the FTL format for non-PCMCIA applications may be an
    infringement of these patents.  For additional information,
    contact M-Systems (http://www.m-sys.com) directly.
      
======================================================================*/
#include <linux/module.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>
/*#define PSYCHO_DEBUG */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/hdreg.h>

#if (LINUX_VERSION_CODE >= 0x20100)
#include <linux/vmalloc.h>
#endif
#if (LINUX_VERSION_CODE >= 0x20303)
#include <linux/blkpg.h>
#endif

#include <linux/mtd/ftl.h>
/*====================================================================*/
/* Stuff which really ought to be in compatmac.h */

#if (LINUX_VERSION_CODE < 0x20328)
#define register_disk(dev, drive, minors, ops, size) \
    do { (dev)->part[(drive)*(minors)].nr_sects = size; \
        if (size == 0) (dev)->part[(drive)*(minors)].start_sect = -1; \
        resetup_one_dev(dev, drive); } while (0)
#endif

#if (LINUX_VERSION_CODE < 0x20320)
#define BLK_DEFAULT_QUEUE(n)    blk_dev[n].request_fn
#define blk_init_queue(q, req)  q = (req)
#define blk_cleanup_queue(q)    q = NULL
#define request_arg_t           void
#else
#define request_arg_t           request_queue_t *q
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,14)
#define BLK_INC_USE_COUNT MOD_INC_USE_COUNT
#define BLK_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define BLK_INC_USE_COUNT do {} while(0)
#define BLK_DEC_USE_COUNT do {} while(0)
#endif

/*====================================================================*/

/* Parameters that can be set with 'insmod' */
static int shuffle_freq = 50;
MODULE_PARM(shuffle_freq, "i");

/*====================================================================*/

/* Major device # for FTL device */
#ifndef FTL_MAJOR
#define FTL_MAJOR	44
#endif

/* Funky stuff for setting up a block device */
#define MAJOR_NR		FTL_MAJOR
#define DEVICE_NAME		"ftl"
#define DEVICE_REQUEST		do_ftl_request
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#define DEVICE_NR(minor)	((minor)>>5)
#define REGION_NR(minor)	(((minor)>>3)&3)
#define PART_NR(minor)		((minor)&7)
#define MINOR_NR(dev,reg,part)	(((dev)<<5)+((reg)<<3)+(part))

#include <linux/blk.h>

/*====================================================================*/

/* Maximum number of separate memory devices we'll allow */
#define MAX_DEV		4

/* Maximum number of regions per device */
#define MAX_REGION	4

/* Maximum number of partitions in an FTL region */
#define PART_BITS	3
#define MAX_PART	8

/* Maximum number of outstanding erase requests per socket */
#define MAX_ERASE	8

/* Sector size -- shouldn't need to change */
#define SECTOR_SIZE	512


/* Each memory region corresponds to a minor device */
typedef struct partition_t {
    struct mtd_info	*mtd;
    u_int32_t		state;
    u_int32_t		*VirtualBlockMap;
    u_int32_t		*VirtualPageMap;
    u_int32_t		FreeTotal;
    struct eun_info_t {
	u_int32_t		Offset;
	u_int32_t		EraseCount;
	u_int32_t		Free;
	u_int32_t		Deleted;
    } *EUNInfo;
    struct xfer_info_t {
	u_int32_t		Offset;
	u_int32_t		EraseCount;
	u_int16_t		state;
    } *XferInfo;
    u_int16_t		bam_index;
    u_int32_t		*bam_cache;
    u_int16_t		DataUnits;
    u_int32_t		BlocksPerUnit;
    erase_unit_header_t	header;
#if 0
    region_info_t	region;
    memory_handle_t	handle;
#endif
    atomic_t		open;
} partition_t;

partition_t *myparts[MAX_MTD_DEVICES];

static void ftl_notify_add(struct mtd_info *mtd);
static void ftl_notify_remove(struct mtd_info *mtd);

void ftl_freepart(partition_t *part);

static struct mtd_notifier ftl_notifier = {
	add:	ftl_notify_add,
	remove:	ftl_notify_remove,
};

/* Partition state flags */
#define FTL_FORMATTED	0x01

/* Transfer unit states */
#define XFER_UNKNOWN	0x00
#define XFER_ERASING	0x01
#define XFER_ERASED	0x02
#define XFER_PREPARED	0x03
#define XFER_FAILED	0x04

static struct hd_struct ftl_hd[MINOR_NR(MAX_DEV, 0, 0)];
static int ftl_sizes[MINOR_NR(MAX_DEV, 0, 0)];
static int ftl_blocksizes[MINOR_NR(MAX_DEV, 0, 0)];

static struct gendisk ftl_gendisk = {
    major:		FTL_MAJOR,
    major_name:		"ftl",
    minor_shift:	PART_BITS,
    max_p:		MAX_PART,
#if (LINUX_VERSION_CODE < 0x20328)
    max_nr:		MAX_DEV*MAX_PART,
#endif
    part:		ftl_hd,
    sizes:		ftl_sizes,
};

/*====================================================================*/

static int ftl_ioctl(struct inode *inode, struct file *file,
		     u_int cmd, u_long arg);
static int ftl_open(struct inode *inode, struct file *file);
static release_t ftl_close(struct inode *inode, struct file *file);
static int ftl_reread_partitions(int minor);

static void ftl_erase_callback(struct erase_info *done);

#if LINUX_VERSION_CODE < 0x20326
static struct file_operations ftl_blk_fops = {
    open:	ftl_open,
    release:	ftl_close,
    ioctl:	ftl_ioctl,
    read:	block_read,
    write:	block_write,
    fsync:	block_fsync
};
#else
static struct block_device_operations ftl_blk_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,14)
    owner:	THIS_MODULE,
#endif
    open:	ftl_open,
    release:	ftl_close,
    ioctl:	ftl_ioctl,
};
#endif

/*======================================================================

    Scan_header() checks to see if a memory region contains an FTL
    partition.  build_maps() reads all the erase unit headers, builds
    the erase unit map, and then builds the virtual page map.
    
======================================================================*/

static int scan_header(partition_t *part)
{
    erase_unit_header_t header;
    loff_t offset, max_offset;
    int ret;
    part->header.FormattedSize = 0;
    max_offset = (0x100000<part->mtd->size)?0x100000:part->mtd->size;
    /* Search first megabyte for a valid FTL header */
    for (offset = 0;
	 (offset + sizeof(header)) < max_offset;
	 offset += part->mtd->erasesize ? : 0x2000) {

	ret = part->mtd->read(part->mtd, offset, sizeof(header), &ret, 
			      (unsigned char *)&header);
	
	if (ret) 
	    return ret;

	if (strcmp(header.DataOrgTuple+3, "FTL100") == 0) break;
    }

    if (offset == max_offset) {
	printk(KERN_NOTICE "ftl_cs: FTL header not found.\n");
	return -ENOENT;
    }
    if ((le16_to_cpu(header.NumEraseUnits) > 65536) || header.BlockSize != 9 ||
	(header.EraseUnitSize < 10) || (header.EraseUnitSize > 31) ||
	(header.NumTransferUnits >= le16_to_cpu(header.NumEraseUnits))) {
	printk(KERN_NOTICE "ftl_cs: FTL header corrupt!\n");
	return -1;
    }
    if ((1 << header.EraseUnitSize) != part->mtd->erasesize) {
	printk(KERN_NOTICE "ftl: FTL EraseUnitSize %x != MTD erasesize %x\n",
	       1 << header.EraseUnitSize,part->mtd->erasesize);
	return -1;
    }
    part->header = header;
    return 0;
}

static int build_maps(partition_t *part)
{
    erase_unit_header_t header;
    u_int16_t xvalid, xtrans, i;
    u_int blocks, j;
    int hdr_ok, ret = -1;
    ssize_t retval;
    loff_t offset;

    /* Set up erase unit maps */
    part->DataUnits = le16_to_cpu(part->header.NumEraseUnits) -
	part->header.NumTransferUnits;
    part->EUNInfo = kmalloc(part->DataUnits * sizeof(struct eun_info_t),
			    GFP_KERNEL);
    if (!part->EUNInfo)
	    goto out;
    for (i = 0; i < part->DataUnits; i++)
	part->EUNInfo[i].Offset = 0xffffffff;
    part->XferInfo =
	kmalloc(part->header.NumTransferUnits * sizeof(struct xfer_info_t),
		GFP_KERNEL);
    if (!part->XferInfo)
	    goto out_EUNInfo;

    xvalid = xtrans = 0;
    for (i = 0; i < le16_to_cpu(part->header.NumEraseUnits); i++) {
	offset = ((i + le16_to_cpu(part->header.FirstPhysicalEUN))
		      << part->header.EraseUnitSize);
	ret = part->mtd->read(part->mtd, offset, sizeof(header), &retval, 
			      (unsigned char *)&header);
	
	if (ret) 
	    goto out_XferInfo;

	ret = -1;
	/* Is this a transfer partition? */
	hdr_ok = (strcmp(header.DataOrgTuple+3, "FTL100") == 0);
	if (hdr_ok && (le16_to_cpu(header.LogicalEUN) < part->DataUnits) &&
	    (part->EUNInfo[le16_to_cpu(header.LogicalEUN)].Offset == 0xffffffff)) {
	    part->EUNInfo[le16_to_cpu(header.LogicalEUN)].Offset = offset;
	    part->EUNInfo[le16_to_cpu(header.LogicalEUN)].EraseCount =
		le32_to_cpu(header.EraseCount);
	    xvalid++;
	} else {
	    if (xtrans == part->header.NumTransferUnits) {
		printk(KERN_NOTICE "ftl_cs: format error: too many "
		       "transfer units!\n");
		goto out_XferInfo;
	    }
	    if (hdr_ok && (le16_to_cpu(header.LogicalEUN) == 0xffff)) {
		part->XferInfo[xtrans].state = XFER_PREPARED;
		part->XferInfo[xtrans].EraseCount = le32_to_cpu(header.EraseCount);
	    } else {
		part->XferInfo[xtrans].state = XFER_UNKNOWN;
		/* Pick anything reasonable for the erase count */
		part->XferInfo[xtrans].EraseCount =
		    le32_to_cpu(part->header.EraseCount);
	    }
	    part->XferInfo[xtrans].Offset = offset;
	    xtrans++;
	}
    }
    /* Check for format trouble */
    header = part->header;
    if ((xtrans != header.NumTransferUnits) ||
	(xvalid+xtrans != le16_to_cpu(header.NumEraseUnits))) {
	printk(KERN_NOTICE "ftl_cs: format error: erase units "
	       "don't add up!\n");
	goto out_XferInfo;
    }
    
    /* Set up virtual page map */
    blocks = le32_to_cpu(header.FormattedSize) >> header.BlockSize;
    part->VirtualBlockMap = vmalloc(blocks * sizeof(u_int32_t));
    if (!part->VirtualBlockMap)
	    goto out_XferInfo;

    memset(part->VirtualBlockMap, 0xff, blocks * sizeof(u_int32_t));
    part->BlocksPerUnit = (1 << header.EraseUnitSize) >> header.BlockSize;

    part->bam_cache = kmalloc(part->BlocksPerUnit * sizeof(u_int32_t),
			      GFP_KERNEL);
    if (!part->bam_cache)
	    goto out_VirtualBlockMap;

    part->bam_index = 0xffff;
    part->FreeTotal = 0;

    for (i = 0; i < part->DataUnits; i++) {
	part->EUNInfo[i].Free = 0;
	part->EUNInfo[i].Deleted = 0;
	offset = part->EUNInfo[i].Offset + le32_to_cpu(header.BAMOffset);
	
	ret = part->mtd->read(part->mtd, offset,  
			      part->BlocksPerUnit * sizeof(u_int32_t), &retval, 
			      (unsigned char *)part->bam_cache);
	
	if (ret) 
		goto out_bam_cache;

	for (j = 0; j < part->BlocksPerUnit; j++) {
	    if (BLOCK_FREE(le32_to_cpu(part->bam_cache[j]))) {
		part->EUNInfo[i].Free++;
		part->FreeTotal++;
	    } else if ((BLOCK_TYPE(le32_to_cpu(part->bam_cache[j])) == BLOCK_DATA) &&
		     (BLOCK_NUMBER(le32_to_cpu(part->bam_cache[j])) < blocks))
		part->VirtualBlockMap[BLOCK_NUMBER(le32_to_cpu(part->bam_cache[j]))] =
		    (i << header.EraseUnitSize) + (j << header.BlockSize);
	    else if (BLOCK_DELETED(le32_to_cpu(part->bam_cache[j])))
		part->EUNInfo[i].Deleted++;
	}
    }
    
    ret = 0;
    goto out;

out_bam_cache:
    kfree(part->bam_cache);
out_VirtualBlockMap:
    vfree(part->VirtualBlockMap);
out_XferInfo:
    kfree(part->XferInfo);
out_EUNInfo:
    kfree(part->EUNInfo);
out:
    return ret;
} /* build_maps */

/*======================================================================

    Erase_xfer() schedules an asynchronous erase operation for a
    transfer unit.
    
======================================================================*/

static int erase_xfer(partition_t *part,
		      u_int16_t xfernum)
{
    int ret;
    struct xfer_info_t *xfer;
    struct erase_info *erase;

    xfer = &part->XferInfo[xfernum];
    DEBUG(1, "ftl_cs: erasing xfer unit at 0x%x\n", xfer->Offset);
    xfer->state = XFER_ERASING;

    /* Is there a free erase slot? Always in MTD. */
    
    
    erase=kmalloc(sizeof(struct erase_info), GFP_KERNEL);
    if (!erase) 
            return -ENOMEM;

    erase->callback = ftl_erase_callback;
    erase->addr = xfer->Offset;
    erase->len = 1 << part->header.EraseUnitSize;
    erase->priv = (u_long)part;
    
    ret = part->mtd->erase(part->mtd, erase);

    if (!ret)
	    xfer->EraseCount++;
    else
	    kfree(erase);

    return ret;
} /* erase_xfer */

/*======================================================================

    Prepare_xfer() takes a freshly erased transfer unit and gives
    it an appropriate header.
    
======================================================================*/

static void ftl_erase_callback(struct erase_info *erase)
{
    partition_t *part;
    struct xfer_info_t *xfer;
    int i;
    
    /* Look up the transfer unit */
    part = (partition_t *)(erase->priv);

    for (i = 0; i < part->header.NumTransferUnits; i++)
	if (part->XferInfo[i].Offset == erase->addr) break;

    if (i == part->header.NumTransferUnits) {
	printk(KERN_NOTICE "ftl_cs: internal error: "
	       "erase lookup failed!\n");
	return;
    }

    xfer = &part->XferInfo[i];
    if (erase->state == MTD_ERASE_DONE)
	xfer->state = XFER_ERASED;
    else {
	xfer->state = XFER_FAILED;
	printk(KERN_NOTICE "ftl_cs: erase failed: state = %d\n",
	       erase->state);
    }

    kfree(erase);

} /* ftl_erase_callback */

static int prepare_xfer(partition_t *part, int i)
{
    erase_unit_header_t header;
    struct xfer_info_t *xfer;
    int nbam, ret;
    u_int32_t ctl;
    ssize_t retlen;
    loff_t offset;

    xfer = &part->XferInfo[i];
    xfer->state = XFER_FAILED;
    
    DEBUG(1, "ftl_cs: preparing xfer unit at 0x%x\n", xfer->Offset);

    /* Write the transfer unit header */
    header = part->header;
    header.LogicalEUN = cpu_to_le16(0xffff);
    header.EraseCount = cpu_to_le32(xfer->EraseCount);

    ret = part->mtd->write(part->mtd, xfer->Offset, sizeof(header),
			   &retlen, (u_char *)&header);

    if (ret) {
	return ret;
    }

    /* Write the BAM stub */
    nbam = (part->BlocksPerUnit * sizeof(u_int32_t) +
	    le32_to_cpu(part->header.BAMOffset) + SECTOR_SIZE - 1) / SECTOR_SIZE;

    offset = xfer->Offset + le32_to_cpu(part->header.BAMOffset);
    ctl = cpu_to_le32(BLOCK_CONTROL);

    for (i = 0; i < nbam; i++, offset += sizeof(u_int32_t)) {

	ret = part->mtd->write(part->mtd, offset, sizeof(u_int32_t), 
			       &retlen, (u_char *)&ctl);

	if (ret)
	    return ret;
    }
    xfer->state = XFER_PREPARED;
    return 0;
    
} /* prepare_xfer */

/*======================================================================

    Copy_erase_unit() takes a full erase block and a transfer unit,
    copies everything to the transfer unit, then swaps the block
    pointers.

    All data blocks are copied to the corresponding blocks in the
    target unit, so the virtual block map does not need to be
    updated.
    
======================================================================*/

static int copy_erase_unit(partition_t *part, u_int16_t srcunit,
			   u_int16_t xferunit)
{
    u_char buf[SECTOR_SIZE];
    struct eun_info_t *eun;
    struct xfer_info_t *xfer;
    u_int32_t src, dest, free, i;
    u_int16_t unit;
    int ret;
    ssize_t retlen;
    loff_t offset;
    u_int16_t srcunitswap = cpu_to_le16(srcunit);

    eun = &part->EUNInfo[srcunit];
    xfer = &part->XferInfo[xferunit];
    DEBUG(2, "ftl_cs: copying block 0x%x to 0x%x\n",
	  eun->Offset, xfer->Offset);
	
    
    /* Read current BAM */
    if (part->bam_index != srcunit) {

	offset = eun->Offset + le32_to_cpu(part->header.BAMOffset);

	ret = part->mtd->read(part->mtd, offset, 
			      part->BlocksPerUnit * sizeof(u_int32_t),
			      &retlen, (u_char *) (part->bam_cache));

	/* mark the cache bad, in case we get an error later */
	part->bam_index = 0xffff;

	if (ret) {
	    printk( KERN_WARNING "ftl: Failed to read BAM cache in copy_erase_unit()!\n");                
	    return ret;
	}
    }
    
    /* Write the LogicalEUN for the transfer unit */
    xfer->state = XFER_UNKNOWN;
    offset = xfer->Offset + 20; /* Bad! */
    unit = cpu_to_le16(0x7fff);

    ret = part->mtd->write(part->mtd, offset, sizeof(u_int16_t),
			   &retlen, (u_char *) &unit);
    
    if (ret) {
	printk( KERN_WARNING "ftl: Failed to write back to BAM cache in copy_erase_unit()!\n");
	return ret;
    }
    
    /* Copy all data blocks from source unit to transfer unit */
    src = eun->Offset; dest = xfer->Offset;

    free = 0;
    ret = 0;
    for (i = 0; i < part->BlocksPerUnit; i++) {
	switch (BLOCK_TYPE(le32_to_cpu(part->bam_cache[i]))) {
	case BLOCK_CONTROL:
	    /* This gets updated later */
	    break;
	case BLOCK_DATA:
	case BLOCK_REPLACEMENT:
	    ret = part->mtd->read(part->mtd, src, SECTOR_SIZE,
                        &retlen, (u_char *) buf);
	    if (ret) {
		printk(KERN_WARNING "ftl: Error reading old xfer unit in copy_erase_unit\n");
		return ret;
            }


	    ret = part->mtd->write(part->mtd, dest, SECTOR_SIZE,
                        &retlen, (u_char *) buf);
	    if (ret)  {
		printk(KERN_WARNING "ftl: Error writing new xfer unit in copy_erase_unit\n");
		return ret;
            }

	    break;
	default:
	    /* All other blocks must be free */
	    part->bam_cache[i] = cpu_to_le32(0xffffffff);
	    free++;
	    break;
	}
	src += SECTOR_SIZE;
	dest += SECTOR_SIZE;
    }

    /* Write the BAM to the transfer unit */
    ret = part->mtd->write(part->mtd, xfer->Offset + le32_to_cpu(part->header.BAMOffset), 
                    part->BlocksPerUnit * sizeof(int32_t), &retlen, 
		    (u_char *)part->bam_cache);
    if (ret) {
	printk( KERN_WARNING "ftl: Error writing BAM in copy_erase_unit\n");
	return ret;
    }

    
    /* All clear? Then update the LogicalEUN again */
    ret = part->mtd->write(part->mtd, xfer->Offset + 20, sizeof(u_int16_t),
			   &retlen, (u_char *)&srcunitswap);

    if (ret) {
	printk(KERN_WARNING "ftl: Error writing new LogicalEUN in copy_erase_unit\n");
	return ret;
    }    
    
    
    /* Update the maps and usage stats*/
    i = xfer->EraseCount;
    xfer->EraseCount = eun->EraseCount;
    eun->EraseCount = i;
    i = xfer->Offset;
    xfer->Offset = eun->Offset;
    eun->Offset = i;
    part->FreeTotal -= eun->Free;
    part->FreeTotal += free;
    eun->Free = free;
    eun->Deleted = 0;
    
    /* Now, the cache should be valid for the new block */
    part->bam_index = srcunit;
    
    return 0;
} /* copy_erase_unit */

/*======================================================================

    reclaim_block() picks a full erase unit and a transfer unit and
    then calls copy_erase_unit() to copy one to the other.  Then, it
    schedules an erase on the expired block.

    What's a good way to decide which transfer unit and which erase
    unit to use?  Beats me.  My way is to always pick the transfer
    unit with the fewest erases, and usually pick the data unit with
    the most deleted blocks.  But with a small probability, pick the
    oldest data unit instead.  This means that we generally postpone
    the next reclaimation as long as possible, but shuffle static
    stuff around a bit for wear leveling.
    
======================================================================*/

static int reclaim_block(partition_t *part)
{
    u_int16_t i, eun, xfer;
    u_int32_t best;
    int queued, ret;

    DEBUG(0, "ftl_cs: reclaiming space...\n");
    DEBUG(3, "NumTransferUnits == %x\n", part->header.NumTransferUnits);
    /* Pick the least erased transfer unit */
    best = 0xffffffff; xfer = 0xffff;
    do {
	queued = 0;
	for (i = 0; i < part->header.NumTransferUnits; i++) {
	    int n=0;
	    if (part->XferInfo[i].state == XFER_UNKNOWN) {
		DEBUG(3,"XferInfo[%d].state == XFER_UNKNOWN\n",i);
		n=1;
		erase_xfer(part, i);
	    }
	    if (part->XferInfo[i].state == XFER_ERASING) {
		DEBUG(3,"XferInfo[%d].state == XFER_ERASING\n",i);
		n=1;
		queued = 1;
	    }
	    else if (part->XferInfo[i].state == XFER_ERASED) {
		DEBUG(3,"XferInfo[%d].state == XFER_ERASED\n",i);
		n=1;
		prepare_xfer(part, i);
	    }
	    if (part->XferInfo[i].state == XFER_PREPARED) {
		DEBUG(3,"XferInfo[%d].state == XFER_PREPARED\n",i);
		n=1;
		if (part->XferInfo[i].EraseCount <= best) {
		    best = part->XferInfo[i].EraseCount;
		    xfer = i;
		}
	    }
		if (!n)
		    DEBUG(3,"XferInfo[%d].state == %x\n",i, part->XferInfo[i].state);

	}
	if (xfer == 0xffff) {
	    if (queued) {
		DEBUG(1, "ftl_cs: waiting for transfer "
		      "unit to be prepared...\n");
		if (part->mtd->sync)
			part->mtd->sync(part->mtd);
	    } else {
		static int ne = 0;
		if (++ne < 5)
		    printk(KERN_NOTICE "ftl_cs: reclaim failed: no "
			   "suitable transfer units!\n");
		else
		    DEBUG(1, "ftl_cs: reclaim failed: no "
			  "suitable transfer units!\n");
			
		return -EIO;
	    }
	}
    } while (xfer == 0xffff);

    eun = 0;
    if ((jiffies % shuffle_freq) == 0) {
	DEBUG(1, "ftl_cs: recycling freshest block...\n");
	best = 0xffffffff;
	for (i = 0; i < part->DataUnits; i++)
	    if (part->EUNInfo[i].EraseCount <= best) {
		best = part->EUNInfo[i].EraseCount;
		eun = i;
	    }
    } else {
	best = 0;
	for (i = 0; i < part->DataUnits; i++)
	    if (part->EUNInfo[i].Deleted >= best) {
		best = part->EUNInfo[i].Deleted;
		eun = i;
	    }
	if (best == 0) {
	    static int ne = 0;
	    if (++ne < 5)
		printk(KERN_NOTICE "ftl_cs: reclaim failed: "
		       "no free blocks!\n");
	    else
		DEBUG(1,"ftl_cs: reclaim failed: "
		       "no free blocks!\n");

	    return -EIO;
	}
    }
    ret = copy_erase_unit(part, eun, xfer);
    if (!ret)
	erase_xfer(part, xfer);
    else
	printk(KERN_NOTICE "ftl_cs: copy_erase_unit failed!\n");
    return ret;
} /* reclaim_block */

/*======================================================================

    Find_free() searches for a free block.  If necessary, it updates
    the BAM cache for the erase unit containing the free block.  It
    returns the block index -- the erase unit is just the currently
    cached unit.  If there are no free blocks, it returns 0 -- this
    is never a valid data block because it contains the header.
    
======================================================================*/

#ifdef PSYCHO_DEBUG
static void dump_lists(partition_t *part)
{
    int i;
    printk(KERN_DEBUG "ftl_cs: Free total = %d\n", part->FreeTotal);
    for (i = 0; i < part->DataUnits; i++)
	printk(KERN_DEBUG "ftl_cs:   unit %d: %d phys, %d free, "
	       "%d deleted\n", i,
	       part->EUNInfo[i].Offset >> part->header.EraseUnitSize,
	       part->EUNInfo[i].Free, part->EUNInfo[i].Deleted);
}
#endif

static u_int32_t find_free(partition_t *part)
{
    u_int16_t stop, eun;
    u_int32_t blk;
    size_t retlen;
    int ret;
    
    /* Find an erase unit with some free space */
    stop = (part->bam_index == 0xffff) ? 0 : part->bam_index;
    eun = stop;
    do {
	if (part->EUNInfo[eun].Free != 0) break;
	/* Wrap around at end of table */
	if (++eun == part->DataUnits) eun = 0;
    } while (eun != stop);

    if (part->EUNInfo[eun].Free == 0)
	return 0;
    
    /* Is this unit's BAM cached? */
    if (eun != part->bam_index) {
	/* Invalidate cache */
	part->bam_index = 0xffff;

	ret = part->mtd->read(part->mtd, 
		       part->EUNInfo[eun].Offset + le32_to_cpu(part->header.BAMOffset),
		       part->BlocksPerUnit * sizeof(u_int32_t),
		       &retlen, (u_char *) (part->bam_cache));
	
	if (ret) {
	    printk(KERN_WARNING"ftl: Error reading BAM in find_free\n");
	    return 0;
	}
	part->bam_index = eun;
    }

    /* Find a free block */
    for (blk = 0; blk < part->BlocksPerUnit; blk++)
	if (BLOCK_FREE(le32_to_cpu(part->bam_cache[blk]))) break;
    if (blk == part->BlocksPerUnit) {
#ifdef PSYCHO_DEBUG
	static int ne = 0;
	if (++ne == 1)
	    dump_lists(part);
#endif
	printk(KERN_NOTICE "ftl_cs: bad free list!\n");
	return 0;
    }
    DEBUG(2, "ftl_cs: found free block at %d in %d\n", blk, eun);
    return blk;
    
} /* find_free */

/*======================================================================

    This gets a memory handle for the region corresponding to the
    minor device number.
    
======================================================================*/

static int ftl_open(struct inode *inode, struct file *file)
{
    int minor = MINOR(inode->i_rdev);
    partition_t *partition;

    if (minor>>4 >= MAX_MTD_DEVICES)
	return -ENODEV;

    partition = myparts[minor>>4];

    if (!partition)
	return -ENODEV;

    if (partition->state != FTL_FORMATTED)
	return -ENXIO;
    
    if (ftl_gendisk.part[minor].nr_sects == 0)
	return -ENXIO;

    BLK_INC_USE_COUNT;

    if (!get_mtd_device(partition->mtd, -1)) {
	    BLK_DEC_USE_COUNT;
	    return -ENXIO;
    }
    
    if ((file->f_mode & 2) && !(partition->mtd->flags & MTD_CLEAR_BITS) ) {
	    put_mtd_device(partition->mtd);
	    BLK_DEC_USE_COUNT;
            return -EROFS;
    }
    
    DEBUG(0, "ftl_cs: ftl_open(%d)\n", minor);

    atomic_inc(&partition->open);

    return 0;
}

/*====================================================================*/

static release_t ftl_close(struct inode *inode, struct file *file)
{
    int minor = MINOR(inode->i_rdev);
    partition_t *part = myparts[minor >> 4];
    int i;
    
    DEBUG(0, "ftl_cs: ftl_close(%d)\n", minor);

    /* Wait for any pending erase operations to complete */
    if (part->mtd->sync)
	    part->mtd->sync(part->mtd);
    
    for (i = 0; i < part->header.NumTransferUnits; i++) {
	if (part->XferInfo[i].state == XFER_ERASED)
	    prepare_xfer(part, i);
    }

    atomic_dec(&part->open);

    put_mtd_device(part->mtd);
    BLK_DEC_USE_COUNT;
    release_return(0);
} /* ftl_close */


/*======================================================================

    Read a series of sectors from an FTL partition.
    
======================================================================*/

static int ftl_read(partition_t *part, caddr_t buffer,
		    u_long sector, u_long nblocks)
{
    u_int32_t log_addr, bsize;
    u_long i;
    int ret;
    size_t offset, retlen;
    
    DEBUG(2, "ftl_cs: ftl_read(0x%p, 0x%lx, %ld)\n",
	  part, sector, nblocks);
    if (!(part->state & FTL_FORMATTED)) {
	printk(KERN_NOTICE "ftl_cs: bad partition\n");
	return -EIO;
    }
    bsize = 1 << part->header.EraseUnitSize;

    for (i = 0; i < nblocks; i++) {
	if (((sector+i) * SECTOR_SIZE) >= le32_to_cpu(part->header.FormattedSize)) {
	    printk(KERN_NOTICE "ftl_cs: bad read offset\n");
	    return -EIO;
	}
	log_addr = part->VirtualBlockMap[sector+i];
	if (log_addr == 0xffffffff)
	    memset(buffer, 0, SECTOR_SIZE);
	else {
	    offset = (part->EUNInfo[log_addr / bsize].Offset
			  + (log_addr % bsize));
	    ret = part->mtd->read(part->mtd, offset, SECTOR_SIZE,
			   &retlen, (u_char *) buffer);

	    if (ret) {
		printk(KERN_WARNING "Error reading MTD device in ftl_read()\n");
		return ret;
	    }
	}
	buffer += SECTOR_SIZE;
    }
    return 0;
} /* ftl_read */

/*======================================================================

    Write a series of sectors to an FTL partition
    
======================================================================*/

static int set_bam_entry(partition_t *part, u_int32_t log_addr,
			 u_int32_t virt_addr)
{
    u_int32_t bsize, blk, le_virt_addr;
#ifdef PSYCHO_DEBUG
    u_int32_t old_addr;
#endif
    u_int16_t eun;
    int ret;
    size_t retlen, offset;

    DEBUG(2, "ftl_cs: set_bam_entry(0x%p, 0x%x, 0x%x)\n",
	  part, log_addr, virt_addr);
    bsize = 1 << part->header.EraseUnitSize;
    eun = log_addr / bsize;
    blk = (log_addr % bsize) / SECTOR_SIZE;
    offset = (part->EUNInfo[eun].Offset + blk * sizeof(u_int32_t) +
		  le32_to_cpu(part->header.BAMOffset));
    
#ifdef PSYCHO_DEBUG
    ret = part->mtd->read(part->mtd, offset, sizeof(u_int32_t),
                        &retlen, (u_char *)&old_addr);
    if (ret) {
	printk(KERN_WARNING"ftl: Error reading old_addr in set_bam_entry: %d\n",ret);
	return ret;
    }
    old_addr = le32_to_cpu(old_addr);

    if (((virt_addr == 0xfffffffe) && !BLOCK_FREE(old_addr)) ||
	((virt_addr == 0) && (BLOCK_TYPE(old_addr) != BLOCK_DATA)) ||
	(!BLOCK_DELETED(virt_addr) && (old_addr != 0xfffffffe))) {
	static int ne = 0;
	if (++ne < 5) {
	    printk(KERN_NOTICE "ftl_cs: set_bam_entry() inconsistency!\n");
	    printk(KERN_NOTICE "ftl_cs:   log_addr = 0x%x, old = 0x%x"
		   ", new = 0x%x\n", log_addr, old_addr, virt_addr);
	}
	return -EIO;
    }
#endif
    le_virt_addr = cpu_to_le32(virt_addr);
    if (part->bam_index == eun) {
#ifdef PSYCHO_DEBUG
	if (le32_to_cpu(part->bam_cache[blk]) != old_addr) {
	    static int ne = 0;
	    if (++ne < 5) {
		printk(KERN_NOTICE "ftl_cs: set_bam_entry() "
		       "inconsistency!\n");
		printk(KERN_NOTICE "ftl_cs:   log_addr = 0x%x, cache"
		       " = 0x%x\n",
		       le32_to_cpu(part->bam_cache[blk]), old_addr);
	    }
	    return -EIO;
	}
#endif
	part->bam_cache[blk] = le_virt_addr;
    }
    ret = part->mtd->write(part->mtd, offset, sizeof(u_int32_t),
                            &retlen, (u_char *)&le_virt_addr);

    if (ret) {
	printk(KERN_NOTICE "ftl_cs: set_bam_entry() failed!\n");
	printk(KERN_NOTICE "ftl_cs:   log_addr = 0x%x, new = 0x%x\n",
	       log_addr, virt_addr);
    }
    return ret;
} /* set_bam_entry */

static int ftl_write(partition_t *part, caddr_t buffer,
		     u_long sector, u_long nblocks)
{
    u_int32_t bsize, log_addr, virt_addr, old_addr, blk;
    u_long i;
    int ret;
    size_t retlen, offset;

    DEBUG(2, "ftl_cs: ftl_write(0x%p, %ld, %ld)\n",
	  part, sector, nblocks);
    if (!(part->state & FTL_FORMATTED)) {
	printk(KERN_NOTICE "ftl_cs: bad partition\n");
	return -EIO;
    }
    /* See if we need to reclaim space, before we start */
    while (part->FreeTotal < nblocks) {
	ret = reclaim_block(part);
	if (ret)
	    return ret;
    }
    
    bsize = 1 << part->header.EraseUnitSize;

    virt_addr = sector * SECTOR_SIZE | BLOCK_DATA;
    for (i = 0; i < nblocks; i++) {
	if (virt_addr >= le32_to_cpu(part->header.FormattedSize)) {
	    printk(KERN_NOTICE "ftl_cs: bad write offset\n");
	    return -EIO;
	}

	/* Grab a free block */
	blk = find_free(part);
	if (blk == 0) {
	    static int ne = 0;
	    if (++ne < 5)
		printk(KERN_NOTICE "ftl_cs: internal error: "
		       "no free blocks!\n");
	    return -ENOSPC;
	}

	/* Tag the BAM entry, and write the new block */
	log_addr = part->bam_index * bsize + blk * SECTOR_SIZE;
	part->EUNInfo[part->bam_index].Free--;
	part->FreeTotal--;
	if (set_bam_entry(part, log_addr, 0xfffffffe)) 
	    return -EIO;
	part->EUNInfo[part->bam_index].Deleted++;
	offset = (part->EUNInfo[part->bam_index].Offset +
		      blk * SECTOR_SIZE);
	ret = part->mtd->write(part->mtd, offset, SECTOR_SIZE, &retlen, 
                                     buffer);

	if (ret) {
	    printk(KERN_NOTICE "ftl_cs: block write failed!\n");
	    printk(KERN_NOTICE "ftl_cs:   log_addr = 0x%x, virt_addr"
		   " = 0x%x, Offset = 0x%x\n", log_addr, virt_addr,
		   offset);
	    return -EIO;
	}
	
	/* Only delete the old entry when the new entry is ready */
	old_addr = part->VirtualBlockMap[sector+i];
	if (old_addr != 0xffffffff) {
	    part->VirtualBlockMap[sector+i] = 0xffffffff;
	    part->EUNInfo[old_addr/bsize].Deleted++;
	    if (set_bam_entry(part, old_addr, 0))
		return -EIO;
	}

	/* Finally, set up the new pointers */
	if (set_bam_entry(part, log_addr, virt_addr))
	    return -EIO;
	part->VirtualBlockMap[sector+i] = log_addr;
	part->EUNInfo[part->bam_index].Deleted--;
	
	buffer += SECTOR_SIZE;
	virt_addr += SECTOR_SIZE;
    }
    return 0;
} /* ftl_write */

/*======================================================================

    IOCTL calls for getting device parameters.

======================================================================*/

static int ftl_ioctl(struct inode *inode, struct file *file,
		     u_int cmd, u_long arg)
{
    struct hd_geometry *geo = (struct hd_geometry *)arg;
    int ret = 0, minor = MINOR(inode->i_rdev);
    partition_t *part= myparts[minor >> 4];
    u_long sect;

    if (!part)
	return -ENODEV; /* How? */

    switch (cmd) {
    case HDIO_GETGEO:
	ret = verify_area(VERIFY_WRITE, (long *)arg, sizeof(*geo));
	if (ret) return ret;
	/* Sort of arbitrary: round size down to 4K boundary */
	sect = le32_to_cpu(part->header.FormattedSize)/SECTOR_SIZE;
	put_user(1, (char *)&geo->heads);
	put_user(8, (char *)&geo->sectors);
	put_user((sect>>3), (short *)&geo->cylinders);
	put_user(ftl_hd[minor].start_sect, (u_long *)&geo->start);
	break;
    case BLKGETSIZE:
	ret = put_user(ftl_hd[minor].nr_sects, (unsigned long *)arg);
	break;
#ifdef BLKGETSIZE64
    case BLKGETSIZE64:
	ret = put_user((u64)ftl_hd[minor].nr_sects << 9, (u64 *)arg);
	break;
#endif
    case BLKRRPART:
	ret = ftl_reread_partitions(minor);
	break;
#if (LINUX_VERSION_CODE < 0x20303)
    case BLKFLSBUF:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	if (!capable(CAP_SYS_ADMIN)) return -EACCES;
#endif
	fsync_dev(inode->i_rdev);
	invalidate_buffers(inode->i_rdev);
	break;
    RO_IOCTLS(inode->i_rdev, arg);
#else
    case BLKROSET:
    case BLKROGET:
    case BLKFLSBUF:
	ret = blk_ioctl(inode->i_rdev, cmd, arg);
	break;
#endif
    default:
	ret = -EINVAL;
    }

    return ret;
} /* ftl_ioctl */

/*======================================================================

    Handler for block device requests

======================================================================*/

static int ftl_reread_partitions(int minor)
{
    partition_t *part = myparts[minor >> 4];
    int i, whole;

    DEBUG(0, "ftl_cs: ftl_reread_partition(%d)\n", minor);
    if ((atomic_read(&part->open) > 1)) {
	    return -EBUSY;
    }
    whole = minor & ~(MAX_PART-1);

    i = MAX_PART - 1;
    while (i-- > 0) {
	if (ftl_hd[whole+i].nr_sects > 0) {
	    kdev_t rdev = MKDEV(FTL_MAJOR, whole+i);

	    invalidate_device(rdev, 1);
	}
	ftl_hd[whole+i].start_sect = 0;
	ftl_hd[whole+i].nr_sects = 0;
    }

    scan_header(part);

    register_disk(&ftl_gendisk, whole >> PART_BITS, MAX_PART,
		  &ftl_blk_fops, le32_to_cpu(part->header.FormattedSize)/SECTOR_SIZE);

#ifdef PCMCIA_DEBUG
    for (i = 0; i < MAX_PART; i++) {
	if (ftl_hd[whole+i].nr_sects > 0)
	    printk(KERN_INFO "  %d: start %ld size %ld\n", i,
		   ftl_hd[whole+i].start_sect,
		   ftl_hd[whole+i].nr_sects);
    }
#endif
    return 0;
}

/*======================================================================

    Handler for block device requests

======================================================================*/

static void do_ftl_request(request_arg_t)
{
    int ret, minor;
    partition_t *part;

    do {
      //	    sti();
	INIT_REQUEST;

	minor = MINOR(CURRENT->rq_dev);
	
	part = myparts[minor >> 4];
	if (part) {
	  ret = 0;
	  
	  switch (CURRENT->cmd) {
	  case READ:
	    ret = ftl_read(part, CURRENT->buffer,
			   CURRENT->sector+ftl_hd[minor].start_sect,
			   CURRENT->current_nr_sectors);
	    if (ret) printk("ftl_read returned %d\n", ret);
	    break;
	    
	  case WRITE:
	    ret = ftl_write(part, CURRENT->buffer,
			    CURRENT->sector+ftl_hd[minor].start_sect,
			    CURRENT->current_nr_sectors);
	    if (ret) printk("ftl_write returned %d\n", ret);
	    break;
	    
	  default:
	    panic("ftl_cs: unknown block command!\n");
	    
	  }
	} else {
	  ret = 1;
	  printk("NULL part in ftl_request\n");
	}
	 
	if (!ret) {
	  CURRENT->sector += CURRENT->current_nr_sectors;
	}
	
	end_request((ret == 0) ? 1 : 0);
    } while (1);
} /* do_ftl_request */

/*====================================================================*/

void ftl_freepart(partition_t *part)
{
    if (part->VirtualBlockMap) {
	vfree(part->VirtualBlockMap);
	part->VirtualBlockMap = NULL;
    }
    if (part->VirtualPageMap) {
	kfree(part->VirtualPageMap);
	part->VirtualPageMap = NULL;
    }
    if (part->EUNInfo) {
	kfree(part->EUNInfo);
	part->EUNInfo = NULL;
    }
    if (part->XferInfo) {
	kfree(part->XferInfo);
	part->XferInfo = NULL;
    }
    if (part->bam_cache) {
	kfree(part->bam_cache);
	part->bam_cache = NULL;
    }
    
} /* ftl_freepart */

static void ftl_notify_add(struct mtd_info *mtd)
{
	partition_t *partition;
	int device;

	for (device=0; device < MAX_MTD_DEVICES && myparts[device]; device++)
		;

	if (device == MAX_MTD_DEVICES) {
		printk(KERN_NOTICE "Maximum number of FTL partitions reached\n"
		       "Not scanning <%s>\n", mtd->name);
		return;
	}

	partition = kmalloc(sizeof(partition_t), GFP_KERNEL);
		
	if (!partition) {
		printk(KERN_WARNING "No memory to scan for FTL on %s\n",
		       mtd->name);
		return;
	}    

	memset(partition, 0, sizeof(partition_t));

	partition->mtd = mtd;

	if ((scan_header(partition) == 0) && 
	    (build_maps(partition) == 0)) {
		
		partition->state = FTL_FORMATTED;
		atomic_set(&partition->open, 0);
		myparts[device] = partition;
		ftl_reread_partitions(device << 4);
#ifdef PCMCIA_DEBUG
		printk(KERN_INFO "ftl_cs: opening %d kb FTL partition\n",
		       le32_to_cpu(partition->header.FormattedSize) >> 10);
#endif
	} else
		kfree(partition);
}

static void ftl_notify_remove(struct mtd_info *mtd)
{
        int i,j;

	/* Q: What happens if you try to remove a device which has
	 *    a currently-open FTL partition on it?
	 *
	 * A: You don't. The ftl_open routine is responsible for
	 *    increasing the use count of the driver module which
	 *    it uses.
	 */

	/* That's the theory, anyway :) */

	for (i=0; i< MAX_MTD_DEVICES; i++)
		if (myparts[i] && myparts[i]->mtd == mtd) {

			if (myparts[i]->state == FTL_FORMATTED)
				ftl_freepart(myparts[i]);
			
			myparts[i]->state = 0;
			for (j=0; j<16; j++) {
				ftl_gendisk.part[j].nr_sects=0;
				ftl_gendisk.part[j].start_sect=0;
			}
			kfree(myparts[i]);
			myparts[i] = NULL;
		}
}

int init_ftl(void)
{
    int i;

    memset(myparts, 0, sizeof(myparts));
    
    DEBUG(0, "$Id: ftl.c,v 1.45 2003/01/24 23:31:27 dwmw2 Exp $\n");
    
    if (register_blkdev(FTL_MAJOR, "ftl", &ftl_blk_fops)) {
	printk(KERN_NOTICE "ftl_cs: unable to grab major "
	       "device number!\n");
	return -EAGAIN;
    }
    
    for (i = 0; i < MINOR_NR(MAX_DEV, 0, 0); i++)
	ftl_blocksizes[i] = 1024;
    for (i = 0; i < MAX_DEV*MAX_PART; i++) {
	ftl_hd[i].nr_sects = 0;
	ftl_hd[i].start_sect = 0;
    }
    blksize_size[FTL_MAJOR] = ftl_blocksizes;
    ftl_gendisk.major = FTL_MAJOR;
    blk_init_queue(BLK_DEFAULT_QUEUE(FTL_MAJOR), &do_ftl_request);
    add_gendisk(&ftl_gendisk);
    
    register_mtd_user(&ftl_notifier);
    
    return 0;
}

static void __exit cleanup_ftl(void)
{
    unregister_mtd_user(&ftl_notifier);

    unregister_blkdev(FTL_MAJOR, "ftl");
    blk_cleanup_queue(BLK_DEFAULT_QUEUE(FTL_MAJOR));
    blksize_size[FTL_MAJOR] = NULL;

    del_gendisk(&ftl_gendisk);
}

module_init(init_ftl);
module_exit(cleanup_ftl);


MODULE_LICENSE("Dual MPL/GPL");
MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("Support code for Flash Translation Layer, used on PCMCIA devices");
