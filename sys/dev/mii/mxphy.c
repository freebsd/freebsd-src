/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Pseudo-driver for internal NWAY support on Macronix 98713/98715/98725
 * PMAC controller chips. The Macronix chips use the same internal
 * NWAY register layout as the DEC/Intel 21143. Technically we're
 * abusing the miibus code to handle the media selection and NWAY
 * support here since there is no MII interface. However the logical
 * operations are roughly the same, and the alternative is to create
 * a fake MII interface in the driver, which is harder to do.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <machine/clock.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <pci/if_mxreg.h>

#include "miibus_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

#define MX_SETBIT(sc, reg, x)                           \
        CSR_WRITE_4(sc, reg,                            \
                CSR_READ_4(sc, reg) | x)

#define MX_CLRBIT(sc, reg, x)                           \
        CSR_WRITE_4(sc, reg,                            \
                CSR_READ_4(sc, reg) & ~x)

#define MIIF_AUTOTIMEOUT	0x0004

static int mxphy_probe		__P((device_t));
static int mxphy_attach		__P((device_t));
static int mxphy_detach		__P((device_t));

static device_method_t mxphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		mxphy_probe),
	DEVMETHOD(device_attach,	mxphy_attach),
	DEVMETHOD(device_detach,	mxphy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t mxphy_devclass;

static driver_t mxphy_driver = {
	"mxphy",
	mxphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(mxphy, miibus, mxphy_driver, mxphy_devclass, 0, 0);

int	mxphy_service __P((struct mii_softc *, struct mii_data *, int));
void	mxphy_status __P((struct mii_softc *));
static int mxphy_auto		__P((struct mii_softc *, int));
static void mxphy_reset		__P((struct mii_softc *));

static int mxphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(dev);

	/*
	 * The mx driver will report a Macronix vendor and device
	 * ID to let us know that it wants us to attach.
	 */
	if (ma->mii_id1 != MX_VENDORID ||
	    ma->mii_id2 != MX_DEVICEID_987x5)
		return(ENXIO);

	device_set_desc(dev, "Macronix NWAY media interface");

	return (0);
}

static int mxphy_attach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	struct mx_softc *mx_sc;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = mxphy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOISOLATE;
	mii->mii_instance++;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);

	/*mxphy_reset(sc);*/
	mx_sc = mii->mii_ifp->if_softc;
	CSR_WRITE_4(mx_sc, MX_10BTSTAT, 0);
	CSR_WRITE_4(mx_sc, MX_10BTCTRL, 0);

	sc->mii_capabilities =
	    BMSR_ANEG|BMSR_100TXFDX|BMSR_100TXHDX|BMSR_10TFDX|BMSR_10THDX;
	sc->mii_capabilities &= ma->mii_capmask;
	device_printf(dev, " ");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->mii_capabilities, sc->mii_inst);
	printf("\n");
#undef ADD

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int mxphy_detach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(device_get_parent(dev));
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return(0);
}

int
mxphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct mx_softc		*mx_sc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;
	u_int32_t		mode;

	mx_sc = mii->mii_ifp->if_softc;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			return (0);
		}
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		sc->mii_flags = 0;
		mii->mii_media_active = IFM_NONE;
		mode = CSR_READ_4(mx_sc, MX_NETCFG);
		mode &= ~(MX_NETCFG_FULLDUPLEX|MX_NETCFG_PORTSEL|
		    MX_NETCFG_PCS|MX_NETCFG_SCRAMBLER|MX_NETCFG_SPEEDSEL);

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			mxphy_reset(sc);
			(void) mxphy_auto(sc, 0);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		case IFM_100_TX:
			mxphy_reset(sc);
			MX_CLRBIT(mx_sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
			mode |= MX_NETCFG_PORTSEL|MX_NETCFG_PCS|
			    MX_NETCFG_SCRAMBLER;
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX)
				mode |= MX_NETCFG_FULLDUPLEX;
			else
				mode &= ~MX_NETCFG_FULLDUPLEX;
			CSR_WRITE_4(mx_sc, MX_NETCFG, mode);
			break;
		case IFM_10_T:
			mxphy_reset(sc);
			MX_CLRBIT(mx_sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
			mode &= ~MX_NETCFG_PORTSEL;
			mode |= MX_NETCFG_SPEEDSEL;
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX)
				mode |= MX_NETCFG_FULLDUPLEX;
			else
				mode &= ~MX_NETCFG_FULLDUPLEX;
			CSR_WRITE_4(mx_sc, MX_NETCFG, mode);
			break;
		default:
			return(EINVAL);
			break;
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		if (sc->mii_flags & MIIF_DOINGAUTO) {
			if (++sc->mii_ticks != 5)
				return(0);
			else {
				sc->mii_ticks = 0;
				sc->mii_flags &= ~MIIF_DOINGAUTO;
				sc->mii_flags |= MIIF_AUTOTIMEOUT;
			}
		}

		sc->mii_flags &= ~MIIF_DOINGAUTO;

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = CSR_READ_4(mx_sc, MX_10BTSTAT) &
		    (MX_TSTAT_LS10|MX_TSTAT_LS100);

		if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX &&
		    !(reg & MX_TSTAT_LS100)) {
			if (sc->mii_flags & MIIF_AUTOTIMEOUT) {
				sc->mii_flags &= ~MIIF_AUTOTIMEOUT;
				break;
			} else
				return(0);
		} else if (IFM_SUBTYPE(mii->mii_media_active) == IFM_10_T &&
		    !(reg & MX_TSTAT_LS10)) {
			if (sc->mii_flags & MIIF_AUTOTIMEOUT) {
				sc->mii_flags &= ~MIIF_AUTOTIMEOUT;
				break;
			} else
				return(0);
		} else if (IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE &&
		    (!(reg & MX_TSTAT_LS10) || !(reg & MX_TSTAT_LS100))) {
			if (sc->mii_flags & MIIF_AUTOTIMEOUT) {
				sc->mii_flags &= ~MIIF_AUTOTIMEOUT;
				break;
			} else
				return(0);
		}

		sc->mii_ticks = 0;
		mxphy_reset(sc);
		mxphy_auto(sc, 0);

		break;
	}

	/* Update the media status. */
	mxphy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		MIIBUS_STATCHG(sc->mii_dev);
		sc->mii_active = mii->mii_media_active;
	}
	return (0);
}

