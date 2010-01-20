/*-
 * Copyright (c) 2006 Juniper Networks.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/tty.h>
#include <machine/bus.h>

#include <machine/ocpbus.h>

#include <dev/quicc/quicc_bfe.h>

static int quicc_ocp_probe(device_t dev);

static device_method_t quicc_ocp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		quicc_ocp_probe),
	DEVMETHOD(device_attach,	quicc_bfe_attach),
	DEVMETHOD(device_detach,	quicc_bfe_detach),

	DEVMETHOD(bus_alloc_resource,	quicc_bus_alloc_resource),
	DEVMETHOD(bus_release_resource,	quicc_bus_release_resource),
	DEVMETHOD(bus_get_resource,	quicc_bus_get_resource),
	DEVMETHOD(bus_read_ivar,	quicc_bus_read_ivar),
	DEVMETHOD(bus_setup_intr,	quicc_bus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	quicc_bus_teardown_intr),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{ 0, 0 }
};

static driver_t quicc_ocp_driver = {
	quicc_driver_name,
	quicc_ocp_methods,
	sizeof(struct quicc_softc),
};

static int
quicc_ocp_probe(device_t dev)
{
	device_t parent;
	uintptr_t clock, devtype;
	int error;

	parent = device_get_parent(dev);

	error = BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_DEVTYPE, &devtype);
	if (error)
		return (error);
	if (devtype != OCPBUS_DEVTYPE_QUICC)
		return (ENXIO);

	if (BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_CLOCK, &clock))
		clock = 0;
	return (quicc_bfe_probe(dev, clock));
}

DRIVER_MODULE(quicc, ocpbus, quicc_ocp_driver, quicc_devclass, 0, 0);
