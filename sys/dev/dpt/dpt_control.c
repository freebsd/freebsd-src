/*
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
 */

/*
 * dpt_control.c: Control Functions and /dev entry points for /dev/dpt
 *
 * Caveat Emptor!	This is work in progress.	The interfaces and
 * functionality of this code will change (possibly radically) in the
 * future.
 */

#ident "$Id: dpt_control.c,v 1.3.2.1 1998/03/06 23:44:09 julian Exp $"

#include "opt_dpt.h"

#include <sys/types.h>
#include <i386/isa/isa.h>
#include <i386/isa/timerreg.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <machine/clock.h>
#include <machine/speaker.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#ifdef	DEVFS
#include <sys/devfsext.h>
void	*devfs_token;
#endif

#include <dev/dpt/dpt.h>
#include <sys/queue.h>

#define INLINE __inline

dpt_sysinfo_t dpt_sysinfo;

/* Entry points and other prototypes */
static vm_offset_t dpt_physmap(u_int32_t paddr, vm_size_t size);
static void dpt_unphysmap(u_int8_t *vaddr, vm_size_t size);

static void dpt_get_sysinfo(void);
static INLINE dpt_softc_t *dpt_minor2softc(int minor_no);
static INLINE int	   dpt_minor2unit(int minor_no);

int dpt_open(dev_t dev, int flags, int fmt, struct proc *p);
int dpt_close(dev_t dev, int flags, int fmt, struct proc *p);
int dpt_write(dev_t dev,struct uio *uio, int ioflag);
int dpt_read(dev_t dev, struct uio *uio, int ioflag);
int dpt_ioctl(dev_t dev,int cmd, caddr_t cmdarg, int flags, struct proc *p);



static dpt_sig_t dpt_sig = { 
    'd', 'P', 't', 'S', 'i', 'G',
    SIG_VERSION, PROC_INTEL, PROC_PENTIUM | PROC_P6,
    FT_HBADRVR, 0,
    OEM_DPT, OS_FREEBSD,
    CAP_PASS | CAP_OVERLAP,
    DEV_ALL, ADF_ALL_MASTER, 0, 0, 
    DPT_RELEASE, DPT_VERSION, DPT_PATCH, 
    DPT_MONTH, DPT_DAY, DPT_YEAR,
    "DPT FreeBSD Driver (c) 1997 Simon Shapiro"
};

#define CDEV_MAJOR		130

static struct cdevsw dpt_cdevsw = {
	dpt_open,	dpt_close,	dpt_read,	dpt_write,
	dpt_ioctl,	nostop,		nullreset,	nodevtotty,
	seltrue,	nommap,		NULL,		"dpt",
	NULL,		-1 };

static struct buf	*dpt_inbuf[DPT_MAX_ADAPTERS];
static char		dpt_rw_command[DPT_MAX_ADAPTERS][DPT_RW_CMD_LEN + 1];

/*
 * Map a physical address to virtual one.
 * This is a first vut, experimental thing
 *
 * Paddr is the physical address to map
 * size is the size of the region, in bytes.
 * Because of alignment problems, we actually round up thesize requested to
 * the next page count.
 */

static vm_offset_t dpt_physmap(u_int32_t req_paddr, vm_size_t req_size)
{
    vm_offset_t va;
    int		ndx;
    vm_size_t	size;
    u_int32_t	paddr;
    u_int32_t	offset;
    
    

    size = (req_size / PAGE_SIZE + 1) * PAGE_SIZE;
    paddr = req_paddr & 0xfffff000;
    offset = req_paddr - paddr;
    
    va = kmem_alloc_pageable(kernel_map, size);
    if ( va == (vm_offset_t)0 )
	return(va);
    
    for ( ndx = 0; ndx < size; ndx += PAGE_SIZE ) {
	pmap_kenter(va + ndx, paddr + ndx);
	invltlb();
    }

    return(va + offset);
}


/* Release virtual space allocated by physmap
 * We ASSUME that the correct srart address and the correct LENGTH
 * are given.
 *
 * Disaster will follow if these assumptions are false!
 */

static void dpt_unphysmap(u_int8_t *vaddr, vm_size_t size)
{
    int ndx;
    
    for (ndx = 0; ndx < size; ndx += PAGE_SIZE ) {
	pmap_kremove((vm_offset_t)vaddr + ndx);
    }

    kmem_free(kernel_map, (vm_offset_t)vaddr, size);
}

