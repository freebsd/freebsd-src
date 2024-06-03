/* SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause) */
/* Copyright 2017-2019 NXP */
#ifndef _ENETC_HW_H_
#define _ENETC_HW_H_

#include <sys/cdefs.h>

#include <sys/param.h>

#define BIT(x)	(1UL << (x))
#define GENMASK(h, l)	(((~0U) - (1U << (l)) + 1) & (~0U >> (32 - 1 - (h))))

#define PCI_VENDOR_FREESCALE	0x1957

/* ENETC device IDs */
#define ENETC_DEV_ID_PF		0xe100
#define ENETC_DEV_ID_VF		0xef00
#define ENETC_DEV_ID_PTP	0xee02

/* ENETC register block BAR */
#define ENETC_BAR_REGS	0

/** SI regs, offset: 0h */
#define ENETC_SIMR	0
#define ENETC_SIMR_EN	BIT(31)
#define ENETC_SIMR_DRXG	BIT(16)
#define ENETC_SIMR_RSSE	BIT(0)
#define ENETC_SICTR0	0x18
#define ENETC_SICTR1	0x1c
#define ENETC_SIPCAPR0	0x20
#define ENETC_SIPCAPR0_QBV	BIT(4)
#define ENETC_SIPCAPR0_PSFP	BIT(9)
#define ENETC_SIPCAPR0_RSS	BIT(8)
#define ENETC_SIPCAPR1	0x24
#define ENETC_SITGTGR	0x30
#define ENETC_SIRBGCR	0x38
/* cache attribute registers for transactions initiated by ENETC */
#define ENETC_SICAR0	0x40
#define ENETC_SICAR1	0x44
#define ENETC_SICAR2	0x48
/* rd snoop, no alloc
 * wr snoop, no alloc, partial cache line update for BDs and full cache line
 * update for data
 */
#define ENETC_SICAR_RD_COHERENT	0x2b2b0000
#define ENETC_SICAR_WR_COHERENT	0x00006727
#define ENETC_SICAR_MSI	0x00300030 /* rd/wr device, no snoop, no alloc */

#define ENETC_SIPMAR0	0x80
#define ENETC_SIPMAR1	0x84

/* VF-PF Message passing */
#define ENETC_DEFAULT_MSG_SIZE	1024	/* and max size */

#define ENETC_PSIMSGRR	0x204
#define ENETC_PSIMSGRR_MR_MASK	GENMASK(2, 1)
#define ENETC_PSIMSGRR_MR(n) BIT((n) + 1) /* n = VSI index */
#define ENETC_PSIVMSGRCVAR0(n)	(0x210 + (n) * 0x8) /* n = VSI index */
#define ENETC_PSIVMSGRCVAR1(n)	(0x214 + (n) * 0x8)

#define ENETC_VSIMSGSR	0x204	/* RO */
#define ENETC_VSIMSGSR_MB	BIT(0)
#define ENETC_VSIMSGSR_MS	BIT(1)
#define ENETC_VSIMSGSNDAR0	0x210
#define ENETC_VSIMSGSNDAR1	0x214

#define ENETC_SIMSGSR_SET_MC(val) ((val) << 16)
#define ENETC_SIMSGSR_GET_MC(val) ((val) >> 16)

/* SI statistics */
#define ENETC_SIROCT	0x300
#define ENETC_SIRFRM	0x308
#define ENETC_SIRUCA	0x310
#define ENETC_SIRMCA	0x318
#define ENETC_SITOCT	0x320
#define ENETC_SITFRM	0x328
#define ENETC_SITUCA	0x330
#define ENETC_SITMCA	0x338
#define ENETC_RBDCR(n)	(0x8180 + (n) * 0x200)

/* Control BDR regs */
#define ENETC_SICBDRMR		0x800
#define ENETC_SICBDRMR_EN	BIT(31)
#define ENETC_SICBDRSR		0x804	/* RO */
#define ENETC_SICBDRBAR0	0x810
#define ENETC_SICBDRBAR1	0x814
#define ENETC_SICBDRPIR		0x818
#define ENETC_SICBDRCIR		0x81c
#define ENETC_SICBDRLENR	0x820

#define ENETC_SICAPR0	0x900
#define ENETC_SICAPR1	0x904

