/*
 *    Disk Array driver for HP SA 5xxx and 6xxx Controllers
 *    Copyright 2000, 2002 Hewlett-Packard Development Company, L.P. 
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to Cciss-discuss@lists.sourceforge.net
 *
 */

#include <linux/config.h>	/* CONFIG_PROC_FS */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/init.h> 
#include <linux/hdreg.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/smp_lock.h>

#include <linux/blk.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>

#define CCISS_DRIVER_VERSION(maj,min,submin) ((maj<<16)|(min<<8)|(submin))
#define DRIVER_NAME "HP CISS Driver (v 2.4.50)"
#define DRIVER_VERSION CCISS_DRIVER_VERSION(2,4,50)

/* Embedded module documentation macros - see modules.h */
MODULE_AUTHOR("Hewlett-Packard Company");
MODULE_DESCRIPTION("Driver for HP SA5xxx SA6xxx Controllers version 2.4.50");
MODULE_SUPPORTED_DEVICE("HP SA5i SA5i+ SA532 SA5300 SA5312 SA641 SA642 SA6400 6i"); 
MODULE_LICENSE("GPL");

#include "cciss_cmd.h"
#include "cciss.h"
#include <linux/cciss_ioctl.h>

/* define the PCI info for the cards we can control */
const struct pci_device_id cciss_pci_device_id[] = {
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISS,
			0x0E11, 0x4070, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSB,
                        0x0E11, 0x4080, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSB,
                        0x0E11, 0x4082, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSB,
                        0x0E11, 0x4083, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC,
                        0x0E11, 0x409A, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC,
                        0x0E11, 0x409B, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC,
                        0x0E11, 0x409C, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC,
                        0x0E11, 0x409D, 0, 0, 0},
	{ PCI_VENDOR_ID_COMPAQ, PCI_DEVICE_ID_COMPAQ_CISSC,
                        0x0E11, 0x4091, 0, 0, 0},
	{0,}
};
MODULE_DEVICE_TABLE(pci, cciss_pci_device_id);

#define NR_PRODUCTS (sizeof(products)/sizeof(struct board_type))

/*  board_id = Subsystem Device ID & Vendor ID
 *  product = Marketing Name for the board
 *  access = Address of the struct of function pointers 
 */
static struct board_type products[] = {
	{ 0x40700E11, "Smart Array 5300", &SA5_access},
	{ 0x40800E11, "Smart Array 5i", &SA5B_access},
	{ 0x40820E11, "Smart Array 532", &SA5B_access},
	{ 0x40830E11, "Smart Array 5312", &SA5B_access},
	{ 0x409A0E11, "Smart Array 641", &SA5_access},
	{ 0x409B0E11, "Smart Array 642", &SA5_access},
	{ 0x409C0E11, "Smart Array 6400", &SA5_access},
	{ 0x409D0E11, "Smart Array 6400 EM", &SA5_access},
	{ 0x40910E11, "Smart Array 6i", &SA5_access},
};

/* How long to wait (in millesconds) for board to go into simple mode */
#define MAX_CONFIG_WAIT 30000 
#define MAX_IOCTL_CONFIG_WAIT 1000

/*define how many times we will try a command because of bus resets */
#define MAX_CMD_RETRIES 3

#define READ_AHEAD 	 128
#define NR_CMDS		 128 /* #commands that can be outstanding */
#define MAX_CTLR	 32 

/* No sense in giving up our preallocated major numbers */
#if MAX_CTLR < 8
#error"cciss.c: MAX_CTLR must be 8 or greater"
#endif

/* Originally cciss driver only supports 8 major number */
#define MAX_CTLR_ORIG  COMPAQ_CISS_MAJOR7 - COMPAQ_CISS_MAJOR + 1

#define CCISS_DMA_MASK 0xFFFFFFFFFFFFFFFF /* 64 bit DMA */

#ifdef CONFIG_CISS_MONITOR_THREAD
static int cciss_monitor(void *ctlr);
static int start_monitor_thread(ctlr_info_t *h, unsigned char *cmd, 
		unsigned long count, int (*cciss_monitor)(void *), int *rc);
static u32 heartbeat_timer = 0;
#else
#define cciss_monitor(x)
#define kill_monitor_thead(x)
#endif

static ctlr_info_t *hba[MAX_CTLR];
static int map_major_to_ctlr[MAX_BLKDEV] = {0}; /* gets ctlr num from maj num */
static struct proc_dir_entry *proc_cciss;

static void do_cciss_request(request_queue_t *q);
static int cciss_open(struct inode *inode, struct file *filep);
static int cciss_release(struct inode *inode, struct file *filep);
static int cciss_ioctl(struct inode *inode, struct file *filep, 
		unsigned int cmd, unsigned long arg);

static int revalidate_logvol(kdev_t dev, int maxusage);
static int frevalidate_logvol(kdev_t dev);
static int deregister_disk(int ctlr, int logvol);
static int register_new_disk(int cltr, int opened_vol, __u64 requested_lun);
static int cciss_rescan_disk(int cltr, int logvol);

static void cciss_getgeometry(int cntl_num);

static inline void addQ(CommandList_struct **Qptr, CommandList_struct *c);
static void start_io( ctlr_info_t *h);

#ifdef CONFIG_PROC_FS
static int cciss_proc_get_info(char *buffer, char **start, off_t offset, 
		int length, int *eof, void *data);
static void cciss_procinit(int i);
#else
static int cciss_proc_get_info(char *buffer, char **start, off_t offset, 
		int length, int *eof, void *data) { return 0;}
static void cciss_procinit(int i) {}
#endif /* CONFIG_PROC_FS */

static struct block_device_operations cciss_fops  = {
	owner:			THIS_MODULE,
	open:			cciss_open, 
	release:        	cciss_release,
        ioctl:			cciss_ioctl,
	revalidate:		frevalidate_logvol,
};

#include "cciss_scsi.c"		/* For SCSI tape support */

#define ENG_GIG	1048576000
#define ENG_GIG_FACTOR (ENG_GIG/512)
#define	RAID_UNKNOWN 6
static const char *raid_label[] = {"0","4","1(0+1)","5","5+1","ADG",
				   "UNKNOWN"};
/*
 * Report information about this controller.
 */
#ifdef CONFIG_PROC_FS
static int cciss_proc_get_info(char *buffer, char **start, off_t offset, 
		int length, int *eof, void *data)
{
	off_t pos = 0;
	off_t len = 0;
	int size, i, ctlr;
	ctlr_info_t *h = (ctlr_info_t*)data;
	drive_info_struct *drv;
	unsigned long flags;
	unsigned int vol_sz, vol_sz_frac;

	spin_lock_irqsave(&io_request_lock, flags);
	if (h->busy_configuring) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return -EBUSY;
	}
	h->busy_configuring = 1;
	spin_unlock_irqrestore(&io_request_lock, flags);
		
	ctlr = h->ctlr;
	size = sprintf(buffer, "%s: HP %s Controller\n"
 		"Board ID: 0x%08lx\n"
		"Firmware Version: %c%c%c%c\n"
 		"IRQ: %d\n"
 		"Logical drives: %d\n"
 		"Current Q depth: %d\n"
 		"Current # commands on controller: %d\n"
 		"Max Q depth since init: %d\n"
		"Max # commands on controller since init: %d\n"
		"Max SG entries since init: %d\n"
		MONITOR_PERIOD_PATTERN 
		MONITOR_DEADLINE_PATTERN
		MONITOR_STATUS_PATTERN 
		"\n",
  		h->devname,
  		h->product_name,
  		(unsigned long)h->board_id,
  		h->firm_ver[0], h->firm_ver[1], h->firm_ver[2], h->firm_ver[3],
  		(unsigned int)h->intr,
  		h->num_luns, 
  		h->Qdepth, h->commands_outstanding,
		h->maxQsinceinit, h->max_outstanding, h->maxSG,
		MONITOR_PERIOD_VALUE(h),
		MONITOR_DEADLINE_VALUE(h),
		CTLR_STATUS(h));
  
	pos += size; len += size;
	cciss_proc_tape_report(ctlr, buffer, &pos, &len);
	for(i=0; i<=h->highest_lun; i++) {
		drv = &h->drv[i];
		if (drv->nr_blocks == 0)
			continue;
		vol_sz = drv->nr_blocks/ENG_GIG_FACTOR; 
		vol_sz_frac = (drv->nr_blocks%ENG_GIG_FACTOR)*100/ENG_GIG_FACTOR;

		if (drv->raid_level > 5)
			drv->raid_level = RAID_UNKNOWN;
		size = sprintf(buffer+len, "cciss/c%dd%d:"
				"\t%4d.%02dGB\tRAID %s\n",
		       		 ctlr, i, vol_sz,vol_sz_frac,
				 raid_label[drv->raid_level]);
		pos += size, len += size;
        }

	*eof = 1;
	*start = buffer+offset;
	len -= offset;
	if (len>length)
		len = length;
	h->busy_configuring = 0;
	return len;
}

static int
cciss_proc_write(struct file *file, const char *buffer,
			unsigned long count, void *data)
{
	unsigned char cmd[80];
	int len;
	ctlr_info_t *h = (ctlr_info_t *) data;
	int rc;

	if (count > sizeof(cmd)-1) 
		return -EINVAL;
	if (copy_from_user(cmd, buffer, count)) 
		return -EFAULT;
	cmd[count] = '\0';
	len = strlen(cmd);	
	if (cmd[len-1] == '\n')
		cmd[--len] = '\0';

#	ifdef CONFIG_CISS_SCSI_TAPE
		if (strcmp("engage scsi", cmd)==0) {
			rc = cciss_engage_scsi(h->ctlr);
			if (rc != 0) 
				return -rc;
			return count;
		}
		/* might be nice to have "disengage" too, but it's not
		   safely possible. (only 1 module use count, lock issues.) */
#	endif

	if (START_MONITOR_THREAD(h, cmd, count, cciss_monitor, &rc) == 0)
		return rc;
	
	return -EINVAL;
}

/*
 * Get us a file in /proc/cciss that says something about each controller.
 * Create /proc/cciss if it doesn't exist yet.
 */
static void __init cciss_procinit(int i)
{
	struct proc_dir_entry *pde;

	if (proc_cciss == NULL) {
		proc_cciss = proc_mkdir("cciss", proc_root_driver);
		if (!proc_cciss) {
			printk("cciss:  proc_mkdir failed\n");
			return;
		}
	}

	pde = create_proc_read_entry(hba[i]->devname,
		S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH,
		proc_cciss, cciss_proc_get_info, hba[i]);
	pde->write_proc = cciss_proc_write;
}
#endif /* CONFIG_PROC_FS */

/* 
 * For operations that cannot sleep, a command block is allocated at init, 
 * and managed by cmd_alloc() and cmd_free() using a simple bitmap to track
 * which ones are free or in use.  For operations that can wait for kmalloc 
 * to possible sleep, this routine can be called with get_from_pool set to 0. 
 * cmd_free() MUST be called with a got_from_pool set to 0 if cmd_alloc was. 
 */ 
static CommandList_struct * cmd_alloc(ctlr_info_t *h, int get_from_pool)
{
	CommandList_struct *c;
	int i; 
	u64bit temp64;
	dma_addr_t cmd_dma_handle, err_dma_handle;

	if (!get_from_pool) {
		c = (CommandList_struct *) pci_alloc_consistent(
			h->pdev, sizeof(CommandList_struct), &cmd_dma_handle); 
        	if (c==NULL)
                 	return NULL;
		memset(c, 0, sizeof(CommandList_struct));

		c->err_info = (ErrorInfo_struct *)pci_alloc_consistent(
					h->pdev, sizeof(ErrorInfo_struct), 
					&err_dma_handle);
	
		if (c->err_info == NULL)
		{
			pci_free_consistent(h->pdev, 
				sizeof(CommandList_struct), c, cmd_dma_handle);
			return NULL;
		}
		memset(c->err_info, 0, sizeof(ErrorInfo_struct));
	} else /* get it out of the controllers pool */ 
	{
	     	do {
                	i = find_first_zero_bit(h->cmd_pool_bits, NR_CMDS);
                        if (i == NR_CMDS)
                                return NULL;
                } while(test_and_set_bit(i%32, h->cmd_pool_bits+(i/32)) != 0);
#ifdef CCISS_DEBUG
		printk(KERN_DEBUG "cciss: using command buffer %d\n", i);
#endif
                c = h->cmd_pool + i;
		memset(c, 0, sizeof(CommandList_struct));
		cmd_dma_handle = h->cmd_pool_dhandle 
					+ i*sizeof(CommandList_struct);
		c->err_info = h->errinfo_pool + i;
		memset(c->err_info, 0, sizeof(ErrorInfo_struct));
		err_dma_handle = h->errinfo_pool_dhandle 
					+ i*sizeof(ErrorInfo_struct);
                h->nr_allocs++;
        }

	c->busaddr = (__u32) cmd_dma_handle;
	temp64.val = (__u64) err_dma_handle;	
	c->ErrDesc.Addr.lower = temp64.val32.lower;
	c->ErrDesc.Addr.upper = temp64.val32.upper;
	c->ErrDesc.Len = sizeof(ErrorInfo_struct);
	
	c->ctlr = h->ctlr;
        return c;


}

/* 
 * Frees a command block that was previously allocated with cmd_alloc(). 
 */
static void cmd_free(ctlr_info_t *h, CommandList_struct *c, int got_from_pool)
{
	int i;
	u64bit temp64;

	if (!got_from_pool) { 
		temp64.val32.lower = c->ErrDesc.Addr.lower;
		temp64.val32.upper = c->ErrDesc.Addr.upper;
		pci_free_consistent(h->pdev, sizeof(ErrorInfo_struct), 
			c->err_info, (dma_addr_t) temp64.val);
		pci_free_consistent(h->pdev, sizeof(CommandList_struct), 
			c, (dma_addr_t) c->busaddr);
	} else 
	{
		i = c - h->cmd_pool;
		clear_bit(i%32, h->cmd_pool_bits+(i/32));
                h->nr_frees++;
        }
}

/*  
 * fills in the disk information. 
 */
static void cciss_geninit( int ctlr)
{
	drive_info_struct *drv;
	int i,j;
	
	/* Loop through each real device */ 
	hba[ctlr]->gendisk.nr_real = 0; 
	for(i=0; i< NWD; i++) {
		drv = &(hba[ctlr]->drv[i]);
		if (!(drv->nr_blocks))
			continue;
		hba[ctlr]->hd[i << NWD_SHIFT].nr_sects = 
		hba[ctlr]->sizes[i << NWD_SHIFT] = drv->nr_blocks;

		/* for each partition */ 
		for(j=0; j<MAX_PART; j++) {
			hba[ctlr]->blocksizes[(i<<NWD_SHIFT) + j] = 1024; 

			hba[ctlr]->hardsizes[ (i<<NWD_SHIFT) + j] = 
				drv->block_size;
		}
	}
	hba[ctlr]->gendisk.nr_real = hba[ctlr]->highest_lun+1;
}
/*
 * Open.  Make sure the device is really there.
 */
