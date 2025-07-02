/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/efi.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#if defined(__amd64__) || defined(__i386__)
#include <machine/pc/bios.h>
#endif
#include <dev/smbios/smbios.h>

/*
 * System Management BIOS Reference Specification, v2.4 Final
 * http://www.dmtf.org/standards/published_documents/DSP0134.pdf
 */

struct smbios_softc {
	device_t		dev;
	union {
		struct smbios_eps *	eps;
		struct smbios3_eps *	eps3;
	};
	bool is_eps3;
};

static void	smbios_identify	(driver_t *, device_t);
static int	smbios_probe	(device_t);
static int	smbios_attach	(device_t);
static int	smbios_detach	(device_t);
static int	smbios_modevent	(module_t, int, void *);

static int	smbios_cksum	(void *);
static bool	smbios_eps3	(void *);

static void
smbios_identify (driver_t *driver, device_t parent)
{
#ifdef ARCH_MAY_USE_EFI
	efi_guid_t efi_smbios = EFI_TABLE_SMBIOS;
	efi_guid_t efi_smbios3 = EFI_TABLE_SMBIOS3;
	void *addr_efi;
#endif
	struct smbios_eps *eps;
	struct smbios3_eps *eps3;
	void *ptr;
	device_t child;
	vm_paddr_t addr = 0;
	size_t map_size = sizeof(*eps);
	uint8_t length;

	if (!device_is_alive(parent))
		return;

#ifdef ARCH_MAY_USE_EFI
	if (!efi_get_table(&efi_smbios3, &addr_efi)) {
		addr = (vm_paddr_t)addr_efi;
		map_size = sizeof(*eps3);
	} else if (!efi_get_table(&efi_smbios, &addr_efi)) {
		addr = (vm_paddr_t)addr_efi;
	}

#endif

#if defined(__amd64__) || defined(__i386__)
	if (addr == 0) {
		addr = bios_sigsearch(SMBIOS_START, SMBIOS3_SIG, SMBIOS3_LEN,
		    SMBIOS_STEP, SMBIOS_OFF);
		if (addr != 0)
			map_size = sizeof(*eps3);
		else
			addr = bios_sigsearch(SMBIOS_START,
			    SMBIOS_SIG, SMBIOS_LEN, SMBIOS_STEP, SMBIOS_OFF);
	}
#endif

	if (addr == 0)
		return;

	ptr = pmap_mapbios(addr, map_size);
	if (ptr == NULL) {
		printf("smbios: Unable to map memory.\n");
		return;
	}
	if (map_size == sizeof(*eps3)) {
		eps3 = ptr;
		length = eps3->length;
		if (memcmp(eps3->anchor_string, SMBIOS3_SIG, SMBIOS3_LEN) != 0)
			goto corrupt_sig;
	} else {
		eps = ptr;
		length = eps->length;
		if (memcmp(eps->anchor_string, SMBIOS_SIG, SMBIOS_LEN) != 0)
			goto corrupt_sig;
	}
	if (length != map_size) {
		/*
		 * SMBIOS v2.1 implementations might use 0x1e because the
		 * standard was then erroneous.
		 */
		if (length == 0x1e && map_size == sizeof(*eps) &&
		    eps->major_version == 2 && eps->minor_version == 1)
			length = map_size;
		else {
			printf("smbios: %s-bit Entry Point: Invalid length: "
			    "Got %hhu, expected %zu\n",
			    map_size == sizeof(*eps3) ? "64" : "32",
			    length, map_size);
			goto unmap_return;
		}
	}

	child = BUS_ADD_CHILD(parent, 5, "smbios", DEVICE_UNIT_ANY);
	device_set_driver(child, driver);

	/* smuggle the phys addr into probe and attach */
	bus_set_resource(child, SYS_RES_MEMORY, 0, addr, length);
	device_set_desc(child, "System Management BIOS");

unmap_return:
	pmap_unmapbios(ptr, map_size);
	return;

corrupt_sig:
	{
		const char *sig;
	        const char *table_ver_str;
		size_t i, end;

		if (map_size == sizeof(*eps3)) {
			sig = eps3->anchor_string;
			table_ver_str = "64";
			end = SMBIOS3_LEN;
		} else {
			sig = eps->anchor_string;
			table_ver_str = "32";
			end = SMBIOS_LEN;
		}

		/* Space after ':' printed by the loop. */
		printf("smbios: %s-bit Entry Point: Corrupt signature (hex):",
		    table_ver_str);
		for (i = 0; i < end; ++i)
			printf(" %02hhx", sig[i]);
		printf("\n");
	}
	goto unmap_return;
}

