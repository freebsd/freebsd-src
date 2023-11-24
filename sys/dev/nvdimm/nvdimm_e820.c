/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Dell EMC Isilon
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
#include <sys/bio.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/efi.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sbuf.h>
#include <sys/uuid.h>

#include <vm/vm_param.h>

#include <machine/metadata.h>
#include <machine/pc/bios.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/nvdimm/nvdimm_var.h>

struct nvdimm_e820_bus {
	SLIST_HEAD(, SPA_mapping) spas;
};

#define	NVDIMM_E820	"nvdimm_e820"

static MALLOC_DEFINE(M_NVDIMM_E820, NVDIMM_E820, "NVDIMM e820 bus memory");

static const struct bios_smap *smapbase;
static struct {
	vm_paddr_t start;
	vm_paddr_t size;
} pram_segments[VM_PHYSSEG_MAX];
static unsigned pram_nreg;

static void
nvdimm_e820_dump_prams(device_t dev, const char *func, int hintunit)
{
	char buffer[256];
	struct sbuf sb;
	bool printed = false;
	unsigned i;

	sbuf_new(&sb, buffer, sizeof(buffer), SBUF_FIXEDLEN);
	sbuf_set_drain(&sb, sbuf_printf_drain, NULL);

	sbuf_printf(&sb, "%s: %s: ", device_get_nameunit(dev), func);
	if (hintunit < 0)
		sbuf_cat(&sb, "Found BIOS PRAM regions: ");
	else
		sbuf_printf(&sb, "Remaining unallocated PRAM regions after "
		    "hint %d: ", hintunit);

	for (i = 0; i < pram_nreg; i++) {
		if (pram_segments[i].size == 0)
			continue;
		if (printed)
			sbuf_putc(&sb, ',');
		else
			printed = true;
		sbuf_printf(&sb, "0x%jx-0x%jx",
		    (uintmax_t)pram_segments[i].start,
		    (uintmax_t)pram_segments[i].start + pram_segments[i].size
		    - 1);
	}

	if (!printed)
		sbuf_cat(&sb, "<none>");
	sbuf_putc(&sb, '\n');
	sbuf_finish(&sb);
	sbuf_delete(&sb);
}

static int
nvdimm_e820_create_spas(device_t dev)
{
	static const vm_size_t HINT_ALL = (vm_size_t)-1;

	ACPI_NFIT_SYSTEM_ADDRESS nfit_sa;
	struct SPA_mapping *spa_mapping;
	enum SPA_mapping_type spa_type;
	struct nvdimm_e820_bus *sc;
	const char *hinttype;
	long hintaddrl, hintsizel;
	vm_paddr_t hintaddr;
	vm_size_t hintsize;
	unsigned i, j;
	int error;

	sc = device_get_softc(dev);
	error = 0;
	nfit_sa = (ACPI_NFIT_SYSTEM_ADDRESS) { 0 };

	if (bootverbose)
		nvdimm_e820_dump_prams(dev, __func__, -1);

	for (i = 0;
	    resource_long_value("nvdimm_spa", i, "maddr", &hintaddrl) == 0;
	    i++) {
		if (resource_long_value("nvdimm_spa", i, "msize", &hintsizel)
		    != 0) {
			device_printf(dev, "hint.nvdimm_spa.%u missing msize\n",
			    i);
			continue;
		}

		hintaddr = (vm_paddr_t)hintaddrl;
		hintsize = (vm_size_t)hintsizel;
		if ((hintaddr & PAGE_MASK) != 0 ||
		    ((hintsize & PAGE_MASK) != 0 && hintsize != HINT_ALL)) {
			device_printf(dev, "hint.nvdimm_spa.%u addr or size "
			    "not page aligned\n", i);
			continue;
		}

		if (resource_string_value("nvdimm_spa", i, "type", &hinttype)
		    != 0) {
			device_printf(dev, "hint.nvdimm_spa.%u missing type\n",
			    i);
			continue;
		}
		spa_type = nvdimm_spa_type_from_name(hinttype);
		if (spa_type == SPA_TYPE_UNKNOWN) {
			device_printf(dev, "hint.nvdimm_spa%u.type does not "
			    "match any known SPA types\n", i);
			continue;
		}

		for (j = 0; j < pram_nreg; j++) {
			if (pram_segments[j].start <= hintaddr &&
			    (hintsize == HINT_ALL ||
			    (pram_segments[j].start + pram_segments[j].size) >=
			    (hintaddr + hintsize)))
				break;
		}

		if (j == pram_nreg) {
			device_printf(dev, "hint.nvdimm_spa%u hint does not "
			    "match any region\n", i);
			continue;
		}

		/* Carve off "SPA" from available regions. */
		if (pram_segments[j].start == hintaddr) {
			/* Easy case first: beginning of segment. */
			if (hintsize == HINT_ALL)
				hintsize = pram_segments[j].size;
			pram_segments[j].start += hintsize;
			pram_segments[j].size -= hintsize;
			/* We might leave an empty segment; who cares. */
		} else if (hintsize == HINT_ALL ||
		    (pram_segments[j].start + pram_segments[j].size) ==
		    (hintaddr + hintsize)) {
			/* 2nd easy case: end of segment. */
			if (hintsize == HINT_ALL)
				hintsize = pram_segments[j].size -
				    (hintaddr - pram_segments[j].start);
			pram_segments[j].size -= hintsize;
		} else {
			/* Hard case: mid segment. */
			if (pram_nreg == nitems(pram_segments)) {
				/* Improbable, but handle gracefully. */
				device_printf(dev, "Ran out of %zu segments\n",
				    nitems(pram_segments));
				error = ENOBUFS;
				break;
			}

			if (j != pram_nreg - 1) {
				memmove(&pram_segments[j + 2],
				    &pram_segments[j + 1],
				    (pram_nreg - 1 - j) *
				    sizeof(pram_segments[0]));
			}
			pram_nreg++;

			pram_segments[j + 1].start = hintaddr + hintsize;
			pram_segments[j + 1].size =
			    (pram_segments[j].start + pram_segments[j].size) -
			    (hintaddr + hintsize);
			pram_segments[j].size = hintaddr -
			    pram_segments[j].start;
		}

		if (bootverbose)
			nvdimm_e820_dump_prams(dev, __func__, (int)i);

		spa_mapping = malloc(sizeof(*spa_mapping), M_NVDIMM_E820,
		    M_WAITOK | M_ZERO);

		/* Mock up a super primitive table for nvdimm_spa_init(). */
		nfit_sa.RangeIndex = i;
		nfit_sa.Flags = 0;
		nfit_sa.Address = hintaddr;
		nfit_sa.Length = hintsize;
		nfit_sa.MemoryMapping = EFI_MD_ATTR_WB | EFI_MD_ATTR_WT |
		    EFI_MD_ATTR_UC;

		error = nvdimm_spa_init(spa_mapping, &nfit_sa, spa_type);
		if (error != 0) {
			nvdimm_spa_fini(spa_mapping);
			free(spa_mapping, M_NVDIMM_E820);
			break;
		}

		SLIST_INSERT_HEAD(&sc->spas, spa_mapping, link);
	}
	return (error);
}

