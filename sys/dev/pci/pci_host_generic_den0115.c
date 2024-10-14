/*-
 * Copyright (c) 2022 Andrew Turner
 * Copyright (c) 2023 Arm Ltd
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pci_host_generic_acpi.h>

#include <dev/psci/psci.h>

#include "pcib_if.h"

static device_probe_t pci_host_acpi_smccc_probe;
static device_attach_t pci_host_acpi_smccc_attach;
static pcib_read_config_t pci_host_acpi_smccc_read_config;
static pcib_write_config_t pci_host_acpi_smccc_write_config;

static bool pci_host_acpi_smccc_pci_version(uint32_t *);

static int
pci_host_acpi_smccc_probe(device_t dev)
{
	ACPI_DEVICE_INFO *devinfo;
	struct resource *res;
	ACPI_HANDLE h;
	int rid, root;

	if (acpi_disabled("pcib") || (h = acpi_get_handle(dev)) == NULL ||
	    ACPI_FAILURE(AcpiGetObjectInfo(h, &devinfo)))
		return (ENXIO);
	root = (devinfo->Flags & ACPI_PCI_ROOT_BRIDGE) != 0;
	AcpiOsFree(devinfo);
	if (!root)
		return (ENXIO);

	/*
	 * Check if we have memory resources. We may have a non-memory
	 * mapped device, e.g. using the Arm PCI Configuration Space
	 * Access Firmware Interface (DEN0115).
	 */
	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 0);
	if (res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
		return (ENXIO);
	}

	/* Check for the PCI_VERSION call */
	if (!pci_host_acpi_smccc_pci_version(NULL)) {
		return (ENXIO);
	}

	device_set_desc(dev, "ARM PCI Firmware config space host controller");
	return (BUS_PROBE_SPECIFIC);
}

#define	SMCCC_PCI_VERSION						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL,			\
    SMCCC_STD_SECURE_SERVICE_CALLS, 0x130)
#define	SMCCC_PCI_FEATURES						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL,			\
    SMCCC_STD_SECURE_SERVICE_CALLS, 0x131)
#define	SMCCC_PCI_READ						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL,			\
    SMCCC_STD_SECURE_SERVICE_CALLS, 0x132)
#define	SMCCC_PCI_WRITE						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL,			\
    SMCCC_STD_SECURE_SERVICE_CALLS, 0x133)
#define	SMCCC_PCI_GET_SEG_INFO						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL,			\
    SMCCC_STD_SECURE_SERVICE_CALLS, 0x134)

CTASSERT(SMCCC_PCI_VERSION == 0x84000130);
CTASSERT(SMCCC_PCI_FEATURES == 0x84000131);
CTASSERT(SMCCC_PCI_READ == 0x84000132);
CTASSERT(SMCCC_PCI_WRITE == 0x84000133);
CTASSERT(SMCCC_PCI_GET_SEG_INFO == 0x84000134);

#define	SMCCC_PCI_MAJOR(x)	(((x) >> 16) & 0x7fff)
#define	SMCCC_PCI_MINOR(x)	((x) & 0xffff)

#define	SMCCC_PCI_SEG_END(x)	(((x) >> 8) & 0xff)
#define	SMCCC_PCI_SEG_START(x)	((x) & 0xff)

static bool
pci_host_acpi_smccc_has_feature(uint32_t pci_func_id)
{
	struct arm_smccc_res result;

	if (arm_smccc_invoke(SMCCC_PCI_FEATURES, pci_func_id, &result) < 0) {
		return (false);
	}

	return (true);
}

static bool
pci_host_acpi_smccc_pci_version(uint32_t *versionp)
{
	struct arm_smccc_res result;

	if (arm_smccc_invoke(SMCCC_PCI_VERSION, &result) < 0) {
		return (false);
	}

	if (versionp != NULL) {
		*versionp = result.a0;
	}

	return (true);
}

static int
pci_host_acpi_smccc_attach(device_t dev)
{
	struct generic_pcie_acpi_softc *sc;
	struct arm_smccc_res result;
	uint32_t version;
	int end, start;
	int error;

	sc = device_get_softc(dev);
	sc->base.quirks |= PCIE_CUSTOM_CONFIG_SPACE_QUIRK;

	MPASS(psci_callfn != NULL);

	/* Read the version */
	if (!pci_host_acpi_smccc_pci_version(&version)) {
		device_printf(dev,
		    "Failed to read the SMCCC PCI version\n");
		return (ENXIO);
	}

	if (bootverbose) {
		device_printf(dev, "Firmware v%d.%d\n",
		    SMCCC_PCI_MAJOR(version), SMCCC_PCI_MINOR(version));
	}

	if (!pci_host_acpi_smccc_has_feature(SMCCC_PCI_READ) ||
	    !pci_host_acpi_smccc_has_feature(SMCCC_PCI_WRITE)) {
		device_printf(dev, "Missing read/write functions\n");
		return (ENXIO);
	}

	error = pci_host_generic_acpi_init(dev);
	if (error != 0)
		return (error);

	if (pci_host_acpi_smccc_has_feature(SMCCC_PCI_GET_SEG_INFO) &&
	    arm_smccc_invoke(SMCCC_PCI_GET_SEG_INFO, sc->base.ecam,
	    &result) == SMCCC_RET_SUCCESS) {
		start = SMCCC_PCI_SEG_START(result.a1);
		end = SMCCC_PCI_SEG_END(result.a1);

		sc->base.bus_start = MAX(sc->base.bus_start, start);
		sc->base.bus_end = MIN(sc->base.bus_end, end);
	}

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static uint32_t
pci_host_acpi_smccc_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct generic_pcie_acpi_softc *sc;
	struct arm_smccc_res result;
	uint32_t addr;

	sc = device_get_softc(dev);

	if ((bus < sc->base.bus_start) || (bus > sc->base.bus_end))
		return (~0U);
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return (~0U);

	addr = (sc->base.ecam << 16) | (bus << 8) | (slot << 3) | (func << 0);
	if (arm_smccc_invoke(SMCCC_PCI_READ, addr, reg, bytes, &result) < 0) {
		return (~0U);
	}

	return (result.a1);
}

static void
pci_host_acpi_smccc_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct generic_pcie_acpi_softc *sc;
	struct arm_smccc_res result;
	uint32_t addr;

	sc = device_get_softc(dev);

	if ((bus < sc->base.bus_start) || (bus > sc->base.bus_end))
		return;
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return;

	addr = (sc->base.ecam << 16) | (bus << 8) | (slot << 3) | (func << 0);
	arm_smccc_invoke(SMCCC_PCI_WRITE, addr, reg, bytes, val, &result);
}

static device_method_t generic_pcie_acpi_smccc_methods[] = {
	DEVMETHOD(device_probe,		pci_host_acpi_smccc_probe),
	DEVMETHOD(device_attach,	pci_host_acpi_smccc_attach),

	/* pcib interface */
	DEVMETHOD(pcib_read_config,	pci_host_acpi_smccc_read_config),
	DEVMETHOD(pcib_write_config,	pci_host_acpi_smccc_write_config),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, generic_pcie_acpi_smccc_driver,
    generic_pcie_acpi_smccc_methods,
    sizeof(struct generic_pcie_acpi_softc), generic_pcie_acpi_driver);

DRIVER_MODULE(pcib_smccc, acpi, generic_pcie_acpi_smccc_driver, 0, 0);
