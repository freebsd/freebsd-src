/*-
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2001
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Broadcom BCM570x family gigabit ethernet driver for FreeBSD.
 *
 * The Broadcom BCM5700 is based on technology originally developed by
 * Alteon Networks as part of the Tigon I and Tigon II gigabit ethernet
 * MAC chips. The BCM5700, sometimes refered to as the Tigon III, has
 * two on-board MIPS R4000 CPUs and can have as much as 16MB of external
 * SSRAM. The BCM5700 supports TCP, UDP and IP checksum offload, jumbo
 * frames, highly configurable RX filtering, and 16 RX and TX queues
 * (which, along with RX filter rules, can be used for QOS applications).
 * Other features, such as TCP segmentation, may be available as part
 * of value-added firmware updates. Unlike the Tigon I and Tigon II,
 * firmware images can be stored in hardware and need not be compiled
 * into the driver.
 *
 * The BCM5700 supports the PCI v2.2 and PCI-X v1.0 standards, and will
 * function in a 32-bit/64-bit 33/66Mhz bus, or a 64-bit/133Mhz bus.
 *
 * The BCM5701 is a single-chip solution incorporating both the BCM5700
 * MAC and a BCM5401 10/100/1000 PHY. Unlike the BCM5700, the BCM5701
 * does not support external SSRAM.
 *
 * Broadcom also produces a variation of the BCM5700 under the "Altima"
 * brand name, which is functionally similar but lacks PCI-X support.
 *
 * Without external SSRAM, you can only have at most 4 TX rings,
 * and the use of the mini RX ring is disabled. This seems to imply
 * that these features are simply not available on the BCM5701. As a
 * result, this driver does not implement any support for the mini RX
 * ring.
 */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"
#include <dev/mii/brgphyreg.h>

#ifdef __sparc64__
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>
#include <machine/ver.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/bge/if_bgereg.h>

#define	BGE_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)
#define	ETHER_MIN_NOPAD		(ETHER_MIN_LEN - ETHER_CRC_LEN) /* i.e., 60 */

MODULE_DEPEND(bge, pci, 1, 1, 1);
MODULE_DEPEND(bge, ether, 1, 1, 1);
MODULE_DEPEND(bge, miibus, 1, 1, 1);

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Various supported device vendors/types and their names. Note: the
 * spec seems to indicate that the hardware still has Alteon's vendor
 * ID burned into it, though it will always be overriden by the vendor
 * ID in the EEPROM. Just to be safe, we cover all possibilities.
 */
static const struct bge_type {
	uint16_t	bge_vid;
	uint16_t	bge_did;
} bge_devs[] = {
	{ ALTEON_VENDORID,	ALTEON_DEVICEID_BCM5700 },
	{ ALTEON_VENDORID,	ALTEON_DEVICEID_BCM5701 },

	{ ALTIMA_VENDORID,	ALTIMA_DEVICE_AC1000 },
	{ ALTIMA_VENDORID,	ALTIMA_DEVICE_AC1002 },
	{ ALTIMA_VENDORID,	ALTIMA_DEVICE_AC9100 },

	{ APPLE_VENDORID,	APPLE_DEVICE_BCM5701 },

	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5700 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5701 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5702 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5702_ALT },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5702X },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5703 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5703_ALT },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5703X },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5704C },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5704S },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5704S_ALT },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5705 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5705F },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5705K },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5705M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5705M_ALT },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5714C },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5714S },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5715 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5715S },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5720 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5721 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5722 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5723 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5750 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5750M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5751 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5751F },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5751M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5752 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5752M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5753 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5753F },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5753M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5754 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5754M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5755 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5755M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5761 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5761E },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5761S },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5761SE },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5764 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5780 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5780S },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5781 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5782 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5784 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5785F },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5785G },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5786 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5787 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5787F },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5787M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5788 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5789 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5901 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5901A2 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5903M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5906 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM5906M },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM57760 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM57780 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM57788 },
	{ BCOM_VENDORID,	BCOM_DEVICEID_BCM57790 },

	{ SK_VENDORID,		SK_DEVICEID_ALTIMA },

	{ TC_VENDORID,		TC_DEVICEID_3C996 },

	{ FJTSU_VENDORID,	FJTSU_DEVICEID_PW008GE4 },
	{ FJTSU_VENDORID,	FJTSU_DEVICEID_PW008GE5 },
	{ FJTSU_VENDORID,	FJTSU_DEVICEID_PP250450 },

	{ 0, 0 }
};

static const struct bge_vendor {
	uint16_t	v_id;
	const char	*v_name;
} bge_vendors[] = {
	{ ALTEON_VENDORID,	"Alteon" },
	{ ALTIMA_VENDORID,	"Altima" },
	{ APPLE_VENDORID,	"Apple" },
	{ BCOM_VENDORID,	"Broadcom" },
	{ SK_VENDORID,		"SysKonnect" },
	{ TC_VENDORID,		"3Com" },
	{ FJTSU_VENDORID,	"Fujitsu" },

	{ 0, NULL }
};

static const struct bge_revision {
	uint32_t	br_chipid;
	const char	*br_name;
} bge_revisions[] = {
	{ BGE_CHIPID_BCM5700_A0,	"BCM5700 A0" },
	{ BGE_CHIPID_BCM5700_A1,	"BCM5700 A1" },
	{ BGE_CHIPID_BCM5700_B0,	"BCM5700 B0" },
	{ BGE_CHIPID_BCM5700_B1,	"BCM5700 B1" },
	{ BGE_CHIPID_BCM5700_B2,	"BCM5700 B2" },
	{ BGE_CHIPID_BCM5700_B3,	"BCM5700 B3" },
	{ BGE_CHIPID_BCM5700_ALTIMA,	"BCM5700 Altima" },
	{ BGE_CHIPID_BCM5700_C0,	"BCM5700 C0" },
	{ BGE_CHIPID_BCM5701_A0,	"BCM5701 A0" },
	{ BGE_CHIPID_BCM5701_B0,	"BCM5701 B0" },
	{ BGE_CHIPID_BCM5701_B2,	"BCM5701 B2" },
	{ BGE_CHIPID_BCM5701_B5,	"BCM5701 B5" },
	{ BGE_CHIPID_BCM5703_A0,	"BCM5703 A0" },
	{ BGE_CHIPID_BCM5703_A1,	"BCM5703 A1" },
	{ BGE_CHIPID_BCM5703_A2,	"BCM5703 A2" },
	{ BGE_CHIPID_BCM5703_A3,	"BCM5703 A3" },
	{ BGE_CHIPID_BCM5703_B0,	"BCM5703 B0" },
	{ BGE_CHIPID_BCM5704_A0,	"BCM5704 A0" },
	{ BGE_CHIPID_BCM5704_A1,	"BCM5704 A1" },
	{ BGE_CHIPID_BCM5704_A2,	"BCM5704 A2" },
	{ BGE_CHIPID_BCM5704_A3,	"BCM5704 A3" },
	{ BGE_CHIPID_BCM5704_B0,	"BCM5704 B0" },
	{ BGE_CHIPID_BCM5705_A0,	"BCM5705 A0" },
	{ BGE_CHIPID_BCM5705_A1,	"BCM5705 A1" },
	{ BGE_CHIPID_BCM5705_A2,	"BCM5705 A2" },
	{ BGE_CHIPID_BCM5705_A3,	"BCM5705 A3" },
	{ BGE_CHIPID_BCM5750_A0,	"BCM5750 A0" },
	{ BGE_CHIPID_BCM5750_A1,	"BCM5750 A1" },
	{ BGE_CHIPID_BCM5750_A3,	"BCM5750 A3" },
	{ BGE_CHIPID_BCM5750_B0,	"BCM5750 B0" },
	{ BGE_CHIPID_BCM5750_B1,	"BCM5750 B1" },
	{ BGE_CHIPID_BCM5750_C0,	"BCM5750 C0" },
	{ BGE_CHIPID_BCM5750_C1,	"BCM5750 C1" },
	{ BGE_CHIPID_BCM5750_C2,	"BCM5750 C2" },
	{ BGE_CHIPID_BCM5714_A0,	"BCM5714 A0" },
	{ BGE_CHIPID_BCM5752_A0,	"BCM5752 A0" },
	{ BGE_CHIPID_BCM5752_A1,	"BCM5752 A1" },
	{ BGE_CHIPID_BCM5752_A2,	"BCM5752 A2" },
	{ BGE_CHIPID_BCM5714_B0,	"BCM5714 B0" },
	{ BGE_CHIPID_BCM5714_B3,	"BCM5714 B3" },
	{ BGE_CHIPID_BCM5715_A0,	"BCM5715 A0" },
	{ BGE_CHIPID_BCM5715_A1,	"BCM5715 A1" },
	{ BGE_CHIPID_BCM5715_A3,	"BCM5715 A3" },
	{ BGE_CHIPID_BCM5755_A0,	"BCM5755 A0" },
	{ BGE_CHIPID_BCM5755_A1,	"BCM5755 A1" },
	{ BGE_CHIPID_BCM5755_A2,	"BCM5755 A2" },
	{ BGE_CHIPID_BCM5722_A0,	"BCM5722 A0" },
	{ BGE_CHIPID_BCM5761_A0,	"BCM5761 A0" },
	{ BGE_CHIPID_BCM5761_A1,	"BCM5761 A1" },
	{ BGE_CHIPID_BCM5784_A0,	"BCM5784 A0" },
	{ BGE_CHIPID_BCM5784_A1,	"BCM5784 A1" },
	/* 5754 and 5787 share the same ASIC ID */
	{ BGE_CHIPID_BCM5787_A0,	"BCM5754/5787 A0" },
	{ BGE_CHIPID_BCM5787_A1,	"BCM5754/5787 A1" },
	{ BGE_CHIPID_BCM5787_A2,	"BCM5754/5787 A2" },
	{ BGE_CHIPID_BCM5906_A1,	"BCM5906 A1" },
	{ BGE_CHIPID_BCM5906_A2,	"BCM5906 A2" },
	{ BGE_CHIPID_BCM57780_A0,	"BCM57780 A0" },
	{ BGE_CHIPID_BCM57780_A1,	"BCM57780 A1" },

	{ 0, NULL }
};

/*
 * Some defaults for major revisions, so that newer steppings
 * that we don't know about have a shot at working.
 */
static const struct bge_revision bge_majorrevs[] = {
	{ BGE_ASICREV_BCM5700,		"unknown BCM5700" },
	{ BGE_ASICREV_BCM5701,		"unknown BCM5701" },
	{ BGE_ASICREV_BCM5703,		"unknown BCM5703" },
	{ BGE_ASICREV_BCM5704,		"unknown BCM5704" },
	{ BGE_ASICREV_BCM5705,		"unknown BCM5705" },
	{ BGE_ASICREV_BCM5750,		"unknown BCM5750" },
	{ BGE_ASICREV_BCM5714_A0,	"unknown BCM5714" },
	{ BGE_ASICREV_BCM5752,		"unknown BCM5752" },
	{ BGE_ASICREV_BCM5780,		"unknown BCM5780" },
	{ BGE_ASICREV_BCM5714,		"unknown BCM5714" },
	{ BGE_ASICREV_BCM5755,		"unknown BCM5755" },
	{ BGE_ASICREV_BCM5761,		"unknown BCM5761" },
	{ BGE_ASICREV_BCM5784,		"unknown BCM5784" },
	{ BGE_ASICREV_BCM5785,		"unknown BCM5785" },
	/* 5754 and 5787 share the same ASIC ID */
	{ BGE_ASICREV_BCM5787,		"unknown BCM5754/5787" },
	{ BGE_ASICREV_BCM5906,		"unknown BCM5906" },
	{ BGE_ASICREV_BCM57780,		"unknown BCM57780" },

	{ 0, NULL }
};

#define	BGE_IS_JUMBO_CAPABLE(sc)	((sc)->bge_flags & BGE_FLAG_JUMBO)
#define	BGE_IS_5700_FAMILY(sc)		((sc)->bge_flags & BGE_FLAG_5700_FAMILY)
#define	BGE_IS_5705_PLUS(sc)		((sc)->bge_flags & BGE_FLAG_5705_PLUS)
#define	BGE_IS_5714_FAMILY(sc)		((sc)->bge_flags & BGE_FLAG_5714_FAMILY)
#define	BGE_IS_575X_PLUS(sc)		((sc)->bge_flags & BGE_FLAG_575X_PLUS)
#define	BGE_IS_5755_PLUS(sc)		((sc)->bge_flags & BGE_FLAG_5755_PLUS)

const struct bge_revision * bge_lookup_rev(uint32_t);
const struct bge_vendor * bge_lookup_vendor(uint16_t);

typedef int	(*bge_eaddr_fcn_t)(struct bge_softc *, uint8_t[]);

static int bge_probe(device_t);
static int bge_attach(device_t);
static int bge_detach(device_t);
static int bge_suspend(device_t);
static int bge_resume(device_t);
static void bge_release_resources(struct bge_softc *);
static void bge_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int bge_dma_alloc(device_t);
static void bge_dma_free(struct bge_softc *);

static int bge_get_eaddr_fw(struct bge_softc *sc, uint8_t ether_addr[]);
static int bge_get_eaddr_mem(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr_nvram(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr_eeprom(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr(struct bge_softc *, uint8_t[]);

static void bge_txeof(struct bge_softc *, uint16_t);
static int bge_rxeof(struct bge_softc *, uint16_t, int);

static void bge_asf_driver_up (struct bge_softc *);
static void bge_tick(void *);
static void bge_stats_update(struct bge_softc *);
static void bge_stats_update_regs(struct bge_softc *);
static int bge_encap(struct bge_softc *, struct mbuf **, uint32_t *);

static void bge_intr(void *);
static int bge_msi_intr(void *);
static void bge_intr_task(void *, int);
static void bge_start_locked(struct ifnet *);
static void bge_start(struct ifnet *);
static int bge_ioctl(struct ifnet *, u_long, caddr_t);
static void bge_init_locked(struct bge_softc *);
static void bge_init(void *);
static void bge_stop(struct bge_softc *);
static void bge_watchdog(struct bge_softc *);
static int bge_shutdown(device_t);
static int bge_ifmedia_upd_locked(struct ifnet *);
static int bge_ifmedia_upd(struct ifnet *);
static void bge_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static uint8_t bge_nvram_getbyte(struct bge_softc *, int, uint8_t *);
static int bge_read_nvram(struct bge_softc *, caddr_t, int, int);

static uint8_t bge_eeprom_getbyte(struct bge_softc *, int, uint8_t *);
static int bge_read_eeprom(struct bge_softc *, caddr_t, int, int);

static void bge_setpromisc(struct bge_softc *);
static void bge_setmulti(struct bge_softc *);
static void bge_setvlan(struct bge_softc *);

static int bge_newbuf_std(struct bge_softc *, int);
static int bge_newbuf_jumbo(struct bge_softc *, int);
static int bge_init_rx_ring_std(struct bge_softc *);
static void bge_free_rx_ring_std(struct bge_softc *);
static int bge_init_rx_ring_jumbo(struct bge_softc *);
static void bge_free_rx_ring_jumbo(struct bge_softc *);
static void bge_free_tx_ring(struct bge_softc *);
static int bge_init_tx_ring(struct bge_softc *);

static int bge_chipinit(struct bge_softc *);
static int bge_blockinit(struct bge_softc *);

static int bge_has_eaddr(struct bge_softc *);
static uint32_t bge_readmem_ind(struct bge_softc *, int);
static void bge_writemem_ind(struct bge_softc *, int, int);
static void bge_writembx(struct bge_softc *, int, int);
#ifdef notdef
static uint32_t bge_readreg_ind(struct bge_softc *, int);
#endif
static void bge_writemem_direct(struct bge_softc *, int, int);
static void bge_writereg_ind(struct bge_softc *, int, int);
static void bge_set_max_readrq(struct bge_softc *);

static int bge_miibus_readreg(device_t, int, int);
static int bge_miibus_writereg(device_t, int, int, int);
static void bge_miibus_statchg(device_t);
#ifdef DEVICE_POLLING
static int bge_poll(struct ifnet *ifp, enum poll_cmd cmd, int count);
#endif

#define	BGE_RESET_START 1
#define	BGE_RESET_STOP  2
static void bge_sig_post_reset(struct bge_softc *, int);
static void bge_sig_legacy(struct bge_softc *, int);
static void bge_sig_pre_reset(struct bge_softc *, int);
static int bge_reset(struct bge_softc *);
static void bge_link_upd(struct bge_softc *);

/*
 * The BGE_REGISTER_DEBUG option is only for low-level debugging.  It may
 * leak information to untrusted users.  It is also known to cause alignment
 * traps on certain architectures.
 */
#ifdef BGE_REGISTER_DEBUG
static int bge_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static int bge_sysctl_reg_read(SYSCTL_HANDLER_ARGS);
static int bge_sysctl_mem_read(SYSCTL_HANDLER_ARGS);
#endif
static void bge_add_sysctls(struct bge_softc *);
static int bge_sysctl_stats(SYSCTL_HANDLER_ARGS);

static device_method_t bge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bge_probe),
	DEVMETHOD(device_attach,	bge_attach),
	DEVMETHOD(device_detach,	bge_detach),
	DEVMETHOD(device_shutdown,	bge_shutdown),
	DEVMETHOD(device_suspend,	bge_suspend),
	DEVMETHOD(device_resume,	bge_resume),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	bge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	bge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	bge_miibus_statchg),

	{ 0, 0 }
};

static driver_t bge_driver = {
	"bge",
	bge_methods,
	sizeof(struct bge_softc)
};

static devclass_t bge_devclass;

DRIVER_MODULE(bge, pci, bge_driver, bge_devclass, 0, 0);
DRIVER_MODULE(miibus, bge, miibus_driver, miibus_devclass, 0, 0);

static int bge_allow_asf = 0;

TUNABLE_INT("hw.bge.allow_asf", &bge_allow_asf);

SYSCTL_NODE(_hw, OID_AUTO, bge, CTLFLAG_RD, 0, "BGE driver parameters");
SYSCTL_INT(_hw_bge, OID_AUTO, allow_asf, CTLFLAG_RD, &bge_allow_asf, 0,
	"Allow ASF mode if available");

#define	SPARC64_BLADE_1500_MODEL	"SUNW,Sun-Blade-1500"
#define	SPARC64_BLADE_1500_PATH_BGE	"/pci@1f,700000/network@2"
#define	SPARC64_BLADE_2500_MODEL	"SUNW,Sun-Blade-2500"
#define	SPARC64_BLADE_2500_PATH_BGE	"/pci@1c,600000/network@3"
#define	SPARC64_OFW_SUBVENDOR		"subsystem-vendor-id"

static int
bge_has_eaddr(struct bge_softc *sc)
{
#ifdef __sparc64__
	char buf[sizeof(SPARC64_BLADE_1500_PATH_BGE)];
	device_t dev;
	uint32_t subvendor;

	dev = sc->bge_dev;

	/*
	 * The on-board BGEs found in sun4u machines aren't fitted with
	 * an EEPROM which means that we have to obtain the MAC address
	 * via OFW and that some tests will always fail.  We distinguish
	 * such BGEs by the subvendor ID, which also has to be obtained
	 * from OFW instead of the PCI configuration space as the latter
	 * indicates Broadcom as the subvendor of the netboot interface.
	 * For early Blade 1500 and 2500 we even have to check the OFW
	 * device path as the subvendor ID always defaults to Broadcom
	 * there.
	 */
	if (OF_getprop(ofw_bus_get_node(dev), SPARC64_OFW_SUBVENDOR,
	    &subvendor, sizeof(subvendor)) == sizeof(subvendor) &&
	    subvendor == SUN_VENDORID)
		return (0);
	memset(buf, 0, sizeof(buf));
	if (OF_package_to_path(ofw_bus_get_node(dev), buf, sizeof(buf)) > 0) {
		if (strcmp(sparc64_model, SPARC64_BLADE_1500_MODEL) == 0 &&
		    strcmp(buf, SPARC64_BLADE_1500_PATH_BGE) == 0)
			return (0);
		if (strcmp(sparc64_model, SPARC64_BLADE_2500_MODEL) == 0 &&
		    strcmp(buf, SPARC64_BLADE_2500_PATH_BGE) == 0)
			return (0);
	}
#endif
	return (1);
}