#define ENETC_PSIIER	0xa00
#define ENETC_PSIIER_MR_MASK	GENMASK(2, 1)
#define ENETC_PSIIDR	0xa08
#define ENETC_SITXIDR	0xa18
#define ENETC_SIRXIDR	0xa28
#define ENETC_SIMSIVR	0xa30

#define ENETC_SIMSITRV(n) (0xB00 + (n) * 0x4)
#define ENETC_SIMSIRRV(n) (0xB80 + (n) * 0x4)

#define ENETC_SIUEFDCR	0xe28

#define ENETC_SIRFSCAPR	0x1200
#define ENETC_SIRFSCAPR_GET_NUM_RFS(val) ((val) & 0x7f)
#define ENETC_SIRSSCAPR	0x1600
#define ENETC_SIRSSCAPR_GET_NUM_RSS(val) (BIT((val) & 0xf) * 32)

/** SI BDR sub-blocks, n = 0..7 */
enum enetc_bdr_type {TX, RX};
#define ENETC_BDR_OFF(i)	((i) * 0x200)
#define ENETC_BDR(t, i, r)	(0x8000 + (t) * 0x100 + ENETC_BDR_OFF(i) + (r))
/* RX BDR reg offsets */
#define ENETC_RBMR	0
#define ENETC_RBMR_AL	BIT(0)
#define ENETC_RBMR_BDS	BIT(2)
#define ENETC_RBMR_VTE	BIT(5)
#define ENETC_RBMR_EN	BIT(31)
#define ENETC_RBSR	0x4
#define ENETC_RBBSR	0x8
#define ENETC_RBCIR	0xc
#define ENETC_RBBAR0	0x10
#define ENETC_RBBAR1	0x14
#define ENETC_RBPIR	0x18
#define ENETC_RBLENR	0x20
#define ENETC_RBIER	0xa0
#define ENETC_RBIER_RXTIE	BIT(0)
#define ENETC_RBIDR	0xa4
#define ENETC_RBICR0	0xa8
#define ENETC_RBICR0_ICEN		BIT(31)
#define ENETC_RBICR0_ICPT_MASK		0x1ff
#define ENETC_RBICR0_SET_ICPT(n)	((n) & ENETC_RBICR0_ICPT_MASK)
#define ENETC_RBICR1	0xac

/* TX BDR reg offsets */
#define ENETC_TBMR	0
#define ENETC_TBSR_BUSY	BIT(0)
#define ENETC_TBMR_VIH	BIT(9)
#define ENETC_TBMR_PRIO_MASK		GENMASK(2, 0)
#define ENETC_TBMR_SET_PRIO(val)	((val) & ENETC_TBMR_PRIO_MASK)
#define ENETC_TBMR_EN	BIT(31)
#define ENETC_TBSR	0x4
#define ENETC_TBBAR0	0x10
#define ENETC_TBBAR1	0x14
#define ENETC_TBPIR	0x18
#define ENETC_TBCIR	0x1c
#define ENETC_TBCIR_IDX_MASK	0xffff
#define ENETC_TBLENR	0x20
#define ENETC_TBIER	0xa0
#define ENETC_TBIER_TXT	BIT(0)
#define ENETC_TBIER_TXF	BIT(1)
#define ENETC_TBIDR	0xa4
#define ENETC_TBICR0	0xa8
#define ENETC_TBICR0_ICEN		BIT(31)
#define ENETC_TBICR0_ICPT_MASK		0xf
#define ENETC_TBICR0_SET_ICPT(n) ((ilog2(n) + 1) & ENETC_TBICR0_ICPT_MASK)
#define ENETC_TBICR1	0xac

#define ENETC_RTBLENR_LEN(n)	((n) & ~0x7)

