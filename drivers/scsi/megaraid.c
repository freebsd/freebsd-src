/*===================================================================
 *
 *                    Linux MegaRAID device driver
 *
 * Copyright 2001  LSI Logic Corporation.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Version : v1.18k (Aug 28, 2003)
 *
 * Description: Linux device driver for LSI Logic MegaRAID controller
 *
 * Supported controllers: MegaRAID 418, 428, 438, 466, 762, 467, 471, 490
 *                                      493.
 * History:
 *
 * Version 0.90:
 *     Original source contributed by Dell; integrated it into the kernel and
 *     cleaned up some things.  Added support for 438/466 controllers.
 * Version 0.91:
 *     Aligned mailbox area on 16-byte boundary.
 *     Added schedule() at the end to properly clean up.
 *     Made improvements for conformity to linux driver standards.
 *
 * Version 0.92:
 *     Added support for 2.1 kernels.
 *         Reads from pci_dev struct, so it's not dependent on pcibios.
 *         Added some missing virt_to_bus() translations.
 *     Added support for SMP.
 *         Changed global cli()'s to spinlocks for 2.1, and simulated
 *          spinlocks for 2.0.
 *     Removed setting of SA_INTERRUPT flag when requesting Irq.
 *
 * Version 0.92ac:
 *     Small changes to the comments/formatting. Plus a couple of
 *      added notes. Returned to the authors. No actual code changes
 *      save printk levels.
 *     8 Oct 98        Alan Cox <alan.cox@linux.org>
 *
 *     Merged with 2.1.131 source tree.
 *     12 Dec 98       K. Baranowski <kgb@knm.org.pl>
 *
 * Version 0.93:
 *     Added support for vendor specific ioctl commands (M_RD_IOCTL_CMD+xxh)
 *     Changed some fields in MEGARAID struct to better values.
 *     Added signature check for Rp controllers under 2.0 kernels
 *     Changed busy-wait loop to be time-based
 *     Fixed SMP race condition in isr
 *     Added kfree (sgList) on release
 *     Added #include linux/version.h to megaraid.h for hosts.h
 *     Changed max_id to represent max logical drives instead of targets.
 *
 * Version 0.94:
 *     Got rid of some excess locking/unlocking
 *     Fixed slight memory corruption problem while memcpy'ing into mailbox
 *     Changed logical drives to be reported as luns rather than targets
 *     Changed max_id to 16 since it is now max targets/chan again.
 *     Improved ioctl interface for upcoming megamgr
 *
 * Version 0.95:
 *     Fixed problem of queueing multiple commands to adapter;
 *       still has some strange problems on some setups, so still
 *       defaults to single.  To enable parallel commands change
 *       #define MULTI_IO in megaraid.h
 *     Changed kmalloc allocation to be done in beginning.
 *     Got rid of C++ style comments
 *
 * Version 0.96:
 *     762 fully supported.
 *
 * Version 0.97:
 *     Changed megaraid_command to use wait_queue.
 *
 * Version 1.00:
 *     Checks to see if an irq occurred while in isr, and runs through
 *       routine again.
 *     Copies mailbox to temp area before processing in isr
 *     Added barrier() in busy wait to fix volatility bug
 *     Uses separate list for freed Scbs, keeps track of cmd state
 *     Put spinlocks around entire queue function for now...
 *     Full multi-io commands working stablely without previous problems
 *     Added skipXX LILO option for Madrona motherboard support
 *
 * Version 1.01:
 *     Fixed bug in mega_cmd_done() for megamgr control commands,
 *       the host_byte in the result code from the scsi request to
 *       scsi midlayer is set to DID_BAD_TARGET when adapter's
 *       returned codes are 0xF0 and 0xF4.
 *
 * Version 1.02:
 *     Fixed the tape drive bug by extending the adapter timeout value
 *       for passthrough command to 60 seconds in mega_build_cmd().
 *
 * Version 1.03:
 *    Fixed Madrona support.
 *    Changed the adapter timeout value from 60 sec in 1.02 to 10 min
 *      for bigger and slower tape drive.
 *    Added driver version printout at driver loadup time
 *
 * Version 1.04
 *    Added code for 40 ld FW support.
 *    Added new ioctl command 0x81 to support NEW_READ/WRITE_CONFIG with
 *      data area greater than 4 KB, which is the upper bound for data
 *      tranfer through scsi_ioctl interface.
 *    The additional 32 bit field for 64bit address in the newly defined
 *      mailbox64 structure is set to 0 at this point.
 *
 * Version 1.05
 *    Changed the queing implementation for handling SCBs and completed
 *      commands.
 *    Added spinlocks in the interrupt service routine to enable the driver
 *      function in the SMP environment.
 *    Fixed the problem of unnecessary aborts in the abort entry point, which
 *      also enables the driver to handle large amount of I/O requests for
 *      long duration of time.
 * Version 1.06
 *              Intel Release
 * Version 1.07
 *    Removed the usage of uaccess.h file for kernel versions less than
 *    2.0.36, as this file is not present in those versions.
 *
 * Version 108
 *    Modified mega_ioctl so that 40LD megamanager would run
 *    Made some changes for 2.3.XX compilation , esp wait structures
 *    Code merge between 1.05 and 1.06 .
 *    Bug fixed problem with ioctl interface for concurrency between
 *    8ld and 40ld firwmare
 *    Removed the flawed semaphore logic for handling new config command
 *    Added support for building own scatter / gather list for big user
 *    mode buffers
 *    Added /proc file system support ,so that information is available in
 *    human readable format
 *
 * Version 1a08
 *    Changes for IA64 kernels. Checked for CONFIG_PROC_FS flag
 *
 * Version 1b08
 *    Include file changes.
 * Version 1b08b
 *    Change PCI ID value for the 471 card, use #defines when searching
 *    for megaraid cards.
 *
 * Version 1.10
 *
 *      I) Changes made to make following ioctl commands work in 0x81 interface
 *              a)DCMD_DELETE_LOGDRV
 *              b)DCMD_GET_DISK_CONFIG
 *              c)DCMD_DELETE_DRIVEGROUP
 *              d)NC_SUBOP_ENQUIRY3
 *              e)DCMD_CHANGE_LDNO
 *              f)DCMD_CHANGE_LOOPID
 *              g)DCMD_FC_READ_NVRAM_CONFIG
 *      h)DCMD_WRITE_CONFIG
 *      II) Added mega_build_kernel_sg function
 *  III)Firmware flashing option added
 *
 * Version 1.10a
 *
 *      I)Dell updates included in the source code.
 *              Note:   This change is not tested due to the unavailability of IA64 kernel
 *      and it is in the #ifdef DELL_MODIFICATION macro which is not defined
 *
 * Version 1.10b
 *
 *      I)In M_RD_IOCTL_CMD_NEW command the wrong way of copying the data
 *    to the user address corrected
 *
 * Version 1.10c
 *
 *      I) DCMD_GET_DISK_CONFIG opcode updated for the firmware changes.
 *
 * Version 1.11
 *      I)  Version number changed from 1.10c to 1.11
 *  II) DCMD_WRITE_CONFIG(0x0D) command in the driver changed from
 *      scatter/gather list mode to direct pointer mode..
 *     Fixed bug of undesirably detecting HP onboard controllers which
 *       are disabled.
 *
 *      Version 1.12 (Sep 21, 2000)
 *
 *     I. Changes have been made for Dynamic DMA mapping in IA64 platform.
 *                To enable all these changes define M_RD_DYNAMIC_DMA_SUPPORT in megaraid.h
 *        II. Got rid of windows mode comments
 *       III. Removed unwanted code segments
 *    IV. Fixed bug of HP onboard controller information (commented with
 *                 MEGA_HP_FIX)
 *
 *      Version 1a12
 *      I.      reboot notifier and new ioctl changes ported from 1c09
 *
 *      Version 1b12
 *      I.      Changes in new ioctl interface routines ( Nov 06, 2000 )
 *
 *      Version 1c12
 *      I.      Changes in new ioctl interface routines ( Nov 07, 2000 )
 *
 *      Version 1d12
 *      I.      Compilation error under kernel 2.4.0 for 32-bit machine in mega_ioctl
 *
 *      Version 1e12, 1f12
 *      1.  Fixes for pci_map_single, pci_alloc_consistent along with mailbox
 *          alignment
 *
 *	Version 1.13beta
 *	Added Support for Full 64bit address space support. If firmware
 *	supports 64bit, it goes to 64 bit mode even on x86 32bit 
 *	systems. Data Corruption Issues while running on test9 kernel
 *	on IA64 systems. This issue not seen on test11 on x86 system
 *
 *	Version 1.13c
 *	1. Resolved Memory Leak when using M_RD_IOCTL_CMD interface
 *	2. Resolved Queuing problem when MailBox Blocks
 *	3. Added unregister_reboot_notifier support
 * 
 *	Version 1.13d
 *	Experimental changes in interfacing with the controller in ISR
 *
 *	Version 1.13e
 *	Fixed Broken 2.2.XX compilation changes + misc changes
 *
 *	Version 1.13f to 1.13i
 *	misc changes + code clean up
 *	Cleaned up the ioctl code and added set_mbox_xfer_addr()
 *	Support for START_DEV (6)
 * 	
 *	Version 1.13j
 *	Moved some code to megaraid.h file, replaced some hard coded values 
 *      with respective macros. Changed some functions to static
 *
 *	Version 1.13k
 *	Only some idendation correction to 1.13j 
 *
 *	Version 1.13l , 1.13m, 1.13n, 1.13o
 *	Minor Identation changes + misc changes
 *
 *	Version 1.13q
 *	Paded the new uioctl_t MIMD structure for maintaining alignment 
 *	and size across 32 / 64 bit platforms
 *	Changed the way MIMD IOCTL interface used virt_to_bus() to use pci
 *	memory location
 *
 *	Version 1.13r
 *	2.4.xx SCSI Changes.
 *
 *	Version 1.13s
 *	Stats counter fixes
 *	Temporary fix for some 64 bit firmwares in 2.4.XX kernels
 *
 *	Version	1.13t
 *	Support for 64bit version of READ/WRITE/VIEW DISK CONFIG
 *
 *	Version 1.14
 *	Did away with MEGADEV_IOCTL flag. It is now standard part of driver
 *	without need for a special #define flag
 *	Disabled old scsi ioctl path for kernel versions > 2.3.xx. This is due
 *	to the nature in which the new scsi code queues a new scsi command to 
 *	controller during SCSI IO Completion
 *	Driver now checks for sub-system vendor id before taking ownership of
 *	the controller
 *
 *	Version 1.14a
 *	Added Host re-ordering
 *
 *	Version 1.14b
 *	Corrected some issue which caused the older cards not to work
 *	
 *	Version 1.14c
 *	IOCTL changes for not handling the non-64bit firmwares under 2.4.XX
 *	kernel
 *
 *	Version 1.14d
 *	Fixed Various MIMD Synchronization Issues
 *	
 *	Version 1.14e
 *	Fixed the error handling during card initialization
 *
 *	Version 1.14f
 *	Multiple invocations of mimd phase I ioctl stalls the cpu. Replaced
 *	spinlock with semaphore(mutex)
 *
 *	Version 1.14g
 *	Fixed running out of scbs issues while running MIMD apps under heavy IO
 *
 *	Version 1.14g-ac - 02/03/01
 *	Reformatted to Linux format so I could compare to old one and cross
 *	check bug fixes
 *	Re fixed the assorted missing 'static' cases
 *	Removed some unneeded version checks
 *	Cleaned up some of the VERSION checks in the code
 *	Left 2.0 support but removed 2.1.x support.
 *	Collected much of the compat glue into one spot
 *
 *	Version 1.14g-ac2 - 22/03/01
 *	Fixed a non obvious dereference after free in the driver unload path
 *
 *	Version 1.14i
 *	changes for making 32bit application run on IA64
 *
 *	Version 1.14j
 *	Tue Mar 13 14:27:54 EST 2001 - AM
 *	Changes made in the driver to be able to run applications if the
 *	system has memory >4GB.
 *
 *
 *	Version 1.14k
 *	Thu Mar 15 18:38:11 EST 2001 - AM
 *
 *	Firmware version check removed if subsysid==0x1111 and
 *	subsysvid==0x1111, since its not yet initialized.
 *
 *	changes made to correctly calculate the base in mega_findCard.
 *
 *	Driver informational messages now appear on the console as well as
 *	with dmesg
 *
 *	Older ioctl interface is returned failure on newer(2.4.xx) kernels.
 *
 *	Inclusion of "modversions.h" is still a debatable question. It is
 *	included anyway with this release.
 *
 *	Version 1.14l
 *	Mon Mar 19 17:39:46 EST 2001 - AM
 *
 *	Assorted changes to remove compilation error in 1.14k when compiled
 *	with kernel < 2.4.0
 *
 *	Version 1.14m
 *	Tue Mar 27 12:09:22 EST 2001 - AM
 *
 *	Added support for extended CDBs ( > 10 bytes ) and OBDR ( One Button
 *	Disaster Recovery ) feature.
 *
 *
 *	Version 1.14n
 *	Tue Apr 10 14:28:13 EDT 2001 - AM
 *
 *	"modeversions.h" is no longer included in the code.
 *	2.4.xx style mutex initialization used for older kernels also
 *
 *	Version 1.14o
 *	Wed Apr 18 17:47:26 EDT 2001 - PJ
 *
 *	Before returning status for 'inquiry', we first check if request buffer
 *	is SG list, and then return appropriate status
 *
 *	Version 1.14p
 *	Wed Apr 25 13:44:48 EDT 2001 - PJ
 *
 *	SCSI result made appropriate in case of check conditions for extended
 *	passthru commands
 *
 *	Do not support lun >7 for physically accessed devices 
 *
 *	
 *	Version 1.15
 *	Thu Apr 19 09:38:38 EDT 2001 - AM
 *
 *	1.14l rollover to 1.15 - merged with main trunk after 1.15d
 *
 *	Version 1.15b
 *  Wed May 16 20:10:01 EDT 2001 - AM
 *
 *	"modeversions.h" is no longer included in the code.
 *	2.4.xx style mutex initialization used for older kernels also
 *	Brought in-sync with Alan's changes in 2.4.4
 *	Note: 1.15a is on OBDR branch(main trunk), and is not merged with yet.
 *
 * Version 1.15c
 * Mon May 21 23:10:42 EDT 2001 - AM
 *
 * ioctl interface uses 2.4.x conforming pci dma calls
 * similar calls used for older kernels
 *
 * Version 1.15d
 * Wed May 30 17:30:41 EDT 2001 - AM
 *
 * NULL is not a valid first argument for pci_alloc_consistent() on
 * IA64(2.4.3-2.10.1). Code shuffling done in ioctl interface to get
 * "pci_dev" before making calls to pci interface routines.
 *
 * Version 1.16pre
 * Fri Jun  1 19:40:48 EDT 2001 - AM
 *
 * 1.14p and 1.15d merged
 * ROMB support added
 *
 * Version 1.16-pre1
 * Mon Jun  4 15:01:01 EDT 2001 - AM
 *
 * Non-ROMB firmware do no DMA support 0xA9 command. Value 0xFF
 * (all channels are raid ) is chosen for those firmware.
 *
 * Version 1.16-pre2
 * Mon Jun 11 18:15:31 EDT 2001 - AM
 *
 * Changes for boot from any logical drive
 *
 * Version 1.16
 * Tue Jun 26 18:07:02 EDT 2001 - AM
 *
 * branched at 1.14p
 *
 * Check added for HP 1M/2M controllers if having firmware H.01.07 or
 * H.01.08. If found, disable 64 bit support since these firmware have
 * limitations for 64 bit addressing
 *
 *
 * Version 1.17
 * Thu Jul 12 11:14:09 EDT 2001 - AM
 *
 * 1.16pre2 and 1.16 merged.
 *
 * init_MUTEX and init_MUTEX_LOCKED are defined in 2.2.19. Pre-processor
 * statements are added for them
 *
 * Linus's 2.4.7pre3 kernel introduces a new field 'max_sectors' in Scsi_Host
 * structure, to improve IO performance.
 *
 *
 * Version 1.17a
 * Fri Jul 13 18:44:01 EDT 2001 - AM
 *
 * Starting from kernel 2.4.x, LUN is not < 8 - following SCSI-III. So to have
 * our current formula working to calculate logical drive number, return
 * failure for LUN > 7
 *
 *
 * Version 1.17b
 * Mon Jul 30 19:24:02 EDT 2001 - AM
 *
 * Added support for random deletion of logical drives
 *
 * Version 1.17c
 * Tue Sep 25 09:37:49 EDT 2001 - Atul Mukker <atulm@lsil.com>
 *
 * With single and dual channel controllers, some virtaul channels are
 * displayed negative.
 *
 * Version 1.17a-ac
 * Mon Aug 6 14:59:29 BST 2001 - "Michael Johnson" <johnsom@home.com>
 *
 * Make the HP print formatting and check for buggy firmware runtime not
 * ifdef dependant.
 *
 *
 * Version 1.17d
 * Thu Oct 11 10:48:45 EDT 2001 - Atul Mukker <atulm@lsil.com>
 *
 * Driver 1.17c oops when loaded without controller.
 *
 * Special case for "use_sg == 1" removed while building the scatter gather
 * list.
 *
 * Version 1.18
 * Thu Oct 11 15:02:53 EDT 2001 - Atul Mukker <atulm@lsil.com>
 *
 * References to AMI have been changed to LSI Logic.
 *
 * Version 1.18a
 * Mon Mar 11 11:38:38 EST 2002 - Atul Mukker <Atul.Mukker@lsil.com>
 *
 * RAID On MotherBoard (ROMB) - boot from logical or physical drives
 *
 * Support added for discovery(ROMB) vendor and device ids.
 *
 * Data transfer length for passthru commands must be valid even if the
 * command has an associated scatter-gather list.
 *
 *
 * Version 1.18b
 * Tue Apr 23 11:01:58 EDT 2002 - Atul Mukker <Atul.Mukker@lsil.com>
 *
 * typo corrected for scsi condition CHECK_CONDITION in mega_cmd_done()
 *
 * Support added for PCI_VENDOR_ID_LSI_LOGIC with device id
 * PCI_DEVICE_ID_AMI_MEGARAID3.
 *
 *
 * Version 1.18c
 * Thu May 16 10:27:55 EDT 2002 - Atul Mukker <Atul.Mukker@lsil.com>
 *
 * Retrun valid return status for mega passthru commands resulting in
 * contingent allegiance condition. Check for 64-bit passthru commands also.
 *
 * Do not check_region() anymore and check for return value of
 * request_region()
 *
 * Send valid sense data to appliations using the private ioctl interface.
 *
 *
 * Version 1.18d
 * Wed Aug  7 18:51:51 EDT 2002 - Atul Mukker <atul.mukker@lsil.com>
 *
 * Added support for yellowstone and verde controller
 *
 * Version 1.18e
 * Mon Nov 18 12:11:02 EST 2002 - Atul Mukker <atul.mukker@lsil.com>
 *
 * Don't use virt_to_bus in mega_register_mailbox when you've got the DMA
 * address already. Submitted by Jens Axboe and is included in SuSE Linux
 * Enterprise Server 7.
 *
 * s/pcibios_read_config/pci_read_config - Matt Domsch <mdomsch@dell.com>
 *
 * remove an unsed variable
 *
 * Version 1.18f
 * Tue Dec 10 09:54:39 EST 2002 - Atul Mukker <atul.mukker@lsil.com>
 *
 * remove GFP_DMA flag for ioctl. This was causing overrun of DMA buffers.
 *
 * Version 1.18g
 * Fri Jan 31 18:29:25 EST 2003 - Atul Mukker <atul.mukker@lsil.com>
 *
 * Write the interrupt valid signature 0x10001234 as soon as reading it to
 * flush memory caches.
 *
 * While sending back the inquiry information, check if the original request
 * had an associated scatter-gather list and tranfer data from bounce buffer
 * accordingly.
 *
 * Version 1.18h
 * Thu Feb  6 17:18:48 EST 2003 - Atul Mukker <atul.mukker@lsil.com>
 *
 * Reduce the number of sectors per command to 128 from original value of
 * 1024. Big IO sizes along with certain other operation going on in parallel,
 * e.g., check consistency and rebuild put a heavy constraint on fW resources
 * resulting in aborted commands.
 *
 * Version 1.18i
 * Fri Jun 20 07:39:05 EDT 2003 - Atul Mukker <atulm@lsil.com>
 *
 * Request and reserve memory/IO regions. Otherwise a panic occurs if 2.00.x
 * driver is loaded on top of 1.18x driver
 *
 * Prevent memory leak in cases when data transfer from/to application fails
 * and ioctl is failing.
 *
 * Set the PCI dma_mask to default value of 0xFFFFFFFF when we get a handle to
 * it. The previous value of 64-bit might be sticky and would cause the memory
 * for mailbox and scatter lists to be allocated beyond 4GB. This was observed
 * on an Itenium
 *
 * Version 1.18j
 * Mon Jul  7 14:39:55 EDT 2003 - Atul Mukker <atulm@lsil.com>
 *
 * Disable /proc/megaraid/stat file to prevent buffer overflow error during
 * read of this file.
 *
 * Add support for ioctls on AMD-64 bit platforms
 *			- Sreenivas Bagalkote <sreenib@lsil.com>
 *
 * Version 1.18k
 * Thu Aug 28 10:05:11 EDT 2003 - Atul Mukker <atulm@lsil.com>
 *
 * Make sure to read the correct status and command ids while in ISR. The
 * numstatus and command id array is invalidated before issuing the commands.
 * The ISR busy-waits till the correct values are updated in host memory.
 *
 * BUGS:
 *     Some older 2.1 kernels (eg. 2.1.90) have a bug in pci.c that
 *     fails to detect the controller as a pci device on the system.
 *
 *     Timeout period for upper scsi layer, i.e. SD_TIMEOUT in
 *     /drivers/scsi/sd.c, is too short for this controller. SD_TIMEOUT
 *     value must be increased to (30 * HZ) otherwise false timeouts
 *     will occur in the upper layer.
 *
 *     Never set skip_id. The existing PCI code the megaraid uses fails
 *     to properly check the vendor subid in some cases. Setting this then
 *     makes it steal other i960's and crashes some boxes
 *
 *     Far too many ifdefs for versions.
 *
 *===================================================================*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/blk.h>
#include <linux/wait.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <asm/pgtable.h>

#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/slab.h>	/* for kmalloc() */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)	/* 0x20100 */
#include <linux/bios32.h>
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)	/* 0x20300 */
#include <asm/spinlock.h>
#else
#include <linux/spinlock.h>
#endif
#endif

#include <asm/io.h>
#include <asm/irq.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,0,24)	/* 0x020024 */
#include <asm/uaccess.h>
#endif

/*
 * These header files are required for Shutdown Notification routines
 */
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>

