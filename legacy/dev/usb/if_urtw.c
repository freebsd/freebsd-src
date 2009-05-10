/*-
 * Copyright (c) 2008 Weongyo Jeong <weongyo@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kdb.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/if_urtwreg.h>
#include <dev/usb/if_urtwvar.h>

SYSCTL_NODE(_hw_usb, OID_AUTO, urtw, CTLFLAG_RW, 0, "USB Realtek 8187L");
#ifdef URTW_DEBUG
int urtw_debug = 0;
SYSCTL_INT(_hw_usb_urtw, OID_AUTO, debug, CTLFLAG_RW, &urtw_debug, 0,
    "control debugging printfs");
TUNABLE_INT("hw.usb.urtw.debug", &urtw_debug);
enum {
	URTW_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	URTW_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	URTW_DEBUG_RESET	= 0x00000004,	/* reset processing */
	URTW_DEBUG_TX_PROC	= 0x00000008,	/* tx ISR proc */
	URTW_DEBUG_RX_PROC	= 0x00000010,	/* rx ISR proc */
	URTW_DEBUG_STATE	= 0x00000020,	/* 802.11 state transitions */
	URTW_DEBUG_STAT		= 0x00000040,	/* statistic */
	URTW_DEBUG_ANY		= 0xffffffff
};
#define	DPRINTF(sc, m, fmt, ...) do {				\
	if (sc->sc_debug & (m))					\
		printf(fmt, __VA_ARGS__);			\
} while (0)
#else
#define	DPRINTF(sc, m, fmt, ...) do {				\
	(void) sc;						\
} while (0)
#endif
int urtw_preamble_mode = URTW_PREAMBLE_MODE_LONG;
SYSCTL_INT(_hw_usb_urtw, OID_AUTO, preamble_mode, CTLFLAG_RW,
    &urtw_preamble_mode, 0, "set the preable mode (long or short)");
TUNABLE_INT("hw.usb.urtw.preamble_mode", &urtw_preamble_mode);

/* recognized device vendors/products */
static const struct usb_devno urtw_devs[] = {
#define	URTW_DEV(v,p) { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }
	URTW_DEV(REALTEK, RTL8187),
	URTW_DEV(NETGEAR, WG111V2)
#undef URTW_DEV
};

