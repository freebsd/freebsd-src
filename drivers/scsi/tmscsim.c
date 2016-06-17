/***********************************************************************
 *	FILE NAME : TMSCSIM.C					       *
 *	     BY   : C.L. Huang,  ching@tekram.com.tw		       *
 *	Description: Device Driver for Tekram DC-390(T) PCI SCSI       *
 *		     Bus Master Host Adapter			       *
 * (C)Copyright 1995-1996 Tekram Technology Co., Ltd.		       *
 ***********************************************************************/
/* (C) Copyright: put under GNU GPL in 10/96  (see README.tmscsim)	*
*************************************************************************/
/* $Id: tmscsim.c,v 2.60.2.30 2000/12/20 01:07:12 garloff Exp $		*/
/*	Enhancements and bugfixes by					*
 *	Kurt Garloff <kurt@garloff.de>	<garloff@suse.de>		*
 ***********************************************************************/
/*	HISTORY:							*
 *									*
 *	REV#	DATE	NAME	DESCRIPTION				*
 *	1.00  96/04/24	CLH	First release				*
 *	1.01  96/06/12	CLH	Fixed bug of Media Change for Removable *
 *				Device, scan all LUN. Support Pre2.0.10 *
 *	1.02  96/06/18	CLH	Fixed bug of Command timeout ...	*
 *	1.03  96/09/25	KG	Added tmscsim_proc_info()		*
 *	1.04  96/10/11	CLH	Updating for support KV 2.0.x		*
 *	1.05  96/10/18	KG	Fixed bug in DC390_abort(null ptr deref)*
 *	1.06  96/10/25	KG	Fixed module support			*
 *	1.07  96/11/09	KG	Fixed tmscsim_proc_info()		*
 *	1.08  96/11/18	KG	Fixed null ptr in DC390_Disconnect()	*
 *	1.09  96/11/30	KG	Added register the allocated IO space	*
 *	1.10  96/12/05	CLH	Modified tmscsim_proc_info(), and reset *
 *				pending interrupt in DC390_detect()	*
 *	1.11  97/02/05	KG/CLH	Fixeds problem with partitions greater	*
 *				than 1GB				*
 *	1.12  98/02/15  MJ      Rewritten PCI probing			*
 *	1.13  98/04/08	KG	Support for non DC390, __initfunc decls,*
 *				changed max devs from 10 to 16		*
 *	1.14a 98/05/05	KG	Dynamic DCB allocation, add-single-dev	*
 *				for LUNs if LUN_SCAN (BIOS) not set	*
 *				runtime config using /proc interface	*
 *	1.14b 98/05/06	KG	eliminated cli (); sti (); spinlocks	*
 *	1.14c 98/05/07	KG	2.0.x compatibility			*
 *	1.20a 98/05/07	KG	changed names of funcs to be consistent *
 *				DC390_ (entry points), dc390_ (internal)*
 *				reworked locking			*
 *	1.20b 98/05/12	KG	bugs: version, kfree, _ctmp		*
 *				debug output				*
 *	1.20c 98/05/12	KG	bugs: kfree, parsing, EEpromDefaults	*
 *	1.20d 98/05/14	KG	bugs: list linkage, clear flag after  	*
 *				reset on startup, code cleanup		*
 *	1.20e 98/05/15	KG	spinlock comments, name space cleanup	*
 *				pLastDCB now part of ACB structure	*
 *				added stats, timeout for 2.1, TagQ bug	*
 *				RESET and INQUIRY interface commands	*
 *	1.20f 98/05/18	KG	spinlocks fixes, max_lun fix, free DCBs	*
 *				for missing LUNs, pending int		*
 *	1.20g 98/05/19	KG	Clean up: Avoid short			*
 *	1.20h 98/05/21	KG	Remove AdaptSCSIID, max_lun ...		*
 *	1.20i 98/05/21	KG	Aiiie: Bug with TagQMask       		*
 *	1.20j 98/05/24	KG	Handle STAT_BUSY, handle pACB->pLinkDCB	*
 *				== 0 in remove_dev and DoingSRB_Done	*
 *	1.20k 98/05/25	KG	DMA_INT	(experimental)	       		*
 *	1.20l 98/05/27	KG	remove DMA_INT; DMA_IDLE cmds added;	*
 *	1.20m 98/06/10	KG	glitch configurable; made some global	*
 *				vars part of ACB; use DC390_readX	*
 *	1.20n 98/06/11	KG	startup params				*
 *	1.20o 98/06/15	KG	added TagMaxNum to boot/module params	*
 *				Device Nr -> Idx, TagMaxNum power of 2  *
 *	1.20p 98/06/17	KG	Docu updates. Reset depends on settings *
 *				pci_set_master added; 2.0.xx: pcibios_*	*
 *				used instead of MechNum things ...	*
 *	1.20q 98/06/23	KG	Changed defaults. Added debug code for	*
 *				removable media and fixed it. TagMaxNum	*
 *				fixed for DC390. Locking: ACB, DRV for	*
 *				better IRQ sharing. Spelling: Queueing	*
 *				Parsing and glitch_cfg changes. Display	*
 *				real SyncSpeed value. Made DisConn	*
 *				functional (!)				*
 *	1.20r 98/06/30	KG	Debug macros, allow disabling DsCn, set	*
 *				BIT4 in CtrlR4, EN_PAGE_INT, 2.0 module	*
 *				param -1 fixed.				*
 *	1.20s 98/08/20	KG	Debug info on abort(), try to check PCI,*
 *				phys_to_bus instead of phys_to_virt,	*
 *				fixed sel. process, fixed locking,	*
 *				added MODULE_XXX infos, changed IRQ	*
 *				request flags, disable DMA_INT		*
 *	1.20t 98/09/07	KG	TagQ report fixed; Write Erase DMA Stat;*
 *				initfunc -> __init; better abort;	*
 *				Timeout for XFER_DONE & BLAST_COMPLETE;	*
 *				Allow up to 33 commands being processed *
 *	2.0a  98/10/14	KG	Max Cmnds back to 17. DMA_Stat clearing *
 *				all flags. Clear within while() loops	*
 *				in DataIn_0/Out_0. Null ptr in dumpinfo	*
 *				for pSRB==0. Better locking during init.*
 *				bios_param() now respects part. table.	*
 *	2.0b  98/10/24	KG	Docu fixes. Timeout Msg in DMA Blast.	*
 *				Disallow illegal idx in INQUIRY/REMOVE	*
 *	2.0c  98/11/19	KG	Cleaned up detect/init for SMP boxes, 	*
 *				Write Erase DMA (1.20t) caused problems	*
 *	2.0d  98/12/25	KG	Christmas release ;-) Message handling  *
 *				completely reworked. Handle target ini-	*
 *				tiated SDTR correctly.			*
 *	2.0d1 99/01/25	KG	Try to handle RESTORE_PTR		*
 *	2.0d2 99/02/08	KG	Check for failure of kmalloc, correct 	*
 *				inclusion of scsicam.h, DelayReset	*
 *	2.0d3 99/05/31	KG	DRIVER_OK -> DID_OK, DID_NO_CONNECT,	*
 *				detect Target mode and warn.		*
 *				pcmd->result handling cleaned up.	*
 *	2.0d4 99/06/01	KG	Cleaned selection process. Found bug	*
 *				which prevented more than 16 tags. Now:	*
 *				24. SDTR cleanup. Cleaner multi-LUN	*
 *				handling. Don't modify ControlRegs/FIFO	*
 *				when connected.				*
 *	2.0d5 99/06/01	KG	Clear DevID, Fix INQUIRY after cfg chg.	*
 *	2.0d6 99/06/02	KG	Added ADD special command to allow cfg.	*
 *				before detection. Reset SYNC_NEGO_DONE	*
 *				after a bus reset.			*
 *	2.0d7 99/06/03	KG	Fixed bugs wrt add,remove commands	*
 *	2.0d8 99/06/04	KG	Removed copying of cmnd into CmdBlock.	*
 *				Fixed Oops in _release().		*
 *	2.0d9 99/06/06	KG	Also tag queue INQUIRY, T_U_R, ...	*
 *				Allow arb. no. of Tagged Cmnds. Max 32	*
 *	2.0d1099/06/20	KG	TagMaxNo changes now honoured! Queueing *
 *				clearified (renamed ..) TagMask handling*
 *				cleaned.				*
 *	2.0d1199/06/28	KG	cmd->result now identical to 2.0d2	*
 *	2.0d1299/07/04	KG	Changed order of processing in IRQ	*
 *	2.0d1399/07/05	KG	Don't update DCB fields if removed	*
 *	2.0d1499/07/05	KG	remove_dev: Move kfree() to the end	*
 *	2.0d1599/07/12	KG	use_new_eh_code: 0, ULONG -> UINT where	*
 *				appropriate				*
 *	2.0d1699/07/13	KG	Reenable StartSCSI interrupt, Retry msg	*
 *	2.0d1799/07/15	KG	Remove debug msg. Disable recfg. when	*
 *				there are queued cmnds			*
 *	2.0d1899/07/18	KG	Selection timeout: Don't requeue	*
 *	2.0d1999/07/18	KG	Abort: Only call scsi_done if dequeued	*
 *	2.0d2099/07/19	KG	Rst_Detect: DoingSRB_Done		*
 *	2.0d2199/08/15	KG	dev_id for request/free_irq, cmnd[0] for*
 *				RETRY, SRBdone does DID_ABORT for the 	*
 *				cmd passed by DC390_reset()		*
 *	2.0d2299/08/25	KG	dev_id fixed. can_queue: 42		*
 *	2.0d2399/08/25	KG	Removed some debugging code. dev_id 	*
 *				now is set to pACB. Use u8,u16,u32. 	*
 *	2.0d2499/11/14	KG	Unreg. I/O if failed IRQ alloc. Call	*
 * 				done () w/ DID_BAD_TARGET in case of	*
 *				missing DCB. We	are old EH!!		*
 *	2.0d2500/01/15	KG	2.3.3x compat from Andreas Schultz	*
 *				set unique_id. Disable RETRY message.	*
 *	2.0d2600/01/29	KG	Go to new EH.				*
 *	2.0d2700/01/31	KG	... but maintain 2.0 compat.		*
 *				and fix DCB freeing			*
 *	2.0d2800/02/14	KG	Queue statistics fixed, dump special cmd*
 *				Waiting_Timer for failed StartSCSI	*
 *				New EH: Don't return cmnds to ML on RST *
 *				Use old EH (don't have new EH fns yet)	*
 * 				Reset: Unlock, but refuse to queue	*
 * 				2.3 __setup function			*
 *	2.0e  00/05/22	KG	Return residual for 2.3			*
 *	2.0e1 00/05/25	KG	Compile fixes for 2.3.99		*
 *	2.0e2 00/05/27	KG	Jeff Garzik's pci_enable_device()	*
 *	2.0e3 00/09/29	KG	Some 2.4 changes. Don't try Sync Nego	*
 *				before INQUIRY has reported ability. 	*
 *				Recognise INQUIRY as scanning command.	*
 *	2.0e4 00/10/13	KG	Allow compilation into 2.4 kernel	*
 *	2.0e5 00/11/17	KG	Store Inq.flags in DCB			*
 *	2.0e6 00/11/22  KG	2.4 init function (Thx to O.Schumann)	*
 * 				2.4 PCI device table (Thx to A.Richter)	*
 *	2.0e7 00/11/28	KG	Allow overriding of BIOS settings	*
 *	2.0f  00/12/20	KG	Handle failed INQUIRYs during scan	*
 ***********************************************************************/

/* Uncomment SA_INTERRUPT, if the driver refuses to share its IRQ with other devices */
#define DC390_IRQ SA_SHIRQ /* | SA_INTERRUPT */

/* DEBUG options */
//#define DC390_DEBUG0
//#define DC390_DEBUG1
//#define DC390_DCBDEBUG
//#define DC390_PARSEDEBUG
//#define DC390_REMOVABLEDEBUG
//#define DC390_LOCKDEBUG

/* Debug definitions */
#ifdef DC390_DEBUG0
# define DEBUG0(x) x;
#else
# define DEBUG0(x)
#endif
#ifdef DC390_DEBUG1
# define DEBUG1(x) x;
#else
# define DEBUG1(x)
#endif
#ifdef DC390_DCBDEBUG
# define DCBDEBUG(x) x;
#else
# define DCBDEBUG(x)
#endif
#ifdef DC390_PARSEDEBUG
# define PARSEDEBUG(x) x;
#else
# define PARSEDEBUG(x)
#endif
#ifdef DC390_REMOVABLEDEBUG
# define REMOVABLEDEBUG(x) x;
#else
# define REMOVABLEDEBUG(x)
#endif
#define DCBDEBUG1(x)

/* Includes */
#include <linux/module.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/blk.h>
#include <linux/timer.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"
#include "sd.h"
#include <linux/stat.h>
#include <scsi/scsicam.h>

#include "dc390.h"

#define PCI_DEVICE_ID_AMD53C974 	PCI_DEVICE_ID_AMD_SCSI

/* Locking */

/* Note: Starting from 2.1.9x, the mid-level scsi code issues a 
 * spinlock_irqsave (&io_request_lock) before calling the driver's 
 * routines, so we don't need to lock, except in the IRQ handler.
 * The policy 3, let the midlevel scsi code do the io_request_locks
 * and us locking on a driver specific lock, shouldn't hurt anybody; it
 * just causes a minor performance degradation for setting the locks.
 */

/* spinlock things
 * level 3: lock on both adapter specific locks and (global) io_request_lock
 * level 2: lock on adapter specific locks only
 * level 1: rely on the locking of the mid level code (io_request_lock)
 * undef  : traditional save_flags; cli; restore_flags;
 */

//#define DEBUG_SPINLOCKS 2	/* Set to 0, 1 or 2 in include/linux/spinlock.h */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,30)
# include <linux/init.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,30)
# include <linux/spinlock.h>
#else
# include <asm/spinlock.h>
#endif
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,93) 
# define USE_SPINLOCKS 1
# define NEW_PCI 1
#else
# undef NEW_PCI
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,30)
#  define USE_SPINLOCKS 2
# endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,99)
static struct pci_device_id tmscsim_pci_tbl[] __initdata = {
	{
		vendor: PCI_VENDOR_ID_AMD,
		device: PCI_DEVICE_ID_AMD53C974,
		subvendor: PCI_ANY_ID,
		subdevice: PCI_ANY_ID,
	},
	{ }		/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, tmscsim_pci_tbl);
#endif
	
#ifdef USE_SPINLOCKS

# if USE_SPINLOCKS == 3 /* both */

#  if defined (CONFIG_SMP) || DEBUG_SPINLOCKS > 0
#   define DC390_LOCKA_INIT { spinlock_t __unlocked = SPIN_LOCK_UNLOCKED; pACB->lock = __unlocked; };
#  else
#   define DC390_LOCKA_INIT
#  endif
   spinlock_t dc390_drvlock = SPIN_LOCK_UNLOCKED;

#  define DC390_AFLAGS unsigned long aflags;
#  define DC390_IFLAGS unsigned long iflags;
#  define DC390_DFLAGS unsigned long dflags; 

#  define DC390_LOCK_IO spin_lock_irqsave (&io_request_lock, iflags)
#  define DC390_UNLOCK_IO spin_unlock_irqrestore (&io_request_lock, iflags)

#  define DC390_LOCK_DRV spin_lock_irqsave (&dc390_drvlock, dflags)
#  define DC390_UNLOCK_DRV spin_unlock_irqrestore (&dc390_drvlock, dflags)
#  define DC390_LOCK_DRV_NI spin_lock (&dc390_drvlock)
#  define DC390_UNLOCK_DRV_NI spin_unlock (&dc390_drvlock)

