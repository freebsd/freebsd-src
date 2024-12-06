/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 - 2021 Alstom Group.
 * Copyright (c) 2020 - 2021 Semihalf.
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
 */

/* eSDHC controller driver for NXP QorIQ Layerscape SoCs. */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/syscon/syscon.h>
#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmc_fdt_helpers.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt_gpio.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"
#include "syscon_if.h"

#define	RD4	(sc->read)
#define	WR4	(sc->write)

#define	SDHCI_FSL_PRES_STATE		0x24
#define	SDHCI_FSL_PRES_SDSTB		(1 << 3)
#define	SDHCI_FSL_PRES_COMPAT_MASK	0x000f0f07

#define	SDHCI_FSL_PROT_CTRL		0x28
#define	SDHCI_FSL_PROT_CTRL_WIDTH_1BIT	(0 << 1)
#define	SDHCI_FSL_PROT_CTRL_WIDTH_4BIT	(1 << 1)
#define	SDHCI_FSL_PROT_CTRL_WIDTH_8BIT	(2 << 1)
#define	SDHCI_FSL_PROT_CTRL_WIDTH_MASK	(3 << 1)
#define	SDHCI_FSL_PROT_CTRL_BYTE_SWAP	(0 << 4)
#define	SDHCI_FSL_PROT_CTRL_BYTE_NATIVE	(2 << 4)
#define	SDHCI_FSL_PROT_CTRL_BYTE_MASK	(3 << 4)
#define	SDHCI_FSL_PROT_CTRL_DMA_MASK	(3 << 8)
#define	SDHCI_FSL_PROT_CTRL_VOLT_SEL	(1 << 10)

#define SDHCI_FSL_IRQSTAT		0x30
#define SDHCI_FSL_IRQSTAT_BRR		(1 << 5)
#define SDHCI_FSL_IRQSTAT_CINTSEN	(1 << 8)
#define SDHCI_FSL_IRQSTAT_RTE		(1 << 12)
#define SDHCI_FSL_IRQSTAT_TNE		(1 << 26)

#define	SDHCI_FSL_SYS_CTRL		0x2c
#define	SDHCI_FSL_CLK_IPGEN		(1 << 0)
#define	SDHCI_FSL_CLK_SDCLKEN		(1 << 3)
#define	SDHCI_FSL_CLK_DIVIDER_MASK	0x000000f0
#define	SDHCI_FSL_CLK_DIVIDER_SHIFT	4
#define	SDHCI_FSL_CLK_PRESCALE_MASK	0x0000ff00
#define	SDHCI_FSL_CLK_PRESCALE_SHIFT	8

#define	SDHCI_FSL_WTMK_LVL		0x44
#define	SDHCI_FSL_WTMK_RD_512B		(0 << 0)
#define	SDHCI_FSL_WTMK_WR_512B		(0 << 15)

#define SDHCI_FSL_AUTOCERR		0x3C
#define SDHCI_FSL_AUTOCERR_UHMS_HS200	(3 << 16)
#define SDHCI_FSL_AUTOCERR_UHMS		(7 << 16)
#define SDHCI_FSL_AUTOCERR_EXTN		(1 << 22)
#define SDHCI_FSL_AUTOCERR_SMPCLKSEL	(1 << 23)
#define SDHCI_FSL_AUTOCERR_UHMS_SHIFT	16

#define	SDHCI_FSL_HOST_VERSION		0xfc
#define	SDHCI_FSL_VENDOR_V23		0x13

#define	SDHCI_FSL_CAPABILITIES2		0x114

#define	SDHCI_FSL_TBCTL			0x120

#define SDHCI_FSL_TBSTAT		0x124
#define	SDHCI_FSL_TBCTL_TBEN		(1 << 2)
#define SDHCI_FSL_TBCTL_HS400_EN	(1 << 4)
#define SDHCI_FSL_TBCTL_SAMP_CMD_DQS	(1 << 5)
#define SDHCI_FSL_TBCTL_HS400_WND_ADJ	(1 << 6)
#define SDHCI_FSL_TBCTL_TB_MODE_MASK	0x3
#define SDHCI_FSL_TBCTL_MODE_1		0
#define SDHCI_FSL_TBCTL_MODE_2		1
#define SDHCI_FSL_TBCTL_MODE_3		2
#define SDHCI_FSL_TBCTL_MODE_SW		3

#define SDHCI_FSL_TBPTR			0x128
#define SDHCI_FSL_TBPTR_WND_START_SHIFT 8
#define SDHCI_FSL_TBPTR_WND_MASK	0x7F

#define SDHCI_FSL_SDCLKCTL		0x144
#define SDHCI_FSL_SDCLKCTL_CMD_CLK_CTL	(1 << 15)
#define SDHCI_FSL_SDCLKCTL_LPBK_CLK_SEL	(1 << 31)

#define SDHCI_FSL_SDTIMINGCTL		0x148
#define SDHCI_FSL_SDTIMINGCTL_FLW_CTL	(1 << 15)

#define SDHCI_FSL_DLLCFG0		0x160
#define SDHCI_FSL_DLLCFG0_FREQ_SEL	(1 << 27)
#define SDHCI_FSL_DLLCFG0_RESET		(1 << 30)
#define SDHCI_FSL_DLLCFG0_EN		(1 << 31)

#define SDHCI_FSL_DLLCFG1		0x164
#define SDHCI_FSL_DLLCFG1_PULSE_STRETCH	(1 << 31)

#define SDHCI_FSL_DLLSTAT0		0x170
#define SDHCI_FSL_DLLSTAT0_SLV_STS	(1 << 27)

#define	SDHCI_FSL_ESDHC_CTRL		0x40c
#define	SDHCI_FSL_ESDHC_CTRL_SNOOP	(1 << 6)
#define SDHCI_FSL_ESDHC_CTRL_FAF	(1 << 18)
#define	SDHCI_FSL_ESDHC_CTRL_CLK_DIV2	(1 << 19)

#define SCFG_SDHCIOVSELCR		0x408
#define SCFG_SDHCIOVSELCR_TGLEN		(1 << 0)
#define SCFG_SDHCIOVSELCR_VS		(1 << 31)
#define SCFG_SDHCIOVSELCR_VSELVAL_MASK	(3 << 1)
#define SCFG_SDHCIOVSELCR_VSELVAL_1_8	0x0
#define SCFG_SDHCIOVSELCR_VSELVAL_3_3	0x2

#define SDHCI_FSL_CAN_VDD_MASK		\
    (SDHCI_CAN_VDD_180 | SDHCI_CAN_VDD_300 | SDHCI_CAN_VDD_330)

/* Some platforms do not detect pulse width correctly. */
#define SDHCI_FSL_UNRELIABLE_PULSE_DET	(1 << 0)
/* On some platforms switching voltage to 1.8V is not supported */
#define SDHCI_FSL_UNSUPP_1_8V		(1 << 1)
/* Hardware tuning can fail, fallback to SW tuning in that case. */
#define SDHCI_FSL_TUNING_ERRATUM_TYPE1	(1 << 2)
/*
 * Pointer window might not be set properly on some platforms.
 * Check window and perform SW tuning.
 */
#define SDHCI_FSL_TUNING_ERRATUM_TYPE2	(1 << 3)
/*
 * In HS400 mode only 4, 8, 12 clock dividers can be used.
 * Use the smallest value, bigger than requested in that case.
 */
#define SDHCI_FSL_HS400_LIMITED_CLK_DIV	(1 << 4)