#define urtw_read8_m(sc, val, data)	do {			\
	error = urtw_read8_c(sc, val, data);			\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_write8_m(sc, val, data)	do {			\
	error = urtw_write8_c(sc, val, data);			\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_read16_m(sc, val, data)	do {			\
	error = urtw_read16_c(sc, val, data);			\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_write16_m(sc, val, data)	do {			\
	error = urtw_write16_c(sc, val, data);			\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_read32_m(sc, val, data)	do {			\
	error = urtw_read32_c(sc, val, data);			\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_write32_m(sc, val, data)	do {			\
	error = urtw_write32_c(sc, val, data);			\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_8187_write_phy_ofdm(sc, val, data)	do {		\
	error = urtw_8187_write_phy_ofdm_c(sc, val, data);	\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_8187_write_phy_cck(sc, val, data)	do {		\
	error = urtw_8187_write_phy_cck_c(sc, val, data);	\
	if (error != 0)						\
		goto fail;					\
} while (0)
#define urtw_8225_write(sc, val, data)	do {			\
	error = urtw_8225_write_c(sc, val, data);		\
	if (error != 0)						\
		goto fail;					\
} while (0)

struct urtw_pair {
	uint32_t	reg;
	uint32_t	val;
};

static uint8_t urtw_8225_agc[] = {
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9d, 0x9c, 0x9b,
	0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
	0x8f, 0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88, 0x87, 0x86, 0x85,
	0x84, 0x83, 0x82, 0x81, 0x80, 0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a,
	0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x30, 0x2f,
	0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24,
	0x23, 0x22, 0x21, 0x20, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19,
	0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f, 0x0e,
	0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
	0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};

static uint32_t urtw_8225_channel[] = {
	0x0000,		/* dummy channel 0  */
	0x085c,		/* 1  */
	0x08dc,		/* 2  */
	0x095c,		/* 3  */
	0x09dc,		/* 4  */
	0x0a5c,		/* 5  */
	0x0adc,		/* 6  */
	0x0b5c,		/* 7  */
	0x0bdc,		/* 8  */
	0x0c5c,		/* 9  */
	0x0cdc,		/* 10  */
	0x0d5c,		/* 11  */
	0x0ddc,		/* 12  */
	0x0e5c,		/* 13  */
	0x0f72,		/* 14  */
};

static uint8_t urtw_8225_gain[] = {
	0x23, 0x88, 0x7c, 0xa5,		/* -82dbm  */
	0x23, 0x88, 0x7c, 0xb5,		/* -82dbm  */
	0x23, 0x88, 0x7c, 0xc5,		/* -82dbm  */
	0x33, 0x80, 0x79, 0xc5,		/* -78dbm  */
	0x43, 0x78, 0x76, 0xc5,		/* -74dbm  */
	0x53, 0x60, 0x73, 0xc5,		/* -70dbm  */
	0x63, 0x58, 0x70, 0xc5,		/* -66dbm  */
};

static struct urtw_pair urtw_8225_rf_part1[] = {
	{ 0x00, 0x0067 }, { 0x01, 0x0fe0 }, { 0x02, 0x044d }, { 0x03, 0x0441 },
	{ 0x04, 0x0486 }, { 0x05, 0x0bc0 }, { 0x06, 0x0ae6 }, { 0x07, 0x082a },
	{ 0x08, 0x001f }, { 0x09, 0x0334 }, { 0x0a, 0x0fd4 }, { 0x0b, 0x0391 },
	{ 0x0c, 0x0050 }, { 0x0d, 0x06db }, { 0x0e, 0x0029 }, { 0x0f, 0x0914 },
};

static struct urtw_pair urtw_8225_rf_part2[] = {
	{ 0x00, 0x01 }, { 0x01, 0x02 }, { 0x02, 0x42 }, { 0x03, 0x00 },
	{ 0x04, 0x00 }, { 0x05, 0x00 }, { 0x06, 0x40 }, { 0x07, 0x00 },
	{ 0x08, 0x40 }, { 0x09, 0xfe }, { 0x0a, 0x09 }, { 0x0b, 0x80 },
	{ 0x0c, 0x01 }, { 0x0e, 0xd3 }, { 0x0f, 0x38 }, { 0x10, 0x84 },
	{ 0x11, 0x06 }, { 0x12, 0x20 }, { 0x13, 0x20 }, { 0x14, 0x00 },
	{ 0x15, 0x40 }, { 0x16, 0x00 }, { 0x17, 0x40 }, { 0x18, 0xef },
	{ 0x19, 0x19 }, { 0x1a, 0x20 }, { 0x1b, 0x76 }, { 0x1c, 0x04 },
	{ 0x1e, 0x95 }, { 0x1f, 0x75 }, { 0x20, 0x1f }, { 0x21, 0x27 },
	{ 0x22, 0x16 }, { 0x24, 0x46 }, { 0x25, 0x20 }, { 0x26, 0x90 },
	{ 0x27, 0x88 }
};

static struct urtw_pair urtw_8225_rf_part3[] = {
	{ 0x00, 0x98 }, { 0x03, 0x20 }, { 0x04, 0x7e }, { 0x05, 0x12 },
	{ 0x06, 0xfc }, { 0x07, 0x78 }, { 0x08, 0x2e }, { 0x10, 0x9b },
	{ 0x11, 0x88 }, { 0x12, 0x47 }, { 0x13, 0xd0 }, { 0x19, 0x00 },
	{ 0x1a, 0xa0 }, { 0x1b, 0x08 }, { 0x40, 0x86 }, { 0x41, 0x8d },
	{ 0x42, 0x15 }, { 0x43, 0x18 }, { 0x44, 0x1f }, { 0x45, 0x1e },
	{ 0x46, 0x1a }, { 0x47, 0x15 }, { 0x48, 0x10 }, { 0x49, 0x0a },
	{ 0x4a, 0x05 }, { 0x4b, 0x02 }, { 0x4c, 0x05 }
};

static uint16_t urtw_8225_rxgain[] = {
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0408, 0x0409,
	0x040a, 0x040b, 0x0502, 0x0503, 0x0504, 0x0505, 0x0540, 0x0541,
	0x0542, 0x0543, 0x0544, 0x0545, 0x0580, 0x0581, 0x0582, 0x0583,
	0x0584, 0x0585, 0x0588, 0x0589, 0x058a, 0x058b, 0x0643, 0x0644,
	0x0645, 0x0680, 0x0681, 0x0682, 0x0683, 0x0684, 0x0685, 0x0688,
	0x0689, 0x068a, 0x068b, 0x068c, 0x0742, 0x0743, 0x0744, 0x0745,
	0x0780, 0x0781, 0x0782, 0x0783, 0x0784, 0x0785, 0x0788, 0x0789,
	0x078a, 0x078b, 0x078c, 0x078d, 0x0790, 0x0791, 0x0792, 0x0793,
	0x0794, 0x0795, 0x0798, 0x0799, 0x079a, 0x079b, 0x079c, 0x079d,
	0x07a0, 0x07a1, 0x07a2, 0x07a3, 0x07a4, 0x07a5, 0x07a8, 0x07a9,
	0x07aa, 0x07ab, 0x07ac, 0x07ad, 0x07b0, 0x07b1, 0x07b2, 0x07b3,
	0x07b4, 0x07b5, 0x07b8, 0x07b9, 0x07ba, 0x07bb, 0x07bb
};

static uint8_t urtw_8225_threshold[] = {
	0x8d, 0x8d, 0x8d, 0x8d, 0x9d, 0xad, 0xbd,
};

static uint8_t urtw_8225_tx_gain_cck_ofdm[] = {
	0x02, 0x06, 0x0e, 0x1e, 0x3e, 0x7e
};

static uint8_t urtw_8225_txpwr_cck[] = {
	0x18, 0x17, 0x15, 0x11, 0x0c, 0x08, 0x04, 0x02,
	0x1b, 0x1a, 0x17, 0x13, 0x0e, 0x09, 0x04, 0x02,
	0x1f, 0x1e, 0x1a, 0x15, 0x10, 0x0a, 0x05, 0x02,
	0x22, 0x21, 0x1d, 0x18, 0x11, 0x0b, 0x06, 0x02,
	0x26, 0x25, 0x21, 0x1b, 0x14, 0x0d, 0x06, 0x03,
	0x2b, 0x2a, 0x25, 0x1e, 0x16, 0x0e, 0x07, 0x03
};

static uint8_t urtw_8225_txpwr_cck_ch14[] = {
	0x18, 0x17, 0x15, 0x0c, 0x00, 0x00, 0x00, 0x00,
	0x1b, 0x1a, 0x17, 0x0e, 0x00, 0x00, 0x00, 0x00,
	0x1f, 0x1e, 0x1a, 0x0f, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x21, 0x1d, 0x11, 0x00, 0x00, 0x00, 0x00,
	0x26, 0x25, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00,
	0x2b, 0x2a, 0x25, 0x15, 0x00, 0x00, 0x00, 0x00
};

static uint8_t urtw_8225_txpwr_ofdm[]={
	0x80, 0x90, 0xa2, 0xb5, 0xcb, 0xe4
};

static uint8_t urtw_8225v2_gain_bg[]={
	0x23, 0x15, 0xa5,		/* -82-1dbm  */
	0x23, 0x15, 0xb5,		/* -82-2dbm  */
	0x23, 0x15, 0xc5,		/* -82-3dbm  */
	0x33, 0x15, 0xc5,		/* -78dbm  */
	0x43, 0x15, 0xc5,		/* -74dbm  */
	0x53, 0x15, 0xc5,		/* -70dbm  */
	0x63, 0x15, 0xc5,		/* -66dbm  */
};

static struct urtw_pair urtw_8225v2_rf_part1[] = {
	{ 0x00, 0x02bf }, { 0x01, 0x0ee0 }, { 0x02, 0x044d }, { 0x03, 0x0441 },
	{ 0x04, 0x08c3 }, { 0x05, 0x0c72 }, { 0x06, 0x00e6 }, { 0x07, 0x082a },
	{ 0x08, 0x003f }, { 0x09, 0x0335 }, { 0x0a, 0x09d4 }, { 0x0b, 0x07bb },
	{ 0x0c, 0x0850 }, { 0x0d, 0x0cdf }, { 0x0e, 0x002b }, { 0x0f, 0x0114 }
};

static struct urtw_pair urtw_8225v2_rf_part2[] = {
	{ 0x00, 0x01 }, { 0x01, 0x02 }, { 0x02, 0x42 }, { 0x03, 0x00 },
	{ 0x04, 0x00 },	{ 0x05, 0x00 }, { 0x06, 0x40 }, { 0x07, 0x00 },
	{ 0x08, 0x40 }, { 0x09, 0xfe }, { 0x0a, 0x08 }, { 0x0b, 0x80 },
	{ 0x0c, 0x01 }, { 0x0d, 0x43 }, { 0x0e, 0xd3 }, { 0x0f, 0x38 },
	{ 0x10, 0x84 }, { 0x11, 0x07 }, { 0x12, 0x20 }, { 0x13, 0x20 },
	{ 0x14, 0x00 }, { 0x15, 0x40 }, { 0x16, 0x00 }, { 0x17, 0x40 },
	{ 0x18, 0xef }, { 0x19, 0x19 }, { 0x1a, 0x20 }, { 0x1b, 0x15 },
	{ 0x1c, 0x04 }, { 0x1d, 0xc5 }, { 0x1e, 0x95 }, { 0x1f, 0x75 },
	{ 0x20, 0x1f }, { 0x21, 0x17 }, { 0x22, 0x16 }, { 0x23, 0x80 },
	{ 0x24, 0x46 }, { 0x25, 0x00 }, { 0x26, 0x90 }, { 0x27, 0x88 }
};

static struct urtw_pair urtw_8225v2_rf_part3[] = {
	{ 0x00, 0x98 }, { 0x03, 0x20 }, { 0x04, 0x7e }, { 0x05, 0x12 },
	{ 0x06, 0xfc }, { 0x07, 0x78 }, { 0x08, 0x2e }, { 0x09, 0x11 },
	{ 0x0a, 0x17 }, { 0x0b, 0x11 }, { 0x10, 0x9b }, { 0x11, 0x88 },
	{ 0x12, 0x47 }, { 0x13, 0xd0 }, { 0x19, 0x00 }, { 0x1a, 0xa0 },
	{ 0x1b, 0x08 }, { 0x1d, 0x00 }, { 0x40, 0x86 }, { 0x41, 0x9d },
	{ 0x42, 0x15 }, { 0x43, 0x18 }, { 0x44, 0x36 }, { 0x45, 0x35 },
	{ 0x46, 0x2e }, { 0x47, 0x25 }, { 0x48, 0x1c }, { 0x49, 0x12 },
	{ 0x4a, 0x09 }, { 0x4b, 0x04 }, { 0x4c, 0x05 }
};

static uint16_t urtw_8225v2_rxgain[] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0008, 0x0009,
	0x000a, 0x000b, 0x0102, 0x0103, 0x0104, 0x0105, 0x0140, 0x0141,
	0x0142, 0x0143, 0x0144, 0x0145, 0x0180, 0x0181, 0x0182, 0x0183,
	0x0184, 0x0185, 0x0188, 0x0189, 0x018a, 0x018b, 0x0243, 0x0244,
	0x0245, 0x0280, 0x0281, 0x0282, 0x0283, 0x0284, 0x0285, 0x0288,
	0x0289, 0x028a, 0x028b, 0x028c, 0x0342, 0x0343, 0x0344, 0x0345,
	0x0380, 0x0381, 0x0382, 0x0383, 0x0384, 0x0385, 0x0388, 0x0389,
	0x038a, 0x038b, 0x038c, 0x038d, 0x0390, 0x0391, 0x0392, 0x0393,
	0x0394, 0x0395, 0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d,
	0x03a0, 0x03a1, 0x03a2, 0x03a3, 0x03a4, 0x03a5, 0x03a8, 0x03a9,
	0x03aa, 0x03ab, 0x03ac, 0x03ad, 0x03b0, 0x03b1, 0x03b2, 0x03b3,
	0x03b4, 0x03b5, 0x03b8, 0x03b9, 0x03ba, 0x03bb, 0x03bb
};

static uint8_t urtw_8225v2_tx_gain_cck_ofdm[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
	0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
};

static uint8_t urtw_8225v2_txpwr_cck[] = {
	0x36, 0x35, 0x2e, 0x25, 0x1c, 0x12, 0x09, 0x04
};

static uint8_t urtw_8225v2_txpwr_cck_ch14[] = {
	0x36, 0x35, 0x2e, 0x1b, 0x00, 0x00, 0x00, 0x00
};

static struct urtw_pair urtw_ratetable[] = {
	{  2,  0 }, {   4,  1 }, { 11, 2 }, { 12, 4 }, { 18, 5 },
	{ 22,  3 }, {  24,  6 }, { 36, 7 }, { 48, 8 }, { 72, 9 },
	{ 96, 10 }, { 108, 11 }
};

static struct ieee80211vap *urtw_vap_create(struct ieee80211com *,
			    const char name[IFNAMSIZ], int unit, int opmode,
			    int flags, const uint8_t bssid[IEEE80211_ADDR_LEN],
			    const uint8_t mac[IEEE80211_ADDR_LEN]);
static void		urtw_vap_delete(struct ieee80211vap *);
static void		urtw_init(void *);
static void		urtw_stop(struct ifnet *, int);
static int		urtw_ioctl(struct ifnet *, u_long, caddr_t);
static void		urtw_start(struct ifnet *);
static int		urtw_alloc_rx_data_list(struct urtw_softc *);
static int		urtw_alloc_tx_data_list(struct urtw_softc *);
static void		urtw_free_data_list(struct urtw_softc *,
			    usbd_pipe_handle, usbd_pipe_handle,
			    struct urtw_data data[], int);
static int		urtw_raw_xmit(struct ieee80211_node *, struct mbuf *,
			    const struct ieee80211_bpf_params *);
static void		urtw_scan_start(struct ieee80211com *);
static void		urtw_scan_end(struct ieee80211com *);
static void		urtw_set_channel(struct ieee80211com *);
static void		urtw_update_mcast(struct ifnet *);
static void		urtw_rxeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static int		urtw_tx_start(struct urtw_softc *,
			    struct ieee80211_node *, struct mbuf *, int);
static void		urtw_txeof_low(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		urtw_txeof_normal(usbd_xfer_handle, 
			    usbd_private_handle, usbd_status);
static int		urtw_newstate(struct ieee80211vap *,
			    enum ieee80211_state, int);
static void		urtw_ledtask(void *);
static void		urtw_ledusbtask(void *);
static void		urtw_ctxtask(void *);
static void		urtw_task(void *);
static void		urtw_watchdog(void *);
static void		urtw_set_multi(void *);
static int		urtw_isbmode(uint16_t);
static uint16_t		urtw_rate2rtl(int);
static uint16_t		urtw_rtl2rate(int);
static usbd_status	urtw_set_rate(struct urtw_softc *);
static usbd_status	urtw_update_msr(struct urtw_softc *);
static usbd_status	urtw_read8_c(struct urtw_softc *, int, uint8_t *);
static usbd_status	urtw_read16_c(struct urtw_softc *, int, uint16_t *);
static usbd_status	urtw_read32_c(struct urtw_softc *, int, uint32_t *);
static usbd_status	urtw_write8_c(struct urtw_softc *, int, uint8_t);
static usbd_status	urtw_write16_c(struct urtw_softc *, int, uint16_t);
static usbd_status	urtw_write32_c(struct urtw_softc *, int, uint32_t);
static usbd_status	urtw_eprom_cs(struct urtw_softc *, int);
static usbd_status	urtw_eprom_ck(struct urtw_softc *);
static usbd_status	urtw_eprom_sendbits(struct urtw_softc *, int16_t *,
			    int);
static usbd_status	urtw_eprom_read32(struct urtw_softc *, uint32_t,
			    uint32_t *);
static usbd_status	urtw_eprom_readbit(struct urtw_softc *, int16_t *);
static usbd_status	urtw_eprom_writebit(struct urtw_softc *, int16_t);
static usbd_status	urtw_get_macaddr(struct urtw_softc *);
static usbd_status	urtw_get_txpwr(struct urtw_softc *);
static usbd_status	urtw_get_rfchip(struct urtw_softc *);
static usbd_status	urtw_led_init(struct urtw_softc *);
static usbd_status	urtw_8185_rf_pins_enable(struct urtw_softc *);
static usbd_status	urtw_8185_tx_antenna(struct urtw_softc *, uint8_t);
static usbd_status	urtw_8187_write_phy(struct urtw_softc *, uint8_t,
			    uint32_t);
static usbd_status	urtw_8187_write_phy_ofdm_c(struct urtw_softc *,
			    uint8_t, uint32_t);
static usbd_status	urtw_8187_write_phy_cck_c(struct urtw_softc *, uint8_t,
			    uint32_t);
static usbd_status	urtw_8225_setgain(struct urtw_softc *, int16_t);
static usbd_status	urtw_8225_usb_init(struct urtw_softc *);
static usbd_status	urtw_8225_write_c(struct urtw_softc *, uint8_t,
			    uint16_t);
static usbd_status	urtw_8225_write_s16(struct urtw_softc *, uint8_t, int,
			    uint16_t *);
static usbd_status	urtw_8225_read(struct urtw_softc *, uint8_t,
			    uint32_t *);
static usbd_status	urtw_8225_rf_init(struct urtw_softc *);
static usbd_status	urtw_8225_rf_set_chan(struct urtw_softc *, int);
static usbd_status	urtw_8225_rf_set_sens(struct urtw_softc *, int);
static usbd_status	urtw_8225_set_txpwrlvl(struct urtw_softc *, int);
static usbd_status	urtw_8225v2_rf_init(struct urtw_softc *);
static usbd_status	urtw_8225v2_rf_set_chan(struct urtw_softc *, int);
static usbd_status	urtw_8225v2_set_txpwrlvl(struct urtw_softc *, int);
static usbd_status	urtw_8225v2_setgain(struct urtw_softc *, int16_t);
static usbd_status	urtw_8225_isv2(struct urtw_softc *, int *);
static usbd_status	urtw_read8e(struct urtw_softc *, int, uint8_t *);
static usbd_status	urtw_write8e(struct urtw_softc *, int, uint8_t);
static usbd_status	urtw_8180_set_anaparam(struct urtw_softc *, uint32_t);
static usbd_status	urtw_8185_set_anaparam2(struct urtw_softc *, uint32_t);
static usbd_status	urtw_open_pipes(struct urtw_softc *);
static usbd_status	urtw_close_pipes(struct urtw_softc *);
static usbd_status	urtw_intr_enable(struct urtw_softc *);
static usbd_status	urtw_intr_disable(struct urtw_softc *);
static usbd_status	urtw_reset(struct urtw_softc *);
static usbd_status	urtw_led_on(struct urtw_softc *, int);
static usbd_status	urtw_led_ctl(struct urtw_softc *, int);
static usbd_status	urtw_led_blink(struct urtw_softc *);
static usbd_status	urtw_led_mode0(struct urtw_softc *, int);
static usbd_status	urtw_led_mode1(struct urtw_softc *, int);
static usbd_status	urtw_led_mode2(struct urtw_softc *, int);
static usbd_status	urtw_led_mode3(struct urtw_softc *, int);
static usbd_status	urtw_rx_setconf(struct urtw_softc *);
static usbd_status	urtw_rx_enable(struct urtw_softc *);
static usbd_status	urtw_tx_enable(struct urtw_softc *sc);

static int
urtw_match(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	const struct usb_devno *ud;

	if (uaa->iface != NULL)
		return UMATCH_NONE;
	ud = usb_lookup(urtw_devs, uaa->vendor, uaa->product);

	return (ud != NULL ? UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

static int
urtw_attach(device_t dev)
{
	int ret = 0;
	struct urtw_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct ieee80211com *ic;
	struct ifnet *ifp;
	uint8_t bands;
	uint32_t data;
	usbd_status error;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
#ifdef URTW_DEBUG
	sc->sc_debug = urtw_debug;
#endif

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init(&sc->sc_led_ch, 0);
	callout_init(&sc->sc_watchdog_ch, 0);
	usb_init_task(&sc->sc_ledtask, urtw_ledusbtask, sc);
	usb_init_task(&sc->sc_ctxtask, urtw_ctxtask, sc);
	usb_init_task(&sc->sc_task, urtw_task, sc);

	urtw_read32_m(sc, URTW_RX, &data);
	sc->sc_epromtype = (data & URTW_RX_9356SEL) ? URTW_EEPROM_93C56 :
	    URTW_EEPROM_93C46;

	error = urtw_get_rfchip(sc);
	if (error != 0)
		goto fail;
	error = urtw_get_macaddr(sc);
	if (error != 0)
		goto fail;
	error = urtw_get_txpwr(sc);
	if (error != 0)
		goto fail;
	error = urtw_led_init(sc);
	if (error != 0)
		goto fail;

	sc->sc_rts_retry = URTW_DEFAULT_RTS_RETRY;
	sc->sc_tx_retry = URTW_DEFAULT_TX_RETRY;
	sc->sc_currate = 3;
	sc->sc_preamble_mode = urtw_preamble_mode;

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not allocate ifnet\n");
		ret = ENXIO;
		goto fail;
	}

	ifp->if_softc = sc;
	if_initname(ifp, "urtw", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT; /* USB stack is still under Giant lock */
	ifp->if_init = urtw_init;
	ifp->if_ioctl = urtw_ioctl;
	ifp->if_start = urtw_start;
	/* XXX URTW_TX_DATA_LIST_COUNT */
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic = ifp->if_l2com;
	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_STA |		/* station mode */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_BGSCAN |	/* capable of bg scanning */
	    IEEE80211_C_WPA;		/* 802.11i */

	IEEE80211_ADDR_COPY(ic->ic_myaddr, sc->sc_bssid);

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, NULL, &bands);

	ieee80211_ifattach(ic);
	ic->ic_raw_xmit = urtw_raw_xmit;
	ic->ic_scan_start = urtw_scan_start;
	ic->ic_scan_end = urtw_scan_end;
	ic->ic_set_channel = urtw_set_channel;

	ic->ic_vap_create = urtw_vap_create;
	ic->ic_vap_delete = urtw_vap_delete;
	ic->ic_update_mcast = urtw_update_mcast;

	bpfattach(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + sizeof(sc->sc_txtap));

	sc->sc_rxtap_len = sizeof sc->sc_rxtap;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(URTW_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtap;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(URTW_TX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
fail:
	return (ret);
}

static usbd_status
urtw_open_pipes(struct urtw_softc *sc)
{
	usbd_status error;

	/*
	 * NB: there is no way to distinguish each pipes so we need to hardcode
	 * pipe numbers
	 */

	/* tx pipe - low priority packets  */
	error = usbd_open_pipe(sc->sc_iface, 0x2, USBD_EXCLUSIVE_USE,
	    &sc->sc_txpipe_low);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not open Tx low pipe: %s\n",
		    usbd_errstr(error));
		goto fail;
	}
	/* tx pipe - normal priority packets  */
	error = usbd_open_pipe(sc->sc_iface, 0x3, USBD_EXCLUSIVE_USE,
	    &sc->sc_txpipe_normal);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not open Tx normal pipe: %s\n",
		    usbd_errstr(error));
		goto fail;
	}
	/* rx pipe  */
	error = usbd_open_pipe(sc->sc_iface, 0x81, USBD_EXCLUSIVE_USE,
	    &sc->sc_rxpipe);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not open Rx pipe: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	return (0);
fail:
	(void)urtw_close_pipes(sc);
	return (error);
}

static usbd_status
urtw_close_pipes(struct urtw_softc *sc)
{
	usbd_status error = 0;

	if (sc->sc_rxpipe != NULL) {
		error = usbd_close_pipe(sc->sc_rxpipe);
		if (error != 0)
			goto fail;
		sc->sc_rxpipe = NULL;
	}
	if (sc->sc_txpipe_low != NULL) {
		error = usbd_close_pipe(sc->sc_txpipe_low);
		if (error != 0)
			goto fail;
		sc->sc_txpipe_low = NULL;
	}
	if (sc->sc_txpipe_normal != NULL) {
		error = usbd_close_pipe(sc->sc_txpipe_normal);
		if (error != 0)
			goto fail;
		sc->sc_txpipe_normal = NULL;
	}
fail:
	return (error);
}

static int
urtw_alloc_data_list(struct urtw_softc *sc, struct urtw_data data[],
	int ndata, int maxsz, int fillmbuf)
{
	int i, error;

	for (i = 0; i < ndata; i++) {
		struct urtw_data *dp = &data[i];

		dp->sc = sc;
		dp->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (dp->xfer == NULL) {
			device_printf(sc->sc_dev, "could not allocate xfer\n");
			error = ENOMEM;
			goto fail;
		}
		if (fillmbuf) {
			dp->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
			if (dp->m == NULL) {
				device_printf(sc->sc_dev,
				    "could not allocate rx mbuf\n");
				error = ENOMEM;
				goto fail;
			}
			dp->buf = mtod(dp->m, uint8_t *);
		} else {
			dp->m = NULL;
			dp->buf = usbd_alloc_buffer(dp->xfer, maxsz);
			if (dp->buf == NULL) {
				device_printf(sc->sc_dev,
				    "could not allocate buffer\n");
				error = ENOMEM;
				goto fail;
			}
			if (((unsigned long)dp->buf) % 4)
				device_printf(sc->sc_dev,
				    "warn: unaligned buffer %p\n", dp->buf);
		}
		dp->ni = NULL;
	}

	return 0;

fail:	urtw_free_data_list(sc, NULL, NULL, data, ndata);
	return error;
}

static void
urtw_free_data_list(struct urtw_softc *sc, usbd_pipe_handle pipe1,
    usbd_pipe_handle pipe2, struct urtw_data data[], int ndata)
{
	int i;

	/* make sure no transfers are pending */
	if (pipe1 != NULL)
		usbd_abort_pipe(pipe1);
	if (pipe2 != NULL)
		usbd_abort_pipe(pipe2);

	for (i = 0; i < ndata; i++) {
		struct urtw_data *dp = &data[i];

		if (dp->xfer != NULL) {
			usbd_free_xfer(dp->xfer);
			dp->xfer = NULL;
		}
		if (dp->m != NULL) {
			m_freem(dp->m);
			dp->m = NULL;
		}
		if (dp->ni != NULL) {
			ieee80211_free_node(dp->ni);
			dp->ni = NULL;
		}
	}
}

static int
urtw_alloc_rx_data_list(struct urtw_softc *sc)
{

	return urtw_alloc_data_list(sc,
	    sc->sc_rxdata, URTW_RX_DATA_LIST_COUNT, MCLBYTES, 1 /* mbufs */);
}

static void
urtw_free_rx_data_list(struct urtw_softc *sc)
{

	urtw_free_data_list(sc, sc->sc_rxpipe, NULL, sc->sc_rxdata,
	    URTW_RX_DATA_LIST_COUNT);
}

static int
urtw_alloc_tx_data_list(struct urtw_softc *sc)
{

	return urtw_alloc_data_list(sc,
	    sc->sc_txdata, URTW_TX_DATA_LIST_COUNT, URTW_TX_MAXSIZE,
	    0 /* no mbufs */);
}

static void
urtw_free_tx_data_list(struct urtw_softc *sc)
{

	urtw_free_data_list(sc, sc->sc_txpipe_low, sc->sc_txpipe_normal,
	    sc->sc_txdata, URTW_TX_DATA_LIST_COUNT);
}

static usbd_status
urtw_led_init(struct urtw_softc *sc)
{
	uint32_t rev;
	usbd_status error;

	urtw_read8_m(sc, URTW_PSR, &sc->sc_psr);
	error = urtw_eprom_read32(sc, URTW_EPROM_SWREV, &rev);
	if (error != 0)
		goto fail;

	switch (rev & URTW_EPROM_CID_MASK) {
	case URTW_EPROM_CID_ALPHA0:
		sc->sc_strategy = URTW_SW_LED_MODE1;
		break;
	case URTW_EPROM_CID_SERCOMM_PS:
		sc->sc_strategy = URTW_SW_LED_MODE3;
		break;
	case URTW_EPROM_CID_HW_LED:
		sc->sc_strategy = URTW_HW_LED;
		break;
	case URTW_EPROM_CID_RSVD0:
	case URTW_EPROM_CID_RSVD1:
	default:
		sc->sc_strategy = URTW_SW_LED_MODE0;
		break;
	}

	sc->sc_gpio_ledpin = URTW_LED_PIN_GPIO0;

fail:
	return (error);
}

/* XXX why we should allocalte memory buffer instead of using memory stack?  */
static usbd_status
urtw_8225_write_s16(struct urtw_softc *sc, uint8_t addr, int index,
    uint16_t *data)
{
	uint8_t *buf;
	uint16_t data16;
	usb_device_request_t *req;
	usbd_status error = 0;

	data16 = *data;
	req = (usb_device_request_t *)malloc(sizeof(usb_device_request_t),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (req == NULL) {
		device_printf(sc->sc_dev, "could not allocate a memory\n");
		goto fail0;
	}
	buf = (uint8_t *)malloc(2, M_80211_VAP, M_NOWAIT | M_ZERO);
	if (req == NULL) {
		device_printf(sc->sc_dev, "could not allocate a memory\n");
		goto fail1;
	}

	req->bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req->bRequest = URTW_8187_SETREGS_REQ;
	USETW(req->wValue, addr);
	USETW(req->wIndex, index);
	USETW(req->wLength, sizeof(uint16_t));
	buf[0] = (data16 & 0x00ff);
	buf[1] = (data16 & 0xff00) >> 8;

	error = usbd_do_request(sc->sc_udev, req, buf);

	free(buf, M_80211_VAP);
fail1:	free(req, M_80211_VAP);
fail0:	return (error);
}

static usbd_status
urtw_8225_read(struct urtw_softc *sc, uint8_t addr, uint32_t *data)
{
	int i;
	int16_t bit;
	uint8_t rlen = 12, wlen = 6;
	uint16_t o1, o2, o3, tmp;
	uint32_t d2w = ((uint32_t)(addr & 0x1f)) << 27;
	uint32_t mask = 0x80000000, value = 0;
	usbd_status error;

	urtw_read16_m(sc, URTW_RF_PINS_OUTPUT, &o1);
	urtw_read16_m(sc, URTW_RF_PINS_ENABLE, &o2);
	urtw_read16_m(sc, URTW_RF_PINS_SELECT, &o3);
	urtw_write16_m(sc, URTW_RF_PINS_ENABLE, o2 | URTW_RF_PINS_MAGIC4);
	urtw_write16_m(sc, URTW_RF_PINS_SELECT, o3 | URTW_RF_PINS_MAGIC4);
	o1 &= ~URTW_RF_PINS_MAGIC4;
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, o1 | URTW_BB_HOST_BANG_EN);
	DELAY(5);
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, o1);
	DELAY(5);

	for (i = 0; i < (wlen / 2); i++, mask = mask >> 1) {
		bit = ((d2w & mask) != 0) ? 1 : 0;

		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1 |
		    URTW_BB_HOST_BANG_CLK);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1 |
		    URTW_BB_HOST_BANG_CLK);
		DELAY(2);
		mask = mask >> 1;
		if (i == 2)
			break;
		bit = ((d2w & mask) != 0) ? 1 : 0;
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1 |
		    URTW_BB_HOST_BANG_CLK);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1 |
		    URTW_BB_HOST_BANG_CLK);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1);
		DELAY(1);
	}
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1 | URTW_BB_HOST_BANG_RW |
	    URTW_BB_HOST_BANG_CLK);
	DELAY(2);
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, bit | o1 | URTW_BB_HOST_BANG_RW);
	DELAY(2);
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, o1 | URTW_BB_HOST_BANG_RW);
	DELAY(2);

	mask = 0x800;
	for (i = 0; i < rlen; i++, mask = mask >> 1) {
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT,
		    o1 | URTW_BB_HOST_BANG_RW);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT,
		    o1 | URTW_BB_HOST_BANG_RW | URTW_BB_HOST_BANG_CLK);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT,
		    o1 | URTW_BB_HOST_BANG_RW | URTW_BB_HOST_BANG_CLK);
		DELAY(2);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT,
		    o1 | URTW_BB_HOST_BANG_RW | URTW_BB_HOST_BANG_CLK);
		DELAY(2);

		urtw_read16_m(sc, URTW_RF_PINS_INPUT, &tmp);
		value |= ((tmp & URTW_BB_HOST_BANG_CLK) ? mask : 0);
		urtw_write16_m(sc, URTW_RF_PINS_OUTPUT,
		    o1 | URTW_BB_HOST_BANG_RW);
		DELAY(2);
	}

	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, o1 | URTW_BB_HOST_BANG_EN |
	    URTW_BB_HOST_BANG_RW);
	DELAY(2);

	urtw_write16_m(sc, URTW_RF_PINS_ENABLE, o2);
	urtw_write16_m(sc, URTW_RF_PINS_SELECT, o3);
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, URTW_RF_PINS_OUTPUT_MAGIC1);

	if (data != NULL)
		*data = value;
