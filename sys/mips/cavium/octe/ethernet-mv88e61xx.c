/*-
 * Copyright (c) 2010 Juli Mallett <jmallett@FreeBSD.org>
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

/*
 * Interface to the Marvell 88E61XX SMI/MDIO.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <dev/mii/mii.h>

#include <net/ethernet.h>
#include <net/if.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

#define	MV88E61XX_SMI_PHY_SW	0x10	/* Switch PHY.  */

#define	MV88E61XX_SMI_REG_CMD	0x00	/* Indirect command register.  */
#define	 MV88E61XX_SMI_CMD_BUSY		0x8000	/* Busy bit.  */
#define	 MV88E61XX_SMI_CMD_22		0x1000	/* Clause 22 (default 45.)  */
#define	 MV88E61XX_SMI_CMD_READ		0x0800	/* Read command.  */
#define	 MV88E61XX_SMI_CMD_WRITE	0x0400	/* Write command.  */
#define	 MV88E61XX_SMI_CMD_PHY(phy)	(((phy) & 0x1f) << 5)
#define	 MV88E61XX_SMI_CMD_REG(reg)	((reg) & 0x1f)

#define	MV88E61XX_SMI_REG_DAT	0x01	/* Indirect data register.  */

static int cvm_oct_mv88e61xx_mdio_read(struct ifnet *, int, int);
static void cvm_oct_mv88e61xx_mdio_write(struct ifnet *, int, int, int);
static int cvm_oct_mv88e61xx_smi_read(struct ifnet *, int, int);
static void cvm_oct_mv88e61xx_smi_write(struct ifnet *, int, int, int);
static int cvm_oct_mv88e61xx_smi_wait(struct ifnet *, int);

int
cvm_oct_mv88e61xx_setup_device(struct ifnet *ifp)
{
	cvm_oct_private_t *priv = (cvm_oct_private_t *)ifp->if_softc;

	priv->mdio_read = cvm_oct_mv88e61xx_mdio_read;
	priv->mdio_write = cvm_oct_mv88e61xx_mdio_write;

	return (0);
}

static int
cvm_oct_mv88e61xx_mdio_read(struct ifnet *ifp, int phy_id, int location)
{
	/*
	 * Intercept reads of MII_BMSR.  The miibus uses this to determine
	 * PHY presence and we only want it to look for a PHY attachment
	 * for the switch PHY itself.  The PHY driver will talk to all of
	 * the other ports as need be.
	 */
	switch (location) {
	case MII_BMSR:
		if (phy_id != MV88E61XX_SMI_PHY_SW)
			return (0);
		return (BMSR_EXTSTAT | BMSR_ACOMP | BMSR_LINK);
	default:
		return (cvm_oct_mv88e61xx_smi_read(ifp, phy_id, location));
	}
}

static void
cvm_oct_mv88e61xx_mdio_write(struct ifnet *ifp, int phy_id, int location, int val)
{
	return (cvm_oct_mv88e61xx_smi_write(ifp, phy_id, location, val));
}

static int
cvm_oct_mv88e61xx_smi_read(struct ifnet *ifp, int phy_id, int location)
{
	int error;

	error = cvm_oct_mv88e61xx_smi_wait(ifp, phy_id);
	if (error != 0)
		return (0);

	cvm_oct_mdio_write(ifp, phy_id, MV88E61XX_SMI_REG_CMD,
	    MV88E61XX_SMI_CMD_BUSY | MV88E61XX_SMI_CMD_22 |
	    MV88E61XX_SMI_CMD_READ | MV88E61XX_SMI_CMD_PHY(phy_id) |
	    MV88E61XX_SMI_CMD_REG(location));

	error = cvm_oct_mv88e61xx_smi_wait(ifp, phy_id);
	if (error != 0)
		return (0);

	return (cvm_oct_mdio_read(ifp, phy_id, MV88E61XX_SMI_REG_DAT));
}

static void
cvm_oct_mv88e61xx_smi_write(struct ifnet *ifp, int phy_id, int location, int val)
{
	cvm_oct_mv88e61xx_smi_wait(ifp, phy_id);
	cvm_oct_mdio_write(ifp, phy_id, MV88E61XX_SMI_REG_DAT, val);
	cvm_oct_mdio_write(ifp, phy_id, MV88E61XX_SMI_REG_CMD,
	    MV88E61XX_SMI_CMD_BUSY | MV88E61XX_SMI_CMD_22 |
	    MV88E61XX_SMI_CMD_WRITE | MV88E61XX_SMI_CMD_PHY(phy_id) |
	    MV88E61XX_SMI_CMD_REG(location));
	cvm_oct_mv88e61xx_smi_wait(ifp, phy_id);
}

static int
cvm_oct_mv88e61xx_smi_wait(struct ifnet *ifp, int phy_id)
{
	uint16_t cmd;
	unsigned i;

	for (i = 0; i < 10000; i++) {
		cmd = cvm_oct_mdio_read(ifp, phy_id, MV88E61XX_SMI_REG_CMD);
		if ((cmd & MV88E61XX_SMI_CMD_BUSY) == 0)
			return (0);
	}
	return (ETIMEDOUT);
}