static uint32_t
bge_readmem_ind(struct bge_softc *sc, int off)
{
	device_t dev;
	uint32_t val;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, off, 4);
	val = pci_read_config(dev, BGE_PCI_MEMWIN_DATA, 4);
	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, 0, 4);
	return (val);
}

static void
bge_writemem_ind(struct bge_softc *sc, int off, int val)
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, off, 4);
	pci_write_config(dev, BGE_PCI_MEMWIN_DATA, val, 4);
	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, 0, 4);
}

/*
 * PCI Express only
 */
static void
bge_set_max_readrq(struct bge_softc *sc)
{
	device_t dev;
	uint16_t val;

	dev = sc->bge_dev;

	val = pci_read_config(dev, sc->bge_expcap + PCIR_EXPRESS_DEVICE_CTL, 2);
	if ((val & PCIM_EXP_CTL_MAX_READ_REQUEST) !=
	    BGE_PCIE_DEVCTL_MAX_READRQ_4096) {
		if (bootverbose)
			device_printf(dev, "adjust device control 0x%04x ",
			    val);
		val &= ~PCIM_EXP_CTL_MAX_READ_REQUEST;
		val |= BGE_PCIE_DEVCTL_MAX_READRQ_4096;
		pci_write_config(dev, sc->bge_expcap + PCIR_EXPRESS_DEVICE_CTL,
		    val, 2);
		if (bootverbose)
			printf("-> 0x%04x\n", val);
	}
}

#ifdef notdef
static uint32_t
bge_readreg_ind(struct bge_softc *sc, int off)
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_REG_BASEADDR, off, 4);
	return (pci_read_config(dev, BGE_PCI_REG_DATA, 4));
}
#endif

static void
bge_writereg_ind(struct bge_softc *sc, int off, int val)
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_REG_BASEADDR, off, 4);
	pci_write_config(dev, BGE_PCI_REG_DATA, val, 4);
}

static void
bge_writemem_direct(struct bge_softc *sc, int off, int val)
{
	CSR_WRITE_4(sc, off, val);
}

static void
bge_writembx(struct bge_softc *sc, int off, int val)
{
	if (sc->bge_asicrev == BGE_ASICREV_BCM5906)
		off += BGE_LPMBX_IRQ0_HI - BGE_MBX_IRQ0_HI;

	CSR_WRITE_4(sc, off, val);
}

/*
 * Map a single buffer address.
 */

static void
bge_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bge_dmamap_arg *ctx;

	if (error)
		return;

	ctx = arg;

	if (nseg > ctx->bge_maxsegs) {
		ctx->bge_maxsegs = 0;
		return;
	}

	ctx->bge_busaddr = segs->ds_addr;
}

static uint8_t
bge_nvram_getbyte(struct bge_softc *sc, int addr, uint8_t *dest)
{
	uint32_t access, byte = 0;
	int i;

	/* Lock. */
	CSR_WRITE_4(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_SET1);
	for (i = 0; i < 8000; i++) {
		if (CSR_READ_4(sc, BGE_NVRAM_SWARB) & BGE_NVRAMSWARB_GNT1)
			break;
		DELAY(20);
	}
	if (i == 8000)
		return (1);

	/* Enable access. */
	access = CSR_READ_4(sc, BGE_NVRAM_ACCESS);
	CSR_WRITE_4(sc, BGE_NVRAM_ACCESS, access | BGE_NVRAMACC_ENABLE);

	CSR_WRITE_4(sc, BGE_NVRAM_ADDR, addr & 0xfffffffc);
	CSR_WRITE_4(sc, BGE_NVRAM_CMD, BGE_NVRAM_READCMD);
	for (i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_NVRAM_CMD) & BGE_NVRAMCMD_DONE) {
			DELAY(10);
			break;
		}
	}

	if (i == BGE_TIMEOUT * 10) {
		if_printf(sc->bge_ifp, "nvram read timed out\n");
		return (1);
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_NVRAM_RDDATA);

	*dest = (bswap32(byte) >> ((addr % 4) * 8)) & 0xFF;

	/* Disable access. */
	CSR_WRITE_4(sc, BGE_NVRAM_ACCESS, access);

	/* Unlock. */
	CSR_WRITE_4(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_CLR1);
	CSR_READ_4(sc, BGE_NVRAM_SWARB);

	return (0);
}

/*
 * Read a sequence of bytes from NVRAM.
 */
static int
bge_read_nvram(struct bge_softc *sc, caddr_t dest, int off, int cnt)
{
	int err = 0, i;
	uint8_t byte = 0;

	if (sc->bge_asicrev != BGE_ASICREV_BCM5906)
		return (1);

	for (i = 0; i < cnt; i++) {
		err = bge_nvram_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return (err ? 1 : 0);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.' The
 * BCM570x supports both the traditional bitbang interface and an
 * auto access interface for reading the EEPROM. We use the auto
 * access method.
 */
static uint8_t
bge_eeprom_getbyte(struct bge_softc *sc, int addr, uint8_t *dest)
{
	int i;
	uint32_t byte = 0;

	/*
	 * Enable use of auto EEPROM access so we can avoid
	 * having to use the bitbang method.
	 */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);

	/* Reset the EEPROM, load the clock period. */
	CSR_WRITE_4(sc, BGE_EE_ADDR,
	    BGE_EEADDR_RESET | BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	DELAY(20);

	/* Issue the read EEPROM command. */
	CSR_WRITE_4(sc, BGE_EE_ADDR, BGE_EE_READCMD | addr);

	/* Wait for completion */
	for(i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_EE_ADDR) & BGE_EEADDR_DONE)
			break;
	}

	if (i == BGE_TIMEOUT * 10) {
		device_printf(sc->bge_dev, "EEPROM read timed out\n");
		return (1);
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_EE_DATA);

	*dest = (byte >> ((addr % 4) * 8)) & 0xFF;

	return (0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int
bge_read_eeprom(struct bge_softc *sc, caddr_t dest, int off, int cnt)
{
	int i, error = 0;
	uint8_t byte = 0;

	for (i = 0; i < cnt; i++) {
		error = bge_eeprom_getbyte(sc, off + i, &byte);
		if (error)
			break;
		*(dest + i) = byte;
	}

	return (error ? 1 : 0);
}

static int
bge_miibus_readreg(device_t dev, int phy, int reg)
{
	struct bge_softc *sc;
	uint32_t val, autopoll;
	int i;

	sc = device_get_softc(dev);

	/*
	 * Broadcom's own driver always assumes the internal
	 * PHY is at GMII address 1. On some chips, the PHY responds
	 * to accesses at all addresses, which could cause us to
	 * bogusly attach the PHY 32 times at probe type. Always
	 * restricting the lookup to address 1 is simpler than
	 * trying to figure out which chips revisions should be
	 * special-cased.
	 */
	if (phy != 1)
		return (0);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_READ | BGE_MICOMM_BUSY |
	    BGE_MIPHY(phy) | BGE_MIREG(reg));

	for (i = 0; i < BGE_TIMEOUT; i++) {
		DELAY(10);
		val = CSR_READ_4(sc, BGE_MI_COMM);
		if (!(val & BGE_MICOMM_BUSY))
			break;
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev,
		    "PHY read timed out (phy %d, reg %d, val 0x%08x)\n",
		    phy, reg, val);
		val = 0;
		goto done;
	}

	DELAY(5);
	val = CSR_READ_4(sc, BGE_MI_COMM);

done:
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (val & BGE_MICOMM_READFAIL)
		return (0);

	return (val & 0xFFFF);
}

static int
bge_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct bge_softc *sc;
	uint32_t autopoll;
	int i;

	sc = device_get_softc(dev);

	if (sc->bge_asicrev == BGE_ASICREV_BCM5906 &&
	    (reg == BRGPHY_MII_1000CTL || reg == BRGPHY_MII_AUXCTL))
		return(0);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_WRITE | BGE_MICOMM_BUSY |
	    BGE_MIPHY(phy) | BGE_MIREG(reg) | val);

	for (i = 0; i < BGE_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, BGE_MI_COMM) & BGE_MICOMM_BUSY)) {
			DELAY(5);
			CSR_READ_4(sc, BGE_MI_COMM); /* dummy read */
			break;
		}
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev,
		    "PHY write timed out (phy %d, reg %d, val %d)\n",
		    phy, reg, val);
		return (0);
	}

	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	return (0);
}

static void
bge_miibus_statchg(device_t dev)
{
	struct bge_softc *sc;
	struct mii_data *mii;
	sc = device_get_softc(dev);
	mii = device_get_softc(sc->bge_miibus);

	BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_PORTMODE);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T)
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_GMII);
	else
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_MII);

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
	else
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
}

/*
 * Intialize a standard receive ring descriptor.
 */
static int
bge_newbuf_std(struct bge_softc *sc, int i)
{
	struct mbuf *m;
	struct bge_rx_bd *r;
	bus_dma_segment_t segs[1];
	bus_dmamap_t map;
	int error, nsegs;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	if ((sc->bge_flags & BGE_FLAG_RX_ALIGNBUG) == 0)
		m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_rx_mtag,
	    sc->bge_cdata.bge_rx_std_sparemap, m, segs, &nsegs, 0);
	if (error != 0) {
		m_freem(m);
		return (error);
	}
	if (sc->bge_cdata.bge_rx_std_chain[i] != NULL) {
		bus_dmamap_sync(sc->bge_cdata.bge_rx_mtag,
		    sc->bge_cdata.bge_rx_std_dmamap[i], BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->bge_cdata.bge_rx_mtag,
		    sc->bge_cdata.bge_rx_std_dmamap[i]);
	}
	map = sc->bge_cdata.bge_rx_std_dmamap[i];
	sc->bge_cdata.bge_rx_std_dmamap[i] = sc->bge_cdata.bge_rx_std_sparemap;
	sc->bge_cdata.bge_rx_std_sparemap = map;
	sc->bge_cdata.bge_rx_std_chain[i] = m;
	r = &sc->bge_ldata.bge_rx_std_ring[sc->bge_std];
	r->bge_addr.bge_addr_lo = BGE_ADDR_LO(segs[0].ds_addr);
	r->bge_addr.bge_addr_hi = BGE_ADDR_HI(segs[0].ds_addr);
	r->bge_flags = BGE_RXBDFLAG_END;
	r->bge_len = segs[0].ds_len;
	r->bge_idx = i;

	bus_dmamap_sync(sc->bge_cdata.bge_rx_mtag,
	    sc->bge_cdata.bge_rx_std_dmamap[i], BUS_DMASYNC_PREREAD);

	return (0);
}

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int
bge_newbuf_jumbo(struct bge_softc *sc, int i)
{
	bus_dma_segment_t segs[BGE_NSEG_JUMBO];
	bus_dmamap_t map;
	struct bge_extrx_bd *r;
	struct mbuf *m;
	int error, nsegs;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	m_cljget(m, M_DONTWAIT, MJUM9BYTES);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		return (ENOBUFS);
	}
	m->m_len = m->m_pkthdr.len = MJUM9BYTES;
	if ((sc->bge_flags & BGE_FLAG_RX_ALIGNBUG) == 0)
		m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_mtag_jumbo,
	    sc->bge_cdata.bge_rx_jumbo_sparemap, m, segs, &nsegs, 0);
	if (error != 0) {
		m_freem(m);
		return (error);
	}

	if (sc->bge_cdata.bge_rx_jumbo_chain[i] == NULL) {
		bus_dmamap_sync(sc->bge_cdata.bge_mtag_jumbo,
		    sc->bge_cdata.bge_rx_jumbo_dmamap[i], BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->bge_cdata.bge_mtag_jumbo,
		    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
	}
	map = sc->bge_cdata.bge_rx_jumbo_dmamap[i];
	sc->bge_cdata.bge_rx_jumbo_dmamap[i] =
	    sc->bge_cdata.bge_rx_jumbo_sparemap;
	sc->bge_cdata.bge_rx_jumbo_sparemap = map;
	sc->bge_cdata.bge_rx_jumbo_chain[i] = m;
	/*
	 * Fill in the extended RX buffer descriptor.
	 */
	r = &sc->bge_ldata.bge_rx_jumbo_ring[sc->bge_jumbo];
	r->bge_flags = BGE_RXBDFLAG_JUMBO_RING | BGE_RXBDFLAG_END;
	r->bge_idx = i;
	r->bge_len3 = r->bge_len2 = r->bge_len1 = 0;
	switch (nsegs) {
	case 4:
		r->bge_addr3.bge_addr_lo = BGE_ADDR_LO(segs[3].ds_addr);
		r->bge_addr3.bge_addr_hi = BGE_ADDR_HI(segs[3].ds_addr);
		r->bge_len3 = segs[3].ds_len;
	case 3:
		r->bge_addr2.bge_addr_lo = BGE_ADDR_LO(segs[2].ds_addr);
		r->bge_addr2.bge_addr_hi = BGE_ADDR_HI(segs[2].ds_addr);
		r->bge_len2 = segs[2].ds_len;
	case 2:
		r->bge_addr1.bge_addr_lo = BGE_ADDR_LO(segs[1].ds_addr);
		r->bge_addr1.bge_addr_hi = BGE_ADDR_HI(segs[1].ds_addr);
		r->bge_len1 = segs[1].ds_len;
	case 1:
		r->bge_addr0.bge_addr_lo = BGE_ADDR_LO(segs[0].ds_addr);
		r->bge_addr0.bge_addr_hi = BGE_ADDR_HI(segs[0].ds_addr);
		r->bge_len0 = segs[0].ds_len;
		break;
	default:
		panic("%s: %d segments\n", __func__, nsegs);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_mtag_jumbo,
	    sc->bge_cdata.bge_rx_jumbo_dmamap[i], BUS_DMASYNC_PREREAD);

	return (0);
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
static int
bge_init_rx_ring_std(struct bge_softc *sc)
{
	int error, i;

	bzero(sc->bge_ldata.bge_rx_std_ring, BGE_STD_RX_RING_SZ);
	sc->bge_std = 0;
	for (i = 0; i < BGE_SSLOTS; i++) {
		if ((error = bge_newbuf_std(sc, i)) != 0)
			return (error);
		BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
	};

	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_PREWRITE);

	sc->bge_std = i - 1;
	bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);

	return (0);
}

static void
bge_free_rx_ring_std(struct bge_softc *sc)
{
	int i;

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_chain[i] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_rx_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_rx_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i]);
			m_freem(sc->bge_cdata.bge_rx_std_chain[i]);
			sc->bge_cdata.bge_rx_std_chain[i] = NULL;
		}
		bzero((char *)&sc->bge_ldata.bge_rx_std_ring[i],
		    sizeof(struct bge_rx_bd));
	}
}

static int
bge_init_rx_ring_jumbo(struct bge_softc *sc)
{
	struct bge_rcb *rcb;
	int error, i;

	bzero(sc->bge_ldata.bge_rx_jumbo_ring, BGE_JUMBO_RX_RING_SZ);
	sc->bge_jumbo = 0;
	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if ((error = bge_newbuf_jumbo(sc, i)) != 0)
			return (error);
		BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
	};

	bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
	    sc->bge_cdata.bge_rx_jumbo_ring_map, BUS_DMASYNC_PREWRITE);

	sc->bge_jumbo = i - 1;

	rcb = &sc->bge_ldata.bge_info.bge_jumbo_rx_rcb;
	rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0,
				    BGE_RCB_FLAG_USE_EXT_RX_BD);
	CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);

	bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);

	return (0);
}

static void
bge_free_rx_ring_jumbo(struct bge_softc *sc)
{
	int i;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_chain[i] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
			m_freem(sc->bge_cdata.bge_rx_jumbo_chain[i]);
			sc->bge_cdata.bge_rx_jumbo_chain[i] = NULL;
		}
		bzero((char *)&sc->bge_ldata.bge_rx_jumbo_ring[i],
		    sizeof(struct bge_extrx_bd));
	}
}

static void
bge_free_tx_ring(struct bge_softc *sc)
{
	int i;

	if (sc->bge_ldata.bge_tx_ring == NULL)
		return;

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_chain[i] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_tx_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bge_cdata.bge_tx_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i]);
			m_freem(sc->bge_cdata.bge_tx_chain[i]);
			sc->bge_cdata.bge_tx_chain[i] = NULL;
		}
		bzero((char *)&sc->bge_ldata.bge_tx_ring[i],
		    sizeof(struct bge_tx_bd));
	}
}

static int
bge_init_tx_ring(struct bge_softc *sc)
{
	sc->bge_txcnt = 0;
	sc->bge_tx_saved_considx = 0;

	bzero(sc->bge_ldata.bge_tx_ring, BGE_TX_RING_SZ);
	bus_dmamap_sync(sc->bge_cdata.bge_tx_ring_tag,
	    sc->bge_cdata.bge_tx_ring_map, BUS_DMASYNC_PREWRITE);

	/* Initialize transmit producer index for host-memory send ring. */
	sc->bge_tx_prodidx = 0;
	bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);

	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);

	/* NIC-memory send ring not used; initialize to zero. */
	bge_writembx(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);
	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);

	return (0);
}

static void
bge_setpromisc(struct bge_softc *sc)
{
	struct ifnet *ifp;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC)
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	else
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
}

static void
bge_setmulti(struct bge_softc *sc)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	uint32_t hashes[4] = { 0, 0, 0, 0 };
	int h, i;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		for (i = 0; i < 4; i++)
			CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), 0xFFFFFFFF);
		return;
	}

	/* First, zot all the existing filters. */
	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), 0);

	/* Now program new ones. */
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) & 0x7F;
		hashes[(h & 0x60) >> 5] |= 1 << (h & 0x1F);
	}
	if_maddr_runlock(ifp);

	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), hashes[i]);
}

static void
bge_setvlan(struct bge_softc *sc)
{
	struct ifnet *ifp;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	/* Enable or disable VLAN tag stripping as needed. */
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_KEEP_VLAN_DIAG);
	else
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_KEEP_VLAN_DIAG);
}

static void
bge_sig_pre_reset(sc, type)
	struct bge_softc *sc;
	int type;
{
	/*
	 * Some chips don't like this so only do this if ASF is enabled
	 */
	if (sc->bge_asf_mode)
		bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM, BGE_MAGIC_NUMBER);

	if (sc->bge_asf_mode & ASF_NEW_HANDSHAKE) {
		switch (type) {
		case BGE_RESET_START:
			bge_writemem_ind(sc, BGE_SDI_STATUS, 0x1); /* START */
			break;
		case BGE_RESET_STOP:
			bge_writemem_ind(sc, BGE_SDI_STATUS, 0x2); /* UNLOAD */
			break;
		}
	}
}