fail:
	return (error);
}

static usbd_status
urtw_8225_write_c(struct urtw_softc *sc, uint8_t addr, uint16_t data)
{
	uint16_t d80, d82, d84;
	usbd_status error;

	urtw_read16_m(sc, URTW_RF_PINS_OUTPUT, &d80);
	d80 &= URTW_RF_PINS_MAGIC1;
	urtw_read16_m(sc, URTW_RF_PINS_ENABLE, &d82);
	urtw_read16_m(sc, URTW_RF_PINS_SELECT, &d84);
	d84 &= URTW_RF_PINS_MAGIC2;
	urtw_write16_m(sc, URTW_RF_PINS_ENABLE, d82 | URTW_RF_PINS_MAGIC3);
	urtw_write16_m(sc, URTW_RF_PINS_SELECT, d84 | URTW_RF_PINS_MAGIC3);
	DELAY(10);

	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, d80 | URTW_BB_HOST_BANG_EN);
	DELAY(2);
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, d80);
	DELAY(10);

	error = urtw_8225_write_s16(sc, addr, 0x8225, &data);
	if (error != 0)
		goto fail;

	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, d80 | URTW_BB_HOST_BANG_EN);
	DELAY(10);
	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, d80 | URTW_BB_HOST_BANG_EN);
	urtw_write16_m(sc, URTW_RF_PINS_SELECT, d84);
	usbd_delay_ms(sc->sc_udev, 2);
fail:
	return (error);
}

static usbd_status
urtw_8225_isv2(struct urtw_softc *sc, int *ret)
{
	uint32_t data;
	usbd_status error;

	*ret = 1;

	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, URTW_RF_PINS_MAGIC5);
	urtw_write16_m(sc, URTW_RF_PINS_SELECT, URTW_RF_PINS_MAGIC5);
	urtw_write16_m(sc, URTW_RF_PINS_ENABLE, URTW_RF_PINS_MAGIC5);
	usbd_delay_ms(sc->sc_udev, 500);

	urtw_8225_write(sc, URTW_8225_ADDR_0_MAGIC,
	    URTW_8225_ADDR_0_DATA_MAGIC1);

	error = urtw_8225_read(sc, URTW_8225_ADDR_8_MAGIC, &data);
	if (error != 0)
		goto fail;
	if (data != URTW_8225_ADDR_8_DATA_MAGIC1)
		*ret = 0;
	else {
		error = urtw_8225_read(sc, URTW_8225_ADDR_9_MAGIC, &data);
		if (error != 0)
			goto fail;
		if (data != URTW_8225_ADDR_9_DATA_MAGIC1)
			*ret = 0;
	}

	urtw_8225_write(sc, URTW_8225_ADDR_0_MAGIC,
	    URTW_8225_ADDR_0_DATA_MAGIC2);
fail:
	return (error);
}

static usbd_status
urtw_get_rfchip(struct urtw_softc *sc)
{
	int ret;
	uint32_t data;
	usbd_status error;

	error = urtw_eprom_read32(sc, URTW_EPROM_RFCHIPID, &data);
	if (error != 0)
		goto fail;
	switch (data & 0xff) {
	case URTW_EPROM_RFCHIPID_RTL8225U:
		error = urtw_8225_isv2(sc, &ret);
		if (error != 0)
			goto fail;
		if (ret == 0) {
			sc->sc_rf_init = urtw_8225_rf_init;
			sc->sc_rf_set_sens = urtw_8225_rf_set_sens;
			sc->sc_rf_set_chan = urtw_8225_rf_set_chan;
		} else {
			sc->sc_rf_init = urtw_8225v2_rf_init;
			sc->sc_rf_set_chan = urtw_8225v2_rf_set_chan;
		}
		sc->sc_max_sens = URTW_8225_RF_MAX_SENS;
		sc->sc_sens = URTW_8225_RF_DEF_SENS;
		break;
	default:
		panic("unsupported RF chip %d\n", data & 0xff);
		/* never reach  */
	}

fail:
	return (error);
}

static usbd_status
urtw_get_txpwr(struct urtw_softc *sc)
{
	int i, j;
	uint32_t data;
	usbd_status error;

	error = urtw_eprom_read32(sc, URTW_EPROM_TXPW_BASE, &data);
	if (error != 0)
		goto fail;
	sc->sc_txpwr_cck_base = data & 0xf;
	sc->sc_txpwr_ofdm_base = (data >> 4) & 0xf;

	for (i = 1, j = 0; i < 6; i += 2, j++) {
		error = urtw_eprom_read32(sc, URTW_EPROM_TXPW0 + j, &data);
		if (error != 0)
			goto fail;
		sc->sc_txpwr_cck[i] = data & 0xf;
		sc->sc_txpwr_cck[i + 1] = (data & 0xf00) >> 8;
		sc->sc_txpwr_ofdm[i] = (data & 0xf0) >> 4;
		sc->sc_txpwr_ofdm[i + 1] = (data & 0xf000) >> 12;
	}
	for (i = 1, j = 0; i < 4; i += 2, j++) {
		error = urtw_eprom_read32(sc, URTW_EPROM_TXPW1 + j, &data);
		if (error != 0)
			goto fail;
		sc->sc_txpwr_cck[i + 6] = data & 0xf;
		sc->sc_txpwr_cck[i + 6 + 1] = (data & 0xf00) >> 8;
		sc->sc_txpwr_ofdm[i + 6] = (data & 0xf0) >> 4;
		sc->sc_txpwr_ofdm[i + 6 + 1] = (data & 0xf000) >> 12;
	}
	for (i = 1, j = 0; i < 4; i += 2, j++) {
		error = urtw_eprom_read32(sc, URTW_EPROM_TXPW2 + j, &data);
		if (error != 0)
			goto fail;
		sc->sc_txpwr_cck[i + 6 + 4] = data & 0xf;
		sc->sc_txpwr_cck[i + 6 + 4 + 1] = (data & 0xf00) >> 8;
		sc->sc_txpwr_ofdm[i + 6 + 4] = (data & 0xf0) >> 4;
		sc->sc_txpwr_ofdm[i + 6 + 4 + 1] = (data & 0xf000) >> 12;
	}
fail:
	return (error);
}

