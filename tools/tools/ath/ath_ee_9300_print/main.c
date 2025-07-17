
/*
 * Copyright (c) 2010-2011 Adrian Chadd, Xenion Pty Ltd.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <err.h>

struct ath_hal;

#include "ar9300/ar9300eep.h"

static const char *
eeprom_9300_ctl_idx_to_mode(uint8_t idx)
{
	switch (idx & 0xf) {
	/* 2G CTLs */
	case 1: return "CCK";
	case 2: return "OFDM";
	case 5: return "HT20";
	case 7: return "HT40";
	/* 5G CTLs */
	case 0: return "OFDM";
	case 6: return "HT20";
	case 8: return "HT40";
	default: return "";
	}
}

static const char *
eeprom_9300_ctl_idx_to_regdomain(uint8_t idx)
{
	switch (idx & 0xf0) {
	case 0x10: return "FCC";
	case 0x30: return "ETSI";
	case 0x40: return "JP";
	default: return "";
	}
}

static void
eeprom_9300_hdr_print(const uint16_t *buf)
{
	const ar9300_eeprom_t *ee = (ar9300_eeprom_t *) buf;

	printf("| Version: %d, Template: %d, MAC: %02x:%02x:%02x:%02x:%02x:%02x |\n",
	    ee->eeprom_version,
	    ee->template_version,
	    ee->mac_addr[0],
	    ee->mac_addr[1],
	    ee->mac_addr[2],
	    ee->mac_addr[3],
	    ee->mac_addr[4],
	    ee->mac_addr[5]);
}

static void
eeprom_9300_base_print(const uint16_t *buf)
{
	const ar9300_eeprom_t *ee = (ar9300_eeprom_t *) buf;
	const OSPREY_BASE_EEP_HEADER *ee_base = &ee->base_eep_header;

	printf("| RegDomain: 0x%02x 0x%02x TxRxMask: 0x%02x OpFlags: 0x%02x OpMisc: 0x%02x |\n",
	    ee_base->reg_dmn[0],
	    ee_base->reg_dmn[1],
	    ee_base->txrx_mask,
	    ee_base->op_cap_flags.op_flags,
	    ee_base->op_cap_flags.eepMisc);

	printf("| RfSilent: 0x%02x BtOptions: 0x%02x DeviceCap: 0x%02x DeviceType: 0x%02x |\n",
	    ee_base->rf_silent,
	    ee_base->blue_tooth_options,
	    ee_base->device_cap,
	    ee_base->device_type);

	printf("| pwrTableOffset: %d dB, TuningCaps=0x%02x 0x%02x feature_enable: 0x%02x MiscConfig: 0x%02x |\n",
	    ee_base->pwrTableOffset,
	    ee_base->params_for_tuning_caps[0],
	    ee_base->params_for_tuning_caps[1],
	    ee_base->feature_enable,
	    ee_base->misc_configuration);

	printf("| EepromWriteGpio: %d, WlanDisableGpio: %d, WlanLedGpio: %d RxBandSelectGpio: %d |\n",
	    ee_base->eeprom_write_enable_gpio,
	    ee_base->wlan_disable_gpio,
	    ee_base->wlan_led_gpio,
	    ee_base->rx_band_select_gpio);

	printf("| TxRxGain: %d, SwReg: %d |\n",
	    ee_base->txrxgain,
	    ee_base->swreg);
}

