/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/amdsmu/amdsmu.h>

static bool
amdsmu_match(device_t dev, const struct amdsmu_product **product_out)
{
	const uint16_t vendorid = pci_get_vendor(dev);
	const uint16_t deviceid = pci_get_device(dev);

	for (size_t i = 0; i < nitems(amdsmu_products); i++) {
		const struct amdsmu_product *prod = &amdsmu_products[i];

		if (vendorid == prod->amdsmu_vendorid &&
		    deviceid == prod->amdsmu_deviceid) {
			if (product_out != NULL)
				*product_out = prod;
			return (true);
		}
	}
	return (false);
}

static void
amdsmu_identify(driver_t *driver, device_t parent)
{
	if (device_find_child(parent, "amdsmu", -1) != NULL)
		return;

	if (amdsmu_match(parent, NULL)) {
		if (device_add_child(parent, "amdsmu", -1) == NULL)
			device_printf(parent, "add amdsmu child failed\n");
	}
}

static int
amdsmu_probe(device_t dev)
{
	if (resource_disabled("amdsmu", 0))
		return (ENXIO);
	if (!amdsmu_match(device_get_parent(dev), NULL))
		return (ENXIO);
	device_set_descf(dev, "AMD System Management Unit");

	return (BUS_PROBE_GENERIC);
}

static enum amdsmu_res
amdsmu_wait_res(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	enum amdsmu_res res;

	/*
	 * The SMU has a response ready for us when the response register is
	 * set.  Otherwise, we must wait.
	 */
	for (size_t i = 0; i < SMU_RES_READ_MAX; i++) {
		res = amdsmu_read4(sc, SMU_REG_RESPONSE);
		if (res != SMU_RES_WAIT)
			return (res);
		pause_sbt("amdsmu", ustosbt(SMU_RES_READ_PERIOD_US), 0,
		    C_HARDCLOCK);
	}
	device_printf(dev, "timed out waiting for response from SMU\n");
	return (SMU_RES_WAIT);
}

static int
amdsmu_cmd(device_t dev, enum amdsmu_msg msg, uint32_t arg, uint32_t *ret)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	enum amdsmu_res res;

	/* Wait for SMU to be ready. */
	if (amdsmu_wait_res(dev) == SMU_RES_WAIT)
		return (ETIMEDOUT);

	/* Clear previous response. */
	amdsmu_write4(sc, SMU_REG_RESPONSE, SMU_RES_WAIT);

	/* Write out command to registers. */
	amdsmu_write4(sc, SMU_REG_MESSAGE, msg);
	amdsmu_write4(sc, SMU_REG_ARGUMENT, arg);

	/* Wait for SMU response and handle it. */
	res = amdsmu_wait_res(dev);

	switch (res) {
	case SMU_RES_WAIT:
		return (ETIMEDOUT);
	case SMU_RES_OK:
		if (ret != NULL)
			*ret = amdsmu_read4(sc, SMU_REG_ARGUMENT);
		return (0);
	case SMU_RES_REJECT_BUSY:
		device_printf(dev, "SMU is busy\n");
		return (EBUSY);
	case SMU_RES_REJECT_PREREQ:
	case SMU_RES_UNKNOWN:
	case SMU_RES_FAILED:
		device_printf(dev, "SMU error: %02x\n", res);
		return (EIO);
	}

	return (EINVAL);
}

static void
amdsmu_print_vers(device_t dev)
{
	uint32_t smu_vers;
	uint8_t smu_program;
	uint8_t smu_maj, smu_min, smu_rev;

	if (amdsmu_cmd(dev, SMU_MSG_GETSMUVERSION, 0, &smu_vers) != 0) {
		device_printf(dev, "failed to get SMU version\n");
		return;
	}
	smu_program = (smu_vers >> 24) & 0xFF;
	smu_maj = (smu_vers >> 16) & 0xFF;
	smu_min = (smu_vers >> 8) & 0xFF;
	smu_rev = smu_vers & 0xFF;
	device_printf(dev, "SMU version: %d.%d.%d (program %d)\n",
	    smu_maj, smu_min, smu_rev, smu_program);
}

static int
amdsmu_attach(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	uint32_t physbase_addr_lo, physbase_addr_hi;
	uint64_t physbase_addr;
	int rid = 0;

	/*
	 * Find physical base address for SMU.
	 * XXX I am a little confused about the masks here.  I'm just copying
	 * what Linux does in the amd-pmc driver to get the base address.
	 */
	pci_write_config(dev, SMU_INDEX_ADDRESS, SMU_PHYSBASE_ADDR_LO, 4);
	physbase_addr_lo = pci_read_config(dev, SMU_INDEX_DATA, 4) & 0xFFF00000;

	pci_write_config(dev, SMU_INDEX_ADDRESS, SMU_PHYSBASE_ADDR_HI, 4);
	physbase_addr_hi = pci_read_config(dev, SMU_INDEX_DATA, 4) & 0x0000FFFF;

	physbase_addr = (uint64_t)physbase_addr_hi << 32 | physbase_addr_lo;

	/* Map memory for SMU and its registers. */
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate resource\n");
		return (ENXIO);
	}

	sc->bus_tag = rman_get_bustag(sc->res);

	if (bus_space_map(sc->bus_tag, physbase_addr,
	    SMU_MEM_SIZE, 0, &sc->smu_space) != 0) {
		device_printf(dev, "could not map bus space for SMU\n");
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
		return (ENXIO);
	}
	if (bus_space_map(sc->bus_tag, physbase_addr + SMU_REG_SPACE_OFF,
	    SMU_MEM_SIZE, 0, &sc->reg_space) != 0) {
		device_printf(dev, "could not map bus space for SMU regs\n");
		bus_space_unmap(sc->bus_tag, sc->smu_space, SMU_MEM_SIZE);
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
		return (ENXIO);
	}

	amdsmu_print_vers(dev);
	return (0);
}

static int
amdsmu_detach(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	int rid = 0;

	bus_space_unmap(sc->bus_tag, sc->smu_space, SMU_MEM_SIZE);
	bus_space_unmap(sc->bus_tag, sc->reg_space, SMU_MEM_SIZE);

	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
	return (0);
}

static device_method_t amdsmu_methods[] = {
	DEVMETHOD(device_identify,	amdsmu_identify),
	DEVMETHOD(device_probe,		amdsmu_probe),
	DEVMETHOD(device_attach,	amdsmu_attach),
	DEVMETHOD(device_detach,	amdsmu_detach),
	DEVMETHOD_END
};

static driver_t amdsmu_driver = {
	"amdsmu",
	amdsmu_methods,
	sizeof(struct amdsmu_softc),
};

DRIVER_MODULE(amdsmu, hostb, amdsmu_driver, NULL, NULL);
MODULE_VERSION(amdsmu, 1);
MODULE_DEPEND(amdsmu, amdsmn, 1, 1, 1);
MODULE_PNP_INFO("U16:vendor;U16:device", pci, amdsmu, amdsmu_products,
    nitems(amdsmu_products));
