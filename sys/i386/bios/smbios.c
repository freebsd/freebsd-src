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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/pc/bios.h>

/*
 * SMBIOS Entry Point Structure
 */
struct smbios_eps {
	u_int8_t	Anchor[4];		/* '_SM_' */
	u_int8_t	Checksum;
	u_int8_t	Length;

	u_int8_t	SMBIOS_Major;
	u_int8_t	SMBIOS_Minor;
	u_int8_t	Max_Size;
	u_int8_t	Revision;
	u_int8_t	Formatted_Area;

	u_int8_t	Intermediate_Anchor[5];	/* '_DMI_' */
	u_int8_t	Intermediate_Checksum;

	u_int16_t	Structure_Table_Length;
	u_int32_t	Structure_Table_Address;
	u_int16_t	Structure_Count;

	u_int8_t	SMBIOS_BCD_Revision;
} __packed;

struct smbios_softc {
	device_t		dev;
	struct resource *	res;
	int			rid;

	struct smbios_eps *	eps;
};

#define	SMBIOS_START	0xf0000
#define	SMBIOS_STEP	0x10
#define	SMBIOS_OFF	0
#define	SMBIOS_LEN	4
#define	SMBIOS_SIG	"_SM_"

#define	RES2EPS(res)	((struct smbios_eps *)rman_get_virtual(res))
#define	ADDR2EPS(addr)  ((struct smbios_eps *)BIOS_PADDRTOVADDR(addr))

static devclass_t	smbios_devclass;

static void	smbios_identify	(driver_t *, device_t);
static int	smbios_probe	(device_t);
static int	smbios_attach	(device_t);
static int	smbios_detach	(device_t);
static int	smbios_modevent	(module_t, int, void *);

static int	smbios_cksum	(struct smbios_eps *);

static void
smbios_identify (driver_t *driver, device_t parent)
{
	device_t child;
	u_int32_t addr;
	int length;
	int rid;

	if (!device_is_alive(parent))
		return;

	addr = bios_sigsearch(SMBIOS_START, SMBIOS_SIG, SMBIOS_LEN,
			      SMBIOS_STEP, SMBIOS_OFF);
	if (addr != 0) {
		rid = 0;
		length = ADDR2EPS(addr)->Length;

		child = BUS_ADD_CHILD(parent, 0, "smbios", -1);
		device_set_driver(child, driver);
		bus_set_resource(child, SYS_RES_MEMORY, rid, addr, length);
		device_set_desc(child, "System Management BIOS");
	}

	return;
}

static int
smbios_probe (device_t dev)
{
	struct resource *res;
	int rid;
	int error;

	error = 0;
	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}

	if (smbios_cksum(RES2EPS(res))) {
		device_printf(dev, "SMBIOS checksum failed.\n");
		error = ENXIO;
		goto bad;
	}

bad:
	if (res)
		bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
	return (error);
}

static int
smbios_attach (device_t dev)
{
	struct smbios_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = 0;

	sc->dev = dev;
	sc->rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
		RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}
	sc->eps = RES2EPS(sc->res);

	device_printf(dev, "Version: %d.%02d",
		sc->eps->SMBIOS_Major, sc->eps->SMBIOS_Minor);
	if (bcd2bin(sc->eps->SMBIOS_BCD_Revision))
		printf(", Revision: %d.%02d",
			bcd2bin(sc->eps->SMBIOS_BCD_Revision >> 4),
			bcd2bin(sc->eps->SMBIOS_BCD_Revision & 0x0f));
	printf("\n");

	return (0);
bad:
	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
	return (error);
}

static int
smbios_detach (device_t dev)
{
	struct smbios_softc *sc;

	sc = device_get_softc(dev);

	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);

	return (0);
}

static int
smbios_modevent (mod, what, arg)
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
		devclass_get_devices(smbios_devclass, &devs, &count);
		for (i = 0; i < count; i++) {
			device_delete_child(device_get_parent(devs[i]), devs[i]);
		}
		break;
	default:
		break;
	}

	return (0);
}

static device_method_t smbios_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,      smbios_identify),
	DEVMETHOD(device_probe,         smbios_probe),
	DEVMETHOD(device_attach,        smbios_attach),
	DEVMETHOD(device_detach,        smbios_detach),
	{ 0, 0 }
};

static driver_t smbios_driver = {
	"smbios",
	smbios_methods,
	sizeof(struct smbios_softc),
};

DRIVER_MODULE(smbios, nexus, smbios_driver, smbios_devclass, smbios_modevent, 0);
MODULE_VERSION(smbios, 1);

static int
smbios_cksum (struct smbios_eps *e)
{
	u_int8_t *ptr;
	u_int8_t cksum;
	int i;

	ptr = (u_int8_t *)e;
	cksum = 0;
	for (i = 0; i < e->Length; i++) {
		cksum += ptr[i];
	}

	return (cksum);
}