/*
 * Some SoCs don't have a fixed regulator. Switching voltage
 * requires special routine including syscon registers.
 */
#define SDHCI_FSL_MISSING_VCCQ_REG	(1 << 5)

/*
 * HS400 tuning is done in HS200 mode, but it has to be done using
 * the target frequency. In order to apply the errata above we need to
 * know the target mode during tuning procedure. Use this flag for just that.
 */
#define SDHCI_FSL_HS400_FLAG		(1 << 0)

#define SDHCI_FSL_MAX_RETRIES		20000	/* DELAY(10) * this = 200ms */

struct sdhci_fsl_fdt_softc {
	device_t				dev;
	const struct sdhci_fsl_fdt_soc_data	*soc_data;
	struct resource				*mem_res;
	struct resource				*irq_res;
	void					*irq_cookie;
	uint32_t				baseclk_hz;
	uint32_t				maxclk_hz;
	struct sdhci_fdt_gpio			*gpio;
	struct sdhci_slot			slot;
	bool					slot_init_done;
	uint32_t				cmd_and_mode;
	uint16_t				sdclk_bits;
	struct mmc_helper			fdt_helper;
	uint32_t				div_ratio;
	uint8_t					vendor_ver;
	uint32_t				flags;

	uint32_t (* read)(struct sdhci_fsl_fdt_softc *, bus_size_t);
	void (* write)(struct sdhci_fsl_fdt_softc *, bus_size_t, uint32_t);
};

struct sdhci_fsl_fdt_soc_data {
	int quirks;
	int baseclk_div;
	uint8_t errata;
	char *syscon_compat;
};

static const struct sdhci_fsl_fdt_soc_data sdhci_fsl_fdt_ls1012a_soc_data = {
	.quirks = 0,
	.baseclk_div = 1,
	.errata = SDHCI_FSL_MISSING_VCCQ_REG | SDHCI_FSL_TUNING_ERRATUM_TYPE2,
	.syscon_compat = "fsl,ls1012a-scfg",
};

static const struct sdhci_fsl_fdt_soc_data sdhci_fsl_fdt_ls1028a_soc_data = {
	.quirks = SDHCI_QUIRK_DONT_SET_HISPD_BIT |
	    SDHCI_QUIRK_BROKEN_AUTO_STOP | SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
	.baseclk_div = 2,
	.errata = SDHCI_FSL_UNRELIABLE_PULSE_DET |
	    SDHCI_FSL_HS400_LIMITED_CLK_DIV,
};

static const struct sdhci_fsl_fdt_soc_data sdhci_fsl_fdt_ls1046a_soc_data = {
	.quirks = SDHCI_QUIRK_DONT_SET_HISPD_BIT | SDHCI_QUIRK_BROKEN_AUTO_STOP,
	.baseclk_div = 2,
	.errata = SDHCI_FSL_MISSING_VCCQ_REG | SDHCI_FSL_TUNING_ERRATUM_TYPE2,
	.syscon_compat = "fsl,ls1046a-scfg",
};

static const struct sdhci_fsl_fdt_soc_data sdhci_fsl_fdt_lx2160a_soc_data = {
	.quirks = 0,
	.baseclk_div = 2,
	.errata = SDHCI_FSL_UNRELIABLE_PULSE_DET |
	    SDHCI_FSL_HS400_LIMITED_CLK_DIV,
};

static const struct sdhci_fsl_fdt_soc_data sdhci_fsl_fdt_gen_data = {
	.quirks = 0,
	.baseclk_div = 1,
};

static const struct ofw_compat_data sdhci_fsl_fdt_compat_data[] = {
	{"fsl,ls1012a-esdhc",	(uintptr_t)&sdhci_fsl_fdt_ls1012a_soc_data},
	{"fsl,ls1028a-esdhc",	(uintptr_t)&sdhci_fsl_fdt_ls1028a_soc_data},
	{"fsl,ls1046a-esdhc",	(uintptr_t)&sdhci_fsl_fdt_ls1046a_soc_data},
	{"fsl,esdhc",		(uintptr_t)&sdhci_fsl_fdt_gen_data},
	{NULL,			0}
};

static uint32_t
read_be(struct sdhci_fsl_fdt_softc *sc, bus_size_t off)
{

	return (be32toh(bus_read_4(sc->mem_res, off)));
}

static void
write_be(struct sdhci_fsl_fdt_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, htobe32(val));
}

