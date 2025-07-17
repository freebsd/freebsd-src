/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2010 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom_v14.h"
#include "ah_eeprom_9287.h"

static HAL_STATUS
v9287EepromGet(struct ath_hal *ah, int param, void *val)
{
#define	CHAN_A_IDX	0
#define	CHAN_B_IDX	1
#define	IS_VERS(op, v)	((pBase->version & AR5416_EEP_VER_MINOR_MASK) op (v))
	HAL_EEPROM_9287 *ee = AH_PRIVATE(ah)->ah_eeprom;
	const MODAL_EEP_9287_HEADER *pModal = &ee->ee_base.modalHeader;
	const BASE_EEP_9287_HEADER *pBase = &ee->ee_base.baseEepHeader;
	uint32_t sum;
	uint8_t *macaddr;
	int i;

	switch (param) {
        case AR_EEP_NFTHRESH_2:
		*(int16_t *)val = pModal->noiseFloorThreshCh[0];
		return HAL_OK;
        case AR_EEP_MACADDR:		/* Get MAC Address */
		sum = 0;
		macaddr = val;
		for (i = 0; i < 6; i++) {
			macaddr[i] = pBase->macAddr[i];
			sum += pBase->macAddr[i];
		}
		if (sum == 0 || sum == 0xffff*3) {
			HALDEBUG(ah, HAL_DEBUG_ANY, "%s: bad mac address %s\n",
			    __func__, ath_hal_ether_sprintf(macaddr));
			return HAL_EEBADMAC;
		}
		return HAL_OK;
        case AR_EEP_REGDMN_0:
		return pBase->regDmn[0];
        case AR_EEP_REGDMN_1:
		return pBase->regDmn[1];
        case AR_EEP_OPCAP:
		return pBase->deviceCap;
        case AR_EEP_OPMODE:
		return pBase->opCapFlags;
        case AR_EEP_RFSILENT:
		return pBase->rfSilent;
	case AR_EEP_TXMASK:
		return pBase->txMask;
	case AR_EEP_RXMASK:
		return pBase->rxMask;
	case AR_EEP_OL_PWRCTRL:
		HALASSERT(val == AH_NULL);
		return pBase->openLoopPwrCntl ?  HAL_OK : HAL_EIO;
	case AR_EEP_AMODE:
		return HAL_EIO;		/* no 5GHz for Kiwi */
	case AR_EEP_BMODE:
	case AR_EEP_GMODE:
		HALASSERT(val == AH_NULL);
		return pBase->opCapFlags & AR5416_OPFLAGS_11G ?
		    HAL_OK : HAL_EIO;
	case AR_EEP_32KHZCRYSTAL:
	case AR_EEP_COMPRESS:
	case AR_EEP_FASTFRAME:		/* XXX policy decision, h/w can do it */
	case AR_EEP_WRITEPROTECT:	/* NB: no write protect bit */
		HALASSERT(val == AH_NULL);
		/* fall thru... */
	case AR_EEP_MAXQCU:		/* NB: not in opCapFlags */
	case AR_EEP_KCENTRIES:		/* NB: not in opCapFlags */
		return HAL_EIO;
	case AR_EEP_AES:
	case AR_EEP_BURST:
        case AR_EEP_RFKILL:
	case AR_EEP_TURBO5DISABLE:
	case AR_EEP_TURBO2DISABLE:
		HALASSERT(val == AH_NULL);
		return HAL_OK;
	case AR_EEP_ANTGAINMAX_2:
		*(int8_t *) val = ee->ee_antennaGainMax[1];
		return HAL_OK;
	case AR_EEP_PWR_TABLE_OFFSET:
		*(int8_t *) val = pBase->pwrTableOffset;
		return HAL_OK;
	case AR_EEP_TEMPSENSE_SLOPE:
		if (IS_VERS(>=,  AR9287_EEP_MINOR_VER_2))
			*(int8_t *)val = pBase->tempSensSlope;
		else
			*(int8_t *)val = 0;
		return HAL_OK;
	case AR_EEP_TEMPSENSE_SLOPE_PAL_ON:
		if (IS_VERS(>=,  AR9287_EEP_MINOR_VER_3))
			*(int8_t *)val = pBase->tempSensSlopePalOn;
		else
			*(int8_t *)val = 0;
		return HAL_OK;
        default:
		HALASSERT(0);
		return HAL_EINVAL;
	}
#undef IS_VERS
#undef CHAN_A_IDX
#undef CHAN_B_IDX
}

