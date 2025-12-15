/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Scott Long
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

#include "opt_acpi.h"
#include "opt_thunderbolt.h"

/* PCIe bridge for Thunderbolt */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>
#include <sys/rman.h>

#include <machine/pci_cfgreg.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_private.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>
#include <machine/md_var.h>

#include <dev/thunderbolt/tb_reg.h>
#include <dev/thunderbolt/tb_pcib.h>
#include <dev/thunderbolt/nhi_var.h>
#include <dev/thunderbolt/nhi_reg.h>
#include <dev/thunderbolt/tbcfg_reg.h>
#include <dev/thunderbolt/tb_debug.h>
#include "tb_if.h"

static int	tb_pcib_probe(device_t);
static int	tb_pcib_attach(device_t);
static int	tb_pcib_detach(device_t);
static int	tb_pcib_lc_mailbox(device_t, struct tb_lcmbox_cmd *);
static int	tb_pcib_pcie2cio_read(device_t, u_int, u_int, u_int,
     uint32_t *);
static int	tb_pcib_pcie2cio_write(device_t, u_int, u_int, u_int, uint32_t);
static int	tb_pcib_find_ufp(device_t, device_t *);
static int	tb_pcib_get_debug(device_t, u_int *);

static int	tb_pci_probe(device_t);
static int	tb_pci_attach(device_t);
static int	tb_pci_detach(device_t);

struct tb_pcib_ident {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subvendor;
	uint16_t	subdevice;
	uint32_t	flags;		/* This follows the tb_softc flags */
	const char	*desc;
} tb_pcib_identifiers[] = {
	{ VENDOR_INTEL, TB_DEV_AR_2C, 0xffff, 0xffff, TB_GEN_TB3|TB_HWIF_AR,
	    "Thunderbolt 3 PCI-PCI Bridge (Alpine Ridge 2C)" },
	{ VENDOR_INTEL, TB_DEV_AR_LP, 0xffff, 0xffff, TB_GEN_TB3|TB_HWIF_AR,
	    "Thunderbolt 3 PCI-PCI Bridge (Alpine Ridge LP)" },
	{ VENDOR_INTEL, TB_DEV_AR_C_4C, 0xffff, 0xffff, TB_GEN_TB3|TB_HWIF_AR,
	    "Thunderbolt 3 PCI-PCI Bridge (Alpine Ridge C 4C)" },
	{ VENDOR_INTEL, TB_DEV_AR_C_2C, 0xffff, 0xffff, TB_GEN_TB3|TB_HWIF_AR,
	    "Thunderbolt 3 PCI-PCI Bridge C (Alpine Ridge C 2C)" },
	{ VENDOR_INTEL, TB_DEV_ICL_0, 0xffff, 0xffff, TB_GEN_TB3|TB_HWIF_ICL,
	    "Thunderbolt 3 PCI-PCI Bridge (IceLake)" },
	{ VENDOR_INTEL, TB_DEV_ICL_1, 0xffff, 0xffff, TB_GEN_TB3|TB_HWIF_ICL,
	    "Thunderbolt 3 PCI-PCI Bridge (IceLake)" },
	{ 0, 0, 0, 0, 0, NULL }
};

static struct tb_pcib_ident *
tb_pcib_find_ident(device_t dev)
{
	struct tb_pcib_ident *n;
	uint16_t v, d, sv, sd;

	v = pci_get_vendor(dev);
	d = pci_get_device(dev);
	sv = pci_get_subvendor(dev);
	sd = pci_get_subdevice(dev);

	for (n = tb_pcib_identifiers; n->vendor != 0; n++) {
		if ((n->vendor != v) || (n->device != d))
			continue;
		if (((n->subvendor != 0xffff) && (n->subvendor != sv)) ||
		    ((n->subdevice != 0xffff) && (n->subdevice != sd)))
			continue;
		return (n);
	}

	return (NULL);
}

