/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
* TPS65217 PMIC companion chip for AM335x SoC sitting on I2C bus
*/
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/clock.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/am335x/am335x_rtcvar.h>

#include "iicbus_if.h"

#define TPS65217A		0x7
#define TPS65217B		0xF
#define TPS65217C		0xE
#define TPS65217D		0x6

/* TPS65217 Reisters */
#define	TPS65217_CHIPID_REG	0x00
#define	TPS65217_INT_REG	0x02
#define	 TPS65217_INT_PBM		(1U << 6)
#define	 TPS65217_INT_ACM		(1U << 5)
#define	 TPS65217_INT_USBM		(1U << 4)
#define	 TPS65217_INT_PBI		(1U << 2)
#define	 TPS65217_INT_ACI		(1U << 1)
#define	 TPS65217_INT_USBI		(1U << 0)

#define	TPS65217_STATUS_REG	0x0A
#define	 TPS65217_STATUS_OFF		(1U << 7)
#define	 TPS65217_STATUS_ACPWR		(1U << 3)
#define	 TPS65217_STATUS_USBPWR		(1U << 2)
#define	 TPS65217_STATUS_BT		(1U << 0)

#define MAX_IIC_DATA_SIZE	2


struct am335x_pmic_softc {
	device_t		sc_dev;
	uint32_t		sc_addr;
	struct intr_config_hook enum_hook;
	struct resource		*sc_irq_res;
	void			*sc_intrhand;
};

static void am335x_pmic_shutdown(void *, int);

static int
am335x_pmic_read(device_t dev, uint8_t addr, uint8_t *data, uint8_t size)
{
	struct am335x_pmic_softc *sc = device_get_softc(dev);
	struct iic_msg msg[] = {
		{ sc->sc_addr, IIC_M_WR, 1, &addr },
		{ sc->sc_addr, IIC_M_RD, size, data },
	};
	return (iicbus_transfer(dev, msg, 2));
}

static int
am335x_pmic_write(device_t dev, uint8_t address, uint8_t *data, uint8_t size)
{
	uint8_t buffer[MAX_IIC_DATA_SIZE + 1];
	struct am335x_pmic_softc *sc = device_get_softc(dev);
	struct iic_msg msg[] = {
		{ sc->sc_addr, IIC_M_WR, size + 1, buffer },
	};

	if (size > MAX_IIC_DATA_SIZE)
		return (ENOMEM);

	buffer[0] = address;
	memcpy(buffer + 1, data, size);

	return (iicbus_transfer(dev, msg, 1));
}

static void
am335x_pmic_intr(void *arg)
{
	struct am335x_pmic_softc *sc = (struct am335x_pmic_softc *)arg;
	uint8_t int_reg, status_reg;
	int rv;
	char notify_buf[16];

	THREAD_SLEEPING_OK();
	rv = am335x_pmic_read(sc->sc_dev, TPS65217_INT_REG, &int_reg, 1);
	if (rv != 0) {
		device_printf(sc->sc_dev, "Cannot read interrupt register\n");
		THREAD_NO_SLEEPING();
		return;
	}
	rv = am335x_pmic_read(sc->sc_dev, TPS65217_STATUS_REG, &status_reg, 1);
	if (rv != 0) {
		device_printf(sc->sc_dev, "Cannot read status register\n");
		THREAD_NO_SLEEPING();
		return;
	}
	THREAD_NO_SLEEPING();

	if ((int_reg & TPS65217_INT_PBI) && (status_reg & TPS65217_STATUS_BT))
		shutdown_nice(RB_POWEROFF);
	if (int_reg & TPS65217_INT_ACI) {
		snprintf(notify_buf, sizeof(notify_buf), "notify=0x%02x",
		    (status_reg & TPS65217_STATUS_ACPWR) ? 1 : 0);
		devctl_notify_f("ACPI", "ACAD", "power", notify_buf, M_NOWAIT);
	}
}