static void
bge_sig_post_reset(sc, type)
	struct bge_softc *sc;
	int type;
{
	if (sc->bge_asf_mode & ASF_NEW_HANDSHAKE) {
		switch (type) {
		case BGE_RESET_START:
			bge_writemem_ind(sc, BGE_SDI_STATUS, 0x80000001);
			/* START DONE */
			break;
		case BGE_RESET_STOP:
			bge_writemem_ind(sc, BGE_SDI_STATUS, 0x80000002);
			break;
		}
	}
}

static void
bge_sig_legacy(sc, type)
	struct bge_softc *sc;
	int type;
{
	if (sc->bge_asf_mode) {
		switch (type) {
		case BGE_RESET_START:
			bge_writemem_ind(sc, BGE_SDI_STATUS, 0x1); /* START */
			break;
		case BGE_RESET_STOP:
			bge_writemem_ind(sc, BGE_SDI_STATUS, 0x2); /* UNLOAD */
			break;
		}
	}
}

void bge_stop_fw(struct bge_softc *);
void
bge_stop_fw(sc)
	struct bge_softc *sc;
{
	int i;

	if (sc->bge_asf_mode) {
		bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM_FW, BGE_FW_PAUSE);
		CSR_WRITE_4(sc, BGE_CPU_EVENT,
		    CSR_READ_4(sc, BGE_CPU_EVENT) | (1 << 14));

		for (i = 0; i < 100; i++ ) {
			if (!(CSR_READ_4(sc, BGE_CPU_EVENT) & (1 << 14)))
				break;
			DELAY(10);
		}
	}
}

/*
 * Do endian, PCI and DMA initialization.
 */
static int
bge_chipinit(struct bge_softc *sc)
{
	uint32_t dma_rw_ctl;
	int i;

	/* Set endianness before we access any non-PCI registers. */
	pci_write_config(sc->bge_dev, BGE_PCI_MISC_CTL, BGE_INIT, 4);

	/* Clear the MAC control register */
	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * Clear the MAC statistics block in the NIC's
	 * internal memory.
	 */
	for (i = BGE_STATS_BLOCK;
	    i < BGE_STATS_BLOCK_END + 1; i += sizeof(uint32_t))
		BGE_MEMWIN_WRITE(sc, i, 0);

	for (i = BGE_STATUS_BLOCK;
	    i < BGE_STATUS_BLOCK_END + 1; i += sizeof(uint32_t))
		BGE_MEMWIN_WRITE(sc, i, 0);

	/*
	 * Set up the PCI DMA control register.
	 */
	dma_rw_ctl = BGE_PCIDMARWCTL_RD_CMD_SHIFT(6) |
	    BGE_PCIDMARWCTL_WR_CMD_SHIFT(7);
	if (sc->bge_flags & BGE_FLAG_PCIE) {
		/* Read watermark not used, 128 bytes for write. */
		dma_rw_ctl |= BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
	} else if (sc->bge_flags & BGE_FLAG_PCIX) {
		if (BGE_IS_5714_FAMILY(sc)) {
			/* 256 bytes for read and write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(2) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(2);
			dma_rw_ctl |= (sc->bge_asicrev == BGE_ASICREV_BCM5780) ?
			    BGE_PCIDMARWCTL_ONEDMA_ATONCE_GLOBAL :
			    BGE_PCIDMARWCTL_ONEDMA_ATONCE_LOCAL;
		} else if (sc->bge_asicrev == BGE_ASICREV_BCM5704) {
			/* 1536 bytes for read, 384 bytes for write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(7) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
		} else {
			/* 384 bytes for read and write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(3) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3) |
			    0x0F;
		}
		if (sc->bge_asicrev == BGE_ASICREV_BCM5703 ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5704) {
			uint32_t tmp;

			/* Set ONE_DMA_AT_ONCE for hardware workaround. */
			tmp = CSR_READ_4(sc, BGE_PCI_CLKCTL) & 0x1F;
			if (tmp == 6 || tmp == 7)
				dma_rw_ctl |=
				    BGE_PCIDMARWCTL_ONEDMA_ATONCE_GLOBAL;

			/* Set PCI-X DMA write workaround. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_ASRT_ALL_BE;
		}
	} else {
		/* Conventional PCI bus: 256 bytes for read and write. */
		dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(7) |
		    BGE_PCIDMARWCTL_WR_WAT_SHIFT(7);

		if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
		    sc->bge_asicrev != BGE_ASICREV_BCM5750)
			dma_rw_ctl |= 0x0F;
	}
	if (sc->bge_asicrev == BGE_ASICREV_BCM5700 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5701)
		dma_rw_ctl |= BGE_PCIDMARWCTL_USE_MRM |
		    BGE_PCIDMARWCTL_ASRT_ALL_BE;
	if (sc->bge_asicrev == BGE_ASICREV_BCM5703 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5704)
		dma_rw_ctl &= ~BGE_PCIDMARWCTL_MINDMA;
	pci_write_config(sc->bge_dev, BGE_PCI_DMA_RW_CTL, dma_rw_ctl, 4);

	/*
	 * Set up general mode register.
	 */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS |
	    BGE_MODECTL_MAC_ATTN_INTR | BGE_MODECTL_HOST_SEND_BDS |
	    BGE_MODECTL_TX_NO_PHDR_CSUM);

	/*
	 * BCM5701 B5 have a bug causing data corruption when using
	 * 64-bit DMA reads, which can be terminated early and then
	 * completed later as 32-bit accesses, in combination with
	 * certain bridges.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5701 &&
	    sc->bge_chipid == BGE_CHIPID_BCM5701_B5)
		BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_FORCE_PCI32);

	/*
	 * Tell the firmware the driver is running
	 */
	if (sc->bge_asf_mode & ASF_STACKUP)
		BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/*
	 * Disable memory write invalidate.  Apparently it is not supported
	 * properly by these devices.  Also ensure that INTx isn't disabled,
	 * as these chips need it even when using MSI.
	 */
	PCI_CLRBIT(sc->bge_dev, BGE_PCI_CMD,
	    PCIM_CMD_INTxDIS | PCIM_CMD_MWIEN, 4);

	/* Set the timer prescaler (always 66Mhz) */
	CSR_WRITE_4(sc, BGE_MISC_CFG, BGE_32BITTIME_66MHZ);

	/* XXX: The Linux tg3 driver does this at the start of brgphy_reset. */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5906) {
		DELAY(40);	/* XXX */

		/* Put PHY into ready state */
		BGE_CLRBIT(sc, BGE_MISC_CFG, BGE_MISCCFG_EPHY_IDDQ);
		CSR_READ_4(sc, BGE_MISC_CFG); /* Flush */
		DELAY(40);
	}

	return (0);
}

static int
bge_blockinit(struct bge_softc *sc)
{
	struct bge_rcb *rcb;
	bus_size_t vrcb;
	bge_hostaddr taddr;
	uint32_t val;
	int i;

	/*
	 * Initialize the memory window pointer register so that
	 * we can access the first 32K of internal NIC RAM. This will
	 * allow us to set up the TX send ring RCBs and the RX return
	 * ring RCBs, plus other things which live in NIC memory.
	 */
	CSR_WRITE_4(sc, BGE_PCI_MEMWIN_BASEADDR, 0);

	/* Note: the BCM5704 has a smaller mbuf space than other chips. */

	if (!(BGE_IS_5705_PLUS(sc))) {
		/* Configure mbuf memory pool */
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR, BGE_BUFFPOOL_1);
		if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
		else
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);

		/* Configure DMA resource pool */
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_BASEADDR,
		    BGE_DMA_DESCRIPTORS);
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LEN, 0x2000);
	}

	/* Configure mbuf pool watermarks */
	if (!BGE_IS_5705_PLUS(sc)) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x50);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x20);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);
	} else if (sc->bge_asicrev == BGE_ASICREV_BCM5906) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x04);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x10);
	} else {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x10);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);
	}

	/* Configure DMA resource watermarks */
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LOWAT, 5);
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_HIWAT, 10);

	/* Enable buffer manager */
	if (!(BGE_IS_5705_PLUS(sc))) {
		CSR_WRITE_4(sc, BGE_BMAN_MODE,
		    BGE_BMANMODE_ENABLE | BGE_BMANMODE_LOMBUF_ATTN);

		/* Poll for buffer manager start indication */
		for (i = 0; i < BGE_TIMEOUT; i++) {
			DELAY(10);
			if (CSR_READ_4(sc, BGE_BMAN_MODE) & BGE_BMANMODE_ENABLE)
				break;
		}

		if (i == BGE_TIMEOUT) {
			device_printf(sc->bge_dev,
			    "buffer manager failed to start\n");
			return (ENXIO);
		}
	}

	/* Enable flow-through queues */
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	/* Wait until queue initialization is complete */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_FTQ_RESET) == 0)
			break;
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev, "flow-through queue init failed\n");
		return (ENXIO);
	}

	/* Initialize the standard RX ring control block */
	rcb = &sc->bge_ldata.bge_info.bge_std_rx_rcb;
	rcb->bge_hostaddr.bge_addr_lo =
	    BGE_ADDR_LO(sc->bge_ldata.bge_rx_std_ring_paddr);
	rcb->bge_hostaddr.bge_addr_hi =
	    BGE_ADDR_HI(sc->bge_ldata.bge_rx_std_ring_paddr);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_PREREAD);
	if (BGE_IS_5705_PLUS(sc))
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(512, 0);
	else
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(BGE_MAX_FRAMELEN, 0);
	rcb->bge_nicaddr = BGE_STD_RX_RINGS;
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_HI, rcb->bge_hostaddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_LO, rcb->bge_hostaddr.bge_addr_lo);

	CSR_WRITE_4(sc, BGE_RX_STD_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_NICADDR, rcb->bge_nicaddr);

	/*
	 * Initialize the jumbo RX ring control block
	 * We set the 'ring disabled' bit in the flags
	 * field until we're actually ready to start
	 * using this ring (i.e. once we set the MTU
	 * high enough to require it).
	 */
	if (BGE_IS_JUMBO_CAPABLE(sc)) {
		rcb = &sc->bge_ldata.bge_info.bge_jumbo_rx_rcb;

		rcb->bge_hostaddr.bge_addr_lo =
		    BGE_ADDR_LO(sc->bge_ldata.bge_rx_jumbo_ring_paddr);
		rcb->bge_hostaddr.bge_addr_hi =
		    BGE_ADDR_HI(sc->bge_ldata.bge_rx_jumbo_ring_paddr);
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    BUS_DMASYNC_PREREAD);
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0,
		    BGE_RCB_FLAG_USE_EXT_RX_BD | BGE_RCB_FLAG_RING_DISABLED);
		rcb->bge_nicaddr = BGE_JUMBO_RX_RINGS;
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_HI,
		    rcb->bge_hostaddr.bge_addr_hi);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_LO,
		    rcb->bge_hostaddr.bge_addr_lo);

		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_NICADDR, rcb->bge_nicaddr);

		/* Set up dummy disabled mini ring RCB */
		rcb = &sc->bge_ldata.bge_info.bge_mini_rx_rcb;
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED);
		CSR_WRITE_4(sc, BGE_RX_MINI_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
	}

	/*
	 * Set the BD ring replentish thresholds. The recommended
	 * values are 1/8th the number of descriptors allocated to
	 * each ring.
	 * XXX The 5754 requires a lower threshold, so it might be a
	 * requirement of all 575x family chips.  The Linux driver sets
	 * the lower threshold for all 5705 family chips as well, but there
	 * are reports that it might not need to be so strict.
	 *
	 * XXX Linux does some extra fiddling here for the 5906 parts as
	 * well.
	 */
	if (BGE_IS_5705_PLUS(sc))
		val = 8;
	else
		val = BGE_STD_RX_RING_CNT / 8;
	CSR_WRITE_4(sc, BGE_RBDI_STD_REPL_THRESH, val);
	CSR_WRITE_4(sc, BGE_RBDI_JUMBO_REPL_THRESH, BGE_JUMBO_RX_RING_CNT/8);

	/*
	 * Disable all unused send rings by setting the 'ring disabled'
	 * bit in the flags field of all the TX send ring control blocks.
	 * These are located in NIC memory.
	 */
	vrcb = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	for (i = 0; i < BGE_TX_RINGS_EXTSSRAM_MAX; i++) {
		RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, vrcb, bge_nicaddr, 0);
		vrcb += sizeof(struct bge_rcb);
	}

	/* Configure TX RCB 0 (we use only the first ring) */
	vrcb = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	BGE_HOSTADDR(taddr, sc->bge_ldata.bge_tx_ring_paddr);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, vrcb, bge_nicaddr,
	    BGE_NIC_TXRING_ADDR(0, BGE_TX_RING_CNT));
	if (!(BGE_IS_5705_PLUS(sc)))
		RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(BGE_TX_RING_CNT, 0));

	/* Disable all unused RX return rings */
	vrcb = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	for (i = 0; i < BGE_RX_RINGS_MAX; i++) {
		RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_hi, 0);
		RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_lo, 0);
		RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt,
		    BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, vrcb, bge_nicaddr, 0);
		bge_writembx(sc, BGE_MBX_RX_CONS0_LO +
		    (i * (sizeof(uint64_t))), 0);
		vrcb += sizeof(struct bge_rcb);
	}

	/* Initialize RX ring indexes */
	bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, 0);
	bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, 0);
	bge_writembx(sc, BGE_MBX_RX_MINI_PROD_LO, 0);

	/*
	 * Set up RX return ring 0
	 * Note that the NIC address for RX return rings is 0x00000000.
	 * The return rings live entirely within the host, so the
	 * nicaddr field in the RCB isn't used.
	 */
	vrcb = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	BGE_HOSTADDR(taddr, sc->bge_ldata.bge_rx_return_ring_paddr);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, vrcb, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, vrcb, bge_nicaddr, 0x00000000);
	RCB_WRITE_4(sc, vrcb, bge_maxlen_flags,
	    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt, 0));

	/* Set random backoff seed for TX */
	CSR_WRITE_4(sc, BGE_TX_RANDOM_BACKOFF,
	    IF_LLADDR(sc->bge_ifp)[0] + IF_LLADDR(sc->bge_ifp)[1] +
	    IF_LLADDR(sc->bge_ifp)[2] + IF_LLADDR(sc->bge_ifp)[3] +
	    IF_LLADDR(sc->bge_ifp)[4] + IF_LLADDR(sc->bge_ifp)[5] +
	    BGE_TX_BACKOFF_SEED_MASK);

	/* Set inter-packet gap */
	CSR_WRITE_4(sc, BGE_TX_LENGTHS, 0x2620);

	/*
	 * Specify which ring to use for packets that don't match
	 * any RX rules.
	 */
	CSR_WRITE_4(sc, BGE_RX_RULES_CFG, 0x08);

	/*
	 * Configure number of RX lists. One interrupt distribution
	 * list, sixteen active lists, one bad frames class.
	 */
	CSR_WRITE_4(sc, BGE_RXLP_CFG, 0x181);

	/* Inialize RX list placement stats mask. */
	CSR_WRITE_4(sc, BGE_RXLP_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_RXLP_STATS_CTL, 0x1);

	/* Disable host coalescing until we get it set up */
	CSR_WRITE_4(sc, BGE_HCC_MODE, 0x00000000);

	/* Poll to make sure it's shut down. */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, BGE_HCC_MODE) & BGE_HCCMODE_ENABLE))
			break;
	}

	if (i == BGE_TIMEOUT) {
		device_printf(sc->bge_dev,
		    "host coalescing engine failed to idle\n");
		return (ENXIO);
	}

	/* Set up host coalescing defaults */
	CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS, sc->bge_rx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS, sc->bge_tx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS, sc->bge_rx_max_coal_bds);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS, sc->bge_tx_max_coal_bds);
	if (!(BGE_IS_5705_PLUS(sc))) {
		CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS_INT, 0);
		CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS_INT, 0);
	}
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 1);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 1);

	/* Set up address of statistics block */
	if (!(BGE_IS_5705_PLUS(sc))) {
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_HI,
		    BGE_ADDR_HI(sc->bge_ldata.bge_stats_paddr));
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_LO,
		    BGE_ADDR_LO(sc->bge_ldata.bge_stats_paddr));
		CSR_WRITE_4(sc, BGE_HCC_STATS_BASEADDR, BGE_STATS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_BASEADDR, BGE_STATUS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATS_TICKS, sc->bge_stat_ticks);
	}

	/* Set up address of status block */
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_HI,
	    BGE_ADDR_HI(sc->bge_ldata.bge_status_block_paddr));
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_LO,
	    BGE_ADDR_LO(sc->bge_ldata.bge_status_block_paddr));
	sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx = 0;
	sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx = 0;

	/* Turn on host coalescing state machine */
	CSR_WRITE_4(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);

	/* Turn on RX BD completion state machine and enable attentions */
	CSR_WRITE_4(sc, BGE_RBDC_MODE,
	    BGE_RBDCMODE_ENABLE | BGE_RBDCMODE_ATTN);

	/* Turn on RX list placement state machine */
	CSR_WRITE_4(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);

	/* Turn on RX list selector state machine. */
	if (!(BGE_IS_5705_PLUS(sc)))
		CSR_WRITE_4(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);

	/* Turn on DMA, clear stats */
	CSR_WRITE_4(sc, BGE_MAC_MODE, BGE_MACMODE_TXDMA_ENB |
	    BGE_MACMODE_RXDMA_ENB | BGE_MACMODE_RX_STATS_CLEAR |
	    BGE_MACMODE_TX_STATS_CLEAR | BGE_MACMODE_RX_STATS_ENB |
	    BGE_MACMODE_TX_STATS_ENB | BGE_MACMODE_FRMHDR_DMA_ENB |
	    ((sc->bge_flags & BGE_FLAG_TBI) ?
	    BGE_PORTMODE_TBI : BGE_PORTMODE_MII));

	/* Set misc. local control, enable interrupts on attentions */
	CSR_WRITE_4(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_ONATTN);

#ifdef notdef
	/* Assert GPIO pins for PHY reset */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUT0 |
	    BGE_MLC_MISCIO_OUT1 | BGE_MLC_MISCIO_OUT2);
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUTEN0 |
	    BGE_MLC_MISCIO_OUTEN1 | BGE_MLC_MISCIO_OUTEN2);