static uint32_t
read_le(struct sdhci_fsl_fdt_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static void
write_le(struct sdhci_fsl_fdt_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}


static uint16_t
sdhci_fsl_fdt_get_clock(struct sdhci_fsl_fdt_softc *sc)
{
	uint16_t val;

	val = sc->sdclk_bits | SDHCI_CLOCK_INT_EN;
	if (RD4(sc, SDHCI_FSL_PRES_STATE) & SDHCI_FSL_PRES_SDSTB)
		val |= SDHCI_CLOCK_INT_STABLE;
	if (RD4(sc, SDHCI_FSL_SYS_CTRL) & SDHCI_FSL_CLK_SDCLKEN)
		val |= SDHCI_CLOCK_CARD_EN;

	return (val);
}

/*
 * Calculate clock prescaler and divisor values based on the following formula:
 * `frequency = base clock / (prescaler * divisor)`.
 */
#define	SDHCI_FSL_FDT_CLK_DIV(sc, base, freq, pre, div)			\
	do {								\
		(pre) = (sc)->vendor_ver < SDHCI_FSL_VENDOR_V23 ? 2 : 1;\
		while ((freq) < (base) / ((pre) * 16) && (pre) < 256)	\
			(pre) <<= 1;					\
		/* div/pre can't both be set to 1, according to PM. */	\
		(div) = ((pre) == 1 ? 2 : 1);				\
		while ((freq) < (base) / ((pre) * (div)) && (div) < 16)	\
			++(div);					\
	} while (0)

static void
fsl_sdhc_fdt_set_clock(struct sdhci_fsl_fdt_softc *sc, struct sdhci_slot *slot,
    uint16_t val)
{
	uint32_t prescale, div, val32, div_ratio;

	sc->sdclk_bits = val & SDHCI_DIVIDERS_MASK;
	val32 = RD4(sc, SDHCI_CLOCK_CONTROL);

	if ((val & SDHCI_CLOCK_CARD_EN) == 0) {
		WR4(sc, SDHCI_CLOCK_CONTROL, val32 & ~SDHCI_FSL_CLK_SDCLKEN);
		return;
	}

	/*
	 * Ignore dividers provided by core in `sdhci_set_clock` and calculate
	 * them anew with higher accuracy.
	 */
	SDHCI_FSL_FDT_CLK_DIV(sc, sc->baseclk_hz, slot->clock, prescale, div);

	div_ratio = prescale * div;

	/*
	 * According to limited clock division erratum, clock dividers in hs400
	 * can be only 4, 8 or 12
	 */
	if ((sc->soc_data->errata & SDHCI_FSL_HS400_LIMITED_CLK_DIV) &&
	    (sc->slot.host.ios.timing == bus_timing_mmc_hs400 ||
	     (sc->flags & SDHCI_FSL_HS400_FLAG))) {
		if (div_ratio <= 4) {
			prescale = 4;
			div = 1;
		} else if (div_ratio <= 8) {
			prescale = 4;
			div = 2;
		} else if (div_ratio <= 12) {
			prescale = 4;
			div = 3;
		} else {
			device_printf(sc->dev, "Unsupported clock divider.\n");
		}
	}

	sc->div_ratio = prescale * div;
	if (bootverbose)
		device_printf(sc->dev,
		    "Desired SD/MMC freq: %d, actual: %d; base %d prescale %d divisor %d\n",
		    slot->clock, sc->baseclk_hz / (prescale * div),
		    sc->baseclk_hz, prescale, div);

	prescale >>= 1;
	div -= 1;

	val32 &= ~(SDHCI_FSL_CLK_DIVIDER_MASK | SDHCI_FSL_CLK_PRESCALE_MASK);
	val32 |= div << SDHCI_FSL_CLK_DIVIDER_SHIFT;
	val32 |= prescale << SDHCI_FSL_CLK_PRESCALE_SHIFT;
	val32 |= SDHCI_FSL_CLK_IPGEN | SDHCI_FSL_CLK_SDCLKEN;
	WR4(sc, SDHCI_CLOCK_CONTROL, val32);
}

static uint8_t
sdhci_fsl_fdt_read_1(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t wrk32, val32;

	sc = device_get_softc(dev);

	switch (off) {
	case SDHCI_HOST_CONTROL:
		wrk32 = RD4(sc, SDHCI_FSL_PROT_CTRL);
		val32 = wrk32 & (SDHCI_CTRL_LED | SDHCI_CTRL_CARD_DET |
		    SDHCI_CTRL_FORCE_CARD);
		if (wrk32 & SDHCI_FSL_PROT_CTRL_WIDTH_4BIT)
			val32 |= SDHCI_CTRL_4BITBUS;
		else if (wrk32 & SDHCI_FSL_PROT_CTRL_WIDTH_8BIT)
			val32 |= SDHCI_CTRL_8BITBUS;
		return (val32);
	case SDHCI_POWER_CONTROL:
		return (SDHCI_POWER_ON | SDHCI_POWER_300);
	default:
		break;
	}

	return ((RD4(sc, off & ~3) >> (off & 3) * 8) & UINT8_MAX);
}

static uint16_t
sdhci_fsl_fdt_read_2(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t val32;

	sc = device_get_softc(dev);

	switch (off) {
	case SDHCI_CLOCK_CONTROL:
		return (sdhci_fsl_fdt_get_clock(sc));
	case SDHCI_HOST_VERSION:
		return (RD4(sc, SDHCI_FSL_HOST_VERSION) & UINT16_MAX);
	case SDHCI_TRANSFER_MODE:
		return (sc->cmd_and_mode & UINT16_MAX);
	case SDHCI_COMMAND_FLAGS:
		return (sc->cmd_and_mode >> 16);
	case SDHCI_SLOT_INT_STATUS:
	/*
	 * eSDHC hardware manages only a single slot.
	 * Synthesize a slot interrupt status register for slot 1 below.
	 */
		val32 = RD4(sc, SDHCI_INT_STATUS);
		val32 &= RD4(sc, SDHCI_SIGNAL_ENABLE);
		return (!!val32);
	default:
		return ((RD4(sc, off & ~3) >> (off & 3) * 8) & UINT16_MAX);
	}
}

static uint32_t
sdhci_fsl_fdt_read_4(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t wrk32, val32;

	sc = device_get_softc(dev);

	if (off == SDHCI_BUFFER)
		return (bus_read_4(sc->mem_res, off));
	if (off == SDHCI_CAPABILITIES2)
		off = SDHCI_FSL_CAPABILITIES2;

	val32 = RD4(sc, off);

	if (off == SDHCI_PRESENT_STATE) {
		wrk32 = val32;
		val32 &= SDHCI_FSL_PRES_COMPAT_MASK;
		val32 |= (wrk32 >> 4) & SDHCI_STATE_DAT_MASK;
		val32 |= (wrk32 << 1) & SDHCI_STATE_CMD;
	}

	return (val32);
}

static void
sdhci_fsl_fdt_read_multi_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t *data, bus_size_t count)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = device_get_softc(dev);
	bus_read_multi_4(sc->mem_res, off, data, count);
}

static void
sdhci_fsl_fdt_write_1(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint8_t val)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t val32;

	sc = device_get_softc(dev);

	switch (off) {
	case SDHCI_HOST_CONTROL:
		val32 = RD4(sc, SDHCI_FSL_PROT_CTRL);
		val32 &= ~SDHCI_FSL_PROT_CTRL_WIDTH_MASK;
		val32 |= (val & SDHCI_CTRL_LED);

		if (val & SDHCI_CTRL_8BITBUS)
			val32 |= SDHCI_FSL_PROT_CTRL_WIDTH_8BIT;
		else
			/* Bus width is 1-bit when this flag is not set. */
			val32 |= (val & SDHCI_CTRL_4BITBUS);
		/* Enable SDMA by masking out this field. */
		val32 &= ~SDHCI_FSL_PROT_CTRL_DMA_MASK;
		val32 &= ~(SDHCI_CTRL_CARD_DET | SDHCI_CTRL_FORCE_CARD);
		val32 |= (val & (SDHCI_CTRL_CARD_DET |
		    SDHCI_CTRL_FORCE_CARD));
		WR4(sc, SDHCI_FSL_PROT_CTRL, val32);
		return;
	case SDHCI_POWER_CONTROL:
		return;
	default:
		val32 = RD4(sc, off & ~3);
		val32 &= ~(UINT8_MAX << (off & 3) * 8);
		val32 |= (val << (off & 3) * 8);
		WR4(sc, off & ~3, val32);
		return;
	}
}

static void
sdhci_fsl_fdt_write_2(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint16_t val)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t val32;

	sc = device_get_softc(dev);

	switch (off) {
	case SDHCI_CLOCK_CONTROL:
		fsl_sdhc_fdt_set_clock(sc, slot, val);
		return;
	/*
	 * eSDHC hardware combines command and mode into a single
	 * register. Cache it here, so that command isn't written
	 * until after mode.
	 */
	case SDHCI_TRANSFER_MODE:
		sc->cmd_and_mode = val;
		return;
	case SDHCI_COMMAND_FLAGS:
		sc->cmd_and_mode =
		    (sc->cmd_and_mode & UINT16_MAX) | (val << 16);
		WR4(sc, SDHCI_TRANSFER_MODE, sc->cmd_and_mode);
		sc->cmd_and_mode = 0;
		return;
	case SDHCI_HOST_CONTROL2:
		/*
		 * Switching to HS400 requires a special procedure,
		 * which is done in sdhci_fsl_fdt_set_uhs_timing.
		 */
		if ((val & SDHCI_CTRL2_UHS_MASK) == SDHCI_CTRL2_MMC_HS400)
			val &= ~SDHCI_CTRL2_MMC_HS400;
	default:
		val32 = RD4(sc, off & ~3);
		val32 &= ~(UINT16_MAX << (off & 3) * 8);
		val32 |= ((val & UINT16_MAX) << (off & 3) * 8);
		WR4(sc, off & ~3, val32);
		return;
	}
}

static void
sdhci_fsl_fdt_write_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t val)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = device_get_softc(dev);

	switch (off) {
	case SDHCI_BUFFER:
		bus_write_4(sc->mem_res, off, val);
		return;
	/*
	 * eSDHC hardware lacks support for the SDMA buffer boundary
	 * feature and instead generates SDHCI_INT_DMA_END interrupts
	 * after each completed DMA data transfer.
	 * Since this duplicates the SDHCI_INT_DATA_END functionality,
	 * mask out the unneeded SDHCI_INT_DMA_END interrupt.
	 */
	case SDHCI_INT_ENABLE:
	case SDHCI_SIGNAL_ENABLE:
		val &= ~SDHCI_INT_DMA_END;
	/* FALLTHROUGH. */
	default:
		WR4(sc, off, val);
		return;
	}
}

