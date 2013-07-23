/*-
 * Copyright (c) 2011-2012 Stefan Bethke.
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
#ifndef	__ARSWITCHVAR_H__
#define	__ARSWITCHVAR_H__

typedef enum {
	AR8X16_SWITCH_NONE,
	AR8X16_SWITCH_AR7240,
	AR8X16_SWITCH_AR8216,
	AR8X16_SWITCH_AR8226,
	AR8X16_SWITCH_AR8316,
} ar8x16_switch_type;

/*
 * XXX TODO: start using this where required
 */
#define	AR8X16_IS_SWITCH(_sc, _type) \
	    (!!((_sc)->sc_switchtype == AR8X16_SWITCH_ ## _type))

struct arswitch_softc {
	struct mtx	sc_mtx;		/* serialize access to softc */
	device_t	sc_dev;
	int		phy4cpu;	/* PHY4 is connected to the CPU */
	int		numphys;	/* PHYs we manage */
	int		is_rgmii;	/* PHY mode is RGMII (XXX which PHY?) */
	int		is_gmii;	/* PHY mode is GMII (XXX which PHY?) */
	int		page;
	ar8x16_switch_type	sc_switchtype;
	char		*ifname[AR8X16_NUM_PHYS];
	device_t	miibus[AR8X16_NUM_PHYS];
	struct ifnet	*ifp[AR8X16_NUM_PHYS];
	struct callout	callout_tick;
	etherswitch_info_t info;

	/* VLANs support */
	int		vid[AR8X16_MAX_VLANS];
	uint32_t	vlan_mode;

	struct {
		int (* arswitch_hw_setup) (struct arswitch_softc *);
		int (* arswitch_hw_global_setup) (struct arswitch_softc *);
	} hal;
};

#define	ARSWITCH_LOCK(_sc)			\
	    mtx_lock(&(_sc)->sc_mtx)
#define	ARSWITCH_UNLOCK(_sc)			\
	    mtx_unlock(&(_sc)->sc_mtx)
#define	ARSWITCH_LOCK_ASSERT(_sc, _what)	\
	    mtx_assert(&(_sc)->sc_mtx, (_what))
#define	ARSWITCH_TRYLOCK(_sc)			\
	    mtx_trylock(&(_sc)->sc_mtx)

#if defined(DEBUG)
#define DPRINTF(dev, args...) device_printf(dev, args)
#define DEVERR(dev, err, fmt, args...) do { \
		if (err != 0) device_printf(dev, fmt, err, args); \
	} while (0)
#define DEBUG_INCRVAR(var)	do { \
		var++; \
	} while (0)
#else
#define DPRINTF(dev, args...)
#define DEVERR(dev, err, fmt, args...)
#define DEBUG_INCRVAR(var)
#endif

#endif	/* __ARSWITCHVAR_H__ */

