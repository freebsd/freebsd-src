/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2008 Atheros Communications, Inc.
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
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"                    /* NB: for HAL_PHYERR* */
#endif

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

/*
 * Return the wireless modes (a,b,g,n,t) supported by hardware.
 *
 * This value is what is actually supported by the hardware
 * and is unaffected by regulatory/country code settings.
 *
 */
u_int
ar5416GetWirelessModes(struct ath_hal *ah)
{
	u_int mode;
	struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
	HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;

	mode = ar5212GetWirelessModes(ah);

	/* Only enable HT modes if the NIC supports HT */
	if (pCap->halHTSupport == AH_TRUE && (mode & HAL_MODE_11A))
		mode |= HAL_MODE_11NA_HT20
		     |  HAL_MODE_11NA_HT40PLUS
		     |  HAL_MODE_11NA_HT40MINUS
		     ;
	if (pCap->halHTSupport == AH_TRUE && (mode & HAL_MODE_11G))
		mode |= HAL_MODE_11NG_HT20
		     |  HAL_MODE_11NG_HT40PLUS
		     |  HAL_MODE_11NG_HT40MINUS
		     ;
	return mode;
}

/*
 * Change the LED blinking pattern to correspond to the connectivity
 */
void
ar5416SetLedState(struct ath_hal *ah, HAL_LED_STATE state)
{
	static const uint32_t ledbits[8] = {
		AR_MAC_LED_ASSOC_NONE,		/* HAL_LED_INIT */
		AR_MAC_LED_ASSOC_PEND,		/* HAL_LED_SCAN */
		AR_MAC_LED_ASSOC_PEND,		/* HAL_LED_AUTH */
		AR_MAC_LED_ASSOC_ACTIVE,	/* HAL_LED_ASSOC*/
		AR_MAC_LED_ASSOC_ACTIVE,	/* HAL_LED_RUN */
		AR_MAC_LED_ASSOC_NONE,
		AR_MAC_LED_ASSOC_NONE,
		AR_MAC_LED_ASSOC_NONE,
	};
	uint32_t bits;

	bits = OS_REG_READ(ah, AR_MAC_LED);
	bits = (bits &~ AR_MAC_LED_MODE)
	     | SM(AR_MAC_LED_MODE_POWON, AR_MAC_LED_MODE)
#if 1
	     | SM(AR_MAC_LED_MODE_NETON, AR_MAC_LED_MODE)
#endif
	     ;
	bits = (bits &~ AR_MAC_LED_ASSOC)
	     | SM(ledbits[state & 0x7], AR_MAC_LED_ASSOC);
	OS_REG_WRITE(ah, AR_MAC_LED, bits);
}

/*
 * Reset the current hardware tsf for stamlme.
 */
void
ar5416ResetTsf(struct ath_hal *ah)
{
	uint32_t v;
	int i;

	for (i = 0; i < 10; i++) {
		v = OS_REG_READ(ah, AR_SLP32_MODE);
		if ((v & AR_SLP32_TSF_WRITE_STATUS) == 0)
			break;
		OS_DELAY(10);
	}
	OS_REG_WRITE(ah, AR_RESET_TSF, AR_RESET_TSF_ONCE);	
}

HAL_BOOL
ar5416SetAntennaSwitch(struct ath_hal *ah, HAL_ANT_SETTING settings)
{
	return AH_TRUE;
}

/* Setup decompression for given key index */
HAL_BOOL
ar5416SetDecompMask(struct ath_hal *ah, uint16_t keyidx, int en)
{
	return HAL_OK;
}

/* Setup coverage class */
void
ar5416SetCoverageClass(struct ath_hal *ah, uint8_t coverageclass, int now)
{
	AH_PRIVATE(ah)->ah_coverageClass = coverageclass;
}

/*
 * Return approximation of extension channel busy over an time interval
 * 0% (clear) -> 100% (busy)
 *
 */