static void
sdhci_fsl_fdt_write_multi_4(device_t dev, struct sdhci_slot *slot,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = device_get_softc(dev);
	bus_write_multi_4(sc->mem_res, off, data, count);
}

static void
sdhci_fsl_fdt_irq(void *arg)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = arg;
	sdhci_generic_intr(&sc->slot);
	return;
}

static int
sdhci_fsl_fdt_update_ios(device_t brdev, device_t reqdev)
{
	int err;
	struct sdhci_fsl_fdt_softc *sc;
	struct mmc_ios *ios;
	struct sdhci_slot *slot;

	err = sdhci_generic_update_ios(brdev, reqdev);
	if (err != 0)
		return (err);

	sc = device_get_softc(brdev);
	slot = device_get_ivars(reqdev);
	ios = &slot->host.ios;

	switch (ios->power_mode) {
	case power_on:
		break;
	case power_off:
		if (bootverbose)
			device_printf(sc->dev, "Powering down sd/mmc\n");

		if (sc->fdt_helper.vmmc_supply)
			regulator_disable(sc->fdt_helper.vmmc_supply);
		if (sc->fdt_helper.vqmmc_supply)
			regulator_disable(sc->fdt_helper.vqmmc_supply);
		break;
	case power_up:
		if (bootverbose)
			device_printf(sc->dev, "Powering up sd/mmc\n");

		if (sc->fdt_helper.vmmc_supply)
			regulator_enable(sc->fdt_helper.vmmc_supply);
		if (sc->fdt_helper.vqmmc_supply)
			regulator_enable(sc->fdt_helper.vqmmc_supply);
		break;
	};

	return (0);
}

static int
sdhci_fsl_fdt_switch_syscon_voltage(device_t dev,
    struct sdhci_fsl_fdt_softc *sc, enum mmc_vccq vccq)
{
	struct syscon *syscon;
	phandle_t syscon_node;
	uint32_t reg;

	if (sc->soc_data->syscon_compat == NULL) {
		device_printf(dev, "Empty syscon compat string.\n");
		return (ENXIO);
	}

	syscon_node = ofw_bus_find_compatible(OF_finddevice("/"),
	    sc->soc_data->syscon_compat);

	if (syscon_get_by_ofw_node(dev, syscon_node, &syscon) != 0) {
		device_printf(dev, "Could not find syscon node.\n");
		return (ENXIO);
	}

	reg = SYSCON_READ_4(syscon, SCFG_SDHCIOVSELCR);
	reg &= ~SCFG_SDHCIOVSELCR_VSELVAL_MASK;
	reg |= SCFG_SDHCIOVSELCR_TGLEN;

	switch (vccq) {
	case vccq_180:
		reg |= SCFG_SDHCIOVSELCR_VSELVAL_1_8;
		SYSCON_WRITE_4(syscon, SCFG_SDHCIOVSELCR, reg);

		DELAY(5000);

		reg = SYSCON_READ_4(syscon, SCFG_SDHCIOVSELCR);
		reg |= SCFG_SDHCIOVSELCR_VS;
		break;
	case vccq_330:
		reg |= SCFG_SDHCIOVSELCR_VSELVAL_3_3;
		SYSCON_WRITE_4(syscon, SCFG_SDHCIOVSELCR, reg);

		DELAY(5000);

		reg = SYSCON_READ_4(syscon, SCFG_SDHCIOVSELCR);
		reg &= ~SCFG_SDHCIOVSELCR_VS;
		break;
	default:
		device_printf(dev, "Unsupported voltage requested.\n");
		return (ENXIO);
	}

	SYSCON_WRITE_4(syscon, SCFG_SDHCIOVSELCR, reg);

	return (0);
}

static int
sdhci_fsl_fdt_switch_vccq(device_t brdev, device_t reqdev)
{
	struct sdhci_fsl_fdt_softc *sc;
	struct sdhci_slot *slot;
	regulator_t vqmmc_supply;
	uint32_t val_old, val;
	int uvolt, err = 0;

	sc = device_get_softc(brdev);
	slot = device_get_ivars(reqdev);

	val_old = val = RD4(sc, SDHCI_FSL_PROT_CTRL);

	switch (slot->host.ios.vccq) {
	case vccq_180:
		if (sc->soc_data->errata & SDHCI_FSL_UNSUPP_1_8V)
			return (EOPNOTSUPP);

		val |= SDHCI_FSL_PROT_CTRL_VOLT_SEL;
		uvolt = 1800000;
		break;
	case vccq_330:
		val &= ~SDHCI_FSL_PROT_CTRL_VOLT_SEL;
		uvolt = 3300000;
		break;
	default:
		return (EOPNOTSUPP);
	}

	WR4(sc, SDHCI_FSL_PROT_CTRL, val);

	if (sc->soc_data->errata & SDHCI_FSL_MISSING_VCCQ_REG) {
		err = sdhci_fsl_fdt_switch_syscon_voltage(brdev, sc,
		    slot->host.ios.vccq);
		if (err != 0)
			goto vccq_fail;
	}

	vqmmc_supply = sc->fdt_helper.vqmmc_supply;
	/*
	 * Even though we expect to find a fixed regulator in this controller
	 * family, let's play safe.
	 */
	if (vqmmc_supply != NULL) {
		err = regulator_set_voltage(vqmmc_supply, uvolt, uvolt);
		if (err != 0)
			goto vccq_fail;
	}

	return (0);

vccq_fail:
	device_printf(sc->dev, "Cannot set vqmmc to %d<->%d\n", uvolt, uvolt);
	WR4(sc, SDHCI_FSL_PROT_CTRL, val_old);

	return (err);
}

static int
sdhci_fsl_fdt_get_ro(device_t bus, device_t child)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = device_get_softc(bus);
	return (sdhci_fdt_gpio_get_readonly(sc->gpio));
}

static bool
sdhci_fsl_fdt_get_card_present(device_t dev, struct sdhci_slot *slot)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = device_get_softc(dev);
	return (sdhci_fdt_gpio_get_present(sc->gpio));
}

static uint32_t
sdhci_fsl_fdt_vddrange_to_mask(device_t dev, uint32_t *vdd_ranges, int len)
{
	uint32_t vdd_min, vdd_max;
	uint32_t vdd_mask = 0;
	int i;

	/* Ranges are organized as pairs of values. */
	if ((len % 2) != 0) {
		device_printf(dev, "Invalid voltage range\n");
		return (0);
	}
	len = len / 2;

	for (i = 0; i < len; i++) {
		vdd_min = vdd_ranges[2 * i];
		vdd_max = vdd_ranges[2 * i + 1];

		if (vdd_min > vdd_max || vdd_min < 1650 || vdd_min > 3600 ||
		    vdd_max < 1650 || vdd_max > 3600) {
			device_printf(dev, "Voltage range %d - %d is out of bounds\n",
			    vdd_min, vdd_max);
			return (0);
		}

		if (vdd_min <= 1800 && vdd_max >= 1800)
			vdd_mask |= SDHCI_CAN_VDD_180;
		if (vdd_min <= 3000 && vdd_max >= 3000)
			vdd_mask |= SDHCI_CAN_VDD_300;
		if (vdd_min <= 3300 && vdd_max >= 3300)
			vdd_mask |= SDHCI_CAN_VDD_330;
	}

	return (vdd_mask);
}

