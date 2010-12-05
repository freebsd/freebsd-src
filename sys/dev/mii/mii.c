/*	$NetBSD: mii.c,v 1.12 1999/08/03 19:41:49 drochner Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * MII bus layer, glues MII-capable network interface drivers to sharable
 * PHY drivers.  This exports an interface compatible with BSD/OS 3.0's,
 * plus some NetBSD extensions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h> 

#include <net/if.h>
#include <net/if_media.h>
#include <net/route.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

MODULE_VERSION(miibus, 1);

#include "miibus_if.h"

static int miibus_print_child(device_t dev, device_t child);
static int miibus_read_ivar(device_t dev, device_t child, int which,
    uintptr_t *result);
static int miibus_child_location_str(device_t bus, device_t child, char *buf,
    size_t buflen);
static int miibus_child_pnpinfo_str(device_t bus, device_t child, char *buf,
    size_t buflen);
static int miibus_readreg(device_t, int, int);
static int miibus_writereg(device_t, int, int, int);
static void miibus_statchg(device_t);
static void miibus_linkchg(device_t);
static void miibus_mediainit(device_t);

static device_method_t miibus_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		miibus_probe),
	DEVMETHOD(device_attach,	miibus_attach),
	DEVMETHOD(device_detach,	miibus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	miibus_print_child),
	DEVMETHOD(bus_read_ivar,	miibus_read_ivar),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_child_pnpinfo_str, miibus_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str, miibus_child_location_str),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	miibus_readreg),
	DEVMETHOD(miibus_writereg,	miibus_writereg),
	DEVMETHOD(miibus_statchg,	miibus_statchg),    
	DEVMETHOD(miibus_linkchg,	miibus_linkchg),    
	DEVMETHOD(miibus_mediainit,	miibus_mediainit),    

	{ 0, 0 }
};

devclass_t miibus_devclass;

driver_t miibus_driver = {
	"miibus",
	miibus_methods,
	sizeof(struct mii_data)
};

struct miibus_ivars {
	struct ifnet	*ifp;
	ifm_change_cb_t	ifmedia_upd;
	ifm_stat_cb_t	ifmedia_sts;
	int		mii_flags;
};

int
miibus_probe(device_t dev)
{

	device_set_desc(dev, "MII bus");

	return (BUS_PROBE_SPECIFIC);
}

int
miibus_attach(device_t dev)
{
	struct miibus_ivars	*ivars;
	struct mii_attach_args	*ma;
	struct mii_data		*mii;
	device_t		*children;
	int			i, nchildren;

	mii = device_get_softc(dev);
	nchildren = 0;
	if (device_get_children(dev, &children, &nchildren) == 0) {
		for (i = 0; i < nchildren; i++) {
			ma = device_get_ivars(children[i]);
			ma->mii_data = mii;
		}
		free(children, M_TEMP);
	}
	if (nchildren == 0) {
		device_printf(dev, "cannot get children\n");
		return (ENXIO);
	}
	ivars = device_get_ivars(dev);
	ifmedia_init(&mii->mii_media, IFM_IMASK, ivars->ifmedia_upd,
	    ivars->ifmedia_sts);
	mii->mii_ifp = ivars->ifp;
	mii->mii_ifp->if_capabilities |= IFCAP_LINKSTATE;
	mii->mii_ifp->if_capenable |= IFCAP_LINKSTATE;
	LIST_INIT(&mii->mii_phys);

	return (bus_generic_attach(dev));
}

int
miibus_detach(device_t dev)
{
	struct mii_data		*mii;

	bus_generic_detach(dev);
	mii = device_get_softc(dev);
	ifmedia_removeall(&mii->mii_media);
	mii->mii_ifp = NULL;

	return (0);
}

static int
miibus_print_child(device_t dev, device_t child)
{
	struct mii_attach_args *ma;
	int retval;

	ma = device_get_ivars(child);
	retval = bus_print_child_header(dev, child);
	retval += printf(" PHY %d", ma->mii_phyno);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
miibus_read_ivar(device_t dev, device_t child __unused, int which,
    uintptr_t *result)
{
	struct miibus_ivars *ivars;

	/*
	 * NB: this uses the instance variables of the miibus rather than
	 * its PHY children.
	 */
	ivars = device_get_ivars(dev);
	switch (which) {
	case MIIBUS_IVAR_FLAGS:
		*result = ivars->mii_flags;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
miibus_child_pnpinfo_str(device_t bus __unused, device_t child, char *buf,
    size_t buflen)
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(child);
	snprintf(buf, buflen, "oui=0x%x model=0x%x rev=0x%x",
	    MII_OUI(ma->mii_id1, ma->mii_id2),
	    MII_MODEL(ma->mii_id2), MII_REV(ma->mii_id2));
	return (0);
}