static int cciss_open(struct inode *inode, struct file *filep)
{
 	int ctlr = map_major_to_ctlr[MAJOR(inode->i_rdev)];
	int dsk  = MINOR(inode->i_rdev) >> NWD_SHIFT;

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss_open %x (%x:%x)\n", inode->i_rdev, ctlr, dsk);
#endif /* CCISS_DEBUG */ 

	if (ctlr > MAX_CTLR || hba[ctlr] == NULL || !CTLR_IS_ALIVE(hba[ctlr]))
		return -ENXIO;
	/*
	 * Root is allowed to open raw volume zero even if its not configured
	 * so array config can still work. Root is also allowed to open any
	 * volume that has a LUN ID, so it can issue IOCTL to reread the
	 * disk information.  I don't think I really like this.
	 * but I'm already using way to many device nodes to claim another one
	 * for "raw controller".
	 */
	if (hba[ctlr]->sizes[MINOR(inode->i_rdev)] == 0) { /* not online? */
		if (MINOR(inode->i_rdev) != 0) {	 /* not node 0? */
			/* if not node 0 make sure it is a partition = 0 */
			if (MINOR(inode->i_rdev) & 0x0f) {
				return -ENXIO;
				/* if it is, make sure we have a LUN ID */
			} else if (hba[ctlr]->drv[MINOR(inode->i_rdev)
					>> NWD_SHIFT].LunID == 0) {
				return -ENXIO;
			}
		}
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
	}

	hba[ctlr]->drv[dsk].usage_count++;
	hba[ctlr]->usage_count++;
	return 0;
}
/*
 * Close.  Sync first.
 */
static int cciss_release(struct inode *inode, struct file *filep)
{
	int ctlr = map_major_to_ctlr[MAJOR(inode->i_rdev)];
	int dsk  = MINOR(inode->i_rdev) >> NWD_SHIFT;

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss_release %x (%x:%x)\n", inode->i_rdev, ctlr, dsk);
#endif /* CCISS_DEBUG */

	/* fsync_dev(inode->i_rdev); */

	hba[ctlr]->drv[dsk].usage_count--;
	hba[ctlr]->usage_count--;
	return 0;
}

/*
 * ioctl 
 */
static int cciss_ioctl(struct inode *inode, struct file *filep, 
		unsigned int cmd, unsigned long arg)
{
	int ctlr = map_major_to_ctlr[MAJOR(inode->i_rdev)];
	int dsk  = MINOR(inode->i_rdev) >> NWD_SHIFT;

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss_ioctl: Called with cmd=%x %lx\n", cmd, arg);
#endif /* CCISS_DEBUG */ 
	
	switch(cmd) {
	   case HDIO_GETGEO:
	   {
		struct hd_geometry driver_geo;
		if (hba[ctlr]->drv[dsk].cylinders) {
			driver_geo.heads = hba[ctlr]->drv[dsk].heads;
			driver_geo.sectors = hba[ctlr]->drv[dsk].sectors;
			driver_geo.cylinders = hba[ctlr]->drv[dsk].cylinders;
		} else 
			return -ENXIO;
		driver_geo.start=
			hba[ctlr]->hd[MINOR(inode->i_rdev)].start_sect;
		if (copy_to_user((void *) arg, &driver_geo,
				sizeof( struct hd_geometry)))
			return  -EFAULT;
		return 0;
	   }
	case HDIO_GETGEO_BIG:
	{
		struct hd_big_geometry driver_geo;
		if (hba[ctlr]->drv[dsk].cylinders) {
			driver_geo.heads = hba[ctlr]->drv[dsk].heads;
			driver_geo.sectors = hba[ctlr]->drv[dsk].sectors;
			driver_geo.cylinders = hba[ctlr]->drv[dsk].cylinders;
		} else 
			return -ENXIO;
		driver_geo.start= 
		hba[ctlr]->hd[MINOR(inode->i_rdev)].start_sect;
		if (copy_to_user((void *) arg, &driver_geo,  
				sizeof( struct hd_big_geometry)))
			return  -EFAULT;
		return 0;
	}
	case BLKRRPART:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		return revalidate_logvol(inode->i_rdev, 1);
	case BLKGETSIZE:
	case BLKGETSIZE64:
	case BLKFLSBUF:
	case BLKBSZSET:
	case BLKBSZGET:
	case BLKROSET:
	case BLKROGET:
	case BLKRASET:
	case BLKRAGET:
	case BLKPG:
	case BLKELVGET:
	case BLKELVSET:
		return blk_ioctl(inode->i_rdev, cmd, arg);
	case CCISS_GETPCIINFO:
	{
		cciss_pci_info_struct pciinfo;

		if (!arg) 
			return -EINVAL;
		pciinfo.bus = hba[ctlr]->pdev->bus->number;
		pciinfo.dev_fn = hba[ctlr]->pdev->devfn;
		pciinfo.board_id = hba[ctlr]->board_id;
		if (copy_to_user((void *) arg, &pciinfo,  sizeof( cciss_pci_info_struct )))
			return  -EFAULT;
		return 0;
	}	
	case CCISS_GETINTINFO:
	{
		cciss_coalint_struct intinfo;
		ctlr_info_t *c = hba[ctlr];

		if (!arg) 
			return -EINVAL;
		intinfo.delay = readl(&c->cfgtable->HostWrite.CoalIntDelay);
		intinfo.count = readl(&c->cfgtable->HostWrite.CoalIntCount);
		if (copy_to_user((void *) arg, &intinfo, sizeof( cciss_coalint_struct )))
			return -EFAULT;
                return 0;
        }
	case CCISS_SETINTINFO:
        {
                cciss_coalint_struct intinfo;
                ctlr_info_t *c = hba[ctlr];
		unsigned long flags;
		int i;

		if (!arg) 
			return -EINVAL;	
		if (!capable(CAP_SYS_ADMIN)) 
			return -EPERM;
		if (copy_from_user(&intinfo, (void *) arg, sizeof( cciss_coalint_struct)))
			return -EFAULT;
		if ( (intinfo.delay == 0 ) && (intinfo.count == 0)) {
			return -EINVAL;
		}

		spin_lock_irqsave(&io_request_lock, flags);
		/* Can only safely update if no commands outstanding */ 
		if (c->commands_outstanding > 0 ) {
			spin_unlock_irqrestore(&io_request_lock, flags);
			return -EINVAL;
		}
		/* Update the field, and then ring the doorbell */ 
		writel( intinfo.delay, 
			&(c->cfgtable->HostWrite.CoalIntDelay));
		writel( intinfo.count, 
                        &(c->cfgtable->HostWrite.CoalIntCount));
		writel( CFGTBL_ChangeReq, c->vaddr + SA5_DOORBELL);

		for(i=0;i<MAX_IOCTL_CONFIG_WAIT;i++) {
			if (!(readl(c->vaddr + SA5_DOORBELL) 
					& CFGTBL_ChangeReq))
				break;
			/* delay and try again */
			udelay(1000);
		}	
		spin_unlock_irqrestore(&io_request_lock, flags);
		if (i >= MAX_IOCTL_CONFIG_WAIT)
			/* there is an unlikely case where this can happen,
			 * involving hot replacing a failed 144 GB drive in a 
			 * RAID 5 set just as we attempt this ioctl. */
			return -EAGAIN;
                return 0;
        }
	case CCISS_GETNODENAME:
        {
                NodeName_type NodeName;
                ctlr_info_t *c = hba[ctlr];
		int i; 

		if (!arg) 
			return -EINVAL;
		for(i=0;i<16;i++)
			NodeName[i] = readb(&c->cfgtable->ServerName[i]);
                if (copy_to_user((void *) arg, NodeName, sizeof( NodeName_type)))
                	return  -EFAULT;
                return 0;
        }
	case CCISS_SETNODENAME:
	{
		NodeName_type NodeName;
		ctlr_info_t *c = hba[ctlr];
		unsigned long flags;
		int i;

		if (!arg) 
			return -EINVAL;
		if (!capable(CAP_SYS_ADMIN)) 
			return -EPERM;
		
		if (copy_from_user(NodeName, (void *) arg, sizeof( NodeName_type)))
			return -EFAULT;

		spin_lock_irqsave(&io_request_lock, flags);

			/* Update the field, and then ring the doorbell */ 
		for(i=0;i<16;i++)
			writeb( NodeName[i], &c->cfgtable->ServerName[i]);
			
		writel( CFGTBL_ChangeReq, c->vaddr + SA5_DOORBELL);

		for(i=0;i<MAX_IOCTL_CONFIG_WAIT;i++) {
			if (!(readl(c->vaddr + SA5_DOORBELL) 
					& CFGTBL_ChangeReq))
				break;
			/* delay and try again */
			udelay(1000);
		}	
		spin_unlock_irqrestore(&io_request_lock, flags);
		if (i >= MAX_IOCTL_CONFIG_WAIT)
			/* there is an unlikely case where this can happen,
			 * involving hot replacing a failed 144 GB drive in a 
			 * RAID 5 set just as we attempt this ioctl. */
			return -EAGAIN;
                return 0;
        }

	case CCISS_GETHEARTBEAT:
        {
                Heartbeat_type heartbeat;
                ctlr_info_t *c = hba[ctlr];

		if (!arg) 
			return -EINVAL;
                heartbeat = readl(&c->cfgtable->HeartBeat);
                if (copy_to_user((void *) arg, &heartbeat, sizeof( Heartbeat_type)))
                	return -EFAULT;
                return 0;
        }
	case CCISS_GETBUSTYPES:
        {
                BusTypes_type BusTypes;
                ctlr_info_t *c = hba[ctlr];

		if (!arg) 
			return -EINVAL;
                BusTypes = readl(&c->cfgtable->BusTypes);
                if (copy_to_user((void *) arg, &BusTypes, sizeof( BusTypes_type) ))
                	return  -EFAULT;
                return 0;
        }
	case CCISS_GETFIRMVER:
        {
		FirmwareVer_type firmware;

		if (!arg) 
			return -EINVAL;
		memcpy(firmware, hba[ctlr]->firm_ver, 4);

                if (copy_to_user((void *) arg, firmware, sizeof( FirmwareVer_type)))
                	return -EFAULT;
                return 0;
        }
        case CCISS_GETDRIVVER:
        {
		DriverVer_type DriverVer = DRIVER_VERSION;

                if (!arg) 
			return -EINVAL;

                if (copy_to_user((void *) arg, &DriverVer, sizeof( DriverVer_type) ))
                	return -EFAULT;
                return 0;
        }
	case CCISS_RESCANDISK:
	{
		return cciss_rescan_disk(ctlr, dsk);
	}
	case CCISS_DEREGDISK:
		return deregister_disk(ctlr,dsk);

	case CCISS_REGNEWD:
		return register_new_disk(ctlr, dsk, 0);
	case CCISS_REGNEWDISK:
	{
		__u64 new_logvol;

		if (!arg) 
			return -EINVAL;
		if (copy_from_user(&new_logvol, (void *) arg, 
			sizeof( __u64)))
			return -EFAULT;
		return register_new_disk(ctlr, dsk, new_logvol);
	}
	case CCISS_GETLUNINFO:
	{
		LogvolInfo_struct luninfo;
		int num_parts = 0;
		int i, start;

		luninfo.LunID = hba[ctlr]->drv[dsk].LunID;
		luninfo.num_opens = hba[ctlr]->drv[dsk].usage_count;

		/* count partitions 1 to 15 with sizes > 0 */
  		start = (dsk << NWD_SHIFT);
		for(i=1; i <MAX_PART; i++) {
			int minor = start+i;
			if (hba[ctlr]->sizes[minor] != 0)
				num_parts++;
		}
		luninfo.num_parts = num_parts;
		if (copy_to_user((void *) arg, &luninfo,
				sizeof( LogvolInfo_struct) ))
			return -EFAULT;
		return 0;
	}
	case CCISS_PASSTHRU:
	{
		IOCTL_Command_struct iocommand;
		ctlr_info_t *h = hba[ctlr];
		CommandList_struct *c;
		char 	*buff = NULL;
		u64bit	temp64;
		unsigned long flags;
		DECLARE_COMPLETION(wait);

		if (!arg) 
			return -EINVAL;
	
		if (!capable(CAP_SYS_RAWIO)) 
			return -EPERM;

		if (copy_from_user(&iocommand, (void *) arg, sizeof( IOCTL_Command_struct) ))
			return -EFAULT;
		if ((iocommand.buf_size < 1) && 
				(iocommand.Request.Type.Direction 
				 	!= XFER_NONE)) {	
			return -EINVAL;
		} 
		/* Check kmalloc limits */
		if (iocommand.buf_size > 128000)
			return -EINVAL;
		if (iocommand.buf_size > 0) {
			buff =  kmalloc(iocommand.buf_size, GFP_KERNEL);
			if (buff == NULL) 
				return -ENOMEM;
		}
		if (iocommand.Request.Type.Direction == XFER_WRITE) {
			/* Copy the data into the buffer we created */ 
			if (copy_from_user(buff, iocommand.buf, iocommand.buf_size))
			{
				kfree(buff);
				return -EFAULT;
			}
		}
		if ((c = cmd_alloc(h , 0)) == NULL) {
			kfree(buff);
			return -ENOMEM;
		}
			/* Fill in the command type */
		c->cmd_type = CMD_IOCTL_PEND;
			/* Fill in Command Header */
		c->Header.ReplyQueue = 0;  /* unused in simple mode */
		if (iocommand.buf_size > 0) { 	/* buffer to fill */
			c->Header.SGList = 1;
			c->Header.SGTotal= 1;
		} else	{  /* no buffers to fill  */
			c->Header.SGList = 0;
                	c->Header.SGTotal= 0;
		}
		c->Header.LUN = iocommand.LUN_info;
		c->Header.Tag.lower = c->busaddr;  /* use the kernel address */
						/* the cmd block for tag */
		
		/* Fill in Request block */
		c->Request = iocommand.Request; 
	
		/* Fill in the scatter gather information */
		if (iocommand.buf_size > 0 ) {
			temp64.val = pci_map_single( h->pdev, buff,
                                        iocommand.buf_size, 
                                PCI_DMA_BIDIRECTIONAL);	
			c->SG[0].Addr.lower = temp64.val32.lower;
			c->SG[0].Addr.upper = temp64.val32.upper;
			c->SG[0].Len = iocommand.buf_size;
			c->SG[0].Ext = 0;  /* we are not chaining */
		}
		c->waiting = &wait;

		/* Put the request on the tail of the request queue */
		spin_lock_irqsave(&io_request_lock, flags);
		addQ(&h->reqQ, c);
		h->Qdepth++;
		start_io(h);
		spin_unlock_irqrestore(&io_request_lock, flags);

		wait_for_completion(&wait);

		/* unlock the buffers from DMA */
		temp64.val32.lower = c->SG[0].Addr.lower;
                temp64.val32.upper = c->SG[0].Addr.upper;
                pci_unmap_single( h->pdev, (dma_addr_t) temp64.val,
                	iocommand.buf_size, PCI_DMA_BIDIRECTIONAL);

		/* Copy the error information out */ 
		iocommand.error_info = *(c->err_info);
		if (copy_to_user((void *) arg, &iocommand, 
				sizeof( IOCTL_Command_struct) ) ) {
			kfree(buff);
			cmd_free(h, c, 0);
			return( -EFAULT);
		} 	

		if (iocommand.Request.Type.Direction == XFER_READ) {
                        /* Copy the data out of the buffer we created */
                        if (copy_to_user(iocommand.buf, buff, 
						iocommand.buf_size)) {
                        	kfree(buff);
				cmd_free(h, c, 0);
				return -EFAULT;
			}
                }
                kfree(buff);
		cmd_free(h, c, 0);
                return 0;
	} 
	case CCISS_BIG_PASSTHRU:
	{
		BIG_IOCTL_Command_struct iocommand;
		ctlr_info_t *h = hba[ctlr];
		CommandList_struct *c;
		char 	*buff[MAXSGENTRIES] = {NULL,};
		int	buff_size[MAXSGENTRIES] = {0,};
		u64bit	temp64;
		unsigned long flags;
		BYTE sg_used = 0;
		int status = 0;
		int i;
		DECLARE_COMPLETION(wait);

		if (!arg) 
			return -EINVAL;
		
		if (!capable(CAP_SYS_RAWIO)) 
			return -EPERM;

		if (copy_from_user(&iocommand, (void *) arg, sizeof( BIG_IOCTL_Command_struct) ))
			return -EFAULT;
		if ((iocommand.buf_size < 1) && 
			(iocommand.Request.Type.Direction != XFER_NONE)) {
			return -EINVAL;
		} 
		/* Check kmalloc limits  using all SGs */
		if (iocommand.malloc_size > MAX_KMALLOC_SIZE)
			return -EINVAL;
		if (iocommand.buf_size > iocommand.malloc_size * MAXSGENTRIES)
			return -EINVAL;
		if (iocommand.buf_size > 0) {
			__u32   size_left_alloc = iocommand.buf_size;
			BYTE    *data_ptr = (BYTE *) iocommand.buf;
			while (size_left_alloc > 0) {
				buff_size[sg_used] = (size_left_alloc 
							> iocommand.malloc_size)
					? iocommand.malloc_size : size_left_alloc;
				buff[sg_used] = kmalloc( buff_size[sg_used], 
						GFP_KERNEL);
				if (buff[sg_used] == NULL) {
					status = -ENOMEM;
					goto cleanup1;
				}
				if (iocommand.Request.Type.Direction == 
						XFER_WRITE)
				   /* Copy the data into the buffer created */
				   if (copy_from_user(buff[sg_used], data_ptr, 
						buff_size[sg_used])) {
					status = -ENOMEM;
					goto cleanup1;			
				   }
				size_left_alloc -= buff_size[sg_used];
				data_ptr += buff_size[sg_used];
				sg_used++;
			}
			
		}
		if ((c = cmd_alloc(h , 0)) == NULL) {
			status = -ENOMEM;
			goto cleanup1;	
		}
		/* Fill in the command type */
		c->cmd_type = CMD_IOCTL_PEND;
		/* Fill in Command Header */
		c->Header.ReplyQueue = 0;  /* unused in simple mode */
		
		if (iocommand.buf_size > 0) { 	/* buffer to fill */
			c->Header.SGList = sg_used;
			c->Header.SGTotal= sg_used;
		} else	{	/* no buffers to fill */
			c->Header.SGList = 0;
			c->Header.SGTotal= 0;
		}
		c->Header.LUN = iocommand.LUN_info;
		c->Header.Tag.lower = c->busaddr;  /* use the kernel address */
						/* the cmd block for tag */
		
	/* Fill in Request block */
	c->Request = iocommand.Request; 
	/* Fill in the scatter gather information */
	if (iocommand.buf_size > 0 ) {
		int i;
		for(i=0; i< sg_used; i++) {
			temp64.val = pci_map_single( h->pdev, buff[i], 
					buff_size[i], 
					PCI_DMA_BIDIRECTIONAL);

			c->SG[i].Addr.lower = temp64.val32.lower;
			c->SG[i].Addr.upper = temp64.val32.upper;
			c->SG[i].Len = buff_size[i];
			c->SG[i].Ext = 0;  /* we are not chaining */
		}
	}
	c->waiting = &wait;
	/* Put the request on the tail of the request queue */
	spin_lock_irqsave(&io_request_lock, flags);
	addQ(&h->reqQ, c);
	h->Qdepth++;
	start_io(h);
	spin_unlock_irqrestore(&io_request_lock, flags);
	wait_for_completion(&wait);
	/* unlock the buffers from DMA */
	for(i=0; i< sg_used; i++) {
		temp64.val32.lower = c->SG[i].Addr.lower;
		temp64.val32.upper = c->SG[i].Addr.upper;
		pci_unmap_single( h->pdev, (dma_addr_t) temp64.val,
				buff_size[i], PCI_DMA_BIDIRECTIONAL);
	}
	/* Copy the error information out */
		iocommand.error_info = *(c->err_info);
		if (copy_to_user((void *) arg, &iocommand, 
					sizeof( IOCTL_Command_struct) ) ) {
				cmd_free(h, c, 0);
				status = -EFAULT;
				goto cleanup1;
		}
		if (iocommand.Request.Type.Direction == XFER_READ) {
		/* Copy the data out of the buffer we created */
			BYTE *ptr = (BYTE  *) iocommand.buf;
	        	for(i=0; i< sg_used; i++) {
				if (copy_to_user(ptr, buff[i], buff_size[i])) {
					cmd_free(h, c, 0);
					status = -EFAULT;
					goto cleanup1;

				}
				ptr += buff_size[i];
			}
		}
		cmd_free(h, c, 0);
		status = 0;
		

cleanup1:
		for(i=0; i< sg_used; i++) {
			if (buff[i] != NULL)
				kfree(buff[i]);
		}
		return status;
	}
	default:
		return -EBADRQC;
	}
	
}

