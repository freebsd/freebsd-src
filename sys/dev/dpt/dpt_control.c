/**
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *
 */

/**
 * dpt_control.c: Control Functions and /dev entry points for /dev/dpt*
 *
 * Caveat Emptor!	This is work in progress.	The interfaces and
 * functionality of this code will change (possibly radically) in the
 * future.
 */

#ident "$FreeBSD$"

#include "opt_dpt.h"

#include <i386/include/cputypes.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <scsi/scsiconf.h>

#include <dev/dpt/dpt.h>

#define INLINE __inline

extern char     osrelease[];

static dpt_sysinfo_t   dpt_sysinfo;

/* Entry points and other prototypes */
static vm_offset_t dpt_physmap(u_int32_t paddr, vm_size_t size);
static void     dpt_unphysmap(u_int8_t * vaddr, vm_size_t size);

static void     dpt_get_sysinfo(void);

static int      dpt_open(dev_t dev, int flags, int fmt, struct proc * p);
static int      dpt_close(dev_t dev, int flags, int fmt, struct proc * p);
static int      dpt_write(dev_t dev, struct uio * uio, int ioflag);
static int      dpt_read(dev_t dev, struct uio * uio, int ioflag);
static int      dpt_ioctl(dev_t dev, u_long cmd, caddr_t cmdarg, int flags, struct proc * p);


/* This has to be modified as the processor and CPU are not known yet */
static dpt_sig_t dpt_sig = {
	'd', 'P', 't', 'S', 'i', 'G',
	SIG_VERSION, PROC_INTEL, PROC_386,
	FT_HBADRVR, FTF_PROTECTED,
	OEM_DPT, OS_FREEBSD,
	CAP_PASS | CAP_OVERLAP | CAP_RAID0 | CAP_RAID1 | CAP_RAID5 | CAP_ASPI,
	DEV_ALL, ADF_SC4_PCI | ADF_SC3_PCI, 0, 0,
	DPT_RELEASE, DPT_VERSION, DPT_PATCH,
	DPT_MONTH, DPT_DAY, DPT_YEAR,
	"DPT FreeBSD Driver (c) 1997 Simon Shapiro"
};

#define CDEV_MAJOR	    DPT_CDEV_MAJOR