static int
miibus_child_location_str(device_t bus __unused, device_t child, char *buf,
    size_t buflen)
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(child);
	snprintf(buf, buflen, "phyno=%d", ma->mii_phyno);
	return (0);
}

static int
miibus_readreg(device_t dev, int phy, int reg)
{
	device_t		parent;

	parent = device_get_parent(dev);
	return (MIIBUS_READREG(parent, phy, reg));
}

static int
miibus_writereg(device_t dev, int phy, int reg, int data)
{
	device_t		parent;

	parent = device_get_parent(dev);
	return (MIIBUS_WRITEREG(parent, phy, reg, data));
}

static void
miibus_statchg(device_t dev)
{
	device_t		parent;
	struct mii_data		*mii;

	parent = device_get_parent(dev);
	MIIBUS_STATCHG(parent);

	mii = device_get_softc(dev);
	mii->mii_ifp->if_baudrate = ifmedia_baudrate(mii->mii_media_active);
}

static void
miibus_linkchg(device_t dev)
{
	struct mii_data		*mii;
	device_t		parent;
	int			link_state;

	parent = device_get_parent(dev);
	MIIBUS_LINKCHG(parent);

	mii = device_get_softc(dev);
	
	if (mii->mii_media_status & IFM_AVALID) {
		if (mii->mii_media_status & IFM_ACTIVE)
			link_state = LINK_STATE_UP;
		else
			link_state = LINK_STATE_DOWN;
	} else
		link_state = LINK_STATE_UNKNOWN;
	if_link_state_change(mii->mii_ifp, link_state);
}

static void
miibus_mediainit(device_t dev)
{
	struct mii_data		*mii;
	struct ifmedia_entry	*m;
	int			media = 0;

	/* Poke the parent in case it has any media of its own to add. */
	MIIBUS_MEDIAINIT(device_get_parent(dev));

	mii = device_get_softc(dev);
	LIST_FOREACH(m, &mii->mii_media.ifm_list, ifm_list) {
		media = m->ifm_media;
		if (media == (IFM_ETHER | IFM_AUTO))
			break;
	}

	ifmedia_set(&mii->mii_media, media);
}

/*
 * Helper function used by network interface drivers, attaches the miibus and
 * the PHYs to the network interface driver parent.
 */
