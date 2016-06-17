/*
 * kernel/lvm.c
 *
 * Copyright (C) 1997 - 2002  Heinz Mauelshagen, Sistina Software
 *
 * February-November 1997
 * April-May,July-August,November 1998
 * January-March,May,July,September,October 1999
 * January,February,July,September-November 2000
 * January-May,June,October 2001
 * May-August 2002
 * February 2003
 *
 *
 * LVM driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * LVM driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/*
 * Changelog
 *
 *    09/11/1997 - added chr ioctls VG_STATUS_GET_COUNT
 *                 and VG_STATUS_GET_NAMELIST
 *    18/01/1998 - change lvm_chr_open/close lock handling
 *    30/04/1998 - changed LV_STATUS ioctl to LV_STATUS_BYNAME and
 *               - added   LV_STATUS_BYINDEX ioctl
 *               - used lvm_status_byname_req_t and
 *                      lvm_status_byindex_req_t vars
 *    04/05/1998 - added multiple device support
 *    08/05/1998 - added support to set/clear extendable flag in volume group
 *    09/05/1998 - changed output of lvm_proc_get_global_info() because of
 *                 support for free (eg. longer) logical volume names
 *    12/05/1998 - added spin_locks (thanks to Pascal van Dam
 *                 <pascal@ramoth.xs4all.nl>)
 *    25/05/1998 - fixed handling of locked PEs in lvm_map() and
 *                 lvm_chr_ioctl()
 *    26/05/1998 - reactivated verify_area by access_ok
 *    07/06/1998 - used vmalloc/vfree instead of kmalloc/kfree to go
 *                 beyond 128/256 KB max allocation limit per call
 *               - #ifdef blocked spin_lock calls to avoid compile errors
 *                 with 2.0.x
 *    11/06/1998 - another enhancement to spinlock code in lvm_chr_open()
 *                 and use of LVM_VERSION_CODE instead of my own macros
 *                 (thanks to  Michael Marxmeier <mike@msede.com>)
 *    07/07/1998 - added statistics in lvm_map()
 *    08/07/1998 - saved statistics in lvm_do_lv_extend_reduce()
 *    25/07/1998 - used __initfunc macro
 *    02/08/1998 - changes for official char/block major numbers
 *    07/08/1998 - avoided init_module() and cleanup_module() to be static
 *    30/08/1998 - changed VG lv_open counter from sum of LV lv_open counters
 *                 to sum of LVs open (no matter how often each is)
 *    01/09/1998 - fixed lvm_gendisk.part[] index error
 *    07/09/1998 - added copying of lv_current_pe-array
 *                 in LV_STATUS_BYINDEX ioctl
 *    17/11/1998 - added KERN_* levels to printk
 *    13/01/1999 - fixed LV index bug in lvm_do_lv_create() which hit lvrename
 *    07/02/1999 - fixed spinlock handling bug in case of LVM_RESET
 *                 by moving spinlock code from lvm_chr_open()
 *                 to lvm_chr_ioctl()
 *               - added LVM_LOCK_LVM ioctl to lvm_chr_ioctl()
 *               - allowed LVM_RESET and retrieval commands to go ahead;
 *                 only other update ioctls are blocked now
 *               - fixed pv->pe to NULL for pv_status
 *               - using lv_req structure in lvm_chr_ioctl() now
 *               - fixed NULL ptr reference bug in lvm_do_lv_extend_reduce()
 *                 caused by uncontiguous PV array in lvm_chr_ioctl(VG_REDUCE)
 *    09/02/1999 - changed BLKRASET and BLKRAGET in lvm_chr_ioctl() to
 *                 handle lgoical volume private read ahead sector
 *               - implemented LV read_ahead handling with lvm_blk_read()
 *                 and lvm_blk_write()
 *    10/02/1999 - implemented 2.[12].* support function lvm_hd_name()
 *                 to be used in drivers/block/genhd.c by disk_name()
 *    12/02/1999 - fixed index bug in lvm_blk_ioctl(), HDIO_GETGEO
 *               - enhanced gendisk insert/remove handling
 *    16/02/1999 - changed to dynamic block minor number allocation to
 *                 have as much as 99 volume groups with 256 logical volumes
 *                 as the grand total; this allows having 1 volume group with
 *                 up to 256 logical volumes in it
 *    21/02/1999 - added LV open count information to proc filesystem
 *               - substituted redundant LVM_RESET code by calls
 *                 to lvm_do_vg_remove()
 *    22/02/1999 - used schedule_timeout() to be more responsive
 *                 in case of lvm_do_vg_remove() with lots of logical volumes
 *    19/03/1999 - fixed NULL pointer bug in module_init/lvm_init
 *    17/05/1999 - used DECLARE_WAIT_QUEUE_HEAD macro (>2.3.0)
 *               - enhanced lvm_hd_name support
 *    03/07/1999 - avoided use of KERNEL_VERSION macro based ifdefs and
 *                 memcpy_tofs/memcpy_fromfs macro redefinitions
 *    06/07/1999 - corrected reads/writes statistic counter copy in case
 *                 of striped logical volume
 *    28/07/1999 - implemented snapshot logical volumes
 *                 - lvm_chr_ioctl
 *                   - LV_STATUS_BYINDEX
 *                   - LV_STATUS_BYNAME
 *                 - lvm_do_lv_create
 *                 - lvm_do_lv_remove
 *                 - lvm_map
 *                 - new lvm_snapshot_remap_block
 *                 - new lvm_snapshot_remap_new_block
 *    08/10/1999 - implemented support for multiple snapshots per
 *                 original logical volume
 *    12/10/1999 - support for 2.3.19
 *    11/11/1999 - support for 2.3.28
 *    21/11/1999 - changed lvm_map() interface to buffer_head based
 *    19/12/1999 - support for 2.3.33
 *    01/01/2000 - changed locking concept in lvm_map(),
 *                 lvm_do_vg_create() and lvm_do_lv_remove()
 *    15/01/2000 - fixed PV_FLUSH bug in lvm_chr_ioctl()
 *    24/01/2000 - ported to 2.3.40 including Alan Cox's pointer changes etc.
 *    29/01/2000 - used kmalloc/kfree again for all small structures
 *    20/01/2000 - cleaned up lvm_chr_ioctl by moving code
 *                 to seperated functions
 *               - avoided "/dev/" in proc filesystem output
 *               - avoided inline strings functions lvm_strlen etc.
 *    14/02/2000 - support for 2.3.43
 *               - integrated Andrea Arcagneli's snapshot code
 *    25/06/2000 - james (chip) , IKKHAYD! roffl
 *    26/06/2000 - enhanced lv_extend_reduce for snapshot logical volume
 *                 support
 *    06/09/2000 - added devfs support
 *    07/09/2000 - changed IOP version to 9
 *               - started to add new char ioctl LV_STATUS_BYDEV_T to support
 *                 getting an lv_t based on the dev_t of the Logical Volume
 *    14/09/2000 - enhanced lvm_do_lv_create to upcall VFS functions
 *                 to sync and lock, activate snapshot and unlock the FS
 *                 (to support journaled filesystems)
 *    18/09/2000 - hardsector size support
 *    27/09/2000 - implemented lvm_do_lv_rename() and lvm_do_vg_rename()
 *    30/10/2000 - added Andi Kleen's LV_BMAP ioctl to support LILO
 *    01/11/2000 - added memory information on hash tables to
 *                 lvm_proc_get_global_info()
 *    02/11/2000 - implemented /proc/lvm/ hierarchy
 *    22/11/2000 - changed lvm_do_create_proc_entry_of_pv () to work
 *                 with devfs
 *    26/11/2000 - corrected #ifdef locations for PROC_FS
 *    28/11/2000 - fixed lvm_do_vg_extend() NULL pointer BUG
 *               - fixed lvm_do_create_proc_entry_of_pv() buffer tampering BUG
 *    08/01/2001 - Removed conditional compiles related to PROC_FS,
 *                 procfs is always supported now. (JT)
 *    12/01/2001 - avoided flushing logical volume in case of shrinking
 *                 because of unecessary overhead in case of heavy updates
 *    25/01/2001 - Allow RO open of an inactive LV so it can be reactivated.
 *    31/01/2001 - removed blk_init_queue/blk_cleanup_queue queueing will be
 *                 handled by the proper devices.
 *               - If you try and BMAP a snapshot you now get an -EPERM
 *    01/01/2001 - lvm_map() now calls buffer_IO_error on error for 2.4
 *               - factored __remap_snapshot out of lvm_map
 *    12/02/2001 - move devfs code to create VG before LVs
 *    13/02/2001 - allow VG_CREATE on /dev/lvm
 *    14/02/2001 - removed modversions.h
 *               - tidied device defines for blk.h
 *               - tidied debug statements
 *               - bug: vg[] member not set back to NULL if activation fails
 *               - more lvm_map tidying
 *    15/02/2001 - register /dev/lvm with devfs correctly (major/minor
 *                 were swapped)
 *    19/02/2001 - preallocated buffer_heads for rawio when using
 *                 snapshots [JT]
 *    28/02/2001 - introduced the P_DEV macro and changed some internel
 *                 functions to be static [AD]
 *    28/02/2001 - factored lvm_get_snapshot_use_rate out of blk_ioctl [AD]
 *               - fixed user address accessing bug in lvm_do_lv_create()
 *                 where the check for an existing LV takes place right at
 *                 the beginning
 *    01/03/2001 - Add VG_CREATE_OLD for IOP 10 compatibility
 *    02/03/2001 - Don't destroy usermode pointers in lv_t structures duing
 *                 LV_STATUS_BYxxx
 *                 and remove redundant lv_t variables from same.
 *               - avoid compilation of lvm_dummy_device_request in case of
 *                 Linux >= 2.3.0 to avoid a warning
 *               - added lvm_name argument to printk in buffer allocation
 *                 in order to avoid a warning
 *    04/03/2001 - moved linux/version.h above first use of KERNEL_VERSION
 *                 macros
 *    05/03/2001 - restore copying pe_t array in lvm_do_lv_status_byname. For
 *                 lvdisplay -v (PC)
 *               - restore copying pe_t array in lvm_do_lv_status_byindex (HM)
 *               - added copying pe_t array in lvm_do_lv_status_bydev (HM)
 *               - enhanced lvm_do_lv_status_by{name,index,dev} to be capable
 *                 to copy the lv_block_exception_t array to userspace (HM)
 *    08/03/2001 - initialize new lv_ptr->lv_COW_table_iobuf for snapshots;
 *                 removed obsolete lv_ptr->lv_COW_table_page initialization
 *               - factored lvm_do_pv_flush out of lvm_chr_ioctl (HM)
 *    09/03/2001 - Added _lock_open_count to ensure we only drop the lock
 *                 when the locking process closes.
 *    05/04/2001 - Defer writes to an extent that is being moved [JT]
 *    05/04/2001 - use b_rdev and b_rsector rather than b_dev and b_blocknr in
 *                 lvm_map() in order to make stacking devices more happy (HM)
 *    11/04/2001 - cleaned up the pvmove queue code. I no longer retain the
 *                 rw flag, instead WRITEA's are just dropped [JT]
 *    30/04/2001 - added KERNEL_VERSION > 2.4.3 get_hardsect_size() rather
 *                 than get_hardblocksize() call
 *    03/05/2001 - Use copy_to/from_user to preserve pointers in
 *                 lvm_do_status_by*
 *    11/05/2001 - avoid accesses to inactive snapshot data in
 *                 __update_hardsectsize() and lvm_do_lv_extend_reduce() (JW)
 *    28/05/2001 - implemented missing BLKSSZGET ioctl
 *    05/06/2001 - Move _pe_lock out of fast path for lvm_map when no PEs
 *                 locked.  Make buffer queue flush not need locking.
 *                 Fix lvm_user_bmap() to set b_rsector for new lvm_map(). [AED]
 *    30/06/2001 - Speed up __update_hardsectsize() by checking if PVs have
 *                 the same hardsectsize (very likely) before scanning all LEs
 *                 in the LV each time.  [AED]
 *    12/10/2001 - Use add/del_gendisk() routines in 2.4.10+
 *    01/11/2001 - Backport read_ahead change from Linus kernel [AED]
 *    24/05/2002 - fixed locking bug in lvm_do_le_remap() introduced with 1.0.4
 *    13/06/2002 - use blk_ioctl() to support various standard block ioctls
 *               - support HDIO_GETGEO_BIG ioctl
 *    05/07/2002 - fixed OBO error on vg array access [benh@kernel.crashing.org]
 *    22/07/2002 - streamlined blk_ioctl() call
 *    14/08/2002 - stored fs handle in lvm_do_lv_rename
 *                 [kaoru@bsd.tnes.nec.co.jp]
 *    06/02/2003 - fix persistent snapshot extend/reduce bug in
 *		   lvm_do_lv_extend_reduce() [dalestephenson@mac.com]
 *    04/03/2003 - snapshot extend/reduce memory leak
 *               - VG PE counter wrong [dalestephenson@mac.com]
 *
 */

