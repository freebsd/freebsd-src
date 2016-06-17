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
#define SYM_GLUE_C

#include <linux/module.h>
#include "sym_glue.h"

#define NAME53C		"sym53c"
#define NAME53C8XX	"sym53c8xx"

/*
 *  Simple Wrapper to kernel PCI bus interface.
 */

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
		base |= (((u_long)pdev->base_address[++index]) << 32);
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

#if LINUX_VERSION_CODE  < LinuxVersionCode(2,4,0)
#define pci_enable_device(pdev)		(0)
#endif

#if LINUX_VERSION_CODE  < LinuxVersionCode(2,4,4)
#define scsi_set_pci_device(inst, pdev)	do { ;} while (0)
#endif

/*
 *  Insert a delay in micro-seconds and milli-seconds.
 */
void sym_udelay(int us) { udelay(us); }
void sym_mdelay(int ms) { mdelay(ms); }

/*
 *  SMP threading.
 *
 *  The whole SCSI sub-system under Linux is basically single-threaded.
 *  Everything, including low-level driver interrupt routine, happens 
 *  with the `io_request_lock' held.
 *  The sym53c8xx-1.x drivers series ran their interrupt code using a 
 *  spin mutex per controller. This added complexity without improving 
 *  scalability significantly. the sym-2 driver still use a spinlock 
 *  per controller for safety, but basically runs with the damned 
 *  io_request_lock held.
 */

spinlock_t sym53c8xx_lock = SPIN_LOCK_UNLOCKED;

#define	SYM_LOCK_DRIVER(flags)    spin_lock_irqsave(&sym53c8xx_lock, flags)
#define	SYM_UNLOCK_DRIVER(flags)  spin_unlock_irqrestore(&sym53c8xx_lock,flags)

#define SYM_INIT_LOCK_HCB(np)     spin_lock_init(&np->s.smp_lock);
#define	SYM_LOCK_HCB(np, flags)   spin_lock_irqsave(&np->s.smp_lock, flags)
#define	SYM_UNLOCK_HCB(np, flags) spin_unlock_irqrestore(&np->s.smp_lock, flags)

#define	SYM_LOCK_SCSI(np, flags) \
		spin_lock_irqsave(&io_request_lock, flags)
#define	SYM_UNLOCK_SCSI(np, flags) \
		spin_unlock_irqrestore(&io_request_lock, flags)

/* Ugly, but will make things easier if this locking will ever disappear */
#define	SYM_LOCK_SCSI_NOSAVE(np)	spin_lock_irq(&io_request_lock)
#define	SYM_UNLOCK_SCSI_NORESTORE(np)	spin_unlock_irq(&io_request_lock)

/*
 *  These simple macros limit expression involving 
 *  kernel time values (jiffies) to some that have 
 *  chance not to be too much incorrect. :-)
 */
#define ktime_get(o)		(jiffies + (u_long) o)
#define ktime_exp(b)		((long)(jiffies) - (long)(b) >= 0)
#define ktime_dif(a, b)		((long)(a) - (long)(b))
#define ktime_add(a, o)		((a) + (u_long)(o))
#define ktime_sub(a, o)		((a) - (u_long)(o))

/*
 *  Wrappers to the generic memory allocator.
 */
void *sym_calloc(int size, char *name)
{
	u_long flags;
	void *m;
	SYM_LOCK_DRIVER(flags);
	m = sym_calloc_unlocked(size, name);
	SYM_UNLOCK_DRIVER(flags);
	return m;
}

void sym_mfree(void *m, int size, char *name)
{
	u_long flags;
	SYM_LOCK_DRIVER(flags);
	sym_mfree_unlocked(m, size, name);
	SYM_UNLOCK_DRIVER(flags);
}

#ifdef	SYM_LINUX_DYNAMIC_DMA_MAPPING

void *__sym_calloc_dma(m_pool_ident_t dev_dmat, int size, char *name)
{
	u_long flags;
	void *m;
	SYM_LOCK_DRIVER(flags);
	m = __sym_calloc_dma_unlocked(dev_dmat, size, name);
	SYM_UNLOCK_DRIVER(flags);
	return m;
}

void __sym_mfree_dma(m_pool_ident_t dev_dmat, void *m, int size, char *name)
{
	u_long flags;
	SYM_LOCK_DRIVER(flags);
	__sym_mfree_dma_unlocked(dev_dmat, m, size, name);
	SYM_UNLOCK_DRIVER(flags);
}

m_addr_t __vtobus(m_pool_ident_t dev_dmat, void *m)
{
	u_long flags;
	m_addr_t b;
	SYM_LOCK_DRIVER(flags);
	b = __vtobus_unlocked(dev_dmat, m);
	SYM_UNLOCK_DRIVER(flags);
	return b;
}

#endif	/* SYM_LINUX_DYNAMIC_DMA_MAPPING */


/*
 *  Map/unmap a PCI memory window.
 */
#ifndef SYM_OPT_NO_BUS_MEMORY_MAPPING
static u_long __init pci_map_mem(u_long base, u_long size)
{
	u_long page_base	= ((u_long) base) & PAGE_MASK;
	u_long page_offs	= ((u_long) base) - page_base;
	u_long page_remapped	= (u_long) ioremap(page_base, page_offs+size);

	return page_remapped? (page_remapped + page_offs) : 0UL;
}

static void __init pci_unmap_mem(u_long vaddr, u_long size)
{
	if (vaddr)
		iounmap((void *) (vaddr & PAGE_MASK));
}
#endif

/*
 *  Used to retrieve the host structure when the 
 *  driver is called from the proc FS.
 */
static struct Scsi_Host	*first_host = NULL;

/*
 *  /proc directory entry and proc_info.
 */
#if LINUX_VERSION_CODE < LinuxVersionCode(2,3,27)
static struct proc_dir_entry proc_scsi_sym53c8xx = {
    PROC_SCSI_SYM53C8XX, 9, NAME53C8XX,
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};
#endif

/*
 *  Transfer direction
 *
 *  Until some linux kernel version near 2.3.40, low-level scsi 
 *  drivers were not told about data transfer direction.
 */
#if LINUX_VERSION_CODE > LinuxVersionCode(2, 3, 40)

#define scsi_data_direction(cmd)	(cmd->sc_data_direction)

#else

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

#endif

/*
 *  Driver host data structure.
 */
struct host_data {
     hcb_p ncb;
};

/*
 * Some type that fit DMA addresses as seen from BUS.
 */
#ifndef SYM_LINUX_DYNAMIC_DMA_MAPPING
typedef u_long		bus_addr_t;
#else
#if	SYM_CONF_DMA_ADDRESSING_MODE > 0
typedef dma64_addr_t	bus_addr_t;
#else
typedef dma_addr_t	bus_addr_t;
#endif
#endif

/*
 *  Used by the eh thread to wait for command completion.
 *  It is allocated on the eh thread stack.
 */
struct sym_eh_wait {
	struct semaphore sem;
	struct timer_list timer;
	void (*old_done)(Scsi_Cmnd *);
	int to_do;
	int timed_out;
};

/*
 *  Driver private area in the SCSI command structure.
 */
struct sym_ucmd {		/* Override the SCSI pointer structure */
	SYM_QUEHEAD link_cmdq;	/* Must stay at offset ZERO */
#ifdef SYM_LINUX_DYNAMIC_DMA_MAPPING
	bus_addr_t data_mapping;
	u_char	data_mapped;
#endif
	struct sym_eh_wait *eh_wait;
};

typedef struct sym_ucmd *ucmd_p;

#define SYM_UCMD_PTR(cmd)  ((ucmd_p)(&(cmd)->SCp))
#define SYM_SCMD_PTR(ucmd) sym_que_entry(ucmd, Scsi_Cmnd, SCp)
#define SYM_SOFTC_PTR(cmd) (((struct host_data *)cmd->host->hostdata)->ncb)

/*
 *  Deal with DMA mapping/unmapping.
 */

#ifndef SYM_LINUX_DYNAMIC_DMA_MAPPING

/* Linux versions prior to pci bus iommu kernel interface */

#define __unmap_scsi_data(pdev, cmd)	do {; } while (0)
#define __map_scsi_single_data(pdev, cmd) (__vtobus(pdev,(cmd)->request_buffer))
#define __map_scsi_sg_data(pdev, cmd)	((cmd)->use_sg)
#define __sync_scsi_data(pdev, cmd)	do {; } while (0)

#define bus_sg_dma_address(sc)		vtobus((sc)->address)
#define bus_sg_dma_len(sc)		((sc)->length)

#else /* Linux version with pci bus iommu kernel interface */

#define	bus_unmap_sg(pdev, sgptr, sgcnt, dir)		\
	pci_unmap_sg(pdev, sgptr, sgcnt, dir)

#define	bus_unmap_single(pdev, mapping, bufptr, dir)	\
	pci_unmap_single(pdev, mapping, bufptr, dir)

#define	bus_map_single(pdev, bufptr, bufsiz, dir)	\
	pci_map_single(pdev, bufptr, bufsiz, dir)
 
#define	bus_map_sg(pdev, sgptr, sgcnt, dir)		\
	pci_map_sg(pdev, sgptr, sgcnt, dir)

#define	bus_dma_sync_sg(pdev, sgptr, sgcnt, dir)	\
	pci_dma_sync_sg(pdev, sgptr, sgcnt, dir)

#define	bus_dma_sync_single(pdev, mapping, bufsiz, dir)	\
	pci_dma_sync_single(pdev, mapping, bufsiz, dir)

#define bus_sg_dma_address(sc)	sg_dma_address(sc)
#define bus_sg_dma_len(sc)	sg_dma_len(sc)

static void __unmap_scsi_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	switch(SYM_UCMD_PTR(cmd)->data_mapped) {
	case 2:
		bus_unmap_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);
		break;
	case 1:
		bus_unmap_single(pdev, SYM_UCMD_PTR(cmd)->data_mapping,
				 cmd->request_bufflen, dma_dir);
		break;
	}
	SYM_UCMD_PTR(cmd)->data_mapped = 0;
}

static bus_addr_t __map_scsi_single_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	bus_addr_t mapping;
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	mapping = bus_map_single(pdev, cmd->request_buffer,
				 cmd->request_bufflen, dma_dir);
	if (mapping) {
		SYM_UCMD_PTR(cmd)->data_mapped  = 1;
		SYM_UCMD_PTR(cmd)->data_mapping = mapping;
	}

	return mapping;
}

static int __map_scsi_sg_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	int use_sg;
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	use_sg = bus_map_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);
	if (use_sg > 0) {
		SYM_UCMD_PTR(cmd)->data_mapped  = 2;
		SYM_UCMD_PTR(cmd)->data_mapping = use_sg;
	}

	return use_sg;
}