/* Borrowed and adapted from sd.c */
static int revalidate_logvol(kdev_t dev, int maxusage)
{
        int ctlr, target;
        struct gendisk *gdev;
        unsigned long flags;
        int max_p;
        int start;
        int i;

        target = MINOR(dev) >> NWD_SHIFT;
	ctlr = map_major_to_ctlr[MAJOR(dev)];
        gdev = &(hba[ctlr]->gendisk);

        spin_lock_irqsave(&io_request_lock, flags);
        if (hba[ctlr]->drv[target].usage_count > maxusage) {
                spin_unlock_irqrestore(&io_request_lock, flags);
                printk(KERN_WARNING "cciss: Device busy for "
                        "revalidation (usage=%d)\n",
                        hba[ctlr]->drv[target].usage_count);
                return -EBUSY;
        }
        hba[ctlr]->drv[target].usage_count++;
        spin_unlock_irqrestore(&io_request_lock, flags);

        max_p = gdev->max_p;
        start = target << gdev->minor_shift;

        for(i=max_p-1; i>=0; i--) {
                int minor = start+i;
                invalidate_device(MKDEV(hba[ctlr]->major, minor), 1);
                gdev->part[minor].start_sect = 0;
                gdev->part[minor].nr_sects = 0;

                /* reset the blocksize so we can read the partition table */
                blksize_size[hba[ctlr]->major][minor] = 1024;
        }
	/* setup partitions per disk */
	grok_partitions(gdev, target, MAX_PART, 
			hba[ctlr]->drv[target].nr_blocks);
        hba[ctlr]->drv[target].usage_count--;
        return 0;
}

static int frevalidate_logvol(kdev_t dev)
{
#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss: frevalidate has been called\n");
#endif /* CCISS_DEBUG */ 
	return revalidate_logvol(dev, 0);
}
static int deregister_disk(int ctlr, int logvol)
{
	unsigned long flags;
	struct gendisk *gdev = &(hba[ctlr]->gendisk);
	ctlr_info_t  *h = hba[ctlr];
	int start, max_p, i;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	spin_lock_irqsave(&io_request_lock, flags);
	/* make sure logical volume is NOT is use */
	if (h->drv[logvol].usage_count > 1 || h->busy_configuring) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return -EBUSY;
	}
	h->busy_configuring = 1;
	spin_unlock_irqrestore(&io_request_lock, flags);

	/* invalidate the devices and deregister the disk */
	max_p = gdev->max_p;
	start = logvol << gdev->minor_shift;
	for (i=max_p-1; i>=0; i--) {
		int minor = start+i;
		/* printk("invalidating( %d %d)\n", ctlr, minor); */
		invalidate_device(MKDEV(hba[ctlr]->major, minor), 1);
		/* so open will now fail */
		h->sizes[minor] = 0;
		/* so it will no longer appear in /proc/partitions */
		gdev->part[minor].start_sect = 0;
		gdev->part[minor].nr_sects = 0;
	}
	/* check to see if it was the last disk */
	if (logvol == h->highest_lun) {
		/* if so, find the new hightest lun */
		int i, newhighest =-1;
		for(i=0; i<h->highest_lun; i++) {
			/* if the disk has size > 0, it is available */
			if (h->sizes[i << gdev->minor_shift] != 0)
				newhighest = i;
		}
		h->highest_lun = newhighest;

	}
	--h->num_luns;
	gdev->nr_real = h->highest_lun+1;
	/* zero out the disk size info */
	h->drv[logvol].nr_blocks = 0;
	h->drv[logvol].block_size = 0;
	h->drv[logvol].cylinders = 0;
	h->drv[logvol].LunID = 0;
	h->busy_configuring = 0;
	return 0;
}
static int sendcmd_withirq(__u8	cmd,
	int	ctlr,
	void	*buff,
	size_t	size,
	unsigned int use_unit_num,
	unsigned int log_unit,
	__u8	page_code,
	__u8 cmdtype)
{
	ctlr_info_t *h = hba[ctlr];
	CommandList_struct *c;
	u64bit	buff_dma_handle;
	unsigned long flags;
	int return_status = IO_OK;
	DECLARE_COMPLETION(wait);

	if ((c = cmd_alloc(h , 0)) == NULL)
		return -ENOMEM;
	c->cmd_type = CMD_IOCTL_PEND;
	/* Fill in Command Header */
	c->Header.ReplyQueue = 0;  /* unused in simple mode */
	if (buff != NULL) { 	/* buffer to fill */
		c->Header.SGList = 1;
		c->Header.SGTotal= 1;
	} else {
		/* no buffers to fill */
		c->Header.SGList = 0;
		c->Header.SGTotal= 0;
	}
	c->Header.Tag.lower = c->busaddr;  /* tag is phys addr of cmd */
	/* Fill in Request block */
	c->Request.CDB[0] = cmd;
	c->Request.Type.Type = cmdtype;
	if (cmdtype == TYPE_CMD) {
	switch (cmd) {
		case  CISS_INQUIRY:
			/* If the logical unit number is 0 then, this is going
				to controller so It's a physical command
				mode = 0 target = 0.
				So we have nothing to write.
				Otherwise
				mode = 1  target = LUNID
			*/
			if (use_unit_num != 0) {
				c->Header.LUN.LogDev.VolId =
					hba[ctlr]->drv[log_unit].LunID;
				c->Header.LUN.LogDev.Mode = 1;
			}
			if (page_code != 0) {
				c->Request.CDB[1] = 0x01;
				c->Request.CDB[2] = page_code;
			}
			c->Request.CDBLen = 6;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ; /* Read */
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[4] = size  & 0xFF;
		break;
		case CISS_REPORT_LOG:
			/* Talking to controller so It's a physical command
				mode = 00 target = 0.
				So we have nothing to write.
			*/
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ; /* Read */
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[6] = (size >> 24) & 0xFF;  /* MSB */
			c->Request.CDB[7] = (size >> 16) & 0xFF;
			c->Request.CDB[8] = (size >> 8) & 0xFF;
			c->Request.CDB[9] = size & 0xFF;
		break;
		case CCISS_READ_CAPACITY:
			c->Header.LUN.LogDev.VolId=
				hba[ctlr]->drv[log_unit].LunID;
			c->Header.LUN.LogDev.Mode = 1;
			c->Request.CDBLen = 10;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ; /* Read */
			c->Request.Timeout = 0; /* Don't time out */
		break;
		default:
			printk(KERN_WARNING
				"cciss:  Unknown Command 0x%x sent attempted\n",				cmd);
			cmd_free(h, c, 1);
			return IO_ERROR;
		}
	} else if (cmdtype == TYPE_MSG) {
		switch (cmd) {
		case 3: /* No-Op message */
			c->Request.CDBLen = 1;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			break;
		default:
			printk(KERN_WARNING
				"cciss%d: unknown message type %d\n",
					ctlr, cmd);
			cmd_free(h, c, 1);
			return IO_ERROR;
		}
	} else {
		printk(KERN_WARNING
			"cciss%d: unknown command type %d\n", ctlr, cmdtype);
		cmd_free(h, c, 1);
		return IO_ERROR;
	}

	/* Fill in the scatter gather information */
	if (size > 0) {
		buff_dma_handle.val = (__u64) pci_map_single( h->pdev,
			buff, size, PCI_DMA_BIDIRECTIONAL);
		c->SG[0].Addr.lower = buff_dma_handle.val32.lower;
		c->SG[0].Addr.upper = buff_dma_handle.val32.upper;
		c->SG[0].Len = size;
		c->SG[0].Ext = 0;  /* we are not chaining */
	}

resend_cmd2:
	c->waiting = &wait;
	/* Put the request on the tail of the queue and send it */
	spin_lock_irqsave(&io_request_lock, flags);
	addQ(&h->reqQ, c);
	h->Qdepth++;
	start_io(h);
	spin_unlock_irqrestore(&io_request_lock, flags);

	wait_for_completion(&wait);


	if (c->err_info->CommandStatus != 0) {
		/* an error has occurred */
		switch (c->err_info->CommandStatus) {
			case CMD_TARGET_STATUS:
				printk(KERN_WARNING "cciss: cmd %p has "
					" completed with errors\n", c);
				if (c->err_info->ScsiStatus) {
					printk(KERN_WARNING "cciss: cmd %p "
					"has SCSI Status = %x\n", c,
						c->err_info->ScsiStatus);
				}
			break;
			case CMD_DATA_UNDERRUN:
			case CMD_DATA_OVERRUN:
			/* expected for inquire and report lun commands */
			break;
			case CMD_INVALID:
				printk(KERN_WARNING "cciss: cmd %p is "
					"reported invalid\n", c);
				return_status = IO_ERROR;
			break;
			case CMD_PROTOCOL_ERR:
				printk(KERN_WARNING "cciss: cmd %p has "
					"protocol error \n", c);
				return_status = IO_ERROR;
			break;
			case CMD_HARDWARE_ERR:
				printk(KERN_WARNING "cciss: cmd %p had "
					" hardware error\n", c);
				return_status = IO_ERROR;
				break;
			case CMD_CONNECTION_LOST:
				printk(KERN_WARNING "cciss: cmd %p had "
					"connection lost\n", c);
				return_status = IO_ERROR;
			break;
			case CMD_ABORTED:
				printk(KERN_WARNING "cciss: cmd %p was "
					"aborted\n", c);
				return_status = IO_ERROR;
			break;
			case CMD_ABORT_FAILED:
				printk(KERN_WARNING "cciss: cmd %p reports "
					"abort failed\n", c);
				return_status = IO_ERROR;
			break;
			case CMD_UNSOLICITED_ABORT:
				printk(KERN_WARNING "cciss: cmd %p aborted "
					"do to an unsolicited abort\n", c);
				if (c->retry_count < MAX_CMD_RETRIES) 
				{ 
					printk(KERN_WARNING "retrying cmd\n"); 
					c->retry_count++; 
					/* erase the old error */ 
					/* information */ 
					memset(c->err_info, 0, 
						sizeof(ErrorInfo_struct)); 
					return_status = IO_OK;
					INIT_COMPLETION(wait);
					goto resend_cmd2;
					
				}
				return_status = IO_ERROR;
			break;
			default:
				printk(KERN_WARNING "cciss: cmd %p returned "
					"unknown status %x\n", c,
						c->err_info->CommandStatus);
				return_status = IO_ERROR;
		}
	}

	/* unlock the buffers from DMA */
	pci_unmap_single( h->pdev, (dma_addr_t) buff_dma_handle.val,
			size, PCI_DMA_BIDIRECTIONAL);
	cmd_free(h, c, 0);
	return return_status;
}
static int register_new_disk(int ctlr, int opened_vol, __u64 requested_lun)
{
	struct gendisk *gdev = &(hba[ctlr]->gendisk);
	ctlr_info_t  *h = hba[ctlr];
	int start, max_p, i;
	int num_luns;
	int logvol;
	int new_lun_found = 0;
	int new_lun_index = 0;
	int free_index_found = 0;
	int free_index = 0;
	ReportLunData_struct *ld_buff;
	ReadCapdata_struct *size_buff;
	InquiryData_struct *inq_buff;
	int return_code;
	int listlength = 0;
	__u32 lunid = 0;
	unsigned int block_size;
	unsigned int total_size;
	unsigned long flags;
	int req_lunid = (int) (requested_lun & (__u64) 0xffffffff);

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	/* if we have no space in our disk array left to add anything */
	spin_lock_irqsave(&io_request_lock, flags);
	if (h->num_luns >= CISS_MAX_LUN) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return -EINVAL;
	}
	if (h->busy_configuring) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return -EBUSY;
	}
	h->busy_configuring = 1;
	spin_unlock_irqrestore(&io_request_lock, flags);

	ld_buff = kmalloc(sizeof(ReportLunData_struct), GFP_KERNEL);
	if (ld_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		h->busy_configuring = 0;
		return -ENOMEM;
	}
	memset(ld_buff, 0, sizeof(ReportLunData_struct));
	size_buff = kmalloc(sizeof( ReadCapdata_struct), GFP_KERNEL);
	if (size_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		kfree(ld_buff);
		h->busy_configuring = 0;
		return -ENOMEM;
	}
	inq_buff = kmalloc(sizeof( InquiryData_struct), GFP_KERNEL);
	if (inq_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		kfree(ld_buff);
		kfree(size_buff);
		h->busy_configuring = 0;
		return -ENOMEM;
	}

	return_code = sendcmd_withirq(CISS_REPORT_LOG, ctlr, ld_buff,
			sizeof(ReportLunData_struct), 0, 0, 0, TYPE_CMD);

	if (return_code == IO_OK) {
		listlength = be32_to_cpu(*((__u32 *) &ld_buff->LUNListLength[0]));
	} else {
		/* reading number of logical volumes failed */
		printk(KERN_WARNING "cciss: report logical volume"
			" command failed\n");
		listlength = 0;
		h->busy_configuring = 0;
		return -1;
	}
	num_luns = listlength / 8; /* 8 bytes pre entry */
	if (num_luns > CISS_MAX_LUN)
		num_luns = CISS_MAX_LUN;

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "Length = %x %x %x %x = %d\n", ld_buff->LUNListLength[0],
		ld_buff->LUNListLength[1], ld_buff->LUNListLength[2],
		ld_buff->LUNListLength[3],  num_luns);
