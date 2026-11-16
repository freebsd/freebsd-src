/*	$OpenBSD: if_urtwn.c,v 1.16 2011/02/10 17:26:40 jakemsr Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2014 Kevin Lo <kevlo@FreeBSD.org>
 * Copyright (c) 2015-2016 Andriy Voskoboinyk <avos@FreeBSD.org>
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
#include "opt_wlan.h"

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

#include <dev/rtwn/if_rtwn_debug.h>

#include <dev/rtwn/rtl8188e/r88e.h>
#include <dev/rtwn/rtl8188e/r88e_priv.h>
#include <dev/rtwn/rtl8188e/r88e_rom_image.h>

#include <dev/rtwn/rtl8192c/r92c_var.h>
#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8188f/r88f.h>
#include <dev/rtwn/rtl8188f/r88f_priv.h>
#include <dev/rtwn/rtl8188f/r88f_rom_image.h>

#define R88F_XTAL1_MASK		0x007e0000	/* bits 22:17 */
#define R88F_XTAL0_MASK		0x0001f800	/* bits 16:11 */

int
r88f_efuse_preread(struct rtwn_softc *sc)
{
	int error;
	uint32_t reg;

	/* Switch to the WIFI bank. */
	reg = rtwn_read_4(sc, R92C_EFUSE_TEST);
	reg = RW(reg, R92C_EFUSE_TEST_SEL, 0);
	error = rtwn_write_4(sc, R92C_EFUSE_TEST, reg);

	if (error != 0)
		return (error);

	error = rtwn_write_1(sc, R92C_EFUSE_ACCESS, R92C_EFUSE_ACCESS_OFF);
	rtwn_delay(sc, 10);

	return (error);
}

void
r88f_parse_rom(struct rtwn_softc *sc, uint8_t *buf)
{
	struct r92c_softc *rs = sc->sc_priv;
	struct rtwn_r88e_txpwr *rt = rs->rs_txpwr;
	struct r88f_rom *rom = (struct r88f_rom *)buf;
	int i;

	rt->bw20_tx_pwr_diff = RTWN_SIGN4TO8(MS(rom->tx_pwr_diff, HIGH_PART));
	rt->ofdm_tx_pwr_diff = RTWN_SIGN4TO8(MS(rom->tx_pwr_diff, LOW_PART));

	for (i = 0; i < nitems(rom->ht40_tx_pwr); i++) {
		rt->ht40_tx_pwr[i] = rom->ht40_tx_pwr[i];
		if (rt->ht40_tx_pwr[i] > 0x3f)
			rt->ht40_tx_pwr[i] = 0x27;
	}
	for (i = 0; i < nitems(rom->cck_tx_pwr); i++) {
		rt->cck_tx_pwr[i] = rom->cck_tx_pwr[i];
		if (rt->cck_tx_pwr[i] > 0x3f)
			rt->cck_tx_pwr[i] = 0x22;
	}

	rs->crystalcap = rom->crystalcap & 0x3f;

	rs->regulatory = MS(rom->rf_board_opt, R92C_ROM_RF1_REGULATORY);

	rs->board_type =
	    MS(RTWN_GET_ROM_VAR(rom->rf_board_opt, R92C_BOARD_TYPE_DONGLE),
		R92C_ROM_RF1_BOARD_TYPE);

	RTWN_DPRINTF(sc, RTWN_DEBUG_ROM, "%s: regulatory type %d\n",
	    __func__, rs->regulatory);

	sc->thermal_meter = rom->thermal_meter;

	rtwn_r92c_set_rom_opts(sc, buf);
}