static void __sync_scsi_data(pcidev_t pdev, Scsi_Cmnd *cmd)
{
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	switch(SYM_UCMD_PTR(cmd)->data_mapped) {
	case 2:
		bus_dma_sync_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);
		break;
	case 1:
		bus_dma_sync_single(pdev, SYM_UCMD_PTR(cmd)->data_mapping,
				    cmd->request_bufflen, dma_dir);
		break;
	}
}

#endif	/* SYM_LINUX_DYNAMIC_DMA_MAPPING */

#define unmap_scsi_data(np, cmd)	\
		__unmap_scsi_data(np->s.device, cmd)
#define map_scsi_single_data(np, cmd)	\
		__map_scsi_single_data(np->s.device, cmd)
#define map_scsi_sg_data(np, cmd)	\
		__map_scsi_sg_data(np->s.device, cmd)
#define sync_scsi_data(np, cmd)		\
		__sync_scsi_data(np->s.device, cmd)

/*
 *  Complete a pending CAM CCB.
 */
void sym_xpt_done(hcb_p np, Scsi_Cmnd *ccb)
{
	sym_remque(&SYM_UCMD_PTR(ccb)->link_cmdq);
	unmap_scsi_data(np, ccb);
	ccb->scsi_done(ccb);
}

void sym_xpt_done2(hcb_p np, Scsi_Cmnd *ccb, int cam_status)
{
	sym_set_cam_status(ccb, cam_status);
	sym_xpt_done(np, ccb);
}


/*
 *  Print something that identifies the IO.
 */
void sym_print_addr (ccb_p cp)
{
	Scsi_Cmnd *cmd = cp->cam_ccb;
	if (cmd)
		printf("%s:%d:%d:", sym_name(SYM_SOFTC_PTR(cmd)),
		       cmd->target,cmd->lun);
}

/*
 *  Tell the SCSI layer about a BUS RESET.
 */
void sym_xpt_async_bus_reset(hcb_p np)
{
	printf_notice("%s: SCSI BUS has been reset.\n", sym_name(np));
	np->s.settle_time = ktime_get(sym_driver_setup.settle_delay * HZ);
	np->s.settle_time_valid = 1;
	if (sym_verbose >= 2)
		printf_info("%s: command processing suspended for %d seconds\n",
		            sym_name(np), sym_driver_setup.settle_delay);
}

/*
 *  Tell the SCSI layer about a BUS DEVICE RESET message sent.
 */
void sym_xpt_async_sent_bdr(hcb_p np, int target)
{
	printf_notice("%s: TARGET %d has been reset.\n", sym_name(np), target);
}

/*
 *  Tell the SCSI layer about the new transfer parameters.
 */
void sym_xpt_async_nego_wide(hcb_p np, int target)
{
	if (sym_verbose < 3)
		return;
	sym_announce_transfer_rate(np, target);
}

/*
 *  Choose the more appropriate CAM status if 
 *  the IO encountered an extended error.
 */
static int sym_xerr_cam_status(int cam_status, int x_status)
{
	if (x_status) {
		if	(x_status & XE_PARITY_ERR)
			cam_status = DID_PARITY;
		else if	(x_status &(XE_EXTRA_DATA|XE_SODL_UNRUN|XE_SWIDE_OVRUN))
			cam_status = DID_ERROR;
		else if	(x_status & XE_BAD_PHASE)
			cam_status = DID_ERROR;
		else
			cam_status = DID_ERROR;
	}
	return cam_status;
}

/*
 *  Build CAM result for a failed or auto-sensed IO.
 */
void sym_set_cam_result_error(hcb_p np, ccb_p cp, int resid)
{
	Scsi_Cmnd *csio = cp->cam_ccb;
	u_int cam_status, scsi_status, drv_status;

	drv_status  = 0;
	cam_status  = DID_OK;
	scsi_status = cp->ssss_status;

	if (cp->host_flags & HF_SENSE) {
		scsi_status = cp->sv_scsi_status;
		resid = cp->sv_resid;
		if (sym_verbose && cp->sv_xerr_status)
			sym_print_xerr(cp, cp->sv_xerr_status);
		if (cp->host_status == HS_COMPLETE &&
		    cp->ssss_status == S_GOOD &&
		    cp->xerr_status == 0) {
			cam_status = sym_xerr_cam_status(DID_OK,
							 cp->sv_xerr_status);
			drv_status = DRIVER_SENSE;
			/*
			 *  Bounce back the sense data to user.
			 */
			bzero(&csio->sense_buffer, sizeof(csio->sense_buffer));
			bcopy(cp->sns_bbuf, csio->sense_buffer,
			      MIN(sizeof(csio->sense_buffer),SYM_SNS_BBUF_LEN));
#if 0
			/*
			 *  If the device reports a UNIT ATTENTION condition 
			 *  due to a RESET condition, we should consider all 
			 *  disconnect CCBs for this unit as aborted.
			 */
			if (1) {
				u_char *p;
				p  = (u_char *) csio->sense_data;
				if (p[0]==0x70 && p[2]==0x6 && p[12]==0x29)
					sym_clear_tasks(np, DID_ABORT,
							cp->target,cp->lun, -1);
			}
#endif
		}
		else
			cam_status = DID_ERROR;
	}
	else if (cp->host_status == HS_COMPLETE) 	/* Bad SCSI status */
		cam_status = DID_OK;
	else if (cp->host_status == HS_SEL_TIMEOUT)	/* Selection timeout */
		cam_status = DID_NO_CONNECT;
	else if (cp->host_status == HS_UNEXPECTED)	/* Unexpected BUS FREE*/
		cam_status = DID_ERROR;
	else {						/* Extended error */
		if (sym_verbose) {
			PRINT_ADDR(cp);
			printf ("COMMAND FAILED (%x %x %x).\n",
				cp->host_status, cp->ssss_status,
				cp->xerr_status);
		}
		/*
		 *  Set the most appropriate value for CAM status.
		 */
		cam_status = sym_xerr_cam_status(DID_ERROR, cp->xerr_status);
	}
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,99)
	csio->resid = resid;
#endif
	csio->result = (drv_status << 24) + (cam_status << 16) + scsi_status;
}


/*
 *  Called on successfull INQUIRY response.
 */
void sym_sniff_inquiry(hcb_p np, Scsi_Cmnd *cmd, int resid)
{
	int retv;

	if (!cmd || cmd->use_sg)
		return;

	sync_scsi_data(np, cmd);
	retv = __sym_sniff_inquiry(np, cmd->target, cmd->lun,
				   (u_char *) cmd->request_buffer,
				   cmd->request_bufflen - resid);
	if (retv < 0)
		return;
	else if (retv)
		sym_update_trans_settings(np, &np->target[cmd->target]);
}

/*
 *  Build the scatter/gather array for an I/O.
 */

static int sym_scatter_no_sglist(hcb_p np, ccb_p cp, Scsi_Cmnd *cmd)
{
	struct sym_tblmove *data = &cp->phys.data[SYM_CONF_MAX_SG-1];
	int segment;

	cp->data_len = cmd->request_bufflen;

	if (cmd->request_bufflen) {
		bus_addr_t baddr = map_scsi_single_data(np, cmd);
		if (baddr) {
			sym_build_sge(np, data, baddr, cmd->request_bufflen);
			segment = 1;
		}
		else
			segment = -2;
	}
	else
		segment = 0;

	return segment;
}

static int sym_scatter(hcb_p np, ccb_p cp, Scsi_Cmnd *cmd)
{
	int segment;
	int use_sg = (int) cmd->use_sg;

	cp->data_len = 0;

	if (!use_sg)
		segment = sym_scatter_no_sglist(np, cp, cmd);
	else if (use_sg > SYM_CONF_MAX_SG)
		segment = -1;
	else if ((use_sg = map_scsi_sg_data(np, cmd)) > 0) {
		struct scatterlist *scatter = (struct scatterlist *)cmd->buffer;
		struct sym_tblmove *data;

		data = &cp->phys.data[SYM_CONF_MAX_SG - use_sg];

		for (segment = 0; segment < use_sg; segment++) {
			bus_addr_t baddr = bus_sg_dma_address(&scatter[segment]);
			unsigned int len = bus_sg_dma_len(&scatter[segment]);

			sym_build_sge(np, &data[segment], baddr, len);
			cp->data_len += len;
		}
	}
	else
		segment = -2;

	return segment;
}

/*
 *  Queue a SCSI command.
 */
static int sym_queue_command(hcb_p np, Scsi_Cmnd *ccb)
{
/*	Scsi_Device        *device    = ccb->device; */
	tcb_p	tp;
	lcb_p	lp;
	ccb_p	cp;
	int	order;

	/*
	 *  Minimal checkings, so that we will not 
	 *  go outside our tables.
	 */
	if (ccb->target == np->myaddr ||
	    ccb->target >= SYM_CONF_MAX_TARGET ||
	    ccb->lun    >= SYM_CONF_MAX_LUN) {
		sym_xpt_done2(np, ccb, CAM_DEV_NOT_THERE);
		return 0;
        }

	/*
	 *  Retreive the target descriptor.
	 */
	tp = &np->target[ccb->target];

	/*
	 *  Complete the 1st INQUIRY command with error 
	 *  condition if the device is flagged NOSCAN 
	 *  at BOOT in the NVRAM. This may speed up 
	 *  the boot and maintain coherency with BIOS 
	 *  device numbering. Clearing the flag allows 
	 *  user to rescan skipped devices later.
	 *  We also return error for devices not flagged 
	 *  for SCAN LUNS in the NVRAM since some mono-lun 
	 *  devices behave badly when asked for some non 
	 *  zero LUN. Btw, this is an absolute hack.:-)
	 */
	if (ccb->cmnd[0] == 0x12 || ccb->cmnd[0] == 0x0) {
		if ((tp->usrflags & SYM_SCAN_BOOT_DISABLED) ||
		    ((tp->usrflags & SYM_SCAN_LUNS_DISABLED) && 
		     ccb->lun != 0)) {
			tp->usrflags &= ~SYM_SCAN_BOOT_DISABLED;
			sym_xpt_done2(np, ccb, CAM_DEV_NOT_THERE);
			return 0;
		}
	}

	/*
	 *  Select tagged/untagged.
	 */
	lp = sym_lp(np, tp, ccb->lun);
	order = (lp && lp->s.reqtags) ? M_SIMPLE_TAG : 0;

	/*
	 *  Queue the SCSI IO.
	 */
	cp = sym_get_ccb(np, ccb->target, ccb->lun, order);
	if (!cp)
		return 1;	/* Means resource shortage */
	(void) sym_queue_scsiio(np, ccb, cp);
	return 0;
}

/*
 *  Setup buffers and pointers that address the CDB.
 */
