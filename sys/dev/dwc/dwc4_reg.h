/*-
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#ifndef _DEV_DWC_DWC4_REG_H_
#define _DEV_DWC_DWC4_REG_H_

#define	ETH_MACCR		0x0000
#define	 MACCR_IPC	(1 << 27) /* Checksum Offload */
#define	 MACCR_ACS	(1 << 20) /* Automatic Pad or CRC Stripping */
#define	 MACCR_BE	(1 << 18) /* Packet Burst Enable */
#define	 MACCR_JD	(1 << 17) /* Jabber Disable */
#define	 MACCR_PS	(1 << 15) /* Port Select */
#define	 MACCR_FES	(1 << 14) /* MAC Speed */
#define	 MACCR_DM	(1 << 13) /* Duplex Mode */
#define	 MACCR_TE	(1 << 1) /* Transmitter Enable */
#define	 MACCR_RE	(1 << 0) /* Receiver Enable */
#define	ETH_MACECR		0x0004
#define	ETH_MACPFR		0x0008
#define	 MACPFR_RA		(1 << 31) /* Receive All */
#define	 MACPFR_PM		(1 << 4) /* Pass All Multicast */
#define	 MACPFR_HMC		(1 << 2) /* Hash Multicast */
#define	 MACPFR_PR		(1 << 0) /* Promiscuous Mode */
#define	ETH_MACWJBTR		0x000C
#define	ETH_MACHT0R		0x0010
#define	ETH_MACHT1R		0x0014
#define	ETH_MACVTCR		0x0050
#define	ETH_MACVTDR		0x0054
#define	ETH_MACVHTR		0x0058
#define	ETH_MACVIR		0x0060
#define	ETH_MACiVIR		0x0064
#define	ETH_MACQ0TXFCR		0x0070
#define	ETH_MACRXFCR		0x0090
#define	ETH_MACRXQCR		0x0094

#define	ETH_MACRXQC0R(x)	(0x00A0 + 0x4 * (x))
#define	 MACRXQC0R_RXQ0EN_S	0 /* Receive Queue 0 Enable */
#define	 MACRXQC0R_RXQ0EN_AV	(0x1 << MACRXQC0R_RXQ0EN_S)
#define	 MACRXQC0R_RXQ0EN_GEN	(0x2 << MACRXQC0R_RXQ0EN_S)
#define	 MACRXQC0R_RXQ0EN_M	(0x3 << MACRXQC0R_RXQ0EN_S)
#define	ETH_MACRXQC1R		0x00A4
#define	ETH_MACRXQC2R		0x00A8