#ifdef __x86_64__
#include <asm/ioctl32.h>
#endif

#include "sd.h"
#include "scsi.h"
#include "hosts.h"

#include "megaraid.h"

#ifdef __x86_64__
/*
 * The IOCTL cmd received from 32 bit compiled applications
 */

extern int register_ioctl32_conversion( unsigned int cmd,
				int(*handler)(unsigned int, unsigned int, unsigned long,
						struct file* ));
extern int unregister_ioctl32_conversion( unsigned int cmd );
#endif

/*
 *================================================================
 *  #Defines
 *================================================================
 */

#define MAX_SERBUF 160
#define COM_BASE 0x2f8

static ulong RDINDOOR (mega_host_config * megaCfg)
{
	return readl (megaCfg->base + 0x20);
}

static void WRINDOOR (mega_host_config * megaCfg, ulong value)
{
	writel (value, megaCfg->base + 0x20);
}

static ulong RDOUTDOOR (mega_host_config * megaCfg)
{
	return readl (megaCfg->base + 0x2C);
}

static void WROUTDOOR (mega_host_config * megaCfg, ulong value)
{
	writel (value, megaCfg->base + 0x2C);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)	/* 0x020200 */
#include <linux/smp.h>
#define cpuid smp_processor_id()
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
#define scsi_set_pci_device(x,y)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)	/* 0x020400 */

/*
 *	Linux 2.4 and higher
 *
 *	No driver private lock
 *	Use the io_request_lock not cli/sti
 *	queue task is a simple api without irq forms
 */

MODULE_AUTHOR ("LSI Logic Corporation");
MODULE_DESCRIPTION ("LSI Logic MegaRAID driver");
MODULE_LICENSE ("GPL");

#define DRIVER_LOCK_T
#define DRIVER_LOCK_INIT(p)
#define DRIVER_LOCK(p)
#define DRIVER_UNLOCK(p)
#define IO_LOCK_T unsigned long io_flags = 0
#define IO_LOCK spin_lock_irqsave(&io_request_lock,io_flags);
#define IO_UNLOCK spin_unlock_irqrestore(&io_request_lock,io_flags);

#define queue_task_irq(a,b)     queue_task(a,b)
#define queue_task_irq_off(a,b) queue_task(a,b)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)	/* 0x020200 */

/*
 *	Linux 2.2 and higher
 *
 *	No driver private lock
 *	Use the io_request_lock not cli/sti
 *	No pci region api
 *	queue_task is now a single simple API
 */

static char kernel_version[] = UTS_RELEASE;
MODULE_AUTHOR ("LSI Logic Corporation");
MODULE_DESCRIPTION ("LSI Logic MegaRAID driver");

#define DRIVER_LOCK_T
#define DRIVER_LOCK_INIT(p)
#define DRIVER_LOCK(p)
#define DRIVER_UNLOCK(p)
#define IO_LOCK_T unsigned long io_flags = 0
#define IO_LOCK spin_lock_irqsave(&io_request_lock,io_flags);
#define IO_UNLOCK spin_unlock_irqrestore(&io_request_lock,io_flags);

#define pci_free_consistent(a,b,c,d)
#define pci_unmap_single(a,b,c,d)
#define pci_enable_device(x) (0)
#define queue_task_irq(a,b)     queue_task(a,b)
#define queue_task_irq_off(a,b) queue_task(a,b)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,19)	/* 0x020219 */
#define init_MUTEX_LOCKED(x)    (*(x)=MUTEX_LOCKED)
#define init_MUTEX(x)           (*(x)=MUTEX)
#define DECLARE_WAIT_QUEUE_HEAD(x)	struct wait_queue *x = NULL
#endif


#else

/*
 *	Linux 2.0 macros. Here we have to provide some of our own
 *	functionality. We also only work little endian 32bit.
 *	Again no pci_alloc/free api
 *	IO_LOCK/IO_LOCK_T were never used in 2.0 so now are empty 
 */
 
#define cpuid 0
#define DRIVER_LOCK_T long cpu_flags;
#define DRIVER_LOCK_INIT(p)
#define DRIVER_LOCK(p) \
       		save_flags(cpu_flags); \
       		cli();
#define DRIVER_UNLOCK(p) \
       		restore_flags(cpu_flags);
#define IO_LOCK_T
#define IO_LOCK(p)
#define IO_UNLOCK(p)
#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)

#define pci_free_consistent(a,b,c,d)
#define pci_unmap_single(a,b,c,d)

#define init_MUTEX_LOCKED(x)    (*(x)=MUTEX_LOCKED)
#define init_MUTEX(x)           (*(x)=MUTEX)

#define pci_enable_device(x) (0)

/*
 *	2.0 lacks spinlocks, iounmap/ioremap
 */

#define ioremap vremap
#define iounmap vfree

 /* simulate spin locks */
typedef struct {
	volatile char lock;
} spinlock_t;

#define spin_lock_init(x) { (x)->lock = 0;}
#define spin_lock_irqsave(x,flags) { while ((x)->lock) barrier();\
                                        (x)->lock=1; save_flags(flags);\
                                        cli();}
#define spin_unlock_irqrestore(x,flags) { (x)->lock=0; restore_flags(flags);}

#define DECLARE_WAIT_QUEUE_HEAD(x)	struct wait_queue *x = NULL

#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)	/* 0x020400 */
#define dma_alloc_consistent pci_alloc_consistent
#define dma_free_consistent pci_free_consistent
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,19)	/* 0x020219 */
typedef unsigned long dma_addr_t;
#endif
void *dma_alloc_consistent(void *, size_t, dma_addr_t *);
void dma_free_consistent(void *, size_t, void *, dma_addr_t);
int mega_get_order(int);
int pow_2(int);
#endif

/* set SERDEBUG to 1 to enable serial debugging */
#define SERDEBUG 0
#if SERDEBUG
static void ser_init (void);
static void ser_puts (char *str);
static void ser_putc (char c);
static int ser_printk (const char *fmt, ...);
#endif

#ifdef CONFIG_PROC_FS
#define COPY_BACK if (offset > megaCfg->procidx) { \
		*eof = TRUE; \
        megaCfg->procidx = 0; \
        megaCfg->procbuf[0] = 0; \
        return 0;} \
 if ((count + offset) > megaCfg->procidx) { \
      count = megaCfg->procidx - offset; \
      *eof = TRUE; } \
      memcpy(page, &megaCfg->procbuf[offset], count); \
      megaCfg->procidx = 0; \
      megaCfg->procbuf[0] = 0;
#endif

/*
 * ================================================================
 *                    Global variables
 *================================================================
 */

/*  Use "megaraid=skipXX" as LILO option to prohibit driver from scanning
    XX scsi id on each channel.  Used for Madrona motherboard, where SAF_TE
    processor id cannot be scanned */

static char *megaraid;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0)	/* 0x20100 */
#ifdef MODULE
MODULE_PARM (megaraid, "s");
#endif
#endif
static int skip_id = -1;
static int numCtlrs = 0;
static mega_host_config *megaCtlrs[FC_MAX_CHANNELS] = { 0 };
static struct proc_dir_entry *mega_proc_dir_entry;

#if DEBUG
static u32 maxCmdTime = 0;
#endif

static mega_scb *pLastScb = NULL;
static struct notifier_block mega_notifier = {
	megaraid_reboot_notify,
	NULL,
	0
};

/* For controller re-ordering */
struct mega_hbas mega_hbas[MAX_CONTROLLERS];

/*
 * The File Operations structure for the serial/ioctl interface of the driver
 */
/* For controller re-ordering */ 

static struct file_operations megadev_fops = {
	ioctl:megadev_ioctl_entry,
	open:megadev_open,
	release:megadev_close,
};

/*
 * Array to structures for storing the information about the controllers. This
 * information is sent to the user level applications, when they do an ioctl
 * for this information.
 */
static struct mcontroller mcontroller[MAX_CONTROLLERS];

/* The current driver version */
static u32 driver_ver = 0x118C;

/* major number used by the device for character interface */
static int major;

static struct semaphore mimd_ioctl_sem;
static struct semaphore mimd_entry_mtx;

#if SERDEBUG
volatile static spinlock_t serial_lock;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)	/* 0x20300 */
static struct proc_dir_entry proc_scsi_megaraid = {
	PROC_SCSI_MEGARAID, 8, "megaraid",
	S_IFDIR | S_IRUGO | S_IXUGO, 2
};
#endif

#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry proc_root;
#endif

#define	IS_RAID_CH(this, ch)	( (this->mega_ch_class >> (ch)) & 0x01 )

#if SERDEBUG
static char strbuf[MAX_SERBUF + 1];

static void ser_init (void)
{
	unsigned port = COM_BASE;

	outb (0x80, port + 3);
	outb (0, port + 1);
	/* 9600 Baud, if 19200: outb(6,port) */
	outb (12, port);
	outb (3, port + 3);
	outb (0, port + 1);
}

static void ser_puts (char *str)
{
	char *ptr;

	ser_init ();
	for (ptr = str; *ptr; ++ptr)
		ser_putc (*ptr);
}

static void ser_putc (char c)
{
	unsigned port = COM_BASE;

	while ((inb (port + 5) & 0x20) == 0) ;
	outb (c, port);
	if (c == 0x0a) {
		while ((inb (port + 5) & 0x20) == 0) ;
		outb (0x0d, port);
	}
}

static int ser_printk (const char *fmt, ...)
{
	va_list args;
	int i;
	long flags;

	spin_lock_irqsave (&serial_lock, flags);
	va_start (args, fmt);
	i = vsprintf (strbuf, fmt, args);
	ser_puts (strbuf);
	va_end (args);
	spin_unlock_irqrestore (&serial_lock, flags);

	return i;
}

#define TRACE(a)    { ser_printk a;}

#else
#define TRACE(A)
#endif

#define TRACE1(a)

static void callDone (Scsi_Cmnd * SCpnt)
{
	if (SCpnt->result) {
		TRACE (("*** %.08lx %.02x <%d.%d.%d> = %x\n",
			SCpnt->serial_number, SCpnt->cmnd[0], SCpnt->channel,
			SCpnt->target, SCpnt->lun, SCpnt->result));
	}
	SCpnt->scsi_done (SCpnt);
}

/*-------------------------------------------------------------------------
 *
 *                      Local functions
 *
 *-------------------------------------------------------------------------*/

/*=======================
 * Free a SCB structure
 *=======================
 */
static void mega_freeSCB (mega_host_config * megaCfg, mega_scb * pScb)
{

	mega_scb *pScbtmp;

	if ((pScb == NULL) || (pScb->idx >= 0xFE)) {
		return;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	switch (pScb->dma_type) {
	case M_RD_DMA_TYPE_NONE:
		break;
	case M_RD_PTHRU_WITH_BULK_DATA:
		pci_unmap_single (megaCfg->dev, pScb->dma_h_bulkdata,
				  pScb->pthru->dataxferlen,
				  pScb->dma_direction);
		break;
	case M_RD_EPTHRU_WITH_BULK_DATA:
		pci_unmap_single (megaCfg->dev, pScb->dma_h_bulkdata,
				  pScb->epthru->dataxferlen,
				  pScb->dma_direction);
		break;
	case M_RD_PTHRU_WITH_SGLIST:
	{
		int count;
		for (count = 0; count < pScb->sglist_count; count++) {
			pci_unmap_single (megaCfg->dev,
					  pScb->dma_h_sglist[count],
					  pScb->sgList[count].length,
					  pScb->dma_direction);

		}
		break;
	}
	case M_RD_BULK_DATA_ONLY:
		pci_unmap_single (megaCfg->dev,
				  pScb->dma_h_bulkdata,
				  pScb->iDataSize, pScb->dma_direction);

		break;
	case M_RD_SGLIST_ONLY:
		pci_unmap_sg (megaCfg->dev,
			      pScb->SCpnt->request_buffer,
			      pScb->SCpnt->use_sg, pScb->dma_direction);
		break;
	default:
		break;
	}
#endif

	/* Unlink from pending queue */
	if (pScb == megaCfg->qPendingH) {

		if (megaCfg->qPendingH == megaCfg->qPendingT)
			megaCfg->qPendingH = megaCfg->qPendingT = NULL;
		else
			megaCfg->qPendingH = megaCfg->qPendingH->next;

		megaCfg->qPcnt--;

	} else {
		for (pScbtmp = megaCfg->qPendingH; pScbtmp;
		     pScbtmp = pScbtmp->next) {

			if (pScbtmp->next == pScb) {

				pScbtmp->next = pScb->next;

				if (pScb == megaCfg->qPendingT) {
					megaCfg->qPendingT = pScbtmp;
				}

				megaCfg->qPcnt--;
				break;
			}
		}
	}

	/* Link back into free list */
	pScb->state = SCB_FREE;
	pScb->SCpnt = NULL;

	if (megaCfg->qFreeH == (mega_scb *) NULL) {
		megaCfg->qFreeH = megaCfg->qFreeT = pScb;
	} else {
		megaCfg->qFreeT->next = pScb;
		megaCfg->qFreeT = pScb;
	}

	megaCfg->qFreeT->next = NULL;
	megaCfg->qFcnt++;

}

/*===========================
 * Allocate a SCB structure
 *===========================
 */
static mega_scb *mega_allocateSCB (mega_host_config * megaCfg, Scsi_Cmnd * SCpnt)
{
	mega_scb *pScb;

	/* Unlink command from Free List */
	if ((pScb = megaCfg->qFreeH) != NULL) {
		megaCfg->qFreeH = pScb->next;
		megaCfg->qFcnt--;

		pScb->isrcount = jiffies;
		pScb->next = NULL;
		pScb->state = SCB_ACTIVE;
		pScb->SCpnt = SCpnt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		pScb->dma_type = M_RD_DMA_TYPE_NONE;
#endif

		return pScb;
	}

	printk (KERN_WARNING "Megaraid: Could not allocate free SCB!!!\n");

	return NULL;
}

/* Run through the list of completed requests  and finish it */
static void mega_rundoneq (mega_host_config * megaCfg)
{
	Scsi_Cmnd *SCpnt;

	while ((SCpnt = megaCfg->qCompletedH) != NULL) {
		megaCfg->qCompletedH = (Scsi_Cmnd *) SCpnt->host_scribble;
		megaCfg->qCcnt--;

		SCpnt->host_scribble = (unsigned char *) NULL;	/* XC : sep 14 */
		/* Callback */
		callDone (SCpnt);
	}

	megaCfg->qCompletedH = megaCfg->qCompletedT = NULL;
}

/*
 * Runs through the list of pending requests
 * Assumes that mega_lock spin_lock has been acquired.
 */
static int mega_runpendq (mega_host_config * megaCfg)
{
	mega_scb *pScb;
	int rc;

	/* Issue any pending commands to the card */
	for (pScb = megaCfg->qPendingH; pScb; pScb = pScb->next) {
		if (pScb->state == SCB_ACTIVE) {
			if ((rc =
			     megaIssueCmd (megaCfg, pScb->mboxData, pScb, 1)) == -1)
				return rc;
		}
	}
	return 0;
}

/* Add command to the list of completed requests */

static void mega_cmd_done (mega_host_config * megaCfg, mega_scb * pScb, int status)
{
	int islogical;
	Scsi_Cmnd *SCpnt;
	mega_passthru *pthru;
	mega_ext_passthru *epthru;
	mega_mailbox *mbox;
	struct scatterlist *sgList;
	u8	c;

	if (pScb == NULL) {
		TRACE (("NULL pScb in mega_cmd_done!"));
		printk(KERN_CRIT "NULL pScb in mega_cmd_done!");
	}

	SCpnt = pScb->SCpnt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	pthru = pScb->pthru;
	epthru = pScb->epthru;
#else
	pthru = &pScb->pthru;
	epthru = &pScb->epthru;
#endif

	mbox = (mega_mailbox *) & pScb->mboxData;

	if (SCpnt == NULL) {
		TRACE (("NULL SCpnt in mega_cmd_done!"));
		TRACE (("pScb->idx = ", pScb->idx));
		TRACE (("pScb->state = ", pScb->state));
		TRACE (("pScb->state = ", pScb->state));
		panic(KERN_ERR "megaraid:Problem...!\n");
	}

#if 0
	islogical = ( (SCpnt->channel >= megaCfg->productInfo.SCSIChanPresent) &&
					(SCpnt->channel <= megaCfg->host->max_channel) );
#endif
#if 0
	islogical = (SCpnt->channel == megaCfg->host->max_channel);
#endif
	islogical = megaCfg->logdrv_chan[SCpnt->channel];

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	/* Special Case to handle PassThrough->XferAddrress > 4GB */
	switch (SCpnt->cmnd[0]) {
	case INQUIRY:
	case READ_CAPACITY:
		if ( SCpnt->use_sg ) {
			sgList = (struct scatterlist *)SCpnt->request_buffer;
			memcpy(sgList[0].address, pScb->bounce_buffer,
							SCpnt->request_bufflen);
		} else {
				memcpy (SCpnt->request_buffer, pScb->bounce_buffer,
								SCpnt->request_bufflen);
		}
		break;
	}
#endif

	mega_freeSCB (megaCfg, pScb);

	/*
	 * Do not return the presence of hard disk on the channel so, inquiry
	 * sent, and returned data==hard disk or removable hard disk and not
	 * logical, request should return failure! - PJ
	 */
#if 0
	if (SCpnt->cmnd[0] == INQUIRY && ((((u_char *) SCpnt->request_buffer)[0] & 0x1F) == TYPE_DISK) && !islogical) {
		status = 0xF0;
	}
#endif
	if (SCpnt->cmnd[0] == INQUIRY && !islogical) {
		if ( SCpnt->use_sg ) {
			sgList = (struct scatterlist *)SCpnt->request_buffer;
			memcpy(&c, sgList[0].address, 0x1);
		} else {
			memcpy(&c, SCpnt->request_buffer, 0x1);
		}
#if 0
		if( (c & 0x1F ) == TYPE_DISK ) {
			status = 0xF0;
		}
#endif
		if(IS_RAID_CH(megaCfg, SCpnt->channel) && ((c & 0x1F) == TYPE_DISK)) {
			status = 0xF0;
		}
	}


	/* clear result; otherwise, success returns corrupt value */
	SCpnt->result = 0;

	if ( 0 && SCpnt->cmnd[0] & M_RD_IOCTL_CMD ) {	/* i.e. ioctl cmd such as M_RD_IOCTL_CMD, M_RD_IOCTL_CMD_NEW of megamgr */
		switch (status) {
		case 2:
		case 0xF0:
		case 0xF4:
			SCpnt->result = (DID_BAD_TARGET << 16) | status;
			break;
		default:
			SCpnt->result |= status;
		}		/*end of switch */
	} else {
		/* Convert MegaRAID status to Linux error code */
		switch (status) {
		case 0x00:	/* SUCCESS , i.e. SCSI_STATUS_GOOD */
			SCpnt->result |= (DID_OK << 16);
			break;

		case 0x02:	/* ERROR_ABORTED, i.e. SCSI_STATUS_CHECK_CONDITION */

			/*set sense_buffer and result fields */
			if (mbox->cmd == MEGA_MBOXCMD_PASSTHRU || mbox->cmd ==
							MEGA_MBOXCMD_PASSTHRU64 ) {

				memcpy (SCpnt->sense_buffer, pthru->reqsensearea, 14);

				SCpnt->result = (DRIVER_SENSE << 24) | (DID_OK << 16) |
						(CHECK_CONDITION << 1);

			} else if (mbox->cmd == MEGA_MBOXCMD_EXTPASSTHRU) {

				memcpy( SCpnt->sense_buffer, epthru->reqsensearea, 14);

				SCpnt->result = (DRIVER_SENSE << 24) | (DID_OK << 16) |
						(CHECK_CONDITION << 1);

			} else {
				SCpnt->sense_buffer[0] = 0x70;
				SCpnt->sense_buffer[2] = ABORTED_COMMAND;
				SCpnt->result |= (CHECK_CONDITION << 1);
			}
			break;

		case 0x08:	/* ERR_DEST_DRIVE_FAILED, i.e. SCSI_STATUS_BUSY */
			SCpnt->result |= (DID_BUS_BUSY << 16) | status;
			break;

		default:
			SCpnt->result |= (DID_BAD_TARGET << 16) | status;
			break;
		}
	}

	/* Add Scsi_Command to end of completed queue */
	if (megaCfg->qCompletedH == NULL) {
		megaCfg->qCompletedH = megaCfg->qCompletedT = SCpnt;
	} else {
		megaCfg->qCompletedT->host_scribble = (unsigned char *) SCpnt;
		megaCfg->qCompletedT = SCpnt;
	}

	megaCfg->qCompletedT->host_scribble = (unsigned char *) NULL;
	megaCfg->qCcnt++;
}

/*-------------------------------------------------------------------
 *
 *                 Build a SCB from a Scsi_Cmnd
 *
 * Returns a SCB pointer, or NULL
 * If NULL is returned, the scsi_done function MUST have been called
 *
 *-------------------------------------------------------------------*/

static mega_scb *mega_build_cmd (mega_host_config * megaCfg, Scsi_Cmnd * SCpnt)
{
	mega_scb *pScb;
	mega_mailbox *mbox;
	mega_passthru *pthru;
	mega_ext_passthru *epthru;
	long seg;
	char islogical;
	int		max_ldrv_num;
	int		channel = 0;
	int		target = 0;
	int		ldrv_num = 0;	/* logical drive number */

	if ((SCpnt->cmnd[0] == MEGADEVIOC))
		return megadev_doioctl (megaCfg, SCpnt);

	if ((SCpnt->cmnd[0] == M_RD_IOCTL_CMD)
		    || (SCpnt->cmnd[0] == M_RD_IOCTL_CMD_NEW))
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)  
		return mega_ioctl (megaCfg, SCpnt);	/* Handle IOCTL command */
#else
	{
		printk(KERN_WARNING "megaraid ioctl: older interface - "
				"not supported.\n");
		return NULL;
	}
#endif

#if 0
	islogical = ( (SCpnt->channel >= megaCfg->productInfo.SCSIChanPresent) &&
					(SCpnt->channel <= megaCfg->host->max_channel) );
#endif
#if 0
	islogical = (IS_RAID_CH(SCpnt->channel) && /* virtual ch is raid - AM */
						(SCpnt->channel == megaCfg->host->max_channel));
#endif

	/*
	 * We know on what channels are our logical drives - mega_findCard()
	 */
	islogical = megaCfg->logdrv_chan[SCpnt->channel];

	/*
	 * The theory: If physical drive is chosen for boot, all the physical
	 * devices are exported before the logical drives, otherwise physical
	 * devices are pushed after logical drives, in which case - Kernel sees
	 * the physical devices on virtual channel which is obviously converted
	 * to actual channel on the HBA.
	 */
	if( megaCfg->boot_pdrv_enabled ) {
		if( islogical ) {
			/* logical channel */
			channel = SCpnt->channel - megaCfg->productInfo.SCSIChanPresent;
		}
		else {
			channel = SCpnt->channel; /* this is physical channel */
			target = SCpnt->target;

			/*
			 * boot from a physical disk, that disk needs to be exposed first
			 * IF both the channels are SCSI, then booting from the second
			 * channel is not allowed.
			 */
			if( target == 0 ) {
				target = megaCfg->boot_pdrv_tgt;
			}
			else if( target == megaCfg->boot_pdrv_tgt ) {
				target = 0;
			}
		}
	}
	else {
		if( islogical ) {
			channel = SCpnt->channel; /* this is the logical channel */
		}
		else {
			channel = SCpnt->channel - NVIRT_CHAN;	/* physical channel */
			target = SCpnt->target;
		}
	}

	if ( ! megaCfg->support_ext_cdb ) {
		if (!islogical && SCpnt->lun != 0) {
			SCpnt->result = (DID_BAD_TARGET << 16);
			callDone (SCpnt);
			return NULL;
		}
	}

	if (!islogical && SCpnt->target == skip_id) {
		SCpnt->result = (DID_BAD_TARGET << 16);
		callDone (SCpnt);
		return NULL;
	}

	if (islogical) {

		/* have just LUN 0 for each target on virtual channels */
		if( SCpnt->lun != 0 ) {
			SCpnt->result = (DID_BAD_TARGET << 16);
			callDone (SCpnt);
			return NULL;
		}

		ldrv_num = mega_get_ldrv_num(megaCfg, SCpnt, channel);

	    max_ldrv_num = (megaCfg->flag & BOARD_40LD) ?
						FC_MAX_LOGICAL_DRIVES : MAX_LOGICAL_DRIVES;

		 /*
		  * max_ldrv_num increases by 0x80 if some logical drive was deleted.
		  */
		if(megaCfg->read_ldidmap) {
			max_ldrv_num += 0x80;
		}

		if( ldrv_num > max_ldrv_num ) {
			SCpnt->result = (DID_BAD_TARGET << 16);
			callDone (SCpnt);
			return NULL;
		}

	} else {
		if ( SCpnt->lun > 7) {
				/* Do not support lun >7 for physically accessed devices */
			SCpnt->result = (DID_BAD_TARGET << 16);
			callDone (SCpnt);
			return NULL;
		}
	}
	/*-----------------------------------------------------
	 *
	 *               Logical drive commands
	 *
	 *-----------------------------------------------------*/
	if (islogical) {
		switch (SCpnt->cmnd[0]) {
		case TEST_UNIT_READY:
			memset (SCpnt->request_buffer, 0, SCpnt->request_bufflen);
			SCpnt->result = (DID_OK << 16);
			callDone (SCpnt);
			return NULL;

		case MODE_SENSE:
			memset (SCpnt->request_buffer, 0, SCpnt->cmnd[4]);
			SCpnt->result = (DID_OK << 16);
			callDone (SCpnt);
			return NULL;

		case READ_CAPACITY:
		case INQUIRY:
			if(!(megaCfg->flag & (1L << SCpnt->channel))) {
				printk(KERN_NOTICE
					"scsi%d: scanning virtual channel %d for logical drives.\n",
					megaCfg->host->host_no, channel);

				megaCfg->flag |= (1L << SCpnt->channel);
			}

			/* Allocate a SCB and initialize passthru */
			if ((pScb = mega_allocateSCB (megaCfg, SCpnt)) == NULL) {
				SCpnt->result = (DID_ERROR << 16);
				callDone (SCpnt);
				return NULL;
			}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			pthru = pScb->pthru;
#else
			pthru = &pScb->pthru;
#endif

			mbox = (mega_mailbox *) & pScb->mboxData;
			memset (mbox, 0, sizeof (pScb->mboxData));
			memset (pthru, 0, sizeof (mega_passthru));
			pthru->timeout = 0;
			pthru->ars = 1;
			pthru->reqsenselen = 14;
			pthru->islogical = 1;
			pthru->logdrv = ldrv_num;
			pthru->cdblen = SCpnt->cmd_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			/*Not sure about the direction */
			pScb->dma_direction = PCI_DMA_BIDIRECTIONAL;
			pScb->dma_type = M_RD_PTHRU_WITH_BULK_DATA;

#if 0
/* Normal Code w/o the need for bounce buffer */
			pScb->dma_h_bulkdata
			    = pci_map_single (megaCfg->dev,
					      SCpnt->request_buffer,
					      SCpnt->request_bufflen,
					      pScb->dma_direction);

			pthru->dataxferaddr = pScb->dma_h_bulkdata;
#else
/* Special Code to use bounce buffer for READ_CAPA/INQ */
			pthru->dataxferaddr = pScb->dma_bounce_buffer;
			pScb->dma_type = M_RD_DMA_TYPE_NONE;
#endif

#else
			pthru->dataxferaddr =
			    virt_to_bus (SCpnt->request_buffer);
#endif

			pthru->dataxferlen = SCpnt->request_bufflen;
			memcpy (pthru->cdb, SCpnt->cmnd, SCpnt->cmd_len);

			/* Initialize mailbox area */
			mbox->cmd = MEGA_MBOXCMD_PASSTHRU;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			mbox->xferaddr = pScb->dma_passthruhandle64;
			TRACE1 (("M_RD_PTHRU_WITH_BULK_DATA Enabled \n"));
#else
			mbox->xferaddr = virt_to_bus (pthru);
#endif
			return pScb;

		case READ_6:
		case WRITE_6:
		case READ_10:
		case WRITE_10:
			/* Allocate a SCB and initialize mailbox */
			if ((pScb = mega_allocateSCB (megaCfg, SCpnt)) == NULL) {
				SCpnt->result = (DID_ERROR << 16);
				callDone (SCpnt);
				return NULL;
			}
			mbox = (mega_mailbox *) & pScb->mboxData;

			memset (mbox, 0, sizeof (pScb->mboxData));
			mbox->logdrv = ldrv_num;

			if (megaCfg->flag & BOARD_64BIT) {
				mbox->cmd = (*SCpnt->cmnd == READ_6
					     || *SCpnt->cmnd ==
					     READ_10) ? MEGA_MBOXCMD_LREAD64 :
				    MEGA_MBOXCMD_LWRITE64;
			} else {
				mbox->cmd = (*SCpnt->cmnd == READ_6
					     || *SCpnt->cmnd ==
					     READ_10) ? MEGA_MBOXCMD_LREAD :
				    MEGA_MBOXCMD_LWRITE;
			}

			/* 6-byte */
			if (*SCpnt->cmnd == READ_6 || *SCpnt->cmnd == WRITE_6) {
				mbox->numsectors = (u32) SCpnt->cmnd[4];
				mbox->lba =
				    ((u32) SCpnt->cmnd[1] << 16) |
				    ((u32) SCpnt->cmnd[2] << 8) |
				    (u32) SCpnt->cmnd[3];
				mbox->lba &= 0x1FFFFF;

				if (*SCpnt->cmnd == READ_6) {
					megaCfg->nReads[(int)ldrv_num]++;
					megaCfg->nReadBlocks[(int)ldrv_num] +=
					    mbox->numsectors;
				} else {
					megaCfg->nWrites[(int)ldrv_num]++;
					megaCfg->nWriteBlocks[(int)ldrv_num] +=
					    mbox->numsectors;
				}
			}

			/* 10-byte */
			if (*SCpnt->cmnd == READ_10 || *SCpnt->cmnd == WRITE_10) {
				mbox->numsectors =
				    (u32) SCpnt->cmnd[8] |
				    ((u32) SCpnt->cmnd[7] << 8);
				mbox->lba =
				    ((u32) SCpnt->cmnd[2] << 24) |
				    ((u32) SCpnt->cmnd[3] << 16) |
				    ((u32) SCpnt->cmnd[4] << 8) |
				    (u32) SCpnt->cmnd[5];

				if (*SCpnt->cmnd == READ_10) {
					megaCfg->nReads[(int)ldrv_num]++;
					megaCfg->nReadBlocks[(int)ldrv_num] +=
					    mbox->numsectors;
				} else {
					megaCfg->nWrites[(int)ldrv_num]++;
					megaCfg->nWriteBlocks[(int)ldrv_num] +=
					    mbox->numsectors;
				}
			}

			/* 12-byte */
			if (*SCpnt->cmnd == READ_12 || *SCpnt->cmnd == WRITE_12) {
				mbox->lba =
				    ((u32) SCpnt->cmnd[2] << 24) |
				    ((u32) SCpnt->cmnd[3] << 16) |
				    ((u32) SCpnt->cmnd[4] << 8) |
				    (u32) SCpnt->cmnd[5];

				mbox->numsectors =
				    ((u32) SCpnt->cmnd[6] << 24) |
				    ((u32) SCpnt->cmnd[7] << 16) |
				    ((u32) SCpnt->cmnd[8] << 8) |
				    (u32) SCpnt->cmnd[9];

				if (*SCpnt->cmnd == READ_12) {
					megaCfg->nReads[(int)ldrv_num]++;
					megaCfg->nReadBlocks[(int)ldrv_num] +=
					    mbox->numsectors;
				} else {
					megaCfg->nWrites[(int)ldrv_num]++;
					megaCfg->nWriteBlocks[(int)ldrv_num] +=
					    mbox->numsectors;
				}
			}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			if (*SCpnt->cmnd == READ_6 || *SCpnt->cmnd == READ_10
					|| *SCpnt->cmnd == READ_12) {
				pScb->dma_direction = PCI_DMA_FROMDEVICE;
			} else {	/*WRITE_6 or WRITE_10 */
				pScb->dma_direction = PCI_DMA_TODEVICE;
			}
#endif

			/* Calculate Scatter-Gather info */
			mbox->numsgelements = mega_build_sglist (megaCfg, pScb,
								 (u32 *)&mbox->xferaddr, (u32 *)&seg);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			pScb->iDataSize = seg;

			if (mbox->numsgelements) {
				pScb->dma_type = M_RD_SGLIST_ONLY;
				TRACE1 (("M_RD_SGLIST_ONLY Enabled \n"));
			} else {
				pScb->dma_type = M_RD_BULK_DATA_ONLY;
				TRACE1 (("M_RD_BULK_DATA_ONLY Enabled \n"));
			}
#endif

			return pScb;
		default:
			SCpnt->result = (DID_BAD_TARGET << 16);
			callDone (SCpnt);
			return NULL;
		}
	}
	/*-----------------------------------------------------
	 *
	 *               Passthru drive commands
	 *
	 *-----------------------------------------------------*/
	else {
		/* Allocate a SCB and initialize passthru */
		if ((pScb = mega_allocateSCB (megaCfg, SCpnt)) == NULL) {
			SCpnt->result = (DID_ERROR << 16);
			callDone (SCpnt);
			return NULL;
		}

		mbox = (mega_mailbox *) pScb->mboxData;
		memset (mbox, 0, sizeof (pScb->mboxData));

		if ( megaCfg->support_ext_cdb && SCpnt->cmd_len > 10 ) {
			epthru = mega_prepare_extpassthru(megaCfg, pScb, SCpnt, channel,
					target);
			mbox->cmd = MEGA_MBOXCMD_EXTPASSTHRU;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			mbox->xferaddr = pScb->dma_ext_passthruhandle64;

			if(epthru->numsgelements) {
				pScb->dma_type = M_RD_PTHRU_WITH_SGLIST;
			} else {
				pScb->dma_type = M_RD_EPTHRU_WITH_BULK_DATA;
			}
#else
			mbox->xferaddr = virt_to_bus(epthru);
#endif
		}
		else {
			pthru = mega_prepare_passthru(megaCfg, pScb, SCpnt, channel,
					target);

			/* Initialize mailbox */
			mbox->cmd = MEGA_MBOXCMD_PASSTHRU;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			mbox->xferaddr = pScb->dma_passthruhandle64;

			if (pthru->numsgelements) {
				pScb->dma_type = M_RD_PTHRU_WITH_SGLIST;
			} else {
				pScb->dma_type = M_RD_PTHRU_WITH_BULK_DATA;
			}
#else
			mbox->xferaddr = virt_to_bus(pthru);
#endif
		}
		return pScb;
	}
	return NULL;
}

