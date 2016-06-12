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
	
	BHND_NVRAM_SRC_OTP,	/**< On-chip one-time-programmable
				  *  memory. */

	BHND_NVRAM_SRC_FLASH,	/**< External flash */
	BHND_NVRAM_SRC_SPROM,	/**< External serial EEPROM. */
	
	BHND_NVRAM_SRC_UNKNOWN	/**< No NVRAM source is directly
				  *  attached.
				  *
				  *  This will be returned by ChipCommon
				  *  revisions (rev <= 31) used in early
				  *  chipsets that vend SPROM/OTP via the
				  *  native host bridge interface.
				  *
				  *  For example, PCMCIA cards may vend
				  *  Broadcom NVRAM data via their standard CIS
				  *  table, and earlier PCI(e) devices map
				  *  SPROM statically into PCI BARs, and the
				  *  control registers into PCI config space.
				  
				  *  This will also be returned on later
				  *  devices that are attached via PCI(e) to
				  *  BHND SoCs, but do not include an attached
				  *  SPROM, or programmed OTP. On such SoCs,
				  *  NVRAM configuration for individual devices
				  *  is provided by a common platform NVRAM
				  *  device.
				  */
} bhnd_nvram_src;

#endif /* _BHND_NVRAM_BHND_NVRAM_H_ */
