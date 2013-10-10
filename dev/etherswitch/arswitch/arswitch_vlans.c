/*-
 * Copyright (c) 2013 Luiz Otavio O Souza.
 * Copyright (c) 2011-2012 Stefan Bethke.
 * Copyright (c) 2012 Adrian Chadd.
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
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>

#include <dev/mii/mii.h>

#include <dev/etherswitch/etherswitch.h>
#include <dev/etherswitch/arswitch/arswitchreg.h>
#include <dev/etherswitch/arswitch/arswitchvar.h>
#include <dev/etherswitch/arswitch/arswitch_reg.h>
#include <dev/etherswitch/arswitch/arswitch_vlans.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

static int
arswitch_vlan_op(struct arswitch_softc *sc, uint32_t op, uint32_t vid,
	uint32_t data)
{
	int err;

	if (arswitch_waitreg(sc->sc_dev, AR8X16_REG_VLAN_CTRL,
	    AR8X16_VLAN_ACTIVE, 0, 5))
		return (EBUSY);

	/* Load the vlan data if needed. */
	if (op == AR8X16_VLAN_OP_LOAD) {
		err = arswitch_writereg(sc->sc_dev, AR8X16_REG_VLAN_DATA,
		    (data & AR8X16_VLAN_MEMBER) | AR8X16_VLAN_VALID);
		if (err)
			return (err);
	}

	if (vid != 0)
		op |= ((vid & ETHERSWITCH_VID_MASK) << AR8X16_VLAN_VID_SHIFT);
	op |= AR8X16_VLAN_ACTIVE;
	arswitch_writereg(sc->sc_dev, AR8X16_REG_VLAN_CTRL, op);

	/* Wait for command processing. */
	if (arswitch_waitreg(sc->sc_dev, AR8X16_REG_VLAN_CTRL,
	    AR8X16_VLAN_ACTIVE, 0, 5))
		return (EBUSY);

	return (0);
}

static int
arswitch_flush_dot1q_vlan(struct arswitch_softc *sc)
{

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	return (arswitch_vlan_op(sc, AR8X16_VLAN_OP_FLUSH, 0, 0));
}

static int
arswitch_purge_dot1q_vlan(struct arswitch_softc *sc, int vid)
{

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	return (arswitch_vlan_op(sc, AR8X16_VLAN_OP_PURGE, vid, 0));
}

static int
arswitch_get_dot1q_vlan(struct arswitch_softc *sc, uint32_t *ports, int vid)
{
	uint32_t reg;
	int err;

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	err = arswitch_vlan_op(sc, AR8X16_VLAN_OP_GET, vid, 0);
	if (err)
		return (err);

	reg = arswitch_readreg(sc->sc_dev, AR8X16_REG_VLAN_DATA);
	if ((reg & AR8X16_VLAN_VALID) == 0) {
		*ports = 0;
		return (EINVAL);
	}
	reg &= ((1 << (sc->numphys + 1)) - 1);
	*ports = reg;
	return (0);
}

static int
arswitch_set_dot1q_vlan(struct arswitch_softc *sc, uint32_t ports, int vid)
{
	int err;

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	err = arswitch_vlan_op(sc, AR8X16_VLAN_OP_LOAD, vid, ports);
	if (err)
		return (err);
	return (0);
}

static int
arswitch_get_port_vlan(struct arswitch_softc *sc, uint32_t *ports, int vid)
{
	int port;
	uint32_t reg;

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	/* For port based vlans the vlanid is the same as the port index. */
	port = vid & ETHERSWITCH_VID_MASK;
	reg = arswitch_readreg(sc->sc_dev, AR8X16_REG_PORT_VLAN(port));
	*ports = (reg >> AR8X16_PORT_VLAN_DEST_PORTS_SHIFT);
	*ports &= AR8X16_VLAN_MEMBER;
	return (0);
}

static int
arswitch_set_port_vlan(struct arswitch_softc *sc, uint32_t ports, int vid)
{
	int err, port;

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	/* For port based vlans the vlanid is the same as the port index. */
	port = vid & ETHERSWITCH_VID_MASK;
	err = arswitch_modifyreg(sc->sc_dev, AR8X16_REG_PORT_VLAN(port),
	    AR8X16_VLAN_MEMBER << AR8X16_PORT_VLAN_DEST_PORTS_SHIFT,
	    (ports & AR8X16_VLAN_MEMBER) << AR8X16_PORT_VLAN_DEST_PORTS_SHIFT);
	if (err)
		return (err);
	return (0);
}

