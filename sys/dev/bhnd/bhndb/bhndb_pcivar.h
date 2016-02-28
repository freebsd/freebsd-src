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

#ifndef _BHND_BHNDB_PCIVAR_H_
#define _BHND_BHNDB_PCIVAR_H_

#include <sys/stdint.h>

#include <dev/bhnd/cores/pci/bhnd_pcivar.h>

#include "bhndbvar.h"

/*
 * bhndb(4) PCI driver subclass.
 */

DECLARE_CLASS(bhndb_pci_driver);

struct bhndb_pci_softc;

/*
 * An interconnect-specific function implementing BHNDB_SET_WINDOW_ADDR
 */
typedef int (*bhndb_pci_set_regwin_t)(struct bhndb_pci_softc *sc,
	         const struct bhndb_regwin *rw, bhnd_addr_t addr);


/** 
 * PCI bridge core identification table.
 */
struct bhndb_pci_id {
	uint16_t			 device;	/**< bhnd device ID */
	bhnd_pci_regfmt_t		 regfmt;	/**< register format */
	struct bhnd_device_quirk	*quirks;	/**< quirks table */
};

struct bhndb_pci_softc {
	struct bhndb_softc	bhndb;		/**< parent softc */
	device_t		dev;		/**< bridge device */
	bhnd_devclass_t		pci_devclass;	/**< PCI core's devclass */
	bhndb_pci_set_regwin_t	set_regwin;	/**< regwin handler */

	/*
	 * Initialized in BHNDB_INIT_FULL_CONFIG()
	 */

	device_t		 mdio;		/**< PCIe MDIO device. NULL if not PCIe. */
	bhnd_pci_regfmt_t	 regfmt;	/**< device register format */

	struct resource		*mem_res;	/**< pci core's registers (borrowed reference) */
	bus_size_t		 mem_off;	/**< offset to the PCI core's registers within `mem_res` . */

	struct bhnd_resource	 bhnd_mem_res;	/**< bhnd resource representation of mem_res.
						     this is a simple 'direct' resource mapping */

	uint32_t		 quirks;	/**< BHNDB_PCI(E)_QUIRK flags */

	/**
	 * Driver state specific to BHNDB_PCIE_QUIRK_SDR9_POLARITY.
	 */
	struct {
		/** 
		 * PCIe SerDes RX polarity.
		 *
		 * Initialized to the PCIe link's RX polarity
		 * at attach time. This is used to restore the
		 * correct polarity on resume */
		bool	inv;
	} sdr9_quirk_polarity;
};

/* Declare a bhndb_pci_id entry */
#define	BHNDB_PCI_ID(_device, _desc, ...)	{	\
	BHND_COREID_ ## _device, 			\
	BHND_PCI_REGFMT_ ## _device,			\
	(struct bhnd_device_quirk[]) {			\
		__VA_ARGS__				\
	}						\
}

/* 
 * PCI/PCIe-Gen1 endpoint-mode device quirks
 */
enum {
	/** No quirks */
	BHNDB_PCI_QUIRK_NONE			= 0,

	/**
	 * BCM4306 chips (and possibly others) do not support the idle
	 * low-power clock. Clocking must be bootstrapped at attach/resume by
	 * directly adjusting GPIO registers exposed in the PCI config space,
	 * and correspondingly, explicitly shutdown at detach/suspend.
	 */
	BHNDB_PCI_QUIRK_EXT_CLOCK_GATING	= (1<<1),

	/**
	 * SBTOPCI_PREF and SBTOPCI_BURST must be set on the
	 * SSB_PCICORE_SBTOPCI2 register.
	 */
	BHNDB_PCI_QUIRK_SBTOPCI2_PREF_BURST	= (1<<2),

	/**
	 * SBTOPCI_RC_READMULTI must be set on the SSB_PCICORE_SBTOPCI2
	 * register.
	 */
	BHNDB_PCI_QUIRK_SBTOPCI2_READMULTI	= (1<<3),

	/**
	 * Interrupt masking is handled via the interconnect configuration
	 * registers (SBINTVEC on siba), rather than the PCI_INT_MASK
	 * config register.
	 */
	BHNDB_PCI_QUIRK_SBINTVEC		= (1<<4),

