/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * FreeBSD-specific code.
 */

#include <sys/param.h>
#include <sys/cons.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <dev/twe/twe_compat.h>
#include <dev/twe/twereg.h>
#include <dev/twe/tweio.h>
#include <dev/twe/twevar.h>
#include <dev/twe/twe_tables.h>

#include <sys/devicestat.h>

static devclass_t	twe_devclass;

#ifdef TWE_DEBUG
static u_int32_t	twed_bio_in;
#define TWED_BIO_IN	twed_bio_in++
static u_int32_t	twed_bio_out;
#define TWED_BIO_OUT	twed_bio_out++
#else
#define TWED_BIO_IN
#define TWED_BIO_OUT
#endif

/********************************************************************************
 ********************************************************************************
                                                         Control device interface
 ********************************************************************************
 ********************************************************************************/

static	d_open_t		twe_open;
static	d_close_t		twe_close;
static	d_ioctl_t		twe_ioctl_wrapper;

#define TWE_CDEV_MAJOR  146

static struct cdevsw twe_cdevsw = {
    twe_open,
    twe_close,
    noread,
    nowrite,
    twe_ioctl_wrapper,
    nopoll,
    nommap,
    nostrategy,
    "twe",
    TWE_CDEV_MAJOR,
    nodump,
    nopsize,
    0
};

/********************************************************************************
 * Accept an open operation on the control device.
 */
static int
twe_open(dev_t dev, int flags, int fmt, d_thread_t *td)
{
    int			unit = minor(dev);
    struct twe_softc	*sc = devclass_get_softc(twe_devclass, unit);

    sc->twe_state |= TWE_STATE_OPEN;
    return(0);
}

/********************************************************************************
 * Accept the last close on the control device.
 */
static int
twe_close(dev_t dev, int flags, int fmt, d_thread_t *td)
{
    int			unit = minor(dev);
    struct twe_softc	*sc = devclass_get_softc(twe_devclass, unit);

    sc->twe_state &= ~TWE_STATE_OPEN;
    return (0);
}

/********************************************************************************
 * Handle controller-specific control operations.
 */
static int
twe_ioctl_wrapper(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, d_thread_t *td)
{
    struct twe_softc		*sc = (struct twe_softc *)dev->si_drv1;
    
    return(twe_ioctl(sc, cmd, addr));
}

/********************************************************************************
 ********************************************************************************
                                                             PCI device interface
 ********************************************************************************
 ********************************************************************************/

static int	twe_probe(device_t dev);
static int	twe_attach(device_t dev);
static void	twe_free(struct twe_softc *sc);
static int	twe_detach(device_t dev);
static void	twe_shutdown(device_t dev);
static int	twe_suspend(device_t dev);
static int	twe_resume(device_t dev);
static void	twe_pci_intr(void *arg);
static void	twe_intrhook(void *arg);

static device_method_t twe_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	twe_probe),
    DEVMETHOD(device_attach,	twe_attach),
    DEVMETHOD(device_detach,	twe_detach),
    DEVMETHOD(device_shutdown,	twe_shutdown),
    DEVMETHOD(device_suspend,	twe_suspend),
    DEVMETHOD(device_resume,	twe_resume),

    DEVMETHOD(bus_print_child,	bus_generic_print_child),
    DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
    { 0, 0 }
};

static driver_t twe_pci_driver = {
	"twe",
	twe_methods,
	sizeof(struct twe_softc)
};

#ifdef TWE_OVERRIDE
DRIVER_MODULE(Xtwe, pci, twe_pci_driver, twe_devclass, 0, 0);
#else
DRIVER_MODULE(twe, pci, twe_pci_driver, twe_devclass, 0, 0);
#endif

/********************************************************************************
 * Match a 3ware Escalade ATA RAID controller.
 */
static int
twe_probe(device_t dev)
{

    debug_called(4);

    if ((pci_get_vendor(dev) == TWE_VENDOR_ID) &&
	((pci_get_device(dev) == TWE_DEVICE_ID) || 
	 (pci_get_device(dev) == TWE_DEVICE_ID_ASIC))) {
	device_set_desc(dev, TWE_DEVICE_NAME);
#ifdef TWE_OVERRIDE
	return(0);
#else
	return(-10);
#endif
    }
    return(ENXIO);
}

