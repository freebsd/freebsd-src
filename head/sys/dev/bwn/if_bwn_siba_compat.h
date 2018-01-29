/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Landon J. Fuller <landonf@FreeBSD.org>.
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Landon Fuller
 * under sponsorship from the FreeBSD Foundation.
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

#ifndef _IF_BWN_SIBA_COMPAT_H_
#define _IF_BWN_SIBA_COMPAT_H_

#define	BWN_USE_SIBA	0
#include "if_bwn_siba.h"

#include "if_bwnvar.h"

#define	BWN_BHND_NUM_CORE_PWR	4

/**
 * Compatiblity shim state.
 */
struct bwn_bhnd_ctx {
	device_t	chipc_dev;	/**< ChipCommon device */
	device_t	gpio_dev;	/**< GPIO device */

	device_t	pmu_dev;	/**< PMU device, or NULL if no PMU */
	uint32_t	pmu_cctl_addr;	/**< chipctrl_addr target of
					     reads/writes to/from the
					     chipctrl_data register */

	uint8_t		sromrev;	/**< SROM format revision */

	/* NVRAM variables for which bwn(4) expects the bus to manage storage
	 * for (and in some cases, allow writes). */	
	uint8_t		mac_80211bg[6];	/**< D11 unit 0 */
	uint8_t		mac_80211a[6];	/**< D11 unit 1 */

	uint32_t	boardflags;	/**< boardflags (bwn-writable) */
	uint8_t		pa0maxpwr;	/**< 2GHz max power (bwn-writable) */
};

/**
 * Return the bwn(4) device's bhnd compatiblity context.
 */
static inline struct bwn_bhnd_ctx *
bwn_bhnd_get_ctx(device_t dev)
{
	struct bwn_softc *sc = device_get_softc(dev);
	return (sc->sc_bus_ctx);
}

/**
 * Fetch an NVRAM variable via bhnd_nvram_getvar_*().
 */
#define	BWN_BHND_NVRAM_FETCH_VAR(_dev, _type, _name, _result)		\
do {									\
	int error;							\
									\
	error = bhnd_nvram_getvar_ ## _type(_dev, _name, _result);	\
	if (error) {							\
		panic("NVRAM variable %s unreadable: %d", _name,	\
		    error);						\
	}								\
} while(0)

/**
 * Fetch and return an NVRAM variable via bhnd_nvram_getvar_*().
 */
#define	BWN_BHND_NVRAM_RETURN_VAR(_dev, _type, _name)			\
do {									\
	_type ## _t	value;						\
	BWN_BHND_NVRAM_FETCH_VAR(_dev, _type, _name, &value);		\
	return (value);							\
} while(0)

#endif /* _IF_BWN_SIBA_COMPAT_H_ */
