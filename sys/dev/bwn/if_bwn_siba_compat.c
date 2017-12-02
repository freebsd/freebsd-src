/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 * 
 * Portions of this software were developed by Landon Fuller
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/siba/sibareg.h>

#include <dev/bhnd/cores/chipc/chipc.h>
#include <dev/bhnd/cores/pci/bhnd_pcireg.h>
#include <dev/bhnd/cores/pmu/bhnd_pmu.h>

#include "gpio_if.h"

#include "bhnd_nvram_map.h"

#include "if_bwn_siba_compat.h"

static int		bwn_bhnd_populate_nvram_data(device_t dev,
			    struct bwn_bhnd_ctx *ctx);
static inline bool	bwn_bhnd_is_siba_reg(device_t dev, uint16_t offset);

#define	BWN_ASSERT_VALID_REG(_dev, _offset)				\
	KASSERT(!bwn_bhnd_is_siba_reg(_dev, _offset),			\
	    ("%s: accessing siba-specific register %#jx", __FUNCTION__,	\
		(uintmax_t)(_offset)));

static int
bwn_bhnd_bus_ops_init(device_t dev)
{
	struct bwn_bhnd_ctx	*ctx;
	struct bwn_softc	*sc;
	const struct chipc_caps	*ccaps;
	int			 error;

	sc = device_get_softc(dev);
	ctx = NULL;

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bhnd_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		return (ENXIO);
	}

	/* Allocate PMU state */
	if ((error = bhnd_alloc_pmu(dev))) {
		device_printf(dev, "PMU allocation failed: %d\n", error);
		goto failed;
	}

	/* Allocate our context */
	ctx = malloc(sizeof(struct bwn_bhnd_ctx), M_DEVBUF, M_WAITOK|M_ZERO);

	/* Locate the ChipCommon device */
	ctx->chipc_dev = bhnd_retain_provider(dev, BHND_SERVICE_CHIPC);
	if (ctx->chipc_dev == NULL) {
		device_printf(dev, "ChipCommon not found\n");
		error = ENXIO;
		goto failed;
	}

	/* Locate the GPIO device */
	ctx->gpio_dev = bhnd_retain_provider(dev, BHND_SERVICE_GPIO);
	if (ctx->gpio_dev == NULL) {
		device_printf(dev, "GPIO not found\n");
		error = ENXIO;
		goto failed;
	}

	/* Locate the PMU device (if any) */
	ccaps = BHND_CHIPC_GET_CAPS(ctx->chipc_dev);
	if (ccaps->pmu) {
		ctx->pmu_dev = bhnd_retain_provider(dev, BHND_SERVICE_PMU);
		if (ctx->pmu_dev == NULL) {
			device_printf(dev, "PMU not found\n");
			error = ENXIO;
			goto failed;
		}
	}

	/* Populate NVRAM data */
	if ((error = bwn_bhnd_populate_nvram_data(dev, ctx)))
		goto failed;

	/* Initialize bwn_softc */
	sc->sc_bus_ctx = ctx;
	return (0);

failed:
	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
	    sc->sc_mem_res);

	if (ctx != NULL) {
		if (ctx->chipc_dev != NULL) {
			bhnd_release_provider(dev, ctx->chipc_dev,
			    BHND_SERVICE_CHIPC);
		}

		if (ctx->gpio_dev != NULL) {
			bhnd_release_provider(dev, ctx->gpio_dev,
			    BHND_SERVICE_GPIO);
		}

		if (ctx->pmu_dev != NULL) {
			bhnd_release_provider(dev, ctx->pmu_dev,
			    BHND_SERVICE_PMU);
		}

		free(ctx, M_DEVBUF);
	}

	return (error);
}

static void
bwn_bhnd_bus_ops_fini(device_t dev)
{
	struct bwn_bhnd_ctx	*ctx;
	struct bwn_softc	*sc;

	sc = device_get_softc(dev);
	ctx = sc->sc_bus_ctx;

	bhnd_release_pmu(dev);
	bhnd_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
	    sc->sc_mem_res);

	bhnd_release_provider(dev, ctx->chipc_dev, BHND_SERVICE_CHIPC);
	bhnd_release_provider(dev, ctx->gpio_dev, BHND_SERVICE_GPIO);

	if (ctx->pmu_dev != NULL)
		bhnd_release_provider(dev, ctx->pmu_dev, BHND_SERVICE_PMU);

	free(ctx, M_DEVBUF);
	sc->sc_bus_ctx = NULL;
}


/**
 * Return true if @p offset is within a siba-specific configuration register
 * block.
 */
static inline bool
bwn_bhnd_is_siba_reg(device_t dev, uint16_t offset)
{
	if (offset >= SIBA_CFG0_OFFSET &&
	    offset <= SIBA_CFG0_OFFSET + SIBA_CFG_SIZE)
		return (true);

	if (offset >= SIBA_CFG1_OFFSET &&
	    offset <= SIBA_CFG1_OFFSET + SIBA_CFG_SIZE)
		return (true);
	
	return (false);
}

/* Populate SPROM values from NVRAM */
static int
bwn_bhnd_populate_nvram_data(device_t dev, struct bwn_bhnd_ctx *ctx)
{
	const char	*mac_80211bg_var, *mac_80211a_var;
	int	error;

	/* Fetch SROM revision */
	error = bhnd_nvram_getvar_uint8(dev, BHND_NVAR_SROMREV, &ctx->sromrev);
	if (error) {
		device_printf(dev, "error reading %s: %d\n", BHND_NVAR_SROMREV,
		    error);
		return (error);
	}

	/* Fetch board flags */
	error = bhnd_nvram_getvar_uint32(dev, BHND_NVAR_BOARDFLAGS,
	    &ctx->boardflags);
	if (error) {
		device_printf(dev, "error reading %s: %d\n",
		    BHND_NVAR_BOARDFLAGS, error);
		return (error);
	}

	/* Fetch macaddrs if available; bwn(4) expects any missing macaddr
	 * values to be initialized with 0xFF octets */
	memset(ctx->mac_80211bg, 0xFF, sizeof(ctx->mac_80211bg));
	memset(ctx->mac_80211a, 0xFF, sizeof(ctx->mac_80211a));

	if (ctx->sromrev <= 2) {
		mac_80211bg_var = BHND_NVAR_IL0MACADDR;
		mac_80211a_var = BHND_NVAR_ET1MACADDR;
	} else {
		mac_80211bg_var = BHND_NVAR_MACADDR;
		mac_80211a_var = NULL;
	}

	/* Fetch required D11 core 0 macaddr */
	error = bhnd_nvram_getvar_array(dev, mac_80211bg_var, ctx->mac_80211bg,
	    sizeof(ctx->mac_80211bg), BHND_NVRAM_TYPE_UINT8_ARRAY);
	if (error) {
		device_printf(dev, "error reading %s: %d\n", mac_80211bg_var,
		    error);
		return (error);
	}

	/* Fetch optional D11 core 1 macaddr */
	if (mac_80211a_var != NULL) {
		error = bhnd_nvram_getvar_array(dev, mac_80211a_var,
		    ctx->mac_80211a, sizeof(ctx->mac_80211a),
		    BHND_NVRAM_TYPE_UINT8_ARRAY);

		if (error && error != ENOENT) {
			device_printf(dev, "error reading %s: %d\n",
			    mac_80211a_var, error);
			return (error);
		}
	};

	/* Fetch pa0maxpwr; bwn(4) expects to be able to modify it */
	if ((ctx->sromrev >= 1 && ctx->sromrev <= 3) ||
	    (ctx->sromrev >= 8 && ctx->sromrev <= 10))
	{
		error = bhnd_nvram_getvar_uint8(dev, BHND_NVAR_PA0MAXPWR,
		     &ctx->pa0maxpwr);
		if (error) {
			device_printf(dev, "error reading %s: %d\n",
			    BHND_NVAR_PA0MAXPWR, error);
			return (error);
		}
	}

	return (0);
}

/*
 * Disable PCI-specific MSI interrupt allocation handling
 */

/*
 * pci_find_cap()
 *
 * Referenced by:
 *   bwn_attach()
 */
static int
bhnd_compat_pci_find_cap(device_t dev, int capability, int *capreg)
{
	return (ENODEV);
}

/*
 * pci_alloc_msi()
 *
 * Referenced by:
 *   bwn_attach()
 */
static int
bhnd_compat_pci_alloc_msi(device_t dev, int *count)
{
	return (ENODEV);
}

/*
 * pci_release_msi()
 *
 * Referenced by:
 *   bwn_attach()
 *   bwn_detach()
 */
static int
bhnd_compat_pci_release_msi(device_t dev)
{
	return (ENODEV);
}

/*
 * pci_msi_count()
 *
 * Referenced by:
 *   bwn_attach()
 */
static int
bhnd_compat_pci_msi_count(device_t dev)
{
	return (0);
}

/*
 * siba_get_vendor()
 *
 * Referenced by:
 *   bwn_probe()
 */
static uint16_t
bhnd_compat_get_vendor(device_t dev)
{
	uint16_t vendor = bhnd_get_vendor(dev);

	switch (vendor) {
	case BHND_MFGID_BCM:
		return (SIBA_VID_BROADCOM);
	default:
		return (0x0000);
	}
}

/*
 * siba_get_device()
 *
 * Referenced by:
 *   bwn_probe()
 */