/********************************************************************************
 * Allocate resources, initialise the controller.
 */
static int
twe_attach(device_t dev)
{
    struct twe_softc	*sc;
    int			rid, error;
    u_int32_t		command;

    debug_called(4);

    /*
     * Initialise the softc structure.
     */
    sc = device_get_softc(dev);
    sc->twe_dev = dev;

    sysctl_ctx_init(&sc->sysctl_ctx);
    sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
	device_get_nameunit(dev), CTLFLAG_RD, 0, "");
    if (sc->sysctl_tree == NULL) {
	twe_printf(sc, "cannot add sysctl tree node\n");
	return (ENXIO);
    }
    SYSCTL_ADD_STRING(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	OID_AUTO, "driver_version", CTLFLAG_RD, "$Revision$", 0,
	"TWE driver version");

    /*
     * Make sure we are going to be able to talk to this board.
     */
    command = pci_read_config(dev, PCIR_COMMAND, 2);
    if ((command & PCIM_CMD_PORTEN) == 0) {
	twe_printf(sc, "register window not available\n");
	return(ENXIO);
    }
    /*
     * Force the busmaster enable bit on, in case the BIOS forgot.
     */
    command |= PCIM_CMD_BUSMASTEREN;
    pci_write_config(dev, PCIR_COMMAND, command, 2);

    /*
     * Allocate the PCI register window.
     */
    rid = TWE_IO_CONFIG_REG;
    if ((sc->twe_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE)) == NULL) {
	twe_printf(sc, "can't allocate register window\n");
	twe_free(sc);
	return(ENXIO);
    }
    sc->twe_btag = rman_get_bustag(sc->twe_io);
    sc->twe_bhandle = rman_get_bushandle(sc->twe_io);

    /*
     * Allocate the parent bus DMA tag appropriate for PCI.
     */
    if (bus_dma_tag_create(NULL, 				/* parent */
			   1, 0, 				/* alignment, boundary */
			   BUS_SPACE_MAXADDR_32BIT, 		/* lowaddr */
			   BUS_SPACE_MAXADDR, 			/* highaddr */
			   NULL, NULL, 				/* filter, filterarg */
			   MAXBSIZE, TWE_MAX_SGL_LENGTH,	/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
			   BUS_DMA_ALLOCNOW,			/* flags */
			   &sc->twe_parent_dmat)) {
	twe_printf(sc, "can't allocate parent DMA tag\n");
	twe_free(sc);
	return(ENOMEM);
    }

    /* 
     * Allocate and connect our interrupt.
     */
    rid = 0;
    if ((sc->twe_irq = bus_alloc_resource(sc->twe_dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
	twe_printf(sc, "can't allocate interrupt\n");
	twe_free(sc);
	return(ENXIO);
    }
    if (bus_setup_intr(sc->twe_dev, sc->twe_irq, INTR_TYPE_BIO | INTR_ENTROPY,  twe_pci_intr, sc, &sc->twe_intr)) {
	twe_printf(sc, "can't set up interrupt\n");
	twe_free(sc);
	return(ENXIO);
    }

    /*
     * Create DMA tag for mapping objects into controller-addressable space.
     */
    if (bus_dma_tag_create(sc->twe_parent_dmat, 	/* parent */
			   1, 0, 			/* alignment, boundary */
			   BUS_SPACE_MAXADDR,		/* lowaddr */
			   BUS_SPACE_MAXADDR, 		/* highaddr */
			   NULL, NULL, 			/* filter, filterarg */
			   MAXBSIZE, TWE_MAX_SGL_LENGTH,/* maxsize, nsegments */
			   BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			   0,				/* flags */
			   &sc->twe_buffer_dmat)) {
	twe_printf(sc, "can't allocate data buffer DMA tag\n");
	twe_free(sc);
	return(ENOMEM);
    }

    /*
     * Initialise the controller and driver core.
     */
    if ((error = twe_setup(sc)))
	return(error);

    /*
     * Print some information about the controller and configuration.
     */
    twe_describe_controller(sc);

    /*
     * Create the control device.
     */
    sc->twe_dev_t = make_dev(&twe_cdevsw, device_get_unit(sc->twe_dev), UID_ROOT, GID_OPERATOR,
			     S_IRUSR | S_IWUSR, "twe%d", device_get_unit(sc->twe_dev));
    sc->twe_dev_t->si_drv1 = sc;
    /*
     * Schedule ourselves to bring the controller up once interrupts are available.
     * This isn't strictly necessary, since we disable interrupts while probing the
     * controller, but it is more in keeping with common practice for other disk 
     * devices.
     */
    sc->twe_ich.ich_func = twe_intrhook;
    sc->twe_ich.ich_arg = sc;
    if (config_intrhook_establish(&sc->twe_ich) != 0) {
	twe_printf(sc, "can't establish configuration hook\n");
	twe_free(sc);
	return(ENXIO);
    }

    return(0);
}

/********************************************************************************
 * Free all of the resources associated with (sc).
 *
 * Should not be called if the controller is active.
 */
static void
twe_free(struct twe_softc *sc)
{
    struct twe_request	*tr;

    debug_called(4);

    /* throw away any command buffers */
    while ((tr = twe_dequeue_free(sc)) != NULL)
	twe_free_request(tr);

    /* destroy the data-transfer DMA tag */
    if (sc->twe_buffer_dmat)
	bus_dma_tag_destroy(sc->twe_buffer_dmat);

    /* disconnect the interrupt handler */
    if (sc->twe_intr)
	bus_teardown_intr(sc->twe_dev, sc->twe_irq, sc->twe_intr);
    if (sc->twe_irq != NULL)
	bus_release_resource(sc->twe_dev, SYS_RES_IRQ, 0, sc->twe_irq);

    /* destroy the parent DMA tag */
    if (sc->twe_parent_dmat)
	bus_dma_tag_destroy(sc->twe_parent_dmat);

    /* release the register window mapping */
    if (sc->twe_io != NULL)
	bus_release_resource(sc->twe_dev, SYS_RES_IOPORT, TWE_IO_CONFIG_REG, sc->twe_io);

    /* destroy control device */
    if (sc->twe_dev_t != (dev_t)NULL)
	destroy_dev(sc->twe_dev_t);

    sysctl_ctx_free(&sc->sysctl_ctx);
}

/********************************************************************************
 * Disconnect from the controller completely, in preparation for unload.
 */
static int
twe_detach(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);
    int			s, error;

    debug_called(4);

    error = EBUSY;
    s = splbio();
    if (sc->twe_state & TWE_STATE_OPEN)
	goto out;

    /*	
     * Shut the controller down.
     */
    twe_shutdown(dev);

    twe_free(sc);

    error = 0;
 out:
    splx(s);
    return(error);
}