uint32_t
ar5416Get11nExtBusy(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    uint32_t busy; /* percentage */
    uint32_t cycleCount, ctlBusy, extBusy;

    ctlBusy = OS_REG_READ(ah, AR_RCCNT);
    extBusy = OS_REG_READ(ah, AR_EXTRCCNT);
    cycleCount = OS_REG_READ(ah, AR_CCCNT);

    if (ahp->ah_cycleCount == 0 || ahp->ah_cycleCount > cycleCount) {
        /*
         * Cycle counter wrap (or initial call); it's not possible
         * to accurately calculate a value because the registers
         * right shift rather than wrap--so punt and return 0.
         */
        busy = 0;
        HALDEBUG(ah, HAL_DEBUG_ANY, "%s: cycle counter wrap. ExtBusy = 0\n",
	    __func__);

    } else {
        uint32_t cycleDelta = cycleCount - ahp->ah_cycleCount;
        uint32_t ctlBusyDelta = ctlBusy - ahp->ah_ctlBusy;
        uint32_t extBusyDelta = extBusy - ahp->ah_extBusy;
        uint32_t ctlClearDelta = 0;

        /* Compute control channel rxclear.
         * The cycle delta may be less than the control channel delta.
         * This could be solved by freezing the timers (or an atomic read,
         * if one was available). Checking for the condition should be
         * sufficient.
         */
        if (cycleDelta > ctlBusyDelta) {
            ctlClearDelta = cycleDelta - ctlBusyDelta;
        }

        /* Compute ratio of extension channel busy to control channel clear
         * as an approximation to extension channel cleanliness.
         *
         * According to the hardware folks, ext rxclear is undefined
         * if the ctrl rxclear is de-asserted (i.e. busy)
         */
        if (ctlClearDelta) {
            busy = (extBusyDelta * 100) / ctlClearDelta;
        } else {
            busy = 100;
        }
        if (busy > 100) {
            busy = 100;
        }
#if 0
        HALDEBUG(ah, HAL_DEBUG_ANY, "%s: cycleDelta 0x%x, ctlBusyDelta 0x%x, "
             "extBusyDelta 0x%x, ctlClearDelta 0x%x, "
             "busy %d\n",
              __func__, cycleDelta, ctlBusyDelta, extBusyDelta, ctlClearDelta, busy);
#endif
    }

    ahp->ah_cycleCount = cycleCount;
    ahp->ah_ctlBusy = ctlBusy;
    ahp->ah_extBusy = extBusy;

    return busy;
}

/*
 * Configure 20/40 operation
 *
 * 20/40 = joint rx clear (control and extension)
 * 20    = rx clear (control)
 *
 * - NOTE: must stop MAC (tx) and requeue 40 MHz packets as 20 MHz when changing
 *         from 20/40 => 20 only
 */
void
ar5416Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE mode)
{
    uint32_t macmode;

    /* Configure MAC for 20/40 operation */
    if (mode == HAL_HT_MACMODE_2040) {
        macmode = AR_2040_JOINED_RX_CLEAR;
    } else {
        macmode = 0;
    }
    OS_REG_WRITE(ah, AR_2040_MODE, macmode);
}

/*
 * Get Rx clear (control/extension channel)
 *
 * Returns active low (busy) for ctrl/ext channel
 * Owl 2.0
 */
HAL_HT_RXCLEAR
ar5416Get11nRxClear(struct ath_hal *ah)
{
    HAL_HT_RXCLEAR rxclear = 0;
    uint32_t val;

    val = OS_REG_READ(ah, AR_DIAG_SW);

    /* control channel */
    if (val & AR_DIAG_RXCLEAR_CTL_LOW) {
        rxclear |= HAL_RX_CLEAR_CTL_LOW;
    }
    /* extension channel */
    if (val & AR_DIAG_RXCLEAR_CTL_LOW) {
        rxclear |= HAL_RX_CLEAR_EXT_LOW;
    }
    return rxclear;
}

/*
 * Set Rx clear (control/extension channel)
 *
 * Useful for forcing the channel to appear busy for
 * debugging/diagnostics
 * Owl 2.0
 */
void
ar5416Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear)
{
    /* control channel */
    if (rxclear & HAL_RX_CLEAR_CTL_LOW) {
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RXCLEAR_CTL_LOW);
    } else {
        OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_RXCLEAR_CTL_LOW);
    }
    /* extension channel */
    if (rxclear & HAL_RX_CLEAR_EXT_LOW) {
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RXCLEAR_EXT_LOW);
    } else {
        OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_RXCLEAR_EXT_LOW);
    }
}

HAL_STATUS
ar5416GetCapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
        uint32_t capability, uint32_t *result)
{
	switch (type) {
	case HAL_CAP_BB_HANG:
		switch (capability) {
		case HAL_BB_HANG_RIFS:
			return AR_SREV_SOWL(ah) ? HAL_OK : HAL_ENOTSUPP;
		case HAL_BB_HANG_DFS:
			return AR_SREV_SOWL(ah) ? HAL_OK : HAL_ENOTSUPP;
		case HAL_BB_HANG_RX_CLEAR:
			return AR_SREV_MERLIN(ah) ? HAL_OK : HAL_ENOTSUPP;
		}
		break;
	case HAL_CAP_MAC_HANG:
		return ((ah->ah_macVersion == AR_XSREV_VERSION_OWL_PCI) ||
		    (ah->ah_macVersion == AR_XSREV_VERSION_OWL_PCIE) ||
		    AR_SREV_SOWL(ah)) ?
			HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_DIVERSITY:		/* disable classic fast diversity */
		return HAL_ENXIO;
	default:
		break;
	}
	return ar5212GetCapability(ah, type, capability, result);
}