#endif
	for(i=0; i<  num_luns; i++) {
		int j;
		int lunID_found = 0;

		lunid = (0xff & (unsigned int)(ld_buff->LUN[i][3])) << 24;
		lunid |= (0xff & (unsigned int)(ld_buff->LUN[i][2])) << 16;
		lunid |= (0xff & (unsigned int)(ld_buff->LUN[i][1])) << 8;
		lunid |= 0xff & (unsigned int)(ld_buff->LUN[i][0]);

		/* check to see if this is a new lun */
		for(j=0; j <= h->highest_lun; j++) {
#ifdef CCISS_DEBUG
			printk("Checking %d %x against %x\n", j,h->drv[j].LunID,
						lunid);
#endif /* CCISS_DEBUG */
			if (h->drv[j].LunID == lunid) {
				lunID_found = 1;
				break;
			}

		}
		if (lunID_found == 1)
			continue;
		else {	/* new lun found */
			
#ifdef CCISS_DEBUG
			printk("new lun found at %d\n", i);
#endif /* CCISS_DEBUG */
			if (req_lunid)  /* we are looking for a specific lun */
			{
				if (lunid != req_lunid)
				{
#ifdef CCISS_DEBUG
					printk("new lun %x is not %x\n",
							lunid, req_lunid);
#endif /* CCISS_DEBUG */
					continue;
				}
			}
			new_lun_index = i;
			new_lun_found = 1;
			break;
		}
	}
	if (!new_lun_found) {
		printk(KERN_DEBUG "cciss:  New Logical Volume not found\n");
		h->busy_configuring = 0;
		return -1;
	}
	/* Now find the free index 	*/
	for(i=0; i <CISS_MAX_LUN; i++) {
#ifdef CCISS_DEBUG
		printk("Checking Index %d\n", i);
#endif /* CCISS_DEBUG */
		if (hba[ctlr]->drv[i].LunID == 0) {
#ifdef CCISS_DEBUG
			printk("free index found at %d\n", i);
#endif /* CCISS_DEBUG */
			free_index_found = 1;
			free_index = i;
			break;
		}
	}
	if (!free_index_found) {
		printk(KERN_WARNING "cciss: unable to find free slot for disk\n");
		h->busy_configuring = 0;
		return -1;
	}

	logvol = free_index;
	hba[ctlr]->drv[logvol].LunID = lunid;
		/* there could be gaps in lun numbers, track hightest */
	if (hba[ctlr]->highest_lun < logvol)
		hba[ctlr]->highest_lun = logvol;

	memset(size_buff, 0, sizeof(ReadCapdata_struct));
	return_code = sendcmd_withirq(CCISS_READ_CAPACITY, ctlr,
			size_buff, sizeof(ReadCapdata_struct), 1,
			logvol, 0, TYPE_CMD);
	if (return_code == IO_OK) {
		total_size = (0xff &
			(unsigned int) size_buff->total_size[0]) << 24;
		total_size |= (0xff &
			(unsigned int) size_buff->total_size[1]) << 16;
		total_size |= (0xff &
			(unsigned int) size_buff->total_size[2]) << 8;
		total_size |= (0xff &
			(unsigned int) size_buff->total_size[3]);
		total_size++; /* command returns highest block address */

		block_size = (0xff &
			(unsigned int) size_buff->block_size[0]) << 24;
		block_size |= (0xff &
			(unsigned int) size_buff->block_size[1]) << 16;
		block_size |= (0xff &
			(unsigned int) size_buff->block_size[2]) << 8;
		block_size |= (0xff &
			(unsigned int) size_buff->block_size[3]);
	} else {
		/* read capacity command failed */
		printk(KERN_WARNING "cciss: read capacity failed\n");
		total_size = 0;
		block_size = BLOCK_SIZE;
	}
	printk(KERN_INFO "      blocks= %d block_size= %d\n",
					total_size, block_size);
	/* Execute the command to read the disk geometry */
	memset(inq_buff, 0, sizeof(InquiryData_struct));
	return_code = sendcmd_withirq(CISS_INQUIRY, ctlr, inq_buff,
		sizeof(InquiryData_struct), 1, logvol ,0xC1, TYPE_CMD);
	if (return_code == IO_OK) {
		if (inq_buff->data_byte[8] == 0xFF) {
			printk(KERN_WARNING
			"cciss: reading geometry failed, "
			"volume does not support reading geometry\n");

			hba[ctlr]->drv[logvol].block_size = block_size;
			hba[ctlr]->drv[logvol].nr_blocks = total_size;
			hba[ctlr]->drv[logvol].heads = 255;
			hba[ctlr]->drv[logvol].sectors = 32; /* secs/trk */
			hba[ctlr]->drv[logvol].cylinders = total_size / 255 /32;
			hba[ctlr]->drv[logvol].raid_level = RAID_UNKNOWN;
		} else {
			hba[ctlr]->drv[logvol].block_size = block_size;
			hba[ctlr]->drv[logvol].nr_blocks = total_size;
			hba[ctlr]->drv[logvol].heads = inq_buff->data_byte[6];
			hba[ctlr]->drv[logvol].sectors = inq_buff->data_byte[7];
			hba[ctlr]->drv[logvol].cylinders =
				(inq_buff->data_byte[4] & 0xff) << 8;
			hba[ctlr]->drv[logvol].cylinders +=
				inq_buff->data_byte[5];
			hba[ctlr]->drv[logvol].raid_level = 
				inq_buff->data_byte[8];
		}
	} else {
		/* Get geometry failed */
		printk(KERN_WARNING "cciss: reading geometry failed, "
			"continuing with default geometry\n");

		hba[ctlr]->drv[logvol].block_size = block_size;
		hba[ctlr]->drv[logvol].nr_blocks = total_size;
		hba[ctlr]->drv[logvol].heads = 255;
		hba[ctlr]->drv[logvol].sectors = 32; /* Sectors per track */
		hba[ctlr]->drv[logvol].cylinders = total_size / 255 / 32;
	}
	if (hba[ctlr]->drv[logvol].raid_level > 5)
		hba[ctlr]->drv[logvol].raid_level = RAID_UNKNOWN;
	printk(KERN_INFO "      heads= %d, sectors= %d, cylinders= %d RAID %s\n\n",
		hba[ctlr]->drv[logvol].heads,
		hba[ctlr]->drv[logvol].sectors,
		hba[ctlr]->drv[logvol].cylinders, 
		raid_label[hba[ctlr]->drv[logvol].raid_level]);

	/* special case for c?d0, which may be opened even when
	   it does not "exist".  In that case, don't mess with usage count.
	   Also, /dev/c1d1 could be used to re-add c0d0 so we can't just 
	   check whether logvol == 0, must check logvol != opened_vol */
	if (logvol != opened_vol)
		hba[ctlr]->drv[logvol].usage_count = 0;

	max_p = gdev->max_p;
	start = logvol<< gdev->minor_shift;
	hba[ctlr]->hd[start].nr_sects = total_size;
	hba[ctlr]->sizes[start] = total_size;

	for(i=max_p-1; i>=0; i--) {
		int minor = start+i;
		invalidate_device(MKDEV(hba[ctlr]->major, minor), 1);
		gdev->part[minor].start_sect = 0;
		gdev->part[minor].nr_sects = 0;

		/* reset the blocksize so we can read the partition table */
		blksize_size[hba[ctlr]->major][minor] = block_size;
		hba[ctlr]->hardsizes[minor] = block_size;
	}

	++hba[ctlr]->num_luns;
	gdev->nr_real = hba[ctlr]->highest_lun + 1;
	/* setup partitions per disk */
	grok_partitions(gdev, logvol, MAX_PART,
			hba[ctlr]->drv[logvol].nr_blocks);
	kfree(ld_buff);
	kfree(size_buff);
	kfree(inq_buff);
	h->busy_configuring = 0;
	return logvol;
}

static int cciss_rescan_disk(int ctlr, int logvol)
{
	struct gendisk *gdev = &(hba[ctlr]->gendisk);
	int start, max_p, i;
	ReadCapdata_struct *size_buff;
	InquiryData_struct *inq_buff;
	int return_code;
	unsigned int block_size;
	unsigned int total_size;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	if (hba[ctlr]->sizes[logvol << NWD_SHIFT] != 0) {
		/* disk is possible on line, return just a warning */
		return 1;
	}
	size_buff = kmalloc(sizeof( ReadCapdata_struct), GFP_KERNEL);
	if (size_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		return -1;
	}
	inq_buff = kmalloc(sizeof( InquiryData_struct), GFP_KERNEL);
	if (inq_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		kfree(size_buff);
		return -1;
	}
	memset(size_buff, 0, sizeof(ReadCapdata_struct));
	return_code = sendcmd_withirq(CCISS_READ_CAPACITY, ctlr, size_buff,
				sizeof( ReadCapdata_struct), 1, logvol, 0, 
				TYPE_CMD);
	if (return_code == IO_OK) {
		total_size = (0xff &
			(unsigned int)(size_buff->total_size[0])) << 24;
		total_size |= (0xff &
				(unsigned int)(size_buff->total_size[1])) << 16;
		total_size |= (0xff &
				(unsigned int)(size_buff->total_size[2])) << 8;
		total_size |= (0xff & (unsigned int)
				(size_buff->total_size[3]));
		total_size++; /* command returns highest block address */

		block_size = (0xff &
				(unsigned int)(size_buff->block_size[0])) << 24;
		block_size |= (0xff &
				(unsigned int)(size_buff->block_size[1])) << 16;
		block_size |= (0xff &
				(unsigned int)(size_buff->block_size[2])) << 8;
		block_size |= (0xff &
				(unsigned int)(size_buff->block_size[3]));
	} else { /* read capacity command failed */
		printk(KERN_WARNING "cciss: read capacity failed\n");
		total_size = block_size = 0;
	}
	printk(KERN_INFO "      blocks= %d block_size= %d\n",
					total_size, block_size);
	/* Execute the command to read the disk geometry */
	memset(inq_buff, 0, sizeof(InquiryData_struct));
	return_code = sendcmd_withirq(CISS_INQUIRY, ctlr, inq_buff,
			sizeof(InquiryData_struct), 1, logvol ,0xC1, TYPE_CMD);
	if (return_code == IO_OK) {
		if (inq_buff->data_byte[8] == 0xFF) {
			printk(KERN_WARNING "cciss: reading geometry failed, "
				"volume does not support reading geometry\n");

			hba[ctlr]->drv[logvol].nr_blocks = total_size;
			hba[ctlr]->drv[logvol].heads = 255;
			hba[ctlr]->drv[logvol].sectors = 32; /* Sectors/track */
			hba[ctlr]->drv[logvol].cylinders = total_size / 255 /32;
		} else {
			hba[ctlr]->drv[logvol].nr_blocks = total_size;
			hba[ctlr]->drv[logvol].heads = inq_buff->data_byte[6];
			hba[ctlr]->drv[logvol].sectors = inq_buff->data_byte[7];
			hba[ctlr]->drv[logvol].cylinders =
				(inq_buff->data_byte[4] & 0xff) << 8;
			hba[ctlr]->drv[logvol].cylinders +=
				inq_buff->data_byte[5];
		}
	} else { /* Get geometry failed */
		printk(KERN_WARNING "cciss: reading geometry failed, "
				"continuing with default geometry\n");

		hba[ctlr]->drv[logvol].nr_blocks = total_size;
		hba[ctlr]->drv[logvol].heads = 255;
		hba[ctlr]->drv[logvol].sectors = 32; /* Sectors / track */
		hba[ctlr]->drv[logvol].cylinders = total_size / 255 /32;
	}

	printk(KERN_INFO "      heads= %d, sectors= %d, cylinders= %d \n\n", 
		hba[ctlr]->drv[logvol].heads,
		hba[ctlr]->drv[logvol].sectors,
		hba[ctlr]->drv[logvol].cylinders);
	max_p = gdev->max_p;
	start = logvol<< gdev->minor_shift;
	hba[ctlr]->hd[start].nr_sects = hba[ctlr]->sizes[start]= total_size;

	for (i=max_p-1; i>=0; i--) {
		int minor = start+i;
		invalidate_device(MKDEV(hba[ctlr]->major, minor), 1);
		gdev->part[minor].start_sect = 0;
		gdev->part[minor].nr_sects = 0;

		/* reset the blocksize so we can read the partition table */
		blksize_size[hba[ctlr]->major][minor] = block_size;
		hba[ctlr]->hardsizes[minor] = block_size;
	}

	/* setup partitions per disk */
	grok_partitions(gdev, logvol, MAX_PART,
			hba[ctlr]->drv[logvol].nr_blocks );

	kfree(size_buff);
	kfree(inq_buff);
	return 0;
}
/*
 *   Wait polling for a command to complete.
 *   The memory mapped FIFO is polled for the completion.
 *   Used only at init time, interrupts disabled.
 */
