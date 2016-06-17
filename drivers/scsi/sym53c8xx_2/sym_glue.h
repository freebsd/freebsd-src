/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SYM_GLUE_H
#define SYM_GLUE_H

#if 0
#define SYM_CONF_DMA_ADDRESSING_MODE 2
#endif

#define LinuxVersionCode(v, p, s) (((v)<<16)+((p)<<8)+(s))
#include <linux/version.h>
#if	LINUX_VERSION_CODE < LinuxVersionCode(2, 2, 0)
#error	"This driver requires a kernel version not lower than 2.2.0"
#endif

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,17)
#include <linux/spinlock.h>
#else
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

#include <linux/blk.h>

#ifdef __sparc__
#  include <asm/irq.h>
#endif
#include <linux/init.h>

#ifndef	__init
#define	__init
#endif
#ifndef	__initdata
#define	__initdata
#endif

#include "../scsi.h"
#include "../hosts.h"
#include "../constants.h"
#include "../sd.h"

#include <linux/types.h>

/*
 *  Define BITS_PER_LONG for earlier linux versions.
 */
#ifndef	BITS_PER_LONG
#if (~0UL) == 0xffffffffUL
#define	BITS_PER_LONG	32
#else
#define	BITS_PER_LONG	64
#endif
#endif

typedef	u_long	vm_offset_t;

#ifndef bcopy
#define bcopy(s, d, n)	memcpy((d), (s), (n))
#endif

#ifndef bzero
#define bzero(d, n)	memset((d), 0, (n))
#endif

#ifndef bcmp
#define bcmp(a, b, n)	memcmp((a), (b), (n))
#endif

/*
 *  General driver includes.
 */
#include "sym53c8xx.h"
#include "sym_misc.h"
#include "sym_conf.h"
#include "sym_defs.h"

/*
 * Configuration addendum for Linux.
 */
#if	LINUX_VERSION_CODE >= LinuxVersionCode(2,3,47)
#define	SYM_LINUX_DYNAMIC_DMA_MAPPING
#endif

#define	SYM_CONF_TIMER_INTERVAL		((HZ+1)/2)

#define SYM_OPT_HANDLE_DIR_UNKNOWN
#define SYM_OPT_HANDLE_DEVICE_QUEUEING
#define SYM_OPT_NVRAM_PRE_READ
#define SYM_OPT_SNIFF_INQUIRY
#define SYM_OPT_LIMIT_COMMAND_REORDERING
#define	SYM_OPT_ANNOUNCE_TRANSFER_RATE

#ifdef	SYM_LINUX_DYNAMIC_DMA_MAPPING
#define	SYM_OPT_BUS_DMA_ABSTRACTION
#endif

/*
 *  Print a message with severity.
 */
#define printf_emerg(args...)	printk(KERN_EMERG args)
#define	printf_alert(args...)	printk(KERN_ALERT args)
#define	printf_crit(args...)	printk(KERN_CRIT args)
#define	printf_err(args...)	printk(KERN_ERR	args)
#define	printf_warning(args...)	printk(KERN_WARNING args)
#define	printf_notice(args...)	printk(KERN_NOTICE args)
#define	printf_info(args...)	printk(KERN_INFO args)
#define	printf_debug(args...)	printk(KERN_DEBUG args)
#define	printf(args...)		printk(args)

/*
 *  Insert a delay in micro-seconds and milli-seconds.
 */
void sym_udelay(int us);
void sym_mdelay(int ms);

/*
 *  Let the compiler know about driver data structure names.
 */
typedef struct sym_tcb *tcb_p;
typedef struct sym_lcb *lcb_p;
typedef struct sym_ccb *ccb_p;
typedef struct sym_hcb *hcb_p;
typedef struct sym_stcb *stcb_p;
typedef struct sym_slcb *slcb_p;
typedef struct sym_sccb *sccb_p;
typedef struct sym_shcb *shcb_p;

/*
 *  Define a reference to the O/S dependant IO request.
 */
typedef Scsi_Cmnd *cam_ccb_p;	/* Generic */
typedef Scsi_Cmnd *cam_scsiio_p;/* SCSI I/O */


/*
 *  IO functions definition for big/little endian CPU support.
 *  For now, PCI chips are only supported in little endian addressing mode, 
 */

#ifdef	__BIG_ENDIAN

#define	inw_l2b		inw
#define	inl_l2b		inl
#define	outw_b2l	outw
#define	outl_b2l	outl
#define	readw_l2b	readw
#define	readl_l2b	readl
#define	writew_b2l	writew
#define	writel_b2l	writel