	/**
	 * PCI CLKRUN# should be disabled on attach (via CLKRUN_DSBL).
	 * 
	 * The purpose of this work-around is unclear; there is some
	 * documentation regarding earlier Broadcom drivers supporting
	 * a "force CLKRUN#" *enable* registry key for use on mobile
	 * hardware.
	 */
	BHNDB_PCI_QUIRK_CLKRUN_DSBL		= (1<<5),

	/**
	 * TLP workaround for unmatched address handling is required.
	 * 
	 * This TLP workaround will enable setting of the PCIe UR status bit
	 * on memory access to an unmatched address.
	 */
	BHNDB_PCIE_QUIRK_UR_STATUS_FIX		= (1<<6),

	/**
	 * PCI-PM power management must be explicitly enabled via
	 * the data link control register.
	 */
	BHNDB_PCIE_QUIRK_PCIPM_REQEN		= (1<<7),

	/**
	 * Fix L0s to L0 exit transition on SerDes <= rev9 devices.
	 * 
	 * On these devices, PCIe/SerDes symbol lock can be lost if the
	 * reference clock has not fully stabilized during the L0s to L0
	 * exit transition, triggering an internal reset of the chip.
	 * 
	 * The SerDes RX CDR phase lock timers and proportional/integral
	 * filters must be tweaked to ensure the CDR has fully stabilized
	 * before asserting receive sequencer completion.
	 */
	BHNDB_PCIE_QUIRK_SDR9_L0s_HANG		= (1<<8),

	/**
	 * The idle time for entering L1 low-power state must be
	 * explicitly set (to 114ns) to fix slow L1->L0 transition issues.
	 */
	BHNDB_PCIE_QUIRK_L1_IDLE_THRESH		= (1<<9),
	
	/**
	 * The ASPM L1 entry timer should be extended for better performance,
	 * and restored for better power savings.
	 */
	BHNDB_PCIE_QUIRK_L1_TIMER_PERF		= (1<<10),

	/**
	 * ASPM and ECPM settings must be overridden manually.
	 * 
	 * The override behavior is controlled by the BHND_BFL2_PCIEWAR_OVR
	 * flag. If this flag is set, ASPM/CLKREQ should be overridden as
	 * enabled; otherwise, they should be overridden as disabled.
	 * 
	 * Attach/Resume:
	 *   - Set SRSH_ASPM_ENB flag in the SPROM ASPM register.
	 *   - Set ASPM L0S/L1 in the PCIER_LINK_CTL register.
	 *   - Set SRSH_CLKREQ_ENB flag in the SPROM CLKREQ_REV5 register.
	 *   - Clear ECPM in the PCIER_LINK_CTL register.
	 * 
	 * Detach/Suspend:
	 * - 
	 * - When the device enters D3 state, or system enters S3/S4 state,
	 *   clear ASPM L1 in the PCIER_LINK_CTL register.
	 */
	BHNDB_PCIE_QUIRK_ASPM_OVR		= (1<<11),
	
	/**
	 * Fix SerDes polarity on SerDes <= rev9 devices.
	 *
	 * The SerDes polarity must be saved at device attachment, and
	 * restored on suspend/resume.
	 */
	BHNDB_PCIE_QUIRK_SDR9_POLARITY		= (1<<12),

	/**
	 * The SerDes PLL override flag (CHIPCTRL_4321_PLL_DOWN) must be set on
	 * the ChipCommon core on resume.
	 */
	BHNDB_PCIE_QUIRK_SERDES_NOPLLDOWN	= (1<<13),

        /**
	 * On attach and resume, consult the SPROM to determine whether
	 * the L2/L3-Ready w/o PCI RESET work-around must be applied.
	 *
	 * If L23READY_EXIT_NOPRST is not already set in the SPROM, set it
	 */
	BHNDB_PCIE_QUIRK_SPROM_L23_PCI_RESET	= (1<<14),
	
	/**
	 * The PCIe SerDes supports non-standard extended MDIO register access.
	 * 
	 * The PCIe SerDes supports access to extended MDIO registers via
	 * a non-standard Clause 22 address extension mechanism.
	 */
	BHNDB_PCIE_QUIRK_SD_C22_EXTADDR		= (1<<15),
	
	/**
	 * The PCIe SerDes PLL must be configured to not retry the startup
	 * sequence upon frequency detection failure on SerDes <= rev9 devices
	 * 
	 * The issue this workaround resolves has not be determined.
	 */
	BHNDB_PCIE_QUIRK_SDR9_NO_FREQRETRY	= (1<<16),
};

#endif /* _BHND_BHNDB_PCIVAR_H_ */