static uint16_t
bhnd_compat_get_device(device_t dev)
{
	return (bhnd_get_device(dev));
}

/*
 * siba_get_revid()
 *
 * Referenced by:
 *   bwn_attach()
 *   bwn_attach_core()
 *   bwn_chip_init()
 *   bwn_chiptest()
 *   bwn_core_init()
 *   bwn_core_start()
 *   bwn_pio_idx2base()
 *   bwn_pio_set_txqueue()
 *   bwn_pio_tx_start()
 *   bwn_probe()
 * ... and 19 others
 * 
 */
static uint8_t
bhnd_compat_get_revid(device_t dev)
{
	return (bhnd_get_hwrev(dev));
}

/**
 * Return the PCI bridge root device.
 * 
 * Will panic if a PCI bridge root device is not found.
 */
static device_t
bwn_bhnd_get_pci_dev(device_t dev)
{	device_t bridge_root;

	bridge_root = bhnd_find_bridge_root(dev, devclass_find("pci"));
	if (bridge_root == NULL)
		panic("not a PCI device");

	return (bridge_root);
}

/*
 * siba_get_pci_vendor()
 *
 * Referenced by:
 *   bwn_sprom_bugfixes()
 */
static uint16_t
bhnd_compat_get_pci_vendor(device_t dev)
{
	return (pci_get_vendor(bwn_bhnd_get_pci_dev(dev)));
}

/*
 * siba_get_pci_device()
 *
 * Referenced by:
 *   bwn_attach()
 *   bwn_attach_core()
 *   bwn_nphy_op_prepare_structs()
 *   bwn_sprom_bugfixes()
 */
static uint16_t
bhnd_compat_get_pci_device(device_t dev)
{
	return (pci_get_device(bwn_bhnd_get_pci_dev(dev)));
}

/*
 * siba_get_pci_subvendor()
 *
 * Referenced by:
 *   bwn_led_attach()
 *   bwn_nphy_op_prepare_structs()
 *   bwn_phy_g_prepare_hw()
 *   bwn_phy_hwpctl_init()
 *   bwn_phy_init_b5()
 *   bwn_phy_initn()
 *   bwn_phy_txpower_check()
 *   bwn_radio_init2055_post()
 *   bwn_sprom_bugfixes()
 *   bwn_wa_init()
 */
static uint16_t
bhnd_compat_get_pci_subvendor(device_t dev)
{
	return (pci_get_subvendor(bwn_bhnd_get_pci_dev(dev)));
}

/*
 * siba_get_pci_subdevice()
 *
 * Referenced by:
 *   bwn_nphy_workarounds_rev1_2()
 *   bwn_phy_g_prepare_hw()
 *   bwn_phy_hwpctl_init()
 *   bwn_phy_init_b5()
 *   bwn_phy_initn()
 *   bwn_phy_lp_bbinit_r01()
 *   bwn_phy_txpower_check()
 *   bwn_radio_init2055_post()
 *   bwn_sprom_bugfixes()
 *   bwn_wa_init()
 */
static uint16_t
bhnd_compat_get_pci_subdevice(device_t dev)
{
	return (pci_get_subdevice(bwn_bhnd_get_pci_dev(dev)));
}

/*
 * siba_get_pci_revid()
 *
 * Referenced by:
 *   bwn_phy_g_prepare_hw()
 *   bwn_phy_lp_bbinit_r2()
 *   bwn_sprom_bugfixes()
 *   bwn_wa_init()
 */
static uint8_t
bhnd_compat_get_pci_revid(device_t dev)
{
	return (pci_get_revid(bwn_bhnd_get_pci_dev(dev)));
}

/*
 * siba_get_chipid()
 *
 * Referenced by:
 *   bwn_attach()
 *   bwn_gpio_init()
 *   bwn_mac_switch_freq()
 *   bwn_phy_g_attach()
 *   bwn_phy_g_init_sub()
 *   bwn_phy_g_prepare_hw()
 *   bwn_phy_getinfo()
 *   bwn_phy_lp_calib()
 *   bwn_set_opmode()
 *   bwn_sprom_bugfixes()
 * ... and 9 others
 * 
 */
static uint16_t
bhnd_compat_get_chipid(device_t dev)
{
	return (bhnd_get_chipid(dev)->chip_id);
}

/*
 * siba_get_chiprev()
 *
 * Referenced by:
 *   bwn_phy_getinfo()
 *   bwn_phy_lp_bbinit_r2()
 *   bwn_phy_lp_tblinit_r2()
 *   bwn_set_opmode()
 */
static uint16_t
bhnd_compat_get_chiprev(device_t dev)
{
	return (bhnd_get_chipid(dev)->chip_rev);
}

/*
 * siba_get_chippkg()
 *
 * Referenced by:
 *   bwn_phy_g_init_sub()
 *   bwn_phy_lp_bbinit_r01()
 *   bwn_radio_2056_setup()
 */
static uint8_t
bhnd_compat_get_chippkg(device_t dev)
{
	return (bhnd_get_chipid(dev)->chip_pkg);
}

/*
 * siba_get_type()
 *
 * Referenced by:
 *   bwn_core_init()
 *   bwn_dma_attach()
 *   bwn_nphy_op_prepare_structs()
 *   bwn_sprom_bugfixes()
 */
static enum siba_type
bhnd_compat_get_type(device_t dev)
{
	device_t		bus, hostb;
	bhnd_devclass_t		hostb_devclass;

	bus = device_get_parent(dev);
	hostb = bhnd_bus_find_hostb_device(bus);

	if (hostb == NULL)
		return (SIBA_TYPE_SSB);

	hostb_devclass = bhnd_get_class(hostb);
	switch (hostb_devclass) {
	case BHND_DEVCLASS_PCCARD:
		return (SIBA_TYPE_PCMCIA);
	case BHND_DEVCLASS_PCI:
	case BHND_DEVCLASS_PCIE:
		return (SIBA_TYPE_PCI);
	default:
		panic("unsupported hostb devclass: %d\n", hostb_devclass);
	}
}

/*
 * siba_get_cc_pmufreq()
 *
 * Referenced by:
 *   bwn_phy_lp_b2062_init()
 *   bwn_phy_lp_b2062_switch_channel()
 *   bwn_phy_lp_b2063_switch_channel()
 *   bwn_phy_lp_rxcal_r2()
 */
static uint32_t
bhnd_compat_get_cc_pmufreq(device_t dev)
{
	u_int	freq;
	int	error;

	if ((error = bhnd_get_clock_freq(dev, BHND_CLOCK_ALP, &freq)))
		panic("failed to fetch clock frequency: %d", error);

	/* TODO: bwn(4) immediately multiplies the result by 1000 (MHz -> Hz) */
	return (freq / 1000);
}

/*
 * siba_get_cc_caps()
 *
 * Referenced by:
 *   bwn_phy_lp_b2062_init()
 */
static uint32_t
bhnd_compat_get_cc_caps(device_t dev)
{
	device_t		 chipc;
	const struct chipc_caps	*ccaps;
	uint32_t		 result;

	/* Fetch our ChipCommon device */
	chipc = bhnd_retain_provider(dev, BHND_SERVICE_CHIPC);
	if (chipc == NULL)
		panic("missing ChipCommon device");

	/*
	 * The ChipCommon capability flags are only used in one LP-PHY function,
	 * to assert that a PMU is in fact available.
	 *
	 * We can support this by producing a value containing just that flag. 
	 */
	result = 0;
	ccaps = BHND_CHIPC_GET_CAPS(chipc);
	if (ccaps->pmu)
		result |= SIBA_CC_CAPS_PMU;

	bhnd_release_provider(dev, chipc, BHND_SERVICE_CHIPC);

	return (result);
}

/*
 * siba_get_cc_powerdelay()
 *
 * Referenced by:
 *   bwn_chip_init()
 */
static uint16_t
bhnd_compat_get_cc_powerdelay(device_t dev)
{
	u_int	 delay;
	int	 error;

	if ((error = bhnd_get_clock_latency(dev, BHND_CLOCK_HT, &delay)))
		panic("failed to fetch clock latency: %d", error);

	if (delay > UINT16_MAX)
		panic("%#x would overflow", delay);

	return (delay);
}

/*
 * siba_get_pcicore_revid()
 *
 * Referenced by:
 *   bwn_core_init()
 */
static uint8_t
bhnd_compat_get_pcicore_revid(device_t dev)
{
	device_t	hostb;
	uint8_t		nomatch_revid;

	/* 
	 * This is used by bwn(4) in only bwn_core_init(), where a revid <= 10
	 * results in the BWN_HF_PCI_SLOWCLOCK_WORKAROUND workaround being
	 * enabled.
	 * 
	 * The quirk should only be applied on siba(4) devices using a PCI
	 * core; we handle around this by returning a bogus value >= 10 here.
	 * 
	 * TODO: bwn(4) should match this quirk on:
	 *	- BHND_CHIPTYPE_SIBA
	 *	- BHND_COREID_PCI
	 *	- HWREV_LTE(10)
	 */
	nomatch_revid = 0xFF;

	hostb = bhnd_bus_find_hostb_device(device_get_parent(dev));
	if (hostb == NULL) {
		/* Not a bridged device */
		return (nomatch_revid);
	}

	if (bhnd_get_device(hostb) != BHND_COREID_PCI) {
		/* Not a PCI core */
		return (nomatch_revid);
	}

	/* This is a PCI core; we can return the real core revision */
	return (bhnd_get_hwrev(hostb));
}