static void
sdhci_fsl_fdt_of_parse(device_t dev)
{
	struct sdhci_fsl_fdt_softc *sc;
	phandle_t node;
	pcell_t *voltage_ranges;
	uint32_t vdd_mask = 0;
	ssize_t num_ranges;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	/* Call mmc_fdt_parse in order to get mmc related properties. */
	mmc_fdt_parse(dev, node, &sc->fdt_helper, &sc->slot.host);

	sc->slot.quirks |= SDHCI_QUIRK_MISSING_CAPS;
	sc->slot.caps = sdhci_fsl_fdt_read_4(dev, &sc->slot,
	    SDHCI_CAPABILITIES) & ~(SDHCI_CAN_DO_SUSPEND);
	sc->slot.caps2 = sdhci_fsl_fdt_read_4(dev, &sc->slot,
	    SDHCI_CAPABILITIES2);

	/* Parse the "voltage-ranges" dts property. */
	num_ranges = OF_getencprop_alloc(node, "voltage-ranges",
	    (void **) &voltage_ranges);
	if (num_ranges <= 0)
		return;
	vdd_mask = sdhci_fsl_fdt_vddrange_to_mask(dev, voltage_ranges,
	    num_ranges / sizeof(uint32_t));
	OF_prop_free(voltage_ranges);

	/* Overwrite voltage caps only if we got something from dts. */
	if (vdd_mask != 0 &&
	    (vdd_mask != (sc->slot.caps & SDHCI_FSL_CAN_VDD_MASK))) {
		sc->slot.caps &= ~(SDHCI_FSL_CAN_VDD_MASK);
		sc->slot.caps |= vdd_mask;
	}
}

static int
sdhci_fsl_poll_register(struct sdhci_fsl_fdt_softc *sc,
    uint32_t reg, uint32_t mask, int value)
{
	int retries;

	retries = SDHCI_FSL_MAX_RETRIES;

	while ((RD4(sc, reg) & mask) != value) {
		if (!retries--)
			return (ENXIO);

		DELAY(10);
	}

	return (0);
}

static int
sdhci_fsl_fdt_attach(device_t dev)
{
	struct sdhci_fsl_fdt_softc *sc;
	struct mmc_host *host;
	uint32_t val, buf_order;
	uintptr_t ocd_data;
	uint64_t clk_hz;
	phandle_t node;
	int rid, ret;
	clk_t clk;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);
	ocd_data = ofw_bus_search_compatible(dev,
	    sdhci_fsl_fdt_compat_data)->ocd_data;
	sc->dev = dev;
	sc->flags = 0;
	host = &sc->slot.host;
	rid = 0;

	/*
	 * LX2160A needs its own soc_data in order to apply SoC
	 * specific quriks. Since the controller is identified
	 * only with a generic compatible string we need to do this dance here.
	 */
	if (ofw_bus_node_is_compatible(OF_finddevice("/"), "fsl,lx2160a"))
		sc->soc_data = &sdhci_fsl_fdt_lx2160a_soc_data;
	else
		sc->soc_data = (struct sdhci_fsl_fdt_soc_data *)ocd_data;

	sc->slot.quirks = sc->soc_data->quirks;

	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev,
		    "Could not allocate resources for controller\n");
		return (ENOMEM);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev,
		    "Could not allocate irq resources for controller\n");
		ret = ENOMEM;
		goto err_free_mem;
	}

	ret = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, sdhci_fsl_fdt_irq, sc, &sc->irq_cookie);
	if (ret != 0) {
		device_printf(dev, "Could not setup IRQ handler\n");
		goto err_free_irq_res;
	}

	ret = clk_get_by_ofw_index(dev, node, 0, &clk);
	if (ret != 0) {
		device_printf(dev, "Parent clock not found\n");
		goto err_free_irq;
	}

	ret = clk_get_freq(clk, &clk_hz);
	if (ret != 0) {
		device_printf(dev,
		    "Could not get parent clock frequency\n");
		goto err_free_irq;
	}

	sc->baseclk_hz = clk_hz / sc->soc_data->baseclk_div;

	/* Figure out eSDHC block endianness before we touch any HW regs. */
	if (OF_hasprop(node, "little-endian")) {
		sc->read = read_le;
		sc->write = write_le;
		buf_order = SDHCI_FSL_PROT_CTRL_BYTE_NATIVE;
	} else {
		sc->read = read_be;
		sc->write = write_be;
		buf_order = SDHCI_FSL_PROT_CTRL_BYTE_SWAP;
	}

	sc->vendor_ver = (RD4(sc, SDHCI_FSL_HOST_VERSION) &
	    SDHCI_VENDOR_VER_MASK) >> SDHCI_VENDOR_VER_SHIFT;

	sdhci_fsl_fdt_of_parse(dev);
	sc->maxclk_hz = host->f_max ? host->f_max : sc->baseclk_hz;

	/*
	 * Setting this register affects byte order in SDHCI_BUFFER only.
	 * If the eSDHC block is connected over a big-endian bus, the data
	 * read from/written to the buffer will be already byte swapped.
	 * In such a case, setting SDHCI_FSL_PROT_CTRL_BYTE_SWAP will convert
	 * the byte order again, resulting in a native byte order.
	 * The read/write callbacks accommodate for this behavior.
	 */
	val = RD4(sc, SDHCI_FSL_PROT_CTRL);
	val &= ~SDHCI_FSL_PROT_CTRL_BYTE_MASK;
	WR4(sc, SDHCI_FSL_PROT_CTRL, val | buf_order);

	/*
	 * Gate the SD clock and set its source to
	 * peripheral clock / baseclk_div. The frequency in baseclk_hz is set
	 * to match this.
	 */
	val = RD4(sc, SDHCI_CLOCK_CONTROL);
	WR4(sc, SDHCI_CLOCK_CONTROL, val & ~SDHCI_FSL_CLK_SDCLKEN);
	val = RD4(sc, SDHCI_FSL_ESDHC_CTRL);
	WR4(sc, SDHCI_FSL_ESDHC_CTRL, val | SDHCI_FSL_ESDHC_CTRL_CLK_DIV2);
	sc->slot.max_clk = sc->maxclk_hz;
	sc->gpio = sdhci_fdt_gpio_setup(dev, &sc->slot);

	/*
	 * Set the buffer watermark level to 128 words (512 bytes) for both
	 * read and write. The hardware has a restriction that when the read or
	 * write ready status is asserted, that means you can read exactly the
	 * number of words set in the watermark register before you have to
	 * re-check the status and potentially wait for more data. The main
	 * sdhci driver provides no hook for doing status checking on less than
	 * a full block boundary, so we set the watermark level to be a full
	 * block. Reads and writes where the block size is less than the
	 * watermark size will work correctly too, no need to change the
	 * watermark for different size blocks. However, 128 is the maximum
	 * allowed for the watermark, so PIO is limitted to 512 byte blocks.
	 */
	WR4(sc, SDHCI_FSL_WTMK_LVL, SDHCI_FSL_WTMK_WR_512B |
	    SDHCI_FSL_WTMK_RD_512B);

	ret = sdhci_init_slot(dev, &sc->slot, 0);
	if (ret != 0)
		goto err_free_gpio;
	sc->slot_init_done = true;
	sdhci_start_slot(&sc->slot);

	bus_attach_children(dev);
	return (0);

