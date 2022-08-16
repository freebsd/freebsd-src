/*
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Alexander Motin <mav@FreeBSD.org>
 * Copyright (c) 2015 Peter Grehan <grehan@freebsd.org>
 * Copyright (c) 2013 Jeremiah Lott, Avere Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <sys/types.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/limits.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <machine/vmm_snapshot.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <pthread.h>
#include <pthread_np.h>

#include "e1000_regs.h"
#include "e1000_defines.h"
#include "mii.h"

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"
#include "mevent.h"
#include "net_utils.h"
#include "net_backends.h"

/* Hardware/register definitions XXX: move some to common code. */
#define E82545_VENDOR_ID_INTEL			0x8086
#define E82545_DEV_ID_82545EM_COPPER		0x100F
#define E82545_SUBDEV_ID			0x1008

#define E82545_REVISION_4			4

#define E82545_MDIC_DATA_MASK			0x0000FFFF
#define E82545_MDIC_OP_MASK			0x0c000000
#define E82545_MDIC_IE				0x20000000

#define E82545_EECD_FWE_DIS	0x00000010 /* Flash writes disabled */
#define E82545_EECD_FWE_EN	0x00000020 /* Flash writes enabled */
#define E82545_EECD_FWE_MASK	0x00000030 /* Flash writes mask */

#define E82545_BAR_REGISTER			0
#define E82545_BAR_REGISTER_LEN			(128*1024)
#define E82545_BAR_FLASH			1
#define E82545_BAR_FLASH_LEN			(64*1024)
#define E82545_BAR_IO				2
#define E82545_BAR_IO_LEN			8

#define E82545_IOADDR				0x00000000
#define E82545_IODATA				0x00000004
#define E82545_IO_REGISTER_MAX			0x0001FFFF
#define E82545_IO_FLASH_BASE			0x00080000
#define E82545_IO_FLASH_MAX			0x000FFFFF

#define E82545_ARRAY_ENTRY(reg, offset)		(reg + (offset<<2))
#define E82545_RAR_MAX				15
#define E82545_MTA_MAX				127
#define E82545_VFTA_MAX				127

/* Slightly modified from the driver versions, hardcoded for 3 opcode bits,
 * followed by 6 address bits.
 * TODO: make opcode bits and addr bits configurable?
 * NVM Commands - Microwire */
#define E82545_NVM_OPCODE_BITS	3
#define E82545_NVM_ADDR_BITS	6
#define E82545_NVM_DATA_BITS	16
#define E82545_NVM_OPADDR_BITS	(E82545_NVM_OPCODE_BITS + E82545_NVM_ADDR_BITS)
#define E82545_NVM_ADDR_MASK	((1 << E82545_NVM_ADDR_BITS)-1)
#define E82545_NVM_OPCODE_MASK	\
    (((1 << E82545_NVM_OPCODE_BITS) - 1) << E82545_NVM_ADDR_BITS)
#define E82545_NVM_OPCODE_READ	(0x6 << E82545_NVM_ADDR_BITS)	/* read */
#define E82545_NVM_OPCODE_WRITE	(0x5 << E82545_NVM_ADDR_BITS)	/* write */
#define E82545_NVM_OPCODE_ERASE	(0x7 << E82545_NVM_ADDR_BITS)	/* erase */
#define	E82545_NVM_OPCODE_EWEN	(0x4 << E82545_NVM_ADDR_BITS)	/* wr-enable */

#define	E82545_NVM_EEPROM_SIZE	64 /* 64 * 16-bit values == 128K */

#define E1000_ICR_SRPD		0x00010000

/* This is an arbitrary number.  There is no hard limit on the chip. */
#define I82545_MAX_TXSEGS	64

/* Legacy receive descriptor */
struct e1000_rx_desc {
	uint64_t buffer_addr;	/* Address of the descriptor's data buffer */
	uint16_t length;	/* Length of data DMAed into data buffer */
	uint16_t csum;		/* Packet checksum */
	uint8_t	 status;       	/* Descriptor status */
	uint8_t  errors;	/* Descriptor Errors */
	uint16_t special;
};

/* Transmit descriptor types */
#define	E1000_TXD_MASK		(E1000_TXD_CMD_DEXT | 0x00F00000)
#define E1000_TXD_TYP_L		(0)
#define E1000_TXD_TYP_C		(E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_C)
#define E1000_TXD_TYP_D		(E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D)

/* Legacy transmit descriptor */
struct e1000_tx_desc {
	uint64_t buffer_addr;   /* Address of the descriptor's data buffer */
	union {
		uint32_t data;
		struct {
			uint16_t length;  /* Data buffer length */
			uint8_t  cso;  /* Checksum offset */
			uint8_t  cmd;  /* Descriptor control */
		} flags;
	} lower;
	union {
		uint32_t data;
		struct {
			uint8_t status; /* Descriptor status */
			uint8_t css;  /* Checksum start */
			uint16_t special;
		} fields;
	} upper;
};

/* Context descriptor */
struct e1000_context_desc {
	union {
		uint32_t ip_config;
		struct {
			uint8_t ipcss;  /* IP checksum start */
			uint8_t ipcso;  /* IP checksum offset */
			uint16_t ipcse;  /* IP checksum end */
		} ip_fields;
	} lower_setup;
	union {
		uint32_t tcp_config;
		struct {
			uint8_t tucss;  /* TCP checksum start */
			uint8_t tucso;  /* TCP checksum offset */
			uint16_t tucse;  /* TCP checksum end */
		} tcp_fields;
	} upper_setup;
	uint32_t cmd_and_length;
	union {
		uint32_t data;
		struct {
			uint8_t status;  /* Descriptor status */
			uint8_t hdr_len;  /* Header length */
			uint16_t mss;  /* Maximum segment size */
		} fields;
	} tcp_seg_setup;
};

/* Data descriptor */
struct e1000_data_desc {
	uint64_t buffer_addr;  /* Address of the descriptor's buffer address */
	union {
		uint32_t data;
		struct {
			uint16_t length;  /* Data buffer length */
			uint8_t typ_len_ext;
			uint8_t cmd;
		} flags;
	} lower;
	union {
		uint32_t data;
		struct {
			uint8_t status;  /* Descriptor status */
			uint8_t popts;  /* Packet Options */
			uint16_t special;
		} fields;
	} upper;
};

union e1000_tx_udesc {
	struct e1000_tx_desc td;
	struct e1000_context_desc cd;
	struct e1000_data_desc dd;
};

/* Tx checksum info for a packet. */
struct ck_info {
	int	ck_valid;	/* ck_info is valid */
	uint8_t	ck_start;	/* start byte of cksum calcuation */
	uint8_t	ck_off;		/* offset of cksum insertion */
	uint16_t ck_len;	/* length of cksum calc: 0 is to packet-end */
};

/*
 * Debug printf
 */
static int e82545_debug = 0;
#define WPRINTF(msg,params...) PRINTLN("e82545: " msg, params)
#define DPRINTF(msg,params...) if (e82545_debug) WPRINTF(msg, params)

#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))

/* s/w representation of the RAL/RAH regs */
struct  eth_uni {
	int		eu_valid;
	int		eu_addrsel;
	struct ether_addr eu_eth;
};


struct e82545_softc {
	struct pci_devinst *esc_pi;
	struct vmctx	*esc_ctx;
	struct mevent   *esc_mevpitr;
	pthread_mutex_t	esc_mtx;
	struct ether_addr esc_mac;
	net_backend_t	*esc_be;

	/* General */
	uint32_t	esc_CTRL;	/* x0000 device ctl */
	uint32_t	esc_FCAL;	/* x0028 flow ctl addr lo */
	uint32_t	esc_FCAH;	/* x002C flow ctl addr hi */
	uint32_t	esc_FCT;	/* x0030 flow ctl type */
	uint32_t	esc_VET;	/* x0038 VLAN eth type */
	uint32_t	esc_FCTTV;	/* x0170 flow ctl tx timer */
	uint32_t	esc_LEDCTL;	/* x0E00 LED control */
	uint32_t	esc_PBA;	/* x1000 pkt buffer allocation */

	/* Interrupt control */
	int		esc_irq_asserted;
	uint32_t	esc_ICR;	/* x00C0 cause read/clear */
	uint32_t	esc_ITR;	/* x00C4 intr throttling */
	uint32_t	esc_ICS;	/* x00C8 cause set */
	uint32_t	esc_IMS;	/* x00D0 mask set/read */
	uint32_t	esc_IMC;	/* x00D8 mask clear */

	/* Transmit */
	union e1000_tx_udesc *esc_txdesc;
	struct e1000_context_desc esc_txctx;
	pthread_t	esc_tx_tid;
	pthread_cond_t	esc_tx_cond;
	int		esc_tx_enabled;
	int		esc_tx_active;
	uint32_t	esc_TXCW;	/* x0178 transmit config */
	uint32_t	esc_TCTL;	/* x0400 transmit ctl */
	uint32_t	esc_TIPG;	/* x0410 inter-packet gap */
	uint16_t	esc_AIT;	/* x0458 Adaptive Interframe Throttle */
	uint64_t	esc_tdba;      	/* verified 64-bit desc table addr */
	uint32_t	esc_TDBAL;	/* x3800 desc table addr, low bits */
	uint32_t	esc_TDBAH;	/* x3804 desc table addr, hi 32-bits */
	uint32_t	esc_TDLEN;	/* x3808 # descriptors in bytes */
	uint16_t	esc_TDH;	/* x3810 desc table head idx */
	uint16_t	esc_TDHr;	/* internal read version of TDH */
	uint16_t	esc_TDT;	/* x3818 desc table tail idx */
	uint32_t	esc_TIDV;	/* x3820 intr delay */
	uint32_t	esc_TXDCTL;	/* x3828 desc control */
	uint32_t	esc_TADV;	/* x382C intr absolute delay */

	/* L2 frame acceptance */
	struct eth_uni	esc_uni[16];	/* 16 x unicast MAC addresses */
	uint32_t	esc_fmcast[128]; /* Multicast filter bit-match */
	uint32_t	esc_fvlan[128]; /* VLAN 4096-bit filter */

	/* Receive */
	struct e1000_rx_desc *esc_rxdesc;
	pthread_cond_t	esc_rx_cond;
	int		esc_rx_enabled;
	int		esc_rx_active;
	int		esc_rx_loopback;
	uint32_t	esc_RCTL;	/* x0100 receive ctl */
	uint32_t	esc_FCRTL;	/* x2160 flow cntl thresh, low */
	uint32_t	esc_FCRTH;	/* x2168 flow cntl thresh, hi */
	uint64_t	esc_rdba;	/* verified 64-bit desc table addr */
	uint32_t	esc_RDBAL;	/* x2800 desc table addr, low bits */
	uint32_t	esc_RDBAH;	/* x2804 desc table addr, hi 32-bits*/
	uint32_t	esc_RDLEN;	/* x2808 #descriptors */
	uint16_t	esc_RDH;	/* x2810 desc table head idx */
	uint16_t	esc_RDT;	/* x2818 desc table tail idx */
	uint32_t	esc_RDTR;	/* x2820 intr delay */
	uint32_t	esc_RXDCTL;	/* x2828 desc control */
	uint32_t	esc_RADV;	/* x282C intr absolute delay */
	uint32_t	esc_RSRPD;	/* x2C00 recv small packet detect */
	uint32_t	esc_RXCSUM;     /* x5000 receive cksum ctl */

