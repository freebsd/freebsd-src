/*
 *   $Id: pci-sd0001.h,v 1.1.2.1 2003/06/24 08:40:50 dwmw2 Exp $
 *
 *   linux/arch/sh/kernel/pci_sd0001.h
 *
 *   Support Hitachi Semcon SD0001 SH3 PCI Host Bridge .
 *  
 *
 *   Copyright (C) 2000  Masayuki Okada (macha@adc.hitachi-ul.co.jp)
 *                       Hitachi ULSI Systems Co., Ltd.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *
 *   Revision History
 *    ----------------
 *
 */

/*
 * SD0001 PCIブリッジ　レジスタ、ビット定義
 */


#ifndef __PCI_SD0001_H
#define __PCI_SD0001_H

#define SD0001_IO_BASE (P2SEGADDR(CONFIG_PCI_SD0001_BASE)+0x00800000)
#define SD0001_MEM_BASE (P2SEGADDR(CONFIG_PCI_SD0001_BASE)+0x01000000)

#define SD0001_REG(x) ((volatile u32 *)P2SEGADDR(CONFIG_PCI_SD0001_BASE + (x)))

#define sd0001_writel(value, reg) do { *(SD0001_REG(SD0001_REG_##reg)) = value; } while(0)
#define sd0001_readl(reg) (*(SD0001_REG(SD0001_REG_##reg)))

#define SD0001_REG_REV			(0x00)	/* PCI Class & Revision Code */
#define SD0001_REG_RESET		(0x08)	/* リセット */
#define SD0001_REG_SDRAM_CTL		(0x10)	/* SDRAM モード/制御 */
#define SD0001_REG_INT_STS1		(0x20)	/* 割り込要因表示 */
#define SD0001_REG_INT_ENABLE		(0x24)	/* 割り込みマスク */
#define SD0001_REG_INT_STS2		(0x28)	/* 割込みステータス */
#define SD0001_REG_DMA1_CTL_STS		(0x30)	/* DMA コマンド & ステータス */
#define SD0001_REG_DMA1_SADR		(0x34)	/* DMA ソースアドレス */
#define SD0001_REG_DMA1_DADR		(0x38)	/* DMA ディスティネーションアドレス */
#define SD0001_REG_DMA1_CNT		(0x3c)	/* DMA 転送バイト数 */
#define SD0001_REG_DMA2_CTL_STS		(0x40)	/* DMA コマンド & ステータス */
#define SD0001_REG_DMA2_SADR		(0x44)	/* DMA ソースアドレス */
#define SD0001_REG_DMA2_DADR		(0x48)	/* DMA ディスティネーションアドレス */
#define SD0001_REG_DMA2_CNT		(0x4c)	/* DMA 転送バイト数 */
#define SD0001_REG_PCI_CTL		(0x50)	/* PCIバス動作モード */
#define SD0001_REG_PCI_IO_OFFSET	(0x58)	/* PCI直接I/Oアクセスオフセット */
#define SD0001_REG_PCI_MEM_OFFSET	(0x5c)	/* PCI直接メモリアクセスオフセット */
#define SD0001_REG_INDIRECT_ADR		(0x60)	/* PCI Configurationレジスタアドレス */
#define SD0001_REG_INDIRECT_DATA	(0x64)	/* PCI Configurationデータレジスタ */
#define SD0001_REG_INDIRECT_CTL		(0x68)	/* PCIバス間接アクセス制御 */
#define SD0001_REG_INDIRECT_STS		(0x6c)	/* PCIバス間接アクセスステータス */
#define SD0001_REG_AWAKE		(0x70)	/* AWAKE割込み */
#define SD0001_REG_MAIL			(0x74)	/* Mail通信 */


/*
 * SD0001 レジスタの各ビットの機能の定義
 */
/* MODE レジスタ */

/* RST レジスタ */
#define	 SD0001_RST_SWRST		0x80000000	/* SD0001のリセット */
#define	 SD0001_RST_BUSRST		0x40000000	/* PCIバスソフトリセット */
#define	 SD0001_RST_MASK		0xc0000000	/* RSTレジスタ設定マスク */