err_free_gpio:
	sdhci_fdt_gpio_teardown(sc->gpio);
err_free_irq:
	bus_teardown_intr(dev, sc->irq_res, sc->irq_cookie);
err_free_irq_res:
	bus_free_resource(dev, SYS_RES_IRQ, sc->irq_res);
err_free_mem:
	bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
	return (ret);
}

static int
sdhci_fsl_fdt_detach(device_t dev)
{
	struct sdhci_fsl_fdt_softc *sc;

	sc = device_get_softc(dev);
	if (sc->slot_init_done)
		sdhci_cleanup_slot(&sc->slot);
	if (sc->gpio != NULL)
		sdhci_fdt_gpio_teardown(sc->gpio);
	if (sc->irq_cookie != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_cookie);
	if (sc->irq_res != NULL)
		bus_free_resource(dev, SYS_RES_IRQ, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
	return (0);
}

static int
sdhci_fsl_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev,
	   sdhci_fsl_fdt_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "NXP QorIQ Layerscape eSDHC controller");
	return (BUS_PROBE_DEFAULT);
}

static int
sdhci_fsl_fdt_read_ivar(device_t bus, device_t child, int which,
    uintptr_t *result)
{
	struct sdhci_slot *slot = device_get_ivars(child);

	if (which == MMCBR_IVAR_MAX_DATA && (slot->opt & SDHCI_HAVE_DMA)) {
		/*
		 * In the absence of SDMA buffer boundary functionality,
		 * limit the maximum data length per read/write command
		 * to bounce buffer size.
		 */
		*result = howmany(slot->sdma_bbufsz, 512);
		return (0);
	}
	return (sdhci_generic_read_ivar(bus, child, which, result));
}

static int
sdhci_fsl_fdt_write_ivar(device_t bus, device_t child, int which,
    uintptr_t value)
{
	struct sdhci_fsl_fdt_softc *sc;
	struct sdhci_slot *slot = device_get_ivars(child);
	uint32_t prescale, div;

	/* Don't depend on clock resolution limits from sdhci core. */
	if (which == MMCBR_IVAR_CLOCK) {
		if (value == 0) {
			slot->host.ios.clock = 0;
			return (0);
		}

		sc = device_get_softc(bus);

		SDHCI_FSL_FDT_CLK_DIV(sc, sc->baseclk_hz, value, prescale, div);
		slot->host.ios.clock = sc->baseclk_hz / (prescale * div);

		return (0);
	}

	return (sdhci_generic_write_ivar(bus, child, which, value));
}

static void
sdhci_fsl_fdt_reset(device_t dev, struct sdhci_slot *slot, uint8_t mask)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t val;

	sdhci_generic_reset(dev, slot, mask);

	if (!(mask & SDHCI_RESET_ALL))
		return;

	sc = device_get_softc(dev);

	/* Some registers have to be cleared by hand. */
	if (slot->version >= SDHCI_SPEC_300) {
		val = RD4(sc, SDHCI_FSL_TBCTL);
		val &= ~SDHCI_FSL_TBCTL_TBEN;
		WR4(sc, SDHCI_FSL_TBCTL, val);
	}

	/*
	 * Pulse width detection is not reliable on some boards. Perform
	 * workaround by clearing register's bit according to errata.
	 */
	if (sc->soc_data->errata & SDHCI_FSL_UNRELIABLE_PULSE_DET) {
		val = RD4(sc, SDHCI_FSL_DLLCFG1);
		val &= ~SDHCI_FSL_DLLCFG1_PULSE_STRETCH;
		WR4(sc, SDHCI_FSL_DLLCFG1, val);
	}

	sc->flags = 0;
}

static void
sdhci_fsl_switch_tuning_block(device_t dev, bool enable)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	reg = RD4(sc, SDHCI_FSL_TBCTL);

	if (enable)
		reg |= SDHCI_FSL_TBCTL_TBEN;
	else
		reg &= ~SDHCI_FSL_TBCTL_TBEN;

	WR4(sc, SDHCI_FSL_TBCTL, reg);
}

static int
sdhci_fsl_sw_tuning(struct sdhci_fsl_fdt_softc *sc, device_t bus,
    device_t child, bool hs400, uint32_t wnd_start, uint32_t wnd_end)
{
	uint32_t reg;
	int error;

	if (sc->soc_data->errata & SDHCI_FSL_TUNING_ERRATUM_TYPE1 ||
	    abs(wnd_start - wnd_end) <= (4 * sc->div_ratio + 2)) {
		wnd_start = 5 * sc->div_ratio;
		wnd_end = 3 * sc->div_ratio;
	} else {
		wnd_start = 8 * sc->div_ratio;
		wnd_end = 4 * sc->div_ratio;
	}

	reg = RD4(sc, SDHCI_FSL_TBPTR);
	reg &= ~SDHCI_FSL_TBPTR_WND_MASK;
	reg &= ~(SDHCI_FSL_TBPTR_WND_MASK << SDHCI_FSL_TBPTR_WND_START_SHIFT);
	reg |= wnd_start << SDHCI_FSL_TBPTR_WND_START_SHIFT;
	reg |= wnd_end;
	WR4(sc, SDHCI_FSL_TBPTR, reg);

	/*
	 * Normally those are supposed to be set in sdhci_execute_tuning.
	 * However in our case we need a small delay between setting the two.
	 */
	reg = RD4(sc, SDHCI_FSL_AUTOCERR);
	reg |= SDHCI_FSL_AUTOCERR_EXTN;
	WR4(sc, SDHCI_FSL_AUTOCERR, reg);
	DELAY(10);
	reg |= SDHCI_FSL_AUTOCERR_SMPCLKSEL;
	WR4(sc, SDHCI_FSL_AUTOCERR, reg);

	reg = RD4(sc, SDHCI_FSL_TBCTL);
	reg &= ~SDHCI_FSL_TBCTL_TB_MODE_MASK;
	reg |= SDHCI_FSL_TBCTL_MODE_SW;
	WR4(sc, SDHCI_FSL_TBCTL, reg);

	error = sdhci_generic_tune(bus, child, hs400);
	if (error != 0) {
		device_printf(bus,
		    "Failed to execute generic tune while performing software tuning.\n");
	}

	return (error);
}