static int
mega_get_ldrv_num(mega_host_config *this_hba, Scsi_Cmnd *sc, int channel)
{
	int		tgt;
	int		ldrv_num;

	tgt = sc->target;
	
	if ( tgt > 7 ) tgt--;	/* we do not get inquires for tgt 7 */

	ldrv_num = (channel * 15) + tgt; /* 14 targets per channel */

	/*
	 * If we have a logical drive with boot enabled, project it first
	 */
	if( this_hba->boot_ldrv_enabled ) {
		if( ldrv_num == 0 ) {
			ldrv_num = this_hba->boot_ldrv;
		}
		else {
			if( ldrv_num <= this_hba->boot_ldrv ) {
				ldrv_num--;
			}
		}
	}

	/*
	 * If "delete logical drive" feature is enabled on this controller.
	 * Do only if at least one delete logical drive operation was done.
	 *
	 * Also, after logical drive deletion, instead of logical drive number,
	 * the value returned should be 0x80+logical drive id.
	 *
	 * These is valid only for IO commands.
	 */

	 if( this_hba->support_random_del && this_hba->read_ldidmap ) {
		switch(sc->cmnd[0]) {
		case READ_6:	/* fall through */
		case WRITE_6:	/* fall through */
		case READ_10:	/* fall through */
		case WRITE_10:
			ldrv_num += 0x80;
		}
	 }

	 return ldrv_num;
}


static mega_passthru *
mega_prepare_passthru(mega_host_config *megacfg, mega_scb *scb, Scsi_Cmnd *sc,
		int channel, int target)
{
	mega_passthru *pthru;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	pthru = scb->pthru;
#else
	pthru = &scb->pthru;
#endif
	memset (pthru, 0, sizeof (mega_passthru));

	/* set adapter timeout value to 10 min. for tape drive	*/
	/* 0=6sec/1=60sec/2=10min/3=3hrs 			*/
	pthru->timeout = 2;
	pthru->ars = 1;
	pthru->reqsenselen = 14;
	pthru->islogical = 0;
	pthru->channel = (megacfg->flag & BOARD_40LD) ? 0 : channel;
	pthru->target = (megacfg->flag & BOARD_40LD) ?
	    (channel << 4) | target : target;
	pthru->cdblen = sc->cmd_len;
	pthru->logdrv = sc->lun;

	memcpy (pthru->cdb, sc->cmnd, sc->cmd_len);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	/* Not sure about the direction */
	scb->dma_direction = PCI_DMA_BIDIRECTIONAL;

	/* Special Code for Handling READ_CAPA/ INQ using bounce buffers */
	switch (sc->cmnd[0]) {
	case INQUIRY:
	case READ_CAPACITY:

		if(!(megacfg->flag & (1L << sc->channel))) {
			printk(KERN_NOTICE
				"scsi%d: scanning physical channel %d for devices.\n",
				megacfg->host->host_no, channel);

			megacfg->flag |= (1L << sc->channel);
		}

		pthru->numsgelements = 0;
		pthru->dataxferaddr = scb->dma_bounce_buffer;
		pthru->dataxferlen = sc->request_bufflen;
		break;
	default:
		pthru->numsgelements =
			mega_build_sglist(
				megacfg, scb, (u32 *)&pthru->dataxferaddr,
				(u32 *)&pthru->dataxferlen
			);
		break;
	}
#else
	pthru->numsgelements =
		mega_build_sglist(
			megacfg, scb, (u32 *)&pthru->dataxferaddr,
			(u32 *)&pthru->dataxferlen
		);
#endif
	return pthru;
}

static mega_ext_passthru *
mega_prepare_extpassthru(mega_host_config *megacfg, mega_scb *scb,
		Scsi_Cmnd *sc, int channel, int target)
{
	mega_ext_passthru *epthru;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	epthru = scb->epthru;
#else
	epthru = &scb->epthru;
#endif
	memset(epthru, 0, sizeof(mega_ext_passthru));

	/* set adapter timeout value to 10 min. for tape drive	*/
	/* 0=6sec/1=60sec/2=10min/3=3hrs 			*/
	epthru->timeout = 2;
	epthru->ars = 1;
	epthru->reqsenselen = 14;
	epthru->islogical = 0;
	epthru->channel = (megacfg->flag & BOARD_40LD) ? 0 : channel;
	epthru->target = (megacfg->flag & BOARD_40LD) ?
	    (channel << 4) | target : target;
	epthru->cdblen = sc->cmd_len;
	epthru->logdrv = sc->lun;

	memcpy(epthru->cdb, sc->cmnd, sc->cmd_len);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	/* Not sure about the direction */
	scb->dma_direction = PCI_DMA_BIDIRECTIONAL;

	/* Special Code for Handling READ_CAPA/ INQ using bounce buffers */
	switch (sc->cmnd[0]) {
	case INQUIRY:
	case READ_CAPACITY:
		if(!(megacfg->flag & (1L << sc->channel))) {
			printk(KERN_NOTICE
				"scsi%d: scanning physical channel %d for devices.\n",
				megacfg->host->host_no, channel);

			megacfg->flag |= (1L << sc->channel);
		}

		epthru->numsgelements = 0;
		epthru->dataxferaddr = scb->dma_bounce_buffer;
		epthru->dataxferlen = sc->request_bufflen;
		break;
	default:
		epthru->numsgelements =
			mega_build_sglist(
				megacfg, scb, (u32 *)&epthru->dataxferaddr,
				(u32 *)&epthru->dataxferlen
			);
		break;
	}
#else
	epthru->numsgelements =
		mega_build_sglist(
			megacfg, scb, (u32 *)&epthru->dataxferaddr,
			(u32 *)&epthru->dataxferlen
		);
#endif
	return epthru;
}

/* Handle Driver Level IOCTLs
 * Return value of 0 indicates this function could not handle , so continue
 * processing
*/

static int mega_driver_ioctl (mega_host_config * megaCfg, Scsi_Cmnd * SCpnt)
{
	unsigned char *data = (unsigned char *) SCpnt->request_buffer;
	mega_driver_info driver_info;

	/* If this is not our command dont do anything */
	if (SCpnt->cmnd[0] != M_RD_DRIVER_IOCTL_INTERFACE)
		return 0;

	switch (SCpnt->cmnd[1]) {
	case GET_DRIVER_INFO:
		if (SCpnt->request_bufflen < sizeof (driver_info)) {
			SCpnt->result = DID_BAD_TARGET << 16;
			callDone (SCpnt);
			return 1;
		}

		driver_info.size = sizeof (driver_info) - sizeof (int);
		driver_info.version = MEGARAID_IOCTL_VERSION;
		memcpy (data, &driver_info, sizeof (driver_info));
		break;
	default:
		SCpnt->result = DID_BAD_TARGET << 16;
	}

	callDone (SCpnt);
	return 1;
}

