/* $FreeBSD$ */

/*
 * This is part of the Driver for Video Capture Cards (Frame grabbers)
 * and TV Tuner cards using the Brooktree Bt848, Bt848A, Bt849A, Bt878, Bt879
 * chipset.
 * Copyright Roger Hardiman and Amancio Hasty.
 *
 * bktr_os : This has all the Operating System dependant code,
 *             probe/attach and open/close/ioctl/read/mmap
 *             memory allocation
 *             PCI bus interfacing
 *             
 *
 */

/*
 * 1. Redistributions of source code must retain the 
 * Copyright (c) 1997 Amancio Hasty, 1999 Roger Hardiman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Amancio Hasty and
 *      Roger Hardiman
 * 4. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifdef __FreeBSD__
#include "bktr.h"
#endif /* __FreeBSD__ */

#include "opt_bktr.h"		/* include any kernel config options */

#define FIFO_RISC_DISABLED      0
#define ALL_INTS_DISABLED       0


/*******************/
/* *** FreeBSD *** */
/*******************/
#ifdef __FreeBSD__

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#if (__FreeBSD_version >=400000) || (NSMBUS > 0)
#include <sys/bus.h>		/* used by smbus and newbus */
#endif

#if (__FreeBSD_version >=300000)
#include <machine/bus_memio.h>	/* used by bus space */
#include <machine/bus.h>	/* used by bus space and newbus */
#include <sys/bus.h>
#endif

#if (__FreeBSD_version >=400000)
#include <sys/rman.h>		/* used by newbus */
#include <machine/resource.h>	/* used by newbus */
#endif

#if (__FreeBSD_version < 500000)
#include <machine/clock.h>              /* for DELAY */
#endif

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <sys/sysctl.h>
int bt848_card = -1; 
int bt848_tuner = -1;
int bt848_reverse_mute = -1; 
int bt848_format = -1;
int bt848_slow_msp_audio = -1;

SYSCTL_NODE(_hw, OID_AUTO, bt848, CTLFLAG_RW, 0, "Bt848 Driver mgmt");
SYSCTL_INT(_hw_bt848, OID_AUTO, card, CTLFLAG_RW, &bt848_card, -1, "");
SYSCTL_INT(_hw_bt848, OID_AUTO, tuner, CTLFLAG_RW, &bt848_tuner, -1, "");
SYSCTL_INT(_hw_bt848, OID_AUTO, reverse_mute, CTLFLAG_RW, &bt848_reverse_mute, -1, "");
SYSCTL_INT(_hw_bt848, OID_AUTO, format, CTLFLAG_RW, &bt848_format, -1, "");
SYSCTL_INT(_hw_bt848, OID_AUTO, slow_msp_audio, CTLFLAG_RW, &bt848_slow_msp_audio, -1, "");

#if (__FreeBSD__ == 2)
#define PCIR_REVID     PCI_CLASS_REG
#endif

#endif /* end freebsd section */



/****************/
/* *** BSDI *** */
/****************/
#ifdef __bsdi__
#endif /* __bsdi__ */


/**************************/
/* *** OpenBSD/NetBSD *** */
/**************************/
#if defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/vnode.h>

#include <vm/vm.h>

#ifndef __NetBSD__
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#endif

#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#define BKTR_DEBUG
#ifdef BKTR_DEBUG
int bktr_debug = 0;
#define DPR(x)	(bktr_debug ? printf x : 0)
#else
#define DPR(x)
#endif
#endif /* __NetBSD__ || __OpenBSD__ */


#ifdef __NetBSD__
#include <dev/ic/bt8xx.h>	/* NetBSD location for .h files */
#include <dev/pci/bktr/bktr_reg.h>
#include <dev/pci/bktr/bktr_tuner.h>
#include <dev/pci/bktr/bktr_card.h>
#include <dev/pci/bktr/bktr_audio.h>
#include <dev/pci/bktr/bktr_core.h>
#include <dev/pci/bktr/bktr_os.h>
#else					/* Traditional location for .h files */
#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>	/* extensions to ioctl_meteor.h */
#include <dev/bktr/bktr_reg.h>
#include <dev/bktr/bktr_tuner.h>
#include <dev/bktr/bktr_card.h>
#include <dev/bktr/bktr_audio.h>
#include <dev/bktr/bktr_core.h>
#include <dev/bktr/bktr_os.h>
#if defined(BKTR_USE_FREEBSD_SMBUS)
#include <dev/bktr/bktr_i2c.h>
#endif
#endif



/****************************/
/* *** FreeBSD 4.x code *** */
/****************************/
#if (__FreeBSD_version >= 400000)

static int	bktr_probe( device_t dev );
static int	bktr_attach( device_t dev );
static int	bktr_detach( device_t dev );
static int	bktr_shutdown( device_t dev );
static void	bktr_intr(void *arg) { common_bktr_intr(arg); }

static device_method_t bktr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         bktr_probe),
	DEVMETHOD(device_attach,        bktr_attach),
	DEVMETHOD(device_detach,        bktr_detach),
	DEVMETHOD(device_shutdown,      bktr_shutdown),

	{ 0, 0 }
};

static driver_t bktr_driver = {
	"bktr",
	bktr_methods,
	sizeof(struct bktr_softc),
};

static devclass_t bktr_devclass;

static	d_open_t	bktr_open;
static	d_close_t	bktr_close;
static	d_read_t	bktr_read;
static	d_write_t	bktr_write;
static	d_ioctl_t	bktr_ioctl;
static	d_mmap_t	bktr_mmap;
static	d_poll_t	bktr_poll;