static int __inline sym_setup_cdb(hcb_p np, Scsi_Cmnd *ccb, ccb_p cp)
{
	u32	cmd_ba;
	int	cmd_len;

	/*
	 *  CDB is 16 bytes max.
	 */
	if (ccb->cmd_len > sizeof(cp->cdb_buf)) {
		sym_set_cam_status(cp->cam_ccb, CAM_REQ_INVALID);
		return -1;
	}

	bcopy(ccb->cmnd, cp->cdb_buf, ccb->cmd_len);
	cmd_ba  = CCB_BA (cp, cdb_buf[0]);
	cmd_len = ccb->cmd_len;

	cp->phys.cmd.addr	= cpu_to_scr(cmd_ba);
	cp->phys.cmd.size	= cpu_to_scr(cmd_len);

	return 0;
}

/*
 *  Setup pointers that address the data and start the I/O.
 */
int sym_setup_data_and_start(hcb_p np, Scsi_Cmnd *csio, ccb_p cp)
{
	int dir;
	tcb_p tp = &np->target[cp->target];
	lcb_p lp = sym_lp(np, tp, cp->lun);

	/*
	 *  Build the CDB.
	 */
	if (sym_setup_cdb(np, csio, cp))
		goto out_abort;

	/*
	 *  No direction means no data.
	 */
	dir = scsi_data_direction(csio);
	if (dir != SCSI_DATA_NONE) {
		cp->segments = sym_scatter (np, cp, csio);
		if (cp->segments < 0) {
			if (cp->segments == -2)
				sym_set_cam_status(csio, CAM_RESRC_UNAVAIL);
			else
				sym_set_cam_status(csio, CAM_REQ_TOO_BIG);
			goto out_abort;
		}
	}
	else {
		cp->data_len = 0;
		cp->segments = 0;
	}

	/*
	 *  Set data pointers.
	 */
	sym_setup_data_pointers(np, cp, dir);

	/*
	 *  When `#ifed 1', the code below makes the driver 
	 *  panic on the first attempt to write to a SCSI device.
	 *  It is the first test we want to do after a driver 
	 *  change that does not seem obviously safe. :)
	 */
#if 0
	switch (cp->cdb_buf[0]) {
	case 0x0A: case 0x2A: case 0xAA:
		panic("XXXXXXXXXXXXX WRITE NOT YET ALLOWED XXXXXXXXXXXXXX\n");
		MDELAY(10000);
		break;
	default:
		break;
	}
#endif

	/*
	 *	activate this job.
	 */
	if (lp)
		sym_start_next_ccbs(np, lp, 2);
	else
		sym_put_start_queue(np, cp);
	return 0;

out_abort:
	sym_free_ccb(np, cp);
	sym_xpt_done(np, csio);
	return 0;
}


/*
 *  timer daemon.
 *
 *  Misused to keep the driver running when
 *  interrupts are not configured correctly.
 */
static void sym_timer (hcb_p np)
{
	u_long	thistime = ktime_get(0);

#if LINUX_VERSION_CODE < LinuxVersionCode(2, 4, 0)
	/*
	 *  If release process in progress, let's go
	 *  Set the release stage from 1 to 2 to synchronize
	 *  with the release process.
	 */

	if (np->s.release_stage) {
		if (np->s.release_stage == 1)
			np->s.release_stage = 2;
		return;
	}
#endif

	/*
	 *  Restart the timer.
	 */
#ifdef SYM_CONF_PCIQ_BROKEN_INTR
	np->s.timer.expires = ktime_get((HZ+99)/100);
#else
	np->s.timer.expires = ktime_get(SYM_CONF_TIMER_INTERVAL);
#endif
	add_timer(&np->s.timer);

	/*
	 *  If we are resetting the ncr, wait for settle_time before 
	 *  clearing it. Then command processing will be resumed.
	 */
	if (np->s.settle_time_valid) {
		if (ktime_dif(np->s.settle_time, thistime) <= 0){
			if (sym_verbose >= 2 )
				printk("%s: command processing resumed\n",
				       sym_name(np));
			np->s.settle_time_valid = 0;
		}
		return;
	}

	/*
	 *	Nothing to do for now, but that may come.
	 */
	if (np->s.lasttime + 4*HZ < thistime) {
		np->s.lasttime = thistime;
	}

#ifdef SYM_CONF_PCIQ_MAY_MISS_COMPLETIONS
	/*
	 *  Some way-broken PCI bridges may lead to 
	 *  completions being lost when the clearing 
	 *  of the INTFLY flag by the CPU occurs 
	 *  concurrently with the chip raising this flag.
	 *  If this ever happen, lost completions will 
	 * be reaped here.
	 */
	sym_wakeup_done(np);
#endif

#ifdef SYM_CONF_PCIQ_BROKEN_INTR
	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		/*
		**	Process pending interrupts.
		*/
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("{");
		sym_interrupt(np);
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("}");
	}
#endif /* SYM_CONF_PCIQ_BROKEN_INTR */
}


/*
 *  PCI BUS error handler.
 */
void sym_log_bus_error(hcb_p np)
{
	u_short pci_sts;
	pci_read_config_word(np->s.device, PCI_STATUS, &pci_sts);
	if (pci_sts & 0xf900) {
		pci_write_config_word(np->s.device, PCI_STATUS,
		                         pci_sts);
		printf("%s: PCI STATUS = 0x%04x\n",
			sym_name(np), pci_sts & 0xf900);
	}
}


/*
 *  Requeue awaiting commands.
 */
static void sym_requeue_awaiting_cmds(hcb_p np)
{
	Scsi_Cmnd *cmd;
	ucmd_p ucp = SYM_UCMD_PTR(cmd);
	SYM_QUEHEAD tmp_cmdq;
	int sts;

	sym_que_move(&np->s.wait_cmdq, &tmp_cmdq);

	while ((ucp = (ucmd_p) sym_remque_head(&tmp_cmdq)) != 0) {
		sym_insque_tail(&ucp->link_cmdq, &np->s.busy_cmdq);
		cmd = SYM_SCMD_PTR(ucp);
		sts = sym_queue_command(np, cmd);
		if (sts) {
			sym_remque(&ucp->link_cmdq);
			sym_insque_head(&ucp->link_cmdq, &np->s.wait_cmdq);
		}
	}
}

/*
 *  Linux entry point of the queuecommand() function
 */
int sym53c8xx_queue_command (Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *))
{
	hcb_p  np  = SYM_SOFTC_PTR(cmd);
	ucmd_p ucp = SYM_UCMD_PTR(cmd);
	u_long flags;
	int sts = 0;

	cmd->scsi_done     = done;
	cmd->host_scribble = NULL;
	memset(ucp, 0, sizeof(*ucp));

	SYM_LOCK_HCB(np, flags);

	/*
	 *  Shorten our settle_time if needed for 
	 *  this command not to time out.
	 */
	if (np->s.settle_time_valid && cmd->timeout_per_command) {
		u_long tlimit = ktime_get(cmd->timeout_per_command);
		tlimit = ktime_sub(tlimit, SYM_CONF_TIMER_INTERVAL*2);
		if (ktime_dif(np->s.settle_time, tlimit) > 0) {
			np->s.settle_time = tlimit;
		}
	}

	if (np->s.settle_time_valid || !sym_que_empty(&np->s.wait_cmdq)) {
		sym_insque_tail(&ucp->link_cmdq, &np->s.wait_cmdq);
		goto out;
	}

	sym_insque_tail(&ucp->link_cmdq, &np->s.busy_cmdq);
	sts = sym_queue_command(np, cmd);
	if (sts) {
		sym_remque(&ucp->link_cmdq);
		sym_insque_tail(&ucp->link_cmdq, &np->s.wait_cmdq);
	}
out:
	SYM_UNLOCK_HCB(np, flags);

	return 0;
}

/*
 *  Linux entry point of the interrupt handler.
 */
static void sym53c8xx_intr(int irq, void *dev_id, struct pt_regs * regs)
{
	unsigned long flags;
	unsigned long flags1;
	hcb_p np = (hcb_p) dev_id;

	if (DEBUG_FLAGS & DEBUG_TINY) printf_debug ("[");

	SYM_LOCK_SCSI(np, flags1);
	SYM_LOCK_HCB(np, flags);

	sym_interrupt(np);

	if (!sym_que_empty(&np->s.wait_cmdq) && !np->s.settle_time_valid)
		sym_requeue_awaiting_cmds(np);

	SYM_UNLOCK_HCB(np, flags);
	SYM_UNLOCK_SCSI(np, flags1);

	if (DEBUG_FLAGS & DEBUG_TINY) printf_debug ("]\n");
}

/*
 *  Linux entry point of the timer handler
 */
static void sym53c8xx_timer(unsigned long npref)
{
	hcb_p np = (hcb_p) npref;
	unsigned long flags;
	unsigned long flags1;

	SYM_LOCK_SCSI(np, flags1);
	SYM_LOCK_HCB(np, flags);

	sym_timer(np);

	if (!sym_que_empty(&np->s.wait_cmdq) && !np->s.settle_time_valid)
		sym_requeue_awaiting_cmds(np);

	SYM_UNLOCK_HCB(np, flags);
	SYM_UNLOCK_SCSI(np, flags1);
}


/*
 *  What the eh thread wants us to perform.
 */
#define SYM_EH_ABORT		0
#define SYM_EH_DEVICE_RESET	1
#define SYM_EH_BUS_RESET	2
#define SYM_EH_HOST_RESET	3

/*
 *  What we will do regarding the involved SCSI command.
 */
#define SYM_EH_DO_IGNORE	0
#define SYM_EH_DO_COMPLETE	1
#define SYM_EH_DO_WAIT		2

/*
 *  Our general completion handler.
 */
static void __sym_eh_done(Scsi_Cmnd *cmd, int timed_out)
{
	struct sym_eh_wait *ep = SYM_UCMD_PTR(cmd)->eh_wait;
	if (!ep)
		return;

	/* Try to avoid a race here (not 100% safe) */
	if (!timed_out) {
		ep->timed_out = 0;
		if (ep->to_do == SYM_EH_DO_WAIT && !del_timer(&ep->timer))
			return;
	}

	/* Revert everything */
	SYM_UCMD_PTR(cmd)->eh_wait = 0;
	cmd->scsi_done = ep->old_done;

	/* Wake up the eh thread if it wants to sleep */
	if (ep->to_do == SYM_EH_DO_WAIT)
		up(&ep->sem);
}

/*
 *  scsi_done() alias when error recovery is in progress. 
 */
static void sym_eh_done(Scsi_Cmnd *cmd) { __sym_eh_done(cmd, 0); }

/*
 *  Some timeout handler to avoid waiting too long.
 */
static void sym_eh_timeout(u_long p) { __sym_eh_done((Scsi_Cmnd *)p, 1); }

/*
 *  Generic method for our eh processing.
 *  The 'op' argument tells what we have to do.
 */
