// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2024  Realtek Corporation
 */

#include <linux/module.h>
#include <linux/pci.h>

#include "pci.h"
#include "reg.h"
#include "rtw8852bt.h"

static const struct rtw89_pci_ssid_quirk rtw8852bt_pci_ssid_quirks[] = {
	{RTW89_PCI_SSID(PCI_VENDOR_ID_REALTEK, 0xB520, 0x103C, 0x88E9, HP),
	 .bitmap = BIT(RTW89_QUIRK_THERMAL_PROT_110C)},
	{},
};

static const struct rtw89_pci_info rtw8852bt_pci_info = {
	.gen_def		= &rtw89_pci_gen_ax,
	.txbd_trunc_mode	= MAC_AX_BD_TRUNC,
	.rxbd_trunc_mode	= MAC_AX_BD_TRUNC,
	.rxbd_mode		= MAC_AX_RXBD_PKT,
	.tag_mode		= MAC_AX_TAG_MULTI,
	.tx_burst		= MAC_AX_TX_BURST_2048B,
	.rx_burst		= MAC_AX_RX_BURST_128B,
	.wd_dma_idle_intvl	= MAC_AX_WD_DMA_INTVL_256NS,
	.wd_dma_act_intvl	= MAC_AX_WD_DMA_INTVL_256NS,
	.multi_tag_num		= MAC_AX_TAG_NUM_8,
	.lbc_en			= MAC_AX_PCIE_ENABLE,
	.lbc_tmr		= MAC_AX_LBC_TMR_2MS,
	.autok_en		= MAC_AX_PCIE_DISABLE,
	.io_rcy_en		= MAC_AX_PCIE_DISABLE,
	.io_rcy_tmr		= MAC_AX_IO_RCY_ANA_TMR_6MS,
	.rx_ring_eq_is_full	= false,
	.check_rx_tag		= false,
	.no_rxbd_fs		= false,

	.init_cfg_reg		= R_AX_PCIE_INIT_CFG1,
	.txhci_en_bit		= B_AX_TXHCI_EN,
	.rxhci_en_bit		= B_AX_RXHCI_EN,
	.rxbd_mode_bit		= B_AX_RXBD_MODE,
	.exp_ctrl_reg		= R_AX_PCIE_EXP_CTRL,
	.max_tag_num_mask	= B_AX_MAX_TAG_NUM,
	.rxbd_rwptr_clr_reg	= R_AX_RXBD_RWPTR_CLR,
	.txbd_rwptr_clr2_reg	= 0,
	.dma_io_stop		= {R_AX_PCIE_DMA_STOP1, B_AX_STOP_PCIEIO},
	.dma_stop1		= {R_AX_PCIE_DMA_STOP1, B_AX_TX_STOP1_MASK_V1},
	.dma_stop2		= {0},
	.dma_busy1		= {R_AX_PCIE_DMA_BUSY1, DMA_BUSY1_CHECK_V1},
	.dma_busy2_reg		= 0,
	.dma_busy3_reg		= R_AX_PCIE_DMA_BUSY1,

	.rpwm_addr		= R_AX_PCIE_HRPWM,
	.cpwm_addr		= R_AX_CPWM,
	.mit_addr		= R_AX_INT_MIT_RX,
	.wp_sel_addr		= 0,
	.tx_dma_ch_mask		= BIT(RTW89_TXCH_ACH4) | BIT(RTW89_TXCH_ACH5) |
				  BIT(RTW89_TXCH_ACH6) | BIT(RTW89_TXCH_ACH7) |
				  BIT(RTW89_TXCH_CH10) | BIT(RTW89_TXCH_CH11),
	.bd_idx_addr_low_power	= NULL,
	.dma_addr_set		= &rtw89_pci_ch_dma_addr_set,
	.bd_ram_table		= &rtw89_bd_ram_table_single,

	.ltr_set		= rtw89_pci_ltr_set,
	.fill_txaddr_info	= rtw89_pci_fill_txaddr_info,
	.config_intr_mask	= rtw89_pci_config_intr_mask,
	.enable_intr		= rtw89_pci_enable_intr,
	.disable_intr		= rtw89_pci_disable_intr,
	.recognize_intrs	= rtw89_pci_recognize_intrs,

	.ssid_quirks		= rtw8852bt_pci_ssid_quirks,
};

static const struct rtw89_driver_info rtw89_8852bte_info = {
	.chip = &rtw8852bt_chip_info,
	.variant = NULL,
	.quirks = NULL,
	.bus = {
		.pci = &rtw8852bt_pci_info,
	},
};

static const struct pci_device_id rtw89_8852bte_id_table[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0xb520),
		.driver_data = (kernel_ulong_t)&rtw89_8852bte_info,
	},
	{},
};
MODULE_DEVICE_TABLE(pci, rtw89_8852bte_id_table);

static struct pci_driver rtw89_8852bte_driver = {
	.name		= "rtw89_8852bte",
	.id_table	= rtw89_8852bte_id_table,
	.probe		= rtw89_pci_probe,
	.remove		= rtw89_pci_remove,
	.driver.pm	= &rtw89_pm_ops,
};
module_pci_driver(rtw89_8852bte_driver);

MODULE_AUTHOR("Realtek Corporation");
MODULE_DESCRIPTION("Realtek 802.11ax wireless 8852BE-VT driver");
MODULE_LICENSE("Dual BSD/GPL");
