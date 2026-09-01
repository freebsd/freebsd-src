/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/rtwn/if_rtwnreg.h>
#include <dev/rtwn/if_rtwnvar.h>

#include <dev/rtwn/rtl8188e/r88e_rx_desc.h>

#include <dev/rtwn/rtl8723b/r23b.h>

int8_t
r23b_get_rssi_cck(struct rtwn_softc *sc, void *physt)
{
	struct r88e_rx_phystat *phy = (struct r88e_rx_phystat *)physt;
	int8_t lna_idx, vga_idx, rssi;

	lna_idx = (phy->agc_rpt & 0xe0) >> 5;
	vga_idx = (phy->agc_rpt & 0x1f);

	rssi = -2 * vga_idx;
	switch (lna_idx) {
	case 6:
		rssi -= 34;
		break;
	case 4:
		rssi -= 14;
		break;
	case 1:
		rssi += 6;
		break;
	case 0:
		rssi += 16;
		break;
	default:
		rssi = 0;
		break;
	}

	return (rssi);
}