static usbd_status
urtw_get_macaddr(struct urtw_softc *sc)
{
	uint32_t data;
	usbd_status error;

	error = urtw_eprom_read32(sc, URTW_EPROM_MACADDR, &data);
	if (error != 0)
		goto fail;
	sc->sc_bssid[0] = data & 0xff;
	sc->sc_bssid[1] = (data & 0xff00) >> 8;
	error = urtw_eprom_read32(sc, URTW_EPROM_MACADDR + 1, &data);
	if (error != 0)
		goto fail;
	sc->sc_bssid[2] = data & 0xff;
	sc->sc_bssid[3] = (data & 0xff00) >> 8;
	error = urtw_eprom_read32(sc, URTW_EPROM_MACADDR + 2, &data);
	if (error != 0)
		goto fail;
	sc->sc_bssid[4] = data & 0xff;
	sc->sc_bssid[5] = (data & 0xff00) >> 8;
fail:
	return (error);
}

static usbd_status
urtw_eprom_read32(struct urtw_softc *sc, uint32_t addr, uint32_t *data)
{
#define URTW_READCMD_LEN		3
	int addrlen, i;
	int16_t addrstr[8], data16, readcmd[] = { 1, 1, 0 };
	usbd_status error;

	/* NB: make sure the buffer is initialized  */
	*data = 0;

	/* enable EPROM programming */
	urtw_write8_m(sc, URTW_EPROM_CMD, URTW_EPROM_CMD_PROGRAM_MODE);
	DELAY(URTW_EPROM_DELAY);

	error = urtw_eprom_cs(sc, URTW_EPROM_ENABLE);
	if (error != 0)
		goto fail;
	error = urtw_eprom_ck(sc);
	if (error != 0)
		goto fail;
	error = urtw_eprom_sendbits(sc, readcmd, URTW_READCMD_LEN);
	if (error != 0)
		goto fail;
	if (sc->sc_epromtype == URTW_EEPROM_93C56) {
		addrlen = 8;
		addrstr[0] = addr & (1 << 7);
		addrstr[1] = addr & (1 << 6);
		addrstr[2] = addr & (1 << 5);
		addrstr[3] = addr & (1 << 4);
		addrstr[4] = addr & (1 << 3);
		addrstr[5] = addr & (1 << 2);
		addrstr[6] = addr & (1 << 1);
		addrstr[7] = addr & (1 << 0);
	} else {
		addrlen=6;
		addrstr[0] = addr & (1 << 5);
		addrstr[1] = addr & (1 << 4);
		addrstr[2] = addr & (1 << 3);
		addrstr[3] = addr & (1 << 2);
		addrstr[4] = addr & (1 << 1);
		addrstr[5] = addr & (1 << 0);
	}
	error = urtw_eprom_sendbits(sc, addrstr, addrlen);
	if (error != 0)
		goto fail;

	error = urtw_eprom_writebit(sc, 0);
	if (error != 0)
		goto fail;

	for (i = 0; i < 16; i++) {
		error = urtw_eprom_ck(sc);
		if (error != 0)
			goto fail;
		error = urtw_eprom_readbit(sc, &data16);
		if (error != 0)
			goto fail;

		(*data) |= (data16 << (15 - i));
	}

	error = urtw_eprom_cs(sc, URTW_EPROM_DISABLE);
	if (error != 0)
		goto fail;
	error = urtw_eprom_ck(sc);
	if (error != 0)
		goto fail;

	/* now disable EPROM programming */
	urtw_write8_m(sc, URTW_EPROM_CMD, URTW_EPROM_CMD_NORMAL_MODE);
fail:
	return (error);
#undef URTW_READCMD_LEN
}

static usbd_status
urtw_eprom_readbit(struct urtw_softc *sc, int16_t *data)
{
	uint8_t data8;
	usbd_status error;

	urtw_read8_m(sc, URTW_EPROM_CMD, &data8);
	*data = (data8 & URTW_EPROM_READBIT) ? 1 : 0;
	DELAY(URTW_EPROM_DELAY);

fail:
	return (error);
}

static usbd_status
urtw_eprom_sendbits(struct urtw_softc *sc, int16_t *buf, int buflen)
{
	int i = 0;
	usbd_status error = 0;

	for (i = 0; i < buflen; i++) {
		error = urtw_eprom_writebit(sc, buf[i]);
		if (error != 0)
			goto fail;
		error = urtw_eprom_ck(sc);
		if (error != 0)
			goto fail;
	}
fail:
	return (error);
}

static usbd_status
urtw_eprom_writebit(struct urtw_softc *sc, int16_t bit)
{
	uint8_t data;
	usbd_status error;

	urtw_read8_m(sc, URTW_EPROM_CMD, &data);
	if (bit != 0)
		urtw_write8_m(sc, URTW_EPROM_CMD, data | URTW_EPROM_WRITEBIT);
	else
		urtw_write8_m(sc, URTW_EPROM_CMD, data & ~URTW_EPROM_WRITEBIT);
	DELAY(URTW_EPROM_DELAY);
fail:
	return (error);
}

static usbd_status
urtw_eprom_ck(struct urtw_softc *sc)
{
	uint8_t data;
	usbd_status error;

	/* masking  */
	urtw_read8_m(sc, URTW_EPROM_CMD, &data);
	urtw_write8_m(sc, URTW_EPROM_CMD, data | URTW_EPROM_CK);
	DELAY(URTW_EPROM_DELAY);
	/* unmasking  */
	urtw_read8_m(sc, URTW_EPROM_CMD, &data);
	urtw_write8_m(sc, URTW_EPROM_CMD, data & ~URTW_EPROM_CK);
	DELAY(URTW_EPROM_DELAY);
fail:
	return (error);
}

static usbd_status
urtw_eprom_cs(struct urtw_softc *sc, int able)
{
	uint8_t data;
	usbd_status error;

	urtw_read8_m(sc, URTW_EPROM_CMD, &data);
	if (able == URTW_EPROM_ENABLE)
		urtw_write8_m(sc, URTW_EPROM_CMD, data | URTW_EPROM_CS);
	else
		urtw_write8_m(sc, URTW_EPROM_CMD, data & ~URTW_EPROM_CS);
	DELAY(URTW_EPROM_DELAY);
fail:
	return (error);
}

static usbd_status
urtw_read8_c(struct urtw_softc *sc, int val, uint8_t *data)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = URTW_8187_GETREGS_REQ;
	USETW(req.wValue, val | 0xff00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint8_t));

	error = usbd_do_request(sc->sc_udev, &req, data);
	return (error);
}

static usbd_status
urtw_read8e(struct urtw_softc *sc, int val, uint8_t *data)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = URTW_8187_GETREGS_REQ;
	USETW(req.wValue, val | 0xfe00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint8_t));

	error = usbd_do_request(sc->sc_udev, &req, data);
	return (error);
}

static usbd_status
urtw_read16_c(struct urtw_softc *sc, int val, uint16_t *data)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = URTW_8187_GETREGS_REQ;
	USETW(req.wValue, val | 0xff00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint16_t));

	error = usbd_do_request(sc->sc_udev, &req, data);
	return (error);
}

static usbd_status
urtw_read32_c(struct urtw_softc *sc, int val, uint32_t *data)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = URTW_8187_GETREGS_REQ;
	USETW(req.wValue, val | 0xff00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint32_t));

	error = usbd_do_request(sc->sc_udev, &req, data);
	return (error);
}

static usbd_status
urtw_write8_c(struct urtw_softc *sc, int val, uint8_t data)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = URTW_8187_SETREGS_REQ;
	USETW(req.wValue, val | 0xff00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint8_t));

	return (usbd_do_request(sc->sc_udev, &req, &data));
}

static usbd_status
urtw_write8e(struct urtw_softc *sc, int val, uint8_t data)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = URTW_8187_SETREGS_REQ;
	USETW(req.wValue, val | 0xfe00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint8_t));

	return (usbd_do_request(sc->sc_udev, &req, &data));
}

static usbd_status
urtw_write16_c(struct urtw_softc *sc, int val, uint16_t data)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = URTW_8187_SETREGS_REQ;
	USETW(req.wValue, val | 0xff00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint16_t));

	return (usbd_do_request(sc->sc_udev, &req, &data));
}

static usbd_status
urtw_write32_c(struct urtw_softc *sc, int val, uint32_t data)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = URTW_8187_SETREGS_REQ;
	USETW(req.wValue, val | 0xff00);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(uint32_t));

	return (usbd_do_request(sc->sc_udev, &req, &data));
}

static int
urtw_detach(device_t dev)
{
	struct urtw_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	if (!device_is_attached(dev))
		return 0;

	urtw_stop(ifp, 1);

	callout_drain(&sc->sc_led_ch);
	callout_drain(&sc->sc_watchdog_ch);
	usb_rem_task(sc->sc_udev, &sc->sc_ledtask);
	usb_rem_task(sc->sc_udev, &sc->sc_ctxtask);
	usb_rem_task(sc->sc_udev, &sc->sc_task);

	/* abort and free xfers */
	urtw_free_tx_data_list(sc);
	urtw_free_rx_data_list(sc);
	urtw_close_pipes(sc);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);
	if_free(ifp);
	mtx_destroy(&sc->sc_mtx);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return (0);
}