#include <linux/version.h>

#define MAJOR_NR LVM_BLK_MAJOR
#define DEVICE_OFF(device)
#define LOCAL_END_REQUEST

/* lvm_do_lv_create calls fsync_dev_lockfs()/unlockfs() */
/* #define	LVM_VFS_ENHANCEMENT */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <linux/slab.h>
#include <linux/init.h>

#include <linux/hdreg.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/locks.h>


#include <linux/devfs_fs_kernel.h>
#include <linux/smp_lock.h>
#include <asm/ioctl.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

#ifdef CONFIG_KERNELD
#include <linux/kerneld.h>
#endif

#include <linux/blk.h>
#include <linux/blkpg.h>

#include <linux/errno.h>
#include <linux/lvm.h>

#include "lvm-internal.h"

#define	LVM_CORRECT_READ_AHEAD(a)		\
do {						\
	if ((a) < LVM_MIN_READ_AHEAD ||		\
	    (a) > LVM_MAX_READ_AHEAD)		\
		(a) = LVM_DEFAULT_READ_AHEAD;	\
	read_ahead[MAJOR_NR] = (a);		\
} while(0)

#ifndef WRITEA
#  define WRITEA WRITE
#endif


/*
 * External function prototypes
 */
static int lvm_make_request_fn(request_queue_t *, int,
			       struct buffer_head *);

static int lvm_blk_ioctl(struct inode *, struct file *, uint, ulong);
static int lvm_blk_open(struct inode *, struct file *);

static int lvm_blk_close(struct inode *, struct file *);
static int lvm_get_snapshot_use_rate(lv_t * lv_ptr, void *arg);
static int lvm_user_bmap(struct inode *, struct lv_bmap *);

static int lvm_chr_open(struct inode *, struct file *);
static int lvm_chr_close(struct inode *, struct file *);
static int lvm_chr_ioctl(struct inode *, struct file *, uint, ulong);


/* End external function prototypes */


/*
 * Internal function prototypes
 */
static void lvm_cleanup(void);
static void lvm_init_vars(void);

#ifdef LVM_HD_NAME
extern void (*lvm_hd_name_ptr) (char *, int);
#endif
static int lvm_map(struct buffer_head *, int);
static int lvm_do_lock_lvm(void);
static int lvm_do_le_remap(vg_t *, void *);

static int lvm_do_pv_create(pv_t *, vg_t *, ulong);
static int lvm_do_pv_remove(vg_t *, ulong);
static int lvm_do_lv_create(int, char *, lv_t *);
static int lvm_do_lv_extend_reduce(int, char *, lv_t *);
static int lvm_do_lv_remove(int, char *, int);
static int lvm_do_lv_rename(vg_t *, lv_req_t *, lv_t *);
static int lvm_do_lv_status_byname(vg_t * r, void *);
static int lvm_do_lv_status_byindex(vg_t *, void *);
static int lvm_do_lv_status_bydev(vg_t *, void *);

static int lvm_do_pe_lock_unlock(vg_t * r, void *);

static int lvm_do_pv_change(vg_t *, void *);
static int lvm_do_pv_status(vg_t *, void *);
static int lvm_do_pv_flush(void *);

static int lvm_do_vg_create(void *, int minor);
static int lvm_do_vg_extend(vg_t *, void *);
static int lvm_do_vg_reduce(vg_t *, void *);
static int lvm_do_vg_rename(vg_t *, void *);
static int lvm_do_vg_remove(int);
static void lvm_geninit(struct gendisk *);
static void __update_hardsectsize(lv_t * lv);


static void _queue_io(struct buffer_head *bh, int rw);
static struct buffer_head *_dequeue_io(void);
static void _flush_io(struct buffer_head *bh);

static int _open_pv(pv_t * pv);
static void _close_pv(pv_t * pv);

static unsigned long _sectors_to_k(unsigned long sect);

#ifdef LVM_HD_NAME
void lvm_hd_name(char *, int);
#endif
/* END Internal function prototypes */


/* variables */
char *lvm_version =
    "LVM version " LVM_RELEASE_NAME "(" LVM_RELEASE_DATE ")";
ushort lvm_iop_version = LVM_DRIVER_IOP_VERSION;
int loadtime = 0;
const char *const lvm_name = LVM_NAME;


/* volume group descriptor area pointers */
vg_t *vg[ABS_MAX_VG + 1];

/* map from block minor number to VG and LV numbers */
static struct {
	int vg_number;
	int lv_number;
} vg_lv_map[ABS_MAX_LV];


/* Request structures (lvm_chr_ioctl()) */
static pv_change_req_t pv_change_req;
static pv_status_req_t pv_status_req;
volatile static pe_lock_req_t pe_lock_req;
static le_remap_req_t le_remap_req;
static lv_req_t lv_req;

#ifdef LVM_TOTAL_RESET
static int lvm_reset_spindown = 0;
#endif

static char pv_name[NAME_LEN];
/* static char rootvg[NAME_LEN] = { 0, }; */
static int lock = 0;
static int _lock_open_count = 0;
static uint vg_count = 0;
static long lvm_chr_open_count = 0;
static DECLARE_WAIT_QUEUE_HEAD(lvm_wait);

static spinlock_t lvm_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t lvm_snapshot_lock = SPIN_LOCK_UNLOCKED;

static struct buffer_head *_pe_requests;
static DECLARE_RWSEM(_pe_lock);


struct file_operations lvm_chr_fops = {
	owner:THIS_MODULE,
	open:lvm_chr_open,
	release:lvm_chr_close,
	ioctl:lvm_chr_ioctl,
};

/* block device operations structure needed for 2.3.38? and above */
struct block_device_operations lvm_blk_dops = {
	.owner		= THIS_MODULE,
	.open		= lvm_blk_open,
	.release	= lvm_blk_close,
	.ioctl		= lvm_blk_ioctl,
};


/* gendisk structures */
static struct hd_struct lvm_hd_struct[MAX_LV];
static int lvm_blocksizes[MAX_LV];
static int lvm_hardsectsizes[MAX_LV];
static int lvm_size[MAX_LV];

static struct gendisk lvm_gendisk = {
	.major		= MAJOR_NR,
	.major_name	= LVM_NAME,
	.minor_shift	= 0,
	.max_p		= 1,
	.part		= lvm_hd_struct,
	.sizes		= lvm_size,
	.nr_real	= MAX_LV,
};


/*
 * Driver initialization...
 */
int lvm_init(void)
{
	if (devfs_register_chrdev(LVM_CHAR_MAJOR,
				  lvm_name, &lvm_chr_fops) < 0) {
		printk(KERN_ERR "%s -- devfs_register_chrdev failed\n",
		       lvm_name);
		return -EIO;
	}
	if (devfs_register_blkdev(MAJOR_NR, lvm_name, &lvm_blk_dops) < 0)
	{
		printk("%s -- devfs_register_blkdev failed\n", lvm_name);
		if (devfs_unregister_chrdev(LVM_CHAR_MAJOR, lvm_name) < 0)
			printk(KERN_ERR
			       "%s -- devfs_unregister_chrdev failed\n",
			       lvm_name);
		return -EIO;
	}

	lvm_init_fs();
	lvm_init_vars();
	lvm_geninit(&lvm_gendisk);

	/* insert our gendisk at the corresponding major */
	add_gendisk(&lvm_gendisk);

#ifdef LVM_HD_NAME
	/* reference from drivers/block/genhd.c */
	lvm_hd_name_ptr = lvm_hd_name;
#endif

	blk_queue_make_request(BLK_DEFAULT_QUEUE(MAJOR_NR),
			       lvm_make_request_fn);


	/* initialise the pe lock */
	pe_lock_req.lock = UNLOCK_PE;

	/* optional read root VGDA */
/*
   if ( *rootvg != 0) vg_read_with_pv_and_lv ( rootvg, &vg);
*/

#ifdef MODULE
	printk(KERN_INFO "%s module loaded\n", lvm_version);
#else
	printk(KERN_INFO "%s\n", lvm_version);
#endif

	return 0;
}				/* lvm_init() */

/*
 * cleanup...
 */

static void lvm_cleanup(void)
{
	if (devfs_unregister_chrdev(LVM_CHAR_MAJOR, lvm_name) < 0)
		printk(KERN_ERR "%s -- devfs_unregister_chrdev failed\n",
		       lvm_name);
	if (devfs_unregister_blkdev(MAJOR_NR, lvm_name) < 0)
		printk(KERN_ERR "%s -- devfs_unregister_blkdev failed\n",
		       lvm_name);



	/* delete our gendisk from chain */
	del_gendisk(&lvm_gendisk);

	blk_size[MAJOR_NR] = NULL;
	blksize_size[MAJOR_NR] = NULL;
	hardsect_size[MAJOR_NR] = NULL;

#ifdef LVM_HD_NAME
	/* reference from linux/drivers/block/genhd.c */
	lvm_hd_name_ptr = NULL;
#endif

	/* unregister with procfs and devfs */
	lvm_fin_fs();

#ifdef MODULE
	printk(KERN_INFO "%s -- Module successfully deactivated\n",
	       lvm_name);
#endif

	return;
}				/* lvm_cleanup() */

/*
 * support function to initialize lvm variables
 */
static void __init lvm_init_vars(void)
{
	int v;

	loadtime = CURRENT_TIME;

	lvm_lock = lvm_snapshot_lock = SPIN_LOCK_UNLOCKED;

	pe_lock_req.lock = UNLOCK_PE;
	pe_lock_req.data.lv_dev = 0;
	pe_lock_req.data.pv_dev = 0;
	pe_lock_req.data.pv_offset = 0;

	/* Initialize VG pointers */
	for (v = 0; v < ABS_MAX_VG + 1; v++)
		vg[v] = NULL;

	/* Initialize LV -> VG association */
	for (v = 0; v < ABS_MAX_LV; v++) {
		/* index ABS_MAX_VG never used for real VG */
		vg_lv_map[v].vg_number = ABS_MAX_VG;
		vg_lv_map[v].lv_number = -1;
	}

	return;
}				/* lvm_init_vars() */


/********************************************************************
 *
 * Character device functions
 *
 ********************************************************************/

#define MODE_TO_STR(mode) (mode) & FMODE_READ ? "READ" : "", \
			  (mode) & FMODE_WRITE ? "WRITE" : ""

/*
 * character device open routine
 */
static int lvm_chr_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);

	P_DEV("chr_open MINOR: %d  VG#: %d  mode: %s%s  lock: %d\n",
	      minor, VG_CHR(minor), MODE_TO_STR(file->f_mode), lock);

	/* super user validation */
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	/* Group special file open */
	if (VG_CHR(minor) > MAX_VG)
		return -ENXIO;

	spin_lock(&lvm_lock);
	if (lock == current->pid)
		_lock_open_count++;
	spin_unlock(&lvm_lock);

	lvm_chr_open_count++;

	MOD_INC_USE_COUNT;

	return 0;
}				/* lvm_chr_open() */


/*
 * character device i/o-control routine
 *
 * Only one changing process can do changing ioctl at one time,
 * others will block.
 *
 */