static int sym_eh_handler(int op, char *opname, Scsi_Cmnd *cmd)
{
	hcb_p np = SYM_SOFTC_PTR(cmd);
	unsigned long flags;
	SYM_QUEHEAD *qp;
	int to_do = SYM_EH_DO_IGNORE;
	int sts = -1;
	struct sym_eh_wait eh, *ep = &eh;
	char devname[20];

	sprintf(devname, "%s:%d:%d", sym_name(np), cmd->target, cmd->lun);

	printf_warning("%s: %s operation started.\n", devname, opname);

	SYM_LOCK_HCB(np, flags);

#if 0
	/* This one should be the result of some race, thus to ignore */
	if (cmd->serial_number != cmd->serial_number_at_timeout)
		goto prepare;
#endif

	/* This one is not queued to the core driver -> to complete here */ 
	FOR_EACH_QUEUED_ELEMENT(&np->s.wait_cmdq, qp) {
		if (SYM_SCMD_PTR(qp) == cmd) {
			to_do = SYM_EH_DO_COMPLETE;
			goto prepare;
		}
	}

	/* This one is queued in some place -> to wait for completion */
	FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
		ccb_p cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		if (cp->cam_ccb == cmd) {
			to_do = SYM_EH_DO_WAIT;
			goto prepare;
		}
	}

prepare:
	/* Prepare stuff to either ignore, complete or wait for completion */
	switch(to_do) {
	default:
	case SYM_EH_DO_IGNORE:
		goto finish;
		break;
	case SYM_EH_DO_WAIT:
#if LINUX_VERSION_CODE > LinuxVersionCode(2,3,0)
		init_MUTEX_LOCKED(&ep->sem);
#else
		ep->sem = MUTEX_LOCKED;
#endif
		/* fall through */
	case SYM_EH_DO_COMPLETE:
		ep->old_done = cmd->scsi_done;
		cmd->scsi_done = sym_eh_done;
		SYM_UCMD_PTR(cmd)->eh_wait = ep;
	}

	/* Try to proceed the operation we have been asked for */
	sts = -1;
	switch(op) {
	case SYM_EH_ABORT:
		sts = sym_abort_scsiio(np, cmd, 1);
		break;
	case SYM_EH_DEVICE_RESET:
		sts = sym_reset_scsi_target(np, cmd->target);
		break;
	case SYM_EH_BUS_RESET:
		sym_reset_scsi_bus(np, 1);
		sts = 0;
		break;
	case SYM_EH_HOST_RESET:
		sym_reset_scsi_bus(np, 0);
		sym_start_up (np, 1);
		sts = 0;
		break;
	default:
		break;
	}

	/* On error, restore everything and cross fingers :) */
	if (sts) {
		SYM_UCMD_PTR(cmd)->eh_wait = 0;
		cmd->scsi_done = ep->old_done;
		to_do = SYM_EH_DO_IGNORE;
	}

finish:
	ep->to_do = to_do;
	/* Complete the command with locks held as required by the driver */
	if (to_do == SYM_EH_DO_COMPLETE)
		sym_xpt_done2(np, cmd, CAM_REQ_ABORTED);

	SYM_UNLOCK_HCB(np, flags);

	/* Wait for completion with locks released, as required by kernel */
	if (to_do == SYM_EH_DO_WAIT) {
		init_timer(&ep->timer);
		ep->timer.expires = jiffies + (5*HZ);
		ep->timer.function = sym_eh_timeout;
		ep->timer.data = (u_long)cmd;
		ep->timed_out = 1;	/* Be pessimistic for once :) */
		add_timer(&ep->timer);
		SYM_UNLOCK_SCSI_NORESTORE(np);
		down(&ep->sem);
		SYM_LOCK_SCSI_NOSAVE(np);
		if (ep->timed_out)
			sts = -2;
	}
	printf_warning("%s: %s operation %s.\n", devname, opname,
			sts==0?"complete":sts==-2?"timed-out":"failed");
	return sts? SCSI_FAILED : SCSI_SUCCESS;
}


/*
 * Error handlers called from the eh thread (one thread per HBA).
 */
int sym53c8xx_eh_abort_handler(Scsi_Cmnd *cmd)
{
	return sym_eh_handler(SYM_EH_ABORT, "ABORT", cmd);
}

int sym53c8xx_eh_device_reset_handler(Scsi_Cmnd *cmd)
{
	return sym_eh_handler(SYM_EH_DEVICE_RESET, "DEVICE RESET", cmd);
}

int sym53c8xx_eh_bus_reset_handler(Scsi_Cmnd *cmd)
{
	return sym_eh_handler(SYM_EH_BUS_RESET, "BUS RESET", cmd);
}

int sym53c8xx_eh_host_reset_handler(Scsi_Cmnd *cmd)
{
	return sym_eh_handler(SYM_EH_HOST_RESET, "HOST RESET", cmd);
}

/*
 *  Tune device queuing depth, according to various limits.
 */
static void 
sym_tune_dev_queuing(hcb_p np, int target, int lun, u_short reqtags)
{
	tcb_p	tp = &np->target[target];
	lcb_p	lp = sym_lp(np, tp, lun);
	u_short	oldtags;

	if (!lp)
		return;

	oldtags = lp->s.reqtags;

	if (reqtags > lp->s.scdev_depth)
		reqtags = lp->s.scdev_depth;

	lp->started_limit = reqtags ? reqtags : 2;
	lp->started_max   = 1;
	lp->s.reqtags     = reqtags;

	if (reqtags != oldtags) {
		printf_info("%s:%d:%d: "
		         "tagged command queuing %s, command queue depth %d.\n",
		          sym_name(np), target, lun,
		          lp->s.reqtags ? "enabled" : "disabled",
 		          lp->started_limit);
	}
}

#ifdef	SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT
/*
 *  Linux select queue depths function
 */
#define DEF_DEPTH	(sym_driver_setup.max_tag)
#define ALL_TARGETS	-2
#define NO_TARGET	-1
#define ALL_LUNS	-2
#define NO_LUN		-1

