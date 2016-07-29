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

#include <sys/types.h>
#include <sys/rman.h>

#include <dev/bhnd/nvram/bhnd_spromvar.h>

#include "chipc.h"

DECLARE_CLASS(bhnd_chipc);
extern devclass_t bhnd_chipc_devclass;

struct chipc_region;

/**
 * Supported ChipCommon flash types.
 */
typedef enum {
	CHIPC_FLASH_NONE	= 0,	/**< No flash, or a type unrecognized
					     by the ChipCommon driver */
	CHIPC_PFLASH_CFI	= 1,	/**< CFI-compatible parallel flash */
	CHIPC_SFLASH_ST		= 2,	/**< ST serial flash */
	CHIPC_SFLASH_AT		= 3,	/**< Atmel serial flash */
	CHIPC_QSFLASH_ST	= 4,	/**< ST quad-SPI flash */ 
	CHIPC_QSFLASH_AT	= 5,	/**< Atmel quad-SPI flash */
	CHIPC_NFLASH		= 6,	/**< NAND flash */
	CHIPC_NFLASH_4706	= 7	/**< BCM4706 NAND flash */
} chipc_flash;

const char	*chipc_flash_name(chipc_flash type);
const char	*chipc_flash_bus_name(chipc_flash type);
const char	*chipc_sflash_device_name(chipc_flash type);

/**
 * ChipCommon capability flags;
 */
struct chipc_caps {
	uint8_t		num_uarts;	/**< Number of attached UARTS (1-3) */
	bool		mipseb;		/**< MIPS is big-endian */
	uint8_t		uart_clock;	/**< UART clock source (see CHIPC_CAP_UCLKSEL_*) */
	uint8_t		uart_gpio;	/**< UARTs own GPIO pins 12-15 */

	uint8_t		extbus_type;	/**< ExtBus type (CHIPC_CAP_EXTBUS_*) */

	chipc_flash 	flash_type;	/**< flash type */
	uint8_t		cfi_width;	/**< CFI bus width, 0 if unknown or CFI
					     not present */

	bhnd_nvram_src	nvram_src;	/**< identified NVRAM source */
	bus_size_t	sprom_offset;	/**< Offset to SPROM data within
					     SPROM/OTP, 0 if unknown or not
					     present */
	uint8_t		otp_size;	/**< OTP (row?) size, 0 if not present */

	uint8_t		pll_type;	/**< PLL type */
	bool		power_control;	/**< Power control available */
	bool		jtag_master;	/**< JTAG Master present */
	bool		boot_rom;	/**< Internal boot ROM is active */
	uint8_t		backplane_64;	/**< Backplane supports 64-bit addressing.
					     Note that this does not gaurantee
					     the CPU itself supports 64-bit
					     addressing. */
	bool		pmu;		/**< PMU is present. */
	bool		eci;		/**< ECI (enhanced coexistence inteface) is present. */
	bool		seci;		/**< SECI (serial ECI) is present */
	bool		sprom;		/**< SPROM is present */
	bool		gsio;		/**< GSIO (SPI/I2C) present */
	bool		aob;		/**< AOB (always on bus) present.
					     If set, PMU and GCI registers are
					     not accessible via ChipCommon,
					     and are instead accessible via
					     dedicated cores on the bhnd bus */
};

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
	 * The BCM4706 NAND flash interface is supported, along with the
	 * CHIPC_CAP_4706_NFLASH capability flag.
	 */
	CHIPC_QUIRK_4706_NFLASH			= (1<<2),

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
	CHIPC_QUIRK_4360_FEM_MUX_SPROM		= (1<<5) |
	    CHIPC_QUIRK_MUX_SPROM,

	/** Supports CHIPC_CAPABILITIES_EXT register */
	CHIPC_QUIRK_SUPPORTS_CAP_EXT		= (1<<6),

	/** Supports HND or IPX OTP registers (CHIPC_OTPST, CHIPC_OTPCTRL,
	 *  CHIPC_OTPPROG) */
	CHIPC_QUIRK_SUPPORTS_OTP		= (1<<7),

	/** Supports HND OTP registers. */
	CHIPC_QUIRK_OTP_HND			= (1<<8) |
	    CHIPC_QUIRK_SUPPORTS_OTP,

	/** Supports IPX OTP registers. */
	CHIPC_QUIRK_OTP_IPX			= (1<<9) |
	    CHIPC_QUIRK_SUPPORTS_OTP,

	/** OTP size is defined via CHIPC_OTPLAYOUT register in later
	 *  ChipCommon revisions using the 'IPX' OTP controller. */
	CHIPC_QUIRK_IPX_OTPL_SIZE		= (1<<10)
};

/**
 * chipc child device info.
 */
struct chipc_devinfo {
	struct resource_list	resources;	/**< child resources */
};

/**
 * chipc driver instance state.
 */
struct chipc_softc {
	device_t		dev;

	struct bhnd_resource	*core;		/**< core registers. */
	struct chipc_region	*core_region;	/**< region containing core registers */

	uint32_t		 quirks;	/**< chipc quirk flags */
	struct chipc_caps	 caps;		/**< chipc capabilities */

	struct mtx		 mtx;		/**< state mutex. */
	size_t			 sprom_refcnt;	/**< SPROM pin enable refcount */
	struct rman		 mem_rman;	/**< port memory manager */
	STAILQ_HEAD(, chipc_region) mem_regions;/**< memory allocation records */
};

#define	CHIPC_LOCK_INIT(sc) \
	mtx_init(&(sc)->mtx, device_get_nameunit((sc)->dev), \
	    "BHND chipc driver lock", MTX_DEF)
#define	CHIPC_LOCK(sc)				mtx_lock(&(sc)->mtx)
#define	CHIPC_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	CHIPC_LOCK_ASSERT(sc, what)		mtx_assert(&(sc)->mtx, what)
#define	CHIPC_LOCK_DESTROY(sc)			mtx_destroy(&(sc)->mtx)

#endif /* _BHND_CORES_CHIPC_CHIPCVAR_H_ */