#  define DC390_LOCK_ACB spin_lock_irqsave (&(pACB->lock), aflags)
#  define DC390_UNLOCK_ACB spin_unlock_irqrestore (&(pACB->lock), aflags)
#  define DC390_LOCK_ACB_NI spin_lock (&(pACB->lock))
#  define DC390_UNLOCK_ACB_NI spin_unlock (&(pACB->lock))
//#  define DC390_LOCKA_INIT spin_lock_init (&(pACB->lock))

# else

#  if USE_SPINLOCKS == 2 /* adapter specific locks */

#   if defined (CONFIG_SMP) || DEBUG_SPINLOCKS > 0
#    define DC390_LOCKA_INIT { spinlock_t __unlocked = SPIN_LOCK_UNLOCKED; pACB->lock = __unlocked; };
#   else
#    define DC390_LOCKA_INIT
#   endif
    spinlock_t dc390_drvlock = SPIN_LOCK_UNLOCKED;
#   define DC390_AFLAGS unsigned long aflags;
#   define DC390_IFLAGS 
#  define DC390_DFLAGS unsigned long dflags; 
#   define DC390_LOCK_IO /* spin_lock_irqsave (&io_request_lock, iflags) */
#   define DC390_UNLOCK_IO /* spin_unlock_irqrestore (&io_request_lock, iflags) */
#   define DC390_LOCK_DRV spin_lock_irqsave (&dc390_drvlock, dflags)
#   define DC390_UNLOCK_DRV spin_unlock_irqrestore (&dc390_drvlock, dflags)
#   define DC390_LOCK_DRV_NI spin_lock (&dc390_drvlock)
#   define DC390_UNLOCK_DRV_NI spin_unlock (&dc390_drvlock)
#   define DC390_LOCK_ACB spin_lock_irqsave (&(pACB->lock), aflags)
#   define DC390_UNLOCK_ACB spin_unlock_irqrestore (&(pACB->lock), aflags)
#   define DC390_LOCK_ACB_NI spin_lock (&(pACB->lock))
#   define DC390_UNLOCK_ACB_NI spin_unlock (&(pACB->lock))
//#   define DC390_LOCKA_INIT spin_lock_init (&(pACB->lock))

#  else /* USE_SPINLOCKS == 1: global lock io_request_lock */

#   define DC390_AFLAGS 
#   define DC390_IFLAGS unsigned long iflags;
#   define DC390_DFLAGS unsigned long dflags; 
    spinlock_t dc390_drvlock = SPIN_LOCK_UNLOCKED;
#   define DC390_LOCK_IO spin_lock_irqsave (&io_request_lock, iflags)
#   define DC390_UNLOCK_IO spin_unlock_irqrestore (&io_request_lock, iflags)
#   define DC390_LOCK_DRV spin_lock_irqsave (&dc390_drvlock, dflags)
#   define DC390_UNLOCK_DRV spin_unlock_irqrestore (&dc390_drvlock, dflags)
#   define DC390_LOCK_DRV_NI spin_lock (&dc390_drvlock)
#   define DC390_UNLOCK_DRV_NI spin_unlock (&dc390_drvlock)
#   define DC390_LOCK_ACB /* DC390_LOCK_IO */
#   define DC390_UNLOCK_ACB /* DC390_UNLOCK_IO */
#   define DC390_LOCK_ACB_NI /* spin_lock (&(pACB->lock)) */
#   define DC390_UNLOCK_ACB_NI /* spin_unlock (&(pACB->lock)) */
#   define DC390_LOCKA_INIT /* DC390_LOCKA_INIT */

#  endif /* 2 */
# endif /* 3 */

#else /* USE_SPINLOCKS undefined */

# define DC390_AFLAGS unsigned long aflags;
# define DC390_IFLAGS unsigned long iflags;
# define DC390_DFLAGS unsigned long dflags; 
# define DC390_LOCK_IO save_flags (iflags); cli ()
# define DC390_UNLOCK_IO restore_flags (iflags)
# define DC390_LOCK_DRV save_flags (dflags); cli ()
# define DC390_UNLOCK_DRV restore_flags (dflags)
# define DC390_LOCK_DRV_NI
# define DC390_UNLOCK_DRV_NI
# define DC390_LOCK_ACB save_flags (aflags); cli ()
# define DC390_UNLOCK_ACB restore_flags (aflags)
# define DC390_LOCK_ACB_NI
# define DC390_UNLOCK_ACB_NI
# define DC390_LOCKA_INIT
#endif /* def */


/* These macros are used for uniform access to 2.0.x and 2.1.x PCI config space*/

#ifdef NEW_PCI
# define PDEV pdev
# define PDEVDECL struct pci_dev *pdev
# define PDEVDECL0 struct pci_dev *pdev = NULL
# define PDEVDECL1 struct pci_dev *pdev
# define PDEVSET pACB->pdev=pdev
# define PDEVSET1 pdev=pACB->pdev
# define PCI_WRITE_CONFIG_BYTE(pd, rv, bv) pci_write_config_byte (pd, rv, bv)
# define PCI_READ_CONFIG_BYTE(pd, rv, bv) pci_read_config_byte (pd, rv, bv)
# define PCI_WRITE_CONFIG_WORD(pd, rv, bv) pci_write_config_word (pd, rv, bv)
# define PCI_READ_CONFIG_WORD(pd, rv, bv) pci_read_config_word (pd, rv, bv)
# define PCI_BUS_DEV pdev->bus->number, pdev->devfn
# define PCI_PRESENT pci_present ()
# define PCI_SET_MASTER pci_set_master (pdev)
# define PCI_FIND_DEVICE(vend, id) (pdev = pci_find_device (vend, id, pdev))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,10)
# define PCI_GET_IO_AND_IRQ io_port = pci_resource_start (pdev, 0); irq = pdev->irq
#else
# define PCI_GET_IO_AND_IRQ io_port = pdev->base_address[0] & PCI_BASE_ADDRESS_IO_MASK; irq = pdev->irq
#endif
#else
# include <linux/bios32.h>
# define PDEV pbus, pdevfn
# define PDEVDECL UCHAR pbus, UCHAR pdevfn
# define PDEVDECL0 UCHAR pbus = 0; UCHAR pdevfn = 0; USHORT pci_index = 0; int error
# define PDEVDECL1 UCHAR pbus; UCHAR pdevfn /*; USHORT pci_index */
# define PDEVSET pACB->pbus=pbus; pACB->pdevfn=pdevfn /*; pACB->pci_index=pci_index */
# define PDEVSET1 pbus=pACB->pbus; pdevfn=pACB->pdevfn /*; pci_index=pACB->pci_index */
# define PCI_WRITE_CONFIG_BYTE(pd, rv, bv) pcibios_write_config_byte (pd, rv, bv)
# define PCI_READ_CONFIG_BYTE(pd, rv, bv) pcibios_read_config_byte (pd, rv, bv)
# define PCI_WRITE_CONFIG_WORD(pd, rv, bv) pcibios_write_config_word (pd, rv, bv)
# define PCI_READ_CONFIG_WORD(pd, rv, bv) pcibios_read_config_word (pd, rv, bv)
# define PCI_BUS_DEV pbus, pdevfn
# define PCI_PRESENT pcibios_present ()
# define PCI_SET_MASTER dc390_set_master (pbus, pdevfn)
# define PCI_FIND_DEVICE(vend, id) (!pcibios_find_device (vend, id, pci_index++, &pbus, &pdevfn))
# define PCI_GET_IO_AND_IRQ error = pcibios_read_config_dword (pbus, pdevfn, PCI_BASE_ADDRESS_0, &io_port);	\
 error |= pcibios_read_config_byte (pbus, pdevfn, PCI_INTERRUPT_LINE, &irq);	\
 io_port &= 0xfffe;	\
 if (error) { printk (KERN_ERR "DC390_detect: Error reading PCI config registers!\n"); continue; }
#endif 

#include "tmscsim.h"

#ifndef __init
# define __init
#endif

UCHAR dc390_StartSCSI( PACB pACB, PDCB pDCB, PSRB pSRB );
void dc390_DataOut_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
void dc390_DataIn_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_Command_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_Status_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_MsgOut_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
void dc390_MsgIn_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_DataOutPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_DataInPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
void dc390_CommandPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_StatusPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
void dc390_MsgOutPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_MsgInPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_Nop_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void dc390_Nop_1( PACB pACB, PSRB pSRB, PUCHAR psstatus);

static void dc390_SetXferRate( PACB pACB, PDCB pDCB );
void dc390_Disconnect( PACB pACB );
void dc390_Reselect( PACB pACB );
void dc390_SRBdone( PACB pACB, PDCB pDCB, PSRB pSRB );
void dc390_DoingSRB_Done( PACB pACB, PSCSICMD cmd );
static void dc390_ScsiRstDetect( PACB pACB );
static void dc390_ResetSCSIBus( PACB pACB );
static void __inline__ dc390_RequestSense( PACB pACB, PDCB pDCB, PSRB pSRB );
static void __inline__ dc390_InvalidCmd( PACB pACB );
static void __inline__ dc390_EnableMsgOut_Abort (PACB, PSRB);
static void dc390_remove_dev (PACB pACB, PDCB pDCB);
void do_DC390_Interrupt( int, void *, struct pt_regs *);

int    dc390_initAdapter( PSH psh, ULONG io_port, UCHAR Irq, UCHAR index );
void   dc390_initDCB( PACB pACB, PDCB *ppDCB, UCHAR id, UCHAR lun);
void   dc390_updateDCB (PACB pACB, PDCB pDCB);

#ifdef MODULE
 static int DC390_release(struct Scsi_Host *host);
 static int dc390_shutdown (struct Scsi_Host *host);
#endif


//static PSHT	dc390_pSHT_start = NULL;
//static PSH	dc390_pSH_start = NULL;
//static PSH	dc390_pSH_current = NULL;
static PACB	dc390_pACB_start= NULL;
static PACB	dc390_pACB_current = NULL;
static ULONG	dc390_lastabortedpid = 0;
static UINT	dc390_laststatus = 0;
static UCHAR	dc390_adapterCnt = 0;

/* Startup values, to be overriden on the commandline */
int tmscsim[] = {-2, -2, -2, -2, -2, -2};

# if defined(MODULE) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,30)
MODULE_PARM(tmscsim, "1-6i");
MODULE_PARM_DESC(tmscsim, "Host SCSI ID, Speed (0=10MHz), Device Flags, Adapter Flags, Max Tags (log2(tags)-1), DelayReset (s)");
# endif

#if defined(MODULE) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,30)
MODULE_AUTHOR("C.L. Huang / Kurt Garloff");
MODULE_DESCRIPTION("SCSI host adapter driver for Tekram DC390 and other AMD53C974A based PCI SCSI adapters");
MODULE_LICENSE("GPL");

MODULE_SUPPORTED_DEVICE("sd,sr,sg,st");
#endif

static PVOID dc390_phase0[]={
       dc390_DataOut_0,
       dc390_DataIn_0,
       dc390_Command_0,
       dc390_Status_0,
       dc390_Nop_0,
       dc390_Nop_0,
       dc390_MsgOut_0,
       dc390_MsgIn_0,
       dc390_Nop_1
       };

static PVOID dc390_phase1[]={
       dc390_DataOutPhase,
       dc390_DataInPhase,
       dc390_CommandPhase,
       dc390_StatusPhase,
       dc390_Nop_0,
       dc390_Nop_0,
       dc390_MsgOutPhase,
       dc390_MsgInPhase,
       dc390_Nop_1
       };

#ifdef DC390_DEBUG1
static char* dc390_p0_str[] = {
       "dc390_DataOut_0",
       "dc390_DataIn_0",
       "dc390_Command_0",
       "dc390_Status_0",
       "dc390_Nop_0",
       "dc390_Nop_0",
       "dc390_MsgOut_0",
       "dc390_MsgIn_0",
       "dc390_Nop_1"
       };
     
static char* dc390_p1_str[] = {
       "dc390_DataOutPhase",
       "dc390_DataInPhase",
       "dc390_CommandPhase",
       "dc390_StatusPhase",
       "dc390_Nop_0",
       "dc390_Nop_0",
       "dc390_MsgOutPhase",
       "dc390_MsgInPhase",
       "dc390_Nop_1"
       };
#endif   

/* Devices erroneously pretending to be able to do TagQ */
UCHAR  dc390_baddevname1[2][28] ={
       "SEAGATE ST3390N         9546",
       "HP      C3323-300       4269"};
#define BADDEVCNT	2

static char*  dc390_adapname = "DC390";
UCHAR  dc390_eepromBuf[MAX_ADAPTER_NUM][EE_LEN];
UCHAR  dc390_clock_period1[] = {4, 5, 6, 7, 8, 10, 13, 20};
UCHAR  dc390_clock_speed[] = {100,80,67,57,50, 40, 31, 20};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,30)
struct proc_dir_entry	DC390_proc_scsi_tmscsim ={
       PROC_SCSI_DC390T, 7 ,"tmscsim",
       S_IFDIR | S_IRUGO | S_IXUGO, 2
       };
#endif

/***********************************************************************
 * Functions for access to DC390 EEPROM
 * and some to emulate it
 *
 **********************************************************************/


static void __init dc390_EnDisableCE( UCHAR mode, PDEVDECL, PUCHAR regval )
{
    UCHAR bval;

    bval = 0;
    if(mode == ENABLE_CE)
	*regval = 0xc0;
    else
	*regval = 0x80;
    PCI_WRITE_CONFIG_BYTE(PDEV, *regval, bval);
    if(mode == DISABLE_CE)
        PCI_WRITE_CONFIG_BYTE(PDEV, *regval, bval);
    udelay(160);
}


/* Override EEprom values with explicitly set values */
static void __init dc390_EEprom_Override (UCHAR index)
{
    PUCHAR ptr;
    UCHAR  id;
    ptr = (PUCHAR) dc390_eepromBuf[index];
    
    /* Adapter Settings */
    if (tmscsim[0] != -2)
	ptr[EE_ADAPT_SCSI_ID] = (UCHAR)tmscsim[0];	/* Adapter ID */
    if (tmscsim[3] != -2)
	ptr[EE_MODE2] = (UCHAR)tmscsim[3];
    if (tmscsim[5] != -2)
	ptr[EE_DELAY] = tmscsim[5];			/* Reset delay */
    if (tmscsim[4] != -2)
	ptr[EE_TAG_CMD_NUM] = (UCHAR)tmscsim[4];	/* Tagged Cmds */
    
    /* Device Settings */
    for (id = 0; id < MAX_SCSI_ID; id++)
    {
	if (tmscsim[2] != -2)
		ptr[id<<2] = (UCHAR)tmscsim[2];		/* EE_MODE1 */
	if (tmscsim[1] != -2)
		ptr[(id<<2) + 1] = (UCHAR)tmscsim[1];	/* EE_Speed */
    };
}

/* Handle "-1" case */
static void __init dc390_check_for_safe_settings (void)
{
	if (tmscsim[0] == -1 || tmscsim[0] > 15) /* modules-2.0.0 passes -1 as string */
	{
		tmscsim[0] = 7; tmscsim[1] = 4;
		tmscsim[2] = 0x09; tmscsim[3] = 0x0f;
		tmscsim[4] = 2; tmscsim[5] = 10;
		printk (KERN_INFO "DC390: Using safe settings.\n");
	}
}


#ifndef CONFIG_SCSI_DC390T_NOGENSUPP
int __initdata tmscsim_def[] = {7, 0 /* 10MHz */,
		PARITY_CHK_ | SEND_START_ | EN_DISCONNECT_
		| SYNC_NEGO_ | TAG_QUEUEING_,
		MORE2_DRV | GREATER_1G | RST_SCSI_BUS | ACTIVE_NEGATION
		/* | NO_SEEK */
# ifdef CONFIG_SCSI_MULTI_LUN
		| LUN_CHECK
# endif
		, 3 /* 16 Tags per LUN */, 1 /* s delay after Reset */ };