static int lvm_chr_ioctl(struct inode *inode, struct file *file,
			 uint command, ulong a)
{
	int minor = MINOR(inode->i_rdev);
	uint extendable, l, v;
	void *arg = (void *) a;
	lv_t lv;
	vg_t *vg_ptr = vg[VG_CHR(minor)];

	/* otherwise cc will complain about unused variables */
	(void) lvm_lock;

	P_IOCTL
	    ("chr MINOR: %d  command: 0x%X  arg: %p  VG#: %d  mode: %s%s\n",
	     minor, command, arg, VG_CHR(minor),
	     MODE_TO_STR(file->f_mode));

#ifdef LVM_TOTAL_RESET
	if (lvm_reset_spindown > 0)
		return -EACCES;
#endif

	/* Main command switch */
	switch (command) {
	case LVM_LOCK_LVM:
		/* lock the LVM */
		return lvm_do_lock_lvm();

	case LVM_GET_IOP_VERSION:
		/* check lvm version to ensure driver/tools+lib
		   interoperability */
		if (copy_to_user(arg, &lvm_iop_version, sizeof(ushort)) !=
		    0)
			return -EFAULT;
		return 0;

#ifdef LVM_TOTAL_RESET
	case LVM_RESET:
		/* lock reset function */
		lvm_reset_spindown = 1;
		for (v = 0; v < ABS_MAX_VG; v++) {
			if (vg[v] != NULL)
				lvm_do_vg_remove(v);
		}

#ifdef MODULE
		while (GET_USE_COUNT(&__this_module) < 1)
			MOD_INC_USE_COUNT;
		while (GET_USE_COUNT(&__this_module) > 1)
			MOD_DEC_USE_COUNT;
#endif				/* MODULE */
		lock = 0;	/* release lock */
		wake_up_interruptible(&lvm_wait);
		return 0;
#endif				/* LVM_TOTAL_RESET */


	case LE_REMAP:
		/* remap a logical extent (after moving the physical extent) */
		return lvm_do_le_remap(vg_ptr, arg);

	case PE_LOCK_UNLOCK:
		/* lock/unlock i/o to a physical extent to move it to another
		   physical volume (move's done in user space's pvmove) */
		return lvm_do_pe_lock_unlock(vg_ptr, arg);

	case VG_CREATE_OLD:
		/* create a VGDA */
		return lvm_do_vg_create(arg, minor);

	case VG_CREATE:
		/* create a VGDA, assume VG number is filled in */
		return lvm_do_vg_create(arg, -1);

	case VG_EXTEND:
		/* extend a volume group */
		return lvm_do_vg_extend(vg_ptr, arg);

	case VG_REDUCE:
		/* reduce a volume group */
		return lvm_do_vg_reduce(vg_ptr, arg);

	case VG_RENAME:
		/* rename a volume group */
		return lvm_do_vg_rename(vg_ptr, arg);

	case VG_REMOVE:
		/* remove an inactive VGDA */
		return lvm_do_vg_remove(minor);


	case VG_SET_EXTENDABLE:
		/* set/clear extendability flag of volume group */
		if (vg_ptr == NULL)
			return -ENXIO;
		if (copy_from_user(&extendable, arg, sizeof(extendable)) !=
		    0)
			return -EFAULT;

		if (extendable == VG_EXTENDABLE ||
		    extendable == ~VG_EXTENDABLE) {
			if (extendable == VG_EXTENDABLE)
				vg_ptr->vg_status |= VG_EXTENDABLE;
			else
				vg_ptr->vg_status &= ~VG_EXTENDABLE;
		} else
			return -EINVAL;
		return 0;


	case VG_STATUS:
		/* get volume group data (only the vg_t struct) */
		if (vg_ptr == NULL)
			return -ENXIO;
		if (copy_to_user(arg, vg_ptr, sizeof(vg_t)) != 0)
			return -EFAULT;
		return 0;


	case VG_STATUS_GET_COUNT:
		/* get volume group count */
		if (copy_to_user(arg, &vg_count, sizeof(vg_count)) != 0)
			return -EFAULT;
		return 0;


	case VG_STATUS_GET_NAMELIST:
		/* get volume group names */
		for (l = v = 0; v < ABS_MAX_VG; v++) {
			if (vg[v] != NULL) {
				if (copy_to_user(arg + l * NAME_LEN,
						 vg[v]->vg_name,
						 NAME_LEN) != 0)
					return -EFAULT;
				l++;
			}
		}
		return 0;


	case LV_CREATE:
	case LV_EXTEND:
	case LV_REDUCE:
	case LV_REMOVE:
	case LV_RENAME:
		/* create, extend, reduce, remove or rename a logical volume */
		if (vg_ptr == NULL)
			return -ENXIO;
		if (copy_from_user(&lv_req, arg, sizeof(lv_req)) != 0)
			return -EFAULT;

		if (command != LV_REMOVE) {
			if (copy_from_user(&lv, lv_req.lv, sizeof(lv_t)) !=
			    0)
				return -EFAULT;
		}
		switch (command) {
		case LV_CREATE:
			return lvm_do_lv_create(minor, lv_req.lv_name,
						&lv);

		case LV_EXTEND:
		case LV_REDUCE:
			return lvm_do_lv_extend_reduce(minor,
						       lv_req.lv_name,
						       &lv);
		case LV_REMOVE:
			return lvm_do_lv_remove(minor, lv_req.lv_name, -1);

		case LV_RENAME:
			return lvm_do_lv_rename(vg_ptr, &lv_req, &lv);
		}




	case LV_STATUS_BYNAME:
		/* get status of a logical volume by name */
		return lvm_do_lv_status_byname(vg_ptr, arg);


	case LV_STATUS_BYINDEX:
		/* get status of a logical volume by index */
		return lvm_do_lv_status_byindex(vg_ptr, arg);


	case LV_STATUS_BYDEV:
		/* get status of a logical volume by device */
		return lvm_do_lv_status_bydev(vg_ptr, arg);


	case PV_CHANGE:
		/* change a physical volume */
		return lvm_do_pv_change(vg_ptr, arg);


	case PV_STATUS:
		/* get physical volume data (pv_t structure only) */
		return lvm_do_pv_status(vg_ptr, arg);


	case PV_FLUSH:
		/* physical volume buffer flush/invalidate */
		return lvm_do_pv_flush(arg);


	default:
		printk(KERN_WARNING
		       "%s -- lvm_chr_ioctl: unknown command 0x%x\n",
		       lvm_name, command);
		return -ENOTTY;
	}

	return 0;
}				/* lvm_chr_ioctl */


/*
 * character device close routine
 */
static int lvm_chr_close(struct inode *inode, struct file *file)
{
	P_DEV("chr_close MINOR: %d  VG#: %d\n",
	      MINOR(inode->i_rdev), VG_CHR(MINOR(inode->i_rdev)));

#ifdef LVM_TOTAL_RESET
	if (lvm_reset_spindown > 0) {
		lvm_reset_spindown = 0;
		lvm_chr_open_count = 0;
	}
#endif

	if (lvm_chr_open_count > 0)
		lvm_chr_open_count--;

	spin_lock(&lvm_lock);
	if (lock == current->pid) {
		if (!_lock_open_count) {
			P_DEV("chr_close: unlocking LVM for pid %d\n",
			      lock);
			lock = 0;
			wake_up_interruptible(&lvm_wait);
		} else
			_lock_open_count--;
	}
	spin_unlock(&lvm_lock);

	MOD_DEC_USE_COUNT;

	return 0;
}				/* lvm_chr_close() */



/********************************************************************
 *
 * Block device functions
 *
 ********************************************************************/

/*
 * block device open routine
 */
static int lvm_blk_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	lv_t *lv_ptr;
	vg_t *vg_ptr = vg[VG_BLK(minor)];

	P_DEV("blk_open MINOR: %d  VG#: %d  LV#: %d  mode: %s%s\n",
	      minor, VG_BLK(minor), LV_BLK(minor),
	      MODE_TO_STR(file->f_mode));

#ifdef LVM_TOTAL_RESET
	if (lvm_reset_spindown > 0)
		return -EPERM;
#endif

	if (vg_ptr != NULL &&
	    (vg_ptr->vg_status & VG_ACTIVE) &&
	    (lv_ptr = vg_ptr->lv[LV_BLK(minor)]) != NULL &&
	    LV_BLK(minor) >= 0 && LV_BLK(minor) < vg_ptr->lv_max) {

		/* Check parallel LV spindown (LV remove) */
		if (lv_ptr->lv_status & LV_SPINDOWN)
			return -EPERM;

		/* Check inactive LV and open for read/write */
		/* We need to be able to "read" an inactive LV
		   to re-activate it again */
		if ((file->f_mode & FMODE_WRITE) &&
		    (!(lv_ptr->lv_status & LV_ACTIVE)))
			return -EPERM;

		if (!(lv_ptr->lv_access & LV_WRITE) &&
		    (file->f_mode & FMODE_WRITE))
			return -EACCES;


		/* be sure to increment VG counter */
		if (lv_ptr->lv_open == 0)
			vg_ptr->lv_open++;
		lv_ptr->lv_open++;

		MOD_INC_USE_COUNT;

		P_DEV("blk_open OK, LV size %d\n", lv_ptr->lv_size);

		return 0;
	}
	return -ENXIO;
}				/* lvm_blk_open() */

/* Deliver "hard disk geometry" */
static int _hdio_getgeo(ulong a, lv_t * lv_ptr, int what)
{
	int ret = 0;
	uchar heads = 128;
	uchar sectors = 128;
	ulong start = 0;
	uint cylinders;

	while (heads * sectors > lv_ptr->lv_size) {
		heads >>= 1;
		sectors >>= 1;
	}
	cylinders = lv_ptr->lv_size / heads / sectors;

	switch (what) {
	case 0:
		{
			struct hd_geometry *hd = (struct hd_geometry *) a;

			if (put_user(heads, &hd->heads) ||
			    put_user(sectors, &hd->sectors) ||
			    put_user((ushort) cylinders, &hd->cylinders) ||
			    put_user(start, &hd->start))
				return -EFAULT;
			break;
		}

#ifdef HDIO_GETGEO_BIG
	case 1:
		{
			struct hd_big_geometry *hd =
			    (struct hd_big_geometry *) a;

			if (put_user(heads, &hd->heads) ||
			    put_user(sectors, &hd->sectors) ||
			    put_user(cylinders, &hd->cylinders) ||
			    put_user(start, &hd->start))
				return -EFAULT;
			break;
		}
#endif

	}

	P_IOCTL("%s -- lvm_blk_ioctl -- cylinders: %d\n",
		lvm_name, cylinders);
	return ret;
}


/*
 * block device i/o-control routine
 */
static int lvm_blk_ioctl(struct inode *inode, struct file *file,
			 uint cmd, ulong a)
{
	kdev_t dev = inode->i_rdev;
	int minor = MINOR(dev), ret;
	vg_t *vg_ptr = vg[VG_BLK(minor)];
	lv_t *lv_ptr = vg_ptr->lv[LV_BLK(minor)];
	void *arg = (void *) a;

	P_IOCTL("blk MINOR: %d  cmd: 0x%X  arg: %p  VG#: %d  LV#: %d  "
		"mode: %s%s\n", minor, cmd, arg, VG_BLK(minor),
		LV_BLK(minor), MODE_TO_STR(file->f_mode));

	switch (cmd) {
	case BLKRASET:
		/* set read ahead for block device */
		ret = blk_ioctl(dev, cmd, a);
		if (ret)
			return ret;
		lv_ptr->lv_read_ahead = (long) a;
		LVM_CORRECT_READ_AHEAD(lv_ptr->lv_read_ahead);
		break;

	case HDIO_GETGEO:
#ifdef HDIO_GETGEO_BIG
	case HDIO_GETGEO_BIG:
#endif
		/* get disk geometry */
		P_IOCTL("%s -- lvm_blk_ioctl -- HDIO_GETGEO\n", lvm_name);
		if (!a)
			return -EINVAL;

		switch (cmd) {
		case HDIO_GETGEO:
			return _hdio_getgeo(a, lv_ptr, 0);
#ifdef HDIO_GETGEO_BIG
		case HDIO_GETGEO_BIG:
			return _hdio_getgeo(a, lv_ptr, 1);
#endif
		}

	case LV_BMAP:
		/* turn logical block into (dev_t, block). non privileged. */
		/* don't bmap a snapshot, since the mapping can change */
		if (lv_ptr->lv_access & LV_SNAPSHOT)
			return -EPERM;

		return lvm_user_bmap(inode, (struct lv_bmap *) arg);

	case LV_SET_ACCESS:
		/* set access flags of a logical volume */
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;

		down_write(&lv_ptr->lv_lock);
		lv_ptr->lv_access = (ulong) arg;
		up_write(&lv_ptr->lv_lock);

		if (lv_ptr->lv_access & LV_WRITE)
			set_device_ro(lv_ptr->lv_dev, 0);
		else
			set_device_ro(lv_ptr->lv_dev, 1);
		break;


	case LV_SET_ALLOCATION:
		/* set allocation flags of a logical volume */
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;
		down_write(&lv_ptr->lv_lock);
		lv_ptr->lv_allocation = (ulong) arg;
		up_write(&lv_ptr->lv_lock);
		break;

	case LV_SET_STATUS:
		/* set status flags of a logical volume */
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;
		if (!((ulong) arg & LV_ACTIVE) && lv_ptr->lv_open > 1)
			return -EPERM;
		down_write(&lv_ptr->lv_lock);
		lv_ptr->lv_status = (ulong) arg;
		up_write(&lv_ptr->lv_lock);
		break;

	case LV_SNAPSHOT_USE_RATE:
		return lvm_get_snapshot_use_rate(lv_ptr, arg);

	default:
		/* Handle rest here */
		ret = blk_ioctl(dev, cmd, a);
		if (ret)
			printk(KERN_WARNING
			       "%s -- lvm_blk_ioctl: unknown "
			       "cmd 0x%x\n", lvm_name, cmd);
		return ret;
	}

	return 0;
}				/* lvm_blk_ioctl() */


/*
 * block device close routine
 */
static int lvm_blk_close(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	vg_t *vg_ptr = vg[VG_BLK(minor)];
	lv_t *lv_ptr = vg_ptr->lv[LV_BLK(minor)];

	P_DEV("blk_close MINOR: %d  VG#: %d  LV#: %d\n",
	      minor, VG_BLK(minor), LV_BLK(minor));

	if (lv_ptr->lv_open == 1)
		vg_ptr->lv_open--;
	lv_ptr->lv_open--;

	MOD_DEC_USE_COUNT;

	return 0;
}				/* lvm_blk_close() */

