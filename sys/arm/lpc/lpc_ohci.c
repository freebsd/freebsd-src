/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/rman.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <sys/kdb.h>

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
#include <dev/usb/controller/ohci.h>
#include <dev/usb/controller/ohcireg.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

#define	I2C_START_BIT		(1 << 8)
#define	I2C_STOP_BIT		(1 << 9)
#define	I2C_READ		0x01
#define	I2C_WRITE		0x00
#define	DUMMY_BYTE		0x55

#define	lpc_otg_read_4(_sc, _reg)					\
    bus_space_read_4(_sc->sc_io_tag, _sc->sc_io_hdl, _reg)
#define	lpc_otg_write_4(_sc, _reg, _value)				\
    bus_space_write_4(_sc->sc_io_tag, _sc->sc_io_hdl, _reg, _value)
#define	lpc_otg_wait_write_4(_sc, _wreg, _sreg, _value)			\
    do {								\
    	lpc_otg_write_4(_sc, _wreg, _value);				\
    	while ((lpc_otg_read_4(_sc, _sreg) & _value) != _value);    	\
    } while (0);

static int lpc_ohci_probe(device_t dev);
static int lpc_ohci_attach(device_t dev);
static int lpc_ohci_detach(device_t dev);

static void lpc_otg_i2c_reset(struct ohci_softc *);

static int lpc_isp3101_read(struct ohci_softc *, int);
static void lpc_isp3101_write(struct ohci_softc *, int, int);
static void lpc_isp3101_clear(struct ohci_softc *, int, int);
static void lpc_isp3101_configure(device_t dev, struct ohci_softc *);

static int
lpc_ohci_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "lpc,usb-ohci"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 USB OHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_ohci_attach(device_t dev)
{
	struct ohci_softc *sc = device_get_softc(dev);
	int err;
	int rid;
	int i = 0;
	uint32_t usbctrl;
	uint32_t otgstatus;

	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = OHCI_MAX_DEVICES;

	if (usb_bus_mem_alloc_all(&sc->sc_bus, USB_GET_DMA_TAG(dev),
	    &ohci_iterate_hw_softc))
		return (ENOMEM);

	rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->sc_io_res) {
		device_printf(dev, "cannot map OHCI register space\n");
		goto fail;
	}

	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "cannot allocate interrupt\n");
		goto fail;
	}

	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_bus.bdev))
		goto fail;

	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);
	strlcpy(sc->sc_vendor, "NXP", sizeof(sc->sc_vendor));

	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (void *)ohci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		sc->sc_intr_hdl = NULL;
		goto fail;
	}

	usbctrl = lpc_pwr_read(dev, LPC_CLKPWR_USB_CTRL);
	usbctrl |= LPC_CLKPWR_USB_CTRL_SLAVE_HCLK | LPC_CLKPWR_USB_CTRL_BUSKEEPER;
	lpc_pwr_write(dev, LPC_CLKPWR_USB_CTRL, usbctrl);

	/* Enable OTG I2C clock */
	lpc_otg_wait_write_4(sc, LPC_OTG_CLOCK_CTRL,
	    LPC_OTG_CLOCK_STATUS, LPC_OTG_CLOCK_CTRL_I2C_EN);

	/* Reset OTG I2C bus */
	lpc_otg_i2c_reset(sc);

	lpc_isp3101_configure(dev, sc);

	/* Configure PLL */
	usbctrl &= ~(LPC_CLKPWR_USB_CTRL_CLK_EN1 | LPC_CLKPWR_USB_CTRL_CLK_EN2);
	lpc_pwr_write(dev, LPC_CLKPWR_USB_CTRL, usbctrl);

	usbctrl |= LPC_CLKPWR_USB_CTRL_CLK_EN1;
	lpc_pwr_write(dev, LPC_CLKPWR_USB_CTRL, usbctrl);

	usbctrl |= LPC_CLKPWR_USB_CTRL_FDBKDIV(192-1);
	usbctrl |= LPC_CLKPWR_USB_CTRL_POSTDIV(1);
	usbctrl |= LPC_CLKPWR_USB_CTRL_PLL_PDOWN;

	lpc_pwr_write(dev, LPC_CLKPWR_USB_CTRL, usbctrl);
	do {
		usbctrl = lpc_pwr_read(dev, LPC_CLKPWR_USB_CTRL);
		if (i++ > 100000) {
			device_printf(dev, "USB OTG PLL doesn't lock!\n");
			goto fail;
		}
	} while ((usbctrl & LPC_CLKPWR_USB_CTRL_PLL_LOCK) == 0);

	usbctrl |= LPC_CLKPWR_USB_CTRL_CLK_EN2;
	usbctrl |= LPC_CLKPWR_USB_CTRL_HOST_NEED_CLK_EN;
	lpc_pwr_write(dev, LPC_CLKPWR_USB_CTRL, usbctrl);
	lpc_otg_wait_write_4(sc, LPC_OTG_CLOCK_CTRL, LPC_OTG_CLOCK_STATUS,
	    (LPC_OTG_CLOCK_CTRL_AHB_EN | LPC_OTG_CLOCK_CTRL_OTG_EN |
	    LPC_OTG_CLOCK_CTRL_I2C_EN | LPC_OTG_CLOCK_CTRL_HOST_EN));

	otgstatus = lpc_otg_read_4(sc, LPC_OTG_STATUS);
	lpc_otg_write_4(sc, LPC_OTG_STATUS, otgstatus |
	    LPC_OTG_STATUS_HOST_EN);

	lpc_isp3101_write(sc, LPC_ISP3101_OTG_CONTROL_1,
	    LPC_ISP3101_OTG1_VBUS_DRV);

	err = ohci_init(sc);
	if (err)
		goto fail;

	err = device_probe_and_attach(sc->sc_bus.bdev);
	if (err)
		goto fail;
	
	return (0);