/* Copy defaults over set values where missing */
static void __init dc390_fill_with_defaults (void)
{
	int i;
	PARSEDEBUG(printk(KERN_INFO "DC390: setup %08x %08x %08x %08x %08x %08x\n", tmscsim[0],\
		      tmscsim[1], tmscsim[2], tmscsim[3], tmscsim[4], tmscsim[5]);)
	for (i = 0; i < 6; i++)
	{
		if (tmscsim[i] < 0 || tmscsim[i] > 255)
			tmscsim[i] = tmscsim_def[i];
	}
	/* Sanity checks */
	if (tmscsim[0] >   7) tmscsim[0] =   7;
	if (tmscsim[1] >   7) tmscsim[1] =   4;
	if (tmscsim[4] >   5) tmscsim[4] =   4;
	if (tmscsim[5] > 180) tmscsim[5] = 180;
};
#endif

/* Override defaults on cmdline:
 * tmscsim: AdaptID, MaxSpeed (Index), DevMode (Bitmapped), AdaptMode (Bitmapped)
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,13)
int __init dc390_setup (char *str)
{	
	int ints[8];
	int i, im;
	(void)get_options (str, ARRAY_SIZE(ints), ints);
	im = ints[0];
	if (im > 6)
	{
		printk (KERN_NOTICE "DC390: ignore extra params!\n");
		im = 6;
	};
	for (i = 0; i < im; i++)
		tmscsim[i] = ints[i+1];
	/* dc390_checkparams (); */
	return 1;
};
#ifndef MODULE
__setup("tmscsim=", dc390_setup);
#endif

#else
void __init dc390_setup (char *str, int *ints)
{
	int i, im;
	im = ints[0];
	if (im > 6)
	{
		printk (KERN_NOTICE "DC390: ignore extra params!\n");
		im = 6;
	};
	for (i = 0; i < im; i++)
		tmscsim[i] = ints[i+1];
	/* dc390_checkparams (); */
};
#endif



static void __init dc390_EEpromOutDI( PDEVDECL, PUCHAR regval, UCHAR Carry )
{
    UCHAR bval;

    bval = 0;
    if(Carry)
    {
	bval = 0x40;
	*regval = 0x80;
	PCI_WRITE_CONFIG_BYTE(PDEV, *regval, bval);
    }
    udelay(160);
    bval |= 0x80;
    PCI_WRITE_CONFIG_BYTE(PDEV, *regval, bval);
    udelay(160);
    bval = 0;
    PCI_WRITE_CONFIG_BYTE(PDEV, *regval, bval);
    udelay(160);
}


static UCHAR __init dc390_EEpromInDO( PDEVDECL )
{
    UCHAR bval;

    PCI_WRITE_CONFIG_BYTE(PDEV, 0x80, 0x80);
    udelay(160);
    PCI_WRITE_CONFIG_BYTE(PDEV, 0x80, 0x40);
    udelay(160);
    PCI_READ_CONFIG_BYTE(PDEV, 0x00, &bval);
    if(bval == 0x22)
	return(1);
    else
	return(0);
}


static USHORT __init dc390_EEpromGetData1( PDEVDECL )
{
    UCHAR i;
    UCHAR carryFlag;
    USHORT wval;

    wval = 0;
    for(i=0; i<16; i++)
    {
	wval <<= 1;
	carryFlag = dc390_EEpromInDO(PDEV);
	wval |= carryFlag;
    }
    return(wval);
}


static void __init dc390_Prepare( PDEVDECL, PUCHAR regval, UCHAR EEpromCmd )
{
    UCHAR i,j;
    UCHAR carryFlag;

    carryFlag = 1;
    j = 0x80;
    for(i=0; i<9; i++)
    {
	dc390_EEpromOutDI(PDEV,regval,carryFlag);
	carryFlag = (EEpromCmd & j) ? 1 : 0;
	j >>= 1;
    }
}


static void __init dc390_ReadEEprom( PDEVDECL, PUSHORT ptr)
{
    UCHAR   regval,cmd;
    UCHAR   i;

    cmd = EEPROM_READ;
    for(i=0; i<0x40; i++)
    {
	dc390_EnDisableCE(ENABLE_CE, PDEV, &regval);
	dc390_Prepare(PDEV, &regval, cmd++);
	*ptr++ = dc390_EEpromGetData1(PDEV);
	dc390_EnDisableCE(DISABLE_CE, PDEV, &regval);
    }
}


static void __init dc390_interpret_delay (UCHAR index)
{
    char interpd [] = {1,3,5,10,16,30,60,120};
    dc390_eepromBuf[index][EE_DELAY] = interpd [dc390_eepromBuf[index][EE_DELAY]];
};

static UCHAR __init dc390_CheckEEpromCheckSum( PDEVDECL, UCHAR index )
{
    UCHAR  i;
    char  EEbuf[128];
    USHORT wval, *ptr = (PUSHORT)EEbuf;

    dc390_ReadEEprom( PDEV, ptr );
    memcpy (dc390_eepromBuf[index], EEbuf, EE_ADAPT_SCSI_ID);
    memcpy (&dc390_eepromBuf[index][EE_ADAPT_SCSI_ID], 
	    &EEbuf[REAL_EE_ADAPT_SCSI_ID], EE_LEN - EE_ADAPT_SCSI_ID);
    dc390_interpret_delay (index);
    
    wval = 0;
    for(i=0; i<0x40; i++, ptr++)
	wval += *ptr;
    return (wval == 0x1234 ? 0 : 1);
}


/***********************************************************************
 * Functions for the management of the internal structures 
 * (DCBs, SRBs, Queueing)
 *
 **********************************************************************/
static PDCB __inline__ dc390_findDCB ( PACB pACB, UCHAR id, UCHAR lun)
{
   PDCB pDCB = pACB->pLinkDCB; if (!pDCB) return 0;
   while (pDCB->TargetID != id || pDCB->TargetLUN != lun)
     {
	pDCB = pDCB->pNextDCB;
	if (pDCB == pACB->pLinkDCB)
	  {
	     DCBDEBUG(printk (KERN_WARNING "DC390: DCB not found (DCB=%p, DCBmap[%2x]=%2x)\n",
		     pDCB, id, pACB->DCBmap[id]);)
	     return 0;
	  }
     };
   DCBDEBUG1( printk (KERN_DEBUG "DCB %p (%02x,%02x) found.\n",	\
		      pDCB, pDCB->TargetID, pDCB->TargetLUN);)
   return pDCB;
};

/* Queueing philosphy:
 * There are a couple of lists:
 * - Query: Contains the Scsi Commands not yet turned into SRBs (per ACB)
 *   (Note: For new EH, it is unnecessary!)
 * - Waiting: Contains a list of SRBs not yet sent (per DCB)
 * - Free: List of free SRB slots
 * 
 * If there are no waiting commands for the DCB, the new one is sent to the bus
 * otherwise the oldest one is taken from the Waiting list and the new one is 
 * queued to the Waiting List
 * 
 * Lists are managed using two pointers and eventually a counter
 */


#if 0
/* Look for a SCSI cmd in a SRB queue */
static PSRB dc390_find_cmd_in_SRBq (PSCSICMD cmd, PSRB queue)
{
    PSRB q = queue;
    while (q)
    {
	if (q->pcmd == cmd) return q;
	q = q->pNextSRB;
	if (q == queue) return 0;
    }
    return q;
};
#endif
    

/* Append to Query List */
static void dc390_Query_append( PSCSICMD cmd, PACB pACB )
{
    DEBUG0(printk ("DC390: Append cmd %li to Query\n", cmd->pid);)
    if( !pACB->QueryCnt )
	pACB->pQueryHead = cmd;
    else
	pACB->pQueryTail->next = cmd;

    pACB->pQueryTail = cmd;
    pACB->QueryCnt++;
    pACB->CmdOutOfSRB++;
    cmd->next = NULL;
}


/* Return next cmd from Query list */
static PSCSICMD dc390_Query_get ( PACB pACB )
{
    PSCSICMD  pcmd;

    pcmd = pACB->pQueryHead;
    if (!pcmd) return pcmd;
    DEBUG0(printk ("DC390: Get cmd %li from Query\n", pcmd->pid);)
    pACB->pQueryHead = pcmd->next;
    pcmd->next = NULL;
    if (!pACB->pQueryHead) pACB->pQueryTail = NULL;
    pACB->QueryCnt--;
    return( pcmd );
}


/* Return next free SRB */
static __inline__ PSRB dc390_Free_get ( PACB pACB )
{
    PSRB   pSRB;

    pSRB = pACB->pFreeSRB;
    DEBUG0(printk ("DC390: Get Free SRB %p\n", pSRB);)
    if( pSRB )
    {
	pACB->pFreeSRB = pSRB->pNextSRB;
	pSRB->pNextSRB = NULL;
    }

    return( pSRB );
}

/* Insert SRB oin top of free list */
static __inline__ void dc390_Free_insert (PACB pACB, PSRB pSRB)
{
    DEBUG0(printk ("DC390: Free SRB %p\n", pSRB);)
    pSRB->pNextSRB = pACB->pFreeSRB;
    pACB->pFreeSRB = pSRB;
}


/* Inserts a SRB to the top of the Waiting list */
static __inline__ void dc390_Waiting_insert ( PDCB pDCB, PSRB pSRB )
{
    DEBUG0(printk ("DC390: Insert pSRB %p cmd %li to Waiting\n", pSRB, pSRB->pcmd->pid);)
    pSRB->pNextSRB = pDCB->pWaitingSRB;
    if (!pDCB->pWaitingSRB)
	pDCB->pWaitLast = pSRB;
    pDCB->pWaitingSRB = pSRB;
    pDCB->WaitSRBCnt++;
}


/* Queue SRB to waiting list */
static __inline__ void dc390_Waiting_append ( PDCB pDCB, PSRB pSRB)
{
    DEBUG0(printk ("DC390: Append pSRB %p cmd %li to Waiting\n", pSRB, pSRB->pcmd->pid);)
    if( pDCB->pWaitingSRB )
	pDCB->pWaitLast->pNextSRB = pSRB;
    else
	pDCB->pWaitingSRB = pSRB;

    pDCB->pWaitLast = pSRB;
    pSRB->pNextSRB = NULL;
    pDCB->WaitSRBCnt++;
    pDCB->pDCBACB->CmdInQ++;
}

static __inline__ void dc390_Going_append (PDCB pDCB, PSRB pSRB)
{
    pDCB->GoingSRBCnt++;
    DEBUG0(printk("DC390: Append SRB %p to Going\n", pSRB);)
    /* Append to the list of Going commands */
    if( pDCB->pGoingSRB )
	pDCB->pGoingLast->pNextSRB = pSRB;
    else
	pDCB->pGoingSRB = pSRB;

    pDCB->pGoingLast = pSRB;
    /* No next one in sent list */
    pSRB->pNextSRB = NULL;
};

static __inline__ void dc390_Going_remove (PDCB pDCB, PSRB pSRB)
{
   DEBUG0(printk("DC390: Remove SRB %p from Going\n", pSRB);)
   if (pSRB == pDCB->pGoingSRB)
	pDCB->pGoingSRB = pSRB->pNextSRB;
   else
     {
	PSRB psrb = pDCB->pGoingSRB;
	while (psrb && psrb->pNextSRB != pSRB)
	  psrb = psrb->pNextSRB;
	if (!psrb) 
	  { printk (KERN_ERR "DC390: Remove non-ex. SRB %p from Going!\n", pSRB); return; }
	psrb->pNextSRB = pSRB->pNextSRB;
	if (pSRB == pDCB->pGoingLast)
	  pDCB->pGoingLast = psrb;
     }
   pDCB->GoingSRBCnt--;
};

/* Moves SRB from Going list to the top of Waiting list */
static void dc390_Going_to_Waiting ( PDCB pDCB, PSRB pSRB )
{
    DEBUG0(printk(KERN_INFO "DC390: Going_to_Waiting (SRB %p) pid = %li\n", pSRB, pSRB->pcmd->pid);)
    /* Remove SRB from Going */
    dc390_Going_remove (pDCB, pSRB);
    /* Insert on top of Waiting */
    dc390_Waiting_insert (pDCB, pSRB);
    /* Tag Mask must be freed elsewhere ! (KG, 99/06/18) */
}

/* Moves first SRB from Waiting list to Going list */
static __inline__ void dc390_Waiting_to_Going ( PDCB pDCB, PSRB pSRB )
{	
	/* Remove from waiting list */
	DEBUG0(printk("DC390: Remove SRB %p from head of Waiting\n", pSRB);)
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	if( !pDCB->pWaitingSRB ) pDCB->pWaitLast = NULL;
	pDCB->WaitSRBCnt--;
	dc390_Going_append (pDCB, pSRB);
}

/* 2.0 timer compatibility */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,30)
 static inline int timer_pending(struct timer_list * timer)
 {
	return timer->prev != NULL;
 }
 #define time_after(a,b)         ((long)(b) - (long)(a) < 0)
 #define time_before(a,b)        time_after(b,a)
#endif

void DC390_waiting_timed_out (unsigned long ptr);
/* Sets the timer to wake us up */
static void dc390_waiting_timer (PACB pACB, unsigned long to)
{
	if (timer_pending (&pACB->Waiting_Timer)) return;
	init_timer (&pACB->Waiting_Timer);
	pACB->Waiting_Timer.function = DC390_waiting_timed_out;
	pACB->Waiting_Timer.data = (unsigned long)pACB;
	if (time_before (jiffies + to, pACB->pScsiHost->last_reset))
		pACB->Waiting_Timer.expires = pACB->pScsiHost->last_reset + 1;
	else
		pACB->Waiting_Timer.expires = jiffies + to + 1;
	add_timer (&pACB->Waiting_Timer);
}


/* Send the next command from the waiting list to the bus */
static void dc390_Waiting_process ( PACB pACB )
{
    PDCB   ptr, ptr1;
    PSRB   pSRB;

    if( (pACB->pActiveDCB) || (pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV) ) )
	return;
    if (timer_pending (&pACB->Waiting_Timer)) del_timer (&pACB->Waiting_Timer);
    ptr = pACB->pDCBRunRobin;
    if( !ptr )
      {
	ptr = pACB->pLinkDCB;
	pACB->pDCBRunRobin = ptr;
      }
    ptr1 = ptr;
    if (!ptr1) return;
    do 
      {
	pACB->pDCBRunRobin = ptr1->pNextDCB;
	if( !( pSRB = ptr1->pWaitingSRB ) ||
	    ( ptr1->MaxCommand <= ptr1->GoingSRBCnt ))
	  ptr1 = ptr1->pNextDCB;
	else
	  {
	    /* Try to send to the bus */
	    if( !dc390_StartSCSI(pACB, ptr1, pSRB) )
	      dc390_Waiting_to_Going (ptr1, pSRB);
	    else
	      dc390_waiting_timer (pACB, HZ/5);
	    break;
	  }
      } while (ptr1 != ptr);
    return;
}

/* Wake up waiting queue */
void DC390_waiting_timed_out (unsigned long ptr)
{
	PACB pACB = (PACB)ptr;
	DC390_IFLAGS
	DC390_AFLAGS
	DEBUG0(printk ("DC390: Debug: Waiting queue woken up by timer!\n");)
	DC390_LOCK_IO;
	DC390_LOCK_ACB;
	dc390_Waiting_process (pACB);
	DC390_UNLOCK_ACB;
	DC390_UNLOCK_IO;
}