#else	/* little endian */

#if defined(__i386__)	/* i386 implements full FLAT memory/MMIO model */
#define	inw_raw		inw
#define	inl_raw		inl
#define	outw_raw	outw
#define	outl_raw	outl
#define readb_raw(a)	(*(volatile unsigned char *) (a))
#define readw_raw(a)	(*(volatile unsigned short *) (a))
#define readl_raw(a)	(*(volatile unsigned int *) (a))
#define writeb_raw(b,a)	((*(volatile unsigned char *) (a)) = (b))
#define writew_raw(b,a)	((*(volatile unsigned short *) (a)) = (b))
#define writel_raw(b,a)	((*(volatile unsigned int *) (a)) = (b))

#else	/* Other little-endian */
#define	inw_raw		inw
#define	inl_raw		inl
#define	outw_raw	outw
#define	outl_raw	outl
#define	readw_raw	readw
#define	readl_raw	readl
#define	writew_raw	writew
#define	writel_raw	writel

#endif
#endif

#ifdef	SYM_CONF_CHIP_BIG_ENDIAN
#error	"Chips in BIG ENDIAN addressing mode are not (yet) supported"
#endif


/*
 *  If the chip uses big endian addressing mode over the 
 *  PCI, actual io register addresses for byte and word 
 *  accesses must be changed according to lane routing.
 *  Btw, sym_offb() and sym_offw() macros only apply to 
 *  constants and so donnot generate bloated code.
 */

#if	defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define sym_offb(o)	(((o)&~3)+((~((o)&3))&3))
#define sym_offw(o)	(((o)&~3)+((~((o)&3))&2))

#else

#define sym_offb(o)	(o)
#define sym_offw(o)	(o)

#endif

/*
 *  If the CPU and the chip use same endian-ness adressing,
 *  no byte reordering is needed for script patching.
 *  Macro cpu_to_scr() is to be used for script patching.
 *  Macro scr_to_cpu() is to be used for getting a DWORD 
 *  from the script.
 */

#if	defined(__BIG_ENDIAN) && !defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define cpu_to_scr(dw)	cpu_to_le32(dw)
#define scr_to_cpu(dw)	le32_to_cpu(dw)

#elif	defined(__LITTLE_ENDIAN) && defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define cpu_to_scr(dw)	cpu_to_be32(dw)
#define scr_to_cpu(dw)	be32_to_cpu(dw)

#else

#define cpu_to_scr(dw)	(dw)
#define scr_to_cpu(dw)	(dw)

#endif

/*
 *  Access to the controller chip.
 *
 *  If SYM_CONF_IOMAPPED is defined, the driver will use 
 *  normal IOs instead of the MEMORY MAPPED IO method  
 *  recommended by PCI specifications.
 *  If all PCI bridges, host brigdes and architectures 
 *  would have been correctly designed for PCI, this 
 *  option would be useless.
 *
 *  If the CPU and the chip use same endian-ness adressing,
 *  no byte reordering is needed for accessing chip io 
 *  registers. Functions suffixed by '_raw' are assumed 
 *  to access the chip over the PCI without doing byte 
 *  reordering. Functions suffixed by '_l2b' are 
 *  assumed to perform little-endian to big-endian byte 
 *  reordering, those suffixed by '_b2l' blah, blah,
 *  blah, ...
 */

#if defined(SYM_CONF_IOMAPPED)

/*
 *  IO mapped only input / ouput
 */

#define	INB_OFF(o)        inb (np->s.io_port + sym_offb(o))
#define	OUTB_OFF(o, val)  outb ((val), np->s.io_port + sym_offb(o))

#if	defined(__BIG_ENDIAN) && !defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define	INW_OFF(o)        inw_l2b (np->s.io_port + sym_offw(o))
#define	INL_OFF(o)        inl_l2b (np->s.io_port + (o))

#define	OUTW_OFF(o, val)  outw_b2l ((val), np->s.io_port + sym_offw(o))
#define	OUTL_OFF(o, val)  outl_b2l ((val), np->s.io_port + (o))

#elif	defined(__LITTLE_ENDIAN) && defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define	INW_OFF(o)        inw_b2l (np->s.io_port + sym_offw(o))
#define	INL_OFF(o)        inl_b2l (np->s.io_port + (o))