/*
 * siba_sprom_get_rev()
 *
 * Referenced by:
 *   bwn_nphy_op_prepare_structs()
 *   bwn_nphy_tx_power_ctl_setup()
 *   bwn_nphy_tx_power_fix()
 *   bwn_nphy_workarounds_rev7plus()
 */
static uint8_t
bhnd_compat_sprom_get_rev(device_t dev)
{
	return (bwn_bhnd_get_ctx(dev)->sromrev);
}

/*
 * siba_sprom_get_mac_80211bg()
 *
 * Referenced by:
 *   bwn_attach_post()
 */
static uint8_t *
bhnd_compat_sprom_get_mac_80211bg(device_t dev)
{
	/* 'MAC_80211BG' is il0macaddr or macaddr*/
	return (bwn_bhnd_get_ctx(dev)->mac_80211bg);
}

/*
 * siba_sprom_get_mac_80211a()
 *
 * Referenced by:
 *   bwn_attach_post()
 */
static uint8_t *
bhnd_compat_sprom_get_mac_80211a(device_t dev)
{
	/* 'MAC_80211A' is et1macaddr */
	return (bwn_bhnd_get_ctx(dev)->mac_80211a);
}

/*
 * siba_sprom_get_brev()
 *
 * Referenced by:
 *   bwn_radio_init2055_post()
 */
static uint8_t
bhnd_compat_sprom_get_brev(device_t dev)
{
	/* TODO: bwn(4) needs to switch to uint16_t */
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_BOARDREV);
}

/*
 * siba_sprom_get_ccode()
 *
 * Referenced by:
 *   bwn_phy_g_switch_chan()
 */
static uint8_t
bhnd_compat_sprom_get_ccode(device_t dev)
{
	/* This has been replaced with 'ccode' in later SPROM
	 * revisions, but this API is only called on devices with
	 * spromrev 1. */
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_CC);
}

/*
 * siba_sprom_get_ant_a()
 *
 * Referenced by:
 *   bwn_antenna_sanitize()
 */
static uint8_t
bhnd_compat_sprom_get_ant_a(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_AA5G);
}

/*
 * siba_sprom_get_ant_bg()
 *
 * Referenced by:
 *   bwn_antenna_sanitize()
 */
static uint8_t
bhnd_compat_sprom_get_ant_bg(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_AA2G);
}

/*
 * siba_sprom_get_pa0b0()
 *
 * Referenced by:
 *   bwn_phy_g_attach()
 */
static uint16_t
bhnd_compat_sprom_get_pa0b0(device_t dev)
{
	int16_t value;

	BWN_BHND_NVRAM_FETCH_VAR(dev, int16, BHND_NVAR_PA0B0, &value);

	/* TODO: bwn(4) immediately casts this back to int16_t */
	return ((uint16_t)value);
}

/*
 * siba_sprom_get_pa0b1()
 *
 * Referenced by:
 *   bwn_phy_g_attach()
 */
static uint16_t
bhnd_compat_sprom_get_pa0b1(device_t dev)
{
	int16_t value;

	BWN_BHND_NVRAM_FETCH_VAR(dev, int16, BHND_NVAR_PA0B1, &value);

	/* TODO: bwn(4) immediately casts this back to int16_t */
	return ((uint16_t)value);
}

/*
 * siba_sprom_get_pa0b2()
 *
 * Referenced by:
 *   bwn_phy_g_attach()
 */
static uint16_t
bhnd_compat_sprom_get_pa0b2(device_t dev)
{
	int16_t value;

	BWN_BHND_NVRAM_FETCH_VAR(dev, int16, BHND_NVAR_PA0B2, &value);

	/* TODO: bwn(4) immediately casts this back to int16_t */
	return ((uint16_t)value);
}

/**
 * Fetch an led behavior (ledbhX) NVRAM variable value, for use by
 * siba_sprom_get_gpioX().
 * 
 * ('gpioX' are actually the ledbhX NVRAM variables).
 */
static uint8_t
bhnd_compat_sprom_get_ledbh(device_t dev, const char *name)
{
	uint8_t	value;
	int	error;

	error = bhnd_nvram_getvar_uint8(dev, name, &value);
	if (error && error != ENOENT)
		panic("NVRAM variable %s unreadable: %d", name, error);

	/* For some variables (including ledbhX), a value with all bits set is
	 * treated as uninitialized in the SPROM format; our SPROM parser
	 * detects this case and returns ENOENT, but bwn(4) actually expects
	 * to read the raw value 0xFF value. */
	if (error == ENOENT)
		value = 0xFF;

	return (value);
}

/*
 * siba_sprom_get_gpio0()
 *
 * 'gpioX' are actually the led behavior (ledbh) NVRAM variables.
 *
 * Referenced by:
 *   bwn_led_attach()
 */
static uint8_t
bhnd_compat_sprom_get_gpio0(device_t dev)
{
	return (bhnd_compat_sprom_get_ledbh(dev, BHND_NVAR_LEDBH0));
}

/*
 * siba_sprom_get_gpio1()
 *
 * Referenced by:
 *   bwn_led_attach()
 */
static uint8_t
bhnd_compat_sprom_get_gpio1(device_t dev)
{
	return (bhnd_compat_sprom_get_ledbh(dev, BHND_NVAR_LEDBH1));
}

/*
 * siba_sprom_get_gpio2()
 *
 * Referenced by:
 *   bwn_led_attach()
 */
static uint8_t
bhnd_compat_sprom_get_gpio2(device_t dev)
{
	return (bhnd_compat_sprom_get_ledbh(dev, BHND_NVAR_LEDBH2));
}

/*
 * siba_sprom_get_gpio3()
 *
 * Referenced by:
 *   bwn_led_attach()
 */
static uint8_t
bhnd_compat_sprom_get_gpio3(device_t dev)
{
	return (bhnd_compat_sprom_get_ledbh(dev, BHND_NVAR_LEDBH3));
}

/*
 * siba_sprom_get_maxpwr_bg()
 *
 * Referenced by:
 *   bwn_phy_g_recalc_txpwr()
 */
static uint16_t
bhnd_compat_sprom_get_maxpwr_bg(device_t dev)
{
	return (bwn_bhnd_get_ctx(dev)->pa0maxpwr);
}

/*
 * siba_sprom_set_maxpwr_bg()
 *
 * Referenced by:
 *   bwn_phy_g_recalc_txpwr()
 */
static void
bhnd_compat_sprom_set_maxpwr_bg(device_t dev, uint16_t t)
{
	KASSERT(t <= UINT8_MAX, ("invalid maxpwr value %hu", t));
	bwn_bhnd_get_ctx(dev)->pa0maxpwr = t;
}

/*
 * siba_sprom_get_rxpo2g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rxpo2g(device_t dev)
{
	/* Should be signed, but bwn(4) expects an unsigned value */
	BWN_BHND_NVRAM_RETURN_VAR(dev, int8, BHND_NVAR_RXPO2G);
}

/*
 * siba_sprom_get_rxpo5g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rxpo5g(device_t dev)
{
	/* Should be signed, but bwn(4) expects an unsigned value */
	BWN_BHND_NVRAM_RETURN_VAR(dev, int8, BHND_NVAR_RXPO5G);
}

/*
 * siba_sprom_get_tssi_bg()
 *
 * Referenced by:
 *   bwn_phy_g_attach()
 */
static uint8_t
bhnd_compat_sprom_get_tssi_bg(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_PA0ITSSIT);
}

/*
 * siba_sprom_get_tri2g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_tri2g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TRI2G);
}

/*
 * siba_sprom_get_tri5gl()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_tri5gl(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TRI5GL);
}

/*
 * siba_sprom_get_tri5g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_tri5g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TRI5G);
}

/*
 * siba_sprom_get_tri5gh()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_tri5gh(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TRI5GH);
}

/*
 * siba_sprom_get_rssisav2g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rssisav2g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_RSSISAV2G);
}

/*
 * siba_sprom_get_rssismc2g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rssismc2g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_RSSISMC2G);
}

/*
 * siba_sprom_get_rssismf2g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rssismf2g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_RSSISMF2G);
}

/*
 * siba_sprom_get_bxa2g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_bxa2g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_BXA2G);
}

/*
 * siba_sprom_get_rssisav5g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rssisav5g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_RSSISAV5G);
}

/*
 * siba_sprom_get_rssismc5g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rssismc5g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_RSSISMC5G);
}

/*
 * siba_sprom_get_rssismf5g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_rssismf5g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_RSSISMF5G);
}

/*
 * siba_sprom_get_bxa5g()
 *
 * Referenced by:
 *   bwn_phy_lp_readsprom()
 */
static uint8_t
bhnd_compat_sprom_get_bxa5g(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_BXA5G);
}