/***********************************************************************
 * Function: static void dc390_SendSRB (PACB pACB, PSRB pSRB)
 *
 * Purpose: Send SCSI Request Block (pSRB) to adapter (pACB)
 *
 ***********************************************************************/

static void dc390_SendSRB( PACB pACB, PSRB pSRB )
{
    PDCB   pDCB;

    pDCB = pSRB->pSRBDCB;
    if( (pDCB->MaxCommand <= pDCB->GoingSRBCnt) || (pACB->pActiveDCB) ||
	(pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV)) )
    {
	dc390_Waiting_append (pDCB, pSRB);
	dc390_Waiting_process (pACB);
	return;
    }

#if 0
    if( pDCB->pWaitingSRB )
    {
	dc390_Waiting_append (pDCB, pSRB);
/*	pSRB = GetWaitingSRB(pDCB); */	/* non-existent */
	pSRB = pDCB->pWaitingSRB;
	/* Remove from waiting list */
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	pSRB->pNextSRB = NULL;
	if (!pDCB->pWaitingSRB) pDCB->pWaitLast = NULL;
    }
#endif
	
    if (!dc390_StartSCSI(pACB, pDCB, pSRB))
	dc390_Going_append (pDCB, pSRB);
    else {
	dc390_Waiting_insert (pDCB, pSRB);
	dc390_waiting_timer (pACB, HZ/5);
    };
}

/***********************************************************************
 * Function: static void dc390_BuildSRB (Scsi_Cmd *pcmd, PDCB pDCB, 
 * 					 PSRB pSRB)
 *
 * Purpose: Prepare SRB for being sent to Device DCB w/ command *pcmd
 *
 ***********************************************************************/

static void dc390_BuildSRB (Scsi_Cmnd* pcmd, PDCB pDCB, PSRB pSRB)
{
    pSRB->pSRBDCB = pDCB;
    pSRB->pcmd = pcmd;
    //pSRB->ScsiCmdLen = pcmd->cmd_len;
    //memcpy (pSRB->CmdBlock, pcmd->cmnd, pcmd->cmd_len);
    
    if( pcmd->use_sg )
    {
	pSRB->SGcount = (UCHAR) pcmd->use_sg;
	pSRB->pSegmentList = (PSGL) pcmd->request_buffer;
    }
    else if( pcmd->request_buffer )
    {
	pSRB->SGcount = 1;
	pSRB->pSegmentList = (PSGL) &pSRB->Segmentx;
	pSRB->Segmentx.address = (PUCHAR) pcmd->request_buffer;
	pSRB->Segmentx.length = pcmd->request_bufflen;
    }
    else
	pSRB->SGcount = 0;

    pSRB->SGIndex = 0;
    pSRB->AdaptStatus = 0;
    pSRB->TargetStatus = 0;
    pSRB->MsgCnt = 0;
    if( pDCB->DevType != TYPE_TAPE )
	pSRB->RetryCnt = 1;
    else
	pSRB->RetryCnt = 0;
    pSRB->SRBStatus = 0;
    pSRB->SRBFlag = 0;
    pSRB->SRBState = 0;
    pSRB->TotalXferredLen = 0;
    pSRB->SGBusAddr = 0;
    pSRB->SGToBeXferLen = 0;
    pSRB->ScsiPhase = 0;
    pSRB->EndMessage = 0;
    pSRB->TagNumber = 255;
};

/* Put cmnd from Query to Waiting list and send next Waiting cmnd */
static void dc390_Query_to_Waiting (PACB pACB)
{
    Scsi_Cmnd *pcmd;
    PSRB   pSRB;
    PDCB   pDCB;

    if( pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV) )
	return;

    while (pACB->QueryCnt)
    {
	pSRB = dc390_Free_get ( pACB );
	if (!pSRB) return;
	pcmd = dc390_Query_get ( pACB );
	if (!pcmd) { dc390_Free_insert (pACB, pSRB); return; }; /* should not happen */
	pDCB = dc390_findDCB (pACB, pcmd->target, pcmd->lun);
	if (!pDCB) 
	{ 
		dc390_Free_insert (pACB, pSRB);
		printk (KERN_ERR "DC390: Command in queue to non-existing device!\n");
		pcmd->result = MK_RES(DRIVER_ERROR,DID_ERROR,0,0);
		DC390_UNLOCK_ACB_NI;
		pcmd->done (pcmd);
		DC390_LOCK_ACB_NI;
	};
	dc390_BuildSRB (pcmd, pDCB, pSRB);
	dc390_Waiting_append ( pDCB, pSRB );
    }
}

/***********************************************************************
 * Function : static int DC390_queue_command (Scsi_Cmnd *cmd,
 *					       void (*done)(Scsi_Cmnd *))
 *
 * Purpose : enqueues a SCSI command
 *
 * Inputs : cmd - SCSI command, done - callback function called on 
 *	    completion, with a pointer to the command descriptor.
 *
 * Returns : (depending on kernel version)
 * 2.0.x: always return 0
 * 2.1.x: old model: (use_new_eh_code == 0): like 2.0.x
 *	  TO BE DONE:
 *	  new model: return 0 if successful
 *	  	     return 1 if command cannot be queued (queue full)
 *		     command will be inserted in midlevel queue then ...
 *
 ***********************************************************************/

int DC390_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *))
{
    PDCB   pDCB;
    PSRB   pSRB;
    DC390_AFLAGS
    PACB   pACB = (PACB) cmd->host->hostdata;


    DEBUG0(/*  if(pACB->scan_devices) */	\
	printk(KERN_INFO "DC390: Queue Cmd=%02x,Tgt=%d,LUN=%d (pid=%li)\n",\
		cmd->cmnd[0],cmd->target,cmd->lun,cmd->pid);)

    DC390_LOCK_ACB;
    
    /* Assume BAD_TARGET; will be cleared later */
    cmd->result = DID_BAD_TARGET << 16;
   
    /* TODO: Change the policy: Alway accept TEST_UNIT_READY or INQUIRY 
     * commands and alloc a DCB for the device if not yet there. DCB will
     * be removed in dc390_SRBdone if SEL_TIMEOUT */

    if( (pACB->scan_devices == END_SCAN) && (cmd->cmnd[0] != INQUIRY) )
	pACB->scan_devices = 0;

    else if( (pACB->scan_devices) && (cmd->cmnd[0] == READ_6) )
	pACB->scan_devices = 0;

    if ( ( cmd->target >= pACB->pScsiHost->max_id ) || 
	 (cmd->lun >= pACB->pScsiHost->max_lun) )
    {
/*	printk ("DC390: Ignore target %d lun %d\n",
		cmd->target, cmd->lun); */
	DC390_UNLOCK_ACB;
	//return (1);
	done (cmd);
	return (0);
    }

    if( (pACB->scan_devices || cmd->cmnd[0] == TEST_UNIT_READY || cmd->cmnd[0] == INQUIRY) && 
       !(pACB->DCBmap[cmd->target] & (1 << cmd->lun)) )
    {
        pACB->scan_devices = 1;

	dc390_initDCB( pACB, &pDCB, cmd->target, cmd->lun );
	if (!pDCB)
	  {
	    printk (KERN_ERR "DC390: kmalloc for DCB failed, target %02x lun %02x\n", 
		    cmd->target, cmd->lun);
	    DC390_UNLOCK_ACB;
	    printk ("DC390: No DCB in queue_command!\n");
#ifdef USE_NEW_EH
	    return (1);
#else
	    done (cmd);
	    return (0);
#endif
	  };
            
    }
    else if( !(pACB->scan_devices) && !(pACB->DCBmap[cmd->target] & (1 << cmd->lun)) )
    {
	printk(KERN_INFO "DC390: Ignore target %02x lun %02x\n",
		cmd->target, cmd->lun); 
	DC390_UNLOCK_ACB;
	//return (1);
	done (cmd);
	return (0);
    }
    else
    {
	pDCB = dc390_findDCB (pACB, cmd->target, cmd->lun);
	if (!pDCB)
	 {  /* should never happen */
	    printk (KERN_ERR "DC390: no DCB failed, target %02x lun %02x\n", 
		    cmd->target, cmd->lun);
	    DC390_UNLOCK_ACB;
	    printk ("DC390: No DCB in queuecommand (2)!\n");
#ifdef USE_NEW_EH
	    return (1);
#else
	    done (cmd);
	    return (0);
#endif
	 };
    }

    pACB->Cmds++;
    cmd->scsi_done = done;
    cmd->result = 0;
	
    dc390_Query_to_Waiting (pACB);

    if( pACB->QueryCnt ) /* Unsent commands ? */
    {
	DEBUG0(printk ("DC390: QueryCnt != 0\n");)
	dc390_Query_append ( cmd, pACB );
	dc390_Waiting_process (pACB);
    }
    else if (pDCB->pWaitingSRB)
    {
 	pSRB = dc390_Free_get ( pACB );
	DEBUG0(if (!pSRB) printk ("DC390: No free SRB but Waiting\n"); else printk ("DC390: Free SRB w/ Waiting\n");)
	if (!pSRB) dc390_Query_append (cmd, pACB);
	else 
	  {
	    dc390_BuildSRB (cmd, pDCB, pSRB);
	    dc390_Waiting_append (pDCB, pSRB);
	  }
	dc390_Waiting_process (pACB);
    }
    else
    {
 	pSRB = dc390_Free_get ( pACB );
	DEBUG0(if (!pSRB) printk ("DC390: No free SRB w/o Waiting\n"); else printk ("DC390: Free SRB w/o Waiting\n");)
	if (!pSRB)
	{
	    dc390_Query_append (cmd, pACB);
	    dc390_Waiting_process (pACB);
	}
	else 
	{
	    dc390_BuildSRB (cmd, pDCB, pSRB);
	    dc390_SendSRB (pACB, pSRB);
	};
    };

    DC390_UNLOCK_ACB;
    DEBUG1(printk (KERN_DEBUG " ... command (pid %li) queued successfully.\n", cmd->pid);)
    return(0);
}

/* We ignore mapping problems, as we expect everybody to respect 
 * valid partition tables. Waiting for complaints ;-) */

#ifdef CONFIG_SCSI_DC390T_TRADMAP
/* 
 * The next function, partsize(), is copied from scsicam.c.
 *
 * This is ugly code duplication, but I didn't find another way to solve it:
 * We want to respect the partition table and if it fails, we apply the 
 * DC390 BIOS heuristic. Too bad, just calling scsicam_bios_param() doesn't do
 * the job, because we don't know, whether the values returned are from
 * the part. table or determined by setsize(). Unfortunately the setsize() 
 * values differ from the ones chosen by the DC390 BIOS.
 *
 * Looking forward to seeing suggestions for a better solution! KG, 98/10/14
 */
#include <asm/unaligned.h>

/*
 * Function : static int partsize(struct buffer_head *bh, unsigned long 
 *     capacity,unsigned int *cyls, unsigned int *hds, unsigned int *secs);
 *
 * Purpose : to determine the BIOS mapping used to create the partition
 *	table, storing the results in *cyls, *hds, and *secs 
 *
 * Returns : -1 on failure, 0 on success.
 *
 */

static int partsize(struct buffer_head *bh, unsigned long capacity,
    unsigned int  *cyls, unsigned int *hds, unsigned int *secs) {
    struct partition *p, *largest = NULL;
    int i, largest_cyl;
    int cyl, ext_cyl, end_head, end_cyl, end_sector;
    unsigned int logical_end, physical_end, ext_physical_end;
    

    if (*(unsigned short *) (bh->b_data+510) == 0xAA55) {
	for (largest_cyl = -1, p = (struct partition *) 
    	    (0x1BE + bh->b_data), i = 0; i < 4; ++i, ++p) {
    	    if (!p->sys_ind)
    	    	continue;
    	    cyl = p->cyl + ((p->sector & 0xc0) << 2);
    	    if (cyl > largest_cyl) {
    	    	largest_cyl = cyl;
    	    	largest = p;
    	    }
    	}
    }

    if (largest) {
    	end_cyl = largest->end_cyl + ((largest->end_sector & 0xc0) << 2);
    	end_head = largest->end_head;
    	end_sector = largest->end_sector & 0x3f;

    	physical_end =  end_cyl * (end_head + 1) * end_sector +
    	    end_head * end_sector + end_sector;

	/* This is the actual _sector_ number at the end */
	logical_end = get_unaligned(&largest->start_sect)
			+ get_unaligned(&largest->nr_sects);

	/* This is for >1023 cylinders */
        ext_cyl= (logical_end-(end_head * end_sector + end_sector))
                                        /(end_head + 1) / end_sector;
	ext_physical_end = ext_cyl * (end_head + 1) * end_sector +
            end_head * end_sector + end_sector;

    	if ((logical_end == physical_end) ||
	    (end_cyl==1023 && ext_physical_end==logical_end)) {
    	    *secs = end_sector;
    	    *hds = end_head + 1;
    	    *cyls = capacity / ((end_head + 1) * end_sector);
    	    return 0;
    	}
    }
    return -1;
}

/***********************************************************************
 * Function:
 *   DC390_bios_param
 *
 * Description:
 *   Return the disk geometry for the given SCSI device.
 *   Respect the partition table, otherwise try own heuristic
 *
 * Note:
 *   In contrary to other externally callable funcs (DC390_), we don't lock
 ***********************************************************************/