	/* IO Port register access */
	uint32_t io_addr;

	/* Shadow copy of MDIC */
	uint32_t mdi_control;
	/* Shadow copy of EECD */
	uint32_t eeprom_control;
	/* Latest NVM in/out */
	uint16_t nvm_data;
	uint16_t nvm_opaddr;
	/* stats */
	uint32_t missed_pkt_count; /* dropped for no room in rx queue */
	uint32_t pkt_rx_by_size[6];
	uint32_t pkt_tx_by_size[6];
	uint32_t good_pkt_rx_count;
	uint32_t bcast_pkt_rx_count;
	uint32_t mcast_pkt_rx_count;
	uint32_t good_pkt_tx_count;
	uint32_t bcast_pkt_tx_count;
	uint32_t mcast_pkt_tx_count;
	uint32_t oversize_rx_count;
	uint32_t tso_tx_count;
	uint64_t good_octets_rx;
	uint64_t good_octets_tx;
	uint64_t missed_octets; /* counts missed and oversized */

	uint8_t nvm_bits:6; /* number of bits remaining in/out */
	uint8_t nvm_mode:2;
#define E82545_NVM_MODE_OPADDR  0x0
#define E82545_NVM_MODE_DATAIN  0x1
#define E82545_NVM_MODE_DATAOUT 0x2
	/* EEPROM data */
	uint16_t eeprom_data[E82545_NVM_EEPROM_SIZE];
};

static void e82545_reset(struct e82545_softc *sc, int dev);
static void e82545_rx_enable(struct e82545_softc *sc);
static void e82545_rx_disable(struct e82545_softc *sc);
static void e82545_rx_callback(int fd, enum ev_type type, void *param);
static void e82545_tx_start(struct e82545_softc *sc);
static void e82545_tx_enable(struct e82545_softc *sc);
static void e82545_tx_disable(struct e82545_softc *sc);

static inline int
e82545_size_stat_index(uint32_t size)
{
	if (size <= 64) {
		return 0;
	} else if (size >= 1024) {
		return 5;
	} else {
		/* should be 1-4 */
		return (ffs(size) - 6);
	}
}

static void
e82545_init_eeprom(struct e82545_softc *sc)
{
	uint16_t checksum, i;

        /* mac addr */
	sc->eeprom_data[NVM_MAC_ADDR] = ((uint16_t)sc->esc_mac.octet[0]) |
		(((uint16_t)sc->esc_mac.octet[1]) << 8);
	sc->eeprom_data[NVM_MAC_ADDR+1] = ((uint16_t)sc->esc_mac.octet[2]) |
		(((uint16_t)sc->esc_mac.octet[3]) << 8);
	sc->eeprom_data[NVM_MAC_ADDR+2] = ((uint16_t)sc->esc_mac.octet[4]) |
		(((uint16_t)sc->esc_mac.octet[5]) << 8);

	/* pci ids */
	sc->eeprom_data[NVM_SUB_DEV_ID] = E82545_SUBDEV_ID;
	sc->eeprom_data[NVM_SUB_VEN_ID] = E82545_VENDOR_ID_INTEL;
	sc->eeprom_data[NVM_DEV_ID] = E82545_DEV_ID_82545EM_COPPER;
	sc->eeprom_data[NVM_VEN_ID] = E82545_VENDOR_ID_INTEL;

	/* fill in the checksum */
        checksum = 0;
	for (i = 0; i < NVM_CHECKSUM_REG; i++) {
		checksum += sc->eeprom_data[i];
	}
	checksum = NVM_SUM - checksum;
	sc->eeprom_data[NVM_CHECKSUM_REG] = checksum;
	DPRINTF("eeprom checksum: 0x%x", checksum);
}

static void
e82545_write_mdi(struct e82545_softc *sc, uint8_t reg_addr,
			uint8_t phy_addr, uint32_t data)
{
	DPRINTF("Write mdi reg:0x%x phy:0x%x data: 0x%x", reg_addr, phy_addr, data);
}

static uint32_t
e82545_read_mdi(struct e82545_softc *sc, uint8_t reg_addr,
			uint8_t phy_addr)
{
	//DPRINTF("Read mdi reg:0x%x phy:0x%x", reg_addr, phy_addr);
	switch (reg_addr) {
	case PHY_STATUS:
		return (MII_SR_LINK_STATUS | MII_SR_AUTONEG_CAPS |
			MII_SR_AUTONEG_COMPLETE);
	case PHY_AUTONEG_ADV:
		return NWAY_AR_SELECTOR_FIELD;
	case PHY_LP_ABILITY:
		return 0;
	case PHY_1000T_STATUS:
		return (SR_1000T_LP_FD_CAPS | SR_1000T_REMOTE_RX_STATUS |
			SR_1000T_LOCAL_RX_STATUS);
	case PHY_ID1:
		return (M88E1011_I_PHY_ID >> 16) & 0xFFFF;
	case PHY_ID2:
		return (M88E1011_I_PHY_ID | E82545_REVISION_4) & 0xFFFF;
	default:
		DPRINTF("Unknown mdi read reg:0x%x phy:0x%x", reg_addr, phy_addr);
		return 0;
	}
	/* not reached */
}

static void
e82545_eecd_strobe(struct e82545_softc *sc)
{
	/* Microwire state machine */
	/*
	DPRINTF("eeprom state machine srtobe "
		"0x%x 0x%x 0x%x 0x%x",
		sc->nvm_mode, sc->nvm_bits,
		sc->nvm_opaddr, sc->nvm_data);*/

	if (sc->nvm_bits == 0) {
		DPRINTF("eeprom state machine not expecting data! "
			"0x%x 0x%x 0x%x 0x%x",
			sc->nvm_mode, sc->nvm_bits,
			sc->nvm_opaddr, sc->nvm_data);
		return;
	}
	sc->nvm_bits--;
	if (sc->nvm_mode == E82545_NVM_MODE_DATAOUT) {
		/* shifting out */
		if (sc->nvm_data & 0x8000) {
			sc->eeprom_control |= E1000_EECD_DO;
		} else {
			sc->eeprom_control &= ~E1000_EECD_DO;
		}
		sc->nvm_data <<= 1;
		if (sc->nvm_bits == 0) {
			/* read done, back to opcode mode. */
			sc->nvm_opaddr = 0;
			sc->nvm_mode = E82545_NVM_MODE_OPADDR;
			sc->nvm_bits = E82545_NVM_OPADDR_BITS;
		}
	} else if (sc->nvm_mode == E82545_NVM_MODE_DATAIN) {
		/* shifting in */
		sc->nvm_data <<= 1;
		if (sc->eeprom_control & E1000_EECD_DI) {
			sc->nvm_data |= 1;
		}
		if (sc->nvm_bits == 0) {
			/* eeprom write */
			uint16_t op = sc->nvm_opaddr & E82545_NVM_OPCODE_MASK;
			uint16_t addr = sc->nvm_opaddr & E82545_NVM_ADDR_MASK;
			if (op != E82545_NVM_OPCODE_WRITE) {
				DPRINTF("Illegal eeprom write op 0x%x",
					sc->nvm_opaddr);
			} else if (addr >= E82545_NVM_EEPROM_SIZE) {
				DPRINTF("Illegal eeprom write addr 0x%x",
					sc->nvm_opaddr);
			} else {
				DPRINTF("eeprom write eeprom[0x%x] = 0x%x",
				addr, sc->nvm_data);
				sc->eeprom_data[addr] = sc->nvm_data;
			}
			/* back to opcode mode */
			sc->nvm_opaddr = 0;
			sc->nvm_mode = E82545_NVM_MODE_OPADDR;
			sc->nvm_bits = E82545_NVM_OPADDR_BITS;
		}
	} else if (sc->nvm_mode == E82545_NVM_MODE_OPADDR) {
		sc->nvm_opaddr <<= 1;
		if (sc->eeprom_control & E1000_EECD_DI) {
			sc->nvm_opaddr |= 1;
		}
		if (sc->nvm_bits == 0) {
			uint16_t op = sc->nvm_opaddr & E82545_NVM_OPCODE_MASK;
			switch (op) {
			case E82545_NVM_OPCODE_EWEN:
				DPRINTF("eeprom write enable: 0x%x",
					sc->nvm_opaddr);
				/* back to opcode mode */
				sc->nvm_opaddr = 0;
				sc->nvm_mode = E82545_NVM_MODE_OPADDR;
				sc->nvm_bits = E82545_NVM_OPADDR_BITS;
				break;
			case E82545_NVM_OPCODE_READ:
			{
				uint16_t addr = sc->nvm_opaddr &
					E82545_NVM_ADDR_MASK;
				sc->nvm_mode = E82545_NVM_MODE_DATAOUT;
				sc->nvm_bits = E82545_NVM_DATA_BITS;
				if (addr < E82545_NVM_EEPROM_SIZE) {
					sc->nvm_data = sc->eeprom_data[addr];
					DPRINTF("eeprom read: eeprom[0x%x] = 0x%x",
						addr, sc->nvm_data);
				} else {
					DPRINTF("eeprom illegal read: 0x%x",
						sc->nvm_opaddr);
					sc->nvm_data = 0;
				}
				break;
			}
			case E82545_NVM_OPCODE_WRITE:
				sc->nvm_mode = E82545_NVM_MODE_DATAIN;
				sc->nvm_bits = E82545_NVM_DATA_BITS;
				sc->nvm_data = 0;
				break;
			default:
				DPRINTF("eeprom unknown op: 0x%x",
					sc->nvm_opaddr);
				/* back to opcode mode */
				sc->nvm_opaddr = 0;
				sc->nvm_mode = E82545_NVM_MODE_OPADDR;
				sc->nvm_bits = E82545_NVM_OPADDR_BITS;
			}
		}
	} else {
		DPRINTF("eeprom state machine wrong state! "
			"0x%x 0x%x 0x%x 0x%x",
			sc->nvm_mode, sc->nvm_bits,
			sc->nvm_opaddr, sc->nvm_data);
	}
}

static void
e82545_itr_callback(int fd, enum ev_type type, void *param)
{
	uint32_t new;
	struct e82545_softc *sc = param;

	pthread_mutex_lock(&sc->esc_mtx);
	new = sc->esc_ICR & sc->esc_IMS;
	if (new && !sc->esc_irq_asserted) {
		DPRINTF("itr callback: lintr assert %x", new);
		sc->esc_irq_asserted = 1;
		pci_lintr_assert(sc->esc_pi);
	} else {
		mevent_delete(sc->esc_mevpitr);
		sc->esc_mevpitr = NULL;
	}
	pthread_mutex_unlock(&sc->esc_mtx);
}

