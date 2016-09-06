/*-
 * Copyright (c) 2013 Rui Paulo <rpaulo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/event.h>
#include <sys/selinfo.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_pruss.h>

#ifdef DEBUG
#define	DPRINTF(fmt, ...)	do {	\
	printf("%s: ", __func__);	\
	printf(fmt, __VA_ARGS__);	\
} while (0)
#else
#define	DPRINTF(fmt, ...)
#endif

static device_probe_t		ti_pruss_probe;
static device_attach_t		ti_pruss_attach;
static device_detach_t		ti_pruss_detach;
static void			ti_pruss_intr(void *);
static d_open_t			ti_pruss_open;
static d_mmap_t			ti_pruss_mmap;
static void 			ti_pruss_kq_read_detach(struct knote *);
static int 			ti_pruss_kq_read_event(struct knote *, long);
static d_kqfilter_t		ti_pruss_kqfilter;

#define	TI_PRUSS_IRQS		8

struct ti_pruss_softc {
	struct mtx		sc_mtx;
	struct resource 	*sc_mem_res;
	struct resource 	*sc_irq_res[TI_PRUSS_IRQS];
	void            	*sc_intr[TI_PRUSS_IRQS];
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	struct cdev		*sc_pdev;
	struct selinfo		sc_selinfo;
};

static struct cdevsw ti_pruss_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"ti_pruss",
	.d_open =	ti_pruss_open,
	.d_mmap =	ti_pruss_mmap,
	.d_kqfilter =	ti_pruss_kqfilter,
};

static device_method_t ti_pruss_methods[] = {
	DEVMETHOD(device_probe,		ti_pruss_probe),
	DEVMETHOD(device_attach,	ti_pruss_attach),
	DEVMETHOD(device_detach,	ti_pruss_detach),

	DEVMETHOD_END
};

static driver_t ti_pruss_driver = {
	"ti_pruss",
	ti_pruss_methods,
	sizeof(struct ti_pruss_softc)
};

static devclass_t ti_pruss_devclass;

DRIVER_MODULE(ti_pruss, simplebus, ti_pruss_driver, ti_pruss_devclass, 0, 0);

static struct resource_spec ti_pruss_irq_spec[] = {
	{ SYS_RES_IRQ,	    0,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    1,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    2,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    3,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    4,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    5,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    6,  RF_ACTIVE },
	{ SYS_RES_IRQ,	    7,  RF_ACTIVE },
	{ -1,               0,  0 }
};
CTASSERT(TI_PRUSS_IRQS == nitems(ti_pruss_irq_spec) - 1);

static struct ti_pruss_irq_arg {
	int 		       irq;
	struct ti_pruss_softc *sc;
} ti_pruss_irq_args[TI_PRUSS_IRQS];

static __inline uint32_t
ti_pruss_reg_read(struct ti_pruss_softc *sc, uint32_t reg)
{
	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, reg));
}

static __inline void
ti_pruss_reg_write(struct ti_pruss_softc *sc, uint32_t reg, uint32_t val)
{
	bus_space_write_4(sc->sc_bt, sc->sc_bh, reg, val);
}

static int
ti_pruss_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "ti,pruss-v1") ||
	    ofw_bus_is_compatible(dev, "ti,pruss-v2")) {
		device_set_desc(dev, "TI Programmable Realtime Unit Subsystem");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
ti_pruss_attach(device_t dev)
{
	struct ti_pruss_softc *sc;
	int rid, i;

	if (ti_prcm_clk_enable(PRUSS_CLK) != 0) {
		device_printf(dev, "could not enable PRUSS clock\n");
		return (ENXIO);
	}
	sc = device_get_softc(dev);
	rid = 0;
	mtx_init(&sc->sc_mtx, "TI PRUSS", NULL, MTX_DEF);
	knlist_init_mtx(&sc->sc_selinfo.si_note, &sc->sc_mtx);
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		return (ENXIO);
	}
	sc->sc_bt = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bh = rman_get_bushandle(sc->sc_mem_res);
	if (bus_alloc_resources(dev, ti_pruss_irq_spec, sc->sc_irq_res) != 0) {
		device_printf(dev, "could not allocate interrupt resource\n");
		ti_pruss_detach(dev);
		return (ENXIO);
	}
	for (i = 0; i < TI_PRUSS_IRQS; i++) {
		ti_pruss_irq_args[i].irq = i;
		ti_pruss_irq_args[i].sc = sc;
		if (bus_setup_intr(dev, sc->sc_irq_res[i],
		    INTR_MPSAFE | INTR_TYPE_MISC,
		    NULL, ti_pruss_intr, &ti_pruss_irq_args[i],
		    &sc->sc_intr[i]) != 0) {
			device_printf(dev,
			    "unable to setup the interrupt handler\n");
			ti_pruss_detach(dev);
			return (ENXIO);
		}
	}
	if (ti_pruss_reg_read(sc, PRUSS_AM18XX_INTC) == PRUSS_AM18XX_REV)
		device_printf(dev, "AM18xx PRU-ICSS\n");
	else if (ti_pruss_reg_read(sc, PRUSS_AM33XX_INTC) == PRUSS_AM33XX_REV)
		device_printf(dev, "AM33xx PRU-ICSS\n");

	sc->sc_pdev = make_dev(&ti_pruss_cdevsw, 0, UID_ROOT, GID_WHEEL,
	    0600, "pruss%d", device_get_unit(dev));
	sc->sc_pdev->si_drv1 = dev;

	return (0);
}

static int
ti_pruss_detach(device_t dev)
{
	struct ti_pruss_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < TI_PRUSS_IRQS; i++) {
		if (sc->sc_intr[i])
			bus_teardown_intr(dev, sc->sc_irq_res[i], sc->sc_intr[i]);
		if (sc->sc_irq_res[i])
			bus_release_resource(dev, SYS_RES_IRQ,
			    rman_get_rid(sc->sc_irq_res[i]),
			    sc->sc_irq_res[i]);
	}
	knlist_clear(&sc->sc_selinfo.si_note, 0);
	knlist_destroy(&sc->sc_selinfo.si_note);
	mtx_destroy(&sc->sc_mtx);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->sc_mem_res),
		    sc->sc_mem_res);
	if (sc->sc_pdev)
		destroy_dev(sc->sc_pdev);

	return (0);
}

static void
ti_pruss_intr(void *arg)
{
	int val;
	struct ti_pruss_irq_arg *iap = arg;
	struct ti_pruss_softc *sc = iap->sc;
	/*
	 * Interrupts pr1_host_intr[0:7] are mapped to 
	 * Host-2 to Host-9 of PRU-ICSS IRQ-controller.
	 */
	const int pru_int = iap->irq + 2;
	const int pru_int_mask = (1 << pru_int);

	val = ti_pruss_reg_read(sc, PRUSS_AM33XX_INTC + PRUSS_INTC_HIER);
	DPRINTF("interrupt %p, %d", sc, pru_int);
	if (!(val & pru_int_mask))
		return;
 	ti_pruss_reg_write(sc, PRUSS_AM33XX_INTC + PRUSS_INTC_HIDISR, 
	    pru_int);
	KNOTE_UNLOCKED(&sc->sc_selinfo.si_note, pru_int);
}

