/*-
 * Copyright (c) 2009-2010 Weongyo Jeong <weongyo@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * the Sonics Silicon Backplane driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_arp.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/siba/siba_ids.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/sibavar.h>

#ifdef SIBA_DEBUG
enum {
	SIBA_DEBUG_SCAN		= 0x00000001,	/* scan */
	SIBA_DEBUG_PMU		= 0x00000002,	/* PMU */
	SIBA_DEBUG_PLL		= 0x00000004,	/* PLL */
	SIBA_DEBUG_SWITCHCORE	= 0x00000008,	/* switching core */
	SIBA_DEBUG_SPROM	= 0x00000010,	/* SPROM */
	SIBA_DEBUG_CORE		= 0x00000020,	/* handling cores */
	SIBA_DEBUG_ANY		= 0xffffffff
};
#define DPRINTF(siba, m, fmt, ...) do {			\
	if (siba->siba_debug & (m))			\
		printf(fmt, __VA_ARGS__);		\
} while (0)
#else
#define DPRINTF(siba, m, fmt, ...) do { (void) siba; } while (0)
#endif
#define	N(a)			(sizeof(a) / sizeof(a[0]))

static void	siba_pci_gpio(struct siba_softc *, uint32_t, int);
static void	siba_scan(struct siba_softc *);
static int	siba_switchcore(struct siba_softc *, uint8_t);
static int	siba_pci_switchcore_sub(struct siba_softc *, uint8_t);
static uint32_t	siba_scan_read_4(struct siba_softc *, uint8_t, uint16_t);
static uint16_t	siba_dev2chipid(struct siba_softc *);
static uint16_t	siba_pci_read_2(struct siba_dev_softc *, uint16_t);
static uint32_t	siba_pci_read_4(struct siba_dev_softc *, uint16_t);
static void	siba_pci_write_2(struct siba_dev_softc *, uint16_t, uint16_t);
static void	siba_pci_write_4(struct siba_dev_softc *, uint16_t, uint32_t);
static void	siba_cc_clock(struct siba_cc *,
		    enum siba_clock);
static void	siba_cc_pmu_init(struct siba_cc *);
static void	siba_cc_power_init(struct siba_cc *);
static void	siba_cc_powerup_delay(struct siba_cc *);
static int	siba_cc_clockfreq(struct siba_cc *, int);
static void	siba_cc_pmu1_pll0_init(struct siba_cc *, uint32_t);
static void	siba_cc_pmu0_pll0_init(struct siba_cc *, uint32_t);
static enum siba_clksrc siba_cc_clksrc(struct siba_cc *);
static const struct siba_cc_pmu1_plltab *siba_cc_pmu1_plltab_find(uint32_t);
static uint32_t	siba_cc_pll_read(struct siba_cc *, uint32_t);
static void	siba_cc_pll_write(struct siba_cc *, uint32_t,
		    uint32_t);
static const struct siba_cc_pmu0_plltab *
		siba_cc_pmu0_plltab_findentry(uint32_t);
static int	siba_pci_sprom(struct siba_softc *, struct siba_sprom *);
static int	siba_sprom_read(struct siba_softc *, uint16_t *, uint16_t);
static int	sprom_check_crc(const uint16_t *, size_t);
static uint8_t	siba_crc8(uint8_t, uint8_t);
static void	siba_sprom_r123(struct siba_sprom *, const uint16_t *);
static void	siba_sprom_r45(struct siba_sprom *, const uint16_t *);
static void	siba_sprom_r8(struct siba_sprom *, const uint16_t *);
static int8_t	siba_sprom_r123_antgain(uint8_t, const uint16_t *, uint16_t,
		    uint16_t);
static uint32_t	siba_tmslow_reject_bitmask(struct siba_dev_softc *);
static uint32_t	siba_pcicore_read_4(struct siba_pci *, uint16_t);
static void	siba_pcicore_write_4(struct siba_pci *, uint16_t, uint32_t);
static uint32_t	siba_pcie_read(struct siba_pci *, uint32_t);
static void	siba_pcie_write(struct siba_pci *, uint32_t, uint32_t);
static void	siba_pcie_mdio_write(struct siba_pci *, uint8_t, uint8_t,
		    uint16_t);
static void	siba_pci_read_multi_1(struct siba_dev_softc *, void *, size_t,
		    uint16_t);
static void	siba_pci_read_multi_2(struct siba_dev_softc *, void *, size_t,
		    uint16_t);
static void	siba_pci_read_multi_4(struct siba_dev_softc *, void *, size_t,
		    uint16_t);
static void	siba_pci_write_multi_1(struct siba_dev_softc *, const void *,
		    size_t, uint16_t);
static void	siba_pci_write_multi_2(struct siba_dev_softc *, const void *,
		    size_t, uint16_t);
static void	siba_pci_write_multi_4(struct siba_dev_softc *, const void *,
		    size_t, uint16_t);
static const char *siba_core_name(uint16_t);
static void	siba_pcicore_init(struct siba_pci *);
static uint32_t	siba_read_4_sub(struct siba_dev_softc *, uint16_t);
static void	siba_write_4_sub(struct siba_dev_softc *, uint16_t, uint32_t);
static void	siba_powerup_sub(struct siba_softc *, int);
static int	siba_powerdown_sub(struct siba_softc *);
static int	siba_dev_isup_sub(struct siba_dev_softc *);
static void	siba_dev_up_sub(struct siba_dev_softc *, uint32_t);
static void	siba_dev_down_sub(struct siba_dev_softc *, uint32_t);
int		siba_core_attach(struct siba_softc *);
int		siba_core_detach(struct siba_softc *);
int		siba_core_suspend(struct siba_softc *);
int		siba_core_resume(struct siba_softc *);
uint8_t		siba_getncores(device_t, uint16_t);

static const struct siba_bus_ops siba_pci_ops = {
	.read_2		= siba_pci_read_2,
	.read_4		= siba_pci_read_4,
	.write_2	= siba_pci_write_2,
	.write_4	= siba_pci_write_4,
	.read_multi_1	= siba_pci_read_multi_1,
	.read_multi_2	= siba_pci_read_multi_2,
	.read_multi_4	= siba_pci_read_multi_4,
	.write_multi_1	= siba_pci_write_multi_1,
	.write_multi_2	= siba_pci_write_multi_2,
	.write_multi_4	= siba_pci_write_multi_4,
};

static const struct siba_cc_pmu_res_updown siba_cc_pmu_4325_updown[] =
    SIBA_CC_PMU_4325_RES_UPDOWN;
static const struct siba_cc_pmu_res_depend siba_cc_pmu_4325_depend[] =
    SIBA_CC_PMU_4325_RES_DEPEND;
static const struct siba_cc_pmu_res_updown siba_cc_pmu_4328_updown[] =
    SIBA_CC_PMU_4328_RES_UPDOWN;
static const struct siba_cc_pmu_res_depend siba_cc_pmu_4328_depend[] =
    SIBA_CC_PMU_4328_RES_DEPEND;
static const struct siba_cc_pmu0_plltab siba_cc_pmu0_plltab[] =
    SIBA_CC_PMU0_PLLTAB_ENTRY;
static const struct siba_cc_pmu1_plltab siba_cc_pmu1_plltab[] =
    SIBA_CC_PMU1_PLLTAB_ENTRY;

int
siba_core_attach(struct siba_softc *siba)
{
	struct siba_cc *scc;
	int error;

	KASSERT(siba->siba_type == SIBA_TYPE_PCI,
	    ("unsupported BUS type (%#x)", siba->siba_type));

	siba->siba_ops = &siba_pci_ops;

	siba_pci_gpio(siba, SIBA_GPIO_CRYSTAL | SIBA_GPIO_PLL, 1);
	siba_scan(siba);

	/* XXX init PCI or PCMCIA host devices */

	siba_powerup_sub(siba, 0);

	/* init ChipCommon */
	scc = &siba->siba_cc;
	if (scc->scc_dev != NULL) {
		siba_cc_pmu_init(scc);
		siba_cc_power_init(scc);
		siba_cc_clock(scc, SIBA_CLOCK_FAST);
		siba_cc_powerup_delay(scc);
	}

	error = siba_pci_sprom(siba, &siba->siba_sprom);
	if (error) {
		siba_powerdown_sub(siba);
		return (error);
	}

	siba_pcicore_init(&siba->siba_pci);
	siba_powerdown_sub(siba);

	return (bus_generic_attach(siba->siba_dev));
}

int
siba_core_detach(struct siba_softc *siba)
{
	/* detach & delete all children */
	device_delete_children(siba->siba_dev);
	return (0);
}

static void
siba_pci_gpio(struct siba_softc *siba, uint32_t what, int on)
{
	uint32_t in, out;
	uint16_t status;

	if (siba->siba_type != SIBA_TYPE_PCI)
		return;

	out = pci_read_config(siba->siba_dev, SIBA_GPIO_OUT, 4);
	if (on == 0) {
		if (what & SIBA_GPIO_PLL)
			out |= SIBA_GPIO_PLL;
		if (what & SIBA_GPIO_CRYSTAL)
			out &= ~SIBA_GPIO_CRYSTAL;
		pci_write_config(siba->siba_dev, SIBA_GPIO_OUT, out, 4);
		pci_write_config(siba->siba_dev, SIBA_GPIO_OUT_EN,
		    pci_read_config(siba->siba_dev,
			SIBA_GPIO_OUT_EN, 4) | what, 4);
		return;
	}

	in = pci_read_config(siba->siba_dev, SIBA_GPIO_IN, 4);
	if ((in & SIBA_GPIO_CRYSTAL) != SIBA_GPIO_CRYSTAL) {
		if (what & SIBA_GPIO_CRYSTAL) {
			out |= SIBA_GPIO_CRYSTAL;
			if (what & SIBA_GPIO_PLL)
				out |= SIBA_GPIO_PLL;
			pci_write_config(siba->siba_dev, SIBA_GPIO_OUT, out, 4);
			pci_write_config(siba->siba_dev,
			    SIBA_GPIO_OUT_EN, pci_read_config(siba->siba_dev,
				SIBA_GPIO_OUT_EN, 4) | what, 4);
			DELAY(1000);
		}
		if (what & SIBA_GPIO_PLL) {
			out &= ~SIBA_GPIO_PLL;
			pci_write_config(siba->siba_dev, SIBA_GPIO_OUT, out, 4);
			DELAY(5000);
		}
	}

	status = pci_read_config(siba->siba_dev, PCIR_STATUS, 2);
	status &= ~PCIM_STATUS_STABORT;
	pci_write_config(siba->siba_dev, PCIR_STATUS, status, 2);
}

static void
siba_scan(struct siba_softc *siba)
{
	struct siba_dev_softc *sd;
	uint32_t idhi, tmp;
	device_t child;
	int base, dev_i = 0, error, i, is_pcie, n_80211 = 0, n_cc = 0,
	    n_pci = 0;

	KASSERT(siba->siba_type == SIBA_TYPE_PCI,
	    ("unsupported BUS type (%#x)", siba->siba_type));

	siba->siba_ndevs = 0;
	error = siba_switchcore(siba, 0); /* need the first core */
	if (error)
		return;

	idhi = siba_scan_read_4(siba, 0, SIBA_IDHIGH);
	if (SIBA_IDHIGH_CORECODE(idhi) == SIBA_DEVID_CHIPCOMMON) {
		tmp = siba_scan_read_4(siba, 0, SIBA_CC_CHIPID);
		siba->siba_chipid = SIBA_CC_ID(tmp);
		siba->siba_chiprev = SIBA_CC_REV(tmp);
		siba->siba_chippkg = SIBA_CC_PKG(tmp);
		if (SIBA_IDHIGH_REV(idhi) >= 4)
			siba->siba_ndevs = SIBA_CC_NCORES(tmp);
		siba->siba_cc.scc_caps = siba_scan_read_4(siba, 0,
		    SIBA_CC_CAPS);
	} else {
		if (siba->siba_type == SIBA_TYPE_PCI) {
			siba->siba_chipid = siba_dev2chipid(siba);
			siba->siba_chiprev = pci_read_config(siba->siba_dev,
			    PCIR_REVID, 2);
			siba->siba_chippkg = 0;
		} else {
			siba->siba_chipid = 0x4710;
			siba->siba_chiprev = 0;
			siba->siba_chippkg = 0;
		}
	}
	if (siba->siba_ndevs == 0)
		siba->siba_ndevs = siba_getncores(siba->siba_dev,
		    siba->siba_chipid);
	if (siba->siba_ndevs > SIBA_MAX_CORES) {
		device_printf(siba->siba_dev,
		    "too many siba cores (max %d %d)\n",
		    SIBA_MAX_CORES, siba->siba_ndevs);
		return;
	}

	/* looking basic information about each cores/devices */
	for (i = 0; i < siba->siba_ndevs; i++) {
		error = siba_switchcore(siba, i);
		if (error)
			return;
		sd = &(siba->siba_devs[dev_i]);
		idhi = siba_scan_read_4(siba, i, SIBA_IDHIGH);
		sd->sd_bus = siba;
		sd->sd_id.sd_device = SIBA_IDHIGH_CORECODE(idhi);
		sd->sd_id.sd_rev = SIBA_IDHIGH_REV(idhi);
		sd->sd_id.sd_vendor = SIBA_IDHIGH_VENDOR(idhi);
		sd->sd_ops = siba->siba_ops;
		sd->sd_coreidx = i;

		DPRINTF(siba, SIBA_DEBUG_SCAN,
		    "core %d (%s) found (cc %#xrev %#x vendor %#x)\n",
		    i, siba_core_name(sd->sd_id.sd_device),
		    sd->sd_id.sd_device, sd->sd_id.sd_rev, sd->sd_id.vendor);

		switch (sd->sd_id.sd_device) {
		case SIBA_DEVID_CHIPCOMMON:
			n_cc++;
			if (n_cc > 1) {
				device_printf(siba->siba_dev,
				    "warn: multiple ChipCommon\n");
				break;
			}
			siba->siba_cc.scc_dev = sd;
			break;
		case SIBA_DEVID_80211:
			n_80211++;
			if (n_80211 > 1) {
				device_printf(siba->siba_dev,
				    "warn: multiple 802.11 core\n");
				continue;
			}
			break;
		case SIBA_DEVID_PCI:
		case SIBA_DEVID_PCIE:
			n_pci++;
			error = pci_find_cap(siba->siba_dev, PCIY_EXPRESS,
			    &base);
			is_pcie = (error == 0) ? 1 : 0;

			if (n_pci > 1) {
				device_printf(siba->siba_dev,
				    "warn: multiple PCI(E) cores\n");
				break;
			}
			if (sd->sd_id.sd_device == SIBA_DEVID_PCI &&
			    is_pcie == 1)
				continue;
			if (sd->sd_id.sd_device == SIBA_DEVID_PCIE &&
			    is_pcie == 0)
				continue;
			siba->siba_pci.spc_dev = sd;
			break;
		case SIBA_DEVID_MODEM:
		case SIBA_DEVID_PCMCIA:
			break;
		default:
			device_printf(siba->siba_dev,
			    "unsupported coreid (%s)\n",
			    siba_core_name(sd->sd_id.sd_device));
			break;
		}
		dev_i++;

		child = device_add_child(siba->siba_dev, NULL, -1);
		if (child == NULL) {
			device_printf(siba->siba_dev, "child attach failed\n");
			continue;
		}

		device_set_ivars(child, sd);
	}
	siba->siba_ndevs = dev_i;
}

