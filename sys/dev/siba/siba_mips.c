/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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

/*
 * Child driver for MIPS 3302 core.
 * Interrupt controller registers live here. Interrupts may not be routed
 * to the MIPS core if they are masked out.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/siba/sibavar.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/siba_ids.h>

static int	siba_mips_attach(device_t);
static int	siba_mips_probe(device_t);

static int
siba_mips_probe(device_t dev)
{

	if (siba_get_vendor(dev) == SIBA_VID_BROADCOM &&
	    siba_get_device(dev) == SIBA_DEVID_MIPS_3302) {
		device_set_desc(dev, "MIPS 3302 processor");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

struct siba_mips_softc {
	void *notused;
};

static int
siba_mips_attach(device_t dev)
{
	//struct siba_mips_softc *sc = device_get_softc(dev);
	struct resource *mem;
	int rid;

	/*
	 * Allocate the resources which the parent bus has already
	 * determined for us.
	 * TODO: interrupt routing
	 */
#define MIPS_MEM_RID 0x20
	rid = MIPS_MEM_RID;
	mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (mem == NULL) {
		device_printf(dev, "unable to allocate memory\n");
		return (ENXIO);
	}
#if 0
	device_printf(dev, "start %08lx size %04lx\n",
	    rman_get_start(mem), rman_get_size(mem));
#endif

	return (0);
}

static device_method_t siba_mips_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	siba_mips_attach),
	DEVMETHOD(device_probe,		siba_mips_probe),

	{0, 0},
};

static driver_t siba_mips_driver = {
	"siba_mips",
	siba_mips_methods,
	sizeof(struct siba_softc),
};
static devclass_t siba_mips_devclass;

DRIVER_MODULE(siba_mips, siba, siba_mips_driver, siba_mips_devclass, 0, 0);
