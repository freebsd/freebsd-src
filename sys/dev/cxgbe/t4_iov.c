/*-
 * Copyright (c) 2015-2016 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/iov.h>
#include <dev/pci/pcivar.h>
#include <net/if.h>
#include <net/if_vlan_var.h>

#ifdef PCI_IOV
#include <sys/nv.h>
#include <sys/iov_schema.h>
#include <dev/pci/pci_iov.h>
#endif

#include "common/common.h"
#include "common/t4_regs.h"
#include "t4_if.h"

struct t4iov_softc {
	device_t sc_dev;
	device_t sc_main;
	bool sc_attached;

	int pf;
	int regs_rid;
	struct resource *regs_res;
	bus_space_handle_t bh;
	bus_space_tag_t bt;
};

struct {
	uint16_t device;
	char *desc;
} t4iov_pciids[] = {
	{0x4000, "Chelsio T440-dbg"},
	{0x4001, "Chelsio T420-CR"},
	{0x4002, "Chelsio T422-CR"},
	{0x4003, "Chelsio T440-CR"},
	{0x4004, "Chelsio T420-BCH"},
	{0x4005, "Chelsio T440-BCH"},
	{0x4006, "Chelsio T440-CH"},
	{0x4007, "Chelsio T420-SO"},
	{0x4008, "Chelsio T420-CX"},
	{0x4009, "Chelsio T420-BT"},
	{0x400a, "Chelsio T404-BT"},
	{0x400e, "Chelsio T440-LP-CR"},
}, t5iov_pciids[] = {
	{0x5000, "Chelsio T580-dbg"},
	{0x5001,  "Chelsio T520-CR"},		/* 2 x 10G */
	{0x5002,  "Chelsio T522-CR"},		/* 2 x 10G, 2 X 1G */
	{0x5003,  "Chelsio T540-CR"},		/* 4 x 10G */
	{0x5007,  "Chelsio T520-SO"},		/* 2 x 10G, nomem */
	{0x5009,  "Chelsio T520-BT"},		/* 2 x 10GBaseT */
	{0x500a,  "Chelsio T504-BT"},		/* 4 x 1G */
	{0x500d,  "Chelsio T580-CR"},		/* 2 x 40G */
	{0x500e,  "Chelsio T540-LP-CR"},	/* 4 x 10G */
	{0x5010,  "Chelsio T580-LP-CR"},	/* 2 x 40G */
	{0x5011,  "Chelsio T520-LL-CR"},	/* 2 x 10G */
	{0x5012,  "Chelsio T560-CR"},		/* 1 x 40G, 2 x 10G */
	{0x5014,  "Chelsio T580-LP-SO-CR"},	/* 2 x 40G, nomem */
	{0x5015,  "Chelsio T502-BT"},		/* 2 x 1G */
	{0x5018,  "Chelsio T540-BT"},		/* 4 x 10GBaseT */
	{0x5019,  "Chelsio T540-LP-BT"},	/* 4 x 10GBaseT */
	{0x501a,  "Chelsio T540-SO-BT"},	/* 4 x 10GBaseT, nomem */
	{0x501b,  "Chelsio T540-SO-CR"},	/* 4 x 10G, nomem */
}, t6iov_pciids[] = {
	{0x6000, "Chelsio T6-DBG-25"},		/* 2 x 10/25G, debug */
	{0x6001, "Chelsio T6225-CR"},		/* 2 x 10/25G */
	{0x6002, "Chelsio T6225-SO-CR"},	/* 2 x 10/25G, nomem */
	{0x6003, "Chelsio T6425-CR"},		/* 4 x 10/25G */
	{0x6004, "Chelsio T6425-SO-CR"},	/* 4 x 10/25G, nomem */
	{0x6005, "Chelsio T6225-SO-OCP3"},	/* 2 x 10/25G, nomem */
	{0x6006, "Chelsio T6225-OCP3"},		/* 2 x 10/25G */
	{0x6007, "Chelsio T62100-LP-CR"},	/* 2 x 40/50/100G */
	{0x6008, "Chelsio T62100-SO-CR"},	/* 2 x 40/50/100G, nomem */
	{0x6009, "Chelsio T6210-BT"},		/* 2 x 10GBASE-T */
	{0x600d, "Chelsio T62100-CR"},		/* 2 x 40/50/100G */
	{0x6010, "Chelsio T6-DBG-100"},		/* 2 x 40/50/100G, debug */
	{0x6011, "Chelsio T6225-LL-CR"},	/* 2 x 10/25G */
	{0x6014, "Chelsio T62100-SO-OCP3"},	/* 2 x 40/50/100G, nomem */
	{0x6015, "Chelsio T6201-BT"},		/* 2 x 1000BASE-T */

	/* Custom */
	{0x6080, "Chelsio T6225 80"},
	{0x6081, "Chelsio T62100 81"},
	{0x6082, "Chelsio T6225-CR 82"},
	{0x6083, "Chelsio T62100-CR 83"},
	{0x6084, "Chelsio T64100-CR 84"},
	{0x6085, "Chelsio T6240-SO 85"},
	{0x6086, "Chelsio T6225-SO-CR 86"},
	{0x6087, "Chelsio T6225-CR 87"},
};