static int
siba_switchcore(struct siba_softc *siba, uint8_t idx)
{

	switch (siba->siba_type) {
	case SIBA_TYPE_PCI:
		return (siba_pci_switchcore_sub(siba, idx));
	default:
		KASSERT(0 == 1,
		    ("%s: unsupported bustype %#x", __func__,
		    siba->siba_type));
	}
	return (0);
}

static int
siba_pci_switchcore_sub(struct siba_softc *siba, uint8_t idx)
{
#define RETRY_MAX	50
	int i;
	uint32_t dir;

	dir = SIBA_REGWIN(idx);

	for (i = 0; i < RETRY_MAX; i++) {
		pci_write_config(siba->siba_dev, SIBA_BAR0, dir, 4);
		if (pci_read_config(siba->siba_dev, SIBA_BAR0, 4) == dir)
			return (0);
		DELAY(10);
	}
	return (ENODEV);
#undef RETRY_MAX
}

static int
siba_pci_switchcore(struct siba_softc *siba, struct siba_dev_softc *sd)
{
	int error;

	DPRINTF(siba, SIBA_DEBUG_SWITCHCORE, "Switching to %s core, index %d\n",
	    siba_core_name(sd->sd_id.sd_device), sd->sd_coreidx);

	error = siba_pci_switchcore_sub(siba, sd->sd_coreidx);
	if (error == 0)
		siba->siba_curdev = sd;

	return (error);
}

static uint32_t
siba_scan_read_4(struct siba_softc *siba, uint8_t coreidx,
    uint16_t offset)
{

	(void)coreidx;
	KASSERT(siba->siba_type == SIBA_TYPE_PCI,
	    ("unsupported BUS type (%#x)", siba->siba_type));

	return (SIBA_READ_4(siba, offset));
}

static uint16_t
siba_dev2chipid(struct siba_softc *siba)
{
	uint16_t chipid = 0;

	switch (siba->siba_pci_did) {
	case 0x4301:
		chipid = 0x4301;
		break;
	case 0x4305:
	case 0x4306:
	case 0x4307:
		chipid = 0x4307;
		break;
	case 0x4403:
		chipid = 0x4402;
		break;
	case 0x4610:
	case 0x4611:
	case 0x4612:
	case 0x4613:
	case 0x4614:
	case 0x4615:
		chipid = 0x4610;
		break;
	case 0x4710:
	case 0x4711:
	case 0x4712:
	case 0x4713:
	case 0x4714:
	case 0x4715:
		chipid = 0x4710;
		break;
	case 0x4320:
	case 0x4321:
	case 0x4322:
	case 0x4323:
	case 0x4324:
	case 0x4325:
		chipid = 0x4309;
		break;
	case PCI_DEVICE_ID_BCM4401:
	case PCI_DEVICE_ID_BCM4401B0:
	case PCI_DEVICE_ID_BCM4401B1:
		chipid = 0x4401;
		break;
	default:
		device_printf(siba->siba_dev, "unknown PCI did (%d)\n",
		    siba->siba_pci_did);
	}

	return (chipid);
}

/*
 * Earlier ChipCommon revisions have hardcoded number of cores
 * present dependent on the ChipCommon ID.
 */
uint8_t
siba_getncores(device_t dev, uint16_t chipid)
{
	switch (chipid) {
	case 0x4401:
	case 0x4402:
		return (3);
	case 0x4301:
	case 0x4307:
		return (5);
	case 0x4306:
		return (6);
	case SIBA_CCID_SENTRY5:
		return (7);
	case 0x4310:
		return (8);
	case SIBA_CCID_BCM4710:
	case 0x4610:
	case SIBA_CCID_BCM4704:
		return (9);
	default:
		device_printf(dev, "unknown the chipset ID %#x\n", chipid);
	}

	return (1);
}

static const char *
siba_core_name(uint16_t coreid)
{

	switch (coreid) {
	case SIBA_DEVID_CHIPCOMMON:
		return ("ChipCommon");
	case SIBA_DEVID_ILINE20:
		return ("ILine 20");
	case SIBA_DEVID_SDRAM:
		return ("SDRAM");
	case SIBA_DEVID_PCI:
		return ("PCI");
	case SIBA_DEVID_MIPS:
		return ("MIPS");
	case SIBA_DEVID_ETHERNET:
		return ("Fast Ethernet");
	case SIBA_DEVID_MODEM:
		return ("Modem");
	case SIBA_DEVID_USB11_HOSTDEV:
		return ("USB 1.1 Hostdev");
	case SIBA_DEVID_ADSL:
		return ("ADSL");
	case SIBA_DEVID_ILINE100:
		return ("ILine 100");
	case SIBA_DEVID_IPSEC:
		return ("IPSEC");
	case SIBA_DEVID_PCMCIA:
		return ("PCMCIA");
	case SIBA_DEVID_INTERNAL_MEM:
		return ("Internal Memory");
	case SIBA_DEVID_SDRAMDDR:
		return ("MEMC SDRAM");
	case SIBA_DEVID_EXTIF:
		return ("EXTIF");
	case SIBA_DEVID_80211:
		return ("IEEE 802.11");
	case SIBA_DEVID_MIPS_3302:
		return ("MIPS 3302");
	case SIBA_DEVID_USB11_HOST:
		return ("USB 1.1 Host");
	case SIBA_DEVID_USB11_DEV:
		return ("USB 1.1 Device");
	case SIBA_DEVID_USB20_HOST:
		return ("USB 2.0 Host");
	case SIBA_DEVID_USB20_DEV:
		return ("USB 2.0 Device");
	case SIBA_DEVID_SDIO_HOST:
		return ("SDIO Host");
	case SIBA_DEVID_ROBOSWITCH:
		return ("Roboswitch");
	case SIBA_DEVID_PARA_ATA:
		return ("PATA");
	case SIBA_DEVID_SATA_XORDMA:
		return ("SATA XOR-DMA");
	case SIBA_DEVID_ETHERNET_GBIT:
		return ("GBit Ethernet");
	case SIBA_DEVID_PCIE:
		return ("PCI-Express");
	case SIBA_DEVID_MIMO_PHY:
		return ("MIMO PHY");
	case SIBA_DEVID_SRAM_CTRLR:
		return ("SRAM Controller");
	case SIBA_DEVID_MINI_MACPHY:
		return ("Mini MACPHY");
	case SIBA_DEVID_ARM_1176:
		return ("ARM 1176");
	case SIBA_DEVID_ARM_7TDMI:
		return ("ARM 7TDMI");
	}
	return ("unknown");
}

static uint16_t
siba_pci_read_2(struct siba_dev_softc *sd, uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return (0xffff);

	return (SIBA_READ_2(siba, offset));
}

static uint32_t
siba_pci_read_4(struct siba_dev_softc *sd, uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return (0xffff);

	return (SIBA_READ_4(siba, offset));
}

static void
siba_pci_write_2(struct siba_dev_softc *sd, uint16_t offset, uint16_t value)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return;

	SIBA_WRITE_2(siba, offset, value);
}

static void
siba_pci_write_4(struct siba_dev_softc *sd, uint16_t offset, uint32_t value)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return;

	SIBA_WRITE_4(siba, offset, value);
}

static void
siba_pci_read_multi_1(struct siba_dev_softc *sd, void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0) {
		memset(buffer, 0xff, count);
		return;
	}

	SIBA_READ_MULTI_1(siba, offset, buffer, count);
}

static void
siba_pci_read_multi_2(struct siba_dev_softc *sd, void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0) {
		memset(buffer, 0xff, count);
		return;
	}

	KASSERT(!(count & 1), ("%s:%d: fail", __func__, __LINE__));
	SIBA_READ_MULTI_2(siba, offset, buffer, count >> 1);
}

static void
siba_pci_read_multi_4(struct siba_dev_softc *sd, void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0) {
		memset(buffer, 0xff, count);
		return;
	}

	KASSERT(!(count & 3), ("%s:%d: fail", __func__, __LINE__));
	SIBA_READ_MULTI_4(siba, offset, buffer, count >> 2);
}

static void
siba_pci_write_multi_1(struct siba_dev_softc *sd, const void *buffer,
    size_t count, uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return;

	SIBA_WRITE_MULTI_1(siba, offset, buffer, count);
}

static void
siba_pci_write_multi_2(struct siba_dev_softc *sd, const void *buffer,
    size_t count, uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return;

	KASSERT(!(count & 1), ("%s:%d: fail", __func__, __LINE__));
	SIBA_WRITE_MULTI_2(siba, offset, buffer, count >> 1);
}

static void
siba_pci_write_multi_4(struct siba_dev_softc *sd, const void *buffer,
    size_t count, uint16_t offset)
{
	struct siba_softc *siba = sd->sd_bus;

	if (siba->siba_curdev != sd && siba_pci_switchcore(siba, sd) != 0)
		return;

	KASSERT(!(count & 3), ("%s:%d: fail", __func__, __LINE__));
	SIBA_WRITE_MULTI_4(siba, offset, buffer, count >> 2);
}

void
siba_powerup(device_t dev, int dynamic)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;

	siba_powerup_sub(siba, dynamic);
}

static void
siba_powerup_sub(struct siba_softc *siba, int dynamic)
{

	siba_pci_gpio(siba, SIBA_GPIO_CRYSTAL | SIBA_GPIO_PLL, 1);
	siba_cc_clock(&siba->siba_cc,
	    (dynamic != 0) ? SIBA_CLOCK_DYNAMIC : SIBA_CLOCK_FAST);
}

static void
siba_cc_clock(struct siba_cc *scc, enum siba_clock clock)
{
	struct siba_dev_softc *sd = scc->scc_dev;
	struct siba_softc *siba;
	uint32_t tmp;

	if (sd == NULL)
		return;
	siba = sd->sd_bus;
	/*
	 * chipcommon < r6 (no dynamic clock control)
	 * chipcommon >= r10 (unknown)
	 */
	if (sd->sd_id.sd_rev < 6 || sd->sd_id.sd_rev >= 10 ||
	    (scc->scc_caps & SIBA_CC_CAPS_PWCTL) == 0)
		return;

	switch (clock) {
	case SIBA_CLOCK_DYNAMIC:
		tmp = SIBA_CC_READ32(scc, SIBA_CC_CLKSLOW) &
		    ~(SIBA_CC_CLKSLOW_ENXTAL | SIBA_CC_CLKSLOW_FSLOW |
		    SIBA_CC_CLKSLOW_IPLL);
		if ((tmp & SIBA_CC_CLKSLOW_SRC) != SIBA_CC_CLKSLOW_SRC_CRYSTAL)
			tmp |= SIBA_CC_CLKSLOW_ENXTAL;
		SIBA_CC_WRITE32(scc, SIBA_CC_CLKSLOW, tmp);
		if (tmp & SIBA_CC_CLKSLOW_ENXTAL)
			siba_pci_gpio(siba, SIBA_GPIO_CRYSTAL, 0);
		break;
	case SIBA_CLOCK_SLOW:
		SIBA_CC_WRITE32(scc, SIBA_CC_CLKSLOW,
		    SIBA_CC_READ32(scc, SIBA_CC_CLKSLOW) |
		    SIBA_CC_CLKSLOW_FSLOW);
		break;
	case SIBA_CLOCK_FAST:
		/* crystal on */
		siba_pci_gpio(siba, SIBA_GPIO_CRYSTAL, 1);
		SIBA_CC_WRITE32(scc, SIBA_CC_CLKSLOW,
		    (SIBA_CC_READ32(scc, SIBA_CC_CLKSLOW) |
			SIBA_CC_CLKSLOW_IPLL) & ~SIBA_CC_CLKSLOW_FSLOW);
		break;
	default:
		KASSERT(0 == 1,
		    ("%s: unsupported clock %#x", __func__, clock));
	}
}