static int
sdhci_fsl_fdt_tune(device_t bus, device_t child, bool hs400)
{
	struct sdhci_fsl_fdt_softc *sc;
	uint32_t wnd_start, wnd_end;
	uint32_t clk_divider, reg;
	struct sdhci_slot *slot;
	int error;

	sc = device_get_softc(bus);
	slot = device_get_ivars(child);

	if (sc->slot.host.ios.timing == bus_timing_uhs_sdr50 &&
	    !(slot->opt & SDHCI_SDR50_NEEDS_TUNING))
		return (0);

	/*
	 * For tuning mode SD clock divider must be within 3 to 16.
	 * We also need to match the frequency to whatever mode is used.
	 * For that reason we're just bailing if the dividers don't match
	 * that requirement.
	 */
	clk_divider = sc->baseclk_hz / slot->clock;
	if (clk_divider < 3 || clk_divider > 16)
		return (ENXIO);

	if (hs400)
		sc->flags |= SDHCI_FSL_HS400_FLAG;

	/* Disable clock. */
	fsl_sdhc_fdt_set_clock(sc, slot, sc->sdclk_bits);

	/* Wait for PRSSTAT[SDSTB] to be set by hardware. */
	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(bus,
		    "Timeout while waiting for clock to stabilize.\n");

	/* Flush async IO. */
	reg = RD4(sc, SDHCI_FSL_ESDHC_CTRL);
	reg |= SDHCI_FSL_ESDHC_CTRL_FAF;
	WR4(sc, SDHCI_FSL_ESDHC_CTRL, reg);

	/* Wait for ESDHC[FAF] to be cleared by hardware. */
	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_ESDHC_CTRL,
	    SDHCI_FSL_ESDHC_CTRL_FAF, 0);
	if (error)
		device_printf(bus,
		    "Timeout while waiting for hardware.\n");

	/*
	 * Set TBCTL[TB_EN] register and program valid tuning mode.
	 * According to RM MODE_3 means that:
	 * "eSDHC takes care of the re-tuning during data transfer
	 * (auto re-tuning).".
	 * Tuning mode can only be changed while the clock is disabled.
	 */
	reg = RD4(sc, SDHCI_FSL_TBCTL);
	reg &= ~SDHCI_FSL_TBCTL_TB_MODE_MASK;
	reg |= SDHCI_FSL_TBCTL_TBEN | SDHCI_FSL_TBCTL_MODE_3;
	WR4(sc, SDHCI_FSL_TBCTL, reg);

	/* Enable clock. */
	fsl_sdhc_fdt_set_clock(sc, slot, SDHCI_CLOCK_CARD_EN | sc->sdclk_bits);

	/* Wait for clock to stabilize. */
	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error)
		device_printf(bus,
		    "Timeout while waiting for clock to stabilize.\n");

	/* Perform hardware tuning. */
	error = sdhci_generic_tune(bus, child, hs400);

	reg = RD4(sc, SDHCI_FSL_TBPTR);
	wnd_start = reg >> SDHCI_FSL_TBPTR_WND_START_SHIFT;
	wnd_start &= SDHCI_FSL_TBPTR_WND_MASK;
	wnd_end = reg & SDHCI_FSL_TBPTR_WND_MASK;

	/*
	 * For erratum type2 affected platforms, the controller can erroneously
	 * declare that the tuning was successful. Verify the tuning window to
	 * make sure that we're fine.
	 */
	if (error == 0 &&
	    sc->soc_data->errata & SDHCI_FSL_TUNING_ERRATUM_TYPE2 &&
	    abs(wnd_start - wnd_end) > (4 * sc->div_ratio + 2)) {
		error = EIO;
	}

	/* If hardware tuning failed, try software tuning. */
	if (error != 0 &&
	    (sc->soc_data->errata &
	    (SDHCI_FSL_TUNING_ERRATUM_TYPE1 |
	    SDHCI_FSL_TUNING_ERRATUM_TYPE2))) {
		error = sdhci_fsl_sw_tuning(sc, bus, child, hs400, wnd_start,
		    wnd_end);
		if (error != 0)
			device_printf(bus, "Software tuning failed.\n");
	}

	if (error != 0) {
		sdhci_fsl_switch_tuning_block(bus, false);
		return (error);
	}
	if (hs400) {
		reg = RD4(sc, SDHCI_FSL_SDTIMINGCTL);
		reg |= SDHCI_FSL_SDTIMINGCTL_FLW_CTL;
		WR4(sc, SDHCI_FSL_SDTIMINGCTL, reg);
	}

	return (0);
}

static int
sdhci_fsl_fdt_retune(device_t bus, device_t child, bool reset)
{
	struct sdhci_slot *slot;
	struct sdhci_fsl_fdt_softc *sc;

	slot = device_get_ivars(child);
	sc = device_get_softc(bus);

	if (!(slot->opt & SDHCI_TUNING_ENABLED))
		return (0);

	/* HS400 must be tuned in HS200 mode. */
	if (slot->host.ios.timing == bus_timing_mmc_hs400)
		return (EINVAL);

	/*
	 * Only re-tuning with full reset is supported.
	 * The controller is normally put in "mode 3", which means that
	 * periodic re-tuning is done automatically. See comment in
	 * sdhci_fsl_fdt_tune for details.
	 * Because of that re-tuning should only be triggered as a result
	 * of a CRC error.
	 */
	 if (!reset)
		return (ENOTSUP);

	return (sdhci_fsl_fdt_tune(bus, child,
	    sc->flags & SDHCI_FSL_HS400_FLAG));
}
static void
sdhci_fsl_disable_hs400_mode(device_t dev, struct sdhci_fsl_fdt_softc *sc)
{
	uint32_t reg;
	int error;

	/* Check if HS400 is enabled right now. */
	reg = RD4(sc, SDHCI_FSL_TBCTL);
	if ((reg & SDHCI_FSL_TBCTL_HS400_EN) == 0)
		return;

	reg = RD4(sc, SDHCI_FSL_SDTIMINGCTL);
	reg &= ~SDHCI_FSL_SDTIMINGCTL_FLW_CTL;
	WR4(sc, SDHCI_FSL_SDTIMINGCTL, reg);

	reg = RD4(sc, SDHCI_FSL_SDCLKCTL);
	reg &= ~SDHCI_FSL_SDCLKCTL_CMD_CLK_CTL;
	WR4(sc, SDHCI_FSL_SDCLKCTL, reg);

	fsl_sdhc_fdt_set_clock(sc, &sc->slot, sc->sdclk_bits);
	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(dev,
		    "Internal clock never stabilized.\n");

	reg = RD4(sc, SDHCI_FSL_TBCTL);
	reg &= ~SDHCI_FSL_TBCTL_HS400_EN;
	WR4(sc, SDHCI_FSL_TBCTL, reg);

	fsl_sdhc_fdt_set_clock(sc, &sc->slot, SDHCI_CLOCK_CARD_EN |
	    sc->sdclk_bits);

	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(dev,
		    "Internal clock never stabilized.\n");

	reg = RD4(sc, SDHCI_FSL_DLLCFG0);
	reg &= ~(SDHCI_FSL_DLLCFG0_EN |
	    SDHCI_FSL_DLLCFG0_FREQ_SEL);
	WR4(sc, SDHCI_FSL_DLLCFG0, reg);

	reg = RD4(sc, SDHCI_FSL_TBCTL);
	reg &= ~SDHCI_FSL_TBCTL_HS400_WND_ADJ;
	WR4(sc, SDHCI_FSL_TBCTL, reg);

	sdhci_fsl_switch_tuning_block(dev, false);
}

static void
sdhci_fsl_enable_hs400_mode(device_t dev, struct sdhci_slot *slot,
    struct sdhci_fsl_fdt_softc *sc)
{
	uint32_t reg;
	int error;

	sdhci_fsl_switch_tuning_block(dev, true);
	fsl_sdhc_fdt_set_clock(sc, slot, sc->sdclk_bits);

	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(dev,
		    "Timeout while waiting for clock to stabilize.\n");

	reg = RD4(sc, SDHCI_FSL_TBCTL);
	reg |= SDHCI_FSL_TBCTL_HS400_EN;
	WR4(sc, SDHCI_FSL_TBCTL, reg);
	reg = RD4(sc, SDHCI_FSL_SDCLKCTL);
	reg |= SDHCI_FSL_SDCLKCTL_CMD_CLK_CTL;
	WR4(sc, SDHCI_FSL_SDCLKCTL, reg);