static void
tb_pcib_get_tunables(struct tb_pcib_softc *sc)
{
	char tmpstr[80], oid[80];

	/* Set the default */
	sc->debug = 0;

	/* Grab global variables */
	bzero(oid, 80);
	if (TUNABLE_STR_FETCH("hw.tbolt.debug_level", oid, 80) != 0)
		tb_parse_debug(&sc->debug, oid);

	/* Grab instance variables */
	bzero(oid, 80);
	snprintf(tmpstr, sizeof(tmpstr), "dev.tbolt.%d.debug_level",
	    device_get_unit(sc->dev));
	if (TUNABLE_STR_FETCH(tmpstr, oid, 80) != 0)
		tb_parse_debug(&sc->debug, oid);

	return;
}

static int
tb_pcib_setup_sysctl(struct tb_pcib_softc *sc)
{
	struct sysctl_ctx_list	*ctx = NULL;
	struct sysctl_oid	*tree = NULL;

	ctx = device_get_sysctl_ctx(sc->dev);
	if (ctx != NULL)
		tree = device_get_sysctl_tree(sc->dev);

	if (tree == NULL) {
		tb_printf(sc, "Error: cannot create sysctl nodes\n");
		return (EINVAL);
	}
	sc->sysctl_tree = tree;
	sc->sysctl_ctx = ctx;

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree),
	    OID_AUTO, "debug_level", CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE,
	    &sc->debug, 0, tb_debug_sysctl, "A", "Thunderbolt debug level");

	return (0);
}

/*
 * This is used for both the PCI and ACPI attachments.  It shouldn't return
 * 0, doing so will force the ACPI attachment to fail.
 */
int
tb_pcib_probe_common(device_t dev, char *desc)
{
	device_t ufp;
	struct tb_pcib_ident *n;
	char *suffix;

	if ((n = tb_pcib_find_ident(dev)) != NULL) {
		ufp = NULL;
		if ((TB_FIND_UFP(dev, &ufp) == 0) && (ufp == dev))
			suffix = "(Upstream port)";
		else
			suffix = "(Downstream port)";
		snprintf(desc, TB_DESC_MAX, "%s %s", n->desc, suffix);
		return (BUS_PROBE_VENDOR);
	}
	return (ENXIO);
}

static int
tb_pcib_probe(device_t dev)
{
	char desc[TB_DESC_MAX];
	int val;

	if ((val = tb_pcib_probe_common(dev, desc)) <= 0)
		device_set_desc_copy(dev, desc);

	return (val);
}

int
tb_pcib_attach_common(device_t dev)
{
	device_t ufp;
	struct tb_pcib_ident *n;
	struct tb_pcib_softc *sc;
	uint32_t val;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->vsec = -1;

	n = tb_pcib_find_ident(dev);
	KASSERT(n != NULL, ("Cannot find TB ident"));
	sc->flags = n->flags;

	tb_pcib_get_tunables(sc);
	tb_pcib_setup_sysctl(sc);

	/* XXX Is this necessary for ACPI attachments? */
	tb_debug(sc, DBG_BRIDGE, "busmaster status was %s\n",
	    (pci_read_config(dev, PCIR_COMMAND, 2) & PCIM_CMD_BUSMASTEREN)
	    ? "enabled" : "disabled");
	pci_enable_busmaster(dev);

	/*
	 * Determine if this is an upstream or downstream facing device, and
	 * whether it's the root of the Thunderbolt topology.  It's too bad
	 * that there aren't unique PCI ID's to help with this.
	 */
	ufp = NULL;
	if ((TB_FIND_UFP(dev, &ufp) == 0) && (ufp != NULL)) {
		if (ufp == dev) {
			sc->flags |= TB_FLAGS_ISUFP;
			if (TB_FIND_UFP(device_get_parent(dev), NULL) ==
			    EOPNOTSUPP) {
				sc->flags |= TB_FLAGS_ISROOT;
			}
		}
	}

	/*
	 * Find the PCI Vendor Specific Extended Capability.  It's the magic
	 * wand to configuring the Thunderbolt root bridges.
	 */
	if (TB_IS_AR(sc) || TB_IS_TR(sc)) {
		error = pci_find_extcap(dev, PCIZ_VENDOR, &sc->vsec);
		if (error) {
			tb_printf(sc, "Cannot find VSEC capability: %d\n",
			    error);
			return (ENXIO);
		}
	}

	/*
	 * Take the AR bridge out of low-power mode.
	 * XXX AR only?
	 */
	if ((1 || TB_IS_AR(sc)) && TB_IS_ROOT(sc)) {
		struct tb_lcmbox_cmd cmd;

		cmd.cmd = LC_MBOXOUT_CMD_SXEXIT_TBT;
		cmd.data_in = 0;

		error = TB_LC_MAILBOX(dev, &cmd);
		tb_debug(sc, DBG_BRIDGE, "SXEXIT returned error= %d resp= 0x%x "
		    "data= 0x%x\n", error, cmd.cmd_resp, cmd.data_out);
	}

	/* The downstream facing port on AR needs some help */
	if (TB_IS_AR(sc) && TB_IS_DFP(sc)) {
		tb_debug(sc, DBG_BRIDGE, "Doing AR L1 fixup\n");
		val = pci_read_config(dev, sc->vsec + AR_VSCAP_1C, 4);
		tb_debug(sc, DBG_BRIDGE|DBG_FULL, "VSEC+0x1c= 0x%08x\n", val);
		val |= (1 << 8);
		pci_write_config(dev, sc->vsec + AR_VSCAP_1C, val, 4);

		val = pci_read_config(dev, sc->vsec + AR_VSCAP_B0, 4);
		tb_debug(sc, DBG_BRIDGE|DBG_FULL, "VSEC+0xb0= 0x%08x\n", val);
		val |= (1 << 12);
		pci_write_config(dev, sc->vsec + AR_VSCAP_B0, val, 4);
	}

	return (0);
}

