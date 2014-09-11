/*
 * Copyright (c) 2013 Roger Pau Monné <roger.pau@citrix.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/smp.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include <x86/init.h>
#include <machine/nexusvar.h>
#include <machine/intr_machdep.h>

#include <xen/xen-os.h>
#include <xen/xen_intr.h>

/*
 * Xen nexus(4) driver.
 */
static int
nexus_xen_probe(device_t dev)
{

	if (!xen_pv_domain())
		return (ENXIO);

	return (BUS_PROBE_SPECIFIC);
}

static int
nexus_xen_attach(device_t dev)
{
	int error;
#ifndef XEN
	device_t acpi_dev;
#endif

	nexus_init_resources();
	bus_generic_probe(dev);

#ifndef XEN
	if (xen_initial_domain()) {
		/* Disable some ACPI devices that are not usable by Dom0 */
		acpi_cpu_disabled = true;
		acpi_hpet_disabled = true;
		acpi_timer_disabled = true;

		acpi_dev = BUS_ADD_CHILD(dev, 10, "acpi", 0);
		if (acpi_dev == NULL)
			panic("Unable to add ACPI bus to Xen Dom0");
	}
#endif

	error = bus_generic_attach(dev);
#ifndef XEN
	if (xen_initial_domain() && (error == 0))
		acpi_install_wakeup_handler(device_get_softc(acpi_dev));
#endif

	return (error);
}

static int
nexus_xen_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	int ret;

	/*
	 * ISA and PCI intline IRQs are not preregistered on Xen, so
	 * intercept calls to configure those and register them on the fly.
	 */
	if ((irq < FIRST_MSI_INT) && (intr_lookup_source(irq) == NULL)) {
		ret = xen_register_pirq(irq, trig, pol);
		if (ret != 0)
			return (ret);
		nexus_add_irq(irq);
	}
	return (intr_config_intr(irq, trig, pol));
}

static device_method_t nexus_xen_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_xen_probe),
	DEVMETHOD(device_attach,	nexus_xen_attach),

	/* INTR */
	DEVMETHOD(bus_config_intr,	nexus_xen_config_intr),

	{ 0, 0 }
};

DEFINE_CLASS_1(nexus, nexus_xen_driver, nexus_xen_methods, 1, nexus_driver);
static devclass_t nexus_devclass;

DRIVER_MODULE(nexus_xen, root, nexus_xen_driver, nexus_devclass, 0, 0);