static unsigned long pollcomplete(int ctlr)
{
	unsigned long done;
	int i;

	/* Wait (up to 20 seconds) for a command to complete */

        for (i = 20 * HZ; i > 0; i--) {
		done = hba[ctlr]->access.command_completed(hba[ctlr]);
		if (done == FIFO_EMPTY) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(1);
		} else
			return done;
	}
	/* Invalid address to tell caller we ran out of time */
	return 1;
}
/*
 * Send a command to the controller, and wait for it to complete.  
 * Only used at init time. 
 */
static int sendcmd(
	__u8	cmd,
	int	ctlr,
	void	*buff,
	size_t	size,
	unsigned int use_unit_num, /* 0: address the controller,
				      1: address logical volume log_unit,
				      2: periph device address is scsi3addr */
	unsigned int log_unit,
	__u8	page_code,
	unsigned char *scsi3addr)
{
	CommandList_struct *c;
	int i;
	unsigned long complete;
	ctlr_info_t *info_p= hba[ctlr];
	u64bit buff_dma_handle;
	int status = IO_OK;

	c = cmd_alloc(info_p, 1);
	if (c == NULL) {
		printk(KERN_WARNING "cciss: unable to get memory");
		return IO_ERROR;
	}
	/* Fill in Command Header */
	c->Header.ReplyQueue = 0;  /* unused in simple mode */
	if (buff != NULL) { 	/* buffer to fill */
		c->Header.SGList = 1;
		c->Header.SGTotal= 1;
	} else	{	/* no buffers to fill  */
		c->Header.SGList = 0;
                c->Header.SGTotal= 0;
	}
	c->Header.Tag.lower = c->busaddr;  /* use the kernel address */
					   /* the cmd block for tag */
	/* Fill in Request block */
	switch (cmd) {
		case  CISS_INQUIRY:
			/* If the logical unit number is 0 then, this is going
				to controller so It's a physical command
				mode = 0 target = 0.
				So we have nothing to write. 
				otherwise, if use_unit_num == 1,
				mode = 1(volume set addressing) target = LUNID
				otherwise, if use_unit_num == 2,
				mode = 0(periph dev addr) target = scsi3addr
			*/
			if (use_unit_num == 1) {
				c->Header.LUN.LogDev.VolId=
                                	hba[ctlr]->drv[log_unit].LunID;
                        	c->Header.LUN.LogDev.Mode = 1;
			}
			else if (use_unit_num == 2) {
				memcpy(c->Header.LUN.LunAddrBytes,scsi3addr,8);
				c->Header.LUN.LogDev.Mode = 0; 
							/* phys dev addr */
			}

			/* are we trying to read a vital product page */
			if (page_code != 0) {
				c->Request.CDB[1] = 0x01;
				c->Request.CDB[2] = page_code;
			}
			c->Request.CDBLen = 6;
			c->Request.Type.Type =  TYPE_CMD; /* It is a command. */
			c->Request.Type.Attribute = ATTR_SIMPLE;  
			c->Request.Type.Direction = XFER_READ; /* Read */
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] =  CISS_INQUIRY;
			c->Request.CDB[4] = size  & 0xFF;  
		break;
		case CISS_REPORT_LOG:
		case CISS_REPORT_PHYS:
                        /* Talking to controller so It's a physical command
                                mode = 00 target = 0.
                                So we have nothing to write.
                        */
                        c->Request.CDBLen = 12;
                        c->Request.Type.Type =  TYPE_CMD; /* It is a command. */
                        c->Request.Type.Attribute = ATTR_SIMPLE; 
                        c->Request.Type.Direction = XFER_READ; /* Read */
                        c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] = cmd;
                        c->Request.CDB[6] = (size >> 24) & 0xFF;  /* MSB */
                        c->Request.CDB[7] = (size >> 16) & 0xFF;
                        c->Request.CDB[8] = (size >> 8) & 0xFF;
                        c->Request.CDB[9] = size & 0xFF;
                break;

		case CCISS_READ_CAPACITY:
			c->Header.LUN.LogDev.VolId= 
				hba[ctlr]->drv[log_unit].LunID;
			c->Header.LUN.LogDev.Mode = 1;
			c->Request.CDBLen = 10;
                        c->Request.Type.Type =  TYPE_CMD; /* It is a command. */
                        c->Request.Type.Attribute = ATTR_SIMPLE; 
                        c->Request.Type.Direction = XFER_READ; /* Read */
                        c->Request.Timeout = 0; /* Don't time out */
                        c->Request.CDB[0] = CCISS_READ_CAPACITY;
		break;
		case CCISS_CACHE_FLUSH:
			c->Request.CDBLen = 12;
			c->Request.Type.Type =  TYPE_CMD; /* It is a command. */
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE; /* No data */
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] = BMIC_WRITE;  /* BMIC Passthru */
			c->Request.CDB[6] = BMIC_CACHE_FLUSH;
		break;
		default:
			printk(KERN_WARNING
				"cciss:  Unknown Command 0x%x sent attempted\n",
				  cmd);
			cmd_free(info_p, c, 1);
			return IO_ERROR;
	};
	/* Fill in the scatter gather information */
	if (size > 0) {
		buff_dma_handle.val = (__u64) pci_map_single( info_p->pdev, 
			buff, size, PCI_DMA_BIDIRECTIONAL);
		c->SG[0].Addr.lower = buff_dma_handle.val32.lower;
		c->SG[0].Addr.upper = buff_dma_handle.val32.upper;
		c->SG[0].Len = size;
		c->SG[0].Ext = 0;  /* we are not chaining */
	}
resend_cmd1:
	/*
         * Disable interrupt
         */
#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss: turning intr off\n");
#endif /* CCISS_DEBUG */ 
        info_p->access.set_intr_mask(info_p, CCISS_INTR_OFF);
	
	/* Make sure there is room in the command FIFO */
        /* Actually it should be completely empty at this time. */
        for (i = 200000; i > 0; i--) {
		/* if fifo isn't full go */
                if (!(info_p->access.fifo_full(info_p))) {
			
                        break;
                }
                udelay(10);
                printk(KERN_WARNING "cciss cciss%d: SendCmd FIFO full,"
                        " waiting!\n", ctlr);
        }
        /*
         * Send the cmd
         */
        info_p->access.submit_command(info_p, c);
        complete = pollcomplete(ctlr);

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss: command completed\n");
#endif /* CCISS_DEBUG */

	if (complete != 1) {
		if ( (complete & CISS_ERROR_BIT)
		     && (complete & ~CISS_ERROR_BIT) == c->busaddr) {
			/* if data overrun or underun on Report command 
				ignore it 
			*/
			if (((c->Request.CDB[0] == CISS_REPORT_LOG) ||
			     (c->Request.CDB[0] == CISS_REPORT_PHYS) ||
			     (c->Request.CDB[0] == CISS_INQUIRY)) &&
				((c->err_info->CommandStatus == 
					CMD_DATA_OVERRUN) || 
				 (c->err_info->CommandStatus == 
					CMD_DATA_UNDERRUN)
			 	)) {
				complete = c->busaddr;
			} else {
				if (c->err_info->CommandStatus == 
						CMD_UNSOLICITED_ABORT) {
					printk(KERN_WARNING "cciss: "
						"cmd %p aborted do "
					"to an unsolicited abort \n", c); 
					if (c->retry_count < MAX_CMD_RETRIES) {
						printk(KERN_WARNING
						   "retrying cmd\n");
						c->retry_count++;
						/* erase the old error */
						/* information */
						memset(c->err_info, 0, 
						   sizeof(ErrorInfo_struct));
						goto resend_cmd1;
					} else {
						printk(KERN_WARNING
						   "retried to many times\n");
						status = IO_ERROR;
						goto cleanup1;
					}
				}
				printk(KERN_WARNING "cciss cciss%d: sendcmd"
				" Error %x \n", ctlr, 
					c->err_info->CommandStatus); 
				printk(KERN_WARNING "cciss cciss%d: sendcmd"
				" offensive info\n"
				"  size %x\n   num %x   value %x\n", ctlr,
				  c->err_info->MoreErrInfo.Invalid_Cmd.offense_size,
				  c->err_info->MoreErrInfo.Invalid_Cmd.offense_num,
				  c->err_info->MoreErrInfo.Invalid_Cmd.offense_value);
				status = IO_ERROR;
				goto cleanup1;
			}
		}
                if (complete != c->busaddr) {
                        printk( KERN_WARNING "cciss cciss%d: SendCmd "
                      "Invalid command list address returned! (%lx)\n",
                                ctlr, complete);
                        status = IO_ERROR;
			goto cleanup1;
                }
        } else {
                printk( KERN_WARNING
                        "cciss cciss%d: SendCmd Timeout out, "
                        "No command list address returned!\n",
                        ctlr);
                status = IO_ERROR;
        }
		
cleanup1:	
	/* unlock the data buffer from DMA */
	pci_unmap_single(info_p->pdev, (dma_addr_t) buff_dma_handle.val,
                                size, PCI_DMA_BIDIRECTIONAL);
	cmd_free(info_p, c, 1);
        return status;
} 
/*
 * Map (physical) PCI mem into (virtual) kernel space
 */
static ulong remap_pci_mem(ulong base, ulong size)
{
        ulong page_base        = ((ulong) base) & PAGE_MASK;
        ulong page_offs        = ((ulong) base) - page_base;
        ulong page_remapped    = (ulong) ioremap(page_base, page_offs+size);

        return (ulong) (page_remapped ? (page_remapped + page_offs) : 0UL);
}

/*
 * Enqueuing and dequeuing functions for cmdlists.
 */
static inline void addQ(CommandList_struct **Qptr, CommandList_struct *c)
{
        if (*Qptr == NULL) {
                *Qptr = c;
                c->next = c->prev = c;
        } else {
                c->prev = (*Qptr)->prev;
                c->next = (*Qptr);
                (*Qptr)->prev->next = c;
                (*Qptr)->prev = c;
        }
}

static inline CommandList_struct *removeQ(CommandList_struct **Qptr, 
						CommandList_struct *c)
{
        if (c && c->next != c) {
                if (*Qptr == c) *Qptr = c->next;
                c->prev->next = c->next;
                c->next->prev = c->prev;
        } else {
                *Qptr = NULL;
        }
        return c;
}

/* 
 * Takes jobs of the Q and sends them to the hardware, then puts it on 
 * the Q to wait for completion. 
 */ 
static void start_io( ctlr_info_t *h)
{
	CommandList_struct *c;
	
	while(( c = h->reqQ) != NULL ) {
		/* can't do anything if fifo is full */
		if ((h->access.fifo_full(h))) {
			printk(KERN_WARNING "cciss: fifo full \n");
			return;
		}
		/* Get the frist entry from the Request Q */ 
		removeQ(&(h->reqQ), c);
		h->Qdepth--;
	
		/* Tell the controller execute command */ 
		h->access.submit_command(h, c);
		
		/* Put job onto the completed Q */ 
		addQ (&(h->cmpQ), c); 
	}
}

static inline void complete_buffers( struct buffer_head *bh, int status)
{
	struct buffer_head *xbh;
	
	while(bh) {
		xbh = bh->b_reqnext; 
		bh->b_reqnext = NULL; 
		blk_finished_io(bh->b_size >> 9);
		bh->b_end_io(bh, status);
		bh = xbh;
	}
} 
/* This code assumes io_request_lock is already held */
/* Zeros out the error record and then resends the command back */
/* to the controller */ 
static inline void resend_cciss_cmd( ctlr_info_t *h, CommandList_struct *c)
{
	/* erase the old error information */
	memset(c->err_info, 0, sizeof(ErrorInfo_struct));

	/* add it to software queue and then send it to the controller */
	addQ(&(h->reqQ),c);
	h->Qdepth++;
	if (h->Qdepth > h->maxQsinceinit)
		h->maxQsinceinit = h->Qdepth; 

	start_io(h);
}
/* checks the status of the job and calls complete buffers to mark all 
 * buffers for the completed job. 
 */ 
static inline void complete_command( ctlr_info_t *h, CommandList_struct *cmd, 
		int timeout)
{
	int status = 1;
	int retry_cmd = 0;
	int i, ddir;
	u64bit temp64;
		
	if (timeout)
		status = 0; 

	if (cmd->err_info->CommandStatus != 0) { 
		/* an error has occurred */ 
		switch (cmd->err_info->CommandStatus) {
			unsigned char sense_key;
			case CMD_TARGET_STATUS:
				status = 0;
			
				if (cmd->err_info->ScsiStatus == 0x02) {
					printk(KERN_WARNING "cciss: cmd %p "
						"has CHECK CONDITION,"
						" sense key = 0x%x\n", cmd,
						cmd->err_info->SenseInfo[2]);
					/* check the sense key */
					sense_key = 0xf & 
						cmd->err_info->SenseInfo[2];
					/* recovered error */
					if ( sense_key == 0x1)
						status = 1;
				} else {
					printk(KERN_WARNING "cciss: cmd %p "
						"has SCSI Status 0x%x\n",
						cmd, cmd->err_info->ScsiStatus);
				}
			break;
			case CMD_DATA_UNDERRUN:
				printk(KERN_WARNING "cciss: cmd %p has"
					" completed with data underrun "
					"reported\n", cmd);
			break;
			case CMD_DATA_OVERRUN:
				printk(KERN_WARNING "cciss: cmd %p has"
					" completed with data overrun "
					"reported\n", cmd);
			break;
			case CMD_INVALID:
				printk(KERN_WARNING "cciss: cmd %p is "
					"reported invalid\n", cmd);
				status = 0;
			break;
			case CMD_PROTOCOL_ERR:
                                printk(KERN_WARNING "cciss: cmd %p has "
					"protocol error \n", cmd);
                                status = 0;
                        break;
			case CMD_HARDWARE_ERR:
                                printk(KERN_WARNING "cciss: cmd %p had " 
                                        " hardware error\n", cmd);
                                status = 0;
                        break;
			case CMD_CONNECTION_LOST:
				printk(KERN_WARNING "cciss: cmd %p had "
					"connection lost\n", cmd);
				status=0;
			break;
			case CMD_ABORTED:
				printk(KERN_WARNING "cciss: cmd %p was "
					"aborted\n", cmd);
				status=0;
			break;
			case CMD_ABORT_FAILED:
				printk(KERN_WARNING "cciss: cmd %p reports "
					"abort failed\n", cmd);
				status=0;
			break;
			case CMD_UNSOLICITED_ABORT:
				printk(KERN_WARNING "cciss: cmd %p aborted do "
					"to an unsolicited abort \n",
				       	cmd);
				if (cmd->retry_count < MAX_CMD_RETRIES) {
					retry_cmd=1;
					printk(KERN_WARNING
						"retrying cmd\n");
					cmd->retry_count++;
				} else {
					printk(KERN_WARNING
					"retried to many times\n");
				}
				status=0;
			break;
			case CMD_TIMEOUT:
				printk(KERN_WARNING "cciss: cmd %p timedout\n",
					cmd);
				status=0;
			break;
			default:
				printk(KERN_WARNING "cciss: cmd %p returned "
					"unknown status %x\n", cmd, 
						cmd->err_info->CommandStatus); 
				status=0;
		}
	}
	/* We need to return this command */
	if (retry_cmd) {
		resend_cciss_cmd(h,cmd);
		return;
	}	
	/* command did not need to be retried */
	/* unmap the DMA mapping for all the scatter gather elements */
	if (cmd->Request.Type.Direction == XFER_READ)
		ddir = PCI_DMA_FROMDEVICE;
	else
		ddir = PCI_DMA_TODEVICE;
	for(i=0; i<cmd->Header.SGList; i++) {
		temp64.val32.lower = cmd->SG[i].Addr.lower;
		temp64.val32.upper = cmd->SG[i].Addr.upper;
		pci_unmap_page(hba[cmd->ctlr]->pdev,
			temp64.val, cmd->SG[i].Len, ddir);
	}
	complete_buffers(cmd->rq->bh, status);
#ifdef CCISS_DEBUG
	printk("Done with %p\n", cmd->rq);
#endif /* CCISS_DEBUG */ 
	end_that_request_last(cmd->rq);
	cmd_free(h,cmd,1);
}


