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

#include <dev/rtwn/rtl8723b/r23b.h>
#include <dev/rtwn/rtl8723b/r23b_rom_image.h>

#include <dev/rtwn/rtl8723b/usb/r23bu.h>

void
r23bu_parse_rom(struct rtwn_softc *sc, uint8_t *buf)
{
	struct r23b_rom *rom = (struct r23b_rom *)buf;

	r23b_parse_rom(sc, buf);
	IEEE80211_ADDR_COPY(sc->sc_ic.ic_macaddr, rom->usb.macaddr);
}