#define CDEV_MAJOR 92 
static struct cdevsw bktr_cdevsw = {
	/* open */	bktr_open,
	/* close */	bktr_close,
	/* read */	bktr_read,
	/* write */	bktr_write,
	/* ioctl */	bktr_ioctl,
	/* poll */	bktr_poll,
	/* mmap */	bktr_mmap,
	/* strategy */	nostrategy,
	/* name */	"bktr",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

DRIVER_MODULE(bktr, pci, bktr_driver, bktr_devclass, 0, 0);
#if (__FreeBSD_version > 410000)
MODULE_DEPEND(bktr, bktr_mem, 1,1,1);
MODULE_VERSION(bktr, 1);
#endif


/*
 * the boot time probe routine.
 */
static int
bktr_probe( device_t dev )
{
	unsigned int type = pci_get_devid(dev);
        unsigned int rev  = pci_get_revid(dev);

	if (PCI_VENDOR(type) == PCI_VENDOR_BROOKTREE)
	{
		switch (PCI_PRODUCT(type)) {
		case PCI_PRODUCT_BROOKTREE_BT848:
			if (rev == 0x12)
				device_set_desc(dev, "BrookTree 848A");
			else
				device_set_desc(dev, "BrookTree 848");
			return 0;
		case PCI_PRODUCT_BROOKTREE_BT849:
			device_set_desc(dev, "BrookTree 849A");
			return 0;
		case PCI_PRODUCT_BROOKTREE_BT878:
			device_set_desc(dev, "BrookTree 878");
			return 0;
		case PCI_PRODUCT_BROOKTREE_BT879:
			device_set_desc(dev, "BrookTree 879");
			return 0;
		}
	};

        return ENXIO;
}


/*
 * the attach routine.
 */
static int
bktr_attach( device_t dev )
{
	u_long		latency;
	u_long		fun;
	u_long		val;
	unsigned int	rev;
	unsigned int	unit;
	int		error = 0;
#ifdef BROOKTREE_IRQ
	u_long		old_irq, new_irq;
#endif 

        struct bktr_softc *bktr = device_get_softc(dev);

	unit = device_get_unit(dev);

	/* build the device name for bktr_name() */
	snprintf(bktr->bktr_xname, sizeof(bktr->bktr_xname), "bktr%d",unit);

	/*
	 * Enable bus mastering and Memory Mapped device
	 */
	val = pci_read_config(dev, PCIR_COMMAND, 4);
	val |= (PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, val, 4);

	/*
	 * Map control/status registers.
	 */
	bktr->mem_rid = PCIR_MAPS;
	bktr->res_mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &bktr->mem_rid,
					0, ~0, 1, RF_ACTIVE);


	if (!bktr->res_mem) {
		device_printf(dev, "could not map memory\n");
		error = ENXIO;
		goto fail;
	}
	bktr->memt = rman_get_bustag(bktr->res_mem);
	bktr->memh = rman_get_bushandle(bktr->res_mem);


	/*
	 * Disable the brooktree device
	 */
	OUTL(bktr, BKTR_INT_MASK, ALL_INTS_DISABLED);
	OUTW(bktr, BKTR_GPIO_DMA_CTL, FIFO_RISC_DISABLED);


#ifdef BROOKTREE_IRQ		/* from the configuration file */
	old_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	pci_conf_write(tag, PCI_INTERRUPT_REG, BROOKTREE_IRQ);
	new_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	printf("bktr%d: attach: irq changed from %d to %d\n",
		unit, (old_irq & 0xff), (new_irq & 0xff));
#endif 

	/*
	 * Allocate our interrupt.
	 */
	bktr->irq_rid = 0;
	bktr->res_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &bktr->irq_rid,
				0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (bktr->res_irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, bktr->res_irq, INTR_TYPE_TTY,
                               bktr_intr, bktr, &bktr->res_ih);
	if (error) {
		device_printf(dev, "could not setup irq\n");
		goto fail;

	}


	/* Update the Device Control Register */
	/* on Bt878 and Bt879 cards           */
	fun = pci_read_config( dev, 0x40, 2);
        fun = fun | 1;	/* Enable writes to the sub-system vendor ID */

#if defined( BKTR_430_FX_MODE )
	if (bootverbose) printf("Using 430 FX chipset compatibilty mode\n");
        fun = fun | 2;	/* Enable Intel 430 FX compatibility mode */
#endif

#if defined( BKTR_SIS_VIA_MODE )
	if (bootverbose) printf("Using SiS/VIA chipset compatibilty mode\n");
        fun = fun | 4;	/* Enable SiS/VIA compatibility mode (usefull for
                           OPTi chipset motherboards too */
#endif
	pci_write_config(dev, 0x40, fun, 2);


	/* XXX call bt848_i2c dependent attach() routine */
#if defined(BKTR_USE_FREEBSD_SMBUS)
	if (bt848_i2c_attach(unit, bktr, &bktr->i2c_sc))
		printf("bktr%d: i2c_attach: can't attach\n", unit);
#endif


/*
 * PCI latency timer.  32 is a good value for 4 bus mastering slots, if
 * you have more than four, then 16 would probably be a better value.
 */
#ifndef BROOKTREE_DEF_LATENCY_VALUE
#define BROOKTREE_DEF_LATENCY_VALUE	10
#endif
	latency = pci_read_config(dev, PCI_LATENCY_TIMER, 4);
	latency = (latency >> 8) & 0xff;
	if ( bootverbose ) {
		if (latency)
			printf("brooktree%d: PCI bus latency is", unit);
		else
			printf("brooktree%d: PCI bus latency was 0 changing to",
				unit);
	}
	if ( !latency ) {
		latency = BROOKTREE_DEF_LATENCY_VALUE;
		pci_write_config(dev, PCI_LATENCY_TIMER, latency<<8, 4);
	}
	if ( bootverbose ) {
		printf(" %d.\n", (int) latency);
	}

	/* read the pci device id and revision id */
	fun = pci_get_devid(dev);
        rev = pci_get_revid(dev);

	/* call the common attach code */
	common_bktr_attach( bktr, unit, fun, rev );

