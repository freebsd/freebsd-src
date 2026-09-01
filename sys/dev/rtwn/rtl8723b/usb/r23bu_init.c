/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Andriy Voskoboinyk <avos@FreeBSD.org>
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
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

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/rtwn/if_rtwnreg.h>
#include <dev/rtwn/if_rtwnvar.h>

#include <dev/rtwn/usb/rtwn_usb_var.h>

#include <dev/rtwn/rtl8188e/r88e_reg.h>

#include <dev/rtwn/rtl8192c/r92c_reg.h>

#include <dev/rtwn/rtl8192e/r92e_var.h>

#include <dev/rtwn/rtl8723b/r23b_reg.h>
#include <dev/rtwn/rtl8723b/r23b_var.h>

#include <dev/rtwn/rtl8723b/usb/r23bu.h>

#include <dev/rtwn/rtl8812a/r12a_reg.h>

int
r23bu_power_on(struct rtwn_softc *sc)
{
#define RTWN_CHK(res) do {	\
	if (res != 0)		\
		return (EIO);	\
} while(0)
	int ntries;

	/* Clear suspend and power down bits.*/
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_HSUS | R92C_APS_FSMCO_APDM_HPDN, 0, 1));

	/* Disable GPIO9 as EXT WAKEUP. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_GPIO_INTM + 2, 0x01, 0));

	/* Enable WL suspend. */
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_HSUS | R92C_APS_FSMCO_AFSM_PCIE, 0, 1));

	/* Enable LDOA12 MACRO block for all interfaces. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_LDOA15_CTRL, 0, R92C_LDOA15_CTRL_EN));

	/* Disable BT_GPS_SEL pins. */
	RTWN_CHK(rtwn_setbits_1(sc, 0x067, 0x10, 0));

	/* 1 ms delay. */
	rtwn_delay(sc, 1000);

	/* Release analog Ips to digital isolation. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_SYS_ISO_CTRL,
	    R92C_SYS_ISO_CTRL_IP2MAC, 0));

	/* Disable SW LPS and WL suspend. */
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_APFM_RSM |
	    R92C_APS_FSMCO_AFSM_HSUS |
	    R92C_APS_FSMCO_AFSM_PCIE, 0, 1));

	/* Wait for power ready bit. */
	for (ntries = 0; ntries < 5000; ntries++) {
		if (rtwn_read_4(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_SUS_HOST)
			break;
		rtwn_delay(sc, 10);
	}
	if (ntries == 5000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for chip power up\n");
		return (ETIMEDOUT);
	}

	/* Release WLON reset. */
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO, 0,
	    R92C_APS_FSMCO_RDY_MACON, 2));

	/* Disable HWPDN. */
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_APDM_HPDN, 0, 1));

	/* Disable WL suspend. */
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_HSUS | R92C_APS_FSMCO_AFSM_PCIE, 0, 1));

	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_APS_FSMCO, 0,
	    R92C_APS_FSMCO_APFM_ONMAC, 1));
	for (ntries = 0; ntries < 5000; ntries++) {
		if (!(rtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC))
			break;
		rtwn_delay(sc, 10);
	}
	if (ntries == 5000)
		return (ETIMEDOUT);

	RTWN_CHK(rtwn_setbits_1(sc, R92C_AFE_MISC, 0, 0x40));

	/* Enable falling edge triggering interrupt. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_GPIO_INTM + 1, 0, 0x02));

	/* Enable GPIO9 interrupt mode. */
	RTWN_CHK(rtwn_setbits_1(sc, 0x063, 0, 0x02));

	/* Enable GPIO9 input mode. */
	RTWN_CHK(rtwn_setbits_1(sc, 0x062, 0x02, 0));

	/* Enable HSISR GPIO interrupt. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_HSIMR, 0, 0x01));

	/* Enable HSISR GPIO9 interrupt. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_HSIMR + 2, 0, 0x02));

	/* GPIO9 internal pull up. */
	RTWN_CHK(rtwn_setbits_1(sc, R92C_MULTI_FUNC_CTRL, 0, 0x08));
	RTWN_CHK(rtwn_setbits_1_shift(sc, R92C_MULTI_FUNC_CTRL, 0, 0x4000, 1));

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	RTWN_CHK(rtwn_write_1(sc, R92C_CR, 0x00));
	RTWN_CHK(rtwn_setbits_2(sc, R92C_CR, 0,
	    R92C_CR_HCI_TXDMA_EN | R92C_CR_TXDMA_EN |
	    R92C_CR_HCI_RXDMA_EN | R92C_CR_RXDMA_EN |
	    R92C_CR_PROTOCOL_EN | R92C_CR_SCHEDULE_EN |
	    ((sc->sc_hwcrypto != RTWN_CRYPTO_SW) ? R92C_CR_ENSEC : 0) |
	    R92C_CR_CALTMR_EN));

	/* btcoex power on. */

	RTWN_CHK(rtwn_write_1(sc, R88E_BB_PAD_CTRL + 3, 0x20));
	RTWN_CHK(rtwn_setbits_2(sc, R92C_SYS_FUNC_EN, 0,
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST));
	RTWN_CHK(rtwn_write_1(sc, R23B_GNT_BT + 1, 0x18));
	RTWN_CHK(rtwn_write_1(sc, 0x76e, 0x4));
	RTWN_CHK(rtwn_write_4(sc, R23B_ANT_SEL, R23B_ANT_SEL_S0));
	RTWN_CHK(rtwn_write_1(sc, 0xfe08, 0x1));

	RTWN_CHK(rtwn_setbits_2(sc, R92C_PWR_DATA, 0,
	    R23B_PWR_DATA_RFE_CTRL_EN));

	RTWN_CHK(rtwn_setbits_4(sc, R92C_LEDCFG0, 0, R23B_LEDCFG0_DPDT_SEL_EN));

	RTWN_CHK(rtwn_setbits_1(sc, R88E_BB_PAD_CTRL,
	    R23B_BB_PAD_CTRL_DPDT_SEL_DATA, 0));

	return (0);
