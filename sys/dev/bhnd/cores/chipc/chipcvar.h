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
	CHIPC_QUIRK_NONE		= 0,
	
	/**
	 * The device always provides an external SROM.
	 */
	CHIPC_QUIRK_ALWAYS_HAS_SPROM	= (1<<1),
	
	
	/**
	 * SROM availability must be determined through chip-specific
	 * ChipStatus flags.
	 */
	CHIPC_QUIRK_SPROM_CHECK_CHIPST	= (1<<3),

	/**
	 * Use the rev22 chipstatus register format when determining SPROM
	 * availability.
	 */
	CHIPC_QUIRK_SPROM_CHECK_CST_R22	= (1<<4)|CHIPC_QUIRK_SPROM_CHECK_CHIPST,
	
	/**
	 * Use the rev23 chipstatus register format when determining SPROM
	 * availability.
	 */
	CHIPC_QUIRK_SPROM_CHECK_CST_R23	= (1<<5)|CHIPC_QUIRK_SPROM_CHECK_CHIPST,

	/**
	 * External NAND NVRAM is supported, along with the CHIPC_CAP_NFLASH
	 * capability flag.
	 */
	CHIPC_QUIRK_SUPPORTS_NFLASH	= (1<<6),
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
};

#endif /* _BHND_CORES_CHIPC_CHIPCVAR_H_ */