uint16_t
siba_read_2(device_t dev, uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	return (sd->sd_ops->read_2(sd, offset));
}

uint32_t
siba_read_4(device_t dev, uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	return (siba_read_4_sub(sd, offset));
}

static uint32_t
siba_read_4_sub(struct siba_dev_softc *sd, uint16_t offset)
{

	return (sd->sd_ops->read_4(sd, offset));
}

void
siba_write_2(device_t dev, uint16_t offset, uint16_t value)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->write_2(sd, offset, value);
}

void
siba_write_4(device_t dev, uint16_t offset, uint32_t value)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	return (siba_write_4_sub(sd, offset, value));
}

static void
siba_write_4_sub(struct siba_dev_softc *sd, uint16_t offset, uint32_t value)
{

	sd->sd_ops->write_4(sd, offset, value);
}

void
siba_read_multi_1(device_t dev, void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->read_multi_1(sd, buffer, count, offset);
}

void
siba_read_multi_2(device_t dev, void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->read_multi_2(sd, buffer, count, offset);
}

void
siba_read_multi_4(device_t dev, void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->read_multi_4(sd, buffer, count, offset);
}

void
siba_write_multi_1(device_t dev, const void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->write_multi_1(sd, buffer, count, offset);
}

void
siba_write_multi_2(device_t dev, const void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->write_multi_2(sd, buffer, count, offset);
}

void
siba_write_multi_4(device_t dev, const void *buffer, size_t count,
    uint16_t offset)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	sd->sd_ops->write_multi_4(sd, buffer, count, offset);
}

static void
siba_cc_pmu_init(struct siba_cc *scc)
{
	const struct siba_cc_pmu_res_updown *updown = NULL;
	const struct siba_cc_pmu_res_depend *depend = NULL;
	struct siba_dev_softc *sd = scc->scc_dev;
	struct siba_softc *siba = sd->sd_bus;
	uint32_t min = 0, max = 0, pmucap;
	unsigned int i, updown_size, depend_size;

	if ((scc->scc_caps & SIBA_CC_CAPS_PMU) == 0)
		return;

	pmucap = SIBA_CC_READ32(scc, SIBA_CC_PMUCAPS);
	scc->scc_pmu.rev = (pmucap & SIBA_CC_PMUCAPS_REV);

	DPRINTF(siba, SIBA_DEBUG_PMU, "PMU(r%u) found (caps %#x)\n",
	    scc->scc_pmu.rev, pmucap);

	if (scc->scc_pmu.rev >= 1) {
		if (siba->siba_chiprev < 2 && siba->siba_chipid == 0x4325)
			SIBA_CC_MASK32(scc, SIBA_CC_PMUCTL,
			    ~SIBA_CC_PMUCTL_NOILP);
		else
			SIBA_CC_SET32(scc, SIBA_CC_PMUCTL,
			    SIBA_CC_PMUCTL_NOILP);
	}

	/* initialize PLL & PMU resources */
	switch (siba->siba_chipid) {
	case 0x4312:
		siba_cc_pmu1_pll0_init(scc, 0 /* use default */);
		/* use the default: min = 0xcbb max = 0x7ffff */
		break;
	case 0x4325:
		siba_cc_pmu1_pll0_init(scc, 0 /* use default */);

		updown = siba_cc_pmu_4325_updown;
		updown_size = N(siba_cc_pmu_4325_updown);
		depend = siba_cc_pmu_4325_depend;
		depend_size = N(siba_cc_pmu_4325_depend);

		min = (1 << SIBA_CC_PMU_4325_BURST) |
		    (1 << SIBA_CC_PMU_4325_LN);
		if (SIBA_CC_READ32(scc, SIBA_CC_CHIPSTAT) &
		    SIBA_CC_CHST_4325_PMUTOP_2B)
			min |= (1 << SIBA_CC_PMU_4325_CLBURST);
		max = 0xfffff;
		break;
	case 0x4328:
		siba_cc_pmu0_pll0_init(scc, 0 /* use default */);

		updown = siba_cc_pmu_4328_updown;
		updown_size = N(siba_cc_pmu_4328_updown);
		depend = siba_cc_pmu_4328_depend;
		depend_size = N(siba_cc_pmu_4328_depend);

		min = (1 << SIBA_CC_PMU_4328_EXT_SWITCH_PWM) |
			  (1 << SIBA_CC_PMU_4328_BB_SWITCH_PWM) |
			  (1 << SIBA_CC_PMU_4328_CRYSTAL_EN);

		max = 0xfffff;
		break;
	case 0x5354:
		siba_cc_pmu0_pll0_init(scc, 0 /* use default */);

		max = 0xfffff;
		break;
	default:
		device_printf(siba->siba_dev,
		    "unknown chipid %#x for PLL & PMU init\n",
		    siba->siba_chipid);
	}

	if (updown) {
		for (i = 0; i < updown_size; i++) {
			SIBA_CC_WRITE32(scc, SIBA_CC_PMU_TABSEL,
			    updown[i].res);
			SIBA_CC_WRITE32(scc, SIBA_CC_PMU_UPDNTM,
			    updown[i].updown);
		}
	}
	if (depend) {
		for (i = 0; i < depend_size; i++) {
			SIBA_CC_WRITE32(scc, SIBA_CC_PMU_TABSEL,
			    depend[i].res);
			switch (depend[i].task) {
			case SIBA_CC_PMU_DEP_SET:
				SIBA_CC_WRITE32(scc, SIBA_CC_PMU_DEPMSK,
				    depend[i].depend);
				break;
			case SIBA_CC_PMU_DEP_ADD:
				SIBA_CC_SET32(scc, SIBA_CC_PMU_DEPMSK,
				    depend[i].depend);
				break;
			case SIBA_CC_PMU_DEP_REMOVE:
				SIBA_CC_MASK32(scc, SIBA_CC_PMU_DEPMSK,
				    ~(depend[i].depend));
				break;
			default:
				KASSERT(0 == 1,
				    ("%s:%d: assertion failed",
					__func__, __LINE__));
			}
		}
	}

	if (min)
		SIBA_CC_WRITE32(scc, SIBA_CC_PMU_MINRES, min);
	if (max)
		SIBA_CC_WRITE32(scc, SIBA_CC_PMU_MAXRES, max);
}

static void
siba_cc_power_init(struct siba_cc *scc)
{
	struct siba_softc *siba = scc->scc_dev->sd_bus;
	int maxfreq;

	if (siba->siba_chipid == 0x4321) {
		if (siba->siba_chiprev == 0)
			SIBA_CC_WRITE32(scc, SIBA_CC_CHIPCTL, 0x3a4);
		else if (siba->siba_chiprev == 1)
			SIBA_CC_WRITE32(scc, SIBA_CC_CHIPCTL, 0xa4);
	}

	if ((scc->scc_caps & SIBA_CC_CAPS_PWCTL) == 0)
		return;

	if (scc->scc_dev->sd_id.sd_rev >= 10)
		SIBA_CC_WRITE32(scc, SIBA_CC_CLKSYSCTL,
		    (SIBA_CC_READ32(scc, SIBA_CC_CLKSYSCTL) &
		    0xffff) | 0x40000);
	else {
		maxfreq = siba_cc_clockfreq(scc, 1);
		SIBA_CC_WRITE32(scc, SIBA_CC_PLLONDELAY,
		    (maxfreq * 150 + 999999) / 1000000);
		SIBA_CC_WRITE32(scc, SIBA_CC_FREFSELDELAY,
		    (maxfreq * 15 + 999999) / 1000000);
	}
}

static void
siba_cc_powerup_delay(struct siba_cc *scc)
{
	struct siba_softc *siba = scc->scc_dev->sd_bus;
	int min;

	if (siba->siba_type != SIBA_TYPE_PCI ||
	    !(scc->scc_caps & SIBA_CC_CAPS_PWCTL))
		return;

	min = siba_cc_clockfreq(scc, 0);
	scc->scc_powerup_delay =
	    (((SIBA_CC_READ32(scc, SIBA_CC_PLLONDELAY) + 2) * 1000000) +
	    (min - 1)) / min;
}

static int
siba_cc_clockfreq(struct siba_cc *scc, int max)
{
	enum siba_clksrc src;
	int div = 1, limit = 0;

	src = siba_cc_clksrc(scc);
	if (scc->scc_dev->sd_id.sd_rev < 6) {
		div = (src == SIBA_CC_CLKSRC_PCI) ? 64 :
		    (src == SIBA_CC_CLKSRC_CRYSTAL) ? 32 : 1;
		KASSERT(div != 1,
		    ("%s: unknown clock %d", __func__, src));
	} else if (scc->scc_dev->sd_id.sd_rev < 10) {
		switch (src) {
		case SIBA_CC_CLKSRC_CRYSTAL:
		case SIBA_CC_CLKSRC_PCI:
			div = ((SIBA_CC_READ32(scc, SIBA_CC_CLKSLOW) >> 16) +
			    1) * 4;
			break;
		case SIBA_CC_CLKSRC_LOWPW:
			break;
		}
	} else
		div = ((SIBA_CC_READ32(scc, SIBA_CC_CLKSYSCTL) >> 16) + 1) * 4;

	switch (src) {
	case SIBA_CC_CLKSRC_CRYSTAL:
		limit = (max) ? 20200000 : 19800000;
		break;
	case SIBA_CC_CLKSRC_LOWPW:
		limit = (max) ? 43000 : 25000;
		break;
	case SIBA_CC_CLKSRC_PCI:
		limit = (max) ? 34000000 : 25000000;
		break;
	}

	return (limit / div);
}

static void
siba_cc_pmu1_pll0_init(struct siba_cc *scc, uint32_t freq)
{
	struct siba_dev_softc *sd = scc->scc_dev;
	struct siba_softc *siba = sd->sd_bus;
	const struct siba_cc_pmu1_plltab *e = NULL;
	uint32_t bufsth = 0, pll, pmu;
	unsigned int i;

	KASSERT(freq == 0, ("%s:%d: assertion vail", __func__, __LINE__));
	if (siba->siba_chipid == 0x4312) {
		scc->scc_pmu.freq = 20000;
		return;
	}

	e = siba_cc_pmu1_plltab_find(SIBA_CC_PMU1_DEFAULT_FREQ);
	KASSERT(e != NULL, ("%s:%d: assertion vail", __func__, __LINE__));
	scc->scc_pmu.freq = e->freq;

	pmu = SIBA_CC_READ32(scc, SIBA_CC_PMUCTL);
	if (SIBA_CC_PMUCTL_XF_VAL(pmu) == e->xf)
		return;

	DPRINTF(siba, SIBA_DEBUG_PLL, "change PLL value to %u.%03u MHz\n",
	    (e->freq / 1000), (e->freq % 1000));

	/* turn PLL off */
	switch (siba->siba_chipid) {
	case 0x4325:
		bufsth = 0x222222;
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MINRES,
		    ~((1 << SIBA_CC_PMU_4325_BBPLL_PWR) |
		      (1 << SIBA_CC_PMU_4325_HT)));
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MAXRES,
		    ~((1 << SIBA_CC_PMU_4325_BBPLL_PWR) |
		      (1 << SIBA_CC_PMU_4325_HT)));
		break;
	default:
		KASSERT(0 == 1,
		    ("%s:%d: assertion failed", __func__, __LINE__));
	}
	for (i = 0; i < 1500; i++) {
		if (!(SIBA_CC_READ32(scc, SIBA_CC_CLKCTLSTATUS) &
		      SIBA_CC_CLKCTLSTATUS_HT))
			break;
		DELAY(10);
	}
	if (SIBA_CC_READ32(scc, SIBA_CC_CLKCTLSTATUS) & SIBA_CC_CLKCTLSTATUS_HT)
		device_printf(siba->siba_dev, "failed to turn PLL off!\n");

	pll = siba_cc_pll_read(scc, SIBA_CC_PMU1_PLL0);
	pll &= ~(SIBA_CC_PMU1_PLL0_P1DIV | SIBA_CC_PMU1_PLL0_P2DIV);
	pll |= ((uint32_t)e->p1div << 20) & SIBA_CC_PMU1_PLL0_P1DIV;
	pll |= ((uint32_t)e->p2div << 24) & SIBA_CC_PMU1_PLL0_P2DIV;
	siba_cc_pll_write(scc, SIBA_CC_PMU1_PLL0, pll);

	pll = siba_cc_pll_read(scc, SIBA_CC_PMU1_PLL2);
	pll &= ~(SIBA_CC_PMU1_PLL2_NDIVINT | SIBA_CC_PMU1_PLL2_NDIVMODE);
	pll |= ((uint32_t)e->ndiv_int << 20) & SIBA_CC_PMU1_PLL2_NDIVINT;
	pll |= (1 << 17) & SIBA_CC_PMU1_PLL2_NDIVMODE;
	siba_cc_pll_write(scc, SIBA_CC_PMU1_PLL2, pll);

	pll = siba_cc_pll_read(scc, SIBA_CC_PMU1_PLL3);
	pll &= ~SIBA_CC_PMU1_PLL3_NDIVFRAC;
	pll |= ((uint32_t)e->ndiv_frac << 0) & SIBA_CC_PMU1_PLL3_NDIVFRAC;
	siba_cc_pll_write(scc, SIBA_CC_PMU1_PLL3, pll);

	if (bufsth) {
		pll = siba_cc_pll_read(scc, SIBA_CC_PMU1_PLL5);
		pll &= ~SIBA_CC_PMU1_PLL5_CLKDRV;
		pll |= (bufsth << 8) & SIBA_CC_PMU1_PLL5_CLKDRV;
		siba_cc_pll_write(scc, SIBA_CC_PMU1_PLL5, pll);
	}

	pmu = SIBA_CC_READ32(scc, SIBA_CC_PMUCTL);
	pmu &= ~(SIBA_CC_PMUCTL_ILP | SIBA_CC_PMUCTL_XF);
	pmu |= ((((uint32_t)e->freq + 127) / 128 - 1) << 16) &
	    SIBA_CC_PMUCTL_ILP;
	pmu |= ((uint32_t)e->xf << 2) & SIBA_CC_PMUCTL_XF;
	SIBA_CC_WRITE32(scc, SIBA_CC_PMUCTL, pmu);
}

