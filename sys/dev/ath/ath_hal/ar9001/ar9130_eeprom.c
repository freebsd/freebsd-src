/*
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd
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
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"

#include "ar9001/ar9130_eeprom.h"

/* XXX this shouldn't be done here */
/* This is in 16 bit words; not bytes -adrian */
#define ATH_DATA_EEPROM_SIZE    2048

HAL_BOOL
ar9130EepromRead(struct ath_hal *ah, u_int off, uint16_t *data)
{
	if (ah->ah_eepromdata == AH_NULL) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: no eeprom data!\n", __func__);
		return AH_FALSE;
	}
	if (off > ATH_DATA_EEPROM_SIZE) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "ar9130EepromRead: offset %x > %x\n", off, ATH_DATA_EEPROM_SIZE);
		return AH_FALSE;
	}
	(*data) = ah->ah_eepromdata[off];
	return AH_TRUE;
}
