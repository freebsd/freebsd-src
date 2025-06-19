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
#include <sys/sysctl.h>

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

static int
amdsmu_get_vers(device_t dev)
{
	int err;
	uint32_t smu_vers;
	struct amdsmu_softc *sc = device_get_softc(dev);

	err = amdsmu_cmd(dev, SMU_MSG_GETSMUVERSION, 0, &smu_vers);
	if (err != 0) {
		device_printf(dev, "failed to get SMU version\n");
		return (err);
	}
	sc->smu_program = (smu_vers >> 24) & 0xFF;
	sc->smu_maj = (smu_vers >> 16) & 0xFF;
	sc->smu_min = (smu_vers >> 8) & 0xFF;
	sc->smu_rev = smu_vers & 0xFF;
	device_printf(dev, "SMU version: %d.%d.%d (program %d)\n",
	    sc->smu_maj, sc->smu_min, sc->smu_rev, sc->smu_program);

	return (0);
}

static int
amdsmu_get_ip_blocks(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	const uint16_t deviceid = pci_get_device(dev);
	int err;
	struct amdsmu_metrics *m = &sc->metrics;
	bool active;
	char sysctl_descr[32];

	/* Get IP block count. */
	switch (deviceid) {
	case PCI_DEVICEID_AMD_REMBRANDT_ROOT:
		sc->ip_block_count = 12;
		break;
	case PCI_DEVICEID_AMD_PHOENIX_ROOT:
		sc->ip_block_count = 21;
		break;
	/* TODO How many IP blocks does Strix Point (and the others) have? */
	case PCI_DEVICEID_AMD_STRIX_POINT_ROOT:
	default:
		sc->ip_block_count = nitems(amdsmu_ip_blocks_names);
	}
	KASSERT(sc->ip_block_count <= nitems(amdsmu_ip_blocks_names),
	    ("too many IP blocks for array"));

	/* Get and print out IP blocks. */
	err = amdsmu_cmd(dev, SMU_MSG_GET_SUP_CONSTRAINTS, 0,
	    &sc->active_ip_blocks);
	if (err != 0) {
		device_printf(dev, "failed to get IP blocks\n");
		return (err);
	}
	device_printf(dev, "Active IP blocks: ");
	for (size_t i = 0; i < sc->ip_block_count; i++) {
		active = (sc->active_ip_blocks & (1 << i)) != 0;
		sc->ip_blocks_active[i] = active;
		if (!active)
			continue;
		printf("%s%s", amdsmu_ip_blocks_names[i],
		    i + 1 < sc->ip_block_count ? " " : "\n");
	}

	/* Create a sysctl node for IP blocks. */
	sc->ip_blocks_sysctlnode = SYSCTL_ADD_NODE(sc->sysctlctx,
	    SYSCTL_CHILDREN(sc->sysctlnode), OID_AUTO, "ip_blocks",
	    CTLFLAG_RD, NULL, "SMU metrics");
	if (sc->ip_blocks_sysctlnode == NULL) {
		device_printf(dev, "could not add sysctl node for IP blocks\n");
		return (ENOMEM);
	}

	/* Create a sysctl node for each IP block. */
	for (size_t i = 0; i < sc->ip_block_count; i++) {
		/* Create the sysctl node itself for the IP block. */
		snprintf(sysctl_descr, sizeof sysctl_descr,
		    "Metrics about the %s AMD IP block",
		    amdsmu_ip_blocks_names[i]);
		sc->ip_block_sysctlnodes[i] = SYSCTL_ADD_NODE(sc->sysctlctx,
		    SYSCTL_CHILDREN(sc->ip_blocks_sysctlnode), OID_AUTO,
		    amdsmu_ip_blocks_names[i], CTLFLAG_RD, NULL, sysctl_descr);
		if (sc->ip_block_sysctlnodes[i] == NULL) {
			device_printf(dev,
			    "could not add sysctl node for \"%s\"\n", sysctl_descr);
			continue;
		}
		/*
		 * Create sysctls for if the IP block is currently active, last
		 * active time, and total active time.
		 */
		SYSCTL_ADD_BOOL(sc->sysctlctx,
		    SYSCTL_CHILDREN(sc->ip_block_sysctlnodes[i]), OID_AUTO,
		    "active", CTLFLAG_RD, &sc->ip_blocks_active[i], 0,
		    "IP block is currently active");
		SYSCTL_ADD_U64(sc->sysctlctx,
		    SYSCTL_CHILDREN(sc->ip_block_sysctlnodes[i]), OID_AUTO,
		    "last_time", CTLFLAG_RD, &m->ip_block_last_active_time[i],
		    0, "How long the IP block was active for during the last"
		    " sleep (us)");
#ifdef IP_BLOCK_TOTAL_ACTIVE_TIME
		SYSCTL_ADD_U64(sc->sysctlctx,
		    SYSCTL_CHILDREN(sc->ip_block_sysctlnodes[i]), OID_AUTO,
		    "total_time", CTLFLAG_RD, &m->ip_block_total_active_time[i],
		    0, "How long the IP block was active for during sleep in"
		    " total (us)");
#endif
	}
	return (0);
}