/* Port regs, offset: 1_0000h */
#define ENETC_PORT_BASE		0x10000
#define ENETC_PMR		0x0000
#define ENETC_PMR_SI0EN		BIT(16)
#define ENETC_PMR_EN	GENMASK(18, 16)
#define ENETC_PMR_PSPEED_MASK GENMASK(11, 8)
#define ENETC_PMR_PSPEED_10M	0
#define ENETC_PMR_PSPEED_100M	BIT(8)
#define ENETC_PMR_PSPEED_1000M	BIT(9)
#define ENETC_PMR_PSPEED_2500M	BIT(10)
#define ENETC_PSR		0x0004 /* RO */
#define ENETC_PSIPMR		0x0018
#define ENETC_PSIPMR_SET_UP(n)	BIT(n) /* n = SI index */
#define ENETC_PSIPMR_SET_MP(n)	BIT((n) + 16)
#define ENETC_PSIPVMR		0x001c
#define ENETC_VLAN_PROMISC_MAP_ALL	0x7
#define ENETC_PSIPVMR_SET_VP(simap)	((simap) & 0x7)
#define ENETC_PSIPVMR_SET_VUTA(simap)	(((simap) & 0x7) << 16)
#define ENETC_PSIPMAR0(n)	(0x0100 + (n) * 0x8) /* n = SI index */
#define ENETC_PSIPMAR1(n)	(0x0104 + (n) * 0x8)
#define ENETC_PVCLCTR		0x0208
#define ENETC_PCVLANR1		0x0210
#define ENETC_PCVLANR2		0x0214
#define ENETC_VLAN_TYPE_C	BIT(0)
#define ENETC_VLAN_TYPE_S	BIT(1)
#define ENETC_PVCLCTR_OVTPIDL(bmp)	((bmp) & 0xff) /* VLAN_TYPE */
#define ENETC_PSIVLANR(n)	(0x0240 + (n) * 4) /* n = SI index */
#define ENETC_PSIVLAN_EN	BIT(31)
#define ENETC_PSIVLAN_SET_QOS(val)	((uint32_t)(val) << 12)
#define ENETC_PTXMBAR		0x0608
#define ENETC_PCAPR0		0x0900
#define ENETC_PCAPR0_RXBDR(val)	((val) >> 24)
#define ENETC_PCAPR0_TXBDR(val)	(((val) >> 16) & 0xff)
#define ENETC_PCAPR1		0x0904
#define ENETC_PSICFGR0(n)	(0x0940 + (n) * 0xc)  /* n = SI index */
#define ENETC_PSICFGR0_SET_TXBDR(val)	((val) & 0xff)
#define ENETC_PSICFGR0_SET_RXBDR(val)	(((val) & 0xff) << 16)
#define ENETC_PSICFGR0_VTE	BIT(12)
#define ENETC_PSICFGR0_SIVIE	BIT(14)
#define ENETC_PSICFGR0_ASE	BIT(15)
#define ENETC_PSICFGR0_SIVC(bmp)	(((bmp) & 0xff) << 24) /* VLAN_TYPE */

#define ENETC_PTCCBSR0(n)	(0x1110 + (n) * 8) /* n = 0 to 7*/
#define ENETC_CBSE		BIT(31)
#define ENETC_CBS_BW_MASK	GENMASK(6, 0)
#define ENETC_PTCCBSR1(n)	(0x1114 + (n) * 8) /* n = 0 to 7*/
#define ENETC_RSSHASH_KEY_SIZE	40
#define ENETC_PRSSCAPR		0x1404
#define ENETC_PRSSCAPR_GET_NUM_RSS(val)	(BIT((val) & 0xf) * 32)
#define ENETC_PRSSK(n)		(0x1410 + (n) * 4) /* n = [0..9] */
#define ENETC_PSIVLANFMR	0x1700
#define ENETC_PSIVLANFMR_VS	BIT(0)
#define ENETC_PRFSMR		0x1800
#define ENETC_PRFSMR_RFSE	BIT(31)
#define ENETC_PRFSCAPR		0x1804
#define ENETC_PRFSCAPR_GET_NUM_RFS(val)	((((val) & 0xf) + 1) * 16)
#define ENETC_PSIRFSCFGR(n)	(0x1814 + (n) * 4) /* n = SI index */
#define ENETC_PFPMR		0x1900
#define ENETC_PFPMR_PMACE	BIT(1)
#define ENETC_PFPMR_MWLM	BIT(0)
#define ENETC_EMDIO_BASE	0x1c00
#define ENETC_PSIUMHFR0(n, err)	(((err) ? 0x1d08 : 0x1d00) + (n) * 0x10)
#define ENETC_PSIUMHFR1(n)	(0x1d04 + (n) * 0x10)
#define ENETC_PSIMMHFR0(n, err)	(((err) ? 0x1d00 : 0x1d08) + (n) * 0x10)
#define ENETC_PSIMMHFR1(n)	(0x1d0c + (n) * 0x10)
#define ENETC_PSIVHFR0(n)	(0x1e00 + (n) * 8) /* n = SI index */
#define ENETC_PSIVHFR1(n)	(0x1e04 + (n) * 8) /* n = SI index */
#define ENETC_MMCSR		0x1f00
#define ENETC_MMCSR_ME		BIT(16)
#define ENETC_PTCMSDUR(n)	(0x2020 + (n) * 4) /* n = TC index [0..7] */