static int lvm_get_snapshot_use_rate(lv_t * lv, void *arg)
{
	lv_snapshot_use_rate_req_t lv_rate_req;

	down_read(&lv->lv_lock);
	if (!(lv->lv_access & LV_SNAPSHOT)) {
		up_read(&lv->lv_lock);
		return -EPERM;
	}
	up_read(&lv->lv_lock);

	if (copy_from_user(&lv_rate_req, arg, sizeof(lv_rate_req)))
		return -EFAULT;

	if (lv_rate_req.rate < 0 || lv_rate_req.rate > 100)
		return -EINVAL;

	switch (lv_rate_req.block) {
	case 0:
		down_write(&lv->lv_lock);
		lv->lv_snapshot_use_rate = lv_rate_req.rate;
		up_write(&lv->lv_lock);
		down_read(&lv->lv_lock);
		if (lv->lv_remap_ptr * 100 / lv->lv_remap_end <
		    lv->lv_snapshot_use_rate) {
			up_read(&lv->lv_lock);
			interruptible_sleep_on(&lv->lv_snapshot_wait);
			down_read(&lv->lv_lock);
		}
		up_read(&lv->lv_lock);
		break;

	case O_NONBLOCK:
		break;

	default:
		return -EINVAL;
	}
	down_read(&lv->lv_lock);
	lv_rate_req.rate = lv->lv_remap_ptr * 100 / lv->lv_remap_end;
	up_read(&lv->lv_lock);

	return copy_to_user(arg, &lv_rate_req,
			    sizeof(lv_rate_req)) ? -EFAULT : 0;
}

static int lvm_user_bmap(struct inode *inode, struct lv_bmap *user_result)
{
	struct buffer_head bh;
	unsigned long block;
	int err;

	if (get_user(block, &user_result->lv_block))
		return -EFAULT;

	memset(&bh, 0, sizeof bh);
	bh.b_blocknr = block;
	bh.b_dev = bh.b_rdev = inode->i_rdev;
	bh.b_size = lvm_get_blksize(bh.b_dev);
	bh.b_rsector = block * (bh.b_size >> 9);
	bh.b_end_io = NULL;
	if ((err = lvm_map(&bh, READ)) < 0) {
		printk("lvm map failed: %d\n", err);
		return -EINVAL;
	}

	return put_user(kdev_t_to_nr(bh.b_rdev), &user_result->lv_dev) ||
	    put_user(bh.b_rsector / (bh.b_size >> 9),
		     &user_result->lv_block) ? -EFAULT : 0;
}


/*
 * block device support function for /usr/src/linux/drivers/block/ll_rw_blk.c
 * (see init_module/lvm_init)
 */
static void __remap_snapshot(kdev_t rdev, ulong rsector,
			     ulong pe_start, lv_t * lv, vg_t * vg)
{

	/* copy a chunk from the origin to a snapshot device */
	down_write(&lv->lv_lock);

	/* we must redo lvm_snapshot_remap_block in order to avoid a
	   race condition in the gap where no lock was held */
	if (!lvm_snapshot_remap_block(&rdev, &rsector, pe_start, lv) &&
	    !lvm_snapshot_COW(rdev, rsector, pe_start, rsector, vg, lv))
		lvm_write_COW_table_block(vg, lv);

	up_write(&lv->lv_lock);
}

static inline void _remap_snapshot(kdev_t rdev, ulong rsector,
				   ulong pe_start, lv_t * lv, vg_t * vg)
{
	int r;

	/* check to see if this chunk is already in the snapshot */
	down_read(&lv->lv_lock);
	r = lvm_snapshot_remap_block(&rdev, &rsector, pe_start, lv);
	up_read(&lv->lv_lock);

	if (!r)
		/* we haven't yet copied this block to the snapshot */
		__remap_snapshot(rdev, rsector, pe_start, lv, vg);
}


/*
 * extents destined for a pe that is on the move should be deferred
 */
static inline int _should_defer(kdev_t pv, ulong sector, uint32_t pe_size)
{
	return ((pe_lock_req.lock == LOCK_PE) &&
		(pv == pe_lock_req.data.pv_dev) &&
		(sector >= pe_lock_req.data.pv_offset) &&
		(sector < (pe_lock_req.data.pv_offset + pe_size)));
}

static inline int _defer_extent(struct buffer_head *bh, int rw,
				kdev_t pv, ulong sector, uint32_t pe_size)
{
	if (pe_lock_req.lock == LOCK_PE) {
		down_read(&_pe_lock);
		if (_should_defer(pv, sector, pe_size)) {
			up_read(&_pe_lock);
			down_write(&_pe_lock);
			if (_should_defer(pv, sector, pe_size))
				_queue_io(bh, rw);
			up_write(&_pe_lock);
			return 1;
		}
		up_read(&_pe_lock);
	}
	return 0;
}


static int lvm_map(struct buffer_head *bh, int rw)
{
	int minor = MINOR(bh->b_rdev);
	ulong index;
	ulong pe_start;
	ulong size = bh->b_size >> 9;
	ulong rsector_org = bh->b_rsector;
	ulong rsector_map;
	kdev_t rdev_map;
	vg_t *vg_this = vg[VG_BLK(minor)];
	lv_t *lv = vg_this->lv[LV_BLK(minor)];


	down_read(&lv->lv_lock);
	if (!(lv->lv_status & LV_ACTIVE)) {
		printk(KERN_ALERT
		       "%s - lvm_map: ll_rw_blk for inactive LV %s\n",
		       lvm_name, lv->lv_name);
		goto bad;
	}

	if ((rw == WRITE || rw == WRITEA) && !(lv->lv_access & LV_WRITE)) {
		printk(KERN_CRIT
		       "%s - lvm_map: ll_rw_blk write for readonly LV %s\n",
		       lvm_name, lv->lv_name);
		goto bad;
	}

	P_MAP
	    ("%s - lvm_map minor: %d  *rdev: %s  *rsector: %lu  size:%lu\n",
	     lvm_name, minor, kdevname(bh->b_rdev), rsector_org, size);

	if (rsector_org + size > lv->lv_size) {
		printk(KERN_ALERT
		       "%s - lvm_map access beyond end of device; *rsector: "
		       "%lu or size: %lu wrong for minor: %2d\n",
		       lvm_name, rsector_org, size, minor);
		goto bad;
	}


	if (lv->lv_stripes < 2) {	/* linear mapping */
		/* get the index */
		index = rsector_org / vg_this->pe_size;
		pe_start = lv->lv_current_pe[index].pe;
		rsector_map = lv->lv_current_pe[index].pe +
		    (rsector_org % vg_this->pe_size);
		rdev_map = lv->lv_current_pe[index].dev;

		P_MAP("lv_current_pe[%ld].pe: %d  rdev: %s  rsector:%ld\n",
		      index, lv->lv_current_pe[index].pe,
		      kdevname(rdev_map), rsector_map);

	} else {		/* striped mapping */
		ulong stripe_index;
		ulong stripe_length;

		stripe_length = vg_this->pe_size * lv->lv_stripes;
		stripe_index = (rsector_org % stripe_length) /
		    lv->lv_stripesize;
		index = rsector_org / stripe_length +
		    (stripe_index % lv->lv_stripes) *
		    (lv->lv_allocated_le / lv->lv_stripes);
		pe_start = lv->lv_current_pe[index].pe;
		rsector_map = lv->lv_current_pe[index].pe +
		    (rsector_org % stripe_length) -
		    (stripe_index % lv->lv_stripes) * lv->lv_stripesize -
		    stripe_index / lv->lv_stripes *
		    (lv->lv_stripes - 1) * lv->lv_stripesize;
		rdev_map = lv->lv_current_pe[index].dev;

		P_MAP("lv_current_pe[%ld].pe: %d  rdev: %s  rsector:%ld\n"
		      "stripe_length: %ld  stripe_index: %ld\n",
		      index, lv->lv_current_pe[index].pe,
		      kdevname(rdev_map), rsector_map, stripe_length,
		      stripe_index);
	}

	/*
	 * Queue writes to physical extents on the move until move completes.
	 * Don't get _pe_lock until there is a reasonable expectation that
	 * we need to queue this request, because this is in the fast path.
	 */
	if (rw == WRITE || rw == WRITEA) {
		if (_defer_extent(bh, rw, rdev_map,
				  rsector_map, vg_this->pe_size)) {

			up_read(&lv->lv_lock);
			return 0;
		}

		lv->lv_current_pe[index].writes++;	/* statistic */
	} else
		lv->lv_current_pe[index].reads++;	/* statistic */

	/* snapshot volume exception handling on physical device address base */
	if (!(lv->lv_access & (LV_SNAPSHOT | LV_SNAPSHOT_ORG)))
		goto out;

	if (lv->lv_access & LV_SNAPSHOT) {	/* remap snapshot */
		if (lvm_snapshot_remap_block(&rdev_map, &rsector_map,
					     pe_start, lv) < 0)
			goto bad;

	} else if (rw == WRITE || rw == WRITEA) {	/* snapshot origin */
		lv_t *snap;

		/* start with first snapshot and loop through all of
		   them */
		for (snap = lv->lv_snapshot_next; snap;
		     snap = snap->lv_snapshot_next) {
			/* Check for inactive snapshot */
			if (!(snap->lv_status & LV_ACTIVE))
				continue;

			/* Serializes the COW with the accesses to the
			   snapshot device */
			_remap_snapshot(rdev_map, rsector_map,
					pe_start, snap, vg_this);
		}
	}

      out:
	bh->b_rdev = rdev_map;
	bh->b_rsector = rsector_map;
	up_read(&lv->lv_lock);
	return 1;

      bad:
	if (bh->b_end_io)
		buffer_IO_error(bh);
	up_read(&lv->lv_lock);
	return -1;
}				/* lvm_map() */


/*
 * internal support functions
 */

#ifdef LVM_HD_NAME
/*
 * generate "hard disk" name
 */
void lvm_hd_name(char *buf, int minor)
{
	int len = 0;
	lv_t *lv_ptr;

	if (vg[VG_BLK(minor)] == NULL ||
	    (lv_ptr = vg[VG_BLK(minor)]->lv[LV_BLK(minor)]) == NULL)
		return;
	len = strlen(lv_ptr->lv_name) - 5;
	memcpy(buf, &lv_ptr->lv_name[5], len);
	buf[len] = 0;
	return;
}
#endif




/*
 * make request function
 */
static int lvm_make_request_fn(request_queue_t * q,
			       int rw, struct buffer_head *bh)
{
	return (lvm_map(bh, rw) <= 0) ? 0 : 1;
}


/********************************************************************
 *
 * Character device support functions
 *
 ********************************************************************/
/*
 * character device support function logical volume manager lock
 */
static int lvm_do_lock_lvm(void)
{
      lock_try_again:
	spin_lock(&lvm_lock);
	if (lock != 0 && lock != current->pid) {
		P_DEV("lvm_do_lock_lvm: locked by pid %d ...\n", lock);
		spin_unlock(&lvm_lock);
		interruptible_sleep_on(&lvm_wait);
		if (current->sigpending != 0)
			return -EINTR;
#ifdef LVM_TOTAL_RESET
		if (lvm_reset_spindown > 0)
			return -EACCES;
#endif
		goto lock_try_again;
	}
	lock = current->pid;
	P_DEV("lvm_do_lock_lvm: locking LVM for pid %d\n", lock);
	spin_unlock(&lvm_lock);
	return 0;
}				/* lvm_do_lock_lvm */


/*
 * character device support function lock/unlock physical extend
 */
static int lvm_do_pe_lock_unlock(vg_t * vg_ptr, void *arg)
{
	pe_lock_req_t new_lock;
	struct buffer_head *bh;
	uint p;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&new_lock, arg, sizeof(new_lock)) != 0)
		return -EFAULT;

	switch (new_lock.lock) {
	case LOCK_PE:
		for (p = 0; p < vg_ptr->pv_max; p++) {
			if (vg_ptr->pv[p] != NULL &&
			    new_lock.data.pv_dev == vg_ptr->pv[p]->pv_dev)
				break;
		}
		if (p == vg_ptr->pv_max)
			return -ENXIO;

		/*
		 * this sync releaves memory pressure to lessen the
		 * likelyhood of pvmove being paged out - resulting in
		 * deadlock.
		 *
		 * This method of doing a pvmove is broken
		 */
		fsync_dev(pe_lock_req.data.lv_dev);

		down_write(&_pe_lock);
		if (pe_lock_req.lock == LOCK_PE) {
			up_write(&_pe_lock);
			return -EBUSY;
		}

		/* Should we do to_kdev_t() on the pv_dev and lv_dev??? */
		pe_lock_req.lock = LOCK_PE;
		pe_lock_req.data.lv_dev = new_lock.data.lv_dev;
		pe_lock_req.data.pv_dev = new_lock.data.pv_dev;
		pe_lock_req.data.pv_offset = new_lock.data.pv_offset;
		up_write(&_pe_lock);

		/* some requests may have got through since the fsync */
		fsync_dev(pe_lock_req.data.pv_dev);
		break;

	case UNLOCK_PE:
		down_write(&_pe_lock);
		pe_lock_req.lock = UNLOCK_PE;
		pe_lock_req.data.lv_dev = 0;
		pe_lock_req.data.pv_dev = 0;
		pe_lock_req.data.pv_offset = 0;
		bh = _dequeue_io();
		up_write(&_pe_lock);

		/* handle all deferred io for this PE */
		_flush_io(bh);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}