static void
siba_cc_pmu0_pll0_init(struct siba_cc *scc, uint32_t xtalfreq)
{
	struct siba_dev_softc *sd = scc->scc_dev;
	struct siba_softc *siba = sd->sd_bus;
	const struct siba_cc_pmu0_plltab *e = NULL;
	uint32_t pmu, tmp, pll;
	unsigned int i;

	if ((siba->siba_chipid == 0x5354) && !xtalfreq)
		xtalfreq = 25000;
	if (xtalfreq)
		e = siba_cc_pmu0_plltab_findentry(xtalfreq);
	if (!e)
		e = siba_cc_pmu0_plltab_findentry(
		    SIBA_CC_PMU0_DEFAULT_XTALFREQ);
	KASSERT(e != NULL, ("%s:%d: fail", __func__, __LINE__));
	xtalfreq = e->freq;
	scc->scc_pmu.freq = e->freq;

	pmu = SIBA_CC_READ32(scc, SIBA_CC_PMUCTL);
	if (((pmu & SIBA_CC_PMUCTL_XF) >> 2) == e->xf)
		return;

	DPRINTF(siba, SIBA_DEBUG_PLL, "change PLL value to %u.%03u MHz\n",
	    (xtalfreq / 1000), (xtalfreq % 1000));

	KASSERT(siba->siba_chipid == 0x4328 || siba->siba_chipid == 0x5354,
	    ("%s:%d: fail", __func__, __LINE__));

	switch (siba->siba_chipid) {
	case 0x4328:
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MINRES,
		    ~(1 << SIBA_CC_PMU_4328_BB_PLL_PU));
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MAXRES,
		    ~(1 << SIBA_CC_PMU_4328_BB_PLL_PU));
		break;
	case 0x5354:
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MINRES,
		    ~(1 << SIBA_CC_PMU_5354_BB_PLL_PU));
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MAXRES,
		    ~(1 << SIBA_CC_PMU_5354_BB_PLL_PU));
		break;
	}
	for (i = 1500; i; i--) {
		tmp = SIBA_CC_READ32(scc, SIBA_CC_CLKCTLSTATUS);
		if (!(tmp & SIBA_CC_CLKCTLSTATUS_HT))
			break;
		DELAY(10);
	}
	tmp = SIBA_CC_READ32(scc, SIBA_CC_CLKCTLSTATUS);
	if (tmp & SIBA_CC_CLKCTLSTATUS_HT)
		device_printf(siba->siba_dev, "failed to turn PLL off!\n");

	/* set PDIV */
	pll = siba_cc_pll_read(scc, SIBA_CC_PMU0_PLL0);
	if (xtalfreq >= SIBA_CC_PMU0_PLL0_PDIV_FREQ)
		pll |= SIBA_CC_PMU0_PLL0_PDIV_MSK;
	else
		pll &= ~SIBA_CC_PMU0_PLL0_PDIV_MSK;
	siba_cc_pll_write(scc, SIBA_CC_PMU0_PLL0, pll);

	/* set WILD */
	pll = siba_cc_pll_read(scc, SIBA_CC_PMU0_PLL1);
	pll &= ~(SIBA_CC_PMU0_PLL1_STOPMOD | SIBA_CC_PMU0_PLL1_IMSK |
	    SIBA_CC_PMU0_PLL1_FMSK);
	pll |= ((uint32_t)e->wb_int << 28) & SIBA_CC_PMU0_PLL1_IMSK;
	pll |= ((uint32_t)e->wb_frac << 8) & SIBA_CC_PMU0_PLL1_FMSK;
	if (e->wb_frac == 0)
		pll |= SIBA_CC_PMU0_PLL1_STOPMOD;
	siba_cc_pll_write(scc, SIBA_CC_PMU0_PLL1, pll);

	/* set WILD */
	pll = siba_cc_pll_read(scc, SIBA_CC_PMU0_PLL2);
	pll &= ~SIBA_CC_PMU0_PLL2_IMSKHI;
	pll |= (((uint32_t)e->wb_int >> 4) << 0) & SIBA_CC_PMU0_PLL2_IMSKHI;
	siba_cc_pll_write(scc, SIBA_CC_PMU0_PLL2, pll);

	/* set freq and divisor. */
	pmu = SIBA_CC_READ32(scc, SIBA_CC_PMUCTL);
	pmu &= ~SIBA_CC_PMUCTL_ILP;
	pmu |= (((xtalfreq + 127) / 128 - 1) << 16) & SIBA_CC_PMUCTL_ILP;
	pmu &= ~SIBA_CC_PMUCTL_XF;
	pmu |= ((uint32_t)e->xf << 2) & SIBA_CC_PMUCTL_XF;
	SIBA_CC_WRITE32(scc, SIBA_CC_PMUCTL, pmu);
}

static enum siba_clksrc
siba_cc_clksrc(struct siba_cc *scc)
{
	struct siba_dev_softc *sd = scc->scc_dev;
	struct siba_softc *siba = sd->sd_bus;

	if (sd->sd_id.sd_rev < 6) {
		if (siba->siba_type == SIBA_TYPE_PCI) {
			if (pci_read_config(siba->siba_dev, SIBA_GPIO_OUT, 4) &
			    0x10)
				return (SIBA_CC_CLKSRC_PCI);
			return (SIBA_CC_CLKSRC_CRYSTAL);
		}
		if (siba->siba_type == SIBA_TYPE_SSB ||
		    siba->siba_type == SIBA_TYPE_PCMCIA)
			return (SIBA_CC_CLKSRC_CRYSTAL);
	}
	if (sd->sd_id.sd_rev < 10) {
		switch (SIBA_CC_READ32(scc, SIBA_CC_CLKSLOW) & 0x7) {
		case 0:
			return (SIBA_CC_CLKSRC_LOWPW);
		case 1:
			return (SIBA_CC_CLKSRC_CRYSTAL);
		case 2:
			return (SIBA_CC_CLKSRC_PCI);
		default:
			break;
		}
	}

	return (SIBA_CC_CLKSRC_CRYSTAL);
}

static const struct siba_cc_pmu1_plltab *
siba_cc_pmu1_plltab_find(uint32_t crystalfreq)
{
	const struct siba_cc_pmu1_plltab *e;
	unsigned int i;

	for (i = 0; i < N(siba_cc_pmu1_plltab); i++) {
		e = &siba_cc_pmu1_plltab[i];
		if (crystalfreq == e->freq)
			return (e);
	}

	return (NULL);
}

static uint32_t
siba_cc_pll_read(struct siba_cc *scc, uint32_t offset)
{

	SIBA_CC_WRITE32(scc, SIBA_CC_PLLCTL_ADDR, offset);
	return (SIBA_CC_READ32(scc, SIBA_CC_PLLCTL_DATA));
}

static void
siba_cc_pll_write(struct siba_cc *scc, uint32_t offset, uint32_t value)
{

	SIBA_CC_WRITE32(scc, SIBA_CC_PLLCTL_ADDR, offset);
	SIBA_CC_WRITE32(scc, SIBA_CC_PLLCTL_DATA, value);
}

static const struct siba_cc_pmu0_plltab *
siba_cc_pmu0_plltab_findentry(uint32_t crystalfreq)
{
	const struct siba_cc_pmu0_plltab *e;
	unsigned int i;

	for (i = 0; i < N(siba_cc_pmu0_plltab); i++) {
		e = &siba_cc_pmu0_plltab[i];
		if (e->freq == crystalfreq)
			return (e);
	}

	return (NULL);
}