static void
e82545_icr_assert(struct e82545_softc *sc, uint32_t bits)
{
	uint32_t new;

	DPRINTF("icr assert: 0x%x", bits);

	/*
	 * An interrupt is only generated if bits are set that
	 * aren't already in the ICR, these bits are unmasked,
	 * and there isn't an interrupt already pending.
	 */
	new = bits & ~sc->esc_ICR & sc->esc_IMS;
	sc->esc_ICR |= bits;

	if (new == 0) {
		DPRINTF("icr assert: masked %x, ims %x", new, sc->esc_IMS);
	} else if (sc->esc_mevpitr != NULL) {
		DPRINTF("icr assert: throttled %x, ims %x", new, sc->esc_IMS);
	} else if (!sc->esc_irq_asserted) {
		DPRINTF("icr assert: lintr assert %x", new);
		sc->esc_irq_asserted = 1;
		pci_lintr_assert(sc->esc_pi);
		if (sc->esc_ITR != 0) {
			sc->esc_mevpitr = mevent_add(
			    (sc->esc_ITR + 3905) / 3906,  /* 256ns -> 1ms */
			    EVF_TIMER, e82545_itr_callback, sc);
		}
	}
}

static void
e82545_ims_change(struct e82545_softc *sc, uint32_t bits)
{
	uint32_t new;

	/*
	 * Changing the mask may allow previously asserted
	 * but masked interrupt requests to generate an interrupt.
	 */
	new = bits & sc->esc_ICR & ~sc->esc_IMS;
	sc->esc_IMS |= bits;

	if (new == 0) {
		DPRINTF("ims change: masked %x, ims %x", new, sc->esc_IMS);
	} else if (sc->esc_mevpitr != NULL) {
		DPRINTF("ims change: throttled %x, ims %x", new, sc->esc_IMS);
	} else if (!sc->esc_irq_asserted) {
		DPRINTF("ims change: lintr assert %x", new);
		sc->esc_irq_asserted = 1;
		pci_lintr_assert(sc->esc_pi);
		if (sc->esc_ITR != 0) {
			sc->esc_mevpitr = mevent_add(
			    (sc->esc_ITR + 3905) / 3906,  /* 256ns -> 1ms */
			    EVF_TIMER, e82545_itr_callback, sc);
		}
	}
}

static void
e82545_icr_deassert(struct e82545_softc *sc, uint32_t bits)
{

	DPRINTF("icr deassert: 0x%x", bits);
	sc->esc_ICR &= ~bits;

	/*
	 * If there are no longer any interrupt sources and there
	 * was an asserted interrupt, clear it
	 */
	if (sc->esc_irq_asserted && !(sc->esc_ICR & sc->esc_IMS)) {
		DPRINTF("icr deassert: lintr deassert %x", bits);
		pci_lintr_deassert(sc->esc_pi);
		sc->esc_irq_asserted = 0;
	}
}

static void
e82545_intr_write(struct e82545_softc *sc, uint32_t offset, uint32_t value)
{

	DPRINTF("intr_write: off %x, val %x", offset, value);

	switch (offset) {
	case E1000_ICR:
		e82545_icr_deassert(sc, value);
		break;
	case E1000_ITR:
		sc->esc_ITR = value;
		break;
	case E1000_ICS:
		sc->esc_ICS = value;	/* not used: store for debug */
		e82545_icr_assert(sc, value);
		break;
	case E1000_IMS:
		e82545_ims_change(sc, value);
		break;
	case E1000_IMC:
		sc->esc_IMC = value;	/* for debug */
		sc->esc_IMS &= ~value;
		// XXX clear interrupts if all ICR bits now masked
		// and interrupt was pending ?
		break;
	default:
		break;
	}
}

static uint32_t
e82545_intr_read(struct e82545_softc *sc, uint32_t offset)
{
	uint32_t retval;

	retval = 0;

	DPRINTF("intr_read: off %x", offset);

	switch (offset) {
	case E1000_ICR:
		retval = sc->esc_ICR;
		sc->esc_ICR = 0;
		e82545_icr_deassert(sc, ~0);
		break;
	case E1000_ITR:
		retval = sc->esc_ITR;
		break;
	case E1000_ICS:
		/* write-only register */
		break;
	case E1000_IMS:
		retval = sc->esc_IMS;
		break;
	case E1000_IMC:
		/* write-only register */
		break;
	default:
		break;
	}

	return (retval);
}

static void
e82545_devctl(struct e82545_softc *sc, uint32_t val)
{

	sc->esc_CTRL = val & ~E1000_CTRL_RST;

	if (val & E1000_CTRL_RST) {
		DPRINTF("e1k: s/w reset, ctl %x", val);
		e82545_reset(sc, 1);
	}
	/* XXX check for phy reset ? */
}

static void
e82545_rx_update_rdba(struct e82545_softc *sc)
{

	/* XXX verify desc base/len within phys mem range */
	sc->esc_rdba = (uint64_t)sc->esc_RDBAH << 32 |
	    sc->esc_RDBAL;

	/* Cache host mapping of guest descriptor array */
	sc->esc_rxdesc = paddr_guest2host(sc->esc_ctx,
	    sc->esc_rdba, sc->esc_RDLEN);
}

static void
e82545_rx_ctl(struct e82545_softc *sc, uint32_t val)
{
	int on;

	on = ((val & E1000_RCTL_EN) == E1000_RCTL_EN);

	/* Save RCTL after stripping reserved bits 31:27,24,21,14,11:10,0 */
	sc->esc_RCTL = val & ~0xF9204c01;

	DPRINTF("rx_ctl - %s RCTL %x, val %x",
		on ? "on" : "off", sc->esc_RCTL, val);

	/* state change requested */
	if (on != sc->esc_rx_enabled) {
		if (on) {
			/* Catch disallowed/unimplemented settings */
			//assert(!(val & E1000_RCTL_LBM_TCVR));

			if (sc->esc_RCTL & E1000_RCTL_LBM_TCVR) {
				sc->esc_rx_loopback = 1;
			} else {
				sc->esc_rx_loopback = 0;
			}

			e82545_rx_update_rdba(sc);
			e82545_rx_enable(sc);
		} else {
			e82545_rx_disable(sc);
			sc->esc_rx_loopback = 0;
			sc->esc_rdba = 0;
			sc->esc_rxdesc = NULL;
		}
	}
}

static void
e82545_tx_update_tdba(struct e82545_softc *sc)
{

	/* XXX verify desc base/len within phys mem range */
	sc->esc_tdba = (uint64_t)sc->esc_TDBAH << 32 | sc->esc_TDBAL;

	/* Cache host mapping of guest descriptor array */
	sc->esc_txdesc = paddr_guest2host(sc->esc_ctx, sc->esc_tdba,
            sc->esc_TDLEN);
}

static void
e82545_tx_ctl(struct e82545_softc *sc, uint32_t val)
{
	int on;

	on = ((val & E1000_TCTL_EN) == E1000_TCTL_EN);

	/* ignore TCTL_EN settings that don't change state */
	if (on == sc->esc_tx_enabled)
		return;

	if (on) {
		e82545_tx_update_tdba(sc);
		e82545_tx_enable(sc);
	} else {
		e82545_tx_disable(sc);
		sc->esc_tdba = 0;
		sc->esc_txdesc = NULL;
	}

	/* Save TCTL value after stripping reserved bits 31:25,23,2,0 */
	sc->esc_TCTL = val & ~0xFE800005;
}

static int
e82545_bufsz(uint32_t rctl)
{

	switch (rctl & (E1000_RCTL_BSEX | E1000_RCTL_SZ_256)) {
	case (E1000_RCTL_SZ_2048): return (2048);
	case (E1000_RCTL_SZ_1024): return (1024);
	case (E1000_RCTL_SZ_512): return (512);
	case (E1000_RCTL_SZ_256): return (256);
	case (E1000_RCTL_BSEX|E1000_RCTL_SZ_16384): return (16384);
	case (E1000_RCTL_BSEX|E1000_RCTL_SZ_8192): return (8192);
	case (E1000_RCTL_BSEX|E1000_RCTL_SZ_4096): return (4096);
	}
	return (256);	/* Forbidden value. */
}

/* XXX one packet at a time until this is debugged */
static void
e82545_rx_callback(int fd, enum ev_type type, void *param)
{
	struct e82545_softc *sc = param;
	struct e1000_rx_desc *rxd;
	struct iovec vec[64];
	int left, len, lim, maxpktsz, maxpktdesc, bufsz, i, n, size;
	uint32_t cause = 0;
	uint16_t *tp, tag, head;

	pthread_mutex_lock(&sc->esc_mtx);
	DPRINTF("rx_run: head %x, tail %x", sc->esc_RDH, sc->esc_RDT);

	if (!sc->esc_rx_enabled || sc->esc_rx_loopback) {
		DPRINTF("rx disabled (!%d || %d) -- packet(s) dropped",
		    sc->esc_rx_enabled, sc->esc_rx_loopback);
		while (netbe_rx_discard(sc->esc_be) > 0) {
		}
		goto done1;
	}
	bufsz = e82545_bufsz(sc->esc_RCTL);
	maxpktsz = (sc->esc_RCTL & E1000_RCTL_LPE) ? 16384 : 1522;
	maxpktdesc = (maxpktsz + bufsz - 1) / bufsz;
	size = sc->esc_RDLEN / 16;
	head = sc->esc_RDH;
	left = (size + sc->esc_RDT - head) % size;
	if (left < maxpktdesc) {
		DPRINTF("rx overflow (%d < %d) -- packet(s) dropped",
		    left, maxpktdesc);
		while (netbe_rx_discard(sc->esc_be) > 0) {
		}
		goto done1;
	}

	sc->esc_rx_active = 1;
	pthread_mutex_unlock(&sc->esc_mtx);

	for (lim = size / 4; lim > 0 && left >= maxpktdesc; lim -= n) {

		/* Grab rx descriptor pointed to by the head pointer */
		for (i = 0; i < maxpktdesc; i++) {
			rxd = &sc->esc_rxdesc[(head + i) % size];
			vec[i].iov_base = paddr_guest2host(sc->esc_ctx,
			    rxd->buffer_addr, bufsz);
			vec[i].iov_len = bufsz;
		}
		len = netbe_recv(sc->esc_be, vec, maxpktdesc);
		if (len <= 0) {
			DPRINTF("netbe_recv() returned %d", len);
			goto done;
		}

		/*
		 * Adjust the packet length based on whether the CRC needs
		 * to be stripped or if the packet is less than the minimum
		 * eth packet size.
		 */
		if (len < ETHER_MIN_LEN - ETHER_CRC_LEN)
			len = ETHER_MIN_LEN - ETHER_CRC_LEN;
		if (!(sc->esc_RCTL & E1000_RCTL_SECRC))
			len += ETHER_CRC_LEN;
		n = (len + bufsz - 1) / bufsz;

		DPRINTF("packet read %d bytes, %d segs, head %d",
		    len, n, head);

		/* Apply VLAN filter. */
		tp = (uint16_t *)vec[0].iov_base + 6;
		if ((sc->esc_RCTL & E1000_RCTL_VFE) &&
		    (ntohs(tp[0]) == sc->esc_VET)) {
			tag = ntohs(tp[1]) & 0x0fff;
			if ((sc->esc_fvlan[tag >> 5] &
			    (1 << (tag & 0x1f))) != 0) {
				DPRINTF("known VLAN %d", tag);
			} else {
				DPRINTF("unknown VLAN %d", tag);
				n = 0;
				continue;
			}
		}

		/* Update all consumed descriptors. */
		for (i = 0; i < n - 1; i++) {
			rxd = &sc->esc_rxdesc[(head + i) % size];
			rxd->length = bufsz;
			rxd->csum = 0;
			rxd->errors = 0;
			rxd->special = 0;
			rxd->status = E1000_RXD_STAT_DD;
		}
		rxd = &sc->esc_rxdesc[(head + i) % size];
		rxd->length = len % bufsz;
		rxd->csum = 0;
		rxd->errors = 0;
		rxd->special = 0;
		/* XXX signal no checksum for now */
		rxd->status = E1000_RXD_STAT_PIF | E1000_RXD_STAT_IXSM |
		    E1000_RXD_STAT_EOP | E1000_RXD_STAT_DD;

		/* Schedule receive interrupts. */
		if (len <= sc->esc_RSRPD) {
			cause |= E1000_ICR_SRPD | E1000_ICR_RXT0;
		} else {
			/* XXX: RDRT and RADV timers should be here. */
			cause |= E1000_ICR_RXT0;
		}

		head = (head + n) % size;
		left -= n;
	}

done:
	pthread_mutex_lock(&sc->esc_mtx);
	sc->esc_rx_active = 0;
	if (sc->esc_rx_enabled == 0)
		pthread_cond_signal(&sc->esc_rx_cond);

	sc->esc_RDH = head;
	/* Respect E1000_RCTL_RDMTS */
	left = (size + sc->esc_RDT - head) % size;
	if (left < (size >> (((sc->esc_RCTL >> 8) & 3) + 1)))
		cause |= E1000_ICR_RXDMT0;
	/* Assert all accumulated interrupts. */
	if (cause != 0)
		e82545_icr_assert(sc, cause);
done1:
	DPRINTF("rx_run done: head %x, tail %x", sc->esc_RDH, sc->esc_RDT);
	pthread_mutex_unlock(&sc->esc_mtx);
}