static int
amdsmu_init_metrics(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	int err;
	uint32_t metrics_addr_lo, metrics_addr_hi;
	uint64_t metrics_addr;

	/* Get physical address of logging buffer. */
	err = amdsmu_cmd(dev, SMU_MSG_LOG_GETDRAM_ADDR_LO, 0, &metrics_addr_lo);
	if (err != 0)
		return (err);
	err = amdsmu_cmd(dev, SMU_MSG_LOG_GETDRAM_ADDR_HI, 0, &metrics_addr_hi);
	if (err != 0)
		return (err);
	metrics_addr = ((uint64_t) metrics_addr_hi << 32) | metrics_addr_lo;

	/* Map memory of logging buffer. */
	err = bus_space_map(sc->bus_tag, metrics_addr,
	    sizeof(struct amdsmu_metrics), 0, &sc->metrics_space);
	if (err != 0) {
		device_printf(dev, "could not map bus space for SMU metrics\n");
		return (err);
	}

	/* Start logging for metrics. */
	amdsmu_cmd(dev, SMU_MSG_LOG_RESET, 0, NULL);
	amdsmu_cmd(dev, SMU_MSG_LOG_START, 0, NULL);
	return (0);
}

static int
amdsmu_dump_metrics(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	int err;

	err = amdsmu_cmd(dev, SMU_MSG_LOG_DUMP_DATA, 0, NULL);
	if (err != 0) {
		device_printf(dev, "failed to dump metrics\n");
		return (err);
	}
	bus_space_read_region_4(sc->bus_tag, sc->metrics_space, 0,
	    (uint32_t *)&sc->metrics, sizeof(sc->metrics) / sizeof(uint32_t));

	return (0);
}

static void
amdsmu_fetch_idlemask(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);

	sc->idlemask = amdsmu_read4(sc, SMU_REG_IDLEMASK);
}