static void
eeprom_9300_modal_print(const OSPREY_MODAL_EEP_HEADER *m)
{
	int i;

	printf("| AntCtrl: 0x%08x AntCtrl2: 0x%08x |\n",
	    m->ant_ctrl_common,
	    m->ant_ctrl_common2);

	for (i = 0; i < OSPREY_MAX_CHAINS; i++) {
		printf("| Ch %d: AntCtrl: 0x%08x Atten1: %d, atten1_margin: %d, NfThresh: %d |\n",
		    i,
		    m->ant_ctrl_chain[i],
		    m->xatten1_db[i],
		    m->xatten1_margin[i],
		    m->noise_floor_thresh_ch[i]);
	}

	printf("| Spur: ");
	for (i = 0; i < OSPREY_EEPROM_MODAL_SPURS; i++) {
		printf("(%d: %d) ", i, m->spur_chans[i]);
	}
	printf("|\n");

	printf("| TempSlope: %d, VoltSlope: %d, QuickDrop: %d, XpaBiasLvl %d |\n",
	    m->temp_slope,
	    m->voltSlope,
	    m->quick_drop,
	    m->xpa_bias_lvl);

	printf("| txFrameToDataStart: %d, TxFrameToPaOn: %d, TxEndToXpaOff: %d, TxEndToRxOn: %d, TxFrameToXpaOn: %d |\n",
	    m->tx_frame_to_data_start,
	    m->tx_frame_to_pa_on,
	    m->tx_end_to_xpa_off,
	    m->txEndToRxOn,
	    m->tx_frame_to_xpa_on);

	printf("| txClip: %d, AntGain: %d, SwitchSettling: %d, adcDesiredSize: %d |\n",
	    m->txClip,
	    m->antenna_gain,
	    m->switchSettling,
	    m->adcDesiredSize);

	printf("| Thresh62: %d, PaprdMaskHt20: 0x%08x, PaPrdMaskHt40: 0x%08x |\n",
	    m->thresh62,
	    m->paprd_rate_mask_ht20,
	    m->paprd_rate_mask_ht40);

	printf("| SwitchComSpdt: %02x, XlnaBiasStrength: %d, RfGainCap: %d, TxGainCap: %x\n",
	    m->switchcomspdt,
	    m->xLNA_bias_strength,
	    m->rf_gain_cap,
	    m->tx_gain_cap);

#if 0
    u_int8_t   reserved[MAX_MODAL_RESERVED];
    u_int16_t  switchcomspdt;
    u_int8_t   xLNA_bias_strength;                      // bit: 0,1:chain0, 2,3:chain1, 4,5:chain2
    u_int8_t   rf_gain_cap;
    u_int8_t   tx_gain_cap;                             // bit0:4 txgain cap, txgain index for max_txgain + 20 (10dBm higher than max txgain)
    u_int8_t   futureModal[MAX_MODAL_FUTURE];
    // last 12 bytes stolen and moved to newly created base extension structure
#endif
}

static void
eeprom_9300_print_2g_target_cck(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_2G_CCK_TARGET_POWERS; i++) {
		printf("| Freq %u CCK: pow2x 1/5L %u 5S %u 11L %u 11S %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_cck[i], 1),
		    ee->cal_target_power_cck[i].t_pow2x[LEGACY_TARGET_RATE_1L_5L],
		    ee->cal_target_power_cck[i].t_pow2x[LEGACY_TARGET_RATE_5S],
		    ee->cal_target_power_cck[i].t_pow2x[LEGACY_TARGET_RATE_11L],
		    ee->cal_target_power_cck[i].t_pow2x[LEGACY_TARGET_RATE_11S]);
	}
}

static void
eeprom_9300_print_2g_target_ofdm(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_2G_20_TARGET_POWERS; i++) {
		printf("| Freq %u OFDM: pow2x 6/12/18/24M %u 36M %u 48M %u 54M %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g[i], 1),
		    ee->cal_target_power_2g[i].t_pow2x[LEGACY_TARGET_RATE_6_24],
		    ee->cal_target_power_2g[i].t_pow2x[LEGACY_TARGET_RATE_36],
		    ee->cal_target_power_2g[i].t_pow2x[LEGACY_TARGET_RATE_48],
		    ee->cal_target_power_2g[i].t_pow2x[LEGACY_TARGET_RATE_54]);
	}
}

