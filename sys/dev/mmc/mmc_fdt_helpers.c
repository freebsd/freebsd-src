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
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#ifdef EXT_RESOURCES
#include <dev/extres/regulator/regulator.h>
#endif

static inline void
mmc_fdt_parse_sd_speed(phandle_t node, struct mmc_host *host)
{
	bool no_18v = false;

	/* 
	 * Parse SD supported modes 
	 * All UHS-I modes requires 1.8V signaling.
	 */
	if (OF_hasprop(node, "no1-8-v"))
		no_18v = true;
	if (OF_hasprop(node, "cap-sd-highspeed"))
		host->caps |= MMC_CAP_HSPEED;
	if (OF_hasprop(node, "sd-uhs-sdr12") && no_18v == false)
		host->caps |= MMC_CAP_UHS_SDR12 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "sd-uhs-sdr25") && no_18v == false)
		host->caps |= MMC_CAP_UHS_SDR25 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "sd-uhs-sdr50") && no_18v == false)
		host->caps |= MMC_CAP_UHS_SDR50 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "sd-uhs-sdr104") && no_18v == false)
		host->caps |= MMC_CAP_UHS_SDR104 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "sd-uhs-ddr50") && no_18v == false)
		host->caps |= MMC_CAP_UHS_DDR50 | MMC_CAP_SIGNALING_180;
}

static inline void
mmc_fdt_parse_mmc_speed(phandle_t node, struct mmc_host *host)
{

	/* Parse eMMC supported modes */
	if (OF_hasprop(node, "cap-mmc-highspeed"))
		host->caps |= MMC_CAP_HSPEED;
	if (OF_hasprop(node, "mmc-ddr-1_2v"))
		host->caps |= MMC_CAP_MMC_DDR52_120 | MMC_CAP_SIGNALING_120;
	if (OF_hasprop(node, "mmc-ddr-1_8v"))
		host->caps |= MMC_CAP_MMC_DDR52_180 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "mmc-ddr-3_3v"))
		host->caps |= MMC_CAP_SIGNALING_330;
	if (OF_hasprop(node, "mmc-hs200-1_2v"))
		host->caps |= MMC_CAP_MMC_HS200_120 | MMC_CAP_SIGNALING_120;
	if (OF_hasprop(node, "mmc-hs200-1_8v"))
		host->caps |= MMC_CAP_MMC_HS200_180 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "mmc-hs400-1_2v"))
		host->caps |= MMC_CAP_MMC_HS400_120 | MMC_CAP_SIGNALING_120;
	if (OF_hasprop(node, "mmc-hs400-1_8v"))
		host->caps |= MMC_CAP_MMC_HS400_180 | MMC_CAP_SIGNALING_180;
	if (OF_hasprop(node, "mmc-hs400-enhanced-strobe"))
		host->caps |= MMC_CAP_MMC_ENH_STROBE;
}

int
mmc_fdt_parse(device_t dev, phandle_t node, struct mmc_fdt_helper *helper,
    struct mmc_host *host)
{
	uint32_t bus_width;

	if (node <= 0)
		node = ofw_bus_get_node(dev);
	if (node <= 0)
		return (ENXIO);

	if (OF_getencprop(node, "bus-width", &bus_width, sizeof(uint32_t)) <= 0)
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
	OF_getencprop(node, "max-frequency", &host->f_max, sizeof(uint32_t));

	if (OF_hasprop(node, "broken-cd"))
		helper->props |= MMC_PROP_BROKEN_CD;
	if (OF_hasprop(node, "non-removable"))
		helper->props |= MMC_PROP_NON_REMOVABLE;
	if (OF_hasprop(node, "wp-inverted"))
		helper->props |= MMC_PROP_WP_INVERTED;
	if (OF_hasprop(node, "cd-inverted"))
		helper->props |= MMC_PROP_CD_INVERTED;
	if (OF_hasprop(node, "no-sdio"))
		helper->props |= MMC_PROP_NO_SDIO;
	if (OF_hasprop(node, "no-sd"))
		helper->props |= MMC_PROP_NO_SD;
	if (OF_hasprop(node, "no-mmc"))
		helper->props |= MMC_PROP_NO_MMC;

	if (!(helper->props & MMC_PROP_NO_SD))
		mmc_fdt_parse_sd_speed(node, host);