static int
amdsmu_attach(device_t dev)
{
	struct amdsmu_softc *sc = device_get_softc(dev);
	int err;
	uint32_t physbase_addr_lo, physbase_addr_hi;
	uint64_t physbase_addr;
	int rid = 0;
	struct sysctl_oid *node;

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
		err = ENXIO;
		goto err_smu_space;
	}
	if (bus_space_map(sc->bus_tag, physbase_addr + SMU_REG_SPACE_OFF,
	    SMU_MEM_SIZE, 0, &sc->reg_space) != 0) {
		device_printf(dev, "could not map bus space for SMU regs\n");
		err = ENXIO;
		goto err_reg_space;
	}

	/* sysctl stuff. */
	sc->sysctlctx = device_get_sysctl_ctx(dev);
	sc->sysctlnode = device_get_sysctl_tree(dev);

	/* Get version & add sysctls. */
	if ((err = amdsmu_get_vers(dev)) != 0)
		goto err_dump;

	SYSCTL_ADD_U8(sc->sysctlctx, SYSCTL_CHILDREN(sc->sysctlnode), OID_AUTO,
	    "program", CTLFLAG_RD, &sc->smu_program, 0, "SMU program number");
	SYSCTL_ADD_U8(sc->sysctlctx, SYSCTL_CHILDREN(sc->sysctlnode), OID_AUTO,
	    "version_major", CTLFLAG_RD, &sc->smu_maj, 0,
	    "SMU firmware major version number");
	SYSCTL_ADD_U8(sc->sysctlctx, SYSCTL_CHILDREN(sc->sysctlnode), OID_AUTO,
	    "version_minor", CTLFLAG_RD, &sc->smu_min, 0,
	    "SMU firmware minor version number");
	SYSCTL_ADD_U8(sc->sysctlctx, SYSCTL_CHILDREN(sc->sysctlnode), OID_AUTO,
	    "version_revision", CTLFLAG_RD, &sc->smu_rev, 0,
	    "SMU firmware revision number");

	/* Set up for getting metrics & add sysctls. */
	if ((err = amdsmu_init_metrics(dev)) != 0)
		goto err_dump;
	if ((err = amdsmu_dump_metrics(dev)) != 0)
		goto err_dump;

	node = SYSCTL_ADD_NODE(sc->sysctlctx, SYSCTL_CHILDREN(sc->sysctlnode),
	    OID_AUTO, "metrics", CTLFLAG_RD, NULL, "SMU metrics");
	if (node == NULL) {
		device_printf(dev, "could not add sysctl node for metrics\n");
		err = ENOMEM;
		goto err_dump;
	}

	SYSCTL_ADD_U32(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "table_version", CTLFLAG_RD, &sc->metrics.table_version, 0,
	    "SMU metrics table version");
	SYSCTL_ADD_U32(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "hint_count", CTLFLAG_RD, &sc->metrics.hint_count, 0,
	    "How many times the sleep hint was set");
	SYSCTL_ADD_U32(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "s0i3_last_entry_status", CTLFLAG_RD,
	    &sc->metrics.s0i3_last_entry_status, 0,
	    "1 if last S0i3 entry was successful");
	SYSCTL_ADD_U32(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "time_last_in_s0i2", CTLFLAG_RD, &sc->metrics.time_last_in_s0i2, 0,
	    "Time spent in S0i2 during last sleep (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "time_last_entering_s0i3", CTLFLAG_RD,
	    &sc->metrics.time_last_entering_s0i3, 0,
	    "Time spent entering S0i3 during last sleep (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "total_time_entering_s0i3", CTLFLAG_RD,
	    &sc->metrics.total_time_entering_s0i3, 0,
	    "Total time spent entering S0i3 (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "time_last_resuming", CTLFLAG_RD, &sc->metrics.time_last_resuming,
	    0, "Time spent resuming from last sleep (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "total_time_resuming", CTLFLAG_RD, &sc->metrics.total_time_resuming,
	    0, "Total time spent resuming from sleep (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "time_last_in_s0i3", CTLFLAG_RD, &sc->metrics.time_last_in_s0i3, 0,
	    "Time spent in S0i3 during last sleep (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "total_time_in_s0i3", CTLFLAG_RD, &sc->metrics.total_time_in_s0i3,
	    0, "Total time spent in S0i3 (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "time_last_in_sw_drips", CTLFLAG_RD,
	    &sc->metrics.time_last_in_sw_drips, 0,
	    "Time spent in awake during last sleep (us)");
	SYSCTL_ADD_U64(sc->sysctlctx, SYSCTL_CHILDREN(node), OID_AUTO,
	    "total_time_in_sw_drips", CTLFLAG_RD,
	    &sc->metrics.total_time_in_sw_drips, 0,
	    "Total time spent awake (us)");

	/* Get IP blocks & add sysctls. */
	err = amdsmu_get_ip_blocks(dev);
	if (err != 0)
		goto err_dump;

	/* Get idlemask & add sysctl. */
	amdsmu_fetch_idlemask(dev);
	SYSCTL_ADD_U32(sc->sysctlctx, SYSCTL_CHILDREN(sc->sysctlnode), OID_AUTO,
	    "idlemask", CTLFLAG_RD, &sc->idlemask, 0, "SMU idlemask. This "
	    "value is not documented - only used to help AMD internally debug "
	    "issues");

	return (0);
err_dump:
	bus_space_unmap(sc->bus_tag, sc->reg_space, SMU_MEM_SIZE);
err_reg_space:
	bus_space_unmap(sc->bus_tag, sc->smu_space, SMU_MEM_SIZE);
err_smu_space:
	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
	return (err);
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
