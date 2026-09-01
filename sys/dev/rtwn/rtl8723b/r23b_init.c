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

#include <dev/rtwn/rtl8188e/r88e.h>
#include <dev/rtwn/rtl8188e/r88e_reg.h>

#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8192e/r92e.h>
#include <dev/rtwn/rtl8192e/r92e_var.h>

#include <dev/rtwn/rtl8723b/r23b.h>
#include <dev/rtwn/rtl8723b/r23b_reg.h>
#include <dev/rtwn/rtl8723b/r23b_var.h>
#include <dev/rtwn/rtl8723b/r23b_fw_cmd.h>

#include <dev/rtwn/rtl8821a/r21a_reg.h>

int
r23b_set_page_size(struct rtwn_softc *sc)
{
	return (rtwn_write_1(sc, R92C_PBP, SM(R92C_PBP_PSRX, R92C_PBP_256) |
	    SM(R92C_PBP_PSTX, R92C_PBP_256)));
}

void
r23b_init_antsel(struct rtwn_softc *sc)
{
	rtwn_setbits_4(sc, R88E_BB_PAD_CTRL, 0x1100000, 0);
	rtwn_setbits_4(sc, R92C_GPIO_MUXCFG, 0x10, 0x8);
	rtwn_setbits_4(sc, R92C_LEDCFG0, 0x400000, 0x800000);
	rtwn_setbits_4(sc, 0x944, 0, 0x3);
	rtwn_setbits_4(sc, 0x930, 0xff, 0x77);
	rtwn_setbits_4(sc, R92C_PWR_DATA, 0, 0x800);
}

void
r23b_init_bb_common(struct rtwn_softc *sc)
{
	struct r23b_softc *rs = sc->sc_priv;
	uint8_t val;

	/* PathA RF Power On. */
	rtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);

	rtwn_delay(sc, 10);

	rtwn_rf_write(sc, 0, R92C_RF_IQADJ_G(0), 0x780);

	rtwn_write_1(sc, R92C_SYS_FUNC_EN, R92C_SYS_FUNC_EN_BBRSTB |
	    R92C_SYS_FUNC_EN_BB_GLB_RST | R92C_SYS_FUNC_EN_DIO_PCIE |
	    R92C_SYS_FUNC_EN_PCIEA | R92C_SYS_FUNC_EN_PPLL);

	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);

	for (int i = 0; i < sc->bb_size; i++) {
		const struct rtwn_bb_prog *bb_prog = &sc->bb_prog[i];

		for (int j = 0; j < bb_prog->count; j++) {
			rtwn_write_4(sc, bb_prog->reg[j], bb_prog->val[j]);
			rtwn_delay(sc, 1);
		}
	}

	for (int i = 0; i < sc->agc_size; i++) {
		const struct rtwn_agc_prog *agc_prog = &sc->agc_prog[i];

		for (int j = 0; j < agc_prog->count; j++) {
			rtwn_write_4(sc, R92C_OFDM0_AGCRSSITABLE,
			    agc_prog->val[j]);
			rtwn_delay(sc, 1);
		}
	}

	rtwn_write_4(sc, R92C_OFDM0_AGCCORE1(0), 0x69553422);
	rtwn_delay(sc, 1);
	rtwn_write_4(sc, R92C_OFDM0_AGCCORE1(0), 0x69553420);
	rtwn_delay(sc, 1);
	rtwn_write_4(sc, R92C_HSSI_PARAM2(0), 0x00390204);
	rtwn_delay(sc, 1);

	val = rs->super.crystalcap & 0x3f;
	rtwn_setbits_4(sc, R92C_MAC_PHY_CTRL, R21A_MAC_PHY_CRYSTALCAP_M,
	    SM(R21A_MAC_PHY_CRYSTALCAP, val | (val << 6)));
}

void
r23b_init_rf(struct rtwn_softc *sc)
{
	r92e_init_rf(sc);

	/* Init LCK. */
	rtwn_rf_write(sc, 0, R23B_RF_S0S1, 0xdfbe0);
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW, 0x8c01);
	rtwn_delay(sc, 200000);
	rtwn_rf_write(sc, 0, R23B_RF_S0S1, 0xdffe0);
}

#ifndef RTWN_WITHOUT_UCODE
static void
r23b_write_btreg(struct rtwn_softc *sc, uint8_t addr, uint8_t data)
{
	struct r23b_fw_cmd_btoper cmd;

	cmd.req_num = 0;
	cmd.opcode = R23B_BT_MP_OPER_WRITE_VAL;
	cmd.value = data;
	cmd.addr = 0;
	r88e_fw_cmd(sc, R23B_CMD_BT_MP_OPER, &cmd, sizeof(cmd));

	rtwn_delay(sc, 200000); /* 200ms */

	cmd.req_num = 1;
	cmd.opcode = R23B_BT_MP_OPER_WRITE_ADDR;
	cmd.value = 0;
	cmd.addr = addr;
	r88e_fw_cmd(sc, R23B_CMD_BT_MP_OPER, &cmd, sizeof(cmd));
}
#endif