static int
siba_pci_sprom(struct siba_softc *siba, struct siba_sprom *sprom)
{
	int error = ENOMEM;
	uint16_t *buf;

	buf = malloc(SIBA_SPROMSIZE_R123 * sizeof(uint16_t),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (buf == NULL)
		return (ENOMEM);
	siba_sprom_read(siba, buf, SIBA_SPROMSIZE_R123);
	error = sprom_check_crc(buf, siba->siba_spromsize);
	if (error) {
		free(buf, M_DEVBUF);
		buf = malloc(SIBA_SPROMSIZE_R4 * sizeof(uint16_t),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (buf == NULL)
			return (ENOMEM);
		siba_sprom_read(siba, buf, SIBA_SPROMSIZE_R4);
		error = sprom_check_crc(buf, siba->siba_spromsize);
		if (error)
			device_printf(siba->siba_dev, "warn: bad SPROM CRC\n");
	}

	bzero(sprom, sizeof(*sprom));

	sprom->rev = buf[siba->siba_spromsize - 1] & 0x00FF;
	DPRINTF(siba, SIBA_DEBUG_SPROM, "SPROM rev %d\n",
	    sprom->rev);
	memset(sprom->mac_eth, 0xff, 6);
	memset(sprom->mac_80211a, 0xff, 6);
	if ((siba->siba_chipid & 0xff00) == 0x4400) {
		sprom->rev = 1;
		siba_sprom_r123(sprom, buf);
	} else if (siba->siba_chipid == 0x4321) {
		sprom->rev = 4;
		siba_sprom_r45(sprom, buf);
	} else {
		switch (sprom->rev) {
		case 1:
		case 2:
		case 3:
			siba_sprom_r123(sprom, buf);
			break;
		case 4:
		case 5:
			siba_sprom_r45(sprom, buf);
			break;
		case 8:
			siba_sprom_r8(sprom, buf);
			break;
		default:
			device_printf(siba->siba_dev,
			    "unknown SPROM revision %d.\n", sprom->rev);
			siba_sprom_r123(sprom, buf);
		}
	}

	if (sprom->bf_lo == 0xffff)
		sprom->bf_lo = 0;
	if (sprom->bf_hi == 0xffff)
		sprom->bf_hi = 0;

	free(buf, M_DEVBUF);
	return (error);
}

static int
siba_sprom_read(struct siba_softc *siba, uint16_t *sprom, uint16_t len)
{
	int i;

	for (i = 0; i < len; i++)
		sprom[i] = SIBA_READ_2(siba, SIBA_SPROM_BASE + (i * 2));

	siba->siba_spromsize = len;
	return (0);
}

static int
sprom_check_crc(const uint16_t *sprom, size_t size)
{
	int word;
	uint8_t crc0, crc1 = 0xff;

	crc0 = (sprom[size - 1] & SIBA_SPROM_REV_CRC) >> 8;
	for (word = 0; word < size - 1; word++) {
		crc1 = siba_crc8(crc1, sprom[word] & 0x00ff);
		crc1 = siba_crc8(crc1, (sprom[word] & 0xff00) >> 8);
	}
	crc1 = siba_crc8(crc1, sprom[size - 1] & 0x00ff);
	crc1 ^= 0xff;

	return ((crc0 != crc1) ? EPROTO : 0);
}

static uint8_t
siba_crc8(uint8_t crc, uint8_t data)
{
	static const uint8_t ct[] = {
		0x00, 0xf7, 0xb9, 0x4e, 0x25, 0xd2, 0x9c, 0x6b,
		0x4a, 0xbd, 0xf3, 0x04, 0x6f, 0x98, 0xd6, 0x21,
		0x94, 0x63, 0x2d, 0xda, 0xb1, 0x46, 0x08, 0xff,
		0xde, 0x29, 0x67, 0x90, 0xfb, 0x0c, 0x42, 0xb5,
		0x7f, 0x88, 0xc6, 0x31, 0x5a, 0xad, 0xe3, 0x14,
		0x35, 0xc2, 0x8c, 0x7b, 0x10, 0xe7, 0xa9, 0x5e,
		0xeb, 0x1c, 0x52, 0xa5, 0xce, 0x39, 0x77, 0x80,
		0xa1, 0x56, 0x18, 0xef, 0x84, 0x73, 0x3d, 0xca,
		0xfe, 0x09, 0x47, 0xb0, 0xdb, 0x2c, 0x62, 0x95,
		0xb4, 0x43, 0x0d, 0xfa, 0x91, 0x66, 0x28, 0xdf,
		0x6a, 0x9d, 0xd3, 0x24, 0x4f, 0xb8, 0xf6, 0x01,
		0x20, 0xd7, 0x99, 0x6e, 0x05, 0xf2, 0xbc, 0x4b,
		0x81, 0x76, 0x38, 0xcf, 0xa4, 0x53, 0x1d, 0xea,
		0xcb, 0x3c, 0x72, 0x85, 0xee, 0x19, 0x57, 0xa0,
		0x15, 0xe2, 0xac, 0x5b, 0x30, 0xc7, 0x89, 0x7e,
		0x5f, 0xa8, 0xe6, 0x11, 0x7a, 0x8d, 0xc3, 0x34,
		0xab, 0x5c, 0x12, 0xe5, 0x8e, 0x79, 0x37, 0xc0,
		0xe1, 0x16, 0x58, 0xaf, 0xc4, 0x33, 0x7d, 0x8a,
		0x3f, 0xc8, 0x86, 0x71, 0x1a, 0xed, 0xa3, 0x54,
		0x75, 0x82, 0xcc, 0x3b, 0x50, 0xa7, 0xe9, 0x1e,
		0xd4, 0x23, 0x6d, 0x9a, 0xf1, 0x06, 0x48, 0xbf,
		0x9e, 0x69, 0x27, 0xd0, 0xbb, 0x4c, 0x02, 0xf5,
		0x40, 0xb7, 0xf9, 0x0e, 0x65, 0x92, 0xdc, 0x2b,
		0x0a, 0xfd, 0xb3, 0x44, 0x2f, 0xd8, 0x96, 0x61,
		0x55, 0xa2, 0xec, 0x1b, 0x70, 0x87, 0xc9, 0x3e,
		0x1f, 0xe8, 0xa6, 0x51, 0x3a, 0xcd, 0x83, 0x74,
		0xc1, 0x36, 0x78, 0x8f, 0xe4, 0x13, 0x5d, 0xaa,
		0x8b, 0x7c, 0x32, 0xc5, 0xae, 0x59, 0x17, 0xe0,
		0x2a, 0xdd, 0x93, 0x64, 0x0f, 0xf8, 0xb6, 0x41,
		0x60, 0x97, 0xd9, 0x2e, 0x45, 0xb2, 0xfc, 0x0b,
		0xbe, 0x49, 0x07, 0xf0, 0x9b, 0x6c, 0x22, 0xd5,
		0xf4, 0x03, 0x4d, 0xba, 0xd1, 0x26, 0x68, 0x9f,
	};
	return (ct[crc ^ data]);
}

#define	SIBA_LOWEST_SET_BIT(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))
#define SIBA_OFFSET(offset)	\
	(((offset) - SIBA_SPROM_BASE) / sizeof(uint16_t))
#define	SIBA_SHIFTOUT_SUB(__x, __mask)					\
	(((__x) & (__mask)) / SIBA_LOWEST_SET_BIT(__mask))
#define	SIBA_SHIFTOUT(_var, _offset, _mask)				\
	out->_var = SIBA_SHIFTOUT_SUB(in[SIBA_OFFSET(_offset)], (_mask))
#define SIBA_SHIFTOUT_4(_var, _offset, _mask, _shift)			\
	out->_var = ((((uint32_t)in[SIBA_OFFSET((_offset)+2)] << 16 |	\
	    in[SIBA_OFFSET(_offset)]) & (_mask)) >> (_shift))

static void
siba_sprom_r123(struct siba_sprom *out, const uint16_t *in)
{
	int i;
	uint16_t v;
	int8_t gain;
	uint16_t loc[3];

	if (out->rev == 3)
		loc[0] = SIBA_SPROM3_MAC_80211BG;
	else {
		loc[0] = SIBA_SPROM1_MAC_80211BG;
		loc[1] = SIBA_SPROM1_MAC_ETH;
		loc[2] = SIBA_SPROM1_MAC_80211A;
	}
	for (i = 0; i < 3; i++) {
		v = in[SIBA_OFFSET(loc[0]) + i];
		*(((uint16_t *)out->mac_80211bg) + i) = htobe16(v);
	}
	if (out->rev < 3) {
		for (i = 0; i < 3; i++) {
			v = in[SIBA_OFFSET(loc[1]) + i];
			*(((uint16_t *)out->mac_eth) + i) = htobe16(v);
		}
		for (i = 0; i < 3; i++) {
			v = in[SIBA_OFFSET(loc[2]) + i];
			*(((uint16_t *)out->mac_80211a) + i) = htobe16(v);
		}
	}
	SIBA_SHIFTOUT(mii_eth0, SIBA_SPROM1_ETHPHY,
	    SIBA_SPROM1_ETHPHY_MII_ETH0);
	SIBA_SHIFTOUT(mii_eth1, SIBA_SPROM1_ETHPHY,
	    SIBA_SPROM1_ETHPHY_MII_ETH1);
	SIBA_SHIFTOUT(mdio_eth0, SIBA_SPROM1_ETHPHY,
	    SIBA_SPROM1_ETHPHY_MDIO_ETH0);
	SIBA_SHIFTOUT(mdio_eth1, SIBA_SPROM1_ETHPHY,
	    SIBA_SPROM1_ETHPHY_MDIO_ETH1);
	SIBA_SHIFTOUT(brev, SIBA_SPROM1_BOARDINFO, SIBA_SPROM1_BOARDINFO_BREV);
	SIBA_SHIFTOUT(ccode, SIBA_SPROM1_BOARDINFO,
	    SIBA_SPROM1_BOARDINFO_CCODE);
	SIBA_SHIFTOUT(ant_a, SIBA_SPROM1_BOARDINFO, SIBA_SPROM1_BOARDINFO_ANTA);
	SIBA_SHIFTOUT(ant_bg, SIBA_SPROM1_BOARDINFO,
	    SIBA_SPROM1_BOARDINFO_ANTBG);
	SIBA_SHIFTOUT(pa0b0, SIBA_SPROM1_PA0B0, 0xffff);
	SIBA_SHIFTOUT(pa0b1, SIBA_SPROM1_PA0B1, 0xffff);
	SIBA_SHIFTOUT(pa0b2, SIBA_SPROM1_PA0B2, 0xffff);
	SIBA_SHIFTOUT(pa1b0, SIBA_SPROM1_PA1B0, 0xffff);
	SIBA_SHIFTOUT(pa1b1, SIBA_SPROM1_PA1B1, 0xffff);
	SIBA_SHIFTOUT(pa1b2, SIBA_SPROM1_PA1B2, 0xffff);
	SIBA_SHIFTOUT(gpio0, SIBA_SPROM1_GPIOA, SIBA_SPROM1_GPIOA_P0);
	SIBA_SHIFTOUT(gpio1, SIBA_SPROM1_GPIOA, SIBA_SPROM1_GPIOA_P1);
	SIBA_SHIFTOUT(gpio2, SIBA_SPROM1_GPIOB, SIBA_SPROM1_GPIOB_P2);
	SIBA_SHIFTOUT(gpio3, SIBA_SPROM1_GPIOB, SIBA_SPROM1_GPIOB_P3);

	SIBA_SHIFTOUT(maxpwr_a, SIBA_SPROM1_MAXPWR, SIBA_SPROM1_MAXPWR_A);
	SIBA_SHIFTOUT(maxpwr_bg, SIBA_SPROM1_MAXPWR, SIBA_SPROM1_MAXPWR_BG);
	SIBA_SHIFTOUT(tssi_a, SIBA_SPROM1_TSSI, SIBA_SPROM1_TSSI_A);
	SIBA_SHIFTOUT(tssi_bg, SIBA_SPROM1_TSSI, SIBA_SPROM1_TSSI_BG);
	SIBA_SHIFTOUT(bf_lo, SIBA_SPROM1_BFLOW, 0xffff);
	if (out->rev >= 2)
		SIBA_SHIFTOUT(bf_hi, SIBA_SPROM2_BFHIGH, 0xffff);

	/* antenna gain */
	gain = siba_sprom_r123_antgain(out->rev, in, SIBA_SPROM1_AGAIN_BG, 0);
	out->again.ghz24.a0 = out->again.ghz24.a1 = gain;
	out->again.ghz24.a2 = out->again.ghz24.a3 = gain;
	gain = siba_sprom_r123_antgain(out->rev, in, SIBA_SPROM1_AGAIN_A, 8);
	out->again.ghz5.a0 = out->again.ghz5.a1 = gain;
	out->again.ghz5.a2 = out->again.ghz5.a3 = gain;
}

static void
siba_sprom_r45(struct siba_sprom *out, const uint16_t *in)
{
	int i;
	uint16_t v;
	uint16_t mac_80211bg_offset;

	if (out->rev == 4)
		mac_80211bg_offset = SIBA_SPROM4_MAC_80211BG;
	else
		mac_80211bg_offset = SIBA_SPROM5_MAC_80211BG;
	for (i = 0; i < 3; i++) {
		v = in[SIBA_OFFSET(mac_80211bg_offset) + i];
		*(((uint16_t *)out->mac_80211bg) + i) = htobe16(v);
	}
	SIBA_SHIFTOUT(mii_eth0, SIBA_SPROM4_ETHPHY, SIBA_SPROM4_ETHPHY_ET0A);
	SIBA_SHIFTOUT(mii_eth1, SIBA_SPROM4_ETHPHY, SIBA_SPROM4_ETHPHY_ET1A);
	if (out->rev == 4) {
		SIBA_SHIFTOUT(ccode, SIBA_SPROM4_CCODE, 0xffff);
		SIBA_SHIFTOUT(bf_lo, SIBA_SPROM4_BFLOW, 0xffff);
		SIBA_SHIFTOUT(bf_hi, SIBA_SPROM4_BFHIGH, 0xffff);
	} else {
		SIBA_SHIFTOUT(ccode, SIBA_SPROM5_CCODE, 0xffff);
		SIBA_SHIFTOUT(bf_lo, SIBA_SPROM5_BFLOW, 0xffff);
		SIBA_SHIFTOUT(bf_hi, SIBA_SPROM5_BFHIGH, 0xffff);
	}
	SIBA_SHIFTOUT(ant_a, SIBA_SPROM4_ANTAVAIL, SIBA_SPROM4_ANTAVAIL_A);
	SIBA_SHIFTOUT(ant_bg, SIBA_SPROM4_ANTAVAIL, SIBA_SPROM4_ANTAVAIL_BG);
	SIBA_SHIFTOUT(maxpwr_bg, SIBA_SPROM4_MAXP_BG, SIBA_SPROM4_MAXP_BG_MASK);
	SIBA_SHIFTOUT(tssi_bg, SIBA_SPROM4_MAXP_BG, SIBA_SPROM4_TSSI_BG);
	SIBA_SHIFTOUT(maxpwr_a, SIBA_SPROM4_MAXP_A, SIBA_SPROM4_MAXP_A_MASK);
	SIBA_SHIFTOUT(tssi_a, SIBA_SPROM4_MAXP_A, SIBA_SPROM4_TSSI_A);
	if (out->rev == 4) {
		SIBA_SHIFTOUT(gpio0, SIBA_SPROM4_GPIOA, SIBA_SPROM4_GPIOA_P0);
		SIBA_SHIFTOUT(gpio1, SIBA_SPROM4_GPIOA, SIBA_SPROM4_GPIOA_P1);
		SIBA_SHIFTOUT(gpio2, SIBA_SPROM4_GPIOB, SIBA_SPROM4_GPIOB_P2);
		SIBA_SHIFTOUT(gpio3, SIBA_SPROM4_GPIOB, SIBA_SPROM4_GPIOB_P3);
	} else {
		SIBA_SHIFTOUT(gpio0, SIBA_SPROM5_GPIOA, SIBA_SPROM5_GPIOA_P0);
		SIBA_SHIFTOUT(gpio1, SIBA_SPROM5_GPIOA, SIBA_SPROM5_GPIOA_P1);
		SIBA_SHIFTOUT(gpio2, SIBA_SPROM5_GPIOB, SIBA_SPROM5_GPIOB_P2);
		SIBA_SHIFTOUT(gpio3, SIBA_SPROM5_GPIOB, SIBA_SPROM5_GPIOB_P3);
	}

	/* antenna gain */
	SIBA_SHIFTOUT(again.ghz24.a0, SIBA_SPROM4_AGAIN01, SIBA_SPROM4_AGAIN0);
	SIBA_SHIFTOUT(again.ghz24.a1, SIBA_SPROM4_AGAIN01, SIBA_SPROM4_AGAIN1);
	SIBA_SHIFTOUT(again.ghz24.a2, SIBA_SPROM4_AGAIN23, SIBA_SPROM4_AGAIN2);
	SIBA_SHIFTOUT(again.ghz24.a3, SIBA_SPROM4_AGAIN23, SIBA_SPROM4_AGAIN3);
	bcopy(&out->again.ghz24, &out->again.ghz5, sizeof(out->again.ghz5));
}

static void
siba_sprom_r8(struct siba_sprom *out, const uint16_t *in)
{
	int i;
	uint16_t v;

	for (i = 0; i < 3; i++) {
		v = in[SIBA_OFFSET(SIBA_SPROM8_MAC_80211BG) + i];
		*(((uint16_t *)out->mac_80211bg) + i) = htobe16(v);
	}
	SIBA_SHIFTOUT(ccode, SIBA_SPROM8_CCODE, 0xffff);
	SIBA_SHIFTOUT(bf_lo, SIBA_SPROM8_BFLOW, 0xffff);
	SIBA_SHIFTOUT(bf_hi, SIBA_SPROM8_BFHIGH, 0xffff);
	SIBA_SHIFTOUT(bf2_lo, SIBA_SPROM8_BFL2LO, 0xffff);
	SIBA_SHIFTOUT(bf2_hi, SIBA_SPROM8_BFL2HI, 0xffff);
	SIBA_SHIFTOUT(ant_a, SIBA_SPROM8_ANTAVAIL, SIBA_SPROM8_ANTAVAIL_A);
	SIBA_SHIFTOUT(ant_bg, SIBA_SPROM8_ANTAVAIL, SIBA_SPROM8_ANTAVAIL_BG);
	SIBA_SHIFTOUT(maxpwr_bg, SIBA_SPROM8_MAXP_BG, SIBA_SPROM8_MAXP_BG_MASK);
	SIBA_SHIFTOUT(tssi_bg, SIBA_SPROM8_MAXP_BG, SIBA_SPROM8_TSSI_BG);
	SIBA_SHIFTOUT(maxpwr_a, SIBA_SPROM8_MAXP_A, SIBA_SPROM8_MAXP_A_MASK);
	SIBA_SHIFTOUT(tssi_a, SIBA_SPROM8_MAXP_A, SIBA_SPROM8_TSSI_A);
	SIBA_SHIFTOUT(maxpwr_ah, SIBA_SPROM8_MAXP_AHL,
	    SIBA_SPROM8_MAXP_AH_MASK);
	SIBA_SHIFTOUT(maxpwr_al, SIBA_SPROM8_MAXP_AHL,
	    SIBA_SPROM8_MAXP_AL_MASK);
	SIBA_SHIFTOUT(gpio0, SIBA_SPROM8_GPIOA, SIBA_SPROM8_GPIOA_P0);
	SIBA_SHIFTOUT(gpio1, SIBA_SPROM8_GPIOA, SIBA_SPROM8_GPIOA_P1);
	SIBA_SHIFTOUT(gpio2, SIBA_SPROM8_GPIOB, SIBA_SPROM8_GPIOB_P2);
	SIBA_SHIFTOUT(gpio3, SIBA_SPROM8_GPIOB, SIBA_SPROM8_GPIOB_P3);
	SIBA_SHIFTOUT(tri2g, SIBA_SPROM8_TRI25G, SIBA_SPROM8_TRI2G);
	SIBA_SHIFTOUT(tri5g, SIBA_SPROM8_TRI25G, SIBA_SPROM8_TRI5G);
	SIBA_SHIFTOUT(tri5gl, SIBA_SPROM8_TRI5GHL, SIBA_SPROM8_TRI5GL);
	SIBA_SHIFTOUT(tri5gh, SIBA_SPROM8_TRI5GHL, SIBA_SPROM8_TRI5GH);
	SIBA_SHIFTOUT(rxpo2g, SIBA_SPROM8_RXPO, SIBA_SPROM8_RXPO2G);
	SIBA_SHIFTOUT(rxpo5g, SIBA_SPROM8_RXPO, SIBA_SPROM8_RXPO5G);
	SIBA_SHIFTOUT(rssismf2g, SIBA_SPROM8_RSSIPARM2G, SIBA_SPROM8_RSSISMF2G);
	SIBA_SHIFTOUT(rssismc2g, SIBA_SPROM8_RSSIPARM2G, SIBA_SPROM8_RSSISMC2G);
	SIBA_SHIFTOUT(rssisav2g, SIBA_SPROM8_RSSIPARM2G, SIBA_SPROM8_RSSISAV2G);
	SIBA_SHIFTOUT(bxa2g, SIBA_SPROM8_RSSIPARM2G, SIBA_SPROM8_BXA2G);
	SIBA_SHIFTOUT(rssismf5g, SIBA_SPROM8_RSSIPARM5G, SIBA_SPROM8_RSSISMF5G);
	SIBA_SHIFTOUT(rssismc5g, SIBA_SPROM8_RSSIPARM5G, SIBA_SPROM8_RSSISMC5G);
	SIBA_SHIFTOUT(rssisav5g, SIBA_SPROM8_RSSIPARM5G, SIBA_SPROM8_RSSISAV5G);
	SIBA_SHIFTOUT(bxa5g, SIBA_SPROM8_RSSIPARM5G, SIBA_SPROM8_BXA5G);

	SIBA_SHIFTOUT(pa0b0, SIBA_SPROM8_PA0B0, 0xffff);
	SIBA_SHIFTOUT(pa0b1, SIBA_SPROM8_PA0B1, 0xffff);
	SIBA_SHIFTOUT(pa0b2, SIBA_SPROM8_PA0B2, 0xffff);
	SIBA_SHIFTOUT(pa1b0, SIBA_SPROM8_PA1B0, 0xffff);
	SIBA_SHIFTOUT(pa1b1, SIBA_SPROM8_PA1B1, 0xffff);
	SIBA_SHIFTOUT(pa1b2, SIBA_SPROM8_PA1B2, 0xffff);
	SIBA_SHIFTOUT(pa1lob0, SIBA_SPROM8_PA1LOB0, 0xffff);
	SIBA_SHIFTOUT(pa1lob1, SIBA_SPROM8_PA1LOB1, 0xffff);
	SIBA_SHIFTOUT(pa1lob2, SIBA_SPROM8_PA1LOB2, 0xffff);
	SIBA_SHIFTOUT(pa1hib0, SIBA_SPROM8_PA1HIB0, 0xffff);
	SIBA_SHIFTOUT(pa1hib1, SIBA_SPROM8_PA1HIB1, 0xffff);
	SIBA_SHIFTOUT(pa1hib2, SIBA_SPROM8_PA1HIB2, 0xffff);
	SIBA_SHIFTOUT(cck2gpo, SIBA_SPROM8_CCK2GPO, 0xffff);

	SIBA_SHIFTOUT_4(ofdm2gpo, SIBA_SPROM8_OFDM2GPO, 0xffffffff, 0);
	SIBA_SHIFTOUT_4(ofdm5glpo, SIBA_SPROM8_OFDM5GLPO, 0xffffffff, 0);
	SIBA_SHIFTOUT_4(ofdm5gpo, SIBA_SPROM8_OFDM5GPO, 0xffffffff, 0);
	SIBA_SHIFTOUT_4(ofdm5ghpo, SIBA_SPROM8_OFDM5GHPO, 0xffffffff, 0);

	/* antenna gain */
	SIBA_SHIFTOUT(again.ghz24.a0, SIBA_SPROM8_AGAIN01, SIBA_SPROM8_AGAIN0);
	SIBA_SHIFTOUT(again.ghz24.a1, SIBA_SPROM8_AGAIN01, SIBA_SPROM8_AGAIN1);
	SIBA_SHIFTOUT(again.ghz24.a2, SIBA_SPROM8_AGAIN23, SIBA_SPROM8_AGAIN2);
	SIBA_SHIFTOUT(again.ghz24.a3, SIBA_SPROM8_AGAIN23, SIBA_SPROM8_AGAIN3);
	bcopy(&out->again.ghz24, &out->again.ghz5, sizeof(out->again.ghz5));
}

static int8_t
siba_sprom_r123_antgain(uint8_t sprom_revision, const uint16_t *in,
    uint16_t mask, uint16_t shift)
{
	uint16_t v;
	uint8_t gain;

	v = in[SIBA_OFFSET(SIBA_SPROM1_AGAIN)];
	gain = (v & mask) >> shift;
	gain = (gain == 0xff) ? 2 : (sprom_revision == 1) ? gain << 2 :
	    ((gain & 0xc0) >> 6) | ((gain & 0x3f) << 2);

	return ((int8_t)gain);
}

#undef SIBA_LOWEST_SET_BIT
#undef SIBA_OFFSET
#undef SIBA_SHIFTOUT_SUB
#undef SIBA_SHIFTOUT

int
siba_powerdown(device_t dev)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;

	return (siba_powerdown_sub(siba));
}

static int
siba_powerdown_sub(struct siba_softc *siba)
{
	struct siba_cc *scc;

	if (siba->siba_type == SIBA_TYPE_SSB)
		return (0);

	scc = &siba->siba_cc;
	if (!scc->scc_dev || scc->scc_dev->sd_id.sd_rev < 5)
		return (0);
	siba_cc_clock(scc, SIBA_CLOCK_SLOW);
	siba_pci_gpio(siba, SIBA_GPIO_CRYSTAL | SIBA_GPIO_PLL, 0);
	return (0);
}

static void
siba_pcicore_init(struct siba_pci *spc)
{
	struct siba_dev_softc *sd = spc->spc_dev;
	struct siba_softc *siba;

	if (sd == NULL)
		return;

	siba = sd->sd_bus;
	if (!siba_dev_isup_sub(sd))
		siba_dev_up_sub(sd, 0);

	KASSERT(spc->spc_hostmode == 0,
	    ("%s:%d: hostmode", __func__, __LINE__));
	/* disable PCI interrupt */
	siba_write_4_sub(spc->spc_dev, SIBA_INTR_MASK, 0);
}

int
siba_dev_isup(device_t dev)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	return (siba_dev_isup_sub(sd));
}