#define ENETC_PAR_PORT_CFG	0x3050
#define ENETC_PAR_PORT_L4CD	BIT(0)
#define ENETC_PAR_PORT_L3CD	BIT(1)

#define ENETC_PM0_CMD_CFG	0x8008
#define ENETC_PM1_CMD_CFG	0x9008
#define ENETC_PM0_TX_EN		BIT(0)
#define ENETC_PM0_RX_EN		BIT(1)
#define ENETC_PM0_PROMISC	BIT(4)
#define ENETC_PM0_CMD_XGLP	BIT(10)
#define ENETC_PM0_CMD_TXP	BIT(11)
#define ENETC_PM0_CMD_PHY_TX_EN	BIT(15)
#define ENETC_PM0_CMD_SFD	BIT(21)
#define ENETC_PM0_MAXFRM	0x8014
#define ENETC_SET_TX_MTU(val)	((val) << 16)
#define ENETC_SET_MAXFRM(val)	((val) & 0xffff)
#define ENETC_PM0_RX_FIFO	0x801c
#define ENETC_PM0_RX_FIFO_VAL	1

#define ENETC_PM_IMDIO_BASE	0x8030

#define ENETC_PM0_IF_MODE	0x8300
#define ENETC_PM0_IFM_RG	BIT(2)
#define ENETC_PM0_IFM_RLP	(BIT(5) | BIT(11))
#define ENETC_PM0_IFM_EN_AUTO	BIT(15)
#define ENETC_PM0_IFM_SSP_MASK	GENMASK(14, 13)
#define ENETC_PM0_IFM_SSP_1000	(2 << 13)
#define ENETC_PM0_IFM_SSP_100	(0 << 13)
#define ENETC_PM0_IFM_SSP_10	(1 << 13)
#define ENETC_PM0_IFM_FULL_DPX	BIT(12)
#define ENETC_PM0_IFM_IFMODE_MASK GENMASK(1, 0)
#define ENETC_PM0_IFM_IFMODE_XGMII 0
#define ENETC_PM0_IFM_IFMODE_GMII 2
#define ENETC_PSIDCAPR		0x1b08
#define ENETC_PSIDCAPR_MSK	GENMASK(15, 0)
#define ENETC_PSFCAPR		0x1b18
#define ENETC_PSFCAPR_MSK	GENMASK(15, 0)
#define ENETC_PSGCAPR		0x1b28
#define ENETC_PSGCAPR_GCL_MSK	GENMASK(18, 16)
#define ENETC_PSGCAPR_SGIT_MSK	GENMASK(15, 0)
#define ENETC_PFMCAPR		0x1b38
#define ENETC_PFMCAPR_MSK	GENMASK(15, 0)

