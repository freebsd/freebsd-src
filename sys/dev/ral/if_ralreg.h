/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005
 *	Damien Bergamini <damien.bergamini@free.fr>
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

#define RAL_TX_RING_COUNT	48
#define RAL_ATIM_RING_COUNT	4
#define RAL_PRIO_RING_COUNT	16
#define RAL_BEACON_RING_COUNT	1
#define RAL_RX_RING_COUNT	32

#define RAL_TX_DESC_SIZE	(sizeof (struct ral_tx_desc))
#define RAL_RX_DESC_SIZE	(sizeof (struct ral_rx_desc))

#define RAL_MAX_SCATTER	1

/*
 * Control and status registers.
 */
#define RAL_CSR0	0x0000	/* ASIC version number */
#define RAL_CSR1	0x0004	/* System control */
#define RAL_CSR3	0x000c	/* STA MAC address 0 */
#define RAL_CSR4	0x0010	/* STA MAC address 1 */
#define RAL_CSR5	0x0014	/* BSSID 0 */
#define RAL_CSR6	0x0018	/* BSSID 1 */
#define RAL_CSR7	0x001c	/* Interrupt source */
#define RAL_CSR8	0x0020	/* Interrupt mask */
#define RAL_CSR9	0x0024	/* Maximum frame length */
#define RAL_SECCSR0	0x0028	/* WEP control */
#define RAL_CSR11	0x002c	/* Back-off control */
#define RAL_CSR12	0x0030	/* Synchronization configuration 0 */
#define RAL_CSR13	0x0034	/* Synchronization configuration 1 */
#define RAL_CSR14	0x0038	/* Synchronization control */
#define RAL_CSR15	0x003c	/* Synchronization status */
#define RAL_CSR16	0x0040	/* TSF timer 0 */
#define RAL_CSR17	0x0044	/* TSF timer 1 */
#define RAL_CSR18	0x0048	/* IFS timer 0 */
#define RAL_CSR19	0x004c	/* IFS timer 1 */
#define RAL_CSR20	0x0050	/* WAKEUP timer */
#define RAL_CSR21	0x0054	/* EEPROM control */
#define RAL_CSR22	0x0058	/* CFP control */
#define RAL_TXCSR0	0x0060	/* TX control */
#define RAL_TXCSR1	0x0064	/* TX configuration */
#define RAL_TXCSR2	0x0068	/* TX descriptor configuration */
#define RAL_TXCSR3	0x006c	/* TX ring base address */
#define RAL_TXCSR4	0x0070	/* TX ATIM ring base address */
#define RAL_TXCSR5	0x0074	/* TX PRIO ring base address */
#define RAL_TXCSR6	0x0078	/* Beacon base address */
#define RAL_TXCSR7	0x007c	/* AutoResponder control */
#define RAL_RXCSR0	0x0080	/* RX control */
#define RAL_RXCSR1	0x0084	/* RX descriptor configuration */
#define RAL_RXCSR2	0x0088	/* RX ring base address */
#define RAL_PCICSR	0x008c	/* PCI control */
#define RAL_RXCSR3	0x0090	/* BBP ID 0 */
#define RAL_TXCSR9	0x0094	/* OFDM TX BBP */
#define RAL_ARSP_PLCP_0	0x0098	/* Auto Responder PLCP address */
#define RAL_ARSP_PLCP_1	0x009c	/* Auto Responder PLCP Basic Rate bit mask */
#define RAL_CNT0	0x00a0	/* FCS error counter */
#define RAL_CNT1	0x00ac	/* PLCP error counter */
#define RAL_CNT2	0x00b0	/* Long error counter */
#define RAL_CNT3	0x00b8	/* CCA false alarm counter */
#define RAL_CNT4	0x00bc	/* RX FIFO Overflow counter */
#define RAL_CNT5	0x00c0	/* Tx FIFO Underrun counter */
#define RAL_PWRCSR0	0x00c4	/* Power mode configuration */
#define RAL_PSCSR0	0x00c8	/* Power state transition time */
#define RAL_PSCSR1	0x00cc	/* Power state transition time */
#define RAL_PSCSR2	0x00d0	/* Power state transition time */
#define RAL_PSCSR3	0x00d4	/* Power state transition time */
#define RAL_PWRCSR1	0x00d8	/* Manual power control/status */
#define RAL_TIMECSR	0x00dc	/* Timer control */
#define RAL_MACCSR0	0x00e0	/* MAC configuration */
#define RAL_MACCSR1	0x00e4	/* MAC configuration */
#define RAL_RALINKCSR	0x00e8	/* Ralink RX auto-reset BBCR */
#define RAL_BCNCSR	0x00ec	/* Beacon interval control */
#define RAL_BBPCSR	0x00f0	/* BBP serial control */
#define RAL_RFCSR	0x00f4	/* RF serial control */
#define RAL_LEDCSR	0x00f8	/* LED control */
#define RAL_SECCSR3	0x00fc	/* XXX not documented */
#define RAL_DMACSR0	0x0100	/* Current RX ring address */
#define RAL_DMACSR1	0x0104	/* Current Tx ring address */
#define RAL_DMACSR2	0x0104	/* Current Priority ring address */
#define RAL_DMACSR3	0x0104	/* Current ATIM ring address */
#define RAL_TXACKCSR0	0x0110	/* XXX not documented */
#define RAL_GPIOCSR	0x0120	/* */
#define RAL_BBBPPCSR	0x0124	/* BBP Pin Control */
#define RAL_FIFOCSR0	0x0128	/* TX FIFO pointer */
#define RAL_FIFOCSR1	0x012c	/* RX FIFO pointer */
#define RAL_BCNOCSR	0x0130	/* Beacon time offset */
#define RAL_RLPWCSR	0x0134	/* RX_PE Low Width */
#define RAL_TESTCSR	0x0138	/* Test Mode Select */
#define RAL_PLCP1MCSR	0x013c	/* Signal/Service/Length of ACK/CTS @1M */
#define RAL_PLCP2MCSR	0x0140	/* Signal/Service/Length of ACK/CTS @2M */
#define RAL_PLCP5p5MCSR	0x0144	/* Signal/Service/Length of ACK/CTS @5.5M */
#define RAL_PLCP11MCSR	0x0148	/* Signal/Service/Length of ACK/CTS @11M */
#define RAL_ACKPCTCSR	0x014c	/* ACK/CTS padload consume time */
#define RAL_ARTCSR1	0x0150	/* ACK/CTS padload consume time */
#define RAL_ARTCSR2	0x0154	/* ACK/CTS padload consume time */
#define RAL_SECCSR1	0x0158	/* WEP control */
#define RAL_BBPCSR1	0x015c	/* BBP TX Configuration */