static uint16_t
e82545_carry(uint32_t sum)
{

	sum = (sum & 0xFFFF) + (sum >> 16);
	if (sum > 0xFFFF)
		sum -= 0xFFFF;
	return (sum);
}

static uint16_t
e82545_buf_checksum(uint8_t *buf, int len)
{
	int i;
	uint32_t sum = 0;

	/* Checksum all the pairs of bytes first... */
	for (i = 0; i < (len & ~1U); i += 2)
		sum += *((u_int16_t *)(buf + i));

	/*
	 * If there's a single byte left over, checksum it, too.
	 * Network byte order is big-endian, so the remaining byte is
	 * the high byte.
	 */
	if (i < len)
		sum += htons(buf[i] << 8);

	return (e82545_carry(sum));
}

static uint16_t
e82545_iov_checksum(struct iovec *iov, int iovcnt, int off, int len)
{
	int now, odd;
	uint32_t sum = 0, s;

	/* Skip completely unneeded vectors. */
	while (iovcnt > 0 && iov->iov_len <= off && off > 0) {
		off -= iov->iov_len;
		iov++;
		iovcnt--;
	}

	/* Calculate checksum of requested range. */
	odd = 0;
	while (len > 0 && iovcnt > 0) {
		now = MIN(len, iov->iov_len - off);
		s = e82545_buf_checksum(iov->iov_base + off, now);
		sum += odd ? (s << 8) : s;
		odd ^= (now & 1);
		len -= now;
		off = 0;
		iov++;
		iovcnt--;
	}

	return (e82545_carry(sum));
}

/*
 * Return the transmit descriptor type.
 */
static int
e82545_txdesc_type(uint32_t lower)
{
	int type;

	type = 0;

	if (lower & E1000_TXD_CMD_DEXT)
		type = lower & E1000_TXD_MASK;

	return (type);
}

static void
e82545_transmit_checksum(struct iovec *iov, int iovcnt, struct ck_info *ck)
{
	uint16_t cksum;
	int cklen;

	DPRINTF("tx cksum: iovcnt/s/off/len %d/%d/%d/%d",
	    iovcnt, ck->ck_start, ck->ck_off, ck->ck_len);
	cklen = ck->ck_len ? ck->ck_len - ck->ck_start + 1 : INT_MAX;
	cksum = e82545_iov_checksum(iov, iovcnt, ck->ck_start, cklen);
	*(uint16_t *)((uint8_t *)iov[0].iov_base + ck->ck_off) = ~cksum;
}

static void
e82545_transmit_backend(struct e82545_softc *sc, struct iovec *iov, int iovcnt)
{

	if (sc->esc_be == NULL)
		return;

	(void) netbe_send(sc->esc_be, iov, iovcnt);
}

static void
e82545_transmit_done(struct e82545_softc *sc, uint16_t head, uint16_t tail,
    uint16_t dsize, int *tdwb)
{
	union e1000_tx_udesc *dsc;

	for ( ; head != tail; head = (head + 1) % dsize) {
		dsc = &sc->esc_txdesc[head];
		if (dsc->td.lower.data & E1000_TXD_CMD_RS) {
			dsc->td.upper.data |= E1000_TXD_STAT_DD;
			*tdwb = 1;
		}
	}
}