/* MAC counters */
#define ENETC_PM0_REOCT		0x8100
#define ENETC_PM0_RALN		0x8110
#define ENETC_PM0_RXPF		0x8118
#define ENETC_PM0_RFRM		0x8120
#define ENETC_PM0_RFCS		0x8128
#define ENETC_PM0_RVLAN		0x8130
#define ENETC_PM0_RERR		0x8138
#define ENETC_PM0_RUCA		0x8140
#define ENETC_PM0_RMCA		0x8148
#define ENETC_PM0_RBCA		0x8150
#define ENETC_PM0_RDRP		0x8158
#define ENETC_PM0_RPKT		0x8160
#define ENETC_PM0_RUND		0x8168
#define ENETC_PM0_R64		0x8170
#define ENETC_PM0_R127		0x8178
#define ENETC_PM0_R255		0x8180
#define ENETC_PM0_R511		0x8188
#define ENETC_PM0_R1023		0x8190
#define ENETC_PM0_R1522		0x8198
#define ENETC_PM0_R1523X	0x81A0
#define ENETC_PM0_ROVR		0x81A8
#define ENETC_PM0_RJBR		0x81B0
#define ENETC_PM0_RFRG		0x81B8
#define ENETC_PM0_RCNP		0x81C0
#define ENETC_PM0_RDRNTP	0x81C8
#define ENETC_PM0_TEOCT		0x8200
#define ENETC_PM0_TOCT		0x8208
#define ENETC_PM0_TCRSE		0x8210
#define ENETC_PM0_TXPF		0x8218
#define ENETC_PM0_TFRM		0x8220
#define ENETC_PM0_TFCS		0x8228
#define ENETC_PM0_TVLAN		0x8230
#define ENETC_PM0_TERR		0x8238
#define ENETC_PM0_TUCA		0x8240
#define ENETC_PM0_TMCA		0x8248
#define ENETC_PM0_TBCA		0x8250
#define ENETC_PM0_TPKT		0x8260
#define ENETC_PM0_TUND		0x8268
#define ENETC_PM0_T64		0x8270
#define ENETC_PM0_T127		0x8278
#define ENETC_PM0_T255		0x8280
#define ENETC_PM0_T511		0x8288
#define ENETC_PM0_T1023		0x8290
#define ENETC_PM0_T1522		0x8298
#define ENETC_PM0_T1523X	0x82A0
#define ENETC_PM0_TCNP		0x82C0
#define ENETC_PM0_TDFR		0x82D0
#define ENETC_PM0_TMCOL		0x82D8
#define ENETC_PM0_TSCOL		0x82E0
#define ENETC_PM0_TLCOL		0x82E8
#define ENETC_PM0_TECOL		0x82F0

/* Port counters */
#define ENETC_PICDR(n)		(0x0700 + (n) * 8) /* n = [0..3] */
#define ENETC_PBFDSIR		0x0810
#define ENETC_PFDMSAPR		0x0814
#define ENETC_UFDMF		0x1680
#define ENETC_MFDMF		0x1684
#define ENETC_PUFDVFR		0x1780
#define ENETC_PMFDVFR		0x1784
#define ENETC_PBFDVFR		0x1788

/** Global regs, offset: 2_0000h */
#define ENETC_GLOBAL_BASE	0x20000
#define ENETC_G_EIPBRR0		0x0bf8
#define ENETC_G_EIPBRR1		0x0bfc
#define ENETC_G_EPFBLPR(n)	(0xd00 + 4 * (n))
#define ENETC_G_EPFBLPR1_XGMII	0x80000000

/* Buffer Descriptors (BD) */
union enetc_tx_bd {
	struct {
		uint64_t addr;
		uint16_t buf_len;
		uint16_t frm_len;
		union {
			struct {
				uint8_t reserved[3];
				uint8_t flags;
			}; /* default layout */
			uint32_t txstart;
			uint32_t lstatus;
		};
	};
	struct {
		uint32_t tstamp;
		uint16_t tpid;
		uint16_t vid;
		uint8_t reserved[6];
		uint8_t e_flags;
		uint8_t flags;
	} ext; /* Tx BD extension */
	struct {
		uint32_t tstamp;
		uint8_t reserved[10];
		uint8_t status;
		uint8_t flags;
	} wb; /* writeback descriptor */
};

enum enetc_txbd_flags {
	ENETC_TXBD_FLAGS_RES0 = BIT(0), /* reserved */
	ENETC_TXBD_FLAGS_TSE = BIT(1),
	ENETC_TXBD_FLAGS_W = BIT(2),
	ENETC_TXBD_FLAGS_RES3 = BIT(3), /* reserved */
	ENETC_TXBD_FLAGS_TXSTART = BIT(4),
	ENETC_TXBD_FLAGS_FI = BIT(5),
	ENETC_TXBD_FLAGS_EX = BIT(6),
	ENETC_TXBD_FLAGS_F = BIT(7)
};
#define ENETC_TXBD_TXSTART_MASK GENMASK(24, 0)
#define ENETC_TXBD_FLAGS_OFFSET 24

static inline void enetc_clear_tx_bd(union enetc_tx_bd *txbd)
{
	memset(txbd, 0, sizeof(*txbd));
}

/* Extension flags */
#define ENETC_TXBD_E_FLAGS_VLAN_INS	BIT(0)
#define ENETC_TXBD_E_FLAGS_TWO_STEP_PTP	BIT(2)