#endif

	/* Turn on DMA completion state machine */
	if (!(BGE_IS_5705_PLUS(sc)))
		CSR_WRITE_4(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);

	val = BGE_WDMAMODE_ENABLE | BGE_WDMAMODE_ALL_ATTNS;

	/* Enable host coalescing bug fix. */
	if (BGE_IS_5755_PLUS(sc))
		val |= BGE_WDMAMODE_STATUS_TAG_FIX;

	/* Turn on write DMA state machine */
	CSR_WRITE_4(sc, BGE_WDMA_MODE, val);
	DELAY(40);

	/* Turn on read DMA state machine */
	val = BGE_RDMAMODE_ENABLE | BGE_RDMAMODE_ALL_ATTNS;
	if (sc->bge_asicrev == BGE_ASICREV_BCM5784 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5785 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM57780)
		val |= BGE_RDMAMODE_BD_SBD_CRPT_ATTN |
		    BGE_RDMAMODE_MBUF_RBD_CRPT_ATTN |
		    BGE_RDMAMODE_MBUF_SBD_CRPT_ATTN;
	if (sc->bge_flags & BGE_FLAG_PCIE)
		val |= BGE_RDMAMODE_FIFO_LONG_BURST;
	CSR_WRITE_4(sc, BGE_RDMA_MODE, val);
	DELAY(40);

	/* Turn on RX data completion state machine */
	CSR_WRITE_4(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);

	/* Turn on RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);

	/* Turn on RX data and RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RDBDI_MODE, BGE_RDBDIMODE_ENABLE);

	/* Turn on Mbuf cluster free state machine */
	if (!(BGE_IS_5705_PLUS(sc)))
		CSR_WRITE_4(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	/* Turn on send BD completion state machine */
	CSR_WRITE_4(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/* Turn on send data completion state machine */
	val = BGE_SDCMODE_ENABLE;
	if (sc->bge_asicrev == BGE_ASICREV_BCM5761)
		val |= BGE_SDCMODE_CDELAY;
	CSR_WRITE_4(sc, BGE_SDC_MODE, val);

	/* Turn on send data initiator state machine */
	CSR_WRITE_4(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);

	/* Turn on send BD initiator state machine */
	CSR_WRITE_4(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);

	/* Turn on send BD selector state machine */
	CSR_WRITE_4(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);

	CSR_WRITE_4(sc, BGE_SDI_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_SDI_STATS_CTL,
	    BGE_SDISTATSCTL_ENABLE | BGE_SDISTATSCTL_FASTER);

	/* ack/clear link change events */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED |
	    BGE_MACSTAT_CFG_CHANGED | BGE_MACSTAT_MI_COMPLETE |
	    BGE_MACSTAT_LINK_CHANGED);
	CSR_WRITE_4(sc, BGE_MI_STS, 0);

	/* Enable PHY auto polling (for MII/GMII only) */
	if (sc->bge_flags & BGE_FLAG_TBI) {
		CSR_WRITE_4(sc, BGE_MI_STS, BGE_MISTS_LINK);
	} else {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL | (10 << 16));
		if (sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
		    sc->bge_chipid != BGE_CHIPID_BCM5700_B2)
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
	}

	/*
	 * Clear any pending link state attention.
	 * Otherwise some link state change events may be lost until attention
	 * is cleared by bge_intr() -> bge_link_upd() sequence.
	 * It's not necessary on newer BCM chips - perhaps enabling link
	 * state change attentions implies clearing pending attention.
	 */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED |
	    BGE_MACSTAT_CFG_CHANGED | BGE_MACSTAT_MI_COMPLETE |
	    BGE_MACSTAT_LINK_CHANGED);

	/* Enable link state change attentions. */
	BGE_SETBIT(sc, BGE_MAC_EVT_ENB, BGE_EVTENB_LINK_CHANGED);

	return (0);
}

const struct bge_revision *
bge_lookup_rev(uint32_t chipid)
{
	const struct bge_revision *br;

	for (br = bge_revisions; br->br_name != NULL; br++) {
		if (br->br_chipid == chipid)
			return (br);
	}

	for (br = bge_majorrevs; br->br_name != NULL; br++) {
		if (br->br_chipid == BGE_ASICREV(chipid))
			return (br);
	}

	return (NULL);
}

const struct bge_vendor *
bge_lookup_vendor(uint16_t vid)
{
	const struct bge_vendor *v;

	for (v = bge_vendors; v->v_name != NULL; v++)
		if (v->v_id == vid)
			return (v);

	panic("%s: unknown vendor %d", __func__, vid);
	return (NULL);
}

/*
 * Probe for a Broadcom chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match.
 *
 * Note that since the Broadcom controller contains VPD support, we
 * try to get the device name string from the controller itself instead
 * of the compiled-in string. It guarantees we'll always announce the
 * right product name. We fall back to the compiled-in string when
 * VPD is unavailable or corrupt.
 */
static int
bge_probe(device_t dev)
{
	const struct bge_type *t = bge_devs;
	struct bge_softc *sc = device_get_softc(dev);
	uint16_t vid, did;

	sc->bge_dev = dev;
	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);
	while(t->bge_vid != 0) {
		if ((vid == t->bge_vid) && (did == t->bge_did)) {
			char model[64], buf[96];
			const struct bge_revision *br;
			const struct bge_vendor *v;
			uint32_t id;

			id = pci_read_config(dev, BGE_PCI_MISC_CTL, 4) >>
			    BGE_PCIMISCCTL_ASICREV_SHIFT;
			if (BGE_ASICREV(id) == BGE_ASICREV_USE_PRODID_REG)
				id = pci_read_config(dev,
				    BGE_PCI_PRODID_ASICREV, 4);
			br = bge_lookup_rev(id);
			v = bge_lookup_vendor(vid);
			{
#if __FreeBSD_version > 700024
				const char *pname;

				if (bge_has_eaddr(sc) &&
				    pci_get_vpd_ident(dev, &pname) == 0)
					snprintf(model, 64, "%s", pname);
				else
#endif
					snprintf(model, 64, "%s %s",
					    v->v_name,
					    br != NULL ? br->br_name :
					    "NetXtreme Ethernet Controller");
			}
			snprintf(buf, 96, "%s, %sASIC rev. %#08x", model,
			    br != NULL ? "" : "unknown ", id);
			device_set_desc_copy(dev, buf);
			if (pci_get_subvendor(dev) == DELL_VENDORID)
				sc->bge_flags |= BGE_FLAG_NO_3LED;
			if (did == BCOM_DEVICEID_BCM5755M)
				sc->bge_flags |= BGE_FLAG_ADJUST_TRIM;
			return (0);
		}
		t++;
	}

	return (ENXIO);
}

static void
bge_dma_free(struct bge_softc *sc)
{
	int i;

	/* Destroy DMA maps for RX buffers. */
	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_rx_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i]);
	}
	if (sc->bge_cdata.bge_rx_std_sparemap)
		bus_dmamap_destroy(sc->bge_cdata.bge_rx_mtag,
		    sc->bge_cdata.bge_rx_std_sparemap);

	/* Destroy DMA maps for jumbo RX buffers. */
	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
	}
	if (sc->bge_cdata.bge_rx_jumbo_sparemap)
		bus_dmamap_destroy(sc->bge_cdata.bge_mtag_jumbo,
		    sc->bge_cdata.bge_rx_jumbo_sparemap);

	/* Destroy DMA maps for TX buffers. */
	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_tx_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i]);
	}

	if (sc->bge_cdata.bge_rx_mtag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_mtag);
	if (sc->bge_cdata.bge_tx_mtag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_tx_mtag);


	/* Destroy standard RX ring. */
	if (sc->bge_cdata.bge_rx_std_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_cdata.bge_rx_std_ring_map);
	if (sc->bge_cdata.bge_rx_std_ring_map && sc->bge_ldata.bge_rx_std_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_ldata.bge_rx_std_ring,
		    sc->bge_cdata.bge_rx_std_ring_map);

	if (sc->bge_cdata.bge_rx_std_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_std_ring_tag);

	/* Destroy jumbo RX ring. */
	if (sc->bge_cdata.bge_rx_jumbo_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);

	if (sc->bge_cdata.bge_rx_jumbo_ring_map &&
	    sc->bge_ldata.bge_rx_jumbo_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_ldata.bge_rx_jumbo_ring,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);

	if (sc->bge_cdata.bge_rx_jumbo_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_jumbo_ring_tag);

	/* Destroy RX return ring. */
	if (sc->bge_cdata.bge_rx_return_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_cdata.bge_rx_return_ring_map);

	if (sc->bge_cdata.bge_rx_return_ring_map &&
	    sc->bge_ldata.bge_rx_return_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_ldata.bge_rx_return_ring,
		    sc->bge_cdata.bge_rx_return_ring_map);

	if (sc->bge_cdata.bge_rx_return_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_return_ring_tag);

	/* Destroy TX ring. */
	if (sc->bge_cdata.bge_tx_ring_map)
		bus_dmamap_unload(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_cdata.bge_tx_ring_map);

	if (sc->bge_cdata.bge_tx_ring_map && sc->bge_ldata.bge_tx_ring)
		bus_dmamem_free(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_ldata.bge_tx_ring,
		    sc->bge_cdata.bge_tx_ring_map);

	if (sc->bge_cdata.bge_tx_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_tx_ring_tag);

	/* Destroy status block. */
	if (sc->bge_cdata.bge_status_map)
		bus_dmamap_unload(sc->bge_cdata.bge_status_tag,
		    sc->bge_cdata.bge_status_map);

	if (sc->bge_cdata.bge_status_map && sc->bge_ldata.bge_status_block)
		bus_dmamem_free(sc->bge_cdata.bge_status_tag,
		    sc->bge_ldata.bge_status_block,
		    sc->bge_cdata.bge_status_map);

	if (sc->bge_cdata.bge_status_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_status_tag);

	/* Destroy statistics block. */
	if (sc->bge_cdata.bge_stats_map)
		bus_dmamap_unload(sc->bge_cdata.bge_stats_tag,
		    sc->bge_cdata.bge_stats_map);

	if (sc->bge_cdata.bge_stats_map && sc->bge_ldata.bge_stats)
		bus_dmamem_free(sc->bge_cdata.bge_stats_tag,
		    sc->bge_ldata.bge_stats,
		    sc->bge_cdata.bge_stats_map);

	if (sc->bge_cdata.bge_stats_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_stats_tag);

	/* Destroy the parent tag. */
	if (sc->bge_cdata.bge_parent_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_parent_tag);
}

static int
bge_dma_alloc(device_t dev)
{
	struct bge_dmamap_arg ctx;
	struct bge_softc *sc;
	int i, error;

	sc = device_get_softc(dev);

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->bge_dev),
	    1, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,	NULL,
	    NULL, BUS_SPACE_MAXSIZE_32BIT, 0, BUS_SPACE_MAXSIZE_32BIT,
	    0, NULL, NULL, &sc->bge_cdata.bge_parent_tag);

	if (error != 0) {
		device_printf(sc->bge_dev,
		    "could not allocate parent dma tag\n");
		return (ENOMEM);
	}

	/*
	 * Create tag for Tx mbufs.
	 */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag, 1,
	    0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, MCLBYTES * BGE_NSEG_NEW, BGE_NSEG_NEW, MCLBYTES,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &sc->bge_cdata.bge_tx_mtag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate TX dma tag\n");
		return (ENOMEM);
	}

	/*
	 * Create tag for Rx mbufs.
	 */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag, 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1,
	    MCLBYTES, BUS_DMA_ALLOCNOW, NULL, NULL, &sc->bge_cdata.bge_rx_mtag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate RX dma tag\n");
		return (ENOMEM);
	}

	/* Create DMA maps for RX buffers. */
	error = bus_dmamap_create(sc->bge_cdata.bge_rx_mtag, 0,
	    &sc->bge_cdata.bge_rx_std_sparemap);
	if (error) {
		device_printf(sc->bge_dev,
		    "can't create spare DMA map for RX\n");
		return (ENOMEM);
	}
	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		error = bus_dmamap_create(sc->bge_cdata.bge_rx_mtag, 0,
			    &sc->bge_cdata.bge_rx_std_dmamap[i]);
		if (error) {
			device_printf(sc->bge_dev,
			    "can't create DMA map for RX\n");
			return (ENOMEM);
		}
	}

	/* Create DMA maps for TX buffers. */
	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		error = bus_dmamap_create(sc->bge_cdata.bge_tx_mtag, 0,
			    &sc->bge_cdata.bge_tx_dmamap[i]);
		if (error) {
			device_printf(sc->bge_dev,
			    "can't create DMA map for TX\n");
			return (ENOMEM);
		}
	}

	/* Create tag for standard RX ring. */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STD_RX_RING_SZ, 1, BGE_STD_RX_RING_SZ, 0,
	    NULL, NULL, &sc->bge_cdata.bge_rx_std_ring_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for standard RX ring. */
	error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_std_ring_tag,
	    (void **)&sc->bge_ldata.bge_rx_std_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_rx_std_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_rx_std_ring, BGE_STD_RX_RING_SZ);

	/* Load the address of the standard RX ring. */
	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, sc->bge_ldata.bge_rx_std_ring,
	    BGE_STD_RX_RING_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_rx_std_ring_paddr = ctx.bge_busaddr;

	/* Create tags for jumbo mbufs. */
	if (BGE_IS_JUMBO_CAPABLE(sc)) {
		error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
		    1, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
		    NULL, MJUM9BYTES, BGE_NSEG_JUMBO, PAGE_SIZE,
		    0, NULL, NULL, &sc->bge_cdata.bge_mtag_jumbo);
		if (error) {
			device_printf(sc->bge_dev,
			    "could not allocate jumbo dma tag\n");
			return (ENOMEM);
		}

		/* Create tag for jumbo RX ring. */
		error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
		    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
		    NULL, BGE_JUMBO_RX_RING_SZ, 1, BGE_JUMBO_RX_RING_SZ, 0,
		    NULL, NULL, &sc->bge_cdata.bge_rx_jumbo_ring_tag);

		if (error) {
			device_printf(sc->bge_dev,
			    "could not allocate jumbo ring dma tag\n");
			return (ENOMEM);
		}

		/* Allocate DMA'able memory for jumbo RX ring. */
		error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    (void **)&sc->bge_ldata.bge_rx_jumbo_ring,
		    BUS_DMA_NOWAIT | BUS_DMA_ZERO,
		    &sc->bge_cdata.bge_rx_jumbo_ring_map);
		if (error)
			return (ENOMEM);

		/* Load the address of the jumbo RX ring. */
		ctx.bge_maxsegs = 1;
		ctx.sc = sc;

		error = bus_dmamap_load(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    sc->bge_ldata.bge_rx_jumbo_ring, BGE_JUMBO_RX_RING_SZ,
		    bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

		if (error)
			return (ENOMEM);

		sc->bge_ldata.bge_rx_jumbo_ring_paddr = ctx.bge_busaddr;

		/* Create DMA maps for jumbo RX buffers. */
		error = bus_dmamap_create(sc->bge_cdata.bge_mtag_jumbo,
		    0, &sc->bge_cdata.bge_rx_jumbo_sparemap);
		if (error) {
			device_printf(sc->bge_dev,
			    "can't create spare DMA map for jumbo RX\n");
			return (ENOMEM);
		}
		for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
			error = bus_dmamap_create(sc->bge_cdata.bge_mtag_jumbo,
				    0, &sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
			if (error) {
				device_printf(sc->bge_dev,
				    "can't create DMA map for jumbo RX\n");
				return (ENOMEM);
			}
		}

	}

	/* Create tag for RX return ring. */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_RX_RTN_RING_SZ(sc), 1, BGE_RX_RTN_RING_SZ(sc), 0,
	    NULL, NULL, &sc->bge_cdata.bge_rx_return_ring_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for RX return ring. */
	error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_return_ring_tag,
	    (void **)&sc->bge_ldata.bge_rx_return_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_rx_return_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_rx_return_ring,
	    BGE_RX_RTN_RING_SZ(sc));

	/* Load the address of the RX return ring. */
	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map,
	    sc->bge_ldata.bge_rx_return_ring, BGE_RX_RTN_RING_SZ(sc),
	    bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_rx_return_ring_paddr = ctx.bge_busaddr;

	/* Create tag for TX ring. */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_TX_RING_SZ, 1, BGE_TX_RING_SZ, 0, NULL, NULL,
	    &sc->bge_cdata.bge_tx_ring_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for TX ring. */
	error = bus_dmamem_alloc(sc->bge_cdata.bge_tx_ring_tag,
	    (void **)&sc->bge_ldata.bge_tx_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_tx_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_tx_ring, BGE_TX_RING_SZ);

	/* Load the address of the TX ring. */
	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_tx_ring_tag,
	    sc->bge_cdata.bge_tx_ring_map, sc->bge_ldata.bge_tx_ring,
	    BGE_TX_RING_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_tx_ring_paddr = ctx.bge_busaddr;

	/* Create tag for status block. */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STATUS_BLK_SZ, 1, BGE_STATUS_BLK_SZ, 0,
	    NULL, NULL, &sc->bge_cdata.bge_status_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for status block. */
	error = bus_dmamem_alloc(sc->bge_cdata.bge_status_tag,
	    (void **)&sc->bge_ldata.bge_status_block, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_status_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_status_block, BGE_STATUS_BLK_SZ);

	/* Load the address of the status block. */
	ctx.sc = sc;
	ctx.bge_maxsegs = 1;

	error = bus_dmamap_load(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, sc->bge_ldata.bge_status_block,
	    BGE_STATUS_BLK_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_status_block_paddr = ctx.bge_busaddr;

	/* Create tag for statistics block. */
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STATS_SZ, 1, BGE_STATS_SZ, 0, NULL, NULL,
	    &sc->bge_cdata.bge_stats_tag);

	if (error) {
		device_printf(sc->bge_dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for statistics block. */
	error = bus_dmamem_alloc(sc->bge_cdata.bge_stats_tag,
	    (void **)&sc->bge_ldata.bge_stats, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_stats_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_stats, BGE_STATS_SZ);

	/* Load the address of the statstics block. */
	ctx.sc = sc;
	ctx.bge_maxsegs = 1;

	error = bus_dmamap_load(sc->bge_cdata.bge_stats_tag,
	    sc->bge_cdata.bge_stats_map, sc->bge_ldata.bge_stats,
	    BGE_STATS_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_stats_paddr = ctx.bge_busaddr;

	return (0);
}

/*
 * Return true if this device has more than one port.
 */
static int
bge_has_multiple_ports(struct bge_softc *sc)
{
	device_t dev = sc->bge_dev;
	u_int b, d, f, fscan, s;

	d = pci_get_domain(dev);
	b = pci_get_bus(dev);
	s = pci_get_slot(dev);
	f = pci_get_function(dev);
	for (fscan = 0; fscan <= PCI_FUNCMAX; fscan++)
		if (fscan != f && pci_find_dbsf(d, b, s, fscan) != NULL)
			return (1);
	return (0);
}

/*
 * Return true if MSI can be used with this device.
 */
static int
bge_can_use_msi(struct bge_softc *sc)
{
	int can_use_msi = 0;

	switch (sc->bge_asicrev) {
	case BGE_ASICREV_BCM5714_A0:
	case BGE_ASICREV_BCM5714:
		/*
		 * Apparently, MSI doesn't work when these chips are
		 * configured in single-port mode.
		 */
		if (bge_has_multiple_ports(sc))
			can_use_msi = 1;
		break;
	case BGE_ASICREV_BCM5750:
		if (sc->bge_chiprev != BGE_CHIPREV_5750_AX &&
		    sc->bge_chiprev != BGE_CHIPREV_5750_BX)
			can_use_msi = 1;
		break;
	default:
		if (BGE_IS_575X_PLUS(sc))
			can_use_msi = 1;
	}
	return (can_use_msi);
}

static int
bge_attach(device_t dev)
{
	struct ifnet *ifp;
	struct bge_softc *sc;
	uint32_t hwcfg = 0, misccfg;
	u_char eaddr[ETHER_ADDR_LEN];
	int error, msicount, reg, rid, trys;

	sc = device_get_softc(dev);
	sc->bge_dev = dev;

	TASK_INIT(&sc->bge_intr_task, 0, bge_intr_task, sc);

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = BGE_PCI_BAR0;
	sc->bge_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (sc->bge_res == NULL) {
		device_printf (sc->bge_dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	/* Save various chip information. */
	sc->bge_chipid =
	    pci_read_config(dev, BGE_PCI_MISC_CTL, 4) >>
	    BGE_PCIMISCCTL_ASICREV_SHIFT;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_USE_PRODID_REG)
		sc->bge_chipid = pci_read_config(dev, BGE_PCI_PRODID_ASICREV,
		    4);
	sc->bge_asicrev = BGE_ASICREV(sc->bge_chipid);
	sc->bge_chiprev = BGE_CHIPREV(sc->bge_chipid);

	/*
	 * Don't enable Ethernet@WireSpeed for the 5700, 5906, or the
	 * 5705 A0 and A1 chips.
	 */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5700 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5906 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5705_A0 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5705_A1)
		sc->bge_flags |= BGE_FLAG_WIRESPEED;

	if (bge_has_eaddr(sc))
		sc->bge_flags |= BGE_FLAG_EADDR;

	/* Save chipset family. */
	switch (sc->bge_asicrev) {
	case BGE_ASICREV_BCM5755:
	case BGE_ASICREV_BCM5761:
	case BGE_ASICREV_BCM5784:
	case BGE_ASICREV_BCM5785:
	case BGE_ASICREV_BCM5787:
	case BGE_ASICREV_BCM57780:
		sc->bge_flags |= BGE_FLAG_5755_PLUS | BGE_FLAG_575X_PLUS |
		    BGE_FLAG_5705_PLUS;
		break;
	case BGE_ASICREV_BCM5700:
	case BGE_ASICREV_BCM5701:
	case BGE_ASICREV_BCM5703:
	case BGE_ASICREV_BCM5704:
		sc->bge_flags |= BGE_FLAG_5700_FAMILY | BGE_FLAG_JUMBO;
		break;
	case BGE_ASICREV_BCM5714_A0:
	case BGE_ASICREV_BCM5780:
	case BGE_ASICREV_BCM5714:
		sc->bge_flags |= BGE_FLAG_5714_FAMILY /* | BGE_FLAG_JUMBO */;
		/* FALLTHROUGH */
	case BGE_ASICREV_BCM5750:
	case BGE_ASICREV_BCM5752:
	case BGE_ASICREV_BCM5906:
		sc->bge_flags |= BGE_FLAG_575X_PLUS;
		/* FALLTHROUGH */
	case BGE_ASICREV_BCM5705:
		sc->bge_flags |= BGE_FLAG_5705_PLUS;
		break;
	}

	/* Set various bug flags. */
	if (sc->bge_chipid == BGE_CHIPID_BCM5701_A0 ||
	    sc->bge_chipid == BGE_CHIPID_BCM5701_B0)
		sc->bge_flags |= BGE_FLAG_CRC_BUG;
	if (sc->bge_chiprev == BGE_CHIPREV_5703_AX ||
	    sc->bge_chiprev == BGE_CHIPREV_5704_AX)
		sc->bge_flags |= BGE_FLAG_ADC_BUG;
	if (sc->bge_chipid == BGE_CHIPID_BCM5704_A0)
		sc->bge_flags |= BGE_FLAG_5704_A0_BUG;
	if (BGE_IS_5705_PLUS(sc) &&
	    !(sc->bge_flags & BGE_FLAG_ADJUST_TRIM)) {
		if (sc->bge_asicrev == BGE_ASICREV_BCM5755 ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5761 ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5784 ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5787) {
			if (sc->bge_chipid != BGE_CHIPID_BCM5722_A0)
				sc->bge_flags |= BGE_FLAG_JITTER_BUG;
		} else if (sc->bge_asicrev != BGE_ASICREV_BCM5906)
			sc->bge_flags |= BGE_FLAG_BER_BUG;
	}


	/*
	 * We could possibly check for BCOM_DEVICEID_BCM5788 in bge_probe()
	 * but I do not know the DEVICEID for the 5788M.
	 */
	misccfg = CSR_READ_4(sc, BGE_MISC_CFG) & BGE_MISCCFG_BOARD_ID;
	if (misccfg == BGE_MISCCFG_BOARD_ID_5788 ||
	    misccfg == BGE_MISCCFG_BOARD_ID_5788M)
		sc->bge_flags |= BGE_FLAG_5788;

  	/*
	 * Check if this is a PCI-X or PCI Express device.
  	 */
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		/*
		 * Found a PCI Express capabilities register, this
		 * must be a PCI Express device.
		 */
		sc->bge_flags |= BGE_FLAG_PCIE;
		sc->bge_expcap = reg;
		bge_set_max_readrq(sc);
	} else {
		/*
		 * Check if the device is in PCI-X Mode.
		 * (This bit is not valid on PCI Express controllers.)
		 */
		if (pci_find_extcap(dev, PCIY_PCIX, &reg) == 0)
			sc->bge_pcixcap = reg;
		if ((pci_read_config(dev, BGE_PCI_PCISTATE, 4) &
		    BGE_PCISTATE_PCI_BUSMODE) == 0)
			sc->bge_flags |= BGE_FLAG_PCIX;
	}

	/*
	 * Allocate the interrupt, using MSI if possible.  These devices
	 * support 8 MSI messages, but only the first one is used in
	 * normal operation.
	 */
	rid = 0;
	if (pci_find_extcap(sc->bge_dev, PCIY_MSI, &reg) != 0) {
		sc->bge_msicap = reg;
		if (bge_can_use_msi(sc)) {
			msicount = pci_msi_count(dev);
			if (msicount > 1)
				msicount = 1;
		} else
			msicount = 0;
		if (msicount == 1 && pci_alloc_msi(dev, &msicount) == 0) {
			rid = 1;
			sc->bge_flags |= BGE_FLAG_MSI;
		}
	}

	sc->bge_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->bge_irq == NULL) {
		device_printf(sc->bge_dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	if (bootverbose)
		device_printf(dev,
		    "CHIP ID 0x%08x; ASIC REV 0x%02x; CHIP REV 0x%02x; %s\n",
		    sc->bge_chipid, sc->bge_asicrev, sc->bge_chiprev,
		    (sc->bge_flags & BGE_FLAG_PCIX) ? "PCI-X" :
		    ((sc->bge_flags & BGE_FLAG_PCIE) ? "PCI-E" : "PCI"));

	BGE_LOCK_INIT(sc, device_get_nameunit(dev));

	/* Try to reset the chip. */
	if (bge_reset(sc)) {
		device_printf(sc->bge_dev, "chip reset failed\n");
		error = ENXIO;
		goto fail;
	}

	sc->bge_asf_mode = 0;
	if (bge_allow_asf && (bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_SIG)
	    == BGE_MAGIC_NUMBER)) {
		if (bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_NICCFG)
		    & BGE_HWCFG_ASF) {
			sc->bge_asf_mode |= ASF_ENABLE;
			sc->bge_asf_mode |= ASF_STACKUP;
			if (sc->bge_asicrev == BGE_ASICREV_BCM5750) {
				sc->bge_asf_mode |= ASF_NEW_HANDSHAKE;
			}
		}
	}

	/* Try to reset the chip again the nice way. */
	bge_stop_fw(sc);
	bge_sig_pre_reset(sc, BGE_RESET_STOP);
	if (bge_reset(sc)) {
		device_printf(sc->bge_dev, "chip reset failed\n");
		error = ENXIO;
		goto fail;
	}

	bge_sig_legacy(sc, BGE_RESET_STOP);
	bge_sig_post_reset(sc, BGE_RESET_STOP);

	if (bge_chipinit(sc)) {
		device_printf(sc->bge_dev, "chip initialization failed\n");
		error = ENXIO;
		goto fail;
	}

	error = bge_get_eaddr(sc, eaddr);
	if (error) {
		device_printf(sc->bge_dev,
		    "failed to read station address\n");
		error = ENXIO;
		goto fail;
	}

	/* 5705 limits RX return ring to 512 entries. */
	if (BGE_IS_5705_PLUS(sc))
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT_5705;
	else
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT;

	if (bge_dma_alloc(dev)) {
		device_printf(sc->bge_dev,
		    "failed to allocate DMA resources\n");
		error = ENXIO;
		goto fail;
	}

	/* Set default tuneable values. */
	sc->bge_stat_ticks = BGE_TICKS_PER_SEC;
	sc->bge_rx_coal_ticks = 150;
	sc->bge_tx_coal_ticks = 150;
	sc->bge_rx_max_coal_bds = 10;
	sc->bge_tx_max_coal_bds = 10;

	/* Set up ifnet structure */
	ifp = sc->bge_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->bge_dev, "failed to if_alloc()\n");
		error = ENXIO;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bge_ioctl;
	ifp->if_start = bge_start;
	ifp->if_init = bge_init;
	ifp->if_snd.ifq_drv_maxlen = BGE_TX_RING_CNT - 1;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_hwassist = BGE_CSUM_FEATURES;
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_VLAN_HWTAGGING |
	    IFCAP_VLAN_MTU;