/*
 * Reset vlans to default state.
 */
void
arswitch_reset_vlans(struct arswitch_softc *sc)
{
	uint32_t ports;
	int i, j;

	ARSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	ARSWITCH_LOCK(sc);

	/* Reset all vlan data. */
	memset(sc->vid, 0, sizeof(sc->vid));

	/* Disable the QinQ and egress filters for all ports. */
	for (i = 0; i <= sc->numphys; i++) {
		if (arswitch_modifyreg(sc->sc_dev, AR8X16_REG_PORT_CTRL(i),
		    0x3 << AR8X16_PORT_CTRL_EGRESS_VLAN_MODE_SHIFT |
		    AR8X16_PORT_CTRL_DOUBLE_TAG, 0)) {
			ARSWITCH_UNLOCK(sc);
			return;
		}
	}

	if (arswitch_flush_dot1q_vlan(sc)) {
		ARSWITCH_UNLOCK(sc);
		return;
	}

	if (sc->vlan_mode == ETHERSWITCH_VLAN_DOT1Q) {
		/*
		 * Reset the port based vlan settings and turn on the
		 * ingress filter for all ports.
		 */
		ports = 0;
		for (i = 0; i <= sc->numphys; i++)
			arswitch_modifyreg(sc->sc_dev,
			    AR8X16_REG_PORT_VLAN(i),
			    AR8X16_PORT_VLAN_MODE_MASK |
			    AR8X16_VLAN_MEMBER <<
			    AR8X16_PORT_VLAN_DEST_PORTS_SHIFT,
			    AR8X16_PORT_VLAN_MODE_SECURE <<
			    AR8X16_PORT_VLAN_MODE_SHIFT);

		/*
		 * Setup vlan 1 as PVID for all switch ports.  Add all ports
		 * as members of vlan 1.
		 */
		sc->vid[0] = 1;
		/* Set PVID for everyone. */
		for (i = 0; i <= sc->numphys; i++)
			arswitch_set_pvid(sc, i, sc->vid[0]);
		ports = 0;
		for (i = 0; i <= sc->numphys; i++)
			ports |= (1 << i);
		arswitch_set_dot1q_vlan(sc, ports, sc->vid[0]);
		sc->vid[0] |= ETHERSWITCH_VID_VALID;
	} else if (sc->vlan_mode == ETHERSWITCH_VLAN_PORT) {
		/* Initialize the port based vlans. */
		for (i = 0; i <= sc->numphys; i++) {
			sc->vid[i] = i | ETHERSWITCH_VID_VALID;
			ports = 0;
			for (j = 0; j <= sc->numphys; j++)
				ports |= (1 << j);
			arswitch_modifyreg(sc->sc_dev,
			    AR8X16_REG_PORT_VLAN(i),
			    AR8X16_PORT_VLAN_MODE_MASK |
			    AR8X16_VLAN_MEMBER <<
			    AR8X16_PORT_VLAN_DEST_PORTS_SHIFT,
			    ports << AR8X16_PORT_VLAN_DEST_PORTS_SHIFT |
			    AR8X16_PORT_VLAN_MODE_SECURE <<
			    AR8X16_PORT_VLAN_MODE_PORT_ONLY);
		}
	} else {
		/* Disable the ingress filter and get everyone on all vlans. */
		for (i = 0; i <= sc->numphys; i++)
			arswitch_modifyreg(sc->sc_dev,
			    AR8X16_REG_PORT_VLAN(i),
			    AR8X16_PORT_VLAN_MODE_MASK |
			    AR8X16_VLAN_MEMBER <<
			    AR8X16_PORT_VLAN_DEST_PORTS_SHIFT,
			    AR8X16_VLAN_MEMBER <<
			    AR8X16_PORT_VLAN_DEST_PORTS_SHIFT |
			    AR8X16_PORT_VLAN_MODE_SECURE <<
			    AR8X16_PORT_VLAN_MODE_PORT_ONLY);
	}
	ARSWITCH_UNLOCK(sc);
}