static struct ieee80211vap *
urtw_vap_create(struct ieee80211com *ic,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct urtw_vap *uvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return (NULL);
	uvp = (struct urtw_vap *) malloc(sizeof(struct urtw_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (uvp == NULL)
		return (NULL);
	vap = &uvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = urtw_newstate;

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status);
	ic->ic_opmode = opmode;
	return (vap);
}

static void
urtw_vap_delete(struct ieee80211vap *vap)
{
	struct urtw_vap *uvp = URTW_VAP(vap);

	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

static usbd_status
urtw_set_mode(struct urtw_softc *sc, uint32_t mode)
{
	uint8_t data;
	usbd_status error;

	urtw_read8_m(sc, URTW_EPROM_CMD, &data);
	data = (data & ~URTW_EPROM_CMD_MASK) | (mode << URTW_EPROM_CMD_SHIFT);
	data = data & ~(URTW_EPROM_CS | URTW_EPROM_CK);
	urtw_write8_m(sc, URTW_EPROM_CMD, data);
fail:
	return (error);
}

static usbd_status
urtw_8180_set_anaparam(struct urtw_softc *sc, uint32_t val)
{
	uint8_t data;
	usbd_status error;

	error = urtw_set_mode(sc, URTW_EPROM_CMD_CONFIG);
	if (error)
		goto fail;

	urtw_read8_m(sc, URTW_CONFIG3, &data);
	urtw_write8_m(sc, URTW_CONFIG3, data | URTW_CONFIG3_ANAPARAM_WRITE);
	urtw_write32_m(sc, URTW_ANAPARAM, val);
	urtw_read8_m(sc, URTW_CONFIG3, &data);
	urtw_write8_m(sc, URTW_CONFIG3, data & ~URTW_CONFIG3_ANAPARAM_WRITE);

	error = urtw_set_mode(sc, URTW_EPROM_CMD_NORMAL);
	if (error)
		goto fail;
fail:
	return (error);
}

static usbd_status
urtw_8185_set_anaparam2(struct urtw_softc *sc, uint32_t val)
{
	uint8_t data;
	usbd_status error;

	error = urtw_set_mode(sc, URTW_EPROM_CMD_CONFIG);
	if (error)
		goto fail;

	urtw_read8_m(sc, URTW_CONFIG3, &data);
	urtw_write8_m(sc, URTW_CONFIG3, data | URTW_CONFIG3_ANAPARAM_WRITE);
	urtw_write32_m(sc, URTW_ANAPARAM2, val);
	urtw_read8_m(sc, URTW_CONFIG3, &data);
	urtw_write8_m(sc, URTW_CONFIG3, data & ~URTW_CONFIG3_ANAPARAM_WRITE);

	error = urtw_set_mode(sc, URTW_EPROM_CMD_NORMAL);
	if (error)
		goto fail;
fail:
	return (error);
}

static usbd_status
urtw_intr_disable(struct urtw_softc *sc)
{
	usbd_status error;

	urtw_write16_m(sc, URTW_INTR_MASK, 0);
fail:
	return (error);
}

static usbd_status
urtw_reset(struct urtw_softc *sc)
{
	uint8_t data;
	usbd_status error;

	error = urtw_8180_set_anaparam(sc, URTW_8225_ANAPARAM_ON);
	if (error)
		goto fail;
	error = urtw_8185_set_anaparam2(sc, URTW_8225_ANAPARAM2_ON);
	if (error)
		goto fail;

	error = urtw_intr_disable(sc);
	if (error)
		goto fail;
	usbd_delay_ms(sc->sc_udev, 100);

	error = urtw_write8e(sc, 0x18, 0x10);
	if (error != 0)
		goto fail;
	error = urtw_write8e(sc, 0x18, 0x11);
	if (error != 0)
		goto fail;
	error = urtw_write8e(sc, 0x18, 0x00);
	if (error != 0)
		goto fail;
	usbd_delay_ms(sc->sc_udev, 100);

	urtw_read8_m(sc, URTW_CMD, &data);
	data = (data & 0x2) | URTW_CMD_RST;
	urtw_write8_m(sc, URTW_CMD, data);
	usbd_delay_ms(sc->sc_udev, 100);

	urtw_read8_m(sc, URTW_CMD, &data);
	if (data & URTW_CMD_RST) {
		device_printf(sc->sc_dev, "reset timeout\n");
		goto fail;
	}

	error = urtw_set_mode(sc, URTW_EPROM_CMD_LOAD);
	if (error)
		goto fail;
	usbd_delay_ms(sc->sc_udev, 100);

	error = urtw_8180_set_anaparam(sc, URTW_8225_ANAPARAM_ON);
	if (error)
		goto fail;
	error = urtw_8185_set_anaparam2(sc, URTW_8225_ANAPARAM2_ON);
	if (error)
		goto fail;
fail:
	return (error);
}

static usbd_status
urtw_led_on(struct urtw_softc *sc, int type)
{
	usbd_status error;

	if (type == URTW_LED_GPIO) {
		switch (sc->sc_gpio_ledpin) {
		case URTW_LED_PIN_GPIO0:
			urtw_write8_m(sc, URTW_GPIO, 0x01);
			urtw_write8_m(sc, URTW_GP_ENABLE, 0x00);
			break;
		default:
			panic("unsupported LED PIN type 0x%x",
			    sc->sc_gpio_ledpin);
			/* never reach  */
		}
	} else {
		panic("unsupported LED type 0x%x", type);
		/* never reach  */
	}

	sc->sc_gpio_ledon = 1;
fail:
	return (error);
}

static usbd_status
urtw_led_off(struct urtw_softc *sc, int type)
{
	usbd_status error;

	if (type == URTW_LED_GPIO) {
		switch (sc->sc_gpio_ledpin) {
		case URTW_LED_PIN_GPIO0:
			urtw_write8_m(sc, URTW_GPIO, URTW_GPIO_DATA_MAGIC1);
			urtw_write8_m(sc,
			    URTW_GP_ENABLE, URTW_GP_ENABLE_DATA_MAGIC1);
			break;
		default:
			panic("unsupported LED PIN type 0x%x",
			    sc->sc_gpio_ledpin);
			/* never reach  */
		}
	} else {
		panic("unsupported LED type 0x%x", type);
		/* never reach  */
	}

	sc->sc_gpio_ledon = 0;

fail:
	return (error);
}

static usbd_status
urtw_led_mode0(struct urtw_softc *sc, int mode)
{

	switch (mode) {
	case URTW_LED_CTL_POWER_ON:
		sc->sc_gpio_ledstate = URTW_LED_POWER_ON_BLINK;
		break;
	case URTW_LED_CTL_TX:
		if (sc->sc_gpio_ledinprogress == 1)
			return (0);

		sc->sc_gpio_ledstate = URTW_LED_BLINK_NORMAL;
		sc->sc_gpio_blinktime = 2;
		break;
	case URTW_LED_CTL_LINK:
		sc->sc_gpio_ledstate = URTW_LED_ON;
		break;
	default:
		panic("unsupported LED mode 0x%x", mode);
		/* never reach  */
	}

	switch (sc->sc_gpio_ledstate) {
	case URTW_LED_ON:
		if (sc->sc_gpio_ledinprogress != 0)
			break;
		urtw_led_on(sc, URTW_LED_GPIO);
		break;
	case URTW_LED_BLINK_NORMAL:
		if (sc->sc_gpio_ledinprogress != 0)
			break;
		sc->sc_gpio_ledinprogress = 1;
		sc->sc_gpio_blinkstate = (sc->sc_gpio_ledon != 0) ?
			URTW_LED_OFF : URTW_LED_ON;
		callout_reset(&sc->sc_led_ch, hz, urtw_ledtask, sc);
		break;
	case URTW_LED_POWER_ON_BLINK:
		urtw_led_on(sc, URTW_LED_GPIO);
		usbd_delay_ms(sc->sc_udev, 100);
		urtw_led_off(sc, URTW_LED_GPIO);
		break;
	default:
		panic("unknown LED status 0x%x", sc->sc_gpio_ledstate);
		/* never reach  */
	}
	return (0);
}

static usbd_status
urtw_led_mode1(struct urtw_softc *sc, int mode)
{

	return (USBD_INVAL);
}

static usbd_status
urtw_led_mode2(struct urtw_softc *sc, int mode)
{

	return (USBD_INVAL);
}

static usbd_status
urtw_led_mode3(struct urtw_softc *sc, int mode)
{

	return (USBD_INVAL);
}

static usbd_status
urtw_led_blink(struct urtw_softc *sc)
{
	uint8_t ing = 0;
	usbd_status error;

	if (sc->sc_gpio_blinkstate == URTW_LED_ON)
		error = urtw_led_on(sc, URTW_LED_GPIO);
	else
		error = urtw_led_off(sc, URTW_LED_GPIO);
	sc->sc_gpio_blinktime--;
	if (sc->sc_gpio_blinktime == 0)
		ing = 1;
	else {
		if (sc->sc_gpio_ledstate != URTW_LED_BLINK_NORMAL &&
		    sc->sc_gpio_ledstate != URTW_LED_BLINK_SLOWLY &&
		    sc->sc_gpio_ledstate != URTW_LED_BLINK_CM3)
			ing = 1;
	}
	if (ing == 1) {
		if (sc->sc_gpio_ledstate == URTW_LED_ON &&
		    sc->sc_gpio_ledon == 0)
			error = urtw_led_on(sc, URTW_LED_GPIO);
		else if (sc->sc_gpio_ledstate == URTW_LED_OFF &&
		    sc->sc_gpio_ledon == 1)
			error = urtw_led_off(sc, URTW_LED_GPIO);

		sc->sc_gpio_blinktime = 0;
		sc->sc_gpio_ledinprogress = 0;
		return (0);
	}

	sc->sc_gpio_blinkstate = (sc->sc_gpio_blinkstate != URTW_LED_ON) ?
	    URTW_LED_ON : URTW_LED_OFF;

	switch (sc->sc_gpio_ledstate) {
	case URTW_LED_BLINK_NORMAL:
		callout_reset(&sc->sc_led_ch, hz, urtw_ledtask, sc);
		break;
	default:
		panic("unknown LED status 0x%x", sc->sc_gpio_ledstate);
		/* never reach  */
	}
	return (0);
}

static void
urtw_ledusbtask(void *arg)
{
	struct urtw_softc *sc = arg;

	if (sc->sc_strategy != URTW_SW_LED_MODE0)
		panic("could not process a LED strategy 0x%x", sc->sc_strategy);

	urtw_led_blink(sc);
}

static void
urtw_ledtask(void *arg)
{
	struct urtw_softc *sc = arg;

	/*
	 * NB: to change a status of the led we need at least a sleep so we
	 * can't do it here
	 */
	usb_add_task(sc->sc_udev, &sc->sc_ledtask, USB_TASKQ_DRIVER);
}

static usbd_status
urtw_led_ctl(struct urtw_softc *sc, int mode)
{
	usbd_status error = 0;

	switch (sc->sc_strategy) {
	case URTW_SW_LED_MODE0:
		error = urtw_led_mode0(sc, mode);
		break;
	case URTW_SW_LED_MODE1:
		error = urtw_led_mode1(sc, mode);
		break;
	case URTW_SW_LED_MODE2:
		error = urtw_led_mode2(sc, mode);
		break;
	case URTW_SW_LED_MODE3:
		error = urtw_led_mode3(sc, mode);
		break;
	default:
		panic("unsupported LED mode %d\n", sc->sc_strategy);
		/* never reach  */
	}

	return (error);
}

static usbd_status
urtw_update_msr(struct urtw_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint8_t data;
	usbd_status error;

	urtw_read8_m(sc, URTW_MSR, &data);
	data &= ~URTW_MSR_LINK_MASK;

	if (sc->sc_state == IEEE80211_S_RUN) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
		case IEEE80211_M_MONITOR:
			data |= URTW_MSR_LINK_STA;
			break;
		case IEEE80211_M_IBSS:
			data |= URTW_MSR_LINK_ADHOC;
			break;
		case IEEE80211_M_HOSTAP:
			data |= URTW_MSR_LINK_HOSTAP;
			break;
		default:
			panic("unsupported operation mode 0x%x\n",
			    ic->ic_opmode);
			/* never reach  */
		}
	} else
		data |= URTW_MSR_LINK_NONE;

	urtw_write8_m(sc, URTW_MSR, data);
fail:
	return (error);
}

static uint16_t
urtw_rate2rtl(int rate)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	int i;

	for (i = 0; i < N(urtw_ratetable); i++) {
		if (rate == urtw_ratetable[i].reg)
			return urtw_ratetable[i].val;
	}

	return (3);
#undef N
}

static uint16_t
urtw_rtl2rate(int rate)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	int i;

	for (i = 0; i < N(urtw_ratetable); i++) {
		if (rate == urtw_ratetable[i].val)
			return urtw_ratetable[i].reg;
	}

	return (0);
#undef N
}

static usbd_status
urtw_set_rate(struct urtw_softc *sc)
{
	int i, basic_rate, min_rr_rate, max_rr_rate;
	uint16_t data;
	usbd_status error;

	basic_rate = urtw_rate2rtl(48);
	min_rr_rate = urtw_rate2rtl(12);
	max_rr_rate = urtw_rate2rtl(48);

	urtw_write8_m(sc, URTW_RESP_RATE,
	    max_rr_rate << URTW_RESP_MAX_RATE_SHIFT |
	    min_rr_rate << URTW_RESP_MIN_RATE_SHIFT);

	urtw_read16_m(sc, URTW_BRSR, &data);
	data &= ~URTW_BRSR_MBR_8185;

	for (i = 0; i <= basic_rate; i++)
		data |= (1 << i);

	urtw_write16_m(sc, URTW_BRSR, data);
fail:
	return (error);
}

static usbd_status
urtw_intr_enable(struct urtw_softc *sc)
{
	usbd_status error;

	urtw_write16_m(sc, URTW_INTR_MASK, 0xffff);
fail:
	return (error);
}

static usbd_status
urtw_adapter_start(struct urtw_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	usbd_status error;

	error = urtw_reset(sc);
	if (error)
		goto fail;

	urtw_write8_m(sc, URTW_ADDR_MAGIC1, 0);
	urtw_write8_m(sc, URTW_GPIO, 0);

	/* for led  */
	urtw_write8_m(sc, URTW_ADDR_MAGIC1, 4);
	error = urtw_led_ctl(sc, URTW_LED_CTL_POWER_ON);
	if (error != 0)
		goto fail;

	error = urtw_set_mode(sc, URTW_EPROM_CMD_CONFIG);
	if (error)
		goto fail;
	/* applying MAC address again.  */
	urtw_write32_m(sc, URTW_MAC0, ((uint32_t *)ic->ic_myaddr)[0]);
	urtw_write16_m(sc, URTW_MAC4, ((uint32_t *)ic->ic_myaddr)[1] & 0xffff);
	error = urtw_set_mode(sc, URTW_EPROM_CMD_NORMAL);
	if (error)
		goto fail;

	error = urtw_update_msr(sc);
	if (error)
		goto fail;

	urtw_write32_m(sc, URTW_INT_TIMEOUT, 0);
	urtw_write8_m(sc, URTW_WPA_CONFIG, 0);
	urtw_write8_m(sc, URTW_RATE_FALLBACK, 0x81);
	error = urtw_set_rate(sc);
	if (error != 0)
		goto fail;

	error = sc->sc_rf_init(sc);
	if (error != 0)
		goto fail;
	if (sc->sc_rf_set_sens != NULL)
		sc->sc_rf_set_sens(sc, sc->sc_sens);

	/* XXX correct? to call write16  */
	urtw_write16_m(sc, URTW_PSR, 1);
	urtw_write16_m(sc, URTW_ADDR_MAGIC2, 0x10);
	urtw_write8_m(sc, URTW_TALLY_SEL, 0x80);
	urtw_write8_m(sc, URTW_ADDR_MAGIC3, 0x60);
	/* XXX correct? to call write16  */
	urtw_write16_m(sc, URTW_PSR, 0);
	urtw_write8_m(sc, URTW_ADDR_MAGIC1, 4);

	error = urtw_intr_enable(sc);
	if (error != 0)
		goto fail;

fail:
	return (error);
}

static usbd_status
urtw_rx_setconf(struct urtw_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t data;
	usbd_status error;

	urtw_read32_m(sc, URTW_RX, &data);
	data = data &~ URTW_RX_FILTER_MASK;
#if 0
	data = data | URTW_RX_FILTER_CTL;
#endif
	data = data | URTW_RX_FILTER_MNG | URTW_RX_FILTER_DATA;
	data = data | URTW_RX_FILTER_BCAST | URTW_RX_FILTER_MCAST;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		data = data | URTW_RX_FILTER_ICVERR;
		data = data | URTW_RX_FILTER_PWR;
	}
	if (sc->sc_crcmon == 1 && ic->ic_opmode == IEEE80211_M_MONITOR)
		data = data | URTW_RX_FILTER_CRCERR;

	if (ic->ic_opmode == IEEE80211_M_MONITOR ||
	    (ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC))) {
		data = data | URTW_RX_FILTER_ALLMAC;
	} else {
		data = data | URTW_RX_FILTER_NICMAC;
		data = data | URTW_RX_CHECK_BSSID;
	}

	data = data &~ URTW_RX_FIFO_THRESHOLD_MASK;
	data = data | URTW_RX_FIFO_THRESHOLD_NONE | URTW_RX_AUTORESETPHY;
	data = data &~ URTW_MAX_RX_DMA_MASK;
	data = data | URTW_MAX_RX_DMA_2048 | URTW_RCR_ONLYERLPKT;

	urtw_write32_m(sc, URTW_RX, data);
fail:
	return (error);
}