#define	OUTW_OFF(o, val)  outw_l2b ((val), np->s.io_port + sym_offw(o))
#define	OUTL_OFF(o, val)  outl_l2b ((val), np->s.io_port + (o))

#else

#define	INW_OFF(o)        inw_raw (np->s.io_port + sym_offw(o))
#define	INL_OFF(o)        inl_raw (np->s.io_port + (o))

#define	OUTW_OFF(o, val)  outw_raw ((val), np->s.io_port + sym_offw(o))
#define	OUTL_OFF(o, val)  outl_raw ((val), np->s.io_port + (o))

#endif	/* ENDIANs */

#else	/* defined SYM_CONF_IOMAPPED */

/*
 *  MEMORY mapped IO input / output
 */

#define INB_OFF(o)        readb((char *)np->s.mmio_va + sym_offb(o))
#define OUTB_OFF(o, val)  writeb((val), (char *)np->s.mmio_va + sym_offb(o))

#if	defined(__BIG_ENDIAN) && !defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define INW_OFF(o)        readw_l2b((char *)np->s.mmio_va + sym_offw(o))
#define INL_OFF(o)        readl_l2b((char *)np->s.mmio_va + (o))

#define OUTW_OFF(o, val)  writew_b2l((val), (char *)np->s.mmio_va + sym_offw(o))
#define OUTL_OFF(o, val)  writel_b2l((val), (char *)np->s.mmio_va + (o))

#elif	defined(__LITTLE_ENDIAN) && defined(SYM_CONF_CHIP_BIG_ENDIAN)

#define INW_OFF(o)        readw_b2l((char *)np->s.mmio_va + sym_offw(o))
#define INL_OFF(o)        readl_b2l((char *)np->s.mmio_va + (o))

#define OUTW_OFF(o, val)  writew_l2b((val), (char *)np->s.mmio_va + sym_offw(o))
#define OUTL_OFF(o, val)  writel_l2b((val), (char *)np->s.mmio_va + (o))

#else

#define INW_OFF(o)        readw_raw((char *)np->s.mmio_va + sym_offw(o))
#define INL_OFF(o)        readl_raw((char *)np->s.mmio_va + (o))

#define OUTW_OFF(o, val)  writew_raw((val), (char *)np->s.mmio_va + sym_offw(o))
#define OUTL_OFF(o, val)  writel_raw((val), (char *)np->s.mmio_va + (o))

#endif

#endif	/* defined SYM_CONF_IOMAPPED */

#define OUTRAM_OFF(o, a, l) memcpy_toio(np->s.ram_va + (o), (a), (l))

/*
 *  Remap some status field values.
 */
#define CAM_REQ_CMP		DID_OK
#define CAM_SEL_TIMEOUT		DID_NO_CONNECT
#define CAM_CMD_TIMEOUT		DID_TIME_OUT
#define CAM_REQ_ABORTED		DID_ABORT
#define CAM_UNCOR_PARITY	DID_PARITY
#define CAM_SCSI_BUS_RESET	DID_RESET	
#define CAM_REQUEUE_REQ		DID_SOFT_ERROR
#define	CAM_UNEXP_BUSFREE	DID_ERROR
#define	CAM_SCSI_BUSY		DID_BUS_BUSY

#define	CAM_DEV_NOT_THERE	DID_NO_CONNECT
#define	CAM_REQ_INVALID		DID_ERROR
#define	CAM_REQ_TOO_BIG		DID_ERROR

#define	CAM_RESRC_UNAVAIL	DID_ERROR

/*
 *  Remap SCSI data direction values.
 */
#ifndef	SCSI_DATA_UNKNOWN
#define	SCSI_DATA_UNKNOWN	0
#define	SCSI_DATA_WRITE		1
#define	SCSI_DATA_READ		2
#define	SCSI_DATA_NONE		3
#endif
#define CAM_DIR_NONE		SCSI_DATA_NONE
#define CAM_DIR_IN		SCSI_DATA_READ
#define CAM_DIR_OUT		SCSI_DATA_WRITE
#define CAM_DIR_UNKNOWN		SCSI_DATA_UNKNOWN

/*
 *  These ones are used as return code from 
 *  error recovery handlers under Linux.
 */
#define SCSI_SUCCESS	SUCCESS
#define SCSI_FAILED	FAILED

/*
 *  System specific target data structure.
 *  None for now, under Linux.
 */
/* #define SYM_HAVE_STCB */

/*
 *  System specific lun data structure.
 */
