/*
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd.
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
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"

#include "ah_eeprom_v14.h"

#include "ar9002/ar9280.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"
#include "ar9002/ar9002phy.h"

#include "ar9002/ar9287phy.h"
#include "ar9002/ar9287an.h"
#include "ar9002/ar9287_olc.h"

void
ar9287olcInit(struct ath_hal *ah)
{
	OS_REG_SET_BIT(ah, AR_PHY_TX_PWRCTRL9,
	    AR_PHY_TX_PWRCTRL9_RES_DC_REMOVAL);
	OS_A_REG_RMW_FIELD(ah, AR9287_AN_TXPC0,
	    AR9287_AN_TXPC0_TXPCMODE,
	    AR9287_AN_TXPC0_TXPCMODE_TEMPSENSE);
	OS_DELAY(100);
}

/*
 * Run temperature compensation calibration.
 *
 * The TX gain table is adjusted depending upon the difference
 * between the initial PDADC value and the currently read
 * average TX power sample value. This value is only valid if
 * frames have been transmitted, so currPDADC will be 0 if
 * no frames have yet been transmitted.
 */
void
ar9287olcTemperatureCompensation(struct ath_hal *ah)
{
	uint32_t rddata;
	int32_t delta, currPDADC, slope;

	rddata = OS_REG_READ(ah, AR_PHY_TX_PWRCTRL4);
	currPDADC = MS(rddata, AR_PHY_TX_PWRCTRL_PD_AVG_OUT);

	if (AH5416(ah)->initPDADC == 0 || currPDADC == 0) {
		/*
		 * Zero value indicates that no frames have been transmitted
		 * yet, can't do temperature compensation until frames are
		 * transmitted.
		 */
		return;
	} else {
		int8_t val;
		(void) (ath_hal_eepromGet(ah, AR_EEP_TEMPSENSE_SLOPE, &val));
		slope = val;

		if (slope == 0) { /* to avoid divide by zero case */
			delta = 0;
		} else {
			delta = ((currPDADC - AH5416(ah)->initPDADC)*4) / slope;
		}
		OS_REG_RMW_FIELD(ah, AR_PHY_CH0_TX_PWRCTRL11,
		    AR_PHY_TX_PWRCTRL_OLPC_TEMP_COMP, delta);
		OS_REG_RMW_FIELD(ah, AR_PHY_CH1_TX_PWRCTRL11,
		    AR_PHY_TX_PWRCTRL_OLPC_TEMP_COMP, delta);
	}
}