static usbd_status
urtw_rx_enable(struct urtw_softc *sc)
{
	int i;
	struct urtw_data *rxdata;
	uint8_t data;
	usbd_status error;

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < URTW_RX_DATA_LIST_COUNT; i++) {
		rxdata = &sc->sc_rxdata[i];

		usbd_setup_xfer(rxdata->xfer, sc->sc_rxpipe, rxdata,
		    rxdata->buf, MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT,
		    urtw_rxeof);
		error = usbd_transfer(rxdata->xfer);
		if (error != USBD_IN_PROGRESS && error != 0) {
			device_printf(sc->sc_dev,
			    "could not queue Rx transfer\n");
			goto fail;
		}
	}

	error = urtw_rx_setconf(sc);
	if (error != 0)
		goto fail;

	urtw_read8_m(sc, URTW_CMD, &data);
	urtw_write8_m(sc, URTW_CMD, data | URTW_CMD_RX_ENABLE);
fail:
	return (error);
}

static usbd_status
urtw_tx_enable(struct urtw_softc *sc)
{
	uint8_t data8;
	uint32_t data;
	usbd_status error;

	urtw_read8_m(sc, URTW_CW_CONF, &data8);
	data8 &= ~(URTW_CW_CONF_PERPACKET_CW | URTW_CW_CONF_PERPACKET_RETRY);
	urtw_write8_m(sc, URTW_CW_CONF, data8);

	urtw_read8_m(sc, URTW_TX_AGC_CTL, &data8);
	data8 &= ~URTW_TX_AGC_CTL_PERPACKET_GAIN;
	data8 &= ~URTW_TX_AGC_CTL_PERPACKET_ANTSEL;
	data8 &= ~URTW_TX_AGC_CTL_FEEDBACK_ANT;
	urtw_write8_m(sc, URTW_TX_AGC_CTL, data8);

	urtw_read32_m(sc, URTW_TX_CONF, &data);
	data &= ~URTW_TX_LOOPBACK_MASK;
	data |= URTW_TX_LOOPBACK_NONE;
	data &= ~(URTW_TX_DPRETRY_MASK | URTW_TX_RTSRETRY_MASK);
	data |= sc->sc_tx_retry << URTW_TX_DPRETRY_SHIFT;
	data |= sc->sc_rts_retry << URTW_TX_RTSRETRY_SHIFT;
	data &= ~(URTW_TX_NOCRC | URTW_TX_MXDMA_MASK);
	data |= URTW_TX_MXDMA_2048 | URTW_TX_CWMIN | URTW_TX_DISCW;
	data &= ~URTW_TX_SWPLCPLEN;
	data |= URTW_TX_NOICV;
	urtw_write32_m(sc, URTW_TX_CONF, data);

	urtw_read8_m(sc, URTW_CMD, &data8);
	urtw_write8_m(sc, URTW_CMD, data8 | URTW_CMD_TX_ENABLE);
fail:
	return (error);
}

static void
urtw_init(void *arg)
{
	int ret;
	struct urtw_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	usbd_status error;

	urtw_stop(ifp, 0);

	error = urtw_adapter_start(sc);
	if (error != 0)
		goto fail;

	/* reset softc variables  */
	sc->sc_txidx = sc->sc_tx_low_queued = sc->sc_tx_normal_queued = 0;
	sc->sc_txtimer = 0;

	if (!(sc->sc_flags & URTW_INIT_ONCE)) {
		error = usbd_set_config_no(sc->sc_udev, URTW_CONFIG_NO, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not set configuration no\n");
			goto fail;
		}
		/* get the first interface handle */
		error = usbd_device2interface_handle(sc->sc_udev,
		    URTW_IFACE_INDEX, &sc->sc_iface);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not get interface handle\n");
			goto fail;
		}
		error = urtw_open_pipes(sc);
		if (error != 0)
			goto fail;
		ret = urtw_alloc_rx_data_list(sc);
		if (error != 0)
			goto fail;
		ret = urtw_alloc_tx_data_list(sc);
		if (error != 0)
			goto fail;
		sc->sc_flags |= URTW_INIT_ONCE;
	}

	error = urtw_rx_enable(sc);
	if (error != 0)
		goto fail;
	error = urtw_tx_enable(sc);
	if (error != 0)
		goto fail;

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	callout_reset(&sc->sc_watchdog_ch, hz, urtw_watchdog, sc);
fail:
	return;
}

static void
urtw_set_multi(void *arg)
{
	struct urtw_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (!(ifp->if_flags & IFF_UP))
		return;

	/*
	 * XXX don't know how to set a device.  Lack of docs.  Just try to set
	 * IFF_ALLMULTI flag here.
	 */
	IF_ADDR_LOCK(ifp);
	ifp->if_flags |= IFF_ALLMULTI;
	IF_ADDR_UNLOCK(ifp);
}

static int
urtw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct urtw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		mtx_lock(&Giant);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->sc_if_flags) &
				    (IFF_ALLMULTI | IFF_PROMISC))
					urtw_set_multi(sc);
			} else {
				urtw_init(ifp->if_softc);
				startall = 1;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				urtw_stop(ifp, 1);
		}
		sc->sc_if_flags = ifp->if_flags;
		mtx_unlock(&Giant);
		if (startall)
			ieee80211_start_all(ic);
		break;
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, cmd, data);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static void
urtw_start(struct ifnet *ifp)
{
	struct urtw_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	URTW_LOCK(sc);
	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		if (sc->sc_tx_low_queued >= URTW_TX_DATA_LIST_COUNT ||
		    sc->sc_tx_normal_queued >= URTW_TX_DATA_LIST_COUNT) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;
		m = ieee80211_encap(ni, m);
		if (m == NULL) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		if (urtw_tx_start(sc, ni, m, URTW_PRIORITY_NORMAL) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}

		sc->sc_txtimer = 5;
	}
	URTW_UNLOCK(sc);
}

static void
urtw_txeof_low(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct urtw_data *data = priv;
	struct urtw_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		device_printf(sc->sc_dev, "could not transmit buffer: %s\n",
		    usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_txpipe_low);

		ifp->if_oerrors++;
		return;
	}

	/*
	 * Do any tx complete callback.  Note this must be done before releasing
	 * the node reference.
	 */
	m = data->m;
	if (m != NULL && m->m_flags & M_TXCB) {
		ieee80211_process_callback(data->ni, m, 0);	/* XXX status? */
		m_freem(m);
		data->m = NULL;
	}

	ieee80211_free_node(data->ni);
	data->ni = NULL;

	sc->sc_txtimer = 0;
	ifp->if_opackets++;

	URTW_LOCK(sc);
	sc->sc_tx_low_queued--;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	URTW_UNLOCK(sc);

	urtw_start(ifp);
}

static void
urtw_txeof_normal(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct urtw_data *data = priv;
	struct urtw_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		device_printf(sc->sc_dev, "could not transmit buffer: %s\n",
		    usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_txpipe_normal);

		ifp->if_oerrors++;
		return;
	}

	/*
	 * Do any tx complete callback.  Note this must be done before releasing
	 * the node reference.
	 */
	m = data->m;
	if (m != NULL && m->m_flags & M_TXCB) {
		ieee80211_process_callback(data->ni, m, 0);	/* XXX status? */
		m_freem(m);
		data->m = NULL;
	}

	ieee80211_free_node(data->ni);
	data->ni = NULL;

	sc->sc_txtimer = 0;
	ifp->if_opackets++;

	URTW_LOCK(sc);
	sc->sc_tx_normal_queued--;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	URTW_UNLOCK(sc);

	urtw_start(ifp);
}

static int
urtw_tx_start(struct urtw_softc *sc, struct ieee80211_node *ni, struct mbuf *m0,
    int prior)
{
	int xferlen;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211_frame *wh = mtod(m0, struct ieee80211_frame *);
	struct ieee80211_key *k;
	const struct ieee80211_txparam *tp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = ni->ni_vap;
	struct urtw_data *data;
	usbd_status error;

	URTW_ASSERT_LOCKED(sc);

	/*
	 * Software crypto.
	 */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			device_printf(sc->sc_dev,
			    "ieee80211_crypto_encap returns NULL.\n");
			/* XXX we don't expect the fragmented frames  */
			m_freem(m0);
			return (ENOBUFS);
		}

		/* in case packet header moved, reset pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (bpf_peers_present(ifp->if_bpf)) {
		struct urtw_tx_radiotap_header *tap = &sc->sc_txtap;

		/* XXX Are variables correct?  */
		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m0);
	}

	xferlen = m0->m_pkthdr.len + 4 * 3;
	if((0 == xferlen % 64) || (0 == xferlen % 512))
		xferlen += 1;

	data = &sc->sc_txdata[sc->sc_txidx];
	sc->sc_txidx = (sc->sc_txidx + 1) % URTW_TX_DATA_LIST_COUNT;

	bzero(data->buf, URTW_TX_MAXSIZE);
	data->buf[0] = m0->m_pkthdr.len & 0xff;
	data->buf[1] = (m0->m_pkthdr.len & 0x0f00) >> 8;
	data->buf[1] |= (1 << 7);

	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) &&
	    (sc->sc_preamble_mode == URTW_PREAMBLE_MODE_SHORT) &&
	    (sc->sc_currate != 0))
		data->buf[2] |= 1;
	if ((m0->m_pkthdr.len > vap->iv_rtsthreshold) &&
	    prior == URTW_PRIORITY_LOW)
		panic("TODO tx.");
	if (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG)
		data->buf[2] |= (1 << 1);
	/* RTS rate - 10 means we use a basic rate.  */
	data->buf[2] |= (urtw_rate2rtl(2) << 3);
	/*
	 * XXX currently TX rate control depends on the rate value of
	 * RX descriptor because I don't know how to we can control TX rate
	 * in more smart way.  Please fix me you find a thing.
	 */
	data->buf[3] = sc->sc_currate;
	if (prior == URTW_PRIORITY_NORMAL) {
		tp = &vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)];
		if (IEEE80211_IS_MULTICAST(wh->i_addr1))
			data->buf[3] = urtw_rate2rtl(tp->mcastrate);
		else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
			data->buf[3] = urtw_rate2rtl(tp->ucastrate);
	}
	data->buf[8] = 3;		/* CW minimum  */
	data->buf[8] |= (7 << 4);	/* CW maximum  */
	data->buf[9] |= 11;		/* retry limitation  */

	m_copydata(m0, 0, m0->m_pkthdr.len, (uint8_t *)&data->buf[12]);
	data->ni = ni;
	data->m = m0;

	usbd_setup_xfer(data->xfer,
	    (prior == URTW_PRIORITY_LOW) ? sc->sc_txpipe_low :
	    sc->sc_txpipe_normal, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, URTW_DATA_TIMEOUT,
	    (prior == URTW_PRIORITY_LOW) ? urtw_txeof_low : urtw_txeof_normal);
	error = usbd_transfer(data->xfer);
	if (error != USBD_IN_PROGRESS && error != USBD_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "could not send frame: %s\n",
		    usbd_errstr(error));
		return EIO;
	}

	error = urtw_led_ctl(sc, URTW_LED_CTL_TX);
	if (error != 0)
		device_printf(sc->sc_dev, "could not control LED (%d)\n", error);

	if (prior == URTW_PRIORITY_LOW)
		sc->sc_tx_low_queued++;
	else
		sc->sc_tx_normal_queued++;

	return (0);
}

static int
urtw_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct urtw_softc *sc = ifp->if_softc;

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}
	URTW_LOCK(sc);
	if (sc->sc_tx_low_queued >= URTW_TX_DATA_LIST_COUNT ||
	    sc->sc_tx_normal_queued >= URTW_TX_DATA_LIST_COUNT) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		m_freem(m);
		ieee80211_free_node(ni);
		URTW_UNLOCK(sc);
		return (ENOBUFS);		/* XXX */
	}

	ifp->if_opackets++;
	if (urtw_tx_start(sc, ni, m, URTW_PRIORITY_LOW) != 0) {
		ieee80211_free_node(ni);
		ifp->if_oerrors++;
		URTW_UNLOCK(sc);
		return (EIO);
	}

	sc->sc_txtimer = 5;
	URTW_UNLOCK(sc);
	return (0);
}

static void
urtw_scan_start(struct ieee80211com *ic)
{

	/* XXX do nothing?  */
}

static void
urtw_scan_end(struct ieee80211com *ic)
{

	/* XXX do nothing?  */
}

static void
urtw_set_channel(struct ieee80211com *ic)
{
	struct urtw_softc *sc  = ic->ic_ifp->if_softc;
	struct ifnet *ifp = sc->sc_ifp;

	/*
	 * if the user set a channel explicitly using ifconfig(8) this function
	 * can be called earlier than we're expected that in some cases the
	 * initialization would be failed if setting a channel is called before
	 * the init have done.
	 */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	sc->sc_ctxarg = URTW_SET_CHANNEL;
	usb_add_task(sc->sc_udev, &sc->sc_ctxtask, USB_TASKQ_DRIVER);
}

static void
urtw_update_mcast(struct ifnet *ifp)
{

	/* XXX do nothing?  */
}

static usbd_status
urtw_8225_usb_init(struct urtw_softc *sc)
{
	uint8_t data;
	usbd_status error;

	urtw_write8_m(sc, URTW_RF_PINS_SELECT + 1, 0);
	urtw_write8_m(sc, URTW_GPIO, 0);
	error = urtw_read8e(sc, 0x53, &data);
	if (error)
		goto fail;
	error = urtw_write8e(sc, 0x53, data | (1 << 7));
	if (error)
		goto fail;
	urtw_write8_m(sc, URTW_RF_PINS_SELECT + 1, 4);
	urtw_write8_m(sc, URTW_GPIO, 0x20);
	urtw_write8_m(sc, URTW_GP_ENABLE, 0);

	urtw_write16_m(sc, URTW_RF_PINS_OUTPUT, 0x80);
	urtw_write16_m(sc, URTW_RF_PINS_SELECT, 0x80);
	urtw_write16_m(sc, URTW_RF_PINS_ENABLE, 0x80);

	usbd_delay_ms(sc->sc_udev, 500);
fail:
	return (error);
}

static usbd_status
urtw_8185_rf_pins_enable(struct urtw_softc *sc)
{
	usbd_status error = 0;

	urtw_write16_m(sc, URTW_RF_PINS_ENABLE, 0x1ff7);
fail:
	return (error);
}