union enetc_rx_bd {
	struct {
		uint64_t addr;
		uint8_t reserved[8];
	} w;
	struct {
		uint16_t inet_csum;
		uint16_t parse_summary;
		uint32_t rss_hash;
		uint16_t buf_len;
		uint16_t vlan_opt;
		union {
			struct {
				uint16_t flags;
				uint16_t error;
			};
			uint32_t lstatus;
		};
	} r;
	struct {
		uint32_t tstamp;
		uint8_t reserved[12];
	} ext;
};

#define ENETC_RXBD_PARSER_ERROR	BIT(15)

#define ENETC_RXBD_LSTATUS_R	BIT(30)
#define ENETC_RXBD_LSTATUS_F	BIT(31)
#define ENETC_RXBD_ERR_MASK	0xff
#define ENETC_RXBD_LSTATUS(flags)	((flags) << 16)
#define ENETC_RXBD_FLAG_RSSV	BIT(8)
#define ENETC_RXBD_FLAG_VLAN	BIT(9)
#define ENETC_RXBD_FLAG_TSTMP	BIT(10)
#define ENETC_RXBD_FLAG_TPID	GENMASK(1, 0)

#define ENETC_MAC_ADDR_FILT_CNT	8 /* # of supported entries per port */
#define EMETC_MAC_ADDR_FILT_RES	3 /* # of reserved entries at the beginning */
#define ENETC_MAX_NUM_VFS	2

#define ENETC_CBD_FLAGS_SF	BIT(7) /* short format */
#define ENETC_CBD_STATUS_MASK	0xf

struct enetc_cmd_rfse {
	uint8_t smac_h[6];
	uint8_t smac_m[6];
	uint8_t dmac_h[6];
	uint8_t dmac_m[6];
	uint32_t sip_h[4];	/* Big-endian */
	uint32_t sip_m[4];	/* Big-endian */
	uint32_t dip_h[4];	/* Big-endian */
	uint32_t dip_m[4];	/* Big-endian */
	uint16_t ethtype_h;
	uint16_t ethtype_m;
	uint16_t ethtype4_h;
	uint16_t ethtype4_m;
	uint16_t sport_h;
	uint16_t sport_m;
	uint16_t dport_h;
	uint16_t dport_m;
	uint16_t vlan_h;
	uint16_t vlan_m;
	uint8_t proto_h;
	uint8_t proto_m;
	uint16_t flags;
	uint16_t result;
	uint16_t mode;
};

#define ENETC_RFSE_EN	BIT(15)
#define ENETC_RFSE_MODE_BD	2

#define ENETC_SI_INT_IDX	0
/* base index for Rx/Tx interrupts */
#define ENETC_BDR_INT_BASE_IDX	1

/* Messaging */

/* Command completion status */
enum enetc_msg_cmd_status {
	ENETC_MSG_CMD_STATUS_OK,
	ENETC_MSG_CMD_STATUS_FAIL
};

/* VSI-PSI command message types */
enum enetc_msg_cmd_type {
	ENETC_MSG_CMD_MNG_MAC = 1, /* manage MAC address */
	ENETC_MSG_CMD_MNG_RX_MAC_FILTER,/* manage RX MAC table */
	ENETC_MSG_CMD_MNG_RX_VLAN_FILTER /* manage RX VLAN table */
};

/* VSI-PSI command action types */
enum enetc_msg_cmd_action_type {
	ENETC_MSG_CMD_MNG_ADD = 1,
	ENETC_MSG_CMD_MNG_REMOVE
};

/* PSI-VSI command header format */
struct enetc_msg_cmd_header {
	uint16_t type;	/* command class type */
	uint16_t id;		/* denotes the specific required action */
};

enum bdcr_cmd_class {
	BDCR_CMD_UNSPEC = 0,
	BDCR_CMD_MAC_FILTER,
	BDCR_CMD_VLAN_FILTER,
	BDCR_CMD_RSS,
	BDCR_CMD_RFS,
	BDCR_CMD_PORT_GCL,
	BDCR_CMD_RECV_CLASSIFIER,
	BDCR_CMD_STREAM_IDENTIFY,
	BDCR_CMD_STREAM_FILTER,
	BDCR_CMD_STREAM_GCL,
	BDCR_CMD_FLOW_METER,
	__BDCR_CMD_MAX_LEN,
	BDCR_CMD_MAX_LEN = __BDCR_CMD_MAX_LEN - 1,
};

