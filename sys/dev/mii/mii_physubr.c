/*	$NetBSD: mii_physubr.c,v 1.5 1999/08/03 19:41:49 drochner Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * Subroutines common to all PHYs.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/clock.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD: src/sys/dev/mii/mii_physubr.c,v 1.2 1999/08/28 00:42:14 peter Exp $";
#endif

void	mii_phy_auto_timeout __P((void *));

int
mii_phy_auto(mii, waitfor)
	struct mii_softc *mii;
	int waitfor;
{
	int bmsr, i;

	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		PHY_WRITE(mii, MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA);
		PHY_WRITE(mii, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(mii, MII_BMSR)) & BMSR_ACOMP)
				return (0);
			DELAY(1000);
#if 0
		if ((bmsr & BMSR_ACOMP) == 0)
			printf("%s: autonegotiation failed to complete\n",
			    mii->mii_dev.dv_xname);
#endif
		}

		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return (EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		mii->mii_flags |= MIIF_DOINGAUTO;
		timeout(mii_phy_auto_timeout, mii, hz >> 1);
	}
	return (EJUSTRETURN);
}

void
mii_phy_auto_timeout(arg)
	void *arg;
{
	struct mii_softc *mii = arg;
	int s, bmsr;

	s = splnet();
	mii->mii_flags &= ~MIIF_DOINGAUTO;
	bmsr = PHY_READ(mii, MII_BMSR);
#if 0
	if ((bmsr & BMSR_ACOMP) == 0)
		printf("%s: autonegotiation failed to complete\n",
		    sc->sc_dev.dv_xname);
#endif

	/* Update the media status. */
	(void) (*mii->mii_service)(mii, mii->mii_pdata, MII_POLLSTAT);
	splx(s);
}

void
mii_phy_reset(mii)
	struct mii_softc *mii;
{
	int reg, i;

	if (mii->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(mii, MII_BMCR, reg);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(mii, MII_BMCR); 
		if ((reg & BMCR_RESET) == 0)
			break;
		DELAY(1000);
	}

	if (mii->mii_inst != 0 && ((mii->mii_flags & MIIF_NOISOLATE) == 0))
		PHY_WRITE(mii, MII_BMCR, reg | BMCR_ISO);
}

/*
 * Given an ifmedia word, return the corresponding ANAR value.
 */
int
mii_anar(media)
	int media;
{
	int rv;

	switch (media & (IFM_TMASK|IFM_NMASK|IFM_FDX)) {
	case IFM_ETHER|IFM_10_T:
		rv = ANAR_10|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_10_T|IFM_FDX:
		rv = ANAR_10_FD|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_TX:
		rv = ANAR_TX|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_TX|IFM_FDX:
		rv = ANAR_TX_FD|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_T4:
		rv = ANAR_T4|ANAR_CSMA;
		break;
	default:
		rv = 0;
		break;
	}

	return (rv);
}

/*
 * Given a BMCR value, return the corresponding ifmedia word.
 */
int
mii_media_from_bmcr(bmcr)
	int bmcr;
{
	int rv = IFM_ETHER;

	if (bmcr & BMCR_S100)
		rv |= IFM_100_TX;
	else
		rv |= IFM_10_T;
	if (bmcr & BMCR_FDX)
		rv |= IFM_FDX;

	return (rv);
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_add_media(mii, bmsr, instance)
	struct mii_data *mii;
	int bmsr, instance;
{
	const char *sep = "";

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	if (bmsr & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, instance), 0);
		PRINT("10baseT");
	}
	if (bmsr & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, instance),
		    BMCR_FDX);
		PRINT("10baseT-FDX");
	}
	if (bmsr & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, instance),
		    BMCR_S100);
		PRINT("100baseTX");
	}
	if (bmsr & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, instance),
		    BMCR_S100|BMCR_FDX);
		PRINT("100baseTX-FDX");
	}
	if (bmsr & BMSR_100T4) {
		/*
		 * XXX How do you enable 100baseT4?  I assume we set
		 * XXX BMCR_S100 and then assume the PHYs will take
		 * XXX watever action is necessary to switch themselves
		 * XXX into T4 mode.
		 */
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, instance),
		    BMCR_S100);
		PRINT("100baseT4");
	}
	if (bmsr & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, instance),
		    BMCR_AUTOEN);
		PRINT("auto");
	}
#undef ADD
#undef PRINT
}
