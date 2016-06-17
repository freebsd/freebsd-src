/*
 * ibm_ocp_phy.c
 *
 * PHY drivers for the ibm ocp ethernet driver. Borrowed
 * from sungem_phy.c, though I only kept the generic MII
 * driver for now.
 * 
 * This file should be shared with other drivers or eventually
 * merged as the "low level" part of miilib
 * 
 * (c) 2003, Benjamin Herrenscmidt (benh@kernel.crashing.org)
 *
 */

#include <linux/config.h>

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/delay.h>

#include "ibm_ocp_phy.h"


static inline int __phy_read(struct mii_phy* phy, int id, int reg)
{
	return phy->mdio_read(phy->dev, id, reg);
}

static inline void __phy_write(struct mii_phy* phy, int id, int reg, int val)
{
	phy->mdio_write(phy->dev, id, reg, val);
}

static inline int phy_read(struct mii_phy* phy, int reg)
{
	return phy->mdio_read(phy->dev, phy->mii_id, reg);
}

static inline void phy_write(struct mii_phy* phy, int reg, int val)
{
	phy->mdio_write(phy->dev, phy->mii_id, reg, val);
}

static int reset_one_mii_phy(struct mii_phy* phy, int phy_id)
{
	u16 val;
	int limit = 10000;
	
	val = __phy_read(phy, phy_id, MII_BMCR);
	val &= ~BMCR_ISOLATE;
	val |= BMCR_RESET;
	__phy_write(phy, phy_id, MII_BMCR, val);

	udelay(100);

	while (limit--) {
		val = __phy_read(phy, phy_id, MII_BMCR);
		if ((val & BMCR_RESET) == 0)
			break;
		udelay(10);
	}
	if ((val & BMCR_ISOLATE) && limit > 0)
		__phy_write(phy, phy_id, MII_BMCR, val & ~BMCR_ISOLATE);
	
	return (limit <= 0);
}

static int genmii_setup_aneg(struct mii_phy *phy, u32 advertise)
{
	u16 ctl, adv;
	
	phy->autoneg = 1;
	phy->speed = SPEED_10;
	phy->duplex = DUPLEX_HALF;
	phy->pause = 0;
	phy->advertising = advertise;

	/* Setup standard advertise */
	adv = phy_read(phy, MII_ADVERTISE);
	adv &= ~(ADVERTISE_ALL | ADVERTISE_100BASE4);
	if (advertise & ADVERTISED_10baseT_Half)
		adv |= ADVERTISE_10HALF;
	if (advertise & ADVERTISED_10baseT_Full)
		adv |= ADVERTISE_10FULL;
	if (advertise & ADVERTISED_100baseT_Half)
		adv |= ADVERTISE_100HALF;
	if (advertise & ADVERTISED_100baseT_Full)
		adv |= ADVERTISE_100FULL;
	phy_write(phy, MII_ADVERTISE, adv);

	/* Start/Restart aneg */
	ctl = phy_read(phy, MII_BMCR);
	ctl |= (BMCR_ANENABLE | BMCR_ANRESTART);
	phy_write(phy, MII_BMCR, ctl);

	return 0;
}

static int genmii_setup_forced(struct mii_phy *phy, int speed, int fd)
{
	u16 ctl;
	
	phy->autoneg = 0;
	phy->speed = speed;
	phy->duplex = fd;
	phy->pause = 0;

	ctl = phy_read(phy, MII_BMCR);
	ctl &= ~(BMCR_FULLDPLX|BMCR_SPEED100|BMCR_ANENABLE);

	/* First reset the PHY */
	phy_write(phy, MII_BMCR, ctl | BMCR_RESET);

	/* Select speed & duplex */
	switch(speed) {
	case SPEED_10:
		break;
	case SPEED_100:
		ctl |= BMCR_SPEED100;
		break;
	case SPEED_1000:
	default:
		return -EINVAL;
	}
	if (fd == DUPLEX_FULL)
		ctl |= BMCR_FULLDPLX;
	phy_write(phy, MII_BMCR, ctl);

	return 0;
}

static int genmii_poll_link(struct mii_phy *phy)
{
	u16 status;
	
	(void)phy_read(phy, MII_BMSR);
	status = phy_read(phy, MII_BMSR);
	if ((status & BMSR_LSTATUS) == 0)
		return 0;
	if (phy->autoneg && !(status & BMSR_ANEGCOMPLETE))
		return 0;
	return 1;
}

static int genmii_read_link(struct mii_phy *phy)
{
	u16 lpa;

	if (phy->autoneg) {
		lpa = phy_read(phy, MII_LPA);

		if (lpa & (LPA_10FULL | LPA_100FULL))
			phy->duplex = DUPLEX_FULL;
		else
			phy->duplex = DUPLEX_HALF;
		if (lpa & (LPA_100FULL | LPA_100HALF))
			phy->speed = SPEED_100;
		else
			phy->speed = SPEED_10;
		phy->pause = 0;
	}
	/* On non-aneg, we assume what we put in BMCR is the speed,
	 * though magic-aneg shouldn't prevent this case from occurring
	 */

	 return 0;
}


#define MII_BASIC_FEATURES	(SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full | \
				 SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full | \
				 SUPPORTED_Autoneg | SUPPORTED_TP | SUPPORTED_MII)
#define MII_GBIT_FEATURES	(MII_BASIC_FEATURES | \
				 SUPPORTED_1000baseT_Half | SUPPORTED_1000baseT_Full)


/* Generic implementation for most 10/100 PHYs */
static struct mii_phy_ops generic_phy_ops = {
	setup_aneg:	genmii_setup_aneg,
	setup_forced:	genmii_setup_forced,
	poll_link:	genmii_poll_link,
	read_link:	genmii_read_link
};

static struct mii_phy_def genmii_phy_def = {
	phy_id:		0x00000000,
	phy_id_mask:	0x00000000,
	name:		"Generic MII",
	features:	MII_BASIC_FEATURES,
	magic_aneg:	0,
	ops:		&generic_phy_ops
};

static struct mii_phy_def* mii_phy_table[] = {
	&genmii_phy_def,
	NULL
};

int mii_phy_probe(struct mii_phy *phy, int mii_id)
{
	int rc;
	u32 id;
	struct mii_phy_def* def;
	int i;
	
	phy->autoneg = 0;
	phy->advertising = 0;
	phy->mii_id = mii_id;
	phy->speed = 0;
	phy->duplex = 0;
	phy->pause = 0;
	
	/* Take PHY out of isloate mode and reset it. */
	rc = reset_one_mii_phy(phy, mii_id);
	if (rc)
		return -ENODEV;

	/* Read ID and find matching entry */	
	id = (phy_read(phy, MII_PHYSID1) << 16 | phy_read(phy, MII_PHYSID2))
			 	& 0xfffffff0;
	for (i=0; (def = mii_phy_table[i]) != NULL; i++)
		if ((id & def->phy_id_mask) == def->phy_id)
			break;
	/* Should never be NULL (we have a generic entry), but... */
	if (def == NULL)
		return -ENODEV;

	phy->def = def;

	/* Setup default advertising */
	phy->advertising = def->features;

	return 0;
}

MODULE_LICENSE("GPL");