/* possible flags for register RXCSR0 */
#define RAL_DISABLE_RX		(1 << 0)
#define RAL_DROP_CRC_ERROR	(1 << 1)
#define RAL_DROP_PHY_ERROR	(1 << 2)
#define RAL_DROP_CTL		(1 << 3)
#define RAL_DROP_NOT_TO_ME	(1 << 4)
#define RAL_DROP_TODS		(1 << 5)
#define RAL_DROP_VERSION_ERROR	(1 << 6)

/* possible flags for register CSR1 */
#define RAL_RESET_ASIC	(1 << 0)
#define RAL_RESET_BBP	(1 << 1)
#define RAL_HOST_READY	(1 << 2)

/* possible flags for register CSR14 */
#define RAL_ENABLE_TSF			(1 << 0)
#define RAL_ENABLE_TSF_SYNC(x)		(((x) & 0x3) << 1)
#define RAL_ENABLE_TBCN			(1 << 3)
#define RAL_ENABLE_BEACON_GENERATOR	(1 << 6)

/* possible flags for register CSR21 */
#define RAL_EEPROM_C		(1 << 1)
#define RAL_EEPROM_S		(1 << 2)
#define RAL_EEPROM_D		(1 << 3)
#define RAL_EEPROM_Q		(1 << 4)
#define RAL_EEPROM_93C46	(1 << 5)

#define RAL_EEPROM_SHIFT_D	3
#define RAL_EEPROM_SHIFT_Q	4

/* possible flags for register TXCSR0 */
#define RAL_KICK_TX	(1 << 0)
#define RAL_KICK_ATIM	(1 << 1)
#define RAL_KICK_PRIO	(1 << 2)
#define RAL_ABORT_TX	(1 << 3)

/* possible flags for register SECCSR0 */
#define RAL_KICK_DECRYPT	(1 << 0)

/* possible flags for register SECCSR1 */
#define RAL_KICK_ENCRYPT	(1 << 0)

/* possible flags for register CSR7 */
#define RAL_BEACON_EXPIRE	0x00000001
#define RAL_WAKEUP_EXPIRE	0x00000002
#define RAL_ATIM_EXPIRE		0x00000004
#define RAL_TX_DONE		0x00000008
#define RAL_ATIM_DONE		0x00000010
#define RAL_PRIO_DONE		0x00000020
#define RAL_RX_DONE		0x00000040
#define RAL_DECRYPTION_DONE	0x00000080
#define RAL_ENCRYPTION_DONE	0x00000100

#define RAL_INTR_MASK							\
	(~(RAL_BEACON_EXPIRE | RAL_WAKEUP_EXPIRE | RAL_TX_DONE |	\
	   RAL_PRIO_DONE | RAL_RX_DONE | RAL_DECRYPTION_DONE |		\
	   RAL_ENCRYPTION_DONE))