enum bdcr_cmd_rss {
	BDCR_CMD_RSS_WRITE = 1,
	BDCR_CMD_RSS_READ = 2,
};

/* class 5, command 0 */
struct tgs_gcl_conf {
	uint8_t	atc;	/* init gate value */
	uint8_t	res[7];
	struct {
		uint8_t	res1[4];
		uint16_t	acl_len;
		uint8_t	res2[2];
	};
};

/* gate control list entry */
struct gce {
	uint32_t	period;
	uint8_t	gate;
	uint8_t	res[3];
};

/* tgs_gcl_conf address point to this data space */
struct tgs_gcl_data {
	uint32_t		btl;
	uint32_t		bth;
	uint32_t		ct;
	uint32_t		cte;
	struct gce	entry[];
};

/* class 7, command 0, Stream Identity Entry Configuration */
struct streamid_conf {
	uint32_t	stream_handle;	/* init gate value */
	uint32_t	iports;
		uint8_t	id_type;
		uint8_t	oui[3];
		uint8_t	res[3];
		uint8_t	en;
};

#define ENETC_CBDR_SID_VID_MASK 0xfff
#define ENETC_CBDR_SID_VIDM BIT(12)
#define ENETC_CBDR_SID_TG_MASK 0xc000
/* streamid_conf address point to this data space */
struct streamid_data {
	union {
		uint8_t dmac[6];
		uint8_t smac[6];
	};
	uint16_t     vid_vidm_tg;
};

#define ENETC_CBDR_SFI_PRI_MASK 0x7
#define ENETC_CBDR_SFI_PRIM		BIT(3)
#define ENETC_CBDR_SFI_BLOV		BIT(4)
#define ENETC_CBDR_SFI_BLEN		BIT(5)
#define ENETC_CBDR_SFI_MSDUEN	BIT(6)
#define ENETC_CBDR_SFI_FMITEN	BIT(7)
#define ENETC_CBDR_SFI_ENABLE	BIT(7)
/* class 8, command 0, Stream Filter Instance, Short Format */
struct sfi_conf {
	uint32_t	stream_handle;
		uint8_t	multi;
		uint8_t	res[2];
		uint8_t	sthm;
	/* Max Service Data Unit or Flow Meter Instance Table index.
	 * Depending on the value of FLT this represents either Max
	 * Service Data Unit (max frame size) allowed by the filter
	 * entry or is an index into the Flow Meter Instance table
	 * index identifying the policer which will be used to police
	 * it.
	 */
	uint16_t	fm_inst_table_index;
	uint16_t	msdu;
	uint16_t	sg_inst_table_index;
		uint8_t	res1[2];
	uint32_t	input_ports;
		uint8_t	res2[3];
		uint8_t	en;
};

/* class 8, command 2 stream Filter Instance status query short format
 * command no need structure define
 * Stream Filter Instance Query Statistics Response data
 */
struct sfi_counter_data {
	uint32_t matchl;
	uint32_t matchh;
	uint32_t msdu_dropl;
	uint32_t msdu_droph;
	uint32_t stream_gate_dropl;
	uint32_t stream_gate_droph;
	uint32_t flow_meter_dropl;
	uint32_t flow_meter_droph;
};

#define ENETC_CBDR_SGI_OIPV_MASK 0x7
#define ENETC_CBDR_SGI_OIPV_EN	BIT(3)
#define ENETC_CBDR_SGI_CGTST	BIT(6)
#define ENETC_CBDR_SGI_OGTST	BIT(7)
#define ENETC_CBDR_SGI_CFG_CHG  BIT(1)
#define ENETC_CBDR_SGI_CFG_PND  BIT(2)
#define ENETC_CBDR_SGI_OEX		BIT(4)
#define ENETC_CBDR_SGI_OEXEN	BIT(5)
#define ENETC_CBDR_SGI_IRX		BIT(6)
#define ENETC_CBDR_SGI_IRXEN	BIT(7)
#define ENETC_CBDR_SGI_ACLLEN_MASK 0x3
#define ENETC_CBDR_SGI_OCLLEN_MASK 0xc
#define	ENETC_CBDR_SGI_EN		BIT(7)
/* class 9, command 0, Stream Gate Instance Table, Short Format
 * class 9, command 2, Stream Gate Instance Table entry query write back
 * Short Format
 */
