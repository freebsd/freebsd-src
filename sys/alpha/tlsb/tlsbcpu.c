/* $FreeBSD$ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Node for TLSB CPU Modules found on
 * AlphaServer 8200 and 8400 systems.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/rpb.h>
#include <machine/cpuconf.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/tlsbvar.h>

#define	KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

/*
 * Device methods
 */
static int tlsbcpu_probe(device_t);

static device_method_t tlsbcpu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tlsbcpu_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static devclass_t tlsbcpu_devclass;
static driver_t tlsbcpu_driver = {
	"tlsbcpu", tlsbcpu_methods, 1
};

static int
tlsbcpu_probe(device_t dev)
{
	u_int32_t vid;
	device_t child;
	static int inst = 0;
	struct tlsb_device *tdev = DEVTOTLSB(dev);

	/*
	 * Deal with hooking CPU instances to TurboLaser nodes.
	 */
	if (!TLDEV_ISCPU(tdev->td_tldev)) {
		return (-1);
	}

	vid = TLSB_GET_NODEREG(tdev->td_node, TLVID) & TLVID_VIDA_MASK;
	vid >>= TLVID_VIDA_SHIFT;

	/*
	 * If this is the primary CPU (unit 0 for us), then
	 * attach a gbus. Otherwise don't. This is bogus,
	 * but sufficent for now.
	 */
	if (device_get_unit(dev) != 0) {
		return (0);
	}

	/*
	 * Hook in the first CPU unit.
	 */
	if (device_get_unit(dev) == 0) {
		tlsb_primary_cpu = tdev;
	}
	/*
	 * Make this CPU a candidate for receiving interrupts.
	 */
	TLSB_PUT_NODEREG(tdev->td_node, TLCPUMASK,
	    TLSB_GET_NODEREG(tdev->td_node, TLCPUMASK) | (1 << vid));

	/*
	 * Attach gbus for first instance.
	 */
	if (device_get_unit(dev) == 0) {
		child = device_add_child(dev, "gbus", inst++);
		if (child == NULL) {
			return (-1);
		}
		device_set_ivars(child, tdev);
	}
	return (0);
}
DRIVER_MODULE(tlsbcpu, tlsb, tlsbcpu_driver, tlsbcpu_devclass, 0, 0);