	/* make the device entries */
	bktr->bktrdev = make_dev(&bktr_cdevsw, unit,    
				0, 0, 0444, "bktr%d",  unit);
	bktr->tunerdev= make_dev(&bktr_cdevsw, unit+16,
				0, 0, 0444, "tuner%d", unit);
	bktr->vbidev  = make_dev(&bktr_cdevsw, unit+32,
				0, 0, 0444, "vbi%d"  , unit);


	/* if this is unit 0 (/dev/bktr0, /dev/tuner0, /dev/vbi0) then make */
	/* alias entries to /dev/bktr /dev/tuner and /dev/vbi */
#if (__FreeBSD_version >=500000)
	if (unit == 0) {
		bktr->bktrdev_alias = make_dev_alias(bktr->bktrdev,  "bktr");
		bktr->tunerdev_alias= make_dev_alias(bktr->tunerdev, "tuner");
		bktr->vbidev_alias  = make_dev_alias(bktr->vbidev,   "vbi");
	}
#endif

	return 0;

fail:
	if (bktr->res_irq)
		bus_release_resource(dev, SYS_RES_IRQ, bktr->irq_rid, bktr->res_irq);
	if (bktr->res_mem)
		bus_release_resource(dev, SYS_RES_IRQ, bktr->mem_rid, bktr->res_mem);
	return error;

}

/*
 * the detach routine.
 */
static int
bktr_detach( device_t dev )
{
	unsigned int	unit;

	struct bktr_softc *bktr = device_get_softc(dev);

	unit = device_get_unit(dev);

	/* Disable the brooktree device */
	OUTL(bktr, BKTR_INT_MASK, ALL_INTS_DISABLED);
	OUTW(bktr, BKTR_GPIO_DMA_CTL, FIFO_RISC_DISABLED);

	/* Note: We do not free memory for RISC programs, grab buffer, vbi buffers */
	/* The memory is retained by the bktr_mem module so we can unload and */
	/* then reload the main bktr driver module */

	/* Unregister the /dev/bktrN, tunerN and vbiN devices */
	destroy_dev(bktr->vbidev);
	destroy_dev(bktr->tunerdev);
	destroy_dev(bktr->bktrdev);

	/* If this is unit 0, then destroy the alias entries too */
#if (__FreeBSD_version >=500000)
	if (unit == 0) {
	    destroy_dev(bktr->vbidev_alias);
	    destroy_dev(bktr->tunerdev_alias);
	    destroy_dev(bktr->bktrdev_alias);
	}
#endif

	/*
	 * Deallocate resources.
	 */
	bus_teardown_intr(dev, bktr->res_irq, bktr->res_ih);
	bus_release_resource(dev, SYS_RES_IRQ, bktr->irq_rid, bktr->res_irq);
	bus_release_resource(dev, SYS_RES_MEMORY, bktr->mem_rid, bktr->res_mem);
	 
	return 0;
}

/*
 * the shutdown routine.
 */
static int
bktr_shutdown( device_t dev )
{
	struct bktr_softc *bktr = device_get_softc(dev);

	/* Disable the brooktree device */
	OUTL(bktr, BKTR_INT_MASK, ALL_INTS_DISABLED);
	OUTW(bktr, BKTR_GPIO_DMA_CTL, FIFO_RISC_DISABLED);

	return 0;
}


/*
 * Special Memory Allocation
 */
vm_offset_t
get_bktr_mem( int unit, unsigned size )
{
	vm_offset_t	addr = 0;

	addr = vm_page_alloc_contig(size, 0, 0xffffffff, 1<<24);
	if (addr == 0)
		addr = vm_page_alloc_contig(size, 0, 0xffffffff, PAGE_SIZE);
	if (addr == 0) {
		printf("bktr%d: Unable to allocate %d bytes of memory.\n",
			unit, size);
	}

	return( addr );
}


/*---------------------------------------------------------
**
**	BrookTree 848 character device driver routines
**
**---------------------------------------------------------
*/

#define VIDEO_DEV	0x00
#define TUNER_DEV	0x01
#define VBI_DEV		0x02

#define UNIT(x)		((x) & 0x0f)
#define FUNCTION(x)	(x >> 4)

/*
 * 
 */
int
bktr_open( dev_t dev, int flags, int fmt, struct proc *p )
{
	bktr_ptr_t	bktr;
	int		unit;
	int		result;

	unit = UNIT( minor(dev) );

	/* Get the device data */
	bktr = (struct bktr_softc*)devclass_get_softc(bktr_devclass, unit);
	if (bktr == NULL) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	if (!(bktr->flags & METEOR_INITALIZED)) /* device not found */
		return( ENXIO );	

	/* Record that the device is now busy */
	device_busy(devclass_get_device(bktr_devclass, unit)); 


	if (bt848_card != -1) {
	  if ((bt848_card >> 8   == unit ) &&
	     ( (bt848_card & 0xff) < Bt848_MAX_CARD )) {
	    if ( bktr->bt848_card != (bt848_card & 0xff) ) {
	      bktr->bt848_card = (bt848_card & 0xff);
	      probeCard(bktr, FALSE, unit);
	    }
	  }
	}

	if (bt848_tuner != -1) {
	  if ((bt848_tuner >> 8   == unit ) &&
	     ( (bt848_tuner & 0xff) < Bt848_MAX_TUNER )) {
	    if ( bktr->bt848_tuner != (bt848_tuner & 0xff) ) {
	      bktr->bt848_tuner = (bt848_tuner & 0xff);
	      probeCard(bktr, FALSE, unit);
	    }
	  }
	}

	if (bt848_reverse_mute != -1) {
	  if ((bt848_reverse_mute >> 8)   == unit ) {
	    bktr->reverse_mute = bt848_reverse_mute & 0xff;
	  }
	}

	if (bt848_slow_msp_audio != -1) {
	  if ((bt848_slow_msp_audio >> 8) == unit ) {
	      bktr->slow_msp_audio = (bt848_slow_msp_audio & 0xff);
	  }
	}

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		result = video_open( bktr );
		break;
	case TUNER_DEV:
		result = tuner_open( bktr );
		break;
	case VBI_DEV:
		result = vbi_open( bktr );
		break;
	default:
		result = ENXIO;
		break;
	}

	/* If there was an error opening the device, undo the busy status */
	if (result != 0)
		device_unbusy(devclass_get_device(bktr_devclass, unit)); 
	return( result );
}