int
arswitch_getvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	struct arswitch_softc *sc;
	int err;

	sc = device_get_softc(dev);
	ARSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	if (vg->es_vlangroup > sc->info.es_nvlangroups)
		return (EINVAL);

	/* Reset the members ports. */
	vg->es_untagged_ports = 0;
	vg->es_member_ports = 0;

	/* Not supported. */
	vg->es_fid = 0;

	/* Vlan ID. */
	ARSWITCH_LOCK(sc);
	vg->es_vid = sc->vid[vg->es_vlangroup];
	if ((vg->es_vid & ETHERSWITCH_VID_VALID) == 0) {
		ARSWITCH_UNLOCK(sc);
		return (0);
	}

	/* Member Ports. */
	switch (sc->vlan_mode) {
	case ETHERSWITCH_VLAN_DOT1Q:
		err = arswitch_get_dot1q_vlan(sc, &vg->es_member_ports,
		    vg->es_vid);
		break;
	case ETHERSWITCH_VLAN_PORT:
		err = arswitch_get_port_vlan(sc, &vg->es_member_ports,
		    vg->es_vid);
		break;
	default:
		vg->es_member_ports = 0;
		err = -1;
	}
	ARSWITCH_UNLOCK(sc);
	vg->es_untagged_ports = vg->es_member_ports;
	return (err);
}

int
arswitch_setvgroup(device_t dev, etherswitch_vlangroup_t *vg)
{
	struct arswitch_softc *sc;
	int err, vid;

	sc = device_get_softc(dev);
	ARSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	/* Check VLAN mode. */
	if (sc->vlan_mode == 0)
		return (EINVAL);

	/*
	 * Check if we are changing the vlanid for an already used vtu entry.
	 * Then purge the entry first.
	 */
	ARSWITCH_LOCK(sc);
	vid = sc->vid[vg->es_vlangroup];
	if (sc->vlan_mode == ETHERSWITCH_VLAN_DOT1Q &&
	    (vid & ETHERSWITCH_VID_VALID) != 0 &&
	    (vid & ETHERSWITCH_VID_MASK) !=
	    (vg->es_vid & ETHERSWITCH_VID_MASK)) {
		err = arswitch_purge_dot1q_vlan(sc, vid);
		if (err) {
			ARSWITCH_UNLOCK(sc);
			return (err);
		}
	}

	/* Vlan ID. */
	if (sc->vlan_mode == ETHERSWITCH_VLAN_DOT1Q) {
		sc->vid[vg->es_vlangroup] = vg->es_vid & ETHERSWITCH_VID_MASK;
		/* Setting the vlanid to zero disables the vlangroup. */
		if (sc->vid[vg->es_vlangroup] == 0) {
			ARSWITCH_UNLOCK(sc);
			return (0);
		}
		sc->vid[vg->es_vlangroup] |= ETHERSWITCH_VID_VALID;
		vid = sc->vid[vg->es_vlangroup];
	}

	/* Member Ports. */
	switch (sc->vlan_mode) {
	case ETHERSWITCH_VLAN_DOT1Q:
		err = arswitch_set_dot1q_vlan(sc, vg->es_member_ports, vid);
		break;
	case ETHERSWITCH_VLAN_PORT:
		err = arswitch_set_port_vlan(sc, vg->es_member_ports, vid);
		break;
	default:
		err = -1;
	}
	ARSWITCH_UNLOCK(sc);
	return (err);
}

int
arswitch_get_pvid(struct arswitch_softc *sc, int port, int *pvid)
{
	uint32_t reg;

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	reg = arswitch_readreg(sc->sc_dev, AR8X16_REG_PORT_VLAN(port));
	*pvid = reg & 0xfff;
	return (0);
}

int
arswitch_set_pvid(struct arswitch_softc *sc, int port, int pvid)
{

	ARSWITCH_LOCK_ASSERT(sc, MA_OWNED);
	return (arswitch_modifyreg(sc->sc_dev,
	    AR8X16_REG_PORT_VLAN(port), 0xfff, pvid));
}