/*
 * siba_sprom_get_cck2gpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint16_t
bhnd_compat_sprom_get_cck2gpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint16, BHND_NVAR_CCK2GPO);
}

/*
 * siba_sprom_get_ofdm2gpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint32_t
bhnd_compat_sprom_get_ofdm2gpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint32, BHND_NVAR_OFDM2GPO);
}

/*
 * siba_sprom_get_ofdm5glpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint32_t
bhnd_compat_sprom_get_ofdm5glpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint32, BHND_NVAR_OFDM5GLPO);
}

/*
 * siba_sprom_get_ofdm5gpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint32_t
bhnd_compat_sprom_get_ofdm5gpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint32, BHND_NVAR_OFDM5GPO);
}

/*
 * siba_sprom_get_ofdm5ghpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint32_t
bhnd_compat_sprom_get_ofdm5ghpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint32, BHND_NVAR_OFDM5GHPO);
}

/*
 * siba_sprom_set_bf_lo()
 *
 * Referenced by:
 *   bwn_sprom_bugfixes()
 */
static void
bhnd_compat_sprom_set_bf_lo(device_t dev, uint16_t t)
{
	struct bwn_bhnd_ctx *ctx = bwn_bhnd_get_ctx(dev);
	ctx->boardflags &= ~0xFFFF;
	ctx->boardflags |= t;
}

/*
 * siba_sprom_get_bf_lo()
 *
 * Referenced by:
 *   bwn_bt_enable()
 *   bwn_core_init()
 *   bwn_gpio_init()
 *   bwn_loopback_calcgain()
 *   bwn_phy_g_init_sub()
 *   bwn_phy_g_recalc_txpwr()
 *   bwn_phy_g_set_txpwr()
 *   bwn_phy_g_task_60s()
 *   bwn_rx_rssi_calc()
 *   bwn_sprom_bugfixes()
 * ... and 11 others
 * 
 */
static uint16_t
bhnd_compat_sprom_get_bf_lo(device_t dev)
{
	struct bwn_bhnd_ctx *ctx = bwn_bhnd_get_ctx(dev);
	return (ctx->boardflags & UINT16_MAX);
}

/*
 * siba_sprom_get_bf_hi()
 *
 * Referenced by:
 *   bwn_nphy_gain_ctl_workarounds_rev3()
 *   bwn_phy_lp_bbinit_r01()
 *   bwn_phy_lp_tblinit_txgain()
 */
static uint16_t
bhnd_compat_sprom_get_bf_hi(device_t dev)
{
	struct bwn_bhnd_ctx *ctx = bwn_bhnd_get_ctx(dev);
	return (ctx->boardflags >> 16);
}

/*
 * siba_sprom_get_bf2_lo()
 *
 * Referenced by:
 *   bwn_nphy_op_prepare_structs()
 *   bwn_nphy_workarounds_rev1_2()
 *   bwn_nphy_workarounds_rev3plus()
 *   bwn_phy_initn()
 *   bwn_radio_2056_setup()
 *   bwn_radio_init2055_post()
 */
static uint16_t
bhnd_compat_sprom_get_bf2_lo(device_t dev)
{
	uint32_t bf2;

	BWN_BHND_NVRAM_FETCH_VAR(dev, uint32, BHND_NVAR_BOARDFLAGS2, &bf2);
	return (bf2 & UINT16_MAX);
}

/*
 * siba_sprom_get_bf2_hi()
 *
 * Referenced by:
 *   bwn_nphy_workarounds_rev7plus()
 *   bwn_phy_initn()
 *   bwn_radio_2056_setup()
 */
static uint16_t
bhnd_compat_sprom_get_bf2_hi(device_t dev)
{
	uint32_t bf2;

	BWN_BHND_NVRAM_FETCH_VAR(dev, uint32, BHND_NVAR_BOARDFLAGS2, &bf2);
	return (bf2 >> 16);
}

/*
 * siba_sprom_get_fem_2ghz_tssipos()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_ctl_setup()
 */
static uint8_t
bhnd_compat_sprom_get_fem_2ghz_tssipos(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TSSIPOS2G);
}

/*
 * siba_sprom_get_fem_2ghz_extpa_gain()
 *
 * Referenced by:
 *   bwn_nphy_op_prepare_structs()
 */
static uint8_t
bhnd_compat_sprom_get_fem_2ghz_extpa_gain(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_EXTPAGAIN2G);
}

/*
 * siba_sprom_get_fem_2ghz_pdet_range()
 *
 * Referenced by:
 *   bwn_nphy_workarounds_rev3plus()
 */
static uint8_t
bhnd_compat_sprom_get_fem_2ghz_pdet_range(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_PDETRANGE2G);
}

/*
 * siba_sprom_get_fem_2ghz_tr_iso()
 *
 * Referenced by:
 *   bwn_nphy_get_gain_ctl_workaround_ent()
 */
static uint8_t
bhnd_compat_sprom_get_fem_2ghz_tr_iso(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TRISO2G);
}

/*
 * siba_sprom_get_fem_2ghz_antswlut()
 *
 * Referenced by:
 *   bwn_nphy_tables_init_rev3()
 *   bwn_nphy_tables_init_rev7_volatile()
 */
static uint8_t
bhnd_compat_sprom_get_fem_2ghz_antswlut(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_ANTSWCTL2G);
}

/*
 * siba_sprom_get_fem_5ghz_extpa_gain()
 *
 * Referenced by:
 *   bwn_nphy_get_tx_gain_table()
 *   bwn_nphy_op_prepare_structs()
 */
static uint8_t
bhnd_compat_sprom_get_fem_5ghz_extpa_gain(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_EXTPAGAIN5G);
}

/*
 * siba_sprom_get_fem_5ghz_pdet_range()
 *
 * Referenced by:
 *   bwn_nphy_workarounds_rev3plus()
 */
static uint8_t
bhnd_compat_sprom_get_fem_5ghz_pdet_range(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_PDETRANGE5G);
}

/*
 * siba_sprom_get_fem_5ghz_antswlut()
 *
 * Referenced by:
 *   bwn_nphy_tables_init_rev3()
 *   bwn_nphy_tables_init_rev7_volatile()
 */
static uint8_t
bhnd_compat_sprom_get_fem_5ghz_antswlut(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_ANTSWCTL5G);
}

/*
 * siba_sprom_get_txpid_2g_0()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_2g_0(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID2GA0);
}

/*
 * siba_sprom_get_txpid_2g_1()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_2g_1(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID2GA1);
}

/*
 * siba_sprom_get_txpid_5gl_0()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_5gl_0(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID5GLA0);
}

/*
 * siba_sprom_get_txpid_5gl_1()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_5gl_1(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID5GLA1);
}

/*
 * siba_sprom_get_txpid_5g_0()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_5g_0(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID5GA0);
}

/*
 * siba_sprom_get_txpid_5g_1()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_5g_1(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID5GA1);
}

/*
 * siba_sprom_get_txpid_5gh_0()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_5gh_0(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID5GHA0);
}

/*
 * siba_sprom_get_txpid_5gh_1()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_fix()
 */
static uint8_t
bhnd_compat_sprom_get_txpid_5gh_1(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint8, BHND_NVAR_TXPID5GHA1);
}

/*
 * siba_sprom_get_stbcpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint16_t
bhnd_compat_sprom_get_stbcpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint16, BHND_NVAR_STBCPO);
}

/*
 * siba_sprom_get_cddpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static uint16_t
bhnd_compat_sprom_get_cddpo(device_t dev)
{
	BWN_BHND_NVRAM_RETURN_VAR(dev, uint16, BHND_NVAR_CDDPO);
}

/*
 * siba_powerup()
 *
 * Referenced by:
 *   bwn_attach_core()
 *   bwn_core_init()
 */
static void
bhnd_compat_powerup(device_t dev, int dynamic)
{
	struct bwn_bhnd_ctx	*ctx;
	bhnd_clock		 clock;
	int			 error;

	ctx = bwn_bhnd_get_ctx(dev);

	/* On PMU equipped devices, we do not need to issue a clock request
	 * at powerup */
	if (ctx->pmu_dev != NULL)
		return;

	/* Issue a PMU clock request */
	if (dynamic)
		clock = BHND_CLOCK_DYN;
	else
		clock = BHND_CLOCK_HT;

	if ((error = bhnd_request_clock(dev, clock))) {
		device_printf(dev, "%d clock request failed: %d\n",
		    clock, error);
	}

}

/*
 * siba_powerdown()
 *
 * Referenced by:
 *   bwn_attach_core()
 *   bwn_core_exit()
 *   bwn_core_init()
 */
static int
bhnd_compat_powerdown(device_t dev)
{
	int	error;

	/* Suspend the core */
	if ((error = bhnd_suspend_hw(dev, 0)))
		return (error);

	return (0);
}

/*
 * siba_read_2()
 *
 * Referenced by:
 *   bwn_chip_init()
 *   bwn_chiptest()
 *   bwn_dummy_transmission()
 *   bwn_gpio_init()
 *   bwn_phy_getinfo()
 *   bwn_pio_read_2()
 *   bwn_shm_read_2()
 *   bwn_shm_read_4()
 *   bwn_wme_init()
 *   bwn_wme_loadparams()
 * ... and 23 others
 * 
 */
static uint16_t
bhnd_compat_read_2(device_t dev, uint16_t offset)
{
	struct bwn_softc *sc = device_get_softc(dev);

	BWN_ASSERT_VALID_REG(dev, offset);

	return (bhnd_bus_read_2(sc->sc_mem_res, offset));
}

/*
 * siba_write_2()
 *
 * Referenced by:
 *   bwn_chip_init()
 *   bwn_chiptest()
 *   bwn_crypt_init()
 *   bwn_gpio_init()
 *   bwn_phy_getinfo()
 *   bwn_pio_tx_start()
 *   bwn_set_opmode()
 *   bwn_shm_write_2()
 *   bwn_shm_write_4()
 *   bwn_wme_init()
 * ... and 43 others
 * 
 */
