/*	$NetBSD: miivar.h,v 1.8 1999/04/23 04:24:32 thorpej Exp $	*/

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
 *
 * $FreeBSD: src/sys/dev/mii/miivar.h,v 1.21.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _DEV_MII_MIIVAR_H_
#define	_DEV_MII_MIIVAR_H_

#include <sys/queue.h>

/*
 * Media Independent Interface configuration defintions.
 */

struct mii_softc;

/*
 * Callbacks from MII layer into network interface device driver.
 */
typedef	int (*mii_readreg_t)(struct device *, int, int);
typedef	void (*mii_writereg_t)(struct device *, int, int, int);
typedef	void (*mii_statchg_t)(struct device *);

/*
 * A network interface driver has one of these structures in its softc.
 * It is the interface from the network interface driver to the MII
 * layer.
 */
struct mii_data {
	struct ifmedia mii_media;	/* media information */
	struct ifnet *mii_ifp;		/* pointer back to network interface */

	/*
	 * For network interfaces with multiple PHYs, a list of all
	 * PHYs is required so they can all be notified when a media
	 * request is made.
	 */
	LIST_HEAD(mii_listhead, mii_softc) mii_phys;
	int mii_instance;

	/*
	 * PHY driver fills this in with active media status.
	 */
	int mii_media_status;
	int mii_media_active;

	/*
	 * Calls from MII layer into network interface driver.
	 */
	mii_readreg_t mii_readreg;
	mii_writereg_t mii_writereg;
	mii_statchg_t mii_statchg;
};
typedef struct mii_data mii_data_t;

/*
 * This call is used by the MII layer to call into the PHY driver
 * to perform a `service request'.
 */
typedef	int (*mii_downcall_t)(struct mii_softc *, struct mii_data *, int);

/*
 * Requests that can be made to the downcall.
 */
#define	MII_TICK	1	/* once-per-second tick */
#define	MII_MEDIACHG	2	/* user changed media; perform the switch */
#define	MII_POLLSTAT	3	/* user requested media status; fill it in */

/*
 * Each PHY driver's softc has one of these as the first member.
 * XXX This would be better named "phy_softc", but this is the name
 * XXX BSDI used, and we would like to have the same interface.
 */
struct mii_softc {
	device_t mii_dev;		/* generic device glue */
	
	LIST_ENTRY(mii_softc) mii_list;	/* entry on parent's PHY list */

	int mii_phy;			/* our MII address */
	int mii_inst;			/* instance for ifmedia */

	mii_downcall_t mii_service;	/* our downcall */
	struct mii_data *mii_pdata;	/* pointer to parent's mii_data */

	int mii_flags;			/* misc. flags; see below */
	int mii_capabilities;		/* capabilities from BMSR */
	int mii_extcapabilities;	/* extended capabilities */
	int mii_ticks;			/* MII_TICK counter */
	int mii_anegticks;		/* ticks before retrying aneg */
	int mii_media_active;		/* last active media */
	int mii_media_status;		/* last active status */
};
typedef struct mii_softc mii_softc_t;

/* mii_flags */
#define	MIIF_INITDONE	0x0001		/* has been initialized (mii_data) */
#define	MIIF_NOISOLATE	0x0002		/* do not isolate the PHY */
#define	MIIF_NOLOOP	0x0004		/* no loopback capability */
#define MIIF_AUTOTSLEEP	0x0010		/* use tsleep(), not callout() */
#define MIIF_HAVEFIBER	0x0020		/* from parent: has fiber interface */
#define	MIIF_HAVE_GTCR	0x0040		/* has 100base-T2/1000base-T CR */
#define	MIIF_IS_1000X	0x0080		/* is a 1000BASE-X device */
#define	MIIF_DOPAUSE	0x0100		/* advertise PAUSE capability */
#define	MIIF_IS_HPNA	0x0200		/* is a HomePNA device */
#define	MIIF_FORCEANEG	0x0400		/* force auto-negotiation */