static void inline set_mbox_xfer_addr (mega_host_config * megaCfg, mega_scb * pScb,
		    mega_ioctl_mbox * mbox, u32 direction)
{

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	switch (direction) {
	case TO_DEVICE:
		pScb->dma_direction = PCI_DMA_TODEVICE;
		break;
	case FROM_DEVICE:
		pScb->dma_direction = PCI_DMA_FROMDEVICE;
		break;
	case FROMTO_DEVICE:
		pScb->dma_direction = PCI_DMA_BIDIRECTIONAL;
		break;
	}

	pScb->dma_h_bulkdata
	    = pci_map_single (megaCfg->dev,
			      pScb->buff_ptr,
			      pScb->iDataSize, pScb->dma_direction);
	mbox->xferaddr = pScb->dma_h_bulkdata;
	pScb->dma_type = M_RD_BULK_DATA_ONLY;
	TRACE1 (("M_RD_BULK_DATA_ONLY Enabled \n"));
#else
	mbox->xferaddr = virt_to_bus (pScb->buff_ptr);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

/*--------------------------------------------------------------------
 * build RAID commands for controller, passed down through ioctl()
 *--------------------------------------------------------------------*/
static mega_scb *mega_ioctl (mega_host_config * megaCfg, Scsi_Cmnd * SCpnt)
{
	mega_scb *pScb;
	mega_ioctl_mbox *mbox;
	mega_mailbox *mailbox;
	mega_passthru *pthru;
	u8 *mboxdata;
	long seg, i = 0;
	unsigned char *data = (unsigned char *) SCpnt->request_buffer;

	if ((pScb = mega_allocateSCB (megaCfg, SCpnt)) == NULL) {
		SCpnt->result = (DID_ERROR << 16);
		callDone (SCpnt);
		return NULL;
	}
	pthru = &pScb->pthru;

	mboxdata = (u8 *) & pScb->mboxData;
	mbox = (mega_ioctl_mbox *) & pScb->mboxData;
	mailbox = (mega_mailbox *) & pScb->mboxData;
	memset (mailbox, 0, sizeof (pScb->mboxData));

	if (data[0] == 0x03) {	/* passthrough command */
		unsigned char cdblen = data[2];
		memset (pthru, 0, sizeof (mega_passthru));
		pthru->islogical = (data[cdblen + 3] & 0x80) ? 1 : 0;
		pthru->timeout = data[cdblen + 3] & 0x07;
		pthru->reqsenselen = 14;
		pthru->ars = (data[cdblen + 3] & 0x08) ? 1 : 0;
		pthru->logdrv = data[cdblen + 4];
		pthru->channel = data[cdblen + 5];
		pthru->target = data[cdblen + 6];
		pthru->cdblen = cdblen;
		memcpy (pthru->cdb, &data[3], cdblen);

		mailbox->cmd = MEGA_MBOXCMD_PASSTHRU;


		pthru->numsgelements = mega_build_sglist (megaCfg, pScb,
							  (u32 *) & pthru->
							  dataxferaddr,
							  (u32 *) & pthru->
							  dataxferlen);

		mailbox->xferaddr = virt_to_bus (pthru);

		for (i = 0; i < (SCpnt->request_bufflen - cdblen - 7); i++) {
			data[i] = data[i + cdblen + 7];
		}
		return pScb;
	}
	/* else normal (nonpassthru) command */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,0,24)	/*0x020024 */
	/*
	 *usage of the function copy from user is used in case of data more than
	 *4KB.This is used only with adapters which supports more than 8 logical
	 * drives.This feature is disabled on kernels earlier or same as 2.0.36
	 * as the uaccess.h file is not available with those kernels.
	 */

	if (SCpnt->cmnd[0] == M_RD_IOCTL_CMD_NEW) {
		/* use external data area for large xfers  */
		/* If cmnd[0] is set to M_RD_IOCTL_CMD_NEW then *
		 *   cmnd[4..7] = external user buffer     *
		 *   cmnd[8..11] = length of buffer        *
		 *                                         */
      	char *user_area = (char *)*((u32*)&SCpnt->cmnd[4]);
		u32 xfer_size = *((u32 *) & SCpnt->cmnd[8]);
		switch (data[0]) {
		case FW_FIRE_WRITE:
		case FW_FIRE_FLASH:
			if ((ulong) user_area & (PAGE_SIZE - 1)) {
				printk
				    ("megaraid:user address not aligned on 4K boundary.Error.\n");
				SCpnt->result = (DID_ERROR << 16);
				callDone (SCpnt);
				return NULL;
			}
			break;
		default:
			break;
		}

		if (!(pScb->buff_ptr = kmalloc (xfer_size, GFP_KERNEL))) {
			printk
			    ("megaraid: Insufficient mem for M_RD_IOCTL_CMD_NEW.\n");
			SCpnt->result = (DID_ERROR << 16);
			callDone (SCpnt);
			return NULL;
		}

		copy_from_user (pScb->buff_ptr, user_area, xfer_size);
		pScb->iDataSize = xfer_size;

		switch (data[0]) {
		case DCMD_FC_CMD:
			switch (data[1]) {
			case DCMD_FC_READ_NVRAM_CONFIG:
			case DCMD_GET_DISK_CONFIG:
				{
					if ((ulong) pScb->
					    buff_ptr & (PAGE_SIZE - 1)) {
						printk
						    ("megaraid:user address not sufficient Error.\n");
						SCpnt->result =
						    (DID_ERROR << 16);
						callDone (SCpnt);
						return NULL;
					}

					/*building SG list */
					mega_build_kernel_sg (pScb->buff_ptr,
							      xfer_size,
							      pScb, mbox);
					break;
				}
			default:
				break;
			}	/*switch (data[1]) */
			break;
		}

	}
#endif

	mbox->cmd = data[0];
	mbox->channel = data[1];
	mbox->param = data[2];
	mbox->pad[0] = data[3];
	mbox->logdrv = data[4];

	if (SCpnt->cmnd[0] == M_RD_IOCTL_CMD_NEW) {
		switch (data[0]) {
		case FW_FIRE_WRITE:
			mbox->cmd = FW_FIRE_WRITE;
			mbox->channel = data[1];	/* Current Block Number */
			set_mbox_xfer_addr (megaCfg, pScb, mbox, TO_DEVICE);
			mbox->numsgelements = 0;
			break;
		case FW_FIRE_FLASH:
			mbox->cmd = FW_FIRE_FLASH;
			mbox->channel = data[1] | 0x80;	/* Origin */
			set_mbox_xfer_addr (megaCfg, pScb, mbox, TO_DEVICE);
			mbox->numsgelements = 0;
			break;
		case DCMD_FC_CMD:
			*(mboxdata + 0) = data[0];	/*mailbox byte 0: DCMD_FC_CMD */
			*(mboxdata + 2) = data[1];	/*sub command */
			switch (data[1]) {
			case DCMD_FC_READ_NVRAM_CONFIG:
			case DCMD_FC_READ_NVRAM_CONFIG_64:
				/* number of elements in SG list */
				*(mboxdata + 3) = mbox->numsgelements;
				if (megaCfg->flag & BOARD_64BIT)
					*(mboxdata + 2) =
					    DCMD_FC_READ_NVRAM_CONFIG_64;
				break;
			case DCMD_WRITE_CONFIG:
			case DCMD_WRITE_CONFIG_64:
				if (megaCfg->flag & BOARD_64BIT)
					*(mboxdata + 2) = DCMD_WRITE_CONFIG_64;
				set_mbox_xfer_addr (megaCfg, pScb, mbox,
						    TO_DEVICE);
				mbox->numsgelements = 0;
				break;
			case DCMD_GET_DISK_CONFIG:
			case DCMD_GET_DISK_CONFIG_64:
				if (megaCfg->flag & BOARD_64BIT)
					*(mboxdata + 2) =
					    DCMD_GET_DISK_CONFIG_64;
				*(mboxdata + 3) = data[2];	/*number of elements in SG list */
				/*nr of elements in SG list */
				*(mboxdata + 4) = mbox->numsgelements;
				break;
			case DCMD_DELETE_LOGDRV:
			case DCMD_DELETE_DRIVEGROUP:
			case NC_SUBOP_ENQUIRY3:
				*(mboxdata + 3) = data[2];
				set_mbox_xfer_addr (megaCfg, pScb, mbox,
						    FROMTO_DEVICE);
				mbox->numsgelements = 0;
				break;
			case DCMD_CHANGE_LDNO:
			case DCMD_CHANGE_LOOPID:
				*(mboxdata + 3) = data[2];
				*(mboxdata + 4) = data[3];
				set_mbox_xfer_addr (megaCfg, pScb, mbox,
						    TO_DEVICE);
				mbox->numsgelements = 0;
				break;
			default:
				set_mbox_xfer_addr (megaCfg, pScb, mbox,
						    FROMTO_DEVICE);
				mbox->numsgelements = 0;
				break;
			}	/*switch */
			break;
		default:
			set_mbox_xfer_addr (megaCfg, pScb, mbox, FROMTO_DEVICE);
			mbox->numsgelements = 0;
			break;
		}
	} else {

		mbox->numsgelements = mega_build_sglist (megaCfg, pScb,
							 (u32 *) & mbox->
							 xferaddr,
							 (u32 *) & seg);

		/* Handling some of the fw special commands */
		switch (data[0]) {
		case 6:	/* START_DEV */
			mbox->xferaddr = *((u32 *) & data[i + 6]);
			break;
		default:
			break;
		}

		for (i = 0; i < (SCpnt->request_bufflen - 6); i++) {
			data[i] = data[i + 6];
		}
	}

	return (pScb);
}


static void mega_build_kernel_sg (char *barea, ulong xfersize, mega_scb * pScb, mega_ioctl_mbox * mbox)
{
	ulong i, buffer_area, len, end, end_page, x, idx = 0;

	buffer_area = (ulong) barea;
	i = buffer_area;
	end = buffer_area + xfersize;
	end_page = (end) & ~(PAGE_SIZE - 1);

	do {
		len = PAGE_SIZE - (i % PAGE_SIZE);
		x = pScb->sgList[idx].address =
		    virt_to_bus ((volatile void *) i);
		pScb->sgList[idx].length = len;
		i += len;
		idx++;
	} while (i < end_page);

	if ((end - i) < 0) {
		printk ("megaraid:Error in user address\n");
	}

	if (end - i) {
		pScb->sgList[idx].address = virt_to_bus ((volatile void *) i);
		pScb->sgList[idx].length = end - i;
		idx++;
	}
	mbox->xferaddr = virt_to_bus (pScb->sgList);
	mbox->numsgelements = idx;
}
#endif


#if DEBUG
static unsigned int cum_time = 0;
static unsigned int cum_time_cnt = 0;

static void showMbox (mega_scb * pScb)
{
	mega_mailbox *mbox;

	if (pScb == NULL)
		return;

	mbox = (mega_mailbox *) pScb->mboxData;
	printk ("%u cmd:%x id:%x #scts:%x lba:%x addr:%x logdrv:%x #sg:%x\n",
		pScb->SCpnt->pid,
		mbox->cmd, mbox->cmdid, mbox->numsectors,
		mbox->lba, mbox->xferaddr, mbox->logdrv, mbox->numsgelements);
}

#endif

/*--------------------------------------------------------------------
 * Interrupt service routine
 *--------------------------------------------------------------------*/
static void megaraid_isr (int irq, void *devp, struct pt_regs *regs)
{
	IO_LOCK_T;
	mega_host_config * megaCfg;
	u_char byte, idx, sIdx, tmpBox[MAILBOX_SIZE];
	u32 dword = 0;
	mega_mailbox *mbox;
	mega_scb *pScb;
	u_char qCnt, qStatus;
	u_char completed[MAX_FIRMWARE_STATUS];
	Scsi_Cmnd *SCpnt;

	megaCfg = (mega_host_config *) devp;
	mbox = (mega_mailbox *) tmpBox;

		IO_LOCK;

		/* Check if a valid interrupt is pending */
		if (megaCfg->flag & BOARD_QUARTZ) {
			dword = RDOUTDOOR (megaCfg);
			if (dword != 0x10001234) {
				/* Spurious interrupt */
				IO_UNLOCK;
				return;
			}
			WROUTDOOR (megaCfg, 0x10001234);
		} else {
			byte = READ_PORT (megaCfg->host->io_port, INTR_PORT);
			if ((byte & VALID_INTR_BYTE) == 0) {
				/* Spurious interrupt */
				IO_UNLOCK;
				return;
			}
			WRITE_PORT (megaCfg->host->io_port, INTR_PORT, byte);
		}

		for (idx = 0; idx < MAX_FIRMWARE_STATUS; idx++)
			completed[idx] = 0;


		megaCfg->nInterrupts++;
		while ((qCnt = megaCfg->mbox->numstatus) == 0xFF) ;
		megaCfg->mbox->numstatus = 0xFF;

		/* Get list of completed requests */
		for (idx = 0; idx < qCnt; idx++) {
			while ((completed[idx] = megaCfg->mbox->completed[idx]) == 0xFF);
			megaCfg->mbox->completed[idx] = 0xFF;
		}

		qStatus = megaCfg->mbox->status;

		if (megaCfg->flag & BOARD_QUARTZ) {
			/* Acknowledge interrupt */
			WRINDOOR (megaCfg, 0x2);
			while (RDINDOOR (megaCfg) & 0x02) ;
		} else {
			CLEAR_INTR (megaCfg->host->io_port);
		}

		for (idx = 0; idx < qCnt; idx++) {
			sIdx = completed[idx];
			if ((sIdx > 0) && (sIdx <= MAX_COMMANDS)) {
				pScb = &megaCfg->scbList[sIdx - 1];

				/* ASSERT(pScb->state == SCB_ISSUED); */

#if DEBUG
				if (((jiffies) - pScb->isrcount) > maxCmdTime) {
					maxCmdTime = (jiffies) - pScb->isrcount;
					printk
					    ("megaraid_isr : cmd time = %u\n",
					     maxCmdTime);
				}
#endif
				/*
				 * Assuming that the scsi command, for which 
				 * an abort request was received earlier, has 
				 * completed.
				 */
				if (pScb->state == SCB_ABORTED) {
					SCpnt = pScb->SCpnt;
				}
				if (pScb->state == SCB_RESET) {
					SCpnt = pScb->SCpnt;
					mega_freeSCB (megaCfg, pScb);
					SCpnt->result = (DID_RESET << 16);
					if (megaCfg->qCompletedH == NULL) {
						megaCfg->qCompletedH =
						    megaCfg->qCompletedT =
						    SCpnt;
					} else {
						megaCfg->qCompletedT->
						    host_scribble =
						    (unsigned char *) SCpnt;
						megaCfg->qCompletedT = SCpnt;
					}
					megaCfg->qCompletedT->host_scribble =
					    (unsigned char *) NULL;
					megaCfg->qCcnt++;
					continue;
				}

				/* We don't want the ISR routine to touch M_RD_IOCTL_CMD_NEW commands, so
				 * don't mark them as complete, instead we pop their semaphore so
				 * that the queue routine can finish them off
				 */
				if (pScb->SCpnt->cmnd[0] == M_RD_IOCTL_CMD_NEW) {
					/* save the status byte for the queue routine to use */
					pScb->SCpnt->result = qStatus;
					up (&pScb->ioctl_sem);
				} else {
					/* Mark command as completed */
					mega_cmd_done (megaCfg, pScb, qStatus);
				}
			} else {
				printk
				    ("megaraid: wrong cmd id completed from firmware:id=%x\n",
				     sIdx);
			}
		}

		mega_rundoneq (megaCfg);

		megaCfg->flag &= ~IN_ISR;
		/* Loop through any pending requests */
		mega_runpendq (megaCfg);
		IO_UNLOCK;

}

/*==================================================*/
/* Wait until the controller's mailbox is available */
/*==================================================*/

static inline int mega_busyWaitMbox (mega_host_config * megaCfg)
{
	mega_mailbox *mbox = (mega_mailbox *) megaCfg->mbox;
	long counter;

	for (counter = 0; counter < 10; counter++) {
		if (!mbox->busy) {
			return 0;
		}
		udelay (1);
	}
	return -1;		/* give up after 10 usecs */
}

/*=====================================================
 * Post a command to the card
 *
 * Arguments:
 *   mega_host_config *megaCfg - Controller structure
 *   u_char *mboxData - Mailbox area, 16 bytes
 *   mega_scb *pScb   - SCB posting (or NULL if N/A)
 *   int intr         - if 1, interrupt, 0 is blocking
 * Return Value: (added on 7/26 for 40ld/64bit)
 *   -1: the command was not actually issued out
 *   other cases:
 *     intr==0, return ScsiStatus, i.e. mbox->status
 *     intr==1, return 0
 *=====================================================
 */
static int megaIssueCmd (mega_host_config * megaCfg, u_char * mboxData, 
		mega_scb * pScb, int intr)
{
	volatile mega_mailbox *mbox = (mega_mailbox *) megaCfg->mbox;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	volatile mega_mailbox64 *mbox64 = (mega_mailbox64 *) megaCfg->mbox64;
#endif

	u_char byte;

#if BITS_PER_LONG==64
	u64 phys_mbox;
#else
	u32 phys_mbox;
#endif
	u8 retval = -1;
	int	i;

	mboxData[0x1] = (pScb ? pScb->idx + 1 : 0xFE);	/* Set cmdid */
	mboxData[0xF] = 1;	/* Set busy */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	/* In this case mbox contains physical address */
	phys_mbox = megaCfg->adjdmahandle64;
#else
	phys_mbox = virt_to_bus (megaCfg->mbox);
#endif

	/* Wait until mailbox is free */
	if (mega_busyWaitMbox (megaCfg)) {
		return -1;
	}

	pLastScb = pScb;

	/* Copy mailbox data into host structure */
	megaCfg->mbox64->xferSegment_lo = 0;
	megaCfg->mbox64->xferSegment_hi = 0;

	memcpy ((char *) mbox, mboxData, 16);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	switch (mboxData[0]) {
	case MEGA_MBOXCMD_LREAD64:
	case MEGA_MBOXCMD_LWRITE64:
		mbox64->xferSegment_lo = mbox->xferaddr;
		mbox64->xferSegment_hi = 0;
		mbox->xferaddr = 0xFFFFFFFF;
		break;
	}
#endif

	/* Kick IO */
	if (intr) {
		/* Issue interrupt (non-blocking) command */
		if (megaCfg->flag & BOARD_QUARTZ) {
			mbox->mraid_poll = 0;
			mbox->mraid_ack = 0;

			WRINDOOR (megaCfg, phys_mbox | 0x1);
		} else {
			ENABLE_INTR (megaCfg->host->io_port);
			ISSUE_COMMAND (megaCfg->host->io_port);
		}
		pScb->state = SCB_ISSUED;

		retval = 0;
	} else {		/* Issue non-ISR (blocking) command */
		disable_irq (megaCfg->host->irq);
		if (megaCfg->flag & BOARD_QUARTZ) {
			mbox->mraid_poll = 0;
			mbox->mraid_ack = 0;
			mbox->numstatus = 0xFF;
			mbox->status = 0xFF;
			WRINDOOR (megaCfg, phys_mbox | 0x1);

			while (mbox->numstatus == 0xFF) ;
			while (mbox->mraid_poll != 0x77) ;
			mbox->mraid_poll = 0;
			mbox->mraid_ack = 0x77;
			mbox->numstatus = 0xFF;

			if (pScb) {
				mega_cmd_done (megaCfg, pScb, mbox->status);
			}

			WRINDOOR (megaCfg, phys_mbox | 0x2);
			while (RDINDOOR (megaCfg) & 0x2) ;

		} else {
			DISABLE_INTR (megaCfg->host->io_port);
			ISSUE_COMMAND (megaCfg->host->io_port);

			while (!
			       ((byte =
				 READ_PORT (megaCfg->host->io_port,
					    INTR_PORT)) & INTR_VALID)) ;
			WRITE_PORT (megaCfg->host->io_port, INTR_PORT, byte);

			ENABLE_INTR (megaCfg->host->io_port);
			CLEAR_INTR (megaCfg->host->io_port);

			if (pScb) {
				mega_cmd_done (megaCfg, pScb, mbox->status);
			} else {
				TRACE (("Error: NULL pScb!\n"));
			}
		}

		for (i = 0; i < MAX_FIRMWARE_STATUS; i++) {
				mbox->completed[i] = 0xFF;
		}

		enable_irq (megaCfg->host->irq);
		retval = mbox->status;
	}

	return retval;
}

/*-------------------------------------------------------------------
 * Copies data to SGLIST
 *-------------------------------------------------------------------*/
/* Note:
	For 64 bit cards, we need a minimum of one SG element for read/write
*/

static int
mega_build_sglist (mega_host_config * megaCfg, mega_scb * scb,
		   u32 * buffer, u32 * length)
{
	struct scatterlist *sgList;
	int idx;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	int sgcnt;
#endif

	mega_mailbox *mbox = NULL;

	mbox = (mega_mailbox *) scb->mboxData;
	/* Scatter-gather not used */
	if (scb->SCpnt->use_sg == 0) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		scb->dma_h_bulkdata = pci_map_single (megaCfg->dev,
				      scb->SCpnt->request_buffer,
				      scb->SCpnt->request_bufflen,
				      scb->dma_direction);
		/* We need to handle special commands like READ64, WRITE64
		   as they need a minimum of 1 SG irrespective of actually SG
		 */
		if ((megaCfg->flag & BOARD_64BIT) &&
		    ((mbox->cmd == MEGA_MBOXCMD_LREAD64) ||
		     (mbox->cmd == MEGA_MBOXCMD_LWRITE64))) {
			scb->sg64List[0].address = scb->dma_h_bulkdata;
			scb->sg64List[0].length = scb->SCpnt->request_bufflen;
			*buffer = scb->dma_sghandle64;
			*length = (u32)scb->SCpnt->request_bufflen;
			scb->sglist_count = 1;
			return 1;
		} else {
			*buffer = scb->dma_h_bulkdata;
			*length = (u32) scb->SCpnt->request_bufflen;
		}
#else
		*buffer = virt_to_bus (scb->SCpnt->request_buffer);
		*length = (u32) scb->SCpnt->request_bufflen;
#endif
		return 0;
	}

	sgList = (struct scatterlist *) scb->SCpnt->request_buffer;
#if 0
	if (scb->SCpnt->use_sg == 1) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		scb->dma_h_bulkdata = pci_map_single (megaCfg->dev,
				      sgList[0].address,
				      sgList[0].length, scb->dma_direction);

		if ((megaCfg->flag & BOARD_64BIT) &&
		    ((mbox->cmd == MEGA_MBOXCMD_LREAD64) ||
		     (mbox->cmd == MEGA_MBOXCMD_LWRITE64))) {
			scb->sg64List[0].address = scb->dma_h_bulkdata;
			scb->sg64List[0].length = scb->SCpnt->request_bufflen;
			*buffer = scb->dma_sghandle64;
			*length = 0;
			scb->sglist_count = 1;
			return 1;
		} else {
			*buffer = scb->dma_h_bulkdata;
			*length = (u32) sgList[0].length;
		}
#else
		*buffer = virt_to_bus (sgList[0].address);
		*length = (u32) sgList[0].length;
#endif

		return 0;
	}
#endif
	/* Copy Scatter-Gather list info into controller structure */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	sgcnt = pci_map_sg (megaCfg->dev,
			    sgList, scb->SCpnt->use_sg, scb->dma_direction);

	/* Determine the validity of the new count  */
	if (sgcnt == 0)
		printk ("pci_map_sg returned zero!!! ");

	for (idx = 0; idx < sgcnt; idx++, sgList++) {

		if ((megaCfg->flag & BOARD_64BIT) &&
		    ((mbox->cmd == MEGA_MBOXCMD_LREAD64) ||
		     (mbox->cmd == MEGA_MBOXCMD_LWRITE64))) {
			scb->sg64List[idx].address = sg_dma_address (sgList);
			scb->sg64List[idx].length = sg_dma_len (sgList);
		} else {
			scb->sgList[idx].address = sg_dma_address (sgList);
			scb->sgList[idx].length = sg_dma_len (sgList);
		}

	}

#else
	for (idx = 0; idx < scb->SCpnt->use_sg; idx++) {
		scb->sgList[idx].address = virt_to_bus (sgList[idx].address);
		scb->sgList[idx].length = (u32) sgList[idx].length;
	}
#endif

	/* Reset pointer and length fields */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	*buffer = scb->dma_sghandle64;
	scb->sglist_count = scb->SCpnt->use_sg;
#else
	*buffer = virt_to_bus (scb->sgList);
#endif

#if 0
	*length = 0;
#endif
	/*
	 * For passthru command, dataxferlen must be set, even for commands with a
	 * sg list
	 */
	*length = (u32)scb->SCpnt->request_bufflen;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	/* Return count of SG requests */
	return sgcnt;
#else
	/* Return count of SG requests */
	return scb->SCpnt->use_sg;
#endif
}

/*--------------------------------------------------------------------
 * Initializes the address of the controller's mailbox register
 *  The mailbox register is used to issue commands to the card.
 *  Format of the mailbox area:
 *   00 01 command
 *   01 01 command id
 *   02 02 # of sectors
 *   04 04 logical bus address
 *   08 04 physical buffer address
 *   0C 01 logical drive #
 *   0D 01 length of scatter/gather list
 *   0E 01 reserved
 *   0F 01 mailbox busy
 *   10 01 numstatus byte
 *   11 01 status byte
 *--------------------------------------------------------------------*/