/*
 * Given a minor device number, get its SCSI Unit.
 */

static INLINE int
dpt_minor2unit(int minor)
{
    int unit;

    unit = minor2hba(minor);

    return(unit);
}

/*
 * Given a minor device number,
 * return the pointer to it's softc structure
 */

static INLINE dpt_softc_t *
dpt_minor2softc(int minor_no)
{
    dpt_softc_t *dpt;

    if ( dpt_minor2unit(minor_no) == -1 )
	return(NULL);

    for (dpt = TAILQ_FIRST(&dpt_softc_list);
	 (dpt != NULL) && (dpt->unit != minor_no);
	 dpt = TAILQ_NEXT(dpt, links));

    return(dpt);
}

/*
 * Collect interesting system information
 * The following is one of the worst hacks I have ever allowed my 
 * name to be associated with.
 * There MUST be a system structure that provides this data.
 */

static void
dpt_get_sysinfo(void)
{
    int			i;
    int			j;
    int			ospl;
    char		*addr;
    
    bzero(&dpt_sysinfo, sizeof(dpt_sysinfo_t));

    /*
     * This is really silly, but we better run this in splhigh as we
     * have no clue what we bump into.
     * Let's hope anyone else who does this sort of things protects them
     * with splhigh too.
     */
    ospl = splhigh();
			   
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
    if ( (addr = (char *)dpt_physmap(0x0475, 1024)) == NULL ) {
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
	switch ( i ) {
	case 0:
	    addr = (char *)dpt_physmap(0xC8000, 1024);
	case 1:
	    addr = (char *)dpt_physmap(0xD8000, 1024);
	default:
	    addr = (char *)dpt_physmap(0xDC000, 1024);
	}

	if ( addr == NULL )
	    continue;
	
	if (*((u_int16_t * )addr) == 0xaa55) {
	    if ( (*((u_int32_t * )(addr + 6)) == 0x00202053)
		 && (*((u_int32_t * )(addr + 10)) == 0x00545044)) {
		break;
	    }
	}

	dpt_unphysmap(addr, 1024);
	addr = NULL;
    }

    /*
     * If i < 3, we found it so set up a pointer to the starting
     * version diget by searching for it.
     */
    if (addr != NULL) {
	addr += 0x15;
	for(i = 0; i < 64; ++i)
	    if( (addr[i] == ' ') && (addr[i + 1] == 'v') )
		break;
	if(i < 64) {
	    addr += (i + 4);
	}
	else {
	    dpt_unphysmap(addr, 1024);
	    addr = NULL;
	}
    }

    /* If all is well, set up the SmartROM version fields */
    if (addr != NULL) {
	dpt_sysinfo.smartROMMajorVersion = *addr - '0';
	dpt_sysinfo.smartROMMinorVersion = *(addr + 2);
	dpt_sysinfo.smartROMRevision = *(addr + 3);
	dpt_sysinfo.flags |= SI_SmartROMverValid;
    }
    else {
	dpt_sysinfo.flags |= SI_NO_SmartROM;
    }

    /* Get the conventional memory size from CMOS */
    outb(0x70, 0x16);
    j = inb(0x71);
    j <<= 8;
    outb(0x70, 0x15);
    j |= inb(0x71);
    dpt_sysinfo.conventionalMemSize = j;

    /*
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
	addr = (char *)dpt_physmap(0x0104, 1024);

	if (addr != NULL) {
	    j = *((ushort * )(addr + 2));
	    j *= 16;
	    j += *((ushort * )(addr));
	    dpt_unphysmap(addr, 1024);
	    addr = (char *)dpt_physmap(j, 1024);

	    if (addr != NULL) {
		dpt_sysinfo.drives[0].cylinders = *((ushort * )addr);
		dpt_sysinfo.drives[0].heads = *(addr + 2);
		dpt_sysinfo.drives[0].sectors = *(addr + 14);
		dpt_unphysmap(addr, 1024);
	    }
	}

	if (dpt_sysinfo.numDrives > 1) {
	    /* Get the pointer from Int 46 for the second drive parameters */
	    addr = (char *)dpt_physmap(0x01118, 1024);
	    j = *((ushort * )(addr + 2));
	    j *= 16;
	    j += *((ushort * )(addr));
	    dpt_unphysmap(addr, 1024);
	    addr = (char *)dpt_physmap(j, 1024);
	    
	    if (addr != NULL) {
		dpt_sysinfo.drives[1].cylinders = *((ushort * )addr);
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

    switch ( cpu_class ) {
    case CPUCLASS_386 :
	dpt_sysinfo.processorType = PROC_386;
	break;
    case CPUCLASS_486 :
	dpt_sysinfo.processorType = PROC_486;
	break;
    case CPUCLASS_586 :
	dpt_sysinfo.processorType = PROC_PENTIUM;
	break;
    case CPUCLASS_686 :
	dpt_sysinfo.processorType = PROC_P6;
	break;
    default :
	dpt_sysinfo.flags &= ~SI_ProcessorValid;
	break;
    }

    /* Get the bus I/O bus information */
    dpt_sysinfo.flags |= SI_BusTypeValid;
    dpt_sysinfo.busType = SI_PCI_BUS;
}

int
dpt_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    int		minor_no;
    int		ospl;
    dpt_softc_t *dpt;

    minor_no = minor(dev);

    if ( dpt_minor2unit(minor_no) == -1 )
	return(ENXIO);
    else
	dpt = dpt_minor2softc(minor_no);

    if ( dpt == NULL )
	return(ENXIO);

    ospl = splbio();

    if	(dpt->state & DPT_HA_CONTROL_ACTIVE ) {
	splx(ospl);
	return(EBUSY);
    }
    else {
	dpt->state |= DPT_HA_CONTROL_ACTIVE;
	splx(ospl);
	dpt_inbuf[minor_no] = geteblk(PAGE_SIZE);
    }

    return(0);
}