static HAL_STATUS
v9287EepromSet(struct ath_hal *ah, int param, int v)
{
	HAL_EEPROM_9287 *ee = AH_PRIVATE(ah)->ah_eeprom;

	switch (param) {
		case AR_EEP_ANTGAINMAX_2:
			ee->ee_antennaGainMax[1] = (int8_t) v;
			return HAL_OK;
		default:
			return HAL_EINVAL;
	}
}

static HAL_BOOL
v9287EepromDiag(struct ath_hal *ah, int request,
     const void *args, uint32_t argsize, void **result, uint32_t *resultsize)
{
	HAL_EEPROM_9287 *ee = AH_PRIVATE(ah)->ah_eeprom;

	switch (request) {
	case HAL_DIAG_EEPROM:
		*result = ee;
		*resultsize = sizeof(HAL_EEPROM_9287);
		return AH_TRUE;
	}
	return AH_FALSE;
}

/* Do structure specific swaps if Eeprom format is non native to host */
static void
eepromSwap(HAL_EEPROM_9287 *ee)
{
	uint32_t integer, i;
	uint16_t word;
	MODAL_EEP_9287_HEADER *pModal;

	/* convert Base Eep header */
	word = __bswap16(ee->ee_base.baseEepHeader.length);
	ee->ee_base.baseEepHeader.length = word;

	word = __bswap16(ee->ee_base.baseEepHeader.checksum);
	ee->ee_base.baseEepHeader.checksum = word;

	word = __bswap16(ee->ee_base.baseEepHeader.version);
	ee->ee_base.baseEepHeader.version = word;

	word = __bswap16(ee->ee_base.baseEepHeader.regDmn[0]);
	ee->ee_base.baseEepHeader.regDmn[0] = word;

	word = __bswap16(ee->ee_base.baseEepHeader.regDmn[1]);
	ee->ee_base.baseEepHeader.regDmn[1] = word;

	word = __bswap16(ee->ee_base.baseEepHeader.rfSilent);
	ee->ee_base.baseEepHeader.rfSilent = word;

	word = __bswap16(ee->ee_base.baseEepHeader.blueToothOptions);
	ee->ee_base.baseEepHeader.blueToothOptions = word; 

	word = __bswap16(ee->ee_base.baseEepHeader.deviceCap);
	ee->ee_base.baseEepHeader.deviceCap = word;

	/* convert Modal Eep header */

	/* only 2.4ghz here; so only one modal header entry */
	pModal = &ee->ee_base.modalHeader;

	/* XXX linux/ah_osdep.h only defines __bswap32 for BE */
	integer = __bswap32(pModal->antCtrlCommon);
	pModal->antCtrlCommon = integer;

	for (i = 0; i < AR9287_MAX_CHAINS; i++) {
		integer = __bswap32(pModal->antCtrlChain[i]);
		pModal->antCtrlChain[i] = integer;
	}
	for (i = 0; i < AR5416_EEPROM_MODAL_SPURS; i++) {
		word = __bswap16(pModal->spurChans[i].spurChan);
		pModal->spurChans[i].spurChan = word;
	}
}

static uint16_t 
v9287EepromGetSpurChan(struct ath_hal *ah, int ix, HAL_BOOL is2GHz)
{ 
	HAL_EEPROM_9287 *ee = AH_PRIVATE(ah)->ah_eeprom;

	HALASSERT(is2GHz == AH_TRUE);
	if (is2GHz != AH_TRUE)
		return 0;	/* XXX ? */

	HALASSERT(0 <= ix && ix <  AR5416_EEPROM_MODAL_SPURS);
	return ee->ee_base.modalHeader.spurChans[ix].spurChan;
}

/**************************************************************************
 * fbin2freq
 *
 * Get channel value from binary representation held in eeprom
 * RETURNS: the frequency in MHz
 */