static int
siba_dev_isup_sub(struct siba_dev_softc *sd)
{
	uint32_t reject, val;

	reject = siba_tmslow_reject_bitmask(sd);
	val = siba_read_4_sub(sd, SIBA_TGSLOW);
	val &= SIBA_TGSLOW_CLOCK | SIBA_TGSLOW_RESET | reject;

	return (val == SIBA_TGSLOW_CLOCK);
}

void
siba_dev_up(device_t dev, uint32_t flags)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	siba_dev_up_sub(sd, flags);
}

static void
siba_dev_up_sub(struct siba_dev_softc *sd, uint32_t flags)
{
	uint32_t val;

	siba_dev_down_sub(sd, flags);
	siba_write_4_sub(sd, SIBA_TGSLOW,
	    SIBA_TGSLOW_RESET | SIBA_TGSLOW_CLOCK | SIBA_TGSLOW_FGC | flags);
	siba_read_4_sub(sd, SIBA_TGSLOW);
	DELAY(1);

	if (siba_read_4_sub(sd, SIBA_TGSHIGH) & SIBA_TGSHIGH_SERR)
		siba_write_4_sub(sd, SIBA_TGSHIGH, 0);

	val = siba_read_4_sub(sd, SIBA_IAS);
	if (val & (SIBA_IAS_INBAND_ERR | SIBA_IAS_TIMEOUT)) {
		val &= ~(SIBA_IAS_INBAND_ERR | SIBA_IAS_TIMEOUT);
		siba_write_4_sub(sd, SIBA_IAS, val);
	}

	siba_write_4_sub(sd, SIBA_TGSLOW,
	    SIBA_TGSLOW_CLOCK | SIBA_TGSLOW_FGC | flags);
	siba_read_4_sub(sd, SIBA_TGSLOW);
	DELAY(1);

	siba_write_4_sub(sd, SIBA_TGSLOW, SIBA_TGSLOW_CLOCK | flags);
	siba_read_4_sub(sd, SIBA_TGSLOW);
	DELAY(1);
}

static uint32_t
siba_tmslow_reject_bitmask(struct siba_dev_softc *sd)
{
	uint32_t rev = siba_read_4_sub(sd, SIBA_IDLOW) & SIBA_IDLOW_SSBREV;

	switch (rev) {
	case SIBA_IDLOW_SSBREV_22:
		return (SIBA_TGSLOW_REJECT_22);
	case SIBA_IDLOW_SSBREV_23:
		return (SIBA_TGSLOW_REJECT_23);
	case SIBA_IDLOW_SSBREV_24:
	case SIBA_IDLOW_SSBREV_25:
	case SIBA_IDLOW_SSBREV_26:
	case SIBA_IDLOW_SSBREV_27:
		return (SIBA_TGSLOW_REJECT_23);
	default:
		KASSERT(0 == 1,
		    ("%s:%d: unknown backplane rev %#x\n",
			__func__, __LINE__, rev));
	}
	return (SIBA_TGSLOW_REJECT_22 | SIBA_TGSLOW_REJECT_23);
}

void
siba_dev_down(device_t dev, uint32_t flags)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);

	siba_dev_down_sub(sd, flags);
}