/********************************************************************************
 * Bring the controller down to a dormant state and detach all child devices.
 *
 * Note that we can assume that the bioq on the controller is empty, as we won't
 * allow shutdown if any device is open.
 */
static void
twe_shutdown(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);
    int			i, s;

    debug_called(4);

    s = splbio();

    /* 
     * Delete all our child devices.
     */
    for (i = 0; i < TWE_MAX_UNITS; i++) {
	twe_detach_drive(sc, i);
    }

    /*
     * Bring the controller down.
     */
    twe_deinit(sc);

    splx(s);
}

/********************************************************************************
 * Bring the controller to a quiescent state, ready for system suspend.
 */
static int
twe_suspend(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);
    int			s;

    debug_called(4);

    s = splbio();
    sc->twe_state |= TWE_STATE_SUSPEND;
    
    twe_disable_interrupts(sc);
    splx(s);

    return(0);
}

/********************************************************************************
 * Bring the controller back to a state ready for operation.
 */
static int
twe_resume(device_t dev)
{
    struct twe_softc	*sc = device_get_softc(dev);

    debug_called(4);

    sc->twe_state &= ~TWE_STATE_SUSPEND;
    twe_enable_interrupts(sc);

    return(0);
}

/*******************************************************************************
 * Take an interrupt, or be poked by other code to look for interrupt-worthy
 * status.
 */