void
mxphy_status(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	int reg, anlpar;
	struct mx_softc		*mx_sc;

	mx_sc = mii->mii_ifp->if_softc;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	reg = CSR_READ_4(mx_sc, MX_10BTSTAT) &
	     (MX_TSTAT_LS10|MX_TSTAT_LS100);

	if (!(reg & MX_TSTAT_LS10) || !(reg & MX_TSTAT_LS100))
		mii->mii_media_status |= IFM_ACTIVE;

	if (sc->mii_flags & MIIF_DOINGAUTO) {
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	if (CSR_READ_4(mx_sc, MX_10BTCTRL) & MX_TCTL_AUTONEGENBL &&
	    CSR_READ_4(mx_sc, MX_10BTSTAT) & MX_TSTAT_ANEGSTAT) {
		/* Erg, still trying, I guess... */
		if ((CSR_READ_4(mx_sc, MX_10BTSTAT) &
		    MX_ASTAT_AUTONEGCMP) != MX_ASTAT_AUTONEGCMP) {
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if (CSR_READ_4(mx_sc, MX_10BTSTAT) & MX_TSTAT_LP_CAN_NWAY) {
			anlpar = CSR_READ_4(mx_sc, MX_10BTSTAT) >> 16;
			if (anlpar & ANLPAR_T4)
				mii->mii_media_active |= IFM_100_T4;
			else if (anlpar & ANLPAR_TX_FD)
				mii->mii_media_active |= IFM_100_TX|IFM_FDX;
			else if (anlpar & ANLPAR_TX)
				mii->mii_media_active |= IFM_100_TX;
			else if (anlpar & ANLPAR_10_FD)
				mii->mii_media_active |= IFM_10_T|IFM_FDX;
			else if (anlpar & ANLPAR_10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_NONE;
			return;
		}
		/*
	 	 * If the other side doesn't support NWAY, then the
		 * best we can do is determine if we have a 10Mbps or
		 * 100Mbps link. There's no way to know if the link
		 * is full or half duplex, so we default to half duplex
		 * and hope that the user is clever enough to manually
		 * change the media settings if we're wrong.
		 */
		if (!(reg & MX_TSTAT_LS100))
			mii->mii_media_active |= IFM_100_TX;
		else if (!(reg & MX_TSTAT_LS10))
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_NONE;
		return;
	}

	if (CSR_READ_4(mx_sc, MX_NETCFG) & MX_NETCFG_SCRAMBLER)
		mii->mii_media_active |= IFM_100_TX;
	else
		mii->mii_media_active |= IFM_10_T;
	if (CSR_READ_4(mx_sc, MX_NETCFG) & MX_NETCFG_FULLDUPLEX)
		mii->mii_media_active |= IFM_FDX;


	return;
}

static int
mxphy_auto(mii, waitfor)
	struct mii_softc	*mii;
	int			waitfor;
{
	int			i;
	struct mx_softc		*sc;

	sc = mii->mii_pdata->mii_ifp->if_softc;

	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		CSR_WRITE_4(sc, MX_10BTCTRL, 0x3FFFF);
		MX_CLRBIT(sc, MX_NETCFG, MX_NETCFG_PORTSEL);
		MX_SETBIT(sc, MX_NETCFG, MX_NETCFG_FULLDUPLEX);
		MX_SETBIT(sc, MX_10BTCTRL, MX_TCTL_AUTONEGENBL);
		MX_SETBIT(sc, MX_10BTCTRL, MX_ASTAT_TXDISABLE);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((CSR_READ_4(sc, MX_10BTSTAT) & MX_TSTAT_ANEGSTAT)
			    == MX_ASTAT_AUTONEGCMP)
				return(0);
			DELAY(1000);
		}
		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return(EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0)
		mii->mii_flags |= MIIF_DOINGAUTO;

	return(EJUSTRETURN);
}

static void
mxphy_reset(mii)
	struct mii_softc	*mii;
{
	struct mx_softc		*sc;

	sc = mii->mii_pdata->mii_ifp->if_softc;

	MX_SETBIT(sc, MX_SIARESET, MX_SIA_RESET_NWAY);
	DELAY(1000);
	MX_CLRBIT(sc, MX_SIARESET, MX_SIA_RESET_NWAY);

	return;
}

