/*
 * Copyright (c) 1997,1998 Maxim Bolotin and Oleg Sharoiko.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/ethernet.h> 
#include <net/if.h>
#include <net/if_arp.h>

#include <isa/isavar.h>

#include <dev/cs/if_csvar.h>
#include <dev/cs/if_csreg.h>

static int		cs_isa_probe	(device_t);
static int		cs_isa_attach	(device_t);

static struct isa_pnp_id cs_ids[] = {
	{ 0x4060630e, NULL },		/* CSC6040 */
	{ 0x10104d24, NULL },		/* IBM EtherJet */
	{ 0, NULL }
};

/*
 * Determine if the device is present
 */
static int
cs_isa_probe(device_t dev)
{
	int error = 0;

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, cs_ids);

	/* If the card had a PnP ID that didn't match any we know about */
	if (error == ENXIO)
                goto end;

        /* If we had some other problem. */
        if (!(error == 0 || error == ENOENT))
                goto end;

	error = cs_cs89x0_probe(dev);
end:
	/* Make sure IRQ is assigned for probe message and available */
	if (error == 0)
                error = cs_alloc_irq(dev, 0, 0);

        cs_release_resources(dev);
        return (error);
}

static int
cs_isa_attach(device_t dev)
{
        struct cs_softc *sc = device_get_softc(dev);
        int flags = device_get_flags(dev);
        int error;
        
	cs_alloc_port(dev, 0, CS_89x0_IO_PORTS);
	/* XXX mem appears to not be used at all */
        if (sc->mem_used)
                cs_alloc_memory(dev, sc->mem_rid, sc->mem_used);
        cs_alloc_irq(dev, sc->irq_rid, 0);
                
        error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
	    csintr, sc, &sc->irq_handle);
        if (error) {
                cs_release_resources(dev);
                return (error);
        }              

        return (cs_attach(sc, device_get_unit(dev), flags));
}

static device_method_t cs_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cs_isa_probe),
	DEVMETHOD(device_attach,	cs_isa_attach),
#ifdef CS_HAS_DETACH
	DEVMETHOD(device_detach,	cs_detach),
#endif

	{ 0, 0 }
};

static driver_t cs_isa_driver = {
	"cs",
	cs_isa_methods,
	sizeof(struct cs_softc),
};

extern devclass_t cs_devclass;

DRIVER_MODULE(cs, isa, cs_isa_driver, cs_devclass, 0, 0);
MODULE_DEPEND(cs, isa, 1, 1, 1);
MODULE_DEPEND(cs, ether, 1, 1, 1);