static void
twe_pci_intr(void *arg)
{
    twe_intr((struct twe_softc *)arg);
}

/********************************************************************************
 * Delayed-startup hook
 */
static void
twe_intrhook(void *arg)
{
    struct twe_softc		*sc = (struct twe_softc *)arg;

    /* pull ourselves off the intrhook chain */
    config_intrhook_disestablish(&sc->twe_ich);

    /* call core startup routine */
    twe_init(sc);
}

/********************************************************************************
 * Given a detected drive, attach it to the bio interface.
 *
 * This is called from twe_add_unit.
 */
void
twe_attach_drive(struct twe_softc *sc, struct twe_drive *dr)
{
    char	buf[80];
    int		error;

    dr->td_disk =  device_add_child(sc->twe_dev, NULL, -1);
    if (dr->td_disk == NULL) {
	twe_printf(sc, "device_add_child failed\n");
	return;
    }
    device_set_ivars(dr->td_disk, dr);

    /* 
     * XXX It would make sense to test the online/initialising bits, but they seem to be
     * always set...
     */
    sprintf(buf, "Unit %d, %s, %s",
	    dr->td_unit,
	    twe_describe_code(twe_table_unittype, dr->td_type),
	    twe_describe_code(twe_table_unitstate, dr->td_state & TWE_PARAM_UNITSTATUS_MASK));
    device_set_desc_copy(dr->td_disk, buf);

    if ((error = bus_generic_attach(sc->twe_dev)) != 0)
	twe_printf(sc, "bus_generic_attach returned %d\n", error);
}

/********************************************************************************
 * Detach the specified unit if it exsists
 *
 * This is called from twe_del_unit.
 */
void
twe_detach_drive(struct twe_softc *sc, int unit)
{

    if (sc->twe_drive[unit].td_disk != 0) {
	if (device_delete_child(sc->twe_dev, sc->twe_drive[unit].td_disk) != 0)
	    twe_printf(sc, "failed to delete unit %d\n", unit);
	sc->twe_drive[unit].td_disk = 0;
    }
}

/********************************************************************************
 * Clear a PCI parity error.
 */
void
twe_clear_pci_parity_error(struct twe_softc *sc)
{
    TWE_CONTROL(sc, TWE_CONTROL_CLEAR_PARITY_ERROR);
    pci_write_config(sc->twe_dev, PCIR_STATUS, TWE_PCI_CLEAR_PARITY_ERROR, 2);
}

/********************************************************************************
 * Clear a PCI abort.
 */
void
twe_clear_pci_abort(struct twe_softc *sc)
{
    TWE_CONTROL(sc, TWE_CONTROL_CLEAR_PCI_ABORT);
    pci_write_config(sc->twe_dev, PCIR_STATUS, TWE_PCI_CLEAR_PCI_ABORT, 2);
}

/********************************************************************************
 ********************************************************************************
                                                                      Disk device
 ********************************************************************************
 ********************************************************************************/

/*
 * Disk device softc
 */
struct twed_softc 
{
    device_t		twed_dev;
    dev_t		twed_dev_t;
    struct twe_softc	*twed_controller;	/* parent device softc */
    struct twe_drive	*twed_drive;		/* drive data in parent softc */
    struct disk		twed_disk;		/* generic disk handle */
    struct devstat	twed_stats;		/* accounting */
    struct disklabel	twed_label;		/* synthetic label */
    int			twed_flags;
#define TWED_OPEN	(1<<0)			/* drive is open (can't shut down) */
};

/*
 * Disk device bus interface
 */
static int twed_probe(device_t dev);
static int twed_attach(device_t dev);
static int twed_detach(device_t dev);

static device_method_t twed_methods[] = {
    DEVMETHOD(device_probe,	twed_probe),
    DEVMETHOD(device_attach,	twed_attach),
    DEVMETHOD(device_detach,	twed_detach),
    { 0, 0 }
};

