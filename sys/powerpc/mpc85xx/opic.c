/*-
 * Copyright 2006 by Juniper Networks. All rights reserved.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/openpicvar.h>
#include <machine/ocpbus.h>

#include "pic_if.h"

/*
 * OpenPIC attachment to ocpbus
 */
static int	openpic_ocpbus_probe(device_t);
static uint32_t	openpic_ocpbus_id(device_t);

static device_method_t  openpic_ocpbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		openpic_ocpbus_probe),
	DEVMETHOD(device_attach,	openpic_attach),

	/* PIC interface */
	DEVMETHOD(pic_bind,		openpic_bind),
	DEVMETHOD(pic_config,		openpic_config),
	DEVMETHOD(pic_dispatch,		openpic_dispatch),
	DEVMETHOD(pic_enable,		openpic_enable),
	DEVMETHOD(pic_eoi,		openpic_eoi),
	DEVMETHOD(pic_ipi,		openpic_ipi),
	DEVMETHOD(pic_mask,		openpic_mask),
	DEVMETHOD(pic_unmask,		openpic_unmask),
	DEVMETHOD(pic_id,		openpic_ocpbus_id),

	{ 0, 0 },
};

static driver_t openpic_ocpbus_driver = {
	"openpic",
	openpic_ocpbus_methods,
	sizeof(struct openpic_softc)
};

DRIVER_MODULE(openpic, ocpbus, openpic_ocpbus_driver, openpic_devclass, 0, 0);

static int
openpic_ocpbus_probe (device_t dev)
{
	device_t parent;
	uintptr_t devtype;
	int error;

	parent = device_get_parent(dev);

	error = BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_DEVTYPE, &devtype);
	if (error)
		return (error);
	if (devtype != OCPBUS_DEVTYPE_PIC)
		return (ENXIO);

	device_set_desc(dev, OPENPIC_DEVSTR);
	return (BUS_PROBE_DEFAULT);
}

static uint32_t
openpic_ocpbus_id (device_t dev)
{
	return (OPIC_ID);
}