static int
e82545_transmit(struct e82545_softc *sc, uint16_t head, uint16_t tail,
    uint16_t dsize, uint16_t *rhead, int *tdwb)
{
	uint8_t *hdr, *hdrp;
	struct iovec iovb[I82545_MAX_TXSEGS + 2];
	struct iovec tiov[I82545_MAX_TXSEGS + 2];
	struct e1000_context_desc *cd;
	struct ck_info ckinfo[2];
	struct iovec *iov;
	union  e1000_tx_udesc *dsc;
	int desc, dtype, len, ntype, iovcnt, tcp, tso;
	int mss, paylen, seg, tiovcnt, left, now, nleft, nnow, pv, pvoff;
	unsigned hdrlen, vlen;
	uint32_t tcpsum, tcpseq;
	uint16_t ipcs, tcpcs, ipid, ohead;

	ckinfo[0].ck_valid = ckinfo[1].ck_valid = 0;
	iovcnt = 0;
	ntype = 0;
	tso = 0;
	ohead = head;

	/* iovb[0/1] may be used for writable copy of headers. */
	iov = &iovb[2];

	for (desc = 0; ; desc++, head = (head + 1) % dsize) {
		if (head == tail) {
			*rhead = head;
			return (0);
		}
		dsc = &sc->esc_txdesc[head];
		dtype = e82545_txdesc_type(dsc->td.lower.data);

		if (desc == 0) {
			switch (dtype) {
			case E1000_TXD_TYP_C:
				DPRINTF("tx ctxt desc idx %d: %016jx "
				    "%08x%08x",
				    head, dsc->td.buffer_addr,
				    dsc->td.upper.data, dsc->td.lower.data);
				/* Save context and return */
				sc->esc_txctx = dsc->cd;
				goto done;
			case E1000_TXD_TYP_L:
				DPRINTF("tx legacy desc idx %d: %08x%08x",
				    head, dsc->td.upper.data, dsc->td.lower.data);
				/*
				 * legacy cksum start valid in first descriptor
				 */
				ntype = dtype;
				ckinfo[0].ck_start = dsc->td.upper.fields.css;
				break;
			case E1000_TXD_TYP_D:
				DPRINTF("tx data desc idx %d: %08x%08x",
				    head, dsc->td.upper.data, dsc->td.lower.data);
				ntype = dtype;
				break;
			default:
				break;
			}
		} else {
			/* Descriptor type must be consistent */
			assert(dtype == ntype);
			DPRINTF("tx next desc idx %d: %08x%08x",
			    head, dsc->td.upper.data, dsc->td.lower.data);
		}

		len = (dtype == E1000_TXD_TYP_L) ? dsc->td.lower.flags.length :
		    dsc->dd.lower.data & 0xFFFFF;

		if (len > 0) {
			/* Strip checksum supplied by guest. */
			if ((dsc->td.lower.data & E1000_TXD_CMD_EOP) != 0 &&
			    (dsc->td.lower.data & E1000_TXD_CMD_IFCS) == 0)
				len -= 2;
			if (iovcnt < I82545_MAX_TXSEGS) {
				iov[iovcnt].iov_base = paddr_guest2host(
				    sc->esc_ctx, dsc->td.buffer_addr, len);
				iov[iovcnt].iov_len = len;
			}
			iovcnt++;
		}

		/*
		 * Pull out info that is valid in the final descriptor
		 * and exit descriptor loop.
		 */
		if (dsc->td.lower.data & E1000_TXD_CMD_EOP) {
			if (dtype == E1000_TXD_TYP_L) {
				if (dsc->td.lower.data & E1000_TXD_CMD_IC) {
					ckinfo[0].ck_valid = 1;
					ckinfo[0].ck_off =
					    dsc->td.lower.flags.cso;
					ckinfo[0].ck_len = 0;
				}
			} else {
				cd = &sc->esc_txctx;
				if (dsc->dd.lower.data & E1000_TXD_CMD_TSE)
					tso = 1;
				if (dsc->dd.upper.fields.popts &
				    E1000_TXD_POPTS_IXSM)
					ckinfo[0].ck_valid = 1;
				if (dsc->dd.upper.fields.popts &
				    E1000_TXD_POPTS_IXSM || tso) {
					ckinfo[0].ck_start =
					    cd->lower_setup.ip_fields.ipcss;
					ckinfo[0].ck_off =
					    cd->lower_setup.ip_fields.ipcso;
					ckinfo[0].ck_len =
					    cd->lower_setup.ip_fields.ipcse;
				}
				if (dsc->dd.upper.fields.popts &
				    E1000_TXD_POPTS_TXSM)
					ckinfo[1].ck_valid = 1;
				if (dsc->dd.upper.fields.popts &
				    E1000_TXD_POPTS_TXSM || tso) {
					ckinfo[1].ck_start =
					    cd->upper_setup.tcp_fields.tucss;
					ckinfo[1].ck_off =
					    cd->upper_setup.tcp_fields.tucso;
					ckinfo[1].ck_len =
					    cd->upper_setup.tcp_fields.tucse;
				}
			}
			break;
		}
	}

	if (iovcnt > I82545_MAX_TXSEGS) {
		WPRINTF("tx too many descriptors (%d > %d) -- dropped",
		    iovcnt, I82545_MAX_TXSEGS);
		goto done;
	}

	hdrlen = vlen = 0;
	/* Estimate writable space for VLAN header insertion. */
	if ((sc->esc_CTRL & E1000_CTRL_VME) &&
	    (dsc->td.lower.data & E1000_TXD_CMD_VLE)) {
		hdrlen = ETHER_ADDR_LEN*2;
		vlen = ETHER_VLAN_ENCAP_LEN;
	}
	if (!tso) {
		/* Estimate required writable space for checksums. */
		if (ckinfo[0].ck_valid)
			hdrlen = MAX(hdrlen, ckinfo[0].ck_off + 2);
		if (ckinfo[1].ck_valid)
			hdrlen = MAX(hdrlen, ckinfo[1].ck_off + 2);
		/* Round up writable space to the first vector. */
		if (hdrlen != 0 && iov[0].iov_len > hdrlen &&
		    iov[0].iov_len < hdrlen + 100)
			hdrlen = iov[0].iov_len;
	} else {
		/* In case of TSO header length provided by software. */
		hdrlen = sc->esc_txctx.tcp_seg_setup.fields.hdr_len;

		/*
		 * Cap the header length at 240 based on 7.2.4.5 of
		 * the Intel 82576EB (Rev 2.63) datasheet.
		 */
		if (hdrlen > 240) {
			WPRINTF("TSO hdrlen too large: %d", hdrlen);
			goto done;
		}

		/*
		 * If VLAN insertion is requested, ensure the header
		 * at least holds the amount of data copied during
		 * VLAN insertion below.
		 *
		 * XXX: Realistic packets will include a full Ethernet
		 * header before the IP header at ckinfo[0].ck_start,
		 * but this check is sufficient to prevent
		 * out-of-bounds access below.
		 */
		if (vlen != 0 && hdrlen < ETHER_ADDR_LEN*2) {
			WPRINTF("TSO hdrlen too small for vlan insertion "
			    "(%d vs %d) -- dropped", hdrlen,
			    ETHER_ADDR_LEN*2);
			goto done;
		}

		/*
		 * Ensure that the header length covers the used fields
		 * in the IP and TCP headers as well as the IP and TCP
		 * checksums.  The following fields are accessed below:
		 *
		 * Header | Field | Offset | Length
		 * -------+-------+--------+-------
		 * IPv4   | len   | 2      | 2
		 * IPv4   | ID    | 4      | 2
		 * IPv6   | len   | 4      | 2
		 * TCP    | seq # | 4      | 4
		 * TCP    | flags | 13     | 1
		 * UDP    | len   | 4      | 4
		 */
		if (hdrlen < ckinfo[0].ck_start + 6 ||
		    hdrlen < ckinfo[0].ck_off + 2) {
			WPRINTF("TSO hdrlen too small for IP fields (%d) "
			    "-- dropped", hdrlen);
			goto done;
		}
		if (sc->esc_txctx.cmd_and_length & E1000_TXD_CMD_TCP) {
			if (hdrlen < ckinfo[1].ck_start + 14) {
				WPRINTF("TSO hdrlen too small for TCP fields "
				    "(%d) -- dropped", hdrlen);
				goto done;
			}
		} else {
			if (hdrlen < ckinfo[1].ck_start + 8) {
				WPRINTF("TSO hdrlen too small for UDP fields "
				    "(%d) -- dropped", hdrlen);
				goto done;
			}
		}
		if (ckinfo[1].ck_valid && hdrlen < ckinfo[1].ck_off + 2) {
			WPRINTF("TSO hdrlen too small for TCP/UDP fields "
			    "(%d) -- dropped", hdrlen);
			goto done;
		}
	}

	/* Allocate, fill and prepend writable header vector. */
	if (hdrlen != 0) {
		hdr = __builtin_alloca(hdrlen + vlen);
		hdr += vlen;
		for (left = hdrlen, hdrp = hdr; left > 0;
		    left -= now, hdrp += now) {
			now = MIN(left, iov->iov_len);
			memcpy(hdrp, iov->iov_base, now);
			iov->iov_base += now;
			iov->iov_len -= now;
			if (iov->iov_len == 0) {
				iov++;
				iovcnt--;
			}
		}
		iov--;
		iovcnt++;
		iov->iov_base = hdr;
		iov->iov_len = hdrlen;
	} else
		hdr = NULL;

	/* Insert VLAN tag. */
	if (vlen != 0) {
		hdr -= ETHER_VLAN_ENCAP_LEN;
		memmove(hdr, hdr + ETHER_VLAN_ENCAP_LEN, ETHER_ADDR_LEN*2);
		hdrlen += ETHER_VLAN_ENCAP_LEN;
		hdr[ETHER_ADDR_LEN*2 + 0] = sc->esc_VET >> 8;
		hdr[ETHER_ADDR_LEN*2 + 1] = sc->esc_VET & 0xff;
		hdr[ETHER_ADDR_LEN*2 + 2] = dsc->td.upper.fields.special >> 8;
		hdr[ETHER_ADDR_LEN*2 + 3] = dsc->td.upper.fields.special & 0xff;
		iov->iov_base = hdr;
		iov->iov_len += ETHER_VLAN_ENCAP_LEN;
		/* Correct checksum offsets after VLAN tag insertion. */
		ckinfo[0].ck_start += ETHER_VLAN_ENCAP_LEN;
		ckinfo[0].ck_off += ETHER_VLAN_ENCAP_LEN;
		if (ckinfo[0].ck_len != 0)
			ckinfo[0].ck_len += ETHER_VLAN_ENCAP_LEN;
		ckinfo[1].ck_start += ETHER_VLAN_ENCAP_LEN;
		ckinfo[1].ck_off += ETHER_VLAN_ENCAP_LEN;
		if (ckinfo[1].ck_len != 0)
			ckinfo[1].ck_len += ETHER_VLAN_ENCAP_LEN;
	}

	/* Simple non-TSO case. */
	if (!tso) {
		/* Calculate checksums and transmit. */
		if (ckinfo[0].ck_valid)
			e82545_transmit_checksum(iov, iovcnt, &ckinfo[0]);
		if (ckinfo[1].ck_valid)
			e82545_transmit_checksum(iov, iovcnt, &ckinfo[1]);
		e82545_transmit_backend(sc, iov, iovcnt);
		goto done;
	}

	/* Doing TSO. */
	tcp = (sc->esc_txctx.cmd_and_length & E1000_TXD_CMD_TCP) != 0;
	mss = sc->esc_txctx.tcp_seg_setup.fields.mss;
	paylen = (sc->esc_txctx.cmd_and_length & 0x000fffff);
	DPRINTF("tx %s segmentation offload %d+%d/%d bytes %d iovs",
	    tcp ? "TCP" : "UDP", hdrlen, paylen, mss, iovcnt);
	ipid = ntohs(*(uint16_t *)&hdr[ckinfo[0].ck_start + 4]);
	tcpseq = 0;
	if (tcp)
		tcpseq = ntohl(*(uint32_t *)&hdr[ckinfo[1].ck_start + 4]);
	ipcs = *(uint16_t *)&hdr[ckinfo[0].ck_off];
	tcpcs = 0;
	if (ckinfo[1].ck_valid)	/* Save partial pseudo-header checksum. */
		tcpcs = *(uint16_t *)&hdr[ckinfo[1].ck_off];
	pv = 1;
	pvoff = 0;
	for (seg = 0, left = paylen; left > 0; seg++, left -= now) {
		now = MIN(left, mss);

		/* Construct IOVs for the segment. */
		/* Include whole original header. */
		tiov[0].iov_base = hdr;
		tiov[0].iov_len = hdrlen;
		tiovcnt = 1;
		/* Include respective part of payload IOV. */
		for (nleft = now; pv < iovcnt && nleft > 0; nleft -= nnow) {
			nnow = MIN(nleft, iov[pv].iov_len - pvoff);
			tiov[tiovcnt].iov_base = iov[pv].iov_base + pvoff;
			tiov[tiovcnt++].iov_len = nnow;
			if (pvoff + nnow == iov[pv].iov_len) {
				pv++;
				pvoff = 0;
			} else
				pvoff += nnow;
		}
		DPRINTF("tx segment %d %d+%d bytes %d iovs",
		    seg, hdrlen, now, tiovcnt);

		/* Update IP header. */
		if (sc->esc_txctx.cmd_and_length & E1000_TXD_CMD_IP) {
			/* IPv4 -- set length and ID */
			*(uint16_t *)&hdr[ckinfo[0].ck_start + 2] =
			    htons(hdrlen - ckinfo[0].ck_start + now);
			*(uint16_t *)&hdr[ckinfo[0].ck_start + 4] =
			    htons(ipid + seg);
		} else {
			/* IPv6 -- set length */
			*(uint16_t *)&hdr[ckinfo[0].ck_start + 4] =
			    htons(hdrlen - ckinfo[0].ck_start - 40 +
				  now);
		}

		/* Update pseudo-header checksum. */
		tcpsum = tcpcs;
		tcpsum += htons(hdrlen - ckinfo[1].ck_start + now);

		/* Update TCP/UDP headers. */
		if (tcp) {
			/* Update sequence number and FIN/PUSH flags. */
			*(uint32_t *)&hdr[ckinfo[1].ck_start + 4] =
			    htonl(tcpseq + paylen - left);
			if (now < left) {
				hdr[ckinfo[1].ck_start + 13] &=
				    ~(TH_FIN | TH_PUSH);
			}
		} else {
			/* Update payload length. */
			*(uint32_t *)&hdr[ckinfo[1].ck_start + 4] =
			    hdrlen - ckinfo[1].ck_start + now;
		}

		/* Calculate checksums and transmit. */
		if (ckinfo[0].ck_valid) {
			*(uint16_t *)&hdr[ckinfo[0].ck_off] = ipcs;
			e82545_transmit_checksum(tiov, tiovcnt, &ckinfo[0]);
		}
		if (ckinfo[1].ck_valid) {
			*(uint16_t *)&hdr[ckinfo[1].ck_off] =
			    e82545_carry(tcpsum);
			e82545_transmit_checksum(tiov, tiovcnt, &ckinfo[1]);
		}
		e82545_transmit_backend(sc, tiov, tiovcnt);
	}

done:
	head = (head + 1) % dsize;
	e82545_transmit_done(sc, ohead, head, dsize, tdwb);

	*rhead = head;
	return (desc + 1);
}

static void
e82545_tx_run(struct e82545_softc *sc)
{
	uint32_t cause;
	uint16_t head, rhead, tail, size;
	int lim, tdwb, sent;

	head = sc->esc_TDH;
	tail = sc->esc_TDT;
	size = sc->esc_TDLEN / 16;
	DPRINTF("tx_run: head %x, rhead %x, tail %x",
	    sc->esc_TDH, sc->esc_TDHr, sc->esc_TDT);

	pthread_mutex_unlock(&sc->esc_mtx);
	rhead = head;
	tdwb = 0;
	for (lim = size / 4; sc->esc_tx_enabled && lim > 0; lim -= sent) {
		sent = e82545_transmit(sc, head, tail, size, &rhead, &tdwb);
		if (sent == 0)
			break;
		head = rhead;
	}
	pthread_mutex_lock(&sc->esc_mtx);

	sc->esc_TDH = head;
	sc->esc_TDHr = rhead;
	cause = 0;
	if (tdwb)
		cause |= E1000_ICR_TXDW;
	if (lim != size / 4 && sc->esc_TDH == sc->esc_TDT)
		cause |= E1000_ICR_TXQE;
	if (cause)
		e82545_icr_assert(sc, cause);

	DPRINTF("tx_run done: head %x, rhead %x, tail %x",
	    sc->esc_TDH, sc->esc_TDHr, sc->esc_TDT);
}