static int device_queue_depth(hcb_p np, int target, int lun)
{
	int c, h, t, u, v;
	char *p = sym_driver_setup.tag_ctrl;
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
			if (h == np->s.unit &&
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
#else
#define device_queue_depth(np, t, l)	(sym_driver_setup.max_tag)
#endif	/* SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT */

/*
 * Linux entry point for device queue sizing.
 */
static void 
sym53c8xx_select_queue_depths(struct Scsi_Host *host, 
                              struct scsi_device *devlist)
{
	struct scsi_device *device;

	for (device = devlist; device; device = device->next) {
		hcb_p np;
		tcb_p tp;
		lcb_p lp;
		int reqtags;

		if (device->host != host)
			continue;

		np = ((struct host_data *) host->hostdata)->ncb;
		tp = &np->target[device->id];

		/*
		 *  Get user settings for transfer parameters.
		 */
		tp->inq_byte7_valid = (INQ7_SYNC|INQ7_WIDE16);
		sym_update_trans_settings(np, tp);

		/*
		 *  Allocate the LCB if not yet.
		 *  If it fail, we may well be in the sh*t. :)
		 */
		lp = sym_alloc_lcb(np, device->id, device->lun);
		if (!lp) {
			device->queue_depth = 1;
			continue;
		}

		/*
		 *  Get user flags.
		 */
		lp->curr_flags = lp->user_flags;

		/*
		 *  Select queue depth from driver setup.
		 *  Donnot use more than configured by user.
		 *  Use at least 2.
		 *  Donnot use more than our maximum.
		 */
		reqtags = device_queue_depth(np, device->id, device->lun);
		if (reqtags > tp->usrtags)
			reqtags = tp->usrtags;
		if (!device->tagged_supported)
			reqtags = 0;
#if 1 /* Avoid to locally queue commands for no good reasons */
		if (reqtags > SYM_CONF_MAX_TAG)
			reqtags = SYM_CONF_MAX_TAG;
		device->queue_depth = reqtags ? reqtags : 2;
#else
		device->queue_depth = reqtags ? SYM_CONF_MAX_TAG : 2;
#endif
		lp->s.scdev_depth = device->queue_depth;
		sym_tune_dev_queuing(np, device->id, device->lun, reqtags);
	}
}

/*
 *  Linux entry point for info() function
 */
const char *sym53c8xx_info (struct Scsi_Host *host)
{
	return sym_driver_name();
}


#ifdef SYM_LINUX_PROC_INFO_SUPPORT
/*
 *  Proc file system stuff
 *
 *  A read operation returns adapter information.
 *  A write operation is a control command.
 *  The string is parsed in the driver code and the command is passed 
 *  to the sym_usercmd() function.
 */

#ifdef SYM_LINUX_USER_COMMAND_SUPPORT

struct	sym_usrcmd {
	u_long	target;
	u_long	lun;
	u_long	data;
	u_long	cmd;
};

#define UC_SETSYNC      10
#define UC_SETTAGS	11
#define UC_SETDEBUG	12
#define UC_SETWIDE	14
#define UC_SETFLAG	15
#define UC_SETVERBOSE	17
#define UC_RESETDEV	18
#define UC_CLEARDEV	19

static void sym_exec_user_command (hcb_p np, struct sym_usrcmd *uc)
{
	tcb_p tp;
	int t, l;

	switch (uc->cmd) {
	case 0: return;

#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
	case UC_SETDEBUG:
		sym_debug_flags = uc->data;
		break;
#endif
	case UC_SETVERBOSE:
		np->verbose = uc->data;
		break;
	default:
		/*
		 * We assume that other commands apply to targets.
		 * This should always be the case and avoid the below 
		 * 4 lines to be repeated 6 times.
		 */
		for (t = 0; t < SYM_CONF_MAX_TARGET; t++) {
			if (!((uc->target >> t) & 1))
				continue;
			tp = &np->target[t];

			switch (uc->cmd) {

			case UC_SETSYNC:
				if (!uc->data || uc->data >= 255) {
					tp->tinfo.goal.options = 0;
					tp->tinfo.goal.offset  = 0;
					break;
				}
				if (uc->data <= 9 && np->minsync_dt) {
					if (uc->data < np->minsync_dt)
						uc->data = np->minsync_dt;
					tp->tinfo.goal.options = PPR_OPT_DT;
					tp->tinfo.goal.width   = 1;
					tp->tinfo.goal.period = uc->data;
					tp->tinfo.goal.offset = np->maxoffs_dt;
				}
				else {
					if (uc->data < np->minsync)
						uc->data = np->minsync;
					tp->tinfo.goal.options = 0;
					tp->tinfo.goal.period = uc->data;
					tp->tinfo.goal.offset = np->maxoffs;
				}
				break;
			case UC_SETWIDE:
				tp->tinfo.goal.width = uc->data ? 1 : 0;
				break;
			case UC_SETTAGS:
				for (l = 0; l < SYM_CONF_MAX_LUN; l++)
					sym_tune_dev_queuing(np, t,l, uc->data);
				break;
			case UC_RESETDEV:
				tp->to_reset = 1;
				np->istat_sem = SEM;
				OUTB (nc_istat, SIGP|SEM);
				break;
			case UC_CLEARDEV:
				for (l = 0; l < SYM_CONF_MAX_LUN; l++) {
					lcb_p lp = sym_lp(np, tp, l);
					if (lp) lp->to_clear = 1;
				}
				np->istat_sem = SEM;
				OUTB (nc_istat, SIGP|SEM);
				break;
			case UC_SETFLAG:
				tp->usrflags = uc->data;
				break;
			}
		}
		break;
	}
}

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
 * Parse a control command
 */

static int sym_user_command(hcb_p np, char *buffer, int length)
{
	char *ptr	= buffer;
	int len		= length;
	struct sym_usrcmd cmd, *uc = &cmd;
	int		arg_len;
	u_long 		target;

	bzero(uc, sizeof(*uc));

	if (len > 0 && ptr[len-1] == '\n')
		--len;

	if	((arg_len = is_keyword(ptr, len, "setsync")) != 0)
		uc->cmd = UC_SETSYNC;
	else if	((arg_len = is_keyword(ptr, len, "settags")) != 0)
		uc->cmd = UC_SETTAGS;
	else if	((arg_len = is_keyword(ptr, len, "setverbose")) != 0)
		uc->cmd = UC_SETVERBOSE;
	else if	((arg_len = is_keyword(ptr, len, "setwide")) != 0)
		uc->cmd = UC_SETWIDE;
#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
	else if	((arg_len = is_keyword(ptr, len, "setdebug")) != 0)
		uc->cmd = UC_SETDEBUG;
#endif
	else if	((arg_len = is_keyword(ptr, len, "setflag")) != 0)
		uc->cmd = UC_SETFLAG;
	else if	((arg_len = is_keyword(ptr, len, "resetdev")) != 0)
		uc->cmd = UC_RESETDEV;
	else if	((arg_len = is_keyword(ptr, len, "cleardev")) != 0)
		uc->cmd = UC_CLEARDEV;
	else
		arg_len = 0;

#ifdef DEBUG_PROC_INFO
printk("sym_user_command: arg_len=%d, cmd=%ld\n", arg_len, uc->cmd);
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
printk("sym_user_command: target=%ld\n", target);
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
printk("sym_user_command: data=%ld\n", uc->data);
#endif
		break;
#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
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
			else if	((arg_len = is_keyword(ptr, len, "scatter")))
				uc->data |= DEBUG_SCATTER;
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
			else if	((arg_len = is_keyword(ptr, len, "pointer")))
				uc->data |= DEBUG_POINTER;
			else
				return -EINVAL;
			ptr += arg_len; len -= arg_len;
		}
#ifdef DEBUG_PROC_INFO
printk("sym_user_command: data=%ld\n", uc->data);
#endif
		break;
#endif /* SYM_LINUX_DEBUG_CONTROL_SUPPORT */
	case UC_SETFLAG:
		while (len > 0) {
			SKIP_SPACES(1);
			if	((arg_len = is_keyword(ptr, len, "no_disc")))
				uc->data &= ~SYM_DISC_ENABLED;
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
		long flags;

		SYM_LOCK_HCB(np, flags);
		sym_exec_user_command (np, uc);
		SYM_UNLOCK_HCB(np, flags);
	}
	return length;
}

#endif	/* SYM_LINUX_USER_COMMAND_SUPPORT */


#ifdef SYM_LINUX_USER_INFO_SUPPORT
/*
 *  Informations through the proc file system.
 */
struct info_str {
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
 *  Copy formatted information into the input buffer.
 */
static int sym_host_info(hcb_p np, char *ptr, off_t offset, int len)
{
	struct info_str info;

	info.buffer	= ptr;
	info.length	= len;
	info.offset	= offset;
	info.pos	= 0;

	copy_info(&info, "Chip " NAME53C "%s, device id 0x%x, "
			 "revision id 0x%x\n",
			 np->s.chip_name, np->device_id, np->revision_id);
	copy_info(&info, "On PCI bus %d, device %d, function %d, "
#ifdef __sparc__
		"IRQ %s\n",
#else
		"IRQ %d\n",
#endif
		np->s.bus, (np->s.device_fn & 0xf8) >> 3, np->s.device_fn & 7,
#ifdef __sparc__
		__irq_itoa(np->s.irq));
#else
		(int) np->s.irq);
#endif
	copy_info(&info, "Min. period factor %d, %s SCSI BUS%s\n",
			 (int) (np->minsync_dt ? np->minsync_dt : np->minsync),
			 np->maxwide ? "Wide" : "Narrow",
			 np->minsync_dt ? ", DT capable" : "");

	copy_info(&info, "Max. started commands %d, "
			 "max. commands per LUN %d\n",
			 SYM_CONF_MAX_START, SYM_CONF_MAX_TAG);

	return info.pos > info.offset? info.pos - info.offset : 0;
}
#endif /* SYM_LINUX_USER_INFO_SUPPORT */

/*
 *  Entry point of the scsi proc fs of the driver.
 *  - func = 0 means read  (returns adapter infos)
 *  - func = 1 means write (not yet merget from sym53c8xx)
 */
static int sym53c8xx_proc_info(char *buffer, char **start, off_t offset,
			int length, int hostno, int func)
{
	struct Scsi_Host *host;
	struct host_data *host_data;
	hcb_p np = 0;
	int retv;

	for (host = first_host; host; host = host->next) {
		if (host->hostt != first_host->hostt)
			continue;
		if (host->host_no == hostno) {
			host_data = (struct host_data *) host->hostdata;
			np = host_data->ncb;
			break;
		}
	}

	if (!np)
		return -EINVAL;

	if (func) {
#ifdef	SYM_LINUX_USER_COMMAND_SUPPORT
		retv = sym_user_command(np, buffer, length);
#else
		retv = -EINVAL;
#endif
	}
	else {
		if (start)
			*start = buffer;
#ifdef SYM_LINUX_USER_INFO_SUPPORT
		retv = sym_host_info(np, buffer, offset, length);
#else
		retv = -EINVAL;
#endif
	}

	return retv;
}
#endif /* SYM_LINUX_PROC_INFO_SUPPORT */

/*
 *	Free controller resources.
 */
static void sym_free_resources(hcb_p np)
{
	/*
	 *  Free O/S specific resources.
	 */
	if (np->s.irq)
		free_irq(np->s.irq, np);
	if (np->s.io_port)
		release_region(np->s.io_port, np->s.io_ws);
#ifndef SYM_OPT_NO_BUS_MEMORY_MAPPING
	if (np->s.mmio_va)
		pci_unmap_mem(np->s.mmio_va, np->s.io_ws);
	if (np->s.ram_va)
		pci_unmap_mem(np->s.ram_va, np->ram_ws);
#endif
	/*
	 *  Free O/S independant resources.
	 */
	sym_hcb_free(np);

	sym_mfree_dma(np, sizeof(*np), "HCB");
}

/*
 *  Ask/tell the system about DMA addressing.
 */
#ifdef SYM_LINUX_DYNAMIC_DMA_MAPPING
static int sym_setup_bus_dma_mask(hcb_p np)
{
#if LINUX_VERSION_CODE < LinuxVersionCode(2,4,3)
	if (!pci_dma_supported(np->s.device, 0xffffffffUL))
		goto out_err32;
#else
#if   SYM_CONF_DMA_ADDRESSING_MODE == 0
	if (pci_set_dma_mask(np->s.device, 0xffffffffUL))
		goto out_err32;
#else
#if   SYM_CONF_DMA_ADDRESSING_MODE == 1
#define	PciDmaMask	0xffffffffff
#elif SYM_CONF_DMA_ADDRESSING_MODE == 2
#define	PciDmaMask	0xffffffffffffffff
#endif
	if (np->features & FE_DAC) {
		if (!pci_set_dma_mask(np->s.device, PciDmaMask)) {
			np->use_dac = 1;
			printf_info("%s: using 64 bit DMA addressing\n",
					sym_name(np));
		}
		else {
			if (pci_set_dma_mask(np->s.device, 0xffffffffUL))
				goto out_err32;
		}
	}
#undef	PciDmaMask
#endif
#endif
	return 0;

out_err32:
	printf_warning("%s: 32 BIT DMA ADDRESSING NOT SUPPORTED\n",
			sym_name(np));
	return -1;
}
#endif /* SYM_LINUX_DYNAMIC_DMA_MAPPING */

/*
 *  Host attach and initialisations.
 *
 *  Allocate host data and ncb structure.
 *  Request IO region and remap MMIO region.
 *  Do chip initialization.
 *  If all is OK, install interrupt handling and
 *  start the timer daemon.
 */
static int __init 
sym_attach (Scsi_Host_Template *tpnt, int unit, sym_device *dev)
{
        struct host_data *host_data;
	hcb_p np = 0;
        struct Scsi_Host *instance = 0;
	u_long flags = 0;
	sym_nvram *nvram = dev->nvram;
	struct sym_fw *fw;

	printk(KERN_INFO
		"sym%d: <%s> rev 0x%x on pci bus %d device %d function %d "
#ifdef __sparc__
		"irq %s\n",
#else
		"irq %d\n",
#endif
		unit, dev->chip.name, dev->chip.revision_id,
		dev->s.bus, (dev->s.device_fn & 0xf8) >> 3,
		dev->s.device_fn & 7,
#ifdef __sparc__
		__irq_itoa(dev->s.irq));
#else
		dev->s.irq);
#endif

	/*
	 *  Get the firmware for this chip.
	 */
	fw = sym_find_firmware(&dev->chip);
	if (!fw)
		goto attach_failed;

	/*
	 *	Allocate host_data structure
	 */
        if (!(instance = scsi_register(tpnt, sizeof(*host_data))))
	        goto attach_failed;
	host_data = (struct host_data *) instance->hostdata;

	/*
	 *  Allocate immediately the host control block, 
	 *  since we are only expecting to succeed. :)
	 *  We keep track in the HCB of all the resources that 
	 *  are to be released on error.
	 */
#ifdef	SYM_LINUX_DYNAMIC_DMA_MAPPING
	np = __sym_calloc_dma(dev->pdev, sizeof(*np), "HCB");
	if (np) {
		np->s.device = dev->pdev;
		np->bus_dmat = dev->pdev; /* Result in 1 DMA pool per HBA */
	}
	else
		goto attach_failed;
#else
	np = sym_calloc_dma(sizeof(*np), "HCB");
	if (!np)
		goto attach_failed;
#endif
	host_data->ncb = np;

	SYM_INIT_LOCK_HCB(np);

	/*
	 *  Copy some useful infos to the HCB.
	 */
	np->hcb_ba	= vtobus(np);
	np->verbose	= sym_driver_setup.verbose;
	np->s.device	= dev->pdev;
	np->s.unit	= unit;
	np->device_id	= dev->chip.device_id;
	np->revision_id	= dev->chip.revision_id;
	np->s.bus	= dev->s.bus;
	np->s.device_fn	= dev->s.device_fn;
	np->features	= dev->chip.features;
	np->clock_divn	= dev->chip.nr_divisor;
	np->maxoffs	= dev->chip.offset_max;
	np->maxburst	= dev->chip.burst_max;
	np->myaddr	= dev->host_id;

	/*
	 *  Edit its name.
	 */
	strncpy(np->s.chip_name, dev->chip.name, sizeof(np->s.chip_name)-1);
	sprintf(np->s.inst_name, "sym%d", np->s.unit);

	/*
	 *  Ask/tell the system about DMA addressing.
	 */
#ifdef SYM_LINUX_DYNAMIC_DMA_MAPPING
	if (sym_setup_bus_dma_mask(np))
		goto attach_failed;
#endif

	/*
	 *  Try to map the controller chip to
	 *  virtual and physical memory.
	 */
	np->mmio_ba	= (u32)dev->s.base;
	np->s.io_ws	= (np->features & FE_IO256)? 256 : 128;

#ifndef SYM_CONF_IOMAPPED
	np->s.mmio_va = pci_map_mem(dev->s.base_c, np->s.io_ws);
	if (!np->s.mmio_va) {
		printf_err("%s: can't map PCI MMIO region\n", sym_name(np));
		goto attach_failed;
	}
	else if (sym_verbose > 1)
		printf_info("%s: using memory mapped IO\n", sym_name(np));
#endif /* !defined SYM_CONF_IOMAPPED */

	/*
	 *  Try to map the controller chip into iospace.
	 */
	if (dev->s.io_port) {
		request_region(dev->s.io_port, np->s.io_ws, NAME53C8XX);
		np->s.io_port = dev->s.io_port;
	}

	/*
	 *  Map on-chip RAM if present and supported.
	 */
	if (!(np->features & FE_RAM))
		dev->s.base_2 = 0;
	if (dev->s.base_2) {
		np->ram_ba = (u32)dev->s.base_2;
		if (np->features & FE_RAM8K)
			np->ram_ws = 8192;
		else
			np->ram_ws = 4096;
#ifndef SYM_OPT_NO_BUS_MEMORY_MAPPING
		np->s.ram_va = pci_map_mem(dev->s.base_2_c, np->ram_ws);
		if (!np->s.ram_va) {
			printf_err("%s: can't map PCI MEMORY region\n",
			       sym_name(np));
			goto attach_failed;
		}
#endif
	}

	/*
	 *  Perform O/S independant stuff.
	 */
	if (sym_hcb_attach(np, fw, nvram))
		goto attach_failed;


	/*
	 *  Install the interrupt handler.
	 *  If we synchonize the C code with SCRIPTS on interrupt, 
	 *  we donnot want to share the INTR line at all.
	 */
	if (request_irq(dev->s.irq, sym53c8xx_intr, SA_SHIRQ,
			NAME53C8XX, np)) {
		printf_err("%s: request irq %d failure\n",
			sym_name(np), dev->s.irq);
		goto attach_failed;
	}
	np->s.irq = dev->s.irq;

	/*
	 *  After SCSI devices have been opened, we cannot
	 *  reset the bus safely, so we do it here.
	 */
	SYM_LOCK_HCB(np, flags);
	if (sym_reset_scsi_bus(np, 0)) {
		printf_err("%s: FATAL ERROR: CHECK SCSI BUS - CABLES, "
		           "TERMINATION, DEVICE POWER etc.!\n", sym_name(np));
		SYM_UNLOCK_HCB(np, flags);
		goto attach_failed;
	}

	/*
	 *  Initialize some queue headers.
	 */
	sym_que_init(&np->s.wait_cmdq);
	sym_que_init(&np->s.busy_cmdq);

	/*
	 *  Start the SCRIPTS.
	 */
	sym_start_up (np, 1);

	/*
	 *  Start the timer daemon
	 */
	init_timer(&np->s.timer);
	np->s.timer.data     = (unsigned long) np;
	np->s.timer.function = sym53c8xx_timer;
	np->s.lasttime=0;
	sym_timer (np);

	/*
	 *  Done.
	 */
        if (!first_host)
        	first_host = instance;

	/*
	 *  Fill Linux host instance structure
	 *  and return success.
	 */
	instance->max_channel	= 0;
	instance->this_id	= np->myaddr;
	instance->max_id	= np->maxwide ? 16 : 8;
	instance->max_lun	= SYM_CONF_MAX_LUN;
#ifndef SYM_CONF_IOMAPPED
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,29)
	instance->base		= (unsigned long) np->s.mmio_va;