static void
bhnd_compat_write_2(device_t dev, uint16_t offset, uint16_t value)
{
	struct bwn_softc *sc = device_get_softc(dev);

	BWN_ASSERT_VALID_REG(dev, offset);

	return (bhnd_bus_write_2(sc->sc_mem_res, offset, value));
}

/*
 * siba_read_4()
 *
 * Referenced by:
 *   bwn_attach_core()
 *   bwn_chip_init()
 *   bwn_chiptest()
 *   bwn_core_exit()
 *   bwn_core_init()
 *   bwn_core_start()
 *   bwn_pio_init()
 *   bwn_pio_tx_start()
 *   bwn_reset_core()
 *   bwn_shm_read_4()
 * ... and 42 others
 * 
 */
static uint32_t
bhnd_compat_read_4(device_t dev, uint16_t offset)
{
	struct bwn_softc	*sc = device_get_softc(dev);
	uint16_t		 ioreg;
	int			 error;

	/* bwn(4) fetches IOCTL/IOST values directly from siba-specific target
	 * state registers; we map these directly to bhnd_read_(ioctl|iost) */
	switch (offset) {
	case SB0_REG_ABS(SIBA_CFG0_TMSTATELOW):
		if ((error = bhnd_read_ioctl(dev, &ioreg)))
			panic("error reading IOCTL: %d\n", error);

		return (((uint32_t)ioreg) << SIBA_TML_SICF_SHIFT);

	case SB0_REG_ABS(SIBA_CFG0_TMSTATEHIGH):
		if ((error = bhnd_read_iost(dev, &ioreg)))
			panic("error reading IOST: %d\n", error);

		return (((uint32_t)ioreg) << SIBA_TMH_SISF_SHIFT);
	}

	/* Otherwise, perform a standard bus read */
	BWN_ASSERT_VALID_REG(dev, offset);
	return (bhnd_bus_read_4(sc->sc_mem_res, offset));
}

/*
 * siba_write_4()
 *
 * Referenced by:
 *   bwn_chip_init()
 *   bwn_chiptest()
 *   bwn_core_exit()
 *   bwn_core_start()
 *   bwn_dma_mask()
 *   bwn_dma_rxdirectfifo()
 *   bwn_pio_init()
 *   bwn_reset_core()
 *   bwn_shm_ctlword()
 *   bwn_shm_write_4()
 * ... and 37 others
 * 
 */
static void
bhnd_compat_write_4(device_t dev, uint16_t offset, uint32_t value)
{
	struct bwn_softc	*sc = device_get_softc(dev);
	uint16_t		 ioctl;
	int			 error;

	/* bwn(4) writes IOCTL values directly to siba-specific target state
	 * registers; we map these directly to bhnd_write_ioctl() */
	if (offset == SB0_REG_ABS(SIBA_CFG0_TMSTATELOW)) {
		/* shift IOCTL flags back down to their original values */
		if (value & ~SIBA_TML_SICF_MASK)
			panic("%s: non-IOCTL flags provided", __FUNCTION__);

		ioctl = (value & SIBA_TML_SICF_MASK) >> SIBA_TML_SICF_SHIFT;

		if ((error = bhnd_write_ioctl(dev, ioctl, UINT16_MAX)))
			panic("error writing IOCTL: %d\n", error);
	} else {
		/* Otherwise, perform a standard bus write */
		BWN_ASSERT_VALID_REG(dev, offset);

		bhnd_bus_write_4(sc->sc_mem_res, offset, value);
	}

	return;
}

/*
 * siba_dev_up()
 *
 * Referenced by:
 *   bwn_reset_core()
 */
static void
bhnd_compat_dev_up(device_t dev, uint32_t flags)
{
	uint16_t	ioctl;
	int		error;

	/* shift IOCTL flags back down to their original values */
	if (flags & ~SIBA_TML_SICF_MASK)
		panic("%s: non-IOCTL flags provided", __FUNCTION__);

	ioctl = (flags & SIBA_TML_SICF_MASK) >> SIBA_TML_SICF_SHIFT;

	/* Perform core reset; note that bwn(4) incorrectly assumes that both
	 * RESET and post-RESET ioctl flags should be identical */
	if ((error = bhnd_reset_hw(dev, ioctl, ioctl)))
		panic("%s: core reset failed: %d", __FUNCTION__, error);
}

/*
 * siba_dev_down()
 *
 * Referenced by:
 *   bwn_attach_core()
 *   bwn_core_exit()
 */
static void
bhnd_compat_dev_down(device_t dev, uint32_t flags)
{
	uint16_t	ioctl;
	int		error;

	/* shift IOCTL flags back down to their original values */
	if (flags & ~SIBA_TML_SICF_MASK)
		panic("%s: non-IOCTL flags provided", __FUNCTION__);

	ioctl = (flags & SIBA_TML_SICF_MASK) >> SIBA_TML_SICF_SHIFT;

	/* Put core into RESET state */
	if ((error = bhnd_suspend_hw(dev, ioctl)))
		panic("%s: core suspend failed: %d", __FUNCTION__, error);
}

/*
 * siba_dev_isup()
 *
 * Referenced by:
 *   bwn_core_init()
 */
static int
bhnd_compat_dev_isup(device_t dev)
{
	return (!bhnd_is_hw_suspended(dev));
}

/*
 * siba_pcicore_intr()
 *
 * Referenced by:
 *   bwn_core_init()
 */
static void
bhnd_compat_pcicore_intr(device_t dev)
{
	/* This is handled by bhnd_bhndb on the first call to
	 * bus_setup_intr() */
}

/*
 * siba_dma_translation()
 *
 * Referenced by:
 *   bwn_dma_32_setdesc()
 *   bwn_dma_64_setdesc()
 *   bwn_dma_setup()
 */
static uint32_t
bhnd_compat_dma_translation(device_t dev)
{
	struct bhnd_dma_translation	 dt;
	struct bwn_softc		*sc;
	struct bwn_mac			*mac;
	int				 bwn_dmatype, error;

	sc = device_get_softc(dev);
	mac = sc->sc_curmac;
	KASSERT(mac != NULL, ("no MAC"));

	/* Fetch our DMA translation */
	bwn_dmatype = mac->mac_method.dma.dmatype;
	if ((error = bhnd_get_dma_translation(dev, bwn_dmatype, 0, NULL, &dt)))
		panic("error requesting DMA translation: %d\n", error);

	/*
	 * TODO: bwn(4) needs to switch to bhnd_get_dma_translation().
	 *
	 * Currently, bwn(4) incorrectly assumes that:
	 *  - The 32-bit translation mask is always SIBA_DMA_TRANSLATION_MASK.
	 *  - The 32-bit mask can simply be applied to the top 32-bits of a
	 *    64-bit DMA address.
	 *  - The 64-bit address translation is always derived by shifting the
	 *    32-bit siba_dma_translation() left by 1 bit.
	 *
	 * In practice, these assumptions won't result in any bugs on known
	 * PCI/PCIe Wi-Fi hardware:
	 *  - The 32-bit mask _is_ always SIBA_DMA_TRANSLATION_MASK on
	 *    the subset of devices supported by bwn(4).
	 *  - The 64-bit mask used by bwn(4) is a superset of the real
	 *    mask, and thus:
	 *	- Our DMA tag will still have valid constraints.
	 *	- Our address translation will not be corrupted by
	 *	  applying the mask.
	 *  - The mask falls within the top 16 address bits, and our
	 *    supported 64-bit architectures are all still limited
	 *    to 48-bit addresses anyway; we don't need to worry about
	 *    addressing >= 48-bit host memory.
	 *
	 * However, we will need to resolve these issues in bwn(4) if DMA is to
	 * work on new hardware (e.g. WiSoCs).
	 */
	switch (bwn_dmatype) {
	case BWN_DMA_32BIT:
	case BWN_DMA_30BIT:
		KASSERT((~dt.addr_mask & BHND_DMA_ADDR_BITMASK(32)) ==
		    SIBA_DMA_TRANSLATION_MASK, ("unexpected DMA mask: %#jx",
		    (uintmax_t)dt.addr_mask));

		return (dt.base_addr);

	case BWN_DMA_64BIT:
		/* bwn(4) will shift this left by 32+1 bits before applying it
		 * to the top 32-bits of the DMA address */
		KASSERT((~dt.addr_mask & BHND_DMA_ADDR_BITMASK(33)) == 0,
		    ("DMA64 translation %#jx masks low 33-bits",
		     (uintmax_t)dt.addr_mask));

		return (dt.base_addr >> 33);

	default:
		panic("unknown dma type %d", bwn_dmatype);
	}
}

/*
 * siba_read_multi_2()
 *
 * Referenced by:
 *   bwn_pio_rxeof()
 */
static void
bhnd_compat_read_multi_2(device_t dev, void *buffer, size_t count,
    uint16_t offset)
{
	struct bwn_softc *sc = device_get_softc(dev);

	BWN_ASSERT_VALID_REG(dev, offset);
	return (bhnd_bus_read_multi_2(sc->sc_mem_res, offset, buffer, count));
}

/*
 * siba_read_multi_4()
 *
 * Referenced by:
 *   bwn_pio_rxeof()
 */