static int ar5416DetectMacHang(struct ath_hal *ah);
static int ar5416DetectBBHang(struct ath_hal *ah);

HAL_BOOL
ar5416GetDiagState(struct ath_hal *ah, int request,
	const void *args, uint32_t argsize,
	void **result, uint32_t *resultsize)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	int hangs;

	if (ath_hal_getdiagstate(ah, request, args, argsize, result, resultsize))
		return AH_TRUE;
	switch (request) {
	case HAL_DIAG_EEPROM:
		return ath_hal_eepromDiag(ah, request,
		    args, argsize, result, resultsize);
	case HAL_DIAG_CHECK_HANGS:
		if (argsize != sizeof(int))
			return AH_FALSE;
		hangs = *(const int *) args;
		ahp->ah_hangs = 0;
		if (hangs & HAL_BB_HANGS)
			ahp->ah_hangs |= ar5416DetectBBHang(ah);
		/* NB: if BB is hung MAC will be hung too so skip check */
		if (ahp->ah_hangs == 0 && (hangs & HAL_MAC_HANGS))
			ahp->ah_hangs |= ar5416DetectMacHang(ah);
		*result = &ahp->ah_hangs;
		*resultsize = sizeof(ahp->ah_hangs);
		return AH_TRUE;
	}
	return ar5212GetDiagState(ah, request,
	    args, argsize, result, resultsize);
}

typedef struct {
	uint32_t dma_dbg_3;
	uint32_t dma_dbg_4;
	uint32_t dma_dbg_5;
	uint32_t dma_dbg_6;
} mac_dbg_regs_t;

typedef enum {
	dcu_chain_state		= 0x1,
	dcu_complete_state	= 0x2,
	qcu_state		= 0x4,
	qcu_fsp_ok		= 0x8,
	qcu_fsp_state		= 0x10,
	qcu_stitch_state	= 0x20,
	qcu_fetch_state		= 0x40,
	qcu_complete_state	= 0x80
} hal_mac_hangs_t;

typedef struct {
	int states;
	uint8_t dcu_chain_state;
	uint8_t dcu_complete_state;
	uint8_t qcu_state;
	uint8_t qcu_fsp_ok;
	uint8_t qcu_fsp_state;
	uint8_t qcu_stitch_state;
	uint8_t qcu_fetch_state;
	uint8_t qcu_complete_state;
} hal_mac_hang_check_t;

static HAL_BOOL
ar5416CompareDbgHang(struct ath_hal *ah, const mac_dbg_regs_t *regs,
    const hal_mac_hang_check_t *check)
{
	int found_states;

	found_states = 0;
	if (check->states & dcu_chain_state) {
		int i;

		for (i = 0; i < 6; i++) {
			if (((regs->dma_dbg_4 >> (5*i)) & 0x1f) ==
			    check->dcu_chain_state)
				found_states |= dcu_chain_state;
		}
		for (i = 0; i < 4; i++) {
			if (((regs->dma_dbg_5 >> (5*i)) & 0x1f) ==
			    check->dcu_chain_state)
				found_states |= dcu_chain_state;
		}
	}
	if (check->states & dcu_complete_state) { 
		if ((regs->dma_dbg_6 & 0x3) == check->dcu_complete_state)
			found_states |= dcu_complete_state;
	}
	if (check->states & qcu_stitch_state) { 
		if (((regs->dma_dbg_3 >> 18) & 0xf) == check->qcu_stitch_state)
			found_states |= qcu_stitch_state;
	}
	if (check->states & qcu_fetch_state) { 
		if (((regs->dma_dbg_3 >> 22) & 0xf) == check->qcu_fetch_state)
			found_states |= qcu_fetch_state;
	}
	if (check->states & qcu_complete_state) { 
		if (((regs->dma_dbg_3 >> 26) & 0x7) == check->qcu_complete_state)
			found_states |= qcu_complete_state;
	}
	return (found_states == check->states);
}

#define NUM_STATUS_READS 50