static void
eeprom_9300_print_2g_target_ht20(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_2G_20_TARGET_POWERS; i++) {
		printf("| Freq %u HT20  MCS0-7 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g_ht20[i], 1),
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_4],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_5],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_6],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_7]);
		printf("| Freq %u HT20  MCS8-15 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g_ht20[i], 1),
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_12],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_13],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_14],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_15]);
		printf("| Freq %u HT20  MCS16-23 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g_ht20[i], 1),
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_20],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_21],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_22],
		    ee->cal_target_power_2g_ht20[i].t_pow2x[HT_TARGET_RATE_23]);
	}
}

static void
eeprom_9300_print_2g_target_ht40(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_2G_40_TARGET_POWERS; i++) {
		printf("| Freq %u HT40  MCS0-7 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g_ht40[i], 1),
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_4],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_5],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_6],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_7]);
		printf("| Freq %u HT40  MCS8-15 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g_ht40[i], 1),
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_12],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_13],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_14],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_15]);
		printf("| Freq %u HT40  MCS16-23 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_2g_ht40[i], 1),
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_20],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_21],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_22],
		    ee->cal_target_power_2g_ht40[i].t_pow2x[HT_TARGET_RATE_23]);
	}
}

static void
eeprom_9300_print_2g_ctls(const ar9300_eeprom_t *ee)
{
	int i, j;

	for (i = 0; i < OSPREY_NUM_CTLS_2G; i++) {
		printf("| CTL index 0x%.02x (%s %s)\n",
		    ee->ctl_index_2g[i],
		    eeprom_9300_ctl_idx_to_regdomain(ee->ctl_index_2g[i]),
		    eeprom_9300_ctl_idx_to_mode(ee->ctl_index_2g[i]));
		for (j = 0; j < OSPREY_NUM_BAND_EDGES_2G; j++) {
		  printf("|   Freq %u pow2x %u flags 0x%x\n",
		      FBIN2FREQ(ee->ctl_freqbin_2G[i][j], 1),
		      ee->ctl_power_data_2g[i].ctl_edges[j].t_power,
		      ee->ctl_power_data_2g[i].ctl_edges[j].flag);
		}
		printf("\n");
	}
}

static void
eeprom_9300_print_5g_target_ofdm(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_5G_20_TARGET_POWERS; i++) {
		printf("| Freq %u OFDM: pow2x 6/12/18/24M %u 36M %u 48M %u 54M %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g[i], 0),
		    ee->cal_target_power_5g[i].t_pow2x[LEGACY_TARGET_RATE_6_24],
		    ee->cal_target_power_5g[i].t_pow2x[LEGACY_TARGET_RATE_36],
		    ee->cal_target_power_5g[i].t_pow2x[LEGACY_TARGET_RATE_48],
		    ee->cal_target_power_5g[i].t_pow2x[LEGACY_TARGET_RATE_54]);
	}
}

static void
eeprom_9300_print_5g_target_ht20(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_5G_20_TARGET_POWERS; i++) {
		printf("| Freq %u HT20  MCS0-7 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g_ht20[i], 0),
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_4],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_5],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_6],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_7]);
		printf("| Freq %u HT20  MCS8-15 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g_ht20[i], 0),
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_12],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_13],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_14],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_15]);
		printf("| Freq %u HT20  MCS16-23 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g_ht20[i], 0),
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_20],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_21],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_22],
		    ee->cal_target_power_5g_ht20[i].t_pow2x[HT_TARGET_RATE_23]);
	}
}

static void
eeprom_9300_print_5g_target_ht40(const ar9300_eeprom_t *ee)
{
	int i;

	for (i = 0; i < OSPREY_NUM_5G_40_TARGET_POWERS; i++) {
		printf("| Freq %u HT40  MCS0-7 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g_ht40[i], 0),
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_4],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_5],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_6],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_7]);
		printf("| Freq %u HT40  MCS8-15 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g_ht40[i], 0),
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_12],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_13],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_14],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_15]);
		printf("| Freq %u HT40  MCS16-23 pow2x %u %u %u %u %u %u %u %u\n",
		    FBIN2FREQ(ee->cal_target_freqbin_5g_ht40[i], 0),
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_0_8_16],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_1_3_9_11_17_19],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_20],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_21],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_22],
		    ee->cal_target_power_5g_ht40[i].t_pow2x[HT_TARGET_RATE_23]);
	}
}