static int
tb_pcib_attach(device_t dev)
{
	int error;

	error = tb_pcib_attach_common(dev);
	if (error)
		return (error);
	return (pcib_attach(dev));
}

static int
tb_pcib_detach(device_t dev)
{
	struct tb_pcib_softc *sc;
	int error;

	sc = device_get_softc(dev);

	tb_debug(sc, DBG_BRIDGE|DBG_ROUTER|DBG_EXTRA, "tb_pcib_detach\n");

	/* Put the AR bridge back to sleep */
	/* XXX disable this until power control for downstream switches works */
	if (0 && TB_IS_ROOT(sc)) {
		struct tb_lcmbox_cmd cmd;

		cmd.cmd = LC_MBOXOUT_CMD_GO2SX;
		cmd.data_in = 0;

		error = TB_LC_MAILBOX(dev, &cmd);
		tb_debug(sc, DBG_BRIDGE, "SXEXIT returned error= %d resp= 0x%x "
		    "data= 0x%x\n", error, cmd.cmd_resp, cmd.data_out);
	}

	return (pcib_detach(dev));
}

/* Read/write the Link Controller registers in CFG space */
static int
tb_pcib_lc_mailbox(device_t dev, struct tb_lcmbox_cmd *cmd)
{
	struct tb_pcib_softc *sc;
	uint32_t regcmd, result;
	uint16_t m_in, m_out;
	int vsec, i;

	sc = device_get_softc(dev);
	vsec = TB_PCIB_VSEC(dev);
	if (vsec == -1)
		return (EOPNOTSUPP);

	if (TB_IS_AR(sc)) {
		m_in = AR_LC_MBOX_IN;
		m_out = AR_LC_MBOX_OUT;
	} else if (TB_IS_ICL(sc)) {
		m_in = ICL_LC_MBOX_IN;
		m_out = ICL_LC_MBOX_OUT;
	} else
		return (EOPNOTSUPP);

	/* Set the valid bit to signal we're sending a command */
	regcmd = LC_MBOXOUT_VALID | (cmd->cmd & LC_MBOXOUT_CMD_MASK);
	regcmd |= (cmd->data_in << LC_MBOXOUT_DATA_SHIFT);
	tb_debug(sc, DBG_BRIDGE|DBG_FULL, "Writing LC cmd 0x%x\n", regcmd);
	pci_write_config(dev, vsec + m_out, regcmd, 4);

	for (i = 0; i < 10; i++) {
		pause("nhi", 1 * hz);
		result = pci_read_config(dev, vsec + m_in, 4);
		tb_debug(sc, DBG_BRIDGE|DBG_FULL, "LC Mailbox= 0x%08x\n",
		    result);
		if ((result & LC_MBOXIN_DONE) != 0)
			break;
	}

	/* Clear the valid bit to signal we're done sending the command */
	pci_write_config(dev, vsec + m_out, 0, 4);

	cmd->cmd_resp = result & LC_MBOXIN_CMD_MASK;
	cmd->data_out = result >> LC_MBOXIN_CMD_SHIFT;

	if ((result & LC_MBOXIN_DONE) == 0)
		return (ETIMEDOUT);

	return (0);
}