static int
mega_register_mailbox (mega_host_config * megaCfg, u32 paddr)
{
	/* align on 16-byte boundary */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	megaCfg->mbox = &megaCfg->mailbox64ptr->mailbox;
#else
	megaCfg->mbox = &megaCfg->mailbox64.mailbox;
#endif

#if BITS_PER_LONG==64
	megaCfg->mbox = (mega_mailbox *) ((((u64) megaCfg->mbox) + 16) & ((u64) (-1) ^ 0x0F));
	megaCfg->adjdmahandle64 = (megaCfg->dma_handle64 + 16) & ((u64) (-1) ^ 0x0F);
	megaCfg->mbox64 = (mega_mailbox64 *) ((u_char *) megaCfg->mbox - sizeof (u64));
	paddr = (paddr + 4 + 16) & ((u64) (-1) ^ 0x0F);
#else
	megaCfg->mbox
	    = (mega_mailbox *) ((((u32) megaCfg->mbox) + 16) & 0xFFFFFFF0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	megaCfg->adjdmahandle64 = ((megaCfg->dma_handle64 + 16) & 0xFFFFFFF0);
#endif

	megaCfg->mbox64 = (mega_mailbox64 *) ((u_char *) megaCfg->mbox - 8);
	paddr = (paddr + 4 + 16) & 0xFFFFFFF0;
#endif

	/* Register mailbox area with the firmware */
	if (!(megaCfg->flag & BOARD_QUARTZ)) {
		WRITE_PORT (megaCfg->host->io_port, MBOX_PORT0, paddr & 0xFF);
		WRITE_PORT (megaCfg->host->io_port, MBOX_PORT1,
			    (paddr >> 8) & 0xFF);
		WRITE_PORT (megaCfg->host->io_port, MBOX_PORT2,
			    (paddr >> 16) & 0xFF);
		WRITE_PORT (megaCfg->host->io_port, MBOX_PORT3,
			    (paddr >> 24) & 0xFF);
		WRITE_PORT (megaCfg->host->io_port, ENABLE_MBOX_REGION,
			    ENABLE_MBOX_BYTE);

		CLEAR_INTR (megaCfg->host->io_port);
		ENABLE_INTR (megaCfg->host->io_port);
	}
	return 0;
}

/*---------------------------------------------------------------------------
 * mega_Convert8ldTo40ld() -- takes all info in AdapterInquiry structure and
 * puts it into ProductInfo and Enquiry3 structures for later use
 *---------------------------------------------------------------------------*/
static void mega_Convert8ldTo40ld (mega_RAIDINQ * inquiry,
		       mega_Enquiry3 * enquiry3,
		       megaRaidProductInfo * productInfo)
{
	int i;

	productInfo->MaxConcCmds = inquiry->AdpInfo.MaxConcCmds;
	enquiry3->rbldRate = inquiry->AdpInfo.RbldRate;
	productInfo->SCSIChanPresent = inquiry->AdpInfo.ChanPresent;

	for (i = 0; i < 4; i++) {
		productInfo->FwVer[i] = inquiry->AdpInfo.FwVer[i];
		productInfo->BiosVer[i] = inquiry->AdpInfo.BiosVer[i];
	}
	enquiry3->cacheFlushInterval = inquiry->AdpInfo.CacheFlushInterval;
	productInfo->DramSize = inquiry->AdpInfo.DramSize;

	enquiry3->numLDrv = inquiry->LogdrvInfo.NumLDrv;

	for (i = 0; i < MAX_LOGICAL_DRIVES; i++) {
		enquiry3->lDrvSize[i] = inquiry->LogdrvInfo.LDrvSize[i];
		enquiry3->lDrvProp[i] = inquiry->LogdrvInfo.LDrvProp[i];
		enquiry3->lDrvState[i]
		    = inquiry->LogdrvInfo.LDrvState[i];
	}

	for (i = 0; i < (MAX_PHYSICAL_DRIVES); i++) {
		enquiry3->pDrvState[i]
		    = inquiry->PhysdrvInfo.PDrvState[i];
	}
}

/*-------------------------------------------------------------------
 * Issue an adapter info query to the controller
 *-------------------------------------------------------------------*/
static int mega_i_query_adapter (mega_host_config * megaCfg)
{
	mega_Enquiry3 *enquiry3Pnt;
	mega_mailbox *mbox;
	u_char mboxData[16];

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	dma_addr_t raid_inq_dma_handle = 0, prod_info_dma_handle = 0, enquiry3_dma_handle = 0;
#endif
	u8 retval;

	/* Initialize adapter inquiry mailbox */

	mbox = (mega_mailbox *) mboxData;

	memset ((void *) megaCfg->mega_buffer, 0,
		sizeof (megaCfg->mega_buffer));
	memset (mbox, 0, 16);

/*
 * Try to issue Enquiry3 command
 * if not succeeded, then issue MEGA_MBOXCMD_ADAPTERINQ command and
 * update enquiry3 structure
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	enquiry3_dma_handle = pci_map_single (megaCfg->dev,
			      (void *) megaCfg->mega_buffer,
			      (2 * 1024L), PCI_DMA_FROMDEVICE);

	mbox->xferaddr = enquiry3_dma_handle;
#else
	/*Taken care */
	mbox->xferaddr = virt_to_bus ((void *) megaCfg->mega_buffer);
#endif

	/* Initialize mailbox databuffer addr */
	enquiry3Pnt = (mega_Enquiry3 *) megaCfg->mega_buffer;
	/* point mega_Enguiry3 to the data buf */

	mboxData[0] = FC_NEW_CONFIG;	/* i.e. mbox->cmd=0xA1 */
	mboxData[2] = NC_SUBOP_ENQUIRY3;	/* i.e. 0x0F */
	mboxData[3] = ENQ3_GET_SOLICITED_FULL;	/* i.e. 0x02 */

	/* Issue a blocking command to the card */
	if ((retval = megaIssueCmd (megaCfg, mboxData, NULL, 0)) != 0) {	/* the adapter does not support 40ld */
		mega_RAIDINQ adapterInquiryData;
		mega_RAIDINQ *adapterInquiryPnt = &adapterInquiryData;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		raid_inq_dma_handle = pci_map_single (megaCfg->dev,
				      (void *) adapterInquiryPnt,
				      sizeof (mega_RAIDINQ),
				      PCI_DMA_FROMDEVICE);
		mbox->xferaddr = raid_inq_dma_handle;
#else
		/*taken care */
		mbox->xferaddr = virt_to_bus ((void *) adapterInquiryPnt);
#endif

		mbox->cmd = MEGA_MBOXCMD_ADAPTERINQ;	/*issue old 0x05 command to adapter */
		/* Issue a blocking command to the card */ ;
		retval = megaIssueCmd (megaCfg, mboxData, NULL, 0);

		pci_unmap_single (megaCfg->dev,
				  raid_inq_dma_handle,
				  sizeof (mega_RAIDINQ), PCI_DMA_FROMDEVICE);

		/*update Enquiry3 and ProductInfo structures with mega_RAIDINQ structure*/
		mega_Convert8ldTo40ld (adapterInquiryPnt,
				       enquiry3Pnt,
				       (megaRaidProductInfo *) & megaCfg->
				       productInfo);

	} else {		/* adapter supports 40ld */
		megaCfg->flag |= BOARD_40LD;

		pci_unmap_single (megaCfg->dev,
				  enquiry3_dma_handle,
				  (2 * 1024L), PCI_DMA_FROMDEVICE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
/*get productInfo, which is static information and will be unchanged*/
		prod_info_dma_handle
		    = pci_map_single (megaCfg->dev,
				      (void *) &megaCfg->productInfo,
				      sizeof (megaRaidProductInfo),
				      PCI_DMA_FROMDEVICE);
		mbox->xferaddr = prod_info_dma_handle;
#else
		/*taken care */
		mbox->xferaddr = virt_to_bus ((void *) &megaCfg->productInfo);
#endif

		mboxData[0] = FC_NEW_CONFIG;	/* i.e. mbox->cmd=0xA1 */
		mboxData[2] = NC_SUBOP_PRODUCT_INFO;	/* i.e. 0x0E */

		if ((retval = megaIssueCmd (megaCfg, mboxData, NULL, 0)) != 0)
			printk ("megaraid: Product_info cmd failed with error: %d\n",
				retval);

		pci_unmap_single (megaCfg->dev,
				  prod_info_dma_handle,
				  sizeof (megaRaidProductInfo),
				  PCI_DMA_FROMDEVICE);
	}

	/*
	 * kernel scans the channels from 0 to <= max_channel
	 */
	megaCfg->host->max_channel =
		megaCfg->productInfo.SCSIChanPresent + NVIRT_CHAN -1;

	megaCfg->host->max_id = 16;	/* max targets per channel */

	megaCfg->host->max_lun = 7;	/* Upto 7 luns for non disk devices */

	megaCfg->host->cmd_per_lun = MAX_CMD_PER_LUN;

	megaCfg->numldrv = enquiry3Pnt->numLDrv;
	megaCfg->max_cmds = megaCfg->productInfo.MaxConcCmds;
	if (megaCfg->max_cmds > MAX_COMMANDS)
		megaCfg->max_cmds = MAX_COMMANDS - 1;

	megaCfg->host->can_queue = megaCfg->max_cmds - 1;

	/* use HP firmware and bios version encoding */
	if (megaCfg->productInfo.subSystemVendorID == HP_SUBSYS_ID) {
		sprintf (megaCfg->fwVer, "%c%d%d.%d%d",
			 megaCfg->productInfo.FwVer[2],
			 megaCfg->productInfo.FwVer[1] >> 8,
			 megaCfg->productInfo.FwVer[1] & 0x0f,
			 megaCfg->productInfo.FwVer[0] >> 8,
			 megaCfg->productInfo.FwVer[0] & 0x0f);
		sprintf (megaCfg->biosVer, "%c%d%d.%d%d",
			 megaCfg->productInfo.BiosVer[2],
			 megaCfg->productInfo.BiosVer[1] >> 8,
			 megaCfg->productInfo.BiosVer[1] & 0x0f,
			 megaCfg->productInfo.BiosVer[0] >> 8,
			 megaCfg->productInfo.BiosVer[0] & 0x0f);
	} else {
		memcpy (megaCfg->fwVer, (char *) megaCfg->productInfo.FwVer, 4);
		megaCfg->fwVer[4] = 0;

		memcpy (megaCfg->biosVer, (char *) megaCfg->productInfo.BiosVer, 4);
		megaCfg->biosVer[4] = 0;
	}
	megaCfg->support_ext_cdb = mega_support_ext_cdb(megaCfg);

	printk (KERN_NOTICE "megaraid: [%s:%s] detected %d logical drives" M_RD_CRLFSTR,
		megaCfg->fwVer, megaCfg->biosVer, megaCfg->numldrv);

	if ( megaCfg->support_ext_cdb ) {
		printk(KERN_NOTICE "megaraid: supports extended CDBs.\n");
	}

	/*
	 * I hope that I can unmap here, reason DMA transaction is not required any more
	 * after this
	 */

	return 0;
}

/*-------------------------------------------------------------------------
 *
 *                      Driver interface functions
 *
 *-------------------------------------------------------------------------*/

/*----------------------------------------------------------
 * Returns data to be displayed in /proc/scsi/megaraid/X
 *----------------------------------------------------------*/

int megaraid_proc_info (char *buffer, char **start, off_t offset,
		    int length, int host_no, int inout)
{
	*start = buffer;
	return 0;
}

static int mega_findCard (Scsi_Host_Template * pHostTmpl,
	       u16 pciVendor, u16 pciDev, long flag)
{
	mega_host_config *megaCfg = NULL;
	struct Scsi_Host *host = NULL;
	u_char pciBus, pciDevFun, megaIrq;

	u16 magic;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	u32 magic64;
#endif

	int		i, j;

	unsigned long megaBase;
	unsigned long tbase;

	u16 pciIdx = 0;
	u16 numFound = 0;
	u16 subsysid, subsysvid;
	u8 did_mem_map_f = 0;
	u8 did_io_map_f = 0;
	u8 did_scsi_register_f = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)	/* 0x20100 */
	while (!pcibios_find_device
	       (pciVendor, pciDev, pciIdx, &pciBus, &pciDevFun)) {
#else

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)	/*0x20300 */
	struct pci_dev *pdev = NULL;
#else
	struct pci_dev *pdev = pci_devices;
#endif

	while ((pdev = pci_find_device (pciVendor, pciDev, pdev))) {
		if(pci_enable_device (pdev))
			continue;
		pciBus = pdev->bus->number;
		pciDevFun = pdev->devfn;
#endif

		/*
		 * Set the dma_mask to default value. It might be sticky from previous
		 * insmod-rmmod sequence
		 */
		pdev->dma_mask = 0xFFFFFFFF;

		did_mem_map_f = 0;
		did_io_map_f = 0;
		did_scsi_register_f = 0;

		if ((flag & BOARD_QUARTZ) && (skip_id == -1)) {
				if( (pciVendor == PCI_VENDOR_ID_PERC4_DI_YSTONE &&
					pciDev == PCI_DEVICE_ID_PERC4_DI_YSTONE) ||
					(pciVendor == PCI_VENDOR_ID_PERC4_QC_VERDE &&
					pciDev == PCI_DEVICE_ID_PERC4_QC_VERDE) ) {

					flag |= BOARD_64BIT;
				}
				else {
					pci_read_config_word (pdev, PCI_CONF_AMISIG, &magic);
					if ((magic != AMI_SIGNATURE)
						&& (magic != AMI_SIGNATURE_471)) {
						pciIdx++;
						continue;	/* not an AMI board */
					}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
					pci_read_config_dword (pdev, PCI_CONF_AMISIG64, &magic64);

					if (magic64 == AMI_64BIT_SIGNATURE)
						flag |= BOARD_64BIT;
#endif
				}
		}

		/* Hmmm...Should we not make this more modularized so that in future we dont add
		   for each firmware */

		if (flag & BOARD_QUARTZ) {
			/* Check to see if this is a Dell PERC RAID controller model 466 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)	/* 0x20100 */
			pcibios_read_config_word (pciBus, pciDevFun,
						  PCI_SUBSYSTEM_VENDOR_ID,
						  &subsysvid);
			pcibios_read_config_word (pciBus, pciDevFun,
						  PCI_SUBSYSTEM_ID, &subsysid);
#else
			pci_read_config_word (pdev,
					      PCI_SUBSYSTEM_VENDOR_ID,
					      &subsysvid);
			pci_read_config_word (pdev,
					      PCI_SUBSYSTEM_ID, &subsysid);
#endif

			/*
			 * If we do not find the valid subsys vendor id, refuse to load
			 * the driver. This is part of PCI200X compliance
			 */
			if( (subsysvid != AMI_SUBSYS_ID) &&
					(subsysvid != DELL_SUBSYS_ID) &&
					(subsysvid != LSI_SUBSYS_ID) &&
					(subsysvid != INTEL_SUBSYS_ID) &&
					(subsysvid != HP_SUBSYS_ID) ) continue;

		}

		printk (KERN_NOTICE
			"megaraid: found 0x%4.04x:0x%4.04x:idx %d:bus %d:slot %d:func %d\n",
			pciVendor, pciDev, pciIdx, pciBus, PCI_SLOT (pciDevFun),
			PCI_FUNC (pciDevFun));
		/* Read the base port and IRQ from PCI */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)	/* 0x20100 */
		pcibios_read_config_dword (pciBus, pciDevFun,
					   PCI_BASE_ADDRESS_0,
					   (u_int *) & megaBase);
		pcibios_read_config_byte (pciBus, pciDevFun,
					  PCI_INTERRUPT_LINE, &megaIrq);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)	/*0x20300 */
		megaBase = pdev->base_address[0];
		megaIrq = pdev->irq;
#else

		megaBase = pci_resource_start (pdev, 0);
		megaIrq = pdev->irq;
#endif

		tbase = megaBase;

		pciIdx++;

		if (flag & BOARD_QUARTZ) {

			megaBase &= PCI_BASE_ADDRESS_MEM_MASK;

			if( ! request_mem_region(megaBase, 128,
									"MegaRAID: LSI Logic Corporation" ) ) {

					printk(KERN_WARNING "megaraid: mem region busy!\n");

					continue;
			}

			megaBase = (long) ioremap (megaBase, 128);

			if (!megaBase) {

				printk(KERN_WARNING "megaraid: could not map hba memory!\n");

				release_mem_region(tbase, 128);

				continue;
			}
			did_mem_map_f = 1;

		} else {
			megaBase &= PCI_BASE_ADDRESS_IO_MASK;
			megaBase += 0x10;

			if( ! request_region(megaBase, 16,
									"MegaRAID: LSI Logic Corporation") ) {

					printk(KERN_WARNING "megaraid: region busy.\n");

					continue;
			}
			did_io_map_f = 1;

		}

		/* Initialize SCSI Host structure */
		host = scsi_register (pHostTmpl, sizeof (mega_host_config));
		if (!host)
			goto fail_attach;

		did_scsi_register_f = 1;

		/*
		 * Comment the following initialization if you know 'max_sectors' is
		 * not defined for this kernel.
		 * This field was introduced in Linus's kernel 2.4.7pre3 and it
		 * greatly increases the IO performance - AM
		 */
		host->max_sectors = 128;

		scsi_set_pci_device(host, pdev);
		megaCfg = (mega_host_config *) host->hostdata;
		memset (megaCfg, 0, sizeof (mega_host_config));

		printk (KERN_NOTICE "scsi%d : Found a MegaRAID controller at 0x%x, IRQ: %d"
			M_RD_CRLFSTR, host->host_no, (u_int) megaBase, megaIrq);

		if (flag & BOARD_64BIT)
			printk (KERN_NOTICE "scsi%d : Enabling 64 bit support\n",
				host->host_no);

		/* Copy resource info into structure */
		megaCfg->qCompletedH = NULL;
		megaCfg->qCompletedT = NULL;
		megaCfg->qPendingH = NULL;
		megaCfg->qPendingT = NULL;
		megaCfg->qFreeH = NULL;
		megaCfg->qFreeT = NULL;
		megaCfg->qFcnt = 0;
		megaCfg->qPcnt = 0;
		megaCfg->qCcnt = 0;
		megaCfg->lock_free = SPIN_LOCK_UNLOCKED;
		megaCfg->lock_pend = SPIN_LOCK_UNLOCKED;
		megaCfg->lock_scsicmd = SPIN_LOCK_UNLOCKED;
		megaCfg->flag = flag;
		megaCfg->int_qh = NULL;
		megaCfg->int_qt = NULL;
		megaCfg->int_qlen = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		megaCfg->dev = pdev;
#endif
		megaCfg->host = host;
		megaCfg->base = megaBase;
		megaCfg->host->irq = megaIrq;
		megaCfg->host->io_port = megaBase;
		megaCfg->host->n_io_port = 16;
		megaCfg->host->unique_id = (pciBus << 8) | pciDevFun;
		megaCtlrs[numCtlrs] = megaCfg;

		if (flag & BOARD_QUARTZ) {
				megaCfg->host->base = tbase;
		}

		/* Request our IRQ */
		if (request_irq (megaIrq, megaraid_isr, SA_SHIRQ,
				 "megaraid", megaCfg)) {
			printk (KERN_WARNING
				"megaraid: Couldn't register IRQ %d!\n",
				megaIrq);
			goto fail_attach;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		/*
		 * unmap while releasing the driver, Is it required to be 
		 * PCI_DMA_BIDIRECTIONAL 
		*/

		megaCfg->mailbox64ptr
		    = pci_alloc_consistent (megaCfg->dev,
					    sizeof (mega_mailbox64),
					    &(megaCfg->dma_handle64));

		mega_register_mailbox (megaCfg,megaCfg->dma_handle64);
#else
		mega_register_mailbox (megaCfg,
				       virt_to_bus ((void *) &megaCfg->
						    mailbox64));
#endif

		mega_i_query_adapter (megaCfg);

		if ((subsysid == 0x1111) && (subsysvid == 0x1111)) {

			/*
			 * Which firmware
			 */
			if( strcmp(megaCfg->fwVer, "3.00") == 0 ||
					strcmp(megaCfg->fwVer, "3.01") == 0 ) {

				printk( KERN_WARNING
					"megaraid: Your  card is a Dell PERC 2/SC RAID controller "
					"with  firmware\nmegaraid: 3.00 or 3.01.  This driver is "
					"known to have corruption issues\nmegaraid: with those "
					"firmware versions on this specific card.  In order\n"
					"megaraid: to protect your data, please upgrade your "
					"firmware to version\nmegaraid: 3.10 or later, available "
					"from the Dell Technical Support web\nmegaraid: site at\n"
					"http://support.dell.com/us/en/filelib/download/"
					"index.asp?fileid=2940\n"
				);
			}
		}

		/*
		 * If we have a HP 1M(0x60E7)/2M(0x60E8) controller with
		 * firmware H.01.07 or H.01.08, disable 64 bit support,
		 * since this firmware cannot handle 64 bit addressing
		 */

		if( (subsysvid == HP_SUBSYS_ID) &&
				((subsysid == 0x60E7)||(subsysid == 0x60E8)) ) {

			/*
			 * which firmware
			 */
			if( strcmp(megaCfg->fwVer, "H01.07") == 0 || 
			    strcmp(megaCfg->fwVer, "H01.08") == 0 ||
			    strcmp(megaCfg->fwVer, "H01.09") == 0 )
			{
				printk(KERN_WARNING
						"megaraid: Firmware H.01.07/8/9 on 1M/2M "
						"controllers\nmegaraid: do not support 64 bit "
						"addressing.\n"
						"megaraid: DISABLING 64 bit support.\n");
				megaCfg->flag &= ~BOARD_64BIT;
			}
		}

		if (mega_is_bios_enabled (megaCfg)) {
			mega_hbas[numCtlrs].is_bios_enabled = 1;
		}

		/*
		 * Find out which channel is raid and which is scsi
		 */
		mega_enum_raid_scsi(megaCfg);

		/*
		 * Find out if a logical drive is set as the boot drive. If there is
		 * one, will make that as the first logical drive.
		 * ROMB: Do we have to boot from a physical drive. Then all the
		 * physical drives would appear before the logical disks. Else, all
		 * the physical drives would be exported to the mid layer after
		 * logical disks.
		 */
		mega_get_boot_drv(megaCfg);

		if( ! megaCfg->boot_pdrv_enabled ) {
			for( i = 0; i < NVIRT_CHAN; i++ )
				megaCfg->logdrv_chan[i] = 1;

			for( i = NVIRT_CHAN; i < MAX_CHANNEL + NVIRT_CHAN; i++ )
				megaCfg->logdrv_chan[i] = 0;

			megaCfg->mega_ch_class <<= NVIRT_CHAN;
		}
		else {
			j = megaCfg->productInfo.SCSIChanPresent;
			for( i = 0; i < j; i++ )
				megaCfg->logdrv_chan[i] = 0;

			for( i = j; i < NVIRT_CHAN + j; i++ )
				megaCfg->logdrv_chan[i] = 1;
		}


		mega_hbas[numCtlrs].hostdata_addr = megaCfg;

		/*
		 * Do we support random deletion and addition of logical drives
		 */
		megaCfg->read_ldidmap = 0;	/* set it after first logdrv delete cmd */
		megaCfg->support_random_del = mega_support_random_del(megaCfg);

		/* Initialize SCBs */
		if (mega_init_scb (megaCfg)) {
			pci_free_consistent (megaCfg->dev,
					     sizeof (mega_mailbox64),
					     (void *) megaCfg->mailbox64ptr,
					     megaCfg->dma_handle64);
			goto fail_attach;
		}

		/*
		 * Fill in the structure which needs to be passed back to the
		 * application when it does an ioctl() for controller related
		 * information.
		 */

		i = numCtlrs;
		numCtlrs++;

		mcontroller[i].base = megaBase;
		mcontroller[i].irq = megaIrq;
		mcontroller[i].numldrv = megaCfg->numldrv;
		mcontroller[i].pcibus = pciBus;
		mcontroller[i].pcidev = pciDev;
		mcontroller[i].pcifun = PCI_FUNC (pciDevFun);
		mcontroller[i].pciid = pciIdx;
		mcontroller[i].pcivendor = pciVendor;
		mcontroller[i].pcislot = PCI_SLOT (pciDevFun);
		mcontroller[i].uid = (pciBus << 8) | pciDevFun;

		numFound++;

		/* Set the Mode of addressing to 64 bit */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		if ((megaCfg->flag & BOARD_64BIT) && BITS_PER_LONG == 64)
#if BITS_PER_LONG==64
			pdev->dma_mask = 0xffffffffffffffff;
#else
			pdev->dma_mask = 0xffffffff;
#endif
#endif
		continue;
fail_attach:
		if( did_mem_map_f ) {
				iounmap((void *)megaBase);
				release_mem_region(tbase, 128);
		}
		if( did_io_map_f ) {
				release_region(megaBase, 16);
		}
		if( did_scsi_register_f ) {
				scsi_unregister (host);
		}
	}
	return numFound;
}