static void
eeprom_9300_print_5g_ctls(const ar9300_eeprom_t *ee)
{
	int i, j;

	for (i = 0; i < OSPREY_NUM_CTLS_5G; i++) {
		printf("| CTL index 0x%.02x (%s %s)\n", ee->ctl_index_5g[i],
		    eeprom_9300_ctl_idx_to_regdomain(ee->ctl_index_5g[i]),
		    eeprom_9300_ctl_idx_to_mode(ee->ctl_index_5g[i]));
		for (j = 0; j < OSPREY_NUM_BAND_EDGES_5G; j++) {
		  printf("|   Freq %u pow2x %u Flags 0x%x\n",
		      FBIN2FREQ(ee->ctl_freqbin_5G[i][j], 0),
		      ee->ctl_power_data_5g[i].ctl_edges[j].t_power,
		      ee->ctl_power_data_5g[i].ctl_edges[j].flag);
		}
		printf("\n");
	}
}

static void
load_eeprom_dump(const char *file, uint16_t *buf)
{
	unsigned int r[8];
	FILE *fp;
	char b[1024];
	int i;

	fp = fopen(file, "r");
	if (!fp)
		err(1, "fopen");

	while (!feof(fp)) {
		if (fgets(b, 1024, fp) == NULL)
			break;
		if (feof(fp))
			break;
		if (strlen(b) > 0)
			b[strlen(b)-1] = '\0';
		if (strlen(b) == 0)
			break;
		sscanf(b, "%x: %x %x %x %x %x %x %x %x\n",
		    &i, &r[0], &r[1], &r[2], &r[3], &r[4],
		    &r[5], &r[6], &r[7]);
		buf[i++] = r[0];
		buf[i++] = r[1];
		buf[i++] = r[2];
		buf[i++] = r[3];
		buf[i++] = r[4];
		buf[i++] = r[5];
		buf[i++] = r[6];
		buf[i++] = r[7];
	}
	fclose(fp);
}

void
usage(char *argv[])
{
	printf("Usage: %s <eeprom dump file>\n", argv[0]);
	printf("\n");
	printf("  The eeprom dump file is a text hexdump of an EEPROM.\n");
	printf("  The lines must be formatted as follows:\n");
	printf("  0xAAAA: 0xDD 0xDD 0xDD 0xDD 0xDD 0xDD 0xDD 0xDD\n");
	printf("  where each line must have exactly eight data bytes.\n");
	exit(127);
}

int
main(int argc, char *argv[])
{
	uint16_t *eep = NULL;
	const ar9300_eeprom_t *ee;

	eep = calloc(4096, sizeof(int16_t));

	if (argc < 2)
		usage(argv);

	load_eeprom_dump(argv[1], eep);
	ee = (ar9300_eeprom_t *) eep;

	eeprom_9300_hdr_print(eep);
	eeprom_9300_base_print(eep);

	printf("\n2GHz modal:\n");
	eeprom_9300_modal_print(&ee->modal_header_2g);
	// TODO: open-loop calibration
	eeprom_9300_print_2g_target_cck(ee);
	eeprom_9300_print_2g_target_ofdm(ee);
	eeprom_9300_print_2g_target_ht20(ee);
	eeprom_9300_print_2g_target_ht40(ee);
	eeprom_9300_print_2g_ctls(ee);

	printf("\n5GHz modal:\n");
	eeprom_9300_modal_print(&ee->modal_header_5g);
	eeprom_9300_print_5g_target_ofdm(ee);
	eeprom_9300_print_5g_target_ht20(ee);
	eeprom_9300_print_5g_target_ht40(ee);
	eeprom_9300_print_5g_ctls(ee);

	free(eep);
	exit(0);
}