/*
 * 
 */
int
bktr_close( dev_t dev, int flags, int fmt, struct proc *p )
{
	bktr_ptr_t	bktr;
	int		unit;
	int		result;

	unit = UNIT( minor(dev) );

	/* Get the device data */
	bktr = (struct bktr_softc*)devclass_get_softc(bktr_devclass, unit);
	if (bktr == NULL) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		result = video_close( bktr );
		break;
	case TUNER_DEV:
		result = tuner_close( bktr );
		break;
	case VBI_DEV:
		result = vbi_close( bktr );
		break;
	default:
		return (ENXIO);
		break;
	}

	device_unbusy(devclass_get_device(bktr_devclass, unit)); 
	return( result );
}


/*
 * 
 */
int
bktr_read( dev_t dev, struct uio *uio, int ioflag )
{
	bktr_ptr_t	bktr;
	int		unit;
	
	unit = UNIT(minor(dev));

	/* Get the device data */
	bktr = (struct bktr_softc*)devclass_get_softc(bktr_devclass, unit);
	if (bktr == NULL) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_read( bktr, unit, dev, uio ) );
	case VBI_DEV:
		return( vbi_read( bktr, uio, ioflag ) );
	}
        return( ENXIO );
}


/*
 * 
 */
int
bktr_write( dev_t dev, struct uio *uio, int ioflag )
{
	return( EINVAL ); /* XXX or ENXIO ? */
}


/*
 * 
 */
int
bktr_ioctl( dev_t dev, ioctl_cmd_t cmd, caddr_t arg, int flag, struct proc* pr )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT(minor(dev));

	/* Get the device data */
	bktr = (struct bktr_softc*)devclass_get_softc(bktr_devclass, unit);
	if (bktr == NULL) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	if (bktr->bigbuf == 0)	/* no frame buffer allocated (ioctl failed) */
		return( ENOMEM );

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_ioctl( bktr, unit, cmd, arg, pr ) );
	case TUNER_DEV:
		return( tuner_ioctl( bktr, unit, cmd, arg, pr ) );
	}

	return( ENXIO );
}


/*
 * 
 */
int
bktr_mmap( dev_t dev, vm_offset_t offset, int nprot )
{
	int		unit;
	bktr_ptr_t	bktr;

	unit = UNIT(minor(dev));

	if (FUNCTION(minor(dev)) > 0)	/* only allow mmap on /dev/bktr[n] */
		return( -1 );

	/* Get the device data */
	bktr = (struct bktr_softc*)devclass_get_softc(bktr_devclass, unit);
	if (bktr == NULL) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	if (nprot & PROT_EXEC)
		return( -1 );

	if (offset < 0)
		return( -1 );

	if (offset >= bktr->alloc_pages * PAGE_SIZE)
		return( -1 );

	return( atop(vtophys(bktr->bigbuf) + offset) );
}

int bktr_poll( dev_t dev, int events, struct proc *p)
{
	int		unit;
	bktr_ptr_t	bktr;
	int revents = 0; 
	DECLARE_INTR_MASK(s);

	unit = UNIT(minor(dev));

	/* Get the device data */
	bktr = (struct bktr_softc*)devclass_get_softc(bktr_devclass, unit);
	if (bktr == NULL) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	DISABLE_INTR(s);

	if (events & (POLLIN | POLLRDNORM)) {

		switch ( FUNCTION( minor(dev) ) ) {
		case VBI_DEV:
			if(bktr->vbisize == 0)
				selrecord(p, &bktr->vbi_select);
			else
				revents |= events & (POLLIN | POLLRDNORM);
			break;
		}
	}

	ENABLE_INTR(s);

	return (revents);
}

#endif		/* FreeBSD 4.x specific kernel interface routines */

/**********************************/
/* *** FreeBSD 2.2.x and 3.x  *** */
/**********************************/

#if ((__FreeBSD__ == 2) || (__FreeBSD__ == 3))

static bktr_reg_t brooktree[ NBKTR ];

static const char*	bktr_probe( pcici_t tag, pcidi_t type );
static void		bktr_attach( pcici_t tag, int unit );
static void		bktr_intr(void *arg) { common_bktr_intr(arg); }

static u_long	bktr_count;

static struct	pci_device bktr_device = {
	"bktr",
	bktr_probe,
	bktr_attach,
	&bktr_count
};

DATA_SET (pcidevice_set, bktr_device);

static	d_open_t	bktr_open;
static	d_close_t	bktr_close;
static	d_read_t	bktr_read;
static	d_write_t	bktr_write;
static	d_ioctl_t	bktr_ioctl;
static	d_mmap_t	bktr_mmap;
static	d_poll_t	bktr_poll;

#define CDEV_MAJOR 92 
static struct cdevsw bktr_cdevsw = 
{
	bktr_open,	bktr_close,	bktr_read,	bktr_write,
	bktr_ioctl,	nostop,		nullreset,	nodevtotty,
	bktr_poll,	bktr_mmap,	NULL,		"bktr",
	NULL,		-1
};

static int bktr_devsw_installed;

static void
bktr_drvinit( void *unused )
{
	dev_t dev;

	if ( ! bktr_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&bktr_cdevsw, NULL);
		bktr_devsw_installed = 1;
	}
}

SYSINIT(bktrdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,bktr_drvinit,NULL)

/*
 * the boot time probe routine.
 */