#ifdef IFCAP_VLAN_HWCSUM
	ifp->if_capabilities |= IFCAP_VLAN_HWCSUM;
#endif
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/*
	 * 5700 B0 chips do not support checksumming correctly due
	 * to hardware bugs.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5700_B0) {
		ifp->if_capabilities &= ~IFCAP_HWCSUM;
		ifp->if_capenable &= ~IFCAP_HWCSUM;
		ifp->if_hwassist = 0;
	}

	/*
	 * Figure out what sort of media we have by checking the
	 * hardware config word in the first 32k of NIC internal memory,
	 * or fall back to examining the EEPROM if necessary.
	 * Note: on some BCM5700 cards, this value appears to be unset.
	 * If that's the case, we have to rely on identifying the NIC
	 * by its PCI subsystem ID, as we do below for the SysKonnect
	 * SK-9D41.
	 */
	if (bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_SIG) == BGE_MAGIC_NUMBER)
		hwcfg = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_NICCFG);
	else if ((sc->bge_flags & BGE_FLAG_EADDR) &&
	    (sc->bge_asicrev != BGE_ASICREV_BCM5906)) {
		if (bge_read_eeprom(sc, (caddr_t)&hwcfg, BGE_EE_HWCFG_OFFSET,
		    sizeof(hwcfg))) {
			device_printf(sc->bge_dev, "failed to read EEPROM\n");
			error = ENXIO;
			goto fail;
		}
		hwcfg = ntohl(hwcfg);
	}

	if ((hwcfg & BGE_HWCFG_MEDIA) == BGE_MEDIA_FIBER)
		sc->bge_flags |= BGE_FLAG_TBI;

	/* The SysKonnect SK-9D41 is a 1000baseSX card. */
	if ((pci_read_config(dev, BGE_PCI_SUBSYS, 4) >> 16) == SK_SUBSYSID_9D41)
		sc->bge_flags |= BGE_FLAG_TBI;

	if (sc->bge_flags & BGE_FLAG_TBI) {
		ifmedia_init(&sc->bge_ifmedia, IFM_IMASK, bge_ifmedia_upd,
		    bge_ifmedia_sts);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER | IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER | IFM_1000_SX | IFM_FDX,
		    0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER | IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->bge_ifmedia, IFM_ETHER | IFM_AUTO);
		sc->bge_ifmedia.ifm_media = sc->bge_ifmedia.ifm_cur->ifm_media;
	} else {
		/*
		 * Do transceiver setup and tell the firmware the
		 * driver is down so we can try to get access the
		 * probe if ASF is running.  Retry a couple of times
		 * if we get a conflict with the ASF firmware accessing
		 * the PHY.
		 */
		trys = 0;
		BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);
again:
		bge_asf_driver_up(sc);

		if (mii_phy_probe(dev, &sc->bge_miibus,
		    bge_ifmedia_upd, bge_ifmedia_sts)) {
			if (trys++ < 4) {
				device_printf(sc->bge_dev, "Try again\n");
				bge_miibus_writereg(sc->bge_dev, 1, MII_BMCR,
				    BMCR_RESET);
				goto again;
			}

			device_printf(sc->bge_dev, "MII without any PHY!\n");
			error = ENXIO;
			goto fail;
		}

		/*
		 * Now tell the firmware we are going up after probing the PHY
		 */
		if (sc->bge_asf_mode & ASF_STACKUP)
			BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);
	}

	/*
	 * When using the BCM5701 in PCI-X mode, data corruption has
	 * been observed in the first few bytes of some received packets.
	 * Aligning the packet buffer in memory eliminates the corruption.
	 * Unfortunately, this misaligns the packet payloads.  On platforms
	 * which do not support unaligned accesses, we will realign the
	 * payloads by copying the received packets.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5701 &&
	    sc->bge_flags & BGE_FLAG_PCIX)
                sc->bge_flags |= BGE_FLAG_RX_ALIGNBUG;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
	callout_init_mtx(&sc->bge_stat_ch, &sc->bge_mtx, 0);

	/* Tell upper layer we support long frames. */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	/*
	 * Hookup IRQ last.
	 */
#if __FreeBSD_version > 700030
	if (BGE_IS_5755_PLUS(sc) && sc->bge_flags & BGE_FLAG_MSI) {
		/* Take advantage of single-shot MSI. */
		sc->bge_tq = taskqueue_create_fast("bge_taskq", M_WAITOK,
		    taskqueue_thread_enqueue, &sc->bge_tq);
		if (sc->bge_tq == NULL) {
			device_printf(dev, "could not create taskqueue.\n");
			ether_ifdetach(ifp);
			error = ENXIO;
			goto fail;
		}
		taskqueue_start_threads(&sc->bge_tq, 1, PI_NET, "%s taskq",
		    device_get_nameunit(sc->bge_dev));
		error = bus_setup_intr(dev, sc->bge_irq,
		    INTR_TYPE_NET | INTR_MPSAFE, bge_msi_intr, NULL, sc,
		    &sc->bge_intrhand);
		if (error)
			ether_ifdetach(ifp);
	} else
		error = bus_setup_intr(dev, sc->bge_irq,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, bge_intr, sc,
		    &sc->bge_intrhand);
#else
	error = bus_setup_intr(dev, sc->bge_irq, INTR_TYPE_NET | INTR_MPSAFE,
	   bge_intr, sc, &sc->bge_intrhand);
#endif

	if (error) {
		bge_detach(dev);
		device_printf(sc->bge_dev, "couldn't set up irq\n");
	}

	bge_add_sysctls(sc);

	return (0);

fail:
	bge_release_resources(sc);

	return (error);
}

static int
bge_detach(device_t dev)
{
	struct bge_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->bge_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	BGE_LOCK(sc);
	bge_stop(sc);
	bge_reset(sc);
	BGE_UNLOCK(sc);

	callout_drain(&sc->bge_stat_ch);

	if (sc->bge_tq)
		taskqueue_drain(sc->bge_tq, &sc->bge_intr_task);
	ether_ifdetach(ifp);

	if (sc->bge_flags & BGE_FLAG_TBI) {
		ifmedia_removeall(&sc->bge_ifmedia);
	} else {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->bge_miibus);
	}

	bge_release_resources(sc);

	return (0);
}

static void
bge_release_resources(struct bge_softc *sc)
{
	device_t dev;

	dev = sc->bge_dev;

	if (sc->bge_tq != NULL)
		taskqueue_free(sc->bge_tq);

	if (sc->bge_intrhand != NULL)
		bus_teardown_intr(dev, sc->bge_irq, sc->bge_intrhand);

	if (sc->bge_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->bge_flags & BGE_FLAG_MSI ? 1 : 0, sc->bge_irq);

	if (sc->bge_flags & BGE_FLAG_MSI)
		pci_release_msi(dev);

	if (sc->bge_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    BGE_PCI_BAR0, sc->bge_res);

	if (sc->bge_ifp != NULL)
		if_free(sc->bge_ifp);

	bge_dma_free(sc);

	if (mtx_initialized(&sc->bge_mtx))	/* XXX */
		BGE_LOCK_DESTROY(sc);
}

static int
bge_reset(struct bge_softc *sc)
{
	device_t dev;
	uint32_t cachesize, command, pcistate, reset, val;
	void (*write_op)(struct bge_softc *, int, int);
	uint16_t devctl;
	int i;

	dev = sc->bge_dev;

	if (BGE_IS_575X_PLUS(sc) && !BGE_IS_5714_FAMILY(sc) &&
	    (sc->bge_asicrev != BGE_ASICREV_BCM5906)) {
		if (sc->bge_flags & BGE_FLAG_PCIE)
			write_op = bge_writemem_direct;
		else
			write_op = bge_writemem_ind;
	} else
		write_op = bge_writereg_ind;

	/* Save some important PCI state. */
	cachesize = pci_read_config(dev, BGE_PCI_CACHESZ, 4);
	command = pci_read_config(dev, BGE_PCI_CMD, 4);
	pcistate = pci_read_config(dev, BGE_PCI_PCISTATE, 4);

	pci_write_config(dev, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS | BGE_PCIMISCCTL_MASK_PCI_INTR |
	    BGE_HIF_SWAP_OPTIONS | BGE_PCIMISCCTL_PCISTATE_RW, 4);

	/* Disable fastboot on controllers that support it. */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5752 ||
	    BGE_IS_5755_PLUS(sc)) {
		if (bootverbose)
			device_printf(sc->bge_dev, "Disabling fastboot\n");
		CSR_WRITE_4(sc, BGE_FASTBOOT_PC, 0x0);
	}

	/*
	 * Write the magic number to SRAM at offset 0xB50.
	 * When firmware finishes its initialization it will
	 * write ~BGE_MAGIC_NUMBER to the same location.
	 */
	bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM, BGE_MAGIC_NUMBER);

	reset = BGE_MISCCFG_RESET_CORE_CLOCKS | BGE_32BITTIME_66MHZ;

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_flags & BGE_FLAG_PCIE) {
		if (CSR_READ_4(sc, 0x7E2C) == 0x60)	/* PCIE 1.0 */
			CSR_WRITE_4(sc, 0x7E2C, 0x20);
		if (sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
			/* Prevent PCIE link training during global reset */
			CSR_WRITE_4(sc, BGE_MISC_CFG, 1 << 29);
			reset |= 1 << 29;
		}
	}

	/*
	 * Set GPHY Power Down Override to leave GPHY
	 * powered up in D0 uninitialized.
	 */
	if (BGE_IS_5705_PLUS(sc))
		reset |= 0x04000000;

	/* Issue global reset */
	write_op(sc, BGE_MISC_CFG, reset);

	if (sc->bge_asicrev == BGE_ASICREV_BCM5906) {
		val = CSR_READ_4(sc, BGE_VCPU_STATUS);
		CSR_WRITE_4(sc, BGE_VCPU_STATUS,
		    val | BGE_VCPU_STATUS_DRV_RESET);
		val = CSR_READ_4(sc, BGE_VCPU_EXT_CTRL);
		CSR_WRITE_4(sc, BGE_VCPU_EXT_CTRL,
		    val & ~BGE_VCPU_EXT_CTRL_HALT_CPU);
	}

	DELAY(1000);

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_flags & BGE_FLAG_PCIE) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5750_A0) {
			DELAY(500000); /* wait for link training to complete */
			val = pci_read_config(dev, 0xC4, 4);
			pci_write_config(dev, 0xC4, val | (1 << 15), 4);
		}
		devctl = pci_read_config(dev,
		    sc->bge_expcap + PCIR_EXPRESS_DEVICE_CTL, 2);
		/* Clear enable no snoop and disable relaxed ordering. */
		devctl &= ~(0x0010 | 0x0800);
		/* Set PCIE max payload size to 128. */
		devctl &= ~PCIM_EXP_CTL_MAX_PAYLOAD;
		pci_write_config(dev, sc->bge_expcap + PCIR_EXPRESS_DEVICE_CTL,
		    devctl, 2);
		/* Clear error status. */
		pci_write_config(dev, sc->bge_expcap + PCIR_EXPRESS_DEVICE_STA,
		    0, 2);
	}

	/* Reset some of the PCI state that got zapped by reset. */
	pci_write_config(dev, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS | BGE_PCIMISCCTL_MASK_PCI_INTR |
	    BGE_HIF_SWAP_OPTIONS | BGE_PCIMISCCTL_PCISTATE_RW, 4);
	pci_write_config(dev, BGE_PCI_CACHESZ, cachesize, 4);
	pci_write_config(dev, BGE_PCI_CMD, command, 4);
	write_op(sc, BGE_MISC_CFG, BGE_32BITTIME_66MHZ);

	/* Re-enable MSI, if neccesary, and enable the memory arbiter. */
	if (BGE_IS_5714_FAMILY(sc)) {
		/* This chip disables MSI on reset. */
		if (sc->bge_flags & BGE_FLAG_MSI) {
			val = pci_read_config(dev,
			    sc->bge_msicap + PCIR_MSI_CTRL, 2);
			pci_write_config(dev,
			    sc->bge_msicap + PCIR_MSI_CTRL,
			    val | PCIM_MSICTRL_MSI_ENABLE, 2);
			val = CSR_READ_4(sc, BGE_MSI_MODE);
			CSR_WRITE_4(sc, BGE_MSI_MODE,
			    val | BGE_MSIMODE_ENABLE);
		}
		val = CSR_READ_4(sc, BGE_MARB_MODE);
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE | val);
	} else
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);

	if (sc->bge_asicrev == BGE_ASICREV_BCM5906) {
		for (i = 0; i < BGE_TIMEOUT; i++) {
			val = CSR_READ_4(sc, BGE_VCPU_STATUS);
			if (val & BGE_VCPU_STATUS_INIT_DONE)
				break;
			DELAY(100);
		}
		if (i == BGE_TIMEOUT) {
			device_printf(sc->bge_dev, "reset timed out\n");
			return (1);
		}
	} else {
		/*
		 * Poll until we see the 1's complement of the magic number.
		 * This indicates that the firmware initialization is complete.
		 * We expect this to fail if no chip containing the Ethernet
		 * address is fitted though.
		 */
		for (i = 0; i < BGE_TIMEOUT; i++) {
			DELAY(10);
			val = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM);
			if (val == ~BGE_MAGIC_NUMBER)
				break;
		}

		if ((sc->bge_flags & BGE_FLAG_EADDR) && i == BGE_TIMEOUT)
			device_printf(sc->bge_dev, "firmware handshake timed out, "
			    "found 0x%08x\n", val);
	}

	/*
	 * XXX Wait for the value of the PCISTATE register to
	 * return to its original pre-reset state. This is a
	 * fairly good indicator of reset completion. If we don't
	 * wait for the reset to fully complete, trying to read
	 * from the device's non-PCI registers may yield garbage
	 * results.
	 */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (pci_read_config(dev, BGE_PCI_PCISTATE, 4) == pcistate)
			break;
		DELAY(10);
	}

	if (sc->bge_flags & BGE_FLAG_PCIE) {
		reset = bge_readmem_ind(sc, 0x7C00);
		bge_writemem_ind(sc, 0x7C00, reset | (1 << 25));
	}

	/* Fix up byte swapping. */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS |
	    BGE_MODECTL_BYTESWAP_DATA);

	/* Tell the ASF firmware we are up */
	if (sc->bge_asf_mode & ASF_STACKUP)
		BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * The 5704 in TBI mode apparently needs some special
	 * adjustment to insure the SERDES drive level is set
	 * to 1.2V.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5704 &&
	    sc->bge_flags & BGE_FLAG_TBI) {
		val = CSR_READ_4(sc, BGE_SERDES_CFG);
		val = (val & ~0xFFF) | 0x880;
		CSR_WRITE_4(sc, BGE_SERDES_CFG, val);
	}

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_flags & BGE_FLAG_PCIE &&
	    sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
		val = CSR_READ_4(sc, 0x7C00);
		CSR_WRITE_4(sc, 0x7C00, val | (1 << 25));
	}
	DELAY(10000);

	return(0);
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle two possibilities here:
 * 1) the frame is from the jumbo receive ring
 * 2) the frame is from the standard receive ring
 */