static int
ti_pruss_open(struct cdev *cdev __unused, int oflags __unused,
    int devtype __unused, struct thread *td __unused)
{
	return (0);
}

static int
ti_pruss_mmap(struct cdev *cdev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	device_t dev = cdev->si_drv1;
	struct ti_pruss_softc *sc = device_get_softc(dev);

	if (offset > rman_get_size(sc->sc_mem_res))
		return (-1);
	*paddr = rman_get_start(sc->sc_mem_res) + offset;
	*memattr = VM_MEMATTR_UNCACHEABLE;

	return (0);
}

static struct filterops ti_pruss_kq_read = {
	.f_isfd = 1,
	.f_detach = ti_pruss_kq_read_detach,
	.f_event = ti_pruss_kq_read_event,
};

static void
ti_pruss_kq_read_detach(struct knote *kn)
{
	struct ti_pruss_softc *sc = kn->kn_hook;

	knlist_remove(&sc->sc_selinfo.si_note, kn, 0);
}

static int
ti_pruss_kq_read_event(struct knote *kn, long hint)
{
	kn->kn_data = hint;

	return (hint);
}

static int
ti_pruss_kqfilter(struct cdev *cdev, struct knote *kn)
{
	device_t dev = cdev->si_drv1;
	struct ti_pruss_softc *sc = device_get_softc(dev);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_hook = sc;
		kn->kn_fop = &ti_pruss_kq_read;
		knlist_add(&sc->sc_selinfo.si_note, kn, 0);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}