#else
	instance->base		= (char *) np->s.mmio_va;
#endif
#endif
	instance->irq		= np->s.irq;
	instance->unique_id	= np->s.io_port;
	instance->io_port	= np->s.io_port;
	instance->n_io_port	= np->s.io_ws;
	instance->dma_channel	= 0;
	instance->cmd_per_lun	= SYM_CONF_MAX_TAG;
	instance->can_queue	= (SYM_CONF_MAX_START-2);
	instance->sg_tablesize	= SYM_CONF_MAX_SG;
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0)
	instance->max_cmd_len	= 16;
#endif
	instance->select_queue_depths = sym53c8xx_select_queue_depths;
	instance->highmem_io	= 1;

	SYM_UNLOCK_HCB(np, flags);

	scsi_set_pci_device(instance, dev->pdev);

	/*
	 *  Now let the generic SCSI driver
	 *  look for the SCSI devices on the bus ..
	 */
	return 0;

attach_failed:
	if (!instance) return -1;
	printf_info("%s: giving up ...\n", sym_name(np));
	if (np)
		sym_free_resources(np);
	scsi_unregister(instance);

        return -1;
 }


/*
 *    Detect and try to read SYMBIOS and TEKRAM NVRAM.
 */
#if SYM_CONF_NVRAM_SUPPORT
static void __init sym_get_nvram(sym_device *devp, sym_nvram *nvp)
{
	if (!nvp)
		return;

	devp->nvram = nvp;
	devp->device_id = devp->chip.device_id;
	nvp->type = 0;

	/*
	 *  Get access to chip IO registers
	 */
#ifdef SYM_CONF_IOMAPPED
	request_region(devp->s.io_port, 128, NAME53C8XX);
#else
	devp->s.mmio_va = pci_map_mem(devp->s.base_c, 128);
	if (!devp->s.mmio_va)
		return;
#endif

	/*
	 *  Try to read SYMBIOS|TEKRAM nvram.
	 */
	(void) sym_read_nvram(devp, nvp);

	/*
	 *  Release access to chip IO registers
	 */
#ifdef SYM_CONF_IOMAPPED
	release_region(devp->s.io_port, 128);
#else
	pci_unmap_mem((u_long) devp->s.mmio_va, 128ul);
#endif
}
#endif	/* SYM_CONF_NVRAM_SUPPORT */

/*
 *  Driver setup from the boot command line
 */
#ifdef	SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT

static struct sym_driver_setup
	sym_driver_safe_setup __initdata = SYM_LINUX_DRIVER_SAFE_SETUP;
#ifdef	MODULE
char *sym53c8xx = 0;	/* command line passed by insmod */
MODULE_PARM(sym53c8xx, "s");
#endif

static void __init sym53c8xx_print_driver_setup(void)
{
	printf_info (NAME53C8XX ": setup="
		"mpar:%d,spar:%d,tags:%d,sync:%d,burst:%d,"
		"led:%d,wide:%d,diff:%d,irqm:%d, buschk:%d\n",
		sym_driver_setup.pci_parity,
		sym_driver_setup.scsi_parity,
		sym_driver_setup.max_tag,
		sym_driver_setup.min_sync,
		sym_driver_setup.burst_order,
		sym_driver_setup.scsi_led,
		sym_driver_setup.max_wide,
		sym_driver_setup.scsi_diff,
		sym_driver_setup.irq_mode,
		sym_driver_setup.scsi_bus_check);
	printf_info (NAME53C8XX ": setup="
		"hostid:%d,offs:%d,luns:%d,pcifix:%d,revprob:%d,"
		"verb:%d,debug:0x%x,setlle_delay:%d\n",
		sym_driver_setup.host_id,
		sym_driver_setup.max_offs,
		sym_driver_setup.max_lun,
		sym_driver_setup.pci_fix_up,
		sym_driver_setup.reverse_probe,
		sym_driver_setup.verbose,
		sym_driver_setup.debug,
		sym_driver_setup.settle_delay);
#ifdef DEBUG_2_0_X
MDELAY(5000);
#endif
};

#define OPT_PCI_PARITY		1
#define	OPT_SCSI_PARITY		2
#define OPT_MAX_TAG		3
#define OPT_MIN_SYNC		4
#define OPT_BURST_ORDER		5
#define OPT_SCSI_LED		6
#define OPT_MAX_WIDE		7
#define OPT_SCSI_DIFF		8
#define OPT_IRQ_MODE		9
#define OPT_SCSI_BUS_CHECK	10
#define	OPT_HOST_ID		11
#define OPT_MAX_OFFS		12
#define OPT_MAX_LUN		13
#define OPT_PCI_FIX_UP		14

#define OPT_REVERSE_PROBE	15
#define OPT_VERBOSE		16
#define OPT_DEBUG		17
#define OPT_SETTLE_DELAY	18
#define OPT_USE_NVRAM		19
#define OPT_EXCLUDE		20
#define OPT_SAFE_SETUP		21

static char setup_token[] __initdata =
	"mpar:"		"spar:"
	"tags:"		"sync:"
	"burst:"	"led:"
	"wide:"		"diff:"
	"irqm:"		"buschk:"
	"hostid:"	"offset:"
	"luns:"		"pcifix:"
	"revprob:"	"verb:"
	"debug:"	"settle:"
	"nvram:"	"excl:"
	"safe:"
	;

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
#endif	/* SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT */

