/*
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
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

#include <dev/rtwn/rtl8192c/r92c.h>
#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8192e/r92e.h>
#include <dev/rtwn/rtl8192e/r92e_reg.h>

#include <dev/rtwn/rtl8188f/r88f.h>

#define R88E_RSV_CTRL_MIO_EN		0x0100
#define R88E_RSV_CTRL_MCU_RST		0x0800

#ifndef RTWN_WITHOUT_UCODE
void
r88f_fw_reset(struct rtwn_softc *sc, int reason)
{
	switch (reason) {
	case RTWN_FW_RESET_CHECKSUM:
		rtwn_setbits_1(sc, R92C_MCUFWDL, 0, R92C_MCUFWDL_CHKSUM_RPT);
		/* FALLTHROUGH */
	case RTWN_FW_RESET_DOWNLOAD:
	case RTWN_FW_RESET_SHUTDOWN:
		/* Reset MCU IO wrapper. */
		rtwn_setbits_2(sc, R92C_RSV_CTRL, R88E_RSV_CTRL_MIO_EN, 0);
		rtwn_setbits_2(sc, R92C_SYS_FUNC_EN, R92C_SYS_FUNC_EN_CPUEN, 0);

		/* Enable MCU IO wrapper. */
		rtwn_setbits_2(sc, R92C_RSV_CTRL, 0, R88E_RSV_CTRL_MIO_EN);
		rtwn_setbits_2(sc, R92C_SYS_FUNC_EN, 0, R92C_SYS_FUNC_EN_CPUEN);
		return;
	}
}

void
r88f_fw_download_enable(struct rtwn_softc *sc, int enable)
{
	if (enable) {
		/* Enable FW download. */
		rtwn_setbits_1(sc, R92C_MCUFWDL, 0, R92C_MCUFWDL_EN);
		rtwn_setbits_4(sc, R92C_MCUFWDL, R92C_MCUFWDL_ROM_DLEN, 0);
		/* Wait 100ms */
		rtwn_delay(sc, 100);
	} else {
		rtwn_setbits_1(sc, R92C_MCUFWDL, R92C_MCUFWDL_EN, 0);
	}
}
#endif
