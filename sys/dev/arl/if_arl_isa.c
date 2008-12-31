/*-
 * Copyright (c) 1999-2001, Ivan Sharov, Vitaly Belekhov.
 * Copyright (c) 2004 Stanislav Svirid.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $RISS: if_arl/dev/arl/if_arl_isa.c,v 1.7 2004/03/16 05:30:38 count Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/arl/if_arl_isa.c,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_inet.h"

#ifdef INET
#define ARLCACHE
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_mib.h>
#include <net/if_media.h>

#include <isa/isavar.h>
#include <isa/pnpvar.h>
#include <isa/isa_common.h>

#include <machine/md_var.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <dev/arl/if_arlreg.h>

static void	arl_isa_identify(driver_t *, device_t);
static int	arl_isa_probe	(device_t);
static int	arl_isa_attach	(device_t);
static int	arl_isa_detach	(device_t);
static char*	arl_make_desc	(u_int8_t, u_int8_t);

#define ARL_MAX_ATYPE_LEN	10
static struct arl_type {
	u_int8_t	type;
	char*		desc;
}
arl_type_list[] = {
	{ 0, "450" },
	{ 1, "650" },
	{ 0xb, "670" },
	{ 0xc, "670E" },
	{ 0xd, "650E" },
	{ 0xe, "440LT" },
	{ 0x2e, "655" },
	{ 0x6b, "IC2200" },
	{ 0, 0 }
};

#define ARL_MAX_RTYPE_LEN	10
struct radio_type {
	u_int8_t	type;
	char*		desc;
} radio_type_list []  = {
	{ 1, "092/094"  },
	{ 2, "020"      },
	{ 3, "092A"     },
	{ 4, "020B"     },
	{ 5, "095"      },
	{ 6, "024"      },
	{ 7, "025B"     },
	{ 8, "024B"     },
	{ 9, "024C"     },
	{10, "025C"     },
	{11, "024-1A"   },
	{12, "025-1A"   },
};


static char*
arl_make_desc(hw_type, radio_mod)
	u_int8_t hw_type;
	u_int8_t radio_mod;
{
	static char desc[80];
	char atype[ARL_MAX_ATYPE_LEN], rtype[ARL_MAX_RTYPE_LEN];
	int i;

	*atype = *rtype = 0;

	/* arl type */
	for(i = 0; arl_type_list[i].desc; i++) {
		if (arl_type_list[i].type == hw_type)
			break;
	}

	if (arl_type_list[i].desc)
		strncpy(atype, arl_type_list[i].desc, ARL_MAX_ATYPE_LEN);
	else
		snprintf(atype, ARL_MAX_ATYPE_LEN, "(0x%x)", hw_type);

	/* radio type */
	for(i = 0; radio_type_list[i].desc; i++)
		if (radio_type_list[i].type == radio_mod)
			break;

	if (radio_type_list[i].desc)
		strncpy(rtype, radio_type_list[i].desc, ARL_MAX_RTYPE_LEN);
	else
		snprintf(rtype, ARL_MAX_RTYPE_LEN, "(0x%x)", radio_mod);

	snprintf(desc, 80, "ArLan type %s, radio module %s", atype, rtype);

	return desc;
}

#define ARL_ADDR2VEC(addr) (1 << ((addr - ARL_BASE_START) / ARL_BASE_STEP))

static void
arl_isa_identify (driver_t *driver, device_t parent)
{
	device_t	child;
	struct	arl_softc	*sc;
	int		chunk, found, i;
	u_int16_t	free_mem = 0xFFFF;

	if (bootverbose)
		printf("arl: in identify\n");

	/* Try avoid already added devices */
	for (i = 0; (child = device_find_child(parent, "arl", i)) != NULL; i++) {
		chunk = bus_get_resource_start(child, SYS_RES_MEMORY, 0);
		if (bootverbose)
			device_printf(child, "found at iomem = 0x%0x\n", chunk);
		if (chunk >= ARL_BASE_START && chunk <= ARL_BASE_END)
			free_mem ^= ARL_ADDR2VEC(chunk);
	}

	if (bootverbose)
		printf("arl: free mem vector = 0x%x\n", free_mem);

	for (chunk = ARL_BASE_START; chunk <= ARL_BASE_END; chunk += ARL_BASE_STEP) {
		/* If device 'arl' with this chunk was found early - skip it */
		if ( !(free_mem & ARL_ADDR2VEC(chunk)) )
			continue;

		found = 0;
		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "arl", -1);
		device_set_driver(child, driver);
		sc = device_get_softc(child);
		bzero(sc, sizeof(*sc));

		bus_set_resource(child, SYS_RES_MEMORY, sc->mem_rid, chunk,
			ARL_BASE_STEP);

		if (arl_alloc_memory(child, sc->mem_rid, ARL_BASE_STEP) == 0) {
			ar = (struct arl_private *) rman_get_virtual(sc->mem_res);
			if (!bcmp(ar->textRegion, ARLAN_SIGN, sizeof(ARLAN_SIGN) - 1))
				found++;
		}

		if (bootverbose)
			device_printf(child, "%sfound at 0x%x\n",
					!found ? "not " : "", chunk);

		arl_release_resources(child);
		if (!found) {
			bus_delete_resource(child, SYS_RES_MEMORY, sc->mem_rid);
			device_delete_child(parent, child);
		}

	}
}