void
r23b_post_init(struct rtwn_softc *sc)
{
#ifndef RTWN_WITHOUT_UCODE
	struct r23b_fw_cmd_antinv cmd;
	const uint8_t ps_tdma[5] = { 0x8, 0x0, 0x0, 0x0, 0x0 };
#endif

	if (sc->macid_rpt2_max_num > 0) {
		rtwn_setbits_1(sc, R88E_TX_RPT_CTRL, 0, R88E_TX_RPT2_ENA);
		rtwn_write_1(sc, R88E_TX_RPT_MACID_MAX, sc->macid_rpt2_max_num);
		/* Enable periodic TX report; 32uS units */
		rtwn_write_2(sc, R88E_TX_RPT_TIME, 0xcdf0);
	}

	rtwn_lc_calib(sc);
	rtwn_iq_calib(sc);

	/* Enable RF */

	/* XXX lots of magic numbers here. */

	rtwn_write_1(sc, 0x790, 0x5);
	rtwn_write_1(sc, 0x778, 0x1);
	rtwn_setbits_1(sc, R92C_GPIO_MUXCFG, 0, R92C_GPIO_MUXCFG_ENBT);

	/* WiFi TRx Mask on */
	rtwn_rf_write(sc, 0, R92C_RF_IQADJ_G(0), 0x780);

#ifndef RTWN_WITHOUT_UCODE
	/* BT TRx Mask on */
	r23b_write_btreg(sc, 0x3c, 0x15);

	r88e_fw_cmd(sc, R23B_CMD_BT_GRANT, R23B_CMD_BT_GRANT_LOW,
	    sizeof(R23B_CMD_BT_GRANT_LOW));
#endif

	rtwn_write_1(sc, 0x76e, 0x4);
	rtwn_setbits_1(sc, R88E_BB_PAD_CTRL + 3, 0, 0x20);
	rtwn_setbits_1(sc, R92C_PWR_DATA + 1, 0, 0x8);
	rtwn_write_1(sc, 0x974, 0xff);
	rtwn_setbits_1(sc, 0x944, 0, 0x3);
	rtwn_write_1(sc, 0x930, 0x77);

	rtwn_setbits_4(sc, R92C_LEDCFG0, 0x1000000, 0x800000);
	rtwn_setbits_1(sc, R88E_BB_PAD_CTRL, 0x1, 0);

#ifndef RTWN_WITHOUT_UCODE
	/* Inform firmware of the antenna inversion. */
	cmd.internal_switch = 0;
	cmd.invert = 1;

	r88e_fw_cmd(sc, R23B_CMD_ANT_INV, &cmd, sizeof(cmd));
#endif

	rtwn_write_4(sc, R23B_ANT_SEL, 0x80);

#ifndef RTWN_WITHOUT_UCODE
	r88e_fw_cmd(sc, R23B_CMD_PS_TDMA, ps_tdma, sizeof(ps_tdma));
#endif

	/* btcoex table */
	rtwn_write_4(sc, R23B_BTCOEX_TABLE(0), 0x55555555);
	rtwn_write_4(sc, R23B_BTCOEX_TABLE(1), 0x55555555);
	rtwn_write_4(sc, R23B_BTCOEX_TABLE(2), 0x00ffffff);
	rtwn_write_4(sc, R23B_BTCOEX_TABLE(3), 0x00000003);

#ifndef RTWN_WITHOUT_UCODE
	r88e_fw_cmd(sc, R23B_CMD_BT_INFO, &(uint8_t){ 1 }, sizeof(uint8_t));
	r88e_fw_cmd(sc, R23B_CMD_BT_WLANACT, R23B_CMD_BT_WLANACT_IGNORE,
	    sizeof(R23B_CMD_BT_WLANACT_IGNORE));
#endif

	/* ACK for xmit mgmt frames. */
	rtwn_setbits_4(sc, R92C_FWHW_TXQ_CTRL, 0, R23B_FWHW_TXQ_CTRL_ACK_MGNT);

#ifndef RTWN_WITHOUT_UCODE
	if (sc->sc_flags & RTWN_FW_LOADED) {
		if (sc->sc_ratectl_sysctl == RTWN_RATECTL_FW) {
			/* TODO: implement */
			sc->sc_ratectl = RTWN_RATECTL_NET80211;
		} else
			sc->sc_ratectl = sc->sc_ratectl_sysctl;
	} else
#endif
		sc->sc_ratectl = RTWN_RATECTL_NONE;
}