static const char*
bktr_probe( pcici_t tag, pcidi_t type )
{
        unsigned int rev = pci_conf_read( tag, PCIR_REVID) & 0x000000ff;

	if (PCI_VENDOR(type) == PCI_VENDOR_BROOKTREE)
	{
		switch (PCI_PRODUCT(type)) {
		case PCI_PRODUCT_BROOKTREE_BT848:
			if (rev == 0x12) return("BrookTree 848A");
			else             return("BrookTree 848");
		case PCI_PRODUCT_BROOKTREE_BT849:
			return("BrookTree 849A");
		case PCI_PRODUCT_BROOKTREE_BT878:
			return("BrookTree 878");
		case PCI_PRODUCT_BROOKTREE_BT879:
			return("BrookTree 879");
		}
	};

	return ((char *)0);
}

/*
 * the attach routine.
 */
static	void
bktr_attach( pcici_t tag, int unit )
{
	bktr_ptr_t	bktr;
	u_long		latency;
	u_long		fun;
	unsigned int	rev;
	unsigned long	base;
#ifdef BROOKTREE_IRQ
	u_long		old_irq, new_irq;
#endif 

	bktr = &brooktree[unit];

	if (unit >= NBKTR) {
		printf("brooktree%d: attach: only %d units configured.\n",
		        unit, NBKTR);
		printf("brooktree%d: attach: invalid unit number.\n", unit);
		return;
	}

	/* build the device name for bktr_name() */
	snprintf(bktr->bktr_xname, sizeof(bktr->bktr_xname), "bktr%d",unit);

	/* Enable Memory Mapping */
	fun = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(tag, PCI_COMMAND_STATUS_REG, fun | 2);

	/* Enable Bus Mastering */
	fun = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(tag, PCI_COMMAND_STATUS_REG, fun | 4);

	bktr->tag = tag;


	/*
	 * Map control/status registers
	 */
	pci_map_mem( tag, PCI_MAP_REG_START, (vm_offset_t *) &base,
		     &bktr->phys_base );
#if (__FreeBSD_version >= 300000)
	bktr->memt = I386_BUS_SPACE_MEM; /* XXX should use proper bus space */
	bktr->memh = (bus_space_handle_t)base; /* XXX functions here */
#endif

	/*
	 * Disable the brooktree device
	 */
	OUTL(bktr, BKTR_INT_MASK, ALL_INTS_DISABLED);
	OUTW(bktr, BKTR_GPIO_DMA_CTL, FIFO_RISC_DISABLED);

#ifdef BROOKTREE_IRQ		/* from the configuration file */
	old_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	pci_conf_write(tag, PCI_INTERRUPT_REG, BROOKTREE_IRQ);
	new_irq = pci_conf_read(tag, PCI_INTERRUPT_REG);
	printf("bktr%d: attach: irq changed from %d to %d\n",
		unit, (old_irq & 0xff), (new_irq & 0xff));
#endif 

	/*
	 * setup the interrupt handling routine
	 */
	pci_map_int(tag, bktr_intr, (void*) bktr, &tty_imask);


	/* Update the Device Control Register */
	/* on Bt878 and Bt879 cards */
	fun = pci_conf_read(tag, 0x40);
        fun = fun | 1;	/* Enable writes to the sub-system vendor ID */

#if defined( BKTR_430_FX_MODE )
	if (bootverbose) printf("Using 430 FX chipset compatibilty mode\n");
        fun = fun | 2;	/* Enable Intel 430 FX compatibility mode */
#endif

#if defined( BKTR_SIS_VIA_MODE )
	if (bootverbose) printf("Using SiS/VIA chipset compatibilty mode\n");
        fun = fun | 4;	/* Enable SiS/VIA compatibility mode (usefull for
                           OPTi chipset motherboards too */
#endif
	pci_conf_write(tag, 0x40, fun);


	/* XXX call bt848_i2c dependent attach() routine */
#if defined(BKTR_USE_FREEBSD_SMBUS)
	if (bt848_i2c_attach(unit, bktr, &bktr->i2c_sc))
		printf("bktr%d: i2c_attach: can't attach\n", unit);
#endif


/*
 * PCI latency timer.  32 is a good value for 4 bus mastering slots, if
 * you have more than four, then 16 would probably be a better value.
 */
#ifndef BROOKTREE_DEF_LATENCY_VALUE
#define BROOKTREE_DEF_LATENCY_VALUE	10
#endif
	latency = pci_conf_read(tag, PCI_LATENCY_TIMER);
	latency = (latency >> 8) & 0xff;
	if ( bootverbose ) {
		if (latency)
			printf("brooktree%d: PCI bus latency is", unit);
		else
			printf("brooktree%d: PCI bus latency was 0 changing to",
				unit);
	}
	if ( !latency ) {
		latency = BROOKTREE_DEF_LATENCY_VALUE;
		pci_conf_write(tag, PCI_LATENCY_TIMER,	latency<<8);
	}
	if ( bootverbose ) {
		printf(" %d.\n", (int) latency);
	}


	/* read the pci device id and revision id */
	fun = pci_conf_read(tag, PCI_ID_REG);
        rev = pci_conf_read(tag, PCIR_REVID) & 0x000000ff;

	/* call the common attach code */
	common_bktr_attach( bktr, unit, fun, rev );

}


/*
 * Special Memory Allocation
 */
vm_offset_t
get_bktr_mem( int unit, unsigned size )
{
	vm_offset_t	addr = 0;

	addr = vm_page_alloc_contig(size, 0x100000, 0xffffffff, 1<<24);
	if (addr == 0)
		addr = vm_page_alloc_contig(size, 0x100000, 0xffffffff,
								PAGE_SIZE);
	if (addr == 0) {
		printf("bktr%d: Unable to allocate %d bytes of memory.\n",
			unit, size);
	}

	return( addr );
}

/*---------------------------------------------------------
**
**	BrookTree 848 character device driver routines
**
**---------------------------------------------------------
*/


#define VIDEO_DEV	0x00
#define TUNER_DEV	0x01
#define VBI_DEV		0x02

#define UNIT(x)		((x) & 0x0f)
#define FUNCTION(x)	((x >> 4) & 0x0f)