int
dpt_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    int		minor_no;
    dpt_softc_t *dpt;

    minor_no = minor(dev);
    dpt      = dpt_minor2softc(minor_no);

    if ( (dpt_minor2unit(minor_no) == -1) || (dpt == NULL) )
	return(ENXIO);
    else {
	brelse(dpt_inbuf[minor_no]);
	dpt->state &= ~DPT_HA_CONTROL_ACTIVE;
	return(0);
    }
}

int
dpt_write(dev_t dev, struct uio *uio, int ioflag)
{
    int minor_no;
    int unit;
    int error;

    minor_no = minor(dev);
    unit	= dpt_minor2unit(minor_no);

    if ( unit == -1 ) {
	return(ENXIO);
    } else if (uio->uio_resid > DPT_RW_CMD_LEN ) {
	return(E2BIG);
    } else {
	char *cp;
	int	length;

	cp	   = dpt_inbuf[minor_no]->b_un.b_addr;
	length     = uio->uio_resid; /* uiomove will change it! */
	error      = uiomove(cp, length, uio);
	cp[length] = '\0';

	strcpy(dpt_rw_command[unit], cp);
#ifdef DPT_DEBUG_CONTROL
	if ( error ) {
	    printf("dpt%d: WRITE uimove failed (%d)\n", minor_no, error);
	} else {
	    /*
	     * For lack of anything better to do;
	     * For now, dump the data so we can look at it and rejoice
	     */
	    printf("dpt%d: Command \"%s\" arrived\n",
		   unit, dpt_rw_command[unit]);
	}
#endif
    }

    return(error);
}