static void
siba_dev_down_sub(struct siba_dev_softc *sd, uint32_t flags)
{
	struct siba_softc *siba = sd->sd_bus;
	uint32_t reject, val;
	int i;

	if (siba_read_4_sub(sd, SIBA_TGSLOW) & SIBA_TGSLOW_RESET)
		return;

	reject = siba_tmslow_reject_bitmask(sd);
	siba_write_4_sub(sd, SIBA_TGSLOW, reject | SIBA_TGSLOW_CLOCK);

	for (i = 0; i < 1000; i++) {
		val = siba_read_4_sub(sd, SIBA_TGSLOW);
		if (val & reject)
			break;
		DELAY(10);
	}
	if ((val & reject) == 0) {
		device_printf(siba->siba_dev, "timeout (bit %#x reg %#x)\n",
		    reject, SIBA_TGSLOW);
	}
	for (i = 0; i < 1000; i++) {
		val = siba_read_4_sub(sd, SIBA_TGSHIGH);
		if (!(val & SIBA_TGSHIGH_BUSY))
			break;
		DELAY(10);
	}
	if ((val & SIBA_TGSHIGH_BUSY) != 0) {
		device_printf(siba->siba_dev, "timeout (bit %#x reg %#x)\n",
		    SIBA_TGSHIGH_BUSY, SIBA_TGSHIGH);
	}

	siba_write_4_sub(sd, SIBA_TGSLOW, SIBA_TGSLOW_FGC | SIBA_TGSLOW_CLOCK |
	    reject | SIBA_TGSLOW_RESET | flags);
	siba_read_4_sub(sd, SIBA_TGSLOW);
	DELAY(1);
	siba_write_4_sub(sd, SIBA_TGSLOW, reject | SIBA_TGSLOW_RESET | flags);
	siba_read_4_sub(sd, SIBA_TGSLOW);
	DELAY(1);
}

static void
siba_pcicore_setup(struct siba_pci *spc, struct siba_dev_softc *sd)
{
	struct siba_dev_softc *psd = spc->spc_dev;
	struct siba_softc *siba = psd->sd_bus;
	uint32_t tmp;

	if (psd->sd_id.sd_device == SIBA_DEVID_PCI) {
		siba_pcicore_write_4(spc, SIBA_PCICORE_SBTOPCI2,
		    siba_pcicore_read_4(spc, SIBA_PCICORE_SBTOPCI2) |
		    SIBA_PCICORE_SBTOPCI_PREF | SIBA_PCICORE_SBTOPCI_BURST);

		if (psd->sd_id.sd_rev < 5) {
			tmp = siba_read_4_sub(psd, SIBA_IMCFGLO);
			tmp &= ~SIBA_IMCFGLO_SERTO;
			tmp = (tmp | 2) & ~SIBA_IMCFGLO_REQTO;
			tmp |= 3 << 4 /* SIBA_IMCFGLO_REQTO_SHIFT */;
			siba_write_4_sub(psd, SIBA_IMCFGLO, tmp);

			/* broadcast value */
			sd = (siba->siba_cc.scc_dev != NULL) ?
			    siba->siba_cc.scc_dev : siba->siba_pci.spc_dev;
			if (sd != NULL) {
				siba_write_4_sub(sd, SIBA_PCICORE_BCAST_ADDR,
				    0xfd8);
				siba_read_4_sub(sd, SIBA_PCICORE_BCAST_ADDR);
				siba_write_4_sub(sd,
				    SIBA_PCICORE_BCAST_DATA, 0);
				siba_read_4_sub(sd, SIBA_PCICORE_BCAST_DATA);
			}
		} else if (psd->sd_id.sd_rev >= 11) {
			tmp = siba_pcicore_read_4(spc, SIBA_PCICORE_SBTOPCI2);
			tmp |= SIBA_PCICORE_SBTOPCI_MRM;
			siba_pcicore_write_4(spc, SIBA_PCICORE_SBTOPCI2, tmp);
		}
	} else {
		KASSERT(psd->sd_id.sd_device == SIBA_DEVID_PCIE, ("only PCIE"));
		if ((psd->sd_id.sd_rev == 0) || (psd->sd_id.sd_rev == 1))
			siba_pcie_write(spc, 0x4,
			    siba_pcie_read(spc, 0x4) | 0x8);
		if (psd->sd_id.sd_rev == 0) {
			siba_pcie_mdio_write(spc, 0x1f, 2, 0x8128); /* Timer */
			siba_pcie_mdio_write(spc, 0x1f, 6, 0x0100); /* CDR */
			siba_pcie_mdio_write(spc, 0x1f, 7, 0x1466); /* CDR BW */
		} else if (psd->sd_id.sd_rev == 1)
			siba_pcie_write(spc, 0x100,
			    siba_pcie_read(spc, 0x100) | 0x40);
	}
	spc->spc_inited = 1;
}

void
siba_pcicore_intr(device_t dev)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;
	struct siba_pci *spc = &siba->siba_pci;
	struct siba_dev_softc *psd = spc->spc_dev;
	uint32_t tmp;

	if (siba->siba_type != SIBA_TYPE_PCI || !psd)
		return;

	KASSERT(siba == psd->sd_bus, ("different pointers"));

	/* enable interrupts */
	if (siba->siba_dev != NULL &&
	    (psd->sd_id.sd_rev >= 6 ||
	     psd->sd_id.sd_device == SIBA_DEVID_PCIE)) {
		tmp = pci_read_config(siba->siba_dev, SIBA_IRQMASK, 4);
		tmp |= (1 << sd->sd_coreidx) << 8;
		pci_write_config(siba->siba_dev, SIBA_IRQMASK, tmp, 4);
	} else {
		tmp = siba_read_4_sub(sd, SIBA_TPS);
		tmp &= SIBA_TPS_BPFLAG;
		siba_write_4_sub(psd, SIBA_INTR_MASK,
		    siba_read_4_sub(psd, SIBA_INTR_MASK) | (1 << tmp));
	}

	/* setup PCIcore */
	if (spc->spc_inited == 0)
		siba_pcicore_setup(spc, sd);
}

static uint32_t
siba_pcicore_read_4(struct siba_pci *spc, uint16_t offset)
{

	return (siba_read_4_sub(spc->spc_dev, offset));
}

static void
siba_pcicore_write_4(struct siba_pci *spc, uint16_t offset, uint32_t value)
{

	siba_write_4_sub(spc->spc_dev, offset, value);
}

static uint32_t
siba_pcie_read(struct siba_pci *spc, uint32_t address)
{

	siba_pcicore_write_4(spc, 0x130, address);
	return (siba_pcicore_read_4(spc, 0x134));
}

static void
siba_pcie_write(struct siba_pci *spc, uint32_t address, uint32_t data)
{

	siba_pcicore_write_4(spc, 0x130, address);
	siba_pcicore_write_4(spc, 0x134, data);
}

static void
siba_pcie_mdio_write(struct siba_pci *spc, uint8_t device, uint8_t address,
    uint16_t data)
{
	int i;

	siba_pcicore_write_4(spc, SIBA_PCICORE_MDIO_CTL, 0x80 | 0x2);
	siba_pcicore_write_4(spc, SIBA_PCICORE_MDIO_DATA,
	    (1 << 30) | (1 << 28) |
	    ((uint32_t)device << 22) | ((uint32_t)address << 18) |
	    (1 << 17) | data);
	DELAY(10);
	for (i = 0; i < 10; i++) {
		if (siba_pcicore_read_4(spc, SIBA_PCICORE_MDIO_CTL) & 0x100)
			break;
		DELAY(1000);
	}
	siba_pcicore_write_4(spc, SIBA_PCICORE_MDIO_CTL, 0);
}

uint32_t
siba_dma_translation(device_t dev)
{
#ifdef INVARIANTS
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;

	KASSERT(siba->siba_type == SIBA_TYPE_PCI,
	    ("unsupported bustype %d\n", siba->siba_type));
#endif
	return (SIBA_PCI_DMA);
}

void
siba_barrier(device_t dev, int flags)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;

	SIBA_BARRIER(siba, flags);
}

static void
siba_cc_suspend(struct siba_cc *scc)
{

	siba_cc_clock(scc, SIBA_CLOCK_SLOW);
}

static void
siba_cc_resume(struct siba_cc *scc)
{

	siba_cc_power_init(scc);
	siba_cc_clock(scc, SIBA_CLOCK_FAST);
}

int
siba_core_suspend(struct siba_softc *siba)
{

	siba_cc_suspend(&siba->siba_cc);
	siba_pci_gpio(siba, SIBA_GPIO_CRYSTAL | SIBA_GPIO_PLL, 0);
	return (0);
}

int
siba_core_resume(struct siba_softc *siba)
{

	siba->siba_pci.spc_inited = 0;
	siba->siba_curdev = NULL;

	siba_powerup_sub(siba, 0);
	/* XXX setup H/W for PCMCIA??? */
	siba_cc_resume(&siba->siba_cc);
	siba_powerdown_sub(siba);

	return (0);
}

static void
siba_cc_regctl_setmask(struct siba_cc *cc, uint32_t offset, uint32_t mask,
    uint32_t set)
{

	SIBA_CC_READ32(cc, SIBA_CC_REGCTL_ADDR);
	SIBA_CC_WRITE32(cc, SIBA_CC_REGCTL_ADDR, offset);
	SIBA_CC_READ32(cc, SIBA_CC_REGCTL_ADDR);
	SIBA_CC_WRITE32(cc, SIBA_CC_REGCTL_DATA,
	    (SIBA_CC_READ32(cc, SIBA_CC_REGCTL_DATA) & mask) | set);
	SIBA_CC_READ32(cc, SIBA_CC_REGCTL_DATA);
}

void
siba_cc_pmu_set_ldovolt(device_t dev, int id, uint32_t volt)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;
	struct siba_cc *scc = &siba->siba_cc;
	uint32_t *p = NULL, info[5][3] = {
		{ 2, 25,  0xf },
		{ 3,  1,  0xf },
		{ 3,  9,  0xf },
		{ 3, 17, 0x3f },
		{ 0, 21, 0x3f }
	};

	if (siba->siba_chipid == 0x4312) {
		if (id != SIBA_LDO_PAREF)
			return;
		p = info[4];
		siba_cc_regctl_setmask(scc, p[0], ~(p[2] << p[1]),
		    (volt & p[2]) << p[1]);
		return;
	}
	if (siba->siba_chipid == 0x4328 || siba->siba_chipid == 0x5354) {
		switch (id) {
		case SIBA_LDO_PAREF:
			p = info[3];
			break;
		case SIBA_LDO_VOLT1:
			p = info[0];
			break;
		case SIBA_LDO_VOLT2:
			p = info[1];
			break;
		case SIBA_LDO_VOLT3:
			p = info[2];
			break;
		default:
			KASSERT(0 == 1,
			    ("%s: unsupported voltage ID %#x", __func__, id));
			return;
		}
		siba_cc_regctl_setmask(scc, p[0], ~(p[2] << p[1]),
		    (volt & p[2]) << p[1]);
	}
}

void
siba_cc_pmu_set_ldoparef(device_t dev, uint8_t on)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;
	struct siba_cc *scc = &siba->siba_cc;
	int ldo;

	ldo = ((siba->siba_chipid == 0x4312) ? SIBA_CC_PMU_4312_PA_REF :
	    ((siba->siba_chipid == 0x4328) ? SIBA_CC_PMU_4328_PA_REF :
	    ((siba->siba_chipid == 0x5354) ? SIBA_CC_PMU_5354_PA_REF : -1)));
	if (ldo == -1)
		return;

	if (on)
		SIBA_CC_SET32(scc, SIBA_CC_PMU_MINRES, 1 << ldo);
	else
		SIBA_CC_MASK32(scc, SIBA_CC_PMU_MINRES, ~(1 << ldo));
	SIBA_CC_READ32(scc, SIBA_CC_PMU_MINRES);
}