static int
tb_pcib_pcie2cio_wait(device_t dev, u_int timeout)
{
#if 0
	uint32_t val;
	int vsec;

	vsec = TB_PCIB_VSEC(dev);
	do {
                pci_read_config(dev, vsec + PCIE2CIO_CMD, &val);
                if ((val & PCIE2CIO_CMD_START) == 0) {
                        if (val & PCIE2CIO_CMD_TIMEOUT)
                                break;
                        return 0;
                }

                msleep(50);
        } while (time_before(jiffies, end));

#endif
        return ETIMEDOUT;
}

static int
tb_pcib_pcie2cio_read(device_t dev, u_int space, u_int port, u_int offset,
    uint32_t *val)
{
#if 0
	uint32_t cmd;
	int ret, vsec;

	vsec = TB_PCIB_VSEC(dev);
	if (vsec == -1)
		return (EOPNOTSUPP);

	cmd = index;
        cmd |= (port << PCIE2CIO_CMD_PORT_SHIFT) & PCIE2CIO_CMD_PORT_MASK;
        cmd |= (space << PCIE2CIO_CMD_CS_SHIFT) & PCIE2CIO_CMD_CS_MASK;
        cmd |= PCIE2CIO_CMD_START;
	pci_write_config(dev, vsec + PCIE2CIO_CMD, cmd, 4);

        if ((ret = pci2cio_wait_completion(dev, 5000)) != 0)
                return (ret);

        *val = pci_read_config(dev, vsec + PCIE2CIO_RDDATA, 4);
#endif
	return (0);
}

static int
tb_pcib_pcie2cio_write(device_t dev, u_int space, u_int port, u_int offset,
    uint32_t val)
{
#if 0
	uint32_t cmd;
	int ret, vsec;

	vsec = TB_PCIB_VSEC(dev);
	if (vsec == -1)
		return (EOPNOTSUPP);

        pci_write_config(dev, vsec + PCIE2CIO_WRDATA, val, 4);

        cmd = index;
        cmd |= (port << PCIE2CIO_CMD_PORT_SHIFT) & PCIE2CIO_CMD_PORT_MASK;
        cmd |= (space << PCIE2CIO_CMD_CS_SHIFT) & PCIE2CIO_CMD_CS_MASK;
        cmd |= PCIE2CIO_CMD_WRITE | PCIE2CIO_CMD_START;
        pci_write_config(dev, vsec + PCIE2CIO_CMD, cmd);

#endif
        return (tb_pcib_pcie2cio_wait(dev, 5000));
}

/*
 * The Upstream Facing Port (UFP) in a switch is special, it's the function
 * that responds to some of the special programming mailboxes.  It can't be
 * differentiated by PCI ID, so a heuristic approach to identifying it is
 * required.
 */
static int
tb_pcib_find_ufp(device_t dev, device_t *ufp)
{
	device_t upstream;
	struct tb_pcib_softc *sc;
	uint32_t vsec, val;
	int error;

	upstream = NULL;
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EOPNOTSUPP);

	if (TB_IS_UFP(sc)) {
		upstream = dev;
		error = 0;
		goto out;
	}

	/*
	 * This register is supposed to be filled in on the upstream port
	 * and tells how many downstream ports there are.  It doesn't seem
	 * to get filled in on AR host controllers, but is on various
	 * peripherals.
	 */
	error = pci_find_extcap(dev, PCIZ_VENDOR, &vsec);
	if (error == 0) {
		val = pci_read_config(dev, vsec + 0x18, 4);
		if ((val & 0x1f) > 0) {
			upstream = dev;
			goto out;
		}
	}

	/*
	 * Since we can't trust that the VSEC register is filled in, the only
	 * other option is to see if we're at the top of the topology, which
	 * implies that we're at the upstream port of the host controller.
	 */
	error = TB_FIND_UFP(device_get_parent(dev), ufp);
	if (error == EOPNOTSUPP) {
		upstream = dev;
		error = 0;
		goto out;
	} else
		return (error);