#undef RTWN_CHK
}

void
r23bu_power_off(struct rtwn_softc *sc)
{
	int error, ntries;

	/* Stop Tx report timer. */
	error = rtwn_setbits_1(sc, R88E_TX_RPT_CTRL, R88E_TX_RPT2_ENA, 0);
	if (error == ENXIO) /* hardware gone */
		return;

	/* Stop Rx. */
	rtwn_write_4(sc, R92C_CR, 0x0000);

	/* Move card to Low Power state. */
	/* Block all Tx queues. */
	rtwn_write_1(sc, R92C_TXPAUSE, R92C_TX_QUEUE_ALL);

	for (ntries = 0; ntries < 10; ntries++) {
		/* Should be zero if no packet is transmitting. */
		if (rtwn_read_4(sc, R88E_SCH_TXCMD) == 0)
			break;

		rtwn_delay(sc, 5000);
	}
	if (ntries == 10) {
		device_printf(sc->sc_dev, "%s: failed to block Tx queues\n",
		    __func__);
		return;
	}

	/* CCK and OFDM are disabled, and clock are gated. */
	rtwn_setbits_1(sc, R92C_SYS_FUNC_EN, R92C_SYS_FUNC_EN_BBRSTB, 0);

	rtwn_delay(sc, 1);

	/* Reset whole BB. */
	rtwn_setbits_1(sc, R92C_SYS_FUNC_EN, R92C_SYS_FUNC_EN_BB_GLB_RST, 0);

	/* Reset MAC TRX. */
	rtwn_write_1(sc, R92C_CR, R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN);

	/* check if removed later. (?) */
	rtwn_setbits_1_shift(sc, R92C_CR, R92C_CR_ENSEC, 0, 1);

	/* Respond TxOK to scheduler */
	rtwn_setbits_1(sc, R92C_DUAL_TSF_RST, 0, R92C_DUAL_TSF_RST_TXOK);

#ifndef RTWN_WITHOUT_UCODE
	if ((sc->sc_flags & RTWN_FW_LOADED) &&
	    (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL))
		rtwn_fw_reset(sc, RTWN_FW_RESET_SHUTDOWN);
#endif

	/* Reset MCU. */
	rtwn_setbits_1_shift(sc, R92C_SYS_FUNC_EN, R92C_SYS_FUNC_EN_CPUEN,
	    0, 1);
	rtwn_write_1(sc, R92C_MCUFWDL, 0);

	/* Move card to Disabled state. */
	/* Turn off RF. */
	rtwn_write_1(sc, R92C_RF_CTRL, 0);

	/* Enable rising edge triggering interrupt. */
	rtwn_setbits_1(sc, R92C_GPIO_INTM + 1, 0x02, 0);

	/* Release WLON reset. */
	rtwn_setbits_1_shift(sc, R92C_APS_FSMCO, 0,
	    R92C_APS_FSMCO_RDY_MACON, 2);

	/* Turn off MAC by HW state machine */
	rtwn_setbits_1_shift(sc, R92C_APS_FSMCO, 0, R92C_APS_FSMCO_APFM_OFF, 1);
	for (ntries = 0; ntries < 10; ntries++) {
		/* Wait until it will be disabled. */
		if ((rtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_OFF) == 0)
			break;

		rtwn_delay(sc, 5000);
	}
	if (ntries == 10) {
		device_printf(sc->sc_dev, "%s: could not turn off MAC\n",
		    __func__);
		return;
	}

	rtwn_setbits_1(sc, R92C_AFE_MISC, 0x40, 0);

	/* Analog Ips to digital isolation. */
	rtwn_setbits_1(sc, R92C_SYS_ISO_CTRL, 0, R92C_SYS_ISO_CTRL_IP2MAC);

	/* Disable LDOA12 MACRO block. */
	rtwn_setbits_1(sc, R92C_LDOA15_CTRL, R92C_LDOA15_CTRL_EN, 0);

	/* Enable WL suspend. */
	rtwn_setbits_1_shift(sc, R92C_APS_FSMCO, R92C_APS_FSMCO_AFSM_PCIE,
	    R92C_APS_FSMCO_AFSM_HSUS, 1);

	/* Enable GPIO9 as EXT WAKEUP. */
	rtwn_setbits_1(sc, R92C_GPIO_INTM + 2, 0, 0x01);
}

