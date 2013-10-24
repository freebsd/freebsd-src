/*-
 * Copyright (c) 2013 Bjoern A. Zeeb
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-11-C-0249)
 * ("MRC2"), as part of the DARPA MRC research programme.
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
 * Documentation:
 * - ISP1761 Hi-Speed Universal Serial Bus On-The-Go controller,
 *   Product data sheet
 * - AN10042 ISP176x Linux Programming Guide,
 *   Application note
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/condvar.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ehci.h>
#include <dev/usb/controller/ehcireg.h>

struct ehci_isp1761_softc {
	ehci_softc_t		base;	/* storage for EHCI code */
	int			ehci_isp1761_rid;
#ifdef WITH_ISP1761_DC
	int			ehci_isp1761_dc_rid;
	struct resource		*sc_dc_io_res;
	struct resource		*sc_dc_irq_res;
	void			*sc_dc_intr_hdl;
#endif
};

#define	EHCI_HC_DEVSTR \
	"Philips ISP1761 Hi-Speed Universal Serial Bus On-The-Go controller"

static int ehci_isp1761_detach_fdt(device_t);

/*
 * Bus space accessors for PIO operations.
 *
 * See later comment why we only support 32bit access.  We need to byte swap
 * the values thus have our own bus_space template.
 */
static uint8_t
_ehci_bs_r_1(void * t, bus_space_handle_t h, bus_size_t o)
{

	panic("%s", __func__);
}