struct sgi_table {
	uint8_t	res[8];
	uint8_t	oipv;
	uint8_t	res0[2];
	uint8_t	ocgtst;
	uint8_t	res1[7];
	uint8_t	gset;
	uint8_t	oacl_len;
	uint8_t	res2[2];
	uint8_t	en;
};

#define ENETC_CBDR_SGI_AIPV_MASK 0x7
#define ENETC_CBDR_SGI_AIPV_EN	BIT(3)
#define ENETC_CBDR_SGI_AGTST	BIT(7)

/* class 9, command 1, Stream Gate Control List, Long Format */
struct sgcl_conf {
	uint8_t	aipv;
	uint8_t	res[2];
	uint8_t	agtst;
	uint8_t	res1[4];
	union {
		struct {
			uint8_t res2[4];
			uint8_t acl_len;
			uint8_t res3[3];
		};
		uint8_t cct[8]; /* Config change time */
	};
};

#define ENETC_CBDR_SGL_IOMEN	BIT(0)
#define ENETC_CBDR_SGL_IPVEN	BIT(3)
#define ENETC_CBDR_SGL_GTST		BIT(4)
#define ENETC_CBDR_SGL_IPV_MASK 0xe
/* Stream Gate Control List Entry */
struct sgce {
	uint32_t	interval;
	uint8_t	msdu[3];
	uint8_t	multi;
};

/* stream control list class 9 , cmd 1 data buffer */
struct sgcl_data {
	uint32_t		btl;
	uint32_t		bth;
	uint32_t		ct;
	uint32_t		cte;
	struct sgce	sgcl[0];
};

#define ENETC_CBDR_FMI_MR	BIT(0)
#define ENETC_CBDR_FMI_MREN	BIT(1)
#define ENETC_CBDR_FMI_DOY	BIT(2)
#define	ENETC_CBDR_FMI_CM	BIT(3)
#define ENETC_CBDR_FMI_CF	BIT(4)
#define ENETC_CBDR_FMI_NDOR	BIT(5)
#define ENETC_CBDR_FMI_OALEN	BIT(6)
#define ENETC_CBDR_FMI_IRFPP_MASK GENMASK(4, 0)

/* class 10: command 0/1, Flow Meter Instance Set, short Format */
struct fmi_conf {
	uint32_t	cir;
	uint32_t	cbs;
	uint32_t	eir;
	uint32_t	ebs;
		uint8_t	conf;
		uint8_t	res1;
		uint8_t	ir_fpp;
		uint8_t	res2[4];
		uint8_t	en;
};

struct enetc_cbd {
	union{
		struct sfi_conf sfi_conf;
		struct sgi_table sgi_table;
		struct fmi_conf fmi_conf;
		struct {
			uint32_t	addr[2];
			union {
				uint32_t	opt[4];
				struct tgs_gcl_conf	gcl_conf;
				struct streamid_conf	sid_set;
				struct sgcl_conf	sgcl_conf;
			};
		};	/* Long format */
		uint32_t data[6];
	};
	uint16_t index;
	uint16_t length;
	uint8_t cmd;
	uint8_t cls;
	uint8_t _res;
	uint8_t status_flags;
};

#define ENETC_CLK  400000000ULL

/* port time gating control register */
#define ENETC_QBV_PTGCR_OFFSET		0x11a00
#define ENETC_QBV_TGE			BIT(31)
#define ENETC_QBV_TGPE			BIT(30)

/* Port time gating capability register */
#define ENETC_QBV_PTGCAPR_OFFSET	0x11a08
#define ENETC_QBV_MAX_GCL_LEN_MASK	GENMASK(15, 0)

/* Port time specific departure */
#define ENETC_PTCTSDR(n)	(0x1210 + 4 * (n))
#define ENETC_TSDE		BIT(31)

/* PSFP setting */
#define ENETC_PPSFPMR 0x11b00
#define ENETC_PPSFPMR_PSFPEN BIT(0)
#define ENETC_PPSFPMR_VS BIT(1)
#define ENETC_PPSFPMR_PVC BIT(2)
#define ENETC_PPSFPMR_PVZC BIT(3)

#endif