static void
bhnd_compat_read_multi_4(device_t dev, void *buffer, size_t count,
    uint16_t offset)
{
	struct bwn_softc *sc = device_get_softc(dev);

	BWN_ASSERT_VALID_REG(dev, offset);
	return (bhnd_bus_read_multi_4(sc->sc_mem_res, offset, buffer, count));
}

/*
 * siba_write_multi_2()
 *
 * Referenced by:
 *   bwn_pio_write_multi_2()
 */
static void
bhnd_compat_write_multi_2(device_t dev, const void *buffer, size_t count,
    uint16_t offset)
{
	struct bwn_softc *sc = device_get_softc(dev);

	BWN_ASSERT_VALID_REG(dev, offset);

	/* XXX discarding const to maintain API compatibility with
	 * siba_write_multi_2() */
	bhnd_bus_write_multi_2(sc->sc_mem_res, offset,
	    __DECONST(void *, buffer), count);
}

/*
 * siba_write_multi_4()
 *
 * Referenced by:
 *   bwn_pio_write_multi_4()
 */
static void
bhnd_compat_write_multi_4(device_t dev, const void *buffer, size_t count,
    uint16_t offset)
{
	struct bwn_softc *sc = device_get_softc(dev);

	BWN_ASSERT_VALID_REG(dev, offset);

	/* XXX discarding const to maintain API compatibility with
	 * siba_write_multi_4() */
	bhnd_bus_write_multi_4(sc->sc_mem_res, offset,
	    __DECONST(void *, buffer), count);
}

/*
 * siba_barrier()
 *
 * Referenced by:
 *   bwn_intr()
 *   bwn_intrtask()
 *   bwn_ram_write()
 */
static void
bhnd_compat_barrier(device_t dev, int flags)
{
	struct bwn_softc *sc = device_get_softc(dev);

	/* XXX is siba_barrier()'s use of an offset and length of 0
	 * correct? */
	BWN_ASSERT_VALID_REG(dev, 0);
	bhnd_bus_barrier(sc->sc_mem_res, 0, 0, flags);
}

/*
 * siba_cc_pmu_set_ldovolt()
 *
 * Referenced by:
 *   bwn_phy_lp_bbinit_r01()
 */
static void
bhnd_compat_cc_pmu_set_ldovolt(device_t dev, int id, uint32_t volt)
{
	struct bwn_bhnd_ctx	*ctx;
	int			 error;

	ctx = bwn_bhnd_get_ctx(dev);

	/* Only ever used to set the PAREF LDO voltage */
	if (id != SIBA_LDO_PAREF)
		panic("invalid LDO id: %d", id);

	/* Configuring regulator voltage requires a PMU */
	if (ctx->pmu_dev == NULL)
		panic("no PMU; cannot set LDO voltage");

	error = bhnd_pmu_set_voltage_raw(ctx->pmu_dev, BHND_REGULATOR_PAREF_LDO,
	    volt);
	if (error)
		panic("failed to set LDO voltage: %d", error);
}

/*
 * siba_cc_pmu_set_ldoparef()
 *
 * Referenced by:
 *   bwn_phy_lp_bbinit_r01()
 */
static void
bhnd_compat_cc_pmu_set_ldoparef(device_t dev, uint8_t on)
{
	struct bwn_bhnd_ctx	*ctx;
	int			 error;

	ctx = bwn_bhnd_get_ctx(dev);

	/* Enabling/disabling regulators requires a PMU */
	if (ctx->pmu_dev == NULL)
		panic("no PMU; cannot set LDO voltage");

	if (on) {
		error = bhnd_pmu_enable_regulator(ctx->pmu_dev,
		    BHND_REGULATOR_PAREF_LDO);
	} else {
		error = bhnd_pmu_enable_regulator(ctx->pmu_dev,
		    BHND_REGULATOR_PAREF_LDO);
	}

	if (error) {
		panic("failed to %s PAREF_LDO: %d", on ? "enable" : "disable",
		    error);
	}
}

/*
 * siba_gpio_set()
 *
 * Referenced by:
 *   bwn_chip_exit()
 *   bwn_chip_init()
 *   bwn_gpio_init()
 *   bwn_nphy_superswitch_init()
 */
static void
bhnd_compat_gpio_set(device_t dev, uint32_t value)
{
	struct bwn_bhnd_ctx	*ctx;
	uint32_t		 flags[32];
	int			 error;

	ctx = bwn_bhnd_get_ctx(dev);

	for (size_t i = 0; i < nitems(flags); i++) {
		if (value & (1 << i)) {
			/* Tristate pin */
			flags[i] = (GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);
		} else {
			/* Leave unmodified */
			flags[i] = 0;
		}
	}

	error = GPIO_PIN_CONFIG_32(ctx->gpio_dev, 0, nitems(flags), flags);
	if (error)
		panic("error configuring pin flags: %d", error);
}

/*
 * siba_gpio_get()
 *
 * Referenced by:
 *   bwn_gpio_init()
 */
static uint32_t
bhnd_compat_gpio_get(device_t dev)
{
	struct bwn_bhnd_ctx	*ctx;
	uint32_t		 ctrl;
	int			 npin;
	int			 error;

	/*
	 * We recreate the expected GPIOCTRL register value for bwn_gpio_init()
	 * by querying pins individually for GPIO_PIN_TRISTATE.
	 * 
	 * Once we drop these compatibility shims, the GPIO_PIN_CONFIG_32 method
	 * can be used to set pin configuration without bwn(4) externally
	 * implementing RMW.
	 */

	/* Fetch the total pin count */
	ctx = bwn_bhnd_get_ctx(dev);
	if ((error = GPIO_PIN_MAX(ctx->gpio_dev, &npin)))
		panic("failed to fetch max pin: %d", error);

	/* Must be representable within a 32-bit GPIOCTRL register value */
	KASSERT(npin <= 32, ("unsupported pin count: %u", npin));

	ctrl = 0;
	for (uint32_t pin = 0; pin < npin; pin++) {
		uint32_t flags;

		if ((error = GPIO_PIN_GETFLAGS(ctx->gpio_dev, pin, &flags)))
			panic("error fetching pin%u flags: %d", pin, error);

		if (flags & GPIO_PIN_TRISTATE)
			ctrl |= (1 << pin);
	}

	return (ctrl);
}

/*
 * siba_fix_imcfglobug()
 *
 * Referenced by:
 *   bwn_core_init()
 */
static void
bhnd_compat_fix_imcfglobug(device_t dev)
{
	/* This is handled by siba_bhndb during attach/resume */
}