static int
smbios_probe (device_t dev)
{
	vm_paddr_t pa;
	vm_size_t size;
	void *va;
	int error;

	error = 0;

	pa = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);
	size = bus_get_resource_count(dev, SYS_RES_MEMORY, 0);
	va = pmap_mapbios(pa, size);
	if (va == NULL) {
		device_printf(dev, "Unable to map memory.\n");
		return (ENOMEM);
	}

	if (smbios_cksum(va)) {
		device_printf(dev, "SMBIOS checksum failed.\n");
		error = ENXIO;
	}

	pmap_unmapbios(va, size);
	return (error);
}

static int
smbios_attach (device_t dev)
{
	struct smbios_softc *sc;
	void *va;
	vm_paddr_t pa;
	vm_size_t size;

	sc = device_get_softc(dev);
	sc->dev = dev;
	pa = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);
	size = bus_get_resource_count(dev, SYS_RES_MEMORY, 0);
	va = pmap_mapbios(pa, size);
	if (va == NULL) {
		device_printf(dev, "Unable to map memory.\n");
		return (ENOMEM);
	}
	sc->is_eps3 = smbios_eps3(va);

	if (sc->is_eps3) {
		sc->eps3 = va;
		device_printf(dev, "Entry point: v3 (64-bit), Version: %u.%u\n",
		    sc->eps3->major_version, sc->eps3->minor_version);
		if (bootverbose)
			device_printf(dev,
			    "Docrev: %u, Entry Point Revision: %u\n",
			    sc->eps3->docrev, sc->eps3->entry_point_revision);
	} else {
		const struct smbios_eps *const eps = va;
		const uint8_t bcd = eps->BCD_revision;

		sc->eps = va;
		device_printf(dev, "Entry point: v2.1 (32-bit), Version: %u.%u",
		    eps->major_version, eps->minor_version);
		if (bcd < LIBKERN_LEN_BCD2BIN && bcd2bin(bcd) != 0)
			printf(", BCD Revision: %u.%u\n",
			    bcd2bin(bcd >> 4), bcd2bin(bcd & 0x0f));
		else
			printf("\n");
		if (bootverbose)
			device_printf(dev, "Entry Point Revision: %u\n",
			    eps->entry_point_revision);
	}
	return (0);
}

static int
smbios_detach (device_t dev)
{
	struct smbios_softc *sc;
	vm_size_t size;
	void *va;

	sc = device_get_softc(dev);
	va = (sc->is_eps3 ? (void *)sc->eps3 : (void *)sc->eps);
	if (sc->is_eps3)
		va = sc->eps3;
	else
		va = sc->eps;
	size = bus_get_resource_count(dev, SYS_RES_MEMORY, 0);

	if (va != NULL)
		pmap_unmapbios(va, size);

	return (0);
}

static int
smbios_modevent (module_t mod, int what, void *arg)
{
	device_t *	devs;
	int		count;
	int		i;

	switch (what) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		devclass_get_devices(devclass_find("smbios"), &devs, &count);
		for (i = 0; i < count; i++) {
			device_delete_child(device_get_parent(devs[i]), devs[i]);
		}
		free(devs, M_TEMP);
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

DRIVER_MODULE(smbios, nexus, smbios_driver, smbios_modevent, NULL);
#ifdef ARCH_MAY_USE_EFI
MODULE_DEPEND(smbios, efirt, 1, 1, 1);
#endif
MODULE_VERSION(smbios, 1);


static bool
smbios_eps3 (void *v)
{
	struct smbios3_eps *e;

	e = (struct smbios3_eps *)v;
	return (memcmp(e->anchor_string, SMBIOS3_SIG, SMBIOS3_LEN) == 0);
}

static int
smbios_cksum (void *v)
{
	const u_int8_t *ptr;
	u_int8_t cksum;
	u_int8_t length;
	int i;

	if (smbios_eps3(v)) {
		const struct smbios3_eps *eps3 = v;

		length = eps3->length;
	} else {
		const struct smbios_eps *eps = v;

		length = eps->length;
	}
	ptr = v;
	cksum = 0;
	for (i = 0; i < length; i++)
		cksum += ptr[i];

	return (cksum);
}