static usbd_status
urtw_8187_write_phy(struct urtw_softc *sc, uint8_t addr, uint32_t data)
{
	uint32_t phyw;
	usbd_status error;

	phyw = ((data << 8) | (addr | 0x80));
	urtw_write8_m(sc, URTW_PHY_MAGIC4, ((phyw & 0xff000000) >> 24));
	urtw_write8_m(sc, URTW_PHY_MAGIC3, ((phyw & 0x00ff0000) >> 16));
	urtw_write8_m(sc, URTW_PHY_MAGIC2, ((phyw & 0x0000ff00) >> 8));
	urtw_write8_m(sc, URTW_PHY_MAGIC1, ((phyw & 0x000000ff)));
	usbd_delay_ms(sc->sc_udev, 1);
fail:
	return (error);
}

static usbd_status
urtw_8187_write_phy_ofdm_c(struct urtw_softc *sc, uint8_t addr, uint32_t data)
{

	data = data & 0xff;
	return urtw_8187_write_phy(sc, addr, data);
}

static usbd_status
urtw_8187_write_phy_cck_c(struct urtw_softc *sc, uint8_t addr, uint32_t data)
{

	data = data & 0xff;
	return urtw_8187_write_phy(sc, addr, data | 0x10000);
}

static usbd_status
urtw_8225_setgain(struct urtw_softc *sc, int16_t gain)
{
	usbd_status error;

	urtw_8187_write_phy_ofdm(sc, 0x0d, urtw_8225_gain[gain * 4]);
	urtw_8187_write_phy_ofdm(sc, 0x1b, urtw_8225_gain[gain * 4 + 2]);
	urtw_8187_write_phy_ofdm(sc, 0x1d, urtw_8225_gain[gain * 4 + 3]);
	urtw_8187_write_phy_ofdm(sc, 0x23, urtw_8225_gain[gain * 4 + 1]);
fail:
	return (error);
}

static usbd_status
urtw_8225_set_txpwrlvl(struct urtw_softc *sc, int chan)
{
	int i, idx, set;
	uint8_t *cck_pwltable;
	uint8_t cck_pwrlvl_max, ofdm_pwrlvl_min, ofdm_pwrlvl_max;
	uint8_t cck_pwrlvl = sc->sc_txpwr_cck[chan] & 0xff;
	uint8_t ofdm_pwrlvl = sc->sc_txpwr_ofdm[chan] & 0xff;
	usbd_status error;

	cck_pwrlvl_max = 11;
	ofdm_pwrlvl_max = 25;	/* 12 -> 25  */
	ofdm_pwrlvl_min = 10;

	/* CCK power setting */
	cck_pwrlvl = (cck_pwrlvl > cck_pwrlvl_max) ? cck_pwrlvl_max : cck_pwrlvl;
	idx = cck_pwrlvl % 6;
	set = cck_pwrlvl / 6;
	cck_pwltable = (chan == 14) ? urtw_8225_txpwr_cck_ch14 :
	    urtw_8225_txpwr_cck;

	urtw_write8_m(sc, URTW_TX_GAIN_CCK,
	    urtw_8225_tx_gain_cck_ofdm[set] >> 1);
	for (i = 0; i < 8; i++) {
		urtw_8187_write_phy_cck(sc, 0x44 + i,
		    cck_pwltable[idx * 8 + i]);
	}
	usbd_delay_ms(sc->sc_udev, 1);

	/* OFDM power setting */
	ofdm_pwrlvl = (ofdm_pwrlvl > (ofdm_pwrlvl_max - ofdm_pwrlvl_min)) ?
	    ofdm_pwrlvl_max : ofdm_pwrlvl + ofdm_pwrlvl_min;
	ofdm_pwrlvl = (ofdm_pwrlvl > 35) ? 35 : ofdm_pwrlvl;

	idx = ofdm_pwrlvl % 6;
	set = ofdm_pwrlvl / 6;

	error = urtw_8185_set_anaparam2(sc, URTW_8225_ANAPARAM2_ON);
	if (error)
		goto fail;
	urtw_8187_write_phy_ofdm(sc, 2, 0x42);
	urtw_8187_write_phy_ofdm(sc, 6, 0);
	urtw_8187_write_phy_ofdm(sc, 8, 0);

	urtw_write8_m(sc, URTW_TX_GAIN_OFDM,
	    urtw_8225_tx_gain_cck_ofdm[set] >> 1);
	urtw_8187_write_phy_ofdm(sc, 0x5, urtw_8225_txpwr_ofdm[idx]);
	urtw_8187_write_phy_ofdm(sc, 0x7, urtw_8225_txpwr_ofdm[idx]);
	usbd_delay_ms(sc->sc_udev, 1);
fail:
	return (error);
}

static usbd_status
urtw_8185_tx_antenna(struct urtw_softc *sc, uint8_t ant)
{
	usbd_status error;

	urtw_write8_m(sc, URTW_TX_ANTENNA, ant);
	usbd_delay_ms(sc->sc_udev, 1);
fail:
	return (error);
}

static usbd_status
urtw_8225_rf_init(struct urtw_softc *sc)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	int i;
	uint16_t data;
	usbd_status error;

	error = urtw_8180_set_anaparam(sc, URTW_8225_ANAPARAM_ON);
	if (error)
		goto fail;

	error = urtw_8225_usb_init(sc);
	if (error)
		goto fail;

	urtw_write32_m(sc, URTW_RF_TIMING, 0x000a8008);
	urtw_read16_m(sc, URTW_BRSR, &data);		/* XXX ??? */
	urtw_write16_m(sc, URTW_BRSR, 0xffff);
	urtw_write32_m(sc, URTW_RF_PARA, 0x100044);

	error = urtw_set_mode(sc, URTW_EPROM_CMD_CONFIG);
	if (error)
		goto fail;
	urtw_write8_m(sc, URTW_CONFIG3, 0x44);
	error = urtw_set_mode(sc, URTW_EPROM_CMD_NORMAL);
	if (error)
		goto fail;

	error = urtw_8185_rf_pins_enable(sc);
	if (error)
		goto fail;
	usbd_delay_ms(sc->sc_udev, 1000);

	for (i = 0; i < N(urtw_8225_rf_part1); i++) {
		urtw_8225_write(sc, urtw_8225_rf_part1[i].reg,
		    urtw_8225_rf_part1[i].val);
		usbd_delay_ms(sc->sc_udev, 1);
	}
	usbd_delay_ms(sc->sc_udev, 100);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_2_MAGIC, URTW_8225_ADDR_2_DATA_MAGIC1);
	usbd_delay_ms(sc->sc_udev, 200);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_2_MAGIC, URTW_8225_ADDR_2_DATA_MAGIC2);
	usbd_delay_ms(sc->sc_udev, 200);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_0_MAGIC, URTW_8225_ADDR_0_DATA_MAGIC3);

	for (i = 0; i < 95; i++) {
		urtw_8225_write(sc, URTW_8225_ADDR_1_MAGIC, (uint8_t)(i + 1));
		urtw_8225_write(sc, URTW_8225_ADDR_2_MAGIC, urtw_8225_rxgain[i]);
	}

	urtw_8225_write(sc,
	    URTW_8225_ADDR_0_MAGIC, URTW_8225_ADDR_0_DATA_MAGIC4);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_0_MAGIC, URTW_8225_ADDR_0_DATA_MAGIC5);

	for (i = 0; i < 128; i++) {
		urtw_8187_write_phy_ofdm(sc, 0xb, urtw_8225_agc[i]);
		usbd_delay_ms(sc->sc_udev, 1);
		urtw_8187_write_phy_ofdm(sc, 0xa, (uint8_t)i + 0x80);
		usbd_delay_ms(sc->sc_udev, 1);
	}

	for (i = 0; i < N(urtw_8225_rf_part2); i++) {
		urtw_8187_write_phy_ofdm(sc, urtw_8225_rf_part2[i].reg,
		    urtw_8225_rf_part2[i].val);
		usbd_delay_ms(sc->sc_udev, 1);
	}

	error = urtw_8225_setgain(sc, 4);
	if (error)
		goto fail;

	for (i = 0; i < N(urtw_8225_rf_part3); i++) {
		urtw_8187_write_phy_cck(sc, urtw_8225_rf_part3[i].reg,
		    urtw_8225_rf_part3[i].val);
		usbd_delay_ms(sc->sc_udev, 1);
	}

	urtw_write8_m(sc, URTW_ADDR_MAGIC4, 0x0d);

	error = urtw_8225_set_txpwrlvl(sc, 1);
	if (error)
		goto fail;

	urtw_8187_write_phy_cck(sc, 0x10, 0x9b);
	usbd_delay_ms(sc->sc_udev, 1);
	urtw_8187_write_phy_ofdm(sc, 0x26, 0x90);
	usbd_delay_ms(sc->sc_udev, 1);

	/* TX ant A, 0x0 for B */
	error = urtw_8185_tx_antenna(sc, 0x3);
	if (error)
		goto fail;
	urtw_write32_m(sc, URTW_ADDR_MAGIC5, 0x3dc00002);

	error = urtw_8225_rf_set_chan(sc, 1);
fail:
	return (error);
#undef N
}

static usbd_status
urtw_8225_rf_set_chan(struct urtw_softc *sc, int chan)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211_channel *c = ic->ic_curchan;
	usbd_status error;

	error = urtw_8225_set_txpwrlvl(sc, chan);
	if (error)
		goto fail;
	urtw_8225_write(sc, URTW_8225_ADDR_7_MAGIC, urtw_8225_channel[chan]);
	usbd_delay_ms(sc->sc_udev, 10);

	urtw_write8_m(sc, URTW_SIFS, 0x22);

	if (sc->sc_state == IEEE80211_S_ASSOC &&
	    ic->ic_flags & IEEE80211_F_SHSLOT)
		urtw_write8_m(sc, URTW_SLOT, 0x9);
	else
		urtw_write8_m(sc, URTW_SLOT, 0x14);

	if (IEEE80211_IS_CHAN_G(c)) {
		/* for G */
		urtw_write8_m(sc, URTW_DIFS, 0x14);
		urtw_write8_m(sc, URTW_EIFS, 0x5b - 0x14);
		urtw_write8_m(sc, URTW_CW_VAL, 0x73);
	} else {
		/* for B */
		urtw_write8_m(sc, URTW_DIFS, 0x24);
		urtw_write8_m(sc, URTW_EIFS, 0x5b - 0x24);
		urtw_write8_m(sc, URTW_CW_VAL, 0xa5);
	}

fail:
	return (error);
}

static usbd_status
urtw_8225_rf_set_sens(struct urtw_softc *sc, int sens)
{
	usbd_status error;

	if (sens < 0 || sens > 6)
		return -1;

	if (sens > 4)
		urtw_8225_write(sc,
		    URTW_8225_ADDR_C_MAGIC, URTW_8225_ADDR_C_DATA_MAGIC1);
	else
		urtw_8225_write(sc,
		    URTW_8225_ADDR_C_MAGIC, URTW_8225_ADDR_C_DATA_MAGIC2);

	sens = 6 - sens;
	error = urtw_8225_setgain(sc, sens);
	if (error)
		goto fail;

	urtw_8187_write_phy_cck(sc, 0x41, urtw_8225_threshold[sens]);

fail:
	return (error);
}

static void
urtw_stop(struct ifnet *ifp, int disable)
{
	struct urtw_softc *sc = ifp->if_softc;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->sc_led_ch);
	callout_stop(&sc->sc_watchdog_ch);

	if (sc->sc_rxpipe != NULL)
		usbd_abort_pipe(sc->sc_rxpipe);
	if (sc->sc_txpipe_low != NULL)
		usbd_abort_pipe(sc->sc_txpipe_low);
	if (sc->sc_txpipe_normal != NULL)
		usbd_abort_pipe(sc->sc_txpipe_normal);
}

static int
urtw_isbmode(uint16_t rate)
{

	rate = urtw_rtl2rate(rate);

	return ((rate <= 22 && rate != 12 && rate != 18) ||
	    rate == 44) ? (1) : (0);
}

static void
urtw_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	int actlen, flen, len, nf, rssi;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;
	struct urtw_data *data = priv;
	struct urtw_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint8_t *desc, quality, rate;
	usbd_status error;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rxpipe);
		ifp->if_ierrors++;
		goto skip;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &actlen, NULL);
	if (actlen < URTW_MIN_RXBUFSZ) {
		ifp->if_ierrors++;
		goto skip;
	}

	/* 4 dword and 4 byte CRC  */
	len = actlen - (4 * 4);
	desc = data->buf + len;
	flen = ((desc[1] & 0x0f) << 8) + (desc[0] & 0xff);
	if (flen > actlen) {
		ifp->if_ierrors++;
		goto skip;
	}

	rate = (desc[2] & 0xf0) >> 4;
	quality = desc[4] & 0xff;
	/* XXX correct?  */
	rssi = (desc[6] & 0xfe) >> 1;
	if (!urtw_isbmode(rate)) {
		rssi = (rssi > 90) ? 90 : ((rssi < 25) ? 25 : rssi);
		rssi = ((90 - rssi) * 100) / 65;
	} else {
		rssi = (rssi > 90) ? 95 : ((rssi < 30) ? 30 : rssi);
		rssi = ((95 - rssi) * 100) / 65;
	}

	mnew = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (mnew == NULL) {
		ifp->if_ierrors++;
		goto skip;
	}

	m = data->m;
	data->m = mnew;
	data->buf = mtod(mnew, uint8_t *);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = flen - 4;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct urtw_rx_radiotap_header *tap = &sc->sc_rxtap;

		/* XXX Are variables correct?  */
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_dbm_antsignal = (int8_t)rssi;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
	}

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA)
		sc->sc_currate = (rate > 0) ? rate : sc->sc_currate;
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);
	/* XXX correct?  */
	nf = (quality > 64) ? 0 : ((64 - quality) * 100) / 64;
	/* send the frame to the 802.11 layer */
	if (ni != NULL) {
		(void) ieee80211_input(ni, m, rssi, -nf, 0);
		/* node is no longer needed */
		ieee80211_free_node(ni);
	} else
		(void) ieee80211_input_all(ic, m, rssi, -nf, 0);

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->sc_rxpipe, data, data->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, urtw_rxeof);
	error = usbd_transfer(xfer);
	if (error != USBD_IN_PROGRESS && error != 0)
		device_printf(sc->sc_dev, "could not queue Rx transfer\n");
}