static int
bge_rxeof(struct bge_softc *sc, uint16_t rx_prod, int holdlck)
{
	struct ifnet *ifp;
	int rx_npkts = 0, stdcnt = 0, jumbocnt = 0;
	uint16_t rx_cons;

	rx_cons = sc->bge_rx_saved_considx;

	/* Nothing to do. */
	if (rx_cons == rx_prod)
		return (rx_npkts);

	ifp = sc->bge_ifp;

	bus_dmamap_sync(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_POSTWRITE);
	if (ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN >
	    (MCLBYTES - ETHER_ALIGN))
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map, BUS_DMASYNC_POSTWRITE);

	while (rx_cons != rx_prod) {
		struct bge_rx_bd	*cur_rx;
		uint32_t		rxidx;
		struct mbuf		*m = NULL;
		uint16_t		vlan_tag = 0;
		int			have_tag = 0;

#ifdef DEVICE_POLLING
		if (ifp->if_capenable & IFCAP_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif

		cur_rx = &sc->bge_ldata.bge_rx_return_ring[rx_cons];

		rxidx = cur_rx->bge_idx;
		BGE_INC(rx_cons, sc->bge_return_ring_cnt);

		if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING &&
		    cur_rx->bge_flags & BGE_RXBDFLAG_VLAN_TAG) {
			have_tag = 1;
			vlan_tag = cur_rx->bge_vlan_tag;
		}

		if (cur_rx->bge_flags & BGE_RXBDFLAG_JUMBO_RING) {
			jumbocnt++;
			m = sc->bge_cdata.bge_rx_jumbo_chain[rxidx];
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
				continue;
			}
			if (bge_newbuf_jumbo(sc, rxidx) != 0) {
				BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
				ifp->if_iqdrops++;
				continue;
			}
			BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
		} else {
			stdcnt++;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
				continue;
			}
			m = sc->bge_cdata.bge_rx_std_chain[rxidx];
			if (bge_newbuf_std(sc, rxidx) != 0) {
				BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
				ifp->if_iqdrops++;
				continue;
			}
			BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
		}

		ifp->if_ipackets++;
#ifndef __NO_STRICT_ALIGNMENT
		/*
		 * For architectures with strict alignment we must make sure
		 * the payload is aligned.
		 */
		if (sc->bge_flags & BGE_FLAG_RX_ALIGNBUG) {
			bcopy(m->m_data, m->m_data + ETHER_ALIGN,
			    cur_rx->bge_len);
			m->m_data += ETHER_ALIGN;
		}
#endif
		m->m_pkthdr.len = m->m_len = cur_rx->bge_len - ETHER_CRC_LEN;
		m->m_pkthdr.rcvif = ifp;

		if (ifp->if_capenable & IFCAP_RXCSUM) {
			if (cur_rx->bge_flags & BGE_RXBDFLAG_IP_CSUM) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				if ((cur_rx->bge_ip_csum ^ 0xFFFF) == 0)
					m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			}
			if (cur_rx->bge_flags & BGE_RXBDFLAG_TCP_UDP_CSUM &&
			    m->m_pkthdr.len >= ETHER_MIN_NOPAD) {
				m->m_pkthdr.csum_data =
				    cur_rx->bge_tcp_udp_csum;
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			}
		}

		/*
		 * If we received a packet with a vlan tag,
		 * attach that information to the packet.
		 */
		if (have_tag) {
#if __FreeBSD_version > 700022
			m->m_pkthdr.ether_vtag = vlan_tag;
			m->m_flags |= M_VLANTAG;
#else
			VLAN_INPUT_TAG_NEW(ifp, m, vlan_tag);
			if (m == NULL)
				continue;
#endif
		}

		if (holdlck != 0) {
			BGE_UNLOCK(sc);
			(*ifp->if_input)(ifp, m);
			BGE_LOCK(sc);
		} else
			(*ifp->if_input)(ifp, m);
		rx_npkts++;

		if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
			return (rx_npkts);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map, BUS_DMASYNC_PREREAD);
	if (stdcnt > 0)
		bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_PREWRITE);

	if (jumbocnt > 0)
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map, BUS_DMASYNC_PREWRITE);

	sc->bge_rx_saved_considx = rx_cons;
	bge_writembx(sc, BGE_MBX_RX_CONS0_LO, sc->bge_rx_saved_considx);
	if (stdcnt)
		bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);
	if (jumbocnt)
		bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);
#ifdef notyet
	/*
	 * This register wraps very quickly under heavy packet drops.
	 * If you need correct statistics, you can enable this check.
	 */
	if (BGE_IS_5705_PLUS(sc))
		ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_DROPS);
#endif
	return (rx_npkts);
}

static void
bge_txeof(struct bge_softc *sc, uint16_t tx_cons)
{
	struct bge_tx_bd *cur_tx = NULL;
	struct ifnet *ifp;

	BGE_LOCK_ASSERT(sc);

	/* Nothing to do. */
	if (sc->bge_tx_saved_considx == tx_cons)
		return;

	ifp = sc->bge_ifp;

	bus_dmamap_sync(sc->bge_cdata.bge_tx_ring_tag,
	    sc->bge_cdata.bge_tx_ring_map, BUS_DMASYNC_POSTWRITE);
	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->bge_tx_saved_considx != tx_cons) {
		uint32_t		idx = 0;

		idx = sc->bge_tx_saved_considx;
		cur_tx = &sc->bge_ldata.bge_tx_ring[idx];
		if (cur_tx->bge_flags & BGE_TXBDFLAG_END)
			ifp->if_opackets++;
		if (sc->bge_cdata.bge_tx_chain[idx] != NULL) {
			bus_dmamap_sync(sc->bge_cdata.bge_tx_mtag,
			    sc->bge_cdata.bge_tx_dmamap[idx],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bge_cdata.bge_tx_mtag,
			    sc->bge_cdata.bge_tx_dmamap[idx]);
			m_freem(sc->bge_cdata.bge_tx_chain[idx]);
			sc->bge_cdata.bge_tx_chain[idx] = NULL;
		}
		sc->bge_txcnt--;
		BGE_INC(sc->bge_tx_saved_considx, BGE_TX_RING_CNT);
	}

	if (cur_tx != NULL)
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	if (sc->bge_txcnt == 0)
		sc->bge_timer = 0;
}

#ifdef DEVICE_POLLING
static int
bge_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct bge_softc *sc = ifp->if_softc;
	uint16_t rx_prod, tx_cons;
	uint32_t statusword;
	int rx_npkts = 0;

	BGE_LOCK(sc);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		BGE_UNLOCK(sc);
		return (rx_npkts);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	rx_prod = sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx;
	tx_cons = sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx;

	statusword = atomic_readandclear_32(
	    &sc->bge_ldata.bge_status_block->bge_status);

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Note link event. It will be processed by POLL_AND_CHECK_STATUS. */
	if (statusword & BGE_STATFLAG_LINKSTATE_CHANGED)
		sc->bge_link_evt++;

	if (cmd == POLL_AND_CHECK_STATUS)
		if ((sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
		    sc->bge_chipid != BGE_CHIPID_BCM5700_B2) ||
		    sc->bge_link_evt || (sc->bge_flags & BGE_FLAG_TBI))
			bge_link_upd(sc);

	sc->rxcycles = count;
	rx_npkts = bge_rxeof(sc, rx_prod, 1);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		BGE_UNLOCK(sc);
		return (rx_npkts);
	}
	bge_txeof(sc, tx_cons);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bge_start_locked(ifp);

	BGE_UNLOCK(sc);
	return (rx_npkts);
}
#endif /* DEVICE_POLLING */

static int
bge_msi_intr(void *arg)
{
	struct bge_softc *sc;

	sc = (struct bge_softc *)arg;
	/*
	 * This interrupt is not shared and controller already
	 * disabled further interrupt.
	 */
	taskqueue_enqueue(sc->bge_tq, &sc->bge_intr_task);
	return (FILTER_HANDLED);
}

static void
bge_intr_task(void *arg, int pending)
{
	struct bge_softc *sc;
	struct ifnet *ifp;
	uint32_t status;
	uint16_t rx_prod, tx_cons;

	sc = (struct bge_softc *)arg;
	ifp = sc->bge_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	/* Get updated status block. */
	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* Save producer/consumer indexess. */
	rx_prod = sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx;
	tx_cons = sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx;
	status = sc->bge_ldata.bge_status_block->bge_status;
	sc->bge_ldata.bge_status_block->bge_status = 0;
	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/* Let controller work. */
	bge_writembx(sc, BGE_MBX_IRQ0_LO, 0);

	if ((status & BGE_STATFLAG_LINKSTATE_CHANGED) != 0) {
		BGE_LOCK(sc);
		bge_link_upd(sc);
		BGE_UNLOCK(sc);
	}
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/* Check RX return ring producer/consumer. */
		bge_rxeof(sc, rx_prod, 0);
	}
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		BGE_LOCK(sc);
		/* Check TX ring producer/consumer. */
		bge_txeof(sc, tx_cons);
	    	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			bge_start_locked(ifp);
		BGE_UNLOCK(sc);
	}
}

static void
bge_intr(void *xsc)
{
	struct bge_softc *sc;
	struct ifnet *ifp;
	uint32_t statusword;
	uint16_t rx_prod, tx_cons;

	sc = xsc;

	BGE_LOCK(sc);

	ifp = sc->bge_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		BGE_UNLOCK(sc);
		return;
	}
#endif

	/*
	 * Ack the interrupt by writing something to BGE_MBX_IRQ0_LO.  Don't
	 * disable interrupts by writing nonzero like we used to, since with
	 * our current organization this just gives complications and
	 * pessimizations for re-enabling interrupts.  We used to have races
	 * instead of the necessary complications.  Disabling interrupts
	 * would just reduce the chance of a status update while we are
	 * running (by switching to the interrupt-mode coalescence
	 * parameters), but this chance is already very low so it is more
	 * efficient to get another interrupt than prevent it.
	 *
	 * We do the ack first to ensure another interrupt if there is a
	 * status update after the ack.  We don't check for the status
	 * changing later because it is more efficient to get another
	 * interrupt than prevent it, not quite as above (not checking is
	 * a smaller optimization than not toggling the interrupt enable,
	 * since checking doesn't involve PCI accesses and toggling require
	 * the status check).  So toggling would probably be a pessimization
	 * even with MSI.  It would only be needed for using a task queue.
	 */
	bge_writembx(sc, BGE_MBX_IRQ0_LO, 0);

	/*
	 * Do the mandatory PCI flush as well as get the link status.
	 */
	statusword = CSR_READ_4(sc, BGE_MAC_STS) & BGE_MACSTAT_LINK_CHANGED;

	/* Make sure the descriptor ring indexes are coherent. */
	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	rx_prod = sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx;
	tx_cons = sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx;
	sc->bge_ldata.bge_status_block->bge_status = 0;
	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if ((sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5700_B2) ||
	    statusword || sc->bge_link_evt)
		bge_link_upd(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/* Check RX return ring producer/consumer. */
		bge_rxeof(sc, rx_prod, 1);
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/* Check TX ring producer/consumer. */
		bge_txeof(sc, tx_cons);
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bge_start_locked(ifp);

	BGE_UNLOCK(sc);
}

static void
bge_asf_driver_up(struct bge_softc *sc)
{
	if (sc->bge_asf_mode & ASF_STACKUP) {
		/* Send ASF heartbeat aprox. every 2s */
		if (sc->bge_asf_count)
			sc->bge_asf_count --;
		else {
			sc->bge_asf_count = 5;
			bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM_FW,
			    BGE_FW_DRV_ALIVE);
			bge_writemem_ind(sc, BGE_SOFTWARE_GENNCOMM_FW_LEN, 4);
			bge_writemem_ind(sc, BGE_SOFTWARE_GENNCOMM_FW_DATA, 3);
			CSR_WRITE_4(sc, BGE_CPU_EVENT,
			    CSR_READ_4(sc, BGE_CPU_EVENT) | (1 << 14));
		}
	}
}

static void
bge_tick(void *xsc)
{
	struct bge_softc *sc = xsc;
	struct mii_data *mii = NULL;

	BGE_LOCK_ASSERT(sc);

	/* Synchronize with possible callout reset/stop. */
	if (callout_pending(&sc->bge_stat_ch) ||
	    !callout_active(&sc->bge_stat_ch))
	    	return;

	if (BGE_IS_5705_PLUS(sc))
		bge_stats_update_regs(sc);
	else
		bge_stats_update(sc);

	if ((sc->bge_flags & BGE_FLAG_TBI) == 0) {
		mii = device_get_softc(sc->bge_miibus);
		/*
		 * Do not touch PHY if we have link up. This could break
		 * IPMI/ASF mode or produce extra input errors
		 * (extra errors was reported for bcm5701 & bcm5704).
		 */
		if (!sc->bge_link)
			mii_tick(mii);
	} else {
		/*
		 * Since in TBI mode auto-polling can't be used we should poll
		 * link status manually. Here we register pending link event
		 * and trigger interrupt.
		 */
#ifdef DEVICE_POLLING
		/* In polling mode we poll link state in bge_poll(). */
		if (!(sc->bge_ifp->if_capenable & IFCAP_POLLING))
#endif
		{
		sc->bge_link_evt++;
		if (sc->bge_asicrev == BGE_ASICREV_BCM5700 ||
		    sc->bge_flags & BGE_FLAG_5788)
			BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
		else
			BGE_SETBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_COAL_NOW);
		}
	}

	bge_asf_driver_up(sc);
	bge_watchdog(sc);

	callout_reset(&sc->bge_stat_ch, hz, bge_tick, sc);
}

static void
bge_stats_update_regs(struct bge_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->bge_ifp;

	ifp->if_collisions += CSR_READ_4(sc, BGE_MAC_STATS +
	    offsetof(struct bge_mac_stats_regs, etherStatsCollisions));

	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_OUT_OF_BDS);
	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_DROPS);
	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_ERRORS);
}

static void
bge_stats_update(struct bge_softc *sc)
{
	struct ifnet *ifp;
	bus_size_t stats;
	uint32_t cnt;	/* current register value */

	ifp = sc->bge_ifp;

	stats = BGE_MEMWIN_START + BGE_STATS_BLOCK;

#define	READ_STAT(sc, stats, stat) \
	CSR_READ_4(sc, stats + offsetof(struct bge_stats, stat))

	cnt = READ_STAT(sc, stats, txstats.etherStatsCollisions.bge_addr_lo);
	ifp->if_collisions += (uint32_t)(cnt - sc->bge_tx_collisions);
	sc->bge_tx_collisions = cnt;

	cnt = READ_STAT(sc, stats, ifInDiscards.bge_addr_lo);
	ifp->if_ierrors += (uint32_t)(cnt - sc->bge_rx_discards);
	sc->bge_rx_discards = cnt;

	cnt = READ_STAT(sc, stats, txstats.ifOutDiscards.bge_addr_lo);
	ifp->if_oerrors += (uint32_t)(cnt - sc->bge_tx_discards);
	sc->bge_tx_discards = cnt;

#undef	READ_STAT
}

/*
 * Pad outbound frame to ETHER_MIN_NOPAD for an unusual reason.
 * The bge hardware will pad out Tx runts to ETHER_MIN_NOPAD,
 * but when such padded frames employ the bge IP/TCP checksum offload,
 * the hardware checksum assist gives incorrect results (possibly
 * from incorporating its own padding into the UDP/TCP checksum; who knows).
 * If we pad such runts with zeros, the onboard checksum comes out correct.
 */