/* Normally, this is a static structure.  But we need it in pci/dpt_pci.c */
static struct cdevsw dpt_cdevsw = {
	/* open */	dpt_open,
	/* close */	dpt_close,
	/* read */	dpt_read,
	/* write */	dpt_write,
	/* ioctl */	dpt_ioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"dpt",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static struct buf *dpt_inbuf[DPT_MAX_ADAPTERS];
static char     dpt_rw_command[DPT_MAX_ADAPTERS][DPT_RW_CMD_LEN + 1];


/*
 * Given a minor device number,
 * return the pointer to its softc structure
 */

dpt_softc_t *
dpt_minor2softc(int minor_no)
{
	dpt_softc_t    *dpt;

	if (dpt_minor2unit(minor_no & ~SCSI_CONTROL_MASK) == -1)
		return (NULL);

	for (dpt = TAILQ_FIRST(&dpt_softc_list);
	     (dpt != NULL) && (dpt->unit != (minor_no & ~SCSI_CONTROL_MASK));
	     dpt = TAILQ_NEXT(dpt, links));

	return (dpt);
}

/**
 * Map a physical address to virtual one.
 * This is a first cut, experimental thing
 *
 * Paddr is the physical address to map
 * size is the size of the region, in bytes.
 * Because of alignment problems, we actually round up the size requested to
 * the next page count.
 */

static          vm_offset_t
dpt_physmap(u_int32_t req_paddr, vm_size_t req_size)
{
	vm_offset_t     va;
	int             ndx;
	vm_size_t       size;
	u_int32_t       paddr;
	u_int32_t       offset;



	size = (req_size / PAGE_SIZE + 1) * PAGE_SIZE;
	paddr = req_paddr & 0xfffff000;
	offset = req_paddr - paddr;

	va = kmem_alloc_pageable(kernel_map, size);
	if (va == (vm_offset_t) 0)
		return (va);

	for (ndx = 0; ndx < size; ndx += PAGE_SIZE) {
		pmap_kenter(va + ndx, paddr + ndx);
		invltlb();
	}

	return (va + offset);
}


/*
 * Release virtual space allocated by physmap We ASSUME that the correct
 * start address and the correct LENGTH are given.
 * 
 * Disaster will follow if these assumptions are false!
 */

static void
dpt_unphysmap(u_int8_t * vaddr, vm_size_t size)
{
	int             ndx;

	for (ndx = 0; ndx < size; ndx += PAGE_SIZE) {
		pmap_kremove((vm_offset_t) vaddr + ndx);
	}

	kmem_free(kernel_map, (vm_offset_t) vaddr, size);
}

/**
 * Collect interesting system information
 * The following is one of the worst hacks I have ever allowed my
 * name to be associated with.
 * There MUST be a system structure that provides this data.
 */

static void
dpt_get_sysinfo(void)
{
	int             i;
	int             j;
	int             ospl;
	char           *addr;

	bzero(&dpt_sysinfo, sizeof(dpt_sysinfo_t));

	/**
         * This is really silly, but we better run this in splhigh as we
         * have no clue what we bump into.
         * Let's hope anyone else who does this sort of things protects them
         * with splhigh too.
         */
	ospl = splhigh();

	switch (cpu_class) {
	case CPUCLASS_386:
		dpt_sig.Processor = dpt_sysinfo.processorType = PROC_386;
		break;
	case CPUCLASS_486:
		dpt_sig.Processor = dpt_sysinfo.processorType = PROC_486;
		break;
	case CPUCLASS_586:
		dpt_sig.Processor = dpt_sysinfo.processorType = PROC_PENTIUM;
		break;
	case CPUCLASS_686:
		dpt_sig.Processor = dpt_sysinfo.processorType = PROC_P6;
		break;
	default:
		dpt_sig.Processor = dpt_sysinfo.flags &= ~SI_ProcessorValid;
		break;
	}

	/* Get The First Drive Type From CMOS */
	outb(0x70, 0x12);
	i = inb(0x71);
	j = i >> 4;

	if (i == 0x0f) {
		outb(0x70, 0x19);
		j = inb(0x71);
	}
	dpt_sysinfo.drive0CMOS = j;

	/* Get The Second Drive Type From CMOS */
	j = i & 0x0f;
	if (i == 0x0f) {
		outb(0x70, 0x1a);
		j = inb(0x71);
	}
	dpt_sysinfo.drive1CMOS = j;

	/* Get The Number Of Drives From The Bios Data Area */
	if ((addr = (char *) dpt_physmap(0x0475, 1024)) == NULL) {
		printf("DPT:  Cannot map BIOS address 0x0475.  No sysinfo... :-(\n");
		return;
	}
	dpt_sysinfo.numDrives = *addr;
	dpt_unphysmap(addr, 1024);

	/* Get the processor fields from the SIG structure, and set the flags */
	dpt_sysinfo.processorFamily = dpt_sig.ProcessorFamily;
	dpt_sysinfo.flags = SI_CMOS_Valid | SI_NumDrivesValid;

	/* Go out and look for SmartROM */
	for (i = 0; i < 3; ++i) {
		switch (i) {
		case 0:
			addr = (char *) dpt_physmap(0xC8000, 1024);
		case 1:
			addr = (char *) dpt_physmap(0xD8000, 1024);
		default:
			addr = (char *) dpt_physmap(0xDC000, 1024);
		}

		if (addr == NULL)
			continue;

		if (*((u_int16_t *) addr) == 0xaa55) {
			if ((*((u_int32_t *) (addr + 6)) == 0x00202053)
			  && (*((u_int32_t *) (addr + 10)) == 0x00545044)) {
				break;
			}
		}
		dpt_unphysmap(addr, 1024);
		addr = NULL;
	}

	/**
         * If i < 3, we founday it so set up a pointer to the starting
         * version digit by searching for it.
         */
	if (addr != NULL) {
		addr += 0x15;
		for (i = 0; i < 64; ++i)
			if ((addr[i] == ' ') && (addr[i + 1] == 'v'))
				break;
		if (i < 64) {
			addr += (i + 4);
		} else {
			dpt_unphysmap(addr, 1024);
			addr = NULL;
		}
	}
	/* If all is well, set up the SmartROM version fields */
	if (addr != NULL) {
		dpt_sysinfo.smartROMMajorVersion = *addr - '0';	/* Assumes ASCII */
		dpt_sysinfo.smartROMMinorVersion = *(addr + 2);
		dpt_sysinfo.smartROMRevision = *(addr + 3);
		dpt_sysinfo.flags |= SI_SmartROMverValid;
	} else {
		dpt_sysinfo.flags |= SI_NO_SmartROM;
	}

	/* Get the conventional memory size from CMOS */
	outb(0x70, 0x16);
	j = inb(0x71);
	j <<= 8;
	outb(0x70, 0x15);
	j |= inb(0x71);
	dpt_sysinfo.conventionalMemSize = j;

	/**
         * Get the extended memory found at power on from CMOS
         */
	outb(0x70, 0x31);
	j = inb(0x71);
	j <<= 8;
	outb(0x70, 0x30);
	j |= inb(0x71);
	dpt_sysinfo.extendedMemSize = j;
	dpt_sysinfo.flags |= SI_MemorySizeValid;

	/* If there is 1 or 2 drives found, set up the drive parameters */
	if (dpt_sysinfo.numDrives > 0) {
		/* Get the pointer from int 41 for the first drive parameters */
		addr = (char *) dpt_physmap(0x0104, 1024);

		if (addr != NULL) {
			j = *((ushort *) (addr + 2));
			j *= 16;
			j += *((ushort *) (addr));
			dpt_unphysmap(addr, 1024);
			addr = (char *) dpt_physmap(j, 1024);

			if (addr != NULL) {
				dpt_sysinfo.drives[0].cylinders = *((ushort *) addr);
				dpt_sysinfo.drives[0].heads = *(addr + 2);
				dpt_sysinfo.drives[0].sectors = *(addr + 14);
				dpt_unphysmap(addr, 1024);
			}
		}
		if (dpt_sysinfo.numDrives > 1) {
			/*
			 * Get the pointer from Int 46 for the second drive
			 * parameters
			 */
			addr = (char *) dpt_physmap(0x01118, 1024);
			j = *((ushort *) (addr + 2));
			j *= 16;
			j += *((ushort *) (addr));
			dpt_unphysmap(addr, 1024);
			addr = (char *) dpt_physmap(j, 1024);

			if (addr != NULL) {
				dpt_sysinfo.drives[1].cylinders = *((ushort *) addr);
				dpt_sysinfo.drives[1].heads = *(addr + 2);
				dpt_sysinfo.drives[1].sectors = *(addr + 14);
				dpt_unphysmap(addr, 1024);
			}
		}
		dpt_sysinfo.flags |= SI_DriveParamsValid;
	}
	splx(ospl);

	/* Get the processor information */
	dpt_sysinfo.flags |= SI_ProcessorValid;

	/* Get the bus I/O bus information */
	dpt_sysinfo.flags |= SI_BusTypeValid;
	dpt_sysinfo.busType = HBA_BUS_PCI;

	/* XXX Use _FreeBSD_Version_ */
	dpt_sysinfo.osType = OS_FREEBSD;
	dpt_sysinfo.osMajorVersion = osrelease[0] - '0';
	if (osrelease[1] == '.')
		dpt_sysinfo.osMinorVersion = osrelease[2] - '0';
	else
		dpt_sysinfo.osMinorVersion = 0;
	if (osrelease[3] == '.')
		dpt_sysinfo.osRevision = osrelease[4] - '0';
	else
		dpt_sysinfo.osMinorVersion = 0;
	if (osrelease[5] == '.')
		dpt_sysinfo.osSubRevision = osrelease[6] - '0';
	else
		dpt_sysinfo.osMinorVersion = 0;


	dpt_sysinfo.flags |= SI_OSversionValid;
}

static int
dpt_open(dev_t dev, int flags, int fmt, struct proc * p)
{
	int             minor_no;
	int             ospl;
	dpt_softc_t    *dpt;

	minor_no = minor(dev);

	if (dpt_minor2unit(minor_no) == -1)
		return (ENXIO);
	else
		dpt = dpt_minor2softc(minor_no);

	if (dpt == NULL)
		return (ENXIO);

	ospl = splbio();

	if (dpt->state & DPT_HA_CONTROL_ACTIVE) {
		splx(ospl);
		return (EBUSY);
	} else {
		if ((dpt_inbuf[minor_no & ~SCSI_CONTROL_MASK] = geteblk(PAGE_SIZE))
		    == NULL) {
#ifdef DPT_DEBUG_CONTROL
			printf("dpt%d: Failed to obtain an I/O buffer\n",
			       minor_no & ~SCSI_CONTROL_MASK);
#endif
			splx(ospl);
			return (EINVAL);
		}
	}

	dpt->state |= DPT_HA_CONTROL_ACTIVE;
	splx(ospl);
	return (0);
}

static int
dpt_close(dev_t dev, int flags, int fmt, struct proc * p)
{
	int             minor_no;
	dpt_softc_t    *dpt;

	minor_no = minor(dev);
	dpt = dpt_minor2softc(minor_no);

	if ((dpt_minor2unit(minor_no) == -1) || (dpt == NULL))
		return (ENXIO);
	else {
		brelse(dpt_inbuf[minor_no & ~SCSI_CONTROL_MASK]);
		dpt->state &= ~DPT_HA_CONTROL_ACTIVE;
		return (0);
	}
}

static int
dpt_write(dev_t dev, struct uio * uio, int ioflag)
{
	int             minor_no;
	int             unit;
	int             error;

	minor_no = minor(dev);

	if (minor_no & SCSI_CONTROL_MASK) {
#ifdef DPT_DEBUG_CONTROL
		printf("dpt%d:  I/O attempted to control channel (%x)\n",
		       dpt_minor2unit(minor_no), minor_no);
#endif
		return (ENXIO);
	}
	unit = dpt_minor2unit(minor_no);

	if (unit == -1) {
		return (ENXIO);
	} else if (uio->uio_resid > DPT_RW_CMD_LEN) {
		return (E2BIG);
	} else {
		char           *cp;
		int             length;

		cp = dpt_inbuf[minor_no]->b_data;
		length = uio->uio_resid;	/* uiomove will change it! */

		if ((error = uiomove(cp, length, uio) != 0)) {
#ifdef DPT_DEBUG_CONTROL
			printf("dpt%d: uiomove(%x, %d, %x) failed (%d)\n",
			       minor_no, cp, length, uio, error);
#endif
			return (error);
		} else {
			cp[length] = '\0';

			/* A real kludge, to allow plain echo(1) to work */
			if (cp[length - 1] == '\n')
				cp[length - 1] = '\0';

			strncpy(dpt_rw_command[unit], cp, DPT_RW_CMD_LEN);
#ifdef DPT_DEBUG_CONTROL
			/**
			 * For lack of anything better to do;
			 * For now, dump the data so we can look at it and rejoice
			 */
			printf("dpt%d: Command \"%s\" arrived\n",
			       unit, dpt_rw_command[unit]);
#endif
		}
	}

	return (error);
}

static int
dpt_read(dev_t dev, struct uio * uio, int ioflag)
{
	dpt_softc_t    *dpt;
	int             error;
	int             minor_no;
	int             ospl;

	minor_no = minor(dev);
	error = 0;

#ifdef DPT_DEBUG_CONTROL
	printf("dpt%d: read, count = %d, dev = %08x\n",
	       minor_no, uio->uio_resid, dev);
#endif

	if (minor_no & SCSI_CONTROL_MASK) {
#ifdef DPT_DEBUG_CONTROL
		printf("dpt%d:  I/O attempted to control channel (%x)\n",
		       dpt_minor2unit(minor_no), minor_no);
#endif
		return (ENXIO);
	}
	if (dpt_minor2unit(minor_no) == -1) {
		return (ENXIO);
	}
	/*
	 * else if ( uio->uio_resid > PAGE_SIZE ) { return(E2BIG); }
	 */ 
	else {
		char           *work_buffer;
		char           *wbp;
		char           *command;
		int             work_size;
		int             ndx;
		int             x;

		if ((dpt = dpt_minor2softc(minor_no)) == NULL)
			return (ENXIO);

		work_buffer = (u_int8_t *) malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
		wbp = work_buffer;
		work_size = 0;

		ospl = splbio();

		command = dpt_rw_command[dpt->unit];
		if (strcmp(command, DPT_RW_CMD_DUMP_SOFTC) == 0) {
			x = sprintf(wbp, "dpt%d:%s:%s:%s:%s:%x\n",
				    dpt->unit,
				    dpt->board_data.vendor,
				    dpt->board_data.modelNum,
				    dpt->board_data.firmware,
				    dpt->board_data.protocol,
				    dpt->EATA_revision);
			work_size += x;
			wbp += x;

		} else if (strcmp(command, DPT_RW_CMD_DUMP_SYSINFO) == 0) {
			x = sprintf(wbp, "dpt%d:%d:%d:%d:%d:%d:%d:%d:%d:%s:"
				    "%d:%d:%d:%d:%d:%d:%d:%d\n",
				    dpt->unit,
				    dpt_sysinfo.drive0CMOS,
				    dpt_sysinfo.drive1CMOS,
				    dpt_sysinfo.numDrives,
				    dpt_sysinfo.processorFamily,
				    dpt_sysinfo.processorType,
				    dpt_sysinfo.smartROMMajorVersion,
				    dpt_sysinfo.smartROMMinorVersion,
				    dpt_sysinfo.smartROMRevision,
				    i2bin(dpt_sysinfo.flags,
					  sizeof(dpt->queue_status) * 8),
				    dpt_sysinfo.conventionalMemSize,
				    dpt_sysinfo.extendedMemSize,
			     dpt_sysinfo.osType, dpt_sysinfo.osMajorVersion,
			 dpt_sysinfo.osMinorVersion, dpt_sysinfo.osRevision,
			    dpt_sysinfo.osSubRevision, dpt_sysinfo.busType);
			work_size += x;
			wbp += x;

			for (ndx = 0; ndx < 16; ndx++) {
				if (dpt_sysinfo.drives[ndx].cylinders != 0) {
					x = sprintf(wbp, "dpt%d:d%dc%dh%ds%d\n",
						    dpt->unit,
						    ndx,
					  dpt_sysinfo.drives[ndx].cylinders,
					      dpt_sysinfo.drives[ndx].heads,
					   dpt_sysinfo.drives[ndx].sectors);
					work_size += x;
					wbp += x;
				}
			}
		} else if (strcmp(command, DPT_RW_CMD_DUMP_METRICS) == 0) {
			x = sprintf(wbp,
				    "dpt%d: No metrics available.\n"
				    "Run the dpt_dm command, or use the\n"
			   "DPT_IOCTL_INTERNAL_METRICS ioctl system call\n",
				    dpt->unit);
			work_size += x;
			wbp += x;
		} else if (strcmp(command, DPT_RW_CMD_CLEAR_METRICS) == 0) {
#ifdef DPT_MEASURE_PERFORMANCE
			dpt_reset_performance(dpt);
#endif				/* DPT_MEASURE_PERFORMANCE */

			x = sprintf(wbp, "dpt%d: Metrics have been cleared\n",
				    dpt->unit);
			work_size += x;
			wbp += x;
		} else if (strcmp(command, DPT_RW_CMD_SHOW_LED) == 0) {

			x = sprintf(wbp, "dpt%d:%s\n",
				dpt->unit, i2bin(dpt_blinking_led(dpt), 8));
			work_size += x;
			wbp += x;
		} else {
#ifdef DPT_DEBUG_CONTROL
			printf("dpt%d: Bad READ state (%s)\n", minor_no, command);
#endif
			splx(ospl);
			error = EINVAL;
		}

		if (error == 0) {
			work_buffer[work_size++] = '\0';
			error = uiomove(work_buffer, work_size, uio);
			uio->uio_resid = 0;
#ifdef DPT_DEBUG_CONTROL
			if (error) {
				printf("dpt%d: READ uimove failed (%d)\n", dpt->unit, error);
			}
#endif
		}
	}
	splx(ospl);
	return (error);
}

/**
 * This is the control syscall interface.
 * It should be binary compatible with UnixWare,
 * if not totally syntatically so.
 */

static int
dpt_ioctl(dev_t dev, u_long cmd, caddr_t cmdarg, int flags, struct proc * p)
{
	int             minor_no;
	dpt_softc_t    *dpt;
	dpt_user_softc_t udpt;
	int             result;
	int             ndx;
	eata_pt_t      *eata_pass_thru;

	minor_no = minor(dev);
	result = 0;

	if (!(minor_no & SCSI_CONTROL_MASK)) {
#ifdef DPT_DEBUG_CONTROL
		printf("dpt%d:  Control attempted to I/O channel (%x)\n",
		       dpt_minor2unit(minor_no), minor_no);
#endif				/* DEBUG */
		return (ENXIO);
	} else
		minor_no &= ~SCSI_CONTROL_MASK;

#ifdef DPT_DEBUG_CONTROL
	printf("dpt%d: IOCTL(%x, %x, %p, %x, %p)\n",
	       minor_no, dev, cmd, cmdarg, flags, p);
#endif				/* DEBUG */

	if ((dpt = dpt_minor2softc(minor_no)) == NULL)
		return (result);

	switch (cmd) {
#ifdef DPT_MEASURE_PERFORMANCE
	case DPT_IOCTL_INTERNAL_METRICS:	    
		memcpy(cmdarg, &dpt->performance, sizeof(dpt->performance));
		return (0);
#endif				/* DPT_MEASURE_PERFORMANCE */
	case DPT_IOCTL_SOFTC:
		udpt.unit = dpt->unit;
		udpt.handle_interrupts = dpt->handle_interrupts;
		udpt.target_mode_enabled = dpt->target_mode_enabled;
		udpt.spare = dpt->spare;

		udpt.total_ccbs_count = dpt->total_ccbs_count;
		udpt.free_ccbs_count = dpt->free_ccbs_count;
		udpt.waiting_ccbs_count = dpt->waiting_ccbs_count;
		udpt.submitted_ccbs_count = dpt->submitted_ccbs_count;
		udpt.completed_ccbs_count = dpt->completed_ccbs_count;

		udpt.queue_status = dpt->queue_status;
		udpt.free_lock = dpt->free_lock;
		udpt.waiting_lock = dpt->waiting_lock;
		udpt.submitted_lock = dpt->submitted_lock;
		udpt.completed_lock = dpt->completed_lock;

		udpt.commands_processed = dpt->commands_processed;
		udpt.lost_interrupts = dpt->lost_interrupts;

		udpt.channels = dpt->channels;
		udpt.max_id = dpt->max_id;
		udpt.max_lun = dpt->max_lun;

		udpt.io_base = dpt->io_base;
		udpt.v_membase = (u_int8_t *) dpt->v_membase;
		udpt.p_membase = (u_int8_t *) dpt->p_membase;

		udpt.irq = dpt->irq;
		udpt.dma_channel = dpt->dma_channel;

		udpt.board_data = dpt->board_data;
		udpt.EATA_revision = dpt->EATA_revision;
		udpt.bustype = dpt->bustype;
		udpt.state = dpt->state;

		udpt.primary = dpt->primary;
		udpt.more_support = dpt->more_support;
		udpt.immediate_support = dpt->immediate_support;
		udpt.broken_INQUIRY = dpt->broken_INQUIRY;
		udpt.spare2 = dpt->spare2;

		for (ndx = 0; ndx < MAX_CHANNELS; ndx++) {
			udpt.resetlevel[ndx] = dpt->resetlevel[ndx];
			udpt.hostid[ndx] = dpt->hostid[ndx];
		}

		udpt.last_ccb = dpt->last_ccb;
		udpt.cplen = dpt->cplen;
		udpt.cppadlen = dpt->cppadlen;
		udpt.queuesize = dpt->queuesize;
		udpt.sgsize = dpt->sgsize;
		udpt.cache_type = dpt->cache_type;
		udpt.cache_size = dpt->cache_size;

		memcpy(cmdarg, &udpt, sizeof(dpt_user_softc_t));
		return (0);
	case SDI_SEND:
	case DPT_IOCTL_SEND:
		eata_pass_thru = (eata_pt_t *) cmdarg;

		if ((eata_pass_thru->eataID[0] != 'E')
		    || (eata_pass_thru->eataID[1] != 'A')
		    || (eata_pass_thru->eataID[2] != 'T')
		    || (eata_pass_thru->eataID[3] != 'A')) {
			return (EFAULT);
		}
		switch (eata_pass_thru->command) {
		case DPT_SIGNATURE:
			return (copyout((char *) &dpt_sig,
				 (caddr_t *) eata_pass_thru->command_buffer,
					sizeof(dpt_sig)));
		case DPT_NUMCTRLS:
			return (copyout((char *) &dpt_controllers_present,
				 (caddr_t *) eata_pass_thru->command_buffer,
					sizeof(dpt_controllers_present)));
		case DPT_CTRLINFO:
			{
				dpt_compat_ha_t compat_softc;
				int             ndx;

				compat_softc.ha_state = dpt->state;	/* Different Meaning! */
				for (ndx = 0; ndx < MAX_CHANNELS; ndx++)
					compat_softc.ha_id[ndx] = dpt->hostid[ndx];

				compat_softc.ha_vect = dpt->irq;
				compat_softc.ha_base = BaseRegister(dpt);
				compat_softc.ha_max_jobs = dpt->total_ccbs_count;
				compat_softc.ha_cache = dpt->cache_type;
				compat_softc.ha_cachesize = dpt->cache_size;
				compat_softc.ha_nbus = dpt->dma_channel + 1;
				compat_softc.ha_ntargets = dpt->max_id + 1;
				compat_softc.ha_nluns = dpt->max_lun + 1;
				compat_softc.ha_tshift = (dpt->max_id == 7) ? 3 : 4;
				compat_softc.ha_bshift = 2;
				compat_softc.ha_npend = dpt->submitted_ccbs_count;
				compat_softc.ha_active_jobs = dpt->waiting_ccbs_count;
				strncpy(compat_softc.ha_fw_version,
				    dpt->board_data.firmware,
				    sizeof(compat_softc.ha_fw_version));
				compat_softc.ha_ccb = NULL;
				compat_softc.ha_cblist = NULL;
				compat_softc.ha_dev = NULL;
				compat_softc.ha_StPkt_lock = NULL;
				compat_softc.ha_ccb_lock = NULL;
				compat_softc.ha_LuQWaiting = NULL;
				compat_softc.ha_QWait_lock = NULL;
				compat_softc.ha_QWait_opri = NULL;

				return (copyout((char *) &compat_softc,
				 (caddr_t *) eata_pass_thru->command_buffer,
						sizeof(dpt_compat_ha_t)));
			}
			break;

		case DPT_SYSINFO:
			return (copyout((char *) &dpt_sysinfo,
				 (caddr_t *) eata_pass_thru->command_buffer,
					sizeof(dpt_sysinfo)));
		case EATAUSRCMD:
			result = dpt_user_cmd(dpt, eata_pass_thru, cmdarg, minor_no);
			return (result);
		case DPT_BLINKLED:
			result = dpt_blinking_led(dpt);
			return (copyout((caddr_t) & result,
				 (caddr_t *) eata_pass_thru->command_buffer,
					sizeof(result)));
		default:
			printf("dpt%d: Invalid (%x) pass-throu command\n",
			       dpt->unit, eata_pass_thru->command);
			result = EINVAL;
		}

	default:
		printf("dpt%d: Invalid (%lx) IOCTL\n", dpt->unit, cmd);
		return (EINVAL);

	}

	return (result);
}

static          dpt_devsw_installed = 0;

static void
dpt_drvinit(void *unused)
{

	if (!dpt_devsw_installed) {
		if (bootverbose)
			printf("DPT:  RAID Manager driver, Version %d.%d.%d\n",
			       DPT_CTL_RELEASE, DPT_CTL_VERSION, DPT_CTL_PATCH);

		/* Add the I/O (data) channel */
		cdevsw_add(&dpt_cdevsw);

		dpt_devsw_installed = 1;
	}
	dpt_get_sysinfo();
}

SYSINIT(dpt_dev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR, dpt_drvinit, NULL)
/* End of the dpt_control driver */