/* Tx descriptor */
struct ral_tx_desc {
	uint32_t	flags;
#define RAL_TX_BUSY		(1 << 0)
#define RAL_TX_VALID		(1 << 1)

#define RAL_TX_RESULT_MASK	0x0000001c
#define RAL_TX_SUCCESS		(0 << 2)
#define RAL_TX_SUCCESS_RETRY	(1 << 2)
#define RAL_TX_FAIL_RETRY	(2 << 2)
#define RAL_TX_FAIL_INVALID	(3 << 2)
#define RAL_TX_FAIL_OTHER	(4 << 2)

#define RAL_TX_MORE_FRAG	(1 << 8)
#define RAL_TX_ACK		(1 << 9)
#define RAL_TX_TIMESTAMP	(1 << 10)
#define RAL_TX_OFDM		(1 << 11)
#define RAL_TX_CIPHER_BUSY	(1 << 12)

#define RAL_TX_IFS_MASK		0x00006000
#define RAL_TX_IFS_BACKOFF	(0 << 13)
#define RAL_TX_IFS_SIFS		(1 << 13)
#define RAL_TX_IFS_NEWBACKOFF	(2 << 13)
#define RAL_TX_IFS_NONE		(3 << 13)

#define RAL_TX_LONG_RETRY	(1 << 15)

#define RAL_TX_CIPHER_MASK	0xe0000000
#define RAL_TX_CIPHER_NONE	(0 << 29)
#define RAL_TX_CIPHER_WEP40	(1 << 29)
#define RAL_TX_CIPHER_WEP104	(2 << 29)
#define RAL_TX_CIPHER_TKIP	(3 << 29)
#define RAL_TX_CIPHER_AES	(4 << 29)

	uint32_t	physaddr;
	uint16_t	wme;
#define RAL_LOGCWMAX(x)		(((x) & 0xf) << 12)
#define RAL_LOGCWMIN(x)		(((x) & 0xf) << 8)
#define RAL_AIFSN(x)		(((x) & 0x3) << 6)
#define RAL_IVOFFSET(x)		(((x) & 0x3f))

	uint16_t	reserved1;
	uint8_t		plcp_signal;
	uint8_t		plcp_service;
#define RAL_PLCP_LENGEXT	0x80

	uint16_t	plcp_length;
	uint32_t	iv;
	uint32_t	eiv;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint32_t	reserved2[2];
} __packed;

/* Rx descriptor */
struct ral_rx_desc {
	uint32_t	flags;
#define RAL_RX_BUSY		(1 << 0)
#define RAL_RX_CRC_ERROR	(1 << 5)
#define RAL_RX_PHY_ERROR	(1 << 7)
#define RAL_RX_CIPHER_BUSY	(1 << 8)
#define RAL_RX_ICV_ERROR	(1 << 9)

#define RAL_RX_CIPHER_MASK	0xe0000000
#define RAL_RX_CIPHER_NONE	(0 << 29)
#define RAL_RX_CIPHER_WEP40	(1 << 29)
#define RAL_RX_CIPHER_WEP104	(2 << 29)
#define RAL_RX_CIPHER_TKIP	(3 << 29)
#define RAL_RX_CIPHER_AES	(4 << 29)

	uint32_t	physaddr;
	uint8_t		rate;
	uint8_t		rssi;
	uint8_t		ta[IEEE80211_ADDR_LEN];
	uint32_t	iv;
	uint32_t	eiv;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint32_t	reserved[2];
} __packed;

#define RAL_RF1	0
#define RAL_RF2	2
#define RAL_RF3	1
#define RAL_RF4	3

#define RAL_RF1_AUTOTUNE	0x08000
#define RAL_RF3_AUTOTUNE	0x00040

#define RAL_BBP_BUSY	(1 << 15)
#define RAL_BBP_WRITE	(1 << 16)
#define RAL_RF_20BIT	(20 << 24)
#define RAL_RF_BUSY	(1 << 31)

#define RAL_RF_2522	0x00
#define RAL_RF_2523	0x01
#define RAL_RF_2524	0x02
#define RAL_RF_2525	0x03
#define RAL_RF_2525E	0x04
#define RAL_RF_2526	0x05
/* dual-band RF */
#define RAL_RF_5222	0x10

#define RAL_BBP_VERSION	0
#define RAL_BBP_TX	2
#define RAL_BBP_RX	14

#define RAL_BBP_ANTA		0x00
#define RAL_BBP_DIVERSITY	0x01
#define RAL_BBP_ANTB		0x02
#define RAL_BBP_ANTMASK		0x03
#define RAL_BBP_FLIPIQ		0x04

#define RAL_LED_MODE_DEFAULT		0
#define RAL_LED_MODE_TXRX_ACTIVITY	1
#define RAL_LED_MODE_SINGLE		2
#define RAL_LED_MODE_ASUS		3

#define RAL_JAPAN_FILTER	0x8

#define RAL_EEPROM_DELAY	1	/* minimum hold time (microsecond) */

#define RAL_EEPROM_CONFIG0	16
#define RAL_EEPROM_BBP_BASE	19
#define RAL_EEPROM_TXPOWER	35

/*
 * control and status registers access macros
 */
#define RAL_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define RAL_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

/*
 * EEPROM access macro
 */
#define RAL_EEPROM_CTL(sc, val) do {					\
	RAL_WRITE((sc), RAL_CSR21, (val));				\
	DELAY(RAL_EEPROM_DELAY);					\
} while (/* CONSTCOND */0)