/*
 * character device support function logical extend remap
 */
static int lvm_do_le_remap(vg_t * vg_ptr, void *arg)
{
	uint l, le;
	lv_t *lv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&le_remap_req, arg,
			   sizeof(le_remap_req_t)) != 0)
		return -EFAULT;

	for (l = 0; l < vg_ptr->lv_max; l++) {
		lv_ptr = vg_ptr->lv[l];

		if (!lv_ptr)
			continue;

		if (strcmp(lv_ptr->lv_name, le_remap_req.lv_name) == 0) {
			down_write(&lv_ptr->lv_lock);
			for (le = 0; le < lv_ptr->lv_allocated_le; le++) {
				if (lv_ptr->lv_current_pe[le].dev ==
				    le_remap_req.old_dev &&
				    lv_ptr->lv_current_pe[le].pe ==
				    le_remap_req.old_pe) {
					lv_ptr->lv_current_pe[le].dev =
					    le_remap_req.new_dev;
					lv_ptr->lv_current_pe[le].pe =
					    le_remap_req.new_pe;
					__update_hardsectsize(lv_ptr);
					up_write(&lv_ptr->lv_lock);
					return 0;
				}
			}
			up_write(&lv_ptr->lv_lock);
			return -EINVAL;
		}
	}
	return -ENXIO;
}				/* lvm_do_le_remap() */


/*
 * character device support function VGDA create
 */
static int lvm_do_vg_create(void *arg, int minor)
{
	int ret = 0;
	ulong l, ls = 0, p, size;
	lv_t lv;
	vg_t *vg_ptr;
	lv_t **snap_lv_ptr;

	if ((vg_ptr = kmalloc(sizeof(vg_t), GFP_KERNEL)) == NULL) {
		printk(KERN_CRIT
		       "%s -- VG_CREATE: kmalloc error VG at line %d\n",
		       lvm_name, __LINE__);
		return -ENOMEM;
	}
	/* get the volume group structure */
	if (copy_from_user(vg_ptr, arg, sizeof(vg_t)) != 0) {
		P_IOCTL
		    ("lvm_do_vg_create ERROR: copy VG ptr %p (%d bytes)\n",
		     arg, sizeof(vg_t));
		kfree(vg_ptr);
		return -EFAULT;
	}

	/* VG_CREATE now uses minor number in VG structure */
	if (minor == -1)
		minor = vg_ptr->vg_number;

	/* check limits */
	if (minor >= ABS_MAX_VG) {
		kfree(vg_ptr);
		return -EFAULT;
	}

	/* Validate it */
	if (vg[VG_CHR(minor)] != NULL) {
		P_IOCTL("lvm_do_vg_create ERROR: VG %d in use\n", minor);
		kfree(vg_ptr);
		return -EPERM;
	}

	/* we are not that active so far... */
	vg_ptr->vg_status &= ~VG_ACTIVE;
	vg_ptr->pe_allocated = 0;

	if (vg_ptr->pv_max > ABS_MAX_PV) {
		printk(KERN_WARNING
		       "%s -- Can't activate VG: ABS_MAX_PV too small\n",
		       lvm_name);
		kfree(vg_ptr);
		return -EPERM;
	}

	if (vg_ptr->lv_max > ABS_MAX_LV) {
		printk(KERN_WARNING
		       "%s -- Can't activate VG: ABS_MAX_LV too small for %u\n",
		       lvm_name, vg_ptr->lv_max);
		kfree(vg_ptr);
		return -EPERM;
	}

	/* create devfs and procfs entries */
	lvm_fs_create_vg(vg_ptr);

	vg[VG_CHR(minor)] = vg_ptr;

	/* get the physical volume structures */
	vg_ptr->pv_act = vg_ptr->pv_cur = 0;
	for (p = 0; p < vg_ptr->pv_max; p++) {
		pv_t *pvp;
		/* user space address */
		if ((pvp = vg_ptr->pv[p]) != NULL) {
			ret = lvm_do_pv_create(pvp, vg_ptr, p);
			if (ret != 0) {
				lvm_do_vg_remove(minor);
				return ret;
			}
		}
	}

	size = vg_ptr->lv_max * sizeof(lv_t *);
	if ((snap_lv_ptr = vmalloc(size)) == NULL) {
		printk(KERN_CRIT
		       "%s -- VG_CREATE: vmalloc error snapshot LVs at line %d\n",
		       lvm_name, __LINE__);
		lvm_do_vg_remove(minor);
		return -EFAULT;
	}
	memset(snap_lv_ptr, 0, size);

	/* get the logical volume structures */
	vg_ptr->lv_cur = 0;
	for (l = 0; l < vg_ptr->lv_max; l++) {
		lv_t *lvp;
		/* user space address */
		if ((lvp = vg_ptr->lv[l]) != NULL) {
			if (copy_from_user(&lv, lvp, sizeof(lv_t)) != 0) {
				P_IOCTL
				    ("ERROR: copying LV ptr %p (%d bytes)\n",
				     lvp, sizeof(lv_t));
				goto copy_fault;
			}
			if (lv.lv_access & LV_SNAPSHOT) {
				snap_lv_ptr[ls] = lvp;
				vg_ptr->lv[l] = NULL;
				ls++;
				continue;
			}
			vg_ptr->lv[l] = NULL;
			/* only create original logical volumes for now */
			if (lvm_do_lv_create(minor, lv.lv_name, &lv) != 0) {
				goto copy_fault;
			}
		}
	}

	/* Second path to correct snapshot logical volumes which are not
	   in place during first path above */
	for (l = 0; l < ls; l++) {
		lv_t *lvp = snap_lv_ptr[l];
		if (copy_from_user(&lv, lvp, sizeof(lv_t)) != 0) {
			goto copy_fault;
		}
		if (lvm_do_lv_create(minor, lv.lv_name, &lv) != 0) {
			goto copy_fault;
		}
	}

	vfree(snap_lv_ptr);

	vg_count++;


	MOD_INC_USE_COUNT;

	/* let's go active */
	vg_ptr->vg_status |= VG_ACTIVE;

	return 0;
copy_fault:
	lvm_do_vg_remove(minor);
	vfree(snap_lv_ptr);
	return -EFAULT;
}				/* lvm_do_vg_create() */


/*
 * character device support function VGDA extend
 */
static int lvm_do_vg_extend(vg_t * vg_ptr, void *arg)
{
	int ret = 0;
	uint p;
	pv_t *pv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (vg_ptr->pv_cur < vg_ptr->pv_max) {
		for (p = 0; p < vg_ptr->pv_max; p++) {
			if ((pv_ptr = vg_ptr->pv[p]) == NULL) {
				ret = lvm_do_pv_create(arg, vg_ptr, p);
				if (ret != 0)
					return ret;
				pv_ptr = vg_ptr->pv[p];
				vg_ptr->pe_total += pv_ptr->pe_total;
				return 0;
			}
		}
	}
	return -EPERM;
}				/* lvm_do_vg_extend() */


/*
 * character device support function VGDA reduce
 */
static int lvm_do_vg_reduce(vg_t * vg_ptr, void *arg)
{
	uint p;
	pv_t *pv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(pv_name, arg, sizeof(pv_name)) != 0)
		return -EFAULT;

	for (p = 0; p < vg_ptr->pv_max; p++) {
		pv_ptr = vg_ptr->pv[p];
		if (pv_ptr != NULL &&
		    strcmp(pv_ptr->pv_name, pv_name) == 0) {
			if (pv_ptr->lv_cur > 0)
				return -EPERM;
			lvm_do_pv_remove(vg_ptr, p);
			/* Make PV pointer array contiguous */
			for (; p < vg_ptr->pv_max - 1; p++)
				vg_ptr->pv[p] = vg_ptr->pv[p + 1];
			vg_ptr->pv[p + 1] = NULL;
			return 0;
		}
	}
	return -ENXIO;
}				/* lvm_do_vg_reduce */


/*
 * character device support function VG rename
 */
static int lvm_do_vg_rename(vg_t * vg_ptr, void *arg)
{
	int l = 0, p = 0, len = 0;
	char vg_name[NAME_LEN] = { 0, };
	char lv_name[NAME_LEN] = { 0, };
	char *ptr = NULL;
	lv_t *lv_ptr = NULL;
	pv_t *pv_ptr = NULL;

	/* If the VG doesn't exist in the kernel then just exit */
	if (!vg_ptr)
		return 0;

	if (copy_from_user(vg_name, arg, sizeof(vg_name)) != 0)
		return -EFAULT;

	lvm_fs_remove_vg(vg_ptr);

	strncpy(vg_ptr->vg_name, vg_name, sizeof(vg_name) - 1);
	for (l = 0; l < vg_ptr->lv_max; l++) {
		if ((lv_ptr = vg_ptr->lv[l]) == NULL)
			continue;
		memset(lv_ptr->vg_name, 0, sizeof(*vg_name));
		strncpy(lv_ptr->vg_name, vg_name, sizeof(vg_name));
		ptr = strrchr(lv_ptr->lv_name, '/');
		ptr = ptr ? ptr + 1 : lv_ptr->lv_name;
		strncpy(lv_name, ptr, sizeof(lv_name));
		len = sizeof(LVM_DIR_PREFIX);
		strcpy(lv_ptr->lv_name, LVM_DIR_PREFIX);
		strncat(lv_ptr->lv_name, vg_name, NAME_LEN - len);
		strcat(lv_ptr->lv_name, "/");
		len += strlen(vg_name) + 1;
		strncat(lv_ptr->lv_name, lv_name, NAME_LEN - len);
	}
	for (p = 0; p < vg_ptr->pv_max; p++) {
		if ((pv_ptr = vg_ptr->pv[p]) == NULL)
			continue;
		strncpy(pv_ptr->vg_name, vg_name, NAME_LEN);
	}

	lvm_fs_create_vg(vg_ptr);

	/* Need to add PV entries */
	for (p = 0; p < vg_ptr->pv_act; p++) {
		pv_t *pv_ptr = vg_ptr->pv[p];

		if (pv_ptr)
			lvm_fs_create_pv(vg_ptr, pv_ptr);
	}

	/* Need to add LV entries */
	for (l = 0; l < vg_ptr->lv_max; l++) {
		lv_t *lv_ptr = vg_ptr->lv[l];

		if (!lv_ptr)
			continue;

		lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].de =
		    lvm_fs_create_lv(vg_ptr, lv_ptr);
	}

	return 0;
}				/* lvm_do_vg_rename */


/*
 * character device support function VGDA remove
 */
static int lvm_do_vg_remove(int minor)
{
	int i;
	vg_t *vg_ptr = vg[VG_CHR(minor)];
	pv_t *pv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;

#ifdef LVM_TOTAL_RESET
	if (vg_ptr->lv_open > 0 && lvm_reset_spindown == 0)
#else
	if (vg_ptr->lv_open > 0)
#endif
		return -EPERM;

	/* let's go inactive */
	vg_ptr->vg_status &= ~VG_ACTIVE;

	/* remove from procfs and devfs */
	lvm_fs_remove_vg(vg_ptr);

	/* free LVs */
	/* first free snapshot logical volumes */
	for (i = 0; i < vg_ptr->lv_max; i++) {
		if (vg_ptr->lv[i] != NULL &&
		    vg_ptr->lv[i]->lv_access & LV_SNAPSHOT) {
			lvm_do_lv_remove(minor, NULL, i);
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout(1);
		}
	}
	/* then free the rest of the LVs */
	for (i = 0; i < vg_ptr->lv_max; i++) {
		if (vg_ptr->lv[i] != NULL) {
			lvm_do_lv_remove(minor, NULL, i);
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout(1);
		}
	}

	/* free PVs */
	for (i = 0; i < vg_ptr->pv_max; i++) {
		if ((pv_ptr = vg_ptr->pv[i]) != NULL) {
			P_KFREE("%s -- kfree %d\n", lvm_name, __LINE__);
			lvm_do_pv_remove(vg_ptr, i);
		}
	}

	P_KFREE("%s -- kfree %d\n", lvm_name, __LINE__);
	kfree(vg_ptr);
	vg[VG_CHR(minor)] = NULL;

	vg_count--;

	MOD_DEC_USE_COUNT;

	return 0;
}				/* lvm_do_vg_remove() */


/*
 * character device support function physical volume create
 */