static int
urtw_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct urtw_vap *rvp = URTW_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct urtw_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, URTW_DEBUG_STATE, "%s: %s -> %s\n", __func__,
	    ieee80211_state_name[vap->iv_state],
	    ieee80211_state_name[nstate]);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;

	if (nstate == IEEE80211_S_INIT) {
		rvp->newstate(vap, nstate, arg);
		return (0);
	} else {
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);
		return (EINPROGRESS);
	}
}

static usbd_status
urtw_8225v2_setgain(struct urtw_softc *sc, int16_t gain)
{
	uint8_t *gainp;
	usbd_status error;

	/* XXX for A?  */
	gainp = urtw_8225v2_gain_bg;
	urtw_8187_write_phy_ofdm(sc, 0x0d, gainp[gain * 3]);
	usbd_delay_ms(sc->sc_udev, 1);
	urtw_8187_write_phy_ofdm(sc, 0x1b, gainp[gain * 3 + 1]);
	usbd_delay_ms(sc->sc_udev, 1);
	urtw_8187_write_phy_ofdm(sc, 0x1d, gainp[gain * 3 + 2]);
	usbd_delay_ms(sc->sc_udev, 1);
	urtw_8187_write_phy_ofdm(sc, 0x21, 0x17);
	usbd_delay_ms(sc->sc_udev, 1);
fail:
	return (error);
}

static usbd_status
urtw_8225v2_set_txpwrlvl(struct urtw_softc *sc, int chan)
{
	int i;
	uint8_t *cck_pwrtable;
	uint8_t cck_pwrlvl_max = 15, ofdm_pwrlvl_max = 25, ofdm_pwrlvl_min = 10;
	uint8_t cck_pwrlvl = sc->sc_txpwr_cck[chan] & 0xff;
	uint8_t ofdm_pwrlvl = sc->sc_txpwr_ofdm[chan] & 0xff;
	usbd_status error;

	/* CCK power setting */
	cck_pwrlvl = (cck_pwrlvl > cck_pwrlvl_max) ? cck_pwrlvl_max : cck_pwrlvl;
	cck_pwrlvl += sc->sc_txpwr_cck_base;
	cck_pwrlvl = (cck_pwrlvl > 35) ? 35 : cck_pwrlvl;
	cck_pwrtable = (chan == 14) ? urtw_8225v2_txpwr_cck_ch14 :
	    urtw_8225v2_txpwr_cck;

	for (i = 0; i < 8; i++)
		urtw_8187_write_phy_cck(sc, 0x44 + i, cck_pwrtable[i]);

	urtw_write8_m(sc, URTW_TX_GAIN_CCK,
	    urtw_8225v2_tx_gain_cck_ofdm[cck_pwrlvl]);
	usbd_delay_ms(sc->sc_udev, 1);

	/* OFDM power setting */
	ofdm_pwrlvl = (ofdm_pwrlvl > (ofdm_pwrlvl_max - ofdm_pwrlvl_min)) ?
		ofdm_pwrlvl_max : ofdm_pwrlvl + ofdm_pwrlvl_min;
	ofdm_pwrlvl += sc->sc_txpwr_ofdm_base;
	ofdm_pwrlvl = (ofdm_pwrlvl > 35) ? 35 : ofdm_pwrlvl;

	error = urtw_8185_set_anaparam2(sc, URTW_8225_ANAPARAM2_ON);
	if (error)
		goto fail;

	urtw_8187_write_phy_ofdm(sc, 2, 0x42);
	urtw_8187_write_phy_ofdm(sc, 5, 0x0);
	urtw_8187_write_phy_ofdm(sc, 6, 0x40);
	urtw_8187_write_phy_ofdm(sc, 7, 0x0);
	urtw_8187_write_phy_ofdm(sc, 8, 0x40);

	urtw_write8_m(sc, URTW_TX_GAIN_OFDM,
	    urtw_8225v2_tx_gain_cck_ofdm[ofdm_pwrlvl]);
	usbd_delay_ms(sc->sc_udev, 1);
fail:
	return (error);
}

static usbd_status
urtw_8225v2_rf_init(struct urtw_softc *sc)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	int i;
	uint16_t data;
	uint32_t data32;
	usbd_status error;

	error = urtw_8180_set_anaparam(sc, URTW_8225_ANAPARAM_ON);
	if (error)
		goto fail;

	error = urtw_8225_usb_init(sc);
	if (error)
		goto fail;

	urtw_write32_m(sc, URTW_RF_TIMING, 0x000a8008);
	urtw_read16_m(sc, URTW_BRSR, &data);		/* XXX ??? */
	urtw_write16_m(sc, URTW_BRSR, 0xffff);
	urtw_write32_m(sc, URTW_RF_PARA, 0x100044);

	error = urtw_set_mode(sc, URTW_EPROM_CMD_CONFIG);
	if (error)
		goto fail;
	urtw_write8_m(sc, URTW_CONFIG3, 0x44);
	error = urtw_set_mode(sc, URTW_EPROM_CMD_NORMAL);
	if (error)
		goto fail;

	error = urtw_8185_rf_pins_enable(sc);
	if (error)
		goto fail;

	usbd_delay_ms(sc->sc_udev, 500);

	for (i = 0; i < N(urtw_8225v2_rf_part1); i++) {
		urtw_8225_write(sc, urtw_8225v2_rf_part1[i].reg,
		    urtw_8225v2_rf_part1[i].val);
	}
	usbd_delay_ms(sc->sc_udev, 50);

	urtw_8225_write(sc,
	    URTW_8225_ADDR_0_MAGIC, URTW_8225_ADDR_0_DATA_MAGIC1);

	for (i = 0; i < 95; i++) {
		urtw_8225_write(sc, URTW_8225_ADDR_1_MAGIC, (uint8_t)(i + 1));
		urtw_8225_write(sc, URTW_8225_ADDR_2_MAGIC,
		    urtw_8225v2_rxgain[i]);
	}

	urtw_8225_write(sc,
	    URTW_8225_ADDR_3_MAGIC, URTW_8225_ADDR_3_DATA_MAGIC1);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_5_MAGIC, URTW_8225_ADDR_5_DATA_MAGIC1);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_0_MAGIC, URTW_8225_ADDR_0_DATA_MAGIC2);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_2_MAGIC, URTW_8225_ADDR_2_DATA_MAGIC1);
	usbd_delay_ms(sc->sc_udev, 100);
	urtw_8225_write(sc,
	    URTW_8225_ADDR_2_MAGIC, URTW_8225_ADDR_2_DATA_MAGIC2);
	usbd_delay_ms(sc->sc_udev, 100);

	error = urtw_8225_read(sc, URTW_8225_ADDR_6_MAGIC, &data32);
	if (error != 0)
		goto fail;
	if (data32 != URTW_8225_ADDR_6_DATA_MAGIC1)
		device_printf(sc->sc_dev, "expect 0xe6!! (0x%x)\n", data32);
	if (!(data32 & URTW_8225_ADDR_6_DATA_MAGIC2)) {
		urtw_8225_write(sc,
		    URTW_8225_ADDR_2_MAGIC, URTW_8225_ADDR_2_DATA_MAGIC1);
		usbd_delay_ms(sc->sc_udev, 100);
		urtw_8225_write(sc,
		    URTW_8225_ADDR_2_MAGIC, URTW_8225_ADDR_2_DATA_MAGIC2);
		usbd_delay_ms(sc->sc_udev, 50);
		error = urtw_8225_read(sc, URTW_8225_ADDR_6_MAGIC, &data32);
		if (error != 0)
			goto fail;
		if (!(data32 & URTW_8225_ADDR_6_DATA_MAGIC2))
			device_printf(sc->sc_dev, "RF calibration failed\n");
	}
	usbd_delay_ms(sc->sc_udev, 100);

	urtw_8225_write(sc,
	    URTW_8225_ADDR_0_MAGIC, URTW_8225_ADDR_0_DATA_MAGIC6);
	for (i = 0; i < 128; i++) {
		urtw_8187_write_phy_ofdm(sc, 0xb, urtw_8225_agc[i]);
		urtw_8187_write_phy_ofdm(sc, 0xa, (uint8_t)i + 0x80);
	}

	for (i = 0; i < N(urtw_8225v2_rf_part2); i++) {
		urtw_8187_write_phy_ofdm(sc, urtw_8225v2_rf_part2[i].reg,
		    urtw_8225v2_rf_part2[i].val);
	}

	error = urtw_8225v2_setgain(sc, 4);
	if (error)
		goto fail;

	for (i = 0; i < N(urtw_8225v2_rf_part3); i++) {
		urtw_8187_write_phy_cck(sc, urtw_8225v2_rf_part3[i].reg,
		    urtw_8225v2_rf_part3[i].val);
	}

	urtw_write8_m(sc, URTW_ADDR_MAGIC4, 0x0d);

	error = urtw_8225v2_set_txpwrlvl(sc, 1);
	if (error)
		goto fail;

	urtw_8187_write_phy_cck(sc, 0x10, 0x9b);
	urtw_8187_write_phy_ofdm(sc, 0x26, 0x90);

	/* TX ant A, 0x0 for B */
	error = urtw_8185_tx_antenna(sc, 0x3);
	if (error)
		goto fail;
	urtw_write32_m(sc, URTW_ADDR_MAGIC5, 0x3dc00002);

	error = urtw_8225_rf_set_chan(sc, 1);
fail:
	return (error);
#undef N
}

static usbd_status
urtw_8225v2_rf_set_chan(struct urtw_softc *sc, int chan)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211_channel *c = ic->ic_curchan;
	usbd_status error;

	error = urtw_8225v2_set_txpwrlvl(sc, chan);
	if (error)
		goto fail;

	urtw_8225_write(sc, URTW_8225_ADDR_7_MAGIC, urtw_8225_channel[chan]);
	usbd_delay_ms(sc->sc_udev, 10);

	urtw_write8_m(sc, URTW_SIFS, 0x22);

	if(sc->sc_state == IEEE80211_S_ASSOC &&
	    ic->ic_flags & IEEE80211_F_SHSLOT)
		urtw_write8_m(sc, URTW_SLOT, 0x9);
	else
		urtw_write8_m(sc, URTW_SLOT, 0x14);

	if (IEEE80211_IS_CHAN_G(c)) {
		/* for G */
		urtw_write8_m(sc, URTW_DIFS, 0x14);
		urtw_write8_m(sc, URTW_EIFS, 0x5b - 0x14);
		urtw_write8_m(sc, URTW_CW_VAL, 0x73);
	} else {
		/* for B */
		urtw_write8_m(sc, URTW_DIFS, 0x24);
		urtw_write8_m(sc, URTW_EIFS, 0x5b - 0x24);
		urtw_write8_m(sc, URTW_CW_VAL, 0xa5);
	}

fail:
	return (error);
}

static void
urtw_ctxtask(void *arg)
{
	struct urtw_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t data;
	usbd_status error;

	switch (sc->sc_ctxarg) {
	case URTW_SET_CHANNEL:
		/*
		 * during changing th channel we need to temporarily be disable 
		 * TX.
		 */
		urtw_read32_m(sc, URTW_TX_CONF, &data);
		data &= ~URTW_TX_LOOPBACK_MASK;
		urtw_write32_m(sc, URTW_TX_CONF, data | URTW_TX_LOOPBACK_MAC);
		error = sc->sc_rf_set_chan(sc,
		    ieee80211_chan2ieee(ic, ic->ic_curchan));
		if (error != 0)
			goto fail;
		usbd_delay_ms(sc->sc_udev, 10);
		urtw_write32_m(sc, URTW_TX_CONF, data | URTW_TX_LOOPBACK_NONE);
		break;
	default:
		panic("unknown argument.\n");
	}

fail:
	if (error != 0)
		device_printf(sc->sc_dev, "could not change the channel\n");
	return;
}

static void
urtw_task(void *arg)
{
	struct urtw_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = vap->iv_bss;
	struct urtw_vap *uvp = URTW_VAP(vap);
	usbd_status error = 0;

	switch (sc->sc_state) {
	case IEEE80211_S_RUN:
		/* setting bssid.  */
		urtw_write32_m(sc, URTW_BSSID, ((uint32_t *)ni->ni_bssid)[0]);
		urtw_write16_m(sc, URTW_BSSID + 4,
		    ((uint16_t *)ni->ni_bssid)[2]);
		urtw_update_msr(sc);
		/* XXX maybe the below would be incorrect.  */
		urtw_write16_m(sc, URTW_ATIM_WND, 2);
		urtw_write16_m(sc, URTW_ATIM_TR_ITV, 100);
		urtw_write16_m(sc, URTW_BEACON_INTERVAL, 0x64);
		urtw_write16_m(sc, URTW_BEACON_INTERVAL_TIME, 100);
		error = urtw_led_ctl(sc, URTW_LED_CTL_LINK);
		if (error != 0)
			device_printf(sc->sc_dev,
			    "could not control LED (%d)\n", error);
		break;
	default:
		break;
	}

fail:
	if (error != 0)
		printf("error duing processing RUN state.");

	IEEE80211_LOCK(ic);
	uvp->newstate(vap, sc->sc_state, sc->sc_arg);
	if (vap->iv_newstate_cb != NULL)
		vap->iv_newstate_cb(vap, sc->sc_state, sc->sc_arg);
	IEEE80211_UNLOCK(ic);
}

static void
urtw_watchdog(void *arg)
{
	struct urtw_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->sc_txtimer > 0) {
		if (--sc->sc_txtimer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			ifp->if_oerrors++;
			return;
		}
		callout_reset(&sc->sc_watchdog_ch, hz, urtw_watchdog, sc);
	}
}

static device_method_t urtw_methods[] = {
	DEVMETHOD(device_probe, urtw_match),
	DEVMETHOD(device_attach, urtw_attach),
	DEVMETHOD(device_detach, urtw_detach),
	{ 0, 0 }
};
static driver_t urtw_driver = {
	"urtw",
	urtw_methods,
	sizeof(struct urtw_softc)
};
static devclass_t urtw_devclass;

DRIVER_MODULE(urtw, uhub, urtw_driver, urtw_devclass, usbd_driver_load, 0);
MODULE_DEPEND(urtw, wlan, 1, 1, 1);
MODULE_DEPEND(urtw, usb, 1, 1, 1);