#define	ETH_MACISR		0x00B0
#define	ETH_MACIER		0x00B4
#define	ETH_MACRXTXSR		0x00B8
#define	ETH_MACPCSR		0x00C0
#define	ETH_MACRWKPFR		0x00C4
#define	ETH_MACLCSR		0x00D0
#define	ETH_MACLTCR		0x00D4
#define	ETH_MACLETR		0x00D8
#define	ETH_MAC1USTCR		0x00DC
#define	ETH_MACPHYCSR		0x00F8
#define	ETH_MACVR		0x0110
#define	ETH_MACDR		0x0114
#define	ETH_MACHWF0R		0x011C
#define	ETH_MACHWF1R		0x0120
#define	ETH_MACHWF2R		0x0124
#define	ETH_MACHWF3R		0x0128
#define	ETH_MACMDIOAR		0x0200
#define	 MACMDIOAR_CR_S		8 /* CSR Clock Range */
#define	 MACMDIOAR_CR_M		(0xf << MACMDIOAR_CR_S)
#define	 MACMDIOAR_GOC_S	2 /* GMII Operation Command */
#define	 MACMDIOAR_GOC_M	(0x3 << MACMDIOAR_GOC_S)
#define	 MACMDIOAR_WRITE	(0x1 << MACMDIOAR_GOC_S)
#define	 MACMDIOAR_POST_RD_INC	(0x2 << MACMDIOAR_GOC_S)
#define	 MACMDIOAR_READ		(0x3 << MACMDIOAR_GOC_S)
#define	 MACMDIOAR_GB		(1 << 0) /* GMII Busy */
#define	 MACMDIOAR_PA_S		21 /* Physical Layer Address */
#define	 MACMDIOAR_PA_M		(0x1f << MACMDIOAR_PA_S)
#define	 MACMDIOAR_RDA_S	16 /* Register/Device Address */
#define	 MACMDIOAR_RDA_M	(0x1f << MACMDIOAR_RDA_S)
#define	ETH_MACMDIODR		0x0204
#define	ETH_MACARPAR		0x0210
#define	ETH_MACCSRSWCR		0x0230
#define	ETH_MACFPECSR		0x0234
#define	ETH_MACPRSTIMR		0x0240
#define	ETH_MACPRSTIMUR		0x0244
#define	ETH_MACA0HR		0x0300
#define	ETH_MACA0LR		0x0304
#define	ETH_MACA1HR		0x0308
#define	ETH_MACA1LR		0x030C
#define	ETH_MACA2HR		0x0310
#define	ETH_MACA2LR		0x0314
#define	ETH_MACA3HR		0x0318
#define	ETH_MACA3LR		0x031C
#define	ETH_MMC_CONTROL		0x0700
#define	 MMC_CONTROL_CNTRST	(1 << 0) /* Counters Reset */
#define	ETH_MMC_RX_INTERRUPT			0x0704
#define	ETH_MMC_TX_INTERRUPT			0x0708
#define	ETH_MMC_RX_INTERRUPT_MASK		0x070C
#define	ETH_MMC_TX_INTERRUPT_MASK		0x0710
#define	ETH_TX_OCTET_COUNT_GOOD_BAD		0x0714
#define	ETH_TX_PACKET_COUNT_GOOD_BAD		0x0718
#define	ETH_TX_BROADCAST_PACKETS_GOOD		0x071C
#define	ETH_TX_MULTICAST_PACKETS_GOOD		0x0720
#define	ETH_TX_64OCTETS_PACKETS_GOOD_BAD	0x0724
#define	ETH_TX_64TO127OCTETS_PACKETS_GOOD_BAD	0x0728
#define	ETH_TX_128TO255OCTETS_PACKETS_GOOD_BAD	0x072C
#define	ETH_TX_256TO511OCTETS_PACKETS_GOOD_BAD	0x0730
#define	ETH_TX_512TO1023OCTETS_PACKETS_GOOD_BAD	0x0734
#define	ETH_TX_1024TOMAXOCTETS_PACKETS_GOOD_BAD	0x0738
#define	ETH_TX_UNICAST_PACKETS_GOOD_BAD		0x073C
#define	ETH_TX_MULTICAST_PACKETS_GOOD_BAD	0x0740
#define	ETH_TX_BROADCAST_PACKETS_GOOD_BAD	0x0744
#define	ETH_TX_UNDERFLOW_ERROR_PACKETS		0x0748
#define	ETH_TX_SINGLE_COLLISION_GOOD_PACKETS	0x074C
#define	ETH_TX_MULTIPLE_COLLISION_GOOD_PACKETS	0x0750
#define	ETH_TX_DEFERRED_PACKETS			0x0754
#define	ETH_TX_LATE_COLLISION_PACKETS		0x0758
#define	ETH_TX_EXCESSIVE_COLLISION_PACKETS	0x075C
#define	ETH_TX_CARRIER_ERROR_PACKETS		0x0760
#define	ETH_TX_OCTET_COUNT_GOOD			0x0764
#define	ETH_TX_PACKET_COUNT_GOOD		0x0768
#define	ETH_TX_EXCESSIVE_DEFERRAL_ERROR		0x076C
#define	ETH_TX_PAUSE_PACKETS			0x0770
#define	ETH_TX_VLAN_PACKETS_GOOD		0x0774
#define	ETH_TX_OSIZE_PACKETS_GOOD		0x0778
#define	ETH_RX_PACKETS_COUNT_GOOD_BAD		0x0780
#define	ETH_RX_OCTET_COUNT_GOOD_BAD		0x0784
#define	ETH_RX_OCTET_COUNT_GOOD			0x0788
#define	ETH_RX_BROADCAST_PACKETS_GOOD		0x078C
#define	ETH_RX_MULTICAST_PACKETS_GOOD		0x0790
#define	ETH_RX_CRC_ERROR_PACKETS		0x0794
#define	ETH_RX_ALIGNMENT_ERROR_PACKETS		0x0798
#define	ETH_RX_RUNT_ERROR_PACKETS		0x079C
#define	ETH_RX_JABBER_ERROR_PACKETS		0x07A0
#define	ETH_RX_UNDERSIZE_PACKETS_GOOD		0x07A4
#define	ETH_RX_OVERSIZE_PACKETS_GOOD		0x07A8
#define	ETH_RX_64OCTETS_PACKETS_GOOD_BAD	0x07AC
#define	ETH_RX_65TO127OCTETS_PACKETS_GOOD_BAD	0x07B0
#define	ETH_RX_128TO255OCTETS_PACKETS_GOOD_BAD	0x07B4
#define	ETH_RX_256TO511OCTETS_PACKETS_GOOD_BAD	0x07B8
#define	ETH_RX_512TO1023OCTETS_PACKETS_GOOD_BAD	0x07BC
#define	ETH_RX_1024TOMAXOCTETS_PACKETS_GOOD_BAD	0x07C0
#define	ETH_RX_UNICAST_PACKETS_GOOD		0x07C4
#define	ETH_RX_LENGTH_ERROR_PACKETS		0x07C8
#define	ETH_RX_OUT_OF_RANGE_PACKETS		0x07CC
#define	ETH_RX_PAUSE_PACKETS			0x07D0
#define	ETH_RX_FIFO_OVERFLOW_PACKETS		0x07D4
#define	ETH_RX_VLAN_PACKETS_GOOD_BAD		0x07D8
#define	ETH_RX_WATCHDOG_ERROR_PACKETS		0x07DC
#define	ETH_RX_RECEIVE_ERROR			0x07E0
#define	ETH_RX_CONTROL_PACKETS_GOOD		0x07E4
#define	ETH_TX_LPI_USEC_CNTR			0x07EC
#define	ETH_TX_LPI_TRAN_CNTR			0x07F0
#define	ETH_RX_LPI_USEC_CNTR			0x07F4
#define	ETH_RX_LPI_TRAN_CNTR			0x07F8
#define	ETH_MMC_FPE_TX_ISR			0x08A0
#define	ETH_MMC_FPE_TX_IMR			0x08A4
#define	ETH_MMC_FPE_TX_FCR			0x08A8
#define	ETH_MMC_TX_HRCR				0x08AC
#define	ETH_MMC_FPE_RX_ISR			0x08C0
#define	ETH_MMC_FPE_RX_IMR			0x08C4
#define	ETH_RX_PACKET_ASM_ERR			0x08C8
#define	ETH_RX_PACKET_SMD_ERR			0x08CC
#define	ETH_RX_PACKET_ASM_OKR			0x08D0
#define	ETH_RX_FPE_FRAG_CR			0x08D4
#define	ETH_MACL3L4C0R				0x0900
#define	ETH_MACL4A0R				0x0904
#define	ETH_MACL3A00R				0x0910
#define	ETH_MACL3A10R				0x0914
#define	ETH_MACL3A20R				0x0918
#define	ETH_MACL3A30R				0x091C
#define	ETH_MACL3L4C1R				0x0930
#define	ETH_MACL4A1R				0x0934
#define	ETH_MACL3A01R				0x0940
#define	ETH_MACL3A11R				0x0944
#define	ETH_MACL3A21R				0x0948
#define	ETH_MACL3A31R				0x094C
#define	ETH_MAC_IACR				0x0A70
#define	ETH_MAC_TMRQR				0x0A74
#define	ETH_MACTSCR				0x0B00
#define	ETH_MACSSIR				0x0B04
#define	ETH_MACSTSR				0x0B08
#define	ETH_MACSTNR				0x0B0C
#define	ETH_MACSTSUR				0x0B10
#define	ETH_MACSTNUR				0x0B14
#define	ETH_MACTSAR				0x0B18
#define	ETH_MACTSSR				0x0B20
#define	ETH_MACRXDTI				0x0B24
#define	ETH_MACTXDTI				0x0B28
#define	ETH_MACTXTSSNR				0x0B30
#define	ETH_MACTXTSSSR				0x0B34
#define	ETH_MACACR				0x0B40
#define	ETH_MACATSNR				0x0B48
#define	ETH_MACATSSR				0x0B4C
#define	ETH_MACTSIACR				0x0B50
#define	ETH_MACTSEACR				0x0B54
#define	ETH_MACTSICNR				0x0B58
#define	ETH_MACTSECNR				0x0B5C
#define	ETH_MACTSILR				0x0B68
#define	ETH_MACTSELR				0x0B6C
#define	ETH_MACPPSCR				0x0B70
#define	ETH_MACPPSTTS0R				0x0B80
#define	ETH_MACPPSTTN0R				0x0B84
#define	ETH_MACPPSI0R				0x0B88
#define	ETH_MACPPSW0R				0x0B8C
#define	ETH_MACPPSTTS1R				0x0B90
#define	ETH_MACPPSTTN1R				0x0B94
#define	ETH_MACPPSI1R				0x0B98
#define	ETH_MACPPSW1R				0x0B9C
#define	ETH_MACPOCR				0x0BC0
#define	ETH_MACSPI0R				0x0BC4
#define	ETH_MACSPI1R				0x0BC8
#define	ETH_MACSPI2R				0x0BCC
#define	ETH_MACLMIR				0x0BD0

int dwc4_miibus_read_reg(device_t dev, int phy, int reg);
int dwc4_miibus_write_reg(device_t dev, int phy, int reg, int val);
void dwc4_miibus_statchg(device_t dev);
void dwc4_core_setup(struct dwc_softc *sc);
void dwc4_enable_mac(struct dwc_softc *sc, bool enable);
void dwc4_enable_csum_offload(struct dwc_softc *sc);
void dwc4_setup_rxfilter(struct dwc_softc *sc);
void dwc4_get_hwaddr(struct dwc_softc *sc, uint8_t *hwaddr);
void dwc4_harvest_stats(struct dwc_softc *sc);
void dwc4_intr(struct dwc_softc *softc);
void dwc4_intr_disable(struct dwc_softc *sc);

#endif	/* !_DEV_DWC_DWC4_REG_H_ */