static int lvm_do_pv_create(pv_t * pvp, vg_t * vg_ptr, ulong p)
{
	pv_t *pv;
	int err;

	if (!vg_ptr)
		return -ENXIO;

	pv = kmalloc(sizeof(pv_t), GFP_KERNEL);
	if (pv == NULL) {
		printk(KERN_CRIT
		       "%s -- PV_CREATE: kmalloc error PV at line %d\n",
		       lvm_name, __LINE__);
		return -ENOMEM;
	}

	memset(pv, 0, sizeof(*pv));

	if (copy_from_user(pv, pvp, sizeof(pv_t)) != 0) {
		P_IOCTL
		    ("lvm_do_pv_create ERROR: copy PV ptr %p (%d bytes)\n",
		     pvp, sizeof(pv_t));
		kfree(pv);
		return -EFAULT;
	}

	if ((err = _open_pv(pv))) {
		kfree(pv);
		return err;
	}

	/* We don't need the PE list
	   in kernel space as with LVs pe_t list (see below) */
	pv->pe = NULL;
	pv->pe_allocated = 0;
	pv->pv_status = PV_ACTIVE;
	vg_ptr->pv_act++;
	vg_ptr->pv_cur++;
	lvm_fs_create_pv(vg_ptr, pv);

	vg_ptr->pv[p] = pv;
	return 0;
}				/* lvm_do_pv_create() */


/*
 * character device support function physical volume remove
 */
static int lvm_do_pv_remove(vg_t * vg_ptr, ulong p)
{
	pv_t *pv = vg_ptr->pv[p];

	lvm_fs_remove_pv(vg_ptr, pv);

	vg_ptr->pe_total -= pv->pe_total;
	vg_ptr->pv_cur--;
	vg_ptr->pv_act--;

	_close_pv(pv);
	kfree(pv);

	vg_ptr->pv[p] = NULL;

	return 0;
}


static void __update_hardsectsize(lv_t * lv)
{
	int max_hardsectsize = 0, hardsectsize = 0;
	int p;

	/* Check PVs first to see if they all have same sector size */
	for (p = 0; p < lv->vg->pv_cur; p++) {
		pv_t *pv = lv->vg->pv[p];
		if (pv && (hardsectsize = lvm_sectsize(pv->pv_dev))) {
			if (max_hardsectsize == 0)
				max_hardsectsize = hardsectsize;
			else if (hardsectsize != max_hardsectsize) {
				P_DEV
				    ("%s PV[%d] (%s) sector size %d, not %d\n",
				     lv->lv_name, p, kdevname(pv->pv_dev),
				     hardsectsize, max_hardsectsize);
				break;
			}
		}
	}

	/* PVs have different block size, need to check each LE sector size */
	if (hardsectsize != max_hardsectsize) {
		int le;
		for (le = 0; le < lv->lv_allocated_le; le++) {
			hardsectsize =
			    lvm_sectsize(lv->lv_current_pe[le].dev);
			if (hardsectsize > max_hardsectsize) {
				P_DEV
				    ("%s LE[%d] (%s) blocksize %d not %d\n",
				     lv->lv_name, le,
				     kdevname(lv->lv_current_pe[le].dev),
				     hardsectsize, max_hardsectsize);
				max_hardsectsize = hardsectsize;
			}
		}

		/* only perform this operation on active snapshots */
		if ((lv->lv_access & LV_SNAPSHOT) &&
		    (lv->lv_status & LV_ACTIVE)) {
			int e;
			for (e = 0; e < lv->lv_remap_end; e++) {
				hardsectsize =
				    lvm_sectsize(lv->lv_block_exception[e].
						 rdev_new);
				if (hardsectsize > max_hardsectsize)
					max_hardsectsize = hardsectsize;
			}
		}
	}

	if (max_hardsectsize == 0)
		max_hardsectsize = SECTOR_SIZE;
	P_DEV("hardblocksize for LV %s is %d\n",
	      kdevname(lv->lv_dev), max_hardsectsize);
	lvm_hardsectsizes[MINOR(lv->lv_dev)] = max_hardsectsize;
}

/*
 * character device support function logical volume create
 */
static int lvm_do_lv_create(int minor, char *lv_name, lv_t * lv)
{
	int e, ret, l, le, l_new, p, size, activate = 1;
	ulong lv_status_save;
	lv_block_exception_t *lvbe = lv->lv_block_exception;
	vg_t *vg_ptr = vg[VG_CHR(minor)];
	lv_t *lv_ptr = NULL;
	pe_t *pep;

	if (!(pep = lv->lv_current_pe))
		return -EINVAL;

	if (_sectors_to_k(lv->lv_chunk_size) > LVM_SNAPSHOT_MAX_CHUNK)
		return -EINVAL;

	for (l = 0; l < vg_ptr->lv_cur; l++) {
		if (vg_ptr->lv[l] != NULL &&
		    strcmp(vg_ptr->lv[l]->lv_name, lv_name) == 0)
			return -EEXIST;
	}

	/* in case of lv_remove(), lv_create() pair */
	l_new = -1;
	if (vg_ptr->lv[lv->lv_number] == NULL)
		l_new = lv->lv_number;
	else {
		for (l = 0; l < vg_ptr->lv_max; l++) {
			if (vg_ptr->lv[l] == NULL)
				if (l_new == -1)
					l_new = l;
		}
	}
	if (l_new == -1)
		return -EPERM;
	else
		l = l_new;

	if ((lv_ptr = kmalloc(sizeof(lv_t), GFP_KERNEL)) == NULL) {;
		printk(KERN_CRIT
		       "%s -- LV_CREATE: kmalloc error LV at line %d\n",
		       lvm_name, __LINE__);
		return -ENOMEM;
	}
	/* copy preloaded LV */
	memcpy((char *) lv_ptr, (char *) lv, sizeof(lv_t));

	lv_status_save = lv_ptr->lv_status;
	lv_ptr->lv_status &= ~LV_ACTIVE;
	lv_ptr->lv_snapshot_org = NULL;
	lv_ptr->lv_snapshot_prev = NULL;
	lv_ptr->lv_snapshot_next = NULL;
	lv_ptr->lv_block_exception = NULL;
	lv_ptr->lv_iobuf = NULL;
	lv_ptr->lv_COW_table_iobuf = NULL;
	lv_ptr->lv_snapshot_hash_table = NULL;
	lv_ptr->lv_snapshot_hash_table_size = 0;
	lv_ptr->lv_snapshot_hash_mask = 0;
	init_rwsem(&lv_ptr->lv_lock);

	lv_ptr->lv_snapshot_use_rate = 0;

	vg_ptr->lv[l] = lv_ptr;

	/* get the PE structures from user space if this
	   is not a snapshot logical volume */
	if (!(lv_ptr->lv_access & LV_SNAPSHOT)) {
		size = lv_ptr->lv_allocated_le * sizeof(pe_t);

		if ((lv_ptr->lv_current_pe = vmalloc(size)) == NULL) {
			printk(KERN_CRIT
			       "%s -- LV_CREATE: vmalloc error LV_CURRENT_PE of %d Byte "
			       "at line %d\n", lvm_name, size, __LINE__);
			P_KFREE("%s -- kfree %d\n", lvm_name, __LINE__);
			kfree(lv_ptr);
			vg_ptr->lv[l] = NULL;
			return -ENOMEM;
		}
		if (copy_from_user(lv_ptr->lv_current_pe, pep, size)) {
			P_IOCTL("ERROR: copying PE ptr %p (%d bytes)\n",
				pep, sizeof(size));
			vfree(lv_ptr->lv_current_pe);
			kfree(lv_ptr);
			vg_ptr->lv[l] = NULL;
			return -EFAULT;
		}
		/* correct the PE count in PVs */
		for (le = 0; le < lv_ptr->lv_allocated_le; le++) {
			vg_ptr->pe_allocated++;
			for (p = 0; p < vg_ptr->pv_cur; p++) {
				if (vg_ptr->pv[p]->pv_dev ==
				    lv_ptr->lv_current_pe[le].dev)
					vg_ptr->pv[p]->pe_allocated++;
			}
		}
	} else {
		/* Get snapshot exception data and block list */
		if (lvbe != NULL) {
			lv_ptr->lv_snapshot_org =
			    vg_ptr->lv[LV_BLK(lv_ptr->lv_snapshot_minor)];
			if (lv_ptr->lv_snapshot_org != NULL) {
				size =
				    lv_ptr->lv_remap_end *
				    sizeof(lv_block_exception_t);

				if (!size) {
					printk(KERN_WARNING
					       "%s -- zero length exception table requested\n",
					       lvm_name);
					kfree(lv_ptr);
					return -EINVAL;
				}

				if ((lv_ptr->lv_block_exception =
				     vmalloc(size)) == NULL) {
					printk(KERN_CRIT
					       "%s -- lvm_do_lv_create: vmalloc error LV_BLOCK_EXCEPTION "
					       "of %d byte at line %d\n",
					       lvm_name, size, __LINE__);
					P_KFREE("%s -- kfree %d\n",
						lvm_name, __LINE__);
					kfree(lv_ptr);
					vg_ptr->lv[l] = NULL;
					return -ENOMEM;
				}
				if (copy_from_user
				    (lv_ptr->lv_block_exception, lvbe,
				     size)) {
					vfree(lv_ptr->lv_block_exception);
					kfree(lv_ptr);
					vg_ptr->lv[l] = NULL;
					return -EFAULT;
				}

				if (lv_ptr->lv_block_exception[0].
				    rsector_org ==
				    LVM_SNAPSHOT_DROPPED_SECTOR) {
					printk(KERN_WARNING
					       "%s -- lvm_do_lv_create: snapshot has been dropped and will not be activated\n",
					       lvm_name);
					activate = 0;
				}

				/* point to the original logical volume */
				lv_ptr = lv_ptr->lv_snapshot_org;

				lv_ptr->lv_snapshot_minor = 0;
				lv_ptr->lv_snapshot_org = lv_ptr;
				/* our new one now back points to the previous last in the chain
				   which can be the original logical volume */
				lv_ptr = vg_ptr->lv[l];
				/* now lv_ptr points to our new last snapshot logical volume */
				lv_ptr->lv_current_pe =
				    lv_ptr->lv_snapshot_org->lv_current_pe;
				lv_ptr->lv_allocated_snapshot_le =
				    lv_ptr->lv_allocated_le;
				lv_ptr->lv_allocated_le =
				    lv_ptr->lv_snapshot_org->
				    lv_allocated_le;
				lv_ptr->lv_current_le =
				    lv_ptr->lv_snapshot_org->lv_current_le;
				lv_ptr->lv_size =
				    lv_ptr->lv_snapshot_org->lv_size;
				lv_ptr->lv_stripes =
				    lv_ptr->lv_snapshot_org->lv_stripes;
				lv_ptr->lv_stripesize =
				    lv_ptr->lv_snapshot_org->lv_stripesize;

				/* Update the VG PE(s) used by snapshot reserve space. */
				vg_ptr->pe_allocated +=
				    lv_ptr->lv_allocated_snapshot_le;

				if ((ret =
				     lvm_snapshot_alloc(lv_ptr)) != 0) {
					vfree(lv_ptr->lv_block_exception);
					kfree(lv_ptr);
					vg_ptr->lv[l] = NULL;
					return ret;
				}
				for (e = 0; e < lv_ptr->lv_remap_ptr; e++)
					lvm_hash_link(lv_ptr->
						      lv_block_exception +
						      e,
						      lv_ptr->
						      lv_block_exception
						      [e].rdev_org,
						      lv_ptr->
						      lv_block_exception
						      [e].rsector_org,
						      lv_ptr);
				/* need to fill the COW exception table data
				   into the page for disk i/o */
				if (lvm_snapshot_fill_COW_page
				    (vg_ptr, lv_ptr)) {
					kfree(lv_ptr);
					vg_ptr->lv[l] = NULL;
					return -EINVAL;
				}
				init_waitqueue_head(&lv_ptr->
						    lv_snapshot_wait);
			} else {
				kfree(lv_ptr);
				vg_ptr->lv[l] = NULL;
				return -EFAULT;
			}
		} else {
			kfree(vg_ptr->lv[l]);
			vg_ptr->lv[l] = NULL;
			return -EINVAL;
		}
	}			/* if ( vg[VG_CHR(minor)]->lv[l]->lv_access & LV_SNAPSHOT) */

	lv_ptr = vg_ptr->lv[l];
	lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].start_sect = 0;
	lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].nr_sects = lv_ptr->lv_size;
	lvm_size[MINOR(lv_ptr->lv_dev)] = lv_ptr->lv_size >> 1;
	vg_lv_map[MINOR(lv_ptr->lv_dev)].vg_number = vg_ptr->vg_number;
	vg_lv_map[MINOR(lv_ptr->lv_dev)].lv_number = lv_ptr->lv_number;
	LVM_CORRECT_READ_AHEAD(lv_ptr->lv_read_ahead);
	vg_ptr->lv_cur++;
	lv_ptr->lv_status = lv_status_save;
	lv_ptr->vg = vg_ptr;

	__update_hardsectsize(lv_ptr);

	/* optionally add our new snapshot LV */
	if (lv_ptr->lv_access & LV_SNAPSHOT) {
		lv_t *org = lv_ptr->lv_snapshot_org, *last;

		/* sync the original logical volume */
		fsync_dev(org->lv_dev);
#ifdef	LVM_VFS_ENHANCEMENT
		/* VFS function call to sync and lock the filesystem */
		fsync_dev_lockfs(org->lv_dev);
#endif

		down_write(&org->lv_lock);
		org->lv_access |= LV_SNAPSHOT_ORG;
		lv_ptr->lv_access &= ~LV_SNAPSHOT_ORG;	/* this can only hide an userspace bug */


		/* Link in the list of snapshot volumes */
		for (last = org; last->lv_snapshot_next;
		     last = last->lv_snapshot_next);
		lv_ptr->lv_snapshot_prev = last;
		last->lv_snapshot_next = lv_ptr;
		up_write(&org->lv_lock);
	}

	/* activate the logical volume */
	if (activate)
		lv_ptr->lv_status |= LV_ACTIVE;
	else
		lv_ptr->lv_status &= ~LV_ACTIVE;

	if (lv_ptr->lv_access & LV_WRITE)
		set_device_ro(lv_ptr->lv_dev, 0);
	else
		set_device_ro(lv_ptr->lv_dev, 1);