static driver_t twed_driver = {
    "twed",
    twed_methods,
    sizeof(struct twed_softc)
};

static devclass_t	twed_devclass;
#ifdef TWE_OVERRIDE
DRIVER_MODULE(Xtwed, Xtwe, twed_driver, twed_devclass, 0, 0);
#else
DRIVER_MODULE(twed, twe, twed_driver, twed_devclass, 0, 0);
#endif

/*
 * Disk device control interface.
 */
static	d_open_t	twed_open;
static	d_close_t	twed_close;
static	d_strategy_t	twed_strategy;
static	d_dump_t	twed_dump;

#define TWED_CDEV_MAJOR	147

static struct cdevsw twed_cdevsw = {
    twed_open,
    twed_close,
    physread,
    physwrite,
    noioctl,
    nopoll,
    nommap,
    twed_strategy,
    "twed",
    TWED_CDEV_MAJOR,
    twed_dump,
    nopsize,
    D_DISK
};

static struct cdevsw	tweddisk_cdevsw;
#ifdef FREEBSD_4
static int		disks_registered = 0;
#endif

/********************************************************************************
 * Handle open from generic layer.
 *
 * Note that this is typically only called by the diskslice code, and not
 * for opens on subdevices (eg. slices, partitions).
 */
static int
twed_open(dev_t dev, int flags, int fmt, d_thread_t *td)
{
    struct twed_softc	*sc = (struct twed_softc *)dev->si_drv1;
    struct disklabel	*label;

    debug_called(4);
	
    if (sc == NULL)
	return (ENXIO);

    /* check that the controller is up and running */
    if (sc->twed_controller->twe_state & TWE_STATE_SHUTDOWN)
	return(ENXIO);

    /* build synthetic label */
    label = &sc->twed_disk.d_label;
    bzero(label, sizeof(*label));
    label->d_type = DTYPE_ESDI;
    label->d_secsize    = TWE_BLOCK_SIZE;
    label->d_nsectors   = sc->twed_drive->td_sectors;
    label->d_ntracks    = sc->twed_drive->td_heads;
    label->d_ncylinders = sc->twed_drive->td_cylinders;
    label->d_secpercyl  = sc->twed_drive->td_sectors * sc->twed_drive->td_heads;
    label->d_secperunit = sc->twed_drive->td_size;

    sc->twed_flags |= TWED_OPEN;
    return (0);
}

/********************************************************************************
 * Handle last close of the disk device.
 */
static int
twed_close(dev_t dev, int flags, int fmt, d_thread_t *td)
{
    struct twed_softc	*sc = (struct twed_softc *)dev->si_drv1;

    debug_called(4);
	
    if (sc == NULL)
	return (ENXIO);

    sc->twed_flags &= ~TWED_OPEN;
    return (0);
}

/********************************************************************************
 * Handle an I/O request.
 */
static void
twed_strategy(twe_bio *bp)
{
    struct twed_softc	*sc = (struct twed_softc *)TWE_BIO_SOFTC(bp);

    debug_called(4);

    TWED_BIO_IN;

    /* bogus disk? */
    if (sc == NULL) {
	TWE_BIO_SET_ERROR(bp, EINVAL);
	printf("twe: bio for invalid disk!\n");
	TWE_BIO_DONE(bp);
	TWED_BIO_OUT;
	return;
    }

    /* perform accounting */
    TWE_BIO_STATS_START(bp);

    /* queue the bio on the controller */
    twe_enqueue_bio(sc->twed_controller, bp);

    /* poke the controller to start I/O */
    twe_startio(sc->twed_controller);
    return;
}

/********************************************************************************
 * System crashdump support
 */