int
dpt_read(dev_t dev, struct uio *uio, int ioflag)
{
    dpt_softc_t *dpt;
    int error;
    int minor_no;
    int ospl;

    minor_no = minor(dev);
    error	= 0;

#ifdef DPT_DEBUG_CONTROL
    printf("dpt%d: read, count = %d, dev = %08x\n",
	   minor_no, uio->uio_resid, dev);
#endif

    if ( dpt_minor2unit(minor_no) == -1 ) {
	return(ENXIO);
    } else if ( uio->uio_resid > PAGE_SIZE ) { /* DEV_BSIZE = 512 */
	return(E2BIG);
    }
    else {
	char *work_buffer;
	char *wbp;
	char *command;
	int	work_size;
	int	ndx;
	int x;

	if ( (dpt = dpt_minor2softc(minor_no)) == NULL )
	    return(ENXIO);

	work_buffer = (u_int8_t *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
	wbp = work_buffer;
	work_size = 0;

	ospl = splbio();

	command = dpt_rw_command[dpt->unit];
	if (strcmp(command, DPT_RW_CMD_DUMP_SOFTC) == 0) {
	    x = sprintf(wbp, "dpt%d:%s:%d:%d:%d:%d:%d:%s:%s:%s:%s:%s:%d\n",
			dpt->unit,
			dpt->handle_interrupts ? "Yes" : "No",
			dpt->total_ccbs_count,
			dpt->free_ccbs_count,
			dpt->waiting_ccbs_count,
			dpt->submitted_ccbs_count,
			dpt->completed_ccbs_count,
			i2bin(dpt->queue_status,
			      sizeof(dpt->queue_status) * 8),
			i2bin(dpt->free_lock, sizeof(dpt->free_lock) * 8),
			i2bin(dpt->waiting_lock,
			      sizeof(dpt->waiting_lock) * 8),
			i2bin(dpt->submitted_lock,
			      sizeof(dpt->submitted_lock) * 8),
			i2bin(dpt->completed_lock,
			      sizeof(dpt->completed_lock) * 8),
			dpt->commands_processed);
	    work_size += x;
	    wbp += x;

#ifdef DPT_MEASURE_PERFORMANCE
	    /* Interrupt related measurements */
	    x = sprintf(wbp, "dpt%d:%d:%d:%d:%d\n",
			dpt->unit,
			dpt->performance.aborted_interrupts,
			dpt->performance.spurious_interrupts,
			dpt->performance.min_intr_time,
			dpt->performance.max_intr_time);
	    work_size += x;
	    wbp += x;

	    /* SCSI Commands, can be no more than 256 of them */
	    for (ndx = 0; ndx < 256; ndx++) {
		if (dpt->performance.command_count[ndx] != 0) {
		    x = sprintf(wbp, "dpt%d:%d:%s:%d:%d:%d\n",
				dpt->unit, ndx,
				scsi_cmd_name((u_int8_t)ndx), 
				dpt->performance.command_count[ndx],
				dpt->performance.min_command_time[ndx],
				dpt->performance.max_command_time[ndx]);
		    work_size += x;
		    wbp += x;
		}
	    }
	    
	    x = sprintf(wbp, "\n");
	    work_size += x;
	    wbp += x;

	    /* READ/WRITE statistics, per block size */

	    for ( ndx = 0; ndx < 10; ndx++) {
		char *mask;
		
		x = sprintf(wbp, "dpt%d:%d:%d:%d\n", dpt->unit,
			    dpt->performance.read_by_size_count[ndx],
			    dpt->performance.read_by_size_min_time[ndx],
			    dpt->performance.read_by_size_max_time[ndx]);
		work_size += x;
		wbp += x;
	    }
	    
	    for ( ndx = 0; ndx < 10; ndx++) {
		char *mask;
		
		x = sprintf(wbp, "dpt%d:%d:%d:%d\n", dpt->unit,
			    dpt->performance.write_by_size_count[ndx],
			    dpt->performance.write_by_size_min_time[ndx],
			    dpt->performance.write_by_size_max_time[ndx]);
		work_size += x;
		wbp += x;
	    }
	    
	    x = sprintf(wbp, "dpt%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n", 
			dpt->unit,
			dpt->performance.max_waiting_count,
			dpt->performance.min_waiting_time,
			dpt->performance.max_waiting_time,
			dpt->performance.max_submit_count,
			dpt->performance.min_submit_time,
			dpt->performance.max_submit_time,
			dpt->performance.max_complete_count,
			dpt->performance.min_complete_time,
			dpt->performance.max_complete_time);
	    work_size += x;
	    wbp += x;

	    x = sprintf(wbp, "dpt%d:%d:%d:%d:%d\n",
			dpt->unit,
			dpt->performance.command_collisions,
			dpt->performance.command_too_busy,
			dpt->performance.max_eata_tries,
			dpt->performance.min_eata_tries);
	    work_size += x;
	    wbp += x;

#endif
	    x = sprintf(wbp, "dpt%d:%d:%d:%x:%x:%x:%d:%d:%p:%p:%p\n",
			dpt->unit,
			dpt->max_id,
			dpt->max_lun,
			dpt->io_base,
			dpt->v_membase,
			dpt->p_membase,
			dpt->irq,
			dpt->dma_channel,
			dpt->sc_scsi_link[0],
			dpt->sc_scsi_link[1],
			dpt->sc_scsi_link[2]);
	    work_size += x;
	    wbp += x;

	    x = sprintf(wbp, "dpt%d:%x:%x:%s:%s:%s:%s:%x\n",
			dpt->unit,
			dpt->board_data.deviceType,
			dpt->board_data.rm_dtq,
			dpt->board_data.vendor,
			dpt->board_data.modelNum,
			dpt->board_data.firmware,
			dpt->board_data.protocol,
			dpt->EATA_revision);
	    work_size += x;
	    wbp += x;

	    x = sprintf(wbp,"dpt%d:%x:%d:%s:%s:%s:%s:%s\n",
		      dpt->unit,
		      dpt->bustype,
		      dpt->channels,
		      i2bin((u_int32_t)dpt->state, sizeof(dpt->state) * 8),
		      dpt->primary ? "Yes" : "No",
		      dpt->more_support ? "Yes" : "No",
		      dpt->immediate_support ? "Yes" : "No",
		      dpt->broken_INQUIRY ? "Yes" : "No");
	    work_size += x;
	    wbp += x;

	    x = sprintf(wbp,"dpt%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
		      dpt->unit,
		      dpt->resetlevel[0],
		      dpt->resetlevel[1],
		      dpt->resetlevel[2],
		      dpt->cplen,		
		      dpt->cppadlen,
		      dpt->queuesize,
		      dpt->sgsize,
		      dpt->hostid[0],
		      dpt->hostid[1],
		      dpt->hostid[2]);
	    work_size += x;
	    wbp += x;

	    x = sprintf(wbp,"dpt%d:%s CACHE:%d\n",
		      dpt->unit,
		      (dpt->cache_type == DPT_NO_CACHE)
		      ? "No"
		      : (dpt->cache_type == DPT_CACHE_WRITETHROUGH)
		      ? "Write-Through" : "Write-Back",
		      dpt->cache_size);
	    work_size += x;
	    wbp += x;

	} else if (strcmp(command, DPT_RW_CMD_DUMP_SYSINFO)  == 0 ) {
	    x = sprintf(wbp,"dpt%d:%d:%d:%d:%d:%d:%d:%d:%d:%s:"
			"%d:%d:%d:%d:%d:%d:%d:%d\n",
			dpt->unit,
			dpt_sysinfo.drive0CMOS, dpt_sysinfo.drive1CMOS,
			dpt_sysinfo.numDrives,
			dpt_sysinfo.processorFamily, dpt_sysinfo.processorType,
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

	    for ( ndx = 0; ndx < 16; ndx++ ) {
		if ( dpt_sysinfo.drives[ndx].cylinders != 0 ) {
		    x = sprintf(wbp,"dpt%d:d%dc%dh%ds%d\n",
				dpt->unit,
				ndx,
				dpt_sysinfo.drives[ndx].cylinders,
				dpt_sysinfo.drives[ndx].heads,
				dpt_sysinfo.drives[ndx].sectors);
		    work_size += x;
		    wbp += x;
		}
		
	    }
	} else if (strcmp(command, DPT_RW_CMD_DUMP_METRICS)  == 0 ) {
#ifdef DPT_MEASURE_PERFORMANCE
	    /* Interrupt related measurements */
	    x = sprintf(wbp, "dpt%d:%d:%d:%d:%d\n",
			dpt->unit,
			dpt->performance.aborted_interrupts,
			dpt->performance.spurious_interrupts,
			dpt->performance.min_intr_time,
			dpt->performance.max_intr_time);
	    work_size += x;
	    wbp += x;

	    /* SCSI Commands, can be no more than 256 of them */
	    for (ndx = 0; ndx < 256; ndx++) {
		if (dpt->performance.command_count[ndx] != 0) {
		    x = sprintf(wbp, "dpt%d:%d:%s:%d:%d:%d\n",
				dpt->unit, ndx,
				scsi_cmd_name((u_int8_t)ndx), 
				dpt->performance.command_count[ndx],
				dpt->performance.min_command_time[ndx],
				dpt->performance.max_command_time[ndx]);
		    work_size += x;
		    wbp += x;
		}
	    }
	    
	    x = sprintf(wbp, "\n");
	    work_size += x;
	    wbp += x;

	    /* READ/WRITE statistics, per block size */

	    for ( ndx = 0; ndx < 10; ndx++) {
		char *mask;
		
		x = sprintf(wbp, "dpt%d:%d:%d:%d\n", dpt->unit,
			    dpt->performance.read_by_size_count[ndx],
			    dpt->performance.read_by_size_min_time[ndx],
			    dpt->performance.read_by_size_max_time[ndx]);
		work_size += x;
		wbp += x;
	    }
	    
	    for ( ndx = 0; ndx < 10; ndx++) {
		char *mask;
		
		x = sprintf(wbp, "dpt%d:%d:%d:%dd\n", dpt->unit,
			    dpt->performance.write_by_size_count[ndx],
			    dpt->performance.write_by_size_min_time[ndx],
			    dpt->performance.write_by_size_max_time[ndx]);
		work_size += x;
		wbp += x;
	    }
	    
	    x = sprintf(wbp, "dpt%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n", 
			dpt->unit,
			dpt->performance.max_waiting_count,
			dpt->performance.min_waiting_time,
			dpt->performance.max_waiting_time,
			dpt->performance.max_submit_count,
			dpt->performance.min_submit_time,
			dpt->performance.max_submit_time,
			dpt->performance.max_complete_count,
			dpt->performance.min_complete_time,
			dpt->performance.max_complete_time);
	    work_size += x;
	    wbp += x;

	    x = sprintf(wbp, "dpt%d:%d:%d:%d:%d\n",
			dpt->unit,
			dpt->performance.command_collisions,
			dpt->performance.command_too_busy,
			dpt->performance.max_eata_tries,
			dpt->performance.min_eata_tries);
	    work_size += x;
	    wbp += x;
	    
#else
	    x = sprintf(wbp,
			"No metrics available.\n"
			"You must compile the driver with the "
			"DPT_MEASURE_PERFORMANCE"
			"option enabled\n");
	    work_size += x;
	    wbp += x;		
#endif	
	} else if (strcmp(command, DPT_RW_CMD_CLEAR_METRICS) == 0 ) {
#ifdef DPT_MEASURE_PERFORMANCE	
	    bzero(&dpt->performance, sizeof(dpt->performance));
#endif /* DPT_MEASURE_PERFORMANCE */

	    x = sprintf(wbp, "dpt%d: Metrics have been cleared\n",
			dpt->unit);
	    work_size += x;
	    wbp		+= x;	
	} else if (strcmp(command, DPT_RW_CMD_SHOW_LED) == 0 ) {
#ifdef DPT_MEASURE_PERFORMANCE	
	    bzero(&dpt->performance, sizeof(dpt->performance));
#endif /* DPT_MEASURE_PERFORMANCE */

	    x = sprintf(wbp, "dpt%d:%s\n",
			dpt->unit, i2bin(dpt_blinking_led(dpt), 8));
	    work_size += x;
	    wbp		+= x;	
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
    return(error);
}

/*
 * This is the control syscall interface.
 * It should be binary compatible with UnixWare, if not totally syntatically so.
 */

int
dpt_ioctl(dev_t dev, int cmd, caddr_t cmdarg, int flags, struct proc *p)
{
    int			minor_no;
    dpt_softc_t	*dpt;
    int			result;
    eata_pt_t	eata_pass_thru;

    minor_no = minor(dev);
    result	= 0;

#ifdef DPT_DEBUG_CONTROL
    printf("dpt%d: ioctl cmd = %x\n", minor_no, cmd);
#endif /* DEBUG */

    if ( (dpt = dpt_minor2softc(minor_no)) == NULL )
	return(ENXIO);

    switch (cmd) {
    case DPT_IOCTL_INTERNAL_METRICS:
#ifdef DPT_MEASURE_PERFORMANCE
	result = copyout((char *)&dpt->performance, 
			 (dpt_softc_t *)(*(caddr_t *)cmdarg),
			 sizeof(dpt_perf_t));
#else
	result = ENXIO;
#endif /* DPT_MEASURE_PERFORMANCE */
	break;
    case DPT_IOCTL_SOFTC:
	result = copyout((char *)&dpt, 
			 (dpt_softc_t *)(*(caddr_t *)cmdarg),
			 sizeof(dpt_softc_t));
	break;
    case DPT_IOCTL_SEND:
	if ( (result = copyin(cmdarg, (char *)&eata_pass_thru,
			      sizeof(eata_pt_t))) != 0 )
	    return(EFAULT);

	if ( (eata_pass_thru.eataID[0]	!= 'E')
	     || (eata_pass_thru.eataID[1] != 'A')
	     || (eata_pass_thru.eataID[2] != 'T')
	     || (eata_pass_thru.eataID[3] != 'A') ) {
	    return(ENXIO);
	}

	switch ( eata_pass_thru.command ) {
	case DPT_SIGNATURE:
	    if ( (result = copyout((char *)&dpt_sig,
				   eata_pass_thru.command_buffer,
				   sizeof(dpt_sig_t))) != 0 ) {
		return(EFAULT);
	    }
	    break;
	case DPT_NUMCTRLS:
	    if ( (result = copyout((char *)&dpt_controllers_present,
				   eata_pass_thru.command_buffer,
				   sizeof(dpt_controllers_present))) != 0 ) {
		return(EFAULT);
	}
	    break;	
	case DPT_CTRLINFO:
	{
	    dpt_compat_ha_t compat_softc;
	    int ndx;

	    compat_softc.ha_state	= dpt->state; /*Different Meaning! */
	    for ( ndx = 0; ndx < 4; ndx++ )
		compat_softc.ha_id[ndx]	= dpt->hostid[ndx];

	    compat_softc.ha_vect	= dpt->irq;
	    compat_softc.ha_base	= BaseRegister(dpt);
	    compat_softc.ha_max_jobs	= dpt->total_ccbs_count;
	    compat_softc.ha_cache	= dpt->cache_type;
	    compat_softc.ha_cachesize	= dpt->cache_size;
	    compat_softc.ha_nbus	= dpt->dma_channel + 1;
	    compat_softc.ha_ntargets	= dpt->max_id +1;
	    compat_softc.ha_nluns	= dpt->max_lun +1;
	    compat_softc.ha_tshift	= (dpt->max_id == 7) ? 3 : 4;
	    compat_softc.ha_bshift	= 2;
	    compat_softc.ha_npend	= dpt->submitted_ccbs_count; 
	    compat_softc.ha_active_jobs	= dpt->waiting_ccbs_count;
	    strncpy(compat_softc.ha_fw_version, dpt->board_data.firmware, 4);
	    compat_softc.ha_ccb		= NULL;
	    compat_softc.ha_cblist	= NULL;
	    compat_softc.ha_dev		= NULL;
	    compat_softc.ha_StPkt_lock	= NULL;
	    compat_softc.ha_ccb_lock	= NULL;
	    compat_softc.ha_LuQWaiting	= NULL;
	    compat_softc.ha_QWait_lock	= NULL;
	    compat_softc.ha_QWait_opri	= NULL;

	    if ( (result = copyout((char *)&dpt_sig,
				   eata_pass_thru.command_buffer,
				   sizeof(dpt_sig_t))) != 0 ) {
		return(EFAULT);
	    }
	}
	break;
	
	case DPT_SYSINFO: 
	    /* Copy out the info structure to the user */
	    if ( (result = copyout((char *)&dpt_sysinfo,
				   eata_pass_thru.command_buffer,
				   sizeof(dpt_sysinfo_t))) != 0 ) {
		return(EFAULT);
	    }
	break;
	
	case EATAUSRCMD:
	    return(dpt_user_cmd(dpt, &eata_pass_thru, cmdarg, minor_no));
	case DPT_BLINKLED:
	    result = dpt_blinking_led(dpt);
	    if (copyout((caddr_t) &result, eata_pass_thru.command_buffer, 4)) {
		return(EFAULT);
	    }
	    break;

	default:
	    result = EINVAL;
	}
	return(result);
	
    }
    
    return(result);
}

static dpt_devsw_installed = 0;

static void
dpt_drvinit(void *unused)
{
    dev_t dev;

    if( ! dpt_devsw_installed ) {
	dev = makedev(CDEV_MAJOR, 0);
	cdevsw_add(&dev,&dpt_cdevsw, NULL);
	dpt_devsw_installed = 1;
	printf("DPT:  RAID Manager driver, Version %d.%d.%d\n",
	       DPT_CTL_RELEASE, DPT_CTL_VERSION, DPT_CTL_PATCH);
#ifdef DEVFS
	devfs_token = devfs_add_devswf(&dpt_cdevsw, 0, DV_CHR,
				       UID_ROOT, GID_WHEEL, 0600, "dpt");
#endif
    }
    dpt_get_sysinfo();
}

SYSINIT(dpt_dev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR, dpt_drvinit, NULL)

/* End of the dpt_control driver */