int DC390_bios_param (Disk *disk, kdev_t devno, int geom[])
{
    int heads, sectors, cylinders;
    PACB pACB = (PACB) disk->device->host->hostdata;
    struct buffer_head *bh;
    int ret_code = -1;
    int size = disk->capacity;

    if ((bh = bread(MKDEV(MAJOR(devno), MINOR(devno)&~0xf), 0, block_size(devno))))
    {
	/* try to infer mapping from partition table */
	ret_code = partsize (bh, (unsigned long) size, (unsigned int *) geom + 2,
			     (unsigned int *) geom + 0, (unsigned int *) geom + 1);
	brelse (bh);
    }
    if (ret_code == -1)
    {
	heads = 64;
	sectors = 32;
	cylinders = size / (heads * sectors);

	if ( (pACB->Gmode2 & GREATER_1G) && (cylinders > 1024) )
	{
		heads = 255;
		sectors = 63;
		cylinders = size / (heads * sectors);
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;
    }

    return (0);
}
#else
int DC390_bios_param (Disk *disk, kdev_t devno, int geom[])
{
    return scsicam_bios_param (disk, devno, geom);
};
#endif


void dc390_dumpinfo (PACB pACB, PDCB pDCB, PSRB pSRB)
{
    USHORT pstat; PDEVDECL1;
    if (!pDCB) pDCB = pACB->pActiveDCB;
    if (!pSRB && pDCB) pSRB = pDCB->pActiveSRB;

    if (pSRB) 
    {
	printk ("DC390: SRB: Xferred %08lx, Remain %08lx, State %08x, Phase %02x\n",
		pSRB->TotalXferredLen, pSRB->SGToBeXferLen, pSRB->SRBState,
		pSRB->ScsiPhase);
	printk ("DC390: AdpaterStatus: %02x, SRB Status %02x\n", pSRB->AdaptStatus, pSRB->SRBStatus);
    };
    printk ("DC390: Status of last IRQ (DMA/SC/Int/IRQ): %08x\n", dc390_laststatus);
    printk ("DC390: Register dump: SCSI block:\n");
    printk ("DC390: XferCnt  Cmd Stat IntS IRQS FFIS Ctl1 Ctl2 Ctl3 Ctl4\n");
    printk ("DC390:  %06x   %02x   %02x   %02x",
	    DC390_read8(CtcReg_Low) + (DC390_read8(CtcReg_Mid) << 8) + (DC390_read8(CtcReg_High) << 16),
	    DC390_read8(ScsiCmd), DC390_read8(Scsi_Status), DC390_read8(Intern_State));
    printk ("   %02x   %02x   %02x   %02x   %02x   %02x\n",
	    DC390_read8(INT_Status), DC390_read8(Current_Fifo), DC390_read8(CtrlReg1),
	    DC390_read8(CtrlReg2), DC390_read8(CtrlReg3), DC390_read8(CtrlReg4));
    DC390_write32 (DMA_ScsiBusCtrl, WRT_ERASE_DMA_STAT | EN_INT_ON_PCI_ABORT);
    if (DC390_read8(Current_Fifo) & 0x1f)
      {
	printk ("DC390: FIFO:");
	while (DC390_read8(Current_Fifo) & 0x1f) printk (" %02x", DC390_read8(ScsiFifo));
	printk ("\n");
      };
    printk ("DC390: Register dump: DMA engine:\n");
    printk ("DC390: Cmd   STrCnt    SBusA    WrkBC    WrkAC Stat SBusCtrl\n");
    printk ("DC390:  %02x %08x %08x %08x %08x   %02x %08x\n",
	    DC390_read8(DMA_Cmd), DC390_read32(DMA_XferCnt), DC390_read32(DMA_XferAddr),
	    DC390_read32(DMA_Wk_ByteCntr), DC390_read32(DMA_Wk_AddrCntr),
	    DC390_read8(DMA_Status), DC390_read32(DMA_ScsiBusCtrl));
    DC390_write32 (DMA_ScsiBusCtrl, EN_INT_ON_PCI_ABORT);
    PDEVSET1; PCI_READ_CONFIG_WORD(PDEV, PCI_STATUS, &pstat);
    printk ("DC390: Register dump: PCI Status: %04x\n", pstat);
    printk ("DC390: In case of driver trouble read linux/drivers/scsi/README.tmscsim\n");
};


/***********************************************************************
 * Function : int DC390_abort (Scsi_Cmnd *cmd)
 *
 * Purpose : Abort an errant SCSI command
 *
 * Inputs : cmd - command to abort
 *
 * Returns : 0 on success, -1 on failure.
 *
 * Status: Buggy !
 ***********************************************************************/

int DC390_abort (Scsi_Cmnd *cmd)
{
    PDCB  pDCB;
    PSRB  pSRB, psrb;
    UINT  count, i;
    PSCSICMD  pcmd;
    int   status;
    //ULONG sbac;
    DC390_AFLAGS
    PACB  pACB = (PACB) cmd->host->hostdata;

    DC390_LOCK_ACB;

    printk ("DC390: Abort command (pid %li, Device %02i-%02i)\n",
	    cmd->pid, cmd->target, cmd->lun);

    /* First scan Query list */
    if( pACB->QueryCnt )
    {
	pcmd = pACB->pQueryHead;
	if( pcmd == cmd )
	{
	    /* Found: Dequeue */
	    pACB->pQueryHead = pcmd->next;
	    pcmd->next = NULL;
	    if (cmd == pACB->pQueryTail) pACB->pQueryTail = NULL;
	    pACB->QueryCnt--;
	    status = SCSI_ABORT_SUCCESS;
	    goto  ABO_X;
	}
	for( count = pACB->QueryCnt, i=0; i<count-1; i++)
	{
	    if( pcmd->next == cmd )
	    {
		pcmd->next = cmd->next;
		cmd->next = NULL;
		if (cmd == pACB->pQueryTail) pACB->pQueryTail = NULL;
		pACB->QueryCnt--;
		status = SCSI_ABORT_SUCCESS;
		goto  ABO_X;
	    }
	    else
	    {
		pcmd = pcmd->next;
	    }
	}
    }
	
    pDCB = dc390_findDCB (pACB, cmd->target, cmd->lun);
    if( !pDCB ) goto  NOT_RUN;

    /* Added 98/07/02 KG */
    /*
    pSRB = pDCB->pActiveSRB;
    if (pSRB && pSRB->pcmd == cmd )
	goto ON_GOING;
     */
    
    pSRB = pDCB->pWaitingSRB;
    if( !pSRB )
	goto  ON_GOING;

    /* Now scan Waiting queue */
    if( pSRB->pcmd == cmd )
    {
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	goto  IN_WAIT;
    }
    else
    {
	psrb = pSRB;
	if( !(psrb->pNextSRB) )
	    goto ON_GOING;
	while( psrb->pNextSRB->pcmd != cmd )
	{
	    psrb = psrb->pNextSRB;
	    if( !(psrb->pNextSRB) || psrb == pSRB)
		goto ON_GOING;
	}
	pSRB = psrb->pNextSRB;
	psrb->pNextSRB = pSRB->pNextSRB;
	if( pSRB == pDCB->pWaitLast )
	    pDCB->pWaitLast = psrb;
IN_WAIT:
	dc390_Free_insert (pACB, pSRB);
	pDCB->WaitSRBCnt--;
	cmd->next = NULL;
	status = SCSI_ABORT_SUCCESS;
	goto  ABO_X;
    }

    /* SRB has already been sent ! */
ON_GOING:
    /* abort() is too stupid for already sent commands at the moment. 
     * If it's called we are in trouble anyway, so let's dump some info 
     * into the syslog at least. (KG, 98/08/20,99/06/20) */
    dc390_dumpinfo (pACB, pDCB, pSRB);
    pSRB = pDCB->pGoingSRB;
    pDCB->DCBFlag |= ABORT_DEV_;
    /* Now for the hard part: The command is currently processed */
    for( count = pDCB->GoingSRBCnt, i=0; i<count; i++)
    {
	if( pSRB->pcmd != cmd )
	    pSRB = pSRB->pNextSRB;
	else
	{
	    if( (pACB->pActiveDCB == pDCB) && (pDCB->pActiveSRB == pSRB) )
	    {
		status = SCSI_ABORT_BUSY;
		printk ("DC390: Abort current command (pid %li, SRB %p)\n",
			cmd->pid, pSRB);
		goto  ABO_X;
	    }
	    else
	    {
		status = SCSI_ABORT_SNOOZE;
		goto  ABO_X;
	    }
	}
    }

NOT_RUN:
    status = SCSI_ABORT_NOT_RUNNING;

ABO_X:
    cmd->result = DID_ABORT << 16;
    printk(KERN_INFO "DC390: Aborted pid %li with status %i\n", cmd->pid, status);
#if 0
    if (cmd->pid == dc390_lastabortedpid) /* repeated failure ? */
	{
		/* Let's do something to help the bus getting clean again */
		DC390_write8 (DMA_Cmd, DMA_IDLE_CMD);
		DC390_write8 (ScsiCmd, DMA_COMMAND);
		//DC390_write8 (ScsiCmd, CLEAR_FIFO_CMD);
		//DC390_write8 (ScsiCmd, RESET_ATN_CMD);
		DC390_write8 (ScsiCmd, NOP_CMD);
		//udelay (10000);
		//DC390_read8 (INT_Status);
		//DC390_write8 (ScsiCmd, EN_SEL_RESEL);
	};
    sbac = DC390_read32 (DMA_ScsiBusCtrl);
    if (sbac & SCSI_BUSY)
    {	/* clear BSY, SEL and ATN */
	printk (KERN_WARNING "DC390: Reset SCSI device: ");
	//DC390_write32 (DMA_ScsiBusCtrl, (sbac | SCAM) & ~SCSI_LINES);
	//udelay (250);
	//sbac = DC390_read32 (DMA_ScsiBusCtrl);
	//printk ("%08lx ", sbac);
	//DC390_write32 (DMA_ScsiBusCtrl, sbac & ~(SCSI_LINES | SCAM));
	//udelay (100);
	//sbac = DC390_read32 (DMA_ScsiBusCtrl);
	//printk ("%08lx ", sbac);
	DC390_write8 (ScsiCmd, RST_DEVICE_CMD);
	udelay (250);
	DC390_write8 (ScsiCmd, NOP_CMD);
	sbac = DC390_read32 (DMA_ScsiBusCtrl);
	printk ("%08lx\n", sbac);
    };
#endif
    dc390_lastabortedpid = cmd->pid;
    DC390_UNLOCK_ACB;
    //do_DC390_Interrupt (pACB->IRQLevel, 0, 0);
#ifndef USE_NEW_EH	
    if (status == SCSI_ABORT_SUCCESS) cmd->scsi_done(cmd);
#endif	
    return( status );
}


static void dc390_ResetDevParam( PACB pACB )
{
    PDCB   pDCB, pdcb;

    pDCB = pACB->pLinkDCB;
    if (! pDCB) return;
    pdcb = pDCB;
    do
    {
	pDCB->SyncMode &= ~SYNC_NEGO_DONE;
	pDCB->SyncPeriod = 0;
	pDCB->SyncOffset = 0;
	pDCB->TagMask = 0;
	pDCB->CtrlR3 = FAST_CLK;
	pDCB->CtrlR4 &= NEGATE_REQACKDATA | CTRL4_RESERVED | NEGATE_REQACK;
	pDCB->CtrlR4 |= pACB->glitch_cfg;
	pDCB = pDCB->pNextDCB;
    }
    while( pdcb != pDCB );
    pACB->ACBFlag &= ~(RESET_DEV | RESET_DONE | RESET_DETECT);

}

#if 0
/* Moves all SRBs from Going to Waiting for all DCBs */
static void dc390_RecoverSRB( PACB pACB )
{
    PDCB   pDCB, pdcb;
    PSRB   psrb, psrb2;
    UINT   cnt, i;

    pDCB = pACB->pLinkDCB;
    if( !pDCB ) return;
    pdcb = pDCB;
    do
    {
	cnt = pdcb->GoingSRBCnt;
	psrb = pdcb->pGoingSRB;
	for (i=0; i<cnt; i++)
	{
	    psrb2 = psrb;
	    psrb = psrb->pNextSRB;
/*	    dc390_RewaitSRB( pDCB, psrb ); */
	    if( pdcb->pWaitingSRB )
	    {
		psrb2->pNextSRB = pdcb->pWaitingSRB;
		pdcb->pWaitingSRB = psrb2;
	    }
	    else
	    {
		pdcb->pWaitingSRB = psrb2;
		pdcb->pWaitLast = psrb2;
		psrb2->pNextSRB = NULL;
	    }
	}
	pdcb->GoingSRBCnt = 0;
	pdcb->pGoingSRB = NULL;
	pdcb->TagMask = 0;
	pdcb = pdcb->pNextDCB;
    } while( pdcb != pDCB );
}
#endif

/***********************************************************************
 * Function : int DC390_reset (Scsi_Cmnd *cmd, ...)
 *
 * Purpose : perform a hard reset on the SCSI bus
 *
 * Inputs : cmd - command which caused the SCSI RESET
 *	    resetFlags - how hard to try
 *
 * Returns : 0 on success.
 ***********************************************************************/

int DC390_reset (Scsi_Cmnd *cmd, unsigned int resetFlags)
{
    UCHAR   bval;
    DC390_AFLAGS
    PACB    pACB = (PACB) cmd->host->hostdata;

    printk(KERN_INFO "DC390: RESET ... ");

    DC390_LOCK_ACB;
    if (timer_pending (&pACB->Waiting_Timer)) del_timer (&pACB->Waiting_Timer);
    bval = DC390_read8 (CtrlReg1);
    bval |= DIS_INT_ON_SCSI_RST;
    DC390_write8 (CtrlReg1, bval);	/* disable IRQ on bus reset */

    pACB->ACBFlag |= RESET_DEV;
    dc390_ResetSCSIBus( pACB );

    dc390_ResetDevParam( pACB );
    udelay (1000);
    pACB->pScsiHost->last_reset = jiffies + 3*HZ/2 
		+ HZ * dc390_eepromBuf[pACB->AdapterIndex][EE_DELAY];
    
    DC390_write8 (ScsiCmd, CLEAR_FIFO_CMD);
    DC390_read8 (INT_Status);		/* Reset Pending INT */

    dc390_DoingSRB_Done( pACB, cmd );
    /* dc390_RecoverSRB (pACB); */
    pACB->pActiveDCB = NULL;

    pACB->ACBFlag = 0;
    bval = DC390_read8 (CtrlReg1);
    bval &= ~DIS_INT_ON_SCSI_RST;
    DC390_write8 (CtrlReg1, bval);	/* re-enable interrupt */

    dc390_Waiting_process( pACB );

    printk("done\n");
    DC390_UNLOCK_ACB;
    return( SCSI_RESET_SUCCESS );
}

#include "scsiiom.c"


/***********************************************************************
 * Function : static void dc390_initDCB()
 *
 * Purpose :  initialize the internal structures for a DCB (to be malloced)
 *
 * Inputs : SCSI id and lun
 ***********************************************************************/

void dc390_initDCB( PACB pACB, PDCB *ppDCB, UCHAR id, UCHAR lun )
{
    PEEprom	prom;
    UCHAR	index;
    PDCB pDCB, pDCB2;

    pDCB = kmalloc (sizeof(DC390_DCB), GFP_ATOMIC);
    DCBDEBUG(printk (KERN_INFO "DC390: alloc mem for DCB (ID %i, LUN %i): %p\n"	\
	    id, lun, pDCB);)
 
    *ppDCB = pDCB; pDCB2 = 0;
    if (!pDCB) return;
    if( pACB->DCBCnt == 0 )
    {
	pACB->pLinkDCB = pDCB;
	pACB->pDCBRunRobin = pDCB;
    }
    else
    {
	pACB->pLastDCB->pNextDCB = pDCB;
    };
   
    pACB->DCBCnt++;
   
    pDCB->pNextDCB = pACB->pLinkDCB;
    pACB->pLastDCB = pDCB;

    pDCB->pDCBACB = pACB;
    pDCB->TargetID = id;
    pDCB->TargetLUN = lun;
    pDCB->pWaitingSRB = NULL;
    pDCB->pGoingSRB = NULL;
    pDCB->GoingSRBCnt = 0;
    pDCB->WaitSRBCnt = 0;
    pDCB->pActiveSRB = NULL;
    pDCB->TagMask = 0;
    pDCB->MaxCommand = 1;
    index = pACB->AdapterIndex;
    pDCB->DCBFlag = 0;

    /* Is there a corresp. LUN==0 device ? */
    if (lun != 0)
	pDCB2 = dc390_findDCB (pACB, id, 0);
    prom = (PEEprom) &dc390_eepromBuf[index][id << 2];
    /* Some values are for all LUNs: Copy them */
    /* In a clean way: We would have an own structure for a SCSI-ID */
    if (pDCB2)
    {
      pDCB->DevMode = pDCB2->DevMode;
      pDCB->SyncMode = pDCB2->SyncMode;
      pDCB->SyncPeriod = pDCB2->SyncPeriod;
      pDCB->SyncOffset = pDCB2->SyncOffset;
      pDCB->NegoPeriod = pDCB2->NegoPeriod;
      
      pDCB->CtrlR3 = pDCB2->CtrlR3;
      pDCB->CtrlR4 = pDCB2->CtrlR4;
      pDCB->Inquiry7 = pDCB2->Inquiry7;
    }
    else
    {		
      pDCB->DevMode = prom->EE_MODE1;
      pDCB->SyncMode = 0;
      pDCB->SyncPeriod = 0;
      pDCB->SyncOffset = 0;
      pDCB->NegoPeriod = (dc390_clock_period1[prom->EE_SPEED] * 25) >> 2;
            
      pDCB->CtrlR3 = FAST_CLK;
      
      pDCB->CtrlR4 = pACB->glitch_cfg | CTRL4_RESERVED;
      if( dc390_eepromBuf[index][EE_MODE2] & ACTIVE_NEGATION)
	pDCB->CtrlR4 |= NEGATE_REQACKDATA | NEGATE_REQACK;
      pDCB->Inquiry7 = 0;
    }

    pACB->DCBmap[id] |= (1 << lun);
    dc390_updateDCB(pACB, pDCB);
}

/***********************************************************************
 * Function : static void dc390_updateDCB()
 *
 * Purpose :  Set the configuration dependent DCB parameters
 ***********************************************************************/

void dc390_updateDCB (PACB pACB, PDCB pDCB)
{
  pDCB->SyncMode &= EN_TAG_QUEUEING | SYNC_NEGO_DONE /*| EN_ATN_STOP*/;
  if (pDCB->DevMode & TAG_QUEUEING_) {
	//if (pDCB->SyncMode & EN_TAG_QUEUEING) pDCB->MaxCommand = pACB->TagMaxNum;
  } else {
	pDCB->SyncMode &= ~EN_TAG_QUEUEING;
	pDCB->MaxCommand = 1;
  };

  if( pDCB->DevMode & SYNC_NEGO_ )
	pDCB->SyncMode |= SYNC_ENABLE;
  else {
	pDCB->SyncMode &= ~(SYNC_NEGO_DONE | SYNC_ENABLE);
	pDCB->SyncOffset &= ~0x0f;
  };

  //if (! (pDCB->DevMode & EN_DISCONNECT_)) pDCB->SyncMode &= ~EN_ATN_STOP; 

  pDCB->CtrlR1 = pACB->pScsiHost->this_id;
  if( pDCB->DevMode & PARITY_CHK_ )
	pDCB->CtrlR1 |= PARITY_ERR_REPO;
};  


/***********************************************************************
 * Function : static void dc390_updateDCBs ()
 *
 * Purpose :  Set the configuration dependent DCB params for all DCBs
 ***********************************************************************/

static void dc390_updateDCBs (PACB pACB)
{
  int i;
  PDCB pDCB = pACB->pLinkDCB;
  for (i = 0; i < pACB->DCBCnt; i++)
    {
      dc390_updateDCB (pACB, pDCB);
      pDCB = pDCB->pNextDCB;
    };
};
  

/***********************************************************************
 * Function : static void dc390_initSRB()
 *
 * Purpose :  initialize the internal structures for a given SRB
 *
 * Inputs : psrb - pointer to this scsi request block structure
 ***********************************************************************/

static void __inline__ dc390_initSRB( PSRB psrb )
{
  /* psrb->PhysSRB = virt_to_phys( psrb ); */
}


void dc390_linkSRB( PACB pACB )
{
    UINT   count, i;

    count = pACB->SRBCount;
    for( i=0; i<count; i++)
    {
	if( i != count-1 )
	    pACB->SRB_array[i].pNextSRB = &pACB->SRB_array[i+1];
	else
	    pACB->SRB_array[i].pNextSRB = NULL;
	dc390_initSRB( &pACB->SRB_array[i] );
    }
}


/***********************************************************************
 * Function : static void dc390_initACB ()
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : psh - pointer to this host adapter's structure
 *	    io_port, Irq, index: Resources and adapter index
 ***********************************************************************/

void __init dc390_initACB (PSH psh, ULONG io_port, UCHAR Irq, UCHAR index)
{
    PACB    pACB;
    UCHAR   i;
    DC390_AFLAGS

    psh->can_queue = MAX_CMD_QUEUE;
    psh->cmd_per_lun = MAX_CMD_PER_LUN;
    psh->this_id = (int) dc390_eepromBuf[index][EE_ADAPT_SCSI_ID];
    psh->io_port = io_port;
    psh->n_io_port = 0x80;
    psh->irq = Irq;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,50)
    psh->base = io_port;
#else
    psh->base = (char*)io_port;
#endif	
    psh->unique_id = io_port;
    psh->dma_channel = -1;
    psh->last_reset = jiffies;
	
    pACB = (PACB) psh->hostdata;
    DC390_LOCKA_INIT;
    DC390_LOCK_ACB;

    pACB->pScsiHost = psh;
    pACB->IOPortBase = (USHORT) io_port;
    pACB->IRQLevel = Irq;

    DEBUG0(printk (KERN_INFO "DC390: Adapter index %i, ID %i, IO 0x%08x, IRQ 0x%02x\n",	\
	    index, psh->this_id, (int)io_port, Irq);)
   
    psh->max_id = 8;

    if( psh->max_id - 1 == dc390_eepromBuf[index][EE_ADAPT_SCSI_ID] )
	psh->max_id--;
    psh->max_lun = 1;
    if( dc390_eepromBuf[index][EE_MODE2] & LUN_CHECK )
	psh->max_lun = 8;

    pACB->pLinkDCB = NULL;
    pACB->pDCBRunRobin = NULL;
    pACB->pActiveDCB = NULL;
    pACB->pFreeSRB = pACB->SRB_array;
    pACB->SRBCount = MAX_SRB_CNT;
    pACB->QueryCnt = 0;
    pACB->pQueryHead = NULL;
    pACB->AdapterIndex = index;
    pACB->status = 0;
    psh->this_id = dc390_eepromBuf[index][EE_ADAPT_SCSI_ID];
    pACB->DeviceCnt = 0;
    pACB->DCBCnt = 0;
    pACB->TagMaxNum = 2 << dc390_eepromBuf[index][EE_TAG_CMD_NUM];
    pACB->ACBFlag = 0;
    pACB->scan_devices = 1;
    pACB->MsgLen = 0;
    pACB->Ignore_IRQ = 0;
    pACB->Gmode2 = dc390_eepromBuf[index][EE_MODE2];
    dc390_linkSRB( pACB );
    pACB->pTmpSRB = &pACB->TmpSRB;
    dc390_initSRB( pACB->pTmpSRB );
    for(i=0; i<MAX_SCSI_ID; i++)
	pACB->DCBmap[i] = 0;
    pACB->sel_timeout = SEL_TIMEOUT;
    pACB->glitch_cfg = EATER_25NS;
    pACB->Cmds = pACB->CmdInQ = pACB->CmdOutOfSRB = 0;
    pACB->SelLost = pACB->SelConn = 0;
    init_timer (&pACB->Waiting_Timer);
}