int
twed_dump(dev_t dev)
{
    struct twed_softc	*twed_sc = (struct twed_softc *)dev->si_drv1;
    struct twe_softc	*twe_sc  = (struct twe_softc *)twed_sc->twed_controller;
    u_int		count, blkno, secsize;
    vm_paddr_t		addr = 0;
    long		blkcnt;
    int			dumppages = MAXDUMPPGS;
    int			error;
    int			i;

    if ((error = disk_dumpcheck(dev, &count, &blkno, &secsize)))
        return(error);

    if (!twed_sc || !twe_sc)
	return(ENXIO);

    blkcnt = howmany(PAGE_SIZE, secsize);

    while (count > 0) {
	caddr_t va = NULL;

	if ((count / blkcnt) < dumppages)
	    dumppages = count / blkcnt;

	for (i = 0; i < dumppages; ++i) {
	    vm_paddr_t a = addr + (i * PAGE_SIZE);
	    if (is_physical_memory(a))
		va = pmap_kenter_temporary(trunc_page(a), i);
	    else
		va = pmap_kenter_temporary(trunc_page(0), i);
	}

	if ((error = twe_dump_blocks(twe_sc, twed_sc->twed_drive->td_unit, blkno, va, 
				     (PAGE_SIZE * dumppages) / TWE_BLOCK_SIZE)) != 0)
	    return(error);


	if (dumpstatus(addr, (off_t)count * DEV_BSIZE) < 0)
	    return(EINTR);

	blkno += blkcnt * dumppages;
	count -= blkcnt * dumppages;
	addr += PAGE_SIZE * dumppages;
    }
    return(0);
}

/********************************************************************************
 * Handle completion of an I/O request.
 */
void
twed_intr(twe_bio *bp)
{
    debug_called(4);

    /* if no error, transfer completed */
    if (!TWE_BIO_HAS_ERROR(bp))
	TWE_BIO_RESID(bp) = 0;

    TWE_BIO_STATS_END(bp);
    TWE_BIO_DONE(bp);
    TWED_BIO_OUT;
}

/********************************************************************************
 * Default probe stub.
 */
static int
twed_probe(device_t dev)
{
    return (0);
}

/********************************************************************************
 * Attach a unit to the controller.
 */
static int
twed_attach(device_t dev)
{
    struct twed_softc	*sc;
    device_t		parent;
    dev_t		dsk;
    
    debug_called(4);

    /* initialise our softc */
    sc = device_get_softc(dev);
    parent = device_get_parent(dev);
    sc->twed_controller = (struct twe_softc *)device_get_softc(parent);
    sc->twed_drive = device_get_ivars(dev);
    sc->twed_dev = dev;

    /* report the drive */
    twed_printf(sc, "%uMB (%u sectors)\n",
		sc->twed_drive->td_size / ((1024 * 1024) / TWE_BLOCK_SIZE),
		sc->twed_drive->td_size);
    
    devstat_add_entry(&sc->twed_stats, "twed", device_get_unit(dev), TWE_BLOCK_SIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER, 
		      DEVSTAT_PRIORITY_ARRAY);

    /* attach a generic disk device to ourselves */
    dsk = disk_create(device_get_unit(dev), &sc->twed_disk, 0, &twed_cdevsw, &tweddisk_cdevsw);
    dsk->si_drv1 = sc;
    dsk->si_drv2 = &sc->twed_drive->td_unit;
    sc->twed_dev_t = dsk;
#ifdef FREEBSD_4
    disks_registered++;
#endif

    /* set the maximum I/O size to the theoretical maximum allowed by the S/G list size */
    dsk->si_iosize_max = (TWE_MAX_SGL_LENGTH - 1) * PAGE_SIZE;

    return (0);
}

/********************************************************************************
 * Disconnect ourselves from the system.
 */
static int
twed_detach(device_t dev)
{
    struct twed_softc *sc = (struct twed_softc *)device_get_softc(dev);

    debug_called(4);

    if (sc->twed_flags & TWED_OPEN)
	return(EBUSY);

    devstat_remove_entry(&sc->twed_stats);
#ifdef FREEBSD_4
    if (--disks_registered == 0)
	cdevsw_remove(&tweddisk_cdevsw);
#else
    disk_destroy(sc->twed_dev_t);
#endif

    return(0);
}

/********************************************************************************
 ********************************************************************************
                                                                             Misc
 ********************************************************************************
 ********************************************************************************/

static void	twe_setup_data_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error);
static void	twe_setup_request_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error);

