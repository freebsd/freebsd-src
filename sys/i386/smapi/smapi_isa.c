/*-
 * Copyright (c) 2003 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

/* And all this for BIOS_PADDRTOVADDR() */
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/pc/bios.h>

#include <machine/smapi.h>
#include <i386/smapi/smapi_var.h>

static void	smapi_isa_identify	(driver_t *, device_t);
static int	smapi_isa_probe		(device_t);
static int	smapi_isa_attach	(device_t);
static int	smapi_isa_detach	(device_t);
static int	smapi_modevent		(module_t, int, void *);

static int	smapi_header_cksum	(struct smapi_bios_header *);

#define	SMAPI_START	0xf0000
#define	SMAPI_END	0xffff0
#define	SMAPI_STEP	0x10

#define	SMAPI_SIGNATURE(h)	((h->signature[0] == '$') && \
				 (h->signature[1] == 'S') && \
				 (h->signature[2] == 'M') && \
				 (h->signature[3] == 'B'))
#define	RES2HEADER(res)		((struct smapi_bios_header *)rman_get_virtual(res))

static void
smapi_isa_identify (driver_t *driver, device_t parent)
{
	device_t child;
	struct resource *res;
	u_int32_t chunk;
	int rid;

	rid = 0;
	chunk = SMAPI_START;

	child = BUS_ADD_CHILD(parent, 0, "smapi", -1);
	device_set_driver(child, driver);

	while (chunk < SMAPI_END) {
		bus_set_resource(child, SYS_RES_MEMORY, rid, chunk,
			sizeof(struct smapi_bios_header));
		res = bus_alloc_resource(child, SYS_RES_MEMORY, &rid,
			0ul, ~0ul, 1, RF_ACTIVE);
		if (res == NULL) {
			bus_delete_resource(child, SYS_RES_MEMORY, rid);
			chunk += SMAPI_STEP;
			continue;
		}

		if (SMAPI_SIGNATURE(RES2HEADER(res))) {
				goto found;
		} else {
			bus_release_resource(child, SYS_RES_MEMORY, rid, res);
			bus_delete_resource(child, SYS_RES_MEMORY, rid);
		}

		chunk += SMAPI_STEP;
	}

	device_delete_child(parent, child);
	return;

found:
	bus_release_resource(child, SYS_RES_MEMORY, rid, res);
	device_set_desc(child, "SMAPI BIOS");
	return;
}

static int
smapi_isa_probe (device_t dev)
{
	struct resource *res;
	int rid;
	int error;

	error = 0;
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
		0ul, ~0ul, 1, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}

	if (smapi_header_cksum(RES2HEADER(res))) {
		device_printf(dev, "SMAPI header checksum failed.\n");
		error = ENXIO;
		goto bad;
	}

bad:
	if (res)
		bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
	return (error);
}

static int
smapi_isa_attach (device_t dev)
{
	struct smapi_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = 0;

	sc->dev = dev;
	sc->rid = 0;
	sc->res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->rid,
		0ul, ~0ul, 1, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}
	sc->header = (struct smapi_bios_header *)rman_get_virtual(sc->res);
	sc->smapi32_entry = (u_int32_t)BIOS_PADDRTOVADDR(
					sc->header->prot32_segment +
					sc->header->prot32_offset);

	if (smapi_attach(sc)) {
		device_printf(dev, "SMAPI attach failed.\n");
		error = ENXIO;
		goto bad;
	}

	device_printf(dev, "Version %d.%02d, Length %d, Checksum 0x%02x\n",
		bcd2bin(sc->header->version_major),
		bcd2bin(sc->header->version_minor),
		sc->header->length,
		sc->header->checksum);
	device_printf(dev, "Information=0x%b\n",
		sc->header->information,
		"\020"
		"\001REAL_VM86"
		"\002PROTECTED_16"
		"\003PROTECTED_32");

	if (bootverbose) {
		if (sc->header->information & SMAPI_REAL_VM86)
			device_printf(dev, "Real/VM86 mode: Segment 0x%04x, Offset 0x%04x\n",
				sc->header->real16_segment,
				sc->header->real16_offset);
		if (sc->header->information & SMAPI_PROT_16BIT)
			device_printf(dev, "16-bit Protected mode: Segment 0x%08x, Offset 0x%04x\n",
				sc->header->prot16_segment,
				sc->header->prot16_offset);
		if (sc->header->information & SMAPI_PROT_32BIT)
			device_printf(dev, "32-bit Protected mode: Segment 0x%08x, Offset 0x%08x\n",
				sc->header->prot32_segment,
				sc->header->prot32_offset);
	}

	return (0);
bad:
	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
	return (error);
}

static int
smapi_isa_detach (device_t dev)
{
	struct smapi_softc *sc;

	sc = device_get_softc(dev);

	(void)smapi_detach(sc);

	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);

	return (0);
}

static int
smapi_modevent (mod, what, arg)
        module_t        mod;
        int             what;
        void *          arg;
{
	device_t *	devs;
	int		count;
	int		i;

	switch (what) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		devclass_get_devices(smapi_devclass, &devs, &count);
		for (i = 0; i < count; i++) {
			device_delete_child(device_get_parent(devs[i]), devs[i]);
		}
		break;
	default:
		break;
	}

	return (0);
}

static device_method_t smapi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,      smapi_isa_identify),
	DEVMETHOD(device_probe,         smapi_isa_probe),
	DEVMETHOD(device_attach,        smapi_isa_attach),
	DEVMETHOD(device_detach,        smapi_isa_detach),
	{ 0, 0 }
};

static driver_t smapi_driver = {
	"smapi",
	smapi_methods,
	sizeof(struct smapi_softc),
};

DRIVER_MODULE(smapi, legacy, smapi_driver, smapi_devclass, smapi_modevent, 0);
MODULE_VERSION(smapi, 1);

static int
smapi_header_cksum (struct smapi_bios_header *header)
{
	u_int8_t *ptr;
	u_int8_t cksum;
	int i;

	ptr = (u_int8_t *)header;
	cksum = 0;
	for (i = 0; i < header->length; i++) {
		cksum += ptr[i];	
	}

	return (cksum);
}