static _Noreturn void *
e82545_tx_thread(void *param)
{
	struct e82545_softc *sc = param;

	pthread_mutex_lock(&sc->esc_mtx);
	for (;;) {
		while (!sc->esc_tx_enabled || sc->esc_TDHr == sc->esc_TDT) {
			if (sc->esc_tx_enabled && sc->esc_TDHr != sc->esc_TDT)
				break;
			sc->esc_tx_active = 0;
			if (sc->esc_tx_enabled == 0)
				pthread_cond_signal(&sc->esc_tx_cond);
			pthread_cond_wait(&sc->esc_tx_cond, &sc->esc_mtx);
		}
		sc->esc_tx_active = 1;

		/* Process some tx descriptors.  Lock dropped inside. */
		e82545_tx_run(sc);
	}
}

static void
e82545_tx_start(struct e82545_softc *sc)
{

	if (sc->esc_tx_active == 0)
		pthread_cond_signal(&sc->esc_tx_cond);
}

static void
e82545_tx_enable(struct e82545_softc *sc)
{

	sc->esc_tx_enabled = 1;
}

static void
e82545_tx_disable(struct e82545_softc *sc)
{

	sc->esc_tx_enabled = 0;
	while (sc->esc_tx_active)
		pthread_cond_wait(&sc->esc_tx_cond, &sc->esc_mtx);
}

static void
e82545_rx_enable(struct e82545_softc *sc)
{

	sc->esc_rx_enabled = 1;
}

static void
e82545_rx_disable(struct e82545_softc *sc)
{

	sc->esc_rx_enabled = 0;
	while (sc->esc_rx_active)
		pthread_cond_wait(&sc->esc_rx_cond, &sc->esc_mtx);
}

static void
e82545_write_ra(struct e82545_softc *sc, int reg, uint32_t wval)
{
	struct eth_uni *eu;
	int idx;

	idx = reg >> 1;
	assert(idx < 15);

	eu = &sc->esc_uni[idx];

	if (reg & 0x1) {
		/* RAH */
		eu->eu_valid = ((wval & E1000_RAH_AV) == E1000_RAH_AV);
		eu->eu_addrsel = (wval >> 16) & 0x3;
		eu->eu_eth.octet[5] = wval >> 8;
		eu->eu_eth.octet[4] = wval;
	} else {
		/* RAL */
		eu->eu_eth.octet[3] = wval >> 24;
		eu->eu_eth.octet[2] = wval >> 16;
		eu->eu_eth.octet[1] = wval >> 8;
		eu->eu_eth.octet[0] = wval;
	}
}

static uint32_t
e82545_read_ra(struct e82545_softc *sc, int reg)
{
	struct eth_uni *eu;
	uint32_t retval;
	int idx;

	idx = reg >> 1;
	assert(idx < 15);

	eu = &sc->esc_uni[idx];

	if (reg & 0x1) {
		/* RAH */
		retval = (eu->eu_valid << 31) |
			 (eu->eu_addrsel << 16) |
			 (eu->eu_eth.octet[5] << 8) |
			 eu->eu_eth.octet[4];
	} else {
		/* RAL */
		retval = (eu->eu_eth.octet[3] << 24) |
			 (eu->eu_eth.octet[2] << 16) |
			 (eu->eu_eth.octet[1] << 8) |
			 eu->eu_eth.octet[0];
	}

	return (retval);
}

static void
e82545_write_register(struct e82545_softc *sc, uint32_t offset, uint32_t value)
{
	int ridx;

	if (offset & 0x3) {
		DPRINTF("Unaligned register write offset:0x%x value:0x%x", offset, value);
		return;
	}
	DPRINTF("Register write: 0x%x value: 0x%x", offset, value);

	switch (offset) {
	case E1000_CTRL:
	case E1000_CTRL_DUP:
		e82545_devctl(sc, value);
		break;
	case E1000_FCAL:
		sc->esc_FCAL = value;
		break;
	case E1000_FCAH:
		sc->esc_FCAH = value & ~0xFFFF0000;
		break;
	case E1000_FCT:
		sc->esc_FCT = value & ~0xFFFF0000;
		break;
	case E1000_VET:
		sc->esc_VET = value & ~0xFFFF0000;
		break;
	case E1000_FCTTV:
		sc->esc_FCTTV = value & ~0xFFFF0000;
		break;
	case E1000_LEDCTL:
		sc->esc_LEDCTL = value & ~0x30303000;
		break;
	case E1000_PBA:
		sc->esc_PBA = value & 0x0000FF80;
		break;
	case E1000_ICR:
	case E1000_ITR:
	case E1000_ICS:
	case E1000_IMS:
	case E1000_IMC:
		e82545_intr_write(sc, offset, value);
		break;
	case E1000_RCTL:
		e82545_rx_ctl(sc, value);
		break;
	case E1000_FCRTL:
		sc->esc_FCRTL = value & ~0xFFFF0007;
		break;
	case E1000_FCRTH:
		sc->esc_FCRTH = value & ~0xFFFF0007;
		break;
	case E1000_RDBAL(0):
		sc->esc_RDBAL = value & ~0xF;
		if (sc->esc_rx_enabled) {
			/* Apparently legal: update cached address */
			e82545_rx_update_rdba(sc);
		}
		break;
	case E1000_RDBAH(0):
		assert(!sc->esc_rx_enabled);
		sc->esc_RDBAH = value;
		break;
	case E1000_RDLEN(0):
		assert(!sc->esc_rx_enabled);
		sc->esc_RDLEN = value & ~0xFFF0007F;
		break;
	case E1000_RDH(0):
		/* XXX should only ever be zero ? Range check ? */
		sc->esc_RDH = value;
		break;
	case E1000_RDT(0):
		/* XXX if this opens up the rx ring, do something ? */
		sc->esc_RDT = value;
		break;
	case E1000_RDTR:
		/* ignore FPD bit 31 */
		sc->esc_RDTR = value & ~0xFFFF0000;
		break;
	case E1000_RXDCTL(0):
		sc->esc_RXDCTL = value & ~0xFEC0C0C0;
		break;
	case E1000_RADV:
		sc->esc_RADV = value & ~0xFFFF0000;
		break;
	case E1000_RSRPD:
		sc->esc_RSRPD = value & ~0xFFFFF000;
		break;
	case E1000_RXCSUM:
		sc->esc_RXCSUM = value & ~0xFFFFF800;
		break;
	case E1000_TXCW:
		sc->esc_TXCW = value & ~0x3FFF0000;
		break;
	case E1000_TCTL:
		e82545_tx_ctl(sc, value);
		break;
	case E1000_TIPG:
		sc->esc_TIPG = value;
		break;
	case E1000_AIT:
		sc->esc_AIT = value;
		break;
	case E1000_TDBAL(0):
		sc->esc_TDBAL = value & ~0xF;
		if (sc->esc_tx_enabled)
			e82545_tx_update_tdba(sc);
		break;
	case E1000_TDBAH(0):
		sc->esc_TDBAH = value;
		if (sc->esc_tx_enabled)
			e82545_tx_update_tdba(sc);
		break;
	case E1000_TDLEN(0):
		sc->esc_TDLEN = value & ~0xFFF0007F;
		if (sc->esc_tx_enabled)
			e82545_tx_update_tdba(sc);
		break;
	case E1000_TDH(0):
		//assert(!sc->esc_tx_enabled);
		/* XXX should only ever be zero ? Range check ? */
		sc->esc_TDHr = sc->esc_TDH = value;
		break;
	case E1000_TDT(0):
		/* XXX range check ? */
		sc->esc_TDT = value;
		if (sc->esc_tx_enabled)
			e82545_tx_start(sc);
		break;
	case E1000_TIDV:
		sc->esc_TIDV = value & ~0xFFFF0000;
		break;
	case E1000_TXDCTL(0):
		//assert(!sc->esc_tx_enabled);
		sc->esc_TXDCTL = value & ~0xC0C0C0;
		break;
	case E1000_TADV:
		sc->esc_TADV = value & ~0xFFFF0000;
		break;
	case E1000_RAL(0) ... E1000_RAH(15):
		/* convert to u32 offset */
		ridx = (offset - E1000_RAL(0)) >> 2;
		e82545_write_ra(sc, ridx, value);
		break;
	case E1000_MTA ... (E1000_MTA + (127*4)):
		sc->esc_fmcast[(offset - E1000_MTA) >> 2] = value;
		break;
	case E1000_VFTA ... (E1000_VFTA + (127*4)):
		sc->esc_fvlan[(offset - E1000_VFTA) >> 2] = value;
		break;
	case E1000_EECD:
	{
		//DPRINTF("EECD write 0x%x -> 0x%x", sc->eeprom_control, value);
		/* edge triggered low->high */
		uint32_t eecd_strobe = ((sc->eeprom_control & E1000_EECD_SK) ?
			0 : (value & E1000_EECD_SK));
		uint32_t eecd_mask = (E1000_EECD_SK|E1000_EECD_CS|
					E1000_EECD_DI|E1000_EECD_REQ);
		sc->eeprom_control &= ~eecd_mask;
		sc->eeprom_control |= (value & eecd_mask);
		/* grant/revoke immediately */
		if (value & E1000_EECD_REQ) {
			sc->eeprom_control |= E1000_EECD_GNT;
		} else {
                        sc->eeprom_control &= ~E1000_EECD_GNT;
		}
		if (eecd_strobe && (sc->eeprom_control & E1000_EECD_CS)) {
			e82545_eecd_strobe(sc);
		}
		return;
	}
	case E1000_MDIC:
	{
		uint8_t reg_addr = (uint8_t)((value & E1000_MDIC_REG_MASK) >>
						E1000_MDIC_REG_SHIFT);
		uint8_t phy_addr = (uint8_t)((value & E1000_MDIC_PHY_MASK) >>
						E1000_MDIC_PHY_SHIFT);
		sc->mdi_control =
			(value & ~(E1000_MDIC_ERROR|E1000_MDIC_DEST));
		if ((value & E1000_MDIC_READY) != 0) {
			DPRINTF("Incorrect MDIC ready bit: 0x%x", value);
			return;
		}
		switch (value & E82545_MDIC_OP_MASK) {
		case E1000_MDIC_OP_READ:
			sc->mdi_control &= ~E82545_MDIC_DATA_MASK;
			sc->mdi_control |= e82545_read_mdi(sc, reg_addr, phy_addr);
			break;
		case E1000_MDIC_OP_WRITE:
			e82545_write_mdi(sc, reg_addr, phy_addr,
				value & E82545_MDIC_DATA_MASK);
			break;
		default:
			DPRINTF("Unknown MDIC op: 0x%x", value);
			return;
		}
		/* TODO: barrier? */
		sc->mdi_control |= E1000_MDIC_READY;
		if (value & E82545_MDIC_IE) {
			// TODO: generate interrupt
		}
		return;
	}
	case E1000_MANC:
	case E1000_STATUS:
		return;
	default:
		DPRINTF("Unknown write register: 0x%x value:%x", offset, value);
		return;
	}
}