static __inline int
bge_cksum_pad(struct mbuf *m)
{
	int padlen = ETHER_MIN_NOPAD - m->m_pkthdr.len;
	struct mbuf *last;

	/* If there's only the packet-header and we can pad there, use it. */
	if (m->m_pkthdr.len == m->m_len && M_WRITABLE(m) &&
	    M_TRAILINGSPACE(m) >= padlen) {
		last = m;
	} else {
		/*
		 * Walk packet chain to find last mbuf. We will either
		 * pad there, or append a new mbuf and pad it.
		 */
		for (last = m; last->m_next != NULL; last = last->m_next);
		if (!(M_WRITABLE(last) && M_TRAILINGSPACE(last) >= padlen)) {
			/* Allocate new empty mbuf, pad it. Compact later. */
			struct mbuf *n;

			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == NULL)
				return (ENOBUFS);
			n->m_len = 0;
			last->m_next = n;
			last = n;
		}
	}

	/* Now zero the pad area, to avoid the bge cksum-assist bug. */
	memset(mtod(last, caddr_t) + last->m_len, 0, padlen);
	last->m_len += padlen;
	m->m_pkthdr.len += padlen;

	return (0);
}

/*
 * Encapsulate an mbuf chain in the tx ring  by coupling the mbuf data
 * pointers to descriptors.
 */
static int
bge_encap(struct bge_softc *sc, struct mbuf **m_head, uint32_t *txidx)
{
	bus_dma_segment_t	segs[BGE_NSEG_NEW];
	bus_dmamap_t		map;
	struct bge_tx_bd	*d;
	struct mbuf		*m = *m_head;
	uint32_t		idx = *txidx;
	uint16_t		csum_flags;
	int			nsegs, i, error;

	csum_flags = 0;
	if (m->m_pkthdr.csum_flags) {
		if (m->m_pkthdr.csum_flags & CSUM_IP)
			csum_flags |= BGE_TXBDFLAG_IP_CSUM;
		if (m->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP)) {
			csum_flags |= BGE_TXBDFLAG_TCP_UDP_CSUM;
			if (m->m_pkthdr.len < ETHER_MIN_NOPAD &&
			    (error = bge_cksum_pad(m)) != 0) {
				m_freem(m);
				*m_head = NULL;
				return (error);
			}
		}
		if (m->m_flags & M_LASTFRAG)
			csum_flags |= BGE_TXBDFLAG_IP_FRAG_END;
		else if (m->m_flags & M_FRAG)
			csum_flags |= BGE_TXBDFLAG_IP_FRAG;
	}

	map = sc->bge_cdata.bge_tx_dmamap[idx];
	error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_tx_mtag, map, m, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		m = m_collapse(m, M_DONTWAIT, BGE_NSEG_NEW);
		if (m == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;
		error = bus_dmamap_load_mbuf_sg(sc->bge_cdata.bge_tx_mtag, map,
		    m, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error) {
			m_freem(m);
			*m_head = NULL;
			return (error);
		}
	} else if (error != 0)
		return (error);

	/* Check if we have enough free send BDs. */
	if (sc->bge_txcnt + nsegs >= BGE_TX_RING_CNT) {
		bus_dmamap_unload(sc->bge_cdata.bge_tx_mtag, map);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_tx_mtag, map, BUS_DMASYNC_PREWRITE);

	for (i = 0; ; i++) {
		d = &sc->bge_ldata.bge_tx_ring[idx];
		d->bge_addr.bge_addr_lo = BGE_ADDR_LO(segs[i].ds_addr);
		d->bge_addr.bge_addr_hi = BGE_ADDR_HI(segs[i].ds_addr);
		d->bge_len = segs[i].ds_len;
		d->bge_flags = csum_flags;
		if (i == nsegs - 1)
			break;
		BGE_INC(idx, BGE_TX_RING_CNT);
	}

	/* Mark the last segment as end of packet... */
	d->bge_flags |= BGE_TXBDFLAG_END;

	/* ... and put VLAN tag into first segment.  */
	d = &sc->bge_ldata.bge_tx_ring[*txidx];
#if __FreeBSD_version > 700022
	if (m->m_flags & M_VLANTAG) {
		d->bge_flags |= BGE_TXBDFLAG_VLAN_TAG;
		d->bge_vlan_tag = m->m_pkthdr.ether_vtag;
	} else
		d->bge_vlan_tag = 0;
#else
	{
		struct m_tag		*mtag;

		if ((mtag = VLAN_OUTPUT_TAG(sc->bge_ifp, m)) != NULL) {
			d->bge_flags |= BGE_TXBDFLAG_VLAN_TAG;
			d->bge_vlan_tag = VLAN_TAG_VALUE(mtag);
		} else
			d->bge_vlan_tag = 0;
	}
#endif

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.
	 */
	sc->bge_cdata.bge_tx_dmamap[*txidx] = sc->bge_cdata.bge_tx_dmamap[idx];
	sc->bge_cdata.bge_tx_dmamap[idx] = map;
	sc->bge_cdata.bge_tx_chain[idx] = m;
	sc->bge_txcnt += nsegs;

	BGE_INC(idx, BGE_TX_RING_CNT);
	*txidx = idx;

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start_locked(struct ifnet *ifp)
{
	struct bge_softc *sc;
	struct mbuf *m_head;
	uint32_t prodidx;
	int count;

	sc = ifp->if_softc;
	BGE_LOCK_ASSERT(sc);

	if (!sc->bge_link ||
	    (ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	prodidx = sc->bge_tx_prodidx;

	for (count = 0; !IFQ_DRV_IS_EMPTY(&ifp->if_snd);) {
		if (sc->bge_txcnt > BGE_TX_RING_CNT - 16) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * XXX
		 * The code inside the if() block is never reached since we
		 * must mark CSUM_IP_FRAGS in our if_hwassist to start getting
		 * requests to checksum TCP/UDP in a fragmented packet.
		 *
		 * XXX
		 * safety overkill.  If this is a fragmented packet chain
		 * with delayed TCP/UDP checksums, then only encapsulate
		 * it if we have enough descriptors to handle the entire
		 * chain at once.
		 * (paranoia -- may not actually be needed)
		 */
		if (m_head->m_flags & M_FIRSTFRAG &&
		    m_head->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
			if ((BGE_TX_RING_CNT - sc->bge_txcnt) <
			    m_head->m_pkthdr.csum_data + 16) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
		}

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (bge_encap(sc, &m_head, &prodidx)) {
			if (m_head == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		++count;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#ifdef ETHER_BPF_MTAP
		ETHER_BPF_MTAP(ifp, m_head);
#else
		BPF_MTAP(ifp, m_head);
#endif
	}

	if (count > 0) {
		bus_dmamap_sync(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_cdata.bge_tx_ring_map, BUS_DMASYNC_PREWRITE);
		/* Transmit. */
		bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);
		/* 5700 b2 errata */
		if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
			bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);

		sc->bge_tx_prodidx = prodidx;

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		sc->bge_timer = 5;
	}
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start(struct ifnet *ifp)
{
	struct bge_softc *sc;

	sc = ifp->if_softc;
	BGE_LOCK(sc);
	bge_start_locked(ifp);
	BGE_UNLOCK(sc);
}

static void
bge_init_locked(struct bge_softc *sc)
{
	struct ifnet *ifp;
	uint16_t *m;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	/* Cancel pending I/O and flush buffers. */
	bge_stop(sc);

	bge_stop_fw(sc);
	bge_sig_pre_reset(sc, BGE_RESET_START);
	bge_reset(sc);
	bge_sig_legacy(sc, BGE_RESET_START);
	bge_sig_post_reset(sc, BGE_RESET_START);

	bge_chipinit(sc);

	/*
	 * Init the various state machines, ring
	 * control blocks and firmware.
	 */
	if (bge_blockinit(sc)) {
		device_printf(sc->bge_dev, "initialization failure\n");
		return;
	}

	ifp = sc->bge_ifp;

	/* Specify MTU. */
	CSR_WRITE_4(sc, BGE_RX_MTU, ifp->if_mtu +
	    ETHER_HDR_LEN + ETHER_CRC_LEN +
	    (ifp->if_capenable & IFCAP_VLAN_MTU ? ETHER_VLAN_ENCAP_LEN : 0));

	/* Load our MAC address. */
	m = (uint16_t *)IF_LLADDR(sc->bge_ifp);
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_LO, htons(m[0]));
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_HI, (htons(m[1]) << 16) | htons(m[2]));

	/* Program promiscuous mode. */
	bge_setpromisc(sc);

	/* Program multicast filter. */
	bge_setmulti(sc);

	/* Program VLAN tag stripping. */
	bge_setvlan(sc);

	/* Init RX ring. */
	if (bge_init_rx_ring_std(sc) != 0) {
		device_printf(sc->bge_dev, "no memory for std Rx buffers.\n");
		bge_stop(sc);
		return;
	}

	/*
	 * Workaround for a bug in 5705 ASIC rev A0. Poll the NIC's
	 * memory to insure that the chip has in fact read the first
	 * entry of the ring.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5705_A0) {
		uint32_t		v, i;
		for (i = 0; i < 10; i++) {
			DELAY(20);
			v = bge_readmem_ind(sc, BGE_STD_RX_RINGS + 8);
			if (v == (MCLBYTES - ETHER_ALIGN))
				break;
		}
		if (i == 10)
			device_printf (sc->bge_dev,
			    "5705 A0 chip failed to load RX ring\n");
	}

	/* Init jumbo RX ring. */
	if (ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN >
	    (MCLBYTES - ETHER_ALIGN)) {
		if (bge_init_rx_ring_jumbo(sc) != 0) {
			device_printf(sc->bge_dev, "no memory for std Rx buffers.\n");
			bge_stop(sc);
			return;
		}
	}

	/* Init our RX return ring index. */
	sc->bge_rx_saved_considx = 0;

	/* Init our RX/TX stat counters. */
	sc->bge_rx_discards = sc->bge_tx_discards = sc->bge_tx_collisions = 0;

	/* Init TX ring. */
	bge_init_tx_ring(sc);

	/* Turn on transmitter. */
	BGE_SETBIT(sc, BGE_TX_MODE, BGE_TXMODE_ENABLE);

	/* Turn on receiver. */
	BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);

	/* Tell firmware we're alive. */
	BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

#ifdef DEVICE_POLLING
	/* Disable interrupts if we are polling. */
	if (ifp->if_capenable & IFCAP_POLLING) {
		BGE_SETBIT(sc, BGE_PCI_MISC_CTL,
		    BGE_PCIMISCCTL_MASK_PCI_INTR);
		bge_writembx(sc, BGE_MBX_IRQ0_LO, 1);
	} else
#endif

	/* Enable host interrupts. */
	{
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_CLEAR_INTA);
	BGE_CLRBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	bge_writembx(sc, BGE_MBX_IRQ0_LO, 0);
	}

	bge_ifmedia_upd_locked(ifp);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->bge_stat_ch, hz, bge_tick, sc);
}

static void
bge_init(void *xsc)
{
	struct bge_softc *sc = xsc;

	BGE_LOCK(sc);
	bge_init_locked(sc);
	BGE_UNLOCK(sc);
}

/*
 * Set media options.
 */
static int
bge_ifmedia_upd(struct ifnet *ifp)
{
	struct bge_softc *sc = ifp->if_softc;
	int res;

	BGE_LOCK(sc);
	res = bge_ifmedia_upd_locked(ifp);
	BGE_UNLOCK(sc);

	return (res);
}

static int
bge_ifmedia_upd_locked(struct ifnet *ifp)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	struct mii_softc *miisc;
	struct ifmedia *ifm;

	BGE_LOCK_ASSERT(sc);

	ifm = &sc->bge_ifmedia;

	/* If this is a 1000baseX NIC, enable the TBI port. */
	if (sc->bge_flags & BGE_FLAG_TBI) {
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return (EINVAL);
		switch(IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
			/*
			 * The BCM5704 ASIC appears to have a special
			 * mechanism for programming the autoneg
			 * advertisement registers in TBI mode.
			 */
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704) {
				uint32_t sgdig;
				sgdig = CSR_READ_4(sc, BGE_SGDIG_STS);
				if (sgdig & BGE_SGDIGSTS_DONE) {
					CSR_WRITE_4(sc, BGE_TX_TBI_AUTONEG, 0);
					sgdig = CSR_READ_4(sc, BGE_SGDIG_CFG);
					sgdig |= BGE_SGDIGCFG_AUTO |
					    BGE_SGDIGCFG_PAUSE_CAP |
					    BGE_SGDIGCFG_ASYM_PAUSE;
					CSR_WRITE_4(sc, BGE_SGDIG_CFG,
					    sgdig | BGE_SGDIGCFG_SEND);
					DELAY(5);
					CSR_WRITE_4(sc, BGE_SGDIG_CFG, sgdig);
				}
			}
			break;
		case IFM_1000_SX:
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
				BGE_CLRBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			} else {
				BGE_SETBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			}
			break;
		default:
			return (EINVAL);
		}
		return (0);
	}

	sc->bge_link_evt++;
	mii = device_get_softc(sc->bge_miibus);
	if (mii->mii_instance)
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	mii_mediachg(mii);

	/*
	 * Force an interrupt so that we will call bge_link_upd
	 * if needed and clear any pending link state attention.
	 * Without this we are not getting any further interrupts
	 * for link state changes and thus will not UP the link and
	 * not be able to send in bge_start_locked. The only
	 * way to get things working was to receive a packet and
	 * get an RX intr.
	 * bge_tick should help for fiber cards and we might not
	 * need to do this here if BGE_FLAG_TBI is set but as
	 * we poll for fiber anyway it should not harm.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5700 ||
	    sc->bge_flags & BGE_FLAG_5788)
		BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
	else
		BGE_SETBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_COAL_NOW);

	return (0);
}

/*
 * Report current media status.
 */
static void
bge_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	BGE_LOCK(sc);

	if (sc->bge_flags & BGE_FLAG_TBI) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;
		if (CSR_READ_4(sc, BGE_MAC_STS) &
		    BGE_MACSTAT_TBI_PCS_SYNCHED)
			ifmr->ifm_status |= IFM_ACTIVE;
		else {
			ifmr->ifm_active |= IFM_NONE;
			BGE_UNLOCK(sc);
			return;
		}
		ifmr->ifm_active |= IFM_1000_SX;
		if (CSR_READ_4(sc, BGE_MAC_MODE) & BGE_MACMODE_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
		else
			ifmr->ifm_active |= IFM_FDX;
		BGE_UNLOCK(sc);
		return;
	}

	mii = device_get_softc(sc->bge_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	BGE_UNLOCK(sc);
}

static int
bge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct bge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct mii_data *mii;
	int flags, mask, error = 0;

	switch (command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN ||
		    ((BGE_IS_JUMBO_CAPABLE(sc)) &&
		    ifr->ifr_mtu > BGE_JUMBO_MTU) ||
		    ((!BGE_IS_JUMBO_CAPABLE(sc)) &&
		    ifr->ifr_mtu > ETHERMTU))
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			bge_init(sc);
		}
		break;
	case SIOCSIFFLAGS:
		BGE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.  Similarly for ALLMULTI.
			 */
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				flags = ifp->if_flags ^ sc->bge_if_flags;
				if (flags & IFF_PROMISC)
					bge_setpromisc(sc);
				if (flags & IFF_ALLMULTI)
					bge_setmulti(sc);
			} else
				bge_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				bge_stop(sc);
			}
		}
		sc->bge_if_flags = ifp->if_flags;
		BGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			BGE_LOCK(sc);
			bge_setmulti(sc);
			BGE_UNLOCK(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->bge_flags & BGE_FLAG_TBI) {
			error = ifmedia_ioctl(ifp, ifr,
			    &sc->bge_ifmedia, command);
		} else {
			mii = device_get_softc(sc->bge_miibus);
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		}
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(bge_poll, ifp);
				if (error)
					return (error);
				BGE_LOCK(sc);
				BGE_SETBIT(sc, BGE_PCI_MISC_CTL,
				    BGE_PCIMISCCTL_MASK_PCI_INTR);
				bge_writembx(sc, BGE_MBX_IRQ0_LO, 1);
				ifp->if_capenable |= IFCAP_POLLING;
				BGE_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				BGE_LOCK(sc);
				BGE_CLRBIT(sc, BGE_PCI_MISC_CTL,
				    BGE_PCIMISCCTL_MASK_PCI_INTR);
				bge_writembx(sc, BGE_MBX_IRQ0_LO, 0);
				ifp->if_capenable &= ~IFCAP_POLLING;
				BGE_UNLOCK(sc);
			}
		}
#endif
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if (IFCAP_HWCSUM & ifp->if_capenable &&
			    IFCAP_HWCSUM & ifp->if_capabilities)
				ifp->if_hwassist = BGE_CSUM_FEATURES;
			else
				ifp->if_hwassist = 0;
#ifdef VLAN_CAPABILITIES
			VLAN_CAPABILITIES(ifp);
#endif
		}

		if (mask & IFCAP_VLAN_MTU) {
			ifp->if_capenable ^= IFCAP_VLAN_MTU;
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			bge_init(sc);
		}

		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			BGE_LOCK(sc);
			bge_setvlan(sc);
			BGE_UNLOCK(sc);
#ifdef VLAN_CAPABILITIES
			VLAN_CAPABILITIES(ifp);
#endif
		}

		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