/*---------------------------------------------------------
 * Detects if a megaraid controller exists in this system
 *---------------------------------------------------------*/

int megaraid_detect (Scsi_Host_Template * pHostTmpl)
{
	int ctlridx = 0, count = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)	/*0x20300 */
	pHostTmpl->proc_dir = &proc_scsi_megaraid;
#else
	pHostTmpl->proc_name = "megaraid";
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)	/* 0x20100 */
	if (!pcibios_present ()) {
		printk (KERN_WARNING "megaraid: PCI bios not present."
			M_RD_CRLFSTR);
		return 0;
	}
#endif
	skip_id = -1;
	if (megaraid && !strncmp (megaraid, "skip", strlen ("skip"))) {
		if (megaraid[4] != '\0') {
			skip_id = megaraid[4] - '0';
			if (megaraid[5] != '\0') {
				skip_id = (skip_id * 10) + (megaraid[5] - '0');
			}
		}
		skip_id = (skip_id > 15) ? -1 : skip_id;
	}

	printk (KERN_NOTICE "megaraid: " MEGARAID_VERSION);

	memset (mega_hbas, 0, sizeof (mega_hbas));

	/* Detect ROMBs first */
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_DISCOVERY,
				PCI_DEVICE_ID_DISCOVERY, BOARD_QUARTZ);
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_PERC4_DI_YSTONE,
				PCI_DEVICE_ID_PERC4_DI_YSTONE, BOARD_QUARTZ);
	/* Then detect cards based on date they were produced, oldest first */
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_AMI,
				PCI_DEVICE_ID_AMI_MEGARAID, 0);
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_AMI,
				PCI_DEVICE_ID_AMI_MEGARAID2, 0);
	count += mega_findCard (pHostTmpl, 0x8086,
				PCI_DEVICE_ID_AMI_MEGARAID3, BOARD_QUARTZ);
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_AMI,
				PCI_DEVICE_ID_AMI_MEGARAID3, BOARD_QUARTZ);
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_LSI_LOGIC,
				PCI_DEVICE_ID_AMI_MEGARAID3, BOARD_QUARTZ);
	count += mega_findCard (pHostTmpl, PCI_VENDOR_ID_PERC4_QC_VERDE,
				PCI_DEVICE_ID_PERC4_QC_VERDE, BOARD_QUARTZ);

	mega_reorder_hosts ();

#ifdef CONFIG_PROC_FS
	if (count) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)	/*0x20300 */
		mega_proc_dir_entry = proc_mkdir ("megaraid", &proc_root);
#else
		mega_proc_dir_entry = create_proc_entry ("megaraid",
							 S_IFDIR | S_IRUGO |
							 S_IXUGO, &proc_root);
#endif
		if (!mega_proc_dir_entry)
			printk ("megaraid: failed to create megaraid root\n");
		else
			for (ctlridx = 0; ctlridx < count; ctlridx++)
				mega_create_proc_entry (ctlridx,
							mega_proc_dir_entry);
	}
#endif

	/*
	 * Register the driver as a character device, for applications to access
	 * it for ioctls.
	 * Ideally, this should go in the init_module() routine, but since it is
	 * hidden in the file "scsi_module.c" ( included in the end ), we define
	 * it here
	 * First argument (major) to register_chrdev implies a dynamic major
	 * number allocation.
	 */
	if (count) {
		major = register_chrdev (0, "megadev", &megadev_fops);

		/*
		 * Register the Shutdown Notification hook in kernel
		 */
		if (register_reboot_notifier (&mega_notifier)) {
			printk ("MegaRAID Shutdown routine not registered!!\n");
		}

		init_MUTEX (&mimd_entry_mtx);
#ifdef __x86_64__
		/*
		 * Register the 32-bit ioctl conversion
		 */
		register_ioctl32_conversion( MEGAIOCCMD, sys_ioctl );
#endif
	}

	return count;
}

/*---------------------------------------------------------------------
 * Release the controller's resources
 *---------------------------------------------------------------------*/
int megaraid_release (struct Scsi_Host *pSHost)
{
	mega_host_config *megaCfg;
	mega_mailbox *mbox;
	u_char mboxData[16];
	int i;

	megaCfg = (mega_host_config *) pSHost->hostdata;
	mbox = (mega_mailbox *) mboxData;

	/* Flush cache to disk */
	memset (mbox, 0, 16);
	mboxData[0] = 0xA;

	free_irq (megaCfg->host->irq, megaCfg);	/* Must be freed first, otherwise
						   extra interrupt is generated */

	/* Issue a blocking (interrupts disabled) command to the card */
	megaIssueCmd (megaCfg, mboxData, NULL, 0);

	/* Free our resources */
	if (megaCfg->flag & BOARD_QUARTZ) {
		iounmap ((void *) megaCfg->base);
		release_mem_region(megaCfg->host->base, 128);
	} else {
		release_region (megaCfg->host->io_port, 16);
	}

	mega_freeSgList (megaCfg);
	pci_free_consistent (megaCfg->dev,
			     sizeof (mega_mailbox64),
			     (void *) megaCfg->mailbox64ptr,
			     megaCfg->dma_handle64);

#ifdef CONFIG_PROC_FS
	if (megaCfg->controller_proc_dir_entry) {
		remove_proc_entry ("stat", megaCfg->controller_proc_dir_entry);
		remove_proc_entry ("status",
				   megaCfg->controller_proc_dir_entry);
		remove_proc_entry ("config",
				   megaCfg->controller_proc_dir_entry);
		remove_proc_entry ("mailbox",
				   megaCfg->controller_proc_dir_entry);
		for (i = 0; i < numCtlrs; i++) {
			char buf[12] = { 0 };
			sprintf (buf, "%d", i);
			remove_proc_entry (buf, mega_proc_dir_entry);
		}
		remove_proc_entry ("megaraid", &proc_root);
	}
#endif

	/*
	 *	Release the controller memory. A word of warning this frees
	 *	hostdata and that includes megaCfg-> so be careful what you
	 *	dereference beyond this point
	 */
	 
	scsi_unregister (pSHost);

	/*
	 * Unregister the character device interface to the driver. Ideally this
	 * should have been done in cleanup_module routine. Since this is hidden
	 * in file "scsi_module.c", we do it here.
	 * major is the major number of the character device returned by call to
	 * register_chrdev() routine.
	 */

	unregister_chrdev (major, "megadev");
	unregister_reboot_notifier (&mega_notifier);
#ifdef __x86_64__
	unregister_ioctl32_conversion( MEGAIOCCMD );
#endif

	return 0;
}

static int mega_is_bios_enabled (mega_host_config * megacfg)
{
	mega_mailbox *mboxpnt;
	unsigned char mbox[16];
	int ret;

	mboxpnt = (mega_mailbox *) mbox;

	memset (mbox, 0, sizeof (mbox));
	memset ((void *) megacfg->mega_buffer,
		0, sizeof (megacfg->mega_buffer));

	/*
	 * issue command to find out if the BIOS is enabled for this controller
	 */
	mbox[0] = IS_BIOS_ENABLED;
	mbox[2] = GET_BIOS;

	mboxpnt->xferaddr = virt_to_bus ((void *) megacfg->mega_buffer);

	ret = megaIssueCmd (megacfg, mbox, NULL, 0);

	return (*(char *) megacfg->mega_buffer);
}

/*
 * Find out what channels are RAID/SCSI
 */
static void
mega_enum_raid_scsi(mega_host_config *megacfg)
{
	mega_mailbox *mboxp;
	unsigned char mbox[16];
	int		i;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	dma_addr_t	dma_handle;
#endif

	mboxp = (mega_mailbox *)mbox;

	memset(mbox, 0, sizeof(mbox));
	/*
	 * issue command to find out what channels are raid/scsi
	 */
	mbox[0] = CHNL_CLASS;
	mbox[2] = GET_CHNL_CLASS;

	memset((void *)megacfg->mega_buffer, 0, sizeof(megacfg->mega_buffer));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	dma_handle = pci_map_single(megacfg->dev, (void *)megacfg->mega_buffer,
			      (2 * 1024L), PCI_DMA_FROMDEVICE);

	mboxp->xferaddr = dma_handle;
#else
	mboxp->xferaddr = virt_to_bus((void *)megacfg->mega_buffer);
#endif

	/*
	 * Non-ROMB firware fail this command, so all channels
	 * must be shown RAID
	 */
	megacfg->mega_ch_class = 0xFF;
	if( megaIssueCmd(megacfg, mbox, NULL, 0) == 0 ) {
		megacfg->mega_ch_class = *((char *)megacfg->mega_buffer);
	}

	for( i = 0; i < megacfg->productInfo.SCSIChanPresent; i++ ) {
		if( (megacfg->mega_ch_class >> i) & 0x01 )
			printk(KERN_NOTICE"megaraid: channel[%d] is raid.\n", i+1);
		else
			printk(KERN_NOTICE"megaraid: channel[%d] is scsi.\n", i+1);
	}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	pci_unmap_single(megacfg->dev, dma_handle,
				  (2 * 1024L), PCI_DMA_FROMDEVICE);
#endif

}


/*
 * get the boot logical drive number if enabled
 */
void
mega_get_boot_drv(mega_host_config *megacfg)
{
	mega_mailbox *mboxp;
	unsigned char mbox[16];
	struct private_bios_data *prv_bios_data;
	u16		cksum = 0;
	u8		*cksum_p;
	u8		boot_pdrv;
	int		i;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	dma_addr_t	dma_handle;
#endif

	mboxp = (mega_mailbox *)mbox;

	memset(mbox, 0, sizeof(mbox));

	mbox[0] = BIOS_PVT_DATA;
	mbox[2] = GET_BIOS_PVT_DATA;

	memset((void *)megacfg->mega_buffer, 0, sizeof(megacfg->mega_buffer));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	dma_handle = pci_map_single(megacfg->dev, (void *)megacfg->mega_buffer,
			      (2 * 1024L), PCI_DMA_FROMDEVICE);

	mboxp->xferaddr = dma_handle;
#else
	mboxp->xferaddr = virt_to_bus((void *)megacfg->mega_buffer);
#endif

	megacfg->boot_ldrv_enabled = 0;
	megacfg->boot_ldrv = 0;

	megacfg->boot_pdrv_enabled = 0;
	megacfg->boot_pdrv_ch = 0;
	megacfg->boot_pdrv_tgt = 0;

	if( megaIssueCmd(megacfg, mbox, NULL, 0) == 0 ) {
		prv_bios_data = (struct private_bios_data *)megacfg->mega_buffer;

		cksum = 0;
		cksum_p = (u8 *)prv_bios_data;
		for( i = 0; i < 14; i++ ) {
			cksum += *cksum_p++;
		}

		if( prv_bios_data->cksum == (u16)(0-cksum) ) {

			/*
			 * If MSB is set, a physical drive is set as boot device
			 */
			if( prv_bios_data->boot_drv & 0x80 ) {
				megacfg->boot_pdrv_enabled = 1;
				boot_pdrv = prv_bios_data->boot_drv & 0x7F;
				megacfg->boot_pdrv_ch = boot_pdrv / 16;
				megacfg->boot_pdrv_tgt = boot_pdrv % 16;
			}
			else {
				megacfg->boot_ldrv_enabled = 1;
				megacfg->boot_ldrv = prv_bios_data->boot_drv;
			}
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	pci_unmap_single(megacfg->dev, dma_handle,
				  (2 * 1024L), PCI_DMA_FROMDEVICE);
#endif

}


static void mega_reorder_hosts (void)
{
	struct Scsi_Host *shpnt;
	struct Scsi_Host *shone;
	struct Scsi_Host *shtwo;
	mega_host_config *boot_host;
	int i;

	/*
	 * Find the (first) host which has it's BIOS enabled
	 */
	boot_host = NULL;
	for (i = 0; i < MAX_CONTROLLERS; i++) {
		if (mega_hbas[i].is_bios_enabled) {
			boot_host = mega_hbas[i].hostdata_addr;
			break;
		}
	}

	if (boot_host == NULL) {
		printk (KERN_WARNING "megaraid: no BIOS enabled.\n");
		return;
	}

	/*
	 * Traverse through the list of SCSI hosts for our HBA locations
	 */
	shone = shtwo = NULL;
	for (shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next) {
		/* Is it one of ours? */
		for (i = 0; i < MAX_CONTROLLERS; i++) {
			if ((mega_host_config *) shpnt->hostdata ==
			    mega_hbas[i].hostdata_addr) {
				/* Does this one has BIOS enabled */
				if (mega_hbas[i].hostdata_addr == boot_host) {

					/* Are we first */
					if (shtwo == NULL)	/* Yes! */
						return;
					else {	/* :-( */
						shone = shpnt;
					}
				} else {
					if (!shtwo) {
						/* were we here before? xchng first */
						shtwo = shpnt;
					}
				}
				break;
			}
		}
		/*
		 * Have we got the boot host and one which does not have the bios
		 * enabled.
		 */
		if (shone && shtwo)
			break;
	}
	if (shone && shtwo) {
		mega_swap_hosts (shone, shtwo);
	}

	return;
}

static void mega_swap_hosts (struct Scsi_Host *shone, struct Scsi_Host *shtwo)
{
	struct Scsi_Host *prevtoshtwo;
	struct Scsi_Host *prevtoshone;
	struct Scsi_Host *save = NULL;;

	/* Are these two nodes adjacent */
	if (shtwo->next == shone) {

		if (shtwo == scsi_hostlist && shone->next == NULL) {

			/* just two nodes */
			scsi_hostlist = shone;
			shone->next = shtwo;
			shtwo->next = NULL;
		} else if (shtwo == scsi_hostlist) {
			/* first two nodes of the list */

			scsi_hostlist = shone;
			shtwo->next = shone->next;
			scsi_hostlist->next = shtwo;
		} else if (shone->next == NULL) {
			/* last two nodes of the list */

			prevtoshtwo = scsi_hostlist;

			while (prevtoshtwo->next != shtwo)
				prevtoshtwo = prevtoshtwo->next;

			prevtoshtwo->next = shone;
			shone->next = shtwo;
			shtwo->next = NULL;
		} else {
			prevtoshtwo = scsi_hostlist;

			while (prevtoshtwo->next != shtwo)
				prevtoshtwo = prevtoshtwo->next;

			prevtoshtwo->next = shone;
			shtwo->next = shone->next;
			shone->next = shtwo;
		}

	} else if (shtwo == scsi_hostlist && shone->next == NULL) {
		/* shtwo at head, shone at tail, not adjacent */

		prevtoshone = scsi_hostlist;

		while (prevtoshone->next != shone)
			prevtoshone = prevtoshone->next;

		scsi_hostlist = shone;
		shone->next = shtwo->next;
		prevtoshone->next = shtwo;
		shtwo->next = NULL;
	} else if (shtwo == scsi_hostlist && shone->next != NULL) {
		/* shtwo at head, shone is not at tail */

		prevtoshone = scsi_hostlist;
		while (prevtoshone->next != shone)
			prevtoshone = prevtoshone->next;

		scsi_hostlist = shone;
		prevtoshone->next = shtwo;
		save = shtwo->next;
		shtwo->next = shone->next;
		shone->next = save;
	} else if (shone->next == NULL) {
		/* shtwo not at head, shone at tail */

		prevtoshtwo = scsi_hostlist;
		prevtoshone = scsi_hostlist;

		while (prevtoshtwo->next != shtwo)
			prevtoshtwo = prevtoshtwo->next;
		while (prevtoshone->next != shone)
			prevtoshone = prevtoshone->next;

		prevtoshtwo->next = shone;
		shone->next = shtwo->next;
		prevtoshone->next = shtwo;
		shtwo->next = NULL;

	} else {
		prevtoshtwo = scsi_hostlist;
		prevtoshone = scsi_hostlist;
		save = NULL;;

		while (prevtoshtwo->next != shtwo)
			prevtoshtwo = prevtoshtwo->next;
		while (prevtoshone->next != shone)
			prevtoshone = prevtoshone->next;

		prevtoshtwo->next = shone;
		save = shone->next;
		shone->next = shtwo->next;
		prevtoshone->next = shtwo;
		shtwo->next = save;
	}
	return;
}

static inline void mega_freeSgList (mega_host_config * megaCfg)
{
	int i;

	for (i = 0; i < megaCfg->max_cmds; i++) {
		if (megaCfg->scbList[i].sgList)
			pci_free_consistent (megaCfg->dev,
					     sizeof (mega_64sglist) *
					     MAX_SGLIST,
					     megaCfg->scbList[i].sgList,
					     megaCfg->scbList[i].
					     dma_sghandle64);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)	/* 0x020400 */
			kfree (megaCfg->scbList[i].sgList);	/* free sgList */
#endif
	}
}

/*----------------------------------------------
 * Get information about the card/driver
 *----------------------------------------------*/
const char *megaraid_info (struct Scsi_Host *pSHost)
{
	static char buffer[512];
	mega_host_config *megaCfg;

	megaCfg = (mega_host_config *) pSHost->hostdata;

	sprintf (buffer,
		 "LSI Logic MegaRAID %s %d commands %d targs %d chans %d luns",
		 megaCfg->fwVer, megaCfg->productInfo.MaxConcCmds,
		 megaCfg->host->max_id-1, megaCfg->host->max_channel,
		 megaCfg->host->max_lun);
	return buffer;
}

/*-----------------------------------------------------------------
 * Perform a SCSI command
 * Mailbox area:
 *   00 01 command
 *   01 01 command id
 *   02 02 # of sectors
 *   04 04 logical bus address
 *   08 04 physical buffer address
 *   0C 01 logical drive #
 *   0D 01 length of scatter/gather list
 *   0E 01 reserved
 *   0F 01 mailbox busy
 *   10 01 numstatus byte
 *   11 01 status byte
 *-----------------------------------------------------------------*/
int megaraid_queue (Scsi_Cmnd * SCpnt, void (*pktComp) (Scsi_Cmnd *))
{
	DRIVER_LOCK_T mega_host_config * megaCfg;
	mega_scb *pScb;
	char *user_area = NULL;

	megaCfg = (mega_host_config *) SCpnt->host->hostdata;
	DRIVER_LOCK (megaCfg);

#if 0
	if (!(megaCfg->flag & (1L << SCpnt->channel))) {
		if (SCpnt->channel < megaCfg->productInfo.SCSIChanPresent)
			printk ( KERN_NOTICE
				"scsi%d: scanning channel %d for devices.\n",
				megaCfg->host->host_no, SCpnt->channel);
		else
			printk ( KERN_NOTICE
				"scsi%d: scanning virtual channel %d for logical drives.\n",
				megaCfg->host->host_no,
				SCpnt->channel-megaCfg->productInfo.SCSIChanPresent+1);

		megaCfg->flag |= (1L << SCpnt->channel);
	}
#endif

	SCpnt->scsi_done = pktComp;

	if (mega_driver_ioctl (megaCfg, SCpnt))
		return 0;

	/* If driver in abort or reset.. cancel this command */
	if (megaCfg->flag & IN_ABORT) {
		SCpnt->result = (DID_ABORT << 16);
		/* Add Scsi_Command to end of completed queue */
		if (megaCfg->qCompletedH == NULL) {
			megaCfg->qCompletedH = megaCfg->qCompletedT = SCpnt;
		} else {
			megaCfg->qCompletedT->host_scribble =
			    (unsigned char *) SCpnt;
			megaCfg->qCompletedT = SCpnt;
		}
		megaCfg->qCompletedT->host_scribble = (unsigned char *) NULL;
		megaCfg->qCcnt++;

		DRIVER_UNLOCK (megaCfg);
		return 0;
	} else if (megaCfg->flag & IN_RESET) {
		SCpnt->result = (DID_RESET << 16);
		/* Add Scsi_Command to end of completed queue */
		if (megaCfg->qCompletedH == NULL) {
			megaCfg->qCompletedH = megaCfg->qCompletedT = SCpnt;
		} else {
			megaCfg->qCompletedT->host_scribble =
			    (unsigned char *) SCpnt;
			megaCfg->qCompletedT = SCpnt;
		}
		megaCfg->qCompletedT->host_scribble = (unsigned char *) NULL;
		megaCfg->qCcnt++;

		DRIVER_UNLOCK (megaCfg);
		return 0;
	}

	megaCfg->flag |= IN_QUEUE;
	/* Allocate and build a SCB request */
	if ((pScb = mega_build_cmd (megaCfg, SCpnt)) != NULL) {

		/*
		 * Check if the HBA is in quiescent state, e.g., during a delete
		 * logical drive opertion. If it is, queue the commands in the
		 * internal queue until the delete operation is complete.
		 */
		if( ! megaCfg->quiescent ) {
			/* Add SCB to the head of the pending queue */
			if (megaCfg->qPendingH == NULL) {
				megaCfg->qPendingH = megaCfg->qPendingT = pScb;
			} else {
				megaCfg->qPendingT->next = pScb;
				megaCfg->qPendingT = pScb;
			}
			megaCfg->qPendingT->next = NULL;
			megaCfg->qPcnt++;

			if (mega_runpendq (megaCfg) == -1) {
				DRIVER_UNLOCK (megaCfg);
				return 0;
			}
		}
		else {
			/* Add SCB to the internal queue */
			if (megaCfg->int_qh == NULL) {
				megaCfg->int_qh = megaCfg->int_qt = pScb;
			} else {
				megaCfg->int_qt->next = pScb;
				megaCfg->int_qt = pScb;
			}
			megaCfg->int_qt->next = NULL;
			megaCfg->int_qlen++;
		}

		if (pScb->SCpnt->cmnd[0] == M_RD_IOCTL_CMD_NEW) {
			init_MUTEX_LOCKED (&pScb->ioctl_sem);
			spin_unlock_irq (&io_request_lock);
			down (&pScb->ioctl_sem);
    		user_area = (char *)*((u32*)&pScb->SCpnt->cmnd[4]);
			if (copy_to_user
			    (user_area, pScb->buff_ptr, pScb->iDataSize)) {
				printk
				    ("megaraid: Error copying ioctl return value to user buffer.\n");
				pScb->SCpnt->result = (DID_ERROR << 16);
			}
			spin_lock_irq (&io_request_lock);
			DRIVER_LOCK (megaCfg);
			kfree (pScb->buff_ptr);
			pScb->buff_ptr = NULL;
			mega_cmd_done (megaCfg, pScb, pScb->SCpnt->result);
			mega_rundoneq (megaCfg);
			mega_runpendq (megaCfg);
			DRIVER_UNLOCK (megaCfg);
		}

		megaCfg->flag &= ~IN_QUEUE;

	}

	DRIVER_UNLOCK (megaCfg);
	return 0;
}

/*----------------------------------------------------------------------
 * Issue a blocking command to the controller
 *----------------------------------------------------------------------*/
volatile static int internal_done_flag = 0;
volatile static int internal_done_errcode = 0;

static DECLARE_WAIT_QUEUE_HEAD (internal_wait);

static void internal_done (Scsi_Cmnd * SCpnt)
{
	internal_done_errcode = SCpnt->result;
	internal_done_flag++;
	wake_up (&internal_wait);
}

/* shouldn't be used, but included for completeness */

int megaraid_command (Scsi_Cmnd * SCpnt)
{
	internal_done_flag = 0;

	/* Queue command, and wait until it has completed */
	megaraid_queue (SCpnt, internal_done);

	while (!internal_done_flag) {
		interruptible_sleep_on (&internal_wait);
	}

	return internal_done_errcode;
}

/*---------------------------------------------------------------------
 * Abort a previous SCSI request
 *---------------------------------------------------------------------*/
int megaraid_abort (Scsi_Cmnd * SCpnt)
{
	mega_host_config *megaCfg;
	int rc;			/*, idx; */
	mega_scb *pScb;

	rc = SCSI_ABORT_NOT_RUNNING;

	megaCfg = (mega_host_config *) SCpnt->host->hostdata;

	megaCfg->flag |= IN_ABORT;

	for (pScb = megaCfg->qPendingH; pScb; pScb = pScb->next) {
		if (pScb->SCpnt == SCpnt) {
			/* Found an aborting command */
#if DEBUG
			showMbox (pScb);
#endif

	/*
	 * If the command is queued to be issued to the firmware, abort the scsi cmd,
	 * If the command is already aborted in a previous call to the _abort entry
	 *  point, return SCSI_ABORT_SNOOZE, suggesting a reset.
	 * If the command is issued to the firmware, which might complete after
	 *  some time, we will mark the scb as aborted, and return to the mid layer,
	 *  that abort could not be done.
	 *  In the ISR, when this command actually completes, we will perform a normal
	 *  completion.
	 *
	 * Oct 27, 1999
	 */

			switch (pScb->state) {
			case SCB_ABORTED:	/* Already aborted */
				rc = SCSI_ABORT_SNOOZE;
				break;
			case SCB_ISSUED:	/* Waiting on ISR result */
				rc = SCSI_ABORT_NOT_RUNNING;
				pScb->state = SCB_ABORTED;
				break;
			case SCB_ACTIVE:	/* still on the pending queue */
				mega_freeSCB (megaCfg, pScb);
				SCpnt->result = (DID_ABORT << 16);
				if (megaCfg->qCompletedH == NULL) {
					megaCfg->qCompletedH =
					    megaCfg->qCompletedT = SCpnt;
				} else {
					megaCfg->qCompletedT->host_scribble =
					    (unsigned char *) SCpnt;
					megaCfg->qCompletedT = SCpnt;
				}
				megaCfg->qCompletedT->host_scribble =
				    (unsigned char *) NULL;
				megaCfg->qCcnt++;
				rc = SCSI_ABORT_SUCCESS;
				break;
			default:
				printk
				    ("megaraid_abort: unknown command state!!\n");
				rc = SCSI_ABORT_NOT_RUNNING;
				break;
			}
			break;
		}
	}

	megaCfg->flag &= ~IN_ABORT;

#if DEBUG
	if (megaCfg->flag & IN_QUEUE)
		printk ("ma:flag is in queue\n");
	if (megaCfg->qCompletedH == NULL)
		printk ("ma:qchead == null\n");
#endif

	/*
	 * This is required here to complete any completed requests to be communicated
	 * over to the mid layer.
	 * Calling just mega_rundoneq() did not work.
	 */
	if (megaCfg->qCompletedH) {
		SCpnt = megaCfg->qCompletedH;
		megaCfg->qCompletedH = (Scsi_Cmnd *) SCpnt->host_scribble;
		megaCfg->qCcnt--;

		SCpnt->host_scribble = (unsigned char *) NULL;
		/* Callback */
		callDone (SCpnt);
	}
	mega_rundoneq (megaCfg);

	return rc;
}

/*---------------------------------------------------------------------
 * Reset a previous SCSI request
 *---------------------------------------------------------------------*/

int megaraid_reset (Scsi_Cmnd * SCpnt, unsigned int rstflags)
{
	mega_host_config *megaCfg;
	int idx;
	int rc;
	mega_scb *pScb;

	rc = SCSI_RESET_NOT_RUNNING;
	megaCfg = (mega_host_config *) SCpnt->host->hostdata;

	megaCfg->flag |= IN_RESET;

	printk
	    ("megaraid_RESET: %.08lx cmd=%.02x <c=%d.t=%d.l=%d>, flag = %x\n",
	     SCpnt->serial_number, SCpnt->cmnd[0], SCpnt->channel,
	     SCpnt->target, SCpnt->lun, rstflags);

	TRACE (("RESET: %.08lx %.02x <%d.%d.%d>\n",
		SCpnt->serial_number, SCpnt->cmnd[0], SCpnt->channel,
		SCpnt->target, SCpnt->lun));

	/*
	 * Walk list of SCBs for any that are still outstanding
	 */
	for (idx = 0; idx < megaCfg->max_cmds; idx++) {
		if (megaCfg->scbList[idx].state != SCB_FREE) {
			SCpnt = megaCfg->scbList[idx].SCpnt;
			pScb = &megaCfg->scbList[idx];
			if (SCpnt != NULL) {
				pScb->state = SCB_RESET;
				break;
			}
		}
	}

	megaCfg->flag &= ~IN_RESET;

	mega_rundoneq (megaCfg);
	return rc;
}

#ifdef CONFIG_PROC_FS
/* Following code handles /proc fs  */
static int proc_printf (mega_host_config * megaCfg, const char *fmt, ...)
{
	va_list args;
	int i;

	if (megaCfg->procidx > PROCBUFSIZE)
		return 0;

	va_start (args, fmt);
	i = vsprintf ((megaCfg->procbuf + megaCfg->procidx), fmt, args);
	va_end (args);

	megaCfg->procidx += i;
	return i;
}

static int proc_read_config (char *page, char **start, off_t offset,
		  int count, int *eof, void *data)
{

	mega_host_config *megaCfg = (mega_host_config *) data;

	*start = page;

	if (megaCfg->productInfo.ProductName[0] != 0)
		proc_printf (megaCfg, "%s\n", megaCfg->productInfo.ProductName);

	proc_printf (megaCfg, "Controller Type: ");

	if (megaCfg->flag & BOARD_QUARTZ)
		proc_printf (megaCfg, "438/466/467/471/493\n");
	else
		proc_printf (megaCfg, "418/428/434\n");

	if (megaCfg->flag & BOARD_40LD)
		proc_printf (megaCfg,
			     "Controller Supports 40 Logical Drives\n");

	if (megaCfg->flag & BOARD_64BIT)
		proc_printf (megaCfg,
			     "Controller / Driver uses 64 bit memory addressing\n");

	proc_printf (megaCfg, "Base = %08x, Irq = %d, ", megaCfg->base,
		     megaCfg->host->irq);

	proc_printf (megaCfg, "Logical Drives = %d, Channels = %d\n",
		     megaCfg->numldrv, megaCfg->productInfo.SCSIChanPresent);

	proc_printf (megaCfg, "Version =%s:%s, DRAM = %dMb\n",
		     megaCfg->fwVer, megaCfg->biosVer,
		     megaCfg->productInfo.DramSize);

	proc_printf (megaCfg,
		     "Controller Queue Depth = %d, Driver Queue Depth = %d\n",
		     megaCfg->productInfo.MaxConcCmds, megaCfg->max_cmds);
	COPY_BACK;
	return count;
}

static int proc_read_stat (char *page, char **start, off_t offset,
		int count, int *eof, void *data)
{
	mega_host_config *megaCfg = (mega_host_config *) data;

	*start = page;

	proc_printf (megaCfg, "Statistical Information for this controller\n");
	proc_printf (megaCfg, "Interrupts Collected = %lu\n",
		     megaCfg->nInterrupts);

	proc_printf (megaCfg, "INTERFACE DISABLED\n");
	COPY_BACK;
	return count;

#if 0	// can cause buffer overrun with 40 logical drives and IO information
	for (i = 0; i < megaCfg->numldrv; i++) {
		proc_printf (megaCfg, "Logical Drive %d:\n", i);

		proc_printf (megaCfg,
			     "\tReads Issued = %lu, Writes Issued = %lu\n",
			     megaCfg->nReads[i], megaCfg->nWrites[i]);

		proc_printf (megaCfg,
			     "\tSectors Read = %lu, Sectors Written = %lu\n\n",
			     megaCfg->nReadBlocks[i], megaCfg->nWriteBlocks[i]);

	}

	COPY_BACK;
	return count;
#endif
}

static int proc_read_status (char *page, char **start, off_t offset,
		  int count, int *eof, void *data)
{
	mega_host_config *megaCfg = (mega_host_config *) data;
	*start = page;

	proc_printf (megaCfg, "TBD\n");
	COPY_BACK;
	return count;
}

static int proc_read_mbox (char *page, char **start, off_t offset,
		int count, int *eof, void *data)
{

	mega_host_config *megaCfg = (mega_host_config *) data;
	volatile mega_mailbox *mbox = megaCfg->mbox;

	*start = page;

	proc_printf (megaCfg, "Contents of Mail Box Structure\n");
	proc_printf (megaCfg, "  Fw Command   = 0x%02x\n", mbox->cmd);
	proc_printf (megaCfg, "  Cmd Sequence = 0x%02x\n", mbox->cmdid);
	proc_printf (megaCfg, "  No of Sectors= %04d\n", mbox->numsectors);
	proc_printf (megaCfg, "  LBA          = 0x%02x\n", mbox->lba);
	proc_printf (megaCfg, "  DTA          = 0x%08x\n", mbox->xferaddr);
	proc_printf (megaCfg, "  Logical Drive= 0x%02x\n", mbox->logdrv);
	proc_printf (megaCfg, "  No of SG Elmt= 0x%02x\n", mbox->numsgelements);
	proc_printf (megaCfg, "  Busy         = %01x\n", mbox->busy);
	proc_printf (megaCfg, "  Status       = 0x%02x\n", mbox->status);

	/* proc_printf(megaCfg, "Dump of MailBox\n");
	for (i = 0; i < 16; i++)
        	proc_printf(megaCfg, "%02x ",*(mbox + i));

	proc_printf(megaCfg, "\n\nNumber of Status = %02d\n",mbox->numstatus);

	for (i = 0; i < 46; i++) {
        	proc_printf(megaCfg,"%02d ",*(mbox + 16 + i));
        if (i%16)
                proc_printf(megaCfg,"\n");
	}

	if (!mbox->numsgelements) {
	        dta = phys_to_virt(mbox->xferaddr);
	        for (i = 0; i < mbox->numsgelements; i++)
	                if (dta) {
	                        proc_printf(megaCfg,"Addr = %08x\n", (ulong)*(dta + i));                        proc_printf(megaCfg,"Length = %08x\n",
	                                (ulong)*(dta + i + 4));
	                }
	}*/
	COPY_BACK;
	return count;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)	/*0x20300 */
#define CREATE_READ_PROC(string, fxn) create_proc_read_entry(string, \
                                         S_IRUSR | S_IFREG,\
                                         controller_proc_dir_entry,\
                                         fxn, megaCfg)