/*
 * 
 */
int
bktr_open( dev_t dev, int flags, int fmt, struct proc *p )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT( minor(dev) );
	if (unit >= NBKTR)			/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[ unit ]);

	if (!(bktr->flags & METEOR_INITALIZED)) /* device not found */
		return( ENXIO );	


	if (bt848_card != -1) {
	  if ((bt848_card >> 8   == unit ) &&
	     ( (bt848_card & 0xff) < Bt848_MAX_CARD )) {
	    if ( bktr->bt848_card != (bt848_card & 0xff) ) {
	      bktr->bt848_card = (bt848_card & 0xff);
	      probeCard(bktr, FALSE, unit);
	    }
	  }
	}

	if (bt848_tuner != -1) {
	  if ((bt848_tuner >> 8   == unit ) &&
	     ( (bt848_tuner & 0xff) < Bt848_MAX_TUNER )) {
	    if ( bktr->bt848_tuner != (bt848_tuner & 0xff) ) {
	      bktr->bt848_tuner = (bt848_tuner & 0xff);
	      probeCard(bktr, FALSE, unit);
	    }
	  }
	}

	if (bt848_reverse_mute != -1) {
	  if ((bt848_reverse_mute >> 8)   == unit ) {
	    bktr->reverse_mute = bt848_reverse_mute & 0xff;
	  }
	}

	if (bt848_slow_msp_audio != -1) {
	  if ((bt848_slow_msp_audio >> 8) == unit ) {
	      bktr->slow_msp_audio = (bt848_slow_msp_audio & 0xff);
	  }
	}

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_open( bktr ) );
	case TUNER_DEV:
		return( tuner_open( bktr ) );
	case VBI_DEV:
		return( vbi_open( bktr ) );
	}
	return( ENXIO );
}


/*
 * 
 */
int
bktr_close( dev_t dev, int flags, int fmt, struct proc *p )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT( minor(dev) );
	if (unit >= NBKTR)			/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[ unit ]);

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_close( bktr ) );
	case TUNER_DEV:
		return( tuner_close( bktr ) );
	case VBI_DEV:
		return( vbi_close( bktr ) );
	}

	return( ENXIO );
}

/*
 * 
 */
int
bktr_read( dev_t dev, struct uio *uio, int ioflag )
{
	bktr_ptr_t	bktr;
	int		unit;
	
	unit = UNIT(minor(dev));
	if (unit >= NBKTR)	/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[unit]);

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_read( bktr, unit, dev, uio ) );
	case VBI_DEV:
		return( vbi_read( bktr, uio, ioflag ) );
	}
        return( ENXIO );
}


/*
 * 
 */
int
bktr_write( dev_t dev, struct uio *uio, int ioflag )
{
	return( EINVAL ); /* XXX or ENXIO ? */
}

/*
 * 
 */
int
bktr_ioctl( dev_t dev, ioctl_cmd_t cmd, caddr_t arg, int flag, struct proc* pr )
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT(minor(dev));
	if (unit >= NBKTR)	/* unit out of range */
		return( ENXIO );

	bktr = &(brooktree[ unit ]);

	if (bktr->bigbuf == 0)	/* no frame buffer allocated (ioctl failed) */
		return( ENOMEM );

	switch ( FUNCTION( minor(dev) ) ) {
	case VIDEO_DEV:
		return( video_ioctl( bktr, unit, cmd, arg, pr ) );
	case TUNER_DEV:
		return( tuner_ioctl( bktr, unit, cmd, arg, pr ) );
	}

	return( ENXIO );
}

/*
 * bktr_mmap.
 * Note: 2.2.5/2.2.6/2.2.7/3.0 users must manually
 * edit the line below and change  "vm_offset_t" to "int"
 */
int bktr_mmap( dev_t dev, vm_offset_t offset, int nprot )

{
	int		unit;
	bktr_ptr_t	bktr;

	unit = UNIT(minor(dev));

	if (unit >= NBKTR || FUNCTION(minor(dev)) > 0)
		return( -1 );

	bktr = &(brooktree[ unit ]);

	if (nprot & PROT_EXEC)
		return( -1 );

	if (offset < 0)
		return( -1 );

	if (offset >= bktr->alloc_pages * PAGE_SIZE)
		return( -1 );

	return( i386_btop(vtophys(bktr->bigbuf) + offset) );
}

int bktr_poll( dev_t dev, int events, struct proc *p)
{
	int		unit;
	bktr_ptr_t	bktr;
	int revents = 0; 

	unit = UNIT(minor(dev));

	if (unit >= NBKTR)
		return( -1 );

	bktr = &(brooktree[ unit ]);

	disable_intr();

	if (events & (POLLIN | POLLRDNORM)) {

		switch ( FUNCTION( minor(dev) ) ) {
		case VBI_DEV:
			if(bktr->vbisize == 0)
				selrecord(p, &bktr->vbi_select);
			else
				revents |= events & (POLLIN | POLLRDNORM);
			break;
		}
	}

	enable_intr();

	return (revents);
}


#endif		/* FreeBSD 2.2.x and 3.x specific kernel interface routines */


/*****************/
/* *** BSDI  *** */
/*****************/

#if defined(__bsdi__)
#endif		/* __bsdi__ BSDI specific kernel interface routines */


/*****************************/
/* *** OpenBSD / NetBSD  *** */
/*****************************/
#if defined(__NetBSD__) || defined(__OpenBSD__)

#define IPL_VIDEO       IPL_BIO         /* XXX */

static	int		bktr_intr(void *arg) { return common_bktr_intr(arg); }

#define bktr_open       bktropen
#define bktr_close      bktrclose
#define bktr_read       bktrread
#define bktr_write      bktrwrite
#define bktr_ioctl      bktrioctl
#define bktr_mmap       bktrmmap

vm_offset_t vm_page_alloc_contig(vm_offset_t, vm_offset_t,
                                 vm_offset_t, vm_offset_t);