/* SH3 SD0001  PCI Devices Space & Mamory Size */
#define	 SD0001_PCI_IO_WINDOW		0x00800000	/* PCI I/O空間 Window: 8MiB */
#define	 SD0001_PCI_MEM_WINDOW		0x03000000	/* PCI MEM空間 Window: 48MiB */
#define  SD0001_PCI_WINDOW_SHIFT	22		
#define	 SD0001_SDRAM_MAX		0x04000000	/* SDMANサイズ 64MiB */


/* pci_ctl レジスタ */
#define  SD0001_CTL_MASTER_SWAP		0x80000000	/* PCIバスマスター転送時の
							   Byte Swap ON */
#define  SD0001_CTL_PCI_EDCONV		0x40000000	/* PCIデバイスアクセス時の
							   Endian変換 ON */
#define	 SD0001_CTL_RETRY_MASK		0x000000f0	/* PCIリトライオーバ検出カウント値 */
#define  SD0001_CTL_NOGNT		0x00000002	/* PCI GNT信号の発行停止 */

/* SD_MDCTL レジスタ */
#define	 SD0001_SDMD_KIND_MASK		0xc0000000	/* SDRAM 種別 設定マスク*/
#define	 SD0001_SDMD_KIND_16		0x00000000	/* SDRAM 16Mbits */
#define	 SD0001_SDMD_KIND_64		0x40000000	/* SDRAM 64Mbits */
#define	 SD0001_SDMD_KIND_128		0x80000000	/* SDRAM 128Mbits */
#define	 SD0001_SDMD_KIND_256		0xc0000000	/* SDRAM 256Mbits */
#define	 SD0001_SDMD_SIZE_MASK		0x30000000	/* SDRAM SIZE 設定マスク */
#define	 SD0001_SDMD_SIZE_4		0x00000000	/* データバス 4bits */
#define	 SD0001_SDMD_SIZE_8		0x10000000	/* データバス 8bits */
#define	 SD0001_SDMD_SIZE_16		0x20000000	/* データバス 16bits */
#define	 SD0001_SDMD_SIZE_32		0x30000000	/* データバス 32bits */
#define	 SD0001_SDMD_REF_MASK		0x0000f000	/* リフレッシュ・サイクル 設定マスク */
#define	 SD0001_SDMD_REF_DEF		0x00000000	/* リフレッシュ・サイクル デフォルト(128cycles) */
#define	 SD0001_SDMD_REF_128		0x00001000	/* リフレッシュ・サイクル 128cycles */
#define	 SD0001_SDMD_REF_256		0x00002000	/* リフレッシュ・サイクル 256cycles */
#define	 SD0001_SDMD_REF_384		0x00003000	/* リフレッシュ・サイクル 384cycles */
#define	 SD0001_SDMD_REF_512		0x00004000	/* リフレッシュ・サイクル 512cycles */
#define	 SD0001_SDMD_REF_640		0x00005000	/* リフレッシュ・サイクル 640cycles */
#define	 SD0001_SDMD_REF_768		0x00006000	/* リフレッシュ・サイクル 768cycles */
#define	 SD0001_SDMD_REF_896		0x00007000	/* リフレッシュ・サイクル 896cycles */
#define	 SD0001_SDMD_REF_STOP		0x00008000	/* リフレッシュ停止 */

#define	 SD0001_SDMD_LMODE_MASK		0x00000070	/* CASレイテンシ 設定マスク */
#define  SD0001_SDMD_LMODE_1		0x00000000	/* CASレイテンシ 1 */
#define  SD0001_SDMD_LMODE_2		0x00000010	/* CASレイテンシ 2 */
#define  SD0001_SDMD_LMODE_3		0x00000020	/* CASレイテンシ 3 */
#define  SD0001_SDMD_MASK		0xf000f070	/* SDMS 設定マスク */


