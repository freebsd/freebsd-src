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

#include <dev/rtwn/rtl8192c/r92c.h>
#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8192e/r92e.h>

#include <dev/rtwn/rtl8723b/r23b.h>

#ifndef RTWN_WITHOUT_UCODE
void
r23b_fw_reset(struct rtwn_softc *sc, int reason)
{
	switch (reason) {
	case RTWN_FW_RESET_DOWNLOAD:
	case RTWN_FW_RESET_SHUTDOWN:
		return (r92c_fw_reset(sc, reason));
	case RTWN_FW_RESET_CHECKSUM:
		return (r92e_fw_reset(sc, reason));
	}
}

void
r23b_fw_download_enable(struct rtwn_softc *sc, int enable)
{
	if (!enable) {
		/* MCU download disable. */
		rtwn_setbits_1(sc, R92C_MCUFWDL, R92C_MCUFWDL_EN, 0);
		return;
	}

	/* 8051 enable. */
	rtwn_setbits_1_shift(sc, R92C_SYS_FUNC_EN, 0,
	    R92C_SYS_FUNC_EN_CPUEN, 1);
	/* MCU firmware download enable. */
	rtwn_setbits_1(sc, R92C_MCUFWDL, 0, R92C_MCUFWDL_EN);

	for (int ntries = 0; ntries <= 100; ntries++) {
		uint8_t val;

		val = rtwn_read_1(sc, R92C_MCUFWDL);
		if (val & R92C_MCUFWDL_EN)
			break;

		rtwn_write_1(sc, R92C_MCUFWDL, val | R92C_MCUFWDL_EN);
		rtwn_delay(sc, 1000);
	}

	/* 8051 reset. */
	rtwn_setbits_1_shift(sc, R92C_MCUFWDL, R92C_MCUFWDL_ROM_DLEN,
	    0, 2);
}
#endif