static void
_ehci_bs_w_1(void *t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{

	panic("%s", __func__);
}

static uint16_t
_ehci_bs_r_2(void *t, bus_space_handle_t h, bus_size_t o)
{

	panic("%s", __func__);
}

static void
_ehci_bs_w_2(void *t, bus_space_handle_t h, bus_size_t o, uint16_t v)
{

	panic("%s", __func__);
}

static uint32_t
_ehci_bs_r_4(void *t, bus_space_handle_t h, bus_size_t o)
{
	bus_space_tag_t tp;

	tp = (bus_space_tag_t)t;
	return (le32toh(bus_space_read_4(tp, h, o)));
}

static void
_ehci_bs_w_4(void *t, bus_space_handle_t h, bus_size_t o, uint32_t v)
{
	bus_space_tag_t tp;

	tp = (bus_space_tag_t)t;
	bus_space_write_4(tp, h, o, htole32(v));
}

static int
fdt_bs_map(void *t __unused, bus_addr_t addr, bus_size_t size __unused,
    int flags __unused, bus_space_handle_t *bshp)
{

	*bshp = MIPS_PHYS_TO_DIRECT_UNCACHED(addr);
	return (0);
}

struct bus_space ehci_isp1761_fdt_io_bs_tag_template = {
	/* cookie */
	.bs_cookie =	(void *) 0,

	/* mapping/unmapping */
	.bs_map =	fdt_bs_map,
	.bs_unmap =	generic_bs_unmap,
	.bs_subregion =	generic_bs_subregion,

	/* allocation/deallocation */
	.bs_alloc =	generic_bs_alloc,
	.bs_free =	generic_bs_free,

	/* barrier */
	.bs_barrier =	generic_bs_barrier,

	/* Read (single). */
	.bs_r_1	=	_ehci_bs_r_1,
	.bs_r_2	=	_ehci_bs_r_2,
	.bs_r_4	=	_ehci_bs_r_4,
	.bs_r_8 =	generic_bs_r_8,

	/* read multiple */
	.bs_rm_1 =	generic_bs_rm_1,
	.bs_rm_2 =	generic_bs_rm_2,
	.bs_rm_4 =	generic_bs_rm_4,
	.bs_rm_8 =	generic_bs_rm_8,

	/* read region */
	.bs_rr_1 =	generic_bs_rr_1,
	.bs_rr_2 =	generic_bs_rr_2,
	.bs_rr_4 =	generic_bs_rr_4,
	.bs_rr_8 =	generic_bs_rr_8,

	/* write (single) */
	.bs_w_1	=	_ehci_bs_w_1,
	.bs_w_2	=	_ehci_bs_w_2,
	.bs_w_4	=	_ehci_bs_w_4,
	.bs_w_8 =	generic_bs_w_8,

	/* write multiple */
	.bs_wm_1 =	generic_bs_wm_1,
	.bs_wm_2 =	generic_bs_wm_2,
	.bs_wm_4 =	generic_bs_wm_4,
	.bs_wm_8 =	generic_bs_wm_8,

	/* write region */
	.bs_wr_1 =	generic_bs_wr_1,
	.bs_wr_2 =	generic_bs_wr_2,
	.bs_wr_4 =	generic_bs_wr_4,
	.bs_wr_8 =	generic_bs_wr_8,

	/* set multiple */
	.bs_sm_1 =	generic_bs_sm_1,
	.bs_sm_2 =	generic_bs_sm_2,
	.bs_sm_4 =	generic_bs_sm_4,
	.bs_sm_8 =	generic_bs_sm_8,

	/* set region */
	.bs_sr_1 =	generic_bs_sr_1,
	.bs_sr_2 =	generic_bs_sr_2,
	.bs_sr_4 =	generic_bs_sr_4,
	.bs_sr_8 =	generic_bs_sr_8,

	/* copy */
	.bs_c_1 =	generic_bs_c_1,
	.bs_c_2 =	generic_bs_c_2,
	.bs_c_4 =	generic_bs_c_4,
	.bs_c_8 =	generic_bs_c_8,

	/* read (single) stream */
	.bs_r_1_s =	generic_bs_r_1,
	.bs_r_2_s =	generic_bs_r_2,
	.bs_r_4_s =	generic_bs_r_4,
	.bs_r_8_s =	generic_bs_r_8,

	/* read multiple stream */
	.bs_rm_1_s =	generic_bs_rm_1,
	.bs_rm_2_s =	generic_bs_rm_2,
	.bs_rm_4_s =	generic_bs_rm_4,
	.bs_rm_8_s =	generic_bs_rm_8,

	/* read region stream */
	.bs_rr_1_s =	generic_bs_rr_1,
	.bs_rr_2_s =	generic_bs_rr_2,
	.bs_rr_4_s =	generic_bs_rr_4,
	.bs_rr_8_s =	generic_bs_rr_8,

	/* write (single) stream */
	.bs_w_1_s =	generic_bs_w_1,
	.bs_w_2_s =	generic_bs_w_2,
	.bs_w_4_s =	generic_bs_w_4,
	.bs_w_8_s =	generic_bs_w_8,

	/* write multiple stream */
	.bs_wm_1_s =	generic_bs_wm_1,
	.bs_wm_2_s =	generic_bs_wm_2,
	.bs_wm_4_s =	generic_bs_wm_4,
	.bs_wm_8_s =	generic_bs_wm_8,

	/* write region stream */
	.bs_wr_1_s =	generic_bs_wr_1,
	.bs_wr_2_s =	generic_bs_wr_2,
	.bs_wr_4_s =	generic_bs_wr_4,
	.bs_wr_8_s =	generic_bs_wr_8,
};

bus_space_tag_t	ehci_isp1761_bus_space_fdt_bs =
    &ehci_isp1761_fdt_io_bs_tag_template;

static int
ehci_isp1761_probe_fdt(device_t dev)
{

	if (ofw_bus_is_compatible(dev, "philips,isp1761")) {
		device_set_desc(dev, EHCI_HC_DEVSTR);
		return (BUS_PROBE_DEFAULT);
	}
        return (ENXIO);
}

static int
ehci_isp1761_attach_fdt(device_t dev)
{
	struct ehci_isp1761_softc *isc;
	ehci_softc_t *sc;
	int error, rid;

	isc = device_get_softc(dev);
	sc = &isc->base;

	/* Initialise some bus fields. */
	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = EHCI_MAX_DEVICES;

	/* Get all "DMA" memory. */
	if (usb_bus_mem_alloc_all(&sc->sc_bus,
	    USB_GET_DMA_TAG(dev), &ehci_iterate_hw_softc)) {
		return (ENOMEM);
	}

	/* Get our memory mapped USB config area. */
	isc->ehci_isp1761_rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &isc->ehci_isp1761_rid, RF_ACTIVE|RF_SHAREABLE);
	if (sc->sc_io_res == NULL) {
		device_printf(dev, "failed to map memory for USB\n");
		error = ENXIO;
		goto error;
	}
#ifdef	WITH_ISP1761_DC
	isc->ehci_isp1761_rid = 1;
	isc->sc_dc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &isc->ehci_isp1761_dc_rid, RF_ACTIVE|RF_SHAREABLE);
	if (isc->sc_dc_io_res == NULL) {
		device_printf(dev, "failed to map DC memory for USB\n");
		error = ENXIO;
		goto error;
	}
#endif

	/*
	 * Update handles.  Store old tag in cookie.
	 * The bus framework will do the right thing for us.
	 */
	ehci_isp1761_bus_space_fdt_bs->bs_cookie = rman_get_bustag(sc->sc_io_res);
        rman_set_rid(sc->sc_io_res, isc->ehci_isp1761_rid);
        rman_set_bustag(sc->sc_io_res, ehci_isp1761_bus_space_fdt_bs);
        rman_set_bushandle(sc->sc_io_res, rman_get_bushandle(sc->sc_io_res));

        sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
        sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res) - 0x400;	/* regs. */

#ifdef WITH_ISP1761_DC
	/* Hook up the DC interrupt. */
	/* XXX-BZ do we need it, and in here? */
	rid = 0;
	isc->sc_dc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (isc->sc_dc_irq_res == NULL) {
		device_printf(dev, "Could not allocate DC irq\n");
		error = ENXIO;
		goto error;
	}
#endif

	/* Hook up the HC interrupt. */
	rid = 1;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Could not allocate HC irq\n");
		error = ENXIO;
		goto error;
	}

	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (sc->sc_bus.bdev == 0) {
		device_printf(dev, "Could not add USB device\n");
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);
	device_set_desc(sc->sc_bus.bdev, EHCI_HC_DEVSTR);

	sprintf(sc->sc_vendor, "Philips");
	sc->sc_id_vendor = 0x04cc;		/* Philips */

	/* Interrupts go! */