int
mii_attach(device_t dev, device_t *miibus, struct ifnet *ifp,
    ifm_change_cb_t ifmedia_upd, ifm_stat_cb_t ifmedia_sts, int capmask,
    int phyloc, int offloc, int flags)
{
	struct miibus_ivars *ivars;
	struct mii_attach_args ma, *args;
	device_t *children, phy;
	int bmsr, first, i, nchildren, offset, phymax, phymin, rv;

	if (phyloc != MII_PHY_ANY && offloc != MII_OFFSET_ANY) {
		printf("%s: phyloc and offloc specified\n", __func__);
		return (EINVAL);
	}

	if (offloc != MII_OFFSET_ANY && (offloc < 0 || offloc >= MII_NPHY)) {
		printf("%s: ivalid offloc %d\n", __func__, offloc);
		return (EINVAL);
	}

	if (phyloc == MII_PHY_ANY) {
		phymin = 0;
		phymax = MII_NPHY - 1;
	} else {
		if (phyloc < 0 || phyloc >= MII_NPHY) {
			printf("%s: ivalid phyloc %d\n", __func__, phyloc);
			return (EINVAL);
		}
		phymin = phymax = phyloc;
	}

	first = 0;
	if (*miibus == NULL) {
		first = 1;
		ivars = malloc(sizeof(*ivars), M_DEVBUF, M_NOWAIT);
		if (ivars == NULL)
			return (ENOMEM);
		ivars->ifp = ifp;
		ivars->ifmedia_upd = ifmedia_upd;
		ivars->ifmedia_sts = ifmedia_sts;
		ivars->mii_flags = flags;
		*miibus = device_add_child(dev, "miibus", -1);
		if (*miibus == NULL) {
			rv = ENXIO;
			goto fail;
		}
		device_set_ivars(*miibus, ivars);
	} else {
		ivars = device_get_ivars(*miibus);
		if (ivars->ifp != ifp || ivars->ifmedia_upd != ifmedia_upd ||
		    ivars->ifmedia_sts != ifmedia_sts ||
		    ivars->mii_flags != flags) {
			printf("%s: non-matching invariant\n", __func__);
			return (EINVAL);
		}
		/*
		 * Assignment of the attach arguments mii_data for the first
		 * pass is done in miibus_attach(), i.e. once the miibus softc
		 * has been allocated.
		 */
		ma.mii_data = device_get_softc(*miibus);
	} 

	ma.mii_capmask = capmask;

	phy = NULL;
	offset = 0;
	for (ma.mii_phyno = phymin; ma.mii_phyno <= phymax; ma.mii_phyno++) {
		/*
		 * Make sure we haven't already configured a PHY at this
		 * address.  This allows mii_attach() to be called
		 * multiple times.
		 */
		if (device_get_children(*miibus, &children, &nchildren) == 0) {
			for (i = 0; i < nchildren; i++) {
				args = device_get_ivars(children[i]);
				if (args->mii_phyno == ma.mii_phyno) {
					/*
					 * Yes, there is already something
					 * configured at this address.
					 */
					free(children, M_TEMP);
					goto skip;
				}
			}
			free(children, M_TEMP);
		}

		/*
		 * Check to see if there is a PHY at this address.  Note,
		 * many braindead PHYs report 0/0 in their ID registers,
		 * so we test for media in the BMSR.
	 	 */
		bmsr = MIIBUS_READREG(dev, ma.mii_phyno, MII_BMSR);
		if (bmsr == 0 || bmsr == 0xffff ||
		    (bmsr & (BMSR_EXTSTAT | BMSR_MEDIAMASK)) == 0) {
			/* Assume no PHY at this address. */
			continue;
		}

		/*
		 * There is a PHY at this address.  If we were given an
		 * `offset' locator, skip this PHY if it doesn't match.
		 */
		if (offloc != MII_OFFSET_ANY && offloc != offset)
			goto skip;

		/*
		 * Extract the IDs. Braindead PHYs will be handled by
		 * the `ukphy' driver, as we have no ID information to
		 * match on.
	 	 */
		ma.mii_id1 = MIIBUS_READREG(dev, ma.mii_phyno, MII_PHYIDR1);
		ma.mii_id2 = MIIBUS_READREG(dev, ma.mii_phyno, MII_PHYIDR2);

		args = malloc(sizeof(struct mii_attach_args), M_DEVBUF,
		    M_NOWAIT);
		if (args == NULL)
			goto skip;
		bcopy((char *)&ma, (char *)args, sizeof(ma));
		phy = device_add_child(*miibus, NULL, -1);
		if (phy == NULL) {
			free(args, M_DEVBUF);
			goto skip;
		}
		device_set_ivars(phy, args);
 skip:
		offset++;
	}

	if (first != 0) {
		if (phy == NULL) {
			rv = ENXIO;
			goto fail;
		}
		rv = bus_generic_attach(dev);
		if (rv != 0)
			goto fail;

		/* Attaching of the PHY drivers is done in miibus_attach(). */
		return (0);
	}
	rv = bus_generic_attach(*miibus);
	if (rv != 0)
		goto fail;

	return (0);

 fail:
	if (*miibus != NULL)
		device_delete_child(dev, *miibus);
	free(ivars, M_DEVBUF);
	if (first != 0)
		*miibus = NULL;
	return (rv);
}

/*
 * Media changed; notify all PHYs.
 */
int
mii_mediachg(struct mii_data *mii)
{
	struct mii_softc *child;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int rv;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	LIST_FOREACH(child, &mii->mii_phys, mii_list) {
		/*
		 * If the media indicates a different PHY instance,
		 * isolate this one.
		 */
		if (IFM_INST(ife->ifm_media) != child->mii_inst) {
			if ((child->mii_flags & MIIF_NOISOLATE) != 0) {
				device_printf(child->mii_dev, "%s: "
				    "can't handle non-zero PHY instance %d\n",
				    __func__, child->mii_inst);
				continue;
			}
			PHY_WRITE(child, MII_BMCR, PHY_READ(child, MII_BMCR) |
			    BMCR_ISO);
			continue;
		}
		rv = (*child->mii_service)(child, mii, MII_MEDIACHG);
		if (rv)
			return (rv);
	}
	return (0);
}

/*
 * Call the PHY tick routines, used during autonegotiation.
 */
void
mii_tick(struct mii_data *mii)
{
	struct mii_softc *child;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	LIST_FOREACH(child, &mii->mii_phys, mii_list) {
		/*
		 * If this PHY instance isn't currently selected, just skip
		 * it.
		 */
		if (IFM_INST(ife->ifm_media) != child->mii_inst)
			continue;
		(void)(*child->mii_service)(child, mii, MII_TICK);
	}
}

/*
 * Get media status from PHYs.
 */
void
mii_pollstat(struct mii_data *mii)
{
	struct mii_softc *child;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	LIST_FOREACH(child, &mii->mii_phys, mii_list) {
		/*
		 * If we're not polling this PHY instance, just skip it.
		 */
		if (IFM_INST(ife->ifm_media) != child->mii_inst)
			continue;
		(void)(*child->mii_service)(child, mii, MII_POLLSTAT);
	}
}

/*
 * Inform the PHYs that the interface is down.
 */
void
mii_down(struct mii_data *mii)
{
	struct mii_softc *child;

	LIST_FOREACH(child, &mii->mii_phys, mii_list)
		mii_phy_down(child);
}