	if (!(helper->props & MMC_PROP_NO_MMC))
		mmc_fdt_parse_mmc_speed(node, host);

#ifdef EXT_RESOURCES
	/*
	 * Get the regulators if they are supported and
	 * clean the non supported modes based on the available voltages.
	 */
	if (regulator_get_by_ofw_property(dev, 0, "vmmc-supply",
	    &helper->vmmc_supply) == 0) {
		if (bootverbose)
			device_printf(dev, "vmmc-supply regulator found\n");
	}
	if (regulator_get_by_ofw_property(dev, 0, "vqmmc-supply",
	    &helper->vqmmc_supply) == 0 && bootverbose) {
		if (bootverbose)
			device_printf(dev, "vqmmc-supply regulator found\n");
	}

	if (helper->vqmmc_supply != NULL) {
		if (regulator_check_voltage(helper->vqmmc_supply, 1200000) == 0)
			host->caps |= MMC_CAP_SIGNALING_120;
		else
			host->caps &= ~( MMC_CAP_MMC_HS400_120 |
			    MMC_CAP_MMC_HS200_120 |
			    MMC_CAP_MMC_DDR52_120);
		if (regulator_check_voltage(helper->vqmmc_supply, 1800000) == 0)
			host->caps |= MMC_CAP_SIGNALING_180;
		else
			host->caps &= ~(MMC_CAP_MMC_HS400_180 |
			    MMC_CAP_MMC_HS200_180 |
			    MMC_CAP_MMC_DDR52_180 |
			    MMC_CAP_UHS_DDR50 |
			    MMC_CAP_UHS_SDR104 |
			    MMC_CAP_UHS_SDR50 |
			    MMC_CAP_UHS_SDR25);
		if (regulator_check_voltage(helper->vqmmc_supply, 3300000) == 0)
			host->caps |= MMC_CAP_SIGNALING_330;
	} else
		host->caps |= MMC_CAP_SIGNALING_330;
#endif

	return (0);
}

/*
 * Card detect interrupt handler.
 */
static void
cd_intr(void *arg)
{
	struct mmc_fdt_helper *helper = arg;

	taskqueue_enqueue_timeout(taskqueue_swi_giant,
	    &helper->cd_delayed_task, -(hz / 2));
}

static void
cd_card_task(void *arg, int pending __unused)
{
	struct mmc_fdt_helper *helper = arg;
	bool cd_present;

	cd_present = mmc_fdt_gpio_get_present(helper);
	if(helper->cd_handler && cd_present != helper->cd_present)
		helper->cd_handler(helper->dev,
		    cd_present);
	helper->cd_present = cd_present;

	/* If we're polling re-schedule the task */
	if (helper->cd_ihandler == NULL)
		taskqueue_enqueue_timeout_sbt(taskqueue_swi_giant,
		    &helper->cd_delayed_task, mstosbt(500), 0, C_PREL(2));
}

/*
 * Card detect setup.
 */
static void
cd_setup(struct mmc_fdt_helper *helper, phandle_t node)
{
	int pincaps;
	device_t dev;
	const char *cd_mode_str;

	dev = helper->dev;
	/*
	 * If the device is flagged as non-removable, set that slot option, and
	 * set a flag to make sdhci_fdt_gpio_get_present() always return true.
	 */
	if (helper->props & MMC_PROP_NON_REMOVABLE) {
		helper->cd_disabled = true;
		if (bootverbose)
			device_printf(dev, "Non-removable media\n");
		return;
	}

	/*
	 * If there is no cd-gpios property, then presumably the hardware
	 * PRESENT_STATE register and interrupts will reflect card state
	 * properly, and there's nothing more for us to do.  Our get_present()
	 * will return sdhci_generic_get_card_present() because cd_pin is NULL.
	 *
	 * If there is a property, make sure we can read the pin.
	 */
	if (gpio_pin_get_by_ofw_property(dev, node, "cd-gpios",
	    &helper->cd_pin))
		return;

	if (gpio_pin_getcaps(helper->cd_pin, &pincaps) != 0 ||
	    !(pincaps & GPIO_PIN_INPUT)) {
		device_printf(dev, "Cannot read card-detect gpio pin; "
		    "setting card-always-present flag.\n");
		helper->cd_disabled = true;
		return;
	}

	/*
	 * If the pin can trigger an interrupt on both rising and falling edges,
	 * we can use it to detect card presence changes.  If not, we'll request
	 * card presence polling instead of using interrupts.
	 */
	if (!(pincaps & GPIO_INTR_EDGE_BOTH)) {
		if (bootverbose)
			device_printf(dev, "Cannot configure "
			    "GPIO_INTR_EDGE_BOTH for card detect\n");
		goto without_interrupts;
	}

	if (helper->cd_handler == NULL) {
		if (bootverbose)
			device_printf(dev, "Cannot configure "
			    "interrupts as no cd_handler is set\n");
		goto without_interrupts;
	}

	/*
	 * Create an interrupt resource from the pin and set up the interrupt.
	 */
	if ((helper->cd_ires = gpio_alloc_intr_resource(dev, &helper->cd_irid,
	    RF_ACTIVE, helper->cd_pin, GPIO_INTR_EDGE_BOTH)) == NULL) {
		if (bootverbose)
			device_printf(dev, "Cannot allocate an IRQ for card "
			    "detect GPIO\n");
		goto without_interrupts;
	}

	if (bus_setup_intr(dev, helper->cd_ires, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, cd_intr, helper, &helper->cd_ihandler) != 0) {
		device_printf(dev, "Unable to setup card-detect irq handler\n");
		helper->cd_ihandler = NULL;
		goto without_interrupts;
	}

without_interrupts:
	TIMEOUT_TASK_INIT(taskqueue_swi_giant, &helper->cd_delayed_task, 0,
	    cd_card_task, helper);

	/*
	 * If we have a readable gpio pin, but didn't successfully configure
	 * gpio interrupts, setup a timeout task to poll the pin
	 */
	if (helper->cd_ihandler == NULL) {
		cd_mode_str = "polling";
	} else {
		cd_mode_str = "interrupts";
	}

	if (bootverbose) {
		device_printf(dev, "Card presence detect on %s pin %u, "
		    "configured for %s.\n",
		    device_get_nameunit(helper->cd_pin->dev), helper->cd_pin->pin,
		    cd_mode_str);
	}
}