static inline int cpq_new_segment(request_queue_t *q, struct request *rq,
                                  int max_segments)
{
        if (rq->nr_segments < MAXSGENTRIES) {
                rq->nr_segments++;
                return 1;
        }
        return 0;
}

static int cpq_back_merge_fn(request_queue_t *q, struct request *rq,
                             struct buffer_head *bh, int max_segments)
{
	if (blk_seg_merge_ok(rq->bhtail, bh))	
                return 1;
        return cpq_new_segment(q, rq, max_segments);
}

static int cpq_front_merge_fn(request_queue_t *q, struct request *rq,
                             struct buffer_head *bh, int max_segments)
{
	if (blk_seg_merge_ok(bh, rq->bh))
                return 1;
        return cpq_new_segment(q, rq, max_segments);
}

static int cpq_merge_requests_fn(request_queue_t *q, struct request *rq,
                                 struct request *nxt, int max_segments)
{
        int total_segments = rq->nr_segments + nxt->nr_segments;

	if (blk_seg_merge_ok(rq->bhtail, nxt->bh))
                total_segments--;

        if (total_segments > MAXSGENTRIES)
                return 0;

        rq->nr_segments = total_segments;
        return 1;
}

/* 
 * Get a request and submit it to the controller. 
 * Currently we do one request at a time.  Ideally we would like to send
 * everything to the controller on the first call, but there is a danger
 * of holding the io_request_lock for to long.  
 */
static void do_cciss_request(request_queue_t *q)
{
	ctlr_info_t *h= q->queuedata; 
	CommandList_struct *c;
	int log_unit, start_blk, seg;
	unsigned long long lastdataend;
	struct buffer_head *bh;
	struct list_head *queue_head = &q->queue_head;
	struct request *creq;
	u64bit temp64;
	struct scatterlist tmp_sg[MAXSGENTRIES];
	int i, ddir;

	if (q->plugged)
		goto startio;

next:
	if (list_empty(queue_head))
		goto startio;

	creq =	blkdev_entry_next_request(queue_head); 
	if (creq->nr_segments > MAXSGENTRIES)
                BUG();

	if( h->ctlr != map_major_to_ctlr[MAJOR(creq->rq_dev)] ) {
                printk(KERN_WARNING "doreq cmd for %d, %x at %p\n",
                                h->ctlr, creq->rq_dev, creq);
                blkdev_dequeue_request(creq);
                complete_buffers(creq->bh, 0);
		end_that_request_last(creq);
		goto startio;
        }

	/* make sure controller is alive. */
	if (!CTLR_IS_ALIVE(h)) {
                printk(KERN_WARNING "cciss%d: I/O quit ", h->ctlr);
                blkdev_dequeue_request(creq);
                complete_buffers(creq->bh, 0);
		end_that_request_last(creq);
		return;
	}

	if (( c = cmd_alloc(h, 1)) == NULL)
		goto startio;

	blkdev_dequeue_request(creq);

	spin_unlock_irq(&io_request_lock);

	c->cmd_type = CMD_RWREQ;      
	c->rq = creq;
	bh = creq->bh;
	
	/* fill in the request */ 
	log_unit = MINOR(creq->rq_dev) >> NWD_SHIFT; 
	c->Header.ReplyQueue = 0;  /* unused in simple mode */
	c->Header.Tag.lower = c->busaddr;  /* use the physical address */
					/* the cmd block for tag */
	c->Header.LUN.LogDev.VolId= hba[h->ctlr]->drv[log_unit].LunID;
	c->Header.LUN.LogDev.Mode = 1;
	c->Request.CDBLen = 10; /* 12 byte commands not in FW yet. */
	c->Request.Type.Type =  TYPE_CMD; /* It is a command.  */
	c->Request.Type.Attribute = ATTR_SIMPLE; 
	c->Request.Type.Direction = 
		(creq->cmd == READ) ? XFER_READ: XFER_WRITE; 
	c->Request.Timeout = 0; /* Don't time out */
	c->Request.CDB[0] = (creq->cmd == READ) ? CCISS_READ : CCISS_WRITE;
	start_blk = hba[h->ctlr]->hd[MINOR(creq->rq_dev)].start_sect + creq->sector;
#ifdef CCISS_DEBUG
	if (bh == NULL)
		panic("cciss: bh== NULL?");
	printk(KERN_DEBUG "cciss: sector =%d nr_sectors=%d\n",(int) creq->sector,
		(int) creq->nr_sectors);	
#endif /* CCISS_DEBUG */
	seg = 0;
	lastdataend = ~0ULL;
	while(bh) {
		if (bh_phys(bh) == lastdataend)
		{  /* tack it on to the last segment */
			tmp_sg[seg-1].length +=bh->b_size;
			lastdataend += bh->b_size;
		} else {
			if (seg == MAXSGENTRIES)
				BUG();
			tmp_sg[seg].page = bh->b_page;
			tmp_sg[seg].length = bh->b_size;
			tmp_sg[seg].offset = bh_offset(bh);
			lastdataend = bh_phys(bh) + bh->b_size;
			seg++;
		}
		bh = bh->b_reqnext;
	}

	/* get the DMA records for the setup */ 
	if (c->Request.Type.Direction == XFER_READ)
		ddir = PCI_DMA_FROMDEVICE;
	else
		ddir = PCI_DMA_TODEVICE;
	for (i=0; i<seg; i++) {
		c->SG[i].Len = tmp_sg[i].length;
		temp64.val = pci_map_page(h->pdev, tmp_sg[i].page,
			    tmp_sg[i].offset, tmp_sg[i].length, ddir);
		c->SG[i].Addr.lower = temp64.val32.lower;
                c->SG[i].Addr.upper = temp64.val32.upper;
                c->SG[i].Ext = 0;  /* we are not chaining */
	}
	/* track how many SG entries we are using */ 
	if (seg > h->maxSG)
		h->maxSG = seg; 

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "cciss: Submitting %d sectors in %d segments\n", sect, seg);
#endif /* CCISS_DEBUG */

	c->Header.SGList = c->Header.SGTotal = seg;
	c->Request.CDB[1]= 0;
	c->Request.CDB[2]= (start_blk >> 24) & 0xff;	/* MSB */
	c->Request.CDB[3]= (start_blk >> 16) & 0xff;
	c->Request.CDB[4]= (start_blk >>  8) & 0xff;
	c->Request.CDB[5]= start_blk & 0xff;
	c->Request.CDB[6]= 0; /* (sect >> 24) & 0xff; MSB */
	c->Request.CDB[7]= (creq->nr_sectors >>  8) & 0xff; 
	c->Request.CDB[8]= creq->nr_sectors & 0xff; 
	c->Request.CDB[9] = c->Request.CDB[11] = c->Request.CDB[12] = 0;

	spin_lock_irq(&io_request_lock);

	addQ(&(h->reqQ),c);
	h->Qdepth++;
	if (h->Qdepth > h->maxQsinceinit)
		h->maxQsinceinit = h->Qdepth; 

	goto next;

startio:
	start_io(h);
}

static void do_cciss_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	ctlr_info_t *h = dev_id;
	CommandList_struct *c;
	unsigned long flags;
	__u32 a, a1;


	/* Is this interrupt for us? */
	if ((h->access.intr_pending(h) == 0) || (h->interrupts_enabled == 0))
		return;

	/*
	 * If there are completed commands in the completion queue,
	 * we had better do something about it.
	 */
	spin_lock_irqsave(&io_request_lock, flags);
	while( h->access.intr_pending(h)) {
		while((a = h->access.command_completed(h)) != FIFO_EMPTY) {
			a1 = a;
			a &= ~3;
			if ((c = h->cmpQ) == NULL) {  
				printk(KERN_WARNING "cciss: Completion of %08lx ignored\n", (unsigned long)a1);
				continue;	
			} 
			while(c->busaddr != a) {
				c = c->next;
				if (c == h->cmpQ) 
					break;
			}
			/*
			 * If we've found the command, take it off the
			 * completion Q and free it
			 */
			 if (c->busaddr == a) {
				removeQ(&h->cmpQ, c);
				if (c->cmd_type == CMD_RWREQ) {
					complete_command(h, c, 0);
				} else if (c->cmd_type == CMD_IOCTL_PEND) {
					complete(c->waiting);
				}
#				ifdef CONFIG_CISS_SCSI_TAPE
				else if (c->cmd_type == CMD_SCSI) {
					complete_scsi_command(c, 0, a1);
				}
#				endif
				continue;
			}
		}
	}
	/*
	 * See if we can queue up some more IO
	 */
	do_cciss_request(BLK_DEFAULT_QUEUE(h->major));
	spin_unlock_irqrestore(&io_request_lock, flags);
}
/* 
 *  We cannot read the structure directly, for portablity we must use 
 *   the io functions.
 *   This is for debug only. 
 */
#ifdef CCISS_DEBUG
static void print_cfg_table( CfgTable_struct *tb)
{
	int i;
	char temp_name[17];

	printk("Controller Configuration information\n");
	printk("------------------------------------\n");
	for(i=0;i<4;i++)
		temp_name[i] = readb(&(tb->Signature[i]));
	temp_name[4]='\0';
	printk("   Signature = %s\n", temp_name); 
	printk("   Spec Number = %d\n", readl(&(tb->SpecValence)));
	printk("   Transport methods supported = 0x%x\n", 
				readl(&(tb-> TransportSupport)));
	printk("   Transport methods active = 0x%x\n", 
				readl(&(tb->TransportActive)));
	printk("   Requested transport Method = 0x%x\n", 
			readl(&(tb->HostWrite.TransportRequest)));
	printk("   Coalese Interrupt Delay = 0x%x\n", 
			readl(&(tb->HostWrite.CoalIntDelay)));
	printk("   Coalese Interrupt Count = 0x%x\n", 
			readl(&(tb->HostWrite.CoalIntCount)));
	printk("   Max outstanding commands = 0x%d\n", 
			readl(&(tb->CmdsOutMax)));
	printk("   Bus Types = 0x%x\n", readl(&(tb-> BusTypes)));
	for(i=0;i<16;i++)
		temp_name[i] = readb(&(tb->ServerName[i]));
	temp_name[16] = '\0';
	printk("   Server Name = %s\n", temp_name);
	printk("   Heartbeat Counter = 0x%x\n\n\n", 
			readl(&(tb->HeartBeat)));
}
#endif /* CCISS_DEBUG */ 

static void release_io_mem(ctlr_info_t *c)
{
	/* if IO mem was not protected do nothing */
	if (c->io_mem_addr == 0)
		return;
	release_region(c->io_mem_addr, c->io_mem_length);
	c->io_mem_addr = 0;
	c->io_mem_length = 0;
}
static int find_PCI_BAR_index(struct pci_dev *pdev,
               unsigned long pci_bar_addr)
{
	int i, offset, mem_type, bar_type;
	if (pci_bar_addr == PCI_BASE_ADDRESS_0) /* looking for BAR zero? */
		return 0;
	offset = 0;
	for (i=0; i<DEVICE_COUNT_RESOURCE; i++) {
		bar_type = pci_resource_flags(pdev, i) &
			PCI_BASE_ADDRESS_SPACE; 
		if (bar_type == PCI_BASE_ADDRESS_SPACE_IO)
			offset += 4;
		else {
			mem_type = pci_resource_flags(pdev, i) &
				PCI_BASE_ADDRESS_MEM_TYPE_MASK; 
			switch (mem_type) {
				case PCI_BASE_ADDRESS_MEM_TYPE_32:
				case PCI_BASE_ADDRESS_MEM_TYPE_1M:
					offset += 4; /* 32 bit */
					break;
				case PCI_BASE_ADDRESS_MEM_TYPE_64:
					offset += 8;
					break;
				default: /* reserved in PCI 2.2 */
					printk(KERN_WARNING "Base address is invalid\n");
					return -1;	
				break;
			}
		}
		if (offset == pci_bar_addr - PCI_BASE_ADDRESS_0)
			return i+1;
	}
	return -1;
}
			
static int cciss_pci_init(ctlr_info_t *c, struct pci_dev *pdev)
{
	ushort subsystem_vendor_id, subsystem_device_id, command;
	unchar irq = pdev->irq, ready = 0;
	__u32 board_id, scratchpad;
	__u64 cfg_offset;
	__u32 cfg_base_addr;
	__u64 cfg_base_addr_index;
	int i;

	/* check to see if controller has been disabled */
	/* BEFORE we try to enable it */
	(void) pci_read_config_word(pdev, PCI_COMMAND,&command);
	if (!(command & 0x02)) {
		printk(KERN_WARNING "cciss: controller appears to be disabled\n");
		return -1;
	}
	if (pci_enable_device(pdev)) {
		printk(KERN_ERR "cciss: Unable to Enable PCI device\n");
		return -1;
	}
	if (pci_set_dma_mask(pdev, CCISS_DMA_MASK ) != 0) {
		printk(KERN_ERR "cciss:  Unable to set DMA mask\n");
		return -1;
	}
	
	subsystem_vendor_id = pdev->subsystem_vendor;
	subsystem_device_id = pdev->subsystem_device;
	board_id = (((__u32) (subsystem_device_id << 16) & 0xffff0000) |
					subsystem_vendor_id );


	/* search for our IO range so we can protect it */
	for (i=0; i<DEVICE_COUNT_RESOURCE; i++) {
		/* is this an IO range */
		if (pci_resource_flags(pdev, i) & 0x01) {
			c->io_mem_addr = pci_resource_start(pdev, i);
			c->io_mem_length = pci_resource_end(pdev, i) -
				pci_resource_start(pdev, i) + 1; 
#ifdef CCISS_DEBUG
			printk("IO value found base_addr[%d] %lx %lx\n", i,
				c->io_mem_addr, c->io_mem_length);
#endif /* CCISS_DEBUG */
			/* register the IO range */
			if (!request_region( c->io_mem_addr,
                                        c->io_mem_length, "cciss")) {
				printk(KERN_WARNING 
					"cciss I/O memory range already in "
					"use addr=%lx length=%ld\n",
				c->io_mem_addr, c->io_mem_length);
				c->io_mem_addr= 0;
				c->io_mem_length = 0;
			}
			break;
		}
	}

#ifdef CCISS_DEBUG
	printk("command = %x\n", command);
	printk("irq = %x\n", irq);
	printk("board_id = %x\n", board_id);
#endif /* CCISS_DEBUG */ 

	c->intr = irq;

	/*
	 * Memory base addr is first addr , the second points to the config
         *   table
	 */

	c->paddr = pci_resource_start(pdev, 0); /* addressing mode bits already removed */
#ifdef CCISS_DEBUG
	printk("address 0 = %x\n", c->paddr);
#endif /* CCISS_DEBUG */ 
	c->vaddr = remap_pci_mem(c->paddr, 200);
	/* Wait for the board to become ready.  (PCI hotplug needs this.)
	 * We poll for up to 120 secs, once per 100ms. */
	for (i=0; i < 1200; i++) {
		scratchpad = readl(c->vaddr + SA5_SCRATCHPAD_OFFSET);
		if (scratchpad == 0xffff0000) {
			ready = 1;
			break;
		}
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 10); /* wait 100ms */
	}
	if (!ready) {
		printk(KERN_WARNING "cciss: Board not ready.  Timed out.\n");
		return -1;
	}

	/* get the address index number */
	cfg_base_addr = readl(c->vaddr + SA5_CTCFG_OFFSET);
	cfg_base_addr &= (__u32) 0x0000ffff;