#else
#define CREATE_READ_PROC(string, fxn) create_proc_read_entry(string,S_IRUSR | S_IFREG, controller_proc_dir_entry, fxn, megaCfg)

static struct proc_dir_entry *
create_proc_read_entry (const char *string,
			int mode,
			struct proc_dir_entry *parent,
			read_proc_t * fxn, mega_host_config * megaCfg)
{
	struct proc_dir_entry *temp = NULL;

	temp = kmalloc (sizeof (struct proc_dir_entry), GFP_KERNEL);
	if (!temp)
		return NULL;
	memset (temp, 0, sizeof (struct proc_dir_entry));

	if ((temp->name = kmalloc (strlen (string) + 1, GFP_KERNEL)) == NULL) {
		kfree (temp);
		return NULL;
	}

	strcpy ((char *) temp->name, string);
	temp->namelen = strlen (string);
	temp->mode = mode; /*S_IFREG | S_IRUSR */ ;
	temp->data = (void *) megaCfg;
	temp->read_proc = fxn;
	proc_register (parent, temp);
	return temp;
}
#endif

static void mega_create_proc_entry (int index, struct proc_dir_entry *parent)
{
	u_char string[64] = { 0 };
	mega_host_config *megaCfg = megaCtlrs[index];
	struct proc_dir_entry *controller_proc_dir_entry = NULL;

	sprintf (string, "%d", index);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)	/*0x20300 */
	controller_proc_dir_entry =
	    megaCfg->controller_proc_dir_entry = proc_mkdir (string, parent);
#else
	controller_proc_dir_entry =
	    megaCfg->controller_proc_dir_entry =
	    create_proc_entry (string, S_IFDIR | S_IRUGO | S_IXUGO, parent);
#endif

	if (!controller_proc_dir_entry)
		printk ("\nmegaraid: proc_mkdir failed\n");
	else {
		megaCfg->proc_read =
		    CREATE_READ_PROC ("config", proc_read_config);
		megaCfg->proc_status =
		    CREATE_READ_PROC ("status", proc_read_status);
		megaCfg->proc_stat = CREATE_READ_PROC ("stat", proc_read_stat);
		megaCfg->proc_mbox =
		    CREATE_READ_PROC ("mailbox", proc_read_mbox);
	}

}
#endif				/* CONFIG_PROC_FS */

/*-------------------------------------------------------------
 * Return the disk geometry for a particular disk
 * Input:
 *   Disk *disk - Disk geometry
 *   kdev_t dev - Device node
 *   int *geom  - Returns geometry fields
 *     geom[0] = heads
 *     geom[1] = sectors
 *     geom[2] = cylinders
 *-------------------------------------------------------------*/
int megaraid_biosparam (Disk * disk, kdev_t dev, int *geom)
{
	int heads, sectors, cylinders;
	mega_host_config *megaCfg;

	/* Get pointer to host config structure */
	megaCfg = (mega_host_config *) disk->device->host->hostdata;

	if( IS_RAID_CH(megaCfg, disk->device->channel)) {
			/* Default heads (64) & sectors (32) */
			heads = 64;
			sectors = 32;
			cylinders = disk->capacity / (heads * sectors);

			/* Handle extended translation size for logical drives > 1Gb */
			if (disk->capacity >= 0x200000) {
				heads = 255;
				sectors = 63;
				cylinders = disk->capacity / (heads * sectors);
			}

			/* return result */
			geom[0] = heads;
			geom[1] = sectors;
			geom[2] = cylinders;
	}
	else {
		if( mega_partsize(disk, dev, geom) == 0 ) return 0;

		printk(KERN_WARNING
				"megaraid: invalid partition on this disk on channel %d\n",
				disk->device->channel);

		/* Default heads (64) & sectors (32) */
		heads = 64;
		sectors = 32;
		cylinders = disk->capacity / (heads * sectors);

		/* Handle extended translation size for logical drives > 1Gb */
		if (disk->capacity >= 0x200000) {
			heads = 255;
			sectors = 63;
			cylinders = disk->capacity / (heads * sectors);
		}

		/* return result */
		geom[0] = heads;
		geom[1] = sectors;
		geom[2] = cylinders;
	}

	return 0;
}

/*
 * Function : static int mega_partsize(Disk * disk, kdev_t dev, int *geom)
 *
 * Purpose : to determine the BIOS mapping used to create the partition
 *			table, storing the results (cyls, hds, and secs) in geom
 *
 * Note:	Code is picked from scsicam.h
 *
 * Returns : -1 on failure, 0 on success.
 */
static int
mega_partsize(Disk * disk, kdev_t dev, int *geom)
{
	struct buffer_head *bh;
	struct partition *p, *largest = NULL;
	int i, largest_cyl;
	int heads, cyls, sectors;
	int capacity = disk->capacity;

	int ma = MAJOR(dev);
	int mi = (MINOR(dev) & ~0xf);

	int block = 1024; 

	if(blksize_size[ma])
		block = blksize_size[ma][mi];
		
	if(!(bh = bread(MKDEV(ma,mi), 0, block)))
		return -1;

	if( *(unsigned short *)(bh->b_data + 510) == 0xAA55 ) {

		for( largest_cyl = -1, p = (struct partition *)(0x1BE + bh->b_data),
				i = 0; i < 4; ++i, ++p) {

			if (!p->sys_ind) continue;

			cyls = p->end_cyl + ((p->end_sector & 0xc0) << 2);

			if(cyls >= largest_cyl) {
				largest_cyl = cyls;
				largest = p;
			}
		}
	}

	if (largest) {
		heads = largest->end_head + 1;
		sectors = largest->end_sector & 0x3f;

		if (heads == 0 || sectors == 0) {
			brelse(bh);
			return -1;
		}

		cyls = capacity/(heads * sectors);

		geom[0] = heads;
		geom[1] = sectors;
		geom[2] = cyls;

		brelse(bh);
		return 0;
	}

	brelse(bh);
	return -1;
}


/*
 * This routine will be called when the use has done a forced shutdown on the
 * system. Flush the Adapter cache, that's the most we can do.
 */
static int megaraid_reboot_notify (struct notifier_block *this, unsigned long code,
			void *unused)
{
	struct Scsi_Host *pSHost;
	mega_host_config *megaCfg;
	mega_mailbox *mbox;
	u_char mboxData[16];
	int i;

	if (code == SYS_DOWN || code == SYS_HALT) {
		for (i = 0; i < numCtlrs; i++) {
			pSHost = megaCtlrs[i]->host;

			megaCfg = (mega_host_config *) pSHost->hostdata;
			mbox = (mega_mailbox *) mboxData;

			/* Flush cache to disk */
			memset (mbox, 0, 16);
			mboxData[0] = 0xA;

			/*
			 * Free irq, otherwise extra interrupt is generated
			 */
			free_irq (megaCfg->host->irq, megaCfg);

			/*
			   * Issue a blocking (interrupts disabled) command to
			   * the card
			 */
			megaIssueCmd (megaCfg, mboxData, NULL, 0);
		}
	}
	return NOTIFY_DONE;
}

static int mega_init_scb (mega_host_config * megacfg)
{
	int idx;

#if DEBUG
	if (megacfg->max_cmds >= MAX_COMMANDS) {
		printk ("megaraid:ctlr max cmds = %x : MAX_CMDS = %x",
			megacfg->max_cmds, MAX_COMMANDS);
	}
#endif

	for (idx = megacfg->max_cmds - 1; idx >= 0; idx--) {

		megacfg->scbList[idx].idx = idx;

		/*
		 * ISR will make this flag zero to indicate the command has been
		 * completed. This is only for user ioctl calls. Rest of the driver
		 * and the mid-layer operations are not connected with this flag.
		 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		megacfg->scbList[idx].sgList =
		    pci_alloc_consistent (megacfg->dev,
					  sizeof (mega_64sglist) * MAX_SGLIST,
					  &(megacfg->scbList[idx].
					    dma_sghandle64));

		megacfg->scbList[idx].sg64List =
		    (mega_64sglist *) megacfg->scbList[idx].sgList;
#else
		megacfg->scbList[idx].sgList = kmalloc (sizeof (mega_sglist) * MAX_SGLIST, GFP_ATOMIC | GFP_DMA);
#endif

		if (megacfg->scbList[idx].sgList == NULL) {
			printk (KERN_WARNING
				"Can't allocate sglist for id %d\n", idx);
			mega_freeSgList (megacfg);
			return -1;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		megacfg->scbList[idx].pthru = pci_alloc_consistent (megacfg->dev,
					  sizeof (mega_passthru),
					  &(megacfg->scbList[idx].
					    dma_passthruhandle64));

		if (megacfg->scbList[idx].pthru == NULL) {
			printk (KERN_WARNING
				"Can't allocate passthru for id %d\n", idx);
		}

		megacfg->scbList[idx].epthru =
			pci_alloc_consistent(
				megacfg->dev, sizeof(mega_ext_passthru),
				&(megacfg->scbList[idx].dma_ext_passthruhandle64)
			);

		if (megacfg->scbList[idx].epthru == NULL) {
			printk (KERN_WARNING
				"Can't allocate extended passthru for id %d\n", idx);
		}
		/* 
		 * Allocate a 256 Byte Bounce Buffer for handling INQ/RD_CAPA 
		 */
		megacfg->scbList[idx].bounce_buffer = pci_alloc_consistent (megacfg->dev,
					  256,
					  &(megacfg->scbList[idx].
					    dma_bounce_buffer));

		if (!megacfg->scbList[idx].bounce_buffer)
			printk
			    ("megaraid: allocation for bounce buffer failed\n");

		megacfg->scbList[idx].dma_type = M_RD_DMA_TYPE_NONE;
#endif

		if (idx < MAX_COMMANDS) {
			/*
			 * Link to free list
			 * lock not required since we are loading the driver, so no
			 * commands possible right now.
			 */
			enq_scb_freelist (megacfg, &megacfg->scbList[idx],
					  NO_LOCK, INTR_ENB);

		}
	}

	return 0;
}

/*
 * Enqueues a SCB
 */
static void enq_scb_freelist (mega_host_config * megacfg, mega_scb * scb, int lock,
		  int intr)
{

	if (lock == INTERNAL_LOCK || intr == INTR_DIS) {
		if (intr == INTR_DIS)
			spin_lock_irq (&megacfg->lock_free);
		else
			spin_lock (&megacfg->lock_free);
	}

	scb->state = SCB_FREE;
	scb->SCpnt = NULL;

	if (megacfg->qFreeH == (mega_scb *) NULL) {
		megacfg->qFreeH = megacfg->qFreeT = scb;
	} else {
		megacfg->qFreeT->next = scb;
		megacfg->qFreeT = scb;
	}

	megacfg->qFreeT->next = NULL;
	megacfg->qFcnt++;

	if (lock == INTERNAL_LOCK || intr == INTR_DIS) {
		if (intr == INTR_DIS)
			spin_unlock_irq (&megacfg->lock_free);
		else
			spin_unlock (&megacfg->lock_free);
	}
}