static uint16_t
fbin2freq(uint8_t fbin, HAL_BOOL is2GHz)
{
	/*
	 * Reserved value 0xFF provides an empty definition both as
	 * an fbin and as a frequency - do not convert
	*/
	if (fbin == AR5416_BCHAN_UNUSED)
		return fbin;
	return (uint16_t)((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

/*
 * Copy EEPROM Conformance Testing Limits contents 
 * into the allocated space
 */
/* USE CTLS from chain zero */ 
#define CTL_CHAIN	0 

static void
v9287EepromReadCTLInfo(struct ath_hal *ah, HAL_EEPROM_9287 *ee)
{
	RD_EDGES_POWER *rep = ee->ee_rdEdgesPower;
	int i, j;

	HALASSERT(AR9287_NUM_CTLS <= sizeof(ee->ee_rdEdgesPower)/NUM_EDGES);

	for (i = 0; ee->ee_base.ctlIndex[i] != 0 && i < AR9287_NUM_CTLS; i++) {
		for (j = 0; j < NUM_EDGES; j ++) {
			/* XXX Confirm this is the right thing to do when an invalid channel is stored */
			if (ee->ee_base.ctlData[i].ctlEdges[CTL_CHAIN][j].bChannel == AR5416_BCHAN_UNUSED) {
				rep[j].rdEdge = 0;
				rep[j].twice_rdEdgePower = 0;
				rep[j].flag = 0;
			} else {
				rep[j].rdEdge = fbin2freq(
				    ee->ee_base.ctlData[i].ctlEdges[CTL_CHAIN][j].bChannel,
				    (ee->ee_base.ctlIndex[i] & CTL_MODE_M) != CTL_11A);
				rep[j].twice_rdEdgePower = MS(ee->ee_base.ctlData[i].ctlEdges[CTL_CHAIN][j].tPowerFlag, CAL_CTL_EDGES_POWER);
				rep[j].flag = MS(ee->ee_base.ctlData[i].ctlEdges[CTL_CHAIN][j].tPowerFlag, CAL_CTL_EDGES_FLAG) != 0;
			}
		}
		rep += NUM_EDGES;
	}
	ee->ee_numCtls = i;
	HALDEBUG(ah, HAL_DEBUG_ATTACH | HAL_DEBUG_EEPROM,
	    "%s Numctls = %u\n",__func__,i);
}

/*
 * Reclaim any EEPROM-related storage.
 */
static void
v9287EepromDetach(struct ath_hal *ah)
{
	HAL_EEPROM_9287 *ee = AH_PRIVATE(ah)->ah_eeprom;

	ath_hal_free(ee);
	AH_PRIVATE(ah)->ah_eeprom = AH_NULL;
}

#define owl_get_eep_ver(_ee)   \
    (((_ee)->ee_base.baseEepHeader.version >> 12) & 0xF)
#define owl_get_eep_rev(_ee)   \
    (((_ee)->ee_base.baseEepHeader.version) & 0xFFF)

HAL_STATUS
ath_hal_9287EepromAttach(struct ath_hal *ah)
{
#define	NW(a)	(sizeof(a) / sizeof(uint16_t))
	HAL_EEPROM_9287 *ee = AH_PRIVATE(ah)->ah_eeprom;
	uint16_t *eep_data, magic;
	HAL_BOOL need_swap;
	u_int w, off, len;
	uint32_t sum;

	HALASSERT(ee == AH_NULL);

	/*
	 * Don't check magic if we're supplied with an EEPROM block,
	 * typically this is from Howl but it may also be from later
	 * boards w/ an embedded WMAC.
	 */
	if (ah->ah_eepromdata == NULL) {
		if (!ath_hal_eepromRead(ah, AR5416_EEPROM_MAGIC_OFFSET, &magic)) {
			HALDEBUG(ah, HAL_DEBUG_ANY,
			    "%s Error reading Eeprom MAGIC\n", __func__);
			return HAL_EEREAD;
		}
		HALDEBUG(ah, HAL_DEBUG_ATTACH, "%s Eeprom Magic = 0x%x\n",
		    __func__, magic);
		if (magic != AR5416_EEPROM_MAGIC) {
			HALDEBUG(ah, HAL_DEBUG_ANY, "Bad magic number\n");
			return HAL_EEMAGIC;
		}
	}

	ee = ath_hal_malloc(sizeof(HAL_EEPROM_9287));
	if (ee == AH_NULL) {
		/* XXX message */
		return HAL_ENOMEM;
	}

	eep_data = (uint16_t *) ee;
	for (w = 0; w < NW(struct ar9287_eeprom); w++) {
		off = AR9287_EEP_START_LOC + w;
		if (!ath_hal_eepromRead(ah, off, &eep_data[w])) {
			HALDEBUG(ah, HAL_DEBUG_ANY,
			    "%s eeprom read error at offset 0x%x\n",
			    __func__, off);
			return HAL_EEREAD;
		}
	}
	/* Convert to eeprom native eeprom endian format */
	/*
	 * XXX this is likely incorrect but will do for now
	 * XXX to get embedded boards working.
	 */
	if (ah->ah_eepromdata == NULL && isBigEndian()) {
		for (w = 0; w < NW(HAL_EEPROM_9287); w++)
			eep_data[w] = __bswap16(eep_data[w]);
	}

	/*
	 * At this point, we're in the native eeprom endian format
	 * Now, determine the eeprom endian by looking at byte 26??
	 */
	need_swap = ((ee->ee_base.baseEepHeader.eepMisc & AR5416_EEPMISC_BIG_ENDIAN) != 0) ^ isBigEndian();
	if (need_swap) {
		HALDEBUG(ah, HAL_DEBUG_ATTACH | HAL_DEBUG_EEPROM,
		    "Byte swap EEPROM contents.\n");
		len = __bswap16(ee->ee_base.baseEepHeader.length);
	} else {
		len = ee->ee_base.baseEepHeader.length;
	}
	len = AH_MIN(len, sizeof(HAL_EEPROM_9287)) / sizeof(uint16_t);

	/* Apply the checksum, done in native eeprom format */
	/* XXX - Need to check to make sure checksum calculation is done
	 * in the correct endian format.  Right now, it seems it would
	 * cast the raw data to host format and do the calculation, which may
	 * not be correct as the calculation may need to be done in the native
	 * eeprom format 
	 */
	sum = 0;
	for (w = 0; w < len; w++)
		sum ^= eep_data[w];
	/* Check CRC - Attach should fail on a bad checksum */
	if (sum != 0xffff) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "Bad EEPROM checksum 0x%x (Len=%u)\n", sum, len);
		return HAL_EEBADSUM;
	}

	if (need_swap)
		eepromSwap(ee);	/* byte swap multi-byte data */

	/* swap words 0+2 so version is at the front */
	magic = eep_data[0];
	eep_data[0] = eep_data[2];
	eep_data[2] = magic;

	HALDEBUG(ah, HAL_DEBUG_ATTACH | HAL_DEBUG_EEPROM,
	    "%s Eeprom Version %u.%u\n", __func__,
	    owl_get_eep_ver(ee), owl_get_eep_rev(ee));

	/* NB: must be after all byte swapping */
	if (owl_get_eep_ver(ee) != AR5416_EEP_VER) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "Bad EEPROM version 0x%x\n", owl_get_eep_ver(ee));
		return HAL_EEBADSUM;
	}

	v9287EepromReadCTLInfo(ah, ee);		/* Get CTLs */

	AH_PRIVATE(ah)->ah_eeprom = ee;
	AH_PRIVATE(ah)->ah_eeversion = ee->ee_base.baseEepHeader.version;
	AH_PRIVATE(ah)->ah_eepromDetach = v9287EepromDetach;
	AH_PRIVATE(ah)->ah_eepromGet = v9287EepromGet;
	AH_PRIVATE(ah)->ah_eepromSet = v9287EepromSet;
	AH_PRIVATE(ah)->ah_getSpurChan = v9287EepromGetSpurChan;
	AH_PRIVATE(ah)->ah_eepromDiag = v9287EepromDiag;
	return HAL_OK;
#undef NW
}