#ifdef CCISS_DEBUG
	printk("cfg base address = %x\n", cfg_base_addr);
#endif /* CCISS_DEBUG */
	cfg_base_addr_index =
		find_PCI_BAR_index(pdev, cfg_base_addr);
#ifdef CCISS_DEBUG
	printk("cfg base address index = %x\n", cfg_base_addr_index);
#endif /* CCISS_DEBUG */
	if (cfg_base_addr_index == -1) {
		printk(KERN_WARNING "cciss: Cannot find cfg_base_addr_index\n");
		release_io_mem(c);
		return -1;
	}

	cfg_offset = readl(c->vaddr + SA5_CTMEM_OFFSET);
#ifdef CCISS_DEBUG
	printk("cfg offset = %x\n", cfg_offset);
#endif /* CCISS_DEBUG */
	c->cfgtable = (CfgTable_struct *) 
		remap_pci_mem(pci_resource_start(pdev, cfg_base_addr_index)
				+ cfg_offset, sizeof(CfgTable_struct));
	c->board_id = board_id;

#ifdef CCISS_DEBUG
	print_cfg_table(c->cfgtable); 
#endif /* CCISS_DEBUG */

	for(i=0; i<NR_PRODUCTS; i++) {
		if (board_id == products[i].board_id) {
			c->product_name = products[i].product_name;
			c->access = *(products[i].access);
			break;
		}
	}
	if (i == NR_PRODUCTS) {
		printk(KERN_WARNING "cciss: Sorry, I don't know how"
			" to access the Smart Array controller %08lx\n", 
				(unsigned long)board_id);
		return -1;
	}
	if (  (readb(&c->cfgtable->Signature[0]) != 'C') ||
	      (readb(&c->cfgtable->Signature[1]) != 'I') ||
	      (readb(&c->cfgtable->Signature[2]) != 'S') ||
	      (readb(&c->cfgtable->Signature[3]) != 'S') ) {
		printk("Does not appear to be a valid CISS config table\n");
		return -1;
	}

#ifdef CONFIG_X86
{
	/* Need to enable prefetch in the SCSI core for 6400 in x86 */
	__u32 prefetch;
	prefetch = readl(&(c->cfgtable->SCSI_Prefetch));
	prefetch |= 0x100;
	writel(prefetch, &(c->cfgtable->SCSI_Prefetch));
}
#endif

#ifdef CCISS_DEBUG
	printk("Trying to put board into Simple mode\n");
#endif /* CCISS_DEBUG */ 
	c->max_commands = readl(&(c->cfgtable->CmdsOutMax));
	/* Update the field, and then ring the doorbell */ 
	writel( CFGTBL_Trans_Simple, 
		&(c->cfgtable->HostWrite.TransportRequest));
	writel( CFGTBL_ChangeReq, c->vaddr + SA5_DOORBELL);

	/* Here, we wait, possibly for a long time, (4 secs or more). 
	 * In some unlikely cases, (e.g. A failed 144 GB drive in a 
	 * RAID 5 set was hot replaced just as we're coming in here) it 
	 * can take that long.  Normally (almost always) we will wait 
	 * less than 1 sec. */
	for(i=0;i<MAX_CONFIG_WAIT;i++) {
		if (!(readl(c->vaddr + SA5_DOORBELL) & CFGTBL_ChangeReq))
			break;
		/* delay and try again */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}	

#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "I counter got to %d %x\n", i, readl(c->vaddr + SA5_DOORBELL));
#endif /* CCISS_DEBUG */
#ifdef CCISS_DEBUG
	print_cfg_table(c->cfgtable);	
#endif /* CCISS_DEBUG */ 

	if (!(readl(&(c->cfgtable->TransportActive)) & CFGTBL_Trans_Simple)) {
		printk(KERN_WARNING "cciss: unable to get board into"
					" simple mode\n");
		return -1;
	}
	return 0;

}

/* 
 * Gets information about the local volumes attached to the controller. 
 */ 
static void cciss_getgeometry(int cntl_num)
{
	ReportLunData_struct *ld_buff;
	ReadCapdata_struct *size_buff;
	InquiryData_struct *inq_buff;
	int return_code;
	int i;
	int listlength = 0;
	__u32 lunid = 0;
	int block_size;
	int total_size; 

	ld_buff = kmalloc(sizeof(ReportLunData_struct), GFP_KERNEL);
	if (ld_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		return;
	}
	memset(ld_buff, 0, sizeof(ReportLunData_struct));
	size_buff = kmalloc(sizeof( ReadCapdata_struct), GFP_KERNEL);
        if (size_buff == NULL) {
                printk(KERN_ERR "cciss: out of memory\n");
		kfree(ld_buff);
                return;
        }
	inq_buff = kmalloc(sizeof( InquiryData_struct), GFP_KERNEL);
        if (inq_buff == NULL) {
                printk(KERN_ERR "cciss: out of memory\n");
                kfree(ld_buff);
		kfree(size_buff);
                return;
        }
	/* Get the firmware version */ 
	return_code = sendcmd(CISS_INQUIRY, cntl_num, inq_buff, 
		sizeof(InquiryData_struct), 0, 0 ,0, NULL);
	if (return_code == IO_OK) {
		hba[cntl_num]->firm_ver[0] = inq_buff->data_byte[32];
		hba[cntl_num]->firm_ver[1] = inq_buff->data_byte[33];
		hba[cntl_num]->firm_ver[2] = inq_buff->data_byte[34];
		hba[cntl_num]->firm_ver[3] = inq_buff->data_byte[35];
	} else  {	/* send command failed */
		printk(KERN_WARNING "cciss: unable to determine firmware"
			" version of controller\n");
	}
	/* Get the number of logical volumes */ 
	return_code = sendcmd(CISS_REPORT_LOG, cntl_num, ld_buff, 
			sizeof(ReportLunData_struct), 0, 0, 0, NULL);

	if (return_code == IO_OK) {
#ifdef CCISS_DEBUG
		printk("LUN Data\n--------------------------\n");
#endif /* CCISS_DEBUG */ 

		listlength = be32_to_cpu(*((__u32 *) &ld_buff->LUNListLength[0]));
	} else { /* reading number of logical volumes failed */
		printk(KERN_WARNING "cciss: report logical volume"
			" command failed\n");
		listlength = 0;
	}
	hba[cntl_num]->num_luns = listlength / 8; /* 8 bytes pre entry */
	if (hba[cntl_num]->num_luns > CISS_MAX_LUN) {
		printk(KERN_ERR "cciss:  only %d number of logical volumes supported\n",
			CISS_MAX_LUN);
		hba[cntl_num]->num_luns = CISS_MAX_LUN;
	}
#ifdef CCISS_DEBUG
	printk(KERN_DEBUG "Length = %x %x %x %x = %d\n", ld_buff->LUNListLength[0],
		ld_buff->LUNListLength[1], ld_buff->LUNListLength[2],
		ld_buff->LUNListLength[3],  hba[cntl_num]->num_luns);
#endif /* CCISS_DEBUG */

	hba[cntl_num]->highest_lun = hba[cntl_num]->num_luns-1;
	for(i=0; i<  hba[cntl_num]->num_luns; i++) {
	  	lunid = (0xff & (unsigned int)(ld_buff->LUN[i][3])) << 24;
        	lunid |= (0xff & (unsigned int)(ld_buff->LUN[i][2])) << 16;
        	lunid |= (0xff & (unsigned int)(ld_buff->LUN[i][1])) << 8;
        	lunid |= 0xff & (unsigned int)(ld_buff->LUN[i][0]);
		hba[cntl_num]->drv[i].LunID = lunid;

#ifdef CCISS_DEBUG
	  	printk(KERN_DEBUG "LUN[%d]:  %x %x %x %x = %x\n", i, 
		ld_buff->LUN[i][0], ld_buff->LUN[i][1],ld_buff->LUN[i][2], 
		ld_buff->LUN[i][3], hba[cntl_num]->drv[i].LunID);
#endif /* CCISS_DEBUG */

	  	memset(size_buff, 0, sizeof(ReadCapdata_struct));
	  	return_code = sendcmd(CCISS_READ_CAPACITY, cntl_num, size_buff, 
				sizeof( ReadCapdata_struct), 1, i, 0, NULL);
	  	if (return_code == IO_OK) {
			total_size = (0xff & 
				(unsigned int)(size_buff->total_size[0])) << 24;
			total_size |= (0xff & 
				(unsigned int)(size_buff->total_size[1])) << 16;
			total_size |= (0xff & 
				(unsigned int)(size_buff->total_size[2])) << 8;
			total_size |= (0xff & (unsigned int)
				(size_buff->total_size[3])); 
			total_size++; 	/* command returns highest */
					/* block address */

			block_size = (0xff & 
				(unsigned int)(size_buff->block_size[0])) << 24;
                	block_size |= (0xff & 
				(unsigned int)(size_buff->block_size[1])) << 16;
                	block_size |= (0xff & 
				(unsigned int)(size_buff->block_size[2])) << 8;
                	block_size |= (0xff & 
				(unsigned int)(size_buff->block_size[3]));
		} else {	/* read capacity command failed */ 
			printk(KERN_WARNING "cciss: read capacity failed\n");
			total_size = block_size = 0; 
		}	
		printk(KERN_INFO "      blocks= %d block_size= %d\n", 
					total_size, block_size);

		/* Execute the command to read the disk geometry */
		memset(inq_buff, 0, sizeof(InquiryData_struct));
		return_code = sendcmd(CISS_INQUIRY, cntl_num, inq_buff,
			sizeof(InquiryData_struct), 1, i, 0xC1, NULL );
	  	if (return_code == IO_OK) {
			if (inq_buff->data_byte[8] == 0xFF) {
			   printk(KERN_WARNING "cciss: reading geometry failed, volume does not support reading geometry\n");

                           hba[cntl_num]->drv[i].block_size = block_size;
                           hba[cntl_num]->drv[i].nr_blocks = total_size;
                           hba[cntl_num]->drv[i].heads = 255;
                           hba[cntl_num]->drv[i].sectors = 32; /* Sectors */
			   					/* per track */
                           hba[cntl_num]->drv[i].cylinders = total_size 
				   				/ 255 / 32;
			} else {

		 	   hba[cntl_num]->drv[i].block_size = block_size;
                           hba[cntl_num]->drv[i].nr_blocks = total_size;
                           hba[cntl_num]->drv[i].heads = 
					inq_buff->data_byte[6]; 
                           hba[cntl_num]->drv[i].sectors = 
					inq_buff->data_byte[7]; 
			   hba[cntl_num]->drv[i].cylinders = 
					(inq_buff->data_byte[4] & 0xff) << 8;
			   hba[cntl_num]->drv[i].cylinders += 
                                        inq_buff->data_byte[5];
                           hba[cntl_num]->drv[i].raid_level = 
					inq_buff->data_byte[8]; 
			}
		}
		else {	/* Get geometry failed */
			printk(KERN_WARNING "cciss: reading geometry failed, continuing with default geometry\n"); 

			hba[cntl_num]->drv[i].block_size = block_size;
			hba[cntl_num]->drv[i].nr_blocks = total_size;
			hba[cntl_num]->drv[i].heads = 255;
			hba[cntl_num]->drv[i].sectors = 32; 	/* Sectors */
								/* per track */
			hba[cntl_num]->drv[i].cylinders = total_size / 255 / 32;
		}
		if (hba[cntl_num]->drv[i].raid_level > 5)
			hba[cntl_num]->drv[i].raid_level = RAID_UNKNOWN;
		printk(KERN_INFO "      heads= %d, sectors= %d, cylinders= %d RAID %s\n\n",
			hba[cntl_num]->drv[i].heads, 
			hba[cntl_num]->drv[i].sectors,
			hba[cntl_num]->drv[i].cylinders,
			raid_label[hba[cntl_num]->drv[i].raid_level]); 
	}
	kfree(ld_buff);
	kfree(size_buff);
	kfree(inq_buff);
}	

/* Function to find the first free pointer into our hba[] array */
/* Returns -1 if no free entries are left.  */
static int alloc_cciss_hba(void)
{
	int i;
	for(i=0; i< MAX_CTLR; i++) {
		if (hba[i] == NULL) {
			hba[i] = kmalloc(sizeof(ctlr_info_t), GFP_KERNEL);
			if (hba[i]==NULL) {
				printk(KERN_ERR "cciss: out of memory.\n");
				return -1;
			}
			return i;
		}
	}
	printk(KERN_WARNING 
		"cciss: This driver supports a maximum of %d controllers.\n"
		"You can change this value in cciss.c and recompile.\n",
		MAX_CTLR);
	return -1;
}