#ifdef	LVM_VFS_ENHANCEMENT
/* VFS function call to unlock the filesystem */
	if (lv_ptr->lv_access & LV_SNAPSHOT)
		unlockfs(lv_ptr->lv_snapshot_org->lv_dev);
#endif

	lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].de =
	    lvm_fs_create_lv(vg_ptr, lv_ptr);
	return 0;
}				/* lvm_do_lv_create() */


/*
 * character device support function logical volume remove
 */
static int lvm_do_lv_remove(int minor, char *lv_name, int l)
{
	uint le, p;
	vg_t *vg_ptr = vg[VG_CHR(minor)];
	lv_t *lv_ptr;

	if (!vg_ptr)
		return -ENXIO;

	if (l == -1) {
		for (l = 0; l < vg_ptr->lv_max; l++) {
			if (vg_ptr->lv[l] != NULL &&
			    strcmp(vg_ptr->lv[l]->lv_name, lv_name) == 0) {
				break;
			}
		}
	}
	if (l == vg_ptr->lv_max)
		return -ENXIO;

	lv_ptr = vg_ptr->lv[l];
#ifdef LVM_TOTAL_RESET
	if (lv_ptr->lv_open > 0 && lvm_reset_spindown == 0)
#else
	if (lv_ptr->lv_open > 0)
#endif
		return -EBUSY;

	/* check for deletion of snapshot source while
	   snapshot volume still exists */
	if ((lv_ptr->lv_access & LV_SNAPSHOT_ORG) &&
	    lv_ptr->lv_snapshot_next != NULL)
		return -EPERM;

	lvm_fs_remove_lv(vg_ptr, lv_ptr);

	if (lv_ptr->lv_access & LV_SNAPSHOT) {
		/*
		 * Atomically make the the snapshot invisible
		 * to the original lv before playing with it.
		 */
		lv_t *org = lv_ptr->lv_snapshot_org;
		down_write(&org->lv_lock);

		/* remove this snapshot logical volume from the chain */
		lv_ptr->lv_snapshot_prev->lv_snapshot_next =
		    lv_ptr->lv_snapshot_next;
		if (lv_ptr->lv_snapshot_next != NULL) {
			lv_ptr->lv_snapshot_next->lv_snapshot_prev =
			    lv_ptr->lv_snapshot_prev;
		}

		/* no more snapshots? */
		if (!org->lv_snapshot_next) {
			org->lv_access &= ~LV_SNAPSHOT_ORG;
		}
		up_write(&org->lv_lock);

		lvm_snapshot_release(lv_ptr);

		/* Update the VG PE(s) used by snapshot reserve space. */
		vg_ptr->pe_allocated -= lv_ptr->lv_allocated_snapshot_le;
	}

	lv_ptr->lv_status |= LV_SPINDOWN;

	/* sync the buffers */
	fsync_dev(lv_ptr->lv_dev);

	lv_ptr->lv_status &= ~LV_ACTIVE;

	/* invalidate the buffers */
	invalidate_buffers(lv_ptr->lv_dev);

	/* reset generic hd */
	lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].start_sect = -1;
	lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].nr_sects = 0;
	lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].de = 0;
	lvm_size[MINOR(lv_ptr->lv_dev)] = 0;

	/* reset VG/LV mapping */
	vg_lv_map[MINOR(lv_ptr->lv_dev)].vg_number = ABS_MAX_VG;
	vg_lv_map[MINOR(lv_ptr->lv_dev)].lv_number = -1;

	/* correct the PE count in PVs if this is not a snapshot
	   logical volume */
	if (!(lv_ptr->lv_access & LV_SNAPSHOT)) {
		/* only if this is no snapshot logical volume because
		   we share the lv_current_pe[] structs with the
		   original logical volume */
		for (le = 0; le < lv_ptr->lv_allocated_le; le++) {
			vg_ptr->pe_allocated--;
			for (p = 0; p < vg_ptr->pv_cur; p++) {
				if (vg_ptr->pv[p]->pv_dev ==
				    lv_ptr->lv_current_pe[le].dev)
					vg_ptr->pv[p]->pe_allocated--;
			}
		}
		vfree(lv_ptr->lv_current_pe);
	}

	P_KFREE("%s -- kfree %d\n", lvm_name, __LINE__);
	kfree(lv_ptr);
	vg_ptr->lv[l] = NULL;
	vg_ptr->lv_cur--;
	return 0;
}				/* lvm_do_lv_remove() */


/*
 * logical volume extend / reduce
 */
static int __extend_reduce_snapshot(vg_t * vg_ptr, lv_t * old_lv,
				    lv_t * new_lv)
{
	ulong size;
	lv_block_exception_t *lvbe;

	if (!new_lv->lv_block_exception)
		return -ENXIO;

	size = new_lv->lv_remap_end * sizeof(lv_block_exception_t);
	if ((lvbe = vmalloc(size)) == NULL) {
		printk(KERN_CRIT
		       "%s -- lvm_do_lv_extend_reduce: vmalloc "
		       "error LV_BLOCK_EXCEPTION of %lu Byte at line %d\n",
		       lvm_name, size, __LINE__);
		return -ENOMEM;
	}

	if ((new_lv->lv_remap_end > old_lv->lv_remap_end) &&
	    (copy_from_user(lvbe, new_lv->lv_block_exception, size))) {
		vfree(lvbe);
		return -EFAULT;
	}
	new_lv->lv_block_exception = lvbe;

	if (lvm_snapshot_alloc_hash_table(new_lv)) {
		vfree(new_lv->lv_block_exception);
		return -ENOMEM;
	}

	return 0;
}

static int __extend_reduce(vg_t * vg_ptr, lv_t * old_lv, lv_t * new_lv)
{
	ulong size, l, p, end;
	pe_t *pe;

	/* allocate space for new pe structures */
	size = new_lv->lv_current_le * sizeof(pe_t);
	if ((pe = vmalloc(size)) == NULL) {
		printk(KERN_CRIT
		       "%s -- lvm_do_lv_extend_reduce: "
		       "vmalloc error LV_CURRENT_PE of %lu Byte at line %d\n",
		       lvm_name, size, __LINE__);
		return -ENOMEM;
	}

	/* get the PE structures from user space */
	if (copy_from_user(pe, new_lv->lv_current_pe, size)) {
		if (old_lv->lv_access & LV_SNAPSHOT)
			vfree(new_lv->lv_snapshot_hash_table);
		vfree(pe);
		return -EFAULT;
	}

	new_lv->lv_current_pe = pe;

	/* reduce allocation counters on PV(s) */
	for (l = 0; l < old_lv->lv_allocated_le; l++) {
		vg_ptr->pe_allocated--;
		for (p = 0; p < vg_ptr->pv_cur; p++) {
			if (vg_ptr->pv[p]->pv_dev ==
			    old_lv->lv_current_pe[l].dev) {
				vg_ptr->pv[p]->pe_allocated--;
				break;
			}
		}
	}

	/* extend the PE count in PVs */
	for (l = 0; l < new_lv->lv_allocated_le; l++) {
		vg_ptr->pe_allocated++;
		for (p = 0; p < vg_ptr->pv_cur; p++) {
			if (vg_ptr->pv[p]->pv_dev ==
			    new_lv->lv_current_pe[l].dev) {
				vg_ptr->pv[p]->pe_allocated++;
				break;
			}
		}
	}

	/* save available i/o statistic data */
	if (old_lv->lv_stripes < 2) {	/* linear logical volume */
		end = min(old_lv->lv_current_le, new_lv->lv_current_le);
		for (l = 0; l < end; l++) {
			new_lv->lv_current_pe[l].reads +=
			    old_lv->lv_current_pe[l].reads;

			new_lv->lv_current_pe[l].writes +=
			    old_lv->lv_current_pe[l].writes;
		}

	} else {		/* striped logical volume */
		uint i, j, source, dest, end, old_stripe_size,
		    new_stripe_size;

		old_stripe_size =
		    old_lv->lv_allocated_le / old_lv->lv_stripes;
		new_stripe_size =
		    new_lv->lv_allocated_le / new_lv->lv_stripes;
		end = min(old_stripe_size, new_stripe_size);

		for (i = source = dest = 0; i < new_lv->lv_stripes; i++) {
			for (j = 0; j < end; j++) {
				new_lv->lv_current_pe[dest + j].reads +=
				    old_lv->lv_current_pe[source +
							  j].reads;
				new_lv->lv_current_pe[dest + j].writes +=
				    old_lv->lv_current_pe[source +
							  j].writes;
			}
			source += old_stripe_size;
			dest += new_stripe_size;
		}
	}

	return 0;
}

static int lvm_do_lv_extend_reduce(int minor, char *lv_name, lv_t * new_lv)
{
	int r;
	ulong l, e, size;
	vg_t *vg_ptr = vg[VG_CHR(minor)];
	lv_t *old_lv;
	pe_t *pe;

	if (!vg_ptr)
		return -ENXIO;

	if ((pe = new_lv->lv_current_pe) == NULL)
		return -EINVAL;

	for (l = 0; l < vg_ptr->lv_max; l++)
		if (vg_ptr->lv[l]
		    && !strcmp(vg_ptr->lv[l]->lv_name, lv_name))
			break;

	if (l == vg_ptr->lv_max)
		return -ENXIO;

	old_lv = vg_ptr->lv[l];

	if (old_lv->lv_access & LV_SNAPSHOT) {
		/* only perform this operation on active snapshots */
		if (old_lv->lv_status & LV_ACTIVE)
			r = __extend_reduce_snapshot(vg_ptr, old_lv,
						     new_lv);
		else
			r = -EPERM;

	} else
		r = __extend_reduce(vg_ptr, old_lv, new_lv);

	if (r)
		return r;

	/* copy relevant fields */
	down_write(&old_lv->lv_lock);

	if (new_lv->lv_access & LV_SNAPSHOT) {
		size = (new_lv->lv_remap_end > old_lv->lv_remap_end) ?
		    old_lv->lv_remap_ptr : new_lv->lv_remap_end;
		size *= sizeof(lv_block_exception_t);
		memcpy(new_lv->lv_block_exception,
		       old_lv->lv_block_exception, size);
		vfree(old_lv->lv_block_exception);
		vfree(old_lv->lv_snapshot_hash_table);

		old_lv->lv_remap_end = new_lv->lv_remap_end;
		old_lv->lv_block_exception = new_lv->lv_block_exception;
		old_lv->lv_snapshot_hash_table =
		    new_lv->lv_snapshot_hash_table;
		old_lv->lv_snapshot_hash_table_size =
		    new_lv->lv_snapshot_hash_table_size;
		old_lv->lv_snapshot_hash_mask =
		    new_lv->lv_snapshot_hash_mask;

		for (e = 0; e < old_lv->lv_remap_ptr; e++)
			lvm_hash_link(new_lv->lv_block_exception + e,
				      new_lv->lv_block_exception[e].
				      rdev_org,
				      new_lv->lv_block_exception[e].
				      rsector_org, new_lv);

		vg_ptr->pe_allocated -= old_lv->lv_allocated_snapshot_le;
		vg_ptr->pe_allocated += new_lv->lv_allocated_le;
		old_lv->lv_allocated_snapshot_le = new_lv->lv_allocated_le;
	} else {
		vfree(old_lv->lv_current_pe);
		vfree(old_lv->lv_snapshot_hash_table);

		old_lv->lv_size = new_lv->lv_size;
		old_lv->lv_allocated_le = new_lv->lv_allocated_le;
		old_lv->lv_current_le = new_lv->lv_current_le;
		old_lv->lv_current_pe = new_lv->lv_current_pe;
		lvm_gendisk.part[MINOR(old_lv->lv_dev)].nr_sects =
		    old_lv->lv_size;
		lvm_size[MINOR(old_lv->lv_dev)] = old_lv->lv_size >> 1;

		if (old_lv->lv_access & LV_SNAPSHOT_ORG) {
			lv_t *snap;
			for (snap = old_lv->lv_snapshot_next; snap;
			     snap = snap->lv_snapshot_next) {
				down_write(&snap->lv_lock);
				snap->lv_current_pe =
				    old_lv->lv_current_pe;
				snap->lv_allocated_le =
				    old_lv->lv_allocated_le;
				snap->lv_current_le =
				    old_lv->lv_current_le;
				snap->lv_size = old_lv->lv_size;

				lvm_gendisk.part[MINOR(snap->lv_dev)].
				    nr_sects = old_lv->lv_size;
				lvm_size[MINOR(snap->lv_dev)] =
				    old_lv->lv_size >> 1;
				__update_hardsectsize(snap);
				up_write(&snap->lv_lock);
			}
		}
	}

	__update_hardsectsize(old_lv);
	up_write(&old_lv->lv_lock);

	return 0;
}				/* lvm_do_lv_extend_reduce() */