/* 割り込み マスク */
#define	 SD0001_INT_INTEN		0x80000000	/* 全割り込みマスク */
#define	 SD0001_INT_RETRY		0x20000000	/* PCIバスリトライ回数オーバー */
#define	 SD0001_INT_TO			0x10000000	/* PCIバスタイムアウト */
#define	 SD0001_INT_SSERR		0x08000000	/* SERR#アサート */
#define	 SD0001_INT_RSERR		0x04000000	/* SERR#検出 */
#define	 SD0001_INT_RPERR		0x02000000	/* PERR#検出 */
#define	 SD0001_INT_SPERR		0x01000000	/* PERR#アサート */
#define	 SD0001_INT_STABT		0x00800000	/* ターゲットアボート発行 */
#define	 SD0001_INT_RTABT		0x00400000	/* ターゲットアボート検出 */
#define	 SD0001_INT_LOCK		0x00200000	/* デッドロック検出 */
#define	 SD0001_INT_RMABT		0x00100000	/* マスターアボート検出 */
#define	 SD0001_INT_BUSERR		0x3ff00000	/* PCIバスエラー割込み */
#define	 SD0001_INT_AWINT		0x00001000	/* AWAKE割込み */
#define	 SD0001_INT_DMA1		0x00000020	/* DMAチャネル1完了 */
#define	 SD0001_INT_DMA2		0x00000010	/* DMAチャネル2完了 */
#define	 SD0001_INT_INTD		0x00000008	/* PCIバスINTD# */
#define	 SD0001_INT_INTC		0x00000004	/* PCIバスINTC# */
#define	 SD0001_INT_INTB		0x00000002	/* PCIバスINTB# */
#define	 SD0001_INT_INTA		0x00000001	/* PCIバスINTA# */
#define	 SD0001_INT_VAILD		0x3ff0103f	/* 割り込み有効ビット */

/* CONFIG_ADDRESS レジスタ */
#define	 SD0001_CONFIG_ADDR_EN		0x80000000	/* コンフィグレーションサイクルイネーブル */


/* INDRCT_CMD レジスタ */
#define	 SD0001_INDRCTC_BE_MASK		0x000f0000	/* バイトイネーブル */
#define	 SD0001_INDRCTC_BE_BYTE		0x00010000	/* バイトアクセス開始位置 */
#define	 SD0001_INDRCTC_BE_WORD		0x00030000	/* ワードアクセス開始位置 */
#define	 SD0001_INDRCTC_BE_LONG		0x000f0000	/* ロングワードアクセス */
#define	 SD0001_INDRCTC_CMDEN		0x00008000	/* CMD Enable */
#define	 SD0001_INDRCTC_CMD_IOR		0x00000200	/* I/O Read CMD */
#define	 SD0001_INDRCTC_CMD_IOW		0x00000300	/* I/O Write CMD */
#define	 SD0001_INDRCTC_CMD_MEMR	0x00000600	/* Memory Read CMD */
#define	 SD0001_INDRCTC_CMD_MEMW	0x00000700	/* Memory Write CMD */
#define	 SD0001_INDRCTC_CMD_INTA	0x00000000	/* Interrupt Ack CMD */
#define	 SD0001_INDRCTC_CMD_MASK	0x00000f00	/* CMD コード */
#define	 SD0001_INDRCTC_FLGRESET	0x00000080	/* INDRCT_FLGのエラーフラグのリセット */
#define	 SD0001_INDRCTC_IOWT		0x00000008	/* 間接I/Oライト指示 */
#define	 SD0001_INDRCTC_IORD		0x00000004	/* 間接I/Oリード指示 */
#define	 SD0001_INDRCTC_COWT		0x00000002	/* コンフィグレーションライト指示 */
#define	 SD0001_INDRCTC_CORD		0x00000001	/* コンフィグレーションリード指示 */
#define	 SD0001_INDRCTC_MASK		0x000f8f8f	/* INDRCT_CMD 設定時マスク */

/* INDRCT_FLG レジスタ */
#define	 SD0001_INDRCTF_MABTRCV		0x00080000	/* マスターアボート発生 */
#define	 SD0001_INDRCTF_INDFLG		0x00000001	/* 間接アクセス実行中 */


