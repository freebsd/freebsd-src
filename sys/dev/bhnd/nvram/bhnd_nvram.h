/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
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

#ifndef _BHND_NVRAM_BHND_NVRAM_H_
#define _BHND_NVRAM_BHND_NVRAM_H_

/**
 * NVRAM data sources supported by bhnd(4) devices.
 */
typedef enum {
	BHND_NVRAM_SRC_CIS,	/**< Default CIS source; this may
				  *  apply, for example, to PCMCIA cards
				  *  vending Broadcom NVRAM data via
				  *  their standard CIS table. */
	
	BHND_NVRAM_SRC_OTP,	/**< On-chip one-time-programmable
				  *  memory. */

	BHND_NVRAM_SRC_NFLASH,	/**< External flash device accessible
				  *  via on-chip flash core, such
				  *  as the NAND/QSPI controller cores
				  *  used on Northstar devices to access
				  *  NVRAM. */
	BHND_NVRAM_SRC_SPROM,	/**< External serial EEPROM. */
	
	BHND_NVRAM_SRC_NONE	/**< No NVRAM source is directly
				  *  attached. This is used on devices
				  *  attached via PCI(e) to BHND SoCs,
				  *  where to avoid unnecessary flash
				  *  hardware, NVRAM configuration for
				  *  individual devices is provided by
				  *  hardware attached to the SoC
				  *  itself.
				  */
} bhnd_nvram_src_t;

#endif /* _BHND_NVRAM_BHND_NVRAM_H_ */