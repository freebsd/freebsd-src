/******************************************************************************
**  High Performance device driver for the Symbios 53C896 controller.
**
**  Copyright (C) 1998-2001  Gerard Roudier <groudier@free.fr>
**
**  This driver also supports all the Symbios 53C8XX controller family, 
**  except 53C810 revisions < 16, 53C825 revisions < 16 and all 
**  revisions of 53C815 controllers.
**
**  This driver is based on the Linux port of the FreeBSD ncr driver.
** 
**  Copyright (C) 1994  Wolfgang Stanglmeier
**  
**-----------------------------------------------------------------------------
**  
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  The Linux port of the FreeBSD ncr driver has been achieved in 
**  november 1995 by:
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**-----------------------------------------------------------------------------
**
**  Major contributions:
**  --------------------
**
**  NVRAM detection and reading.
**    Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
**
*******************************************************************************
*/

/*
**	Supported SCSI features:
**	    Synchronous data transfers
**	    Wide16 SCSI BUS
**	    Disconnection/Reselection
**	    Tagged command queuing
**	    SCSI Parity checking
**
**	Supported NCR/SYMBIOS chips:
**		53C810A	  (8 bits, Fast 10,	 no rom BIOS) 
**		53C825A	  (Wide,   Fast 10,	 on-board rom BIOS)
**		53C860	  (8 bits, Fast 20,	 no rom BIOS)
**		53C875	  (Wide,   Fast 20,	 on-board rom BIOS)
**		53C876	  (Wide,   Fast 20 Dual, on-board rom BIOS)
**		53C895	  (Wide,   Fast 40,	 on-board rom BIOS)
**		53C895A	  (Wide,   Fast 40,	 on-board rom BIOS)
**		53C896	  (Wide,   Fast 40 Dual, on-board rom BIOS)
**		53C897	  (Wide,   Fast 40 Dual, on-board rom BIOS)
**		53C1510D  (Wide,   Fast 40 Dual, on-board rom BIOS)
**		53C1010	  (Wide,   Fast 80 Dual, on-board rom BIOS)
**		53C1010_66(Wide,   Fast 80 Dual, on-board rom BIOS, 33/66MHz PCI)
**
**	Other features:
**		Memory mapped IO
**		Module
**		Shared IRQ
*/

/*
**	Name and version of the driver
*/
#define SCSI_NCR_DRIVER_NAME	"sym53c8xx-1.7.3c-20010512"

#define SCSI_NCR_DEBUG_FLAGS	(0)

#define NAME53C		"sym53c"
#define NAME53C8XX	"sym53c8xx"

/*==========================================================
**
**      Include files
**
**==========================================================
*/

#define LinuxVersionCode(v, p, s) (((v)<<16)+((p)<<8)+(s))

#include <linux/module.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,17)
#include <linux/spinlock.h>
#elif LINUX_VERSION_CODE >= LinuxVersionCode(2,1,93)
#include <asm/spinlock.h>
#endif
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/stat.h>

#include <linux/version.h>
#include <linux/blk.h>

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,35)
#include <linux/init.h>
#endif

#ifndef	__init
#define	__init
#endif
#ifndef	__initdata
#define	__initdata
#endif

#if LINUX_VERSION_CODE <= LinuxVersionCode(2,1,92)
#include <linux/bios32.h>
#endif

#include "scsi.h"
#include "hosts.h"
#include "constants.h"
#include "sd.h"

#include <linux/types.h>

/*
**	Define BITS_PER_LONG for earlier linux versions.
*/
#ifndef	BITS_PER_LONG
#if (~0UL) == 0xffffffffUL
#define	BITS_PER_LONG	32
#else
#define	BITS_PER_LONG	64
#endif
#endif

/*
**	Define the BSD style u_int32 and u_int64 type.
**	Are in fact u_int32_t and u_int64_t :-)
*/
typedef u32 u_int32;
typedef u64 u_int64;

#include "sym53c8xx.h"

/*
**	Donnot compile integrity checking code for Linux-2.3.0 
**	and above since SCSI data structures are not ready yet.
*/
/* #if LINUX_VERSION_CODE < LinuxVersionCode(2,3,0) */
#if 0
#define	SCSI_NCR_INTEGRITY_CHECKING
#endif

#define MIN(a,b)        (((a) < (b)) ? (a) : (b))
#define MAX(a,b)        (((a) > (b)) ? (a) : (b))

/*
**	Hmmm... What complex some PCI-HOST bridges actually are, 
**	despite the fact that the PCI specifications are looking 
**	so smart and simple! ;-)
*/
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,47)
#define SCSI_NCR_DYNAMIC_DMA_MAPPING
#endif

/*==========================================================
**
**	A la VMS/CAM-3 queue management.
**	Implemented from linux list management.
**
**==========================================================
*/

typedef struct xpt_quehead {
	struct xpt_quehead *flink;	/* Forward  pointer */
	struct xpt_quehead *blink;	/* Backward pointer */
} XPT_QUEHEAD;

#define xpt_que_init(ptr) do { \
	(ptr)->flink = (ptr); (ptr)->blink = (ptr); \
} while (0)

static inline void __xpt_que_add(struct xpt_quehead * new,
	struct xpt_quehead * blink,
	struct xpt_quehead * flink)
{
	flink->blink	= new;
	new->flink	= flink;
	new->blink	= blink;
	blink->flink	= new;
}

static inline void __xpt_que_del(struct xpt_quehead * blink,
	struct xpt_quehead * flink)
{
	flink->blink = blink;
	blink->flink = flink;
}

static inline int xpt_que_empty(struct xpt_quehead *head)
{
	return head->flink == head;
}

static inline void xpt_que_splice(struct xpt_quehead *list,
	struct xpt_quehead *head)
{
	struct xpt_quehead *first = list->flink;

	if (first != list) {
		struct xpt_quehead *last = list->blink;
		struct xpt_quehead *at   = head->flink;

		first->blink = head;
		head->flink  = first;

		last->flink = at;
		at->blink   = last;
	}
}

#define xpt_que_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


#define xpt_insque(new, pos)		__xpt_que_add(new, pos, (pos)->flink)

#define xpt_remque(el)			__xpt_que_del((el)->blink, (el)->flink)

#define xpt_insque_head(new, head)	__xpt_que_add(new, head, (head)->flink)

static inline struct xpt_quehead *xpt_remque_head(struct xpt_quehead *head)
{
	struct xpt_quehead *elem = head->flink;

	if (elem != head)
		__xpt_que_del(head, elem->flink);
	else
		elem = 0;
	return elem;
}

#define xpt_insque_tail(new, head)	__xpt_que_add(new, (head)->blink, head)

static inline struct xpt_quehead *xpt_remque_tail(struct xpt_quehead *head)
{
	struct xpt_quehead *elem = head->blink;

	if (elem != head)
		__xpt_que_del(elem->blink, head);
	else
		elem = 0;
	return elem;
}

/*==========================================================
**
**	Configuration and Debugging
**
**==========================================================
*/

/*
**    SCSI address of this device.
**    The boot routines should have set it.
**    If not, use this.
*/

#ifndef SCSI_NCR_MYADDR
#define SCSI_NCR_MYADDR      (7)
#endif

/*
**    The maximum number of tags per logic unit.
**    Used only for devices that support tags.
*/

#ifndef SCSI_NCR_MAX_TAGS
#define SCSI_NCR_MAX_TAGS    (8)
#endif

/*
**    TAGS are actually unlimited (256 tags/lun).
**    But Linux only supports 255. :)
*/
#if	SCSI_NCR_MAX_TAGS > 255
#define	MAX_TAGS	255
#else
#define	MAX_TAGS SCSI_NCR_MAX_TAGS
#endif

/*
**    Since the ncr chips only have a 8 bit ALU, we try to be clever 
**    about offset calculation in the TASK TABLE per LUN that is an 
**    array of DWORDS = 4 bytes.
*/
#if	MAX_TAGS > (512/4)
#define MAX_TASKS  (1024/4)
#elif	MAX_TAGS > (256/4) 
#define MAX_TASKS  (512/4)
#else
#define MAX_TASKS  (256/4)
#endif

/*
**    This one means 'NO TAG for this job'
*/
#define NO_TAG	(256)

/*
**    Number of targets supported by the driver.
**    n permits target numbers 0..n-1.
**    Default is 16, meaning targets #0..#15.
**    #7 .. is myself.
*/

#ifdef SCSI_NCR_MAX_TARGET
#define MAX_TARGET  (SCSI_NCR_MAX_TARGET)
#else
#define MAX_TARGET  (16)
#endif

/*
**    Number of logic units supported by the driver.
**    n enables logic unit numbers 0..n-1.
**    The common SCSI devices require only
**    one lun, so take 1 as the default.
*/

#ifdef SCSI_NCR_MAX_LUN
#define MAX_LUN    64
#else
#define MAX_LUN    (1)
#endif

/*
**    Asynchronous pre-scaler (ns). Shall be 40 for 
**    the SCSI timings to be compliant.
*/
 
#ifndef SCSI_NCR_MIN_ASYNC
#define SCSI_NCR_MIN_ASYNC (40)
#endif

/*
**    The maximum number of jobs scheduled for starting.
**    We allocate 4 entries more than the value we announce 
**    to the SCSI upper layer. Guess why ! :-)
*/

#ifdef SCSI_NCR_CAN_QUEUE
#define MAX_START   (SCSI_NCR_CAN_QUEUE + 4)
#else
#define MAX_START   (MAX_TARGET + 7 * MAX_TAGS)
#endif

/*
**    We donnot want to allocate more than 1 PAGE for the 
**    the start queue and the done queue. We hard-code entry 
**    size to 8 in order to let cpp do the checking.
**    Allows 512-4=508 pending IOs for i386 but Linux seems for 
**    now not able to provide the driver with this amount of IOs.
*/
#if	MAX_START > PAGE_SIZE/8
#undef	MAX_START
#define MAX_START (PAGE_SIZE/8)
#endif

/*
**    The maximum number of segments a transfer is split into.
**    We support up to 127 segments for both read and write.
*/

#define MAX_SCATTER (SCSI_NCR_MAX_SCATTER)
#define	SCR_SG_SIZE	(2)

/*
**	other
*/

#define NCR_SNOOP_TIMEOUT (1000000)

/*==========================================================
**
**	Miscallaneous BSDish defines.
**
**==========================================================
*/

#define u_char		unsigned char
#define u_short		unsigned short
#define u_int		unsigned int
#define u_long		unsigned long

#ifndef bcopy
#define bcopy(s, d, n)	memcpy((d), (s), (n))
#endif

#ifndef bzero
#define bzero(d, n)	memset((d), 0, (n))
#endif
 
#ifndef offsetof
#define offsetof(t, m)	((size_t) (&((t *)0)->m))
#endif

/*
**	Simple Wrapper to kernel PCI bus interface.
**
**	This wrapper allows to get rid of old kernel PCI interface 
**	and still allows to preserve linux-2.0 compatibilty.
**	In fact, it is mostly an incomplete emulation of the new 
**	PCI code for pre-2.2 kernels. When kernel-2.0 support 
**	will be dropped, we will just have to remove most of this 
**	code.
*/

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,2,0)

typedef struct pci_dev *pcidev_t;
#define PCIDEV_NULL		(0)
#define PciBusNumber(d)		(d)->bus->number
#define PciDeviceFn(d)		(d)->devfn
#define PciVendorId(d)		(d)->vendor
#define PciDeviceId(d)		(d)->device
#define PciIrqLine(d)		(d)->irq

static u_long __init
pci_get_base_cookie(struct pci_dev *pdev, int index)
{
	u_long base;

#if LINUX_VERSION_CODE > LinuxVersionCode(2,3,12)
	base = pdev->resource[index].start;
#else
	base = pdev->base_address[index];
#if BITS_PER_LONG > 32
	if ((base & 0x7) == 0x4)
		*base |= (((u_long)pdev->base_address[++index]) << 32);
#endif
#endif
	return (base & ~0x7ul);
}

static int __init
pci_get_base_address(struct pci_dev *pdev, int index, u_long *base)
{
	u32 tmp;
#define PCI_BAR_OFFSET(index) (PCI_BASE_ADDRESS_0 + (index<<2))

	pci_read_config_dword(pdev, PCI_BAR_OFFSET(index), &tmp);
	*base = tmp;
	++index;
	if ((tmp & 0x7) == 0x4) {
#if BITS_PER_LONG > 32
		pci_read_config_dword(pdev, PCI_BAR_OFFSET(index), &tmp);
		*base |= (((u_long)tmp) << 32);
#endif
		++index;
	}
	return index;
#undef PCI_BAR_OFFSET
}

#else	/* Incomplete emulation of current PCI code for pre-2.2 kernels */

typedef unsigned int pcidev_t;
#define PCIDEV_NULL		(~0u)
#define PciBusNumber(d)		((d)>>8)
#define PciDeviceFn(d)		((d)&0xff)
#define __PciDev(busn, devfn)	(((busn)<<8)+(devfn))

#define pci_present pcibios_present

#define pci_read_config_byte(d, w, v) \
	pcibios_read_config_byte(PciBusNumber(d), PciDeviceFn(d), w, v)
#define pci_read_config_word(d, w, v) \
	pcibios_read_config_word(PciBusNumber(d), PciDeviceFn(d), w, v)
#define pci_read_config_dword(d, w, v) \
	pcibios_read_config_dword(PciBusNumber(d), PciDeviceFn(d), w, v)

#define pci_write_config_byte(d, w, v) \
	pcibios_write_config_byte(PciBusNumber(d), PciDeviceFn(d), w, v)
#define pci_write_config_word(d, w, v) \
	pcibios_write_config_word(PciBusNumber(d), PciDeviceFn(d), w, v)
#define pci_write_config_dword(d, w, v) \
	pcibios_write_config_dword(PciBusNumber(d), PciDeviceFn(d), w, v)

static pcidev_t __init
pci_find_device(unsigned int vendor, unsigned int device, pcidev_t prev)
{
	static unsigned short pci_index;
	int retv;
	unsigned char bus_number, device_fn;

	if (prev == PCIDEV_NULL)
		pci_index = 0;
	else
		++pci_index;
	retv = pcibios_find_device (vendor, device, pci_index,
				    &bus_number, &device_fn);
	return retv ? PCIDEV_NULL : __PciDev(bus_number, device_fn);
}

static u_short __init PciVendorId(pcidev_t dev)
{
	u_short vendor_id;
	pci_read_config_word(dev, PCI_VENDOR_ID, &vendor_id);
	return vendor_id;
}

static u_short __init PciDeviceId(pcidev_t dev)
{
	u_short device_id;
	pci_read_config_word(dev, PCI_DEVICE_ID, &device_id);
	return device_id;
}

static u_int __init PciIrqLine(pcidev_t dev)
{
	u_char irq;
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	return irq;
}

static int __init 
pci_get_base_address(pcidev_t dev, int offset, u_long *base)
{
	u_int32 tmp;
	
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + offset, &tmp);
	*base = tmp;
	offset += sizeof(u_int32);
	if ((tmp & 0x7) == 0x4) {
#if BITS_PER_LONG > 32
		pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + offset, &tmp);
		*base |= (((u_long)tmp) << 32);
#endif
		offset += sizeof(u_int32);
	}
	return offset;
}
static u_long __init
pci_get_base_cookie(struct pci_dev *pdev, int offset)
{
	u_long base;

	(void) pci_get_base_address(dev, offset, &base);

	return base;
}

#endif	/* LINUX_VERSION_CODE >= LinuxVersionCode(2,2,0) */

/* Does not make sense in earlier kernels */
#if LINUX_VERSION_CODE < LinuxVersionCode(2,4,0)
#define pci_enable_device(pdev)		(0)
#endif
#if LINUX_VERSION_CODE < LinuxVersionCode(2,4,4)
#define	scsi_set_pci_device(inst, pdev)	(0)
#endif

/*==========================================================
**
**	Debugging tags
**
**==========================================================
*/

#define DEBUG_ALLOC    (0x0001)
#define DEBUG_PHASE    (0x0002)
#define DEBUG_QUEUE    (0x0008)
#define DEBUG_RESULT   (0x0010)
#define DEBUG_POINTER  (0x0020)
#define DEBUG_SCRIPT   (0x0040)
#define DEBUG_TINY     (0x0080)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
#define DEBUG_TAGS     (0x0400)
#define DEBUG_IC       (0x0800)

/*
**    Enable/Disable debug messages.
**    Can be changed at runtime too.
*/

#ifdef SCSI_NCR_DEBUG_INFO_SUPPORT
static int ncr_debug = SCSI_NCR_DEBUG_FLAGS;
	#define DEBUG_FLAGS ncr_debug
#else
	#define DEBUG_FLAGS	SCSI_NCR_DEBUG_FLAGS
#endif

/*
**	SMP threading.
**
**	Assuming that SMP systems are generally high end systems and may 
**	use several SCSI adapters, we are using one lock per controller 
**	instead of some global one. For the moment (linux-2.1.95), driver's 
**	entry points are called with the 'io_request_lock' lock held, so:
**	- We are uselessly loosing a couple of micro-seconds to lock the 
**	  controller data structure.
**	- But the driver is not broken by design for SMP and so can be 
**	  more resistant to bugs or bad changes in the IO sub-system code.
**	- A small advantage could be that the interrupt code is grained as 
**	  wished (e.g.: threaded by controller).
*/

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,93)

spinlock_t sym53c8xx_lock = SPIN_LOCK_UNLOCKED;
#define	NCR_LOCK_DRIVER(flags)     spin_lock_irqsave(&sym53c8xx_lock, flags)
#define	NCR_UNLOCK_DRIVER(flags)   spin_unlock_irqrestore(&sym53c8xx_lock,flags)

#define NCR_INIT_LOCK_NCB(np)      spin_lock_init(&np->smp_lock);
#define	NCR_LOCK_NCB(np, flags)    spin_lock_irqsave(&np->smp_lock, flags)
#define	NCR_UNLOCK_NCB(np, flags)  spin_unlock_irqrestore(&np->smp_lock, flags)

#define	NCR_LOCK_SCSI_DONE(np, flags) \
		spin_lock_irqsave(&io_request_lock, flags)
#define	NCR_UNLOCK_SCSI_DONE(np, flags) \
		spin_unlock_irqrestore(&io_request_lock, flags)

#else

#define	NCR_LOCK_DRIVER(flags)     do { save_flags(flags); cli(); } while (0)
#define	NCR_UNLOCK_DRIVER(flags)   do { restore_flags(flags); } while (0)

#define	NCR_INIT_LOCK_NCB(np)      do { } while (0)
#define	NCR_LOCK_NCB(np, flags)    do { save_flags(flags); cli(); } while (0)
#define	NCR_UNLOCK_NCB(np, flags)  do { restore_flags(flags); } while (0)

#define	NCR_LOCK_SCSI_DONE(np, flags)    do {;} while (0)
#define	NCR_UNLOCK_SCSI_DONE(np, flags)  do {;} while (0)

#endif

/*
**	Memory mapped IO
**
**	Since linux-2.1, we must use ioremap() to map the io memory space.
**	iounmap() to unmap it. That allows portability.
**	Linux 1.3.X and 2.0.X allow to remap physical pages addresses greater 
**	than the highest physical memory address to kernel virtual pages with 
**	vremap() / vfree(). That was not portable but worked with i386 
**	architecture.
*/

#if LINUX_VERSION_CODE < LinuxVersionCode(2,1,0)
#define ioremap vremap
#define iounmap vfree
#endif

#ifdef __sparc__
#  include <asm/irq.h>
#  define memcpy_to_pci(a, b, c)	memcpy_toio((a), (b), (c))
#elif defined(__alpha__)
#  define memcpy_to_pci(a, b, c)	memcpy_toio((a), (b), (c))
#else	/* others */
#  define memcpy_to_pci(a, b, c)	memcpy_toio((a), (b), (c))
#endif

#ifndef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
static u_long __init remap_pci_mem(u_long base, u_long size)
{
	u_long page_base	= ((u_long) base) & PAGE_MASK;
	u_long page_offs	= ((u_long) base) - page_base;
	u_long page_remapped	= (u_long) ioremap(page_base, page_offs+size);

	return page_remapped? (page_remapped + page_offs) : 0UL;
}

static void __init unmap_pci_mem(u_long vaddr, u_long size)
{
	if (vaddr)
		iounmap((void *) (vaddr & PAGE_MASK));
}

#endif /* not def SCSI_NCR_PCI_MEM_NOT_SUPPORTED */

/*
**	Insert a delay in micro-seconds and milli-seconds.
**	-------------------------------------------------
**	Under Linux, udelay() is restricted to delay < 1 milli-second.
**	In fact, it generally works for up to 1 second delay.
**	Since 2.1.105, the mdelay() function is provided for delays 
**	in milli-seconds.
**	Under 2.0 kernels, udelay() is an inline function that is very 
**	inaccurate on Pentium processors.
*/

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,105)
#define UDELAY udelay
#define MDELAY mdelay
#else
static void UDELAY(long us) { udelay(us); }
static void MDELAY(long ms) { while (ms--) UDELAY(1000); }
#endif

/*
**	Simple power of two buddy-like allocator
**	----------------------------------------
**	This simple code is not intended to be fast, but to provide 
**	power of 2 aligned memory allocations.
**	Since the SCRIPTS processor only supplies 8 bit arithmetic,
**	this allocator allows simple and fast address calculations  
**	from the SCRIPTS code. In addition, cache line alignment 
**	is guaranteed for power of 2 cache line size.
**	Enhanced in linux-2.3.44 to provide a memory pool per pcidev 
**	to support dynamic dma mapping. (I would have preferred a 
**	real bus astraction, btw).
*/

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,0)
#define __GetFreePages(flags, order) __get_free_pages(flags, order)
#else
#define __GetFreePages(flags, order) __get_free_pages(flags, order, 0)
#endif

#define MEMO_SHIFT	4	/* 16 bytes minimum memory chunk */
#if PAGE_SIZE >= 8192
#define MEMO_PAGE_ORDER	0	/* 1 PAGE  maximum */
#else
#define MEMO_PAGE_ORDER	1	/* 2 PAGES maximum */
#endif
#define MEMO_FREE_UNUSED	/* Free unused pages immediately */
#define MEMO_WARN	1
#define MEMO_GFP_FLAGS	GFP_ATOMIC
#define MEMO_CLUSTER_SHIFT	(PAGE_SHIFT+MEMO_PAGE_ORDER)
#define MEMO_CLUSTER_SIZE	(1UL << MEMO_CLUSTER_SHIFT)
#define MEMO_CLUSTER_MASK	(MEMO_CLUSTER_SIZE-1)

typedef u_long m_addr_t;	/* Enough bits to bit-hack addresses */
typedef pcidev_t m_bush_t;	/* Something that addresses DMAable */

typedef struct m_link {		/* Link between free memory chunks */
	struct m_link *next;
} m_link_s;

#ifdef	SCSI_NCR_DYNAMIC_DMA_MAPPING
typedef struct m_vtob {		/* Virtual to Bus address translation */
	struct m_vtob *next;
	m_addr_t vaddr;
	m_addr_t baddr;
} m_vtob_s;
#define VTOB_HASH_SHIFT		5
#define VTOB_HASH_SIZE		(1UL << VTOB_HASH_SHIFT)
#define VTOB_HASH_MASK		(VTOB_HASH_SIZE-1)
#define VTOB_HASH_CODE(m)	\
	((((m_addr_t) (m)) >> MEMO_CLUSTER_SHIFT) & VTOB_HASH_MASK)
#endif

typedef struct m_pool {		/* Memory pool of a given kind */
#ifdef	SCSI_NCR_DYNAMIC_DMA_MAPPING
	m_bush_t bush;
	m_addr_t (*getp)(struct m_pool *);
	void (*freep)(struct m_pool *, m_addr_t);
#define M_GETP()		mp->getp(mp)
#define M_FREEP(p)		mp->freep(mp, p)
#define GetPages()		__GetFreePages(MEMO_GFP_FLAGS, MEMO_PAGE_ORDER)
#define FreePages(p)		free_pages(p, MEMO_PAGE_ORDER)
	int nump;
	m_vtob_s *(vtob[VTOB_HASH_SIZE]);
	struct m_pool *next;
#else
#define M_GETP()		__GetFreePages(MEMO_GFP_FLAGS, MEMO_PAGE_ORDER)
#define M_FREEP(p)		free_pages(p, MEMO_PAGE_ORDER)
#endif	/* SCSI_NCR_DYNAMIC_DMA_MAPPING */
	struct m_link h[PAGE_SHIFT-MEMO_SHIFT+MEMO_PAGE_ORDER+1];
} m_pool_s;

static void *___m_alloc(m_pool_s *mp, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	int j;
	m_addr_t a;
	m_link_s *h = mp->h;

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return 0;

	while (size > s) {
		s <<= 1;
		++i;
	}

	j = i;
	while (!h[j].next) {
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			h[j].next = (m_link_s *) M_GETP();
			if (h[j].next)
				h[j].next->next = 0;
			break;
		}
		++j;
		s <<= 1;
	}
	a = (m_addr_t) h[j].next;
	if (a) {
		h[j].next = h[j].next->next;
		while (j > i) {
			j -= 1;
			s >>= 1;
			h[j].next = (m_link_s *) (a+s);
			h[j].next->next = 0;
		}
	}
#ifdef DEBUG
	printk("___m_alloc(%d) = %p\n", size, (void *) a);
#endif
	return (void *) a;
}

static void ___m_free(m_pool_s *mp, void *ptr, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	m_link_s *q;
	m_addr_t a, b;
	m_link_s *h = mp->h;

#ifdef DEBUG
	printk("___m_free(%p, %d)\n", ptr, size);
#endif

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return;

	while (size > s) {
		s <<= 1;
		++i;
	}

	a = (m_addr_t) ptr;

	while (1) {
#ifdef MEMO_FREE_UNUSED
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			M_FREEP(a);
			break;
		}
#endif
		b = a ^ s;
		q = &h[i];
		while (q->next && q->next != (m_link_s *) b) {
			q = q->next;
		}
		if (!q->next) {
			((m_link_s *) a)->next = h[i].next;
			h[i].next = (m_link_s *) a;
			break;
		}
		q->next = q->next->next;
		a = a & b;
		s <<= 1;
		++i;
	}
}

static void *__m_calloc2(m_pool_s *mp, int size, char *name, int uflags)
{
	void *p;

	p = ___m_alloc(mp, size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("new %-10s[%4d] @%p.\n", name, size, p);

	if (p)
		bzero(p, size);
	else if (uflags & MEMO_WARN)
		printk (NAME53C8XX ": failed to allocate %s[%d]\n", name, size);

	return p;
}

#define __m_calloc(mp, s, n)	__m_calloc2(mp, s, n, MEMO_WARN)

static void __m_free(m_pool_s *mp, void *ptr, int size, char *name)
{
	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("freeing %-10s[%4d] @%p.\n", name, size, ptr);

	___m_free(mp, ptr, size);

}

/*
 * With pci bus iommu support, we use a default pool of unmapped memory 
 * for memory we donnot need to DMA from/to and one pool per pcidev for 
 * memory accessed by the PCI chip. `mp0' is the default not DMAable pool.
 */

#ifndef	SCSI_NCR_DYNAMIC_DMA_MAPPING

static m_pool_s mp0;

#else

static m_addr_t ___mp0_getp(m_pool_s *mp)
{
	m_addr_t m = GetPages();
	if (m)
		++mp->nump;
	return m;
}

static void ___mp0_freep(m_pool_s *mp, m_addr_t m)
{
	FreePages(m);
	--mp->nump;
}

static m_pool_s mp0 = {0, ___mp0_getp, ___mp0_freep};

#endif	/* SCSI_NCR_DYNAMIC_DMA_MAPPING */

static void *m_calloc(int size, char *name)
{
	u_long flags;
	void *m;
	NCR_LOCK_DRIVER(flags);
	m = __m_calloc(&mp0, size, name);
	NCR_UNLOCK_DRIVER(flags);
	return m;
}

static void m_free(void *ptr, int size, char *name)
{
	u_long flags;
	NCR_LOCK_DRIVER(flags);
	__m_free(&mp0, ptr, size, name);
	NCR_UNLOCK_DRIVER(flags);
}

/*
 * DMAable pools.
 */

#ifndef	SCSI_NCR_DYNAMIC_DMA_MAPPING

/* Without pci bus iommu support, all the memory is assumed DMAable */

#define __m_calloc_dma(b, s, n)		m_calloc(s, n)
#define __m_free_dma(b, p, s, n)	m_free(p, s, n)
#define __vtobus(b, p)			virt_to_bus(p)

#else

/*
 * With pci bus iommu support, we maintain one pool per pcidev and a 
 * hashed reverse table for virtual to bus physical address translations.
 */
static m_addr_t ___dma_getp(m_pool_s *mp)
{
	m_addr_t vp;
	m_vtob_s *vbp;

	vbp = __m_calloc(&mp0, sizeof(*vbp), "VTOB");
	if (vbp) {
		dma_addr_t daddr;
		vp = (m_addr_t) pci_alloc_consistent(mp->bush,
						     PAGE_SIZE<<MEMO_PAGE_ORDER,
						     &daddr);
		if (vp) {
			int hc = VTOB_HASH_CODE(vp);
			vbp->vaddr = vp;
			vbp->baddr = daddr;
			vbp->next = mp->vtob[hc];
			mp->vtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
		else
			__m_free(&mp0, vbp, sizeof(*vbp), "VTOB");
	}
	return 0;
}

static void ___dma_freep(m_pool_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_HASH_CODE(m);

	vbpp = &mp->vtob[hc];
	while (*vbpp && (*vbpp)->vaddr != m)
		vbpp = &(*vbpp)->next;
	if (*vbpp) {
		vbp = *vbpp;
		*vbpp = (*vbpp)->next;
		pci_free_consistent(mp->bush, PAGE_SIZE<<MEMO_PAGE_ORDER,
				    (void *)vbp->vaddr, (dma_addr_t)vbp->baddr);
		__m_free(&mp0, vbp, sizeof(*vbp), "VTOB");
		--mp->nump;
	}
}

static inline m_pool_s *___get_dma_pool(m_bush_t bush)
{
	m_pool_s *mp;
	for (mp = mp0.next; mp && mp->bush != bush; mp = mp->next);
	return mp;
}

static m_pool_s *___cre_dma_pool(m_bush_t bush)
{
	m_pool_s *mp;
	mp = __m_calloc(&mp0, sizeof(*mp), "MPOOL");
	if (mp) {
		bzero(mp, sizeof(*mp));
		mp->bush = bush;
		mp->getp = ___dma_getp;
		mp->freep = ___dma_freep;
		mp->next = mp0.next;
		mp0.next = mp;
	}
	return mp;
}

static void ___del_dma_pool(m_pool_s *p)
{
	struct m_pool **pp = &mp0.next;

	while (*pp && *pp != p)
		pp = &(*pp)->next;
	if (*pp) {
		*pp = (*pp)->next;
		__m_free(&mp0, p, sizeof(*p), "MPOOL");
	}
}

static void *__m_calloc_dma(m_bush_t bush, int size, char *name)
{
	u_long flags;
	struct m_pool *mp;
	void *m = 0;

	NCR_LOCK_DRIVER(flags);
	mp = ___get_dma_pool(bush);
	if (!mp)
		mp = ___cre_dma_pool(bush);
	if (mp)
		m = __m_calloc(mp, size, name);
	if (mp && !mp->nump)
		___del_dma_pool(mp);
	NCR_UNLOCK_DRIVER(flags);

	return m;
}

static void __m_free_dma(m_bush_t bush, void *m, int size, char *name)
{
	u_long flags;
	struct m_pool *mp;

	NCR_LOCK_DRIVER(flags);
	mp = ___get_dma_pool(bush);
	if (mp)
		__m_free(mp, m, size, name);
	if (mp && !mp->nump)
		___del_dma_pool(mp);
	NCR_UNLOCK_DRIVER(flags);
}

static m_addr_t __vtobus(m_bush_t bush, void *m)
{
	u_long flags;
	m_pool_s *mp;
	int hc = VTOB_HASH_CODE(m);
	m_vtob_s *vp = 0;
	m_addr_t a = ((m_addr_t) m) & ~MEMO_CLUSTER_MASK;

	NCR_LOCK_DRIVER(flags);
	mp = ___get_dma_pool(bush);
	if (mp) {
		vp = mp->vtob[hc];
		while (vp && (m_addr_t) vp->vaddr != a)
			vp = vp->next;
	}
	NCR_UNLOCK_DRIVER(flags);
	return vp ? vp->baddr + (((m_addr_t) m) - a) : 0;
}

#endif	/* SCSI_NCR_DYNAMIC_DMA_MAPPING */

#define _m_calloc_dma(np, s, n)		__m_calloc_dma(np->pdev, s, n)
#define _m_free_dma(np, p, s, n)	__m_free_dma(np->pdev, p, s, n)
#define m_calloc_dma(s, n)		_m_calloc_dma(np, s, n)
#define m_free_dma(p, s, n)		_m_free_dma(np, p, s, n)
#define _vtobus(np, p)			__vtobus(np->pdev, p)
#define vtobus(p)			_vtobus(np, p)

/*
 *  Deal with DMA mapping/unmapping.
 */

#ifndef SCSI_NCR_DYNAMIC_DMA_MAPPING

/* Linux versions prior to pci bus iommu kernel interface */

#define __unmap_scsi_data(pdev, cmd)	do {; } while (0)
#define __map_scsi_single_data(pdev, cmd) (__vtobus(pdev,(cmd)->request_buffer))
#define __map_scsi_sg_data(pdev, cmd)	((cmd)->use_sg)
#define __sync_scsi_data(pdev, cmd)	do {; } while (0)

#define scsi_sg_dma_address(sc)		vtobus((sc)->address)
#define scsi_sg_dma_len(sc)		((sc)->length)

#else

/* Linux version with pci bus iommu kernel interface */

/* To keep track of the dma mapping (sg/single) that has been set */
#define __data_mapped(cmd)	(cmd)->SCp.phase
#define __data_mapping(cmd)	(cmd)->SCp.dma_handle

static void __unmap_scsi_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	switch(__data_mapped(cmd)) {
	case 2:
		pci_unmap_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);
		break;
	case 1:
		pci_unmap_page(pdev, __data_mapping(cmd),
			       cmd->request_bufflen, dma_dir);
		break;
	}
	__data_mapped(cmd) = 0;
}

static dma_addr_t __map_scsi_single_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	dma_addr_t mapping;
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	if (cmd->request_bufflen == 0)
		return 0;

	mapping = pci_map_page(pdev,
			       virt_to_page(cmd->request_buffer),
			       ((unsigned long)cmd->request_buffer &
				~PAGE_MASK),
			       cmd->request_bufflen, dma_dir);
	__data_mapped(cmd) = 1;
	__data_mapping(cmd) = mapping;

	return mapping;
}

static int __map_scsi_sg_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	int use_sg;
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	if (cmd->use_sg == 0)
		return 0;

	use_sg = pci_map_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);
	__data_mapped(cmd) = 2;
	__data_mapping(cmd) = use_sg;

	return use_sg;
}

static void __sync_scsi_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	switch(__data_mapped(cmd)) {
	case 2:
		pci_dma_sync_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);
		break;
	case 1:
		pci_dma_sync_single(pdev, __data_mapping(cmd),
				    cmd->request_bufflen, dma_dir);
		break;
	}
}

#define scsi_sg_dma_address(sc)		sg_dma_address(sc)
#define scsi_sg_dma_len(sc)		sg_dma_len(sc)

#endif	/* SCSI_NCR_DYNAMIC_DMA_MAPPING */

#define unmap_scsi_data(np, cmd)	__unmap_scsi_data(np->pdev, cmd)
#define map_scsi_single_data(np, cmd)	__map_scsi_single_data(np->pdev, cmd)
#define map_scsi_sg_data(np, cmd)	__map_scsi_sg_data(np->pdev, cmd)
#define sync_scsi_data(np, cmd)		__sync_scsi_data(np->pdev, cmd)


/*
 * Print out some buffer.
 */
static void ncr_print_hex(u_char *p, int n)
{
	while (n-- > 0)
		printk (" %x", *p++);
}

static void ncr_printl_hex(char *label, u_char *p, int n)
{
	printk("%s", label);
	ncr_print_hex(p, n);
	printk (".\n");
}

/*
**	Transfer direction
**
**	Until some linux kernel version near 2.3.40, low-level scsi 
**	drivers were not told about data transfer direction.
**	We check the existence of this feature that has been expected 
**	for a _long_ time by all SCSI driver developers by just 
**	testing against the definition of SCSI_DATA_UNKNOWN. Indeed 
**	this is a hack, but testing against a kernel version would 
**	have been a shame. ;-)
*/
#ifdef	SCSI_DATA_UNKNOWN

#define scsi_data_direction(cmd)	(cmd->sc_data_direction)

#else

#define	SCSI_DATA_UNKNOWN	0
#define	SCSI_DATA_WRITE		1
#define	SCSI_DATA_READ		2
#define	SCSI_DATA_NONE		3

static __inline__ int scsi_data_direction(Scsi_Cmnd *cmd)
{
	int direction;

	switch((int) cmd->cmnd[0]) {
	case 0x08:  /*	READ(6)				08 */
	case 0x28:  /*	READ(10)			28 */
	case 0xA8:  /*	READ(12)			A8 */
		direction = SCSI_DATA_READ;
		break;
	case 0x0A:  /*	WRITE(6)			0A */
	case 0x2A:  /*	WRITE(10)			2A */
	case 0xAA:  /*	WRITE(12)			AA */
		direction = SCSI_DATA_WRITE;
		break;
	default:
		direction = SCSI_DATA_UNKNOWN;
		break;
	}

	return direction;
}

#endif	/* SCSI_DATA_UNKNOWN */

/*
**	Head of list of NCR boards
**
**	For kernel version < 1.3.70, host is retrieved by its irq level.
**	For later kernels, the internal host control block address 
**	(struct ncb) is used as device id parameter of the irq stuff.
*/

static struct Scsi_Host	*first_host = NULL;


/*
**	/proc directory entry and proc_info function
*/
#if LINUX_VERSION_CODE < LinuxVersionCode(2,3,27)
static struct proc_dir_entry proc_scsi_sym53c8xx = {
    PROC_SCSI_SYM53C8XX, 9, NAME53C8XX,
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};
#endif
#ifdef SCSI_NCR_PROC_INFO_SUPPORT
static int sym53c8xx_proc_info(char *buffer, char **start, off_t offset,
			int length, int hostno, int func);
#endif

/*
**	Driver setup.
**
**	This structure is initialized from linux config options.
**	It can be overridden at boot-up by the boot command line.
*/
static struct ncr_driver_setup
	driver_setup			= SCSI_NCR_DRIVER_SETUP;

#ifdef	SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
static struct ncr_driver_setup
	driver_safe_setup __initdata	= SCSI_NCR_DRIVER_SAFE_SETUP;
# ifdef	MODULE
char *sym53c8xx = 0;	/* command line passed by insmod */
#  if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,30)
MODULE_PARM(sym53c8xx, "s");
#  endif
# endif
#endif

/*
**	Other Linux definitions
*/
#define SetScsiResult(cmd, h_sts, s_sts) \
	cmd->result = (((h_sts) << 16) + ((s_sts) & 0x7f))

/* We may have to remind our amnesiac SCSI layer of the reason of the abort */
#if 0
#define SetScsiAbortResult(cmd)	\
	  SetScsiResult(	\
	    cmd, 		\
	    (cmd)->abort_reason == DID_TIME_OUT ? DID_TIME_OUT : DID_ABORT, \
	    0xff)
#else
#define SetScsiAbortResult(cmd) SetScsiResult(cmd, DID_ABORT, 0xff)
#endif

static void sym53c8xx_select_queue_depths(
	struct Scsi_Host *host, struct scsi_device *devlist);
static void sym53c8xx_intr(int irq, void *dev_id, struct pt_regs * regs);
static void sym53c8xx_timeout(unsigned long np);

#define initverbose (driver_setup.verbose)
#define bootverbose (np->verbose)

#ifdef SCSI_NCR_NVRAM_SUPPORT
static u_char Tekram_sync[16] __initdata =
	{25,31,37,43, 50,62,75,125, 12,15,18,21, 6,7,9,10};
#endif /* SCSI_NCR_NVRAM_SUPPORT */

/*
**	Structures used by sym53c8xx_detect/sym53c8xx_pci_init to 
**	transmit device configuration to the ncr_attach() function.
*/
typedef struct {
	int	bus;
	u_char	device_fn;
	u_long	base;
	u_long	base_2;
	u_long	io_port;
	u_long	base_c;
	u_long	base_2_c;
	int	irq;
/* port and reg fields to use INB, OUTB macros */
	u_long	base_io;
	volatile struct ncr_reg	*reg;
} ncr_slot;

typedef struct {
	int type;
#define	SCSI_NCR_SYMBIOS_NVRAM	(1)
#define	SCSI_NCR_TEKRAM_NVRAM	(2)
#ifdef	SCSI_NCR_NVRAM_SUPPORT
	union {
		Symbios_nvram Symbios;
		Tekram_nvram Tekram;
	} data;
#endif
} ncr_nvram;

/*
**	Structure used by sym53c8xx_detect/sym53c8xx_pci_init
**	to save data on each detected board for ncr_attach().
*/
typedef struct {
	pcidev_t  pdev;
	ncr_slot  slot;
	ncr_chip  chip;
	ncr_nvram *nvram;
	u_char host_id;
#ifdef	SCSI_NCR_PQS_PDS_SUPPORT
	u_char pqs_pds;
#endif
	int attach_done;
} ncr_device;

/*==========================================================
**
**	assert ()
**
**==========================================================
**
**	modified copy from 386bsd:/usr/include/sys/assert.h
**
**----------------------------------------------------------
*/

#define	assert(expression) { \
	if (!(expression)) { \
		(void)panic( \
			"assertion \"%s\" failed: file \"%s\", line %d\n", \
			#expression, \
			__FILE__, __LINE__); \
	} \
}

/*==========================================================
**
**	Command control block states.
**
**==========================================================
*/

#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	/* sync/wide data transfer*/
#define HS_DISCONNECT	(3)	/* Disconnected by target */

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	/* Selection timeout      */
#define HS_RESET	(6|HS_DONEMASK)	/* SCSI reset	          */
#define HS_ABORTED	(7|HS_DONEMASK)	/* Transfer aborted       */
#define HS_TIMEOUT	(8|HS_DONEMASK)	/* Software timeout       */
#define HS_FAIL		(9|HS_DONEMASK)	/* SCSI or PCI bus errors */
#define HS_UNEXPECTED	(10|HS_DONEMASK)/* Unexpected disconnect  */

#define DSA_INVALID 0xffffffff

/*==========================================================
**
**	Software Interrupt Codes
**
**==========================================================
*/

#define	SIR_BAD_STATUS		(1)
#define	SIR_SEL_ATN_NO_MSG_OUT	(2)
#define	SIR_MSG_RECEIVED	(3)
#define	SIR_MSG_WEIRD		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_SCRIPT_STOPPED	(7)
#define	SIR_REJECT_TO_SEND	(8)
#define	SIR_SWIDE_OVERRUN	(9)
#define	SIR_SODL_UNDERRUN	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_TARGET_SELECTED	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_ABORT_SENT		(17)
#define	SIR_RESEL_ABORTED	(18)
#define	SIR_MSG_OUT_DONE	(19)
#define	SIR_AUTO_SENSE_DONE	(20)
#define	SIR_DUMMY_INTERRUPT	(21)
#define	SIR_DATA_OVERRUN	(22)
#define	SIR_BAD_PHASE		(23)
#define	SIR_MAX			(23)

/*==========================================================
**
**	Extended error bits.
**	xerr_status field of struct ccb.
**
**==========================================================
*/

#define	XE_EXTRA_DATA	(1)	/* unexpected data phase	 */
#define	XE_BAD_PHASE	(2)	/* illegal phase (4/5)		 */
#define	XE_PARITY_ERR	(4)	/* unrecovered SCSI parity error */
#define XE_SODL_UNRUN   (1<<3)
#define XE_SWIDE_OVRUN  (1<<4)

/*==========================================================
**
**	Negotiation status.
**	nego_status field	of struct ccb.
**
**==========================================================
*/

#define NS_NOCHANGE	(0)
#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(4)

/*==========================================================
**
**	"Special features" of targets.
**	quirks field		of struct tcb.
**	actualquirks field	of struct ccb.
**
**==========================================================
*/

#define	QUIRK_AUTOSAVE	(0x01)

/*==========================================================
**
**	Capability bits in Inquire response byte 7.
**
**==========================================================
*/

#define	INQ7_QUEUE	(0x02)
#define	INQ7_SYNC	(0x10)
#define	INQ7_WIDE16	(0x20)

/*==========================================================
**
**	A CCB hashed table is used to retrieve CCB address 
**	from DSA value.
**
**==========================================================
*/

#define CCB_HASH_SHIFT		8
#define CCB_HASH_SIZE		(1UL << CCB_HASH_SHIFT)
#define CCB_HASH_MASK		(CCB_HASH_SIZE-1)
#define CCB_HASH_CODE(dsa)	(((dsa) >> 11) & CCB_HASH_MASK)

/*==========================================================
**
**	Declaration of structs.
**
**==========================================================
*/

struct tcb;
struct lcb;
struct ccb;
struct ncb;
struct script;

typedef struct ncb * ncb_p;
typedef struct tcb * tcb_p;
typedef struct lcb * lcb_p;
typedef struct ccb * ccb_p;

struct link {
	ncrcmd	l_cmd;
	ncrcmd	l_paddr;
};

struct	usrcmd {
	u_long	target;
	u_long	lun;
	u_long	data;
	u_long	cmd;
};

#define UC_SETSYNC      10
#define UC_SETTAGS	11
#define UC_SETDEBUG	12
#define UC_SETORDER	13
#define UC_SETWIDE	14
#define UC_SETFLAG	15
#define UC_SETVERBOSE	17
#define UC_RESETDEV	18
#define UC_CLEARDEV	19

#define	UF_TRACE	(0x01)
#define	UF_NODISC	(0x02)
#define	UF_NOSCAN	(0x04)

/*========================================================================
**
**	Declaration of structs:		target control block
**
**========================================================================
*/
struct tcb {
	/*----------------------------------------------------------------
	**	LUN tables.
	**	An array of bus addresses is used on reselection by 
	**	the SCRIPT.
	**----------------------------------------------------------------
	*/
	u_int32		*luntbl;	/* lcbs bus address table	*/
	u_int32		b_luntbl;	/* bus address of this table	*/
	u_int32		b_lun0;		/* bus address of lun0		*/
	lcb_p		l0p;		/* lcb of LUN #0 (normal case)	*/
#if MAX_LUN > 1
	lcb_p		*lmp;		/* Other lcb's [1..MAX_LUN]	*/
#endif
	/*----------------------------------------------------------------
	**	Target capabilities.
	**----------------------------------------------------------------
	*/
	u_char		inq_done;	/* Target capabilities received	*/
	u_char		inq_byte7;	/* Contains these capabilities	*/

	/*----------------------------------------------------------------
	**	Some flags.
	**----------------------------------------------------------------
	*/
	u_char		to_reset;	/* This target is to be reset	*/

	/*----------------------------------------------------------------
	**	Pointer to the ccb used for negotiation.
	**	Prevent from starting a negotiation for all queued commands 
	**	when tagged command queuing is enabled.
	**----------------------------------------------------------------
	*/
	ccb_p   nego_cp;

	/*----------------------------------------------------------------
	**	negotiation of wide and synch transfer and device quirks.
	**	sval, wval and uval are read from SCRIPTS and so have alignment 
	**	constraints.
	**----------------------------------------------------------------
	*/
/*0*/	u_char	uval;
/*1*/	u_char	sval;
/*2*/	u_char	filler2;
/*3*/	u_char	wval;
	u_short	period;
	u_char	minsync;
	u_char	maxoffs;
	u_char	quirks;
	u_char	widedone;

#ifdef	SCSI_NCR_INTEGRITY_CHECKING
	u_char ic_min_sync;
	u_char ic_max_width;
	u_char ic_done;
#endif
	u_char ic_maximums_set;
	u_char ppr_negotiation;

	/*----------------------------------------------------------------
	**	User settable limits and options.
	**	These limits are read from the NVRAM if present.
	**----------------------------------------------------------------
	*/
	u_char	usrsync;
	u_char	usrwide;
	u_short	usrtags;
	u_char	usrflag;
};

/*========================================================================
**
**	Declaration of structs:		lun control block
**
**========================================================================
*/
struct lcb {
	/*----------------------------------------------------------------
	**	On reselection, SCRIPTS use this value as a JUMP address 
	**	after the IDENTIFY has been successfully received.
	**	This field is set to 'resel_tag' if TCQ is enabled and 
	**	to 'resel_notag' if TCQ is disabled.
	**	(Must be at zero due to bad lun handling on reselection)
	**----------------------------------------------------------------
	*/
/*0*/	u_int32		resel_task;

	/*----------------------------------------------------------------
	**	Task table used by the script processor to retrieve the 
	**	task corresponding to a reselected nexus. The TAG is used 
	**	as offset to determine the corresponding entry.
	**	Each entry contains the associated CCB bus address.
	**----------------------------------------------------------------
	*/
	u_int32		tasktbl_0;	/* Used if TCQ not enabled	*/
	u_int32		*tasktbl;
	u_int32		b_tasktbl;

	/*----------------------------------------------------------------
	**	CCB queue management.
	**----------------------------------------------------------------
	*/
	XPT_QUEHEAD	busy_ccbq;	/* Queue of busy CCBs		*/
	XPT_QUEHEAD	wait_ccbq;	/* Queue of waiting for IO CCBs	*/
	u_short		busyccbs;	/* CCBs busy for this lun	*/
	u_short		queuedccbs;	/* CCBs queued to the controller*/
	u_short		queuedepth;	/* Queue depth for this lun	*/
	u_short		scdev_depth;	/* SCSI device queue depth	*/
	u_short		maxnxs;		/* Max possible nexuses		*/

	/*----------------------------------------------------------------
	**	Control of tagged command queuing.
	**	Tags allocation is performed using a circular buffer.
	**	This avoids using a loop for tag allocation.
	**----------------------------------------------------------------
	*/
	u_short		ia_tag;		/* Tag allocation index		*/
	u_short		if_tag;		/* Tag release index		*/
	u_char		*cb_tags;	/* Circular tags buffer		*/
	u_char		inq_byte7;	/* Store unit CmdQ capability	*/
	u_char		usetags;	/* Command queuing is active	*/
	u_char		to_clear;	/* User wants to clear all tasks*/
	u_short		maxtags;	/* Max NR of tags asked by user	*/
	u_short		numtags;	/* Current number of tags	*/

	/*----------------------------------------------------------------
	**	QUEUE FULL and ORDERED tag control.
	**----------------------------------------------------------------
	*/
	u_short		num_good;	/* Nr of GOOD since QUEUE FULL	*/
	u_short		tags_sum[2];	/* Tags sum counters		*/
	u_char		tags_si;	/* Current index to tags sum	*/
	u_long		tags_stime;	/* Last time we switch tags_sum	*/
};

/*========================================================================
**
**      Declaration of structs: actions for a task.
**
**========================================================================
**
**	It is part of the CCB and is called by the scripts processor to 
**	start or restart the data structure (nexus).
**
**------------------------------------------------------------------------
*/
struct action {
	u_int32		start;
	u_int32		restart;
};

/*========================================================================
**
**      Declaration of structs: Phase mismatch context.
**
**========================================================================
**
**	It is part of the CCB and is used as parameters for the DATA 
**	pointer. We need two contexts to handle correctly the SAVED 
**	DATA POINTER.
**
**------------------------------------------------------------------------
*/
struct pm_ctx {
	struct scr_tblmove sg;	/* Updated interrupted SG block	*/
	u_int32	ret;		/* SCRIPT return address	*/
};

/*========================================================================
**
**      Declaration of structs:     global HEADER.
**
**========================================================================
**
**	In earlier driver versions, this substructure was copied from the 
**	ccb to a global address after selection (or reselection) and copied 
**	back before disconnect. Since we are now using LOAD/STORE DSA 
**	RELATIVE instructions, the script is able to access directly these 
**	fields, and so, this header is no more copied.
**
**------------------------------------------------------------------------
*/

struct head {
	/*----------------------------------------------------------------
	**	Start and restart SCRIPTS addresses (must be at 0).
	**----------------------------------------------------------------
	*/
	struct action	go;

	/*----------------------------------------------------------------
	**	Saved data pointer.
	**	Points to the position in the script responsible for the
	**	actual transfer of data.
	**	It's written after reception of a SAVE_DATA_POINTER message.
	**	The goalpointer points after the last transfer command.
	**----------------------------------------------------------------
	*/
	u_int32		savep;
	u_int32		lastp;
	u_int32		goalp;

	/*----------------------------------------------------------------
	**	Alternate data pointer.
	**	They are copied back to savep/lastp/goalp by the SCRIPTS 
	**	when the direction is unknown and the device claims data out.
	**----------------------------------------------------------------
	*/
	u_int32		wlastp;
	u_int32		wgoalp;

	/*----------------------------------------------------------------
	**	Status fields.
	**----------------------------------------------------------------
	*/
	u_char		status[4];	/* host status			*/
};

/*
**	LUN control block lookup.
**	We use a direct pointer for LUN #0, and a table of pointers 
**	which is only allocated for devices that support LUN(s) > 0.
*/
#if MAX_LUN <= 1
#define ncr_lp(np, tp, lun) (!lun) ? (tp)->l0p : 0
#else
#define ncr_lp(np, tp, lun) \
	(!lun) ? (tp)->l0p : (tp)->lmp ? (tp)->lmp[(lun)] : 0
#endif

/*
**	The status bytes are used by the host and the script processor.
**
**	The four bytes (status[4]) are copied to the scratchb register
**	(declared as scr0..scr3 in ncr_reg.h) just after the select/reselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_REG are used.
*/

/*
**	Last four bytes (script)
*/
#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  HF_REG	scr3
#define  HF_PRT	nc_scr3

/*
**	Last four bytes (host)
*/
#define  actualquirks  phys.header.status[0]
#define  host_status   phys.header.status[1]
#define  scsi_status   phys.header.status[2]
#define  host_flags    phys.header.status[3]

/*
**	Host flags
*/
#define HF_IN_PM0	1u
#define HF_IN_PM1	(1u<<1)
#define HF_ACT_PM	(1u<<2)
#define HF_DP_SAVED	(1u<<3)
#define HF_AUTO_SENSE	(1u<<4)
#define HF_DATA_IN	(1u<<5)
#define HF_PM_TO_C	(1u<<6)
#define HF_EXT_ERR	(1u<<7)

#ifdef SCSI_NCR_IARB_SUPPORT
#define HF_HINT_IARB	(1u<<7)
#endif

/*
**	This one is stolen from QU_REG.:)
*/
#define HF_DATA_ST	(1u<<7)

/*==========================================================
**
**      Declaration of structs:     Data structure block
**
**==========================================================
**
**	During execution of a ccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the ccb.
**	This substructure contains the header with
**	the script-processor-changable data and
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/

struct dsb {

	/*
	**	Header.
	*/

	struct head	header;

	/*
	**	Table data for Script
	*/

	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove smsg_ext ;
	struct scr_tblmove cmd   ;
	struct scr_tblmove sense ;
	struct scr_tblmove wresid;
	struct scr_tblmove data [MAX_SCATTER];

	/*
	**	Phase mismatch contexts.
	**	We need two to handle correctly the
	**	SAVED DATA POINTER.
	*/

	struct pm_ctx pm0;
	struct pm_ctx pm1;
};


/*========================================================================
**
**      Declaration of structs:     Command control block.
**
**========================================================================
*/
struct ccb {
	/*----------------------------------------------------------------
	**	This is the data structure which is pointed by the DSA 
	**	register when it is executed by the script processor.
	**	It must be the first entry.
	**----------------------------------------------------------------
	*/
	struct dsb	phys;

	/*----------------------------------------------------------------
	**	The general SCSI driver provides a
	**	pointer to a control block.
	**----------------------------------------------------------------
	*/
	Scsi_Cmnd	*cmd;		/* SCSI command 		*/
	u_char		cdb_buf[16];	/* Copy of CDB			*/
	u_char		sense_buf[64];
	int		data_len;	/* Total data length		*/
	int		segments;	/* Number of SG segments	*/

	/*----------------------------------------------------------------
	**	Message areas.
	**	We prepare a message to be sent after selection.
	**	We may use a second one if the command is rescheduled 
	**	due to CHECK_CONDITION or QUEUE FULL status.
	**      Contents are IDENTIFY and SIMPLE_TAG.
	**	While negotiating sync or wide transfer,
	**	a SDTR or WDTR message is appended.
	**----------------------------------------------------------------
	*/
	u_char		scsi_smsg [12];
	u_char		scsi_smsg2[12];

	/*----------------------------------------------------------------
	**	Miscellaneous status'.
	**----------------------------------------------------------------
	*/
	u_char		nego_status;	/* Negotiation status		*/
	u_char		xerr_status;	/* Extended error flags		*/
	u_int32		extra_bytes;	/* Extraneous bytes transferred	*/

	/*----------------------------------------------------------------
	**	Saved info for auto-sense
	**----------------------------------------------------------------
	*/
	u_char		sv_scsi_status;
	u_char		sv_xerr_status;

	/*----------------------------------------------------------------
	**	Other fields.
	**----------------------------------------------------------------
	*/
	u_long		p_ccb;		/* BUS address of this CCB	*/
	u_char		sensecmd[6];	/* Sense command		*/
	u_char		to_abort;	/* This CCB is to be aborted	*/
	u_short		tag;		/* Tag for this transfer	*/
					/*  NO_TAG means no tag		*/
	u_char		tags_si;	/* Lun tags sum index (0,1)	*/

	u_char		target;
	u_char		lun;
	u_short		queued;
	ccb_p		link_ccb;	/* Host adapter CCB chain	*/
	ccb_p		link_ccbh;	/* Host adapter CCB hash chain	*/
	XPT_QUEHEAD	link_ccbq;	/* Link to unit CCB queue	*/
	u_int32		startp;		/* Initial data pointer		*/
	u_int32		lastp0;		/* Initial 'lastp'		*/
	int		ext_sg;		/* Extreme data pointer, used	*/
	int		ext_ofs;	/*  to calculate the residual.	*/
	int		resid;
};

#define CCB_PHYS(cp,lbl)	(cp->p_ccb + offsetof(struct ccb, lbl))


/*========================================================================
**
**      Declaration of structs:     NCR device descriptor
**
**========================================================================
*/
struct ncb {
	/*----------------------------------------------------------------
	**	Idle task and invalid task actions and their bus
	**	addresses.
	**----------------------------------------------------------------
	*/
	struct action	idletask;
	struct action	notask;
	struct action	bad_i_t_l;
	struct action	bad_i_t_l_q;
	u_long		p_idletask;
	u_long		p_notask;
	u_long		p_bad_i_t_l;
	u_long		p_bad_i_t_l_q;

	/*----------------------------------------------------------------
	**	Dummy lun table to protect us against target returning bad  
	**	lun number on reselection.
	**----------------------------------------------------------------
	*/
	u_int32		*badluntbl;	/* Table physical address	*/
	u_int32		resel_badlun;	/* SCRIPT handler BUS address	*/

	/*----------------------------------------------------------------
	**	Bit 32-63 of the on-chip RAM bus address in LE format.
	**	The START_RAM64 script loads the MMRS and MMWS from this 
	**	field.
	**----------------------------------------------------------------
	*/
	u_int32		scr_ram_seg;

	/*----------------------------------------------------------------
	**	CCBs management queues.
	**----------------------------------------------------------------
	*/
	Scsi_Cmnd	*waiting_list;	/* Commands waiting for a CCB	*/
					/*  when lcb is not allocated.	*/
	Scsi_Cmnd	*done_list;	/* Commands waiting for done()  */
					/* callback to be invoked.      */ 
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,93)
	spinlock_t	smp_lock;	/* Lock for SMP threading       */
#endif

	/*----------------------------------------------------------------
	**	Chip and controller indentification.
	**----------------------------------------------------------------
	*/
	int		unit;		/* Unit number			*/
	char		chip_name[8];	/* Chip name			*/
	char		inst_name[16];	/* ncb instance name		*/

	/*----------------------------------------------------------------
	**	Initial value of some IO register bits.
	**	These values are assumed to have been set by BIOS, and may 
	**	be used for probing adapter implementation differences.
	**----------------------------------------------------------------
	*/
	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest3, sv_ctest4,
		sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4, sv_stest1, sv_scntl4;

	/*----------------------------------------------------------------
	**	Actual initial value of IO register bits used by the 
	**	driver. They are loaded at initialisation according to  
	**	features that are to be enabled.
	**----------------------------------------------------------------
	*/
	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest3, rv_ctest4, 
		rv_ctest5, rv_stest2, rv_ccntl0, rv_ccntl1, rv_scntl4;

	/*----------------------------------------------------------------
	**	Target data.
	**	Target control block bus address array used by the SCRIPT 
	**	on reselection.
	**----------------------------------------------------------------
	*/
	struct tcb	target[MAX_TARGET];
	u_int32		*targtbl;

	/*----------------------------------------------------------------
	**	Virtual and physical bus addresses of the chip.
	**----------------------------------------------------------------
	*/
#ifndef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
	u_long		base_va;	/* MMIO base virtual address	*/
	u_long		base2_va;	/* On-chip RAM virtual address	*/
#endif
	u_long		base_ba;	/* MMIO base bus address	*/
	u_long		base_io;	/* IO space base address	*/
	u_long		base_ws;	/* (MM)IO window size		*/
	u_long		base2_ba;	/* On-chip RAM bus address	*/
	u_long		base2_ws;	/* On-chip RAM window size	*/
	u_int		irq;		/* IRQ number			*/
	volatile			/* Pointer to volatile for 	*/
	struct ncr_reg	*reg;		/*  memory mapped IO.		*/

	/*----------------------------------------------------------------
	**	SCRIPTS virtual and physical bus addresses.
	**	'script'  is loaded in the on-chip RAM if present.
	**	'scripth' stays in main memory for all chips except the 
	**	53C895A and 53C896 that provide 8K on-chip RAM.
	**----------------------------------------------------------------
	*/
	struct script	*script0;	/* Copies of script and scripth	*/
	struct scripth	*scripth0;	/*  relocated for this ncb.	*/
	u_long		p_script;	/* Actual script and scripth	*/
	u_long		p_scripth;	/*  bus addresses.		*/
	u_long		p_scripth0;

	/*----------------------------------------------------------------
	**	General controller parameters and configuration.
	**----------------------------------------------------------------
	*/
	pcidev_t	pdev;
	u_short		device_id;	/* PCI device id		*/
	u_char		revision_id;	/* PCI device revision id	*/
	u_char		bus;		/* PCI BUS number		*/
	u_char		device_fn;	/* PCI BUS device and function	*/
	u_char		myaddr;		/* SCSI id of the adapter	*/
	u_char		maxburst;	/* log base 2 of dwords burst	*/
	u_char		maxwide;	/* Maximum transfer width	*/
	u_char		minsync;	/* Minimum sync period factor	*/
	u_char		maxsync;	/* Maximum sync period factor	*/
	u_char		maxoffs;	/* Max scsi offset		*/
	u_char		maxoffs_st;	/* Max scsi offset in ST mode	*/
	u_char		multiplier;	/* Clock multiplier (1,2,4)	*/
	u_char		clock_divn;	/* Number of clock divisors	*/
	u_long		clock_khz;	/* SCSI clock frequency in KHz	*/
	u_int		features;	/* Chip features map		*/

	/*----------------------------------------------------------------
	**	Range for the PCI clock frequency measurement result
	**	that ensures the algorithm used by the driver can be 
	**	trusted for the SCSI clock frequency measurement.
	**	(Assuming a PCI clock frequency of 33 MHz).
	**----------------------------------------------------------------
	*/
	u_int		pciclock_min;
	u_int		pciclock_max;

	/*----------------------------------------------------------------
	**	Start queue management.
	**	It is filled up by the host processor and accessed by the 
	**	SCRIPTS processor in order to start SCSI commands.
	**----------------------------------------------------------------
	*/
	u_long		p_squeue;	/* Start queue BUS address	*/
	u_int32		*squeue;	/* Start queue virtual address	*/
	u_short		squeueput;	/* Next free slot of the queue	*/
	u_short		actccbs;	/* Number of allocated CCBs	*/
	u_short		queuedepth;	/* Start queue depth		*/

	/*----------------------------------------------------------------
	**	Command completion queue.
	**	It is the same size as the start queue to avoid overflow.
	**----------------------------------------------------------------
	*/
	u_short		dqueueget;	/* Next position to scan	*/
	u_int32		*dqueue;	/* Completion (done) queue	*/

	/*----------------------------------------------------------------
	**	Timeout handler.
	**----------------------------------------------------------------
	*/
	struct timer_list timer;	/* Timer handler link header	*/
	u_long		lasttime;
	u_long		settle_time;	/* Resetting the SCSI BUS	*/

	/*----------------------------------------------------------------
	**	Debugging and profiling.
	**----------------------------------------------------------------
	*/
	struct ncr_reg	regdump;	/* Register dump		*/
	u_long		regtime;	/* Time it has been done	*/

	/*----------------------------------------------------------------
	**	Miscellaneous buffers accessed by the scripts-processor.
	**	They shall be DWORD aligned, because they may be read or 
	**	written with a script command.
	**----------------------------------------------------------------
	*/
	u_char		msgout[12];	/* Buffer for MESSAGE OUT 	*/
	u_char		msgin [12];	/* Buffer for MESSAGE IN	*/
	u_int32		lastmsg;	/* Last SCSI message sent	*/
	u_char		scratch;	/* Scratch for SCSI receive	*/

	/*----------------------------------------------------------------
	**	Miscellaneous configuration and status parameters.
	**----------------------------------------------------------------
	*/
	u_char		scsi_mode;	/* Current SCSI BUS mode	*/
	u_char		order;		/* Tag order to use		*/
	u_char		verbose;	/* Verbosity for this controller*/
	u_int32		ncr_cache;	/* Used for cache test at init.	*/
	u_long		p_ncb;		/* BUS address of this NCB	*/

	/*----------------------------------------------------------------
	**	CCB lists and queue.
	**----------------------------------------------------------------
	*/
	ccb_p ccbh[CCB_HASH_SIZE];	/* CCB hashed by DSA value	*/
	struct ccb	*ccbc;		/* CCB chain			*/
	XPT_QUEHEAD	free_ccbq;	/* Queue of available CCBs	*/

	/*----------------------------------------------------------------
	**	IMMEDIATE ARBITRATION (IARB) control.
	**	We keep track in 'last_cp' of the last CCB that has been 
	**	queued to the SCRIPTS processor and clear 'last_cp' when 
	**	this CCB completes. If last_cp is not zero at the moment 
	**	we queue a new CCB, we set a flag in 'last_cp' that is 
	**	used by the SCRIPTS as a hint for setting IARB.
	**	We donnot set more than 'iarb_max' consecutive hints for 
	**	IARB in order to leave devices a chance to reselect.
	**	By the way, any non zero value of 'iarb_max' is unfair. :)
	**----------------------------------------------------------------
	*/
#ifdef SCSI_NCR_IARB_SUPPORT
	struct ccb	*last_cp;	/* Last queud CCB used for IARB	*/
	u_short		iarb_max;	/* Max. # consecutive IARB hints*/
	u_short		iarb_count;	/* Actual # of these hints	*/
#endif

	/*----------------------------------------------------------------
	**	We need the LCB in order to handle disconnections and 
	**	to count active CCBs for task management. So, we use 
	**	a unique CCB for LUNs we donnot have the LCB yet.
	**	This queue normally should have at most 1 element.
	**----------------------------------------------------------------
	*/
	XPT_QUEHEAD	b0_ccbq;

	/*----------------------------------------------------------------
	**	We use a different scatter function for 896 rev 1.
	**----------------------------------------------------------------
	*/
	int (*scatter) (ncb_p, ccb_p, Scsi_Cmnd *);

	/*----------------------------------------------------------------
	**	Command abort handling.
	**	We need to synchronize tightly with the SCRIPTS 
	**	processor in order to handle things correctly.
	**----------------------------------------------------------------
	*/
	u_char		abrt_msg[4];	/* Message to send buffer	*/
	struct scr_tblmove abrt_tbl;	/* Table for the MOV of it 	*/
	struct scr_tblsel  abrt_sel;	/* Sync params for selection	*/
	u_char		istat_sem;	/* Tells the chip to stop (SEM)	*/

	/*----------------------------------------------------------------
	**	Fields that should be removed or changed.
	**----------------------------------------------------------------
	*/
	struct usrcmd	user;		/* Command from user		*/
	volatile u_char	release_stage;	/* Synchronisation stage on release  */

	/*----------------------------------------------------------------
	**	Fields that are used (primarily) for integrity check
	**----------------------------------------------------------------
	*/
	unsigned char  check_integrity; /* Enable midlayer integ. check on
					 * bus scan. */
#ifdef	SCSI_NCR_INTEGRITY_CHECKING
	unsigned char check_integ_par;	/* Set if par or Init. Det. error
					 * used only during integ check */
#endif
};

#define NCB_PHYS(np, lbl)	 (np->p_ncb + offsetof(struct ncb, lbl))
#define NCB_SCRIPT_PHYS(np,lbl)	 (np->p_script  + offsetof (struct script, lbl))
#define NCB_SCRIPTH_PHYS(np,lbl) (np->p_scripth + offsetof (struct scripth,lbl))
#define NCB_SCRIPTH0_PHYS(np,lbl) (np->p_scripth0+offsetof (struct scripth,lbl))

/*==========================================================
**
**
**      Script for NCR-Processor.
**
**	Use ncr_script_fill() to create the variable parts.
**	Use ncr_script_copy_and_bind() to make a copy and
**	bind to physical addresses.
**
**
**==========================================================
**
**	We have to know the offsets of all labels before
**	we reach them (for forward jumps).
**	Therefore we declare a struct here.
**	If you make changes inside the script,
**	DONT FORGET TO CHANGE THE LENGTHS HERE!
**
**----------------------------------------------------------
*/

/*
**	Script fragments which are loaded into the on-chip RAM 
**	of 825A, 875, 876, 895, 895A and 896 chips.
*/
struct script {
	ncrcmd	start		[ 14];
	ncrcmd	getjob_begin	[  4];
	ncrcmd	getjob_end	[  4];
	ncrcmd	select		[  8];
	ncrcmd	wf_sel_done	[  2];
	ncrcmd	send_ident	[  2];
#ifdef SCSI_NCR_IARB_SUPPORT
	ncrcmd	select2		[  8];
#else
	ncrcmd	select2		[  2];
#endif
	ncrcmd  command		[  2];
	ncrcmd  dispatch	[ 28];
	ncrcmd  sel_no_cmd	[ 10];
	ncrcmd  init		[  6];
	ncrcmd  clrack		[  4];
	ncrcmd  disp_status	[  4];
	ncrcmd  datai_done	[ 26];
	ncrcmd  datao_done	[ 12];
	ncrcmd  ign_i_w_r_msg	[  4];
	ncrcmd  datai_phase	[  2];
	ncrcmd  datao_phase	[  4];
	ncrcmd  msg_in		[  2];
	ncrcmd  msg_in2		[ 10];
#ifdef SCSI_NCR_IARB_SUPPORT
	ncrcmd  status		[ 14];
#else
	ncrcmd  status		[ 10];
#endif
	ncrcmd  complete	[  8];
#ifdef SCSI_NCR_PCIQ_MAY_REORDER_WRITES
	ncrcmd  complete2	[ 12];
#else
	ncrcmd  complete2	[ 10];
#endif
#ifdef SCSI_NCR_PCIQ_SYNC_ON_INTR
	ncrcmd	done		[ 18];
#else
	ncrcmd	done		[ 14];
#endif
	ncrcmd	done_end	[  2];
	ncrcmd  save_dp		[  8];
	ncrcmd  restore_dp	[  4];
	ncrcmd  disconnect	[ 20];
#ifdef SCSI_NCR_IARB_SUPPORT
	ncrcmd  idle		[  4];
#else
	ncrcmd  idle		[  2];
#endif
#ifdef SCSI_NCR_IARB_SUPPORT
	ncrcmd  ungetjob	[  6];
#else
	ncrcmd  ungetjob	[  4];
#endif
	ncrcmd	reselect	[  4];
	ncrcmd	reselected	[ 20];
	ncrcmd	resel_scntl4	[ 30];
#if   MAX_TASKS*4 > 512
	ncrcmd	resel_tag	[ 18];
#elif MAX_TASKS*4 > 256
	ncrcmd	resel_tag	[ 12];
#else
	ncrcmd	resel_tag	[  8];
#endif
	ncrcmd	resel_go	[  6];
	ncrcmd	resel_notag	[  2];
	ncrcmd	resel_dsa	[  8];
	ncrcmd  data_in		[MAX_SCATTER * SCR_SG_SIZE];
	ncrcmd  data_in2	[  4];
	ncrcmd  data_out	[MAX_SCATTER * SCR_SG_SIZE];
	ncrcmd  data_out2	[  4];
	ncrcmd  pm0_data	[ 12];
	ncrcmd  pm0_data_out	[  6];
	ncrcmd  pm0_data_end	[  6];
	ncrcmd  pm1_data	[ 12];
	ncrcmd  pm1_data_out	[  6];
	ncrcmd  pm1_data_end	[  6];
};

/*
**	Script fragments which stay in main memory for all chips 
**	except for the 895A and 896 that support 8K on-chip RAM.
*/
struct scripth {
	ncrcmd	start64		[  2];
	ncrcmd	no_data		[  2];
	ncrcmd	sel_for_abort	[ 18];
	ncrcmd	sel_for_abort_1	[  2];
	ncrcmd	select_no_atn	[  8];
	ncrcmd	wf_sel_done_no_atn [ 4];

	ncrcmd	msg_in_etc	[ 14];
	ncrcmd	msg_received	[  4];
	ncrcmd	msg_weird_seen	[  4];
	ncrcmd	msg_extended	[ 20];
	ncrcmd  msg_bad		[  6];
	ncrcmd	msg_weird	[  4];
	ncrcmd	msg_weird1	[  8];

	ncrcmd	wdtr_resp	[  6];
	ncrcmd	send_wdtr	[  4];
	ncrcmd	sdtr_resp	[  6];
	ncrcmd	send_sdtr	[  4];
	ncrcmd	ppr_resp	[  6];
	ncrcmd	send_ppr	[  4];
	ncrcmd	nego_bad_phase	[  4];
	ncrcmd	msg_out		[  4];
	ncrcmd	msg_out_done	[  4];
	ncrcmd	data_ovrun	[  2];
	ncrcmd	data_ovrun1	[ 22];
	ncrcmd	data_ovrun2	[  8];
	ncrcmd	abort_resel	[ 16];
	ncrcmd	resend_ident	[  4];
	ncrcmd	ident_break	[  4];
	ncrcmd	ident_break_atn	[  4];
	ncrcmd	sdata_in	[  6];
	ncrcmd  data_io		[  2];
	ncrcmd  data_io_com	[  8];
	ncrcmd  data_io_out	[ 12];
	ncrcmd	resel_bad_lun	[  4];
	ncrcmd	bad_i_t_l	[  4];
	ncrcmd	bad_i_t_l_q	[  4];
	ncrcmd	bad_status	[  6];
	ncrcmd	tweak_pmj	[ 12];
	ncrcmd	pm_handle	[ 20];
	ncrcmd	pm_handle1	[  4];
	ncrcmd	pm_save		[  4];
	ncrcmd	pm0_save	[ 14];
	ncrcmd	pm1_save	[ 14];

	/* WSR handling */
#ifdef SYM_DEBUG_PM_WITH_WSR
	ncrcmd  pm_wsr_handle	[ 44];
#else
	ncrcmd  pm_wsr_handle	[ 42];
#endif
	ncrcmd  wsr_ma_helper	[  4];

	/* Data area */
	ncrcmd	zero		[  1];
	ncrcmd	scratch		[  1];
	ncrcmd	scratch1	[  1];
	ncrcmd	pm0_data_addr	[  1];
	ncrcmd	pm1_data_addr	[  1];
	ncrcmd	saved_dsa	[  1];
	ncrcmd	saved_drs	[  1];
	ncrcmd	done_pos	[  1];
	ncrcmd	startpos	[  1];
	ncrcmd	targtbl		[  1];
	/* End of data area */

#ifdef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
	ncrcmd	start_ram	[  1];
	ncrcmd	script0_ba	[  4];
	ncrcmd	start_ram64	[  3];
	ncrcmd	script0_ba64	[  3];
	ncrcmd	scripth0_ba64	[  6];
	ncrcmd	ram_seg64	[  1];
#endif
	ncrcmd	snooptest	[  6];
	ncrcmd	snoopend	[  2];
};

/*==========================================================
**
**
**      Function headers.
**
**
**==========================================================
*/

static	ccb_p	ncr_alloc_ccb	(ncb_p np);
static	void	ncr_complete	(ncb_p np, ccb_p cp);
static	void	ncr_exception	(ncb_p np);
static	void	ncr_free_ccb	(ncb_p np, ccb_p cp);
static	ccb_p	ncr_ccb_from_dsa(ncb_p np, u_long dsa);
static	void	ncr_init_tcb	(ncb_p np, u_char tn);
static	lcb_p	ncr_alloc_lcb	(ncb_p np, u_char tn, u_char ln);
static	lcb_p	ncr_setup_lcb	(ncb_p np, u_char tn, u_char ln,
				 u_char *inq_data);
static	void	ncr_getclock	(ncb_p np, int mult);
static	u_int	ncr_getpciclock (ncb_p np);
static	void	ncr_selectclock	(ncb_p np, u_char scntl3);
static	ccb_p	ncr_get_ccb	(ncb_p np, u_char tn, u_char ln);
static	void	ncr_init	(ncb_p np, int reset, char * msg, u_long code);
static	void	ncr_int_sbmc	(ncb_p np);
static	void	ncr_int_par	(ncb_p np, u_short sist);
static	void	ncr_int_ma	(ncb_p np);
static	void	ncr_int_sir	(ncb_p np);
static  void    ncr_int_sto     (ncb_p np);
static  void    ncr_int_udc     (ncb_p np);
static	void	ncr_negotiate	(ncb_p np, tcb_p tp);
static	int	ncr_prepare_nego(ncb_p np, ccb_p cp, u_char *msgptr);
#ifdef	SCSI_NCR_INTEGRITY_CHECKING
static	int	ncr_ic_nego(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd, u_char *msgptr);
#endif
static	void	ncr_script_copy_and_bind
				(ncb_p np, ncrcmd *src, ncrcmd *dst, int len);
static  void    ncr_script_fill (struct script * scr, struct scripth * scripth);
static	int	ncr_scatter_896R1 (ncb_p np, ccb_p cp, Scsi_Cmnd *cmd);
static	int	ncr_scatter	(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd);
static	void	ncr_getsync	(ncb_p np, u_char sfac, u_char *fakp, u_char *scntl3p);
static  void    ncr_get_xfer_info(ncb_p np, tcb_p tp, u_char *factor, u_char *offset, u_char *width);
static	void	ncr_setsync	(ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer, u_char scntl4);
static void 	ncr_set_sync_wide_status (ncb_p np, u_char target);
static	void	ncr_setup_tags	(ncb_p np, u_char tn, u_char ln);
static	void	ncr_setwide	(ncb_p np, ccb_p cp, u_char wide, u_char ack);
static	void	ncr_setsyncwide	(ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer, u_char scntl4, u_char wide);
static	int	ncr_show_msg	(u_char * msg);
static	void	ncr_print_msg	(ccb_p cp, char *label, u_char * msg);
static	int	ncr_snooptest	(ncb_p np);
static	void	ncr_timeout	(ncb_p np);
static  void    ncr_wakeup      (ncb_p np, u_long code);
static  int     ncr_wakeup_done (ncb_p np);
static	void	ncr_start_next_ccb (ncb_p np, lcb_p lp, int maxn);
static	void	ncr_put_start_queue(ncb_p np, ccb_p cp);
static	void	ncr_chip_reset	(ncb_p np);
static	void	ncr_soft_reset	(ncb_p np);
static	void	ncr_start_reset	(ncb_p np);
static	int	ncr_reset_scsi_bus (ncb_p np, int enab_int, int settle_delay);
static	int	ncr_compute_residual (ncb_p np, ccb_p cp);

#ifdef SCSI_NCR_USER_COMMAND_SUPPORT
static	void	ncr_usercmd	(ncb_p np);
#endif

static int ncr_attach (Scsi_Host_Template *tpnt, int unit, ncr_device *device);
static void ncr_free_resources(ncb_p np);

static void insert_into_waiting_list(ncb_p np, Scsi_Cmnd *cmd);
static Scsi_Cmnd *retrieve_from_waiting_list(int to_remove, ncb_p np, Scsi_Cmnd *cmd);
static void process_waiting_list(ncb_p np, int sts);

#define remove_from_waiting_list(np, cmd) \
		retrieve_from_waiting_list(1, (np), (cmd))
#define requeue_waiting_list(np) process_waiting_list((np), DID_OK)
#define reset_waiting_list(np) process_waiting_list((np), DID_RESET)

#ifdef SCSI_NCR_NVRAM_SUPPORT
static  void	ncr_get_nvram	       (ncr_device *devp, ncr_nvram *nvp);
static  int	sym_read_Tekram_nvram  (ncr_slot *np, u_short device_id,
				        Tekram_nvram *nvram);
static  int	sym_read_Symbios_nvram (ncr_slot *np, Symbios_nvram *nvram);
#endif

/*==========================================================
**
**
**      Global static data.
**
**
**==========================================================
*/

static inline char *ncr_name (ncb_p np)
{
	return np->inst_name;
}


/*==========================================================
**
**
**      Scripts for NCR-Processor.
**
**      Use ncr_script_bind for binding to physical addresses.
**
**
**==========================================================
**
**	NADDR generates a reference to a field of the controller data.
**	PADDR generates a reference to another part of the script.
**	RADDR generates a reference to a script processor register.
**	FADDR generates a reference to a script processor register
**		with offset.
**
**----------------------------------------------------------
*/

#define	RELOC_SOFTC	0x40000000
#define	RELOC_LABEL	0x50000000
#define	RELOC_REGISTER	0x60000000
#if 0
#define	RELOC_KVAR	0x70000000
#endif
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncb, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct scripth, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#define	KVAR(which)	(RELOC_KVAR | (which))

#define SCR_DATA_ZERO	0xf00ff00f

#ifdef	RELOC_KVAR
#define	SCRIPT_KVAR_JIFFIES	(0)
#define	SCRIPT_KVAR_FIRST	SCRIPT_KVAR_JIFFIES
#define	SCRIPT_KVAR_LAST	SCRIPT_KVAR_JIFFIES
/*
 * Kernel variables referenced in the scripts.
 * THESE MUST ALL BE ALIGNED TO A 4-BYTE BOUNDARY.
 */
static void *script_kvars[] __initdata =
	{ (void *)&jiffies };
#endif

static	struct script script0 __initdata = {
/*--------------------------< START >-----------------------*/ {
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**      Clear SIGP.
	*/
	SCR_FROM_REG (ctest2),
		0,

	/*
	**	Stop here if the C code wants to perform 
	**	some error recovery procedure manually.
	**	(Indicate this by setting SEM in ISTAT)
	*/
	SCR_FROM_REG (istat),
		0,
	/*
	**	Report to the C code the next position in 
	**	the start queue the SCRIPTS will schedule.
	**	The C code must not change SCRATCHA.
	*/
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (startpos),
	SCR_INT ^ IFTRUE (MASK (SEM, SEM)),
		SIR_SCRIPT_STOPPED,

	/*
	**	Start the next job.
	**
	**	@DSA	 = start point for this job.
	**	SCRATCHA = address of this job in the start queue.
	**
	**	We will restore startpos with SCRATCHA if we fails the 
	**	arbitration or if it is the idle job.
	**
	**	The below GETJOB_BEGIN to GETJOB_END section of SCRIPTS 
	**	is a critical path. If it is partially executed, it then 
	**	may happen that the job address is not yet in the DSA 
	**	and the next queue position points to the next JOB.
	*/
	SCR_LOAD_ABS (dsa, 4),
		PADDRH (startpos),
	SCR_LOAD_REL (temp, 4),
		4,
}/*-------------------------< GETJOB_BEGIN >------------------*/,{
	SCR_STORE_ABS (temp, 4),
		PADDRH (startpos),
	SCR_LOAD_REL (dsa, 4),
		0,
}/*-------------------------< GETJOB_END >--------------------*/,{
	SCR_LOAD_REL (temp, 4),
		0,
	SCR_RETURN,
		0,

}/*-------------------------< SELECT >----------------------*/,{
	/*
	**	DSA	contains the address of a scheduled
	**		data structure.
	**
	**	SCRATCHA contains the address of the start queue  
	**		entry which points to the next job.
	**
	**	Set Initiator mode.
	**
	**	(Target mode is left as an exercise for the reader)
	*/

	SCR_CLR (SCR_TRG),
		0,
	/*
	**      And try to select this target.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (ungetjob),
	/*
	**	Now there are 4 possibilities:
	**
	**	(1) The ncr looses arbitration.
	**	This is ok, because it will try again,
	**	when the bus becomes idle.
	**	(But beware of the timeout function!)
	**
	**	(2) The ncr is reselected.
	**	Then the script processor takes the jump
	**	to the RESELECT label.
	**
	**	(3) The ncr wins arbitration.
	**	Then it will execute SCRIPTS instruction until 
	**	the next instruction that checks SCSI phase.
	**	Then will stop and wait for selection to be 
	**	complete or selection time-out to occur.
	**
	**	After having won arbitration, the ncr SCRIPTS  
	**	processor is able to execute instructions while 
	**	the SCSI core is performing SCSI selection. But 
	**	some script instruction that is not waiting for 
	**	a valid phase (or selection timeout) to occur 
	**	breaks the selection procedure, by probably 
	**	affecting timing requirements.
	**	So we have to wait immediately for the next phase 
	**	or the selection to complete or time-out.
	*/

	/*
	**      load the savep (saved pointer) into
	**      the actual data pointer.
	*/
	SCR_LOAD_REL (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	/*
	**      Initialize the status registers
	*/
	SCR_LOAD_REL (scr0, 4),
		offsetof (struct ccb, phys.header.status),

}/*-------------------------< WF_SEL_DONE >----------------------*/,{
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_SEL_ATN_NO_MSG_OUT,
}/*-------------------------< SEND_IDENT >----------------------*/,{
	/*
	**	Selection complete.
	**	Send the IDENTIFY and SIMPLE_TAG messages
	**	(and the M_X_SYNC_REQ / M_X_WIDE_REQ message)
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
}/*-------------------------< SELECT2 >----------------------*/,{
#ifdef SCSI_NCR_IARB_SUPPORT
	/*
	**	Set IMMEDIATE ARBITRATION if we have been given 
	**	a hint to do so. (Some job to do after this one).
	*/
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (HF_HINT_IARB, HF_HINT_IARB)),
		8,
	SCR_REG_REG (scntl1, SCR_OR, IARB),
		0,
#endif
	/*
	**	Anticipate the COMMAND phase.
	**	This is the PHASE we expect at this point.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_COMMAND)),
		PADDR (sel_no_cmd),

}/*-------------------------< COMMAND >--------------------*/,{
	/*
	**	... and send the command
	*/
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),

}/*-----------------------< DISPATCH >----------------------*/,{
	/*
	**	MSG_IN is the only phase that shall be 
	**	entered at least once for each (re)selection.
	**	So we test it first.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (IF (SCR_DATA_OUT)),
		PADDR (datao_phase),
	SCR_JUMP ^ IFTRUE (IF (SCR_DATA_IN)),
		PADDR (datai_phase),
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDRH (msg_out),
	/*
	 *  Discard as many illegal phases as 
	 *  required and tell the C code about.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_ILG_OUT)),
		16,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_ILG_OUT)),
		-16,
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_ILG_IN)),
		16,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		NADDR (scratch),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_ILG_IN)),
		-16,
	SCR_INT,
		SIR_BAD_PHASE,
	SCR_JUMP,
		PADDR (dispatch),
}/*---------------------< SEL_NO_CMD >----------------------*/,{
	/*
	**	The target does not switch to command 
	**	phase after IDENTIFY has been sent.
	**
	**	If it stays in MSG OUT phase send it 
	**	the IDENTIFY again.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (resend_ident),
	/*
	**	If target does not switch to MSG IN phase 
	**	and we sent a negotiation, assert the 
	**	failure immediately.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	/*
	**	Jump to dispatcher.
	*/
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< INIT >------------------------*/,{
	/*
	**	Wait for the SCSI RESET signal to be 
	**	inactive before restarting operations, 
	**	since the chip may hang on SEL_ATN 
	**	if SCSI RESET is active.
	*/
	SCR_FROM_REG (sstat0),
		0,
	SCR_JUMPR ^ IFTRUE (MASK (IRST, IRST)),
		-16,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< CLRACK >----------------------*/,{
	/*
	**	Terminate possible pending message phase.
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< DISP_STATUS >----------------------*/,{
	/*
	**	Anticipate STATUS phase.
	**
	**	Does spare 3 SCRIPTS instructions when we have 
	**	completed the INPUT of the data.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< DATAI_DONE >-------------------*/,{
	/*
	 *  If the device wants us to send more data,
	 *  we must count the extra bytes.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_DATA_IN)),
		PADDRH (data_ovrun),
	/*
	**	If the SWIDE is not full, jump to dispatcher.
	**	We anticipate a STATUS phase.
	**	If we get later an IGNORE WIDE RESIDUE, we 
	**	will alias it as a MODIFY DP (-1).
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMP ^ IFFALSE (MASK (WSR, WSR)),
		PADDR (disp_status),
	/*
	**	The SWIDE is full.
	**	Clear this condition.
	*/
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	/*
         *	We are expecting an IGNORE RESIDUE message
         *	from the device, otherwise we are in data
         *	overrun condition. Check against MSG_IN phase.
	*/
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_SWIDE_OVERRUN,	
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (disp_status),
	/*
	 *	We are in MSG_IN phase,
	 *	Read the first byte of the message.
	 *	If it is not an IGNORE RESIDUE message,
	 *	signal overrun and jump to message
	 *	processing.
	 */
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
	SCR_INT ^ IFFALSE (DATA (M_IGN_RESIDUE)),
		SIR_SWIDE_OVERRUN,	
	SCR_JUMP ^ IFFALSE (DATA (M_IGN_RESIDUE)),
		PADDR (msg_in2),

	/*
	 *	We got the message we expected.
	 *	Read the 2nd byte, and jump to dispatcher.
	 */
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (disp_status),

}/*-------------------------< DATAO_DONE >-------------------*/,{
	/*
	 *  If the device wants us to send more data,
	 *  we must count the extra bytes.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		PADDRH (data_ovrun),
	/*
	**	If the SODL is not full jump to dispatcher.
	**	We anticipate a MSG IN phase or a STATUS phase.
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMP ^ IFFALSE (MASK (WSS, WSS)),
		PADDR (disp_status),
	/*
	**	The SODL is full, clear this condition.
	*/
	SCR_REG_REG (scntl2, SCR_OR, WSS),
		0,
	/*
	**	And signal a DATA UNDERRUN condition 
	**	to the C code.
	*/
	SCR_INT,
		SIR_SODL_UNDERRUN,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< IGN_I_W_R_MSG >--------------*/,{
	/*
	**	We jump here from the phase mismatch interrupt, 
	**	When we have a SWIDE and the device has presented 
	**	a IGNORE WIDE RESIDUE message on the BUS.
	**	We just have to throw away this message and then 
	**	to jump to dispatcher.
	*/
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (scratch),
	/*
	**	Clear ACK and jump to dispatcher.
	*/
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< DATAI_PHASE >------------------*/,{
	SCR_RETURN,
		0,
}/*-------------------------< DATAO_PHASE >------------------*/,{
	/*
	**	Patch for 53c1010_66 only - to allow A0 part
	**	to operate properly in a 33MHz PCI bus.
	**
	** 	SCR_REG_REG(scntl4, SCR_OR, 0x0c),
	**		0,
	*/
	SCR_NO_OP,
		0,
	SCR_RETURN,
		0,
}/*-------------------------< MSG_IN >--------------------*/,{
	/*
	**	Get the first byte of the message.
	**
	**	The script processor doesn't negate the
	**	ACK signal after this transfer.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
}/*-------------------------< MSG_IN2 >--------------------*/,{
	/*
	**	Check first against 1 byte messages 
	**	that we handle from SCRIPTS.
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (M_DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (M_SAVE_DP)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_RESTORE_DP)),
		PADDR (restore_dp),
	/*
	**	We handle all other messages from the 
	**	C code, so no need to waste on-chip RAM 
	**	for those ones.
	*/
	SCR_JUMP,
		PADDRH (msg_in_etc),

}/*-------------------------< STATUS >--------------------*/,{
	/*
	**	get the status
	*/
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
#ifdef SCSI_NCR_IARB_SUPPORT
	/*
	**	If STATUS is not GOOD, clear IMMEDIATE ARBITRATION, 
	**	since we may have to tamper the start queue from 
	**	the C code.
	*/
	SCR_JUMPR ^ IFTRUE (DATA (S_GOOD)),
		8,
	SCR_REG_REG (scntl1, SCR_AND, ~IARB),
		0,
#endif
	/*
	**	save status to scsi_status.
	**	mark as complete.
	*/
	SCR_TO_REG (SS_REG),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	/*
	**	Anticipate the MESSAGE PHASE for 
	**	the TASK COMPLETE message.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
	**	Copy the data pointer to LASTP in header.
	*/
	SCR_STORE_REL (temp, 4),
		offsetof (struct ccb, phys.header.lastp),
	/*
	**	When we terminate the cycle by clearing ACK,
	**	the target may disconnect immediately.
	**
	**	We don't want to be told of an
	**	"unexpected disconnect",
	**	so we disable this feature.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	/*
	**	Terminate cycle ...
	*/
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	... and wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
}/*-------------------------< COMPLETE2 >-----------------*/,{
	/*
	**	Save host status to header.
	*/
	SCR_STORE_REL (scr0, 4),
		offsetof (struct ccb, phys.header.status),

#ifdef SCSI_NCR_PCIQ_MAY_REORDER_WRITES
	/*
	**	Some bridges may reorder DMA writes to memory.
	**	We donnot want the CPU to deal with completions  
	**	without all the posted write having been flushed 
	**	to memory. This DUMMY READ should flush posted 
	**	buffers prior to the CPU having to deal with 
	**	completions.
	*/
	SCR_LOAD_REL (scr0, 4),	/* DUMMY READ */
		offsetof (struct ccb, phys.header.status),
#endif
	/*
	**	If command resulted in not GOOD status,
	**	call the C code if needed.
	*/
	SCR_FROM_REG (SS_REG),
		0,
	SCR_CALL ^ IFFALSE (DATA (S_GOOD)),
		PADDRH (bad_status),

	/*
	**	If we performed an auto-sense, call 
	**	the C code to synchronyze task aborts 
	**	with UNIT ATTENTION conditions.
	*/
	SCR_FROM_REG (HF_REG),
		0,
	SCR_INT ^ IFTRUE (MASK (HF_AUTO_SENSE, HF_AUTO_SENSE)),
		SIR_AUTO_SENSE_DONE,

}/*------------------------< DONE >-----------------*/,{
#ifdef SCSI_NCR_PCIQ_SYNC_ON_INTR
	/*
	**	It seems that some bridges flush everything 
	**	when the INTR line is raised. For these ones, 
	**	we can just ensure that the INTR line will be 
	**	raised before each completion. So, if it happens 
	**	that we have been faster that the CPU, we just 
	**	have to synchronize with it. A dummy programmed 
	**	interrupt will do the trick.
	**	Note that we overlap at most 1 IO with the CPU 
	**	in this situation and that the IRQ line must not 
	**	be shared.
	*/
	SCR_FROM_REG (istat),
		0,
	SCR_INT ^ IFTRUE (MASK (INTF, INTF)),
		SIR_DUMMY_INTERRUPT,
#endif
	/*
	**	Copy the DSA to the DONE QUEUE and 
	**	signal completion to the host.
	**	If we are interrupted between DONE 
	**	and DONE_END, we must reset, otherwise 
	**	the completed CCB will be lost.
	*/
	SCR_STORE_ABS (dsa, 4),
		PADDRH (saved_dsa),
	SCR_LOAD_ABS (dsa, 4),
		PADDRH (done_pos),
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (saved_dsa),
	SCR_STORE_REL (scratcha, 4),
		0,
	/*
	**	The instruction below reads the DONE QUEUE next 
	**	free position from memory.
	**	In addition it ensures that all PCI posted writes  
	**	are flushed and so the DSA value of the done 
	**	CCB is visible by the CPU before INTFLY is raised.
	*/
	SCR_LOAD_REL (temp, 4),
		4,
	SCR_INT_FLY,
		0,
	SCR_STORE_ABS (temp, 4),
		PADDRH (done_pos),
}/*------------------------< DONE_END >-----------------*/,{
	SCR_JUMP,
		PADDR (start),

}/*-------------------------< SAVE_DP >------------------*/,{
	/*
	**	Clear ACK immediately.
	**	No need to delay it.
	*/
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	Keep track we received a SAVE DP, so 
	**	we will switch to the other PM context 
	**	on the next PM since the DP may point 
	**	to the current PM context.
	*/
	SCR_REG_REG (HF_REG, SCR_OR, HF_DP_SAVED),
		0,
	/*
	**	SAVE_DP message:
	**	Copy the data pointer to SAVEP in header.
	*/
	SCR_STORE_REL (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< RESTORE_DP >---------------*/,{
	/*
	**	RESTORE_DP message:
	**	Copy SAVEP in header to actual data pointer.
	*/
	SCR_LOAD_REL  (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< DISCONNECT >---------------*/,{
	/*
	**	DISCONNECTing  ...
	**
	**	disable the "unexpected disconnect" feature,
	**	and remove the ACK signal.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	Wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
	/*
	**	Status is: DISCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	/*
	**	Save host status to header.
	*/
	SCR_STORE_REL (scr0, 4),
		offsetof (struct ccb, phys.header.status),
	/*
	**	If QUIRK_AUTOSAVE is set,
	**	do an "save pointer" operation.
	*/
	SCR_FROM_REG (QU_REG),
		0,
	SCR_JUMP ^ IFFALSE (MASK (QUIRK_AUTOSAVE, QUIRK_AUTOSAVE)),
		PADDR (start),
	/*
	**	like SAVE_DP message:
	**	Remember we saved the data pointer.
	**	Copy data pointer to SAVEP in header.
	*/
	SCR_REG_REG (HF_REG, SCR_OR, HF_DP_SAVED),
		0,
	SCR_STORE_REL (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	SCR_JUMP,
		PADDR (start),

}/*-------------------------< IDLE >------------------------*/,{
	/*
	**	Nothing to do?
	**	Wait for reselect.
	**	This NOP will be patched with LED OFF
	**	SCR_REG_REG (gpreg, SCR_OR, 0x01)
	*/
	SCR_NO_OP,
		0,
#ifdef SCSI_NCR_IARB_SUPPORT
	SCR_JUMPR,
		8,
#endif
}/*-------------------------< UNGETJOB >-----------------*/,{
#ifdef SCSI_NCR_IARB_SUPPORT
	/*
	**	Set IMMEDIATE ARBITRATION, for the next time.
	**	This will give us better chance to win arbitration 
	**	for the job we just wanted to do.
	*/
	SCR_REG_REG (scntl1, SCR_OR, IARB),
		0,
#endif
	/*
	**	We are not able to restart the SCRIPTS if we are 
	**	interrupted and these instruction haven't been 
	**	all executed. BTW, this is very unlikely to 
	**	happen, but we check that from the C code.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_STORE_ABS (scratcha, 4),
		PADDRH (startpos),
}/*-------------------------< RESELECT >--------------------*/,{
	/*
	**	make the host status invalid.
	*/
	SCR_CLR (SCR_TRG),
		0,
	/*
	**	Sleep waiting for a reselection.
	**	If SIGP is set, special treatment.
	**
	**	Zu allem bereit ..
	*/
	SCR_WAIT_RESEL,
		PADDR(start),
}/*-------------------------< RESELECTED >------------------*/,{
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**      load the target id into the sdid
	*/
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	/*
	**	load the target control block address
	*/
	SCR_LOAD_ABS (dsa, 4),
		PADDRH (targtbl),
	SCR_SFBR_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_AND, 0x3c),
		0,
	SCR_LOAD_REL (dsa, 4),
		0,
	/*
	**	Load the synchronous transfer registers.
	*/
	SCR_LOAD_REL (scntl3, 1),
		offsetof(struct tcb, wval),
	SCR_LOAD_REL (sxfer, 1),
		offsetof(struct tcb, sval),
}/*-------------------------< RESEL_SCNTL4 >------------------*/,{
	/*
	**	Write with uval value. Patch if device
	**	does not support Ultra3.
	**	
	**	SCR_LOAD_REL (scntl4, 1),
	**		offsetof(struct tcb, uval),
	*/

	SCR_NO_OP,
		0,
        /*
         *  We expect MESSAGE IN phase.
         *  If not, get help from the C code.
         */
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_RESEL_NO_MSG_IN,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),

	/*
	 *  If IDENTIFY LUN #0, use a faster path 
	 *  to find the LCB structure.
	 */
	SCR_JUMPR ^ IFTRUE (MASK (0x80, 0xbf)),
		56,
	/*
	 *  If message isn't an IDENTIFY, 
	 *  tell the C code about.
	 */
	SCR_INT ^ IFFALSE (MASK (0x80, 0x80)),
		SIR_RESEL_NO_IDENTIFY,
	/*
	 *  It is an IDENTIFY message,
	 *  Load the LUN control block address.
	 */
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct tcb, b_luntbl),
	SCR_SFBR_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_AND, 0xfc),
		0,
	SCR_LOAD_REL (dsa, 4),
		0,
	SCR_JUMPR,
		8,
	/*
	**	LUN 0 special case (but usual one :))
	*/
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct tcb, b_lun0),

	/*
	**	Load the reselect task action for this LUN.
	**	Load the tasks DSA array for this LUN.
	**	Call the action.
	*/
	SCR_LOAD_REL (temp, 4),
		offsetof(struct lcb, resel_task),
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct lcb, b_tasktbl),
	SCR_RETURN,
		0,
}/*-------------------------< RESEL_TAG >-------------------*/,{
	/*
	**	ACK the IDENTIFY or TAG previously received
	*/

	SCR_CLR (SCR_ACK),
		0,
	/*
	**	Read IDENTIFY + SIMPLE + TAG using a single MOVE.
	**	Agressive optimization, is'nt it?
	**	No need to test the SIMPLE TAG message, since the 
	**	driver only supports conformant devices for tags. ;-)
	*/
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (msgin),
	/*
	**	Read the TAG from the SIDL.
	**	Still an aggressive optimization. ;-)
	**	Compute the CCB indirect jump address which 
	**	is (#TAG*2 & 0xfc) due to tag numbering using 
	**	1,3,5..MAXTAGS*2+1 actual values.
	*/
	SCR_REG_SFBR (sidl, SCR_SHL, 0),
		0,
#if MAX_TASKS*4 > 512
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 2),
		0,
	SCR_REG_REG (sfbr, SCR_SHL, 0),
		0,
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 1),
		0,
#elif MAX_TASKS*4 > 256
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 1),
		0,
#endif
	/*
	**	Retrieve the DSA of this task.
	**	JUMP indirectly to the restart point of the CCB.
	*/
	SCR_SFBR_REG (dsa, SCR_AND, 0xfc),
		0,
}/*-------------------------< RESEL_GO >-------------------*/,{
	SCR_LOAD_REL (dsa, 4),
		0,
	SCR_LOAD_REL (temp, 4),
		offsetof(struct ccb, phys.header.go.restart),
	SCR_RETURN,
		0,
	/* In normal situations we branch to RESEL_DSA */
}/*-------------------------< RESEL_NOTAG >-------------------*/,{
	/*
	**	JUMP indirectly to the restart point of the CCB.
	*/
	SCR_JUMP,
		PADDR (resel_go),

}/*-------------------------< RESEL_DSA >-------------------*/,{
	/*
	**	Ack the IDENTIFY or TAG previously received.
	*/
	SCR_CLR (SCR_ACK),
		0,
	/*
	**      load the savep (saved pointer) into
	**      the actual data pointer.
	*/
	SCR_LOAD_REL (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	/*
	**      Initialize the status registers
	*/
	SCR_LOAD_REL (scr0, 4),
		offsetof (struct ccb, phys.header.status),
	/*
	**	Jump to dispatcher.
	*/
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< DATA_IN >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTER parameter,
**	it is filled in at runtime.
**
**  ##===========< i=0; i<MAX_SCATTER >=========
**  ||	SCR_CHMOV_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_IN2 >-------------------*/,{
	SCR_CALL,
		PADDR (datai_done),
	SCR_JUMP,
		PADDRH (data_ovrun),
}/*-------------------------< DATA_OUT >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTER parameter,
**	it is filled in at runtime.
**
**  ##===========< i=0; i<MAX_SCATTER >=========
**  ||	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_OUT2 >-------------------*/,{
	SCR_CALL,
		PADDR (datao_done),
	SCR_JUMP,
		PADDRH (data_ovrun),

}/*-------------------------< PM0_DATA >--------------------*/,{
	/*
	**	Read our host flags to SFBR, so we will be able 
	**	to check against the data direction we expect.
	*/
	SCR_FROM_REG (HF_REG),
		0,
	/*
	**	Check against actual DATA PHASE.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR (pm0_data_out),
	/*
	**	Actual phase is DATA IN.
	**	Check against expected direction.
	*/
	SCR_JUMP ^ IFFALSE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDRH (data_ovrun),
	/*
	**	Keep track we are moving data from the 
	**	PM0 DATA mini-script.
	*/
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM0),
		0,
	/*
	**	Move the data to memory.
	*/
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct ccb, phys.pm0.sg),
	SCR_JUMP,
		PADDR (pm0_data_end),
}/*-------------------------< PM0_DATA_OUT >----------------*/,{
	/*
	**	Actual phase is DATA OUT.
	**	Check against expected direction.
	*/
	SCR_JUMP ^ IFTRUE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDRH (data_ovrun),
	/*
	**	Keep track we are moving data from the 
	**	PM0 DATA mini-script.
	*/
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM0),
		0,
	/*
	**	Move the data from memory.
	*/
	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
		offsetof (struct ccb, phys.pm0.sg),
}/*-------------------------< PM0_DATA_END >----------------*/,{
	/*
	**	Clear the flag that told we were moving  
	**	data from the PM0 DATA mini-script.
	*/
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_IN_PM0)),
		0,
	/*
	**	Return to the previous DATA script which 
	**	is guaranteed by design (if no bug) to be 
	**	the main DATA script for this transfer.
	*/
	SCR_LOAD_REL (temp, 4),
		offsetof (struct ccb, phys.pm0.ret),
	SCR_RETURN,
		0,
}/*-------------------------< PM1_DATA >--------------------*/,{
	/*
	**	Read our host flags to SFBR, so we will be able 
	**	to check against the data direction we expect.
	*/
	SCR_FROM_REG (HF_REG),
		0,
	/*
	**	Check against actual DATA PHASE.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR (pm1_data_out),
	/*
	**	Actual phase is DATA IN.
	**	Check against expected direction.
	*/
	SCR_JUMP ^ IFFALSE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDRH (data_ovrun),
	/*
	**	Keep track we are moving data from the 
	**	PM1 DATA mini-script.
	*/
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM1),
		0,
	/*
	**	Move the data to memory.
	*/
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct ccb, phys.pm1.sg),
	SCR_JUMP,
		PADDR (pm1_data_end),
}/*-------------------------< PM1_DATA_OUT >----------------*/,{
	/*
	**	Actual phase is DATA OUT.
	**	Check against expected direction.
	*/
	SCR_JUMP ^ IFTRUE (MASK (HF_DATA_IN, HF_DATA_IN)),
		PADDRH (data_ovrun),
	/*
	**	Keep track we are moving data from the 
	**	PM1 DATA mini-script.
	*/
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM1),
		0,
	/*
	**	Move the data from memory.
	*/
	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
		offsetof (struct ccb, phys.pm1.sg),
}/*-------------------------< PM1_DATA_END >----------------*/,{
	/*
	**	Clear the flag that told we were moving  
	**	data from the PM1 DATA mini-script.
	*/
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_IN_PM1)),
		0,
	/*
	**	Return to the previous DATA script which 
	**	is guaranteed by design (if no bug) to be 
	**	the main DATA script for this transfer.
	*/
	SCR_LOAD_REL (temp, 4),
		offsetof (struct ccb, phys.pm1.ret),
	SCR_RETURN,
		0,
}/*---------------------------------------------------------*/
};


static	struct scripth scripth0 __initdata = {
/*------------------------< START64 >-----------------------*/{
	/*
	**	SCRIPT entry point for the 895A and the 896.
	**	For now, there is no specific stuff for that 
	**	chip at this point, but this may come.
	*/
	SCR_JUMP,
		PADDR (init),
}/*-------------------------< NO_DATA >-------------------*/,{
	SCR_JUMP,
		PADDRH (data_ovrun),
}/*-----------------------< SEL_FOR_ABORT >------------------*/,{
	/*
	**	We are jumped here by the C code, if we have 
	**	some target to reset or some disconnected 
	**	job to abort. Since error recovery is a serious 
	**	busyness, we will really reset the SCSI BUS, if 
	**	case of a SCSI interrupt occuring in this path.
	*/

	/*
	**	Set initiator mode.
	*/
	SCR_CLR (SCR_TRG),
		0,
	/*
	**      And try to select this target.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct ncb, abrt_sel),
		PADDR (reselect),

	/*
	**	Wait for the selection to complete or 
	**	the selection to time out.
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		-8,
	/*
	**	Call the C code.
	*/
	SCR_INT,
		SIR_TARGET_SELECTED,
	/*
	**	The C code should let us continue here. 
	**	Send the 'kiss of death' message.
	**	We expect an immediate disconnect once 
	**	the target has eaten the message.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct ncb, abrt_tbl),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	/*
	**	Tell the C code that we are done.
	*/
	SCR_INT,
		SIR_ABORT_SENT,
}/*-----------------------< SEL_FOR_ABORT_1 >--------------*/,{
	/*
	**	Jump at scheduler.
	*/
	SCR_JUMP,
		PADDR (start),

}/*------------------------< SELECT_NO_ATN >-----------------*/,{
	/*
	**	Set Initiator mode.
	**      And try to select this target without ATN.
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR (ungetjob),
	/*
	**      load the savep (saved pointer) into
	**      the actual data pointer.
	*/
	SCR_LOAD_REL (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	/*
	**      Initialize the status registers
	*/
	SCR_LOAD_REL (scr0, 4),
		offsetof (struct ccb, phys.header.status),

}/*------------------------< WF_SEL_DONE_NO_ATN >-----------------*/,{
	/*
	**	Wait immediately for the next phase or 
	**	the selection to complete or time-out.
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		0,
	SCR_JUMP,
		PADDR (select2),

}/*-------------------------< MSG_IN_ETC >--------------------*/,{
	/*
	**	If it is an EXTENDED (variable size message)
	**	Handle it.
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDRH (msg_extended),
	/*
	**	Let the C code handle any other 
	**	1 byte message.
	*/
	SCR_JUMP ^ IFTRUE (MASK (0x00, 0xf0)),
		PADDRH (msg_received),
	SCR_JUMP ^ IFTRUE (MASK (0x10, 0xf0)),
		PADDRH (msg_received),
	/*
	**	We donnot handle 2 bytes messages from SCRIPTS.
	**	So, let the C code deal with these ones too.
	*/
	SCR_JUMP ^ IFFALSE (MASK (0x20, 0xf0)),
		PADDRH (msg_weird_seen),
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	SCR_JUMP,
		PADDRH (msg_received),

}/*-------------------------< MSG_RECEIVED >--------------------*/,{
	SCR_LOAD_REL (scratcha, 4),	/* DUMMY READ */
		0,
	SCR_INT,
		SIR_MSG_RECEIVED,

}/*-------------------------< MSG_WEIRD_SEEN >------------------*/,{
	SCR_LOAD_REL (scratcha, 4),	/* DUMMY READ */
		0,
	SCR_INT,
		SIR_MSG_WEIRD,

}/*-------------------------< MSG_EXTENDED >--------------------*/,{
	/*
	**	Clear ACK and get the next byte 
	**	assumed to be the message length.
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	**	Try to catch some unlikely situations as 0 length 
	**	or too large the length.
	*/
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDRH (msg_weird_seen),
	SCR_TO_REG (scratcha),
		0,
	SCR_REG_REG (sfbr, SCR_ADD, (256-8)),
		0,
	SCR_JUMP ^ IFTRUE (CARRYSET),
		PADDRH (msg_weird_seen),
	/*
	**	We donnot handle extended messages from SCRIPTS.
	**	Read the amount of data correponding to the 
	**	message length and call the C code.
	*/
	SCR_STORE_REL (scratcha, 1),
		offsetof (struct dsb, smsg_ext.size),
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_TBL ^ SCR_MSG_IN,
		offsetof (struct dsb, smsg_ext),
	SCR_JUMP,
		PADDRH (msg_received),

}/*-------------------------< MSG_BAD >------------------*/,{
	/*
	**	unimplemented message - reject it.
	*/
	SCR_INT,
		SIR_REJECT_TO_SEND,
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_WEIRD >--------------------*/,{
	/*
	**	weird message received
	**	ignore all MSG IN phases and reject it.
	*/
	SCR_INT,
		SIR_REJECT_TO_SEND,
	SCR_SET (SCR_ATN),
		0,
}/*-------------------------< MSG_WEIRD1 >--------------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (scratch),
	SCR_JUMP,
		PADDRH (msg_weird1),
}/*-------------------------< WDTR_RESP >----------------*/,{
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_WDTR >----------------*/,{
	/*
	**	Send the M_X_WIDE_REQ
	*/
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMP,
		PADDRH (msg_out_done),

}/*-------------------------< SDTR_RESP >-------------*/,{
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_SDTR >-------------*/,{
	/*
	**	Send the M_X_SYNC_REQ
	*/
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMP,
		PADDRH (msg_out_done),

}/*-------------------------< PPR_RESP >-------------*/,{
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_PPR >-------------*/,{
	/*
	**	Send the M_X_PPR_REQ
	*/
	SCR_MOVE_ABS (8) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMP,
		PADDRH (msg_out_done),

}/*-------------------------< NEGO_BAD_PHASE >------------*/,{
	SCR_INT,
		SIR_NEGO_PROTO,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	**	The target requests a message.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	/*
	**	... wait for the next phase
	**	if it's a message out, send it again, ...
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (msg_out),
}/*-------------------------< MSG_OUT_DONE >--------------*/,{
	/*
	**	... else clear the message ...
	*/
	SCR_INT,
		SIR_MSG_OUT_DONE,
	/*
	**	... and process the next phase
	*/
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< DATA_OVRUN >-----------------------*/,{
	/*
	 *  Use scratcha to count the extra bytes.
	 */
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (zero),
}/*-------------------------< DATA_OVRUN1 >----------------------*/,{
	/*
	 *  The target may want to transfer too much data.
	 *
	 *  If phase is DATA OUT write 1 byte and count it.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		16,
	SCR_CHMOV_ABS (1) ^ SCR_DATA_OUT,
		NADDR (scratch),
	SCR_JUMP,
		PADDRH (data_ovrun2),
	/*
	 *  If WSR is set, clear this condition, and 
	 *  count this byte.
	 */
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDRH (data_ovrun2),
	/*
	 *  Finally check against DATA IN phase.
	 *  Signal data overrun to the C code 
	 *  and jump to dispatcher if not so.
	 *  Read 1 byte otherwise and count it.
	 */
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_DATA_IN)),
		16,
	SCR_INT,
		SIR_DATA_OVERRUN,
	SCR_JUMP,
		PADDR (dispatch),
	SCR_CHMOV_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
}/*-------------------------< DATA_OVRUN2 >----------------------*/,{
	/*
	 *  Count this byte.
	 *  This will allow to return a negative 
	 *  residual to user.
	 */
	SCR_REG_REG (scratcha,  SCR_ADD,  0x01),
		0,
	SCR_REG_REG (scratcha1, SCR_ADDC, 0),
		0,
	SCR_REG_REG (scratcha2, SCR_ADDC, 0),
		0,
	/*
	 *  .. and repeat as required.
	 */
	SCR_JUMP,
		PADDRH (data_ovrun1),

}/*-------------------------< ABORT_RESEL >----------------*/,{
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	send the abort/abortag/reset message
	**	we expect an immediate disconnect
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_INT,
		SIR_RESEL_ABORTED,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< RESEND_IDENT >-------------------*/,{
	/*
	**	The target stays in MSG OUT phase after having acked 
	**	Identify [+ Tag [+ Extended message ]]. Targets shall
	**	behave this way on parity error.
	**	We must send it again all the messages.
	*/
	SCR_SET (SCR_ATN), /* Shall be asserted 2 deskew delays before the  */
		0,         /* 1rst ACK = 90 ns. Hope the NCR is'nt too fast */
	SCR_JUMP,
		PADDR (send_ident),
}/*-------------------------< IDENT_BREAK >-------------------*/,{
	SCR_CLR (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (select2),
}/*-------------------------< IDENT_BREAK_ATN >----------------*/,{
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (select2),
}/*-------------------------< SDATA_IN >-------------------*/,{
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	SCR_CALL,
		PADDR (datai_done),
	SCR_JUMP,
		PADDRH (data_ovrun),
}/*-------------------------< DATA_IO >--------------------*/,{
	/*
	**	We jump here if the data direction was unknown at the 
	**	time we had to queue the command to the scripts processor.
	**	Pointers had been set as follow in this situation:
	**	  savep   -->   DATA_IO
	**	  lastp   -->   start pointer when DATA_IN
	**	  goalp   -->   goal  pointer when DATA_IN
	**	  wlastp  -->   start pointer when DATA_OUT
	**	  wgoalp  -->   goal  pointer when DATA_OUT
	**	This script sets savep/lastp/goalp according to the 
	**	direction chosen by the target.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		PADDRH(data_io_out),
}/*-------------------------< DATA_IO_COM >-----------------*/,{
	/*
	**	Direction is DATA IN.
	**	Warning: we jump here, even when phase is DATA OUT.
	*/
	SCR_LOAD_REL  (scratcha, 4),
		offsetof (struct ccb, phys.header.lastp),
	SCR_STORE_REL (scratcha, 4),
		offsetof (struct ccb, phys.header.savep),

	/*
	**	Jump to the SCRIPTS according to actual direction.
	*/
	SCR_LOAD_REL  (temp, 4),
		offsetof (struct ccb, phys.header.savep),
	SCR_RETURN,
		0,
}/*-------------------------< DATA_IO_OUT >-----------------*/,{
	/*
	**	Direction is DATA OUT.
	*/
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_DATA_IN)),
		0,
	SCR_LOAD_REL  (scratcha, 4),
		offsetof (struct ccb, phys.header.wlastp),
	SCR_STORE_REL (scratcha, 4),
		offsetof (struct ccb, phys.header.lastp),
	SCR_LOAD_REL  (scratcha, 4),
		offsetof (struct ccb, phys.header.wgoalp),
	SCR_STORE_REL (scratcha, 4),
		offsetof (struct ccb, phys.header.goalp),
	SCR_JUMP,
		PADDRH(data_io_com),

}/*-------------------------< RESEL_BAD_LUN >---------------*/,{
	/*
	**	Message is an IDENTIFY, but lun is unknown.
	**	Signal problem to C code for logging the event.
	**	Send a M_ABORT to clear all pending tasks.
	*/
	SCR_INT,
		SIR_RESEL_BAD_LUN,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< BAD_I_T_L >------------------*/,{
	/*
	**	We donnot have a task for that I_T_L.
	**	Signal problem to C code for logging the event.
	**	Send a M_ABORT message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_I_T_L,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< BAD_I_T_L_Q >----------------*/,{
	/*
	**	We donnot have a task that matches the tag.
	**	Signal problem to C code for logging the event.
	**	Send a M_ABORTTAG message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_I_T_L_Q,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< BAD_STATUS >-----------------*/,{
	/*
	**	Anything different from INTERMEDIATE 
	**	CONDITION MET should be a bad SCSI status, 
	**	given that GOOD status has already been tested.
	**	Call the C code.
	*/
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (startpos),
	SCR_INT ^ IFFALSE (DATA (S_COND_MET)),
		SIR_BAD_STATUS,
	SCR_RETURN,
		0,

}/*-------------------------< TWEAK_PMJ >------------------*/,{
	/*
	**	Disable PM handling from SCRIPTS for the data phase 
	**	and so force PM to be handled from C code if HF_PM_TO_C 
	**	flag is set.
	*/
	SCR_FROM_REG(HF_REG),
		0,
	SCR_JUMPR ^ IFTRUE (MASK (HF_PM_TO_C, HF_PM_TO_C)),
		16,
	SCR_REG_REG (ccntl0, SCR_OR, ENPMJ),
		0,
	SCR_RETURN,
 		0,
	SCR_REG_REG (ccntl0, SCR_AND, (~ENPMJ)),
		0,
	SCR_RETURN,
 		0,

}/*-------------------------< PM_HANDLE >------------------*/,{
	/*
	**	Phase mismatch handling.
	**
	**	Since we have to deal with 2 SCSI data pointers  
	**	(current and saved), we need at least 2 contexts.
	**	Each context (pm0 and pm1) has a saved area, a 
	**	SAVE mini-script and a DATA phase mini-script.
	*/
	/*
	**	Get the PM handling flags.
	*/
	SCR_FROM_REG (HF_REG),
		0,
	/*
	**	If no flags (1rst PM for example), avoid 
	**	all the below heavy flags testing.
	**	This makes the normal case a bit faster.
	*/
	SCR_JUMP ^ IFTRUE (MASK (0, (HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED))),
		PADDRH (pm_handle1),
	/*
	**	If we received a SAVE DP, switch to the 
	**	other PM context since the savep may point 
	**	to the current PM context.
	*/
	SCR_JUMPR ^ IFFALSE (MASK (HF_DP_SAVED, HF_DP_SAVED)),
		8,
	SCR_REG_REG (sfbr, SCR_XOR, HF_ACT_PM),
		0,
	/*
	**	If we have been interrupt in a PM DATA mini-script,
	**	we take the return address from the corresponding 
	**	saved area.
	**	This ensure the return address always points to the 
	**	main DATA script for this transfer.
	*/
	SCR_JUMP ^ IFTRUE (MASK (0, (HF_IN_PM0 | HF_IN_PM1))),
		PADDRH (pm_handle1),
	SCR_JUMPR ^ IFFALSE (MASK (HF_IN_PM0, HF_IN_PM0)),
		16,
	SCR_LOAD_REL (ia, 4),
		offsetof(struct ccb, phys.pm0.ret),
	SCR_JUMP,
		PADDRH (pm_save),
	SCR_LOAD_REL (ia, 4),
		offsetof(struct ccb, phys.pm1.ret),
	SCR_JUMP,
		PADDRH (pm_save),
}/*-------------------------< PM_HANDLE1 >-----------------*/,{
	/*
	**	Normal case.
	**	Update the return address so that it 
	**	will point after the interrupted MOVE.
	*/
	SCR_REG_REG (ia, SCR_ADD, 8),
		0,
	SCR_REG_REG (ia1, SCR_ADDC, 0),
		0,
}/*-------------------------< PM_SAVE >--------------------*/,{
	/*
	**	Clear all the flags that told us if we were 
	**	interrupted in a PM DATA mini-script and/or 
	**	we received a SAVE DP.
	*/
	SCR_SFBR_REG (HF_REG, SCR_AND, (~(HF_IN_PM0|HF_IN_PM1|HF_DP_SAVED))),
		0,
	/*
	**	Choose the current PM context.
	*/
	SCR_JUMP ^ IFTRUE (MASK (HF_ACT_PM, HF_ACT_PM)),
		PADDRH (pm1_save),
}/*-------------------------< PM0_SAVE >-------------------*/,{
	SCR_STORE_REL (ia, 4),
		offsetof(struct ccb, phys.pm0.ret),
	/*
	**	If WSR bit is set, either UA and RBC may 
	**	have to be changed whatever the device wants 
	**	to ignore this residue ot not.
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_CALL ^ IFTRUE (MASK (WSR, WSR)),
		PADDRH (pm_wsr_handle),
	/*
	**	Save the remaining byte count, the updated 
	**	address and the return address.
	*/
	SCR_STORE_REL (rbc, 4),
		offsetof(struct ccb, phys.pm0.sg.size),
	SCR_STORE_REL (ua, 4),
		offsetof(struct ccb, phys.pm0.sg.addr),
	/*
	**	Set the current pointer at the PM0 DATA mini-script.
	*/
	SCR_LOAD_ABS (temp, 4),
		PADDRH (pm0_data_addr),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< PM1_SAVE >-------------------*/,{
	SCR_STORE_REL (ia, 4),
		offsetof(struct ccb, phys.pm1.ret),
	/*
	**	If WSR bit is set, either UA and RBC may 
	**	have been changed whatever the device wants 
	**	to ignore this residue or not.
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_CALL ^ IFTRUE (MASK (WSR, WSR)),
		PADDRH (pm_wsr_handle),
	/*
	**	Save the remaining byte count, the updated 
	**	address and the return address.
	*/
	SCR_STORE_REL (rbc, 4),
		offsetof(struct ccb, phys.pm1.sg.size),
	SCR_STORE_REL (ua, 4),
		offsetof(struct ccb, phys.pm1.sg.addr),
	/*
	**	Set the current pointer at the PM1 DATA mini-script.
	*/
	SCR_LOAD_ABS (temp, 4),
		PADDRH (pm1_data_addr),
	SCR_JUMP,
		PADDR (dispatch),
}/*--------------------------< PM_WSR_HANDLE >-----------------------*/,{
	/*
	 *  Phase mismatch handling from SCRIPT with WSR set.
	 *  Such a condition can occur if the chip wants to 
	 *  execute a CHMOV(size > 1) when the WSR bit is 
	 *  set and the target changes PHASE.
	 */
#ifdef	SYM_DEBUG_PM_WITH_WSR
	/*
	 *  Some debugging may still be needed.:)
	 */ 
	SCR_INT,
		SIR_PM_WITH_WSR,
#endif
	/*
	 *  We must move the residual byte to memory.
	 *
	 *  UA contains bit 0..31 of the address to 
	 *  move the residual byte.
	 *  Move it to the table indirect.
	 */
	SCR_STORE_REL (ua, 4),
		offsetof (struct ccb, phys.wresid.addr),
	/*
	 *  Increment UA (move address to next position).
	 */
	SCR_REG_REG (ua, SCR_ADD, 1),
		0,
	SCR_REG_REG (ua1, SCR_ADDC, 0),
		0,
	SCR_REG_REG (ua2, SCR_ADDC, 0),
		0,
	SCR_REG_REG (ua3, SCR_ADDC, 0),
		0,
	/*
	 *  Compute SCRATCHA as:
	 *  - size to transfer = 1 byte.
	 *  - bit 24..31 = high address bit [32...39].
	 */
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (zero),
	SCR_REG_REG (scratcha, SCR_OR, 1),
		0,
	SCR_FROM_REG (rbc3),
		0,
	SCR_TO_REG (scratcha3),
		0,
	/*
	 *  Move this value to the table indirect.
	 */
	SCR_STORE_REL (scratcha, 4),
		offsetof (struct ccb, phys.wresid.size),
	/*
	 *  Wait for a valid phase.
	 *  While testing with bogus QUANTUM drives, the C1010 
	 *  sometimes raised a spurious phase mismatch with 
	 *  WSR and the CHMOV(1) triggered another PM.
	 *  Waiting explicitely for the PHASE seemed to avoid 
	 *  the nested phase mismatch. Btw, this didn't happen 
	 *  using my IBM drives.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_IN)),
		0,
	/*
	 *  Perform the move of the residual byte.
	 */
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct ccb, phys.wresid),
	/*
	 *  We can now handle the phase mismatch with UA fixed.
	 *  RBC[0..23]=0 is a special case that does not require 
	 *  a PM context. The C code also checks against this.
	 */
	SCR_FROM_REG (rbc),
		0,
	SCR_RETURN ^ IFFALSE (DATA (0)),
		0,
	SCR_FROM_REG (rbc1),
		0,
	SCR_RETURN ^ IFFALSE (DATA (0)),
		0,
	SCR_FROM_REG (rbc2),
		0,
	SCR_RETURN ^ IFFALSE (DATA (0)),
		0,
	/*
	 *  RBC[0..23]=0.
	 *  Not only we donnot need a PM context, but this would 
	 *  lead to a bogus CHMOV(0). This condition means that 
	 *  the residual was the last byte to move from this CHMOV.
	 *  So, we just have to move the current data script pointer 
	 *  (i.e. TEMP) to the SCRIPTS address following the 
	 *  interrupted CHMOV and jump to dispatcher.
	 */
	SCR_STORE_ABS (ia, 4),
		PADDRH (scratch),
	SCR_LOAD_ABS (temp, 4),
		PADDRH (scratch),
	SCR_JUMP,
		PADDR (dispatch),
}/*--------------------------< WSR_MA_HELPER >-----------------------*/,{
	/*
	 *  Helper for the C code when WSR bit is set.
	 *  Perform the move of the residual byte.
	 */
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct ccb, phys.wresid),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< ZERO >------------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SCRATCH >---------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SCRATCH1 >--------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< PM0_DATA_ADDR >---------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< PM1_DATA_ADDR >---------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SAVED_DSA >-------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SAVED_DRS >-------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< DONE_POS >--------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< STARTPOS >--------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< TARGTBL >---------------------*/,{
	SCR_DATA_ZERO,


/*
** We may use MEMORY MOVE instructions to load the on chip-RAM,
** if it happens that mapping PCI memory is not possible.
** But writing the RAM from the CPU is the preferred method, 
** since PCI 2.2 seems to disallow PCI self-mastering.
*/

#ifdef SCSI_NCR_PCI_MEM_NOT_SUPPORTED

}/*-------------------------< START_RAM >-------------------*/,{
	/*
	**	Load the script into on-chip RAM, 
	**	and jump to start point.
	*/
	SCR_COPY (sizeof (struct script)),
}/*-------------------------< SCRIPT0_BA >--------------------*/,{
		0,
		PADDR (start),
	SCR_JUMP,
		PADDR (init),

}/*-------------------------< START_RAM64 >--------------------*/,{
	/*
	**	Load the RAM and start for 64 bit PCI (895A,896).
	**	Both scripts (script and scripth) are loaded into 
	**	the RAM which is 8K (4K for 825A/875/895).
	**	We also need to load some 32-63 bit segments 
	**	address of the SCRIPTS processor.
	**	LOAD/STORE ABSOLUTE always refers to on-chip RAM 
	**	in our implementation. The main memory is 
	**	accessed using LOAD/STORE DSA RELATIVE.
	*/
	SCR_LOAD_REL (mmws, 4),
		offsetof (struct ncb, scr_ram_seg),
	SCR_COPY (sizeof(struct script)),
}/*-------------------------< SCRIPT0_BA64 >--------------------*/,{
		0,
		PADDR (start),
	SCR_COPY (sizeof(struct scripth)),
}/*-------------------------< SCRIPTH0_BA64 >--------------------*/,{
		0,
		PADDRH  (start64),
	SCR_LOAD_REL  (mmrs, 4),
		offsetof (struct ncb, scr_ram_seg),
	SCR_JUMP64,
		PADDRH (start64),
}/*-------------------------< RAM_SEG64 >--------------------*/,{
		0,

#endif /* SCSI_NCR_PCI_MEM_NOT_SUPPORTED */

}/*-------------------------< SNOOPTEST >-------------------*/,{
	/*
	**	Read the variable.
	*/
	SCR_LOAD_REL (scratcha, 4),
		offsetof(struct ncb, ncr_cache),
	SCR_STORE_REL (temp, 4),
		offsetof(struct ncb, ncr_cache),
	SCR_LOAD_REL (temp, 4),
		offsetof(struct ncb, ncr_cache),
}/*-------------------------< SNOOPEND >-------------------*/,{
	/*
	**	And stop.
	*/
	SCR_INT,
		99,
}/*--------------------------------------------------------*/
};

/*==========================================================
**
**
**	Fill in #define dependent parts of the script
**
**
**==========================================================
*/

void __init ncr_script_fill (struct script * scr, struct scripth * scrh)
{
	int	i;
	ncrcmd	*p;

	p = scr->data_in;
	for (i=0; i<MAX_SCATTER; i++) {
		*p++ =SCR_CHMOV_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	};

	assert ((u_long)p == (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scr->data_out;

	for (i=0; i<MAX_SCATTER; i++) {
		*p++ =SCR_CHMOV_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	};

	assert ((u_long)p == (u_long)&scr->data_out + sizeof (scr->data_out));
}

/*==========================================================
**
**
**	Copy and rebind a script.
**
**
**==========================================================
*/

static void __init 
ncr_script_copy_and_bind (ncb_p np,ncrcmd *src,ncrcmd *dst,int len)
{
	ncrcmd  opcode, new, old, tmp1, tmp2;
	ncrcmd	*start, *end;
	int relocs;
	int opchanged = 0;

	start = src;
	end = src + len/4;

	while (src < end) {

		opcode = *src++;
		*dst++ = cpu_to_scr(opcode);

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0) {
			printk (KERN_INFO "%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), (int) (src-start-1));
			MDELAY (10000);
			continue;
		};

		/*
		**	We use the bogus value 0xf00ff00f ;-)
		**	to reserve data area in SCRIPTS.
		*/
		if (opcode == SCR_DATA_ZERO) {
			dst[-1] = 0;
			continue;
		}

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printk (KERN_INFO "%p:  <%x>\n",
				(src-1), (unsigned)opcode);

		/*
		**	We don't have to decode ALL commands
		*/
		switch (opcode >> 28) {

		case 0xf:
			/*
			**	LOAD / STORE DSA relative, don't relocate.
			*/
			relocs = 0;
			break;
		case 0xe:
			/*
			**	LOAD / STORE absolute.
			*/
			relocs = 1;
			break;
		case 0xc:
			/*
			**	COPY has TWO arguments.
			*/
			relocs = 2;
			tmp1 = src[0];
			tmp2 = src[1];
#ifdef	RELOC_KVAR
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
#endif
			if ((tmp1 ^ tmp2) & 3) {
				printk (KERN_ERR"%s: ERROR1 IN SCRIPT at %d.\n",
					ncr_name(np), (int) (src-start-1));
				MDELAY (1000);
			}
			/*
			**	If PREFETCH feature not enabled, remove 
			**	the NO FLUSH bit if present.
			*/
			if ((opcode & SCR_NO_FLUSH) &&
			    !(np->features & FE_PFEN)) {
				dst[-1] = cpu_to_scr(opcode & ~SCR_NO_FLUSH);
				++opchanged;
			}
			break;

		case 0x0:
			/*
			**	MOVE/CHMOV (absolute address)
			*/
			if (!(np->features & FE_WIDE))
				dst[-1] = cpu_to_scr(opcode | OPC_MOVE);
			relocs = 1;
			break;

		case 0x1:
			/*
			**	MOVE/CHMOV (table indirect)
			*/
			if (!(np->features & FE_WIDE))
				dst[-1] = cpu_to_scr(opcode | OPC_MOVE);
			relocs = 0;
			break;

		case 0x8:
			/*
			**	JUMP / CALL
			**	dont't relocate if relative :-)
			*/
			if (opcode & 0x00800000)
				relocs = 0;
			else if ((opcode & 0xf8400000) == 0x80400000)/*JUMP64*/
				relocs = 2;
			else
				relocs = 1;
			break;

		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			relocs = 1;
			break;

		default:
			relocs = 0;
			break;
		};

		if (!relocs) {
			*dst++ = cpu_to_scr(*src++);
			continue;
		}
		while (relocs--) {
			old = *src++;

			switch (old & RELOC_MASK) {
			case RELOC_REGISTER:
				new = (old & ~RELOC_MASK) + np->base_ba;
				break;
			case RELOC_LABEL:
				new = (old & ~RELOC_MASK) + np->p_script;
				break;
			case RELOC_LABELH:
				new = (old & ~RELOC_MASK) + np->p_scripth;
				break;
			case RELOC_SOFTC:
				new = (old & ~RELOC_MASK) + np->p_ncb;
				break;
#ifdef	RELOC_KVAR
			case RELOC_KVAR:
				new=0;
				if (((old & ~RELOC_MASK) < SCRIPT_KVAR_FIRST) ||
				    ((old & ~RELOC_MASK) > SCRIPT_KVAR_LAST))
					panic("ncr KVAR out of range");
				new = vtobus(script_kvars[old & ~RELOC_MASK]);
#endif
				break;
			case 0:
				/* Don't relocate a 0 address. */
				if (old == 0) {
					new = old;
					break;
				}
				/* fall through */
			default:
				new = 0;	/* For 'cc' not to complain */
				panic("ncr_script_copy_and_bind: "
				      "weird relocation %x\n", old);
				break;
			}

			*dst++ = cpu_to_scr(new);
		}
	};
}

/*==========================================================
**
**
**      Auto configuration:  attach and init a host adapter.
**
**
**==========================================================
*/

/*
**	Linux host data structure.
*/

struct host_data {
     struct ncb *ncb;
};

/*
**	Print something which allows to retrieve the controler type, unit,
**	target, lun concerned by a kernel message.
*/

static void PRINT_TARGET(ncb_p np, int target)
{
	printk(KERN_INFO "%s-<%d,*>: ", ncr_name(np), target);
}

static void PRINT_LUN(ncb_p np, int target, int lun)
{
	printk(KERN_INFO "%s-<%d,%d>: ", ncr_name(np), target, lun);
}

static void PRINT_ADDR(Scsi_Cmnd *cmd)
{
	struct host_data *host_data = (struct host_data *) cmd->host->hostdata;
	PRINT_LUN(host_data->ncb, cmd->target, cmd->lun);
}

/*==========================================================
**
**	NCR chip clock divisor table.
**	Divisors are multiplied by 10,000,000 in order to make 
**	calculations more simple.
**
**==========================================================
*/

#define _5M 5000000
static u_long div_10M[] =
	{2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};


/*===============================================================
**
**	Prepare io register values used by ncr_init() according 
**	to selected and supported features.
**
**	NCR/SYMBIOS chips allow burst lengths of 2, 4, 8, 16, 32, 64,
**	128 transfers. All chips support at least 16 transfers bursts. 
**	The 825A, 875 and 895 chips support bursts of up to 128 
**	transfers and the 895A and 896 support bursts of up to 64 
**	transfers. All other chips support up to 16 transfers bursts.
**
**	For PCI 32 bit data transfers each transfer is a DWORD (4 bytes).
**	It is a QUADWORD (8 bytes) for PCI 64 bit data transfers.
**	Only the 896 is able to perform 64 bit data transfers.
**
**	We use log base 2 (burst length) as internal code, with 
**	value 0 meaning "burst disabled".
**
**===============================================================
*/

/*
 *	Burst length from burst code.
 */
#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

/*
 *	Burst code from io register bits.
 */
#define burst_code(dmode, ctest4, ctest5) \
	(ctest4) & 0x80? 0 : (((dmode) & 0xc0) >> 6) + ((ctest5) & 0x04) + 1

/*
 *	Set initial io register bits from burst code.
 */
static inline void ncr_init_burst(ncb_p np, u_char bc)
{
	np->rv_ctest4	&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		np->rv_ctest4	|= 0x80;
	}
	else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

#ifdef SCSI_NCR_NVRAM_SUPPORT

/*
**	Get target set-up from Symbios format NVRAM.
*/

static void __init 
ncr_Symbios_setup_target(ncb_p np, int target, Symbios_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	Symbios_target *tn = &nvram->target[target];

	tp->usrsync = tn->sync_period ? (tn->sync_period + 3) / 4 : 255;
	tp->usrwide = tn->bus_width == 0x10 ? 1 : 0;
	tp->usrtags =
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? MAX_TAGS : 0;

	if (!(tn->flags & SYMBIOS_DISCONNECT_ENABLE))
		tp->usrflag |= UF_NODISC;
	if (!(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME))
		tp->usrflag |= UF_NOSCAN;
}

/*
**	Get target set-up from Tekram format NVRAM.
*/

static void __init
ncr_Tekram_setup_target(ncb_p np, int target, Tekram_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	struct Tekram_target *tn = &nvram->target[target];
	int i;

	if (tn->flags & TEKRAM_SYNC_NEGO) {
		i = tn->sync_index & 0xf;
		tp->usrsync = Tekram_sync[i];
	}

	tp->usrwide = (tn->flags & TEKRAM_WIDE_NEGO) ? 1 : 0;

	if (tn->flags & TEKRAM_TAGGED_COMMANDS) {
		tp->usrtags = 2 << nvram->max_tags_index;
	}

	if (!(tn->flags & TEKRAM_DISCONNECT_ENABLE))
		tp->usrflag = UF_NODISC;
 
	/* If any device does not support parity, we will not use this option */
	if (!(tn->flags & TEKRAM_PARITY_CHECK))
		np->rv_scntl0  &= ~0x0a; /* SCSI parity checking disabled */
}
#endif /* SCSI_NCR_NVRAM_SUPPORT */

/*
**	Save initial settings of some IO registers.
**	Assumed to have been set by BIOS.
*/
static void __init ncr_save_initial_setting(ncb_p np)
{
	np->sv_scntl0	= INB(nc_scntl0) & 0x0a;
	np->sv_dmode	= INB(nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(nc_dcntl)  & 0xa8;
	np->sv_ctest3	= INB(nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(nc_ctest4) & 0x80;
	np->sv_gpcntl	= INB(nc_gpcntl);
	np->sv_stest2	= INB(nc_stest2) & 0x20;
	np->sv_stest4	= INB(nc_stest4);
	np->sv_stest1	= INB(nc_stest1);

 	np->sv_scntl3   = INB(nc_scntl3) & 0x07;
 
 	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
 	 	(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66) ){
 		/*
 		** C1010 always uses large fifo, bit 5 rsvd
 		** scntl4 used ONLY with C1010
 		*/
 		np->sv_ctest5 = INB(nc_ctest5) & 0x04 ; 
 		np->sv_scntl4 = INB(nc_scntl4); 
         }
         else {
 		np->sv_ctest5 = INB(nc_ctest5) & 0x24 ; 
 		np->sv_scntl4 = 0;
         }
}

/*
**	Prepare io register values used by ncr_init() 
**	according to selected and supported features.
*/
static int __init ncr_prepare_setting(ncb_p np, ncr_nvram *nvram)
{
	u_char	burst_max;
	u_long	period;
	int i;

	/*
	**	Wide ?
	*/

	np->maxwide	= (np->features & FE_WIDE)? 1 : 0;

 	/*
	 *  Guess the frequency of the chip's clock.
	 */
	if	(np->features & (FE_ULTRA3 | FE_ULTRA2))
		np->clock_khz = 160000;
	else if	(np->features & FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	/*
	 *  Get the clock multiplier factor.
 	 */
	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	/*
	 *  Measure SCSI clock frequency for chips 
	 *  it may vary from assumed one.
	 */
	if (np->features & FE_VARCLK)
		ncr_getclock(np, np->multiplier);

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 *
	 * Note: For C1010 the async divisor is 2(8) if he
	 * quadrupler is disabled (enabled).
	 */

	if ( (np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
		(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {

		np->rv_scntl3 = 0; 
	}
	else
	{
		i = np->clock_divn - 1;
		while (--i >= 0) {
			if (10ul * SCSI_NCR_MIN_ASYNC * np->clock_khz 
							> div_10M[i]) {
				++i;
				break;
			}
		}
		np->rv_scntl3 = i+1;
	}


	/*
	 * Save the ultra3 register for the C1010/C1010_66
	 */

	np->rv_scntl4 = np->sv_scntl4;

	/*
	 * Minimum synchronous period factor supported by the chip.
	 * Btw, 'period' is in tenths of nanoseconds.
	 */

	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	/*
	 * Fix up. If sync. factor is 10 (160000Khz clock) and chip
	 * supports ultra3, then min. sync. period 12.5ns and the factor is 9 
	 * Also keep track of the maximum offset in ST mode which may differ  
	 * from the maximum offset in DT mode. For now hardcoded to 31. 
	 */

	if (np->features & FE_ULTRA3) {
		if (np->minsync == 10)
			np->minsync = 9;
		np->maxoffs_st = 31;
	}
	else
		np->maxoffs_st = np->maxoffs;

	/*
	 * Check against chip SCSI standard support (SCSI-2,ULTRA,ULTRA2).
	 *
	 * Transfer period minimums: SCSI-1 200 (50); Fast 100 (25)
	 *			Ultra 50 (12); Ultra2 (6); Ultra3 (3)		
	 */

	if	(np->minsync < 25 && !(np->features & (FE_ULTRA|FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 25;
	else if	(np->minsync < 12 && (np->features & FE_ULTRA))
		np->minsync = 12;
	else if	(np->minsync < 10 && (np->features & FE_ULTRA2))
		np->minsync = 10;
	else if	(np->minsync < 9 && (np->features & FE_ULTRA3))
		np->minsync = 9;

	/*
	 * Maximum synchronous period factor supported by the chip.
	 */

	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	/*
	**	64 bit (53C895A or 53C896) ?
	*/
	if (np->features & FE_DAC) {
		if (np->features & FE_DAC_IN_USE)
			np->rv_ccntl1	|= (XTIMOD | EXTIBMV);
		else
			np->rv_ccntl1	|= (DDAC);
	}

	/*
	**	Phase mismatch handled by SCRIPTS (53C895A, 53C896 or C1010) ?
  	*/
	if (np->features & FE_NOPM)
		np->rv_ccntl0	|= (ENPMJ);

	/*
	**	Prepare initial value of other IO registers
	*/
#if defined SCSI_NCR_TRUST_BIOS_SETTING
	np->rv_scntl0	= np->sv_scntl0;
	np->rv_dmode	= np->sv_dmode;
	np->rv_dcntl	= np->sv_dcntl;
	np->rv_ctest3	= np->sv_ctest3;
	np->rv_ctest4	= np->sv_ctest4;
	np->rv_ctest5	= np->sv_ctest5;
	burst_max	= burst_code(np->sv_dmode, np->sv_ctest4, np->sv_ctest5);
#else

	/*
	**	Select burst length (dwords)
	*/
	burst_max	= driver_setup.burst_max;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest4, np->sv_ctest5);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	/*
	**	DEL 352 - 53C810 Rev x11 - Part Number 609-0392140 - ITEM 2.
	**	This chip and the 860 Rev 1 may wrongly use PCI cache line 
	**	based transactions on LOAD/STORE instructions. So we have 
	**	to prevent these chips from using such PCI transactions in 
	**	this driver. The generic sym53c8xx driver that does not use 
	**	LOAD/STORE instructions does not need this work-around.
	*/
	if ((np->device_id == PCI_DEVICE_ID_NCR_53C810 &&
	     np->revision_id >= 0x10 && np->revision_id <= 0x11) ||
	    (np->device_id == PCI_DEVICE_ID_NCR_53C860 &&
	     np->revision_id <= 0x1))
		np->features &= ~(FE_WRIE|FE_ERL|FE_ERMP);

	/*
	**	DEL ? - 53C1010 Rev 1 - Part Number 609-0393638
	**	64-bit Slave Cycles must be disabled.
	*/
	if ( ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) && (np->revision_id < 0x02) )
		|| (np->device_id == PCI_DEVICE_ID_LSI_53C1010_66 ) )
		np->rv_ccntl1  |=  0x10;

	/*
	**	Select all supported special features.
	**	If we are using on-board RAM for scripts, prefetch (PFEN) 
	**	does not help, but burst op fetch (BOF) does.
	**	Disabling PFEN makes sure BOF will be used.
	*/
	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		/* Enable Read Line */
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		/* Burst Opcode Fetch */
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	/* Enable Read Multiple */
#if 1
	if ((np->features & FE_PFEN) && !np->base2_ba)
#else
	if (np->features & FE_PFEN)
#endif
		np->rv_dcntl	|= PFEN;	/* Prefetch Enable */
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	/* Cache Line Size Enable */
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	/* Write and Invalidate */


	if ( (np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66) &&
			(np->features & FE_DFS))
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size */
						/* C1010/C1010_66 always large fifo */

	/*
	**	Select some other
	*/
	if (driver_setup.master_parity)
		np->rv_ctest4	|= MPEE;	/* Master parity checking */
	if (driver_setup.scsi_parity)
		np->rv_scntl0	|= 0x0a;	/*  full arb., ena parity, par->ATN  */

#ifdef SCSI_NCR_NVRAM_SUPPORT
	/*
	**	Get parity checking, host ID and verbose mode from NVRAM
	**/
	if (nvram) {
		switch(nvram->type) {
		case SCSI_NCR_TEKRAM_NVRAM:
			np->myaddr = nvram->data.Tekram.host_id & 0x0f;
			break;
		case SCSI_NCR_SYMBIOS_NVRAM:
			if (!(nvram->data.Symbios.flags & SYMBIOS_PARITY_ENABLE))
				np->rv_scntl0  &= ~0x0a;
			np->myaddr = nvram->data.Symbios.host_id & 0x0f;
			if (nvram->data.Symbios.flags & SYMBIOS_VERBOSE_MSGS)
				np->verbose += 1;
			break;
		}
	}
#endif
	/*
	**  Get SCSI addr of host adapter (set by bios?).
	*/
	if (np->myaddr == 255) {
		np->myaddr = INB(nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SCSI_NCR_MYADDR;
	}

#endif /* SCSI_NCR_TRUST_BIOS_SETTING */

	/*
	 *	Prepare initial io register bits for burst length
	 */
	ncr_init_burst(np, burst_max);

	/*
	**	Set SCSI BUS mode.
	**
	**	- ULTRA2 chips (895/895A/896) 
	**	  and ULTRA 3 chips (1010) report the current 
	**	  BUS mode through the STEST4 IO register.
	**	- For previous generation chips (825/825A/875), 
	**	  user has to tell us how to check against HVD, 
	**	  since a 100% safe algorithm is not possible.
	*/
	np->scsi_mode = SMODE_SE;
	if	(np->features & (FE_ULTRA2 | FE_ULTRA3))
		np->scsi_mode = (np->sv_stest4 & SMODE);
	else if	(np->features & FE_DIFF) {
		switch(driver_setup.diff_support) {
		case 4:	/* Trust previous settings if present, then GPIO3 */
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
				break;
			}
		case 3:	/* SYMBIOS controllers report HVD through GPIO3 */
			if (nvram && nvram->type != SCSI_NCR_SYMBIOS_NVRAM)
				break;
			if (INB(nc_gpreg) & 0x08)
				break;
		case 2:	/* Set HVD unconditionally */
			np->scsi_mode = SMODE_HVD;
		case 1:	/* Trust previous settings for HVD */
			if (np->sv_stest2 & 0x20)
				np->scsi_mode = SMODE_HVD;
			break;
		default:/* Don't care about HVD */	
			break;
		}
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;

	/*
	**	Set LED support from SCRIPTS.
	**	Ignore this feature for boards known to use a 
	**	specific GPIO wiring and for the 895A or 896 
	**	that drive the LED directly.
	**	Also probe initial setting of GPIO0 as output.
	*/
	if ((driver_setup.led_pin ||
	     (nvram && nvram->type == SCSI_NCR_SYMBIOS_NVRAM)) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	/*
	**	Set irq mode.
	*/
	switch(driver_setup.irqm & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	/*
	**	Configure targets according to driver setup.
	**	If NVRAM present get targets setup from NVRAM.
	**	Allow to override sync, wide and NOSCAN from 
	**	boot command line.
	*/
	for (i = 0 ; i < MAX_TARGET ; i++) {
		tcb_p tp = &np->target[i];

		tp->usrsync = 255;
#ifdef SCSI_NCR_NVRAM_SUPPORT
		if (nvram) {
			switch(nvram->type) {
			case SCSI_NCR_TEKRAM_NVRAM:
				ncr_Tekram_setup_target(np, i, &nvram->data.Tekram);
				break;
			case SCSI_NCR_SYMBIOS_NVRAM:
				ncr_Symbios_setup_target(np, i, &nvram->data.Symbios);
				break;
			}
			if (driver_setup.use_nvram & 0x2)
				tp->usrsync = driver_setup.default_sync;
			if (driver_setup.use_nvram & 0x4)
				tp->usrwide = driver_setup.max_wide;
			if (driver_setup.use_nvram & 0x8)
				tp->usrflag &= ~UF_NOSCAN;
		}
		else {
#else
		if (1) {
#endif
			tp->usrsync = driver_setup.default_sync;
			tp->usrwide = driver_setup.max_wide;
			tp->usrtags = MAX_TAGS;
			if (!driver_setup.disconnection)
				np->target[i].usrflag = UF_NODISC;
		}
	}

	/*
	**	Announce all that stuff to user.
	*/

	i = nvram ? nvram->type : 0;
	printk(KERN_INFO "%s: %sID %d, Fast-%d%s%s\n", ncr_name(np),
		i  == SCSI_NCR_SYMBIOS_NVRAM ? "Symbios format NVRAM, " :
		(i == SCSI_NCR_TEKRAM_NVRAM  ? "Tekram format NVRAM, " : ""),
		np->myaddr,
		np->minsync < 10 ? 80 : 
			(np->minsync < 12 ? 40 : (np->minsync < 25 ? 20 : 10) ),
		(np->rv_scntl0 & 0xa)	? ", Parity Checking"	: ", NO Parity",
		(np->rv_stest2 & 0x20)	? ", Differential"	: "");

	if (bootverbose > 1) {
		printk (KERN_INFO "%s: initial SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			ncr_name(np), np->sv_scntl3, np->sv_dmode, np->sv_dcntl,
			np->sv_ctest3, np->sv_ctest4, np->sv_ctest5);

		printk (KERN_INFO "%s: final   SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			ncr_name(np), np->rv_scntl3, np->rv_dmode, np->rv_dcntl,
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	if (bootverbose && np->base2_ba)
		printk (KERN_INFO "%s: on-chip RAM at 0x%lx\n",
			ncr_name(np), np->base2_ba);

	return 0;
}


#ifdef SCSI_NCR_DEBUG_NVRAM

void __init ncr_display_Symbios_nvram(ncb_p np, Symbios_nvram *nvram)
{
	int i;

	/* display Symbios nvram host data */
	printk(KERN_DEBUG "%s: HOST ID=%d%s%s%s%s%s\n",
		ncr_name(np), nvram->host_id & 0x0f,
		(nvram->flags  & SYMBIOS_SCAM_ENABLE)	? " SCAM"	:"",
		(nvram->flags  & SYMBIOS_PARITY_ENABLE)	? " PARITY"	:"",
		(nvram->flags  & SYMBIOS_VERBOSE_MSGS)	? " VERBOSE"	:"", 
		(nvram->flags  & SYMBIOS_CHS_MAPPING)	? " CHS_ALT"	:"", 
		(nvram->flags1 & SYMBIOS_SCAN_HI_LO)	? " HI_LO"	:"");

	/* display Symbios nvram drive data */
	for (i = 0 ; i < 15 ; i++) {
		struct Symbios_target *tn = &nvram->target[i];
		printk(KERN_DEBUG "%s-%d:%s%s%s%s WIDTH=%d SYNC=%d TMO=%d\n",
		ncr_name(np), i,
		(tn->flags & SYMBIOS_DISCONNECT_ENABLE)	? " DISC"	: "",
		(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME)	? " SCAN_BOOT"	: "",
		(tn->flags & SYMBIOS_SCAN_LUNS)		? " SCAN_LUNS"	: "",
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? " TCQ"	: "",
		tn->bus_width,
		tn->sync_period / 4,
		tn->timeout);
	}
}

static u_char Tekram_boot_delay[7] __initdata = {3, 5, 10, 20, 30, 60, 120};

void __init ncr_display_Tekram_nvram(ncb_p np, Tekram_nvram *nvram)
{
	int i, tags, boot_delay;
	char *rem;

	/* display Tekram nvram host data */
	tags = 2 << nvram->max_tags_index;
	boot_delay = 0;
	if (nvram->boot_delay_index < 6)
		boot_delay = Tekram_boot_delay[nvram->boot_delay_index];
	switch((nvram->flags & TEKRAM_REMOVABLE_FLAGS) >> 6) {
	default:
	case 0:	rem = "";			break;
	case 1: rem = " REMOVABLE=boot device";	break;
	case 2: rem = " REMOVABLE=all";		break;
	}

	printk(KERN_DEBUG
		"%s: HOST ID=%d%s%s%s%s%s%s%s%s%s BOOT DELAY=%d tags=%d\n",
		ncr_name(np), nvram->host_id & 0x0f,
		(nvram->flags1 & SYMBIOS_SCAM_ENABLE)	? " SCAM"	:"",
		(nvram->flags & TEKRAM_MORE_THAN_2_DRIVES) ? " >2DRIVES"	:"",
		(nvram->flags & TEKRAM_DRIVES_SUP_1GB)	? " >1GB"	:"",
		(nvram->flags & TEKRAM_RESET_ON_POWER_ON) ? " RESET"	:"",
		(nvram->flags & TEKRAM_ACTIVE_NEGATION)	? " ACT_NEG"	:"",
		(nvram->flags & TEKRAM_IMMEDIATE_SEEK)	? " IMM_SEEK"	:"",
		(nvram->flags & TEKRAM_SCAN_LUNS)	? " SCAN_LUNS"	:"",
		(nvram->flags1 & TEKRAM_F2_F6_ENABLED)	? " F2_F6"	:"",
		rem, boot_delay, tags);

	/* display Tekram nvram drive data */
	for (i = 0; i <= 15; i++) {
		int sync, j;
		struct Tekram_target *tn = &nvram->target[i];
		j = tn->sync_index & 0xf;
		sync = Tekram_sync[j];
		printk(KERN_DEBUG "%s-%d:%s%s%s%s%s%s PERIOD=%d\n",
		ncr_name(np), i,
		(tn->flags & TEKRAM_PARITY_CHECK)	? " PARITY"	: "",
		(tn->flags & TEKRAM_SYNC_NEGO)		? " SYNC"	: "",
		(tn->flags & TEKRAM_DISCONNECT_ENABLE)	? " DISC"	: "",
		(tn->flags & TEKRAM_START_CMD)		? " START"	: "",
		(tn->flags & TEKRAM_TAGGED_COMMANDS)	? " TCQ"	: "",
		(tn->flags & TEKRAM_WIDE_NEGO)		? " WIDE"	: "",
		sync);
	}
}
#endif /* SCSI_NCR_DEBUG_NVRAM */

/*
**	Host attach and initialisations.
**
**	Allocate host data and ncb structure.
**	Request IO region and remap MMIO region.
**	Do chip initialization.
**	If all is OK, install interrupt handling and
**	start the timer daemon.
*/

static int __init 
ncr_attach (Scsi_Host_Template *tpnt, int unit, ncr_device *device)
{
        struct host_data *host_data;
	ncb_p np = 0;
        struct Scsi_Host *instance = 0;
	u_long flags = 0;
	ncr_nvram *nvram = device->nvram;
	int i;

	printk(KERN_INFO NAME53C "%s-%d: rev 0x%x on pci bus %d device %d function %d "
#ifdef __sparc__
		"irq %s\n",
#else
		"irq %d\n",
#endif
		device->chip.name, unit, device->chip.revision_id,
		device->slot.bus, (device->slot.device_fn & 0xf8) >> 3,
		device->slot.device_fn & 7,
#ifdef __sparc__
		__irq_itoa(device->slot.irq));
#else
		device->slot.irq);
#endif

	/*
	**	Allocate host_data structure
	*/
        if (!(instance = scsi_register(tpnt, sizeof(*host_data))))
	        goto attach_error;
	host_data = (struct host_data *) instance->hostdata;

	/*
	**	Allocate the host control block.
	*/
	np = __m_calloc_dma(device->pdev, sizeof(struct ncb), "NCB");
	if (!np)
		goto attach_error;
	NCR_INIT_LOCK_NCB(np);
	np->pdev  = device->pdev;
	np->p_ncb = vtobus(np);
	host_data->ncb = np;

	/*
	**	Store input informations in the host data structure.
	*/
	strncpy(np->chip_name, device->chip.name, sizeof(np->chip_name) - 1);
	np->unit	= unit;
	np->verbose	= driver_setup.verbose;
	sprintf(np->inst_name, NAME53C "%s-%d", np->chip_name, np->unit);
	np->device_id	= device->chip.device_id;
	np->revision_id	= device->chip.revision_id;
	np->bus		= device->slot.bus;
	np->device_fn	= device->slot.device_fn;
	np->features	= device->chip.features;
	np->clock_divn	= device->chip.nr_divisor;
	np->maxoffs	= device->chip.offset_max;
	np->maxburst	= device->chip.burst_max;
	np->myaddr	= device->host_id;

	/*
	**	Allocate the start queue.
	*/
	np->squeue = (ncrcmd *)
		m_calloc_dma(sizeof(ncrcmd)*(MAX_START*2), "SQUEUE");
	if (!np->squeue)
		goto attach_error;
	np->p_squeue = vtobus(np->squeue);

	/*
	**	Allocate the done queue.
	*/
	np->dqueue = (ncrcmd *)
		m_calloc_dma(sizeof(ncrcmd)*(MAX_START*2), "DQUEUE");
	if (!np->dqueue)
		goto attach_error;

	/*
	**	Allocate the target bus address array.
	*/
	np->targtbl = (u_int32 *) m_calloc_dma(256, "TARGTBL");
	if (!np->targtbl)
		goto attach_error;

	/*
	**	Allocate SCRIPTS areas
	*/
	np->script0	= (struct script *) 
		m_calloc_dma(sizeof(struct script),  "SCRIPT");
	if (!np->script0)
		goto attach_error;
	np->scripth0	= (struct scripth *)
		m_calloc_dma(sizeof(struct scripth), "SCRIPTH");
	if (!np->scripth0)
		goto attach_error;

	/*
	**	Initialyze the CCB free queue and,
	**	allocate some CCB. We need at least ONE.
	*/
	xpt_que_init(&np->free_ccbq);
	xpt_que_init(&np->b0_ccbq);
	if (!ncr_alloc_ccb(np))
		goto attach_error;

	/*
	**    Initialize timer structure
        **
        */
	init_timer(&np->timer);
	np->timer.data     = (unsigned long) np;
	np->timer.function = sym53c8xx_timeout;

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	np->base_ba	= device->slot.base;
	np->base_ws	= (np->features & FE_IO256)? 256 : 128;
	np->base2_ba	= (np->features & FE_RAM)? device->slot.base_2 : 0;

#ifndef SCSI_NCR_IOMAPPED
	np->base_va = remap_pci_mem(device->slot.base_c, np->base_ws);
	if (!np->base_va) {
		printk(KERN_ERR "%s: can't map PCI MMIO region\n",ncr_name(np));
		goto attach_error;
	}
	else if (bootverbose > 1)
		printk(KERN_INFO "%s: using memory mapped IO\n", ncr_name(np));

	/*
	**	Make the controller's registers available.
	**	Now the INB INW INL OUTB OUTW OUTL macros
	**	can be used safely.
	*/

	np->reg = (struct ncr_reg *) np->base_va;

#endif /* !defined SCSI_NCR_IOMAPPED */

	/*
	**	If on-chip RAM is used, make sure SCRIPTS isn't too large.
	*/
	if (np->base2_ba && sizeof(struct script) > 4096) {
		printk(KERN_ERR "%s: script too large.\n", ncr_name(np));
		goto attach_error;
	}

	/*
	**	Try to map the controller chip into iospace.
	*/

	if (device->slot.io_port) {
		request_region(device->slot.io_port, np->base_ws, NAME53C8XX);
		np->base_io = device->slot.io_port;
	}

#ifdef SCSI_NCR_NVRAM_SUPPORT
	if (nvram) {
		switch(nvram->type) {
		case SCSI_NCR_SYMBIOS_NVRAM:
#ifdef SCSI_NCR_DEBUG_NVRAM
			ncr_display_Symbios_nvram(np, &nvram->data.Symbios);
#endif
			break;
		case SCSI_NCR_TEKRAM_NVRAM:
#ifdef SCSI_NCR_DEBUG_NVRAM
			ncr_display_Tekram_nvram(np, &nvram->data.Tekram);
#endif
			break;
		default:
			nvram = 0;
#ifdef SCSI_NCR_DEBUG_NVRAM
			printk(KERN_DEBUG "%s: NVRAM: None or invalid data.\n", ncr_name(np));
#endif
		}
	}
#endif

 	/*
	**	Save setting of some IO registers, so we will 
	**	be able to probe specific implementations.
	*/
	ncr_save_initial_setting (np);

	/*
	**	Reset the chip now, since it has been reported 
	**	that SCSI clock calibration may not work properly 
	**	if the chip is currently active.
	*/
	ncr_chip_reset (np);

	/*
	**	Do chip dependent initialization.
	*/
	(void) ncr_prepare_setting(np, nvram);

	/*
	**	Check the PCI clock frequency if needed.
	**	
	**	Must be done after ncr_prepare_setting since it destroys 
	**	STEST1 that is used to probe for the clock multiplier.
	**
	**	The range is currently [22688 - 45375 Khz], given 
	**	the values used by ncr_getclock().
	**	This calibration of the frequecy measurement 
	**	algorithm against the PCI clock frequency is only 
	**	performed if the driver has had to measure the SCSI 
	**	clock due to other heuristics not having been enough 
	**	to deduce the SCSI clock frequency.
	**
	**	When the chip has been initialized correctly by the 
	**	SCSI BIOS, the driver deduces the presence of the 
	**	clock multiplier and the value of the SCSI clock from 
	**	initial values of IO registers, and therefore no 
	**	clock measurement is performed.
	**	Normally the driver should never have to measure any 
	**	clock, unless the controller may use a 80 MHz clock 
	**	or has a clock multiplier and any of the following 
	**	condition is met:
	**
	**	- No SCSI BIOS is present.
	**	- SCSI BIOS did'nt enable the multiplier for some reason.
	**	- User has disabled the controller from the SCSI BIOS.
	**	- User booted the O/S from another O/S that did'nt enable 
	**	  the multiplier for some reason.
	**
	**	As a result, the driver may only have to measure some 
	**	frequency in very unusual situations.
	**
	**	For this reality test against the PCI clock to really 
	**	protect against flaws in the udelay() calibration or 
	**	driver problem that affect the clock measurement 
	**	algorithm, the actual PCI clock frequency must be 33 MHz.
	*/
	i = np->pciclock_max ? ncr_getpciclock(np) : 0;
	if (i && (i < np->pciclock_min  || i > np->pciclock_max)) {
		printk(KERN_ERR "%s: PCI clock (%u KHz) is out of range "
			"[%u KHz - %u KHz].\n",
		       ncr_name(np), i, np->pciclock_min, np->pciclock_max);
		goto attach_error;
	}

	/*
	**	Patch script to physical addresses
	*/
	ncr_script_fill (&script0, &scripth0);

	np->p_script	= vtobus(np->script0);
	np->p_scripth	= vtobus(np->scripth0);
	np->p_scripth0	= np->p_scripth;

	if (np->base2_ba) {
		np->p_script	= np->base2_ba;
		if (np->features & FE_RAM8K) {
			np->base2_ws = 8192;
			np->p_scripth = np->p_script + 4096;
#if BITS_PER_LONG > 32
			np->scr_ram_seg = cpu_to_scr(np->base2_ba >> 32);
#endif
		}
		else
			np->base2_ws = 4096;
#ifndef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
		np->base2_va = 
			remap_pci_mem(device->slot.base_2_c, np->base2_ws);
		if (!np->base2_va) {
			printk(KERN_ERR "%s: can't map PCI MEMORY region\n",
			       ncr_name(np));
			goto attach_error;
		}
#endif
	}

	ncr_script_copy_and_bind (np, (ncrcmd *) &script0, (ncrcmd *) np->script0, sizeof(struct script));
	ncr_script_copy_and_bind (np, (ncrcmd *) &scripth0, (ncrcmd *) np->scripth0, sizeof(struct scripth));

	/*
	**	Patch some variables in SCRIPTS
	*/
	np->scripth0->pm0_data_addr[0] = 
			cpu_to_scr(NCB_SCRIPT_PHYS(np, pm0_data));
	np->scripth0->pm1_data_addr[0] = 
			cpu_to_scr(NCB_SCRIPT_PHYS(np, pm1_data));

	/*
	**	Patch if not Ultra 3 - Do not write to scntl4
	*/
	if (np->features & FE_ULTRA3) {
		np->script0->resel_scntl4[0] = cpu_to_scr(SCR_LOAD_REL (scntl4, 1));
		np->script0->resel_scntl4[1] = cpu_to_scr(offsetof(struct tcb, uval));
	}


#ifdef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
	np->scripth0->script0_ba[0]	= cpu_to_scr(vtobus(np->script0));
	np->scripth0->script0_ba64[0]	= cpu_to_scr(vtobus(np->script0));
	np->scripth0->scripth0_ba64[0]	= cpu_to_scr(vtobus(np->scripth0));
	np->scripth0->ram_seg64[0]	= np->scr_ram_seg;
#endif
	/*
	**	Prepare the idle and invalid task actions.
	*/
	np->idletask.start	= cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	np->idletask.restart	= cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_i_t_l));
	np->p_idletask		= NCB_PHYS(np, idletask);

	np->notask.start	= cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	np->notask.restart	= cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_i_t_l));
	np->p_notask		= NCB_PHYS(np, notask);

	np->bad_i_t_l.start	= cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	np->bad_i_t_l.restart	= cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_i_t_l));
	np->p_bad_i_t_l		= NCB_PHYS(np, bad_i_t_l);

	np->bad_i_t_l_q.start	= cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	np->bad_i_t_l_q.restart	= cpu_to_scr(NCB_SCRIPTH_PHYS (np,bad_i_t_l_q));
	np->p_bad_i_t_l_q	= NCB_PHYS(np, bad_i_t_l_q);

	/*
	**	Allocate and prepare the bad lun table.
	*/
	np->badluntbl = m_calloc_dma(256, "BADLUNTBL");
	if (!np->badluntbl)
		goto attach_error;

	assert (offsetof(struct lcb, resel_task) == 0);
	np->resel_badlun = cpu_to_scr(NCB_SCRIPTH_PHYS(np, resel_bad_lun));

	for (i = 0 ; i < 64 ; i++)
		np->badluntbl[i] = cpu_to_scr(NCB_PHYS(np, resel_badlun));

	/*
	**	Prepare the target bus address array.
	*/
	np->scripth0->targtbl[0] = cpu_to_scr(vtobus(np->targtbl));
	for (i = 0 ; i < MAX_TARGET ; i++) {
		np->targtbl[i] = cpu_to_scr(NCB_PHYS(np, target[i]));
		np->target[i].b_luntbl = cpu_to_scr(vtobus(np->badluntbl));
		np->target[i].b_lun0   = cpu_to_scr(NCB_PHYS(np, resel_badlun));
	}

	/*
	**    Patch the script for LED support.
	*/

	if (np->features & FE_LED0) {
		np->script0->idle[0]  =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_OR,  0x01));
		np->script0->reselected[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
		np->script0->start[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
	}

	/*
	**	Patch the script to provide an extra clock cycle on
	**	data out phase - 53C1010_66MHz part only.
	**	(Fixed in rev. 1 of the chip)
	*/
	if (np->device_id == PCI_DEVICE_ID_LSI_53C1010_66 &&
	    np->revision_id < 1){
		np->script0->datao_phase[0] =
				cpu_to_scr(SCR_REG_REG(scntl4, SCR_OR, 0x0c));
	}

#ifdef SCSI_NCR_IARB_SUPPORT
	/*
	**    If user does not want to use IMMEDIATE ARBITRATION
	**    when we are reselected while attempting to arbitrate,
	**    patch the SCRIPTS accordingly with a SCRIPT NO_OP.
	*/
	if (!(driver_setup.iarb & 1))
		np->script0->ungetjob[0] = cpu_to_scr(SCR_NO_OP);
	/*
	**    If user wants IARB to be set when we win arbitration 
	**    and have other jobs, compute the max number of consecutive 
	**    settings of IARB hint before we leave devices a chance to 
	**    arbitrate for reselection.
	*/
	np->iarb_max = (driver_setup.iarb >> 4);
#endif

	/*
	**	DEL 472 - 53C896 Rev 1 - Part Number 609-0393055 - ITEM 5.
	*/
	if (np->device_id == PCI_DEVICE_ID_NCR_53C896 &&
	    np->revision_id <= 0x1 && (np->features & FE_NOPM)) {
		np->scatter = ncr_scatter_896R1;
		np->script0->datai_phase[0] = cpu_to_scr(SCR_JUMP);
		np->script0->datai_phase[1] = 
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, tweak_pmj));
		np->script0->datao_phase[0] = cpu_to_scr(SCR_JUMP);
		np->script0->datao_phase[1] = 
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, tweak_pmj));
	}
	else
#ifdef DEBUG_896R1
		np->scatter = ncr_scatter_896R1;
#else
		np->scatter = ncr_scatter;
#endif

	/*
	**	Reset chip.
	**	We should use ncr_soft_reset(), but we donnot want to do 
	**	so, since we may not be safe if ABRT interrupt occurs due 
	**	to the BIOS or previous O/S having enable this interrupt.
	**
	**	For C1010 need to set ABRT bit prior to SRST if SCRIPTs
	**	are running. Not true in this case.
	*/
	ncr_chip_reset(np);

	/*
	**	Now check the cache handling of the pci chipset.
	*/

	if (ncr_snooptest (np)) {
		printk (KERN_ERR "CACHE INCORRECTLY CONFIGURED.\n");
		goto attach_error;
	};

	/*
	**	Install the interrupt handler.
	**	If we synchonize the C code with SCRIPTS on interrupt, 
	**	we donnot want to share the INTR line at all.
	*/
	if (request_irq(device->slot.irq, sym53c8xx_intr,
#ifdef SCSI_NCR_PCIQ_SYNC_ON_INTR
			((driver_setup.irqm & 0x20) ? 0 : SA_INTERRUPT),
#else
			((driver_setup.irqm & 0x10) ? 0 : SA_SHIRQ) |
#if LINUX_VERSION_CODE < LinuxVersionCode(2,2,0)
			((driver_setup.irqm & 0x20) ? 0 : SA_INTERRUPT),
#else
			0,
#endif
#endif
			NAME53C8XX, np)) {
		printk(KERN_ERR "%s: request irq %d failure\n",
			ncr_name(np), device->slot.irq);
		goto attach_error;
	}
	np->irq = device->slot.irq;

	/*
	**	After SCSI devices have been opened, we cannot
	**	reset the bus safely, so we do it here.
	**	Interrupt handler does the real work.
	**	Process the reset exception,
	**	if interrupts are not enabled yet.
	**	Then enable disconnects.
	*/
	NCR_LOCK_NCB(np, flags);
	if (ncr_reset_scsi_bus(np, 0, driver_setup.settle_delay) != 0) {
		printk(KERN_ERR "%s: FATAL ERROR: CHECK SCSI BUS - CABLES, TERMINATION, DEVICE POWER etc.!\n", ncr_name(np));

		NCR_UNLOCK_NCB(np, flags);
		goto attach_error;
	}
	ncr_exception (np);

	/*
	**	The middle-level SCSI driver does not
	**	wait for devices to settle.
	**	Wait synchronously if more than 2 seconds.
	*/
	if (driver_setup.settle_delay > 2) {
		printk(KERN_INFO "%s: waiting %d seconds for scsi devices to settle...\n",
			ncr_name(np), driver_setup.settle_delay);
		MDELAY (1000 * driver_setup.settle_delay);
	}

	/*
	**	start the timeout daemon
	*/
	np->lasttime=0;
	ncr_timeout (np);

	/*
	**  use SIMPLE TAG messages by default
	*/
#ifdef SCSI_NCR_ALWAYS_SIMPLE_TAG
	np->order = M_SIMPLE_TAG;
#endif

	/*
	**  Done.
	*/
        if (!first_host)
        	first_host = instance;

	/*
	**	Fill Linux host instance structure
	**	and return success.
	*/
	instance->max_channel	= 0;
	instance->this_id	= np->myaddr;
	instance->max_id	= np->maxwide ? 16 : 8;
	instance->max_lun	= MAX_LUN;
#ifndef SCSI_NCR_IOMAPPED
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,29)
	instance->base		= (unsigned long) np->reg;
#else
	instance->base		= (char *) np->reg;
#endif
#endif
	instance->irq		= np->irq;
	instance->unique_id	= np->base_io;
	instance->io_port	= np->base_io;
	instance->n_io_port	= np->base_ws;
	instance->dma_channel	= 0;
	instance->cmd_per_lun	= MAX_TAGS;
	instance->can_queue	= (MAX_START-4);
	scsi_set_pci_device(instance, device->pdev);

	np->check_integrity       = 0;

#ifdef	SCSI_NCR_INTEGRITY_CHECKING
	instance->check_integrity = 0;

#ifdef SCSI_NCR_ENABLE_INTEGRITY_CHECK
	if ( !(driver_setup.bus_check & 0x04) ) {
		np->check_integrity       = 1;
		instance->check_integrity = 1;
	}
#endif
#endif
	
	instance->select_queue_depths = sym53c8xx_select_queue_depths;

	NCR_UNLOCK_NCB(np, flags);

	/*
	**	Now let the generic SCSI driver
	**	look for the SCSI devices on the bus ..
	*/
	return 0;

attach_error:
	if (!instance) return -1;
	printk(KERN_INFO "%s: giving up ...\n", ncr_name(np));
	if (np)
		ncr_free_resources(np);
	scsi_unregister(instance);

        return -1;
 }


/*
**	Free controller resources.
*/
static void ncr_free_resources(ncb_p np)
{
	ccb_p cp;
	tcb_p tp;
	lcb_p lp;
	int target, lun;

	if (np->irq)
		free_irq(np->irq, np);
	if (np->base_io)
		release_region(np->base_io, np->base_ws);
#ifndef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
	if (np->base_va)
		unmap_pci_mem(np->base_va, np->base_ws);
	if (np->base2_va)
		unmap_pci_mem(np->base2_va, np->base2_ws);
#endif
	if (np->scripth0)
		m_free_dma(np->scripth0, sizeof(struct scripth), "SCRIPTH");
	if (np->script0)
		m_free_dma(np->script0, sizeof(struct script), "SCRIPT");
	if (np->squeue)
		m_free_dma(np->squeue, sizeof(ncrcmd)*(MAX_START*2), "SQUEUE");
	if (np->dqueue)
		m_free_dma(np->dqueue, sizeof(ncrcmd)*(MAX_START*2),"DQUEUE");

	while ((cp = np->ccbc) != NULL) {
		np->ccbc = cp->link_ccb;
		m_free_dma(cp, sizeof(*cp), "CCB");
	}

	if (np->badluntbl)
		m_free_dma(np->badluntbl, 256,"BADLUNTBL");

	for (target = 0; target < MAX_TARGET ; target++) {
		tp = &np->target[target];
		for (lun = 0 ; lun < MAX_LUN ; lun++) {
			lp = ncr_lp(np, tp, lun);
			if (!lp)
				continue;
			if (lp->tasktbl != &lp->tasktbl_0)
				m_free_dma(lp->tasktbl, MAX_TASKS*4, "TASKTBL");
			if (lp->cb_tags)
				m_free(lp->cb_tags, MAX_TAGS, "CB_TAGS");
			m_free_dma(lp, sizeof(*lp), "LCB");
		}
#if MAX_LUN > 1
		if (tp->lmp)
			m_free(tp->lmp, MAX_LUN * sizeof(lcb_p), "LMP");
		if (tp->luntbl)
			m_free_dma(tp->luntbl, 256, "LUNTBL");
#endif 
	}

	if (np->targtbl)
		m_free_dma(np->targtbl, 256, "TARGTBL");

	m_free_dma(np, sizeof(*np), "NCB");
}


/*==========================================================
**
**
**	Done SCSI commands list management.
**
**	We donnot enter the scsi_done() callback immediately 
**	after a command has been seen as completed but we 
**	insert it into a list which is flushed outside any kind 
**	of driver critical section.
**	This allows to do minimal stuff under interrupt and 
**	inside critical sections and to also avoid locking up 
**	on recursive calls to driver entry points under SMP.
**	In fact, the only kernel point which is entered by the 
**	driver with a driver lock set is get_free_pages(GFP_ATOMIC...) 
**	that shall not reenter the driver under any circumstance.
**
**==========================================================
*/
static inline void ncr_queue_done_cmd(ncb_p np, Scsi_Cmnd *cmd)
{
	unmap_scsi_data(np, cmd);
	cmd->host_scribble = (char *) np->done_list;
	np->done_list = cmd;
}

static inline void ncr_flush_done_cmds(Scsi_Cmnd *lcmd)
{
	Scsi_Cmnd *cmd;

	while (lcmd) {
		cmd = lcmd;
		lcmd = (Scsi_Cmnd *) cmd->host_scribble;
		cmd->scsi_done(cmd);
	}
}

/*==========================================================
**
**
**	Prepare the next negotiation message for integrity check,
**	if needed.
**
**	Fill in the part of message buffer that contains the 
**	negotiation and the nego_status field of the CCB.
**	Returns the size of the message in bytes.
**
**	If tp->ppr_negotiation is 1 and a M_REJECT occurs, then
**	we disable ppr_negotiation.  If the first ppr_negotiation is
**	successful, set this flag to 2.
**
**==========================================================
*/
#ifdef	SCSI_NCR_INTEGRITY_CHECKING
static int ncr_ic_nego(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd, u_char *msgptr)
{
	tcb_p tp = &np->target[cp->target];
	int msglen = 0;
	int nego = 0;
	u_char new_width, new_offset, new_period;
	u_char no_increase;

	if (tp->ppr_negotiation == 1)	/* PPR message successful */
		tp->ppr_negotiation = 2;

	if (tp->inq_done) {

		if (!tp->ic_maximums_set) {
			tp->ic_maximums_set = 1;

			/* 
			 * Check against target, host and user limits  
			 */
			if ( (tp->inq_byte7 & INQ7_WIDE16) && 
					np->maxwide  && tp->usrwide) 
				tp->ic_max_width = 1;
			else
				tp->ic_max_width = 0;
			

			if ((tp->inq_byte7 & INQ7_SYNC) && tp->maxoffs)
				tp->ic_min_sync = (tp->minsync < np->minsync) ?
							np->minsync : tp->minsync;
			else 
				tp->ic_min_sync = 255;
			
			tp->period   = 1;
			tp->widedone = 1;

			/*
			 * Enable PPR negotiation - only if Ultra3 support
			 * is accessible.
			 */

#if 0
			if (tp->ic_max_width && (tp->ic_min_sync != 255 ))
				tp->ppr_negotiation = 1;
#endif
			tp->ppr_negotiation = 0;
			if (np->features & FE_ULTRA3) {
			    if (tp->ic_max_width && (tp->ic_min_sync == 0x09))
				tp->ppr_negotiation = 1;
			}

			if (!tp->ppr_negotiation)
				cmd->ic_nego &= ~NS_PPR;
		}

		if (DEBUG_FLAGS & DEBUG_IC) {
			printk("%s: cmd->ic_nego %d, 1st byte 0x%2X\n",
				ncr_name(np), cmd->ic_nego, cmd->cmnd[0]);
		}

		/* Previous command recorded a parity or an initiator
		 * detected error condition. Force bus to narrow for this
		 * target. Clear flag. Negotation on request sense.
		 * Note: kernel forces 2 bus resets :o( but clears itself out. 
		 * Minor bug? in scsi_obsolete.c (ugly)
		 */
		if (np->check_integ_par) { 
			printk("%s: Parity Error. Target set to narrow.\n",
				ncr_name(np));
			tp->ic_max_width = 0;
			tp->widedone = tp->period = 0;
		}

		/* Initializing:
		 * If ic_nego == NS_PPR, we are in the initial test for
		 * PPR messaging support. If driver flag is clear, then
		 * either we don't support PPR nego (narrow or async device)
		 * or this is the second TUR and we have had a M. REJECT 
		 * or unexpected disconnect on the first PPR negotiation.  
		 * Do not negotiate, reset nego flags (in case a reset has
		 * occurred), clear ic_nego and return.
		 * General case: Kernel will clear flag on a fallback. 
		 * Do only SDTR or WDTR in the future.
		 */
                if (!tp->ppr_negotiation &&  (cmd->ic_nego == NS_PPR )) {
			tp->ppr_negotiation = 0;
			cmd->ic_nego &= ~NS_PPR;
			tp->widedone = tp->period = 1;
			return msglen;
		}
		else if (( tp->ppr_negotiation && !(cmd->ic_nego & NS_PPR )) || 
                        (!tp->ppr_negotiation &&  (cmd->ic_nego & NS_PPR )) ) {
			tp->ppr_negotiation = 0;
			cmd->ic_nego &= ~NS_PPR;
		}

		/*
		 * Always check the PPR nego. flag bit if ppr_negotiation
		 * is set.  If the ic_nego PPR bit is clear,
		 * there must have been a fallback. Do only
		 * WDTR / SDTR in the future.
		 */
		if ((tp->ppr_negotiation) && (!(cmd->ic_nego & NS_PPR)))
			tp->ppr_negotiation = 0;

		/* In case of a bus reset, ncr_negotiate will reset 
                 * the flags tp->widedone and tp->period to 0, forcing
		 * a new negotiation.  Do WDTR then SDTR. If PPR, do both.
		 * Do NOT increase the period.  It is possible for the Scsi_Cmnd
		 * flags to be set to increase the period when a bus reset 
		 * occurs - we don't want to change anything.
		 */

		no_increase = 0;

		if (tp->ppr_negotiation && (!tp->widedone) && (!tp->period) ) {
			cmd->ic_nego = NS_PPR;
			tp->widedone = tp->period = 1;
			no_increase = 1;
		}
		else if (!tp->widedone) {
			cmd->ic_nego = NS_WIDE;
			tp->widedone = 1;
			no_increase = 1;
		}
		else if (!tp->period) {
			cmd->ic_nego = NS_SYNC;
			tp->period = 1;
			no_increase = 1;
		}

		new_width = cmd->ic_nego_width & tp->ic_max_width;

		switch (cmd->ic_nego_sync) {
		case 2: /* increase the period */
			if (!no_increase) {
			    if (tp->ic_min_sync <= 0x09)      
				tp->ic_min_sync = 0x0A;
			    else if (tp->ic_min_sync <= 0x0A) 
				tp->ic_min_sync = 0x0C;
			    else if (tp->ic_min_sync <= 0x0C) 
				tp->ic_min_sync = 0x19;
			    else if (tp->ic_min_sync <= 0x19) 
				tp->ic_min_sync *= 2;
			    else  {
				tp->ic_min_sync = 255;
				cmd->ic_nego_sync = 0;
				tp->maxoffs = 0;
			    }
			}
			new_period  = tp->maxoffs?tp->ic_min_sync:0;
			new_offset  = tp->maxoffs;
			break;

		case 1: /* nego. to maximum */
			new_period  = tp->maxoffs?tp->ic_min_sync:0;
			new_offset  = tp->maxoffs;
			break;

		case 0:	/* nego to async */
		default:
			new_period = 0;
			new_offset = 0;
			break;
		};
		

		nego = NS_NOCHANGE;
		if (tp->ppr_negotiation) { 
			u_char options_byte = 0;

			/*
			** Must make sure data is consistent.
			** If period is 9 and sync, must be wide and DT bit set.
			** else period must be larger. If the width is 0, 
			** reset bus to wide but increase the period to 0x0A.
			** Note: The strange else clause is due to the integrity check.
			** If fails at 0x09, wide, the I.C. code will redo at the same
			** speed but a narrow bus. The driver must take care of slowing
			** the bus speed down.
			**
			** The maximum offset in ST mode is 31, in DT mode 62 (1010/1010_66 only)
			*/
			if ( (new_period==0x09) && new_offset) {
				if (new_width) 
					options_byte = 0x02;
				else {
					tp->ic_min_sync = 0x0A;
					new_period = 0x0A;
					cmd->ic_nego_width = 1;
					new_width = 1;
				}
			}
			if (!options_byte && new_offset > np->maxoffs_st)
				new_offset = np->maxoffs_st;

			nego = NS_PPR;
			
			msgptr[msglen++] = M_EXTENDED;
			msgptr[msglen++] = 6;
			msgptr[msglen++] = M_X_PPR_REQ;
			msgptr[msglen++] = new_period;
			msgptr[msglen++] = 0;
			msgptr[msglen++] = new_offset;
			msgptr[msglen++] = new_width;
			msgptr[msglen++] = options_byte;

		}
		else {
			switch (cmd->ic_nego & ~NS_PPR) {
			case NS_WIDE:
			    /*
			    **	WDTR negotiation on if device supports
			    **  wide or if wide device forced narrow
			    **	due to a parity error. 
			    */

			    cmd->ic_nego_width &= tp->ic_max_width;

			    if (tp->ic_max_width | np->check_integ_par) {
				nego = NS_WIDE;
				msgptr[msglen++] = M_EXTENDED;
				msgptr[msglen++] = 2;
				msgptr[msglen++] = M_X_WIDE_REQ;
				msgptr[msglen++] = new_width;
			    }
		 	    break;

			case NS_SYNC:
			    /*
			    **	negotiate synchronous transfers
			    **	Target must support sync transfers.
			    **  Min. period = 0x0A, maximum offset of 31=0x1f.
		    	    */

			    if (tp->inq_byte7 & INQ7_SYNC) {

				if (new_offset && (new_period < 0x0A)) {
					tp->ic_min_sync = 0x0A;
					new_period = 0x0A;
				}
				if (new_offset > np->maxoffs_st)
					new_offset = np->maxoffs_st;
				nego = NS_SYNC;
				msgptr[msglen++] = M_EXTENDED;
				msgptr[msglen++] = 3;
				msgptr[msglen++] = M_X_SYNC_REQ;
				msgptr[msglen++] = new_period;
				msgptr[msglen++] = new_offset;
			    }
			    else 
				cmd->ic_nego_sync = 0;
			    break;

			case NS_NOCHANGE:
			    break;
			}
		}

	};

	cp->nego_status = nego;
	np->check_integ_par = 0;

	if (nego) {
		tp->nego_cp = cp;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, nego == NS_WIDE ?
				  "wide/narrow msgout":
				(nego == NS_SYNC ? "sync/async msgout" : "ppr msgout"), 
				msgptr);
		};
	};

	return msglen;
}
#endif	/* SCSI_NCR_INTEGRITY_CHECKING */

/*==========================================================
**
**
**	Prepare the next negotiation message if needed.
**
**	Fill in the part of message buffer that contains the 
**	negotiation and the nego_status field of the CCB.
**	Returns the size of the message in bytes.
**
**
**==========================================================
*/


static int ncr_prepare_nego(ncb_p np, ccb_p cp, u_char *msgptr)
{
	tcb_p tp = &np->target[cp->target];
	int msglen = 0;
	int nego = 0;
	u_char width, offset, factor, last_byte;

	if (!np->check_integrity) {
		/* If integrity checking disabled, enable PPR messaging
		 * if device supports wide, sync and ultra 3
		 */
		if (tp->ppr_negotiation == 1) /* PPR message successful */
			tp->ppr_negotiation = 2;

		if ((tp->inq_done) && (!tp->ic_maximums_set)) {
			tp->ic_maximums_set = 1;

			/*
			 * Issue PPR only if board is capable
			 * and set-up for Ultra3 transfers.
			 */
			tp->ppr_negotiation = 0;
			if ( (np->features & FE_ULTRA3) &&
				(tp->usrwide) && (tp->maxoffs) &&
				(tp->minsync == 0x09) )
					tp->ppr_negotiation = 1;
		}
	}

	if (tp->inq_done) {
		/*
		 * Get the current width, offset and period
		 */
		ncr_get_xfer_info( np, tp, &factor,
						&offset, &width);

		/*
		**	negotiate wide transfers ?
		*/

		if (!tp->widedone) {
			if (tp->inq_byte7 & INQ7_WIDE16) {
				if (tp->ppr_negotiation)
					nego = NS_PPR;
				else
					nego = NS_WIDE;

				width = tp->usrwide;
#ifdef	SCSI_NCR_INTEGRITY_CHECKING
				if (tp->ic_done)
		       			 width &= tp->ic_max_width;
#endif
			} else
				tp->widedone=1;

		};

		/*
		**	negotiate synchronous transfers?
		*/

		if ((nego != NS_WIDE) && !tp->period) {
			if (tp->inq_byte7 & INQ7_SYNC) {
				if (tp->ppr_negotiation)
					nego = NS_PPR;
				else
					nego = NS_SYNC;
				
				/* Check for async flag */
				if (tp->maxoffs == 0) {
				    offset = 0;
				    factor = 0;
				}
				else {
				    offset = tp->maxoffs;
				    factor = tp->minsync;
#ifdef	SCSI_NCR_INTEGRITY_CHECKING
			 	    if ((tp->ic_done) && 
						(factor < tp->ic_min_sync))
		       			 factor = tp->ic_min_sync;
#endif
				}

			} else {
				offset = 0;
				factor = 0;
				tp->period  =0xffff;
				PRINT_TARGET(np, cp->target);
				printk ("target did not report SYNC.\n");
			};
		};
	};

	switch (nego) {
	case NS_PPR:
		/*
		** Must make sure data is consistent.
		** If period is 9 and sync, must be wide and DT bit set
		** else period must be larger. 
		** Maximum offset is 31=0x1f is ST mode, 62 if DT mode
		*/
		last_byte = 0;
		if ( (factor==9) && offset) {
			if (!width) {
				factor = 0x0A;
			}
			else 
				last_byte = 0x02;
		}
		if (!last_byte && offset > np->maxoffs_st)
			offset = np->maxoffs_st;

		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 6;
		msgptr[msglen++] = M_X_PPR_REQ;
		msgptr[msglen++] = factor;
		msgptr[msglen++] = 0;
		msgptr[msglen++] = offset;
		msgptr[msglen++] = width;
		msgptr[msglen++] = last_byte;
		break;
	case NS_SYNC:
		/*
		** Never negotiate faster than Ultra 2 (25ns periods)
		*/
		if (offset && (factor < 0x0A)) {
			factor = 0x0A;
			tp->minsync = 0x0A;
		}
		if (offset > np->maxoffs_st)
			offset = np->maxoffs_st;

		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 3;
		msgptr[msglen++] = M_X_SYNC_REQ;
		msgptr[msglen++] = factor;
		msgptr[msglen++] = offset;
		break;
	case NS_WIDE:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 2;
		msgptr[msglen++] = M_X_WIDE_REQ;
		msgptr[msglen++] = width;
		break;
	};

	cp->nego_status = nego;

	if (nego) {
		tp->nego_cp = cp;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, nego == NS_WIDE ?
				  "wide msgout":
				(nego == NS_SYNC ? "sync msgout" : "ppr msgout"), 
				msgptr);
		};
	};

	return msglen;
}

/*==========================================================
**
**
**	Start execution of a SCSI command.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/
static int ncr_queue_command (ncb_p np, Scsi_Cmnd *cmd)
{
/*	Scsi_Device        *device    = cmd->device; */
	tcb_p tp                      = &np->target[cmd->target];
	lcb_p lp		      = ncr_lp(np, tp, cmd->lun);
	ccb_p cp;

	u_char	idmsg, *msgptr;
	u_int   msglen;
	int	direction;
	u_int32	lastp, goalp;

	/*---------------------------------------------
	**
	**      Some shortcuts ...
	**
	**---------------------------------------------
	*/
	if ((cmd->target == np->myaddr	  ) ||
		(cmd->target >= MAX_TARGET) ||
		(cmd->lun    >= MAX_LUN   )) {
		return(DID_BAD_TARGET);
        }

	/*---------------------------------------------
	**
	**	Complete the 1st TEST UNIT READY command
	**	with error condition if the device is 
	**	flagged NOSCAN, in order to speed up 
	**	the boot.
	**
	**---------------------------------------------
	*/
	if ((cmd->cmnd[0] == 0 || cmd->cmnd[0] == 0x12) && 
	    (tp->usrflag & UF_NOSCAN)) {
		tp->usrflag &= ~UF_NOSCAN;
		return DID_BAD_TARGET;
	}

	if (DEBUG_FLAGS & DEBUG_TINY) {
		PRINT_ADDR(cmd);
		printk ("CMD=%x ", cmd->cmnd[0]);
	}

	/*---------------------------------------------------
	**
	**	Assign a ccb / bind cmd.
	**	If resetting, shorten settle_time if necessary
	**	in order to avoid spurious timeouts.
	**	If resetting or no free ccb,
	**	insert cmd into the waiting list.
	**
	**----------------------------------------------------
	*/
	if (np->settle_time && cmd->timeout_per_command >= HZ) {
		u_long tlimit = ktime_get(cmd->timeout_per_command - HZ);
		if (ktime_dif(np->settle_time, tlimit) > 0)
			np->settle_time = tlimit;
	}

        if (np->settle_time || !(cp=ncr_get_ccb (np, cmd->target, cmd->lun))) {
		insert_into_waiting_list(np, cmd);
		return(DID_OK);
	}
	cp->cmd = cmd;

	/*---------------------------------------------------
	**
	**	Enable tagged queue if asked by scsi ioctl
	**
	**----------------------------------------------------
	*/
#if 0	/* This stuff was only useful for linux-1.2.13 */
	if (lp && !lp->numtags && cmd->device && cmd->device->tagged_queue) {
		lp->numtags = tp->usrtags;
		ncr_setup_tags (np, cp->target, cp->lun);
	}
#endif

	/*----------------------------------------------------
	**
	**	Build the identify / tag / sdtr message
	**
	**----------------------------------------------------
	*/

	idmsg = M_IDENTIFY | cp->lun;

	if (cp ->tag != NO_TAG || (lp && !(tp->usrflag & UF_NODISC)))
		idmsg |= 0x40;

	msgptr = cp->scsi_smsg;
	msglen = 0;
	msgptr[msglen++] = idmsg;

	if (cp->tag != NO_TAG) {
		char order = np->order;

		/*
		**	Force ordered tag if necessary to avoid timeouts 
		**	and to preserve interactivity.
		*/
		if (lp && ktime_exp(lp->tags_stime)) {
			lp->tags_si = !(lp->tags_si);
			if (lp->tags_sum[lp->tags_si]) {
				order = M_ORDERED_TAG;
				if ((DEBUG_FLAGS & DEBUG_TAGS)||bootverbose>0){ 
					PRINT_ADDR(cmd);
					printk("ordered tag forced.\n");
				}
			}
			lp->tags_stime = ktime_get(3*HZ);
		}

		if (order == 0) {
			/*
			**	Ordered write ops, unordered read ops.
			*/
			switch (cmd->cmnd[0]) {
			case 0x08:  /* READ_SMALL (6) */
			case 0x28:  /* READ_BIG  (10) */
			case 0xa8:  /* READ_HUGE (12) */
				order = M_SIMPLE_TAG;
				break;
			default:
				order = M_ORDERED_TAG;
			}
		}
		msgptr[msglen++] = order;
		/*
		**	For less than 128 tags, actual tags are numbered 
		**	1,3,5,..2*MAXTAGS+1,since we may have to deal 
		**	with devices that have problems with #TAG 0 or too 
		**	great #TAG numbers. For more tags (up to 256), 
		**	we use directly our tag number.
		*/
#if MAX_TASKS > (512/4)
		msgptr[msglen++] = cp->tag;
#else
		msgptr[msglen++] = (cp->tag << 1) + 1;
#endif
	}

	cp->host_flags	= 0;

	/*----------------------------------------------------
	**
	**	Build the data descriptors
	**
	**----------------------------------------------------
	*/

	direction = scsi_data_direction(cmd);
	if (direction != SCSI_DATA_NONE) {
		cp->segments = np->scatter (np, cp, cp->cmd);
		if (cp->segments < 0) {
			ncr_free_ccb(np, cp);
			return(DID_ERROR);
		}
	}
	else {
		cp->data_len = 0;
		cp->segments = 0;
	}

	/*---------------------------------------------------
	**
	**	negotiation required?
	**
	**	(nego_status is filled by ncr_prepare_nego())
	**
	**---------------------------------------------------
	*/

	cp->nego_status = 0;

#ifdef	SCSI_NCR_INTEGRITY_CHECKING
	if ((np->check_integrity && tp->ic_done) || !np->check_integrity) {
		 if ((!tp->widedone || !tp->period) && !tp->nego_cp && lp) {
			msglen += ncr_prepare_nego (np, cp, msgptr + msglen);
		 }
	}
	else if (np->check_integrity && (cmd->ic_in_progress)) { 
		msglen += ncr_ic_nego (np, cp, cmd, msgptr + msglen);
        }
	else if (np->check_integrity && cmd->ic_complete) {
		u_long current_period;
		u_char current_offset, current_width, current_factor;

		ncr_get_xfer_info (np, tp, &current_factor,
					&current_offset, &current_width);

		tp->ic_max_width = current_width;
		tp->ic_min_sync  = current_factor;

		if      (current_factor == 9) 	current_period = 125;
		else if (current_factor == 10) 	current_period = 250;
		else if (current_factor == 11) 	current_period = 303;
		else if (current_factor == 12) 	current_period = 500;
		else  			current_period = current_factor * 40;

		/*
                 * Negotiation for this target is complete. Update flags.
                 */
		tp->period = current_period;
		tp->widedone = 1;
		tp->ic_done = 1;

		printk("%s: Integrity Check Complete: \n", ncr_name(np)); 

		printk("%s: %s %s SCSI", ncr_name(np), 
				current_offset?"SYNC":"ASYNC",
				tp->ic_max_width?"WIDE":"NARROW");
		if (current_offset) {
			u_long mbs = 10000 * (tp->ic_max_width + 1); 

			printk(" %d.%d  MB/s", 
				(int) (mbs / current_period), (int) (mbs % current_period));

			printk(" (%d ns, %d offset)\n", 
				  (int) current_period/10, current_offset);
		}
		else 
			printk(" %d MB/s. \n ", (tp->ic_max_width+1)*5);
        }
#else
	if ((!tp->widedone || !tp->period) && !tp->nego_cp && lp) {
		msglen += ncr_prepare_nego (np, cp, msgptr + msglen);
	}
#endif	/* SCSI_NCR_INTEGRITY_CHECKING */


	/*----------------------------------------------------
	**
	**	Determine xfer direction.
	**
	**----------------------------------------------------
	*/
	if (!cp->data_len)
		direction = SCSI_DATA_NONE;

	/*
	**	If data direction is UNKNOWN, speculate DATA_READ 
	**	but prepare alternate pointers for WRITE in case 
	**	of our speculation will be just wrong.
	**	SCRIPTS will swap values if needed.
	*/
	switch(direction) {
	case SCSI_DATA_UNKNOWN:
	case SCSI_DATA_WRITE:
		goalp = NCB_SCRIPT_PHYS (np, data_out2) + 8;
		lastp = goalp - 8 - (cp->segments * (SCR_SG_SIZE*4));
		if (direction != SCSI_DATA_UNKNOWN)
			break;
		cp->phys.header.wgoalp	= cpu_to_scr(goalp);
		cp->phys.header.wlastp	= cpu_to_scr(lastp);
		/* fall through */
	case SCSI_DATA_READ:
		cp->host_flags |= HF_DATA_IN;
		goalp = NCB_SCRIPT_PHYS (np, data_in2) + 8;
		lastp = goalp - 8 - (cp->segments * (SCR_SG_SIZE*4));
		break;
	default:
	case SCSI_DATA_NONE:
		lastp = goalp = NCB_SCRIPTH_PHYS (np, no_data);
		break;
	}

	/*
	**	Set all pointers values needed by SCRIPTS.
	**	If direction is unknown, start at data_io.
	*/
	cp->phys.header.lastp = cpu_to_scr(lastp);
	cp->phys.header.goalp = cpu_to_scr(goalp);

	if (direction == SCSI_DATA_UNKNOWN)
		cp->phys.header.savep = 
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, data_io));
	else
		cp->phys.header.savep= cpu_to_scr(lastp);

	/*
	**	Save the initial data pointer in order to be able 
	**	to redo the command.
	**	We also have to save the initial lastp, since it 
	**	will be changed to DATA_IO if we don't know the data 
	**	direction and the device completes the command with 
	**	QUEUE FULL status (without entering the data phase).
	*/
	cp->startp = cp->phys.header.savep;
	cp->lastp0 = cp->phys.header.lastp;

	/*----------------------------------------------------
	**
	**	fill in ccb
	**
	**----------------------------------------------------
	**
	**
	**	physical -> virtual backlink
	**	Generic SCSI command
	*/

	/*
	**	Startqueue
	*/
	cp->phys.header.go.start   = cpu_to_scr(NCB_SCRIPT_PHYS (np,select));
	cp->phys.header.go.restart = cpu_to_scr(NCB_SCRIPT_PHYS (np,resel_dsa));
	/*
	**	select
	*/
	cp->phys.select.sel_id		= cp->target;
	cp->phys.select.sel_scntl3	= tp->wval;
	cp->phys.select.sel_sxfer	= tp->sval;
	cp->phys.select.sel_scntl4	= tp->uval;
	/*
	**	message
	*/
	cp->phys.smsg.addr	= cpu_to_scr(CCB_PHYS (cp, scsi_smsg));
	cp->phys.smsg.size	= cpu_to_scr(msglen);

	/*
	**	command
	*/
	memcpy(cp->cdb_buf, cmd->cmnd, MIN(cmd->cmd_len, sizeof(cp->cdb_buf)));
	cp->phys.cmd.addr	= cpu_to_scr(CCB_PHYS (cp, cdb_buf[0]));
	cp->phys.cmd.size	= cpu_to_scr(cmd->cmd_len);

	/*
	**	status
	*/
	cp->actualquirks	= tp->quirks;
	cp->host_status		= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
	cp->scsi_status		= S_ILLEGAL;
	cp->xerr_status		= 0;
	cp->extra_bytes		= 0;

	/*
	**	extreme data pointer.
	**	shall be positive, so -1 is lower than lowest.:)
	*/
	cp->ext_sg  = -1;
	cp->ext_ofs = 0;

	/*----------------------------------------------------
	**
	**	Critical region: start this job.
	**
	**----------------------------------------------------
	*/

	/*
	**	activate this job.
	*/

	/*
	**	insert next CCBs into start queue.
	**	2 max at a time is enough to flush the CCB wait queue.
	*/
	if (lp)
		ncr_start_next_ccb(np, lp, 2);
	else
		ncr_put_start_queue(np, cp);

	/*
	**	Command is successfully queued.
	*/

	return(DID_OK);
}


/*==========================================================
**
**
**	Insert a CCB into the start queue and wake up the 
**	SCRIPTS processor.
**
**
**==========================================================
*/

static void ncr_start_next_ccb(ncb_p np, lcb_p lp, int maxn)
{
	XPT_QUEHEAD *qp;
	ccb_p cp;

	while (maxn-- && lp->queuedccbs < lp->queuedepth) {
		qp = xpt_remque_head(&lp->wait_ccbq);
		if (!qp)
			break;
		++lp->queuedccbs;
		cp = xpt_que_entry(qp, struct ccb, link_ccbq);
		xpt_insque_tail(qp, &lp->busy_ccbq);
		lp->tasktbl[cp->tag == NO_TAG ? 0 : cp->tag] =
			cpu_to_scr(cp->p_ccb);
		ncr_put_start_queue(np, cp);
	}
}

static void ncr_put_start_queue(ncb_p np, ccb_p cp)
{
	u_short	qidx;

#ifdef SCSI_NCR_IARB_SUPPORT
	/*
	**	If the previously queued CCB is not yet done, 
	**	set the IARB hint. The SCRIPTS will go with IARB 
	**	for this job when starting the previous one.
	**	We leave devices a chance to win arbitration by 
	**	not using more than 'iarb_max' consecutive 
	**	immediate arbitrations.
	*/
	if (np->last_cp && np->iarb_count < np->iarb_max) {
		np->last_cp->host_flags |= HF_HINT_IARB;
		++np->iarb_count;
	}
	else
		np->iarb_count = 0;
	np->last_cp = cp;
#endif
	
	/*
	**	insert into start queue.
	*/
	qidx = np->squeueput + 2;
	if (qidx >= MAX_START*2) qidx = 0;

	np->squeue [qidx]	   = cpu_to_scr(np->p_idletask);
	MEMORY_BARRIER();
	np->squeue [np->squeueput] = cpu_to_scr(cp->p_ccb);

	np->squeueput = qidx;
	cp->queued = 1;

	if (DEBUG_FLAGS & DEBUG_QUEUE)
		printk ("%s: queuepos=%d.\n", ncr_name (np), np->squeueput);

	/*
	**	Script processor may be waiting for reselect.
	**	Wake it up.
	*/
	MEMORY_BARRIER();
	OUTB (nc_istat, SIGP|np->istat_sem);
}


/*==========================================================
**
**	Soft reset the chip.
**
**	Some 896 and 876 chip revisions may hang-up if we set 
**	the SRST (soft reset) bit at the wrong time when SCRIPTS 
**	are running.
**	So, we need to abort the current operation prior to 
**	soft resetting the chip.
**
**==========================================================
*/

static void ncr_chip_reset (ncb_p np)
{
	OUTB (nc_istat, SRST);
	UDELAY (10);
	OUTB (nc_istat, 0);
}

static void ncr_soft_reset(ncb_p np)
{
	u_char istat = 0;
	int i;

	if (!(np->features & FE_ISTAT1) || !(INB (nc_istat1) & SRUN))
		goto do_chip_reset;

	OUTB (nc_istat, CABRT);
	for (i = 100000 ; i ; --i) {
		istat = INB (nc_istat);
		if (istat & SIP) {
			INW (nc_sist);
		}
		else if (istat & DIP) {
			if (INB (nc_dstat) & ABRT)
				break;
		}
		UDELAY(5);
	}
	OUTB (nc_istat, 0);
	if (!i)
		printk("%s: unable to abort current chip operation, "
		       "ISTAT=0x%02x.\n", ncr_name(np), istat);
do_chip_reset:
	ncr_chip_reset(np);
}

/*==========================================================
**
**
**	Start reset process.
**	The interrupt handler will reinitialize the chip.
**	The timeout handler will wait for settle_time before 
**	clearing it and so resuming command processing.
**
**
**==========================================================
*/
static void ncr_start_reset(ncb_p np)
{
	(void) ncr_reset_scsi_bus(np, 1, driver_setup.settle_delay);
}
 
static int ncr_reset_scsi_bus(ncb_p np, int enab_int, int settle_delay)
{
	u_int32 term;
	int retv = 0;

	np->settle_time	= ktime_get(settle_delay * HZ);

	if (bootverbose > 1)
		printk("%s: resetting, "
			"command processing suspended for %d seconds\n",
			ncr_name(np), settle_delay);

	ncr_soft_reset(np);	/* Soft reset the chip */
	UDELAY (2000);	/* The 895/6 need time for the bus mode to settle */
	if (enab_int)
		OUTW (nc_sien, RST);
	/*
	**	Enable Tolerant, reset IRQD if present and 
	**	properly set IRQ mode, prior to resetting the bus.
	*/
	OUTB (nc_stest3, TE);
	OUTB (nc_dcntl, (np->rv_dcntl & IRQM));
	OUTB (nc_scntl1, CRST);
	UDELAY (200);

	if (!driver_setup.bus_check)
		goto out;
	/*
	**	Check for no terminators or SCSI bus shorts to ground.
	**	Read SCSI data bus, data parity bits and control signals.
	**	We are expecting RESET to be TRUE and other signals to be 
	**	FALSE.
	*/
	term =	INB(nc_sstat0);
	term =	((term & 2) << 7) + ((term & 1) << 17);	/* rst sdp0 */
	term |= ((INB(nc_sstat2) & 0x01) << 26) |	/* sdp1     */
		((INW(nc_sbdl) & 0xff)   << 9)  |	/* d7-0     */
		((INW(nc_sbdl) & 0xff00) << 10) |	/* d15-8    */
		INB(nc_sbcl);	/* req ack bsy sel atn msg cd io    */

	if (!(np->features & FE_WIDE))
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printk("%s: suspicious SCSI data while resetting the BUS.\n",
			ncr_name(np));
		printk("%s: %sdp0,d7-0,rst,req,ack,bsy,sel,atn,msg,c/d,i/o = "
			"0x%lx, expecting 0x%lx\n",
			ncr_name(np),
			(np->features & FE_WIDE) ? "dp1,d15-8," : "",
			(u_long)term, (u_long)(2<<7));
		if (driver_setup.bus_check == 1)
			retv = 1;
	}
out:
	OUTB (nc_scntl1, 0);
	return retv;
}

/*==========================================================
**
**
**	Reset the SCSI BUS.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/
static int ncr_reset_bus (ncb_p np, Scsi_Cmnd *cmd, int sync_reset)
{
/*	Scsi_Device        *device    = cmd->device; */
	ccb_p cp;
	int found;

/*
 * Return immediately if reset is in progress.
 */
	if (np->settle_time) {
		return SCSI_RESET_PUNT;
	}
/*
 * Start the reset process.
 * The script processor is then assumed to be stopped.
 * Commands will now be queued in the waiting list until a settle 
 * delay of 2 seconds will be completed.
 */
	ncr_start_reset(np);
/*
 * First, look in the wakeup list
 */
	for (found=0, cp=np->ccbc; cp; cp=cp->link_ccb) {
		/*
		**	look for the ccb of this command.
		*/
		if (cp->host_status == HS_IDLE) continue;
		if (cp->cmd == cmd) {
			found = 1;
			break;
		}
	}
/*
 * Then, look in the waiting list
 */
	if (!found && retrieve_from_waiting_list(0, np, cmd))
		found = 1;
/*
 * Wake-up all awaiting commands with DID_RESET.
 */
	reset_waiting_list(np);
/*
 * Wake-up all pending commands with HS_RESET -> DID_RESET.
 */
	ncr_wakeup(np, HS_RESET);
/*
 * If the involved command was not in a driver queue, and the 
 * scsi driver told us reset is synchronous, and the command is not 
 * currently in the waiting list, complete it with DID_RESET status,
 * in order to keep it alive.
 */
	if (!found && sync_reset && !retrieve_from_waiting_list(0, np, cmd)) {
		SetScsiResult(cmd, DID_RESET, 0);
		ncr_queue_done_cmd(np, cmd);
	}

	return SCSI_RESET_SUCCESS;
}

/*==========================================================
**
**
**	Abort an SCSI command.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/
static int ncr_abort_command (ncb_p np, Scsi_Cmnd *cmd)
{
/*	Scsi_Device        *device    = cmd->device; */
	ccb_p cp;

/*
 * First, look for the scsi command in the waiting list
 */
	if (remove_from_waiting_list(np, cmd)) {
		SetScsiAbortResult(cmd);
		ncr_queue_done_cmd(np, cmd);
		return SCSI_ABORT_SUCCESS;
	}

/*
 * Then, look in the wakeup list
 */
	for (cp=np->ccbc; cp; cp=cp->link_ccb) {
		/*
		**	look for the ccb of this command.
		*/
		if (cp->host_status == HS_IDLE) continue;
		if (cp->cmd == cmd)
			break;
	}

	if (!cp) {
		return SCSI_ABORT_NOT_RUNNING;
	}

	/*
	**	Keep track we have to abort this job.
	*/
	cp->to_abort = 1;

	/*
	**	Tell the SCRIPTS processor to stop 
	**	and synchronize with us.
	*/
	np->istat_sem = SEM;

	/*
	**      If there are no requests, the script
	**      processor will sleep on SEL_WAIT_RESEL.
	**      Let's wake it up, since it may have to work.
	*/
	OUTB (nc_istat, SIGP|SEM);

	/*
	**	Tell user we are working for him.
	*/
	return SCSI_ABORT_PENDING;
}

/*==========================================================
**
**	Linux release module stuff.
**
**	Called before unloading the module
**	Detach the host.
**	We have to free resources and halt the NCR chip
**
**==========================================================
*/

#ifdef MODULE
static int ncr_detach(ncb_p np)
{
	int i;

	printk("%s: detaching ...\n", ncr_name(np));

/*
**	Stop the ncr_timeout process
**	Set release_stage to 1 and wait that ncr_timeout() set it to 2.
*/
	np->release_stage = 1;
	for (i = 50 ; i && np->release_stage != 2 ; i--) MDELAY (100);
	if (np->release_stage != 2)
		printk("%s: the timer seems to be already stopped\n",
			ncr_name(np));
	else np->release_stage = 2;

/*
**	Reset NCR chip.
**	We should use ncr_soft_reset(), but we donnot want to do 
**	so, since we may not be safe if interrupts occur.
*/

	printk("%s: resetting chip\n", ncr_name(np));
	ncr_chip_reset(np);

/*
**	Restore bios setting for automatic clock detection.
*/
	OUTB(nc_dmode,	np->sv_dmode);
	OUTB(nc_dcntl,	np->sv_dcntl);
	OUTB(nc_ctest3,	np->sv_ctest3);
	OUTB(nc_ctest4,	np->sv_ctest4);
	OUTB(nc_ctest5,	np->sv_ctest5);
	OUTB(nc_gpcntl,	np->sv_gpcntl);
	OUTB(nc_stest2,	np->sv_stest2);

	ncr_selectclock(np, np->sv_scntl3);
/*
**	Free host resources
*/
	ncr_free_resources(np);

	return 1;
}
#endif

/*==========================================================
**
**
**	Complete execution of a SCSI command.
**	Signal completion to the generic SCSI driver.
**
**
**==========================================================
*/

void ncr_complete (ncb_p np, ccb_p cp)
{
	Scsi_Cmnd *cmd;
	tcb_p tp;
	lcb_p lp;

	/*
	**	Sanity check
	*/
	if (!cp || !cp->cmd)
		return;

	/*
	**	Print some debugging info.
	*/

	if (DEBUG_FLAGS & DEBUG_TINY)
		printk ("CCB=%lx STAT=%x/%x\n", (unsigned long)cp,
			cp->host_status,cp->scsi_status);

	/*
	**	Get command, target and lun pointers.
	*/

	cmd = cp->cmd;
	cp->cmd = NULL;
	tp = &np->target[cp->target];
	lp = ncr_lp(np, tp, cp->lun);

	/*
	**	We donnot queue more than 1 ccb per target 
	**	with negotiation at any time. If this ccb was 
	**	used for negotiation, clear this info in the tcb.
	*/

	if (cp == tp->nego_cp)
		tp->nego_cp = 0;

#ifdef SCSI_NCR_IARB_SUPPORT
	/*
	**	We just complete the last queued CCB.
	**	Clear this info that is no more relevant.
	*/
	if (cp == np->last_cp)
		np->last_cp = 0;
#endif

	/*
	**	If auto-sense performed, change scsi status, 
	**	Otherwise, compute the residual.
	*/
	if (cp->host_flags & HF_AUTO_SENSE) {
		cp->scsi_status = cp->sv_scsi_status;
		cp->xerr_status = cp->sv_xerr_status;
	}
	else {
		cp->resid = 0;
		if (cp->xerr_status ||
		    cp->phys.header.lastp != cp->phys.header.goalp)
			cp->resid = ncr_compute_residual(np, cp);
	}

	/*
	**	Check for extended errors.
	*/

	if (cp->xerr_status) {
		if (cp->xerr_status & XE_PARITY_ERR) {
			PRINT_ADDR(cmd);
			printk ("unrecovered SCSI parity error.\n");
		}
		if (cp->xerr_status & XE_EXTRA_DATA) {
			PRINT_ADDR(cmd);
			printk ("extraneous data discarded.\n");
		}
		if (cp->xerr_status & XE_BAD_PHASE) {
			PRINT_ADDR(cmd);
			printk ("illegal scsi phase (4/5).\n");
		}
		if (cp->xerr_status & XE_SODL_UNRUN) {
			PRINT_ADDR(cmd);
			printk ("ODD transfer in DATA OUT phase.\n");
		}
		if (cp->xerr_status & XE_SWIDE_OVRUN){
			PRINT_ADDR(cmd);
			printk ("ODD transfer in DATA IN phase.\n");
		}

		if (cp->host_status==HS_COMPLETE)
			cp->host_status = HS_FAIL;
	}

	/*
	**	Print out any error for debugging purpose.
	*/
	if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
		if (cp->host_status!=HS_COMPLETE || cp->scsi_status!=S_GOOD ||
		    cp->resid) {
			PRINT_ADDR(cmd);
			printk ("ERROR: cmd=%x host_status=%x scsi_status=%x "
				"data_len=%d residual=%d\n",
				cmd->cmnd[0], cp->host_status, cp->scsi_status,
				cp->data_len, cp->resid);
		}
	}

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,99)
	/*
	**	Move residual byte count to user structure.
	*/
	cmd->resid = cp->resid;
#endif
	/*
	**	Check the status.
	*/
	if (   (cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_GOOD ||
		    cp->scsi_status == S_COND_MET)) {
                /*
		**	All went well (GOOD status).
		**	CONDITION MET status is returned on 
                **	`Pre-Fetch' or `Search data' success.
                */
		SetScsiResult(cmd, DID_OK, cp->scsi_status);

		/*
		**	Allocate the lcb if not yet.
		*/
		if (!lp)
			ncr_alloc_lcb (np, cp->target, cp->lun);

		/*
		**	On standard INQUIRY response (EVPD and CmDt 
		**	not set), setup logical unit according to 
		**	announced capabilities (we need the 1rst 8 bytes).
		*/
		if (cmd->cmnd[0] == 0x12 && !(cmd->cmnd[1] & 0x3) &&
		    cmd->request_bufflen - cp->resid > 7 && !cmd->use_sg) {
			sync_scsi_data(np, cmd);	/* SYNC the data */
			ncr_setup_lcb (np, cp->target, cp->lun,
				       (char *) cmd->request_buffer);
		}

		/*
		**	If tags was reduced due to queue full,
		**	increase tags if 1000 good status received.
		*/
		if (lp && lp->usetags && lp->numtags < lp->maxtags) {
			++lp->num_good;
			if (lp->num_good >= 1000) {
				lp->num_good = 0;
				++lp->numtags;
				ncr_setup_tags (np, cp->target, cp->lun);
			}
		}
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CHECK_COND)) {
		/*
		**   Check condition code
		*/
		SetScsiResult(cmd, DID_OK, S_CHECK_COND);

		if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
			PRINT_ADDR(cmd);
			ncr_printl_hex("sense data:", cmd->sense_buffer, 14);
		}
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CONFLICT)) {
		/*
		**   Reservation Conflict condition code
		*/
		SetScsiResult(cmd, DID_OK, S_CONFLICT);

	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_BUSY ||
		    cp->scsi_status == S_QUEUE_FULL)) {

		/*
		**   Target is busy.
		*/
		SetScsiResult(cmd, DID_OK, cp->scsi_status);

	} else if ((cp->host_status == HS_SEL_TIMEOUT)
		|| (cp->host_status == HS_TIMEOUT)) {

		/*
		**   No response
		*/
		SetScsiResult(cmd, DID_TIME_OUT, cp->scsi_status);

	} else if (cp->host_status == HS_RESET) {

		/*
		**   SCSI bus reset
		*/
		SetScsiResult(cmd, DID_RESET, cp->scsi_status);

	} else if (cp->host_status == HS_ABORTED) {

		/*
		**   Transfer aborted
		*/
		SetScsiAbortResult(cmd);

	} else {
		int did_status;

		/*
		**  Other protocol messes
		*/
		PRINT_ADDR(cmd);
		printk ("COMMAND FAILED (%x %x) @%p.\n",
			cp->host_status, cp->scsi_status, cp);

		did_status = DID_ERROR;
		if (cp->xerr_status & XE_PARITY_ERR)
			did_status = DID_PARITY;

		SetScsiResult(cmd, did_status, cp->scsi_status);
	}

	/*
	**	trace output
	*/

	if (tp->usrflag & UF_TRACE) {
		PRINT_ADDR(cmd);
		printk (" CMD:");
		ncr_print_hex(cmd->cmnd, cmd->cmd_len);

		if (cp->host_status==HS_COMPLETE) {
			switch (cp->scsi_status) {
			case S_GOOD:
				printk ("  GOOD");
				break;
			case S_CHECK_COND:
				printk ("  SENSE:");
				ncr_print_hex(cmd->sense_buffer, 14);
				break;
			default:
				printk ("  STAT: %x\n", cp->scsi_status);
				break;
			}
		} else printk ("  HOSTERROR: %x", cp->host_status);
		printk ("\n");
	}

	/*
	**	Free this ccb
	*/
	ncr_free_ccb (np, cp);

	/*
	**	requeue awaiting scsi commands for this lun.
	*/
	if (lp && lp->queuedccbs < lp->queuedepth &&
	    !xpt_que_empty(&lp->wait_ccbq))
		ncr_start_next_ccb(np, lp, 2);

	/*
	**	requeue awaiting scsi commands for this controller.
	*/
	if (np->waiting_list)
		requeue_waiting_list(np);

	/*
	**	signal completion to generic driver.
	*/
	ncr_queue_done_cmd(np, cmd);
}

/*==========================================================
**
**
**	Signal all (or one) control block done.
**
**
**==========================================================
*/

/*
**	The NCR has completed CCBs.
**	Look at the DONE QUEUE.
**
**	On architectures that may reorder LOAD/STORE operations, 
**	a memory barrier may be needed after the reading of the 
**	so-called `flag' and prior to dealing with the data.
*/
int ncr_wakeup_done (ncb_p np)
{
	ccb_p cp;
	int i, n;
	u_long dsa;

	n = 0;
	i = np->dqueueget;
	while (1) {
		dsa = scr_to_cpu(np->dqueue[i]);
		if (!dsa)
			break;
		np->dqueue[i] = 0;
		if ((i = i+2) >= MAX_START*2)
			i = 0;

		cp = ncr_ccb_from_dsa(np, dsa);
		if (cp) {
			MEMORY_BARRIER();
			ncr_complete (np, cp);
			++n;
		}
		else
			printk (KERN_ERR "%s: bad DSA (%lx) in done queue.\n",
				ncr_name(np), dsa);
	}
	np->dqueueget = i;

	return n;
}

/*
**	Complete all active CCBs.
*/
void ncr_wakeup (ncb_p np, u_long code)
{
	ccb_p cp = np->ccbc;

	while (cp) {
		if (cp->host_status != HS_IDLE) {
			cp->host_status = code;
			ncr_complete (np, cp);
		}
		cp = cp->link_ccb;
	}
}

/*==========================================================
**
**
**	Start NCR chip.
**
**
**==========================================================
*/

void ncr_init (ncb_p np, int reset, char * msg, u_long code)
{
 	int	i;
	u_long	phys;

 	/*
	**	Reset chip if asked, otherwise just clear fifos.
 	*/

	if (reset)
		ncr_soft_reset(np);
	else {
		OUTB (nc_stest3, TE|CSF);
		OUTONB (nc_ctest3, CLF);
	}
 
	/*
	**	Message.
	*/

	if (msg) printk (KERN_INFO "%s: restart (%s).\n", ncr_name (np), msg);

	/*
	**	Clear Start Queue
	*/
	phys = np->p_squeue;
	np->queuedepth = MAX_START - 1;	/* 1 entry needed as end marker */
	for (i = 0; i < MAX_START*2; i += 2) {
		np->squeue[i]   = cpu_to_scr(np->p_idletask);
		np->squeue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->squeue[MAX_START*2-1] = cpu_to_scr(phys);


	/*
	**	Start at first entry.
	*/
	np->squeueput = 0;
	np->scripth0->startpos[0] = cpu_to_scr(phys);

	/*
	**	Clear Done Queue
	*/
	phys = vtobus(np->dqueue);
	for (i = 0; i < MAX_START*2; i += 2) {
		np->dqueue[i]   = 0;
		np->dqueue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->dqueue[MAX_START*2-1] = cpu_to_scr(phys);

	/*
	**	Start at first entry.
	*/
	np->scripth0->done_pos[0] = cpu_to_scr(phys);
	np->dqueueget = 0;

	/*
	**	Wakeup all pending jobs.
	*/
	ncr_wakeup (np, code);

	/*
	**	Init chip.
	*/

	OUTB (nc_istat,  0x00   );	/*  Remove Reset, abort */
	UDELAY (2000);	/* The 895 needs time for the bus mode to settle */

	OUTB (nc_scntl0, np->rv_scntl0 | 0xc0);
					/*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00);		/*  odd parity, and remove CRST!! */

	ncr_selectclock(np, np->rv_scntl3);	/* Select SCSI clock */

	OUTB (nc_scid  , RRE|np->myaddr);	/* Adapter SCSI address */
	OUTW (nc_respid, 1ul<<np->myaddr);	/* Id to respond to */
	OUTB (nc_istat , SIGP	);		/*  Signal Process */
	OUTB (nc_dmode , np->rv_dmode);		/* Burst length, dma mode */
	OUTB (nc_ctest5, np->rv_ctest5);	/* Large fifo + large burst */

	OUTB (nc_dcntl , NOCOM|np->rv_dcntl);	/* Protect SFBR */
	OUTB (nc_ctest3, np->rv_ctest3);	/* Write and invalidate */
	OUTB (nc_ctest4, np->rv_ctest4);	/* Master parity checking */

	if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
		(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66)){
		OUTB (nc_stest2, EXT|np->rv_stest2); 
		/* Extended Sreq/Sack filtering, not supported in C1010/C1010_66 */
	}
	OUTB (nc_stest3, TE);			/* TolerANT enable */
	OUTB (nc_stime0, 0x0c);			/* HTH disabled  STO 0.25 sec */

	/*
	**	DEL 441 - 53C876 Rev 5 - Part Number 609-0392787/2788 - ITEM 2.
	**	Disable overlapped arbitration for all dual-function 
	**	devices, regardless revision id.
	**	We may consider it is a post-chip-design feature. ;-)
 	**
 	**	Errata applies to all 896 and 1010 parts.
	*/
	if (np->device_id == PCI_DEVICE_ID_NCR_53C875)
		OUTB (nc_ctest0, (1<<5));
 	else if (np->device_id == PCI_DEVICE_ID_NCR_53C896  ||
 		 np->device_id == PCI_DEVICE_ID_LSI_53C1010 ||
 		 np->device_id == PCI_DEVICE_ID_LSI_53C1010_66 )
		np->rv_ccntl0 |= DPR;

	/*
	**	C1010_66MHz rev 0 part requies AIPCNTL1 bit 3 to be set.
	*/
	if (np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)
		OUTB(nc_aipcntl1, (1<<3));

	/*
	**  Write CCNTL0/CCNTL1 for chips capable of 64 bit addressing 
	**  and/or hardware phase mismatch, since only such chips 
	**  seem to support those IO registers.
	*/
	if (np->features & (FE_DAC | FE_NOPM)) {
		OUTB (nc_ccntl0, np->rv_ccntl0);
		OUTB (nc_ccntl1, np->rv_ccntl1);
	}

	/*
 	**	If phase mismatch handled by scripts (53C895A or 53C896 
 	**	or 53C1010 or 53C1010_66), set PM jump addresses. 
	*/

	if (np->features & FE_NOPM) {
		printk(KERN_INFO "%s: handling phase mismatch from SCRIPTS.\n", 
		       ncr_name(np));
		OUTL (nc_pmjad1, NCB_SCRIPTH_PHYS (np, pm_handle));
		OUTL (nc_pmjad2, NCB_SCRIPTH_PHYS (np, pm_handle));
	}

	/*
	**    Enable GPIO0 pin for writing if LED support from SCRIPTS.
	**    Also set GPIO5 and clear GPIO6 if hardware LED control.
	*/

	if (np->features & FE_LED0)
		OUTB(nc_gpcntl, INB(nc_gpcntl) & ~0x01);
	else if (np->features & FE_LEDC)
		OUTB(nc_gpcntl, (INB(nc_gpcntl) & ~0x41) | 0x20);


	/*
	**      enable ints
	*/

	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST|PAR);
	OUTB (nc_dien , MDPE|BF|SSI|SIR|IID);

	/*
	**	For 895/895A/896/c1010
	**	Enable SBMC interrupt and save current SCSI bus mode.
	*/
	if ( (np->features & FE_ULTRA2) || (np->features & FE_ULTRA3) ) {
		OUTONW (nc_sien, SBMC);
		np->scsi_mode = INB (nc_stest4) & SMODE;
	}

	/*
	**	Fill in target structure.
	**	Reinitialize usrsync.
	**	Reinitialize usrwide.
	**	Prepare sync negotiation according to actual SCSI bus mode.
	*/

	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->to_reset = 0;

		tp->sval    = 0;
		tp->wval    = np->rv_scntl3;
		tp->uval    = np->rv_scntl4;

		if (tp->usrsync != 255) {
			if (tp->usrsync <= np->maxsync) {
				if (tp->usrsync < np->minsync) {
					tp->usrsync = np->minsync;
				}
			}
			else
				tp->usrsync = 255;
		};

		if (tp->usrwide > np->maxwide)
			tp->usrwide = np->maxwide;

		ncr_negotiate (np, tp);
	}

	/*
	**    Download SCSI SCRIPTS to on-chip RAM if present,
	**    and start script processor.
	**    We do the download preferently from the CPU.
	**    For platforms that may not support PCI memory mapping,
	**    we use a simple SCRIPTS that performs MEMORY MOVEs.
	*/
	if (np->base2_ba) {
		if (bootverbose)
			printk ("%s: Downloading SCSI SCRIPTS.\n",
				ncr_name(np));
#ifdef SCSI_NCR_PCI_MEM_NOT_SUPPORTED
		if (np->base2_ws == 8192)
			phys = NCB_SCRIPTH0_PHYS (np, start_ram64);
		else
			phys = NCB_SCRIPTH_PHYS (np, start_ram);
#else
		if (np->base2_ws == 8192) {
			memcpy_to_pci(np->base2_va + 4096,
					np->scripth0, sizeof(struct scripth));
			OUTL (nc_mmws, np->scr_ram_seg);
			OUTL (nc_mmrs, np->scr_ram_seg);
			OUTL (nc_sfs,  np->scr_ram_seg);
			phys = NCB_SCRIPTH_PHYS (np, start64);
		}
		else
			phys = NCB_SCRIPT_PHYS (np, init);
		memcpy_to_pci(np->base2_va, np->script0, sizeof(struct script));
#endif /* SCSI_NCR_PCI_MEM_NOT_SUPPORTED */
	}
	else
		phys = NCB_SCRIPT_PHYS (np, init);

	np->istat_sem = 0;

	OUTL (nc_dsa, np->p_ncb);
	OUTL_DSP (phys);
}

/*==========================================================
**
**	Prepare the negotiation values for wide and
**	synchronous transfers.
**
**==========================================================
*/

static void ncr_negotiate (struct ncb* np, struct tcb* tp)
{
	/*
	**	minsync unit is 4ns !
	*/

	u_long minsync = tp->usrsync;

	/*
	**	SCSI bus mode limit
	*/

	if (np->scsi_mode && np->scsi_mode == SMODE_SE) {
		if (minsync < 12) minsync = 12;
	}

	/*
	**	our limit ..
	*/

	if (minsync < np->minsync)
		minsync = np->minsync;

	/*
	**	divider limit
	*/

	if (minsync > np->maxsync)
		minsync = 255;

	tp->minsync = minsync;
	tp->maxoffs = (minsync<255 ? np->maxoffs : 0);

	/*
	**	period=0: has to negotiate sync transfer
	*/

	tp->period=0;

	/*
	**	widedone=0: has to negotiate wide transfer
	*/
	tp->widedone=0;
}

/*==========================================================
**
**	Get clock factor and sync divisor for a given 
**	synchronous factor period.
**	Returns the clock factor (in sxfer) and scntl3 
**	synchronous divisor field.
**
**==========================================================
*/

static void ncr_getsync(ncb_p np, u_char sfac, u_char *fakp, u_char *scntl3p)
{
	u_long	clk = np->clock_khz;	/* SCSI clock frequency in kHz	*/
	int	div = np->clock_divn;	/* Number of divisors supported	*/
	u_long	fak;			/* Sync factor in sxfer		*/
	u_long	per;			/* Period in tenths of ns	*/
	u_long	kpc;			/* (per * clk)			*/

	/*
	**	Compute the synchronous period in tenths of nano-seconds
	**	from sfac.
	**
	**	Note, if sfac == 9, DT is being used. Double the period of 125
	**	to 250. 
	*/
	if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;

	/*
	**	Look for the greatest clock divisor that allows an 
	**	input speed faster than the period.
	*/
	kpc = per * clk;
	while (--div >= 0)
		if (kpc >= (div_10M[div] << 2)) break;

	/*
	**	Calculate the lowest clock factor that allows an output 
	**	speed not faster than the period.
	*/
	fak = (kpc - 1) / div_10M[div] + 1;

#if 0	/* This optimization does not seem very useful */

	per = (fak * div_10M[div]) / clk;

	/*
	**	Why not to try the immediate lower divisor and to choose 
	**	the one that allows the fastest output speed ?
	**	We dont want input speed too much greater than output speed.
	*/
	if (div >= 1 && fak < 8) {
		u_long fak2, per2;
		fak2 = (kpc - 1) / div_10M[div-1] + 1;
		per2 = (fak2 * div_10M[div-1]) / clk;
		if (per2 < per && fak2 <= 8) {
			fak = fak2;
			per = per2;
			--div;
		}
	}
#endif

	if (fak < 4) fak = 4;	/* Should never happen, too bad ... */

	/*
	**	Compute and return sync parameters for the ncr
	*/
	*fakp		= fak - 4;

	/*
	** If sfac < 25, and 8xx parts, desire that the chip operate at 
	** least at Ultra speeds.  Must set bit 7 of scntl3.
	** For C1010, do not set this bit. If operating at Ultra3 speeds,
	**	set the U3EN bit instead.
	*/ 
	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010)  ||
			(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {
		*scntl3p	= (div+1) << 4;
		*fakp		= 0;
	}
	else {
		*scntl3p	= ((div+1) << 4) + (sfac < 25 ? 0x80 : 0);
		*fakp		= fak - 4;
	}
}

/*==========================================================
**
**	Utility routine to return the current bus width	
**	synchronous period and offset.
**	Utilizes target sval, wval and uval  
**
**==========================================================
*/
static void ncr_get_xfer_info(ncb_p np, tcb_p tp, u_char *factor, 
			u_char *offset, u_char *width)
{

	u_char idiv;
	u_long period;

	*width = (tp->wval & EWS) ? 1 : 0;

	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
		(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66))
		*offset  = (tp->sval & 0x3f);
	else
		*offset  = (tp->sval & 0x1f);

        /*
	 * Midlayer signal to the driver that all of the scsi commands
	 * for the integrity check have completed. Save the negotiated
 	 * parameters (extracted from sval, wval and uval). 
	 * See ncr_setsync for alg. details.
	 */

	idiv = (tp->wval>>4) & 0x07;

	if ( *offset && idiv ) {
	  	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) || 
	  		(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)){
		    if (tp->uval & 0x80)
			period = (2*div_10M[idiv-1])/np->clock_khz;
	    	    else 
	    		period = (4*div_10M[idiv-1])/np->clock_khz;
	  	}
	  	else
	   	    period = (((tp->sval>>5)+4)*div_10M[idiv-1])/np->clock_khz;
	}
	else
		period = 0xffff;

	if	(period <= 125)		*factor =   9;
	else if	(period <= 250)		*factor =  10;
	else if	(period <= 303)		*factor  = 11;
	else if	(period <= 500)		*factor  = 12;
	else				*factor  = (period + 40 - 1) / 40;

}


/*==========================================================
**
**	Set actual values, sync status and patch all ccbs of 
**	a target according to new sync/wide agreement.
**
**==========================================================
*/

static void ncr_set_sync_wide_status (ncb_p np, u_char target)
{
	ccb_p cp = np->ccbc;
	tcb_p tp = &np->target[target];

	/*
	**	set actual value and sync_status
	**
	**	TEMP register contains current scripts address
	**	which is data type/direction/dependent.
	*/
	OUTB (nc_sxfer, tp->sval);
	OUTB (nc_scntl3, tp->wval);
	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010)  ||
			(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) 
		OUTB (nc_scntl4, tp->uval); 

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = np->ccbc; cp; cp = cp->link_ccb) {
		if (cp->host_status == HS_IDLE)
			continue;
		if (cp->target != target)
			continue;
		cp->phys.select.sel_scntl3 = tp->wval;
		cp->phys.select.sel_sxfer  = tp->sval;
		if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
				(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66))
			cp->phys.select.sel_scntl4 = tp->uval;
	};
}

/*==========================================================
**
**	Switch sync mode for current job and it's target
**
**==========================================================
*/

static void ncr_setsync (ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer, 
					u_char scntl4)
{
	tcb_p tp;
	u_char target = INB (nc_sdid) & 0x0f;
	u_char idiv;
	u_char offset;

	assert (cp);
	if (!cp) return;

	assert (target == (cp->target & 0xf));

	tp = &np->target[target];

	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
			(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {
		offset = sxfer & 0x3f; /* bits 5-0 */
		scntl3 = (scntl3 & 0xf0) | (tp->wval & EWS);
		scntl4 = (scntl4 & 0x80);
	}
	else {
		offset = sxfer & 0x1f; /* bits 4-0 */
		if (!scntl3 || !offset)
			scntl3 = np->rv_scntl3;

		scntl3 = (scntl3 & 0xf0) | (tp->wval & EWS) | 
				(np->rv_scntl3 & 0x07);
	}
	

	/*
	**	Deduce the value of controller sync period from scntl3.
	**	period is in tenths of nano-seconds.
	*/

	idiv = ((scntl3 >> 4) & 0x7);
	if ( offset && idiv) {
		if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
			(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {
			/* Note: If extra data hold clocks are used,
			 * the formulas below must be modified.
			 * When scntl4 == 0, ST mode.
			 */
			if (scntl4 & 0x80)
				tp->period = (2*div_10M[idiv-1])/np->clock_khz;
			else
				tp->period = (4*div_10M[idiv-1])/np->clock_khz;
		}
		else 
			tp->period = (((sxfer>>5)+4)*div_10M[idiv-1])/np->clock_khz;
	}
	else
		tp->period = 0xffff;


	/*
	**	 Stop there if sync parameters are unchanged
	*/
	if (tp->sval == sxfer && tp->wval == scntl3 && tp->uval == scntl4) return;
	tp->sval = sxfer;
	tp->wval = scntl3;
	tp->uval = scntl4;

	/*
	**	Bells and whistles   ;-)
	**	Donnot announce negotiations due to auto-sense, 
	**	unless user really want us to be verbose. :)
	*/
	if ( bootverbose < 2 && (cp->host_flags & HF_AUTO_SENSE))
		goto next;
	PRINT_TARGET(np, target);
	if (offset) {
		unsigned f10 = 100000 << (tp->widedone ? tp->widedone -1 : 0);
		unsigned mb10 = (f10 + tp->period/2) / tp->period;
		char *scsi;

		/*
		**  Disable extended Sreq/Sack filtering
		*/
		if ((tp->period <= 2000) && 
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66))
				OUTOFFB (nc_stest2, EXT);

		/*
		**	Bells and whistles   ;-)
		*/
		if	(tp->period < 250)	scsi = "FAST-80";
		else if	(tp->period < 500)	scsi = "FAST-40";
		else if	(tp->period < 1000)	scsi = "FAST-20";
		else if	(tp->period < 2000)	scsi = "FAST-10";
		else				scsi = "FAST-5";

		printk ("%s %sSCSI %d.%d MB/s (%d.%d ns, offset %d)\n", scsi,
			tp->widedone > 1 ? "WIDE " : "",
			mb10 / 10, mb10 % 10, tp->period / 10, tp->period % 10,
			offset);
	} else
		printk ("%sasynchronous.\n", tp->widedone > 1 ? "wide " : "");
next:
	/*
	**	set actual value and sync_status
	**	patch ALL ccbs of this target.
	*/
	ncr_set_sync_wide_status(np, target);
}


/*==========================================================
**
**	Switch wide mode for current job and it's target
**	SCSI specs say: a SCSI device that accepts a WDTR 
**	message shall reset the synchronous agreement to 
**	asynchronous mode.
**
**==========================================================
*/

static void ncr_setwide (ncb_p np, ccb_p cp, u_char wide, u_char ack)
{
	u_short target = INB (nc_sdid) & 0x0f;
	tcb_p tp;
	u_char	scntl3;
	u_char	sxfer;

	assert (cp);
	if (!cp) return;

	assert (target == (cp->target & 0xf));

	tp = &np->target[target];
	tp->widedone  =  wide+1;
	scntl3 = (tp->wval & (~EWS)) | (wide ? EWS : 0);

	sxfer = ack ? 0 : tp->sval;

	/*
	**	 Stop there if sync/wide parameters are unchanged
	*/
	if (tp->sval == sxfer && tp->wval == scntl3) return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	/*
	**	Bells and whistles   ;-)
	*/
	if (bootverbose >= 2) {
		PRINT_TARGET(np, target);
		if (scntl3 & EWS)
			printk ("WIDE SCSI (16 bit) enabled.\n");
		else
			printk ("WIDE SCSI disabled.\n");
	}

	/*
	**	set actual value and sync_status
	**	patch ALL ccbs of this target.
	*/
	ncr_set_sync_wide_status(np, target);
}


/*==========================================================
**
**	Switch sync/wide mode for current job and it's target
**	PPR negotiations only
**
**==========================================================
*/

static void ncr_setsyncwide (ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer, 
				u_char scntl4, u_char wide)
{
	tcb_p tp;
	u_char target = INB (nc_sdid) & 0x0f;
	u_char idiv;
	u_char offset;

	assert (cp);
	if (!cp) return;

	assert (target == (cp->target & 0xf));

	tp = &np->target[target];
	tp->widedone  =  wide+1;

	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
			(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {
		offset = sxfer & 0x3f; /* bits 5-0 */
		scntl3 = (scntl3 & 0xf0) | (wide ? EWS : 0);
		scntl4 = (scntl4 & 0x80);
	}
	else {
		offset = sxfer & 0x1f; /* bits 4-0 */
		if (!scntl3 || !offset)
			scntl3 = np->rv_scntl3;

		scntl3 = (scntl3 & 0xf0) | (wide ? EWS : 0) | 
				(np->rv_scntl3 & 0x07);
	}
	

	/*
	**	Deduce the value of controller sync period from scntl3.
	**	period is in tenths of nano-seconds.
	*/

	idiv = ((scntl3 >> 4) & 0x7);
	if ( offset && idiv) {
		if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
			(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {
			/* Note: If extra data hold clocks are used,
			 * the formulas below must be modified.
			 * When scntl4 == 0, ST mode.
			 */
			if (scntl4 & 0x80)
				tp->period = (2*div_10M[idiv-1])/np->clock_khz;
			else
				tp->period = (4*div_10M[idiv-1])/np->clock_khz;
		}
		else 
			tp->period = (((sxfer>>5)+4)*div_10M[idiv-1])/np->clock_khz;
	}
	else
		tp->period = 0xffff;


	/*
	**	 Stop there if sync parameters are unchanged
	*/
	if (tp->sval == sxfer && tp->wval == scntl3 && tp->uval == scntl4) return;
	tp->sval = sxfer;
	tp->wval = scntl3;
	tp->uval = scntl4;

	/*
	**	Bells and whistles   ;-)
	**	Donnot announce negotiations due to auto-sense, 
	**	unless user really want us to be verbose. :)
	*/
	if ( bootverbose < 2 && (cp->host_flags & HF_AUTO_SENSE))
		goto next;
	PRINT_TARGET(np, target);
	if (offset) {
		unsigned f10 = 100000 << (tp->widedone ? tp->widedone -1 : 0);
		unsigned mb10 = (f10 + tp->period/2) / tp->period;
		char *scsi;

		/*
		**  Disable extended Sreq/Sack filtering
		*/
		if ((tp->period <= 2000) && 
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66))
				OUTOFFB (nc_stest2, EXT);

		/*
		**	Bells and whistles   ;-)
		*/
		if	(tp->period < 250)	scsi = "FAST-80";
		else if	(tp->period < 500)	scsi = "FAST-40";
		else if	(tp->period < 1000)	scsi = "FAST-20";
		else if	(tp->period < 2000)	scsi = "FAST-10";
		else				scsi = "FAST-5";

		printk ("%s %sSCSI %d.%d MB/s (%d.%d ns, offset %d)\n", scsi,
			tp->widedone > 1 ? "WIDE " : "",
			mb10 / 10, mb10 % 10, tp->period / 10, tp->period % 10,
			offset);
	} else
		printk ("%sasynchronous.\n", tp->widedone > 1 ? "wide " : "");
next:
	/*
	**	set actual value and sync_status
	**	patch ALL ccbs of this target.
	*/
	ncr_set_sync_wide_status(np, target);
}




/*==========================================================
**
**	Switch tagged mode for a target.
**
**==========================================================
*/

static void ncr_setup_tags (ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = ncr_lp(np, tp, ln);
	u_short reqtags, maxdepth;

	/*
	**	Just in case ...
	*/
	if ((!tp) || (!lp))
		return;

	/*
	**	If SCSI device queue depth is not yet set, leave here.
	*/
	if (!lp->scdev_depth)
		return;

	/*
	**	Donnot allow more tags than the SCSI driver can queue 
	**	for this device.
	**	Donnot allow more tags than we can handle.
	*/
	maxdepth = lp->scdev_depth;
	if (maxdepth > lp->maxnxs)	maxdepth    = lp->maxnxs;
	if (lp->maxtags > maxdepth)	lp->maxtags = maxdepth;
	if (lp->numtags > maxdepth)	lp->numtags = maxdepth;

	/*
	**	only devices conformant to ANSI Version >= 2
	**	only devices capable of tagged commands
	**	only if enabled by user ..
	*/
	if ((lp->inq_byte7 & INQ7_QUEUE) && lp->numtags > 1) {
		reqtags = lp->numtags;
	} else {
		reqtags = 1;
	};

	/*
	**	Update max number of tags
	*/
	lp->numtags = reqtags;
	if (lp->numtags > lp->maxtags)
		lp->maxtags = lp->numtags;

	/*
	**	If we want to switch tag mode, we must wait 
	**	for no CCB to be active.
	*/
	if	(reqtags > 1 && lp->usetags) {	 /* Stay in tagged mode    */
		if (lp->queuedepth == reqtags)	 /* Already announced	   */
			return;
		lp->queuedepth	= reqtags;
	}
	else if	(reqtags <= 1 && !lp->usetags) { /* Stay in untagged mode  */
		lp->queuedepth	= reqtags;
		return;
	}
	else {					 /* Want to switch tag mode */
		if (lp->busyccbs)		 /* If not yet safe, return */
			return;
		lp->queuedepth	= reqtags;
		lp->usetags	= reqtags > 1 ? 1 : 0;
	}

	/*
	**	Patch the lun mini-script, according to tag mode.
	*/
	lp->resel_task = lp->usetags?
			cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_tag)) :
			cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_notag));

	/*
	**	Announce change to user.
	*/
	if (bootverbose) {
		PRINT_LUN(np, tn, ln);
		if (lp->usetags)
			printk("tagged command queue depth set to %d\n", reqtags);
		else
			printk("tagged command queueing disabled\n");
	}
}

/*----------------------------------------------------
**
**	handle user commands
**
**----------------------------------------------------
*/

#ifdef SCSI_NCR_USER_COMMAND_SUPPORT

static void ncr_usercmd (ncb_p np)
{
	u_char t;
	tcb_p tp;
	int ln;
	u_long size;

	switch (np->user.cmd) {
	case 0: return;

	case UC_SETDEBUG:
#ifdef SCSI_NCR_DEBUG_INFO_SUPPORT
		ncr_debug = np->user.data;
#endif
		break;

	case UC_SETORDER:
		np->order = np->user.data;
		break;

	case UC_SETVERBOSE:
		np->verbose = np->user.data;
		break;

	default:
		/*
		**	We assume that other commands apply to targets.
		**	This should always be the case and avoid the below 
		**	4 lines to be repeated 5 times.
		*/
		for (t = 0; t < MAX_TARGET; t++) {
			if (!((np->user.target >> t) & 1))
				continue;
			tp = &np->target[t];

			switch (np->user.cmd) {

			case UC_SETSYNC:
				tp->usrsync = np->user.data;
				ncr_negotiate (np, tp);
				break;

			case UC_SETWIDE:
				size = np->user.data;
				if (size > np->maxwide)
					size=np->maxwide;
				tp->usrwide = size;
				ncr_negotiate (np, tp);
				break;

			case UC_SETTAGS:
				tp->usrtags = np->user.data;
				for (ln = 0; ln < MAX_LUN; ln++) {
					lcb_p lp;
					lp = ncr_lp(np, tp, ln);
					if (!lp)
						continue;
					lp->numtags = np->user.data;
					lp->maxtags = lp->numtags;
					ncr_setup_tags (np, t, ln);
				}
				break;

			case UC_RESETDEV:
				tp->to_reset = 1;
				np->istat_sem = SEM;
				OUTB (nc_istat, SIGP|SEM);
				break;

			case UC_CLEARDEV:
				for (ln = 0; ln < MAX_LUN; ln++) {
					lcb_p lp;
					lp = ncr_lp(np, tp, ln);
					if (lp)
						lp->to_clear = 1;
				}
				np->istat_sem = SEM;
				OUTB (nc_istat, SIGP|SEM);
				break;

			case UC_SETFLAG:
				tp->usrflag = np->user.data;
				break;
			}
		}
		break;
	}
	np->user.cmd=0;
}
#endif

/*==========================================================
**
**
**	ncr timeout handler.
**
**
**==========================================================
**
**	Misused to keep the driver running when
**	interrupts are not configured correctly.
**
**----------------------------------------------------------
*/

static void ncr_timeout (ncb_p np)
{
	u_long	thistime = ktime_get(0);

	/*
	**	If release process in progress, let's go
	**	Set the release stage from 1 to 2 to synchronize
	**	with the release process.
	*/

	if (np->release_stage) {
		if (np->release_stage == 1) np->release_stage = 2;
		return;
	}

#ifdef SCSI_NCR_PCIQ_BROKEN_INTR
	np->timer.expires = ktime_get((HZ+9)/10);
#else
	np->timer.expires = ktime_get(SCSI_NCR_TIMER_INTERVAL);
#endif
	add_timer(&np->timer);

	/*
	**	If we are resetting the ncr, wait for settle_time before 
	**	clearing it. Then command processing will be resumed.
	*/
	if (np->settle_time) {
		if (np->settle_time <= thistime) {
			if (bootverbose > 1)
				printk("%s: command processing resumed\n", ncr_name(np));
			np->settle_time	= 0;
			requeue_waiting_list(np);
		}
		return;
	}

	/*
	**	Nothing to do for now, but that may come.
	*/
	if (np->lasttime + 4*HZ < thistime) {
		np->lasttime = thistime;
	}

#ifdef SCSI_NCR_PCIQ_MAY_MISS_COMPLETIONS
	/*
	**	Some way-broken PCI bridges may lead to 
	**	completions being lost when the clearing 
	**	of the INTFLY flag by the CPU occurs 
	**	concurrently with the chip raising this flag.
	**	If this ever happen, lost completions will 
	**	be reaped here.
	*/
	ncr_wakeup_done(np);
#endif

#ifdef SCSI_NCR_PCIQ_BROKEN_INTR
	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		/*
		**	Process pending interrupts.
		*/
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("{");
		ncr_exception (np);
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("}");
	}
#endif /* SCSI_NCR_PCIQ_BROKEN_INTR */
}

/*==========================================================
**
**	log message for real hard errors
**
**	"ncr0 targ 0?: ERROR (ds:si) (so-si-sd) (sxfer/scntl3) @ name (dsp:dbc)."
**	"	      reg: r0 r1 r2 r3 r4 r5 r6 ..... rf."
**
**	exception register:
**		ds:	dstat
**		si:	sist
**
**	SCSI bus lines:
**		so:	control lines as driver by NCR.
**		si:	control lines as seen by NCR.
**		sd:	scsi data lines as seen by NCR.
**
**	wide/fastmode:
**		sxfer:	(see the manual)
**		scntl3:	(see the manual)
**
**	current script command:
**		dsp:	script address (relative to start of script).
**		dbc:	first word of script command.
**
**	First 24 register of the chip:
**		r0..rf
**
**==========================================================
*/

static void ncr_log_hard_error(ncb_p np, u_short sist, u_char dstat)
{
	u_int32	dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL (nc_dsp);

	if (dsp > np->p_script && dsp <= np->p_script + sizeof(struct script)) {
		script_ofs	= dsp - np->p_script;
		script_size	= sizeof(struct script);
		script_base	= (u_char *) np->script0;
		script_name	= "script";
	}
	else if (np->p_scripth < dsp && 
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		script_ofs	= dsp - np->p_scripth;
		script_size	= sizeof(struct scripth);
		script_base	= (u_char *) np->scripth0;
		script_name	= "scripth";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= 0;
		script_name	= "mem";
	}

	printk ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%s %x:%08x).\n",
		ncr_name (np), (unsigned)INB (nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB (nc_socl), (unsigned)INB (nc_sbcl), (unsigned)INB (nc_sbdl),
		(unsigned)INB (nc_sxfer),(unsigned)INB (nc_scntl3), script_name, script_ofs,
		(unsigned)INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printk ("%s: script cmd = %08x\n", ncr_name(np),
			scr_to_cpu((int) *(ncrcmd *)(script_base + script_ofs)));
	}

        printk ("%s: regdump:", ncr_name(np));
        for (i=0; i<24;i++)
            printk (" %02x", (unsigned)INB_OFF(i));
        printk (".\n");
}

/*============================================================
**
**	ncr chip exception handler.
**
**============================================================
**
**	In normal situations, interrupt conditions occur one at 
**	a time. But when something bad happens on the SCSI BUS, 
**	the chip may raise several interrupt flags before 
**	stopping and interrupting the CPU. The additionnal 
**	interrupt flags are stacked in some extra registers 
**	after the SIP and/or DIP flag has been raised in the 
**	ISTAT. After the CPU has read the interrupt condition 
**	flag from SIST or DSTAT, the chip unstacks the other 
**	interrupt flags and sets the corresponding bits in 
**	SIST or DSTAT. Since the chip starts stacking once the 
**	SIP or DIP flag is set, there is a small window of time 
**	where the stacking does not occur.
**
**	Typically, multiple interrupt conditions may happen in 
**	the following situations:
**
**	- SCSI parity error + Phase mismatch  (PAR|MA)
**	  When an parity error is detected in input phase 
**	  and the device switches to msg-in phase inside a 
**	  block MOV.
**	- SCSI parity error + Unexpected disconnect (PAR|UDC)
**	  When a stupid device does not want to handle the 
**	  recovery of an SCSI parity error.
**	- Some combinations of STO, PAR, UDC, ...
**	  When using non compliant SCSI stuff, when user is 
**	  doing non compliant hot tampering on the BUS, when 
**	  something really bad happens to a device, etc ...
**
**	The heuristic suggested by SYMBIOS to handle 
**	multiple interrupts is to try unstacking all 
**	interrupts conditions and to handle them on some 
**	priority based on error severity.
**	This will work when the unstacking has been 
**	successful, but we cannot be 100 % sure of that, 
**	since the CPU may have been faster to unstack than 
**	the chip is able to stack. Hmmm ... But it seems that 
**	such a situation is very unlikely to happen.
**
**	If this happen, for example STO catched by the CPU 
**	then UDC happenning before the CPU have restarted 
**	the SCRIPTS, the driver may wrongly complete the 
**	same command on UDC, since the SCRIPTS didn't restart 
**	and the DSA still points to the same command.
**	We avoid this situation by setting the DSA to an 
**	invalid value when the CCB is completed and before 
**	restarting the SCRIPTS.
**
**	Another issue is that we need some section of our 
**	recovery procedures to be somehow uninterruptible and 
**	that the SCRIPTS processor does not provides such a 
**	feature. For this reason, we handle recovery preferently 
**	from the C code	and check against some SCRIPTS 
**	critical sections from the C code.
**
**	Hopefully, the interrupt handling of the driver is now 
**	able to resist to weird BUS error conditions, but donnot 
**	ask me for any guarantee that it will never fail. :-)
**	Use at your own decision and risk.
**
**============================================================
*/

void ncr_exception (ncb_p np)
{
	u_char	istat, istatc;
	u_char	dstat;
	u_short	sist;
	int	i;

	/*
	**	interrupt on the fly ?
	**
	**	A `dummy read' is needed to ensure that the 
	**	clear of the INTF flag reaches the device 
	**	before the scanning of the DONE queue.
	*/
	istat = INB (nc_istat);
	if (istat & INTF) {
		OUTB (nc_istat, (istat & SIGP) | INTF | np->istat_sem);
		istat = INB (nc_istat);		/* DUMMY READ */
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("F ");
		(void)ncr_wakeup_done (np);
	};

	if (!(istat & (SIP|DIP)))
		return;

#if 0	/* We should never get this one */
	if (istat & CABRT)
		OUTB (nc_istat, CABRT);
#endif

	/*
	**	Steinbach's Guideline for Systems Programming:
	**	Never test for an error condition you don't know how to handle.
	*/

	/*========================================================
	**	PAR and MA interrupts may occur at the same time,
	**	and we need to know of both in order to handle 
	**	this situation properly. We try to unstack SCSI 
	**	interrupts for that reason. BTW, I dislike a LOT 
	**	such a loop inside the interrupt routine.
	**	Even if DMA interrupt stacking is very unlikely to 
	**	happen, we also try unstacking these ones, since 
	**	this has no performance impact.
	**=========================================================
	*/
	sist	= 0;
	dstat	= 0;
	istatc	= istat;
	do {
		if (istatc & SIP)
			sist  |= INW (nc_sist);
		if (istatc & DIP)
			dstat |= INB (nc_dstat);
		istatc = INB (nc_istat);
		istat |= istatc;
	} while (istatc & (SIP|DIP));

	if (DEBUG_FLAGS & DEBUG_TINY)
		printk ("<%d|%x:%x|%x:%x>",
			(int)INB(nc_scr0),
			dstat,sist,
			(unsigned)INL(nc_dsp),
			(unsigned)INL(nc_dbc));

	/*
	**	On paper, a memory barrier may be needed here.
	**	And since we are paranoid ... :)
	*/
	MEMORY_BARRIER();

	/*========================================================
	**	First, interrupts we want to service cleanly.
	**
	**	Phase mismatch (MA) is the most frequent interrupt 
	**	for chip earlier than the 896 and so we have to service 
	**	it as quickly as possible.
	**	A SCSI parity error (PAR) may be combined with a phase 
	**	mismatch condition (MA).
	**	Programmed interrupts (SIR) are used to call the C code 
	**	from SCRIPTS.
	**	The single step interrupt (SSI) is not used in this 
	**	driver.
	**=========================================================
	*/

	if (!(sist  & (STO|GEN|HTH|SGE|UDC|SBMC|RST)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & PAR)	ncr_int_par (np, sist);
		else if (sist & MA)	ncr_int_ma (np);
		else if (dstat & SIR)	ncr_int_sir (np);
		else if (dstat & SSI)	OUTONB_STD ();
		else			goto unknown_int;
		return;
	};

	/*========================================================
	**	Now, interrupts that donnot happen in normal 
	**	situations and that we may need to recover from.
	**
	**	On SCSI RESET (RST), we reset everything.
	**	On SCSI BUS MODE CHANGE (SBMC), we complete all 
	**	active CCBs with RESET status, prepare all devices 
	**	for negotiating again and restart the SCRIPTS.
	**	On STO and UDC, we complete the CCB with the corres- 
	**	ponding status and restart the SCRIPTS.
	**=========================================================
	*/

	if (sist & RST) {
		ncr_init (np, 1, bootverbose ? "scsi reset" : NULL, HS_RESET);
		return;
	};

	OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);		/* clear scsi fifo */

	if (!(sist  & (GEN|HTH|SGE)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & SBMC)	ncr_int_sbmc (np);
		else if (sist & STO)	ncr_int_sto (np);
		else if (sist & UDC)	ncr_int_udc (np);
		else			goto unknown_int;
		return;
	};

	/*=========================================================
	**	Now, interrupts we are not able to recover cleanly.
	**
	**	Do the register dump.
	**	Log message for hard errors.
	**	Reset everything.
	**=========================================================
	*/
	if (ktime_exp(np->regtime)) {
		np->regtime = ktime_get(10*HZ);
		for (i = 0; i<sizeof(np->regdump); i++)
			((char*)&np->regdump)[i] = INB_OFF(i);
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	};

	ncr_log_hard_error(np, sist, dstat);

	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
		(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) {
		u_char ctest4_o, ctest4_m;
		u_char shadow;

		/* 
		 * Get shadow register data 
		 * Write 1 to ctest4
		 */
		ctest4_o = INB(nc_ctest4);

		OUTB(nc_ctest4, ctest4_o | 0x10);
		
		ctest4_m = INB(nc_ctest4);
		shadow = INW_OFF(0x42);

		OUTB(nc_ctest4, ctest4_o);

		printk("%s: ctest4/sist original 0x%x/0x%X  mod: 0x%X/0x%x\n", 
			ncr_name(np), ctest4_o, sist, ctest4_m, shadow);
	}

	if ((sist & (GEN|HTH|SGE)) ||
		(dstat & (MDPE|BF|ABRT|IID))) {
		ncr_start_reset(np);
		return;
	};

unknown_int:
	/*=========================================================
	**	We just miss the cause of the interrupt. :(
	**	Print a message. The timeout will do the real work.
	**=========================================================
	*/
	printk(	"%s: unknown interrupt(s) ignored, "
		"ISTAT=0x%x DSTAT=0x%x SIST=0x%x\n",
		ncr_name(np), istat, dstat, sist);
}


/*==========================================================
**
**	generic recovery from scsi interrupt
**
**==========================================================
**
**	The doc says that when the chip gets an SCSI interrupt,
**	it tries to stop in an orderly fashion, by completing 
**	an instruction fetch that had started or by flushing 
**	the DMA fifo for a write to memory that was executing.
**	Such a fashion is not enough to know if the instruction 
**	that was just before the current DSP value has been 
**	executed or not.
**
**	There are 3 small SCRIPTS sections that deal with the 
**	start queue and the done queue that may break any 
**	assomption from the C code if we are interrupted 
**	inside, so we reset if it happens. Btw, since these 
**	SCRIPTS sections are executed while the SCRIPTS hasn't 
**	started SCSI operations, it is very unlikely to happen.
**
**	All the driver data structures are supposed to be 
**	allocated from the same 4 GB memory window, so there 
**	is a 1 to 1 relationship between DSA and driver data 
**	structures. Since we are careful :) to invalidate the 
**	DSA when we complete a command or when the SCRIPTS 
**	pushes a DSA into a queue, we can trust it when it 
**	points to a CCB.
**
**----------------------------------------------------------
*/
static void ncr_recover_scsi_int (ncb_p np, u_char hsts)
{
	u_int32	dsp	= INL (nc_dsp);
	u_int32	dsa	= INL (nc_dsa);
	ccb_p cp	= ncr_ccb_from_dsa(np, dsa);

	/*
	**	If we haven't been interrupted inside the SCRIPTS 
	**	critical pathes, we can safely restart the SCRIPTS 
	**	and trust the DSA value if it matches a CCB.
	*/
	if ((!(dsp > NCB_SCRIPT_PHYS (np, getjob_begin) &&
	       dsp < NCB_SCRIPT_PHYS (np, getjob_end) + 1)) &&
	    (!(dsp > NCB_SCRIPT_PHYS (np, ungetjob) &&
	       dsp < NCB_SCRIPT_PHYS (np, reselect) + 1)) &&
	    (!(dsp > NCB_SCRIPTH_PHYS (np, sel_for_abort) &&
	       dsp < NCB_SCRIPTH_PHYS (np, sel_for_abort_1) + 1)) &&
	    (!(dsp > NCB_SCRIPT_PHYS (np, done) &&
	       dsp < NCB_SCRIPT_PHYS (np, done_end) + 1))) {
		if (cp) {
			cp->host_status = hsts;
			ncr_complete (np, cp);
		}
		OUTL (nc_dsa, DSA_INVALID);
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
		OUTB (nc_stest3, TE|CSF);		/* clear scsi fifo */
		OUTL_DSP (NCB_SCRIPT_PHYS (np, start));
	}
	else
		goto reset_all;

	return;

reset_all:
	ncr_start_reset(np);
}

/*==========================================================
**
**	ncr chip exception handler for selection timeout
**
**==========================================================
**
**	There seems to be a bug in the 53c810.
**	Although a STO-Interrupt is pending,
**	it continues executing script commands.
**	But it will fail and interrupt (IID) on
**	the next instruction where it's looking
**	for a valid phase.
**
**----------------------------------------------------------
*/

void ncr_int_sto (ncb_p np)
{
	u_int32	dsp	= INL (nc_dsp);

	if (DEBUG_FLAGS & DEBUG_TINY) printk ("T");

	if (dsp == NCB_SCRIPT_PHYS (np, wf_sel_done) + 8 ||
	    !(driver_setup.recovery & 1))
		ncr_recover_scsi_int(np, HS_SEL_TIMEOUT);
	else
		ncr_start_reset(np);
}

/*==========================================================
**
**	ncr chip exception handler for unexpected disconnect
**
**==========================================================
**
**----------------------------------------------------------
*/
void ncr_int_udc (ncb_p np)
{
	u_int32 dsa = INL (nc_dsa);
	ccb_p   cp  = ncr_ccb_from_dsa(np, dsa);

	/*
	 * Fix Up. Some disks respond to a PPR negotation with
	 * a bus free instead of a message reject. 
	 * Disable ppr negotiation if this is first time
	 * tried ppr negotiation.
	 */
	if (cp) {
		tcb_p tp = &np->target[cp->target];
		if (tp->ppr_negotiation == 1)
			tp->ppr_negotiation = 0;
	}
	
	printk ("%s: unexpected disconnect\n", ncr_name(np));
	ncr_recover_scsi_int(np, HS_UNEXPECTED);
}

/*==========================================================
**
**	ncr chip exception handler for SCSI bus mode change
**
**==========================================================
**
**	spi2-r12 11.2.3 says a transceiver mode change must 
**	generate a reset event and a device that detects a reset 
**	event shall initiate a hard reset. It says also that a
**	device that detects a mode change shall set data transfer 
**	mode to eight bit asynchronous, etc...
**	So, just resetting should be enough.
**	 
**
**----------------------------------------------------------
*/

static void ncr_int_sbmc (ncb_p np)
{
	u_char scsi_mode = INB (nc_stest4) & SMODE;

	printk("%s: SCSI bus mode change from %x to %x.\n",
		ncr_name(np), np->scsi_mode, scsi_mode);

	np->scsi_mode = scsi_mode;


	/*
	**	Suspend command processing for 1 second and 
	**	reinitialize all except the chip.
	*/
	np->settle_time	= ktime_get(1*HZ);
	ncr_init (np, 0, bootverbose ? "scsi mode change" : NULL, HS_RESET);
}

/*==========================================================
**
**	ncr chip exception handler for SCSI parity error.
**
**==========================================================
**
**	When the chip detects a SCSI parity error and is 
**	currently executing a (CH)MOV instruction, it does 
**	not interrupt immediately, but tries to finish the 
**	transfer of the current scatter entry before 
**	interrupting. The following situations may occur:
**
**	- The complete scatter entry has been transferred 
**	  without the device having changed phase.
**	  The chip will then interrupt with the DSP pointing 
**	  to the instruction that follows the MOV.
**
**	- A phase mismatch occurs before the MOV finished 
**	  and phase errors are to be handled by the C code.
**	  The chip will then interrupt with both PAR and MA 
**	  conditions set.
**
**	- A phase mismatch occurs before the MOV finished and 
**	  phase errors are to be handled by SCRIPTS (895A or 896).
**	  The chip will load the DSP with the phase mismatch 
**	  JUMP address and interrupt the host processor.
**
**----------------------------------------------------------
*/

static void ncr_int_par (ncb_p np, u_short sist)
{
	u_char	hsts	= INB (HS_PRT);
	u_int32	dsp	= INL (nc_dsp);
	u_int32	dbc	= INL (nc_dbc);
	u_int32	dsa	= INL (nc_dsa);
	u_char	sbcl	= INB (nc_sbcl);
	u_char	cmd	= dbc >> 24;
	int phase	= cmd & 7;
	ccb_p	cp	= ncr_ccb_from_dsa(np, dsa);

	printk("%s: SCSI parity error detected: SCR1=%d DBC=%x SBCL=%x\n",
		ncr_name(np), hsts, dbc, sbcl);

	/*
	**	Check that the chip is connected to the SCSI BUS.
	*/
	if (!(INB (nc_scntl1) & ISCON)) {
	    	if (!(driver_setup.recovery & 1)) {
			ncr_recover_scsi_int(np, HS_FAIL);
			return;
		}
		goto reset_all;
	}

	/*
	**	If the nexus is not clearly identified, reset the bus.
	**	We will try to do better later.
	*/
	if (!cp)
		goto reset_all;

	/*
	**	Check instruction was a MOV, direction was INPUT and 
	**	ATN is asserted.
	*/
	if ((cmd & 0xc0) || !(phase & 1) || !(sbcl & 0x8))
		goto reset_all;

	/*
	**	Keep track of the parity error.
	*/
	OUTONB (HF_PRT, HF_EXT_ERR);
	cp->xerr_status |= XE_PARITY_ERR;

	/*
	**	Prepare the message to send to the device.
	*/
	np->msgout[0] = (phase == 7) ? M_PARITY : M_ID_ERROR;

#ifdef	SCSI_NCR_INTEGRITY_CHECKING
	/*
	**	Save error message. For integrity check use only.
	*/
	if (np->check_integrity) 
		np->check_integ_par = np->msgout[0];
#endif

	/*
	**	If the old phase was DATA IN or DT DATA IN phase, 
	** 	we have to deal with the 3 situations described above.
	**	For other input phases (MSG IN and STATUS), the device 
	**	must resend the whole thing that failed parity checking 
	**	or signal error. So, jumping to dispatcher should be OK.
	*/
	if ((phase == 1) || (phase == 5)) {
		/* Phase mismatch handled by SCRIPTS */
		if (dsp == NCB_SCRIPTH_PHYS (np, pm_handle))
			OUTL_DSP (dsp);
		/* Phase mismatch handled by the C code */
		else if (sist & MA)
			ncr_int_ma (np);
		/* No phase mismatch occurred */
		else {
			OUTL (nc_temp, dsp);
			OUTL_DSP (NCB_SCRIPT_PHYS (np, dispatch));
		}
	}
	else 
		OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
	return;

reset_all:
	ncr_start_reset(np);
	return;
}

/*==========================================================
**
**
**	ncr chip exception handler for phase errors.
**
**
**==========================================================
**
**	We have to construct a new transfer descriptor,
**	to transfer the rest of the current block.
**
**----------------------------------------------------------
*/

static void ncr_int_ma (ncb_p np)
{
	u_int32	dbc;
	u_int32	rest;
	u_int32	dsp;
	u_int32	dsa;
	u_int32	nxtdsp;
	u_int32	*vdsp;
	u_int32	oadr, olen;
	u_int32	*tblp;
        u_int32	newcmd;
	u_int	delta;
	u_char	cmd;
	u_char	hflags, hflags0;
	struct pm_ctx *pm;
	ccb_p	cp;

	dsp	= INL (nc_dsp);
	dbc	= INL (nc_dbc);
	dsa	= INL (nc_dsa);

	cmd	= dbc >> 24;
	rest	= dbc & 0xffffff;
	delta	= 0;

	/*
	**	locate matching cp.
	*/
	cp = ncr_ccb_from_dsa(np, dsa);

	if (DEBUG_FLAGS & DEBUG_PHASE)
		printk("CCB = %2x %2x %2x %2x %2x %2x\n", 
			cp->cmd->cmnd[0], cp->cmd->cmnd[1], cp->cmd->cmnd[2],
			cp->cmd->cmnd[3], cp->cmd->cmnd[4], cp->cmd->cmnd[5]);

	/*
	**	Donnot take into account dma fifo and various buffers in 
	**	INPUT phase since the chip flushes everything before 
	**	raising the MA interrupt for interrupted INPUT phases.
	**	For DATA IN phase, we will check for the SWIDE later.
	*/
	if ((cmd & 7) != 1 && (cmd & 7) != 5) {
		u_int32 dfifo;
		u_char ss0, ss2;

		/*
		**  If C1010, DFBC contains number of bytes in DMA fifo.
		**  else read DFIFO, CTEST[4-6] using 1 PCI bus ownership.
		*/
		if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
				(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)) 
			delta = INL(nc_dfbc) & 0xffff;
		else {
			dfifo = INL(nc_dfifo);

			/*
			**	Calculate remaining bytes in DMA fifo.
			**	C1010 - always large fifo, value in dfbc
			**	Otherwise, (CTEST5 = dfifo >> 16)
			*/
			if (dfifo & (DFS << 16))
				delta = ((((dfifo >> 8) & 0x300) |
				          (dfifo & 0xff)) - rest) & 0x3ff;
			else
				delta = ((dfifo & 0xff) - rest) & 0x7f;

			/*
			**	The data in the dma fifo has not been 
			**	transferred to the target -> add the amount 
			**	to the rest and clear the data.
			**	Check the sstat2 register in case of wide
			**	transfer.
			*/

		}
		
		rest += delta;
		ss0  = INB (nc_sstat0);
		if (ss0 & OLF) rest++;
		if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) && 
				(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66) && (ss0 & ORF)) 
			rest++;
		if (cp && (cp->phys.select.sel_scntl3 & EWS)) {
			ss2 = INB (nc_sstat2);
			if (ss2 & OLF1) rest++;
			if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
					(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66) && (ss2 & ORF)) 
				rest++;
		};

		/*
		**	Clear fifos.
		*/
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* dma fifo  */
		OUTB (nc_stest3, TE|CSF);		/* scsi fifo */
	}

	/*
	**	log the information
	*/

	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
		printk ("P%x%x RL=%d D=%d ", cmd&7, INB(nc_sbcl)&7,
			(unsigned) rest, (unsigned) delta);

	/*
	**	try to find the interrupted script command,
	**	and the address at which to continue.
	*/
	vdsp	= 0;
	nxtdsp	= 0;
	if	(dsp >  np->p_script &&
		 dsp <= np->p_script + sizeof(struct script)) {
		vdsp = (u_int32 *)((char*)np->script0 + (dsp-np->p_script-8));
		nxtdsp = dsp;
	}
	else if	(dsp >  np->p_scripth &&
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		vdsp = (u_int32 *)((char*)np->scripth0 + (dsp-np->p_scripth-8));
		nxtdsp = dsp;
	}

	/*
	**	log the information
	*/
	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printk ("\nCP=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, (unsigned)dsp, (unsigned)nxtdsp, vdsp, cmd);
	};

	if (!vdsp) {
		printk ("%s: interrupted SCRIPT address not found.\n", 
			ncr_name (np));
		goto reset_all;
	}

	if (!cp) {
		printk ("%s: SCSI phase error fixup: CCB already dequeued.\n", 
			ncr_name (np));
		goto reset_all;
	}

	/*
	**	get old startaddress and old length.
	*/

	oadr = scr_to_cpu(vdsp[1]);

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u_int32 *) ((char*) &cp->phys + oadr);
		olen = scr_to_cpu(tblp[0]);
		oadr = scr_to_cpu(tblp[1]);
	} else {
		tblp = (u_int32 *) 0;
		olen = scr_to_cpu(vdsp[0]) & 0xffffff;
	};

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printk ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (scr_to_cpu(vdsp[0]) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	};

	/*
	**	check cmd against assumed interrupted script command.
	**	If dt data phase, the MOVE instruction hasn't bit 4 of 
	**	the phase.
	*/

	if (((cmd & 2) ? cmd : (cmd & ~4)) != (scr_to_cpu(vdsp[0]) >> 24)) {
		PRINT_ADDR(cp->cmd);
		printk ("internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			(unsigned)cmd, (unsigned)scr_to_cpu(vdsp[0]) >> 24);

		goto reset_all;
	};

	/*
	**	if old phase not dataphase, leave here.
	**	C/D line is low if data.
	*/

	if (cmd & 0x02) {
		PRINT_ADDR(cp->cmd);
		printk ("phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, INB(nc_sbcl)&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);
		goto unexpected_phase;
	};

	/*
	**	Choose the correct PM save area.
	**
	**	Look at the PM_SAVE SCRIPT if you want to understand 
	**	this stuff. The equivalent code is implemented in 
	**	SCRIPTS for the 895A and 896 that are able to handle 
	**	PM from the SCRIPTS processor.
	*/

	hflags0 = INB (HF_PRT);
	hflags = hflags0;

	if (hflags & (HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED)) {
		if (hflags & HF_IN_PM0)
			nxtdsp = scr_to_cpu(cp->phys.pm0.ret);
		else if	(hflags & HF_IN_PM1)
			nxtdsp = scr_to_cpu(cp->phys.pm1.ret);

		if (hflags & HF_DP_SAVED)
			hflags ^= HF_ACT_PM;
	}

	if (!(hflags & HF_ACT_PM)) {
		pm = &cp->phys.pm0;
		newcmd = NCB_SCRIPT_PHYS(np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		newcmd = NCB_SCRIPT_PHYS(np, pm1_data);
	}

	hflags &= ~(HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED);
	if (hflags != hflags0)
		OUTB (HF_PRT, hflags);

	/*
	**	fillin the phase mismatch context
	*/

	pm->sg.addr = cpu_to_scr(oadr + olen - rest);
	pm->sg.size = cpu_to_scr(rest);
	pm->ret     = cpu_to_scr(nxtdsp);

	/*
	**	If we have a SWIDE,
	**	- prepare the address to write the SWIDE from SCRIPTS,
	**	- compute the SCRIPTS address to restart from,
	**	- move current data pointer context by one byte.
	*/
	nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
	if ( ((cmd & 7) == 1  || (cmd & 7) == 5)  
		&& cp && (cp->phys.select.sel_scntl3 & EWS) &&
	    (INB (nc_scntl2) & WSR)) {
		u32 tmp;

#ifdef  SYM_DEBUG_PM_WITH_WSR
		PRINT_ADDR(cp);
		printk ("MA interrupt with WSR set - "
			"pm->sg.addr=%x - pm->sg.size=%d\n",
			pm->sg.addr, pm->sg.size);
#endif
		/*
		 *  Set up the table indirect for the MOVE
		 *  of the residual byte and adjust the data
		 *  pointer context.
		 */
		tmp = scr_to_cpu(pm->sg.addr);
		cp->phys.wresid.addr = cpu_to_scr(tmp);
		pm->sg.addr = cpu_to_scr(tmp + 1);
 		tmp = scr_to_cpu(pm->sg.size);
		cp->phys.wresid.size = cpu_to_scr((tmp&0xff000000) | 1);
		pm->sg.size = cpu_to_scr(tmp - 1);

		/*
		 *  If only the residual byte is to be moved,
		 *  no PM context is needed.
		 */
		if ((tmp&0xffffff) == 1)
                        newcmd = pm->ret;

		/*
		 *  Prepare the address of SCRIPTS that will
		 *  move the residual byte to memory.
		 */
		nxtdsp = NCB_SCRIPTH_PHYS (np, wsr_ma_helper);
        }

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		PRINT_ADDR(cp->cmd);
		printk ("PM %x %x %x / %x %x %x.\n",
			hflags0, hflags, newcmd,
			(unsigned)scr_to_cpu(pm->sg.addr),
			(unsigned)scr_to_cpu(pm->sg.size),
			(unsigned)scr_to_cpu(pm->ret));
	}

	/*
	**	Restart the SCRIPTS processor.
	*/

	OUTL (nc_temp, newcmd);
	OUTL_DSP (nxtdsp);
	return;

	/*
	**	Unexpected phase changes that occurs when the current phase 
	**	is not a DATA IN or DATA OUT phase are due to error conditions.
	**	Such event may only happen when the SCRIPTS is using a 
	**	multibyte SCSI MOVE.
	**
	**	Phase change		Some possible cause
	**
	**	COMMAND  --> MSG IN	SCSI parity error detected by target.
	**	COMMAND  --> STATUS	Bad command or refused by target.
	**	MSG OUT  --> MSG IN     Message rejected by target.
	**	MSG OUT  --> COMMAND    Bogus target that discards extended
	**				negotiation messages.
	**
	**	The code below does not care of the new phase and so 
	**	trusts the target. Why to annoy it ?
	**	If the interrupted phase is COMMAND phase, we restart at
	**	dispatcher.
	**	If a target does not get all the messages after selection, 
	**	the code assumes blindly that the target discards extended 
	**	messages and clears the negotiation status.
	**	If the target does not want all our response to negotiation,
	**	we force a SIR_NEGO_PROTO interrupt (it is a hack that avoids 
	**	bloat for such a should_not_happen situation).
	**	In all other situation, we reset the BUS.
	**	Are these assumptions reasonnable ? (Wait and see ...)
	*/
unexpected_phase:
	dsp -= 8;
	nxtdsp = 0;

	switch (cmd & 7) {
	case 2:	/* COMMAND phase */
		nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
		break;
#if 0
	case 3:	/* STATUS  phase */
		nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
		break;
#endif
	case 6:	/* MSG OUT phase */
		/*
		**	If the device may want to use untagged when we want 
		**	tagged, we prepare an IDENTIFY without disc. granted, 
		**	since we will not be able to handle reselect.
		**	Otherwise, we just don't care.
		*/
		if	(dsp == NCB_SCRIPT_PHYS (np, send_ident)) {
			if (cp->tag != NO_TAG && olen - rest <= 3) {
				cp->host_status = HS_BUSY;
				np->msgout[0] = M_IDENTIFY | cp->lun;
				nxtdsp = NCB_SCRIPTH_PHYS (np, ident_break_atn);
			}
			else
				nxtdsp = NCB_SCRIPTH_PHYS (np, ident_break);
		}
		else if	(dsp == NCB_SCRIPTH_PHYS (np, send_wdtr) ||
			 dsp == NCB_SCRIPTH_PHYS (np, send_sdtr) ||
			 dsp == NCB_SCRIPTH_PHYS (np, send_ppr)) {
			nxtdsp = NCB_SCRIPTH_PHYS (np, nego_bad_phase);
		}
		break;
#if 0
	case 7:	/* MSG IN  phase */
		nxtdsp = NCB_SCRIPT_PHYS (np, clrack);
		break;
#endif
	}

	if (nxtdsp) {
		OUTL_DSP (nxtdsp);
		return;
	}

reset_all:
	ncr_start_reset(np);
}

/*==========================================================
**
**	ncr chip handler for QUEUE FULL and CHECK CONDITION
**
**==========================================================
**
**	On QUEUE FULL status, we set the actual tagged command 
**	queue depth to the number of disconnected CCBs that is 
**	hopefully a good value to avoid further QUEUE FULL.
**
**	On CHECK CONDITION or COMMAND TERMINATED, we use the  
**	CCB of the failed command for performing a REQUEST 
**	SENSE SCSI command.
**
**	We do not want to change the order commands will be 
**	actually queued to the device after we received a 
**	QUEUE FULL status. We also want to properly deal with 
**	contingent allegiance condition. For these reasons, 
**	we remove from the start queue all commands for this 
**	LUN that haven't been yet queued to the device and 
**	put them back in the correponding LUN queue, then  
**	requeue the CCB that failed in front of the LUN queue.
**	I just hope this not to be performed too often. :)
**
**	If we are using IMMEDIATE ARBITRATION, we clear the 
**	IARB hint for every commands we encounter in order not 
**	to be stuck with a won arbitration and no job to queue 
**	to a device.
**----------------------------------------------------------
*/

static void ncr_sir_to_redo(ncb_p np, int num, ccb_p cp)
{
	Scsi_Cmnd *cmd	= cp->cmd;
	tcb_p tp	= &np->target[cp->target];
	lcb_p lp	= ncr_lp(np, tp, cp->lun);
	ccb_p		cp2;
	int		busyccbs = 1;
	u_int32		startp;
	u_char		s_status = INB (SS_PRT);
	int		msglen;
	int		i, j;


	/*
	**	If the LCB is not yet available, then only 
	**	1 IO is accepted, so we should have it.
	*/
	if (!lp)
		goto next;	
	/*
	**	Remove all CCBs queued to the chip for that LUN and put 
	**	them back in the LUN CCB wait queue.
	*/
	busyccbs = lp->queuedccbs;
	i = (INL (nc_scratcha) - np->p_squeue) / 4;
	j = i;
	while (i != np->squeueput) {
		cp2 = ncr_ccb_from_dsa(np, scr_to_cpu(np->squeue[i]));
		assert(cp2);
#ifdef SCSI_NCR_IARB_SUPPORT
		/* IARB hints may not be relevant any more. Forget them. */
		cp2->host_flags &= ~HF_HINT_IARB;
#endif
		if (cp2 && cp2->target == cp->target && cp2->lun == cp->lun) {
			xpt_remque(&cp2->link_ccbq);
			xpt_insque_head(&cp2->link_ccbq, &lp->wait_ccbq);
			--lp->queuedccbs;
			cp2->queued = 0;
		}
		else {
			if (i != j)
				np->squeue[j] = np->squeue[i];
			if ((j += 2) >= MAX_START*2) j = 0;
		}
		if ((i += 2) >= MAX_START*2) i = 0;
	}
	if (i != j)		/* Copy back the idle task if needed */
		np->squeue[j] = np->squeue[i];
	np->squeueput = j;	/* Update our current start queue pointer */

	/*
	**	Requeue the interrupted CCB in front of the 
	**	LUN CCB wait queue to preserve ordering.
	*/
	xpt_remque(&cp->link_ccbq);
	xpt_insque_head(&cp->link_ccbq, &lp->wait_ccbq);
	--lp->queuedccbs;
	cp->queued = 0;

next:

#ifdef SCSI_NCR_IARB_SUPPORT
	/* IARB hint may not be relevant any more. Forget it. */
	cp->host_flags &= ~HF_HINT_IARB;
	if (np->last_cp)
		np->last_cp = 0;
#endif

	/*
	**	Now we can restart the SCRIPTS processor safely.
	*/
	OUTL_DSP (NCB_SCRIPT_PHYS (np, start));

	switch(s_status) {
	default:
	case S_BUSY:
		ncr_complete(np, cp);
		break;
	case S_QUEUE_FULL:
		if (!lp || !lp->queuedccbs) {
			ncr_complete(np, cp);
			break;
		}
		if (bootverbose >= 1) {
			PRINT_ADDR(cmd);
			printk ("QUEUE FULL! %d busy, %d disconnected CCBs\n",
				busyccbs, lp->queuedccbs);
		}
		/*
		**	Decrease number of tags to the number of 
		**	disconnected commands.
		*/
		if (lp->queuedccbs < lp->numtags) {
			lp->numtags	= lp->queuedccbs;
			lp->num_good	= 0;
			ncr_setup_tags (np, cp->target, cp->lun);
		}
		/*
		**	Repair the offending CCB.
		*/
		cp->phys.header.savep	= cp->startp;
		cp->phys.header.lastp	= cp->lastp0;
		cp->host_status 	= HS_BUSY;
		cp->scsi_status 	= S_ILLEGAL;
		cp->xerr_status		= 0;
		cp->extra_bytes		= 0;
		cp->host_flags		&= (HF_PM_TO_C|HF_DATA_IN);

		break;

	case S_TERMINATED:
	case S_CHECK_COND:
		/*
		**	If we were requesting sense, give up.
		*/
		if (cp->host_flags & HF_AUTO_SENSE) {
			ncr_complete(np, cp);
			break;
		}

		/*
		**	Save SCSI status and extended error.
		**	Compute the data residual now.
		*/
		cp->sv_scsi_status = cp->scsi_status;
		cp->sv_xerr_status = cp->xerr_status;
		cp->resid = ncr_compute_residual(np, cp);

		/*
		**	Device returned CHECK CONDITION status.
		**	Prepare all needed data strutures for getting 
		**	sense data.
		*/

		/*
		**	identify message
		*/
		cp->scsi_smsg2[0]	= M_IDENTIFY | cp->lun;
		msglen = 1;

		/*
		**	If we are currently using anything different from 
		**	async. 8 bit data transfers with that target,
		**	start a negotiation, since the device may want 
		**	to report us a UNIT ATTENTION condition due to 
		**	a cause we currently ignore, and we donnot want 
		**	to be stuck with WIDE and/or SYNC data transfer.
		**
		**	cp->nego_status is filled by ncr_prepare_nego().
		**
		**	Do NOT negotiate if performing integrity check
		**	or if integrity check has completed, all check
		**	conditions will have been cleared.
		*/

#ifdef	SCSI_NCR_INTEGRITY_CHECKING
		if (DEBUG_FLAGS & DEBUG_IC) {
		printk("%s: ncr_sir_to_redo: ic_done %2X, in_progress %2X\n",
			ncr_name(np), tp->ic_done, cp->cmd->ic_in_progress);
		}

		/*
		**	If parity error during integrity check,
		**	set the target width to narrow. Otherwise,
		**	do not negotiate on a request sense.
		*/
		if ( np->check_integ_par && np->check_integrity 
						&& cp->cmd->ic_in_progress ) { 
			cp->nego_status = 0;
			msglen +=
			    ncr_ic_nego (np, cp, cmd ,&cp->scsi_smsg2[msglen]);
		}

		if (!np->check_integrity || 
		   	(np->check_integrity && 
				(!cp->cmd->ic_in_progress && !tp->ic_done)) ) { 
		    ncr_negotiate(np, tp);
		    cp->nego_status = 0;
		    {
			u_char sync_offset;
			if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
					(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66))
				sync_offset = tp->sval & 0x3f;
			else
				sync_offset = tp->sval & 0x1f;

		        if ((tp->wval & EWS) || sync_offset)
			  msglen +=
			    ncr_prepare_nego (np, cp, &cp->scsi_smsg2[msglen]);
		    }

		}
#else
		ncr_negotiate(np, tp);
		cp->nego_status = 0;
		if ((tp->wval & EWS) || (tp->sval & 0x1f))
			msglen +=
			    ncr_prepare_nego (np, cp, &cp->scsi_smsg2[msglen]);
#endif	/* SCSI_NCR_INTEGRITY_CHECKING */

		/*
		**	Message table indirect structure.
		*/
		cp->phys.smsg.addr	= cpu_to_scr(CCB_PHYS (cp, scsi_smsg2));
		cp->phys.smsg.size	= cpu_to_scr(msglen);

		/*
		**	sense command
		*/
		cp->phys.cmd.addr	= cpu_to_scr(CCB_PHYS (cp, sensecmd));
		cp->phys.cmd.size	= cpu_to_scr(6);

		/*
		**	patch requested size into sense command
		*/
		cp->sensecmd[0]		= 0x03;
		cp->sensecmd[1]		= cp->lun << 5;
		cp->sensecmd[4]		= sizeof(cp->sense_buf);

		/*
		**	sense data
		*/
		bzero(cp->sense_buf, sizeof(cp->sense_buf));
		cp->phys.sense.addr	= cpu_to_scr(CCB_PHYS(cp,sense_buf[0]));
		cp->phys.sense.size	= cpu_to_scr(sizeof(cp->sense_buf));

		/*
		**	requeue the command.
		*/
		startp = NCB_SCRIPTH_PHYS (np, sdata_in);

		cp->phys.header.savep	= cpu_to_scr(startp);
		cp->phys.header.goalp	= cpu_to_scr(startp + 16);
		cp->phys.header.lastp	= cpu_to_scr(startp);
		cp->phys.header.wgoalp	= cpu_to_scr(startp + 16);
		cp->phys.header.wlastp	= cpu_to_scr(startp);

		cp->host_status	= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
		cp->scsi_status = S_ILLEGAL;
		cp->host_flags	= (HF_AUTO_SENSE|HF_DATA_IN);

		cp->phys.header.go.start =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, select));

		/*
		**	If lp not yet allocated, requeue the command.
		*/
		if (!lp)
			ncr_put_start_queue(np, cp);
		break;
	}

	/*
	**	requeue awaiting scsi commands for this lun.
	*/
	if (lp)
		ncr_start_next_ccb(np, lp, 1);

	return;
}

/*----------------------------------------------------------
**
**	After a device has accepted some management message 
**	as BUS DEVICE RESET, ABORT TASK, etc ..., or when 
**	a device signals a UNIT ATTENTION condition, some 
**	tasks are thrown away by the device. We are required 
**	to reflect that on our tasks list since the device 
**	will never complete these tasks.
**
**	This function completes all disconnected CCBs for a 
**	given target that matches the following criteria:
**	- lun=-1  means any logical UNIT otherwise a given one.
**	- task=-1 means any task, otherwise a given one.
**----------------------------------------------------------
*/
static int ncr_clear_tasks(ncb_p np, u_char hsts, 
			   int target, int lun, int task)
{
	int i = 0;
	ccb_p cp;

	for (cp = np->ccbc; cp; cp = cp->link_ccb) {
		if (cp->host_status != HS_DISCONNECT)
			continue;
		if (cp->target != target)
			continue;
		if (lun != -1 && cp->lun != lun)
			continue;
		if (task != -1 && cp->tag != NO_TAG && cp->scsi_smsg[2] != task)
			continue;
		cp->host_status = hsts;
		cp->scsi_status = S_ILLEGAL;
		ncr_complete(np, cp);
		++i;
	}
	return i;
}

/*==========================================================
**
**	ncr chip handler for TASKS recovery.
**
**==========================================================
**
**	We cannot safely abort a command, while the SCRIPTS 
**	processor is running, since we just would be in race 
**	with it.
**
**	As long as we have tasks to abort, we keep the SEM 
**	bit set in the ISTAT. When this bit is set, the 
**	SCRIPTS processor interrupts (SIR_SCRIPT_STOPPED) 
**	each time it enters the scheduler.
**
**	If we have to reset a target, clear tasks of a unit,
**	or to perform the abort of a disconnected job, we 
**	restart the SCRIPTS for selecting the target. Once 
**	selected, the SCRIPTS interrupts (SIR_TARGET_SELECTED).
**	If it loses arbitration, the SCRIPTS will interrupt again 
**	the next time it will enter its scheduler, and so on ...
**
**	On SIR_TARGET_SELECTED, we scan for the more 
**	appropriate thing to do:
**
**	- If nothing, we just sent a M_ABORT message to the 
**	  target to get rid of the useless SCSI bus ownership.
**	  According to the specs, no tasks shall be affected.
**	- If the target is to be reset, we send it a M_RESET 
**	  message.
**	- If a logical UNIT is to be cleared , we send the 
**	  IDENTIFY(lun) + M_ABORT.
**	- If an untagged task is to be aborted, we send the 
**	  IDENTIFY(lun) + M_ABORT.
**	- If a tagged task is to be aborted, we send the 
**	  IDENTIFY(lun) + task attributes + M_ABORT_TAG.
**
**	Once our 'kiss of death' :) message has been accepted 
**	by the target, the SCRIPTS interrupts again 
**	(SIR_ABORT_SENT). On this interrupt, we complete 
**	all the CCBs that should have been aborted by the 
**	target according to our message.
**	
**----------------------------------------------------------
*/
static void ncr_sir_task_recovery(ncb_p np, int num)
{
	ccb_p cp;
	tcb_p tp;
	int target=-1, lun=-1, task;
	int i, k;
	u_char *p;

	switch(num) {
	/*
	**	The SCRIPTS processor stopped before starting
	**	the next command in order to allow us to perform 
	**	some task recovery.
	*/
	case SIR_SCRIPT_STOPPED:

		/*
		**	Do we have any target to reset or unit to clear ?
		*/
		for (i = 0 ; i < MAX_TARGET ; i++) {
			tp = &np->target[i];
			if (tp->to_reset || (tp->l0p && tp->l0p->to_clear)) {
				target = i;
				break;
			}
			if (!tp->lmp)
				continue;
			for (k = 1 ; k < MAX_LUN ; k++) {
				if (tp->lmp[k] && tp->lmp[k]->to_clear) {
					target	= i;
					break;
				}
			}
			if (target != -1)
				break;
		}

		/*
		**	If not, look at the CCB list for any 
		**	disconnected CCB to be aborted.
		*/
		if (target == -1) {
			for (cp = np->ccbc; cp; cp = cp->link_ccb) {
				if (cp->host_status != HS_DISCONNECT)
					continue;
				if (cp->to_abort) {
					target = cp->target;
					break;
				}
			}
		}

		/*
		**	If some target is to be selected, 
		**	prepare and start the selection.
		*/
		if (target != -1) {
			tp = &np->target[target];
			np->abrt_sel.sel_id	= target;
			np->abrt_sel.sel_scntl3 = tp->wval;
			np->abrt_sel.sel_sxfer  = tp->sval;
			np->abrt_sel.sel_scntl4 = tp->uval;
			OUTL(nc_dsa, np->p_ncb);
			OUTL_DSP (NCB_SCRIPTH_PHYS (np, sel_for_abort));
			return;
		}

		/*
		**	Nothing is to be selected, so we donnot need 
		**	to synchronize with the SCRIPTS anymore.
		**	Remove the SEM flag from the ISTAT.
		*/
		np->istat_sem = 0;
		OUTB (nc_istat, SIGP);

		/*
		**	Now look at CCBs to abort that haven't started yet.
		**	Remove all those CCBs from the start queue and 
		**	complete them with appropriate status.
		**	Btw, the SCRIPTS processor is still stopped, so 
		**	we are not in race.
		*/
		for (cp = np->ccbc; cp; cp = cp->link_ccb) {
			if (cp->host_status != HS_BUSY &&
			    cp->host_status != HS_NEGOTIATE)
				continue;
			if (!cp->to_abort)
				continue;
#ifdef SCSI_NCR_IARB_SUPPORT
			/*
			**    If we are using IMMEDIATE ARBITRATION, we donnot 
			**    want to cancel the last queued CCB, since the 
			**    SCRIPTS may have anticipated the selection.
			*/
			if (cp == np->last_cp) {
				cp->to_abort = 0;
				continue;
			}
#endif
			/*
			**	Compute index of next position in the start 
			**	queue the SCRIPTS will schedule.
			*/
			i = (INL (nc_scratcha) - np->p_squeue) / 4;

			/*
			**	Remove the job from the start queue.
			*/
			k = -1;
			while (1) {
				if (i == np->squeueput)
					break;
				if (k == -1) {		/* Not found yet */
					if (cp == ncr_ccb_from_dsa(np,
						     scr_to_cpu(np->squeue[i])))
						k = i;	/* Found */
				}
				else {
					/*
					**    Once found, we have to move 
					**    back all jobs by 1 position.
					*/
					np->squeue[k] = np->squeue[i];
					k += 2;
					if (k >= MAX_START*2)
						k = 0;
				}

				i += 2;
				if (i >= MAX_START*2)
					i = 0;
			}
			/*
			**	If job removed, repair the start queue.
			*/
			if (k != -1) {
				np->squeue[k] = np->squeue[i]; /* Idle task */
				np->squeueput = k; /* Start queue pointer */
			}
			cp->host_status = HS_ABORTED;
			cp->scsi_status = S_ILLEGAL;
			ncr_complete(np, cp);
		}
		break;
	/*
	**	The SCRIPTS processor has selected a target 
	**	we may have some manual recovery to perform for.
	*/
	case SIR_TARGET_SELECTED:
		target = (INB (nc_sdid) & 0xf);
		tp = &np->target[target];

		np->abrt_tbl.addr = cpu_to_scr(vtobus(np->abrt_msg));

		/*
		**	If the target is to be reset, prepare a 
		**	M_RESET message and clear the to_reset flag 
		**	since we donnot expect this operation to fail.
		*/
		if (tp->to_reset) {
			np->abrt_msg[0] = M_RESET;
			np->abrt_tbl.size = 1;
			tp->to_reset = 0;
			break;
		}

		/*
		**	Otherwise, look for some logical unit to be cleared.
		*/
		if (tp->l0p && tp->l0p->to_clear)
			lun = 0;
		else if (tp->lmp) {
			for (k = 1 ; k < MAX_LUN ; k++) {
				if (tp->lmp[k] && tp->lmp[k]->to_clear) {
					lun = k;
					break;
				}
			}
		}

		/*
		**	If a logical unit is to be cleared, prepare 
		**	an IDENTIFY(lun) + ABORT MESSAGE.
		*/
		if (lun != -1) {
			lcb_p lp = ncr_lp(np, tp, lun);
			lp->to_clear = 0; /* We donnot expect to fail here */
			np->abrt_msg[0] = M_IDENTIFY | lun;
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
			break;
		}

		/*
		**	Otherwise, look for some disconnected job to 
		**	abort for this target.
		*/
		for (cp = np->ccbc; cp; cp = cp->link_ccb) {
			if (cp->host_status != HS_DISCONNECT)
				continue;
			if (cp->target != target)
				continue;
			if (cp->to_abort)
				break;
		}

		/*
		**	If we have none, probably since the device has 
		**	completed the command before we won abitration,
		**	send a M_ABORT message without IDENTIFY.
		**	According to the specs, the device must just 
		**	disconnect the BUS and not abort any task.
		*/
		if (!cp) {
			np->abrt_msg[0] = M_ABORT;
			np->abrt_tbl.size = 1;
			break;
		}

		/*
		**	We have some task to abort.
		**	Set the IDENTIFY(lun)
		*/
		np->abrt_msg[0] = M_IDENTIFY | cp->lun;

		/*
		**	If we want to abort an untagged command, we 
		**	will send a IDENTIFY + M_ABORT.
		**	Otherwise (tagged command), we will send 
		**	a IDENTITFY + task attributes + ABORT TAG.
		*/
		if (cp->tag == NO_TAG) {
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
		}
		else {
			np->abrt_msg[1] = cp->scsi_smsg[1];
			np->abrt_msg[2] = cp->scsi_smsg[2];
			np->abrt_msg[3] = M_ABORT_TAG;
			np->abrt_tbl.size = 4;
		}
		cp->to_abort = 0; /* We donnot expect to fail here */
		break;

	/*
	**	The target has accepted our message and switched 
	**	to BUS FREE phase as we expected.
	*/
	case SIR_ABORT_SENT:
		target = (INB (nc_sdid) & 0xf);
		tp = &np->target[target];
		
		/*
		**	If we didn't abort anything, leave here.
		*/
		if (np->abrt_msg[0] == M_ABORT)
			break;

		/*
		**	If we sent a M_RESET, then a hardware reset has 
		**	been performed by the target.
		**	- Reset everything to async 8 bit
		**	- Tell ourself to negotiate next time :-)
		**	- Prepare to clear all disconnected CCBs for 
		**	  this target from our task list (lun=task=-1)
		*/
		lun = -1;
		task = -1;
		if (np->abrt_msg[0] == M_RESET) {
			tp->sval = 0;
			tp->wval = np->rv_scntl3;
			tp->uval = np->rv_scntl4; 
			ncr_set_sync_wide_status(np, target);
			ncr_negotiate(np, tp);
		}

		/*
		**	Otherwise, check for the LUN and TASK(s) 
		**	concerned by the cancelation.
		**	If it is not ABORT_TAG then it is CLEAR_QUEUE 
		**	or an ABORT message :-)
		*/
		else {
			lun = np->abrt_msg[0] & 0x3f;
			if (np->abrt_msg[1] == M_ABORT_TAG)
				task = np->abrt_msg[2];
		}

		/*
		**	Complete all the CCBs the device should have 
		**	aborted due to our 'kiss of death' message.
		*/
		(void) ncr_clear_tasks(np, HS_ABORTED, target, lun, task);
		break;

	/*
	**	We have performed a auto-sense that succeeded.
	**	If the device reports a UNIT ATTENTION condition 
	**	due to a RESET condition, we must complete all 
	**	disconnect CCBs for this unit since the device 
	**	shall have thrown them away.
	**	Since I haven't time to guess what the specs are 
	**	expecting for other UNIT ATTENTION conditions, I 
	**	decided to only care about RESET conditions. :)
	*/
	case SIR_AUTO_SENSE_DONE:
		cp = ncr_ccb_from_dsa(np, INL (nc_dsa));
		if (!cp)
			break;
		memcpy(cp->cmd->sense_buffer, cp->sense_buf,
		       sizeof(cp->cmd->sense_buffer));
		p  = &cp->cmd->sense_buffer[0];

		if (p[0] != 0x70 || p[2] != 0x6 || p[12] != 0x29)
			break;
#if 0
		(void) ncr_clear_tasks(np, HS_RESET, cp->target, cp->lun, -1);
#endif
		break;
	}

	/*
	**	Print to the log the message we intend to send.
	*/
	if (num == SIR_TARGET_SELECTED) {
		PRINT_TARGET(np, target);
		ncr_printl_hex("control msgout:", np->abrt_msg,
			      np->abrt_tbl.size);
		np->abrt_tbl.size = cpu_to_scr(np->abrt_tbl.size);
	}

	/*
	**	Let the SCRIPTS processor continue.
	*/
	OUTONB_STD ();
}


/*==========================================================
**
**	Gérard's alchemy:) that deals with with the data 
**	pointer for both MDP and the residual calculation.
**
**==========================================================
**
**	I didn't want to bloat the code by more than 200 
**	lignes for the handling of both MDP and the residual.
**	This has been achieved by using a data pointer 
**	representation consisting in an index in the data 
**	array (dp_sg) and a negative offset (dp_ofs) that 
**	have the following meaning:
**
**	- dp_sg = MAX_SCATTER
**	  we are at the end of the data script.
**	- dp_sg < MAX_SCATTER
**	  dp_sg points to the next entry of the scatter array 
**	  we want to transfer.
**	- dp_ofs < 0
**	  dp_ofs represents the residual of bytes of the 
**	  previous entry scatter entry we will send first.
**	- dp_ofs = 0
**	  no residual to send first.
**
**	The function ncr_evaluate_dp() accepts an arbitray 
**	offset (basically from the MDP message) and returns 
**	the corresponding values of dp_sg and dp_ofs.
**
**----------------------------------------------------------
*/

static int ncr_evaluate_dp(ncb_p np, ccb_p cp, u_int32 scr, int *ofs)
{
	u_int32	dp_scr;
	int	dp_ofs, dp_sg, dp_sgmin;
	int	tmp;
	struct pm_ctx *pm;

	/*
	**	Compute the resulted data pointer in term of a script 
	**	address within some DATA script and a signed byte offset.
	*/
	dp_scr = scr;
	dp_ofs = *ofs;
	if	(dp_scr == NCB_SCRIPT_PHYS (np, pm0_data))
		pm = &cp->phys.pm0;
	else if (dp_scr == NCB_SCRIPT_PHYS (np, pm1_data))
		pm = &cp->phys.pm1;
	else
		pm = 0;

	if (pm) {
		dp_scr  = scr_to_cpu(pm->ret);
		dp_ofs -= scr_to_cpu(pm->sg.size);
	}

	/*
	**	Deduce the index of the sg entry.
	**	Keep track of the index of the first valid entry.
	**	If result is dp_sg = MAX_SCATTER, then we are at the 
	**	end of the data and vice-versa.
	*/
	tmp = scr_to_cpu(cp->phys.header.goalp);
	dp_sg = MAX_SCATTER;
	if (dp_scr != tmp)
		dp_sg -= (tmp - 8 - (int)dp_scr) / (SCR_SG_SIZE*4);
	dp_sgmin = MAX_SCATTER - cp->segments;

	/*
	**	Move to the sg entry the data pointer belongs to.
	**
	**	If we are inside the data area, we expect result to be:
	**
	**	Either,
	**	    dp_ofs = 0 and dp_sg is the index of the sg entry
	**	    the data pointer belongs to (or the end of the data)
	**	Or,
	**	    dp_ofs < 0 and dp_sg is the index of the sg entry 
	**	    the data pointer belongs to + 1.
	*/
	if (dp_ofs < 0) {
		int n;
		while (dp_sg > dp_sgmin) {
			--dp_sg;
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			n = dp_ofs + (tmp & 0xffffff);
			if (n > 0) {
				++dp_sg;
				break;
			}
			dp_ofs = n;
		}
	}
	else if (dp_ofs > 0) {
		while (dp_sg < MAX_SCATTER) {
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			dp_ofs -= (tmp & 0xffffff);
			++dp_sg;
			if (dp_ofs <= 0)
				break;
		}
	}

	/*
	**	Make sure the data pointer is inside the data area.
	**	If not, return some error.
	*/
	if	(dp_sg < dp_sgmin || (dp_sg == dp_sgmin && dp_ofs < 0))
		goto out_err;
	else if	(dp_sg > MAX_SCATTER || (dp_sg == MAX_SCATTER && dp_ofs > 0))
		goto out_err;

	/*
	**	Save the extreme pointer if needed.
	*/
	if (dp_sg > cp->ext_sg ||
            (dp_sg == cp->ext_sg && dp_ofs > cp->ext_ofs)) {
		cp->ext_sg  = dp_sg;
		cp->ext_ofs = dp_ofs;
	}

	/*
	**	Return data.
	*/
	*ofs = dp_ofs;
	return dp_sg;

out_err:
	return -1;
}

/*==========================================================
**
**	ncr chip handler for MODIFY DATA POINTER MESSAGE
**
**==========================================================
**
**	We also call this function on IGNORE WIDE RESIDUE 
**	messages that do not match a SWIDE full condition.
**	Btw, we assume in that situation that such a message 
**	is equivalent to a MODIFY DATA POINTER (offset=-1).
**
**----------------------------------------------------------
*/

static void ncr_modify_dp(ncb_p np, tcb_p tp, ccb_p cp, int ofs)
{
	int dp_ofs	= ofs;
	u_int32 dp_scr	= INL (nc_temp);
	u_int32	dp_ret;
	u_int32	tmp;
	u_char	hflags;
	int	dp_sg;
	struct pm_ctx *pm;

	/*
	**	Not supported for auto_sense;
	*/
	if (cp->host_flags & HF_AUTO_SENSE)
		goto out_reject;

	/*
	**	Apply our alchemy:) (see comments in ncr_evaluate_dp()), 
	**	to the resulted data pointer.
	*/
	dp_sg = ncr_evaluate_dp(np, cp, dp_scr, &dp_ofs);
	if (dp_sg < 0)
		goto out_reject;

	/*
	**	And our alchemy:) allows to easily calculate the data 
	**	script address we want to return for the next data phase.
	*/
	dp_ret = cpu_to_scr(cp->phys.header.goalp);
	dp_ret = dp_ret - 8 - (MAX_SCATTER - dp_sg) * (SCR_SG_SIZE*4);

	/*
	**	If offset / scatter entry is zero we donnot need 
	**	a context for the new current data pointer.
	*/
	if (dp_ofs == 0) {
		dp_scr = dp_ret;
		goto out_ok;
	}

	/*
	**	Get a context for the new current data pointer.
	*/
	hflags = INB (HF_PRT);

	if (hflags & HF_DP_SAVED)
		hflags ^= HF_ACT_PM;

	if (!(hflags & HF_ACT_PM)) {
		pm  = &cp->phys.pm0;
		dp_scr = NCB_SCRIPT_PHYS (np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		dp_scr = NCB_SCRIPT_PHYS (np, pm1_data);
	}

	hflags &= ~(HF_DP_SAVED);

	OUTB (HF_PRT, hflags);

	/*
	**	Set up the new current data pointer.
	**	ofs < 0 there, and for the next data phase, we 
	**	want to transfer part of the data of the sg entry 
	**	corresponding to index dp_sg-1 prior to returning 
	**	to the main data script.
	*/
	pm->ret = cpu_to_scr(dp_ret);
	tmp  = scr_to_cpu(cp->phys.data[dp_sg-1].addr);
	tmp += scr_to_cpu(cp->phys.data[dp_sg-1].size) + dp_ofs;
	pm->sg.addr = cpu_to_scr(tmp);
	pm->sg.size = cpu_to_scr(-dp_ofs);

out_ok:
	OUTL (nc_temp, dp_scr);
	OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
	return;

out_reject:
	OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
}


/*==========================================================
**
**	ncr chip calculation of the data residual.
**
**==========================================================
**
**	As I used to say, the requirement of data residual 
**	in SCSI is broken, useless and cannot be achieved 
**	without huge complexity.
**	But most OSes and even the official CAM require it.
**	When stupidity happens to be so widely spread inside 
**	a community, it gets hard to convince.
**
**	Anyway, I don't care, since I am not going to use 
**	any software that considers this data residual as 
**	a relevant information. :)
**	
**----------------------------------------------------------
*/

static int ncr_compute_residual(ncb_p np, ccb_p cp)
{
	int dp_sg, dp_sgmin, tmp;
	int resid=0;
	int dp_ofs = 0;

	/*
	 *	Check for some data lost or just thrown away.
	 *	We are not required to be quite accurate in this
	 *	situation. Btw, if we are odd for output and the
	 *	device claims some more data, it may well happen
	 *	than our residual be zero. :-)
	 */
	if (cp->xerr_status & (XE_EXTRA_DATA|XE_SODL_UNRUN|XE_SWIDE_OVRUN)) {
		if (cp->xerr_status & XE_EXTRA_DATA)
			resid -= cp->extra_bytes;
		if (cp->xerr_status & XE_SODL_UNRUN)
			++resid;
		if (cp->xerr_status & XE_SWIDE_OVRUN)
			--resid;
	}


	/*
	**	If SCRIPTS reaches its goal point, then 
	**	there is no additionnal residual.
	*/
	if (cp->phys.header.lastp == cp->phys.header.goalp)
		return resid;

	/*
	**	If the last data pointer is data_io (direction 
	**	unknown), then no data transfer should have 
	**	taken place.
	*/
	if (cp->phys.header.lastp == NCB_SCRIPTH_PHYS (np, data_io))
		return cp->data_len;

	/*
	**	If no data transfer occurs, or if the data
	**	pointer is weird, return full residual.
	*/
	if (cp->startp == cp->phys.header.lastp ||
	    ncr_evaluate_dp(np, cp, scr_to_cpu(cp->phys.header.lastp),
			    &dp_ofs) < 0) {
		return cp->data_len;
	}

	/*
	**	We are now full comfortable in the computation 
	**	of the data residual (2's complement).
	*/
	dp_sgmin = MAX_SCATTER - cp->segments;
	resid = -cp->ext_ofs;
	for (dp_sg = cp->ext_sg; dp_sg < MAX_SCATTER; ++dp_sg) {
		tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
		resid += (tmp & 0xffffff);
	}

	/*
	**	Hopefully, the result is not too wrong.
	*/
	return resid;
}

/*==========================================================
**
**	Print out the containt of a SCSI message.
**
**==========================================================
*/

static int ncr_show_msg (u_char * msg)
{
	u_char i;
	printk ("%x",*msg);
	if (*msg==M_EXTENDED) {
		for (i=1;i<8;i++) {
			if (i-1>msg[1]) break;
			printk ("-%x",msg[i]);
		};
		return (i+1);
	} else if ((*msg & 0xf0) == 0x20) {
		printk ("-%x",msg[1]);
		return (2);
	};
	return (1);
}

static void ncr_print_msg (ccb_p cp, char *label, u_char *msg)
{
	if (cp)
		PRINT_ADDR(cp->cmd);
	if (label)
		printk ("%s: ", label);

	(void) ncr_show_msg (msg);
	printk (".\n");
}

/*===================================================================
**
**	Negotiation for WIDE and SYNCHRONOUS DATA TRANSFER.
**
**===================================================================
**
**	Was Sie schon immer ueber transfermode negotiation wissen wollten ...
**
**	We try to negotiate sync and wide transfer only after
**	a successful inquire command. We look at byte 7 of the
**	inquire data to determine the capabilities of the target.
**
**	When we try to negotiate, we append the negotiation message
**	to the identify and (maybe) simple tag message.
**	The host status field is set to HS_NEGOTIATE to mark this
**	situation.
**
**	If the target doesn't answer this message immediately
**	(as required by the standard), the SIR_NEGO_FAILED interrupt
**	will be raised eventually.
**	The handler removes the HS_NEGOTIATE status, and sets the
**	negotiated value to the default (async / nowide).
**
**	If we receive a matching answer immediately, we check it
**	for validity, and set the values.
**
**	If we receive a Reject message immediately, we assume the
**	negotiation has failed, and fall back to standard values.
**
**	If we receive a negotiation message while not in HS_NEGOTIATE
**	state, it's a target initiated negotiation. We prepare a
**	(hopefully) valid answer, set our parameters, and send back 
**	this answer to the target.
**
**	If the target doesn't fetch the answer (no message out phase),
**	we assume the negotiation has failed, and fall back to default
**	settings (SIR_NEGO_PROTO interrupt).
**
**	When we set the values, we adjust them in all ccbs belonging 
**	to this target, in the controller's register, and in the "phys"
**	field of the controller's struct ncb.
**
**---------------------------------------------------------------------
*/

/*==========================================================
**
**	ncr chip handler for SYNCHRONOUS DATA TRANSFER 
**	REQUEST (SDTR) message.
**
**==========================================================
**
**	Read comments above.
**
**----------------------------------------------------------
*/
static void ncr_sync_nego(ncb_p np, tcb_p tp, ccb_p cp)
{
	u_char	scntl3, scntl4;
	u_char	chg, ofs, per, fak;

	/*
	**	Synchronous request message received.
	*/

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		ncr_print_msg(cp, "sync msg in", np->msgin);
	};

	/*
	**	get requested values.
	*/

	chg = 0;
	per = np->msgin[3];
	ofs = np->msgin[4];
	if (ofs==0) per=255;

	/*
	**      if target sends SDTR message,
	**	      it CAN transfer synch.
	*/

	if (ofs)
		tp->inq_byte7 |= INQ7_SYNC;

	/*
	**	check values against driver limits.
	*/

	if (per < np->minsync)
		{chg = 1; per = np->minsync;}
	if (per < tp->minsync)
		{chg = 1; per = tp->minsync;}
	if (ofs > np->maxoffs_st)
		{chg = 1; ofs = np->maxoffs_st;}
	if (ofs > tp->maxoffs)
		{chg = 1; ofs = tp->maxoffs;}

	/*
	**	Check against controller limits.
	*/
	fak	= 7;
	scntl3	= 0;
	scntl4  = 0;
	if (ofs != 0) {
		ncr_getsync(np, per, &fak, &scntl3);
		if (fak > 7) {
			chg = 1;
			ofs = 0;
		}
	}
	if (ofs == 0) {
		fak	= 7;
		per	= 0;
		scntl3	= 0;
		scntl4  = 0;
		tp->minsync = 0;
	}

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		PRINT_ADDR(cp->cmd);
		printk ("sync: per=%d scntl3=0x%x scntl4=0x%x ofs=%d fak=%d chg=%d.\n",
			per, scntl3, scntl4, ofs, fak, chg);
	}

	if (INB (HS_PRT) == HS_NEGOTIATE) {
		OUTB (HS_PRT, HS_BUSY);
		switch (cp->nego_status) {
		case NS_SYNC:
			/*
			**      This was an answer message
			*/
			if (chg) {
				/*
				**	Answer wasn't acceptable.
				*/
				ncr_setsync (np, cp, 0, 0xe0, 0);
				OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
			} else {
				/*
				**	Answer is ok.
				*/
				if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
					(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66))
				  ncr_setsync (np, cp, scntl3, (fak<<5)|ofs,0);
				else
				  ncr_setsync (np, cp, scntl3, ofs, scntl4);

				OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
			};
			return;

		case NS_WIDE:
			ncr_setwide (np, cp, 0, 0);
			break;
		};
	};

	/*
	**	It was a request. Set value and
	**      prepare an answer message
	*/

	if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66))
		ncr_setsync (np, cp, scntl3, (fak<<5)|ofs,0);
	else
		ncr_setsync (np, cp, scntl3, ofs, scntl4);

	np->msgout[0] = M_EXTENDED;
	np->msgout[1] = 3;
	np->msgout[2] = M_X_SYNC_REQ;
	np->msgout[3] = per;
	np->msgout[4] = ofs;

	cp->nego_status = NS_SYNC;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		ncr_print_msg(cp, "sync msgout", np->msgout);
	}

	np->msgin [0] = M_NOOP;

	if (!ofs)
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
	else
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, sdtr_resp));
}

/*==========================================================
**
**	ncr chip handler for WIDE DATA TRANSFER REQUEST 
**	(WDTR) message.
**
**==========================================================
**
**	Read comments above.
**
**----------------------------------------------------------
*/
static void ncr_wide_nego(ncb_p np, tcb_p tp, ccb_p cp)
{
	u_char	chg, wide;

	/*
	**	Wide request message received.
	*/
	if (DEBUG_FLAGS & DEBUG_NEGO) {
		ncr_print_msg(cp, "wide msgin", np->msgin);
	};

	/*
	**	get requested values.
	*/

	chg  = 0;
	wide = np->msgin[3];

	/*
	**      if target sends WDTR message,
	**	      it CAN transfer wide.
	*/

	if (wide)
		tp->inq_byte7 |= INQ7_WIDE16;

	/*
	**	check values against driver limits.
	*/

	if (wide > tp->usrwide)
		{chg = 1; wide = tp->usrwide;}

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		PRINT_ADDR(cp->cmd);
		printk ("wide: wide=%d chg=%d.\n", wide, chg);
	}

	if (INB (HS_PRT) == HS_NEGOTIATE) {
		OUTB (HS_PRT, HS_BUSY);
		switch (cp->nego_status) {
		case NS_WIDE:
			/*
			**      This was an answer message
			*/
			if (chg) {
				/*
				**	Answer wasn't acceptable.
				*/
				ncr_setwide (np, cp, 0, 1);
				OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
			} else {
				/*
				**	Answer is ok.
				*/
				ncr_setwide (np, cp, wide, 1);
				OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
			};
			return;

		case NS_SYNC:
			ncr_setsync (np, cp, 0, 0xe0, 0);
			break;
		};
	};

	/*
	**	It was a request, set value and
	**      prepare an answer message
	*/

	ncr_setwide (np, cp, wide, 1);

	np->msgout[0] = M_EXTENDED;
	np->msgout[1] = 2;
	np->msgout[2] = M_X_WIDE_REQ;
	np->msgout[3] = wide;

	np->msgin [0] = M_NOOP;

	cp->nego_status = NS_WIDE;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		ncr_print_msg(cp, "wide msgout", np->msgout);
	}

	OUTL_DSP (NCB_SCRIPTH_PHYS (np, wdtr_resp));
}
/*==========================================================
**
**	ncr chip handler for PARALLEL PROTOCOL REQUEST 
**	(PPR) message.
**
**==========================================================
**
**	Read comments above.
**
**----------------------------------------------------------
*/
static void ncr_ppr_nego(ncb_p np, tcb_p tp, ccb_p cp)
{
	u_char	scntl3, scntl4;
	u_char	chg, ofs, per, fak, wth, dt;

	/*
	**	PPR message received.
	*/

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		ncr_print_msg(cp, "ppr msg in", np->msgin);
	};

	/*
	**	get requested values.
	*/

	chg = 0;
	per = np->msgin[3];
	ofs = np->msgin[5];
	wth = np->msgin[6];
	dt  = np->msgin[7];
	if (ofs==0) per=255;

	/*
	**      if target sends sync (wide),
	**	      it CAN transfer synch (wide).
	*/

	if (ofs)
		tp->inq_byte7 |= INQ7_SYNC;

	if (wth)
		tp->inq_byte7 |= INQ7_WIDE16;

	/*
	**	check values against driver limits.
	*/

	if (wth > tp->usrwide)
		{chg = 1; wth = tp->usrwide;}
	if (per < np->minsync)
		{chg = 1; per = np->minsync;}
	if (per < tp->minsync)
		{chg = 1; per = tp->minsync;}
	if (ofs > tp->maxoffs)
		{chg = 1; ofs = tp->maxoffs;}

	/*
	**	Check against controller limits.
	*/
	fak	= 7;
	scntl3	= 0;
	scntl4  = 0;
	if (ofs != 0) {
		scntl4 = dt ? 0x80 : 0;
		ncr_getsync(np, per, &fak, &scntl3);
		if (fak > 7) {
			chg = 1;
			ofs = 0;
		}
	}
	if (ofs == 0) {
		fak	= 7;
		per	= 0;
		scntl3	= 0;
		scntl4  = 0;
		tp->minsync = 0;
	}

	/*
	**	If target responds with Ultra 3 speed
	**	but narrow or not DT, reject.
	**	If target responds with DT request 
	**	but not Ultra3 speeds, reject message,
	**	reset min sync for target to 0x0A and
	**	set flags to re-negotiate.
	*/

	if   ((per == 0x09) && ofs && (!wth || !dt))  
		chg = 1;
	else if (( (per > 0x09) && dt) ) 
		chg = 2;

	/* Not acceptable since beyond controller limit */
	if (!dt && ofs > np->maxoffs_st)
		{chg = 2; ofs = np->maxoffs_st;}

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		PRINT_ADDR(cp->cmd);
		printk ("ppr: wth=%d per=%d scntl3=0x%x scntl4=0x%x ofs=%d fak=%d chg=%d.\n",
			wth, per, scntl3, scntl4, ofs, fak, chg);
	}

	if (INB (HS_PRT) == HS_NEGOTIATE) {
		OUTB (HS_PRT, HS_BUSY);
		switch (cp->nego_status) {
		case NS_PPR:
			/*
			**      This was an answer message
			*/
			if (chg) {
				/*
				**	Answer wasn't acceptable.
				*/
				if (chg == 2) {
					/* Send message reject and reset flags for
					** host to re-negotiate with min period 0x0A.
					*/
					tp->minsync = 0x0A;
					tp->period = 0;
					tp->widedone = 0;
				}
				ncr_setsyncwide (np, cp, 0, 0xe0, 0, 0);
				OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
			} else {
				/*
				**	Answer is ok.
				*/

				if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
					(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66))
				  ncr_setsyncwide (np, cp, scntl3, (fak<<5)|ofs,0, wth);
				else
				  ncr_setsyncwide (np, cp, scntl3, ofs, scntl4, wth);

				OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
				
			};
			return;

		case NS_SYNC:
			ncr_setsync (np, cp, 0, 0xe0, 0);
			break;

		case NS_WIDE:
			ncr_setwide (np, cp, 0, 0);
			break;
		};
	};

	/*
	**	It was a request. Set value and
	**      prepare an answer message
	**
	**	If narrow or not DT and requesting Ultra3
	**	slow the bus down and force ST. If not
	**	requesting Ultra3, force ST.
	**	Max offset is 31=0x1f if ST mode.
	*/

	if  ((per == 0x09) && ofs && (!wth || !dt)) {
		per = 0x0A;
		dt = 0;
	}
	else if ( (per > 0x09) && dt) {
		dt = 0;
	}
	if (!dt && ofs > np->maxoffs_st)
		ofs = np->maxoffs_st;

	if ((np->device_id != PCI_DEVICE_ID_LSI_53C1010) &&
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66))
		ncr_setsyncwide (np, cp, scntl3, (fak<<5)|ofs,0, wth);
	else
		ncr_setsyncwide (np, cp, scntl3, ofs, scntl4, wth);

	np->msgout[0] = M_EXTENDED;
	np->msgout[1] = 6;
	np->msgout[2] = M_X_PPR_REQ;
	np->msgout[3] = per;
	np->msgout[4] = 0;		
	np->msgout[5] = ofs;
	np->msgout[6] = wth;
	np->msgout[7] = dt;

	cp->nego_status = NS_PPR;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		ncr_print_msg(cp, "ppr msgout", np->msgout);
	}

	np->msgin [0] = M_NOOP;

	if (!ofs)
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
	else
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, ppr_resp));
}



/*
**	Reset SYNC or WIDE to default settings.
**	Called when a negotiation does not succeed either 
**	on rejection or on protocol error.
*/
static void ncr_nego_default(ncb_p np, tcb_p tp, ccb_p cp)
{
	/*
	**	any error in negotiation:
	**	fall back to default mode.
	*/
	switch (cp->nego_status) {

	case NS_SYNC:
		ncr_setsync (np, cp, 0, 0xe0, 0);
		break;

	case NS_WIDE:
		ncr_setwide (np, cp, 0, 0);
		break;

	case NS_PPR:
		/*
		 * ppr_negotiation is set to 1 on the first ppr nego command.
		 * If ppr is successful, it is reset to 2.
		 * If unsuccessful it is reset to 0.
		 */
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			tcb_p tp=&np->target[cp->target];
			u_char factor, offset, width;

			ncr_get_xfer_info ( np, tp, &factor, &offset, &width);

			printk("Current factor %d offset %d width %d\n",
				factor, offset, width);	
		}
		if (tp->ppr_negotiation == 2)
			ncr_setsyncwide (np, cp, 0, 0xe0, 0, 0);
		else if (tp->ppr_negotiation == 1) {

			/* First ppr command has received a  M REJECT.
			 * Do not change the existing wide/sync parameter
			 * values (asyn/narrow if this as the first nego;
			 * may be different if target initiates nego.).
			 */
			tp->ppr_negotiation = 0;
		}
		else
		{
			tp->ppr_negotiation = 0;
			ncr_setwide (np, cp, 0, 0);
		}
		break;
	};
	np->msgin [0] = M_NOOP;
	np->msgout[0] = M_NOOP;
	cp->nego_status = 0;
}

/*==========================================================
**
**	ncr chip handler for MESSAGE REJECT received for 
**	a WIDE or SYNCHRONOUS negotiation.
**
**	clear the PPR negotiation flag, all future nego.
**	will be SDTR and WDTR
**
**==========================================================
**
**	Read comments above.
**
**----------------------------------------------------------
*/
static void ncr_nego_rejected(ncb_p np, tcb_p tp, ccb_p cp)
{
	ncr_nego_default(np, tp, cp);
	OUTB (HS_PRT, HS_BUSY);
}


/*==========================================================
**
**
**      ncr chip exception handler for programmed interrupts.
**
**
**==========================================================
*/

void ncr_int_sir (ncb_p np)
{
	u_char	num	= INB (nc_dsps);
	u_long	dsa	= INL (nc_dsa);
	ccb_p	cp	= ncr_ccb_from_dsa(np, dsa);
	u_char	target	= INB (nc_sdid) & 0x0f;
	tcb_p	tp	= &np->target[target];
	int	tmp;

	if (DEBUG_FLAGS & DEBUG_TINY) printk ("I#%d", num);

	switch (num) {
	/*
	**	See comments in the SCRIPTS code.
	*/
#ifdef SCSI_NCR_PCIQ_SYNC_ON_INTR
	case SIR_DUMMY_INTERRUPT:
		goto out;
#endif

	/*
	**	The C code is currently trying to recover from something.
	**	Typically, user want to abort some command.
	*/
	case SIR_SCRIPT_STOPPED:
	case SIR_TARGET_SELECTED:
	case SIR_ABORT_SENT:
	case SIR_AUTO_SENSE_DONE:
		ncr_sir_task_recovery(np, num);
		return;
	/*
	**	The device didn't go to MSG OUT phase after having 
	**	been selected with ATN. We donnot want to handle 
	**	that.
	*/
	case SIR_SEL_ATN_NO_MSG_OUT:
		printk ("%s:%d: No MSG OUT phase after selection with ATN.\n",
			ncr_name (np), target);
		goto out_stuck;
	/*
	**	The device didn't switch to MSG IN phase after 
	**	having reseleted the initiator.
	*/
	case SIR_RESEL_NO_MSG_IN:
	/*
	**	After reselection, the device sent a message that wasn't 
	**	an IDENTIFY.
	*/
	case SIR_RESEL_NO_IDENTIFY:
		/*
		**	If devices reselecting without sending an IDENTIFY 
		**	message still exist, this should help.
		**	We just assume lun=0, 1 CCB, no tag.
		*/
		if (tp->l0p) { 
			OUTL (nc_dsa, scr_to_cpu(tp->l0p->tasktbl[0]));
			OUTL_DSP (NCB_SCRIPT_PHYS (np, resel_go));
			return;
		}
	/*
	**	The device reselected a LUN we donnot know of.
	*/
	case SIR_RESEL_BAD_LUN:
		np->msgout[0] = M_RESET;
		goto out;
	/*
	**	The device reselected for an untagged nexus and we 
	**	haven't any.
	*/
	case SIR_RESEL_BAD_I_T_L:
		np->msgout[0] = M_ABORT;
		goto out;
	/*
	**	The device reselected for a tagged nexus that we donnot 
	**	have.
	*/
	case SIR_RESEL_BAD_I_T_L_Q:
		np->msgout[0] = M_ABORT_TAG;
		goto out;
	/*
	**	The SCRIPTS let us know that the device has grabbed 
	**	our message and will abort the job.
	*/
	case SIR_RESEL_ABORTED:
		np->lastmsg = np->msgout[0];
		np->msgout[0] = M_NOOP;
		printk ("%s:%d: message %x sent on bad reselection.\n",
			ncr_name (np), target, np->lastmsg);
		goto out;
	/*
	**	The SCRIPTS let us know that a message has been 
	**	successfully sent to the device.
	*/
	case SIR_MSG_OUT_DONE:
		np->lastmsg = np->msgout[0];
		np->msgout[0] = M_NOOP;
		/* Should we really care of that */
		if (np->lastmsg == M_PARITY || np->lastmsg == M_ID_ERROR) {
			if (cp) {
				cp->xerr_status &= ~XE_PARITY_ERR;
                                if (!cp->xerr_status)
					OUTOFFB (HF_PRT, HF_EXT_ERR);
			}
		}
		goto out;
	/*
	**	The device didn't send a GOOD SCSI status.
	**	We may have some work to do prior to allow 
	**	the SCRIPTS processor to continue.
	*/
	case SIR_BAD_STATUS:
		if (!cp)
			goto out;
		ncr_sir_to_redo(np, num, cp);
		return;
	/*
	**	We are asked by the SCRIPTS to prepare a 
	**	REJECT message.
	*/
	case SIR_REJECT_TO_SEND:
		ncr_print_msg(cp, "M_REJECT to send for ", np->msgin);
		np->msgout[0] = M_REJECT;
		goto out;
	/*
	**	We have been ODD at the end of a DATA IN 
	**	transfer and the device didn't send a 
	**	IGNORE WIDE RESIDUE message.
	**	It is a data overrun condition.
	*/
	case SIR_SWIDE_OVERRUN:
                if (cp) {
                        OUTONB (HF_PRT, HF_EXT_ERR);
                        cp->xerr_status |= XE_SWIDE_OVRUN;
                }
		goto out;
	/*
	**	We have been ODD at the end of a DATA OUT 
	**	transfer.
	**	It is a data underrun condition.
	*/
	case SIR_SODL_UNDERRUN:
                if (cp) {
                        OUTONB (HF_PRT, HF_EXT_ERR);
                        cp->xerr_status |= XE_SODL_UNRUN;
                }
		goto out;
	/*
	**	The device wants us to tranfer more data than 
	**	expected or in the wrong direction.
	**	The number of extra bytes is in scratcha.
	**	It is a data overrun condition.
	*/
	case SIR_DATA_OVERRUN:
		if (cp) {
			OUTONB (HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_EXTRA_DATA;
			cp->extra_bytes += INL (nc_scratcha);
		}
		goto out;
	/*
	**	The device switched to an illegal phase (4/5).
	*/
	case SIR_BAD_PHASE:
		if (cp) {
			OUTONB (HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_BAD_PHASE;
		}
		goto out;
	/*
	**	We received a message.
	*/
	case SIR_MSG_RECEIVED:
		if (!cp)
			goto out_stuck;
		switch (np->msgin [0]) {
		/*
		**	We received an extended message.
		**	We handle MODIFY DATA POINTER, SDTR, WDTR 
		**	and reject all other extended messages.
		*/
		case M_EXTENDED:
			switch (np->msgin [2]) {
			case M_X_MODIFY_DP:
				if (DEBUG_FLAGS & DEBUG_POINTER)
					ncr_print_msg(cp,"modify DP",np->msgin);
				tmp = (np->msgin[3]<<24) + (np->msgin[4]<<16) + 
				      (np->msgin[5]<<8)  + (np->msgin[6]);
				ncr_modify_dp(np, tp, cp, tmp);
				return;
			case M_X_SYNC_REQ:
				ncr_sync_nego(np, tp, cp);
				return;
			case M_X_WIDE_REQ:
				ncr_wide_nego(np, tp, cp);
				return;
			case M_X_PPR_REQ:
				ncr_ppr_nego(np, tp, cp);
				return;
			default:
				goto out_reject;
			}
			break;
		/*
		**	We received a 1/2 byte message not handled from SCRIPTS.
		**	We are only expecting MESSAGE REJECT and IGNORE WIDE 
		**	RESIDUE messages that haven't been anticipated by 
		**	SCRIPTS on SWIDE full condition. Unanticipated IGNORE 
		**	WIDE RESIDUE messages are aliased as MODIFY DP (-1).
		*/
		case M_IGN_RESIDUE:
			if (DEBUG_FLAGS & DEBUG_POINTER)
				ncr_print_msg(cp,"ign wide residue", np->msgin);
			ncr_modify_dp(np, tp, cp, -1);
			return;
		case M_REJECT:
			if (INB (HS_PRT) == HS_NEGOTIATE)
				ncr_nego_rejected(np, tp, cp);
			else {
				PRINT_ADDR(cp->cmd);
				printk ("M_REJECT received (%x:%x).\n",
					scr_to_cpu(np->lastmsg), np->msgout[0]);
			}
			goto out_clrack;
			break;
		default:
			goto out_reject;
		}
		break;
	/*
	**	We received an unknown message.
	**	Ignore all MSG IN phases and reject it.
	*/
	case SIR_MSG_WEIRD:
		ncr_print_msg(cp, "WEIRD message received", np->msgin);
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_weird));
		return;
	/*
	**	Negotiation failed.
	**	Target does not send us the reply.
	**	Remove the HS_NEGOTIATE status.
	*/
	case SIR_NEGO_FAILED:
		OUTB (HS_PRT, HS_BUSY);
	/*
	**	Negotiation failed.
	**	Target does not want answer message.
	*/
	case SIR_NEGO_PROTO:
		ncr_nego_default(np, tp, cp);
		goto out;
	};

out:
	OUTONB_STD ();
	return;
out_reject:
	OUTL_DSP (NCB_SCRIPTH_PHYS (np, msg_bad));
	return;
out_clrack:
	OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
	return;
out_stuck:
	return;
}


/*==========================================================
**
**
**	Acquire a control block
**
**
**==========================================================
*/

static	ccb_p ncr_get_ccb (ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = ncr_lp(np, tp, ln);
	u_short tag = NO_TAG;
	XPT_QUEHEAD *qp;
	ccb_p cp = (ccb_p) 0;

	/*
	**	Allocate a new CCB if needed.
	*/
	if (xpt_que_empty(&np->free_ccbq))
		(void) ncr_alloc_ccb(np);

	/*
	**	Look for a free CCB
	*/
	qp = xpt_remque_head(&np->free_ccbq);
	if (!qp)
		goto out;
	cp = xpt_que_entry(qp, struct ccb, link_ccbq);

	/*
	**	If the LCB is not yet available and we already 
	**	have queued a CCB for a LUN without LCB,
	**	give up. Otherwise all is fine. :-)
	*/
	if (!lp) {
		if (xpt_que_empty(&np->b0_ccbq))
			xpt_insque_head(&cp->link_ccbq, &np->b0_ccbq);
		else
			goto out_free;
	} else {
		/*
		**	Tune tag mode if asked by user.
		*/
		if (lp->queuedepth != lp->numtags) {
			ncr_setup_tags(np, tn, ln);
		}

		/*
		**	Get a tag for this nexus if required.
		**	Keep from using more tags than we can handle.
		*/
		if (lp->usetags) {
			if (lp->busyccbs < lp->maxnxs) {
				tag = lp->cb_tags[lp->ia_tag];
				++lp->ia_tag;
				if (lp->ia_tag == MAX_TAGS)
					lp->ia_tag = 0;
				cp->tags_si = lp->tags_si;
				++lp->tags_sum[cp->tags_si];
			}
			else
				goto out_free;
		}

		/*
		**	Put the CCB in the LUN wait queue and 
		**	count it as busy.
		*/
		xpt_insque_tail(&cp->link_ccbq, &lp->wait_ccbq);
		++lp->busyccbs;
	}

	/*
	**	Remember all informations needed to free this CCB.
	*/
	cp->to_abort = 0;
	cp->tag	   = tag;
	cp->target = tn;
	cp->lun    = ln;

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_LUN(np, tn, ln);
		printk ("ccb @%p using tag %d.\n", cp, tag);
	}

out:
	return cp;
out_free:
	xpt_insque_head(&cp->link_ccbq, &np->free_ccbq);
	return (ccb_p) 0;
}

/*==========================================================
**
**
**	Release one control block
**
**
**==========================================================
*/

static void ncr_free_ccb (ncb_p np, ccb_p cp)
{
	tcb_p tp = &np->target[cp->target];
	lcb_p lp = ncr_lp(np, tp, cp->lun);

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_LUN(np, cp->target, cp->lun);
		printk ("ccb @%p freeing tag %d.\n", cp, cp->tag);
	}

	/*
	**	If lun control block available, make available 
	**	the task slot and the tag if any.
	**	Decrement counters.
	*/
	if (lp) {
		if (cp->tag != NO_TAG) {
			lp->cb_tags[lp->if_tag++] = cp->tag;
			if (lp->if_tag == MAX_TAGS)
				lp->if_tag = 0;
			--lp->tags_sum[cp->tags_si];
			lp->tasktbl[cp->tag] = cpu_to_scr(np->p_bad_i_t_l_q);
		} else {
			lp->tasktbl[0] = cpu_to_scr(np->p_bad_i_t_l);
		}
		--lp->busyccbs;
		if (cp->queued) {
			--lp->queuedccbs;
		}
	}

	/*
	**	Make this CCB available.
	*/
	xpt_remque(&cp->link_ccbq);
	xpt_insque_head(&cp->link_ccbq, &np->free_ccbq);
	cp -> host_status = HS_IDLE;
	cp -> queued = 0;
}

/*------------------------------------------------------------------------
**	Allocate a CCB and initialize its fixed part.
**------------------------------------------------------------------------
**------------------------------------------------------------------------
*/
static ccb_p ncr_alloc_ccb(ncb_p np)
{
	ccb_p cp = 0;
	int hcode;

	/*
	**	Allocate memory for this CCB.
	*/
	cp = m_calloc_dma(sizeof(struct ccb), "CCB");
	if (!cp)
		return 0;

	/*
	**	Count it and initialyze it.
	*/
	np->actccbs++;

	/*
	**	Remember virtual and bus address of this ccb.
	*/
	cp->p_ccb 	   = vtobus(cp);

	/*
	**	Insert this ccb into the hashed list.
	*/
	hcode = CCB_HASH_CODE(cp->p_ccb);
	cp->link_ccbh = np->ccbh[hcode];
	np->ccbh[hcode] = cp;

	/*
	**	Initialyze the start and restart actions.
	*/
	cp->phys.header.go.start   = cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	cp->phys.header.go.restart = cpu_to_scr(NCB_SCRIPTH_PHYS(np,bad_i_t_l));

	/*
	**	Initilialyze some other fields.
	*/
	cp->phys.smsg_ext.addr = cpu_to_scr(NCB_PHYS(np, msgin[2]));

	/*
	**	Chain into wakeup list and free ccb queue.
	*/
	cp->link_ccb	= np->ccbc;
	np->ccbc	= cp;

	xpt_insque_head(&cp->link_ccbq, &np->free_ccbq);

	return cp;
}

/*------------------------------------------------------------------------
**	Look up a CCB from a DSA value.
**------------------------------------------------------------------------
**------------------------------------------------------------------------
*/
static ccb_p ncr_ccb_from_dsa(ncb_p np, u_long dsa)
{
	int hcode;
	ccb_p cp;

	hcode = CCB_HASH_CODE(dsa);
	cp = np->ccbh[hcode];
	while (cp) {
		if (cp->p_ccb == dsa)
			break;
		cp = cp->link_ccbh;
	}

	return cp;
}

/*==========================================================
**
**
**      Allocation of resources for Targets/Luns/Tags.
**
**
**==========================================================
*/


/*------------------------------------------------------------------------
**	Target control block initialisation.
**------------------------------------------------------------------------
**	This data structure is fully initialized after a SCSI command 
**	has been successfully completed for this target.
**------------------------------------------------------------------------
*/
static void ncr_init_tcb (ncb_p np, u_char tn)
{
	/*
	**	Check some alignments required by the chip.
	*/	
	assert (( (offsetof(struct ncr_reg, nc_sxfer) ^
		offsetof(struct tcb    , sval    )) &3) == 0);
	assert (( (offsetof(struct ncr_reg, nc_scntl3) ^
		offsetof(struct tcb    , wval    )) &3) == 0);
	if ((np->device_id == PCI_DEVICE_ID_LSI_53C1010) ||
		(np->device_id == PCI_DEVICE_ID_LSI_53C1010_66)){
		assert (( (offsetof(struct ncr_reg, nc_scntl4) ^
			offsetof(struct tcb    , uval    )) &3) == 0);
	}
}

/*------------------------------------------------------------------------
**	Lun control block allocation and initialization.
**------------------------------------------------------------------------
**	This data structure is allocated and initialized after a SCSI 
**	command has been successfully completed for this target/lun.
**------------------------------------------------------------------------
*/
static lcb_p ncr_alloc_lcb (ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = ncr_lp(np, tp, ln);

	/*
	**	Already done, return.
	*/
	if (lp)
		return lp;

	/*
	**	Initialize the target control block if not yet.
	*/
	ncr_init_tcb(np, tn);

	/*
	**	Allocate the lcb bus address array.
	**	Compute the bus address of this table.
	*/
	if (ln && !tp->luntbl) {
		int i;

		tp->luntbl = m_calloc_dma(256, "LUNTBL");
		if (!tp->luntbl)
			goto fail;
		for (i = 0 ; i < 64 ; i++)
			tp->luntbl[i] = cpu_to_scr(NCB_PHYS(np, resel_badlun));
		tp->b_luntbl = cpu_to_scr(vtobus(tp->luntbl));
	}

	/*
	**	Allocate the table of pointers for LUN(s) > 0, if needed.
	*/
	if (ln && !tp->lmp) {
		tp->lmp = m_calloc(MAX_LUN * sizeof(lcb_p), "LMP");
		if (!tp->lmp)
			goto fail;
	}

	/*
	**	Allocate the lcb.
	**	Make it available to the chip.
	*/
	lp = m_calloc_dma(sizeof(struct lcb), "LCB");
	if (!lp)
		goto fail;
	if (ln) {
		tp->lmp[ln] = lp;
		tp->luntbl[ln] = cpu_to_scr(vtobus(lp));
	}
	else {
		tp->l0p = lp;
		tp->b_lun0 = cpu_to_scr(vtobus(lp));
	}

	/*
	**	Initialize the CCB queue headers.
	*/
	xpt_que_init(&lp->busy_ccbq);
	xpt_que_init(&lp->wait_ccbq);

	/*
	**	Set max CCBs to 1 and use the default task array 
	**	by default.
	*/
	lp->maxnxs	= 1;
	lp->tasktbl	= &lp->tasktbl_0;
	lp->b_tasktbl	= cpu_to_scr(vtobus(lp->tasktbl));
	lp->tasktbl[0]	= cpu_to_scr(np->p_notask);
	lp->resel_task	= cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_notag));

	/*
	**	Initialize command queuing control.
	*/
	lp->busyccbs	= 1;
	lp->queuedccbs	= 1;
	lp->queuedepth	= 1;
fail:
	return lp;
}


/*------------------------------------------------------------------------
**	Lun control block setup on INQUIRY data received.
**------------------------------------------------------------------------
**	We only support WIDE, SYNC for targets and CMDQ for logical units.
**	This setup is done on each INQUIRY since we are expecting user 
**	will play with CHANGE DEFINITION commands. :-)
**------------------------------------------------------------------------
*/
static lcb_p ncr_setup_lcb (ncb_p np, u_char tn, u_char ln, u_char *inq_data)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = ncr_lp(np, tp, ln);
	u_char inq_byte7;
	int i;

	/*
	**	If no lcb, try to allocate it.
	*/
	if (!lp && !(lp = ncr_alloc_lcb(np, tn, ln)))
		goto fail;

#if 0	/* No more used. Left here as provision */
	/*
	**	Get device quirks.
	*/
	tp->quirks = 0;
	if (tp->quirks && bootverbose) {
		PRINT_LUN(np, tn, ln);
		printk ("quirks=%x.\n", tp->quirks);
	}
#endif

	/*
	**	Evaluate trustable target/unit capabilities.
	**	We only believe device version >= SCSI-2 that 
	**	use appropriate response data format (2).
	**	But it seems that some CCS devices also 
	**	support SYNC and I donnot want to frustrate 
	**	anybody. ;-)
	*/
	inq_byte7 = 0;
	if	((inq_data[2] & 0x7) >= 2 && (inq_data[3] & 0xf) == 2)
		inq_byte7 = inq_data[7];
	else if ((inq_data[2] & 0x7) == 1 && (inq_data[3] & 0xf) == 1)
		inq_byte7 = INQ7_SYNC;

	/*
	**	Throw away announced LUN capabilities if we are told 
	**	that there is no real device supported by the logical unit.
	*/
	if ((inq_data[0] & 0xe0) > 0x20 || (inq_data[0] & 0x1f) == 0x1f)
		inq_byte7 &= (INQ7_SYNC | INQ7_WIDE16);

	/*
	**	If user is wanting SYNC, force this feature.
	*/
	if (driver_setup.force_sync_nego)
		inq_byte7 |= INQ7_SYNC;

	/*
	**	Don't do PPR negotiations on SCSI-2 devices unless
	**	they set the DT bit (0x04) in byte 57 of the INQUIRY
	**	return data.
	*/
	if (((inq_data[2] & 0x07) < 3) && (inq_data[4] < 53 ||
					   !(inq_data[56] & 0x04))) {
		if (tp->minsync < 10)
			tp->minsync = 10;
		if (tp->usrsync < 10)
			tp->usrsync = 10;
	}

	/*
	**	Prepare negotiation if SIP capabilities have changed.
	*/
	tp->inq_done = 1;
	if ((inq_byte7 ^ tp->inq_byte7) & (INQ7_SYNC | INQ7_WIDE16)) {
		tp->inq_byte7 = inq_byte7;
		ncr_negotiate(np, tp);
	}

	/*
	**	If unit supports tagged commands, allocate and 
	**	initialyze the task table if not yet.
	*/
	if ((inq_byte7 & INQ7_QUEUE) && lp->tasktbl == &lp->tasktbl_0) {
		lp->tasktbl = m_calloc_dma(MAX_TASKS*4, "TASKTBL");
		if (!lp->tasktbl) {
			lp->tasktbl = &lp->tasktbl_0;
			goto fail;
		}
		lp->b_tasktbl = cpu_to_scr(vtobus(lp->tasktbl));
		for (i = 0 ; i < MAX_TASKS ; i++)
			lp->tasktbl[i] = cpu_to_scr(np->p_notask);

		lp->cb_tags = m_calloc(MAX_TAGS, "CB_TAGS");
		if (!lp->cb_tags)
			goto fail;
		for (i = 0 ; i < MAX_TAGS ; i++)
			lp->cb_tags[i] = i;

		lp->maxnxs = MAX_TAGS;
		lp->tags_stime = ktime_get(3*HZ);
	}

	/*
	**	Adjust tagged queueing status if needed.
	*/
	if ((inq_byte7 ^ lp->inq_byte7) & INQ7_QUEUE) {
		lp->inq_byte7 = inq_byte7;
		lp->numtags   = lp->maxtags;
		ncr_setup_tags (np, tn, ln);
	}

fail:
	return lp;
}

/*==========================================================
**
**
**	Build Scatter Gather Block
**
**
**==========================================================
**
**	The transfer area may be scattered among
**	several non adjacent physical pages.
**
**	We may use MAX_SCATTER blocks.
**
**----------------------------------------------------------
*/

/*
**	We try to reduce the number of interrupts caused
**	by unexpected phase changes due to disconnects.
**	A typical harddisk may disconnect before ANY block.
**	If we wanted to avoid unexpected phase changes at all
**	we had to use a break point every 512 bytes.
**	Of course the number of scatter/gather blocks is
**	limited.
**	Under Linux, the scatter/gatter blocks are provided by 
**	the generic driver. We just have to copy addresses and 
**	sizes to the data segment array.
*/

/*
**	For 64 bit systems, we use the 8 upper bits of the size field 
**	to provide bus address bits 32-39 to the SCRIPTS processor.
**	This allows the 895A and 896 to address up to 1 TB of memory.
**	For 32 bit chips on 64 bit systems, we must be provided with 
**	memory addresses that fit into the first 32 bit bus address 
**	range and so, this does not matter and we expect an error from 
**	the chip if this ever happen.
**
**	We use a separate function for the case Linux does not provide 
**	a scatter list in order to allow better code optimization 
**	for the case we have a scatter list (BTW, for now this just wastes  
**	about 40 bytes of code for x86, but my guess is that the scatter 
**	code will get more complex later).
*/

#define SCATTER_ONE(data, badd, len)					\
	(data)->addr = cpu_to_scr(badd);				\
	(data)->size = cpu_to_scr((((badd) >> 8) & 0xff000000) + len);

#define CROSS_16MB(p, n) (((((u_long) p) + n - 1) ^ ((u_long) p)) & ~0xffffff)

static	int ncr_scatter_no_sglist(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd)
{
	struct scr_tblmove *data = &cp->phys.data[MAX_SCATTER-1];
	int segment;

	cp->data_len = cmd->request_bufflen;

	if (cmd->request_bufflen) {
		dma_addr_t baddr = map_scsi_single_data(np, cmd);

		SCATTER_ONE(data, baddr, cmd->request_bufflen);
		if (CROSS_16MB(baddr, cmd->request_bufflen)) {
			cp->host_flags |= HF_PM_TO_C;
#ifdef DEBUG_896R1
printk("He! we are crossing a 16 MB boundary (0x%lx, 0x%x)\n",
	baddr, cmd->request_bufflen);
#endif
		}
		segment = 1;
	}
	else
		segment = 0;

	return segment;
}

/*
**	DEL 472 - 53C896 Rev 1 - Part Number 609-0393055 - ITEM 5.
**
**	We disable data phase mismatch handling from SCRIPTS for data 
**	transfers that contains scatter/gather entries that cross  
**	a 16 MB boundary.
**	We use a different scatter function for 896 rev. 1 that needs 
**	such a work-around. Doing so, we do not affect performance for 
**	other chips.
**	This problem should not be triggered for disk IOs under Linux, 
**	since such IOs are performed using pages and buffers that are 
**	nicely power-of-two sized and aligned. But, since this may change 
**	at any time, a work-around was required.
*/
static int ncr_scatter_896R1(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd)
{
	int segn;
	int use_sg = (int) cmd->use_sg;

	cp->data_len = 0;

	if (!use_sg)
		segn = ncr_scatter_no_sglist(np, cp, cmd);
	else if (use_sg > MAX_SCATTER)
		segn = -1;
	else {
		struct scatterlist *scatter = (struct scatterlist *)cmd->buffer;
		struct scr_tblmove *data;

		use_sg = map_scsi_sg_data(np, cmd);
		data = &cp->phys.data[MAX_SCATTER - use_sg];

		for (segn = 0; segn < use_sg; segn++) {
			dma_addr_t baddr = scsi_sg_dma_address(&scatter[segn]);
			unsigned int len = scsi_sg_dma_len(&scatter[segn]);

			SCATTER_ONE(&data[segn],
				    baddr,
				    len);
			if (CROSS_16MB(baddr, scatter[segn].length)) {
				cp->host_flags |= HF_PM_TO_C;
#ifdef DEBUG_896R1
printk("He! we are crossing a 16 MB boundary (0x%lx, 0x%x)\n",
	baddr, scatter[segn].length);
#endif
			}
			cp->data_len += len;
		}
	}

	return segn;
}

static int ncr_scatter(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd)
{
	int segment;
	int use_sg = (int) cmd->use_sg;

	cp->data_len = 0;

	if (!use_sg)
		segment = ncr_scatter_no_sglist(np, cp, cmd);
	else if (use_sg > MAX_SCATTER)
		segment = -1;
	else {
		struct scatterlist *scatter = (struct scatterlist *)cmd->buffer;
		struct scr_tblmove *data;

		use_sg = map_scsi_sg_data(np, cmd);
		data = &cp->phys.data[MAX_SCATTER - use_sg];

		for (segment = 0; segment < use_sg; segment++) {
			dma_addr_t baddr = scsi_sg_dma_address(&scatter[segment]);
			unsigned int len = scsi_sg_dma_len(&scatter[segment]);

			SCATTER_ONE(&data[segment],
				    baddr,
				    len);
			cp->data_len += len;
		}
	}

	return segment;
}

/*==========================================================
**
**
**	Test the pci bus snoop logic :-(
**
**	Has to be called with interrupts disabled.
**
**
**==========================================================
*/

#ifndef SCSI_NCR_IOMAPPED
static int __init ncr_regtest (struct ncb* np)
{
	register volatile u_int32 data;
	/*
	**	ncr registers may NOT be cached.
	**	write 0xffffffff to a read only register area,
	**	and try to read it back.
	*/
	data = 0xffffffff;
	OUTL_OFF(offsetof(struct ncr_reg, nc_dstat), data);
	data = INL_OFF(offsetof(struct ncr_reg, nc_dstat));
#if 1
	if (data == 0xffffffff) {
#else
	if ((data & 0xe2f0fffd) != 0x02000080) {
#endif
		printk ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return (0x10);
	};
	return (0);
}
#endif

static int __init ncr_snooptest (struct ncb* np)
{
	u_int32	ncr_rd, ncr_wr, ncr_bk, host_rd, host_wr, pc;
	u_char  dstat;
	int	i, err=0;
#ifndef SCSI_NCR_IOMAPPED
	if (np->reg) {
            err |= ncr_regtest (np);
            if (err) return (err);
	}
#endif
restart_test:
	/*
	**	Enable Master Parity Checking as we intend 
	**	to enable it for normal operations.
	*/
	OUTB (nc_ctest4, (np->rv_ctest4 & MPEE));
	/*
	**	init
	*/
	pc  = NCB_SCRIPTH0_PHYS (np, snooptest);
	host_wr = 1;
	ncr_wr  = 2;
	/*
	**	Set memory and register.
	*/
	np->ncr_cache = cpu_to_scr(host_wr);
	OUTL (nc_temp, ncr_wr);
	/*
	**	Start script (exchange values)
	*/
	OUTL (nc_dsa, np->p_ncb);
	OUTL_DSP (pc);
	/*
	**	Wait 'til done (with timeout)
	*/
	for (i=0; i<NCR_SNOOP_TIMEOUT; i++)
		if (INB(nc_istat) & (INTF|SIP|DIP))
			break;
	if (i>=NCR_SNOOP_TIMEOUT) {
		printk ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	};
	/*
	**	Check for fatal DMA errors.
	*/
	dstat = INB (nc_dstat);
#if 1	/* Band aiding for broken hardwares that fail PCI parity */
	if ((dstat & MDPE) && (np->rv_ctest4 & MPEE)) {
		printk ("%s: PCI DATA PARITY ERROR DETECTED - "
			"DISABLING MASTER DATA PARITY CHECKING.\n",
			ncr_name(np));
		np->rv_ctest4 &= ~MPEE;
		goto restart_test;
	}
#endif
	if (dstat & (MDPE|BF|IID)) {
		printk ("CACHE TEST FAILED: DMA error (dstat=0x%02x).", dstat);
		return (0x80);
	}
	/*
	**	Save termination position.
	*/
	pc = INL (nc_dsp);
	/*
	**	Read memory and register.
	*/
	host_rd = scr_to_cpu(np->ncr_cache);
	ncr_rd  = INL (nc_scratcha);
	ncr_bk  = INL (nc_temp);
	/*
	**	Check termination position.
	*/
	if (pc != NCB_SCRIPTH0_PHYS (np, snoopend)+8) {
		printk ("CACHE TEST FAILED: script execution failed.\n");
		printk ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) NCB_SCRIPTH0_PHYS (np, snooptest), (u_long) pc,
			(u_long) NCB_SCRIPTH0_PHYS (np, snoopend) +8);
		return (0x40);
	};
	/*
	**	Show results.
	*/
	if (host_wr != ncr_rd) {
		printk ("CACHE TEST FAILED: host wrote %d, ncr read %d.\n",
			(int) host_wr, (int) ncr_rd);
		err |= 1;
	};
	if (host_rd != ncr_wr) {
		printk ("CACHE TEST FAILED: ncr wrote %d, host read %d.\n",
			(int) ncr_wr, (int) host_rd);
		err |= 2;
	};
	if (ncr_bk != ncr_wr) {
		printk ("CACHE TEST FAILED: ncr wrote %d, read back %d.\n",
			(int) ncr_wr, (int) ncr_bk);
		err |= 4;
	};
	return (err);
}

/*==========================================================
**
**	Determine the ncr's clock frequency.
**	This is essential for the negotiation
**	of the synchronous transfer rate.
**
**==========================================================
**
**	Note: we have to return the correct value.
**	THERE IS NO SAFE DEFAULT VALUE.
**
**	Most NCR/SYMBIOS boards are delivered with a 40 Mhz clock.
**	53C860 and 53C875 rev. 1 support fast20 transfers but 
**	do not have a clock doubler and so are provided with a 
**	80 MHz clock. All other fast20 boards incorporate a doubler 
**	and so should be delivered with a 40 MHz clock.
**	The recent fast40 chips  (895/896/895A) and the
**	fast80 chip (C1010) use a 40 Mhz base clock 
**	and provide a clock quadrupler (160 Mhz). The code below 
**	tries to deal as cleverly as possible with all this stuff.
**
**----------------------------------------------------------
*/

/*
 *	Select NCR SCSI clock frequency
 */
static void ncr_selectclock(ncb_p np, u_char scntl3)
{
	if (np->multiplier < 2) {
		OUTB(nc_scntl3,	scntl3);
		return;
	}

	if (bootverbose >= 2)
		printk ("%s: enabling clock multiplier\n", ncr_name(np));

	OUTB(nc_stest1, DBLEN);	   /* Enable clock multiplier		  */

	if ( (np->device_id != PCI_DEVICE_ID_LSI_53C1010) && 
			(np->device_id != PCI_DEVICE_ID_LSI_53C1010_66) && 
						(np->multiplier > 2)) {  
		int i = 20;	 /* Poll bit 5 of stest4 for quadrupler */
		while (!(INB(nc_stest4) & LCKFRQ) && --i > 0)
			UDELAY (20);
		if (!i)
		    printk("%s: the chip cannot lock the frequency\n",
						 ncr_name(np));

	} else			/* Wait 120 micro-seconds for multiplier*/
		UDELAY (120);

	OUTB(nc_stest3, HSC);		/* Halt the scsi clock		*/
	OUTB(nc_scntl3,	scntl3);
	OUTB(nc_stest1, (DBLEN|DBLSEL));/* Select clock multiplier	*/
	OUTB(nc_stest3, 0x00);		/* Restart scsi clock 		*/
}


/*
 *	calculate NCR SCSI clock frequency (in KHz)
 */
static unsigned __init ncrgetfreq (ncb_p np, int gen)
{
	unsigned int ms = 0;
	unsigned int f;
	int count;

	/*
	 * Measure GEN timer delay in order 
	 * to calculate SCSI clock frequency
	 *
	 * This code will never execute too
	 * many loop iterations (if DELAY is 
	 * reasonably correct). It could get
	 * too low a delay (too high a freq.)
	 * if the CPU is slow executing the 
	 * loop for some reason (an NMI, for
	 * example). For this reason we will
	 * if multiple measurements are to be 
	 * performed trust the higher delay 
	 * (lower frequency returned).
	 */
	OUTW (nc_sien , 0x0);/* mask all scsi interrupts */
				/* enable general purpose timer */
	(void) INW (nc_sist);	/* clear pending scsi interrupt */
	OUTB (nc_dien , 0);	/* mask all dma interrupts */
	(void) INW (nc_sist);	/* another one, just to be sure :) */
	OUTB (nc_scntl3, 4);	/* set pre-scaler to divide by 3 */
	OUTB (nc_stime1, 0);	/* disable general purpose timer */
	OUTB (nc_stime1, gen);	/* set to nominal delay of 1<<gen * 125us */
				/* Temporary fix for udelay issue with Alpha
					platform */
	while (!(INW(nc_sist) & GEN) && ms++ < 100000) {
		/* count 1ms */
		for (count = 0; count < 10; count++)
			UDELAY (100);	
	}
	OUTB (nc_stime1, 0);	/* disable general purpose timer */
 	/*
 	 * set prescaler to divide by whatever 0 means
 	 * 0 ought to choose divide by 2, but appears
 	 * to set divide by 3.5 mode in my 53c810 ...
 	 */
 	OUTB (nc_scntl3, 0);

  	/*
 	 * adjust for prescaler, and convert into KHz 
	 * scale values derived empirically.
  	 */
	f = ms ? ((1 << gen) * 4340) / ms : 0;

	if (bootverbose >= 2)
		printk ("%s: Delay (GEN=%d): %u msec, %u KHz\n",
			ncr_name(np), gen, ms, f);

	return f;
}

static unsigned __init ncr_getfreq (ncb_p np)
{
	u_int f1, f2;
	int gen = 11;

	(void) ncrgetfreq (np, gen);	/* throw away first result */
	f1 = ncrgetfreq (np, gen);
	f2 = ncrgetfreq (np, gen);
	if (f1 > f2) f1 = f2;		/* trust lower result	*/
	return f1;
}

/*
 *	Get/probe NCR SCSI clock frequency
 */
static void __init ncr_getclock (ncb_p np, int mult)
{
	unsigned char scntl3 = np->sv_scntl3;
	unsigned char stest1 = np->sv_stest1;
	unsigned f1;

	np->multiplier = 1;
	f1 = 40000;

	/*
	**	True with 875/895/896/895A with clock multiplier selected
	*/
	if (mult > 1 && (stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		if (bootverbose >= 2)
			printk ("%s: clock multiplier found\n", ncr_name(np));
		np->multiplier = mult;
	}

	/*
	**	If multiplier not found or scntl3 not 7,5,3,
	**	reset chip and get frequency from general purpose timer.
	**	Otherwise trust scntl3 BIOS setting.
	*/
	if (np->multiplier != mult || (scntl3 & 7) < 3 || !(scntl3 & 1)) {
		OUTB (nc_stest1, 0);		/* make sure doubler is OFF */
		f1 = ncr_getfreq (np);

		if (bootverbose)
			printk ("%s: NCR clock is %uKHz\n", ncr_name(np), f1);

		if	(f1 < 55000)		f1 =  40000;
		else				f1 =  80000;

		/*
		**	Suggest to also check the PCI clock frequency 
		**	to make sure our frequency calculation algorithm 
		**	is not too biased.
		*/
		if (np->features & FE_66MHZ) {
			np->pciclock_min = (66000*55+80-1)/80;
			np->pciclock_max = (66000*55)/40;
		}
		else {
			np->pciclock_min = (33000*55+80-1)/80;
			np->pciclock_max = (33000*55)/40;
		}

		if (f1 == 40000 && mult > 1) {
			if (bootverbose >= 2)
				printk ("%s: clock multiplier assumed\n", ncr_name(np));
			np->multiplier	= mult;
		}
	} else {
		if	((scntl3 & 7) == 3)	f1 =  40000;
		else if	((scntl3 & 7) == 5)	f1 =  80000;
		else 				f1 = 160000;

		f1 /= np->multiplier;
	}

	/*
	**	Compute controller synchronous parameters.
	*/
	f1		*= np->multiplier;
	np->clock_khz	= f1;
}

/*
 *	Get/probe PCI clock frequency
 */
static u_int __init ncr_getpciclock (ncb_p np)
{
	static u_int f;

	OUTB (nc_stest1, SCLK);	/* Use the PCI clock as SCSI clock */
	f = ncr_getfreq (np);
	OUTB (nc_stest1, 0);

	return f;
}

/*===================== LINUX ENTRY POINTS SECTION ==========================*/

#ifndef uchar
#define uchar unsigned char
#endif

#ifndef ushort
#define ushort unsigned short
#endif

#ifndef ulong
#define ulong unsigned long
#endif

/* ---------------------------------------------------------------------
**
**	Driver setup from the boot command line
**
** ---------------------------------------------------------------------
*/

#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

#define OPT_TAGS		1
#define OPT_MASTER_PARITY	2
#define OPT_SCSI_PARITY		3
#define OPT_DISCONNECTION	4
#define OPT_SPECIAL_FEATURES	5
#define OPT_RESERVED_1		6
#define OPT_FORCE_SYNC_NEGO	7
#define OPT_REVERSE_PROBE	8
#define OPT_DEFAULT_SYNC	9
#define OPT_VERBOSE		10
#define OPT_DEBUG		11
#define OPT_BURST_MAX		12
#define OPT_LED_PIN		13
#define OPT_MAX_WIDE		14
#define OPT_SETTLE_DELAY	15
#define OPT_DIFF_SUPPORT	16
#define OPT_IRQM		17
#define OPT_PCI_FIX_UP		18
#define OPT_BUS_CHECK		19
#define OPT_OPTIMIZE		20
#define OPT_RECOVERY		21
#define OPT_SAFE_SETUP		22
#define OPT_USE_NVRAM		23
#define OPT_EXCLUDE		24
#define OPT_HOST_ID		25

#ifdef SCSI_NCR_IARB_SUPPORT
#define OPT_IARB		26
#endif

static char setup_token[] __initdata = 
	"tags:"   "mpar:"
	"spar:"   "disc:"
	"specf:"  "_rsvd1:"
	"fsn:"    "revprob:"
	"sync:"   "verb:"
	"debug:"  "burst:"
	"led:"    "wide:"
	"settle:" "diff:"
	"irqm:"   "pcifix:"
	"buschk:" "optim:"
	"recovery:"
	"safe:"   "nvram:"
	"excl:"   "hostid:"
#ifdef SCSI_NCR_IARB_SUPPORT
	"iarb:"
#endif
	;	/* DONNOT REMOVE THIS ';' */

#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

static int __init get_setup_token(char *p)
{
	char *cur = setup_token;
	char *pc;
	int i = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		++pc;
		++i;
		if (!strncmp(p, cur, pc - cur))
			return i;
		cur = pc;
	}
	return 0;
}


int __init sym53c8xx_setup(char *str)
{
#ifdef SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
	char *cur = str;
	char *pc, *pv;
	unsigned long val;
	int i,  c;
	int xi = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		char *pe;

		val = 0;
		pv = pc;
		c = *++pv;

		if	(c == 'n')
			val = 0;
		else if	(c == 'y')
			val = 1;
		else
			val = (int) simple_strtoul(pv, &pe, 0);

		switch (get_setup_token(cur)) {
		case OPT_TAGS:
			driver_setup.default_tags = val;
			if (pe && *pe == '/') {
				i = 0;
				while (*pe && *pe != ARG_SEP && 
					i < sizeof(driver_setup.tag_ctrl)-1) {
					driver_setup.tag_ctrl[i++] = *pe++;
				}
				driver_setup.tag_ctrl[i] = '\0';
			}
			break;
		case OPT_MASTER_PARITY:
			driver_setup.master_parity = val;
			break;
		case OPT_SCSI_PARITY:
			driver_setup.scsi_parity = val;
			break;
		case OPT_DISCONNECTION:
			driver_setup.disconnection = val;
			break;
		case OPT_SPECIAL_FEATURES:
			driver_setup.special_features = val;
			break;
		case OPT_FORCE_SYNC_NEGO:
			driver_setup.force_sync_nego = val;
			break;
		case OPT_REVERSE_PROBE:
			driver_setup.reverse_probe = val;
			break;
		case OPT_DEFAULT_SYNC:
			driver_setup.default_sync = val;
			break;
		case OPT_VERBOSE:
			driver_setup.verbose = val;
			break;
		case OPT_DEBUG:
			driver_setup.debug = val;
			break;
		case OPT_BURST_MAX:
			driver_setup.burst_max = val;
			break;
		case OPT_LED_PIN:
			driver_setup.led_pin = val;
			break;
		case OPT_MAX_WIDE:
			driver_setup.max_wide = val? 1:0;
			break;
		case OPT_SETTLE_DELAY:
			driver_setup.settle_delay = val;
			break;
		case OPT_DIFF_SUPPORT:
			driver_setup.diff_support = val;
			break;
		case OPT_IRQM:
			driver_setup.irqm = val;
			break;
		case OPT_PCI_FIX_UP:
			driver_setup.pci_fix_up	= val;
			break;
		case OPT_BUS_CHECK:
			driver_setup.bus_check = val;
			break;
		case OPT_OPTIMIZE:
			driver_setup.optimize = val;
			break;
		case OPT_RECOVERY:
			driver_setup.recovery = val;
			break;
		case OPT_USE_NVRAM:
			driver_setup.use_nvram = val;
			break;
		case OPT_SAFE_SETUP:
			memcpy(&driver_setup, &driver_safe_setup,
				sizeof(driver_setup));
			break;
		case OPT_EXCLUDE:
			if (xi < SCSI_NCR_MAX_EXCLUDES)
				driver_setup.excludes[xi++] = val;
			break;
		case OPT_HOST_ID:
			driver_setup.host_id = val;
			break;
#ifdef SCSI_NCR_IARB_SUPPORT
		case OPT_IARB:
			driver_setup.iarb = val;
			break;
#endif
		default:
			printk("sym53c8xx_setup: unexpected boot option '%.*s' ignored\n", (int)(pc-cur+1), cur);
			break;
		}

		if ((cur = strchr(cur, ARG_SEP)) != NULL)
			++cur;
	}
#endif /* SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT */
	return 1;
}

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,13)
#ifndef MODULE
__setup("sym53c8xx=", sym53c8xx_setup);
#endif
#endif

static int 
sym53c8xx_pci_init(Scsi_Host_Template *tpnt, pcidev_t pdev, ncr_device *device);

/*
**   Linux entry point for SYM53C8XX devices detection routine.
**
**   Called by the middle-level scsi drivers at initialization time,
**   or at module installation.
**
**   Read the PCI configuration and try to attach each
**   detected NCR board.
**
**   If NVRAM is present, try to attach boards according to 
**   the used defined boot order.
**
**   Returns the number of boards successfully attached.
*/

static void __init ncr_print_driver_setup(void)
{
#define YesNo(y)	y ? 'y' : 'n'
	printk (NAME53C8XX ": setup=disc:%c,specf:%d,tags:%d,sync:%d,"
		"burst:%d,wide:%c,diff:%d,revprob:%c,buschk:0x%x\n",
		YesNo(driver_setup.disconnection),
		driver_setup.special_features,
		driver_setup.default_tags,
		driver_setup.default_sync,
		driver_setup.burst_max,
		YesNo(driver_setup.max_wide),
		driver_setup.diff_support,
		YesNo(driver_setup.reverse_probe),
		driver_setup.bus_check);

	printk (NAME53C8XX ": setup=mpar:%c,spar:%c,fsn=%c,verb:%d,debug:0x%x,"
		"led:%c,settle:%d,irqm:0x%x,nvram:0x%x,pcifix:0x%x\n",
		YesNo(driver_setup.master_parity),
		YesNo(driver_setup.scsi_parity),
		YesNo(driver_setup.force_sync_nego),
		driver_setup.verbose,
		driver_setup.debug,
		YesNo(driver_setup.led_pin),
		driver_setup.settle_delay,
		driver_setup.irqm,
		driver_setup.use_nvram,
		driver_setup.pci_fix_up);
#undef YesNo
}

/*===================================================================
**   SYM53C8XX devices description table and chip ids list.
**===================================================================
*/

static ncr_chip	ncr_chip_table[] __initdata	= SCSI_NCR_CHIP_TABLE;
static ushort	ncr_chip_ids[]   __initdata	= SCSI_NCR_CHIP_IDS;

#ifdef	SCSI_NCR_PQS_PDS_SUPPORT
/*===================================================================
**    Detect all NCR PQS/PDS boards and keep track of their bus nr.
**
**    The NCR PQS or PDS card is constructed as a DEC bridge
**    behind which sit a proprietary NCR memory controller and
**    four or two 53c875s as separate devices.  In its usual mode
**    of operation, the 875s are slaved to the memory controller
**    for all transfers.  We can tell if an 875 is part of a
**    PQS/PDS or not since if it is, it will be on the same bus
**    as the memory controller.  To operate with the Linux
**    driver, the memory controller is disabled and the 875s
**    freed to function independently.  The only wrinkle is that
**    the preset SCSI ID (which may be zero) must be read in from
**    a special configuration space register of the 875
**===================================================================
*/
#define	SCSI_NCR_MAX_PQS_BUS	16
static int pqs_bus[SCSI_NCR_MAX_PQS_BUS] __initdata = { 0 };

static void __init ncr_detect_pqs_pds(void)
{
	short index;
	pcidev_t dev = PCIDEV_NULL;

	for(index=0; index < SCSI_NCR_MAX_PQS_BUS; index++) {
		u_char tmp;

		dev = pci_find_device(0x101a, 0x0009, dev);
		if (dev == PCIDEV_NULL) {
			pqs_bus[index] = -1;
			break;
		}
		printk(KERN_INFO NAME53C8XX ": NCR PQS/PDS memory controller detected on bus %d\n", PciBusNumber(dev));
		pci_read_config_byte(dev, 0x44, &tmp);
		/* bit 1: allow individual 875 configuration */
		tmp |= 0x2;
		pci_write_config_byte(dev, 0x44, tmp);
		pci_read_config_byte(dev, 0x45, &tmp);
		/* bit 2: drive individual 875 interrupts to the bus */
		tmp |= 0x4;
		pci_write_config_byte(dev, 0x45, tmp);

		pqs_bus[index] = PciBusNumber(dev);
	}
}
#endif /* SCSI_NCR_PQS_PDS_SUPPORT */

/*===================================================================
**    Detect all 53c8xx hosts and then attach them.
**
**    If we are using NVRAM, once all hosts are detected, we need to 
**    check any NVRAM for boot order in case detect and boot order 
**    differ and attach them using the order in the NVRAM.
**
**    If no NVRAM is found or data appears invalid attach boards in 
**    the order they are detected.
**===================================================================
*/
int __init sym53c8xx_detect(Scsi_Host_Template *tpnt)
{
	pcidev_t pcidev;
	int i, j, chips, hosts, count;
	int attach_count = 0;
	ncr_device *devtbl, *devp;
#ifdef SCSI_NCR_NVRAM_SUPPORT
	ncr_nvram  nvram0, nvram, *nvp;
#endif

	/*
	**    PCI is required.
	*/
	if (!pci_present())
		return 0;

	/*
	**    Initialize driver general stuff.
	*/
#ifdef SCSI_NCR_PROC_INFO_SUPPORT
#if LINUX_VERSION_CODE < LinuxVersionCode(2,3,27)
     tpnt->proc_dir  = &proc_scsi_sym53c8xx;
#else
     tpnt->proc_name = NAME53C8XX;
#endif
     tpnt->proc_info = sym53c8xx_proc_info;
#endif

#if	defined(SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT) && defined(MODULE)
if (sym53c8xx)
	sym53c8xx_setup(sym53c8xx);
#endif
#ifdef SCSI_NCR_DEBUG_INFO_SUPPORT
	ncr_debug = driver_setup.debug;
#endif

	if (initverbose >= 2)
		ncr_print_driver_setup();

	/*
	**	Allocate the device table since we donnot want to 
	**	overflow the kernel stack.
	**	1 x 4K PAGE is enough for more than 40 devices for i386.
	*/
	devtbl = m_calloc(PAGE_SIZE, "devtbl");
	if (!devtbl)
		return 0;

	/*
	**    Detect all NCR PQS/PDS memory controllers.
	*/
#ifdef	SCSI_NCR_PQS_PDS_SUPPORT
	ncr_detect_pqs_pds();
#endif

	/* 
	**    Detect all 53c8xx hosts.
	**    Save the first Symbios NVRAM content if any 
	**    for the boot order.
	*/
	chips	= sizeof(ncr_chip_ids)	/ sizeof(ncr_chip_ids[0]);
	hosts	= PAGE_SIZE		/ sizeof(*devtbl);
#ifdef SCSI_NCR_NVRAM_SUPPORT
	nvp = (driver_setup.use_nvram & 0x1) ? &nvram0 : 0;
#endif
	j = 0;
	count = 0;
	pcidev = PCIDEV_NULL;
	while (1) {
		char *msg = "";
		if (count >= hosts)
			break;
		if (j >= chips)
			break;
		i = driver_setup.reverse_probe ? chips - 1 - j : j;
		pcidev = pci_find_device(PCI_VENDOR_ID_NCR, ncr_chip_ids[i],
					 pcidev);
		if (pcidev == PCIDEV_NULL) {
			++j;
			continue;
		}
		if (pci_enable_device(pcidev)) /* @!*!$&*!%-*#;! */
			continue;
		/* Some HW as the HP LH4 may report twice PCI devices */
		for (i = 0; i < count ; i++) {
			if (devtbl[i].slot.bus	     == PciBusNumber(pcidev) && 
			    devtbl[i].slot.device_fn == PciDeviceFn(pcidev))
				break;
		}
		if (i != count)	/* Ignore this device if we already have it */
			continue;
		devp = &devtbl[count];
		devp->host_id = driver_setup.host_id;
		devp->attach_done = 0;
		if (sym53c8xx_pci_init(tpnt, pcidev, devp)) {
			continue;
		}
		++count;
#ifdef SCSI_NCR_NVRAM_SUPPORT
		if (nvp) {
			ncr_get_nvram(devp, nvp);
			switch(nvp->type) {
			case SCSI_NCR_SYMBIOS_NVRAM:
				/*
				 *   Switch to the other nvram buffer, so that 
				 *   nvram0 will contain the first Symbios 
				 *   format NVRAM content with boot order.
				 */
				nvp = &nvram;
				msg = "with Symbios NVRAM";
				break;
			case SCSI_NCR_TEKRAM_NVRAM:
				msg = "with Tekram NVRAM";
				break;
			}
		}
#endif
#ifdef	SCSI_NCR_PQS_PDS_SUPPORT
		if (devp->pqs_pds)
			msg = "(NCR PQS/PDS)";
#endif
		printk(KERN_INFO NAME53C8XX ": 53c%s detected %s\n",
		       devp->chip.name, msg);
	}

	/*
	**    If we have found a SYMBIOS NVRAM, use first the NVRAM boot 
	**    sequence as device boot order.
	**    check devices in the boot record against devices detected. 
	**    attach devices if we find a match. boot table records that 
	**    do not match any detected devices will be ignored. 
	**    devices that do not match any boot table will not be attached
	**    here but will attempt to be attached during the device table 
	**    rescan.
	*/
#ifdef SCSI_NCR_NVRAM_SUPPORT
	if (!nvp || nvram0.type != SCSI_NCR_SYMBIOS_NVRAM)
		goto next;
	for (i = 0; i < 4; i++) {
		Symbios_host *h = &nvram0.data.Symbios.host[i];
		for (j = 0 ; j < count ; j++) {
			devp = &devtbl[j];
			if (h->device_fn != devp->slot.device_fn ||
			    h->bus_nr	 != devp->slot.bus	 ||
			    h->device_id != devp->chip.device_id)
				continue;
			if (devp->attach_done)
				continue;
			if (h->flags & SYMBIOS_INIT_SCAN_AT_BOOT) {
				ncr_get_nvram(devp, nvp);
				if (!ncr_attach (tpnt, attach_count, devp))
					attach_count++;
			}
			else if (!(driver_setup.use_nvram & 0x80))
				printk(KERN_INFO NAME53C8XX
				       ": 53c%s state OFF thus not attached\n",
				       devp->chip.name);
			else
				continue;

			devp->attach_done = 1;
			break;
		}
	}
next:
#endif

	/* 
	**    Rescan device list to make sure all boards attached.
	**    Devices without boot records will not be attached yet
	**    so try to attach them here.
	*/
	for (i= 0; i < count; i++) {
		devp = &devtbl[i];
		if (!devp->attach_done) {
#ifdef SCSI_NCR_NVRAM_SUPPORT
			ncr_get_nvram(devp, nvp);
#endif
			if (!ncr_attach (tpnt, attach_count, devp))
				attach_count++;
		}
	}

	m_free(devtbl, PAGE_SIZE, "devtbl");

	return attach_count;
}

/*===================================================================
**   Read and check the PCI configuration for any detected NCR 
**   boards and save data for attaching after all boards have 
**   been detected.
**===================================================================
*/
static int __init
sym53c8xx_pci_init(Scsi_Host_Template *tpnt, pcidev_t pdev, ncr_device *device)
{
	u_short vendor_id, device_id, command, status_reg;
	u_char cache_line_size, latency_timer;
	u_char suggested_cache_line_size = 0;
	u_char pci_fix_up = driver_setup.pci_fix_up;
	u_char revision;
	u_int irq;
	u_long base, base_c, base_2, base_2_c, io_port; 
	int i;
	ncr_chip *chip;

	printk(KERN_INFO NAME53C8XX ": at PCI bus %d, device %d, function %d\n",
		PciBusNumber(pdev),
		(int) (PciDeviceFn(pdev) & 0xf8) >> 3,
		(int) (PciDeviceFn(pdev) & 7));

	/*
	**    Read info from the PCI config space.
	**    pci_read_config_xxx() functions are assumed to be used for 
	**    successfully detected PCI devices.
	*/
	vendor_id = PciVendorId(pdev);
	device_id = PciDeviceId(pdev);
	irq	  = PciIrqLine(pdev);

	i = pci_get_base_address(pdev, 0, &io_port);
	io_port = pci_get_base_cookie(pdev, 0);

	base_c = pci_get_base_cookie(pdev, i);
	i = pci_get_base_address(pdev, i, &base);

	base_2_c = pci_get_base_cookie(pdev, i);
	(void) pci_get_base_address(pdev, i, &base_2);

	pci_read_config_word(pdev, PCI_COMMAND,		&command);
	pci_read_config_byte(pdev, PCI_CLASS_REVISION,	&revision);
	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE,	&cache_line_size);
	pci_read_config_byte(pdev, PCI_LATENCY_TIMER,	&latency_timer);
	pci_read_config_word(pdev, PCI_STATUS,		&status_reg);

#ifdef SCSI_NCR_PQS_PDS_SUPPORT
	/*
	**    Match the BUS number for PQS/PDS devices.
	**    Read the SCSI ID from a special register mapped
	**    into the configuration space of the individual
	**    875s.  This register is set up by the PQS bios
	*/
	for(i = 0; i < SCSI_NCR_MAX_PQS_BUS && pqs_bus[i] != -1; i++) {
		u_char tmp;
		if (pqs_bus[i] == PciBusNumber(pdev)) {
			pci_read_config_byte(pdev, 0x84, &tmp);
			device->pqs_pds = 1;
			device->host_id = tmp;
			break;
		}
	}
#endif /* SCSI_NCR_PQS_PDS_SUPPORT */

	/*
	**	If user excludes this chip, donnot initialize it.
	*/
	for (i = 0 ; i < SCSI_NCR_MAX_EXCLUDES ; i++) {
		if (driver_setup.excludes[i] ==
				(io_port & PCI_BASE_ADDRESS_IO_MASK))
			return -1;
	}
	/*
	**    Check if the chip is supported
	*/
	chip = 0;
	for (i = 0; i < sizeof(ncr_chip_table)/sizeof(ncr_chip_table[0]); i++) {
		if (device_id != ncr_chip_table[i].device_id)
			continue;
		if (revision > ncr_chip_table[i].revision_id)
			continue;
		if (!(ncr_chip_table[i].features & FE_LDSTR))
			break;
		chip = &device->chip;
		memcpy(chip, &ncr_chip_table[i], sizeof(*chip));
		chip->revision_id = revision;
		break;
	}

#ifdef SCSI_NCR_DYNAMIC_DMA_MAPPING
	/* Configure DMA attributes.  For DAC capable boards, we can encode
	** 32+8 bits for SCSI DMA data addresses with the extra bits used
	** in the size field.  We use normal 32-bit PCI addresses for
	** descriptors.
	*/
	if (chip && (chip->features & FE_DAC)) {
		if (pci_set_dma_mask(pdev, (u64) 0xffffffffff))
			chip->features &= ~FE_DAC_IN_USE;
		else
			chip->features |= FE_DAC_IN_USE;
	}

	if (chip && !(chip->features & FE_DAC_IN_USE)) {
		if (pci_set_dma_mask(pdev, (u64) 0xffffffff)) {
			printk(KERN_WARNING NAME53C8XX
			       "32 BIT PCI BUS DMA ADDRESSING NOT SUPPORTED\n");
			return -1;
		}
	}
#endif

	/*
	**	Ignore Symbios chips controlled by SISL RAID controller.
	**	This controller sets value 0x52414944 at RAM end - 16.
	*/
#if defined(__i386__) && !defined(SCSI_NCR_PCI_MEM_NOT_SUPPORTED)
	if (chip && (base_2_c & PCI_BASE_ADDRESS_MEM_MASK)) {
		unsigned int ram_size, ram_val;
		u_long ram_ptr;

		if (chip->features & FE_RAM8K)
			ram_size = 8192;
		else
			ram_size = 4096;

		ram_ptr = remap_pci_mem(base_2_c & PCI_BASE_ADDRESS_MEM_MASK,
					ram_size);
		if (ram_ptr) {
			ram_val = readl_raw(ram_ptr + ram_size - 16);
			unmap_pci_mem(ram_ptr, ram_size);
			if (ram_val == 0x52414944) {
				printk(NAME53C8XX": not initializing, "
				       "driven by SISL RAID controller.\n");
				return -1;
			}
		}
	}
#endif /* i386 and PCI MEMORY accessible */

	if (!chip) {
		printk(NAME53C8XX ": not initializing, device not supported\n");
		return -1;
	}

#ifdef __powerpc__
	/*
	**	Fix-up for power/pc.
	**	Should not be performed by the driver.
	*/
	if ((command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY))
		    != (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) {
		printk(NAME53C8XX ": setting%s%s...\n",
		(command & PCI_COMMAND_IO)     ? "" : " PCI_COMMAND_IO",
		(command & PCI_COMMAND_MEMORY) ? "" : " PCI_COMMAND_MEMORY");
		command |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
		pci_write_config_word(pdev, PCI_COMMAND, command);
	}

#if LINUX_VERSION_CODE < LinuxVersionCode(2,2,0)
	if ( is_prep ) {
		if (io_port >= 0x10000000) {
			printk(NAME53C8XX ": reallocating io_port (Wacky IBM)");
			io_port = (io_port & 0x00FFFFFF) | 0x01000000;
			pci_write_config_dword(pdev,
					       PCI_BASE_ADDRESS_0, io_port);
		}
		if (base >= 0x10000000) {
			printk(NAME53C8XX ": reallocating base (Wacky IBM)");
			base = (base & 0x00FFFFFF) | 0x01000000;
			pci_write_config_dword(pdev,
					       PCI_BASE_ADDRESS_1, base);
		}
		if (base_2 >= 0x10000000) {
			printk(NAME53C8XX ": reallocating base2 (Wacky IBM)");
			base_2 = (base_2 & 0x00FFFFFF) | 0x01000000;
			pci_write_config_dword(pdev,
					       PCI_BASE_ADDRESS_2, base_2);
		}
	}
#endif
#endif	/* __powerpc__ */

#if defined(__i386__) && !defined(MODULE)
	if (!cache_line_size) {
#if LINUX_VERSION_CODE < LinuxVersionCode(2,1,75)
		extern char x86;
		switch(x86) {
#else
		switch(boot_cpu_data.x86) {
#endif
		case 4:	suggested_cache_line_size = 4; break;
		case 6:
		case 5:	suggested_cache_line_size = 8; break;
		}
	}
#endif	/* __i386__ */

	/*
	**    Check availability of IO space, memory space.
	**    Enable master capability if not yet.
	**
	**    We shouldn't have to care about the IO region when 
	**    we are using MMIO. But calling check_region() from 
	**    both the ncr53c8xx and the sym53c8xx drivers prevents 
	**    from attaching devices from the both drivers.
	**    If you have a better idea, let me know.
	*/
/* #ifdef SCSI_NCR_IOMAPPED */
#if 1
	if (!(command & PCI_COMMAND_IO)) { 
		printk(NAME53C8XX ": I/O base address (0x%lx) disabled.\n",
			(long) io_port);
		io_port = 0;
	}
#endif
	if (!(command & PCI_COMMAND_MEMORY)) {
		printk(NAME53C8XX ": PCI_COMMAND_MEMORY not set.\n");
		base	= 0;
		base_2	= 0;
	}
	io_port &= PCI_BASE_ADDRESS_IO_MASK;
	base	&= PCI_BASE_ADDRESS_MEM_MASK;
	base_2	&= PCI_BASE_ADDRESS_MEM_MASK;

/* #ifdef SCSI_NCR_IOMAPPED */
#if 1
	if (io_port && check_region (io_port, 128)) {
		printk(NAME53C8XX ": IO region 0x%lx[0..127] is in use\n",
			(long) io_port);
		io_port = 0;
	}
	if (!io_port)
		return -1;
#endif
#ifndef SCSI_NCR_IOMAPPED
	if (!base) {
		printk(NAME53C8XX ": MMIO base address disabled.\n");
		return -1;
	}
#endif

	/*
	**    Set MASTER capable and PARITY bit, if not yet.
	*/
	if ((command & (PCI_COMMAND_MASTER | PCI_COMMAND_PARITY))
		     != (PCI_COMMAND_MASTER | PCI_COMMAND_PARITY)) {
		printk(NAME53C8XX ": setting%s%s...(fix-up)\n",
		(command & PCI_COMMAND_MASTER) ? "" : " PCI_COMMAND_MASTER",
		(command & PCI_COMMAND_PARITY) ? "" : " PCI_COMMAND_PARITY");
		command |= (PCI_COMMAND_MASTER | PCI_COMMAND_PARITY);
		pci_write_config_word(pdev, PCI_COMMAND, command);
	}

	/*
	**    Fix some features according to driver setup.
	*/
	if (!(driver_setup.special_features & 1))
		chip->features &= ~FE_SPECIAL_SET;
	else {
		if (driver_setup.special_features & 2)
			chip->features &= ~FE_WRIE;
		if (driver_setup.special_features & 4)
			chip->features &= ~FE_NOPM;
	}

	/*
	** Work around for errant bit in 895A. The 66Mhz
	** capable bit is set erroneously. Clear this bit.
	** (Item 1 DEL 533)
	**
	** Make sure Config space and Features agree.
	**
	** Recall: writes are not normal to status register -
	** write a 1 to clear and a 0 to leave unchanged.
	** Can only reset bits.
	*/
	if (chip->features & FE_66MHZ) {
		if (!(status_reg & PCI_STATUS_66MHZ))
			chip->features &= ~FE_66MHZ;
	}
	else {
		if (status_reg & PCI_STATUS_66MHZ) {
			status_reg = PCI_STATUS_66MHZ;
			pci_write_config_word(pdev, PCI_STATUS, status_reg);
			pci_read_config_word(pdev, PCI_STATUS, &status_reg);
		}
	}

	/*
	**	Some features are required to be enabled in order to 
	**	work around some chip problems. :) ;)
	**	(ITEM 12 of a DEL about the 896 I haven't yet).
	**	We must ensure the chip will use WRITE AND INVALIDATE.
	**	The revision number limit is for now arbitrary.
	*/
	if (device_id == PCI_DEVICE_ID_NCR_53C896 && revision <= 0x10) {
		chip->features	|= (FE_WRIE | FE_CLSE);
		pci_fix_up	|=  3;	/* Force appropriate PCI fix-up */
	}

#ifdef	SCSI_NCR_PCI_FIX_UP_SUPPORT
	/*
	**    Try to fix up PCI config according to wished features.
	*/
	if ((pci_fix_up & 1) && (chip->features & FE_CLSE) && 
	    !cache_line_size && suggested_cache_line_size) {
		cache_line_size = suggested_cache_line_size;
		pci_write_config_byte(pdev,
				      PCI_CACHE_LINE_SIZE, cache_line_size);
		printk(NAME53C8XX ": PCI_CACHE_LINE_SIZE set to %d (fix-up).\n",
			cache_line_size);
	}

	if ((pci_fix_up & 2) && cache_line_size &&
	    (chip->features & FE_WRIE) && !(command & PCI_COMMAND_INVALIDATE)) {
		printk(NAME53C8XX": setting PCI_COMMAND_INVALIDATE (fix-up)\n");
		command |= PCI_COMMAND_INVALIDATE;
		pci_write_config_word(pdev, PCI_COMMAND, command);
	}

	/*
	**    Tune PCI LATENCY TIMER according to burst max length transfer.
	**    (latency timer >= burst length + 6, we add 10 to be quite sure)
	*/

	if (chip->burst_max && (latency_timer == 0 || (pci_fix_up & 4))) {
		uchar lt = (1 << chip->burst_max) + 6 + 10;
		if (latency_timer < lt) {
			printk(NAME53C8XX 
			       ": changing PCI_LATENCY_TIMER from %d to %d.\n",
			       (int) latency_timer, (int) lt);
			latency_timer = lt;
			pci_write_config_byte(pdev,
					      PCI_LATENCY_TIMER, latency_timer);
		}
	}

#endif	/* SCSI_NCR_PCI_FIX_UP_SUPPORT */

 	/*
	**    Initialise ncr_device structure with items required by ncr_attach.
	*/
	device->pdev		= pdev;
	device->slot.bus	= PciBusNumber(pdev);
	device->slot.device_fn	= PciDeviceFn(pdev);
	device->slot.base	= base;
	device->slot.base_2	= base_2;
	device->slot.base_c	= base_c;
	device->slot.base_2_c	= base_2_c;
	device->slot.io_port	= io_port;
	device->slot.irq	= irq;
	device->attach_done	= 0;

	return 0;
}


/*===================================================================
**    Detect and try to read SYMBIOS and TEKRAM NVRAM.
**
**    Data can be used to order booting of boards.
**
**    Data is saved in ncr_device structure if NVRAM found. This
**    is then used to find drive boot order for ncr_attach().
**
**    NVRAM data is passed to Scsi_Host_Template later during 
**    ncr_attach() for any device set up.
*===================================================================
*/
#ifdef SCSI_NCR_NVRAM_SUPPORT
static void __init ncr_get_nvram(ncr_device *devp, ncr_nvram *nvp)
{
	devp->nvram = nvp;
	if (!nvp)
		return;
	/*
	**    Get access to chip IO registers
	*/
#ifdef SCSI_NCR_IOMAPPED
	request_region(devp->slot.io_port, 128, NAME53C8XX);
	devp->slot.base_io = devp->slot.io_port;
#else
	devp->slot.reg = 
		(struct ncr_reg *) remap_pci_mem(devp->slot.base_c, 128);
	if (!devp->slot.reg)
		return;
#endif

	/*
	**    Try to read SYMBIOS nvram.
	**    Try to read TEKRAM nvram if Symbios nvram not found.
	*/
	if	(!sym_read_Symbios_nvram(&devp->slot, &nvp->data.Symbios))
		nvp->type = SCSI_NCR_SYMBIOS_NVRAM;
	else if	(!sym_read_Tekram_nvram(&devp->slot, devp->chip.device_id,
					&nvp->data.Tekram))
		nvp->type = SCSI_NCR_TEKRAM_NVRAM;
	else {
		nvp->type = 0;
		devp->nvram = 0;
	}

	/*
	** Release access to chip IO registers
	*/
#ifdef SCSI_NCR_IOMAPPED
	release_region(devp->slot.base_io, 128);
#else
	unmap_pci_mem((u_long) devp->slot.reg, 128ul);
#endif

}
#endif	/* SCSI_NCR_NVRAM_SUPPORT */

/*
**   Linux select queue depths function
*/

#define DEF_DEPTH	(driver_setup.default_tags)
#define ALL_TARGETS	-2
#define NO_TARGET	-1
#define ALL_LUNS	-2
#define NO_LUN		-1

static int device_queue_depth(ncb_p np, int target, int lun)
{
	int c, h, t, u, v;
	char *p = driver_setup.tag_ctrl;
	char *ep;

	h = -1;
	t = NO_TARGET;
	u = NO_LUN;
	while ((c = *p++) != 0) {
		v = simple_strtoul(p, &ep, 0);
		switch(c) {
		case '/':
			++h;
			t = ALL_TARGETS;
			u = ALL_LUNS;
			break;
		case 't':
			if (t != target)
				t = (target == v) ? v : NO_TARGET;
			u = ALL_LUNS;
			break;
		case 'u':
			if (u != lun)
				u = (lun == v) ? v : NO_LUN;
			break;
		case 'q':
			if (h == np->unit &&
				(t == ALL_TARGETS || t == target) &&
				(u == ALL_LUNS    || u == lun))
				return v;
			break;
		case '-':
			t = ALL_TARGETS;
			u = ALL_LUNS;
			break;
		default:
			break;
		}
		p = ep;
	}
	return DEF_DEPTH;
}

static void sym53c8xx_select_queue_depths(struct Scsi_Host *host, struct scsi_device *devlist)
{
	struct scsi_device *device;

	for (device = devlist; device; device = device->next) {
		ncb_p np;
		tcb_p tp;
		lcb_p lp;
		int numtags;

		if (device->host != host)
			continue;

		np = ((struct host_data *) host->hostdata)->ncb;
		tp = &np->target[device->id];
		lp = ncr_lp(np, tp, device->lun);

		/*
		**	Select queue depth from driver setup.
		**	Donnot use more than configured by user.
		**	Use at least 2.
		**	Donnot use more than our maximum.
		*/
		numtags = device_queue_depth(np, device->id, device->lun);
		if (numtags > tp->usrtags)
			numtags = tp->usrtags;
		if (!device->tagged_supported)
			numtags = 1;
		device->queue_depth = numtags;
		if (device->queue_depth < 2)
			device->queue_depth = 2;
		if (device->queue_depth > MAX_TAGS)
			device->queue_depth = MAX_TAGS;

		/*
		**	Since the queue depth is not tunable under Linux,
		**	we need to know this value in order not to 
		**	announce stupid things to user.
		*/
		if (lp) {
			lp->numtags = lp->maxtags = numtags;
			lp->scdev_depth = device->queue_depth;
		}
		ncr_setup_tags (np, device->id, device->lun);

#ifdef DEBUG_SYM53C8XX
printk("sym53c8xx_select_queue_depth: host=%d, id=%d, lun=%d, depth=%d\n",
	np->unit, device->id, device->lun, device->queue_depth);
#endif
	}
}

/*
**   Linux entry point for info() function
*/
const char *sym53c8xx_info (struct Scsi_Host *host)
{
	return SCSI_NCR_DRIVER_NAME;
}

/*
**   Linux entry point of queuecommand() function
*/

int sym53c8xx_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *))
{
     ncb_p np = ((struct host_data *) cmd->host->hostdata)->ncb;
     unsigned long flags;
     int sts;

#ifdef DEBUG_SYM53C8XX
printk("sym53c8xx_queue_command\n");
#endif

     cmd->scsi_done     = done;
     cmd->host_scribble = NULL;
     cmd->SCp.ptr       = NULL;
     cmd->SCp.buffer    = NULL;
#ifdef SCSI_NCR_DYNAMIC_DMA_MAPPING
     __data_mapped(cmd) = 0;
     __data_mapping(cmd) = 0;
#endif

     NCR_LOCK_NCB(np, flags);

     if ((sts = ncr_queue_command(np, cmd)) != DID_OK) {
	  SetScsiResult(cmd, sts, 0);
#ifdef DEBUG_SYM53C8XX
printk("sym53c8xx : command not queued - result=%d\n", sts);
#endif
     }
#ifdef DEBUG_SYM53C8XX
     else
printk("sym53c8xx : command successfully queued\n");
#endif

     NCR_UNLOCK_NCB(np, flags);

     if (sts != DID_OK) {
          unmap_scsi_data(np, cmd);
          done(cmd);
     }

     return sts;
}

/*
**   Linux entry point of the interrupt handler.
**   Since linux versions > 1.3.70, we trust the kernel for 
**   passing the internal host descriptor as 'dev_id'.
**   Otherwise, we scan the host list and call the interrupt 
**   routine for each host that uses this IRQ.
*/

static void sym53c8xx_intr(int irq, void *dev_id, struct pt_regs * regs)
{
     unsigned long flags;
     ncb_p np = (ncb_p) dev_id;
     Scsi_Cmnd *done_list;

#ifdef DEBUG_SYM53C8XX
     printk("sym53c8xx : interrupt received\n");
#endif

     if (DEBUG_FLAGS & DEBUG_TINY) printk ("[");

     NCR_LOCK_NCB(np, flags);
     ncr_exception(np);
     done_list     = np->done_list;
     np->done_list = 0;
     NCR_UNLOCK_NCB(np, flags);

     if (DEBUG_FLAGS & DEBUG_TINY) printk ("]\n");

     if (done_list) {
          NCR_LOCK_SCSI_DONE(np, flags);
          ncr_flush_done_cmds(done_list);
          NCR_UNLOCK_SCSI_DONE(np, flags);
     }
}

/*
**   Linux entry point of the timer handler
*/

static void sym53c8xx_timeout(unsigned long npref)
{
     ncb_p np = (ncb_p) npref;
     unsigned long flags;
     Scsi_Cmnd *done_list;

     NCR_LOCK_NCB(np, flags);
     ncr_timeout((ncb_p) np);
     done_list     = np->done_list;
     np->done_list = 0;
     NCR_UNLOCK_NCB(np, flags);

     if (done_list) {
          NCR_LOCK_SCSI_DONE(np, flags);
          ncr_flush_done_cmds(done_list);
          NCR_UNLOCK_SCSI_DONE(np, flags);
     }
}

/*
**   Linux entry point of reset() function
*/

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
int sym53c8xx_reset(Scsi_Cmnd *cmd, unsigned int reset_flags)
#else
int sym53c8xx_reset(Scsi_Cmnd *cmd)
#endif
{
	ncb_p np = ((struct host_data *) cmd->host->hostdata)->ncb;
	int sts;
	unsigned long flags;
	Scsi_Cmnd *done_list;

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	printk("sym53c8xx_reset: pid=%lu reset_flags=%x serial_number=%ld serial_number_at_timeout=%ld\n",
		cmd->pid, reset_flags, cmd->serial_number, cmd->serial_number_at_timeout);
#else
	printk("sym53c8xx_reset: command pid %lu\n", cmd->pid);
#endif

	NCR_LOCK_NCB(np, flags);

	/*
	 * We have to just ignore reset requests in some situations.
	 */
#if defined SCSI_RESET_NOT_RUNNING
	if (cmd->serial_number != cmd->serial_number_at_timeout) {
		sts = SCSI_RESET_NOT_RUNNING;
		goto out;
	}
#endif
	/*
	 * If the mid-level driver told us reset is synchronous, it seems 
	 * that we must call the done() callback for the involved command, 
	 * even if this command was not queued to the low-level driver, 
	 * before returning SCSI_RESET_SUCCESS.
	 */

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	sts = ncr_reset_bus(np, cmd,
	(reset_flags & (SCSI_RESET_SYNCHRONOUS | SCSI_RESET_ASYNCHRONOUS)) == SCSI_RESET_SYNCHRONOUS);
#else
	sts = ncr_reset_bus(np, cmd, 0);
#endif

	/*
	 * Since we always reset the controller, when we return success, 
	 * we add this information to the return code.
	 */
#if defined SCSI_RESET_HOST_RESET
	if (sts == SCSI_RESET_SUCCESS)
		sts |= SCSI_RESET_HOST_RESET;
#endif

out:
	done_list     = np->done_list;
	np->done_list = 0;
	NCR_UNLOCK_NCB(np, flags);

	ncr_flush_done_cmds(done_list);

	return sts;
}

/*
**   Linux entry point of abort() function
*/

int sym53c8xx_abort(Scsi_Cmnd *cmd)
{
	ncb_p np = ((struct host_data *) cmd->host->hostdata)->ncb;
	int sts;
	unsigned long flags;
	Scsi_Cmnd *done_list;

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	printk("sym53c8xx_abort: pid=%lu serial_number=%ld serial_number_at_timeout=%ld\n",
		cmd->pid, cmd->serial_number, cmd->serial_number_at_timeout);
#else
	printk("sym53c8xx_abort: command pid %lu\n", cmd->pid);
#endif

	NCR_LOCK_NCB(np, flags);

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	/*
	 * We have to just ignore abort requests in some situations.
	 */
	if (cmd->serial_number != cmd->serial_number_at_timeout) {
		sts = SCSI_ABORT_NOT_RUNNING;
		goto out;
	}
#endif

	sts = ncr_abort_command(np, cmd);
out:
	done_list     = np->done_list;
	np->done_list = 0;
	NCR_UNLOCK_NCB(np, flags);

	ncr_flush_done_cmds(done_list);

	return sts;
}


#ifdef MODULE
int sym53c8xx_release(struct Scsi_Host *host)
{
#ifdef DEBUG_SYM53C8XX
printk("sym53c8xx : release\n");
#endif
     ncr_detach(((struct host_data *) host->hostdata)->ncb);

     return 1;
}
#endif


/*
**	Scsi command waiting list management.
**
**	It may happen that we cannot insert a scsi command into the start queue,
**	in the following circumstances.
** 		Too few preallocated ccb(s), 
**		maxtags < cmd_per_lun of the Linux host control block,
**		etc...
**	Such scsi commands are inserted into a waiting list.
**	When a scsi command complete, we try to requeue the commands of the
**	waiting list.
*/

#define next_wcmd host_scribble

static void insert_into_waiting_list(ncb_p np, Scsi_Cmnd *cmd)
{
	Scsi_Cmnd *wcmd;

#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx inserted into waiting list\n", ncr_name(np), (u_long) cmd);
#endif
	cmd->next_wcmd = 0;
	if (!(wcmd = np->waiting_list)) np->waiting_list = cmd;
	else {
		while ((wcmd->next_wcmd) != 0)
			wcmd = (Scsi_Cmnd *) wcmd->next_wcmd;
		wcmd->next_wcmd = (char *) cmd;
	}
}

static Scsi_Cmnd *retrieve_from_waiting_list(int to_remove, ncb_p np, Scsi_Cmnd *cmd)
{
	Scsi_Cmnd **pcmd = &np->waiting_list;

	while (*pcmd) {
		if (cmd == *pcmd) {
			if (to_remove) {
				*pcmd = (Scsi_Cmnd *) cmd->next_wcmd;
				cmd->next_wcmd = 0;
			}
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx retrieved from waiting list\n", ncr_name(np), (u_long) cmd);
#endif
			return cmd;
		}
		pcmd = (Scsi_Cmnd **) &(*pcmd)->next_wcmd;
	}
	return 0;
}

static void process_waiting_list(ncb_p np, int sts)
{
	Scsi_Cmnd *waiting_list, *wcmd;

	waiting_list = np->waiting_list;
	np->waiting_list = 0;

#ifdef DEBUG_WAITING_LIST
	if (waiting_list) printk("%s: waiting_list=%lx processing sts=%d\n", ncr_name(np), (u_long) waiting_list, sts);
#endif
	while ((wcmd = waiting_list) != 0) {
		waiting_list = (Scsi_Cmnd *) wcmd->next_wcmd;
		wcmd->next_wcmd = 0;
		if (sts == DID_OK) {
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx trying to requeue\n", ncr_name(np), (u_long) wcmd);
#endif
			sts = ncr_queue_command(np, wcmd);
		}
		if (sts != DID_OK) {
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx done forced sts=%d\n", ncr_name(np), (u_long) wcmd, sts);
#endif
			SetScsiResult(wcmd, sts, 0);
			ncr_queue_done_cmd(np, wcmd);
		}
	}
}

#undef next_wcmd

#ifdef SCSI_NCR_PROC_INFO_SUPPORT

/*=========================================================================
**	Proc file system stuff
**
**	A read operation returns adapter information.
**	A write operation is a control command.
**	The string is parsed in the driver code and the command is passed 
**	to the ncr_usercmd() function.
**=========================================================================
*/

#ifdef SCSI_NCR_USER_COMMAND_SUPPORT

#define is_digit(c)	((c) >= '0' && (c) <= '9')
#define digit_to_bin(c)	((c) - '0')
#define is_space(c)	((c) == ' ' || (c) == '\t')

static int skip_spaces(char *ptr, int len)
{
	int cnt, c;

	for (cnt = len; cnt > 0 && (c = *ptr++) && is_space(c); cnt--);

	return (len - cnt);
}

static int get_int_arg(char *ptr, int len, u_long *pv)
{
	int	cnt, c;
	u_long	v;

	for (v = 0, cnt = len; cnt > 0 && (c = *ptr++) && is_digit(c); cnt--) {
		v = (v * 10) + digit_to_bin(c);
	}

	if (pv)
		*pv = v;

	return (len - cnt);
}

static int is_keyword(char *ptr, int len, char *verb)
{
	int verb_len = strlen(verb);

	if (len >= strlen(verb) && !memcmp(verb, ptr, verb_len))
		return verb_len;
	else
		return 0;

}

#define SKIP_SPACES(min_spaces)						\
	if ((arg_len = skip_spaces(ptr, len)) < (min_spaces))		\
		return -EINVAL;						\
	ptr += arg_len; len -= arg_len;

#define GET_INT_ARG(v)							\
	if (!(arg_len = get_int_arg(ptr, len, &(v))))			\
		return -EINVAL;						\
	ptr += arg_len; len -= arg_len;


/*
**	Parse a control command
*/

static int ncr_user_command(ncb_p np, char *buffer, int length)
{
	char *ptr	= buffer;
	int len		= length;
	struct usrcmd	 *uc = &np->user;
	int		arg_len;
	u_long 		target;

	bzero(uc, sizeof(*uc));

	if (len > 0 && ptr[len-1] == '\n')
		--len;

	if	((arg_len = is_keyword(ptr, len, "setsync")) != 0)
		uc->cmd = UC_SETSYNC;
	else if	((arg_len = is_keyword(ptr, len, "settags")) != 0)
		uc->cmd = UC_SETTAGS;
	else if	((arg_len = is_keyword(ptr, len, "setorder")) != 0)
		uc->cmd = UC_SETORDER;
	else if	((arg_len = is_keyword(ptr, len, "setverbose")) != 0)
		uc->cmd = UC_SETVERBOSE;
	else if	((arg_len = is_keyword(ptr, len, "setwide")) != 0)
		uc->cmd = UC_SETWIDE;
	else if	((arg_len = is_keyword(ptr, len, "setdebug")) != 0)
		uc->cmd = UC_SETDEBUG;
	else if	((arg_len = is_keyword(ptr, len, "setflag")) != 0)
		uc->cmd = UC_SETFLAG;
	else if	((arg_len = is_keyword(ptr, len, "resetdev")) != 0)
		uc->cmd = UC_RESETDEV;
	else if	((arg_len = is_keyword(ptr, len, "cleardev")) != 0)
		uc->cmd = UC_CLEARDEV;
	else
		arg_len = 0;

#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: arg_len=%d, cmd=%ld\n", arg_len, uc->cmd);
#endif

	if (!arg_len)
		return -EINVAL;
	ptr += arg_len; len -= arg_len;

	switch(uc->cmd) {
	case UC_SETSYNC:
	case UC_SETTAGS:
	case UC_SETWIDE:
	case UC_SETFLAG:
	case UC_RESETDEV:
	case UC_CLEARDEV:
		SKIP_SPACES(1);
		if ((arg_len = is_keyword(ptr, len, "all")) != 0) {
			ptr += arg_len; len -= arg_len;
			uc->target = ~0;
		} else {
			GET_INT_ARG(target);
			uc->target = (1<<target);
#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: target=%ld\n", target);
#endif
		}
		break;
	}

	switch(uc->cmd) {
	case UC_SETVERBOSE:
	case UC_SETSYNC:
	case UC_SETTAGS:
	case UC_SETWIDE:
		SKIP_SPACES(1);
		GET_INT_ARG(uc->data);
#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: data=%ld\n", uc->data);
#endif
		break;
	case UC_SETORDER:
		SKIP_SPACES(1);
		if	((arg_len = is_keyword(ptr, len, "simple")))
			uc->data = M_SIMPLE_TAG;
		else if	((arg_len = is_keyword(ptr, len, "ordered")))
			uc->data = M_ORDERED_TAG;
		else if	((arg_len = is_keyword(ptr, len, "default")))
			uc->data = 0;
		else
			return -EINVAL;
		break;
	case UC_SETDEBUG:
		while (len > 0) {
			SKIP_SPACES(1);
			if	((arg_len = is_keyword(ptr, len, "alloc")))
				uc->data |= DEBUG_ALLOC;
			else if	((arg_len = is_keyword(ptr, len, "phase")))
				uc->data |= DEBUG_PHASE;
			else if	((arg_len = is_keyword(ptr, len, "queue")))
				uc->data |= DEBUG_QUEUE;
			else if	((arg_len = is_keyword(ptr, len, "result")))
				uc->data |= DEBUG_RESULT;
			else if	((arg_len = is_keyword(ptr, len, "pointer")))
				uc->data |= DEBUG_POINTER;
			else if	((arg_len = is_keyword(ptr, len, "script")))
				uc->data |= DEBUG_SCRIPT;
			else if	((arg_len = is_keyword(ptr, len, "tiny")))
				uc->data |= DEBUG_TINY;
			else if	((arg_len = is_keyword(ptr, len, "timing")))
				uc->data |= DEBUG_TIMING;
			else if	((arg_len = is_keyword(ptr, len, "nego")))
				uc->data |= DEBUG_NEGO;
			else if	((arg_len = is_keyword(ptr, len, "tags")))
				uc->data |= DEBUG_TAGS;
			else
				return -EINVAL;
			ptr += arg_len; len -= arg_len;
		}
#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: data=%ld\n", uc->data);
#endif
		break;
	case UC_SETFLAG:
		while (len > 0) {
			SKIP_SPACES(1);
			if	((arg_len = is_keyword(ptr, len, "trace")))
				uc->data |= UF_TRACE;
			else if	((arg_len = is_keyword(ptr, len, "no_disc")))
				uc->data |= UF_NODISC;
			else
				return -EINVAL;
			ptr += arg_len; len -= arg_len;
		}
		break;
	default:
		break;
	}

	if (len)
		return -EINVAL;
	else {
		unsigned long flags;

		NCR_LOCK_NCB(np, flags);
		ncr_usercmd (np);
		NCR_UNLOCK_NCB(np, flags);
	}
	return length;
}

#endif	/* SCSI_NCR_USER_COMMAND_SUPPORT */

#ifdef SCSI_NCR_USER_INFO_SUPPORT

struct info_str
{
	char *buffer;
	int length;
	int offset;
	int pos;
};

static void copy_mem_info(struct info_str *info, char *data, int len)
{
	if (info->pos + len > info->length)
		len = info->length - info->pos;

	if (info->pos + len < info->offset) {
		info->pos += len;
		return;
	}
	if (info->pos < info->offset) {
		data += (info->offset - info->pos);
		len  -= (info->offset - info->pos);
	}

	if (len > 0) {
		memcpy(info->buffer + info->pos, data, len);
		info->pos += len;
	}
}

static int copy_info(struct info_str *info, char *fmt, ...)
{
	va_list args;
	char buf[81];
	int len;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	copy_mem_info(info, buf, len);
	return len;
}

/*
**	Copy formatted information into the input buffer.
*/

static int ncr_host_info(ncb_p np, char *ptr, off_t offset, int len)
{
	struct info_str info;

	info.buffer	= ptr;
	info.length	= len;
	info.offset	= offset;
	info.pos	= 0;

	copy_info(&info, "General information:\n");
	copy_info(&info, "  Chip " NAME53C "%s, device id 0x%x, "
			 "revision id 0x%x\n",
			 np->chip_name, np->device_id,	np->revision_id);
	copy_info(&info, "  On PCI bus %d, device %d, function %d, "
#ifdef __sparc__
		"IRQ %s\n",
#else
		"IRQ %d\n",
#endif
		np->bus, (np->device_fn & 0xf8) >> 3, np->device_fn & 7,
#ifdef __sparc__
		__irq_itoa(np->irq));
#else
		(int) np->irq);
#endif
	copy_info(&info, "  Synchronous period factor %d, "
			 "max commands per lun %d\n",
			 (int) np->minsync, MAX_TAGS);

	if (driver_setup.debug || driver_setup.verbose > 1) {
		copy_info(&info, "  Debug flags 0x%x, verbosity level %d\n",
			  driver_setup.debug, driver_setup.verbose);
	}

	return info.pos > info.offset? info.pos - info.offset : 0;
}

#endif /* SCSI_NCR_USER_INFO_SUPPORT */

/*
**	Entry point of the scsi proc fs of the driver.
**	- func = 0 means read  (returns adapter infos)
**	- func = 1 means write (parse user control command)
*/

static int sym53c8xx_proc_info(char *buffer, char **start, off_t offset,
			int length, int hostno, int func)
{
	struct Scsi_Host *host;
	struct host_data *host_data;
	ncb_p ncb = 0;
	int retv;

#ifdef DEBUG_PROC_INFO
printk("sym53c8xx_proc_info: hostno=%d, func=%d\n", hostno, func);
#endif

	for (host = first_host; host; host = host->next) {
		if (host->hostt != first_host->hostt)
			continue;
		if (host->host_no == hostno) {
			host_data = (struct host_data *) host->hostdata;
			ncb = host_data->ncb;
			break;
		}
	}

	if (!ncb)
		return -EINVAL;

	if (func) {
#ifdef	SCSI_NCR_USER_COMMAND_SUPPORT
		retv = ncr_user_command(ncb, buffer, length);
#else
		retv = -EINVAL;
#endif
	}
	else {
		if (start)
			*start = buffer;
#ifdef SCSI_NCR_USER_INFO_SUPPORT
		retv = ncr_host_info(ncb, buffer, offset, length);
#else
		retv = -EINVAL;
#endif
	}

	return retv;
}


/*=========================================================================
**	End of proc file system stuff
**=========================================================================
*/
#endif


#ifdef SCSI_NCR_NVRAM_SUPPORT

/*
 *  24C16 EEPROM reading.
 *
 *  GPOI0 - data in/data out
 *  GPIO1 - clock
 *  Symbios NVRAM wiring now also used by Tekram.
 */

#define SET_BIT 0
#define CLR_BIT 1
#define SET_CLK 2
#define CLR_CLK 3

/*
 *  Set/clear data/clock bit in GPIO0
 */
static void __init
S24C16_set_bit(ncr_slot *np, u_char write_bit, u_char *gpreg, int bit_mode)
{
	UDELAY (5);
	switch (bit_mode){
	case SET_BIT:
		*gpreg |= write_bit;
		break;
	case CLR_BIT:
		*gpreg &= 0xfe;
		break;
	case SET_CLK:
		*gpreg |= 0x02;
		break;
	case CLR_CLK:
		*gpreg &= 0xfd;
		break;

	}
	OUTB (nc_gpreg, *gpreg);
	UDELAY (5);
}

/*
 *  Send START condition to NVRAM to wake it up.
 */
static void __init S24C16_start(ncr_slot *np, u_char *gpreg)
{
	S24C16_set_bit(np, 1, gpreg, SET_BIT);
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	S24C16_set_bit(np, 0, gpreg, CLR_BIT);
	S24C16_set_bit(np, 0, gpreg, CLR_CLK);
}

/*
 *  Send STOP condition to NVRAM - puts NVRAM to sleep... ZZzzzz!!
 */
static void __init S24C16_stop(ncr_slot *np, u_char *gpreg)
{
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	S24C16_set_bit(np, 1, gpreg, SET_BIT);
}

/*
 *  Read or write a bit to the NVRAM,
 *  read if GPIO0 input else write if GPIO0 output
 */
static void __init 
S24C16_do_bit(ncr_slot *np, u_char *read_bit, u_char write_bit, u_char *gpreg)
{
	S24C16_set_bit(np, write_bit, gpreg, SET_BIT);
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	if (read_bit)
		*read_bit = INB (nc_gpreg);
	S24C16_set_bit(np, 0, gpreg, CLR_CLK);
	S24C16_set_bit(np, 0, gpreg, CLR_BIT);
}

/*
 *  Output an ACK to the NVRAM after reading,
 *  change GPIO0 to output and when done back to an input
 */
static void __init
S24C16_write_ack(ncr_slot *np, u_char write_bit, u_char *gpreg, u_char *gpcntl)
{
	OUTB (nc_gpcntl, *gpcntl & 0xfe);
	S24C16_do_bit(np, 0, write_bit, gpreg);
	OUTB (nc_gpcntl, *gpcntl);
}

/*
 *  Input an ACK from NVRAM after writing,
 *  change GPIO0 to input and when done back to an output
 */
static void __init 
S24C16_read_ack(ncr_slot *np, u_char *read_bit, u_char *gpreg, u_char *gpcntl)
{
	OUTB (nc_gpcntl, *gpcntl | 0x01);
	S24C16_do_bit(np, read_bit, 1, gpreg);
	OUTB (nc_gpcntl, *gpcntl);
}

/*
 *  WRITE a byte to the NVRAM and then get an ACK to see it was accepted OK,
 *  GPIO0 must already be set as an output
 */
static void __init 
S24C16_write_byte(ncr_slot *np, u_char *ack_data, u_char write_data, 
		  u_char *gpreg, u_char *gpcntl)
{
	int x;
	
	for (x = 0; x < 8; x++)
		S24C16_do_bit(np, 0, (write_data >> (7 - x)) & 0x01, gpreg);
		
	S24C16_read_ack(np, ack_data, gpreg, gpcntl);
}

/*
 *  READ a byte from the NVRAM and then send an ACK to say we have got it,
 *  GPIO0 must already be set as an input
 */
static void __init 
S24C16_read_byte(ncr_slot *np, u_char *read_data, u_char ack_data, 
	         u_char *gpreg, u_char *gpcntl)
{
	int x;
	u_char read_bit;

	*read_data = 0;
	for (x = 0; x < 8; x++) {
		S24C16_do_bit(np, &read_bit, 1, gpreg);
		*read_data |= ((read_bit & 0x01) << (7 - x));
	}

	S24C16_write_ack(np, ack_data, gpreg, gpcntl);
}

/*
 *  Read 'len' bytes starting at 'offset'.
 */
static int __init 
sym_read_S24C16_nvram (ncr_slot *np, int offset, u_char *data, int len)
{
	u_char	gpcntl, gpreg;
	u_char	old_gpcntl, old_gpreg;
	u_char	ack_data;
	int	retv = 1;
	int	x;

	/* save current state of GPCNTL and GPREG */
	old_gpreg	= INB (nc_gpreg);
	old_gpcntl	= INB (nc_gpcntl);
	gpcntl		= old_gpcntl & 0x1c;

	/* set up GPREG & GPCNTL to set GPIO0 and GPIO1 in to known state */
	OUTB (nc_gpreg,  old_gpreg);
	OUTB (nc_gpcntl, gpcntl);

	/* this is to set NVRAM into a known state with GPIO0/1 both low */
	gpreg = old_gpreg;
	S24C16_set_bit(np, 0, &gpreg, CLR_CLK);
	S24C16_set_bit(np, 0, &gpreg, CLR_BIT);
		
	/* now set NVRAM inactive with GPIO0/1 both high */
	S24C16_stop(np, &gpreg);
	
	/* activate NVRAM */
	S24C16_start(np, &gpreg);

	/* write device code and random address MSB */
	S24C16_write_byte(np, &ack_data,
		0xa0 | ((offset >> 7) & 0x0e), &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* write random address LSB */
	S24C16_write_byte(np, &ack_data,
		offset & 0xff, &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* regenerate START state to set up for reading */
	S24C16_start(np, &gpreg);
	
	/* rewrite device code and address MSB with read bit set (lsb = 0x01) */
	S24C16_write_byte(np, &ack_data,
		0xa1 | ((offset >> 7) & 0x0e), &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* now set up GPIO0 for inputting data */
	gpcntl |= 0x01;
	OUTB (nc_gpcntl, gpcntl);
		
	/* input all requested data - only part of total NVRAM */
	for (x = 0; x < len; x++) 
		S24C16_read_byte(np, &data[x], (x == (len-1)), &gpreg, &gpcntl);

	/* finally put NVRAM back in inactive mode */
	gpcntl &= 0xfe;
	OUTB (nc_gpcntl, gpcntl);
	S24C16_stop(np, &gpreg);
	retv = 0;
out:
	/* return GPIO0/1 to original states after having accessed NVRAM */
	OUTB (nc_gpcntl, old_gpcntl);
	OUTB (nc_gpreg,  old_gpreg);

	return retv;
}

#undef SET_BIT
#undef CLR_BIT
#undef SET_CLK
#undef CLR_CLK

/*
 *  Try reading Symbios NVRAM.
 *  Return 0 if OK.
 */
static int __init sym_read_Symbios_nvram (ncr_slot *np, Symbios_nvram *nvram)
{
	static u_char Symbios_trailer[6] = {0xfe, 0xfe, 0, 0, 0, 0};
	u_char *data = (u_char *) nvram;
	int len  = sizeof(*nvram);
	u_short	csum;
	int x;

	/* probe the 24c16 and read the SYMBIOS 24c16 area */
	if (sym_read_S24C16_nvram (np, SYMBIOS_NVRAM_ADDRESS, data, len))
		return 1;

	/* check valid NVRAM signature, verify byte count and checksum */
	if (nvram->type != 0 ||
	    memcmp(nvram->trailer, Symbios_trailer, 6) ||
	    nvram->byte_count != len - 12)
		return 1;

	/* verify checksum */
	for (x = 6, csum = 0; x < len - 6; x++)
		csum += data[x];
	if (csum != nvram->checksum)
		return 1;

	return 0;
}

/*
 *  93C46 EEPROM reading.
 *
 *  GPOI0 - data in
 *  GPIO1 - data out
 *  GPIO2 - clock
 *  GPIO4 - chip select
 *
 *  Used by Tekram.
 */

/*
 *  Pulse clock bit in GPIO0
 */
static void __init T93C46_Clk(ncr_slot *np, u_char *gpreg)
{
	OUTB (nc_gpreg, *gpreg | 0x04);
	UDELAY (2);
	OUTB (nc_gpreg, *gpreg);
}

/* 
 *  Read bit from NVRAM
 */
static void __init T93C46_Read_Bit(ncr_slot *np, u_char *read_bit, u_char *gpreg)
{
	UDELAY (2);
	T93C46_Clk(np, gpreg);
	*read_bit = INB (nc_gpreg);
}

/*
 *  Write bit to GPIO0
 */
static void __init T93C46_Write_Bit(ncr_slot *np, u_char write_bit, u_char *gpreg)
{
	if (write_bit & 0x01)
		*gpreg |= 0x02;
	else
		*gpreg &= 0xfd;
		
	*gpreg |= 0x10;
		
	OUTB (nc_gpreg, *gpreg);
	UDELAY (2);

	T93C46_Clk(np, gpreg);
}

/*
 *  Send STOP condition to NVRAM - puts NVRAM to sleep... ZZZzzz!!
 */
static void __init T93C46_Stop(ncr_slot *np, u_char *gpreg)
{
	*gpreg &= 0xef;
	OUTB (nc_gpreg, *gpreg);
	UDELAY (2);

	T93C46_Clk(np, gpreg);
}

/*
 *  Send read command and address to NVRAM
 */
static void __init 
T93C46_Send_Command(ncr_slot *np, u_short write_data, 
		    u_char *read_bit, u_char *gpreg)
{
	int x;

	/* send 9 bits, start bit (1), command (2), address (6)  */
	for (x = 0; x < 9; x++)
		T93C46_Write_Bit(np, (u_char) (write_data >> (8 - x)), gpreg);

	*read_bit = INB (nc_gpreg);
}

/*
 *  READ 2 bytes from the NVRAM
 */
static void __init 
T93C46_Read_Word(ncr_slot *np, u_short *nvram_data, u_char *gpreg)
{
	int x;
	u_char read_bit;

	*nvram_data = 0;
	for (x = 0; x < 16; x++) {
		T93C46_Read_Bit(np, &read_bit, gpreg);

		if (read_bit & 0x01)
			*nvram_data |=  (0x01 << (15 - x));
		else
			*nvram_data &= ~(0x01 << (15 - x));
	}
}

/*
 *  Read Tekram NvRAM data.
 */
static int __init 
T93C46_Read_Data(ncr_slot *np, u_short *data,int len,u_char *gpreg)
{
	u_char	read_bit;
	int	x;

	for (x = 0; x < len; x++)  {

		/* output read command and address */
		T93C46_Send_Command(np, 0x180 | x, &read_bit, gpreg);
		if (read_bit & 0x01)
			return 1; /* Bad */
		T93C46_Read_Word(np, &data[x], gpreg);
		T93C46_Stop(np, gpreg);
	}

	return 0;
}

/*
 *  Try reading 93C46 Tekram NVRAM.
 */
static int __init 
sym_read_T93C46_nvram (ncr_slot *np, Tekram_nvram *nvram)
{
	u_char gpcntl, gpreg;
	u_char old_gpcntl, old_gpreg;
	int retv = 1;

	/* save current state of GPCNTL and GPREG */
	old_gpreg	= INB (nc_gpreg);
	old_gpcntl	= INB (nc_gpcntl);

	/* set up GPREG & GPCNTL to set GPIO0/1/2/4 in to known state, 0 in,
	   1/2/4 out */
	gpreg = old_gpreg & 0xe9;
	OUTB (nc_gpreg, gpreg);
	gpcntl = (old_gpcntl & 0xe9) | 0x09;
	OUTB (nc_gpcntl, gpcntl);

	/* input all of NVRAM, 64 words */
	retv = T93C46_Read_Data(np, (u_short *) nvram,
				sizeof(*nvram) / sizeof(short), &gpreg);
	
	/* return GPIO0/1/2/4 to original states after having accessed NVRAM */
	OUTB (nc_gpcntl, old_gpcntl);
	OUTB (nc_gpreg,  old_gpreg);

	return retv;
}

/*
 *  Try reading Tekram NVRAM.
 *  Return 0 if OK.
 */
static int __init 
sym_read_Tekram_nvram (ncr_slot *np, u_short device_id, Tekram_nvram *nvram)
{
	u_char *data = (u_char *) nvram;
	int len = sizeof(*nvram);
	u_short	csum;
	int x;

	switch (device_id) {
	case PCI_DEVICE_ID_NCR_53C885:
	case PCI_DEVICE_ID_NCR_53C895:
	case PCI_DEVICE_ID_NCR_53C896:
		x = sym_read_S24C16_nvram(np, TEKRAM_24C16_NVRAM_ADDRESS,
					  data, len);
		break;
	case PCI_DEVICE_ID_NCR_53C875:
		x = sym_read_S24C16_nvram(np, TEKRAM_24C16_NVRAM_ADDRESS,
					  data, len);
		if (!x)
			break;
	default:
		x = sym_read_T93C46_nvram(np, nvram);
		break;
	}
	if (x)
		return 1;

	/* verify checksum */
	for (x = 0, csum = 0; x < len - 1; x += 2)
		csum += data[x] + (data[x+1] << 8);
	if (csum != 0x1234)
		return 1;

	return 0;
}

#endif	/* SCSI_NCR_NVRAM_SUPPORT */

/*
**	Module stuff
*/

MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0)
static
#endif
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0) || defined(MODULE)
Scsi_Host_Template driver_template = SYM53C8XX;
#include "scsi_module.c"
#endif