#ifdef WITH_ISP1761_DC
	error = bus_setup_intr(dev, isc->sc_dc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)ehci_interrupt, isc, &isc->sc_dc_intr_hdl);
	if (error != 0) {
		device_printf(dev, "Could not setup DC irq, %d\n", error);
		isc->sc_dc_intr_hdl = NULL;
		goto error;
	}
#endif
	error = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)ehci_interrupt, sc, &sc->sc_intr_hdl);
	if (error != 0) {
		device_printf(dev, "Could not setup HC irq, %d\n", error);
		sc->sc_intr_hdl = NULL;
		goto error;
	}

	/*
	 * Reset the entire USB chip, not just the host controller, so we can
	 * be certain to start from a defined state after soft reset.
	 */
#define	ISP1761_SW_RESET	0x30c
#define	ISP1761_SW_RESET_HC		0x00000001
#define	ISP1761_SW_RESET_ALL		0x00000002
	EWRITE4(sc, ISP1761_SW_RESET, ISP1761_SW_RESET_ALL);
	DELAY(50000);		/* Wait 50ms */

	/*
	 * Force into 32bit mode (even if default).
	 *
	 * XXX-BERI/CHERI:
	 * We force the A1 bit to be always 0 as well to get usable results
	 * with Altera tools which otherwise would do funny things.
	 */
#define	ISP1761_HW_MODE_CTRL	0x300
#define	ISP1761_HW_MODE_CTRL_DBUSW32	0x00000100
	EWRITE4(sc, ISP1761_HW_MODE_CTRL, ISP1761_HW_MODE_CTRL_DBUSW32);

	/* Run a quick one-time check that we are good. */
#define	ISP1761_SCRATCH		0x308
#define	ISP1761_SCRATCH_TEST_PAT	0xface
	EWRITE4(sc, ISP1761_SCRATCH, ISP1761_SCRATCH_TEST_PAT);
	if (EREAD4(sc, ISP1761_SCRATCH) != ISP1761_SCRATCH_TEST_PAT) {
		device_printf(dev, "Scratch register read-back test failed.\n");
		goto error;
	}

#define	ISP1761_HCCHIPID	0x304
	uint32_t chipid = EREAD4(sc, ISP1761_HCCHIPID);
	if (bootverbose)
		device_printf(dev, "Host Controller ChipID 0x%08x\n", chipid);

#if 0
	/*
	 * Not the root hub but the internal hub has TT capabilities, and
	 * that should be discoverable.
	 */
	sc->sc_flags |= EHCI_SCFLG_TT;
	/* XXX any other flags needed? */
#endif

	error = ehci_init(sc);
	if (error == 0) {
		error = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (error != 0) {
		device_printf(dev, "USB init failed err=%d\n", error);
		goto error;
	}
	return (0);

error:
	ehci_isp1761_detach_fdt(dev);

	return (ENXIO);
}

static int
ehci_isp1761_detach_fdt(device_t dev)
{
	struct ehci_isp1761_softc *isc;
	ehci_softc_t *sc;
	device_t bdev;

	isc = device_get_softc(dev);
	sc = &isc->base;

 	if (sc->sc_bus.bdev) {
		bdev = sc->sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}
	/* During module unload there are lots of children leftover. */
	device_delete_children(dev);

 	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		int error;

		/* Only call ehci_detach() after ehci_init(). */
		ehci_detach(sc);

		error = bus_teardown_intr(dev, sc->sc_irq_res,
		     sc->sc_intr_hdl);
		if (error)
			device_printf(dev, "Could not tear down HC irq, %d\n",
			    error);
		sc->sc_intr_hdl = NULL;
#ifdef WITH_ISP1761_DC
		error = bus_teardown_intr(dev, isc->sc_dc_irq_res,
		     isc->sc_dc_intr_hdl);
		if (error)
			device_printf(dev, "Could not tear down DC irq, %d\n",
			    error);
		isc->sc_dc_intr_hdl = NULL;
#endif
	}
#ifdef WITH_ISP1761_DC
	if (isc->sc_dc_io_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    isc->ehci_isp1761_dc_rid, isc->sc_dc_io_res);
		isc->sc_dc_io_res = NULL;
	}
#endif
	if (sc->sc_io_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, isc->ehci_isp1761_rid,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}

	usb_bus_mem_free_all(&sc->sc_bus, &ehci_iterate_hw_softc);

	return (0);
}

static device_method_t ehci_methods_fdt[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ehci_isp1761_probe_fdt),
	DEVMETHOD(device_attach,	ehci_isp1761_attach_fdt),
	DEVMETHOD(device_detach,	ehci_isp1761_detach_fdt),

	DEVMETHOD_END
};

static driver_t ehci_driver_fdt = {
	"ehci",
	ehci_methods_fdt,
	sizeof(struct ehci_isp1761_softc)
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, simplebus, ehci_driver_fdt, ehci_devclass, 0, 0);
MODULE_DEPEND(ehci, usb, 1, 1, 1);

/* end */