/***********************************************************************
 * Function : static int dc390_initAdapter ()
 *
 * Purpose :  initialize the SCSI chip ctrl registers
 *
 * Inputs : psh - pointer to this host adapter's structure
 *	    io_port, Irq, index: Resources
 *
 * Outputs: 0 on success, -1 on error
 ***********************************************************************/

int __init dc390_initAdapter (PSH psh, ULONG io_port, UCHAR Irq, UCHAR index)
{
    PACB   pACB, pACB2;
    UCHAR  dstate;
    int    i;
    
    pACB = (PACB) psh->hostdata;
    
    if (check_region (io_port, psh->n_io_port))
	{
	    printk(KERN_ERR "DC390: register IO ports error!\n");
	    return( -1 );
	}
    else
	request_region (io_port, psh->n_io_port, "tmscsim");

    DC390_read8_ (INT_Status, io_port);		/* Reset Pending INT */

    if( (i = request_irq(Irq, do_DC390_Interrupt, DC390_IRQ, "tmscsim", pACB) ))
      {
	printk(KERN_ERR "DC390: register IRQ error!\n");
	release_region (io_port, psh->n_io_port);
	return( -1 );
      }

    if( !dc390_pACB_start )
      {
	pACB2 = NULL;
	dc390_pACB_start = pACB;
	dc390_pACB_current = pACB;
	pACB->pNextACB = NULL;
      }
    else
      {
	pACB2 = dc390_pACB_current;
	dc390_pACB_current->pNextACB = pACB;
	dc390_pACB_current = pACB;
	pACB->pNextACB = NULL;
      };

    DC390_write8 (CtrlReg1, DIS_INT_ON_SCSI_RST | psh->this_id);	/* Disable SCSI bus reset interrupt */

    if (pACB->Gmode2 & RST_SCSI_BUS)
    {
	dc390_ResetSCSIBus( pACB );
	udelay (1000);
	pACB->pScsiHost->last_reset = jiffies + HZ/2
		    + HZ * dc390_eepromBuf[pACB->AdapterIndex][EE_DELAY];
	/*
	for( i=0; i<(500 + 1000*dc390_eepromBuf[pACB->AdapterIndex][EE_DELAY]); i++ )
		udelay(1000);
	 */
    };
    pACB->ACBFlag = 0;
    DC390_read8 (INT_Status);				/* Reset Pending INT */
    
    DC390_write8 (Scsi_TimeOut, SEL_TIMEOUT);		/* 250ms selection timeout */
    DC390_write8 (Clk_Factor, CLK_FREQ_40MHZ);		/* Conversion factor = 0 , 40MHz clock */
    DC390_write8 (ScsiCmd, NOP_CMD);			/* NOP cmd - clear command register */
    DC390_write8 (CtrlReg2, EN_FEATURE+EN_SCSI2_CMD);	/* Enable Feature and SCSI-2 */
    DC390_write8 (CtrlReg3, FAST_CLK);			/* fast clock */
    DC390_write8 (CtrlReg4, pACB->glitch_cfg |			/* glitch eater */
		(dc390_eepromBuf[index][EE_MODE2] & ACTIVE_NEGATION) ? NEGATE_REQACKDATA : 0);	/* Negation */
    DC390_write8 (CtcReg_High, 0);			/* Clear Transfer Count High: ID */
    DC390_write8 (DMA_Cmd, DMA_IDLE_CMD);
    DC390_write8 (ScsiCmd, CLEAR_FIFO_CMD);
    DC390_write32 (DMA_ScsiBusCtrl, EN_INT_ON_PCI_ABORT);
    dstate = DC390_read8 (DMA_Status);
    DC390_write8 (DMA_Status, dstate);	/* clear */

    return(0);
}


/***********************************************************************
 * Function : static int DC390_init (struct Scsi_Host *host, ...)
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : host - pointer to this host adapter's structure
 *	    io_port - IO ports mapped to this adapter
 *	    Irq - IRQ assigned to this adpater
 *	    PDEVDECL - PCI access handle
 *	    index - Adapter index
 *
 * Outputs: 0 on success, -1 on error
 *
 * Note: written in capitals, because the locking is only done here,
 *	not in DC390_detect, called from outside 
 ***********************************************************************/

static int __init DC390_init (PSHT psht, ULONG io_port, UCHAR Irq, PDEVDECL, UCHAR index)
{
    PSH   psh;
    PACB  pACB;
    DC390_AFLAGS
    
    if (dc390_CheckEEpromCheckSum (PDEV, index))
    {
#ifdef CONFIG_SCSI_DC390T_NOGENSUPP
	printk (KERN_ERR "DC390_init: No EEPROM found!\n");
	return( -1 );
#else
	int speed;
	dc390_adapname = "AM53C974";
	printk (KERN_INFO "DC390_init: No EEPROM found! Trying default settings ...\n");
	dc390_check_for_safe_settings ();
	dc390_fill_with_defaults ();
	dc390_EEprom_Override (index);
	speed = dc390_clock_speed[tmscsim[1]];
	printk (KERN_INFO "DC390: Used defaults: AdaptID=%i, SpeedIdx=%i (%i.%i MHz),"
		" DevMode=0x%02x, AdaptMode=0x%02x, TaggedCmnds=%i (%i), DelayReset=%is\n", 
		tmscsim[0], tmscsim[1], speed/10, speed%10,
		(UCHAR)tmscsim[2], (UCHAR)tmscsim[3], tmscsim[4], 2 << (tmscsim[4]), tmscsim[5]);
#endif
    }
    else
    {
	dc390_check_for_safe_settings ();
	dc390_EEprom_Override (index);
    }
   
    psh = scsi_register( psht, sizeof(DC390_ACB) );
    if( !psh ) return( -1 );
	
    scsi_set_pci_device(psh, pdev);
    pACB = (PACB) psh->hostdata;
    DC390_LOCKA_INIT;
    DC390_LOCK_ACB;

#if 0
    if( !dc390_pSH_start )
    {
        dc390_pSH_start = psh;
        dc390_pSH_current = psh;
    }
    else
    {
        dc390_pSH_current->next = psh;
        dc390_pSH_current = psh;
    }
#endif

    DEBUG0(printk(KERN_INFO "DC390: pSH = %8x,", (UINT) psh);)
    DEBUG0(printk(" Index %02i,", index);)

    dc390_initACB( psh, io_port, Irq, index );
    pACB = (PACB) psh->hostdata;
        
    PDEVSET;

    if( !dc390_initAdapter( psh, io_port, Irq, index ) )
    {
	DEBUG0(printk("\nDC390: pACB = %8x, pDCBmap = %8x, pSRB_array = %8x\n",\
		(UINT) pACB, (UINT) pACB->DCBmap, (UINT) pACB->SRB_array);)
	DEBUG0(printk("DC390: ACB size= %4x, DCB size= %4x, SRB size= %4x\n",\
		sizeof(DC390_ACB), sizeof(DC390_DCB), sizeof(DC390_SRB) );)

	DC390_UNLOCK_ACB;
        return (0);
    }
    else
    {
	//dc390_pSH_start = NULL;
	scsi_unregister( psh );
	DC390_UNLOCK_ACB;
	return( -1 );
    }
}


/***********************************************************************
 * Function : int DC390_detect(Scsi_Host_Template *psht)
 *
 * Purpose : detects and initializes AMD53C974 SCSI chips
 *	     that were autoprobed, overridden on the LILO command line,
 *	     or specified at compile time.
 *
 * Inputs : psht - template for this SCSI adapter
 *
 * Returns : number of host adapters detected
 *
 ***********************************************************************/

#ifndef NEW_PCI
/* Acc. to PCI 2.1 spec it's up to the driver to enable Bus mastering:
 * We use pci_set_master () for 2.1.x and this func for 2.0.x:	*/
static void __init dc390_set_master (PDEVDECL)
{
	USHORT cmd;
	UCHAR lat;
	
	PCI_READ_CONFIG_WORD (PDEV, PCI_COMMAND, &cmd);
	
        if (! (cmd & PCI_COMMAND_MASTER)) {	
		printk("PCI: Enabling bus mastering for device %02x:%02x\n",
		       PCI_BUS_DEV);
		cmd |= PCI_COMMAND_MASTER;
		PCI_WRITE_CONFIG_WORD(PDEV, PCI_COMMAND, cmd);
	}
	PCI_READ_CONFIG_BYTE (PDEV, PCI_LATENCY_TIMER, &lat);
	if (lat < 16 /* || lat == 255 */) {
		printk("PCI: Setting latency timer of device %02x:%02x from %i to 64\n",
		       PCI_BUS_DEV, lat);
		PCI_WRITE_CONFIG_BYTE(PDEV, PCI_LATENCY_TIMER, 64);
	}
	
};
#endif /* ! NEW_PCI */

static void __init dc390_set_pci_cfg (PDEVDECL)
{
	USHORT cmd;
	PCI_READ_CONFIG_WORD (PDEV, PCI_COMMAND, &cmd);
	cmd |= PCI_COMMAND_SERR | PCI_COMMAND_PARITY | PCI_COMMAND_IO;
	PCI_WRITE_CONFIG_WORD (PDEV, PCI_COMMAND, cmd);
	PCI_WRITE_CONFIG_WORD (PDEV, PCI_STATUS, (PCI_STATUS_SIG_SYSTEM_ERROR | PCI_STATUS_DETECTED_PARITY));
};
	