static void free_hba(int i)
{
	kfree(hba[i]);
	hba[i]=NULL;
}
#ifdef CONFIG_CISS_MONITOR_THREAD
static void fail_all_cmds(unsigned long ctlr)
{
	/* If we get here, the board is apparently dead. */
	ctlr_info_t *h = hba[ctlr];
	CommandList_struct *c;
	unsigned long flags;

	printk(KERN_WARNING "cciss%d: controller not responding.\n", h->ctlr);
	h->alive = 0;	/* the controller apparently died... */ 

	spin_lock_irqsave(&io_request_lock, flags);

	pci_disable_device(h->pdev); /* Make sure it is really dead. */

	/* move everything off the request queue onto the completed queue */
	while( (c = h->reqQ) != NULL ) {
		removeQ(&(h->reqQ), c);
		h->Qdepth--;
		addQ (&(h->cmpQ), c); 
	}

	/* Now, fail everything on the completed queue with a HW error */
	while( (c = h->cmpQ) != NULL ) {
		removeQ(&h->cmpQ, c);
		c->err_info->CommandStatus = CMD_HARDWARE_ERR;
		if (c->cmd_type == CMD_RWREQ) {
			complete_command(h, c, 0);
		} else if (c->cmd_type == CMD_IOCTL_PEND)
			complete(c->waiting);
#		ifdef CONFIG_CISS_SCSI_TAPE
			else if (c->cmd_type == CMD_SCSI)
				complete_scsi_command(c, 0, 0);
#		endif
	}
	spin_unlock_irqrestore(&io_request_lock, flags);
	return;
}
static int cciss_monitor(void *ctlr)
{
	/* If the board fails, we ought to detect that.  So we periodically 
	send down a No-Op message and expect it to complete quickly.  If it 
	doesn't, then we assume the board is dead, and fail all commands.  
	This is useful mostly in a multipath configuration, so that failover
	will happen. */

	int rc;
	ctlr_info_t *h = (ctlr_info_t *) ctlr;
	unsigned long flags;
	u32 current_timer;

	daemonize();
	exit_files(current);
	reparent_to_init();

	printk("cciss%d: Monitor thread starting.\n", h->ctlr); 

	/* only listen to signals if the HA was loaded as a module.  */
#define SHUTDOWN_SIGS   (sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM))
	siginitsetinv(&current->blocked, SHUTDOWN_SIGS);
	sprintf(current->comm, "ccissmon%d", h->ctlr);
	h->monitor_thread = current;

	init_timer(&h->watchdog); 
	h->watchdog.function = fail_all_cmds;
	h->watchdog.data = (unsigned long) h->ctlr;
	while (1) {
  		/* check heartbeat timer */
                current_timer = readl(&h->cfgtable->HeartBeat);
  		current_timer &= 0x0fffffff;
  		if (heartbeat_timer == current_timer) {
  			fail_all_cmds(h->ctlr);
  			break;
  		}
  		else
  			heartbeat_timer = current_timer;

		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(h->monitor_period * HZ);
		h->watchdog.expires = jiffies + HZ * h->monitor_deadline;
		add_timer(&h->watchdog);
		/* send down a trivial command (no op message) to ctlr */
		rc = sendcmd_withirq(3, h->ctlr, NULL, 0, 0, 0, 0, TYPE_MSG);
		del_timer(&h->watchdog);
		if (!CTLR_IS_ALIVE(h))
			break;
		if (signal_pending(current)) {
			printk(KERN_WARNING "%s received signal.\n",
				current->comm);
			break;
		}
		if (h->monitor_period == 0) /* zero period means exit thread */
			break;
	}
	printk(KERN_INFO "%s exiting.\n", current->comm);
	spin_lock_irqsave(&io_request_lock, flags);
	h->monitor_started = 0;
	h->monitor_thread = NULL;
	spin_unlock_irqrestore(&io_request_lock, flags);
	return 0;
}
static int start_monitor_thread(ctlr_info_t *h, unsigned char *cmd, 
		unsigned long count, int (*cciss_monitor)(void *), int *rc)
{
	unsigned long flags;
	unsigned int new_period, old_period, new_deadline, old_deadline;

	if (strncmp("monitor", cmd, 7) == 0) {
		new_period = simple_strtol(cmd + 8, NULL, 10);
		spin_lock_irqsave(&io_request_lock, flags);
		new_deadline = h->monitor_deadline;
		spin_unlock_irqrestore(&io_request_lock, flags);
	} else if (strncmp("deadline", cmd, 8) == 0) {
		new_deadline = simple_strtol(cmd + 9, NULL, 10);
		spin_lock_irqsave(&io_request_lock, flags);
		new_period = h->monitor_period;
		spin_unlock_irqrestore(&io_request_lock, flags);
	} else
		return -1;
	if (new_period != 0 && new_period < CCISS_MIN_PERIOD)
		new_period = CCISS_MIN_PERIOD;
	if (new_period > CCISS_MAX_PERIOD)
		new_period = CCISS_MAX_PERIOD;
	if (new_deadline >= new_period) {
		new_deadline = new_period - 5;
		printk(KERN_INFO "setting deadline to %d\n", new_deadline);
	}
	spin_lock_irqsave(&io_request_lock, flags);
	if (h->monitor_started != 0)  {
		old_period = h->monitor_period;
		old_deadline = h->monitor_deadline;
		h->monitor_period = new_period;
		h->monitor_deadline = new_deadline;
		spin_unlock_irqrestore(&io_request_lock, flags);
		if (new_period == 0) {
			printk(KERN_INFO "cciss%d: stopping monitor thread\n",
				h->ctlr);
			*rc = count;
			return 0;
		}
		if (new_period != old_period) 
			printk(KERN_INFO "cciss%d: adjusting monitor thread "
				"period from %d to %d seconds\n",
				h->ctlr, old_period, new_period);
		if (new_deadline != old_deadline) 
			printk(KERN_INFO "cciss%d: adjusting monitor thread "
				"deadline from %d to %d seconds\n",
				h->ctlr, old_deadline, new_deadline);
		*rc = count;
		return 0;
	}
	h->monitor_started = 1;
	h->monitor_period = new_period;
	h->monitor_deadline = new_deadline;
	spin_unlock_irqrestore(&io_request_lock, flags);
	kernel_thread(cciss_monitor, h, 0);
	*rc = count;
	return 0;
}

static void kill_monitor_thread(ctlr_info_t *h)
{
	if (h->monitor_thread)
		send_sig(SIGKILL, h->monitor_thread, 1);
}
#else
#define kill_monitor_thread(h)
#endif
/*
 *  This is it.  Find all the controllers and register them.  I really hate
 *  stealing all these major device numbers.
 *  returns the number of block devices registered.
 */
static int __init cciss_init_one(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	request_queue_t *q;
	int i;
	int j;
	int rc;

	printk(KERN_DEBUG "cciss: Device 0x%x has been found at"
			" bus %d dev %d func %d\n",
		pdev->device, pdev->bus->number, PCI_SLOT(pdev->devfn),
			PCI_FUNC(pdev->devfn));
	i = alloc_cciss_hba();
	if (i < 0 ) 
		return -1;
	memset(hba[i], 0, sizeof(ctlr_info_t));
	if (cciss_pci_init(hba[i], pdev) != 0) {
		free_hba(i);
		return -1;
	}
	sprintf(hba[i]->devname, "cciss%d", i);
	hba[i]->ctlr = i;

	/* register with the major number, or get a dynamic major number */
	/* by passing 0 as argument */

	if (i < MAX_CTLR_ORIG)
		hba[i]->major = MAJOR_NR + i;

	hba[i]->pdev = pdev;
	ASSERT_CTLR_ALIVE(hba[i]);

	rc = (register_blkdev(hba[i]->major, hba[i]->devname, &cciss_fops));
	if (rc < 0) {
		printk(KERN_ERR "cciss:  Unable to get major number "
			"%d for %s\n", hba[i]->major, hba[i]->devname);
		release_io_mem(hba[i]);
		free_hba(i);
		return -1;
	} else {
		if (i < MAX_CTLR_ORIG) {
			hba[i]->major = MAJOR_NR + i;
			map_major_to_ctlr[MAJOR_NR + i] = i;
		} else {
			hba[i]->major = rc;
			map_major_to_ctlr[rc] = i;
		}
	}

	/* make sure the board interrupts are off */
	hba[i]->access.set_intr_mask(hba[i], CCISS_INTR_OFF);
	if (request_irq(hba[i]->intr, do_cciss_intr, 
		SA_INTERRUPT | SA_SHIRQ | SA_SAMPLE_RANDOM, 
			hba[i]->devname, hba[i])) {

		printk(KERN_ERR "cciss: Unable to get irq %d for %s\n",
			hba[i]->intr, hba[i]->devname);
		unregister_blkdev( hba[i]->major, hba[i]->devname);
		map_major_to_ctlr[hba[i]->major] = 0;
		release_io_mem(hba[i]);
		free_hba(i);
		return -1;
	}
	hba[i]->cmd_pool_bits = (__u32*)kmalloc(
        	((NR_CMDS+31)/32)*sizeof(__u32), GFP_KERNEL);
	hba[i]->cmd_pool = (CommandList_struct *)pci_alloc_consistent(
		hba[i]->pdev, NR_CMDS * sizeof(CommandList_struct), 
		&(hba[i]->cmd_pool_dhandle));
	hba[i]->errinfo_pool = (ErrorInfo_struct *)pci_alloc_consistent(
		hba[i]->pdev, NR_CMDS * sizeof( ErrorInfo_struct), 
		&(hba[i]->errinfo_pool_dhandle));
	if ((hba[i]->cmd_pool_bits == NULL) 
		|| (hba[i]->cmd_pool == NULL)
		|| (hba[i]->errinfo_pool == NULL)) {

		if (hba[i]->cmd_pool_bits)
                	kfree(hba[i]->cmd_pool_bits);
                if (hba[i]->cmd_pool)
                	pci_free_consistent(hba[i]->pdev,  
				NR_CMDS * sizeof(CommandList_struct), 
				hba[i]->cmd_pool, hba[i]->cmd_pool_dhandle);	
		if (hba[i]->errinfo_pool)
			pci_free_consistent(hba[i]->pdev,
				NR_CMDS * sizeof( ErrorInfo_struct),
				hba[i]->errinfo_pool, 
				hba[i]->errinfo_pool_dhandle);
                free_irq(hba[i]->intr, hba[i]);
                unregister_blkdev(hba[i]->major, hba[i]->devname);
		map_major_to_ctlr[hba[i]->major] = 0;
		release_io_mem(hba[i]);
		free_hba(i);
                printk( KERN_ERR "cciss: out of memory");
		return -1;
	}

	/* Initialize the pdev driver private data. 
		have it point to hba[i].  */
	pci_set_drvdata(pdev, hba[i]);
	/* command and error info recs zeroed out before 
			they are used */
        memset(hba[i]->cmd_pool_bits, 0, ((NR_CMDS+31)/32)*sizeof(__u32));

#ifdef CCISS_DEBUG	
	printk(KERN_DEBUG "Scanning for drives on controller cciss%d\n",i);
#endif /* CCISS_DEBUG */

	cciss_getgeometry(i);

	cciss_find_non_disk_devices(i);	/* find our tape drives, if any */

	/* Turn the interrupts on so we can service requests */
	hba[i]->access.set_intr_mask(hba[i], CCISS_INTR_ON);

	cciss_procinit(i);

	q = BLK_DEFAULT_QUEUE(hba[i]->major);
	q->queuedata = hba[i];
	blk_init_queue(q, do_cciss_request);
	blk_queue_bounce_limit(q, hba[i]->pdev->dma_mask);
	blk_queue_headactive(q, 0);		

	/* fill in the other Kernel structs */
	blksize_size[hba[i]->major] = hba[i]->blocksizes;
        hardsect_size[hba[i]->major] = hba[i]->hardsizes;
        read_ahead[hba[i]->major] = READ_AHEAD;

	/* Set the pointers to queue functions */ 
	q->back_merge_fn = cpq_back_merge_fn;
        q->front_merge_fn = cpq_front_merge_fn;
        q->merge_requests_fn = cpq_merge_requests_fn;


	/* Fill in the gendisk data */ 	
	hba[i]->gendisk.major = hba[i]->major;
	hba[i]->gendisk.major_name = "cciss";
	hba[i]->gendisk.minor_shift = NWD_SHIFT;
	hba[i]->gendisk.max_p = MAX_PART;
	hba[i]->gendisk.part = hba[i]->hd;
	hba[i]->gendisk.sizes = hba[i]->sizes;
	hba[i]->gendisk.nr_real = hba[i]->highest_lun+1;
	hba[i]->gendisk.fops = &cciss_fops;

	/* Get on the disk list */ 
	add_gendisk(&(hba[i]->gendisk));

	cciss_geninit(i);
	for(j=0; j<NWD; j++)
		register_disk(&(hba[i]->gendisk),
			MKDEV(hba[i]->major, j <<4), 
			MAX_PART, &cciss_fops, 
			hba[i]->drv[j].nr_blocks);

	cciss_register_scsi(i, 1);  /* hook ourself into SCSI subsystem */

	return 1;
}

static void __devexit cciss_remove_one (struct pci_dev *pdev)
{
	ctlr_info_t *tmp_ptr;
	int i;
	char flush_buf[4];
	int return_code; 

	if (pci_get_drvdata(pdev) == NULL) {
		printk( KERN_ERR "cciss: Unable to remove device \n");
		return;
	}
	tmp_ptr = pci_get_drvdata(pdev);
	i = tmp_ptr->ctlr;
	if (hba[i] == NULL) {
		printk(KERN_ERR "cciss: device appears to "
			"already be removed \n");
		return;
	}
	kill_monitor_thread(hba[i]);
	/* no sense in trying to flush a dead board's cache. */
	if (CTLR_IS_ALIVE(hba[i])) {
		/* Turn board interrupts off and flush the cache */
		/* write all data in the battery backed cache to disks */
 	memset(flush_buf, 0, 4);
		return_code = sendcmd(CCISS_CACHE_FLUSH, i, flush_buf,
					4, 0, 0, 0, NULL);
		if (return_code != IO_OK)
 		printk(KERN_WARNING 
				"cciss%d: Error flushing cache\n", i);
 	}
	free_irq(hba[i]->intr, hba[i]);
	pci_set_drvdata(pdev, NULL);
	iounmap((void*)hba[i]->vaddr);
	cciss_unregister_scsi(i);  /* unhook from SCSI subsystem */
	unregister_blkdev(hba[i]->major, hba[i]->devname);
	map_major_to_ctlr[hba[i]->major] = 0;
	remove_proc_entry(hba[i]->devname, proc_cciss);	
	

	/* remove it from the disk list */
	del_gendisk(&(hba[i]->gendisk));

	pci_free_consistent(hba[i]->pdev, NR_CMDS * sizeof(CommandList_struct), 
		hba[i]->cmd_pool, hba[i]->cmd_pool_dhandle);
	pci_free_consistent(hba[i]->pdev, NR_CMDS * sizeof( ErrorInfo_struct),
		hba[i]->errinfo_pool, hba[i]->errinfo_pool_dhandle);
	kfree(hba[i]->cmd_pool_bits);
	release_io_mem(hba[i]);
	free_hba(i);
}	

static struct pci_driver cciss_pci_driver = {
	 name:   "cciss",
	probe:  cciss_init_one,
	remove:  __devexit_p(cciss_remove_one),
	id_table:  cciss_pci_device_id, /* id_table */
};

/*
*  This is it.  Register the PCI driver information for the cards we control
*  the OS will call our registered routines when it finds one of our cards. 
*/
int __init cciss_init(void)
{

	printk(KERN_INFO DRIVER_NAME "\n");
	/* Register for out PCI devices */
	return pci_module_init(&cciss_pci_driver);
}

EXPORT_NO_SYMBOLS;
static int __init init_cciss_module(void)
{

	return cciss_init();
}

static void __exit cleanup_cciss_module(void)
{
	int i;

	pci_unregister_driver(&cciss_pci_driver);
	/* double check that all controller entrys have been removed */
	for (i=0; i< MAX_CTLR; i++) {
		if (hba[i] != NULL) {
			printk(KERN_WARNING "cciss: had to remove"
					" controller %d\n", i);
			cciss_remove_one(hba[i]->pdev);
		}
	}
	remove_proc_entry("cciss", proc_root_driver);
}

module_init(init_cciss_module);
module_exit(cleanup_cciss_module);