/* Awake レジスタ */
#define	 SD0001_AWAKE_AWOK		0x80000000	/* AWAKE READ表示 */
#define	 SD0001_AWAKE_AWV		0x7fffffff	/* AWAKE READ付随情報 */

/* Mail レジスタ */
#define	 SD0001_MAIL_FLAG		0x80000000	/* MAIL 割込み発生 */
#define	 SD0001_MAIL_DATA		0x7fffffff	/* MAIL 割込み付随情報 */


/*
 * SD0001 内蔵 DMAC レジスタ機能定義
 */

/* DMATCR レジスタ */
#define	 SD0001_DMATCR_MASK		0x0fffffff	/* DMATCR設定マスク */
#define	 SD0001_DMATCR_MAX		0x04000000	/* 最大転送バイト数 */



/* DMCMD レジスタ */
#define	 SD0001_DMCMD_EXEC		0x80000000	/* DMA開始指示 / 実行中表示 */
#define	 SD0001_DMCMD_NEND		0x40000000	/* DMA成功による完了 */
#define	 SD0001_DMCMD_AEND		0x20000000	/* DMA失敗による完了 */
#define	 SD0001_DMCMD_STATUS		0x60000000	/* DMA完了ステータス */

#define  SD0001_DMCMD_SWAP		0x00010000	/* 転送データの Byte Swap */
#define	 SD0001_DMCMD_DSR_RAM_PCI	0x00000100	/* DMA転送方向： SDRAM → PCI */
#define	 SD0001_DMCMD_DSR_PCI_RAM	0x00000200	/* DMA転送方向： PCI → SDRAM */
#define	 SD0001_DMCMD_DSR_RAM_RAM	0x00000000	/* DMA転送方向： SDRAM → SDRAM */
#define	 SD0001_DMCMD_MASK		0xe0010f00	/* DMCMDレジスタ設定マスク */


/* INTM PCI レジスタ */
#define  SD0001_INTMPCI_PERR		0x80000000	/* Detected Perr 割込みマスク */
#define  SD0001_INTMPCI_SERR		0x40000000	/* Signalled SERR 割込みマスク */
#define  SD0001_INTMPCI_MBAT		0x20000000	/* Received Master Abort 割込みマスク */
#define  SD0001_INTMPCI_RTABT		0x10000000	/* Received Target Abort 割込みマスク */
#define  SD0001_INTMPCI_STABT		0x08000000	/* Signalled Target Abort 割込みマスク */
#define  SD0001_INTMPCI_DPERR		0x01000000	/* DPerr Detected 割込みマスク */
#define  SD0001_INTMPCI_MAIL		0x00800000	/* MAILレジスタ割込み */


/* RESET PCI レジスタ*/
#define  SD0001_RSTPCI_SWRST		0x80000000	/* ターゲットデバイス リセット */
#define  SD0001_RSTPCI_ALIVE		0x40000000	/* ターゲットモードの時のRESETレジスタの */
							/*    bit30(PCIRST)の値。 Read Only */


extern int pci_setup_sd0001(void);

void sd0001_outl(unsigned long, unsigned long); 
unsigned long sd0001_inl(unsigned long);
void sd0001_outw(unsigned short, unsigned long);
unsigned short sd0001_inw(unsigned long);
void sd0001_outb(unsigned char, unsigned long);
unsigned char sd0001_inb(unsigned long);
void sd0001_insb(unsigned long, void *, unsigned long);
void sd0001_insw(unsigned long, void *, unsigned long);
void sd0001_insl(unsigned long, void *, unsigned long);
void sd0001_outsb(unsigned long, const void *, unsigned long);
void sd0001_outsw(unsigned long, const void *, unsigned long);
void sd0001_outsl(unsigned long, const void *, unsigned long);
unsigned char sd0001_inb_p(unsigned long);
void sd0001_outb_p(unsigned char, unsigned long);

void *sd0001_ioremap(unsigned long, unsigned long);
void sd0001_iounmap(void *);


#endif /* __PCI_SD0001_H */