/*
 * Routines for the character/ioctl interface to the driver
 */
static int megadev_open (struct inode *inode, struct file *filep)
{
	MOD_INC_USE_COUNT;
	return 0;		/* success */
}

static int megadev_ioctl_entry (struct inode *inode, struct file *filep,
		     unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	/*
	 * We do not allow parallel ioctls to the driver as of now.
	 */
	down (&mimd_entry_mtx);
	ret = megadev_ioctl (inode, filep, cmd, arg);
	up (&mimd_entry_mtx);

	return ret;

}

static int megadev_ioctl (struct inode *inode, struct file *filep,
	       unsigned int cmd, unsigned long arg)
{
	int adapno;
	kdev_t dev;
	u32 inlen;
	struct uioctl_t ioc;
	char *kvaddr = NULL;
	int nadap = numCtlrs;
	u8 opcode;
	u32 outlen;
	int ret;
	u8 subopcode;
	Scsi_Cmnd *scsicmd;
	struct Scsi_Host *shpnt;
	char *uaddr;
	struct uioctl_t *uioc;
	dma_addr_t	dma_addr;
	u32		length;
	mega_host_config *megacfg = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)	/* 0x020400 */
	struct pci_dev pdev;
	struct pci_dev *pdevp = &pdev;
#else
	char *pdevp = NULL;
#endif
	IO_LOCK_T;

	if (!inode || !(dev = inode->i_rdev))
		return -EINVAL;

	if (_IOC_TYPE (cmd) != MEGAIOC_MAGIC)
		return (-EINVAL);

	/*
	 * Get the user ioctl structure
	 */
	ret = verify_area (VERIFY_WRITE, (char *) arg, sizeof (struct uioctl_t));

	if (ret)
		return ret;

	if(copy_from_user (&ioc, (char *) arg, sizeof (struct uioctl_t)))
		return -EFAULT;

	/*
	 * The first call the applications should make is to find out the
	 * number of controllers in the system. The next logical call should
	 * be for getting the list of controllers in the system as detected
	 * by the driver.
	 */

	/*
	 * Get the opcode and subopcode for the commands
	 */
	opcode = ioc.ui.fcs.opcode;
	subopcode = ioc.ui.fcs.subopcode;

	switch (opcode) {
	case M_RD_DRIVER_IOCTL_INTERFACE:
		switch (subopcode) {
		case MEGAIOC_QDRVRVER:	/* Query driver version */
			put_user (driver_ver, (u32 *) ioc.data);
			return 0;

		case MEGAIOC_QNADAP:	/* Get # of adapters */
			put_user (nadap, (int *) ioc.data);
			return nadap;

		case MEGAIOC_QADAPINFO:	/* Get adapter information */
			/*
			 * which adapter?
			 */
			adapno = ioc.ui.fcs.adapno;

			/*
			 * The adapter numbers do not start with 0, at least in
			 * the user space. This is just to make sure, 0 is not the
			 * default value which will refer to adapter 1. So the
			 * user needs to make use of macros MKADAP() and GETADAP()
			 * (See megaraid.h) while making ioctl() call.
			 */
			adapno = GETADAP (adapno);

			if (adapno >= numCtlrs)
				return (-ENODEV);

			ret = verify_area (VERIFY_WRITE,
					   ioc.data,
					   sizeof (struct mcontroller));
			if (ret)
				return ret;

			/*
			 * Copy struct mcontroller to user area
			 */
			if (copy_to_user (ioc.data,
				      mcontroller + adapno,
				      sizeof (struct mcontroller)))
				      return -EFAULT;
			return 0;

		default:
			return (-EINVAL);

		}		/* inner switch */
		break;

	case M_RD_IOCTL_CMD_NEW:

		/*
		 * Deletion of logical drives is only handled in 0x80 commands
		 */
		if( ioc.mbox[0] == FC_DEL_LOGDRV && ioc.mbox[2] == OP_DEL_LOGDRV ) {
			return -EINVAL;
		}

		/* which adapter?  */
		adapno = ioc.ui.fcs.adapno;

		/* See comment above: MEGAIOC_QADAPINFO */
		adapno = GETADAP(adapno);

		if (adapno >= numCtlrs)
			return(-ENODEV);

		length = ioc.ui.fcs.length;

		/* Check for zero length buffer or very large buffers */
		if( !length || length > 32*1024 )
			return -EINVAL;

		/* save the user address */
		uaddr = ioc.ui.fcs.buffer;

		/*
		 * For M_RD_IOCTL_CMD_NEW commands, the fields outlen and inlen of
		 * uioctl_t structure are treated as flags. If outlen is 1, the
		 * data is transferred from the device and if inlen is 1, the data
		 * is transferred to the device.
		 */
		outlen = ioc.outlen;
		inlen = ioc.inlen;

		if(outlen) {
			ret = verify_area(VERIFY_WRITE, (char *)ioc.ui.fcs.buffer, length);
			if (ret) return ret;
		}
		if(inlen) {
			ret = verify_area(VERIFY_READ, (char *) ioc.ui.fcs.buffer, length);
			if (ret) return ret;
		}

		/*
		 * Find this host
		 */
		for( shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next ) {
			if( shpnt->hostdata == (unsigned long *)megaCtlrs[adapno] ) {
				megacfg = (mega_host_config *)shpnt->hostdata;
				break;
			}
		}
		if(shpnt == NULL)  return -ENODEV;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		scsicmd = (Scsi_Cmnd *)kmalloc(sizeof(Scsi_Cmnd), GFP_KERNEL);
#else
		scsicmd = (Scsi_Cmnd *)scsi_init_malloc(sizeof(Scsi_Cmnd),
							  GFP_ATOMIC | GFP_DMA);
#endif
		if(scsicmd == NULL) return -ENOMEM;

		memset(scsicmd, 0, sizeof(Scsi_Cmnd));
		scsicmd->host = shpnt;

		if( outlen || inlen ) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			pdevp = &pdev;
			memcpy(pdevp, megacfg->dev, sizeof(struct pci_dev));
			pdevp->dma_mask = 0xffffffff;
#else
			pdevp = NULL;
#endif
			kvaddr = dma_alloc_consistent(pdevp, length, &dma_addr);

			if( kvaddr == NULL ) {
				printk(KERN_WARNING "megaraid:allocation failed\n");
				ret = -ENOMEM;
				goto out_ioctl_cmd_new;
			}

			ioc.ui.fcs.buffer = kvaddr;

			if (inlen) {
				/* copyin the user data */
				if( copy_from_user(kvaddr, (char *)uaddr, length ) ) {
						ret = -EFAULT;
						goto out_ioctl_cmd_new;
				}
			}
		}

		scsicmd->cmnd[0] = MEGADEVIOC;
		scsicmd->request_buffer = (void *)&ioc;

		init_MUTEX_LOCKED(&mimd_ioctl_sem);

		IO_LOCK;
		megaraid_queue(scsicmd, megadev_ioctl_done);

		IO_UNLOCK;

		down(&mimd_ioctl_sem);

		if( !scsicmd->result && outlen ) {
			if (copy_to_user(uaddr, kvaddr, length)) {
				ret = -EFAULT;
				goto out_ioctl_cmd_new;
			}
		}

		/*
		 * copyout the result
		 */
		uioc = (struct uioctl_t *)arg;

		if( ioc.mbox[0] == MEGA_MBOXCMD_PASSTHRU ) {
			put_user( scsicmd->result, &uioc->pthru.scsistatus );
			if (copy_to_user( uioc->pthru.reqsensearea, scsicmd->sense_buffer,
							  MAX_REQ_SENSE_LEN ))
				ret= -EFAULT;
		} else {
			put_user(1, &uioc->mbox[16]);	/* numstatus */
			/* status */
			put_user (scsicmd->result, &uioc->mbox[17]);
		}

out_ioctl_cmd_new:

		if (kvaddr) {
			dma_free_consistent(pdevp, length, kvaddr, dma_addr);
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)	/*0x20400 */
		kfree (scsicmd);
#else
		scsi_init_free((char *)scsicmd, sizeof(Scsi_Cmnd));
#endif

		/* restore the user address */
		ioc.ui.fcs.buffer = uaddr;

		return ret;

	case M_RD_IOCTL_CMD:
		/* which adapter?  */
		adapno = ioc.ui.fcs.adapno;

		/* See comment above: MEGAIOC_QADAPINFO */
		adapno = GETADAP (adapno);

		if (adapno >= numCtlrs)
			return (-ENODEV);

		/* save the user address */
		uaddr = ioc.data;
		outlen = ioc.outlen;
		inlen = ioc.inlen;

		if ((outlen >= IOCTL_MAX_DATALEN) || (inlen >= IOCTL_MAX_DATALEN))
			return (-EINVAL);

		if (outlen) {
			ret = verify_area (VERIFY_WRITE, ioc.data, outlen);
			if (ret) return ret;
		}
		if (inlen) {
			ret = verify_area (VERIFY_READ, ioc.data, inlen);
			if (ret) return ret;
		}

		/*
		 * Find this host
		 */
		for( shpnt = scsi_hostlist; shpnt; shpnt = shpnt->next ) {
			if( shpnt->hostdata == (unsigned long *)megaCtlrs[adapno] ) {
				megacfg = (mega_host_config *)shpnt->hostdata;
				break;
			}
		}
		if(shpnt == NULL)  return -ENODEV;

		/*
		 * ioctls for deleting logical drives is a special case, so check
		 * for it first
		 */
		if( ioc.mbox[0] == FC_DEL_LOGDRV && ioc.mbox[2] == OP_DEL_LOGDRV ) {

			if( !megacfg->support_random_del ) {
				printk("megaraid: logdrv delete on non supporting f/w.\n");
				return -EINVAL;
			}

			uioc = (struct uioctl_t *)arg;

			ret = mega_del_logdrv(megacfg, ioc.mbox[3]);

			put_user(1, &uioc->mbox[16]);	/* numstatus */
			put_user(ret, &uioc->mbox[17]);	/* status */

			/* if deletion failed, let the user know by failing ioctl */
			return ret;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		scsicmd = (Scsi_Cmnd *)kmalloc(sizeof(Scsi_Cmnd), GFP_KERNEL);
#else
		scsicmd = (Scsi_Cmnd *)scsi_init_malloc(sizeof(Scsi_Cmnd),
							  GFP_ATOMIC | GFP_DMA);
#endif
		if(scsicmd == NULL) return -ENOMEM;

		memset(scsicmd, 0, sizeof(Scsi_Cmnd));
		scsicmd->host = shpnt;

		if (outlen || inlen) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			pdevp = &pdev;
			memcpy(pdevp, megacfg->dev, sizeof(struct pci_dev));
			pdevp->dma_mask = 0xffffffff;
#else
			pdevp = NULL;
#endif
			/*
			 * Allocate a page of kernel space.
			 */
			kvaddr = dma_alloc_consistent(pdevp, PAGE_SIZE, &dma_addr);

			if( kvaddr == NULL ) {
				printk (KERN_WARNING "megaraid:allocation failed\n");
				ret = -ENOMEM;
				goto out_ioctl_cmd;
			}

			ioc.data = kvaddr;

			if (inlen) {
				if (ioc.mbox[0] == MEGA_MBOXCMD_PASSTHRU) {
					/* copyin the user data */
					if( copy_from_user (kvaddr, uaddr, ioc.pthru.dataxferlen)){
							ret = -EFAULT;
							goto out_ioctl_cmd;
					}
				} else {
					if( copy_from_user (kvaddr, uaddr, inlen) ) {
							ret = -EFAULT;
							goto out_ioctl_cmd;
					}
				}
			}
		}

		scsicmd->cmnd[0] = MEGADEVIOC;
		scsicmd->request_buffer = (void *) &ioc;

		init_MUTEX_LOCKED (&mimd_ioctl_sem);

		IO_LOCK;
		megaraid_queue (scsicmd, megadev_ioctl_done);

		IO_UNLOCK;
		down (&mimd_ioctl_sem);

		if (!scsicmd->result && outlen) {
			if (ioc.mbox[0] == MEGA_MBOXCMD_PASSTHRU) {
				if (copy_to_user (uaddr, kvaddr, ioc.pthru.dataxferlen)) {
					ret = -EFAULT;
					goto out_ioctl_cmd;
				}
			} else {
				if (copy_to_user (uaddr, kvaddr, outlen)) {
					ret = -EFAULT;
					goto out_ioctl_cmd;
				}
			}
		}

		/*
		 * copyout the result
		 */
		uioc = (struct uioctl_t *) arg;

		if (ioc.mbox[0] == MEGA_MBOXCMD_PASSTHRU) {
			put_user (scsicmd->result, &uioc->pthru.scsistatus);

			/*
			 * If scsicmd->result is 0x02 (CHECK CONDITION) then copy the
			 * SCSI sense data into user area
			 */
			if (copy_to_user( uioc->pthru.reqsensearea, scsicmd->sense_buffer,
							  MAX_REQ_SENSE_LEN ))
				ret = -EFAULT;
							
		} else {
			put_user (1, &uioc->mbox[16]);	/* numstatus */
			put_user (scsicmd->result, &uioc->mbox[17]); /* status */
		}

out_ioctl_cmd:

		if (kvaddr) {
			dma_free_consistent(pdevp, PAGE_SIZE, kvaddr, dma_addr );
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		kfree (scsicmd);
#else
		scsi_init_free((char *)scsicmd, sizeof(Scsi_Cmnd));
#endif

		/* restore user pointer */
		ioc.data = uaddr;

		return ret;

	default:
		return (-EINVAL);

	}/* Outer switch */

	return 0;
}

static void
megadev_ioctl_done(Scsi_Cmnd *sc)
{
	up (&mimd_ioctl_sem);
}

static mega_scb *
megadev_doioctl (mega_host_config * megacfg, Scsi_Cmnd * sc)
{
	u8 cmd;
	struct uioctl_t *ioc = NULL;
	mega_mailbox *mbox = NULL;
	mega_ioctl_mbox *mboxioc = NULL;
	struct mbox_passthru *mboxpthru = NULL;
	mega_scb *scb = NULL;
	mega_passthru *pthru = NULL;

	if ((scb = mega_allocateSCB (megacfg, sc)) == NULL) {
		sc->result = (DID_ERROR << 16);
		callDone (sc);
		return NULL;
	}

	ioc = (struct uioctl_t *) sc->request_buffer;

	memcpy (scb->mboxData, ioc->mbox, sizeof (scb->mboxData));

	/* The generic mailbox */
	mbox = (mega_mailbox *) ioc->mbox;

	/*
	 * Get the user command
	 */
	cmd = ioc->mbox[0];

	switch (cmd) {
	case MEGA_MBOXCMD_PASSTHRU:
		/*
		   * prepare the SCB with information from the user ioctl structure
		 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		pthru = scb->pthru;
#else
		pthru = &scb->pthru;
#endif
		memcpy (pthru, &ioc->pthru, sizeof (mega_passthru));
		mboxpthru = (struct mbox_passthru *) scb->mboxData;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		if (megacfg->flag & BOARD_64BIT) {
			/* This is just a sample with one element 
			   * This if executes onlu on 2.4 kernels
			 */
			mboxpthru->dataxferaddr = scb->dma_passthruhandle64;
			scb->sg64List[0].address =
			    pci_map_single (megacfg->dev,
					    ioc->data,
					    4096, PCI_DMA_BIDIRECTIONAL);
			scb->sg64List[0].length = 4096;	// TODO: Check this
			pthru->dataxferaddr = scb->dma_sghandle64;
			pthru->numsgelements = 1;
			mboxpthru->cmd = MEGA_MBOXCMD_PASSTHRU64;
		} else {
			mboxpthru->dataxferaddr = scb->dma_passthruhandle64;
			pthru->dataxferaddr =
			    pci_map_single (megacfg->dev,
					    ioc->data,
					    4096, PCI_DMA_BIDIRECTIONAL);
			pthru->numsgelements = 0;
		}

#else
		{
			mboxpthru->dataxferaddr = virt_to_bus (&scb->pthru);
			pthru->dataxferaddr = virt_to_bus (ioc->data);
			pthru->numsgelements = 0;
		}
#endif

		pthru->reqsenselen = 14;
		break;

	default:		/* Normal command */
		mboxioc = (mega_ioctl_mbox *) scb->mboxData;

		if (ioc->ui.fcs.opcode == M_RD_IOCTL_CMD_NEW) {
			scb->buff_ptr = ioc->ui.fcs.buffer;
			scb->iDataSize = ioc->ui.fcs.length;
		} else {
			scb->buff_ptr = ioc->data;
			scb->iDataSize = 4096;	// TODO:check it
		}

		set_mbox_xfer_addr (megacfg, scb, mboxioc, FROMTO_DEVICE);
		mboxioc->numsgelements = 0;
		break;
	}

	return scb;
}

static int
megadev_close (struct inode *inode, struct file *filep)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}


static int
mega_support_ext_cdb(mega_host_config *this_hba)
{
	mega_mailbox *mboxpnt;
	unsigned char mbox[16];
	int ret;

	mboxpnt = (mega_mailbox *) mbox;

	memset(mbox, 0, sizeof (mbox));
	/*
	 * issue command to find out if controller supports extended CDBs.
	 */
	mbox[0] = 0xA4;
	mbox[2] = 0x16;

	ret = megaIssueCmd(this_hba, mbox, NULL, 0);

	return !ret;
}


/*
 * Find out if this controller supports random deletion and addition of
 * logical drives
 */
static int
mega_support_random_del(mega_host_config *this_hba)
{
	mega_mailbox *mboxpnt;
	unsigned char mbox[16];
	int ret;

	mboxpnt = (mega_mailbox *)mbox;

	memset(mbox, 0, sizeof(mbox));

	/*
	 * issue command
	 */
	mbox[0] = FC_DEL_LOGDRV;
	mbox[2] = OP_SUP_DEL_LOGDRV;

	ret = megaIssueCmd(this_hba, mbox, NULL, 0);

	return !ret;
}

static int
mega_del_logdrv(mega_host_config *this_hba, int logdrv)
{
	int		rval;
	IO_LOCK_T;
	DECLARE_WAIT_QUEUE_HEAD(wq);
	mega_scb	*scbp;

	/*
	 * Stop sending commands to the controller, queue them internally.
	 * When deletion is complete, ISR will flush the queue.
	 */
	IO_LOCK;
	this_hba->quiescent = 1;
	IO_UNLOCK;

	while( this_hba->qPcnt ) {
			sleep_on_timeout( &wq, 1*HZ );	/* sleep for 1s */
	}
	rval = mega_do_del_logdrv(this_hba, logdrv);

	IO_LOCK;
	/*
	 * Attach the internal queue to the pending queue
	 */
	if( this_hba->qPendingH == NULL ) {
		/*
		 * If pending queue head is null, make internal queue as
		 * pending queue
		 */
		this_hba->qPendingH = this_hba->int_qh;
		this_hba->qPendingT = this_hba->int_qt;
		this_hba->qPcnt = this_hba->int_qlen;
	}
	else {
		/*
		 * Append pending queue to internal queue
		 */
		if( this_hba->int_qt ) {
			this_hba->int_qt->next = this_hba->qPendingH;

			this_hba->qPendingH = this_hba->int_qh;
			this_hba->qPcnt += this_hba->int_qlen;
		}
	}

	this_hba->int_qh = this_hba->int_qt = NULL;
	this_hba->int_qlen = 0;

	/*
	 * If delete operation was successful, add 0x80 to the logical drive
	 * ids for commands in the pending queue.
	 */
	if( this_hba->read_ldidmap) {
		for( scbp = this_hba->qPendingH; scbp; scbp = scbp->next ) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			if( scbp->pthru->logdrv < 0x80 )
				scbp->pthru->logdrv += 0x80;
#else
			if( scbp->pthru.logdrv < 0x80 )
				scbp->pthru.logdrv += 0x80;
#endif
		}
	}
	this_hba->quiescent = 0;

	IO_UNLOCK;

	return rval;
}


static int
mega_do_del_logdrv(mega_host_config *this_hba, int logdrv)
{
	mega_mailbox *mboxpnt;
	unsigned char mbox[16];
	int rval;

	mboxpnt = (mega_mailbox *)mbox;

	memset(mbox, 0, sizeof(mbox));

	mbox[0] = FC_DEL_LOGDRV;
	mbox[2] = OP_DEL_LOGDRV;
	mbox[3] = logdrv;

	rval = megaIssueCmd(this_hba, mbox, NULL, 0);

	/* log this event */
	if( rval != 0 ) {
		printk("megaraid: Attempt to delete logical drive %d failed.",
				logdrv);
		return rval;
	}

	/*
	 * After deleting first logical drive, the logical drives must be
	 * addressed by adding 0x80 to the logical drive id.
	 */
	this_hba->read_ldidmap = 1;

	return rval;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
void *
dma_alloc_consistent(void *dev, size_t size, dma_addr_t *dma_addr)
{
	void	*_tv;
	int		npages;
	int		order = 0;

	/*
	 * How many pages application needs
	 */
	npages = size / PAGE_SIZE;

	/* Do we need one more page */
	if(size % PAGE_SIZE)
		npages++;

	order = mega_get_order(npages);

	_tv = (void *)__get_free_pages(GFP_DMA, order);

	if( _tv != NULL ) {
		memset(_tv, 0, size);
		*(dma_addr) = virt_to_bus(_tv);
	}

	return _tv;
}

/*
 * int mega_get_order(int)
 *
 * returns the order to be used as 2nd argument to __get_free_pages() - which
 * return pages equal to pow(2, order) - AM
 */
int
mega_get_order(int n)
{
	int		i = 0;

	while( pow_2(i++) < n )
		; /* null statement */

	return i-1;
}

/*
 * int pow_2(int)
 *
 * calculates pow(2, i)
 */
int
pow_2(int i)
{
	unsigned int	v = 1;
	
	while(i--)
		v <<= 1;

	return v;
}

void
dma_free_consistent(void *dev, size_t size, void *vaddr, dma_addr_t dma_addr)
{
	int		npages;
	int		order = 0;

	npages = size / PAGE_SIZE;

	if(size % PAGE_SIZE)
		npages++;

	if (npages == 1)
		order = 0;
	else if (npages == 2)
		order = 1;
	else if (npages <= 4)
		order = 2;
	else
		order = 3;

	free_pages((unsigned long)vaddr, order);

}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static
#endif				/* LINUX VERSION 2.4.XX */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) || defined(MODULE)
Scsi_Host_Template driver_template = MEGARAID;

#include "scsi_module.c"
#endif				/* LINUX VERSION 2.4.XX || MODULE */

/* vi: set ts=4: */