	fsl_sdhc_fdt_set_clock(sc, slot, SDHCI_CLOCK_CARD_EN |
	    sc->sdclk_bits);
	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(dev,
		    "Timeout while waiting for clock to stabilize.\n");

	reg = RD4(sc, SDHCI_FSL_DLLCFG0);
	reg |= SDHCI_FSL_DLLCFG0_EN | SDHCI_FSL_DLLCFG0_RESET |
	    SDHCI_FSL_DLLCFG0_FREQ_SEL;
	WR4(sc, SDHCI_FSL_DLLCFG0, reg);

	/*
	 * The reset bit is not a self clearing one.
	 * Give it some time and clear it manually.
	 */
	DELAY(100);
	reg &= ~SDHCI_FSL_DLLCFG0_RESET;
	WR4(sc, SDHCI_FSL_DLLCFG0, reg);

	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_DLLSTAT0,
	    SDHCI_FSL_DLLSTAT0_SLV_STS, SDHCI_FSL_DLLSTAT0_SLV_STS);
	if (error != 0)
		device_printf(dev,
		    "Timeout while waiting for DLL0.\n");

	reg = RD4(sc, SDHCI_FSL_TBCTL);
	reg |= SDHCI_FSL_TBCTL_HS400_WND_ADJ;
	WR4(sc, SDHCI_FSL_TBCTL, reg);

	fsl_sdhc_fdt_set_clock(sc, slot, sc->sdclk_bits);

	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(dev,
		    "timeout while waiting for clock to stabilize.\n");

	reg = RD4(sc, SDHCI_FSL_ESDHC_CTRL);
	reg |= SDHCI_FSL_ESDHC_CTRL_FAF;
	WR4(sc, SDHCI_FSL_ESDHC_CTRL, reg);

	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_ESDHC_CTRL,
	    SDHCI_FSL_ESDHC_CTRL_FAF, 0);
	if (error != 0)
		device_printf(dev,
		    "Timeout while waiting for hardware.\n");

	fsl_sdhc_fdt_set_clock(sc, slot, SDHCI_CLOCK_CARD_EN |
	    sc->sdclk_bits);

	error = sdhci_fsl_poll_register(sc, SDHCI_FSL_PRES_STATE,
	    SDHCI_FSL_PRES_SDSTB, SDHCI_FSL_PRES_SDSTB);
	if (error != 0)
		device_printf(dev,
		    "Timeout while waiting for clock to stabilize.\n");
}

static void
sdhci_fsl_fdt_set_uhs_timing(device_t dev, struct sdhci_slot *slot)
{
	struct sdhci_fsl_fdt_softc *sc;
	const struct mmc_ios *ios;
	uint32_t mode, reg;

	sc = device_get_softc(dev);
	ios = &slot->host.ios;
	mode = 0;

	/*
	 * When we switch to HS400 this function is called twice.
	 * First after the timing is set, and then after the clock
	 * is changed to the target frequency.
	 * The controller can be switched to HS400 only after the latter
	 * is done.
	 */
	if (slot->host.ios.timing == bus_timing_mmc_hs400 &&
	    ios->clock > SD_SDR50_MAX)
		sdhci_fsl_enable_hs400_mode(dev, slot, sc);
	else if (slot->host.ios.timing < bus_timing_mmc_hs400) {
		sdhci_fsl_disable_hs400_mode(dev, sc);

		/*
		 * Switching to HS400 requires a custom procedure executed in
		 * sdhci_fsl_enable_hs400_mode in case above.
		 * For all other modes we just need to set the corresponding flag.
		 */
		reg = RD4(sc, SDHCI_FSL_AUTOCERR);
		reg &= ~SDHCI_FSL_AUTOCERR_UHMS;
		if (ios->clock > SD_SDR50_MAX)
			mode = SDHCI_CTRL2_UHS_SDR104;
		else if (ios->clock > SD_SDR25_MAX)
			mode = SDHCI_CTRL2_UHS_SDR50;
		else if (ios->clock > SD_SDR12_MAX) {
			if (ios->timing == bus_timing_uhs_ddr50 ||
			    ios->timing == bus_timing_mmc_ddr52)
				mode = SDHCI_CTRL2_UHS_DDR50;
			else
				mode = SDHCI_CTRL2_UHS_SDR25;
		} else if (ios->clock > SD_MMC_CARD_ID_FREQUENCY)
			mode = SDHCI_CTRL2_UHS_SDR12;

		reg |= mode << SDHCI_FSL_AUTOCERR_UHMS_SHIFT;
		WR4(sc, SDHCI_FSL_AUTOCERR, reg);
	}
}

static const device_method_t sdhci_fsl_fdt_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			sdhci_fsl_fdt_probe),
	DEVMETHOD(device_attach,		sdhci_fsl_fdt_attach),
	DEVMETHOD(device_detach,		sdhci_fsl_fdt_detach),

	/* Bus interface. */
	DEVMETHOD(bus_read_ivar,		sdhci_fsl_fdt_read_ivar),
	DEVMETHOD(bus_write_ivar,		sdhci_fsl_fdt_write_ivar),

	/* MMC bridge interface. */
	DEVMETHOD(mmcbr_request,		sdhci_generic_request),
	DEVMETHOD(mmcbr_get_ro,			sdhci_fsl_fdt_get_ro),
	DEVMETHOD(mmcbr_acquire_host,		sdhci_generic_acquire_host),
	DEVMETHOD(mmcbr_release_host,		sdhci_generic_release_host),
	DEVMETHOD(mmcbr_switch_vccq,		sdhci_fsl_fdt_switch_vccq),
	DEVMETHOD(mmcbr_update_ios,		sdhci_fsl_fdt_update_ios),
	DEVMETHOD(mmcbr_tune,			sdhci_fsl_fdt_tune),
	DEVMETHOD(mmcbr_retune,			sdhci_fsl_fdt_retune),

	/* SDHCI accessors. */
	DEVMETHOD(sdhci_read_1,			sdhci_fsl_fdt_read_1),
	DEVMETHOD(sdhci_read_2,			sdhci_fsl_fdt_read_2),
	DEVMETHOD(sdhci_read_4,			sdhci_fsl_fdt_read_4),
	DEVMETHOD(sdhci_read_multi_4,		sdhci_fsl_fdt_read_multi_4),
	DEVMETHOD(sdhci_write_1,		sdhci_fsl_fdt_write_1),
	DEVMETHOD(sdhci_write_2,		sdhci_fsl_fdt_write_2),
	DEVMETHOD(sdhci_write_4,		sdhci_fsl_fdt_write_4),
	DEVMETHOD(sdhci_write_multi_4,		sdhci_fsl_fdt_write_multi_4),
	DEVMETHOD(sdhci_get_card_present,	sdhci_fsl_fdt_get_card_present),
	DEVMETHOD(sdhci_reset,			sdhci_fsl_fdt_reset),
	DEVMETHOD(sdhci_set_uhs_timing,		sdhci_fsl_fdt_set_uhs_timing),
	DEVMETHOD_END
};

static driver_t sdhci_fsl_fdt_driver = {
	"sdhci_fsl_fdt",
	sdhci_fsl_fdt_methods,
	sizeof(struct sdhci_fsl_fdt_softc),
};

DRIVER_MODULE(sdhci_fsl_fdt, simplebus, sdhci_fsl_fdt_driver, NULL, NULL);
SDHCI_DEPEND(sdhci_fsl_fdt);

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_fsl_fdt);
#endif