/*
 * character device support function logical volume status by name
 */
static int lvm_do_lv_status_byname(vg_t * vg_ptr, void *arg)
{
	uint l;
	lv_status_byname_req_t lv_status_byname_req;
	void *saved_ptr1;
	void *saved_ptr2;
	lv_t *lv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&lv_status_byname_req, arg,
			   sizeof(lv_status_byname_req_t)) != 0)
		return -EFAULT;

	if (lv_status_byname_req.lv == NULL)
		return -EINVAL;

	for (l = 0; l < vg_ptr->lv_max; l++) {
		if ((lv_ptr = vg_ptr->lv[l]) != NULL &&
		    strcmp(lv_ptr->lv_name,
			   lv_status_byname_req.lv_name) == 0) {
			/* Save usermode pointers */
			if (copy_from_user
			    (&saved_ptr1,
			     &lv_status_byname_req.lv->lv_current_pe,
			     sizeof(void *)) != 0)
				return -EFAULT;
			if (copy_from_user
			    (&saved_ptr2,
			     &lv_status_byname_req.lv->lv_block_exception,
			     sizeof(void *)) != 0)
				return -EFAULT;
			if (copy_to_user(lv_status_byname_req.lv,
					 lv_ptr, sizeof(lv_t)) != 0)
				return -EFAULT;
			if (saved_ptr1 != NULL) {
				if (copy_to_user(saved_ptr1,
						 lv_ptr->lv_current_pe,
						 lv_ptr->lv_allocated_le *
						 sizeof(pe_t)) != 0)
					return -EFAULT;
			}
			/* Restore usermode pointers */
			if (copy_to_user
			    (&lv_status_byname_req.lv->lv_current_pe,
			     &saved_ptr1, sizeof(void *)) != 0)
				return -EFAULT;
			return 0;
		}
	}
	return -ENXIO;
}				/* lvm_do_lv_status_byname() */


/*
 * character device support function logical volume status by index
 */
static int lvm_do_lv_status_byindex(vg_t * vg_ptr, void *arg)
{
	lv_status_byindex_req_t lv_status_byindex_req;
	void *saved_ptr1;
	void *saved_ptr2;
	lv_t *lv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&lv_status_byindex_req, arg,
			   sizeof(lv_status_byindex_req)) != 0)
		return -EFAULT;

	if (lv_status_byindex_req.lv == NULL)
		return -EINVAL;
	if ((lv_ptr = vg_ptr->lv[lv_status_byindex_req.lv_index]) == NULL)
		return -ENXIO;

	/* Save usermode pointers */
	if (copy_from_user
	    (&saved_ptr1, &lv_status_byindex_req.lv->lv_current_pe,
	     sizeof(void *)) != 0)
		return -EFAULT;
	if (copy_from_user
	    (&saved_ptr2, &lv_status_byindex_req.lv->lv_block_exception,
	     sizeof(void *)) != 0)
		return -EFAULT;

	if (copy_to_user(lv_status_byindex_req.lv, lv_ptr, sizeof(lv_t)) !=
	    0)
		return -EFAULT;
	if (saved_ptr1 != NULL) {
		if (copy_to_user(saved_ptr1,
				 lv_ptr->lv_current_pe,
				 lv_ptr->lv_allocated_le *
				 sizeof(pe_t)) != 0)
			return -EFAULT;
	}

	/* Restore usermode pointers */
	if (copy_to_user
	    (&lv_status_byindex_req.lv->lv_current_pe, &saved_ptr1,
	     sizeof(void *)) != 0)
		return -EFAULT;

	return 0;
}				/* lvm_do_lv_status_byindex() */


/*
 * character device support function logical volume status by device number
 */
static int lvm_do_lv_status_bydev(vg_t * vg_ptr, void *arg)
{
	int l;
	lv_status_bydev_req_t lv_status_bydev_req;
	void *saved_ptr1;
	void *saved_ptr2;
	lv_t *lv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&lv_status_bydev_req, arg,
			   sizeof(lv_status_bydev_req)) != 0)
		return -EFAULT;

	for (l = 0; l < vg_ptr->lv_max; l++) {
		if (vg_ptr->lv[l] == NULL)
			continue;
		if (vg_ptr->lv[l]->lv_dev == lv_status_bydev_req.dev)
			break;
	}

	if (l == vg_ptr->lv_max)
		return -ENXIO;
	lv_ptr = vg_ptr->lv[l];

	/* Save usermode pointers */
	if (copy_from_user
	    (&saved_ptr1, &lv_status_bydev_req.lv->lv_current_pe,
	     sizeof(void *)) != 0)
		return -EFAULT;
	if (copy_from_user
	    (&saved_ptr2, &lv_status_bydev_req.lv->lv_block_exception,
	     sizeof(void *)) != 0)
		return -EFAULT;

	if (copy_to_user(lv_status_bydev_req.lv, lv_ptr, sizeof(lv_t)) !=
	    0)
		return -EFAULT;
	if (saved_ptr1 != NULL) {
		if (copy_to_user(saved_ptr1,
				 lv_ptr->lv_current_pe,
				 lv_ptr->lv_allocated_le *
				 sizeof(pe_t)) != 0)
			return -EFAULT;
	}
	/* Restore usermode pointers */
	if (copy_to_user
	    (&lv_status_bydev_req.lv->lv_current_pe, &saved_ptr1,
	     sizeof(void *)) != 0)
		return -EFAULT;

	return 0;
}				/* lvm_do_lv_status_bydev() */


/*
 * character device support function rename a logical volume
 */
static int lvm_do_lv_rename(vg_t * vg_ptr, lv_req_t * lv_req, lv_t * lv)
{
	int l = 0;
	int ret = 0;
	lv_t *lv_ptr = NULL;

	if (!vg_ptr)
		return -ENXIO;

	for (l = 0; l < vg_ptr->lv_max; l++) {
		if ((lv_ptr = vg_ptr->lv[l]) == NULL)
			continue;
		if (lv_ptr->lv_dev == lv->lv_dev) {
			lvm_fs_remove_lv(vg_ptr, lv_ptr);
			strncpy(lv_ptr->lv_name, lv_req->lv_name,
				NAME_LEN);
			lvm_gendisk.part[MINOR(lv_ptr->lv_dev)].de =
				lvm_fs_create_lv(vg_ptr, lv_ptr);
			break;
		}
	}
	if (l == vg_ptr->lv_max)
		ret = -ENODEV;

	return ret;
}				/* lvm_do_lv_rename */


/*
 * character device support function physical volume change
 */
static int lvm_do_pv_change(vg_t * vg_ptr, void *arg)
{
	uint p;
	pv_t *pv_ptr;
	struct block_device *bd;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&pv_change_req, arg,
			   sizeof(pv_change_req)) != 0)
		return -EFAULT;

	for (p = 0; p < vg_ptr->pv_max; p++) {
		pv_ptr = vg_ptr->pv[p];
		if (pv_ptr != NULL &&
		    strcmp(pv_ptr->pv_name, pv_change_req.pv_name) == 0) {

			bd = pv_ptr->bd;
			if (copy_from_user(pv_ptr,
					   pv_change_req.pv,
					   sizeof(pv_t)) != 0)
				return -EFAULT;
			pv_ptr->bd = bd;

			/* We don't need the PE list
			   in kernel space as with LVs pe_t list */
			pv_ptr->pe = NULL;
			return 0;
		}
	}
	return -ENXIO;
}				/* lvm_do_pv_change() */

/*
 * character device support function get physical volume status
 */
static int lvm_do_pv_status(vg_t * vg_ptr, void *arg)
{
	uint p;
	pv_t *pv_ptr;

	if (vg_ptr == NULL)
		return -ENXIO;
	if (copy_from_user(&pv_status_req, arg,
			   sizeof(pv_status_req)) != 0)
		return -EFAULT;

	for (p = 0; p < vg_ptr->pv_max; p++) {
		pv_ptr = vg_ptr->pv[p];
		if (pv_ptr != NULL &&
		    strcmp(pv_ptr->pv_name, pv_status_req.pv_name) == 0) {
			if (copy_to_user(pv_status_req.pv,
					 pv_ptr, sizeof(pv_t)) != 0)
				return -EFAULT;
			return 0;
		}
	}
	return -ENXIO;
}				/* lvm_do_pv_status() */


/*
 * character device support function flush and invalidate all buffers of a PV
 */
static int lvm_do_pv_flush(void *arg)
{
	pv_flush_req_t pv_flush_req;

	if (copy_from_user(&pv_flush_req, arg, sizeof(pv_flush_req)) != 0)
		return -EFAULT;

	fsync_dev(pv_flush_req.pv_dev);
	invalidate_buffers(pv_flush_req.pv_dev);

	return 0;
}


/*
 * support function initialize gendisk variables
 */
static void __init lvm_geninit(struct gendisk *lvm_gdisk)
{
	int i = 0;

#ifdef DEBUG_GENDISK
	printk(KERN_DEBUG "%s -- lvm_gendisk\n", lvm_name);
#endif

	for (i = 0; i < MAX_LV; i++) {
		lvm_gendisk.part[i].start_sect = -1;	/* avoid partition check */
		lvm_size[i] = lvm_gendisk.part[i].nr_sects = 0;
		lvm_blocksizes[i] = BLOCK_SIZE;
	}

	blk_size[MAJOR_NR] = lvm_size;
	blksize_size[MAJOR_NR] = lvm_blocksizes;
	hardsect_size[MAJOR_NR] = lvm_hardsectsizes;

	return;
}				/* lvm_gen_init() */



/* Must have down_write(_pe_lock) when we enqueue buffers */
static void _queue_io(struct buffer_head *bh, int rw)
{
	if (bh->b_reqnext)
		BUG();
	bh->b_reqnext = _pe_requests;
	_pe_requests = bh;
}

/* Must have down_write(_pe_lock) when we dequeue buffers */
static struct buffer_head *_dequeue_io(void)
{
	struct buffer_head *bh = _pe_requests;
	_pe_requests = NULL;
	return bh;
}

/*
 * We do not need to hold _pe_lock to flush buffers.  bh should be taken from
 * _pe_requests under down_write(_pe_lock), and then _pe_requests can be set
 * NULL and we drop _pe_lock.  Any new buffers defered at this time will be
 * added to a new list, and the old buffers can have their I/O restarted
 * asynchronously.
 *
 * If, for some reason, the same PE is locked again before all of these writes
 * have finished, then these buffers will just be re-queued (i.e. no danger).
 */
static void _flush_io(struct buffer_head *bh)
{
	while (bh) {
		struct buffer_head *next = bh->b_reqnext;
		bh->b_reqnext = NULL;
		/* resubmit this buffer head */
		generic_make_request(WRITE, bh);
		bh = next;
	}
}


/*
 * we must open the pv's before we use them
 */
static int _open_pv(pv_t * pv)
{
	int err;
	struct block_device *bd;

	if (!(bd = bdget(kdev_t_to_nr(pv->pv_dev))))
		return -ENOMEM;

	err = blkdev_get(bd, FMODE_READ | FMODE_WRITE, 0, BDEV_FILE);
	if (err)
		return err;

	pv->bd = bd;
	return 0;
}

static void _close_pv(pv_t * pv)
{
	if (pv) {
		struct block_device *bdev = pv->bd;
		pv->bd = NULL;
		if (bdev)
			blkdev_put(bdev, BDEV_FILE);
	}
}


static unsigned long _sectors_to_k(unsigned long sect)
{
	if (SECTOR_SIZE > 1024) {
		return sect * (SECTOR_SIZE / 1024);
	}

	return sect / (1024 / SECTOR_SIZE);
}

MODULE_AUTHOR("Heinz Mauelshagen, Sistina Software");
MODULE_DESCRIPTION("Logical Volume Manager");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

module_init(lvm_init);
module_exit(lvm_cleanup);