/* Default mii_anegticks values */
#define	MII_ANEGTICKS		5
#define	MII_ANEGTICKS_GIGE	17

#define	MIIF_INHERIT_MASK	(MIIF_NOISOLATE|MIIF_NOLOOP|MIIF_AUTOTSLEEP)

/*
 * Used to attach a PHY to a parent.
 */
struct mii_attach_args {
	struct mii_data *mii_data;	/* pointer to parent data */
	int mii_phyno;			/* MII address */
	int mii_id1;			/* PHY ID register 1 */
	int mii_id2;			/* PHY ID register 2 */
	int mii_capmask;		/* capability mask from BMSR */
};
typedef struct mii_attach_args mii_attach_args_t;

/*
 * Used to match a PHY.
 */
struct mii_phydesc {
	u_int32_t mpd_oui;		/* the PHY's OUI */
	u_int32_t mpd_model;		/* the PHY's model */
	const char *mpd_name;		/* the PHY's name */
};
#define MII_PHY_DESC(a, b) { MII_OUI_ ## a, MII_MODEL_ ## a ## _ ## b, \
	MII_STR_ ## a ## _ ## b }
#define MII_PHY_END	{ 0, 0, NULL }

/*
 * An array of these structures map MII media types to BMCR/ANAR settings.
 */
struct mii_media {
	int	mm_bmcr;		/* BMCR settings for this media */
	int	mm_anar;		/* ANAR settings for this media */
	int	mm_gtcr;		/* 100base-T2 or 1000base-T CR */
};

#define	MII_MEDIA_NONE		0
#define	MII_MEDIA_10_T		1
#define	MII_MEDIA_10_T_FDX	2
#define	MII_MEDIA_100_T4	3
#define	MII_MEDIA_100_TX	4
#define	MII_MEDIA_100_TX_FDX	5
#define	MII_MEDIA_1000_X	6
#define	MII_MEDIA_1000_X_FDX	7
#define	MII_MEDIA_1000_T	8
#define	MII_MEDIA_1000_T_FDX	9
#define	MII_NMEDIA		10

#ifdef _KERNEL

#define PHY_READ(p, r) \
	MIIBUS_READREG((p)->mii_dev, (p)->mii_phy, (r))

#define PHY_WRITE(p, r, v) \
	MIIBUS_WRITEREG((p)->mii_dev, (p)->mii_phy, (r), (v))

extern devclass_t	miibus_devclass;
extern driver_t		miibus_driver;

int	miibus_probe(device_t);
int	miibus_attach(device_t);
int	miibus_detach(device_t);

int	mii_anar(int);
void	mii_down(struct mii_data *);
int	mii_mediachg(struct mii_data *);
void	mii_tick(struct mii_data *);
void	mii_pollstat(struct mii_data *);
int	mii_phy_probe(device_t, device_t *, ifm_change_cb_t, ifm_stat_cb_t);
void	mii_add_media(struct mii_softc *);
void	mii_phy_add_media(struct mii_softc *);

int	mii_phy_auto(struct mii_softc *);
int	mii_phy_detach(device_t dev);
void	mii_phy_down(struct mii_softc *);
void	mii_phy_reset(struct mii_softc *);
void	mii_phy_setmedia(struct mii_softc *sc);
void	mii_phy_update(struct mii_softc *, int);
int	mii_phy_tick(struct mii_softc *);

const struct mii_phydesc * mii_phy_match(const struct mii_attach_args *ma,
    const struct mii_phydesc *mpd);
const struct mii_phydesc * mii_phy_match_gen(const struct mii_attach_args *ma,
    const struct mii_phydesc *mpd, size_t endlen);
int mii_phy_dev_probe(device_t dev, const struct mii_phydesc *mpd, int mrv);

void	ukphy_status(struct mii_softc *);
#endif /* _KERNEL */

#endif /* _DEV_MII_MIIVAR_H_ */