fail:
	if (sc->sc_intr_hdl)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intr_hdl);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_io_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_io_res);

	return (ENXIO);
}

static int
lpc_isp3101_read(struct ohci_softc *sc, int reg)
{
	int status;
	int i = 0;

	lpc_otg_write_4(sc, LPC_OTG_I2C_TXRX, 
	    (LPC_ISP3101_I2C_ADDR << 1) | I2C_START_BIT);
	lpc_otg_write_4(sc, LPC_OTG_I2C_TXRX, reg);
	lpc_otg_write_4(sc, LPC_OTG_I2C_TXRX, (LPC_ISP3101_I2C_ADDR << 1) | 
	    I2C_START_BIT | I2C_READ);
	lpc_otg_write_4(sc, LPC_OTG_I2C_TXRX, I2C_STOP_BIT | DUMMY_BYTE);
	
	do {
		status = lpc_otg_read_4(sc, LPC_OTG_I2C_STATUS);
		i++;
	} while ((status & LPC_OTG_I2C_STATUS_TDI) == 0 || i < 100000);

	lpc_otg_write_4(sc, LPC_OTG_I2C_STATUS, LPC_OTG_I2C_STATUS_TDI);

	return (lpc_otg_read_4(sc, LPC_OTG_I2C_TXRX) & 0xff);
}

static void
lpc_otg_i2c_reset(struct ohci_softc *sc)
{
	int ctrl;
	int i = 0;

	lpc_otg_write_4(sc, LPC_OTG_I2C_CLKHI, 0x3f);
	lpc_otg_write_4(sc, LPC_OTG_I2C_CLKLO, 0x3f);

	ctrl = lpc_otg_read_4(sc, LPC_OTG_I2C_CTRL);
	lpc_otg_write_4(sc, LPC_OTG_I2C_CTRL, ctrl | LPC_OTG_I2C_CTRL_SRST);

	do {
		ctrl = lpc_otg_read_4(sc, LPC_OTG_I2C_CTRL);
		i++;
	} while (ctrl & LPC_OTG_I2C_CTRL_SRST);
}