static int
am335x_pmic_probe(device_t dev)
{
	struct am335x_pmic_softc *sc;

	if (!ofw_bus_is_compatible(dev, "ti,tps65217"))
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	/* Convert to 8-bit addressing */
	sc->sc_addr = iicbus_get_addr(dev) << 1;

	device_set_desc(dev, "TI TPS65217 Power Management IC");

	return (0);
}

static void
am335x_pmic_start(void *xdev)
{
	struct am335x_pmic_softc *sc;
	device_t dev = (device_t)xdev;
	uint8_t reg;
	char name[20];
	char pwr[4][11] = {"Unknown", "USB", "AC", "USB and AC"};
	int rv;

	sc = device_get_softc(dev);

	am335x_pmic_read(dev, TPS65217_CHIPID_REG, &reg, 1);
	switch (reg>>4) {
		case TPS65217A:
			sprintf(name, "TPS65217A ver 1.%u", reg & 0xF);
			break;
		case TPS65217B:
			sprintf(name, "TPS65217B ver 1.%u", reg & 0xF);
			break;
		case TPS65217C:
			sprintf(name, "TPS65217C ver 1.%u", reg & 0xF);
			break;
		case TPS65217D:
			sprintf(name, "TPS65217D ver 1.%u", reg & 0xF);
			break;
		default:
			sprintf(name, "Unknown PMIC");
	}

	am335x_pmic_read(dev, TPS65217_STATUS_REG, &reg, 1);
	device_printf(dev, "%s powered by %s\n", name, pwr[(reg>>2)&0x03]);

	EVENTHANDLER_REGISTER(shutdown_final, am335x_pmic_shutdown, dev,
	    SHUTDOWN_PRI_LAST);

	config_intrhook_disestablish(&sc->enum_hook);

	/* Unmask all interrupts and clear pending status */
	reg = 0;
	am335x_pmic_write(dev, TPS65217_INT_REG, &reg, 1);
	am335x_pmic_read(dev, TPS65217_INT_REG, &reg, 1);

	if (sc->sc_irq_res != NULL) {
		rv = bus_setup_intr(dev, sc->sc_irq_res,
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL, am335x_pmic_intr,
		    sc, &sc->sc_intrhand);
		if (rv != 0)
			device_printf(dev,
			    "Unable to setup the irq handler.\n");
	}
}

static int
am335x_pmic_attach(device_t dev)
{
	struct am335x_pmic_softc *sc;
	int rid;

	sc = device_get_softc(dev);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		/* return (ENXIO); */
	}

	sc->enum_hook.ich_func = am335x_pmic_start;
	sc->enum_hook.ich_arg = dev;

	if (config_intrhook_establish(&sc->enum_hook) != 0)
		return (ENOMEM);

	return (0);
}

static void
am335x_pmic_shutdown(void *xdev, int howto)
{
	device_t dev;
	uint8_t reg;

	if (!(howto & RB_POWEROFF))
		return;
	dev = (device_t)xdev;
	/* Set the OFF bit on status register to start the shutdown sequence. */
	reg = TPS65217_STATUS_OFF;
	am335x_pmic_write(dev, TPS65217_STATUS_REG, &reg, 1);
	/* Toggle pmic_pwr_enable to shutdown the PMIC. */
	am335x_rtc_pmic_pwr_toggle();
}

static device_method_t am335x_pmic_methods[] = {
	DEVMETHOD(device_probe,		am335x_pmic_probe),
	DEVMETHOD(device_attach,	am335x_pmic_attach),
	{0, 0},
};

static driver_t am335x_pmic_driver = {
	"am335x_pmic",
	am335x_pmic_methods,
	sizeof(struct am335x_pmic_softc),
};

static devclass_t am335x_pmic_devclass;

DRIVER_MODULE(am335x_pmic, iicbus, am335x_pmic_driver, am335x_pmic_devclass, 0, 0);
MODULE_VERSION(am335x_pmic, 1);
MODULE_DEPEND(am335x_pmic, iicbus, 1, 1, 1);