int __init sym53c8xx_setup(char *str)
{
#ifdef	SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT
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
		case OPT_MAX_TAG:
			sym_driver_setup.max_tag = val;
			if (!(pe && *pe == '/'))
				break;
			i = 0;
			while (*pe && *pe != ARG_SEP && 
				i < sizeof(sym_driver_setup.tag_ctrl)-1) {
				sym_driver_setup.tag_ctrl[i++] = *pe++;
			}
			sym_driver_setup.tag_ctrl[i] = '\0';
			break;
		case OPT_SAFE_SETUP:
			memcpy(&sym_driver_setup, &sym_driver_safe_setup,
				sizeof(sym_driver_setup));
			break;
		case OPT_EXCLUDE:
			if (xi < 8)
				sym_driver_setup.excludes[xi++] = val;
			break;

#define __SIMPLE_OPTION(NAME, name) \
		case OPT_ ## NAME :		\
			sym_driver_setup.name = val;\
			break;

		__SIMPLE_OPTION(PCI_PARITY, pci_parity)
		__SIMPLE_OPTION(SCSI_PARITY, scsi_parity)
		__SIMPLE_OPTION(MIN_SYNC, min_sync)
		__SIMPLE_OPTION(BURST_ORDER, burst_order)
		__SIMPLE_OPTION(SCSI_LED, scsi_led)
		__SIMPLE_OPTION(MAX_WIDE, max_wide)
		__SIMPLE_OPTION(SCSI_DIFF, scsi_diff)
		__SIMPLE_OPTION(IRQ_MODE, irq_mode)
		__SIMPLE_OPTION(SCSI_BUS_CHECK, scsi_bus_check)
		__SIMPLE_OPTION(HOST_ID, host_id)
		__SIMPLE_OPTION(MAX_OFFS, max_offs)
		__SIMPLE_OPTION(MAX_LUN, max_lun)
		__SIMPLE_OPTION(PCI_FIX_UP, pci_fix_up)
		__SIMPLE_OPTION(REVERSE_PROBE, reverse_probe)
		__SIMPLE_OPTION(VERBOSE, verbose)
		__SIMPLE_OPTION(DEBUG, debug)
		__SIMPLE_OPTION(SETTLE_DELAY, settle_delay)
		__SIMPLE_OPTION(USE_NVRAM, use_nvram)

#undef __SIMPLE_OPTION

		default:
			printk("sym53c8xx_setup: unexpected boot option '%.*s' ignored\n", (int)(pc-cur+1), cur);
			break;
		}

		if ((cur = strchr(cur, ARG_SEP)) != NULL)
			++cur;
	}
#endif	/* SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT */
	return 1;
}

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,13)
#ifndef MODULE
__setup("sym53c8xx=", sym53c8xx_setup);
#endif
#endif

#ifdef	SYM_CONF_PQS_PDS_SUPPORT
/*
 *  Detect all NCR PQS/PDS boards and keep track of their bus nr.
 *
 *  The NCR PQS or PDS card is constructed as a DEC bridge
 *  behind which sit a proprietary NCR memory controller and
 *  four or two 53c875s as separate devices.  In its usual mode
 *  of operation, the 875s are slaved to the memory controller
 *  for all transfers.  We can tell if an 875 is part of a
 *  PQS/PDS or not since if it is, it will be on the same bus
 *  as the memory controller.  To operate with the Linux
 *  driver, the memory controller is disabled and the 875s
 *  freed to function independently.  The only wrinkle is that
 *  the preset SCSI ID (which may be zero) must be read in from
 *  a special configuration space register of the 875
 */
#ifndef SYM_CONF_MAX_PQS_BUS
#define SYM_CONF_MAX_PQS_BUS 16
#endif
static int pqs_bus[SYM_CONF_MAX_PQS_BUS] __initdata = { 0 };

static void __init sym_detect_pqs_pds(void)
{
	short index;
	pcidev_t dev = PCIDEV_NULL;

	for(index=0; index < SYM_CONF_MAX_PQS_BUS; index++) {
		u_char tmp;

		dev = pci_find_device(0x101a, 0x0009, dev);
		if (dev == PCIDEV_NULL) {
			pqs_bus[index] = -1;
			break;
		}
		printf_info(NAME53C8XX ": NCR PQS/PDS memory controller detected on bus %d\n", PciBusNumber(dev));
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
#endif /* SYM_CONF_PQS_PDS_SUPPORT */

/*
 *  Read and check the PCI configuration for any detected NCR 
 *  boards and save data for attaching after all boards have 
 *  been detected.
 */
static int __init
sym53c8xx_pci_init(Scsi_Host_Template *tpnt, pcidev_t pdev, sym_device *device)
{
	u_short vendor_id, device_id, command, status_reg;
	u_char cache_line_size;
	u_char suggested_cache_line_size = 0;
	u_char pci_fix_up = SYM_SETUP_PCI_FIX_UP;
	u_char revision;
	u_int irq;
	u_long base, base_2, base_io; 
	u_long base_c, base_2_c, io_port; 
	int i;
	sym_chip *chip;

	/* Choose some short name for this device */
	sprintf(device->s.inst_name, "sym.%d.%d.%d",
		PciBusNumber(pdev),
		(int) (PciDeviceFn(pdev) & 0xf8) >> 3,
		(int) (PciDeviceFn(pdev) & 7));

	/*
	 *  Read needed minimal info from the PCI config space.
	 */
	vendor_id = PciVendorId(pdev);
	device_id = PciDeviceId(pdev);
	irq	  = PciIrqLine(pdev);

	i = pci_get_base_address(pdev, 0, &base_io);
	io_port = pci_get_base_cookie(pdev, 0);

	base_c = pci_get_base_cookie(pdev, i);
	i = pci_get_base_address(pdev, i, &base);

	base_2_c = pci_get_base_cookie(pdev, i);
	(void) pci_get_base_address(pdev, i, &base_2);

	io_port &= PCI_BASE_ADDRESS_IO_MASK;
	base	&= PCI_BASE_ADDRESS_MEM_MASK;
	base_2	&= PCI_BASE_ADDRESS_MEM_MASK;

	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &revision);

	/*
	 *  If user excluded this chip, donnot initialize it.
	 */
	if (base_io) {
		for (i = 0 ; i < 8 ; i++) {
			if (sym_driver_setup.excludes[i] == base_io)
				return -1;
		}
	}

	/*
	 *  Leave here if another driver attached the chip.
	 */
	if (io_port && check_region (io_port, 128)) {
		printf_info("%s: IO region 0x%lx[0..127] is in use\n",
		            sym_name(device), (long) io_port);
		return -1;
	}

	/*
	 *  Check if the chip is supported.
	 */
	chip = sym_lookup_pci_chip_table(device_id, revision);
	if (!chip) {
		printf_info("%s: device not supported\n", sym_name(device));
		return -1;
	}

	/*
	 *  Check if the chip has been assigned resources we need.
	 */
#ifdef SYM_CONF_IOMAPPED
	if (!io_port) {
		printf_info("%s: IO base address disabled.\n",
		            sym_name(device));
		return -1;
	}
#else
	if (!base) {
		printf_info("%s: MMIO base address disabled.\n",
		            sym_name(device));
		return -1;
	}
#endif

	/*
	 *  Ignore Symbios chips controlled by various RAID controllers.
	 *  These controllers set value 0x52414944 at RAM end - 16.
	 */
#if defined(__i386__) && !defined(SYM_OPT_NO_BUS_MEMORY_MAPPING)
	if (base_2_c) {
		unsigned int ram_size, ram_val;
		u_long ram_ptr;

		if (chip->features & FE_RAM8K)
			ram_size = 8192;
		else
			ram_size = 4096;

		ram_ptr = pci_map_mem(base_2_c, ram_size);
		if (ram_ptr) {
			ram_val = readl_raw(ram_ptr + ram_size - 16);
			pci_unmap_mem(ram_ptr, ram_size);
			if (ram_val == 0x52414944) {
				printf_info("%s: not initializing, "
				            "driven by RAID controller.\n",
				            sym_name(device));
				return -1;
			}
		}
	}
#endif /* i386 and PCI MEMORY accessible */

	/*
	 *  Copy the chip description to our device structure, 
	 *  so we can make it match the actual device and options.
	 */
	bcopy(chip, &device->chip, sizeof(device->chip));
	device->chip.revision_id = revision;

	/*
	 *  Read additionnal info from the configuration space.
	 */
	pci_read_config_word(pdev, PCI_COMMAND,		&command);
	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE,	&cache_line_size);

	/*
	 * Enable missing capabilities in the PCI COMMAND register.
	 */
#ifdef SYM_CONF_IOMAPPED
#define	PCI_COMMAND_BITS_TO_ENABLE (PCI_COMMAND_IO | \
	PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_PARITY)
#else
#define	PCI_COMMAND_BITS_TO_ENABLE \
	(PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_PARITY)
#endif
	if ((command & PCI_COMMAND_BITS_TO_ENABLE)
		    != PCI_COMMAND_BITS_TO_ENABLE) {
		printf_info("%s: setting%s%s%s%s...\n", sym_name(device),
		(command & PCI_COMMAND_IO)     ? "" : " PCI_COMMAND_IO",
		(command & PCI_COMMAND_MEMORY) ? "" : " PCI_COMMAND_MEMORY",
		(command & PCI_COMMAND_MASTER) ? "" : " PCI_COMMAND_MASTER",
		(command & PCI_COMMAND_PARITY) ? "" : " PCI_COMMAND_PARITY");
		command |= PCI_COMMAND_BITS_TO_ENABLE;
		pci_write_config_word(pdev, PCI_COMMAND, command);
	}
#undef	PCI_COMMAND_BITS_TO_ENABLE

	/*
	 *  If cache line size is not configured, suggest
	 *  a value for well known CPUs.
	 */
#if defined(__i386__) && !defined(MODULE)
	if (!cache_line_size && boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) {
		switch(boot_cpu_data.x86) {
		case 4:	suggested_cache_line_size = 4;   break;
		case 6: if (boot_cpu_data.x86_model > 8) break;
		case 5:	suggested_cache_line_size = 8;   break;
		}
	}
#endif	/* __i386__ */

	/*
	 *  Some features are required to be enabled in order to 
	 *  work around some chip problems. :) ;)
	 *  (ITEM 12 of a DEL about the 896 I haven't yet).
	 *  We must ensure the chip will use WRITE AND INVALIDATE.
	 *  The revision number limit is for now arbitrary.
	 */
	if (device_id == PCI_DEVICE_ID_NCR_53C896 && revision < 0x4) {
		chip->features	|= (FE_WRIE | FE_CLSE);
		pci_fix_up	|=  3;	/* Force appropriate PCI fix-up */
	}

#ifdef	SYM_CONF_PCI_FIX_UP
	/*
	 *  Try to fix up PCI config according to wished features.
	 */
	if ((pci_fix_up & 1) && (chip->features & FE_CLSE) && 
	    !cache_line_size && suggested_cache_line_size) {
		cache_line_size = suggested_cache_line_size;
		pci_write_config_byte(pdev,
				      PCI_CACHE_LINE_SIZE, cache_line_size);
		printf_info("%s: PCI_CACHE_LINE_SIZE set to %d.\n",
		            sym_name(device), cache_line_size);
	}

	if ((pci_fix_up & 2) && cache_line_size &&
	    (chip->features & FE_WRIE) && !(command & PCI_COMMAND_INVALIDATE)) {
		printf_info("%s: setting PCI_COMMAND_INVALIDATE.\n",
		            sym_name(device));
		command |= PCI_COMMAND_INVALIDATE;
		pci_write_config_word(pdev, PCI_COMMAND, command);
	}