int __init DC390_detect (Scsi_Host_Template *psht)
{
    PDEVDECL0;
    UCHAR   irq;
    UINT    io_port;
    //DC390_IFLAGS
    DC390_DFLAGS

    DC390_LOCK_DRV;
    //dc390_pSHT_start = psht;
    dc390_pACB_start = NULL;

    if ( PCI_PRESENT )
	while (PCI_FIND_DEVICE (PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD53C974))
	{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,30)
	    if (pci_enable_device (pdev))
		continue;
#endif
	    //DC390_LOCK_IO;		/* Remove this when going to new eh */
	    PCI_GET_IO_AND_IRQ;
	    DEBUG0(printk(KERN_INFO "DC390(%i): IO_PORT=%04x,IRQ=%x\n", dc390_adapterCnt, (UINT) io_port, irq);)

	    if( !DC390_init(psht, io_port, irq, PDEV, dc390_adapterCnt))
	    {
		PCI_SET_MASTER;
		dc390_set_pci_cfg (PDEV);
		dc390_adapterCnt++;
	    };
	    //DC390_UNLOCK_IO;		/* Remove when going to new eh */
	}
    else
	printk (KERN_ERR "DC390: No PCI BIOS found!\n");
   
    if (dc390_adapterCnt)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,30)
	psht->proc_name = "tmscsim";
#else
	psht->proc_dir = &DC390_proc_scsi_tmscsim;
#endif
    printk(KERN_INFO "DC390: %i adapters found\n", dc390_adapterCnt);
    DC390_UNLOCK_DRV;
    return( dc390_adapterCnt );
}


/***********************************************************************
 * Functions: dc390_inquiry(), dc390_inquiry_done()
 *
 * Purpose: When changing speed etc., we have to issue an INQUIRY
 *	    command to make sure, we agree upon the nego parameters
 *	    with the device
 ***********************************************************************/

static void dc390_inquiry_done (Scsi_Cmnd* cmd)
{
   printk (KERN_INFO "DC390: INQUIRY (ID %02x LUN %02x) returned %08x\n",
	   cmd->target, cmd->lun, cmd->result);
   if (cmd->result)
   {
	PACB pACB = (PACB)cmd->host->hostdata;
	PDCB pDCB = dc390_findDCB (pACB, cmd->target, cmd->lun);
	printk ("DC390: Unsetting DsCn, Sync and TagQ!\n");
	if (pDCB)
	{
		pDCB->DevMode &= ~(SYNC_NEGO_ | TAG_QUEUEING_ | EN_DISCONNECT_ );
		dc390_updateDCB (pACB, pDCB);
	};
   };
   kfree (cmd);
};

void dc390_inquiry (PACB pACB, PDCB pDCB)
{
   char* buffer;
   Scsi_Cmnd* cmd;
   cmd = kmalloc (sizeof(Scsi_Cmnd) + 256, GFP_ATOMIC);
   if (!cmd) { printk ("DC390: kmalloc failed in inquiry!\n"); return; };
   buffer = (char*)cmd + sizeof(Scsi_Cmnd);

   memset (cmd, 0, sizeof(Scsi_Cmnd) + 256);
   cmd->cmnd[0] = INQUIRY;
   cmd->cmnd[1] = (pDCB->TargetLUN << 5) & 0xe0;
   cmd->cmnd[4] = 0xff;
   
   cmd->cmd_len = 6; cmd->old_cmd_len = 6;
   cmd->host = pACB->pScsiHost;
   cmd->target = pDCB->TargetID;
   cmd->lun = pDCB->TargetLUN; 
   cmd->serial_number = 1;
   cmd->pid = 390;
   cmd->bufflen = 128;
   cmd->buffer = buffer;
   cmd->request_bufflen = 128;
   cmd->request_buffer = &buffer[128];
   cmd->done = dc390_inquiry_done;
   cmd->scsi_done = dc390_inquiry_done;
   cmd->timeout_per_command = HZ;

   cmd->request.rq_status = RQ_SCSI_BUSY;

   pDCB->SyncMode &= ~SYNC_NEGO_DONE;
   printk (KERN_INFO "DC390: Queue INQUIRY command to dev ID %02x LUN %02x\n",
	   pDCB->TargetID, pDCB->TargetLUN);
   DC390_queue_command (cmd, dc390_inquiry_done);
};

/***********************************************************************
 * Functions: dc390_sendstart(), dc390_sendstart_done()
 *
 * Purpose: When changing speed etc., we have to issue an INQUIRY
 *	    command to make sure, we agree upon the nego parameters
 *	    with the device
 ***********************************************************************/

static void dc390_sendstart_done (Scsi_Cmnd* cmd)
{
   printk (KERN_INFO "DC390: SENDSTART (ID %02x LUN %02x) returned %08x\n",
	   cmd->target, cmd->lun, cmd->result);
   kfree (cmd);
};

void dc390_sendstart (PACB pACB, PDCB pDCB)
{
   char* buffer;
   Scsi_Cmnd* cmd;
   cmd = kmalloc (sizeof(Scsi_Cmnd) + 256, GFP_ATOMIC);
   if (!cmd) { printk ("DC390: kmalloc failed in sendstart!\n"); return; };
   buffer = (char*)cmd + sizeof(Scsi_Cmnd);

   memset (cmd, 0, sizeof(Scsi_Cmnd) + 256);
   cmd->cmnd[0] = 0x1b; /* START_STOP_UNIT */
   cmd->cmnd[1] = (pDCB->TargetLUN << 5) & 0xe0;
   cmd->cmnd[4] = 0x01; /* START */
   
   cmd->cmd_len = 6; cmd->old_cmd_len = 6;
   cmd->host = pACB->pScsiHost;
   cmd->target = pDCB->TargetID;
   cmd->lun = pDCB->TargetLUN; 
   cmd->serial_number = 1;
   cmd->pid = 310;
   cmd->bufflen = 128;
   cmd->buffer = buffer;
   cmd->request_bufflen = 128;
   cmd->request_buffer = &buffer[128];
   cmd->done = dc390_sendstart_done;
   cmd->scsi_done = dc390_sendstart_done;
   cmd->timeout_per_command = 5*HZ;

   cmd->request.rq_status = RQ_SCSI_BUSY;

   pDCB->SyncMode &= ~SYNC_NEGO_DONE;
   printk (KERN_INFO "DC390: Queue SEND_START command to dev ID %02x LUN %02x\n",
	   pDCB->TargetID, pDCB->TargetLUN);
   DC390_queue_command (cmd, dc390_sendstart_done);
};

/********************************************************************
 * Function: dc390_set_info()
 *
 * Purpose: Change adapter config
 *
 * Strings are parsed similar to the output of tmscsim_proc_info ()
 * '-' means no change
 *******************************************************************/

static int dc390_scanf (char** p1, char** p2, int* var)
{
   *p2 = *p1;
   *var = simple_strtoul (*p2, p1, 10);
   if (*p2 == *p1) return -1;
   *p1 = strtok (0, " \t\n:=,;.");
   return 0;
};

#define SCANF(p1, p2, var, min, max)		\
if (dc390_scanf (&p1, &p2, &var)) goto einv;	\
else if (var<min || var>max) goto einv2

static int dc390_yesno (char** p, char* var, char bmask)
{
   switch (**p)
     {
      case 'Y': *var |= bmask; break;
      case 'N': *var &= ~bmask; break;
      case '-': break;
      default: return -1;
     }
   *p = strtok (0, " \t\n:=,;");
   return 0;
};

#define YESNO(p, var, bmask)			\
if (dc390_yesno (&p, &var, bmask)) goto einv;	\
else dc390_updateDCB (pACB, pDCB);		\
if (!p) goto ok

static int dc390_search (char **p1, char **p2, char *var, char* txt, int max, int scale, char* ign)
{
   int dum;
   if (! memcmp (*p1, txt, strlen(txt)))
     {
	*p2 = strtok (0, " \t\n:=,;");
	if (!*p2) return -1;
	dum = simple_strtoul (*p2, p1, 10);
	if (*p2 == *p1) return -1;
	if (dum >= 0 && dum <= max) 
	  { *var = (dum * 100) / scale; }
	else return -2;
	*p1 = strtok (0, " \t\n:=,;");
	if (*ign && *p1 && strlen(*p1) >= strlen(ign) && 
	    !(memcmp (*p1, ign, strlen(ign)))) 
		*p1 = strtok (0, " \t\n:=,;");

     }
   return 0;
};

#define SEARCH(p1, p2, var, txt, max)						\
if (dc390_search (&p1, &p2, (PUCHAR)(&var), txt, max, 100, "")) goto einv2;	\
else if (!p1) goto ok2

#define SEARCH2(p1, p2, var, txt, max, scale)					\
if (dc390_search (&p1, &p2, &var, txt, max, scale, "")) goto einv2; 		\
else if (!p1) goto ok2

#define SEARCH3(p1, p2, var, txt, max, scale, ign)				\
if (dc390_search (&p1, &p2, &var, txt, max, scale, ign)) goto einv2;		\
else if (!p1) goto ok2


#ifdef DC390_PARSEDEBUG
static char _prstr[256];
char* prstr (char* p, char* e)
{
   char* c = _prstr;
   while (p < e)
     if (*p == 0) { *c++ = ':'; p++; }
     else if (*p == 10) { *c++ = '\\'; *c++ = 'n'; p++; }
     else *c++ = *p++;
   *c = 0;
   return _prstr;
};
#endif

int dc390_set_info (char *buffer, int length, PACB pACB)
{
  char *pos = buffer, *p0 = buffer;
  char needs_inquiry = 0; 
  int dum = 0;
  char dev;
  PDCB pDCB = pACB->pLinkDCB;
  DC390_IFLAGS
  DC390_AFLAGS 
  pos[length] = 0;

  DC390_LOCK_IO;
  DC390_LOCK_ACB;
  /* UPPERCASE */ 
  /* Don't use kernel toupper, because of 2.0.x bug: ctmp unexported */
  while (*pos) 
    { if (*pos >='a' && *pos <= 'z') *pos = *pos + 'A' - 'a'; pos++; };
  
  /* We should protect __strtok ! */
  /* spin_lock (strtok_lock); */

  /* Remove WS */
  pos = strtok (buffer, " \t:\n=,;");
  if (!pos) goto ok;
   
 next:
  if (!memcmp (pos, "RESET", 5)) goto reset;
  else if (!memcmp (pos, "INQUIRY", 7)) goto inquiry;
  else if (!memcmp (pos, "REMOVE", 6)) goto remove;
  else if (!memcmp (pos, "ADD", 3)) goto add;
  else if (!memcmp (pos, "START", 5)) goto start;
  else if (!memcmp (pos, "DUMP", 4)) goto dump;
  
  if (isdigit (*pos))
    {
      /* Device config line */
      int dev, id, lun; char* pdec;
      char olddevmode;
      
      SCANF (pos, p0, dev, 0, pACB->DCBCnt-1);
      if (pos) { SCANF (pos, p0, id, 0, 7); } else goto einv;
      if (pos) { SCANF (pos, p0, lun, 0, 7); } else goto einv;
      if (!pos) goto einv;
      
      PARSEDEBUG(printk (KERN_INFO "DC390: config line %i %i %i:\"%s\"\n", dev, id, lun, prstr (pos, &buffer[length]));)
      pDCB = pACB->pLinkDCB;
      for (dum = 0; dum < dev; dum++) pDCB = pDCB->pNextDCB;
      /* Sanity Check */
      if (pDCB->TargetID != id || pDCB->TargetLUN != lun) 
	 {
	    printk (KERN_ERR "DC390: no such device: Idx=%02i ID=%02i LUN=%02i\n",
		    dev, id, lun);
	    goto einv2;
	 };

      if (pDCB->pWaitingSRB || pDCB->pGoingSRB)
      {
	  printk ("DC390: Cannot change dev (%i-%i) cfg: Pending requests\n",
		  pDCB->TargetID, pDCB->TargetLUN);
	  goto einv;
      };
	  
      olddevmode = pDCB->DevMode;
      YESNO (pos, pDCB->DevMode, PARITY_CHK_);
      needs_inquiry++;
      YESNO (pos, pDCB->DevMode, SYNC_NEGO_);
      if ((olddevmode & SYNC_NEGO_) == (pDCB->DevMode & SYNC_NEGO_)) needs_inquiry--;
      needs_inquiry++;
      YESNO (pos, pDCB->DevMode, EN_DISCONNECT_);
      if ((olddevmode & EN_DISCONNECT_) == (pDCB->DevMode & EN_DISCONNECT_)) needs_inquiry--;
      YESNO (pos, pDCB->DevMode, SEND_START_);
      needs_inquiry++;
      YESNO (pos, pDCB->DevMode, TAG_QUEUEING_);
      if ((olddevmode & TAG_QUEUEING_) == (pDCB->DevMode & TAG_QUEUEING_)) needs_inquiry--;

      dc390_updateDCB (pACB, pDCB);
      if (!pos) goto ok;
       
      olddevmode = pDCB->NegoPeriod;
      /* Look for decimal point (Speed) */
      pdec = pos; 
      while (pdec++ < &buffer[length]) if (*pdec == '.') break;
      /* NegoPeriod */
      if (*pos != '-')
	{
	  SCANF (pos, p0, dum, 72, 800); 
	  pDCB->NegoPeriod = dum >> 2;
	  if (pDCB->NegoPeriod != olddevmode) needs_inquiry++;
	  if (!pos) goto ok;
	  if (memcmp (pos, "NS", 2) == 0) pos = strtok (0, " \t\n:=,;.");
	}
      else pos = strtok (0, " \t\n:=,;.");
      if (!pos) goto ok;
      
      /* Sync Speed in MHz */
      if (*pos != '-')
	{
	  SCANF (pos, p0, dum, 1, 13); 
	  pDCB->NegoPeriod = (1000/dum) >> 2;
	  if (pDCB->NegoPeriod != olddevmode && !pos) needs_inquiry++;
	  if (!pos) goto ok;
	  /* decimal */
	  if (pos-1 == pdec)
	     {
		int dumold = dum;
		dum = simple_strtoul (pos, &p0, 10) * 10;
		for (; p0-pos > 1; p0--) dum /= 10;
		pDCB->NegoPeriod = (100000/(100*dumold + dum)) >> 2;
		if (pDCB->NegoPeriod < 19) pDCB->NegoPeriod = 19;
		pos = strtok (0, " \t\n:=,;");
		if (!pos) goto ok;
	     };
	  if (*pos == 'M') pos = strtok (0, " \t\n:=,;");
	  if (pDCB->NegoPeriod != olddevmode) needs_inquiry++;
	}
      else pos = strtok (0, " \t\n:=,;");
      /* dc390_updateDCB (pACB, pDCB); */
      if (!pos) goto ok;

      olddevmode = pDCB->SyncOffset;
      /* SyncOffs */
      if (*pos != '-')
	{
	  SCANF (pos, p0, dum, 0, 0x0f); 
	  pDCB->SyncOffset = dum;
	  if (pDCB->SyncOffset > olddevmode) needs_inquiry++;
	}
      else pos = strtok (0, " \t\n:=,;");
      if (!pos) goto ok;
      dc390_updateDCB (pACB, pDCB);

      //olddevmode = pDCB->MaxCommand;
      /* MaxCommand (Tags) */
      if (*pos != '-')
	{
	  SCANF (pos, p0, dum, 1, 32 /*pACB->TagMaxNum*/);
	  if (pDCB->SyncMode & EN_TAG_QUEUEING)
		pDCB->MaxCommand = dum;
	  else printk (KERN_INFO "DC390: Can't set MaxCmd larger than one without Tag Queueing!\n");
	}
      else pos = strtok (0, " \t\n:=,;");

    }
  else
    {
      char* p1 = pos; UCHAR dum, newadaptid;
      PARSEDEBUG(printk (KERN_INFO "DC390: chg adapt cfg \"%s\"\n", prstr (pos, &buffer[length]));)
      dum = GLITCH_TO_NS (pACB->glitch_cfg);
      /* Adapter setting */
      SEARCH (pos, p0, pACB->pScsiHost->max_id, "MAXID", 8); 
      SEARCH (pos, p0, pACB->pScsiHost->max_lun, "MAXLUN", 8); 
      SEARCH (pos, p0, newadaptid, "ADAPTERID", 7);
      SEARCH (pos, p0, pACB->TagMaxNum, "TAGMAXNUM", 32);
      SEARCH (pos, p0, pACB->ACBFlag, "ACBFLAG", 255);
      SEARCH3 (pos, p0, dum, "GLITCHEATER", 40, 1000, "NS");
      SEARCH3 (pos, p0, pACB->sel_timeout, "SELTIMEOUT", 400, 163, "MS");
      SEARCH3 (pos, p0, dc390_eepromBuf[pACB->AdapterIndex][EE_DELAY], "DELAYRESET", 180, 100, "S");
    ok2:
      pACB->glitch_cfg = NS_TO_GLITCH (dum);
      if (pACB->sel_timeout < 60) pACB->sel_timeout = 60;
      DC390_write8 (Scsi_TimeOut, pACB->sel_timeout);
      if (newadaptid != pACB->pScsiHost->this_id)
      {
	pACB->pScsiHost->this_id = newadaptid;
	dc390_ResetDevParam (pACB);
      }	    
      //dum = 0; while (1 << dum <= pACB->TagMaxNum) dum ++;
      //pACB->TagMaxNum &= (1 << --dum);
      dc390_updateDCBs (pACB);
      // All devs should be INQUIRED now
      if (pos == p1) goto einv;
    }
  if (pos) goto next;
      
 ok:
  /* spin_unlock (strtok_lock); */
  DC390_UNLOCK_ACB;
  if (needs_inquiry) 
     { dc390_updateDCB (pACB, pDCB); dc390_inquiry (pACB, pDCB); };
  DC390_UNLOCK_IO;
  return (length);

 einv2:
  pos = p0;
 einv:
  /* spin_unlock (strtok_lock); */
  DC390_UNLOCK_ACB;
  DC390_UNLOCK_IO;
  printk (KERN_WARNING "DC390: parse error near \"%s\"\n", (pos? pos: "NULL"));
  return (-EINVAL);
   
 reset:
     {
	Scsi_Cmnd cmd; cmd.host = pACB->pScsiHost;
	printk (KERN_WARNING "DC390: Driver reset requested!\n");
	DC390_UNLOCK_ACB;
	DC390_reset (&cmd, 0);
	DC390_UNLOCK_IO;
     };
  return (length);

 dump:
     {
	dc390_dumpinfo (pACB, 0, 0);
	DC390_UNLOCK_ACB;
	DC390_UNLOCK_IO;       
     }
  return (length);
	
 inquiry:
     {
	pos = strtok (0, " \t\n.:;="); if (!pos) goto einv;
	dev = simple_strtoul (pos, &p0, 10);
	if (dev >= pACB->DCBCnt) goto einv_dev;
	for (dum = 0; dum < dev; dum++) pDCB = pDCB->pNextDCB;
	printk (KERN_NOTICE " DC390: Issue INQUIRY command to Dev(Idx) %i SCSI ID %i LUN %i\n",
		dev, pDCB->TargetID, pDCB->TargetLUN);
	DC390_UNLOCK_ACB;
	dc390_inquiry (pACB, pDCB);
	DC390_UNLOCK_IO;
     };
   return (length);

 remove:
     {
	pos = strtok (0, " \t\n.:;="); if (!pos) goto einv;
	dev = simple_strtoul (pos, &p0, 10);
	if (dev >= pACB->DCBCnt) goto einv_dev;
	for (dum = 0; dum < dev; dum++) pDCB = pDCB->pNextDCB;
	printk (KERN_NOTICE " DC390: Remove DCB for Dev(Idx) %i SCSI ID %i LUN %i\n",
		dev, pDCB->TargetID, pDCB->TargetLUN);
	/* TO DO: We should make sure no pending commands are left */
	dc390_remove_dev (pACB, pDCB);
	DC390_UNLOCK_ACB;
	DC390_UNLOCK_IO;
     };
   return (length);

 add:
     {
	int id, lun;
	pos = strtok (0, " \t\n.:;=");
	if (pos) { SCANF (pos, p0, id, 0, 7); } else goto einv;
	if (pos) { SCANF (pos, p0, lun, 0, 7); } else goto einv;
	pDCB = dc390_findDCB (pACB, id, lun);
	if (pDCB) { printk ("DC390: ADD: Device already existing\n"); goto einv; };
	dc390_initDCB (pACB, &pDCB, id, lun);
	DC390_UNLOCK_ACB;
	dc390_inquiry (pACB, pDCB);
	DC390_UNLOCK_IO;
     };
   return (length);

 start:
     {
	int id, lun;
	pos = strtok (0, " \t\n.:;=");
	if (pos) { SCANF (pos, p0, id, 0, 7); } else goto einv;
	if (pos) { SCANF (pos, p0, lun, 0, 7); } else goto einv;
	pDCB = dc390_findDCB (pACB, id, lun);
	if (pDCB) printk ("DC390: SendStart: Device already existing ...\n");
	else dc390_initDCB (pACB, &pDCB, id, lun);
	DC390_UNLOCK_ACB;
	dc390_sendstart (pACB, pDCB);
	dc390_inquiry (pACB, pDCB);
	DC390_UNLOCK_IO;
     };
   return (length);

 einv_dev:
   printk (KERN_WARNING "DC390: Ignore cmnd to illegal Dev(Idx) %i. Valid range: 0 - %i.\n", 
	   dev, pACB->DCBCnt - 1);
   DC390_UNLOCK_ACB;
   DC390_UNLOCK_IO;
   return (-EINVAL);
	     
	     
}