static int
nvdimm_e820_remove_spas(device_t dev)
{
	struct nvdimm_e820_bus *sc;
	struct SPA_mapping *spa, *next;

	sc = device_get_softc(dev);

	SLIST_FOREACH_SAFE(spa, &sc->spas, link, next) {
		nvdimm_spa_fini(spa);
		SLIST_REMOVE_HEAD(&sc->spas, link);
		free(spa, M_NVDIMM_E820);
	}
	return (0);
}

static void
nvdimm_e820_identify(driver_t *driver, device_t parent)
{
	device_t child;
	caddr_t kmdp;

	if (resource_disabled(driver->name, 0))
		return;
	/* Just create a single instance of the fake bus. */
	if (device_find_child(parent, driver->name, -1) != NULL)
		return;

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	smapbase = (const void *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);

	/* Only supports BIOS SMAP for now. */
	if (smapbase == NULL)
		return;

	child = BUS_ADD_CHILD(parent, 0, driver->name, -1);
	if (child == NULL)
		device_printf(parent, "add %s child failed\n", driver->name);
}

static int
nvdimm_e820_probe(device_t dev)
{
	/*
	 * nexus panics if a child doesn't have ivars.  BUS_ADD_CHILD uses
	 * nexus_add_child, which creates fuckin ivars.  but sometimes if you
	 * unload and reload nvdimm_e820, the device node stays but the ivars
	 * are deleted??? avoid trivial panic but this is a kludge.
	 */
	if (device_get_ivars(dev) == NULL)
		return (ENXIO);

	device_quiet(dev);
	device_set_desc(dev, "Legacy e820 NVDIMM root device");
	return (BUS_PROBE_NOWILDCARD);
}

static int
nvdimm_e820_attach(device_t dev)
{
	const struct bios_smap *smapend, *smap;
	uint32_t smapsize;
	unsigned nregions;
	int error;

	smapsize = *((const uint32_t *)smapbase - 1);
	smapend = (const void *)((const char *)smapbase + smapsize);

	for (nregions = 0, smap = smapbase; smap < smapend; smap++) {
		if (smap->type != SMAP_TYPE_PRAM || smap->length == 0)
			continue;
		pram_segments[nregions].start = smap->base;
		pram_segments[nregions].size = smap->length;

		device_printf(dev, "Found PRAM 0x%jx +0x%jx\n",
		    (uintmax_t)smap->base, (uintmax_t)smap->length);

		nregions++;
	}

	if (nregions == 0) {
		device_printf(dev, "No e820 PRAM regions detected\n");
		return (ENXIO);
	}
	pram_nreg = nregions;

	error = nvdimm_e820_create_spas(dev);
	return (error);
}

static int
nvdimm_e820_detach(device_t dev)
{
	int error;

	error = nvdimm_e820_remove_spas(dev);
	return (error);
}

static device_method_t nvdimm_e820_methods[] = {
	DEVMETHOD(device_identify, nvdimm_e820_identify),
	DEVMETHOD(device_probe, nvdimm_e820_probe),
	DEVMETHOD(device_attach, nvdimm_e820_attach),
	DEVMETHOD(device_detach, nvdimm_e820_detach),
	DEVMETHOD_END
};

static driver_t	nvdimm_e820_driver = {
	NVDIMM_E820,
	nvdimm_e820_methods,
	sizeof(struct nvdimm_e820_bus),
};

static int
nvdimm_e820_chainevh(struct module *m, int e, void *arg __unused)
{
	devclass_t dc;
	device_t dev, parent;
	int i, error, maxunit;

	switch (e) {
	case MOD_UNLOAD:
		dc = devclass_find(nvdimm_e820_driver.name);
		maxunit = devclass_get_maxunit(dc);
		for (i = 0; i < maxunit; i++) {
			dev = devclass_get_device(dc, i);
			if (dev == NULL)
				continue;
			parent = device_get_parent(dev);
			if (parent == NULL) {
				/* Not sure how this would happen. */
				continue;
			}
			error = device_delete_child(parent, dev);
			if (error != 0)
				return (error);
		}
		break;
	default:
		/* Prevent compiler warning about unhandled cases. */
		break;
	}
	return (0);
}

DRIVER_MODULE(nvdimm_e820, nexus, nvdimm_e820_driver,
    nvdimm_e820_chainevh, NULL);