static inline uint32_t
t4iov_read_reg(struct t4iov_softc *sc, uint32_t reg)
{

	return bus_space_read_4(sc->bt, sc->bh, reg);
}

static int	t4iov_attach_child(device_t dev);

static int
t4iov_probe(device_t dev)
{
	uint16_t d;
	size_t i;

	if (pci_get_vendor(dev) != PCI_VENDOR_ID_CHELSIO)
		return (ENXIO);

	d = pci_get_device(dev);
	for (i = 0; i < nitems(t4iov_pciids); i++) {
		if (d == t4iov_pciids[i].device) {
			device_set_desc(dev, t4iov_pciids[i].desc);
			device_quiet(dev);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
t5iov_probe(device_t dev)
{
	uint16_t d;
	size_t i;

	if (pci_get_vendor(dev) != PCI_VENDOR_ID_CHELSIO)
		return (ENXIO);

	d = pci_get_device(dev);
	for (i = 0; i < nitems(t5iov_pciids); i++) {
		if (d == t5iov_pciids[i].device) {
			device_set_desc(dev, t5iov_pciids[i].desc);
			device_quiet(dev);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
t6iov_probe(device_t dev)
{
	uint16_t d;
	size_t i;

	if (pci_get_vendor(dev) != PCI_VENDOR_ID_CHELSIO)
		return (ENXIO);

	d = pci_get_device(dev);
	for (i = 0; i < nitems(t6iov_pciids); i++) {
		if (d == t6iov_pciids[i].device) {
			device_set_desc(dev, t6iov_pciids[i].desc);
			device_quiet(dev);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
t4iov_attach(device_t dev)
{
	struct t4iov_softc *sc;
	uint32_t pl_rev, whoami;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->regs_rid = PCIR_BAR(0);
	sc->regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(dev, "cannot map registers.\n");
		return (ENXIO);
	}
	sc->bt = rman_get_bustag(sc->regs_res);
	sc->bh = rman_get_bushandle(sc->regs_res);

	pl_rev = t4iov_read_reg(sc, A_PL_REV);
	whoami = t4iov_read_reg(sc, A_PL_WHOAMI);
	if (G_CHIPID(pl_rev) <= CHELSIO_T5)
		sc->pf = G_SOURCEPF(whoami);
	else
		sc->pf = G_T6_SOURCEPF(whoami);

	sc->sc_main = pci_find_dbsf(pci_get_domain(dev), pci_get_bus(dev),
	    pci_get_slot(dev), 4);
	if (sc->sc_main == NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->regs_rid,
		    sc->regs_res);
		return (ENXIO);
	}
	if (T4_IS_MAIN_READY(sc->sc_main) == 0) {
		error = t4iov_attach_child(dev);
		if (error != 0)
			bus_release_resource(dev, SYS_RES_MEMORY, sc->regs_rid,
			    sc->regs_res);
		return (error);
	}
	return (0);
}

static int
t4iov_attach_child(device_t dev)
{
	struct t4iov_softc *sc;
#ifdef PCI_IOV
	nvlist_t *pf_schema, *vf_schema;
#endif
	device_t pdev;
	int error;

	sc = device_get_softc(dev);
	MPASS(!sc->sc_attached);

	/*
	 * PF0-3 are associated with a specific port on the NIC (PF0
	 * with port 0, etc.).  Ask the PF4 driver for the device for
	 * this function's associated port to determine if the port is
	 * present.
	 */
	error = T4_READ_PORT_DEVICE(sc->sc_main, pci_get_function(dev), &pdev);
	if (error)
		return (0);

#ifdef PCI_IOV
	pf_schema = pci_iov_schema_alloc_node();
	vf_schema = pci_iov_schema_alloc_node();
	pci_iov_schema_add_unicast_mac(vf_schema, "mac-addr", 0, NULL);
	pci_iov_schema_add_vlan(vf_schema, "vlan", 0, 0);
	error = pci_iov_attach_name(dev, pf_schema, vf_schema, "%s",
	    device_get_nameunit(pdev));
	if (error) {
		device_printf(dev, "Failed to initialize SR-IOV: %d\n", error);
		return (0);
	}
#endif

	sc->sc_attached = true;
	return (0);
}

static int
t4iov_detach_child(device_t dev)
{
	struct t4iov_softc *sc;
#ifdef PCI_IOV
	int error;
#endif

	sc = device_get_softc(dev);
	if (!sc->sc_attached)
		return (0);

#ifdef PCI_IOV
	error = pci_iov_detach(dev);
	if (error != 0) {
		device_printf(dev, "Failed to disable SR-IOV\n");
		return (error);
	}
#endif

	sc->sc_attached = false;
	return (0);
}

static int
t4iov_detach(device_t dev)
{
	struct t4iov_softc *sc;
	int error;

	sc = device_get_softc(dev);
	if (sc->sc_attached) {
		error = t4iov_detach_child(dev);
		if (error)
			return (error);
	}
	if (sc->regs_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->regs_rid,
		    sc->regs_res);
	}
	return (0);
}

#ifdef PCI_IOV
static int
t4iov_iov_init(device_t dev, uint16_t num_vfs, const struct nvlist *config)
{

	/* XXX: The Linux driver sets up a vf_monitor task on T4 adapters. */
	return (0);
}

static void
t4iov_iov_uninit(device_t dev)
{
}

static int
t4iov_add_vf(device_t dev, uint16_t vfnum, const struct nvlist *config)
{
	const void *mac;
	struct t4iov_softc *sc;
	struct adapter *adap;
	uint8_t ma[ETHER_ADDR_LEN];
	size_t size;
	int rc;

	sc = device_get_softc(dev);
	MPASS(sc->sc_attached);
	MPASS(sc->sc_main != NULL);
	adap = device_get_softc(sc->sc_main);

	if (nvlist_exists_binary(config, "mac-addr")) {
		mac = nvlist_get_binary(config, "mac-addr", &size);
		bcopy(mac, ma, ETHER_ADDR_LEN);

		if (begin_synchronized_op(adap, NULL, SLEEP_OK | INTR_OK,
		    "t4vfma") != 0)
			return (ENXIO);
		rc = -t4_set_vf_mac(adap, sc->pf, vfnum + 1, 1, ma);
		end_synchronized_op(adap, 0);
		if (rc != 0) {
			device_printf(dev,
			    "Failed to set VF%d MAC address to "
			    "%02x:%02x:%02x:%02x:%02x:%02x, rc = %d\n", vfnum,
			    ma[0], ma[1], ma[2], ma[3], ma[4], ma[5], rc);
			return (rc);
		}
	}

	if (nvlist_exists_number(config, "vlan")) {
		uint16_t vlan = nvlist_get_number(config, "vlan");

		/* We can't restrict to VID 0 */
		if (vlan == DOT1Q_VID_NULL)
			return (ENOTSUP);

		if (vlan == VF_VLAN_TRUNK)
			vlan = DOT1Q_VID_NULL;

		if (begin_synchronized_op(adap, NULL, SLEEP_OK | INTR_OK,
		    "t4vfvl") != 0)
			return (ENXIO);
		rc = t4_set_vlan_acl(adap, sc->pf, vfnum + 1, vlan);
		end_synchronized_op(adap, 0);
		if (rc != 0) {
			device_printf(dev,
			    "Failed to set VF%d VLAN to %d, rc = %d\n",
			    vfnum, vlan, rc);
			return (rc);
		}
	}

	return (0);
}
#endif

static device_method_t t4iov_methods[] = {
	DEVMETHOD(device_probe,		t4iov_probe),
	DEVMETHOD(device_attach,	t4iov_attach),
	DEVMETHOD(device_detach,	t4iov_detach),

#ifdef PCI_IOV
	DEVMETHOD(pci_iov_init,		t4iov_iov_init),
	DEVMETHOD(pci_iov_uninit,	t4iov_iov_uninit),
	DEVMETHOD(pci_iov_add_vf,	t4iov_add_vf),
#endif

	DEVMETHOD(t4_attach_child,	t4iov_attach_child),
	DEVMETHOD(t4_detach_child,	t4iov_detach_child),

	DEVMETHOD_END
};

static driver_t t4iov_driver = {
	"t4iov",
	t4iov_methods,
	sizeof(struct t4iov_softc)
};

static device_method_t t5iov_methods[] = {
	DEVMETHOD(device_probe,		t5iov_probe),
	DEVMETHOD(device_attach,	t4iov_attach),
	DEVMETHOD(device_detach,	t4iov_detach),

#ifdef PCI_IOV
	DEVMETHOD(pci_iov_init,		t4iov_iov_init),
	DEVMETHOD(pci_iov_uninit,	t4iov_iov_uninit),
	DEVMETHOD(pci_iov_add_vf,	t4iov_add_vf),
#endif

	DEVMETHOD(t4_attach_child,	t4iov_attach_child),
	DEVMETHOD(t4_detach_child,	t4iov_detach_child),

	DEVMETHOD_END
};

static driver_t t5iov_driver = {
	"t5iov",
	t5iov_methods,
	sizeof(struct t4iov_softc)
};

static device_method_t t6iov_methods[] = {
	DEVMETHOD(device_probe,		t6iov_probe),
	DEVMETHOD(device_attach,	t4iov_attach),
	DEVMETHOD(device_detach,	t4iov_detach),

#ifdef PCI_IOV
	DEVMETHOD(pci_iov_init,		t4iov_iov_init),
	DEVMETHOD(pci_iov_uninit,	t4iov_iov_uninit),
	DEVMETHOD(pci_iov_add_vf,	t4iov_add_vf),
#endif

	DEVMETHOD(t4_attach_child,	t4iov_attach_child),
	DEVMETHOD(t4_detach_child,	t4iov_detach_child),

	DEVMETHOD_END
};

static driver_t t6iov_driver = {
	"t6iov",
	t6iov_methods,
	sizeof(struct t4iov_softc)
};

DRIVER_MODULE(t4iov, pci, t4iov_driver, 0, 0);
MODULE_VERSION(t4iov, 1);

DRIVER_MODULE(t5iov, pci, t5iov_driver, 0, 0);
MODULE_VERSION(t5iov, 1);

DRIVER_MODULE(t6iov, pci, t6iov_driver, 0, 0);
MODULE_VERSION(t6iov, 1);