static int
arl_isa_probe (device_t dev)
{
	struct arl_softc *sc = device_get_softc(dev);
	int error;
	u_char *ptr;
	u_int8_t irq;

	if (isa_get_vendorid(dev))
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "in probe\n");

	bzero(sc, sizeof(struct arl_softc));

	sc->arl_unit = device_get_unit(dev);

	error = arl_alloc_memory(dev, 0, ARL_BASE_STEP);
	if (error) {
		if (bootverbose)
			device_printf(dev, "Error allocating memory (%d)\n", error);
		return (error);
	}

	ar = (struct arl_private *) rman_get_virtual(sc->mem_res);
	if (bcmp(ar->textRegion, ARLAN_SIGN, sizeof(ARLAN_SIGN) - 1)) {
		if (bootverbose)
			device_printf(dev, "not found\n");
		error = ENOENT;
		goto bad;
	}

	irq = ar->irqLevel;
	if (irq == 2)
		irq = 9;

	error = bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	if (error)
		goto bad;

	error = arl_alloc_irq(dev, 0, 0);
	if (error) {
		if (bootverbose)
			device_printf(dev, "Can't allocate IRQ %d\n", irq);
		goto bad;
	}

	ar->controlRegister = 1;	/* freeze board */

	/* Memory test */
	for (ptr = (u_char *) ar;
	     ptr < ((u_char *) ar + ARL_BASE_STEP - 1); ptr++) {
		u_char c;

		c = *ptr; *ptr = ~(*ptr);
		if (*ptr != (u_char)~c) {
			device_printf(dev, "board memory failed at [%lx]\n",
			    rman_get_start(sc->mem_res) + (ptr - (u_char *)ar));
			break; /* skip memory test */
		}
	}

	bzero((void *) ar,  ARL_BASE_STEP - 1);	/* clear board ram */

	if (arl_wait_reset(sc, 100, ARDELAY)) {
		error = ENXIO;
		goto bad;
	}

	if (ar->diagnosticInfo == 0xFF) {
		device_set_desc_copy(dev, arl_make_desc(ar->hardwareType,
			ar->radioModule));
		error = 0;
	} else {
		if (bootverbose)
			device_printf(dev, "board self-test failed (0x%x)!\n",
			       ar->diagnosticInfo);
		error = ENXIO;
	}

bad:
	arl_release_resources(dev);

	return (error);
}

static int
arl_isa_attach (device_t dev)
{
	struct arl_softc *sc = device_get_softc(dev);
	int error;

	if (bootverbose)
		device_printf(dev, "in attach\n");

	arl_alloc_memory(dev, sc->mem_rid, ARL_BASE_STEP);
	arl_alloc_irq(dev, sc->irq_rid, 0);

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       NULL, arl_intr, sc, &sc->irq_handle);
	if (error) {
		arl_release_resources(dev);
		return (error);
	}

#if __FreeBSD_version < 502108
	device_printf(dev, "Ethernet address %6D\n", IFP2ENADDR(sc->arl_ifp), ":");
#endif

	return arl_attach(dev);
}

static int
arl_isa_detach(device_t dev)
{
	struct arl_softc *sc = device_get_softc(dev);

	arl_stop(sc);
	ifmedia_removeall(&sc->arl_ifmedia);
	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
#if __FreeBSD_version < 500100
	ether_ifdetach(sc->arl_ifp, ETHER_BPF_SUPPORTED);
#else
	ether_ifdetach(sc->arl_ifp);
	if_free(sc->arl_ifp);
#endif
	arl_release_resources(dev);

	return (0);
}

static device_method_t arl_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	arl_isa_identify),
	DEVMETHOD(device_probe,		arl_isa_probe),
	DEVMETHOD(device_attach,	arl_isa_attach),
	DEVMETHOD(device_detach,	arl_isa_detach),

	{ 0, 0 }
};

static driver_t arl_isa_driver = {
	"arl",
	arl_isa_methods,
	sizeof(struct arl_softc)
};

extern devclass_t arl_devclass;

DRIVER_MODULE(arl, isa, arl_isa_driver, arl_devclass, 0, 0);
MODULE_DEPEND(arl, isa, 1, 1, 1);
MODULE_DEPEND(arl, ether, 1, 1, 1);