#define SYM_HAVE_SLCB
struct sym_slcb {
	u_short	reqtags;	/* Number of tags requested by user */
	u_short scdev_depth;	/* Queue depth set in select_queue_depth() */
};

/*
 *  System specific command data structure.
 *  Not needed under Linux.
 */
/* struct sym_sccb */

/*
 *  System specific host data structure.
 */
struct sym_shcb {
	/*
	 *  Chip and controller indentification.
	 */
	int		unit;
	char		inst_name[16];
	char		chip_name[8];
	struct pci_dev	*device;

	u_char		bus;		/* PCI BUS number		*/
	u_char		device_fn;	/* PCI BUS device and function	*/

	spinlock_t	smp_lock;	/* Lock for SMP threading       */

	vm_offset_t	mmio_va;	/* MMIO kernel virtual address	*/
	vm_offset_t	ram_va;		/* RAM  kernel virtual address	*/
	u_long		io_port;	/* IO port address cookie	*/
	u_short		io_ws;		/* IO window size		*/
	int		irq;		/* IRQ number			*/

	SYM_QUEHEAD	wait_cmdq;	/* Awaiting SCSI commands	*/
	SYM_QUEHEAD	busy_cmdq;	/* Enqueued SCSI commands	*/

	struct timer_list timer;	/* Timer handler link header	*/
	u_long		lasttime;
	u_long		settle_time;	/* Resetting the SCSI BUS	*/
	u_char		settle_time_valid;
#if LINUX_VERSION_CODE < LinuxVersionCode(2, 4, 0)
	u_char		release_stage;	/* Synchronisation on release	*/
#endif
};

/*
 *  Return the name of the controller.
 */
#define sym_name(np) (np)->s.inst_name

/*
 *  Data structure used as input for the NVRAM reading.
 *  Must resolve the IO macros and sym_name(), when  
 *  used as sub-field 's' of another structure.
 */
typedef struct {
	int	bus;
	u_char	device_fn;
	u_long	base;
	u_long	base_2;
	u_long	base_c;
	u_long	base_2_c;
	int	irq;
/* port and address fields to fit INB, OUTB macros */
	u_long	io_port;
	vm_offset_t mmio_va;
	char	inst_name[16];
} sym_slot;

typedef struct sym_nvram sym_nvram;
typedef struct sym_pci_chip sym_chip;

typedef struct {
	struct pci_dev *pdev;
	sym_slot  s;
	sym_chip  chip;
	sym_nvram *nvram;
	u_short device_id;
	u_char host_id;
#ifdef	SYM_CONF_PQS_PDS_SUPPORT
	u_char pqs_pds;
#endif
	int attach_done;
} sym_device;

typedef sym_device *sdev_p;

/*
 *  The driver definitions (sym_hipd.h) must know about a 
 *  couple of things related to the memory allocator.
 */
typedef u_long m_addr_t;	/* Enough bits to represent any address */
#define SYM_MEM_PAGE_ORDER 0	/* 1 PAGE  maximum */
#define SYM_MEM_CLUSTER_SHIFT	(PAGE_SHIFT+SYM_MEM_PAGE_ORDER)
#ifdef	MODULE
#define SYM_MEM_FREE_UNUSED	/* Free unused pages immediately */
#endif
#ifdef	SYM_LINUX_DYNAMIC_DMA_MAPPING
typedef struct pci_dev *m_pool_ident_t;
#endif

/*
 *  Include driver soft definitions.
 */
#include "sym_fw.h"
#include "sym_hipd.h"

/*
 *  Memory allocator related stuff.
 */

#define SYM_MEM_GFP_FLAGS	GFP_ATOMIC
#define SYM_MEM_WARN	1	/* Warn on failed operations */

#define sym_get_mem_cluster()	\
	__get_free_pages(SYM_MEM_GFP_FLAGS, SYM_MEM_PAGE_ORDER)
#define sym_free_mem_cluster(p)	\
	free_pages(p, SYM_MEM_PAGE_ORDER)

void *sym_calloc(int size, char *name);
void sym_mfree(void *m, int size, char *name);

#ifndef	SYM_LINUX_DYNAMIC_DMA_MAPPING
/*
 *  Simple case.
 *  All the memory assummed DMAable and O/S providing virtual 
 *  to bus physical address translation.
 */
#define __sym_calloc_dma(pool_id, size, name)	sym_calloc(size, name)
#define __sym_mfree_dma(pool_id, m, size, name)	sym_mfree(m, size, name)
#define __vtobus(b, p)				virt_to_bus(p)