#endif	/* SYM_CONF_PCI_FIX_UP */

	/*
	 *  Work around for errant bit in 895A. The 66Mhz
	 *  capable bit is set erroneously. Clear this bit.
	 *  (Item 1 DEL 533)
	 *
	 *  Make sure Config space and Features agree.
	 *
	 *  Recall: writes are not normal to status register -
	 *  write a 1 to clear and a 0 to leave unchanged.
	 *  Can only reset bits.
	 */
	pci_read_config_word(pdev, PCI_STATUS, &status_reg);
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
	 *  Initialise device structure with items required by sym_attach.
	 */
	device->pdev		= pdev;
	device->s.bus		= PciBusNumber(pdev);
	device->s.device_fn	= PciDeviceFn(pdev);
	device->s.base		= base;
	device->s.base_2	= base_2;
	device->s.base_c	= base_c;
	device->s.base_2_c	= base_2_c;
	device->s.io_port	= io_port;
	device->s.irq		= irq;
	device->attach_done	= 0;

	return 0;
}

/*
 *  List of supported NCR chip ids
 */
static u_short sym_chip_ids[] __initdata	= {
	PCI_ID_SYM53C810,
	PCI_ID_SYM53C815,
	PCI_ID_SYM53C825,
	PCI_ID_SYM53C860,
	PCI_ID_SYM53C875,
	PCI_ID_SYM53C875_2,
	PCI_ID_SYM53C885,
	PCI_ID_SYM53C875A,
	PCI_ID_SYM53C895,
	PCI_ID_SYM53C896,
	PCI_ID_SYM53C895A,
	PCI_ID_LSI53C1510D,
 	PCI_ID_LSI53C1010,
 	PCI_ID_LSI53C1010_2
};

/*
 *  Detect all 53c8xx hosts and then attach them.
 *
 *  If we are using NVRAM, once all hosts are detected, we need to 
 *  check any NVRAM for boot order in case detect and boot order 
 *  differ and attach them using the order in the NVRAM.
 *
 *  If no NVRAM is found or data appears invalid attach boards in 
 *  the order they are detected.
 */
int __init sym53c8xx_detect(Scsi_Host_Template *tpnt)
{
	pcidev_t pcidev;
	int i, j, chips, hosts, count;
	int attach_count = 0;
	sym_device *devtbl, *devp;
	sym_nvram  nvram;
#if SYM_CONF_NVRAM_SUPPORT
	sym_nvram  nvram0, *nvp;
#endif

	/*
	 *  PCI is required.
	 */
	if (!pci_present())
		return 0;

	/*
	 *    Initialize driver general stuff.
	 */
#ifdef SYM_LINUX_PROC_INFO_SUPPORT
#if LINUX_VERSION_CODE < LinuxVersionCode(2,3,27)
     tpnt->proc_dir  = &proc_scsi_sym53c8xx;
#else
     tpnt->proc_name = NAME53C8XX;
#endif
     tpnt->proc_info = sym53c8xx_proc_info;
#endif

#ifdef SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT
#ifdef MODULE
if (sym53c8xx)
	sym53c8xx_setup(sym53c8xx);
#endif
#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
	sym_debug_flags = sym_driver_setup.debug;
#endif
	if (boot_verbose >= 2)
		sym53c8xx_print_driver_setup();
#endif /* SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT */

	/*
	 *  Allocate the device table since we donnot want to 
	 *  overflow the kernel stack.
	 *  1 x 4K PAGE is enough for more than 40 devices for i386.
	 */
	devtbl = sym_calloc(PAGE_SIZE, "DEVTBL");
	if (!devtbl)
		return 0;

	/*
	 *  Detect all NCR PQS/PDS memory controllers.
	 */
#ifdef	SYM_CONF_PQS_PDS_SUPPORT
	sym_detect_pqs_pds();
#endif

	/* 
	 *  Detect all 53c8xx hosts.
	 *  Save the first Symbios NVRAM content if any 
	 *  for the boot order.
	 */
	chips	= sizeof(sym_chip_ids)	/ sizeof(sym_chip_ids[0]);
	hosts	= PAGE_SIZE		/ sizeof(*devtbl);
#if SYM_CONF_NVRAM_SUPPORT
	nvp = (sym_driver_setup.use_nvram & 0x1) ? &nvram0 : 0;
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
		i = sym_driver_setup.reverse_probe ? chips - 1 - j : j;
		pcidev = pci_find_device(PCI_VENDOR_ID_NCR, sym_chip_ids[i],
					 pcidev);
		if (pcidev == PCIDEV_NULL) {
			++j;
			continue;
		}
		/* This one is guaranteed by AC to do nothing :-) */
		if (pci_enable_device(pcidev))
			continue;
		/* Some HW as the HP LH4 may report twice PCI devices */
		for (i = 0; i < count ; i++) {
			if (devtbl[i].s.bus       == PciBusNumber(pcidev) && 
			    devtbl[i].s.device_fn == PciDeviceFn(pcidev))
				break;
		}
		if (i != count)	/* Ignore this device if we already have it */
			continue;
		devp = &devtbl[count];
		devp->host_id = SYM_SETUP_HOST_ID;
		devp->attach_done = 0;
		if (sym53c8xx_pci_init(tpnt, pcidev, devp)) {
			continue;
		}
		++count;
#if SYM_CONF_NVRAM_SUPPORT
		if (nvp) {
			sym_get_nvram(devp, nvp);
			switch(nvp->type) {
			case SYM_SYMBIOS_NVRAM:
				/*
				 *   Switch to the other nvram buffer, so that 
				 *   nvram0 will contain the first Symbios 
				 *   format NVRAM content with boot order.
				 */
				nvp = &nvram;
				msg = "with Symbios NVRAM";
				break;
			case SYM_TEKRAM_NVRAM:
				msg = "with Tekram NVRAM";
				break;
			}
		}
#endif
#ifdef	SYM_CONF_PQS_PDS_SUPPORT
		/*
		 *  Match the BUS number for PQS/PDS devices.
		 *  Read the SCSI ID from a special register mapped
		 *  into the configuration space of the individual
		 *  875s.  This register is set up by the PQS bios
		 */
		for(i = 0; i < SYM_CONF_MAX_PQS_BUS && pqs_bus[i] != -1; i++) {
			u_char tmp;
			if (pqs_bus[i] == PciBusNumber(pcidev)) {
				pci_read_config_byte(pcidev, 0x84, &tmp);
				devp->pqs_pds = 1;
				devp->host_id = tmp;
				break;
			}
		}
		if (devp->pqs_pds)
			msg = "(NCR PQS/PDS)";
#endif
		if (boot_verbose)
			printf_info("%s: 53c%s detected %s\n",
			            sym_name(devp), devp->chip.name, msg);
	}

	/*
	 *  If we have found a SYMBIOS NVRAM, use first the NVRAM boot 
	 *  sequence as device boot order.
	 *  check devices in the boot record against devices detected. 
	 *  attach devices if we find a match. boot table records that 
	 *  do not match any detected devices will be ignored. 
	 *  devices that do not match any boot table will not be attached
	 *  here but will attempt to be attached during the device table 
	 *  rescan.
	 */
#if SYM_CONF_NVRAM_SUPPORT
	if (!nvp || nvram0.type != SYM_SYMBIOS_NVRAM)
		goto next;
	for (i = 0; i < 4; i++) {
		Symbios_host *h = &nvram0.data.Symbios.host[i];
		for (j = 0 ; j < count ; j++) {
			devp = &devtbl[j];
			if (h->device_fn != devp->s.device_fn ||
			    h->bus_nr	 != devp->s.bus	 ||
			    h->device_id != devp->chip.device_id)
				continue;
			if (devp->attach_done)
				continue;
			if (h->flags & SYMBIOS_INIT_SCAN_AT_BOOT) {
				sym_get_nvram(devp, nvp);
				if (!sym_attach (tpnt, attach_count, devp))
					attach_count++;
			}
			else if (!(sym_driver_setup.use_nvram & 0x80))
				printf_info(
				      "%s: 53c%s state OFF thus not attached\n",
				      sym_name(devp), devp->chip.name);
			else
				continue;

			devp->attach_done = 1;
			break;
		}
	}
next:
#endif

	/* 
	 *  Rescan device list to make sure all boards attached.
	 *  Devices without boot records will not be attached yet
	 *  so try to attach them here.
	 */
	for (i= 0; i < count; i++) {
		devp = &devtbl[i];
		if (!devp->attach_done) {
			devp->nvram = &nvram;
			nvram.type = 0;
#if SYM_CONF_NVRAM_SUPPORT
			sym_get_nvram(devp, nvp);
#endif
			if (!sym_attach (tpnt, attach_count, devp))
				attach_count++;
		}
	}

	sym_mfree(devtbl, PAGE_SIZE, "DEVTBL");

	return attach_count;
}



#ifdef MODULE
/*
 *  Linux release module stuff.
 *
 *  Called before unloading the module.
 *  Detach the host.
 *  We have to free resources and halt the NCR chip.
 *
 */
static int sym_detach(hcb_p np)
{
	printk("%s: detaching ...\n", sym_name(np));

	/*
	 *  Try to delete the timer.
	 *  In the unlikely situation where this failed,
	 *  try to synchronize with the timer handler.
	 */
#if LINUX_VERSION_CODE < LinuxVersionCode(2, 4, 0)
	np->s.release_stage = 1;
	if (!del_timer(&np->s.timer)) {
		int i = 1000;
		int k = 1;
		while (1) {
			u_long flags;
			SYM_LOCK_HCB(np, flags);
			k = np->s.release_stage;
			SYM_UNLOCK_HCB(np, flags);
			if (k == 2 || !--i)
				break;
			MDELAY(5);
		}
		if (!i)
			printk("%s: failed to kill timer!\n", sym_name(np));
	}
	np->s.release_stage = 2;
#else
	(void)del_timer_sync(&np->s.timer);
#endif

	/*
	 *  Reset NCR chip.
	 *  We should use sym_soft_reset(), but we donnot want to do 
	 *  so, since we may not be safe if interrupts occur.
	 */
	printk("%s: resetting chip\n", sym_name(np));
	OUTB (nc_istat, SRST);
	UDELAY (10);
	OUTB (nc_istat, 0);

	/*
	 *  Free host resources
	 */
	sym_free_resources(np);

	return 1;
}

int sym53c8xx_release(struct Scsi_Host *host)
{
     sym_detach(((struct host_data *) host->hostdata)->ncb);

     return 0;
}
#endif /* MODULE */

/*
 * For bigots to keep silent. :)
 */
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

/*
 * Driver host template.
 */
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0)
static
#endif
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0) || defined(MODULE)
Scsi_Host_Template driver_template = SYM53C8XX;
#include "../scsi_module.c"
#endif
