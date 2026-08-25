/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bitstring.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/iflib.h>

#include "aq_device.h"

#include "aq_fw.h"
#include "aq_dbg.h"

/*
 * Single source of truth for the supported link speeds.
 *
 * TODO: Split this by media type so the AQC100 SFP+ devices use suitable
 * fibre/SFP+ IFM_* subtypes instead of advertising the copper IFM_*_T set.
 */
static const struct aq_media_map {
	uint32_t		link_bit;	/* AQ_LINK_* capability bit */
	enum aq_fw_link_speed	fw_rate;	/* aq_fw_* rate */
	int			ifm;		/* IFM_* media subtype */
	uint32_t		mbps;		/* link speed, Mbit/s */
} aq_media_types[] = {
	{ AQ_LINK_10M,  aq_fw_10M,  IFM_10_T,   10 },
	{ AQ_LINK_100M, aq_fw_100M, IFM_100_TX, 100 },
	{ AQ_LINK_1G,   aq_fw_1G,   IFM_1000_T, 1000 },
	{ AQ_LINK_2G5,  aq_fw_2G5,  IFM_2500_T, 2500 },
	{ AQ_LINK_5G,   aq_fw_5G,   IFM_5000_T, 5000 },
	{ AQ_LINK_10G,  aq_fw_10G,  IFM_10G_T,  10000 },
};

void
aq_mediastatus_update(struct aq_dev *aq_dev, uint32_t link_speed,
const struct aq_hw_fc_info *fc_neg)
{
	struct aq_hw *hw = &aq_dev->hw;
	u_int i;

	aq_dev->media_active = 0;
	if (fc_neg->fc_rx)
		aq_dev->media_active |= IFM_ETH_RXPAUSE;
	if (fc_neg->fc_tx)
		aq_dev->media_active |= IFM_ETH_TXPAUSE;

	for (i = 0; i < nitems(aq_media_types); i++)
		if (link_speed == aq_media_types[i].mbps)
			break;
	aq_dev->media_active |= i < nitems(aq_media_types) ?
	    (aq_media_types[i].ifm | IFM_FDX) : IFM_NONE;

	if (hw->link_rate == aq_fw_speed_auto)
		aq_dev->media_active |= IFM_AUTO;
}

void
aq_mediastatus(if_t ifp, struct ifmediareq *ifmr)
{
	struct aq_dev *aq_dev = iflib_get_softc(if_getsoftc(ifp));

	ifmr->ifm_active = IFM_ETHER;
	ifmr->ifm_status = IFM_AVALID;

	if (aq_dev->linkup)
		ifmr->ifm_status |= IFM_ACTIVE;

	ifmr->ifm_active |= aq_dev->media_active;
}

int
aq_mediachange(if_t ifp)
{
	struct aq_dev          *aq_dev = iflib_get_softc(if_getsoftc(ifp));
	struct aq_hw      *hw = &aq_dev->hw;
	int                old_media_rate = if_getbaudrate(ifp);
	int                old_link_speed = hw->link_rate;
	struct ifmedia    *ifm = iflib_get_media(aq_dev->ctx);
	int                user_media = IFM_SUBTYPE(ifm->ifm_media);
	uint64_t           media_rate;
	u_int              i;

	AQ_DBG_ENTERA("media 0x%x", user_media);

	if (!(ifm->ifm_media & IFM_ETHER)) {
		device_printf(aq_dev->dev,
		    "%s(): aq_dev interface - bad media: 0x%X\n", __FUNCTION__,
		    ifm->ifm_media);
		return (0);    // should never happen
	}

	switch (user_media) {
	case IFM_AUTO: // auto-select media
		hw->link_rate = aq_fw_speed_auto;
		media_rate = -1;
	break;

	case IFM_NONE: // disable media
		media_rate = 0;
		hw->link_rate = 0;
		iflib_link_state_change(aq_dev->ctx, LINK_STATE_DOWN,  0);
	break;

	default:
		for (i = 0; i < nitems(aq_media_types); i++)
			if (user_media == aq_media_types[i].ifm)
				break;
		if (i == nitems(aq_media_types)) {
			device_printf(hw->dev, "unknown media: 0x%X\n",
			    user_media);
			return (0);
		}
		hw->link_rate = aq_media_types[i].fw_rate;
		media_rate = (uint64_t)aq_media_types[i].mbps * 1000;
	break;
	}
	hw->fc.fc_rx = (ifm->ifm_media & IFM_ETH_RXPAUSE) ? 1 : 0;
	hw->fc.fc_tx = (ifm->ifm_media & IFM_ETH_TXPAUSE) ? 1 : 0;

	/* In down state just remember new link speed */
	if (!(if_getflags(ifp) & IFF_UP))
		return (0);

	if ((media_rate != old_media_rate) ||
	    (hw->link_rate != old_link_speed)) {
		// re-initialize hardware with new parameters
		aq_hw_set_link_speed(hw, hw->link_rate);
	}

	AQ_DBG_EXIT(0);
	return (0);
}

static void
aq_add_media_types(struct aq_dev *aq_dev, int media_link_speed)
{
	ifmedia_add(aq_dev->media, IFM_ETHER | media_link_speed | IFM_FDX, 0,
	    NULL);
	ifmedia_add(aq_dev->media, IFM_ETHER | media_link_speed | IFM_FDX |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE, 0, NULL);
	ifmedia_add(aq_dev->media, IFM_ETHER | media_link_speed | IFM_FDX |
	    IFM_ETH_RXPAUSE, 0, NULL);
	ifmedia_add(aq_dev->media, IFM_ETHER | media_link_speed | IFM_FDX |
	    IFM_ETH_TXPAUSE, 0, NULL);
}
void
aq_initmedia(struct aq_dev *aq_dev)
{
	u_int i;

	AQ_DBG_ENTER();

	// ifconfig eth0 none
	ifmedia_add(aq_dev->media, IFM_ETHER | IFM_NONE, 0, NULL);

	ifmedia_add(aq_dev->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	aq_add_media_types(aq_dev, IFM_AUTO);

	for (i = 0; i < nitems(aq_media_types); i++)
		if (aq_dev->link_speeds & aq_media_types[i].link_bit)
			aq_add_media_types(aq_dev, aq_media_types[i].ifm);

	// link is initially autoselect
	ifmedia_set(aq_dev->media,
	    IFM_ETHER | IFM_AUTO | IFM_FDX | IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE);

	AQ_DBG_EXIT(0);
}
