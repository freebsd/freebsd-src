/*
 * Copyright 2019 Emmanuel Vadot <manu@freebsd.org>
 * Copyright (c) 2017 Ian Lepore <ian@freebsd.org> All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/gpio.h>
#include <sys/taskqueue.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmc_helpers.h>

static inline void
mmc_parse_sd_speed(device_t dev, struct mmc_host *host)
{
	bool no_18v = false;

	/*
	 * Parse SD supported modes
	 * All UHS-I modes requires 1.8V signaling.
	 */
	if (device_has_property(dev, "no-1-8-v"))
		no_18v = true;
	if (device_has_property(dev, "cap-sd-highspeed"))
		host->caps |= MMC_CAP_HSPEED;
	if (device_has_property(dev, "sd-uhs-sdr12") && !no_18v)
		host->caps |= MMC_CAP_UHS_SDR12 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "sd-uhs-sdr25") && !no_18v)
		host->caps |= MMC_CAP_UHS_SDR25 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "sd-uhs-sdr50") && !no_18v)
		host->caps |= MMC_CAP_UHS_SDR50 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "sd-uhs-sdr104") && !no_18v)
		host->caps |= MMC_CAP_UHS_SDR104 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "sd-uhs-ddr50") && !no_18v)
		host->caps |= MMC_CAP_UHS_DDR50 | MMC_CAP_SIGNALING_180;
}

static inline void
mmc_parse_mmc_speed(device_t dev, struct mmc_host *host)
{
	/* Parse eMMC supported modes */
	if (device_has_property(dev, "cap-mmc-highspeed"))
		host->caps |= MMC_CAP_HSPEED;
	if (device_has_property(dev, "mmc-ddr-1_2v"))
		host->caps |= MMC_CAP_MMC_DDR52_120 | MMC_CAP_SIGNALING_120;
	if (device_has_property(dev, "mmc-ddr-1_8v"))
		host->caps |= MMC_CAP_MMC_DDR52_180 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "mmc-ddr-3_3v"))
		host->caps |= MMC_CAP_SIGNALING_330;
	if (device_has_property(dev, "mmc-hs200-1_2v"))
		host->caps |= MMC_CAP_MMC_HS200_120 | MMC_CAP_SIGNALING_120;
	if (device_has_property(dev, "mmc-hs200-1_8v"))
		host->caps |= MMC_CAP_MMC_HS200_180 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "mmc-hs400-1_2v"))
		host->caps |= MMC_CAP_MMC_HS400_120 | MMC_CAP_SIGNALING_120;
	if (device_has_property(dev, "mmc-hs400-1_8v"))
		host->caps |= MMC_CAP_MMC_HS400_180 | MMC_CAP_SIGNALING_180;
	if (device_has_property(dev, "mmc-hs400-enhanced-strobe"))
		host->caps |= MMC_CAP_MMC_ENH_STROBE;
}

int
mmc_parse(device_t dev, struct mmc_helper *helper, struct mmc_host *host)
{
	uint64_t bus_width, max_freq;

	bus_width = 0;
	if (device_get_property(dev, "bus-width", &bus_width, sizeof(uint64_t)) <= 0)
		bus_width = 1;

	if (bus_width >= 4)
		host->caps |= MMC_CAP_4_BIT_DATA;
	if (bus_width >= 8)
		host->caps |= MMC_CAP_8_BIT_DATA;

	/*
	 * max-frequency is optional, drivers should tweak this value
	 * if it's not present based on the clock that the mmc controller
	 * operates on
	 */
	max_freq = 0;
	device_get_property(dev, "max-frequency", &max_freq, sizeof(uint64_t));
	host->f_max = max_freq;

	if (device_has_property(dev, "broken-cd"))
		helper->props |= MMC_PROP_BROKEN_CD;
	if (device_has_property(dev, "non-removable"))
		helper->props |= MMC_PROP_NON_REMOVABLE;
	if (device_has_property(dev, "wp-inverted"))
		helper->props |= MMC_PROP_WP_INVERTED;
	if (device_has_property(dev, "cd-inverted"))
		helper->props |= MMC_PROP_CD_INVERTED;
	if (device_has_property(dev, "no-sdio"))
		helper->props |= MMC_PROP_NO_SDIO;
	if (device_has_property(dev, "no-sd"))
		helper->props |= MMC_PROP_NO_SD;
	if (device_has_property(dev, "no-mmc"))
		helper->props |= MMC_PROP_NO_MMC;

	if (!(helper->props & MMC_PROP_NO_SD))
		mmc_parse_sd_speed(dev, host);

	if (!(helper->props & MMC_PROP_NO_MMC))
		mmc_parse_mmc_speed(dev, host);

	return (0);
}