/*
 * Write protect setup.
 */
static void
wp_setup(struct mmc_fdt_helper *helper, phandle_t node)
{
	device_t dev;

	dev = helper->dev;

	if (OF_hasprop(node, "disable-wp")) {
		helper->wp_disabled = true;
		if (bootverbose)
			device_printf(dev, "Write protect disabled\n");
		return;
	}

	if (gpio_pin_get_by_ofw_property(dev, node, "wp-gpios", &helper->wp_pin))
		return;

	if (bootverbose)
		device_printf(dev, "Write protect switch on %s pin %u\n",
		    device_get_nameunit(helper->wp_pin->dev), helper->wp_pin->pin);
}

int
mmc_fdt_gpio_setup(device_t dev, phandle_t node, struct mmc_fdt_helper *helper,
    mmc_fdt_cd_handler handler)
{

	if (node <= 0)
		node = ofw_bus_get_node(dev);
	if (node <= 0) {
		device_printf(dev, "Cannot get node for device\n");
		return (ENXIO);
	}

	helper->dev = dev;
	helper->cd_handler = handler;
	cd_setup(helper, node);
	wp_setup(helper, node);

	/* 
	 * Schedule a card detection
	 */
	taskqueue_enqueue_timeout_sbt(taskqueue_swi_giant,
	    &helper->cd_delayed_task, mstosbt(500), 0, C_PREL(2));
	return (0);
}

void
mmc_fdt_gpio_teardown(struct mmc_fdt_helper *helper)
{

	if (helper == NULL)
		return;

	if (helper->cd_ihandler != NULL)
		bus_teardown_intr(helper->dev, helper->cd_ires, helper->cd_ihandler);
	if (helper->wp_pin != NULL)
		gpio_pin_release(helper->wp_pin);
	if (helper->cd_pin != NULL)
		gpio_pin_release(helper->cd_pin);
	if (helper->cd_ires != NULL)
		bus_release_resource(helper->dev, SYS_RES_IRQ, 0, helper->cd_ires);
}

bool
mmc_fdt_gpio_get_present(struct mmc_fdt_helper *helper)
{
	bool pinstate;

	if (helper->cd_disabled)
		return (true);
	if (helper->cd_pin == NULL)
		return (false);

	gpio_pin_is_active(helper->cd_pin, &pinstate);

	return (pinstate ^ (helper->props & MMC_PROP_CD_INVERTED));
}

bool
mmc_fdt_gpio_get_readonly(struct mmc_fdt_helper *helper)
{
	bool pinstate;

	if (helper->wp_disabled)
		return (false);

	if (helper->wp_pin == NULL)
		return (false);

	gpio_pin_is_active(helper->wp_pin, &pinstate);

	return (pinstate ^ (helper->props & MMC_PROP_WP_INVERTED));
}