/********************************************************************************
 * Allocate a command buffer
 */
MALLOC_DEFINE(TWE_MALLOC_CLASS, "twe commands", "twe commands");

struct twe_request *
twe_allocate_request(struct twe_softc *sc)
{
    struct twe_request	*tr;

    if ((tr = malloc(sizeof(struct twe_request), TWE_MALLOC_CLASS, M_NOWAIT)) == NULL)
	return(NULL);
    bzero(tr, sizeof(*tr));
    tr->tr_sc = sc;
    if (bus_dmamap_create(sc->twe_buffer_dmat, 0, &tr->tr_cmdmap)) {
	twe_free_request(tr);
	return(NULL);
    }
    if (bus_dmamap_create(sc->twe_buffer_dmat, 0, &tr->tr_dmamap)) {
	bus_dmamap_destroy(sc->twe_buffer_dmat, tr->tr_cmdmap);
	twe_free_request(tr);
	return(NULL);
    }    
    return(tr);
}

/********************************************************************************
 * Permanently discard a command buffer.
 */
void
twe_free_request(struct twe_request *tr) 
{
    struct twe_softc	*sc = tr->tr_sc;
    
    debug_called(4);

    bus_dmamap_destroy(sc->twe_buffer_dmat, tr->tr_cmdmap);
    bus_dmamap_destroy(sc->twe_buffer_dmat, tr->tr_dmamap);
    free(tr, TWE_MALLOC_CLASS);
}

/********************************************************************************
 * Map/unmap (tr)'s command and data in the controller's addressable space.
 *
 * These routines ensure that the data which the controller is going to try to
 * access is actually visible to the controller, in a machine-independant 
 * fashion.  Due to a hardware limitation, I/O buffers must be 512-byte aligned
 * and we take care of that here as well.
 */
static void
twe_fillin_sgl(TWE_SG_Entry *sgl, bus_dma_segment_t *segs, int nsegments, int max_sgl)
{
    int i;

    for (i = 0; i < nsegments; i++) {
	sgl[i].address = segs[i].ds_addr;
	sgl[i].length = segs[i].ds_len;
    }
    for (; i < max_sgl; i++) {				/* XXX necessary? */
	sgl[i].address = 0;
	sgl[i].length = 0;
    }
}
		
static void
twe_setup_data_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct twe_request	*tr = (struct twe_request *)arg;
    TWE_Command		*cmd = &tr->tr_command;

    debug_called(4);

    /* save base of first segment in command (applicable if there only one segment) */
    tr->tr_dataphys = segs[0].ds_addr;

    /* correct command size for s/g list size */
    tr->tr_command.generic.size += 2 * nsegments;

    /*
     * Due to the fact that parameter and I/O commands have the scatter/gather list in
     * different places, we need to determine which sort of command this actually is
     * before we can populate it correctly.
     */
    switch(cmd->generic.opcode) {
    case TWE_OP_GET_PARAM:
    case TWE_OP_SET_PARAM:
	cmd->generic.sgl_offset = 2;
	twe_fillin_sgl(&cmd->param.sgl[0], segs, nsegments, TWE_MAX_SGL_LENGTH);
	break;
    case TWE_OP_READ:
    case TWE_OP_WRITE:
	cmd->generic.sgl_offset = 3;
	twe_fillin_sgl(&cmd->io.sgl[0], segs, nsegments, TWE_MAX_SGL_LENGTH);
	break;
    case TWE_OP_ATA_PASSTHROUGH:
	cmd->generic.sgl_offset = 5;
	twe_fillin_sgl(&cmd->ata.sgl[0], segs, nsegments, TWE_MAX_ATA_SGL_LENGTH);
	break;
    default:
	/*
	 * Fall back to what the linux driver does.
	 * Do this because the API may send an opcode
	 * the driver knows nothing about and this will
	 * at least stop PCIABRT's from hosing us.
	 */
	switch (cmd->generic.sgl_offset) {
	case 2:
	    twe_fillin_sgl(&cmd->param.sgl[0], segs, nsegments, TWE_MAX_SGL_LENGTH);
	    break;
	case 3:
	    twe_fillin_sgl(&cmd->io.sgl[0], segs, nsegments, TWE_MAX_SGL_LENGTH);
	    break;
	case 5:
	    twe_fillin_sgl(&cmd->ata.sgl[0], segs, nsegments, TWE_MAX_ATA_SGL_LENGTH);
	    break;
	}
    }
}