#if defined(__OpenBSD__)
static int      bktr_probe __P((struct device *, void *, void *));
#else
static int      bktr_probe __P((struct device *, struct cfdata *, void *));
#endif
static void     bktr_attach __P((struct device *, struct device *, void *));

struct cfattach bktr_ca = {
        sizeof(struct bktr_softc), bktr_probe, bktr_attach
};

#if defined(__NetBSD__)
extern struct cfdriver bktr_cd;
#else
struct cfdriver bktr_cd = {
        NULL, "bktr", DV_DULL
};
#endif

int
bktr_probe(parent, match, aux)
	struct device *parent;
#if defined(__OpenBSD__)
        void *match;
#else
        struct cfdata *match;
#endif
        void *aux;
{
        struct pci_attach_args *pa = aux;

        if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROOKTREE &&
            (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROOKTREE_BT848 ||
             PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROOKTREE_BT849 ||
             PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROOKTREE_BT878 ||
             PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROOKTREE_BT879))
                return 1;

        return 0;
}


/*
 * the attach routine.
 */
static void
bktr_attach(struct device *parent, struct device *self, void *aux)
{
	bktr_ptr_t	bktr;
	u_long		latency;
	u_long		fun;
	unsigned int	rev;

#if defined(__OpenBSD__)
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;

	pci_intr_handle_t ih;
	const char *intrstr;
	int retval;
	int unit;

	bktr = (bktr_ptr_t)self;
	unit = bktr->bktr_dev.dv_unit;

	bktr->pc = pa->pa_pc;
	bktr->tag = pa->pa_tag;
        bktr->dmat = pa->pa_dmat;

	/*
	 * map memory
	 */
	bktr->memt = pa->pa_memt;
	retval = pci_mem_find(pc, pa->pa_tag, PCI_MAPREG_START, 
			      &bktr->phys_base, &bktr->obmemsz, NULL);
	if (!retval)
		retval = bus_space_map(pa->pa_memt, bktr->phys_base,
				       bktr->obmemsz, 0, &bktr->memh);
	if (retval) {
		printf(": couldn't map memory\n");
		return;
	}


	/*
	 * map interrupt
	 */
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);

	bktr->ih = pci_intr_establish(pa->pa_pc, ih, IPL_VIDEO,
				      bktr_intr, bktr, bktr->bktr_dev.dv_xname);
	if (bktr->ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)	
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	if (intrstr != NULL)
		printf(": %s\n", intrstr);
#endif /* __OpenBSD__ */

#if defined(__NetBSD__) 
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr;
	int retval;
	int unit;

	bktr = (bktr_ptr_t)self;
	unit = bktr->bktr_dev.dv_unit;
        bktr->dmat = pa->pa_dmat;

	printf("\n");

	/*
	 * map memory
	 */
	retval = pci_mapreg_map(pa, PCI_MAPREG_START,
				PCI_MAPREG_TYPE_MEM
				| PCI_MAPREG_MEM_TYPE_32BIT, 0,
				&bktr->memt, &bktr->memh, NULL,
				&bktr->obmemsz);
	DPR(("pci_mapreg_map: memt %x, memh %x, size %x\n",
	     bktr->memt, (u_int)bktr->memh, (u_int)bktr->obmemsz));
	if (retval) {
		printf("%s: couldn't map memory\n", bktr_name(bktr));
		return;
	}

	/*
	 * Disable the brooktree device
	 */
	OUTL(bktr, BKTR_INT_MASK, ALL_INTS_DISABLED);
	OUTW(bktr, BKTR_GPIO_DMA_CTL, FIFO_RISC_DISABLED);
	
	/*
	 * map interrupt
	 */
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n",
		       bktr_name(bktr));
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	bktr->ih = pci_intr_establish(pa->pa_pc, ih, IPL_VIDEO,
				      bktr_intr, bktr);
	if (bktr->ih == NULL) {
		printf("%s: couldn't establish interrupt",
		       bktr_name(bktr));
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", bktr_name(bktr),
		       intrstr);
#endif /* __NetBSD__ */
	
/*
 * PCI latency timer.  32 is a good value for 4 bus mastering slots, if
 * you have more than four, then 16 would probably be a better value.
 */
#ifndef BROOKTREE_DEF_LATENCY_VALUE
#define BROOKTREE_DEF_LATENCY_VALUE	10
#endif
	latency = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_LATENCY_TIMER);
	latency = (latency >> 8) & 0xff;

	if (!latency) {
		if (bootverbose) {
			printf("%s: PCI bus latency was 0 changing to %d",
			       bktr_name(bktr), BROOKTREE_DEF_LATENCY_VALUE);
		}
		latency = BROOKTREE_DEF_LATENCY_VALUE;
		pci_conf_write(pa->pa_pc, pa->pa_tag, 
			       PCI_LATENCY_TIMER, latency<<8);
	}


	/* Enabled Bus Master
	   XXX: check if all old DMA is stopped first (e.g. after warm
	   boot) */
	fun = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       fun | PCI_COMMAND_MASTER_ENABLE);

	/* read the pci id and determine the card type */
	fun = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ID_REG);
        rev = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG) & 0x000000ff;

	common_bktr_attach(bktr, unit, fun, rev);
}


/*
 * Special Memory Allocation
 */