out:
	if (ufp != NULL)
		*ufp = upstream;

	return (error);
}

static int
tb_pcib_get_debug(device_t dev, u_int *debug)
{
	struct tb_pcib_softc *sc;

	sc = device_get_softc(dev);
	if ((sc == NULL) || (debug == NULL))
		return (EOPNOTSUPP);

	*debug = sc->debug;
	return (0);
}

static device_method_t tb_pcib_methods[] = {
	DEVMETHOD(device_probe, 	tb_pcib_probe),
	DEVMETHOD(device_attach,	tb_pcib_attach),
	DEVMETHOD(device_detach,	tb_pcib_detach),

	DEVMETHOD(tb_lc_mailbox,	tb_pcib_lc_mailbox),
	DEVMETHOD(tb_pcie2cio_read,	tb_pcib_pcie2cio_read),
	DEVMETHOD(tb_pcie2cio_write,	tb_pcib_pcie2cio_write),

	DEVMETHOD(tb_find_ufp,		tb_pcib_find_ufp),
	DEVMETHOD(tb_get_debug,		tb_pcib_get_debug),

	DEVMETHOD_END
};

DEFINE_CLASS_1(tbolt, tb_pcib_driver, tb_pcib_methods,
    sizeof(struct tb_pcib_softc), pcib_driver);
DRIVER_MODULE_ORDERED(tb_pcib, pci, tb_pcib_driver,
    NULL, NULL, SI_ORDER_MIDDLE);
MODULE_DEPEND(tb_pcib, pci, 1, 1, 1);

static int
tb_pci_probe(device_t dev)
{
	struct tb_pcib_ident *n;
	device_t parent;
	devclass_t dc;

	/*
	 * This driver is only valid if the parent device is a PCI-PCI
	 * bridge.  To determine that, check if the grandparent is a
	 * PCI bus.
	 */
	parent = device_get_parent(dev);
	dc = device_get_devclass(device_get_parent(parent));
	if (strcmp(devclass_get_name(dc), "pci") != 0)
		return (ENXIO);

	if ((n = tb_pcib_find_ident(parent)) != NULL) {
		switch (n->flags & TB_GEN_MASK) {
		case TB_GEN_TB1:
			device_set_desc(dev, "Thunderbolt 1 Link");
			break;
		case TB_GEN_TB2:
			device_set_desc(dev, "Thunderbolt 2 Link");
			break;
		case TB_GEN_TB3:
			device_set_desc(dev, "Thunderbolt 3 Link");
			break;
		case TB_GEN_USB4:
			device_set_desc(dev, "USB4 Link");
			break;
		case TB_GEN_UNK:
			/* Fallthrough */
		default:
			device_set_desc(dev, "Thunderbolt Link");
		}
		return (BUS_PROBE_VENDOR);
	}
	return (ENXIO);
}

static int
tb_pci_attach(device_t dev)
{

	return (pci_attach(dev));
}

static int
tb_pci_detach(device_t dev)
{

	return (pci_detach(dev));
}

static device_method_t tb_pci_methods[] = {
	DEVMETHOD(device_probe, tb_pci_probe),
	DEVMETHOD(device_attach, tb_pci_attach),
	DEVMETHOD(device_detach, tb_pci_detach),

	DEVMETHOD(tb_find_ufp, tb_generic_find_ufp),
	DEVMETHOD(tb_get_debug, tb_generic_get_debug),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pci, tb_pci_driver, tb_pci_methods, sizeof(struct pci_softc),
    pci_driver);
DRIVER_MODULE(tb_pci, pcib, tb_pci_driver, NULL, NULL);
MODULE_DEPEND(tb_pci, pci, 1, 1, 1);
MODULE_VERSION(tb_pci, 1);