#undef SEARCH
#undef YESNO
#undef SCANF

/********************************************************************
 * Function: DC390_proc_info(char* buffer, char **start,
 *			     off_t offset, int length, int hostno, int inout)
 *
 * Purpose: return SCSI Adapter/Device Info
 *
 * Input: buffer: Pointer to a buffer where to write info
 *	  start :
 *	  offset:
 *	  hostno: Host adapter index
 *	  inout : Read (=0) or set(!=0) info
 *
 * Output: buffer: contains info
 *	   length; length of info in buffer
 *
 * return value: length
 *
 ********************************************************************/

#undef SPRINTF
#define SPRINTF(args...) pos += sprintf(pos, ## args)

#define YESNO(YN)		\
 if (YN) SPRINTF(" Yes ");	\
 else SPRINTF(" No  ")


int DC390_proc_info (char *buffer, char **start,
		     off_t offset, int length, int hostno, int inout)
{
  int dev, spd, spd1;
  char *pos = buffer;
  PSH shpnt = 0;
  PACB pACB;
  PDCB pDCB;
  PSCSICMD pcmd;
  DC390_AFLAGS

  pACB = dc390_pACB_start;

  while(pACB != (PACB)-1)
     {
	shpnt = pACB->pScsiHost;
	if (shpnt->host_no == hostno) break;
	pACB = pACB->pNextACB;
     }

  if (pACB == (PACB)-1) return(-ESRCH);
  if(!shpnt) return(-ESRCH);

  if(inout) /* Has data been written to the file ? */
      return dc390_set_info(buffer, length, pACB);
   
  SPRINTF("Tekram DC390/AM53C974 PCI SCSI Host Adapter, ");
  SPRINTF("Driver Version %s\n", DC390_VERSION);

  DC390_LOCK_ACB;

  SPRINTF("SCSI Host Nr %i, ", shpnt->host_no);
  SPRINTF("%s Adapter Nr %i\n", dc390_adapname, pACB->AdapterIndex);
  SPRINTF("IOPortBase 0x%04x, ", pACB->IOPortBase);
  SPRINTF("IRQ %02i\n", pACB->IRQLevel);

  SPRINTF("MaxID %i, MaxLUN %i, ", shpnt->max_id, shpnt->max_lun);
  SPRINTF("AdapterID %i, SelTimeout %i ms, DelayReset %i s\n", 
	  shpnt->this_id, (pACB->sel_timeout*164)/100,
	  dc390_eepromBuf[pACB->AdapterIndex][EE_DELAY]);

  SPRINTF("TagMaxNum %i, Status 0x%02x, ACBFlag 0x%02x, GlitchEater %i ns\n",
	  pACB->TagMaxNum, pACB->status, pACB->ACBFlag, GLITCH_TO_NS(pACB->glitch_cfg)*12);

  SPRINTF("Statistics: Cmnds %li, Cmnds not sent directly %i, Out of SRB conds %i\n",
	  pACB->Cmds, pACB->CmdInQ, pACB->CmdOutOfSRB);
  SPRINTF("            Lost arbitrations %i, Sel. connected %i, Connected: %s\n", 
	  pACB->SelLost, pACB->SelConn, pACB->Connected? "Yes": "No");
   
  SPRINTF("Nr of attached devices: %i, Nr of DCBs: %i\n", pACB->DeviceCnt, pACB->DCBCnt);
  SPRINTF("Map of attached LUNs: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  pACB->DCBmap[0], pACB->DCBmap[1], pACB->DCBmap[2], pACB->DCBmap[3], 
	  pACB->DCBmap[4], pACB->DCBmap[5], pACB->DCBmap[6], pACB->DCBmap[7]);

  SPRINTF("Idx ID LUN Prty Sync DsCn SndS TagQ NegoPeriod SyncSpeed SyncOffs MaxCmd\n");

  pDCB = pACB->pLinkDCB;
  for (dev = 0; dev < pACB->DCBCnt; dev++)
     {
      SPRINTF("%02i  %02i  %02i ", dev, pDCB->TargetID, pDCB->TargetLUN);
      YESNO(pDCB->DevMode & PARITY_CHK_);
      YESNO(pDCB->SyncMode & SYNC_NEGO_DONE);
      YESNO(pDCB->DevMode & EN_DISCONNECT_);
      YESNO(pDCB->DevMode & SEND_START_);
      YESNO(pDCB->SyncMode & EN_TAG_QUEUEING);
      if (pDCB->SyncOffset & 0x0f)
      {
	 int sp = pDCB->SyncPeriod; if (! (pDCB->CtrlR3 & FAST_SCSI)) sp++;
	 SPRINTF("  %03i ns ", (pDCB->NegoPeriod) << 2);
	 spd = 40/(sp); spd1 = 40%(sp);
	 spd1 = (spd1 * 10 + sp/2) / (sp);
	 SPRINTF("   %2i.%1i M      %02i", spd, spd1, (pDCB->SyncOffset & 0x0f));
      }
      else SPRINTF(" (%03i ns)                 ", (pDCB->NegoPeriod) << 2);
      /* Add more info ...*/
      SPRINTF ("      %02i\n", pDCB->MaxCommand);
      pDCB = pDCB->pNextDCB;
     }
    SPRINTF ("Commands in Queues: Query: %li:", pACB->QueryCnt);
    for (pcmd = pACB->pQueryHead; pcmd; pcmd = pcmd->next)
	SPRINTF (" %li", pcmd->pid);
    if (timer_pending(&pACB->Waiting_Timer)) SPRINTF ("Waiting queue timer running\n");
    else SPRINTF ("\n");
    pDCB = pACB->pLinkDCB;
	
    for (dev = 0; dev < pACB->DCBCnt; dev++)
    {
	PSRB pSRB;
	if (pDCB->WaitSRBCnt) 
		    SPRINTF ("DCB (%02i-%i): Waiting: %i:", pDCB->TargetID, pDCB->TargetLUN,
			     pDCB->WaitSRBCnt);
	for (pSRB = pDCB->pWaitingSRB; pSRB; pSRB = pSRB->pNextSRB)
		SPRINTF(" %li", pSRB->pcmd->pid);
	if (pDCB->GoingSRBCnt) 
		    SPRINTF ("\nDCB (%02i-%i): Going  : %i:", pDCB->TargetID, pDCB->TargetLUN,
			     pDCB->GoingSRBCnt);
	for (pSRB = pDCB->pGoingSRB; pSRB; pSRB = pSRB->pNextSRB)
#if 0 //def DC390_DEBUGTRACE
		SPRINTF(" %s\n  ", pSRB->debugtrace);
#else
		SPRINTF(" %li", pSRB->pcmd->pid);
#endif
	if (pDCB->WaitSRBCnt || pDCB->GoingSRBCnt) SPRINTF ("\n");
	pDCB = pDCB->pNextDCB;
    }
	
#ifdef DC390_DEBUGDCB
    SPRINTF ("DCB list for ACB %p:\n", pACB);
    pDCB = pACB->pLinkDCB;
    SPRINTF ("%p", pDCB);
    for (dev = 0; dev < pACB->DCBCnt; dev++, pDCB=pDCB->pNextDCB)
	SPRINTF ("->%p", pDCB->pNextDCB);
    SPRINTF("\n");
#endif
  

  DC390_UNLOCK_ACB;
  *start = buffer + offset;

  if (pos - buffer < offset)
    return 0;
  else if (pos - buffer - offset < length)
    return pos - buffer - offset;
  else
    return length;
}

#undef YESNO
#undef SPRINTF

#ifdef MODULE

/***********************************************************************
 * Function : static int dc390_shutdown (struct Scsi_Host *host)
 *
 * Purpose : does a clean (we hope) shutdown of the SCSI chip.
 *	     Use prior to dumping core, unloading the driver, etc.
 *
 * Returns : 0 on success
 ***********************************************************************/
static int dc390_shutdown (struct Scsi_Host *host)
{
    UCHAR    bval;
    PACB pACB = (PACB)(host->hostdata);
   
/*  pACB->soft_reset(host); */

    printk(KERN_INFO "DC390: shutdown\n");

    pACB->ACBFlag = RESET_DEV;
    bval = DC390_read8 (CtrlReg1);
    bval |= DIS_INT_ON_SCSI_RST;
    DC390_write8 (CtrlReg1, bval);	/* disable interrupt */
    if (pACB->Gmode2 & RST_SCSI_BUS)
		dc390_ResetSCSIBus (pACB);

    if (timer_pending (&pACB->Waiting_Timer)) del_timer (&pACB->Waiting_Timer);
    return( 0 );
}

void dc390_freeDCBs (struct Scsi_Host *host)
{
    PDCB pDCB, nDCB;
    PACB pACB = (PACB)(host->hostdata);
    
    pDCB = pACB->pLinkDCB;
    if (!pDCB) return;
    do
    {
	nDCB = pDCB->pNextDCB;
	DCBDEBUG(printk (KERN_INFO "DC390: Free DCB (ID %i, LUN %i): %p\n",\
		pDCB->TargetID, pDCB->TargetLUN, pDCB);)
	//kfree (pDCB);
	dc390_remove_dev (pACB, pDCB);
	pDCB = nDCB;
    } while (pDCB && pACB->pLinkDCB);

};

int DC390_release (struct Scsi_Host *host)
{
    DC390_AFLAGS DC390_IFLAGS
    PACB pACB = (PACB)(host->hostdata);

    DC390_LOCK_IO;
    DC390_LOCK_ACB;

    /* TO DO: We should check for outstanding commands first. */
    dc390_shutdown (host);

    if (host->irq != SCSI_IRQ_NONE)
    {
	DEBUG0(printk(KERN_INFO "DC390: Free IRQ %i\n",host->irq);)
	free_irq (host->irq, pACB);
    }

    release_region(host->io_port,host->n_io_port);
    dc390_freeDCBs (host);
    DC390_UNLOCK_ACB;
    DC390_UNLOCK_IO;
    return( 1 );
}
#endif /* def MODULE */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,99)
static Scsi_Host_Template driver_template = DC390_T;
#include "scsi_module.c"
#endif