void
r23bu_init_bb(struct rtwn_softc *sc)
{
	/* Enable BB and RF. */
	rtwn_setbits_2(sc, R92C_SYS_FUNC_EN, 0, R92C_SYS_FUNC_EN_BBRSTB |
	    R92C_SYS_FUNC_EN_BB_GLB_RST | R92C_SYS_FUNC_EN_DIO_RF);

	/* Select antenna S0. */
	rtwn_write_4(sc, R23B_ANT_SEL, R23B_ANT_SEL_S0);

	return (r23b_init_bb_common(sc));
}

void
r23bu_init_rx_agg(struct rtwn_softc *sc)
{
	struct r23b_softc *rs = sc->sc_priv;
	uint32_t val;

	rtwn_setbits_1(sc, R92C_TRXDMA_CTRL, 0, R92C_TRXDMA_CTRL_RXDMA_AGG_EN);

	val = rtwn_read_4(sc, R92C_RXDMA_AGG_PG_TH);
	val = RW(val, R23B_RXDMA_AGG_PG_TH_SIZE, rs->super.ac_usb_dma_size);
	val = RW(val, R23B_RXDMA_AGG_PG_TH_TIME, rs->super.ac_usb_dma_time);
	val |= R23B_RXDMA_AGG_PG_TH_EN;
	rtwn_write_4(sc, R92C_RXDMA_AGG_PG_TH, val);
}

void
r23bu_init_ampdu(struct rtwn_softc *sc)
{
	struct rtwn_usb_softc *uc = RTWN_USB_SOFTC(sc);
	uint8_t val;

	val = rtwn_read_1(sc, R12A_RXDMA_PRO);
	val = RW(val, R12A_BURST_SZ,
	    usbd_get_speed(uc->uc_udev) == USB_SPEED_HIGH ?
	    R12A_BURST_SZ_USB2 : R12A_BURST_SZ_USB1);
	val |= R12A_DMA_MODE | SM(R12A_BURST_CNT, 3);
	rtwn_write_1(sc, R12A_RXDMA_PRO, val);

	/* Enable single packet AMPDU. */
	rtwn_setbits_1(sc, R12A_HT_SINGLE_AMPDU, 0,
	    R12A_HT_SINGLE_AMPDU_PKT_ENA);

	rtwn_write_2(sc, R92C_MAX_AGGR_NUM, 0x0c14);
	rtwn_write_1(sc, R12A_AMPDU_MAX_TIME, 0x5e);
	rtwn_write_4(sc, R12A_AMPDU_MAX_LENGTH, UINT32_MAX);

	/*
	 * The vendor driver says this is for
	 * "VHT packet length 11K", but this chip
	 * doesn't support VHT rates...
	 */
	rtwn_write_1(sc, R92C_RX_PKT_LIMIT, 0x18);

	rtwn_write_1(sc, R92C_PIFS, 0);
	rtwn_write_1(sc, R92C_FWHW_TXQ_CTRL, R92C_FWHW_TXQ_CTRL_AMPDU_RTY_NEW);
	rtwn_write_4(sc, R92C_FAST_EDCA_CTRL, 0x03086666);

	rtwn_write_1(sc, R92C_USTIME_TSF, 0x50);
	rtwn_write_1(sc, R92C_USTIME_EDCA, 0x50);

	/* Do not reset MAC. */
	rtwn_setbits_1(sc, R92C_RSV_CTRL, 0, 0x60);
}