static void
lpc_isp3101_write(struct ohci_softc *sc, int reg, int value)
{
	int status;
	int i = 0;

	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl, LPC_OTG_I2C_TXRX,
	    (LPC_ISP3101_I2C_ADDR << 1) | I2C_START_BIT);
	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl, LPC_OTG_I2C_TXRX,
	    (reg | I2C_WRITE));
	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl, LPC_OTG_I2C_TXRX,
	    (value | I2C_STOP_BIT));

	do {
		status = bus_space_read_4(sc->sc_io_tag, sc->sc_io_hdl,
		    LPC_OTG_I2C_STATUS);
		i++;
	} while ((status & LPC_OTG_I2C_STATUS_TDI) == 0 || i < 100000);

	bus_space_write_4(sc->sc_io_tag, sc->sc_io_hdl, LPC_OTG_I2C_STATUS,
	    LPC_OTG_I2C_STATUS_TDI);
}

static __inline void
lpc_isp3101_clear(struct ohci_softc *sc, int reg, int value)
{
	lpc_isp3101_write(sc, (reg | LPC_ISP3101_REG_CLEAR_ADDR), value);
}

static void
lpc_isp3101_configure(device_t dev, struct ohci_softc *sc)
{
	lpc_isp3101_clear(sc, LPC_ISP3101_MODE_CONTROL_1, LPC_ISP3101_MC1_UART_EN);
	lpc_isp3101_clear(sc, LPC_ISP3101_MODE_CONTROL_1, ~LPC_ISP3101_MC1_SPEED_REG);
	lpc_isp3101_write(sc, LPC_ISP3101_MODE_CONTROL_1, LPC_ISP3101_MC1_SPEED_REG);
	lpc_isp3101_clear(sc, LPC_ISP3101_MODE_CONTROL_2, ~0);
	lpc_isp3101_write(sc, LPC_ISP3101_MODE_CONTROL_2,
	    (LPC_ISP3101_MC2_BI_DI | LPC_ISP3101_MC2_PSW_EN
	    | LPC_ISP3101_MC2_SPD_SUSP_CTRL));

	lpc_isp3101_clear(sc, LPC_ISP3101_OTG_CONTROL_1, ~0);
	lpc_isp3101_write(sc, LPC_ISP3101_MODE_CONTROL_1, LPC_ISP3101_MC1_DAT_SE0);
	lpc_isp3101_write(sc, LPC_ISP3101_OTG_CONTROL_1,
	    (LPC_ISP3101_OTG1_DM_PULLDOWN | LPC_ISP3101_OTG1_DP_PULLDOWN));
	
	lpc_isp3101_clear(sc, LPC_ISP3101_OTG_CONTROL_1,
	    (LPC_ISP3101_OTG1_DM_PULLUP | LPC_ISP3101_OTG1_DP_PULLUP));

	lpc_isp3101_clear(sc, LPC_ISP3101_OTG_INTR_LATCH, ~0);
	lpc_isp3101_clear(sc, LPC_ISP3101_OTG_INTR_FALLING, ~0);
	lpc_isp3101_clear(sc, LPC_ISP3101_OTG_INTR_RISING, ~0);

	device_printf(dev,
	    "ISP3101 PHY <vendor:0x%04x, product:0x%04x, version:0x%04x>\n",
	    (lpc_isp3101_read(sc, 0x00) | (lpc_isp3101_read(sc, 0x01) << 8)),
	    (lpc_isp3101_read(sc, 0x03) | (lpc_isp3101_read(sc, 0x04) << 8)),
	    (lpc_isp3101_read(sc, 0x14) | (lpc_isp3101_read(sc, 0x15) << 8)));
}

static int
lpc_ohci_detach(device_t dev)
{
	return (0);
}


static device_method_t lpc_ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_ohci_probe),
	DEVMETHOD(device_attach,	lpc_ohci_attach),
	DEVMETHOD(device_detach,	lpc_ohci_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	{ 0, 0 }
};

static driver_t lpc_ohci_driver = {
	"ohci",
	lpc_ohci_methods,
	sizeof(struct ohci_softc),
};

static devclass_t lpc_ohci_devclass;

DRIVER_MODULE(ohci, simplebus, lpc_ohci_driver, lpc_ohci_devclass, 0, 0);
MODULE_DEPEND(ohci, usb, 1, 1, 1);