#else	/* SYM_LINUX_DYNAMIC_DMA_MAPPING */
/*
 *  Complex case.
 *  We have to provide the driver memory allocator with methods for 
 *  it to maintain virtual to bus physical address translations.
 */

#define sym_m_pool_match(mp_id1, mp_id2)	(mp_id1 == mp_id2)

static __inline m_addr_t sym_m_get_dma_mem_cluster(m_pool_p mp, m_vtob_p vbp)
{
	void *vaddr = 0;
	dma_addr_t baddr = 0;

	vaddr = pci_alloc_consistent(mp->dev_dmat,SYM_MEM_CLUSTER_SIZE, &baddr);
	if (vaddr) {
		vbp->vaddr = (m_addr_t) vaddr;
		vbp->baddr = (m_addr_t) baddr;
	}
	return (m_addr_t) vaddr;
}

static __inline void sym_m_free_dma_mem_cluster(m_pool_p mp, m_vtob_p vbp)
{
	pci_free_consistent(mp->dev_dmat, SYM_MEM_CLUSTER_SIZE,
	                    (void *)vbp->vaddr, (dma_addr_t)vbp->baddr);
}

#define sym_m_create_dma_mem_tag(mp)	(0)

#define sym_m_delete_dma_mem_tag(mp)	do { ; } while (0)

void *__sym_calloc_dma(m_pool_ident_t dev_dmat, int size, char *name);
void __sym_mfree_dma(m_pool_ident_t dev_dmat, void *m, int size, char *name);
m_addr_t __vtobus(m_pool_ident_t dev_dmat, void *m);

#endif	/* SYM_LINUX_DYNAMIC_DMA_MAPPING */

/*
 *  Set the status field of a CAM CCB.
 */
static __inline void 
sym_set_cam_status(Scsi_Cmnd  *ccb, int status)
{
	ccb->result &= ~(0xff  << 16);
	ccb->result |= (status << 16);
}

/*
 *  Get the status field of a CAM CCB.
 */
static __inline int 
sym_get_cam_status(Scsi_Cmnd  *ccb)
{
	return ((ccb->result >> 16) & 0xff);
}

/*
 *  The dma mapping is mostly handled by the 
 *  SCSI layer and the driver glue under Linux.
 */
#define sym_data_dmamap_create(np, cp)		(0)
#define sym_data_dmamap_destroy(np, cp)		do { ; } while (0)
#define sym_data_dmamap_unload(np, cp)		do { ; } while (0)
#define sym_data_dmamap_presync(np, cp)		do { ; } while (0)
#define sym_data_dmamap_postsync(np, cp)	do { ; } while (0)

/*
 *  Async handler for negotiations.
 */
void sym_xpt_async_nego_wide(hcb_p np, int target);
#define sym_xpt_async_nego_sync(np, target)	\
	sym_announce_transfer_rate(np, target)
#define sym_xpt_async_nego_ppr(np, target)	\
	sym_announce_transfer_rate(np, target)

/*
 *  Build CAM result for a successful IO and for a failed IO.
 */
static __inline void sym_set_cam_result_ok(hcb_p np, ccb_p cp, int resid)
{
	Scsi_Cmnd *cmd = cp->cam_ccb;

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,99)
	cmd->resid = resid;
#endif
	cmd->result = (((DID_OK) << 16) + ((cp->ssss_status) & 0x7f));
}
void sym_set_cam_result_error(hcb_p np, ccb_p cp, int resid);

/*
 *  Other O/S specific methods.
 */
#define sym_cam_target_id(ccb)	(ccb)->target
#define sym_cam_target_lun(ccb)	(ccb)->lun
#define	sym_freeze_cam_ccb(ccb)	do { ; } while (0)
void sym_xpt_done(hcb_p np, cam_ccb_p ccb);
void sym_xpt_done2(hcb_p np, cam_ccb_p ccb, int cam_status);
void sym_print_addr (ccb_p cp);
void sym_xpt_async_bus_reset(hcb_p np);
void sym_xpt_async_sent_bdr(hcb_p np, int target);
int  sym_setup_data_and_start (hcb_p np, cam_scsiio_p csio, ccb_p cp);
void sym_log_bus_error(hcb_p np);
#ifdef	SYM_OPT_SNIFF_INQUIRY
void sym_sniff_inquiry(hcb_p np, Scsi_Cmnd *cmd, int resid);
#endif

#endif /* SYM_GLUE_H */
