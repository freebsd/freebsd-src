/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * $FreeBSD$
 */

#ifndef _BHND_CORES_CHIPC_CHIPCVAR_H_
#define _BHND_CORES_CHIPC_CHIPCVAR_H_

#include <dev/bhnd/nvram/bhnd_spromvar.h>

#include "chipc.h"

DECLARE_CLASS(bhnd_chipc);
extern devclass_t bhnd_chipc_devclass;

#define	CHIPC_MAX_RES	1
#define	CHIPC_MAX_RSPEC	(CHIPC_MAX_RES+1)

/* 
 * ChipCommon device quirks / features
 */
enum {
	/** No quirks */
	CHIPC_QUIRK_NONE			= 0,
	
	/**
	 * ChipCommon-controlled SPROM/OTP is supported, along with the
	 * CHIPC_CAP_SPROM capability flag.
	 */
	CHIPC_QUIRK_SUPPORTS_SPROM		= (1<<1),

	/**
	 * External NAND NVRAM is supported, along with the CHIPC_CAP_NFLASH
	 * capability flag.
	 */
	CHIPC_QUIRK_SUPPORTS_NFLASH		= (1<<2),

	/**
	 * The SPROM is attached via muxed pins. The pins must be switched
	 * to allow reading/writing.
	 */
	CHIPC_QUIRK_MUX_SPROM			= (1<<3),
	
	/**
	 * Access to the SPROM uses pins shared with the 802.11a external PA.
	 * 
	 * On modules using these 4331 packages, the CCTRL4331_EXTPA_EN flag
	 * must be cleared to allow SPROM access.
	 */
	CHIPC_QUIRK_4331_EXTPA_MUX_SPROM	= (1<<4) |
	    CHIPC_QUIRK_MUX_SPROM,

	/**
	 * Access to the SPROM uses pins shared with the 802.11a external PA.
	 * 
	 * On modules using these 4331 chip packages, the external PA is
	 * attached via GPIO 2, 5, and sprom_dout pins.
	 * 
	 * When enabling and disabling EXTPA to allow SPROM access, the
	 * CCTRL4331_EXTPA_ON_GPIO2_5 flag must also be set or cleared,
	 * respectively.
	 */
	CHIPC_QUIRK_4331_GPIO2_5_MUX_SPROM	= (1<<5) |
	    CHIPC_QUIRK_4331_EXTPA_MUX_SPROM,

	/**
	 * Access to the SPROM uses pins shared with two 802.11a external PAs.
	 * 
	 * When enabling and disabling EXTPA, the CCTRL4331_EXTPA_EN2 must also
	 * be cleared to allow SPROM access.
	 */
	CHIPC_QUIRK_4331_EXTPA2_MUX_SPROM	= (1<<6) |
	    CHIPC_QUIRK_4331_EXTPA_MUX_SPROM,
	

	/**
	 * SPROM pins are muxed with the FEM control lines on this 4360-family
	 * device. The muxed pins must be switched to allow reading/writing
	 * the SPROM.
	 */
	CHIPC_QUIRK_4360_FEM_MUX_SPROM	= (1<<5) | CHIPC_QUIRK_MUX_SPROM
};

struct chipc_softc {
	device_t		dev;

	struct resource_spec	 rspec[CHIPC_MAX_RSPEC];
	struct bhnd_resource	*res[CHIPC_MAX_RES];

	struct bhnd_resource	*core;		/**< core registers. */
	struct bhnd_chipid	 ccid;		/**< chip identification */
	uint32_t		 quirks;	/**< CHIPC_QUIRK_* quirk flags */
	uint32_t		 caps;		/**< CHIPC_CAP_* capability register flags */
	uint32_t		 cst;		/**< CHIPC_CST* status register flags */
	bhnd_nvram_src_t	 nvram_src;	/**< NVRAM source */
	
	struct mtx		 mtx;		/**< state mutex. */

	struct bhnd_sprom	 sprom;		/**< OTP/SPROM shadow, if any */
};

#define	CHIPC_LOCK_INIT(sc) \
	mtx_init(&(sc)->mtx, device_get_nameunit((sc)->dev), \
	    "BHND chipc driver lock", MTX_DEF)
#define	CHIPC_LOCK(sc)				mtx_lock(&(sc)->mtx)
#define	CHIPC_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	CHIPC_LOCK_ASSERT(sc, what)		mtx_assert(&(sc)->mtx, what)
#define	CHIPC_LOCK_DESTROY(sc)			mtx_destroy(&(sc)->mtx)

#endif /* _BHND_CORES_CHIPC_CHIPCVAR_H_ */