/* Core power NVRAM variables, indexed by D11 core unit number */
static const struct bwn_power_vars {
	const char *itt2ga;
	const char *itt5ga;
	const char *maxp2ga;
	const char *pa2ga;
	const char *pa5ga;
} bwn_power_vars[BWN_BHND_NUM_CORE_PWR] = {
#define	BHND_POWER_NVAR(_idx)					\
	{ BHND_NVAR_ITT2GA ## _idx, BHND_NVAR_ITT5GA ## _idx,	\
	  BHND_NVAR_MAXP2GA ## _idx, BHND_NVAR_PA2GA ## _idx,	\
	  BHND_NVAR_PA5GA ## _idx }
	BHND_POWER_NVAR(0),
	BHND_POWER_NVAR(1),
	BHND_POWER_NVAR(2),
	BHND_POWER_NVAR(3)
#undef BHND_POWER_NVAR
};

static int
bwn_get_core_power_info_r11(device_t dev, const struct bwn_power_vars *v,
    struct siba_sprom_core_pwr_info *c)
{
	int16_t	pa5ga[12];
	int	error;

	/* BHND_NVAR_PA2GA[core] */
	error = bhnd_nvram_getvar_array(dev, v->pa2ga, c->pa_2g,
	    sizeof(c->pa_2g), BHND_NVRAM_TYPE_INT16);
	if (error)
		return (error);

	/* 
	 * BHND_NVAR_PA5GA
	 * 
	 * The NVRAM variable is defined as a single pa5ga[12] array; we have
	 * to split this into pa_5gl[4], pa_5g[4], and pa_5gh[4] for use
	 * by bwn(4);
	 */
	_Static_assert(nitems(pa5ga) == nitems(c->pa_5g) + nitems(c->pa_5gh) +
	    nitems(c->pa_5gl), "cannot split pa5ga into pa_5gl/pa_5g/pa_5gh");

	error = bhnd_nvram_getvar_array(dev, v->pa5ga, pa5ga, sizeof(pa5ga),
	    BHND_NVRAM_TYPE_INT16);
	if (error)
		return (error);

	memcpy(c->pa_5gl, &pa5ga[0], sizeof(c->pa_5gl));
	memcpy(c->pa_5g, &pa5ga[4], sizeof(c->pa_5g));
	memcpy(c->pa_5gh, &pa5ga[8], sizeof(c->pa_5gh));
	return (0);
}

static int
bwn_get_core_power_info_r4_r10(device_t dev,
    const struct bwn_power_vars *v, struct siba_sprom_core_pwr_info *c)
{
	int error;

	/* BHND_NVAR_ITT2GA[core] */
	if ((error = bhnd_nvram_getvar_uint8(dev, v->itt2ga, &c->itssi_2g)))
		return (error);

	/* BHND_NVAR_ITT5GA[core] */
	if ((error = bhnd_nvram_getvar_uint8(dev, v->itt5ga, &c->itssi_5g)))
		return (error);

	return (0);
}

/*
 * siba_sprom_get_core_power_info()
 *
 * Referenced by:
 *   bwn_nphy_tx_power_ctl_setup()
 *   bwn_ppr_load_max_from_sprom()
 */
static int
bhnd_compat_sprom_get_core_power_info(device_t dev, int core,
    struct siba_sprom_core_pwr_info *c)
{
	struct bwn_bhnd_ctx		*ctx;
	const struct bwn_power_vars	*v;
	int				 error;

	if (core < 0 || core >= nitems(bwn_power_vars))
		return (EINVAL);

	ctx = bwn_bhnd_get_ctx(dev);
	if (ctx->sromrev < 4)
		return (ENXIO);

	v = &bwn_power_vars[core];

	/* Any power variables not found in NVRAM (or returning a
	 * shorter array for a particular NVRAM revision) should be zero
	 * initialized */
	memset(c, 0x0, sizeof(*c));

	/* Populate SPROM revision-independent values */
	if ((error = bhnd_nvram_getvar_uint8(dev, v->maxp2ga, &c->maxpwr_2g)))
		return (error);

	/* Populate SPROM revision-specific values */
	if (ctx->sromrev >= 4 && ctx->sromrev <= 10)
		return (bwn_get_core_power_info_r4_r10(dev, v, c));
	else
		return (bwn_get_core_power_info_r11(dev, v, c));
}

/*
 * siba_sprom_get_mcs2gpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static int
bhnd_compat_sprom_get_mcs2gpo(device_t dev, uint16_t *c)
{
	static const char *varnames[] = {
		BHND_NVAR_MCS2GPO0,
		BHND_NVAR_MCS2GPO1,
		BHND_NVAR_MCS2GPO2,
		BHND_NVAR_MCS2GPO3,
		BHND_NVAR_MCS2GPO4,
		BHND_NVAR_MCS2GPO5,
		BHND_NVAR_MCS2GPO6,
		BHND_NVAR_MCS2GPO7
	};

	for (size_t i = 0; i < nitems(varnames); i++) {
		const char *name = varnames[i];
		BWN_BHND_NVRAM_FETCH_VAR(dev, uint16, name, &c[i]);
	}

	return (0);
}

/*
 * siba_sprom_get_mcs5glpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static int
bhnd_compat_sprom_get_mcs5glpo(device_t dev, uint16_t *c)
{
	static const char *varnames[] = {
		BHND_NVAR_MCS5GLPO0,
		BHND_NVAR_MCS5GLPO1,
		BHND_NVAR_MCS5GLPO2,
		BHND_NVAR_MCS5GLPO3,
		BHND_NVAR_MCS5GLPO4,
		BHND_NVAR_MCS5GLPO5,
		BHND_NVAR_MCS5GLPO6,
		BHND_NVAR_MCS5GLPO7
	};

	for (size_t i = 0; i < nitems(varnames); i++) {
		const char *name = varnames[i];
		BWN_BHND_NVRAM_FETCH_VAR(dev, uint16, name, &c[i]);
	}

	return (0);
}

/*
 * siba_sprom_get_mcs5gpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static int
bhnd_compat_sprom_get_mcs5gpo(device_t dev, uint16_t *c)
{
	static const char *varnames[] = {
		BHND_NVAR_MCS5GPO0,
		BHND_NVAR_MCS5GPO1,
		BHND_NVAR_MCS5GPO2,
		BHND_NVAR_MCS5GPO3,
		BHND_NVAR_MCS5GPO4,
		BHND_NVAR_MCS5GPO5,
		BHND_NVAR_MCS5GPO6,
		BHND_NVAR_MCS5GPO7
	};

	for (size_t i = 0; i < nitems(varnames); i++) {
		const char *name = varnames[i];
		BWN_BHND_NVRAM_FETCH_VAR(dev, uint16, name, &c[i]);
	}

	return (0);
}

/*
 * siba_sprom_get_mcs5ghpo()
 *
 * Referenced by:
 *   bwn_ppr_load_max_from_sprom()
 */
static int
bhnd_compat_sprom_get_mcs5ghpo(device_t dev, uint16_t *c)
{
	static const char *varnames[] = {
		BHND_NVAR_MCS5GHPO0,
		BHND_NVAR_MCS5GHPO1,
		BHND_NVAR_MCS5GHPO2,
		BHND_NVAR_MCS5GHPO3,
		BHND_NVAR_MCS5GHPO4,
		BHND_NVAR_MCS5GHPO5,
		BHND_NVAR_MCS5GHPO6,
		BHND_NVAR_MCS5GHPO7
	};

	for (size_t i = 0; i < nitems(varnames); i++) {
		const char *name = varnames[i];
		BWN_BHND_NVRAM_FETCH_VAR(dev, uint16, name, &c[i]);
	}

	return (0);
}

/*
 * siba_pmu_spuravoid_pllupdate()
 *
 * Referenced by:
 *   bwn_nphy_pmu_spur_avoid()
 */
static void
bhnd_compat_pmu_spuravoid_pllupdate(device_t dev, int spur_avoid)
{
	struct bwn_bhnd_ctx	*ctx;
	bhnd_pmu_spuravoid	 mode;
	int			 error;

	ctx = bwn_bhnd_get_ctx(dev);

	if (ctx->pmu_dev == NULL)
		panic("requested spuravoid on non-PMU device");

	switch (spur_avoid) {
	case 0:
		mode = BHND_PMU_SPURAVOID_NONE;
		break;
	case 1:
		mode = BHND_PMU_SPURAVOID_M1;
		break;
	default:
		panic("unknown spur_avoid: %d", spur_avoid);
	}

	if ((error = bhnd_pmu_request_spuravoid(ctx->pmu_dev, mode)))
		panic("spuravoid request failed: %d", error);
}

/*
 * siba_cc_set32()
 *
 * Referenced by:
 *   bwn_phy_initn()
 *   bwn_wireless_core_phy_pll_reset()
 */
static void
bhnd_compat_cc_set32(device_t dev, uint32_t reg, uint32_t val)
{
	struct bwn_bhnd_ctx *ctx = bwn_bhnd_get_ctx(dev);
	
	/*
	 * OR with the current value.
	 *
	 * This function is only ever used to write to either ChipCommon's
	 * chipctrl register or chipctl_data register. Note that chipctl_data
	 * is actually a PMU register; it is not actually mapped by ChipCommon
	 * on Always-on-Bus (AOB) devices with a standalone PMU core.
	 */
	if (dev != ctx->chipc_dev)
		panic("unsupported device: %s", device_get_nameunit(dev));

	switch (reg) {
	case SIBA_CC_CHIPCTL:
		BHND_CHIPC_WRITE_CHIPCTRL(ctx->chipc_dev, val, val);
		break;
	case SIBA_CC_CHIPCTL_DATA:
		bhnd_pmu_write_chipctrl(ctx->pmu_dev, ctx->pmu_cctl_addr, val,
		    val);
		break;
	default:
		panic("unsupported register: %#x", reg);
	}
}

/*
 * siba_cc_mask32()
 *
 * Referenced by:
 *   bwn_wireless_core_phy_pll_reset()
 */
static void
bhnd_compat_cc_mask32(device_t dev, uint32_t reg, uint32_t mask)
{
	struct bwn_bhnd_ctx *ctx = bwn_bhnd_get_ctx(dev);

	/*
	 * AND with the current value.
	 *
	 * This function is only ever used to write to ChipCommon's chipctl_data
	 * register. Note that chipctl_data is actually a PMU register; it is
	 * not actually mapped by ChipCommon on Always-on-Bus (AOB) devices with
	 * a standalone PMU core.
	 */
	if (dev != ctx->chipc_dev)
		panic("unsupported device: %s", device_get_nameunit(dev));

	switch (reg) {
	case SIBA_CC_CHIPCTL_DATA:
		bhnd_pmu_write_chipctrl(ctx->pmu_dev, ctx->pmu_cctl_addr, 0,
		    ~mask);
		break;
	default:
		panic("unsupported register: %#x", reg);
	}
}

/*
 * siba_cc_write32()
 *
 * Referenced by:
 *   bwn_wireless_core_phy_pll_reset()
 */
static void
bhnd_compat_cc_write32(device_t dev, uint32_t reg, uint32_t val)
{
	struct bwn_bhnd_ctx *ctx = bwn_bhnd_get_ctx(dev);

	/*
	 * This function is only ever used to write to ChipCommon's chipctl_addr
	 * register; setting chipctl_addr is handled atomically by
	 * bhnd_pmu_write_chipctrl(), so we merely cache the intended address
	 * for later use when chipctl_data is written.
	 *
	 * Also, note that chipctl_addr is actually a PMU register; it is
	 * not actually mapped by ChipCommon on Always-on-Bus (AOB) devices with
	 * a standalone PMU core.
	 */
	if (dev != ctx->chipc_dev)
		panic("unsupported device: %s", device_get_nameunit(dev));

	switch (reg) {
	case SIBA_CC_CHIPCTL_ADDR:
		ctx->pmu_cctl_addr = val;
		break;
	default:
		panic("unsupported register: %#x", reg);
	}
}

const struct bwn_bus_ops bwn_bhnd_bus_ops = {
	.init				= bwn_bhnd_bus_ops_init,
	.fini				= bwn_bhnd_bus_ops_fini,
	.pci_find_cap			= bhnd_compat_pci_find_cap,
	.pci_alloc_msi			= bhnd_compat_pci_alloc_msi,
	.pci_release_msi		= bhnd_compat_pci_release_msi,
	.pci_msi_count			= bhnd_compat_pci_msi_count,
	.get_vendor			= bhnd_compat_get_vendor,
	.get_device			= bhnd_compat_get_device,
	.get_revid			= bhnd_compat_get_revid,
	.get_pci_vendor			= bhnd_compat_get_pci_vendor,
	.get_pci_device			= bhnd_compat_get_pci_device,
	.get_pci_subvendor		= bhnd_compat_get_pci_subvendor,
	.get_pci_subdevice		= bhnd_compat_get_pci_subdevice,
	.get_pci_revid			= bhnd_compat_get_pci_revid,
	.get_chipid			= bhnd_compat_get_chipid,
	.get_chiprev			= bhnd_compat_get_chiprev,
	.get_chippkg			= bhnd_compat_get_chippkg,
	.get_type			= bhnd_compat_get_type,
	.get_cc_pmufreq			= bhnd_compat_get_cc_pmufreq,
	.get_cc_caps			= bhnd_compat_get_cc_caps,
	.get_cc_powerdelay		= bhnd_compat_get_cc_powerdelay,
	.get_pcicore_revid		= bhnd_compat_get_pcicore_revid,
	.sprom_get_rev			= bhnd_compat_sprom_get_rev,
	.sprom_get_mac_80211bg		= bhnd_compat_sprom_get_mac_80211bg,
	.sprom_get_mac_80211a		= bhnd_compat_sprom_get_mac_80211a,
	.sprom_get_brev			= bhnd_compat_sprom_get_brev,
	.sprom_get_ccode		= bhnd_compat_sprom_get_ccode,
	.sprom_get_ant_a		= bhnd_compat_sprom_get_ant_a,
	.sprom_get_ant_bg		= bhnd_compat_sprom_get_ant_bg,
	.sprom_get_pa0b0		= bhnd_compat_sprom_get_pa0b0,
	.sprom_get_pa0b1		= bhnd_compat_sprom_get_pa0b1,
	.sprom_get_pa0b2		= bhnd_compat_sprom_get_pa0b2,
	.sprom_get_gpio0		= bhnd_compat_sprom_get_gpio0,
	.sprom_get_gpio1		= bhnd_compat_sprom_get_gpio1,
	.sprom_get_gpio2		= bhnd_compat_sprom_get_gpio2,
	.sprom_get_gpio3		= bhnd_compat_sprom_get_gpio3,
	.sprom_get_maxpwr_bg		= bhnd_compat_sprom_get_maxpwr_bg,
	.sprom_set_maxpwr_bg		= bhnd_compat_sprom_set_maxpwr_bg,
	.sprom_get_rxpo2g		= bhnd_compat_sprom_get_rxpo2g,
	.sprom_get_rxpo5g		= bhnd_compat_sprom_get_rxpo5g,
	.sprom_get_tssi_bg		= bhnd_compat_sprom_get_tssi_bg,
	.sprom_get_tri2g		= bhnd_compat_sprom_get_tri2g,
	.sprom_get_tri5gl		= bhnd_compat_sprom_get_tri5gl,
	.sprom_get_tri5g		= bhnd_compat_sprom_get_tri5g,
	.sprom_get_tri5gh		= bhnd_compat_sprom_get_tri5gh,
	.sprom_get_rssisav2g		= bhnd_compat_sprom_get_rssisav2g,
	.sprom_get_rssismc2g		= bhnd_compat_sprom_get_rssismc2g,
	.sprom_get_rssismf2g		= bhnd_compat_sprom_get_rssismf2g,
	.sprom_get_bxa2g		= bhnd_compat_sprom_get_bxa2g,
	.sprom_get_rssisav5g		= bhnd_compat_sprom_get_rssisav5g,
	.sprom_get_rssismc5g		= bhnd_compat_sprom_get_rssismc5g,
	.sprom_get_rssismf5g		= bhnd_compat_sprom_get_rssismf5g,
	.sprom_get_bxa5g		= bhnd_compat_sprom_get_bxa5g,
	.sprom_get_cck2gpo		= bhnd_compat_sprom_get_cck2gpo,
	.sprom_get_ofdm2gpo		= bhnd_compat_sprom_get_ofdm2gpo,
	.sprom_get_ofdm5glpo		= bhnd_compat_sprom_get_ofdm5glpo,
	.sprom_get_ofdm5gpo		= bhnd_compat_sprom_get_ofdm5gpo,
	.sprom_get_ofdm5ghpo		= bhnd_compat_sprom_get_ofdm5ghpo,
	.sprom_get_bf_lo		= bhnd_compat_sprom_get_bf_lo,
	.sprom_set_bf_lo		= bhnd_compat_sprom_set_bf_lo,
	.sprom_get_bf_hi		= bhnd_compat_sprom_get_bf_hi,
	.sprom_get_bf2_lo		= bhnd_compat_sprom_get_bf2_lo,
	.sprom_get_bf2_hi		= bhnd_compat_sprom_get_bf2_hi,
	.sprom_get_fem_2ghz_tssipos	= bhnd_compat_sprom_get_fem_2ghz_tssipos,
	.sprom_get_fem_2ghz_extpa_gain	= bhnd_compat_sprom_get_fem_2ghz_extpa_gain,
	.sprom_get_fem_2ghz_pdet_range	= bhnd_compat_sprom_get_fem_2ghz_pdet_range,
	.sprom_get_fem_2ghz_tr_iso	= bhnd_compat_sprom_get_fem_2ghz_tr_iso,
	.sprom_get_fem_2ghz_antswlut	= bhnd_compat_sprom_get_fem_2ghz_antswlut,
	.sprom_get_fem_5ghz_extpa_gain	= bhnd_compat_sprom_get_fem_5ghz_extpa_gain,
	.sprom_get_fem_5ghz_pdet_range	= bhnd_compat_sprom_get_fem_5ghz_pdet_range,
	.sprom_get_fem_5ghz_antswlut	= bhnd_compat_sprom_get_fem_5ghz_antswlut,
	.sprom_get_txpid_2g_0		= bhnd_compat_sprom_get_txpid_2g_0,
	.sprom_get_txpid_2g_1		= bhnd_compat_sprom_get_txpid_2g_1,
	.sprom_get_txpid_5gl_0		= bhnd_compat_sprom_get_txpid_5gl_0,
	.sprom_get_txpid_5gl_1		= bhnd_compat_sprom_get_txpid_5gl_1,
	.sprom_get_txpid_5g_0		= bhnd_compat_sprom_get_txpid_5g_0,
	.sprom_get_txpid_5g_1		= bhnd_compat_sprom_get_txpid_5g_1,
	.sprom_get_txpid_5gh_0		= bhnd_compat_sprom_get_txpid_5gh_0,
	.sprom_get_txpid_5gh_1		= bhnd_compat_sprom_get_txpid_5gh_1,
	.sprom_get_stbcpo		= bhnd_compat_sprom_get_stbcpo,
	.sprom_get_cddpo		= bhnd_compat_sprom_get_cddpo,
	.powerup			= bhnd_compat_powerup,
	.powerdown			= bhnd_compat_powerdown,
	.read_2				= bhnd_compat_read_2,
	.write_2			= bhnd_compat_write_2,
	.read_4				= bhnd_compat_read_4,
	.write_4			= bhnd_compat_write_4,
	.dev_up				= bhnd_compat_dev_up,
	.dev_down			= bhnd_compat_dev_down,
	.dev_isup			= bhnd_compat_dev_isup,
	.pcicore_intr			= bhnd_compat_pcicore_intr,
	.dma_translation		= bhnd_compat_dma_translation,
	.read_multi_2			= bhnd_compat_read_multi_2,
	.read_multi_4			= bhnd_compat_read_multi_4,
	.write_multi_2			= bhnd_compat_write_multi_2,
	.write_multi_4			= bhnd_compat_write_multi_4,
	.barrier			= bhnd_compat_barrier,
	.cc_pmu_set_ldovolt		= bhnd_compat_cc_pmu_set_ldovolt,
	.cc_pmu_set_ldoparef		= bhnd_compat_cc_pmu_set_ldoparef,
	.gpio_set			= bhnd_compat_gpio_set,
	.gpio_get			= bhnd_compat_gpio_get,
	.fix_imcfglobug			= bhnd_compat_fix_imcfglobug,
	.sprom_get_core_power_info	= bhnd_compat_sprom_get_core_power_info,
	.sprom_get_mcs2gpo		= bhnd_compat_sprom_get_mcs2gpo,
	.sprom_get_mcs5glpo		= bhnd_compat_sprom_get_mcs5glpo,
	.sprom_get_mcs5gpo		= bhnd_compat_sprom_get_mcs5gpo,
	.sprom_get_mcs5ghpo		= bhnd_compat_sprom_get_mcs5ghpo,
	.pmu_spuravoid_pllupdate	= bhnd_compat_pmu_spuravoid_pllupdate,
	.cc_set32			= bhnd_compat_cc_set32,
	.cc_mask32			= bhnd_compat_cc_mask32,
	.cc_write32			= bhnd_compat_cc_write32,
};
