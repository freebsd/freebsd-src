/*-
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Marvel Xenon SDHCI driver defines.
 */

#ifndef	_SDHCI_XENON_H_
#define	_SDHCI_XENON_H_

#define	XENON_LOWEST_SDCLK_FREQ			100000
#define	XENON_MMC_MAX_CLK			400000000

#define	XENON_SYS_OP_CTRL			0x0108
#define	 XENON_AUTO_CLKGATE_DISABLE		(1 << 20)
#define	 XENON_SDCLK_IDLEOFF_ENABLE_SHIFT	8

#define	XENON_SYS_EXT_OP_CTRL			0x010C
#define	 XENON_MASK_CMD_CONFLICT_ERR		(1 << 8)

#define	XENON_SLOT_EMMC_CTRL			0x0130
#define	 XENON_ENABLE_DATA_STROBE		(1 << 24)
#define	 XENON_ENABLE_RESP_STROBE		(1 << 25)

/* Custom HS200 / HS400 Mode Select values in SDHCI_HOST_CONTROL2 register. */
#define	XENON_CTRL2_MMC_HS200			0x5
#define	XENON_CTRL2_MMC_HS400			0x6

/* eMMC PHY */
#define	XENON_EMMC_PHY_REG_BASE			0x170

#define	XENON_EMMC_PHY_TIMING_ADJUST		XENON_EMMC_PHY_REG_BASE
#define	 XENON_SAMPL_INV_QSP_PHASE_SELECT	(1 << 18)
#define	 XENON_TIMING_ADJUST_SDIO_MODE		(1 << 28)
#define	 XENON_TIMING_ADJUST_SLOW_MODE		(1 << 29)
#define	 XENON_PHY_INITIALIZATION		(1U << 31)
#define	 XENON_WAIT_CYCLE_BEFORE_USING_MASK	0xF
#define	 XENON_WAIT_CYCLE_BEFORE_USING_SHIFT	12
#define	 XENON_FC_SYNC_EN_DURATION_MASK		0xF
#define	 XENON_FC_SYNC_EN_DURATION_SHIFT	8
#define	 XENON_FC_SYNC_RST_EN_DURATION_MASK	0xF
#define	 XENON_FC_SYNC_RST_EN_DURATION_SHIFT	4
#define	 XENON_FC_SYNC_RST_DURATION_MASK	0xF
#define	 XENON_FC_SYNC_RST_DURATION_SHIFT	0

#define	XENON_EMMC_PHY_FUNC_CONTROL		(XENON_EMMC_PHY_REG_BASE + 0x4)
#define	 XENON_DQ_ASYNC_MODE			(1 << 4)
#define	 XENON_CMD_DDR_MODE			(1 << 16)
#define	 XENON_DQ_DDR_MODE_SHIFT		8
#define	 XENON_DQ_DDR_MODE_MASK			0xFF
#define	 XENON_ASYNC_DDRMODE_MASK		(1 << 23)
#define	 XENON_ASYNC_DDRMODE_SHIFT		23

#define	XENON_EMMC_PHY_PAD_CONTROL		(XENON_EMMC_PHY_REG_BASE + 0x8)
#define	 XENON_FC_DQ_RECEN			(1 << 24)
#define	 XENON_FC_CMD_RECEN			(1 << 25)
#define	 XENON_FC_QSP_RECEN			(1 << 26)
#define	 XENON_FC_QSN_RECEN			(1 << 27)
#define	 XENON_OEN_QSN				(1 << 28)
#define	 XENON_FC_ALL_CMOS_RECEIVER		0xF000

#define	XENON_EMMC_PHY_PAD_CONTROL1		(XENON_EMMC_PHY_REG_BASE + 0xC)
#define	 XENON_EMMC_FC_CMD_PD			(1 << 8)
#define	 XENON_EMMC_FC_QSP_PD			(1 << 9)
#define	 XENON_EMMC_FC_CMD_PU			(1 << 24)
#define	 XENON_EMMC_FC_QSP_PU			(1 << 25)
#define	 XENON_EMMC_FC_DQ_PD			0xFF
#define	 XENON_EMMC_FC_DQ_PU			(0xFF << 16)

#define	XENON_EMMC_PHY_PAD_CONTROL2		(XENON_EMMC_PHY_REG_BASE + 0x10)
#define	 XENON_ZNR_MASK				0x1F
#define	 XENON_ZNR_SHIFT			8
#define	 XENON_ZPR_MASK				0x1F
#define	 XENON_ZNR_DEF_VALUE			0xF
#define	 XENON_ZPR_DEF_VALUE			0xF

#define	XENON_EMMC_PHY_LOGIC_TIMING_ADJUST	(XENON_EMMC_PHY_REG_BASE + 0x18)
#define	 XENON_LOGIC_TIMING_VALUE		0x00AA8977

DECLARE_CLASS(sdhci_xenon_driver);

struct sdhci_xenon_softc {
	device_t	dev;		/* Controller device */
	int		slot_id;	/* Controller ID */

	struct resource	*mem_res;	/* Memory resource */
	struct resource *irq_res;	/* IRQ resource */
	void		*intrhand;	/* Interrupt handle */

	struct sdhci_slot *slot;	/* SDHCI internal data */

	uint8_t		znr;		/* PHY ZNR */
	uint8_t		zpr;		/* PHY ZPR */
	bool		slow_mode;	/* PHY slow mode */
	bool		wp_inverted;
	bool		skip_regulators; /* Don't switch regulators */

	regulator_t	vmmc_supply;
	regulator_t	vqmmc_supply;

#ifdef FDT
	struct sdhci_fdt_gpio *gpio;	/* GPIO pins for CD detection. */
#endif
};

device_attach_t sdhci_xenon_attach;
device_detach_t sdhci_xenon_detach;

#endif	/* _SDHCI_XENON_H_ */