bge_watchdog(struct bge_softc *sc)
{
	struct ifnet *ifp;

	BGE_LOCK_ASSERT(sc);

	if (sc->bge_timer == 0 || --sc->bge_timer)
		return;

	ifp = sc->bge_ifp;

	if_printf(ifp, "watchdog timeout -- resetting\n");

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	bge_init_locked(sc);

	ifp->if_oerrors++;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
bge_stop(struct bge_softc *sc)
{
	struct ifnet *ifp;
	struct ifmedia_entry *ifm;
	struct mii_data *mii = NULL;
	int mtmp, itmp;

	BGE_LOCK_ASSERT(sc);

	ifp = sc->bge_ifp;

	if ((sc->bge_flags & BGE_FLAG_TBI) == 0)
		mii = device_get_softc(sc->bge_miibus);

	callout_stop(&sc->bge_stat_ch);

	/* Disable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	bge_writembx(sc, BGE_MBX_IRQ0_LO, 1);

	/*
	 * Tell firmware we're shutting down.
	 */
	bge_stop_fw(sc);
	bge_sig_pre_reset(sc, BGE_RESET_STOP);

	/*
	 * Disable all of the receiver blocks.
	 */
	BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);
	if (!(BGE_IS_5705_PLUS(sc)))
		BGE_CLRBIT(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDBDI_MODE, BGE_RBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE);

	/*
	 * Disable all of the transmit blocks.
	 */
	BGE_CLRBIT(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	if (!(BGE_IS_5705_PLUS(sc)))
		BGE_CLRBIT(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/*
	 * Shut down all of the memory managers and related
	 * state machines.
	 */
	BGE_CLRBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE);
	if (!(BGE_IS_5705_PLUS(sc)))
		BGE_CLRBIT(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);
	if (!(BGE_IS_5705_PLUS(sc))) {
		BGE_CLRBIT(sc, BGE_BMAN_MODE, BGE_BMANMODE_ENABLE);
		BGE_CLRBIT(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);
	}

	bge_reset(sc);
	bge_sig_legacy(sc, BGE_RESET_STOP);
	bge_sig_post_reset(sc, BGE_RESET_STOP);

	/*
	 * Keep the ASF firmware running if up.
	 */
	if (sc->bge_asf_mode & ASF_STACKUP)
		BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);
	else
		BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Free the RX lists. */
	bge_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	if (BGE_IS_JUMBO_CAPABLE(sc))
		bge_free_rx_ring_jumbo(sc);

	/* Free TX buffers. */
	bge_free_tx_ring(sc);

	/*
	 * Isolate/power down the PHY, but leave the media selection
	 * unchanged so that things will be put back to normal when
	 * we bring the interface back up.
	 */
	if ((sc->bge_flags & BGE_FLAG_TBI) == 0) {
		itmp = ifp->if_flags;
		ifp->if_flags |= IFF_UP;
		/*
		 * If we are called from bge_detach(), mii is already NULL.
		 */
		if (mii != NULL) {
			ifm = mii->mii_media.ifm_cur;
			mtmp = ifm->ifm_media;
			ifm->ifm_media = IFM_ETHER | IFM_NONE;
			mii_mediachg(mii);
			ifm->ifm_media = mtmp;
		}
		ifp->if_flags = itmp;
	}

	sc->bge_tx_saved_considx = BGE_TXCONS_UNSET;

	/* Clear MAC's link state (PHY may still have link UP). */
	if (bootverbose && sc->bge_link)
		if_printf(sc->bge_ifp, "link DOWN\n");
	sc->bge_link = 0;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
bge_shutdown(device_t dev)
{
	struct bge_softc *sc;

	sc = device_get_softc(dev);
	BGE_LOCK(sc);
	bge_stop(sc);
	bge_reset(sc);
	BGE_UNLOCK(sc);

	return (0);
}

static int
bge_suspend(device_t dev)
{
	struct bge_softc *sc;

	sc = device_get_softc(dev);
	BGE_LOCK(sc);
	bge_stop(sc);
	BGE_UNLOCK(sc);

	return (0);
}

static int
bge_resume(device_t dev)
{
	struct bge_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	BGE_LOCK(sc);
	ifp = sc->bge_ifp;
	if (ifp->if_flags & IFF_UP) {
		bge_init_locked(sc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			bge_start_locked(ifp);
	}
	BGE_UNLOCK(sc);

	return (0);
}

static void
bge_link_upd(struct bge_softc *sc)
{
	struct mii_data *mii;
	uint32_t link, status;

	BGE_LOCK_ASSERT(sc);

	/* Clear 'pending link event' flag. */
	sc->bge_link_evt = 0;

	/*
	 * Process link state changes.
	 * Grrr. The link status word in the status block does
	 * not work correctly on the BCM5700 rev AX and BX chips,
	 * according to all available information. Hence, we have
	 * to enable MII interrupts in order to properly obtain
	 * async link changes. Unfortunately, this also means that
	 * we have to read the MAC status register to detect link
	 * changes, thereby adding an additional register access to
	 * the interrupt handler.
	 *
	 * XXX: perhaps link state detection procedure used for
	 * BGE_CHIPID_BCM5700_B2 can be used for others BCM5700 revisions.
	 */

	if (sc->bge_asicrev == BGE_ASICREV_BCM5700 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5700_B2) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_MI_INTERRUPT) {
			mii = device_get_softc(sc->bge_miibus);
			mii_pollstat(mii);
			if (!sc->bge_link &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
				sc->bge_link++;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link UP\n");
			} else if (sc->bge_link &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE)) {
				sc->bge_link = 0;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link DOWN\n");
			}

			/* Clear the interrupt. */
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
			bge_miibus_readreg(sc->bge_dev, 1, BRGPHY_MII_ISR);
			bge_miibus_writereg(sc->bge_dev, 1, BRGPHY_MII_IMR,
			    BRGPHY_INTRS);
		}
		return;
	}

	if (sc->bge_flags & BGE_FLAG_TBI) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_TBI_PCS_SYNCHED) {
			if (!sc->bge_link) {
				sc->bge_link++;
				if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
					BGE_CLRBIT(sc, BGE_MAC_MODE,
					    BGE_MACMODE_TBI_SEND_CFGS);
				CSR_WRITE_4(sc, BGE_MAC_STS, 0xFFFFFFFF);
				if (bootverbose)
					if_printf(sc->bge_ifp, "link UP\n");
				if_link_state_change(sc->bge_ifp,
				    LINK_STATE_UP);
			}
		} else if (sc->bge_link) {
			sc->bge_link = 0;
			if (bootverbose)
				if_printf(sc->bge_ifp, "link DOWN\n");
			if_link_state_change(sc->bge_ifp, LINK_STATE_DOWN);
		}
	} else if (CSR_READ_4(sc, BGE_MI_MODE) & BGE_MIMODE_AUTOPOLL) {
		/*
		 * Some broken BCM chips have BGE_STATFLAG_LINKSTATE_CHANGED bit
		 * in status word always set. Workaround this bug by reading
		 * PHY link status directly.
		 */
		link = (CSR_READ_4(sc, BGE_MI_STS) & BGE_MISTS_LINK) ? 1 : 0;

		if (link != sc->bge_link ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5700) {
			mii = device_get_softc(sc->bge_miibus);
			mii_pollstat(mii);
			if (!sc->bge_link &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
				sc->bge_link++;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link UP\n");
			} else if (sc->bge_link &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE)) {
				sc->bge_link = 0;
				if (bootverbose)
					if_printf(sc->bge_ifp, "link DOWN\n");
			}
		}
	} else {
		/*
		 * Discard link events for MII/GMII controllers
		 * if MI auto-polling is disabled.
		 */
	}

	/* Clear the attention. */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED |
	    BGE_MACSTAT_CFG_CHANGED | BGE_MACSTAT_MI_COMPLETE |
	    BGE_MACSTAT_LINK_CHANGED);
}

#define BGE_SYSCTL_STAT(sc, ctx, desc, parent, node, oid) \
	SYSCTL_ADD_PROC(ctx, parent, OID_AUTO, oid, CTLTYPE_UINT|CTLFLAG_RD, \
	    sc, offsetof(struct bge_stats, node), bge_sysctl_stats, "IU", \
	    desc)

static void
bge_add_sysctls(struct bge_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children, *schildren;
	struct sysctl_oid *tree;

	ctx = device_get_sysctl_ctx(sc->bge_dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->bge_dev));

#ifdef BGE_REGISTER_DEBUG
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "debug_info",
	    CTLTYPE_INT | CTLFLAG_RW, sc, 0, bge_sysctl_debug_info, "I",
	    "Debug Information");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "reg_read",
	    CTLTYPE_INT | CTLFLAG_RW, sc, 0, bge_sysctl_reg_read, "I",
	    "Register Read");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "mem_read",
	    CTLTYPE_INT | CTLFLAG_RW, sc, 0, bge_sysctl_mem_read, "I",
	    "Memory Read");

#endif

	if (BGE_IS_5705_PLUS(sc))
		return;

	tree = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "stats", CTLFLAG_RD,
	    NULL, "BGE Statistics");
	schildren = children = SYSCTL_CHILDREN(tree);
	BGE_SYSCTL_STAT(sc, ctx, "Frames Dropped Due To Filters",
	    children, COSFramesDroppedDueToFilters,
	    "FramesDroppedDueToFilters");
	BGE_SYSCTL_STAT(sc, ctx, "NIC DMA Write Queue Full",
	    children, nicDmaWriteQueueFull, "DmaWriteQueueFull");
	BGE_SYSCTL_STAT(sc, ctx, "NIC DMA Write High Priority Queue Full",
	    children, nicDmaWriteHighPriQueueFull, "DmaWriteHighPriQueueFull");
	BGE_SYSCTL_STAT(sc, ctx, "NIC No More RX Buffer Descriptors",
	    children, nicNoMoreRxBDs, "NoMoreRxBDs");
	BGE_SYSCTL_STAT(sc, ctx, "Discarded Input Frames",
	    children, ifInDiscards, "InputDiscards");
	BGE_SYSCTL_STAT(sc, ctx, "Input Errors",
	    children, ifInErrors, "InputErrors");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Recv Threshold Hit",
	    children, nicRecvThresholdHit, "RecvThresholdHit");
	BGE_SYSCTL_STAT(sc, ctx, "NIC DMA Read Queue Full",
	    children, nicDmaReadQueueFull, "DmaReadQueueFull");
	BGE_SYSCTL_STAT(sc, ctx, "NIC DMA Read High Priority Queue Full",
	    children, nicDmaReadHighPriQueueFull, "DmaReadHighPriQueueFull");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Send Data Complete Queue Full",
	    children, nicSendDataCompQueueFull, "SendDataCompQueueFull");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Ring Set Send Producer Index",
	    children, nicRingSetSendProdIndex, "RingSetSendProdIndex");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Ring Status Update",
	    children, nicRingStatusUpdate, "RingStatusUpdate");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Interrupts",
	    children, nicInterrupts, "Interrupts");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Avoided Interrupts",
	    children, nicAvoidedInterrupts, "AvoidedInterrupts");
	BGE_SYSCTL_STAT(sc, ctx, "NIC Send Threshold Hit",
	    children, nicSendThresholdHit, "SendThresholdHit");

	tree = SYSCTL_ADD_NODE(ctx, schildren, OID_AUTO, "rx", CTLFLAG_RD,
	    NULL, "BGE RX Statistics");
	children = SYSCTL_CHILDREN(tree);
	BGE_SYSCTL_STAT(sc, ctx, "Inbound Octets",
	    children, rxstats.ifHCInOctets, "Octets");
	BGE_SYSCTL_STAT(sc, ctx, "Fragments",
	    children, rxstats.etherStatsFragments, "Fragments");
	BGE_SYSCTL_STAT(sc, ctx, "Inbound Unicast Packets",
	    children, rxstats.ifHCInUcastPkts, "UcastPkts");
	BGE_SYSCTL_STAT(sc, ctx, "Inbound Multicast Packets",
	    children, rxstats.ifHCInMulticastPkts, "MulticastPkts");
	BGE_SYSCTL_STAT(sc, ctx, "FCS Errors",
	    children, rxstats.dot3StatsFCSErrors, "FCSErrors");
	BGE_SYSCTL_STAT(sc, ctx, "Alignment Errors",
	    children, rxstats.dot3StatsAlignmentErrors, "AlignmentErrors");
	BGE_SYSCTL_STAT(sc, ctx, "XON Pause Frames Received",
	    children, rxstats.xonPauseFramesReceived, "xonPauseFramesReceived");
	BGE_SYSCTL_STAT(sc, ctx, "XOFF Pause Frames Received",
	    children, rxstats.xoffPauseFramesReceived,
	    "xoffPauseFramesReceived");
	BGE_SYSCTL_STAT(sc, ctx, "MAC Control Frames Received",
	    children, rxstats.macControlFramesReceived,
	    "ControlFramesReceived");
	BGE_SYSCTL_STAT(sc, ctx, "XOFF State Entered",
	    children, rxstats.xoffStateEntered, "xoffStateEntered");
	BGE_SYSCTL_STAT(sc, ctx, "Frames Too Long",
	    children, rxstats.dot3StatsFramesTooLong, "FramesTooLong");
	BGE_SYSCTL_STAT(sc, ctx, "Jabbers",
	    children, rxstats.etherStatsJabbers, "Jabbers");
	BGE_SYSCTL_STAT(sc, ctx, "Undersized Packets",
	    children, rxstats.etherStatsUndersizePkts, "UndersizePkts");
	BGE_SYSCTL_STAT(sc, ctx, "Inbound Range Length Errors",
	    children, rxstats.inRangeLengthError, "inRangeLengthError");
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Range Length Errors",
	    children, rxstats.outRangeLengthError, "outRangeLengthError");

	tree = SYSCTL_ADD_NODE(ctx, schildren, OID_AUTO, "tx", CTLFLAG_RD,
	    NULL, "BGE TX Statistics");
	children = SYSCTL_CHILDREN(tree);
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Octets",
	    children, txstats.ifHCOutOctets, "Octets");
	BGE_SYSCTL_STAT(sc, ctx, "TX Collisions",
	    children, txstats.etherStatsCollisions, "Collisions");
	BGE_SYSCTL_STAT(sc, ctx, "XON Sent",
	    children, txstats.outXonSent, "XonSent");
	BGE_SYSCTL_STAT(sc, ctx, "XOFF Sent",
	    children, txstats.outXoffSent, "XoffSent");
	BGE_SYSCTL_STAT(sc, ctx, "Flow Control Done",
	    children, txstats.flowControlDone, "flowControlDone");
	BGE_SYSCTL_STAT(sc, ctx, "Internal MAC TX errors",
	    children, txstats.dot3StatsInternalMacTransmitErrors,
	    "InternalMacTransmitErrors");
	BGE_SYSCTL_STAT(sc, ctx, "Single Collision Frames",
	    children, txstats.dot3StatsSingleCollisionFrames,
	    "SingleCollisionFrames");
	BGE_SYSCTL_STAT(sc, ctx, "Multiple Collision Frames",
	    children, txstats.dot3StatsMultipleCollisionFrames,
	    "MultipleCollisionFrames");
	BGE_SYSCTL_STAT(sc, ctx, "Deferred Transmissions",
	    children, txstats.dot3StatsDeferredTransmissions,
	    "DeferredTransmissions");
	BGE_SYSCTL_STAT(sc, ctx, "Excessive Collisions",
	    children, txstats.dot3StatsExcessiveCollisions,
	    "ExcessiveCollisions");
	BGE_SYSCTL_STAT(sc, ctx, "Late Collisions",
	    children, txstats.dot3StatsLateCollisions,
	    "LateCollisions");
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Unicast Packets",
	    children, txstats.ifHCOutUcastPkts, "UcastPkts");
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Multicast Packets",
	    children, txstats.ifHCOutMulticastPkts, "MulticastPkts");
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Broadcast Packets",
	    children, txstats.ifHCOutBroadcastPkts, "BroadcastPkts");
	BGE_SYSCTL_STAT(sc, ctx, "Carrier Sense Errors",
	    children, txstats.dot3StatsCarrierSenseErrors,
	    "CarrierSenseErrors");
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Discards",
	    children, txstats.ifOutDiscards, "Discards");
	BGE_SYSCTL_STAT(sc, ctx, "Outbound Errors",
	    children, txstats.ifOutErrors, "Errors");
}

static int
bge_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct bge_softc *sc;
	uint32_t result;
	int offset;

	sc = (struct bge_softc *)arg1;
	offset = arg2;
	result = CSR_READ_4(sc, BGE_MEMWIN_START + BGE_STATS_BLOCK + offset +
	    offsetof(bge_hostaddr, bge_addr_lo));
	return (sysctl_handle_int(oidp, &result, 0, req));
}

#ifdef BGE_REGISTER_DEBUG
static int
bge_sysctl_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct bge_softc *sc;
	uint16_t *sbdata;
	int error;
	int result;
	int i, j;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || (req->newptr == NULL))
		return (error);

	if (result == 1) {
		sc = (struct bge_softc *)arg1;

		sbdata = (uint16_t *)sc->bge_ldata.bge_status_block;
		printf("Status Block:\n");
		for (i = 0x0; i < (BGE_STATUS_BLK_SZ / 4); ) {
			printf("%06x:", i);
			for (j = 0; j < 8; j++) {
				printf(" %04x", sbdata[i]);
				i += 4;
			}
			printf("\n");
		}

		printf("Registers:\n");
		for (i = 0x800; i < 0xA00; ) {
			printf("%06x:", i);
			for (j = 0; j < 8; j++) {
				printf(" %08x", CSR_READ_4(sc, i));
				i += 4;
			}
			printf("\n");
		}

		printf("Hardware Flags:\n");
		if (BGE_IS_5755_PLUS(sc))
			printf(" - 5755 Plus\n");
		if (BGE_IS_575X_PLUS(sc))
			printf(" - 575X Plus\n");
		if (BGE_IS_5705_PLUS(sc))
			printf(" - 5705 Plus\n");
		if (BGE_IS_5714_FAMILY(sc))
			printf(" - 5714 Family\n");
		if (BGE_IS_5700_FAMILY(sc))
			printf(" - 5700 Family\n");
		if (sc->bge_flags & BGE_FLAG_JUMBO)
			printf(" - Supports Jumbo Frames\n");
		if (sc->bge_flags & BGE_FLAG_PCIX)
			printf(" - PCI-X Bus\n");
		if (sc->bge_flags & BGE_FLAG_PCIE)
			printf(" - PCI Express Bus\n");
		if (sc->bge_flags & BGE_FLAG_NO_3LED)
			printf(" - No 3 LEDs\n");
		if (sc->bge_flags & BGE_FLAG_RX_ALIGNBUG)
			printf(" - RX Alignment Bug\n");
	}

	return (error);
}

static int
bge_sysctl_reg_read(SYSCTL_HANDLER_ARGS)
{
	struct bge_softc *sc;
	int error;
	uint16_t result;
	uint32_t val;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || (req->newptr == NULL))
		return (error);

	if (result < 0x8000) {
		sc = (struct bge_softc *)arg1;
		val = CSR_READ_4(sc, result);
		printf("reg 0x%06X = 0x%08X\n", result, val);
	}

	return (error);
}

static int
bge_sysctl_mem_read(SYSCTL_HANDLER_ARGS)
{
	struct bge_softc *sc;
	int error;
	uint16_t result;
	uint32_t val;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);
	if (error || (req->newptr == NULL))
		return (error);

	if (result < 0x8000) {
		sc = (struct bge_softc *)arg1;
		val = bge_readmem_ind(sc, result);
		printf("mem 0x%06X = 0x%08X\n", result, val);
	}

	return (error);
}
#endif

static int
bge_get_eaddr_fw(struct bge_softc *sc, uint8_t ether_addr[])
{

	if (sc->bge_flags & BGE_FLAG_EADDR)
		return (1);

#ifdef __sparc64__
	OF_getetheraddr(sc->bge_dev, ether_addr);
	return (0);
#endif
	return (1);
}

static int
bge_get_eaddr_mem(struct bge_softc *sc, uint8_t ether_addr[])
{
	uint32_t mac_addr;

	mac_addr = bge_readmem_ind(sc, 0x0c14);
	if ((mac_addr >> 16) == 0x484b) {
		ether_addr[0] = (uint8_t)(mac_addr >> 8);
		ether_addr[1] = (uint8_t)mac_addr;
		mac_addr = bge_readmem_ind(sc, 0x0c18);
		ether_addr[2] = (uint8_t)(mac_addr >> 24);
		ether_addr[3] = (uint8_t)(mac_addr >> 16);
		ether_addr[4] = (uint8_t)(mac_addr >> 8);
		ether_addr[5] = (uint8_t)mac_addr;
		return (0);
	}
	return (1);
}

static int
bge_get_eaddr_nvram(struct bge_softc *sc, uint8_t ether_addr[])
{
	int mac_offset = BGE_EE_MAC_OFFSET;

	if (sc->bge_asicrev == BGE_ASICREV_BCM5906)
		mac_offset = BGE_EE_MAC_OFFSET_5906;

	return (bge_read_nvram(sc, ether_addr, mac_offset + 2,
	    ETHER_ADDR_LEN));
}

static int
bge_get_eaddr_eeprom(struct bge_softc *sc, uint8_t ether_addr[])
{

	if (sc->bge_asicrev == BGE_ASICREV_BCM5906)
		return (1);

	return (bge_read_eeprom(sc, ether_addr, BGE_EE_MAC_OFFSET + 2,
	   ETHER_ADDR_LEN));
}

static int
bge_get_eaddr(struct bge_softc *sc, uint8_t eaddr[])
{
	static const bge_eaddr_fcn_t bge_eaddr_funcs[] = {
		/* NOTE: Order is critical */
		bge_get_eaddr_fw,
		bge_get_eaddr_mem,
		bge_get_eaddr_nvram,
		bge_get_eaddr_eeprom,
		NULL
	};
	const bge_eaddr_fcn_t *func;

	for (func = bge_eaddr_funcs; *func != NULL; ++func) {
		if ((*func)(sc, eaddr) == 0)
			break;
	}
	return (*func == NULL ? ENXIO : 0);
}