static int
ar5416DetectMacHang(struct ath_hal *ah)
{
	static const hal_mac_hang_check_t hang_sig1 = {
		.dcu_chain_state	= 0x6,
		.dcu_complete_state	= 0x1,
		.states			= dcu_chain_state
					| dcu_complete_state,
	};
	static const hal_mac_hang_check_t hang_sig2 = {
		.qcu_stitch_state	= 0x9,
		.qcu_fetch_state	= 0x8,
		.qcu_complete_state	= 0x4,
		.states			= qcu_stitch_state
					| qcu_fetch_state
					| qcu_complete_state,
        };
	mac_dbg_regs_t mac_dbg;
	int i;

	mac_dbg.dma_dbg_3 = OS_REG_READ(ah, AR_DMADBG_3);
	mac_dbg.dma_dbg_4 = OS_REG_READ(ah, AR_DMADBG_4);
	mac_dbg.dma_dbg_5 = OS_REG_READ(ah, AR_DMADBG_5);
	mac_dbg.dma_dbg_6 = OS_REG_READ(ah, AR_DMADBG_6);
	for (i = 1; i <= NUM_STATUS_READS; i++) {
		if (mac_dbg.dma_dbg_3 != OS_REG_READ(ah, AR_DMADBG_3) ||
		    mac_dbg.dma_dbg_4 != OS_REG_READ(ah, AR_DMADBG_4) ||
		    mac_dbg.dma_dbg_5 != OS_REG_READ(ah, AR_DMADBG_5) ||
		    mac_dbg.dma_dbg_6 != OS_REG_READ(ah, AR_DMADBG_6))
			return 0;
	}

	if (ar5416CompareDbgHang(ah, &mac_dbg, &hang_sig1))
		return HAL_MAC_HANG_SIG1;
	if (ar5416CompareDbgHang(ah, &mac_dbg, &hang_sig2))
		return HAL_MAC_HANG_SIG2;

	HALDEBUG(ah, HAL_DEBUG_ANY, "%s Found an unknown MAC hang signature "
	    "DMADBG_3=0x%x DMADBG_4=0x%x DMADBG_5=0x%x DMADBG_6=0x%x\n",
	    __func__, mac_dbg.dma_dbg_3, mac_dbg.dma_dbg_4, mac_dbg.dma_dbg_5,
	    mac_dbg.dma_dbg_6);

	return HAL_MAC_HANG_UNKNOWN;
}

/*
 * Determine if the baseband using the Observation Bus Register
 */
static int
ar5416DetectBBHang(struct ath_hal *ah)
{
#define N(a) (sizeof(a)/sizeof(a[0]))
	/*
	 * Check the PCU Observation Bus 1 register (0x806c)
	 * NUM_STATUS_READS times
	 *
	 * 4 known BB hang signatures -
	 * [1] bits 8,9,11 are 0. State machine state (bits 25-31) is 0x1E
	 * [2] bits 8,9 are 1, bit 11 is 0. State machine state
	 *     (bits 25-31) is 0x52
	 * [3] bits 8,9 are 1, bit 11 is 0. State machine state
	 *     (bits 25-31) is 0x18
	 * [4] bit 10 is 1, bit 11 is 0. WEP state (bits 12-17) is 0x2,
	 *     Rx State (bits 20-24) is 0x7.
	 */
	static const struct {
		uint32_t val;
		uint32_t mask;
		int code;
	} hang_list[] = {
		/* Reg Value   Reg Mask    Hang Code XXX */
		{ 0x1E000000, 0x7E000B00, HAL_BB_HANG_DFS },
		{ 0x52000B00, 0x7E000B00, HAL_BB_HANG_RIFS },
		{ 0x18000B00, 0x7E000B00, HAL_BB_HANG_RX_CLEAR },
		{ 0x00702400, 0x7E7FFFEF, HAL_BB_HANG_RX_CLEAR }
	};
	uint32_t hang_sig;
	int i;

	hang_sig = OS_REG_READ(ah, AR_OBSERV_1);
	for (i = 1; i <= NUM_STATUS_READS; i++) {
		if (hang_sig != OS_REG_READ(ah, AR_OBSERV_1))
			return 0;
	}
	for (i = 0; i < N(hang_list); i++)
		if ((hang_sig & hang_list[i].mask) == hang_list[i].val) {
			HALDEBUG(ah, HAL_DEBUG_ANY,
			    "%s BB hang, signature 0x%x, code 0x%x\n",
			    __func__, hang_sig, hang_list[i].code);
			return hang_list[i].code;
		}

	HALDEBUG(ah, HAL_DEBUG_ANY, "%s Found an unknown BB hang signature! "
	    "<0x806c>=0x%x\n", __func__, hang_sig);

	return HAL_BB_HANG_UNKNOWN;
#undef N
}
#undef NUM_STATUS_READS
