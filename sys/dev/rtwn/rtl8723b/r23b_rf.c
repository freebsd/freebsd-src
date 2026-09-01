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

#include <dev/rtwn/rtl8188e/r88e_reg.h>

#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8723b/r23b.h>

void
r23b_rf_write(struct rtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{
	uint32_t reg;

	rtwn_write_4(sc, R92C_LSSI_PARAM(chain),
	    SM(R88E_LSSI_PARAM_ADDR, addr) | SM(R92C_LSSI_PARAM_DATA, val));

	/*
	 * The vendor driver claims that writing to 0xb6 may fail under high
	 * temperature conditions, but only attempts to retry during RF init.
	 *
	 * It is not mentioned why 0xb2 is also special.
	 */
	switch (addr) {
	case 0xb6:
		rtwn_delay(sc, 1);

		reg = rtwn_rf_read(sc, chain, addr);
		rtwn_delay(sc, 1);

		for (int retry = 0; (reg & ~0xff) != (val & ~0xff) &&
		    retry < 6; retry++) {
			rtwn_write_4(sc, R92C_LSSI_PARAM(chain),
			    SM(R88E_LSSI_PARAM_ADDR, addr) |
			    SM(R92C_LSSI_PARAM_DATA, val));
			rtwn_delay(sc, 1);

			reg = rtwn_rf_read(sc, chain, addr);
		}
		break;
	case 0xb2:
		rtwn_delay(sc, 1);

		reg = rtwn_rf_read(sc, chain, addr);
		rtwn_delay(sc, 1);

		for (int retry = 0; reg != val && retry < 6; retry++) {
			rtwn_write_4(sc, R92C_LSSI_PARAM(chain),
			    SM(R88E_LSSI_PARAM_ADDR, addr) |
			    SM(R92C_LSSI_PARAM_DATA, val));
			rtwn_delay(sc, 1);

			rtwn_rf_write(sc, chain, R92C_RF_CHNLBW, 0x0fc07);
			rtwn_delay(sc, 1);

			reg = rtwn_rf_read(sc, chain, addr);
		}
		break;
	}
}