int
siba_read_sprom(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct siba_dev_softc *sd = device_get_ivars(child);
	struct siba_softc *siba = sd->sd_bus;

	switch (which) {
	case SIBA_SPROMVAR_REV:
		*result = siba->siba_sprom.rev;
		break;
	case SIBA_SPROMVAR_MAC_80211BG:
		*((uint8_t **) result) = siba->siba_sprom.mac_80211bg;
		break;
	case SIBA_SPROMVAR_MAC_ETH:
		*((uint8_t **) result) = siba->siba_sprom.mac_eth;
		break;
	case SIBA_SPROMVAR_MAC_80211A:
		*((uint8_t **) result) = siba->siba_sprom.mac_80211a;
		break;
	case SIBA_SPROMVAR_MII_ETH0:
		*result = siba->siba_sprom.mii_eth0;
		break;
	case SIBA_SPROMVAR_MII_ETH1:
		*result = siba->siba_sprom.mii_eth1;
		break;
	case SIBA_SPROMVAR_MDIO_ETH0:
		*result = siba->siba_sprom.mdio_eth0;
		break;
	case SIBA_SPROMVAR_MDIO_ETH1:
		*result = siba->siba_sprom.mdio_eth1;
		break;
	case SIBA_SPROMVAR_BREV:
		*result = siba->siba_sprom.brev;
		break;
	case SIBA_SPROMVAR_CCODE:
		*result = siba->siba_sprom.ccode;
		break;
	case SIBA_SPROMVAR_ANT_A:
		*result = siba->siba_sprom.ant_a;
		break;
	case SIBA_SPROMVAR_ANT_BG:
		*result = siba->siba_sprom.ant_bg;
		break;
	case SIBA_SPROMVAR_PA0B0:
		*result = siba->siba_sprom.pa0b0;
		break;
	case SIBA_SPROMVAR_PA0B1:
		*result = siba->siba_sprom.pa0b1;
		break;
	case SIBA_SPROMVAR_PA0B2:
		*result = siba->siba_sprom.pa0b2;
		break;
	case SIBA_SPROMVAR_PA1B0:
		*result = siba->siba_sprom.pa1b0;
		break;
	case SIBA_SPROMVAR_PA1B1:
		*result = siba->siba_sprom.pa1b1;
		break;
	case SIBA_SPROMVAR_PA1B2:
		*result = siba->siba_sprom.pa1b2;
		break;
	case SIBA_SPROMVAR_PA1LOB0:
		*result = siba->siba_sprom.pa1lob0;
		break;
	case SIBA_SPROMVAR_PA1LOB1:
		*result = siba->siba_sprom.pa1lob1;
		break;
	case SIBA_SPROMVAR_PA1LOB2:
		*result = siba->siba_sprom.pa1lob2;
		break;
	case SIBA_SPROMVAR_PA1HIB0:
		*result = siba->siba_sprom.pa1hib0;
		break;
	case SIBA_SPROMVAR_PA1HIB1:
		*result = siba->siba_sprom.pa1hib1;
		break;
	case SIBA_SPROMVAR_PA1HIB2:
		*result = siba->siba_sprom.pa1hib2;
		break;
	case SIBA_SPROMVAR_GPIO0:
		*result = siba->siba_sprom.gpio0;
		break;
	case SIBA_SPROMVAR_GPIO1:
		*result = siba->siba_sprom.gpio1;
		break;
	case SIBA_SPROMVAR_GPIO2:
		*result = siba->siba_sprom.gpio2;
		break;
	case SIBA_SPROMVAR_GPIO3:
		*result = siba->siba_sprom.gpio3;
		break;
	case SIBA_SPROMVAR_MAXPWR_AL:
		*result = siba->siba_sprom.maxpwr_al;
		break;
	case SIBA_SPROMVAR_MAXPWR_A:
		*result = siba->siba_sprom.maxpwr_a;
		break;
	case SIBA_SPROMVAR_MAXPWR_AH:
		*result = siba->siba_sprom.maxpwr_ah;
		break;
	case SIBA_SPROMVAR_MAXPWR_BG:
		*result = siba->siba_sprom.maxpwr_bg;
		break;
	case SIBA_SPROMVAR_RXPO2G:
		*result = siba->siba_sprom.rxpo2g;
		break;
	case SIBA_SPROMVAR_RXPO5G:
		*result = siba->siba_sprom.rxpo5g;
		break;
	case SIBA_SPROMVAR_TSSI_A:
		*result = siba->siba_sprom.tssi_a;
		break;
	case SIBA_SPROMVAR_TSSI_BG:
		*result = siba->siba_sprom.tssi_bg;
		break;
	case SIBA_SPROMVAR_TRI2G:
		*result = siba->siba_sprom.tri2g;
		break;
	case SIBA_SPROMVAR_TRI5GL:
		*result = siba->siba_sprom.tri5gl;
		break;
	case SIBA_SPROMVAR_TRI5G:
		*result = siba->siba_sprom.tri5g;
		break;
	case SIBA_SPROMVAR_TRI5GH:
		*result = siba->siba_sprom.tri5gh;
		break;
	case SIBA_SPROMVAR_RSSISAV2G:
		*result = siba->siba_sprom.rssisav2g;
		break;
	case SIBA_SPROMVAR_RSSISMC2G:
		*result = siba->siba_sprom.rssismc2g;
		break;
	case SIBA_SPROMVAR_RSSISMF2G:
		*result = siba->siba_sprom.rssismf2g;
		break;
	case SIBA_SPROMVAR_BXA2G:
		*result = siba->siba_sprom.bxa2g;
		break;
	case SIBA_SPROMVAR_RSSISAV5G:
		*result = siba->siba_sprom.rssisav5g;
		break;
	case SIBA_SPROMVAR_RSSISMC5G:
		*result = siba->siba_sprom.rssismc5g;
		break;
	case SIBA_SPROMVAR_RSSISMF5G:
		*result = siba->siba_sprom.rssismf5g;
		break;
	case SIBA_SPROMVAR_BXA5G:
		*result = siba->siba_sprom.bxa5g;
		break;
	case SIBA_SPROMVAR_CCK2GPO:
		*result = siba->siba_sprom.cck2gpo;
		break;
	case SIBA_SPROMVAR_OFDM2GPO:
		*result = siba->siba_sprom.ofdm2gpo;
		break;
	case SIBA_SPROMVAR_OFDM5GLPO:
		*result = siba->siba_sprom.ofdm5glpo;
		break;
	case SIBA_SPROMVAR_OFDM5GPO:
		*result = siba->siba_sprom.ofdm5gpo;
		break;
	case SIBA_SPROMVAR_OFDM5GHPO:
		*result = siba->siba_sprom.ofdm5ghpo;
		break;
	case SIBA_SPROMVAR_BF_LO:
		*result = siba->siba_sprom.bf_lo;
		break;
	case SIBA_SPROMVAR_BF_HI:
		*result = siba->siba_sprom.bf_hi;
		break;
	case SIBA_SPROMVAR_BF2_LO:
		*result = siba->siba_sprom.bf2_lo;
		break;
	case SIBA_SPROMVAR_BF2_HI:
		*result = siba->siba_sprom.bf2_hi;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

int
siba_write_sprom(device_t dev, device_t child, int which, uintptr_t value)
{
	struct siba_dev_softc *sd = device_get_ivars(child);
	struct siba_softc *siba = sd->sd_bus;

	switch (which) {
	case SIBA_SPROMVAR_REV:
		siba->siba_sprom.rev = value;
		break;
	case SIBA_SPROMVAR_MII_ETH0:
		siba->siba_sprom.mii_eth0 = value;
		break;
	case SIBA_SPROMVAR_MII_ETH1:
		siba->siba_sprom.mii_eth1 = value;
		break;
	case SIBA_SPROMVAR_MDIO_ETH0:
		siba->siba_sprom.mdio_eth0 = value;
		break;
	case SIBA_SPROMVAR_MDIO_ETH1:
		siba->siba_sprom.mdio_eth1 = value;
		break;
	case SIBA_SPROMVAR_BREV:
		siba->siba_sprom.brev = value;
		break;
	case SIBA_SPROMVAR_CCODE:
		siba->siba_sprom.ccode = value;
		break;
	case SIBA_SPROMVAR_ANT_A:
		siba->siba_sprom.ant_a = value;
		break;
	case SIBA_SPROMVAR_ANT_BG:
		siba->siba_sprom.ant_bg = value;
		break;
	case SIBA_SPROMVAR_PA0B0:
		siba->siba_sprom.pa0b0 = value;
		break;
	case SIBA_SPROMVAR_PA0B1:
		siba->siba_sprom.pa0b1 = value;
		break;
	case SIBA_SPROMVAR_PA0B2:
		siba->siba_sprom.pa0b2 = value;
		break;
	case SIBA_SPROMVAR_PA1B0:
		siba->siba_sprom.pa1b0 = value;
		break;
	case SIBA_SPROMVAR_PA1B1:
		siba->siba_sprom.pa1b1 = value;
		break;
	case SIBA_SPROMVAR_PA1B2:
		siba->siba_sprom.pa1b2 = value;
		break;
	case SIBA_SPROMVAR_PA1LOB0:
		siba->siba_sprom.pa1lob0 = value;
		break;
	case SIBA_SPROMVAR_PA1LOB1:
		siba->siba_sprom.pa1lob1 = value;
		break;
	case SIBA_SPROMVAR_PA1LOB2:
		siba->siba_sprom.pa1lob2 = value;
		break;
	case SIBA_SPROMVAR_PA1HIB0:
		siba->siba_sprom.pa1hib0 = value;
		break;
	case SIBA_SPROMVAR_PA1HIB1:
		siba->siba_sprom.pa1hib1 = value;
		break;
	case SIBA_SPROMVAR_PA1HIB2:
		siba->siba_sprom.pa1hib2 = value;
		break;
	case SIBA_SPROMVAR_GPIO0:
		siba->siba_sprom.gpio0 = value;
		break;
	case SIBA_SPROMVAR_GPIO1:
		siba->siba_sprom.gpio1 = value;
		break;
	case SIBA_SPROMVAR_GPIO2:
		siba->siba_sprom.gpio2 = value;
		break;
	case SIBA_SPROMVAR_GPIO3:
		siba->siba_sprom.gpio3 = value;
		break;
	case SIBA_SPROMVAR_MAXPWR_AL:
		siba->siba_sprom.maxpwr_al = value;
		break;
	case SIBA_SPROMVAR_MAXPWR_A:
		siba->siba_sprom.maxpwr_a = value;
		break;
	case SIBA_SPROMVAR_MAXPWR_AH:
		siba->siba_sprom.maxpwr_ah = value;
		break;
	case SIBA_SPROMVAR_MAXPWR_BG:
		siba->siba_sprom.maxpwr_bg = value;
		break;
	case SIBA_SPROMVAR_RXPO2G:
		siba->siba_sprom.rxpo2g = value;
		break;
	case SIBA_SPROMVAR_RXPO5G:
		siba->siba_sprom.rxpo5g = value;
		break;
	case SIBA_SPROMVAR_TSSI_A:
		siba->siba_sprom.tssi_a = value;
		break;
	case SIBA_SPROMVAR_TSSI_BG:
		siba->siba_sprom.tssi_bg = value;
		break;
	case SIBA_SPROMVAR_TRI2G:
		siba->siba_sprom.tri2g = value;
		break;
	case SIBA_SPROMVAR_TRI5GL:
		siba->siba_sprom.tri5gl = value;
		break;
	case SIBA_SPROMVAR_TRI5G:
		siba->siba_sprom.tri5g = value;
		break;
	case SIBA_SPROMVAR_TRI5GH:
		siba->siba_sprom.tri5gh = value;
		break;
	case SIBA_SPROMVAR_RSSISAV2G:
		siba->siba_sprom.rssisav2g = value;
		break;
	case SIBA_SPROMVAR_RSSISMC2G:
		siba->siba_sprom.rssismc2g = value;
		break;
	case SIBA_SPROMVAR_RSSISMF2G:
		siba->siba_sprom.rssismf2g = value;
		break;
	case SIBA_SPROMVAR_BXA2G:
		siba->siba_sprom.bxa2g = value;
		break;
	case SIBA_SPROMVAR_RSSISAV5G:
		siba->siba_sprom.rssisav5g = value;
		break;
	case SIBA_SPROMVAR_RSSISMC5G:
		siba->siba_sprom.rssismc5g = value;
		break;
	case SIBA_SPROMVAR_RSSISMF5G:
		siba->siba_sprom.rssismf5g = value;
		break;
	case SIBA_SPROMVAR_BXA5G:
		siba->siba_sprom.bxa5g = value;
		break;
	case SIBA_SPROMVAR_CCK2GPO:
		siba->siba_sprom.cck2gpo = value;
		break;
	case SIBA_SPROMVAR_OFDM2GPO:
		siba->siba_sprom.ofdm2gpo = value;
		break;
	case SIBA_SPROMVAR_OFDM5GLPO:
		siba->siba_sprom.ofdm5glpo = value;
		break;
	case SIBA_SPROMVAR_OFDM5GPO:
		siba->siba_sprom.ofdm5gpo = value;
		break;
	case SIBA_SPROMVAR_OFDM5GHPO:
		siba->siba_sprom.ofdm5ghpo = value;
		break;
	case SIBA_SPROMVAR_BF_LO:
		siba->siba_sprom.bf_lo = value;
		break;
	case SIBA_SPROMVAR_BF_HI:
		siba->siba_sprom.bf_hi = value;
		break;
	case SIBA_SPROMVAR_BF2_LO:
		siba->siba_sprom.bf2_lo = value;
		break;
	case SIBA_SPROMVAR_BF2_HI:
		siba->siba_sprom.bf2_hi = value;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

#define	SIBA_GPIOCTL			0x06c

uint32_t
siba_gpio_get(device_t dev)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;
	struct siba_dev_softc *gpiodev, *pcidev = NULL;

	pcidev = siba->siba_pci.spc_dev;
	gpiodev = siba->siba_cc.scc_dev ? siba->siba_cc.scc_dev : pcidev;
	if (!gpiodev)
		return (-1);
	return (siba_read_4_sub(gpiodev, SIBA_GPIOCTL));
}

void
siba_gpio_set(device_t dev, uint32_t value)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;
	struct siba_dev_softc *gpiodev, *pcidev = NULL;

	pcidev = siba->siba_pci.spc_dev;
	gpiodev = siba->siba_cc.scc_dev ? siba->siba_cc.scc_dev : pcidev;
	if (!gpiodev)
		return;
	siba_write_4_sub(gpiodev, SIBA_GPIOCTL, value);
}

void
siba_fix_imcfglobug(device_t dev)
{
	struct siba_dev_softc *sd = device_get_ivars(dev);
	struct siba_softc *siba = sd->sd_bus;
	uint32_t tmp;

	if (siba->siba_pci.spc_dev == NULL)
		return;
	if (siba->siba_pci.spc_dev->sd_id.sd_device != SIBA_DEVID_PCI ||
	    siba->siba_pci.spc_dev->sd_id.sd_rev > 5)
		return;

	tmp = siba_read_4_sub(sd, SIBA_IMCFGLO) &
	    ~(SIBA_IMCFGLO_REQTO | SIBA_IMCFGLO_SERTO);
	switch (siba->siba_type) {
	case SIBA_TYPE_PCI:
	case SIBA_TYPE_PCMCIA:
		tmp |= 0x32;
		break;
	case SIBA_TYPE_SSB:
		tmp |= 0x53;
		break;
	}
	siba_write_4_sub(sd, SIBA_IMCFGLO, tmp);
}