static uint32_t
e82545_read_register(struct e82545_softc *sc, uint32_t offset)
{
	uint32_t retval;
	int ridx;

	if (offset & 0x3) {
		DPRINTF("Unaligned register read offset:0x%x", offset);
		return 0;
	}

	DPRINTF("Register read: 0x%x", offset);

	switch (offset) {
	case E1000_CTRL:
		retval = sc->esc_CTRL;
		break;
	case E1000_STATUS:
		retval = E1000_STATUS_FD | E1000_STATUS_LU |
		    E1000_STATUS_SPEED_1000;
		break;
	case E1000_FCAL:
		retval = sc->esc_FCAL;
		break;
	case E1000_FCAH:
		retval = sc->esc_FCAH;
		break;
	case E1000_FCT:
		retval = sc->esc_FCT;
		break;
	case E1000_VET:
		retval = sc->esc_VET;
		break;
	case E1000_FCTTV:
		retval = sc->esc_FCTTV;
		break;
	case E1000_LEDCTL:
		retval = sc->esc_LEDCTL;
		break;
	case E1000_PBA:
		retval = sc->esc_PBA;
		break;
	case E1000_ICR:
	case E1000_ITR:
	case E1000_ICS:
	case E1000_IMS:
	case E1000_IMC:
		retval = e82545_intr_read(sc, offset);
		break;
	case E1000_RCTL:
		retval = sc->esc_RCTL;
		break;
	case E1000_FCRTL:
		retval = sc->esc_FCRTL;
		break;
	case E1000_FCRTH:
		retval = sc->esc_FCRTH;
		break;
	case E1000_RDBAL(0):
		retval = sc->esc_RDBAL;
		break;
	case E1000_RDBAH(0):
		retval = sc->esc_RDBAH;
		break;
	case E1000_RDLEN(0):
		retval = sc->esc_RDLEN;
		break;
	case E1000_RDH(0):
		retval = sc->esc_RDH;
		break;
	case E1000_RDT(0):
		retval = sc->esc_RDT;
		break;
	case E1000_RDTR:
		retval = sc->esc_RDTR;
		break;
	case E1000_RXDCTL(0):
		retval = sc->esc_RXDCTL;
		break;
	case E1000_RADV:
		retval = sc->esc_RADV;
		break;
	case E1000_RSRPD:
		retval = sc->esc_RSRPD;
		break;
	case E1000_RXCSUM:
		retval = sc->esc_RXCSUM;
		break;
	case E1000_TXCW:
		retval = sc->esc_TXCW;
		break;
	case E1000_TCTL:
		retval = sc->esc_TCTL;
		break;
	case E1000_TIPG:
		retval = sc->esc_TIPG;
		break;
	case E1000_AIT:
		retval = sc->esc_AIT;
		break;
	case E1000_TDBAL(0):
		retval = sc->esc_TDBAL;
		break;
	case E1000_TDBAH(0):
		retval = sc->esc_TDBAH;
		break;
	case E1000_TDLEN(0):
		retval = sc->esc_TDLEN;
		break;
	case E1000_TDH(0):
		retval = sc->esc_TDH;
		break;
	case E1000_TDT(0):
		retval = sc->esc_TDT;
		break;
	case E1000_TIDV:
		retval = sc->esc_TIDV;
		break;
	case E1000_TXDCTL(0):
		retval = sc->esc_TXDCTL;
		break;
	case E1000_TADV:
		retval = sc->esc_TADV;
		break;
	case E1000_RAL(0) ... E1000_RAH(15):
		/* convert to u32 offset */
		ridx = (offset - E1000_RAL(0)) >> 2;
		retval = e82545_read_ra(sc, ridx);
		break;
	case E1000_MTA ... (E1000_MTA + (127*4)):
		retval = sc->esc_fmcast[(offset - E1000_MTA) >> 2];
		break;
	case E1000_VFTA ... (E1000_VFTA + (127*4)):
		retval = sc->esc_fvlan[(offset - E1000_VFTA) >> 2];
		break;
	case E1000_EECD:
		//DPRINTF("EECD read %x", sc->eeprom_control);
		retval = sc->eeprom_control;
		break;
	case E1000_MDIC:
		retval = sc->mdi_control;
		break;
	case E1000_MANC:
		retval = 0;
		break;
	/* stats that we emulate. */
	case E1000_MPC:
		retval = sc->missed_pkt_count;
		break;
	case E1000_PRC64:
		retval = sc->pkt_rx_by_size[0];
		break;
	case E1000_PRC127:
		retval = sc->pkt_rx_by_size[1];
		break;
	case E1000_PRC255:
		retval = sc->pkt_rx_by_size[2];
		break;
	case E1000_PRC511:
		retval = sc->pkt_rx_by_size[3];
		break;
	case E1000_PRC1023:
		retval = sc->pkt_rx_by_size[4];
		break;
	case E1000_PRC1522:
		retval = sc->pkt_rx_by_size[5];
		break;
	case E1000_GPRC:
		retval = sc->good_pkt_rx_count;
		break;
	case E1000_BPRC:
		retval = sc->bcast_pkt_rx_count;
		break;
	case E1000_MPRC:
		retval = sc->mcast_pkt_rx_count;
		break;
	case E1000_GPTC:
	case E1000_TPT:
		retval = sc->good_pkt_tx_count;
		break;
	case E1000_GORCL:
		retval = (uint32_t)sc->good_octets_rx;
		break;
	case E1000_GORCH:
		retval = (uint32_t)(sc->good_octets_rx >> 32);
		break;
	case E1000_TOTL:
	case E1000_GOTCL:
		retval = (uint32_t)sc->good_octets_tx;
		break;
	case E1000_TOTH:
	case E1000_GOTCH:
		retval = (uint32_t)(sc->good_octets_tx >> 32);
		break;
	case E1000_ROC:
		retval = sc->oversize_rx_count;
		break;
	case E1000_TORL:
		retval = (uint32_t)(sc->good_octets_rx + sc->missed_octets);
		break;
	case E1000_TORH:
		retval = (uint32_t)((sc->good_octets_rx +
		    sc->missed_octets) >> 32);
		break;
	case E1000_TPR:
		retval = sc->good_pkt_rx_count + sc->missed_pkt_count +
		    sc->oversize_rx_count;
		break;
	case E1000_PTC64:
		retval = sc->pkt_tx_by_size[0];
		break;
	case E1000_PTC127:
		retval = sc->pkt_tx_by_size[1];
		break;
	case E1000_PTC255:
		retval = sc->pkt_tx_by_size[2];
		break;
	case E1000_PTC511:
		retval = sc->pkt_tx_by_size[3];
		break;
	case E1000_PTC1023:
		retval = sc->pkt_tx_by_size[4];
		break;
	case E1000_PTC1522:
		retval = sc->pkt_tx_by_size[5];
		break;
	case E1000_MPTC:
		retval = sc->mcast_pkt_tx_count;
		break;
	case E1000_BPTC:
		retval = sc->bcast_pkt_tx_count;
		break;
	case E1000_TSCTC:
		retval = sc->tso_tx_count;
		break;
	/* stats that are always 0. */
	case E1000_CRCERRS:
	case E1000_ALGNERRC:
	case E1000_SYMERRS:
	case E1000_RXERRC:
	case E1000_SCC:
	case E1000_ECOL:
	case E1000_MCC:
	case E1000_LATECOL:
	case E1000_COLC:
	case E1000_DC:
	case E1000_TNCRS:
	case E1000_SEC:
	case E1000_CEXTERR:
	case E1000_RLEC:
	case E1000_XONRXC:
	case E1000_XONTXC:
	case E1000_XOFFRXC:
	case E1000_XOFFTXC:
	case E1000_FCRUC:
	case E1000_RNBC:
	case E1000_RUC:
	case E1000_RFC:
	case E1000_RJC:
	case E1000_MGTPRC:
	case E1000_MGTPDC:
	case E1000_MGTPTC:
	case E1000_TSCTFC:
		retval = 0;
		break;
	default:
		DPRINTF("Unknown read register: 0x%x", offset);
		retval = 0;
		break;
	}

	return (retval);
}

static void
e82545_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
	     uint64_t offset, int size, uint64_t value)
{
	struct e82545_softc *sc;

	//DPRINTF("Write bar:%d offset:0x%lx value:0x%lx size:%d", baridx, offset, value, size);

	sc = pi->pi_arg;

	pthread_mutex_lock(&sc->esc_mtx);

	switch (baridx) {
	case E82545_BAR_IO:
		switch (offset) {
		case E82545_IOADDR:
			if (size != 4) {
				DPRINTF("Wrong io addr write sz:%d value:0x%lx", size, value);
			} else
				sc->io_addr = (uint32_t)value;
			break;
		case E82545_IODATA:
			if (size != 4) {
				DPRINTF("Wrong io data write size:%d value:0x%lx", size, value);
			} else if (sc->io_addr > E82545_IO_REGISTER_MAX) {
				DPRINTF("Non-register io write addr:0x%x value:0x%lx", sc->io_addr, value);
			} else
				e82545_write_register(sc, sc->io_addr,
						      (uint32_t)value);
			break;
		default:
			DPRINTF("Unknown io bar write offset:0x%lx value:0x%lx size:%d", offset, value, size);
			break;
		}
		break;
	case E82545_BAR_REGISTER:
		if (size != 4) {
			DPRINTF("Wrong register write size:%d offset:0x%lx value:0x%lx", size, offset, value);
		} else
			e82545_write_register(sc, (uint32_t)offset,
					      (uint32_t)value);
		break;
	default:
		DPRINTF("Unknown write bar:%d off:0x%lx val:0x%lx size:%d",
			baridx, offset, value, size);
	}

	pthread_mutex_unlock(&sc->esc_mtx);
}

static uint64_t
e82545_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
	    uint64_t offset, int size)
{
	struct e82545_softc *sc;
	uint64_t retval;

	//DPRINTF("Read  bar:%d offset:0x%lx size:%d", baridx, offset, size);
	sc = pi->pi_arg;
	retval = 0;

	pthread_mutex_lock(&sc->esc_mtx);

	switch (baridx) {
	case E82545_BAR_IO:
		switch (offset) {
		case E82545_IOADDR:
			if (size != 4) {
				DPRINTF("Wrong io addr read sz:%d", size);
			} else
				retval = sc->io_addr;
			break;
		case E82545_IODATA:
			if (size != 4) {
				DPRINTF("Wrong io data read sz:%d", size);
			}
			if (sc->io_addr > E82545_IO_REGISTER_MAX) {
				DPRINTF("Non-register io read addr:0x%x",
					sc->io_addr);
			} else
				retval = e82545_read_register(sc, sc->io_addr);
			break;
		default:
			DPRINTF("Unknown io bar read offset:0x%lx size:%d",
				offset, size);
			break;
		}
		break;
	case E82545_BAR_REGISTER:
		if (size != 4) {
			DPRINTF("Wrong register read size:%d offset:0x%lx",
				size, offset);
		} else
			retval = e82545_read_register(sc, (uint32_t)offset);
		break;
	default:
		DPRINTF("Unknown read bar:%d offset:0x%lx size:%d",
			baridx, offset, size);
		break;
	}

	pthread_mutex_unlock(&sc->esc_mtx);

	return (retval);
}