vm_offset_t
get_bktr_mem(bktr, dmapp, size)
        bktr_ptr_t bktr;
        bus_dmamap_t *dmapp;
        unsigned int size;
{
        bus_dma_tag_t dmat = bktr->dmat;
        bus_dma_segment_t seg;
        bus_size_t align;
        int rseg;
        caddr_t kva;

        /*
         * Allocate a DMA area
         */
        align = 1 << 24;
        if (bus_dmamem_alloc(dmat, size, align, 0, &seg, 1,
                             &rseg, BUS_DMA_NOWAIT)) {
                align = PAGE_SIZE;
                if (bus_dmamem_alloc(dmat, size, align, 0, &seg, 1,
                                     &rseg, BUS_DMA_NOWAIT)) {
                        printf("%s: Unable to dmamem_alloc of %d bytes\n",
			       bktr_name(bktr), size);
                        return 0;
                }
        }
        if (bus_dmamem_map(dmat, &seg, rseg, size,
                           &kva, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
                printf("%s: Unable to dmamem_map of %d bytes\n",
                        bktr_name(bktr), size);
                bus_dmamem_free(dmat, &seg, rseg);
                return 0;
        }
#ifdef __OpenBSD__
        bktr->dm_mapsize = size;
#endif
        /*
         * Create and locd the DMA map for the DMA area
         */
        if (bus_dmamap_create(dmat, size, 1, size, 0, BUS_DMA_NOWAIT, dmapp)) {
                printf("%s: Unable to dmamap_create of %d bytes\n",
                        bktr_name(bktr), size);
                bus_dmamem_unmap(dmat, kva, size);
                bus_dmamem_free(dmat, &seg, rseg);
                return 0;
        }
        if (bus_dmamap_load(dmat, *dmapp, kva, size, NULL, BUS_DMA_NOWAIT)) {
                printf("%s: Unable to dmamap_load of %d bytes\n",
                        bktr_name(bktr), size);
                bus_dmamem_unmap(dmat, kva, size);
                bus_dmamem_free(dmat, &seg, rseg);
                bus_dmamap_destroy(dmat, *dmapp);
                return 0;
        }
        return (vm_offset_t)kva;
}

void
free_bktr_mem(bktr, dmap, kva)
        bktr_ptr_t bktr;
        bus_dmamap_t dmap;
        vm_offset_t kva;
{
        bus_dma_tag_t dmat = bktr->dmat;

#ifdef __NetBSD__ 
        bus_dmamem_unmap(dmat, (caddr_t)kva, dmap->dm_mapsize);
#else
        bus_dmamem_unmap(dmat, (caddr_t)kva, bktr->dm_mapsize);
#endif
        bus_dmamem_free(dmat, dmap->dm_segs, 1);
        bus_dmamap_destroy(dmat, dmap);
}


/*---------------------------------------------------------
**
**	BrookTree 848 character device driver routines
**
**---------------------------------------------------------
*/


#define VIDEO_DEV	0x00
#define TUNER_DEV	0x01
#define VBI_DEV		0x02

#define UNIT(x)         (minor((x) & 0x0f))
#define FUNCTION(x)     (minor((x >> 4) & 0x0f))

/*
 * 
 */
int
bktr_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT(dev);

	/* unit out of range */
	if ((unit > bktr_cd.cd_ndevs) || (bktr_cd.cd_devs[unit] == NULL))
		return(ENXIO);

	bktr = bktr_cd.cd_devs[unit];

	if (!(bktr->flags & METEOR_INITALIZED)) /* device not found */
		return(ENXIO);	

	switch (FUNCTION(dev)) {
	case VIDEO_DEV:
		return(video_open(bktr));
	case TUNER_DEV:
		return(tuner_open(bktr));
	case VBI_DEV:
		return(vbi_open(bktr));
	}

	return(ENXIO);
}


/*
 * 
 */
int
bktr_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT(dev);

	bktr = bktr_cd.cd_devs[unit];

	switch (FUNCTION(dev)) {
	case VIDEO_DEV:
		return(video_close(bktr));
	case TUNER_DEV:
		return(tuner_close(bktr));
	case VBI_DEV:
		return(vbi_close(bktr));
	}

	return(ENXIO);
}

/*
 * 
 */
int
bktr_read(dev_t dev, struct uio *uio, int ioflag)
{
	bktr_ptr_t	bktr;
	int		unit;
	
	unit = UNIT(dev);

	bktr = bktr_cd.cd_devs[unit];

	switch (FUNCTION(dev)) {
	case VIDEO_DEV:
		return(video_read(bktr, unit, dev, uio));
	case VBI_DEV:
		return(vbi_read(bktr, uio, ioflag));
	}

        return(ENXIO);
}


/*
 * 
 */
int
bktr_write(dev_t dev, struct uio *uio, int ioflag)
{
	/* operation not supported */
	return(EOPNOTSUPP);
}

/*
 * 
 */
int
bktr_ioctl(dev_t dev, ioctl_cmd_t cmd, caddr_t arg, int flag, struct proc* pr)
{
	bktr_ptr_t	bktr;
	int		unit;

	unit = UNIT(dev);

	bktr = bktr_cd.cd_devs[unit];

	if (bktr->bigbuf == 0)	/* no frame buffer allocated (ioctl failed) */
		return(ENOMEM);

	switch (FUNCTION(dev)) {
	case VIDEO_DEV:
		return(video_ioctl(bktr, unit, cmd, arg, pr));
	case TUNER_DEV:
		return(tuner_ioctl(bktr, unit, cmd, arg, pr));
	}

	return(ENXIO);
}

/*
 * 
 */
paddr_t
bktr_mmap(dev_t dev, off_t offset, int nprot)
{
	int		unit;
	bktr_ptr_t	bktr;

	unit = UNIT(dev);

	if (FUNCTION(dev) > 0)	/* only allow mmap on /dev/bktr[n] */
		return(-1);

	bktr = bktr_cd.cd_devs[unit];

	if ((vaddr_t)offset < 0)
		return(-1);

	if ((vaddr_t)offset >= bktr->alloc_pages * PAGE_SIZE)
		return(-1);

#ifdef __NetBSD__
	return (bus_dmamem_mmap(bktr->dmat, bktr->dm_mem->dm_segs, 1,
				(vaddr_t)offset, nprot, BUS_DMA_WAITOK));
#else
	return(i386_btop(vtophys(bktr->bigbuf) + offset));
#endif
}

#endif /* __NetBSD__ || __OpenBSD__ */