static void
twe_setup_request_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct twe_request	*tr = (struct twe_request *)arg;

    debug_called(4);

    /* command can't cross a page boundary */
    tr->tr_cmdphys = segs[0].ds_addr;
}

void
twe_map_request(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;

    debug_called(4);


    /*
     * Map the command into bus space.
     */
    bus_dmamap_load(sc->twe_buffer_dmat, tr->tr_cmdmap, &tr->tr_command, sizeof(tr->tr_command), 
		    twe_setup_request_dmamap, tr, 0);
    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_cmdmap, BUS_DMASYNC_PREWRITE);

    /*
     * If the command involves data, map that too.
     */
    if (tr->tr_data != NULL) {

	/* 
	 * Data must be 64-byte aligned; allocate a fixup buffer if it's not.
	 */
	if (((vm_offset_t)tr->tr_data % TWE_ALIGNMENT) != 0) {
	    tr->tr_realdata = tr->tr_data;				/* save pointer to 'real' data */
	    tr->tr_flags |= TWE_CMD_ALIGNBUF;
	    tr->tr_data = malloc(tr->tr_length, TWE_MALLOC_CLASS, M_NOWAIT);	/* XXX check result here */
	}
	
	/*
	 * Map the data buffer into bus space and build the s/g list.
	 */
	bus_dmamap_load(sc->twe_buffer_dmat, tr->tr_dmamap, tr->tr_data, tr->tr_length, 
			twe_setup_data_dmamap, tr, 0);
	if (tr->tr_flags & TWE_CMD_DATAIN)
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_PREREAD);
	if (tr->tr_flags & TWE_CMD_DATAOUT) {
	    /* if we're using an alignment buffer, and we're writing data, copy the real data out */
	    if (tr->tr_flags & TWE_CMD_ALIGNBUF)
		bcopy(tr->tr_realdata, tr->tr_data, tr->tr_length);
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_PREWRITE);
	}
    }
}

void
twe_unmap_request(struct twe_request *tr)
{
    struct twe_softc	*sc = tr->tr_sc;

    debug_called(4);

    /*
     * Unmap the command from bus space.
     */
    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_cmdmap, BUS_DMASYNC_POSTWRITE);
    bus_dmamap_unload(sc->twe_buffer_dmat, tr->tr_cmdmap); 

    /*
     * If the command involved data, unmap that too.
     */
    if (tr->tr_data != NULL) {
	
	if (tr->tr_flags & TWE_CMD_DATAIN) {
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_POSTREAD);
	    /* if we're using an alignment buffer, and we're reading data, copy the real data in */
	    if (tr->tr_flags & TWE_CMD_ALIGNBUF)
		bcopy(tr->tr_data, tr->tr_realdata, tr->tr_length);
	}
	if (tr->tr_flags & TWE_CMD_DATAOUT)
	    bus_dmamap_sync(sc->twe_buffer_dmat, tr->tr_dmamap, BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->twe_buffer_dmat, tr->tr_dmamap); 
    }

    /* free alignment buffer if it was used */
    if (tr->tr_flags & TWE_CMD_ALIGNBUF) {
	free(tr->tr_data, TWE_MALLOC_CLASS);
	tr->tr_data = tr->tr_realdata;		/* restore 'real' data pointer */
    }
}

#ifdef TWE_DEBUG
/********************************************************************************
 * Print current controller status, call from DDB.
 */
void
twe_report(void)
{
    struct twe_softc	*sc;
    int			i, s;

    s = splbio();
    for (i = 0; (sc = devclass_get_softc(twe_devclass, i)) != NULL; i++)
	twe_print_controller(sc);
    printf("twed: total bio count in %u  out %u\n", twed_bio_in, twed_bio_out);
    splx(s);
}
#endif