static void
e82545_reset(struct e82545_softc *sc, int drvr)
{
	int i;

	e82545_rx_disable(sc);
	e82545_tx_disable(sc);

	/* clear outstanding interrupts */
	if (sc->esc_irq_asserted)
		pci_lintr_deassert(sc->esc_pi);

	/* misc */
	if (!drvr) {
		sc->esc_FCAL = 0;
		sc->esc_FCAH = 0;
		sc->esc_FCT = 0;
		sc->esc_VET = 0;
		sc->esc_FCTTV = 0;
	}
	sc->esc_LEDCTL = 0x07061302;
	sc->esc_PBA = 0x00100030;

	/* start nvm in opcode mode. */
	sc->nvm_opaddr = 0;
	sc->nvm_mode = E82545_NVM_MODE_OPADDR;
	sc->nvm_bits = E82545_NVM_OPADDR_BITS;
	sc->eeprom_control = E1000_EECD_PRES | E82545_EECD_FWE_EN;
	e82545_init_eeprom(sc);

	/* interrupt */
	sc->esc_ICR = 0;
	sc->esc_ITR = 250;
	sc->esc_ICS = 0;
	sc->esc_IMS = 0;
	sc->esc_IMC = 0;

	/* L2 filters */
	if (!drvr) {
		memset(sc->esc_fvlan, 0, sizeof(sc->esc_fvlan));
		memset(sc->esc_fmcast, 0, sizeof(sc->esc_fmcast));
		memset(sc->esc_uni, 0, sizeof(sc->esc_uni));

		/* XXX not necessary on 82545 ?? */
		sc->esc_uni[0].eu_valid = 1;
		memcpy(sc->esc_uni[0].eu_eth.octet, sc->esc_mac.octet,
		    ETHER_ADDR_LEN);
	} else {
		/* Clear RAH valid bits */
		for (i = 0; i < 16; i++)
			sc->esc_uni[i].eu_valid = 0;
	}

	/* receive */
	if (!drvr) {
		sc->esc_RDBAL = 0;
		sc->esc_RDBAH = 0;
	}
	sc->esc_RCTL = 0;
	sc->esc_FCRTL = 0;
	sc->esc_FCRTH = 0;
	sc->esc_RDLEN = 0;
	sc->esc_RDH = 0;
	sc->esc_RDT = 0;
	sc->esc_RDTR = 0;
	sc->esc_RXDCTL = (1 << 24) | (1 << 16); /* default GRAN/WTHRESH */
	sc->esc_RADV = 0;
	sc->esc_RXCSUM = 0;

	/* transmit */
	if (!drvr) {
		sc->esc_TDBAL = 0;
		sc->esc_TDBAH = 0;
		sc->esc_TIPG = 0;
		sc->esc_AIT = 0;
		sc->esc_TIDV = 0;
		sc->esc_TADV = 0;
	}
	sc->esc_tdba = 0;
	sc->esc_txdesc = NULL;
	sc->esc_TXCW = 0;
	sc->esc_TCTL = 0;
	sc->esc_TDLEN = 0;
	sc->esc_TDT = 0;
	sc->esc_TDHr = sc->esc_TDH = 0;
	sc->esc_TXDCTL = 0;
}

static int
e82545_init(struct vmctx *ctx, struct pci_devinst *pi, nvlist_t *nvl)
{
	char nstr[80];
	struct e82545_softc *sc;
	const char *mac;
	int err;

	/* Setup our softc */
	sc = calloc(1, sizeof(*sc));

	pi->pi_arg = sc;
	sc->esc_pi = pi;
	sc->esc_ctx = ctx;

	pthread_mutex_init(&sc->esc_mtx, NULL);
	pthread_cond_init(&sc->esc_rx_cond, NULL);
	pthread_cond_init(&sc->esc_tx_cond, NULL);
	pthread_create(&sc->esc_tx_tid, NULL, e82545_tx_thread, sc);
	snprintf(nstr, sizeof(nstr), "e82545-%d:%d tx", pi->pi_slot,
	    pi->pi_func);
        pthread_set_name_np(sc->esc_tx_tid, nstr);

	pci_set_cfgdata16(pi, PCIR_DEVICE, E82545_DEV_ID_82545EM_COPPER);
	pci_set_cfgdata16(pi, PCIR_VENDOR, E82545_VENDOR_ID_INTEL);
	pci_set_cfgdata8(pi,  PCIR_CLASS, PCIC_NETWORK);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_NETWORK_ETHERNET);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, E82545_SUBDEV_ID);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, E82545_VENDOR_ID_INTEL);

	pci_set_cfgdata8(pi,  PCIR_HDRTYPE, PCIM_HDRTYPE_NORMAL);
	pci_set_cfgdata8(pi,  PCIR_INTPIN, 0x1);

	/* TODO: this card also supports msi, but the freebsd driver for it
	 * does not, so I have not implemented it. */
	pci_lintr_request(pi);

	pci_emul_alloc_bar(pi, E82545_BAR_REGISTER, PCIBAR_MEM32,
		E82545_BAR_REGISTER_LEN);
	pci_emul_alloc_bar(pi, E82545_BAR_FLASH, PCIBAR_MEM32,
		E82545_BAR_FLASH_LEN);
	pci_emul_alloc_bar(pi, E82545_BAR_IO, PCIBAR_IO,
		E82545_BAR_IO_LEN);

	mac = get_config_value_node(nvl, "mac");
	if (mac != NULL) {
		err = net_parsemac(mac, sc->esc_mac.octet);
		if (err) {
			free(sc);
			return (err);
		}
	} else
		net_genmac(pi, sc->esc_mac.octet);

	err = netbe_init(&sc->esc_be, nvl, e82545_rx_callback, sc);
	if (err) {
		free(sc);
		return (err);
	}

	netbe_rx_enable(sc->esc_be);

	/* H/w initiated reset */
	e82545_reset(sc, 0);

	return (0);
}

#ifdef BHYVE_SNAPSHOT
static int
e82545_snapshot(struct vm_snapshot_meta *meta)
{
	int i;
	int ret;
	struct e82545_softc *sc;
	struct pci_devinst *pi;
	uint64_t bitmap_value;

	pi = meta->dev_data;
	sc = pi->pi_arg;

	/* esc_mevp and esc_mevpitr should be reinitiated at init. */
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_mac, meta, ret, done);

	/* General */
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_CTRL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_FCAL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_FCAH, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_FCT, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_VET, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_FCTTV, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_LEDCTL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_PBA, meta, ret, done);

	/* Interrupt control */
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_irq_asserted, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_ICR, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_ITR, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_ICS, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_IMS, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_IMC, meta, ret, done);

	/*
	 * Transmit
	 *
	 * The fields in the unions are in superposition to access certain
	 * bytes in the larger uint variables.
	 * e.g., ip_config = [ipcss|ipcso|ipcse0|ipcse1]
	 */
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_txctx.lower_setup.ip_config, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_txctx.upper_setup.tcp_config, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_txctx.cmd_and_length, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_txctx.tcp_seg_setup.data, meta, ret, done);

	SNAPSHOT_VAR_OR_LEAVE(sc->esc_tx_enabled, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_tx_active, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TXCW, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TCTL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TIPG, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_AIT, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_tdba, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TDBAL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TDBAH, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TDLEN, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TDH, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TDHr, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TDT, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TIDV, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TXDCTL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_TADV, meta, ret, done);

	/* Has dependency on esc_TDLEN; reoreder of fields from struct. */
	SNAPSHOT_GUEST2HOST_ADDR_OR_LEAVE(sc->esc_txdesc, sc->esc_TDLEN,
		true, meta, ret, done);

	/* L2 frame acceptance */
	for (i = 0; i < nitems(sc->esc_uni); i++) {
		SNAPSHOT_VAR_OR_LEAVE(sc->esc_uni[i].eu_valid, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(sc->esc_uni[i].eu_addrsel, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(sc->esc_uni[i].eu_eth, meta, ret, done);
	}

	SNAPSHOT_BUF_OR_LEAVE(sc->esc_fmcast, sizeof(sc->esc_fmcast),
			      meta, ret, done);
	SNAPSHOT_BUF_OR_LEAVE(sc->esc_fvlan, sizeof(sc->esc_fvlan),
			      meta, ret, done);

	/* Receive */
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_rx_enabled, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_rx_active, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_rx_loopback, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RCTL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_FCRTL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_FCRTH, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_rdba, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RDBAL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RDBAH, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RDLEN, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RDH, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RDT, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RDTR, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RXDCTL, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RADV, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RSRPD, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->esc_RXCSUM, meta, ret, done);

	/* Has dependency on esc_RDLEN; reoreder of fields from struct. */
	SNAPSHOT_GUEST2HOST_ADDR_OR_LEAVE(sc->esc_rxdesc, sc->esc_TDLEN,
		true, meta, ret, done);

	/* IO Port register access */
	SNAPSHOT_VAR_OR_LEAVE(sc->io_addr, meta, ret, done);

	/* Shadow copy of MDIC */
	SNAPSHOT_VAR_OR_LEAVE(sc->mdi_control, meta, ret, done);

	/* Shadow copy of EECD */
	SNAPSHOT_VAR_OR_LEAVE(sc->eeprom_control, meta, ret, done);

	/* Latest NVM in/out */
	SNAPSHOT_VAR_OR_LEAVE(sc->nvm_data, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->nvm_opaddr, meta, ret, done);

	/* Stats */
	SNAPSHOT_VAR_OR_LEAVE(sc->missed_pkt_count, meta, ret, done);
	SNAPSHOT_BUF_OR_LEAVE(sc->pkt_rx_by_size, sizeof(sc->pkt_rx_by_size),
			      meta, ret, done);
	SNAPSHOT_BUF_OR_LEAVE(sc->pkt_tx_by_size, sizeof(sc->pkt_tx_by_size),
			      meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->good_pkt_rx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->bcast_pkt_rx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->mcast_pkt_rx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->good_pkt_tx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->bcast_pkt_tx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->mcast_pkt_tx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->oversize_rx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->tso_tx_count, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->good_octets_rx, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->good_octets_tx, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->missed_octets, meta, ret, done);

	if (meta->op == VM_SNAPSHOT_SAVE)
		bitmap_value = sc->nvm_bits;
	SNAPSHOT_VAR_OR_LEAVE(bitmap_value, meta, ret, done);
	if (meta->op == VM_SNAPSHOT_RESTORE)
		sc->nvm_bits = bitmap_value;

	if (meta->op == VM_SNAPSHOT_SAVE)
		bitmap_value = sc->nvm_bits;
	SNAPSHOT_VAR_OR_LEAVE(bitmap_value, meta, ret, done);
	if (meta->op == VM_SNAPSHOT_RESTORE)
		sc->nvm_bits = bitmap_value;

	/* EEPROM data */
	SNAPSHOT_BUF_OR_LEAVE(sc->eeprom_data, sizeof(sc->eeprom_data),
			      meta, ret, done);

done:
	return (ret);
}
#endif

static const struct pci_devemu pci_de_e82545 = {
	.pe_emu = 	"e1000",
	.pe_init =	e82545_init,
	.pe_legacy_config = netbe_legacy_config,
	.pe_barwrite =	e82545_write,
	.pe_barread =	e82545_read,
#ifdef BHYVE_SNAPSHOT
	.pe_snapshot =	e82545_snapshot,
#endif
};
PCI_EMUL_SET(pci_de_e82545);
