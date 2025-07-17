// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2020  Realtek Corporation
 */

#if defined(__FreeBSD__)
#define	LINUXKPI_PARAM_PREFIX	rtw89_pci_
#endif

#include <linux/pci.h>
#if defined(__FreeBSD__)
#include <sys/rman.h>
#endif

#include "mac.h"
#include "pci.h"
#include "reg.h"
#include "ser.h"

static bool rtw89_pci_disable_clkreq;
static bool rtw89_pci_disable_aspm_l1;
static bool rtw89_pci_disable_l1ss;
module_param_named(disable_clkreq, rtw89_pci_disable_clkreq, bool, 0644);
module_param_named(disable_aspm_l1, rtw89_pci_disable_aspm_l1, bool, 0644);
module_param_named(disable_aspm_l1ss, rtw89_pci_disable_l1ss, bool, 0644);
MODULE_PARM_DESC(disable_clkreq, "Set Y to disable PCI clkreq support");
MODULE_PARM_DESC(disable_aspm_l1, "Set Y to disable PCI ASPM L1 support");
MODULE_PARM_DESC(disable_aspm_l1ss, "Set Y to disable PCI L1SS support");

static int rtw89_pci_get_phy_offset_by_link_speed(struct rtw89_dev *rtwdev,
						  u32 *phy_offset)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;
	u32 val;
	int ret;

	ret = pci_read_config_dword(pdev, RTW89_PCIE_L1_STS_V1, &val);
	if (ret)
		return ret;

	val = u32_get_bits(val, RTW89_BCFG_LINK_SPEED_MASK);
	if (val == RTW89_PCIE_GEN1_SPEED) {
		*phy_offset = R_RAC_DIRECT_OFFSET_G1;
	} else if (val == RTW89_PCIE_GEN2_SPEED) {
		*phy_offset = R_RAC_DIRECT_OFFSET_G2;
	} else {
		rtw89_warn(rtwdev, "Unknown PCI link speed %d\n", val);
		return -EFAULT;
	}

	return 0;
}

static int rtw89_pci_rst_bdram_ax(struct rtw89_dev *rtwdev)
{
	u32 val;
	int ret;

	rtw89_write32_set(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_RST_BDRAM);

	ret = read_poll_timeout_atomic(rtw89_read32, val, !(val & B_AX_RST_BDRAM),
				       1, RTW89_PCI_POLL_BDRAM_RST_CNT, false,
				       rtwdev, R_AX_PCIE_INIT_CFG1);

	return ret;
}

static u32 rtw89_pci_dma_recalc(struct rtw89_dev *rtwdev,
				struct rtw89_pci_dma_ring *bd_ring,
				u32 cur_idx, bool tx)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 cnt, cur_rp, wp, rp, len;

	rp = bd_ring->rp;
	wp = bd_ring->wp;
	len = bd_ring->len;

	cur_rp = FIELD_GET(TXBD_HW_IDX_MASK, cur_idx);
	if (tx) {
		cnt = cur_rp >= rp ? cur_rp - rp : len - (rp - cur_rp);
	} else {
		if (info->rx_ring_eq_is_full)
			wp += 1;

		cnt = cur_rp >= wp ? cur_rp - wp : len - (wp - cur_rp);
	}

	bd_ring->rp = cur_rp;

	return cnt;
}

static u32 rtw89_pci_txbd_recalc(struct rtw89_dev *rtwdev,
				 struct rtw89_pci_tx_ring *tx_ring)
{
	struct rtw89_pci_dma_ring *bd_ring = &tx_ring->bd_ring;
	u32 addr_idx = bd_ring->addr.idx;
	u32 cnt, idx;

	idx = rtw89_read32(rtwdev, addr_idx);
	cnt = rtw89_pci_dma_recalc(rtwdev, bd_ring, idx, true);

	return cnt;
}

static void rtw89_pci_release_fwcmd(struct rtw89_dev *rtwdev,
				    struct rtw89_pci *rtwpci,
				    u32 cnt, bool release_all)
{
	struct rtw89_pci_tx_data *tx_data;
	struct sk_buff *skb;
	u32 qlen;

	while (cnt--) {
		skb = skb_dequeue(&rtwpci->h2c_queue);
		if (!skb) {
			rtw89_err(rtwdev, "failed to pre-release fwcmd\n");
			return;
		}
		skb_queue_tail(&rtwpci->h2c_release_queue, skb);
	}

	qlen = skb_queue_len(&rtwpci->h2c_release_queue);
	if (!release_all)
	       qlen = qlen > RTW89_PCI_MULTITAG ? qlen - RTW89_PCI_MULTITAG : 0;

	while (qlen--) {
		skb = skb_dequeue(&rtwpci->h2c_release_queue);
		if (!skb) {
			rtw89_err(rtwdev, "failed to release fwcmd\n");
			return;
		}
		tx_data = RTW89_PCI_TX_SKB_CB(skb);
		dma_unmap_single(&rtwpci->pdev->dev, tx_data->dma, skb->len,
				 DMA_TO_DEVICE);
		dev_kfree_skb_any(skb);
	}
}

static void rtw89_pci_reclaim_tx_fwcmd(struct rtw89_dev *rtwdev,
				       struct rtw89_pci *rtwpci)
{
	struct rtw89_pci_tx_ring *tx_ring = &rtwpci->tx_rings[RTW89_TXCH_CH12];
	u32 cnt;

	cnt = rtw89_pci_txbd_recalc(rtwdev, tx_ring);
	if (!cnt)
		return;
	rtw89_pci_release_fwcmd(rtwdev, rtwpci, cnt, false);
}

static u32 rtw89_pci_rxbd_recalc(struct rtw89_dev *rtwdev,
				 struct rtw89_pci_rx_ring *rx_ring)
{
	struct rtw89_pci_dma_ring *bd_ring = &rx_ring->bd_ring;
	u32 addr_idx = bd_ring->addr.idx;
	u32 cnt, idx;

	idx = rtw89_read32(rtwdev, addr_idx);
	cnt = rtw89_pci_dma_recalc(rtwdev, bd_ring, idx, false);

	return cnt;
}

static void rtw89_pci_sync_skb_for_cpu(struct rtw89_dev *rtwdev,
				       struct sk_buff *skb)
{
	struct rtw89_pci_rx_info *rx_info;
	dma_addr_t dma;

	rx_info = RTW89_PCI_RX_SKB_CB(skb);
	dma = rx_info->dma;
	dma_sync_single_for_cpu(rtwdev->dev, dma, RTW89_PCI_RX_BUF_SIZE,
				DMA_FROM_DEVICE);
}

static void rtw89_pci_sync_skb_for_device(struct rtw89_dev *rtwdev,
					  struct sk_buff *skb)
{
	struct rtw89_pci_rx_info *rx_info;
	dma_addr_t dma;

	rx_info = RTW89_PCI_RX_SKB_CB(skb);
	dma = rx_info->dma;
	dma_sync_single_for_device(rtwdev->dev, dma, RTW89_PCI_RX_BUF_SIZE,
				   DMA_FROM_DEVICE);
}

static void rtw89_pci_rxbd_info_update(struct rtw89_dev *rtwdev,
				       struct sk_buff *skb)
{
	struct rtw89_pci_rx_info *rx_info = RTW89_PCI_RX_SKB_CB(skb);
	struct rtw89_pci_rxbd_info *rxbd_info;
	__le32 info;

	rxbd_info = (struct rtw89_pci_rxbd_info *)skb->data;
	info = rxbd_info->dword;

	rx_info->fs = le32_get_bits(info, RTW89_PCI_RXBD_FS);
	rx_info->ls = le32_get_bits(info, RTW89_PCI_RXBD_LS);
	rx_info->len = le32_get_bits(info, RTW89_PCI_RXBD_WRITE_SIZE);
	rx_info->tag = le32_get_bits(info, RTW89_PCI_RXBD_TAG);
}

static int rtw89_pci_validate_rx_tag(struct rtw89_dev *rtwdev,
				     struct rtw89_pci_rx_ring *rx_ring,
				     struct sk_buff *skb)
{
	struct rtw89_pci_rx_info *rx_info = RTW89_PCI_RX_SKB_CB(skb);
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 target_rx_tag;

	if (!info->check_rx_tag)
		return 0;

	/* valid range is 1 ~ 0x1FFF */
	if (rx_ring->target_rx_tag == 0)
		target_rx_tag = 1;
	else
		target_rx_tag = rx_ring->target_rx_tag;

	if (rx_info->tag != target_rx_tag) {
		rtw89_debug(rtwdev, RTW89_DBG_UNEXP, "mismatch RX tag 0x%x 0x%x\n",
			    rx_info->tag, target_rx_tag);
		return -EAGAIN;
	}

	return 0;
}

static
int rtw89_pci_sync_skb_for_device_and_validate_rx_info(struct rtw89_dev *rtwdev,
						       struct rtw89_pci_rx_ring *rx_ring,
						       struct sk_buff *skb)
{
	struct rtw89_pci_rx_info *rx_info = RTW89_PCI_RX_SKB_CB(skb);
	int rx_tag_retry = 100;
	int ret;

	do {
		rtw89_pci_sync_skb_for_cpu(rtwdev, skb);
		rtw89_pci_rxbd_info_update(rtwdev, skb);

		ret = rtw89_pci_validate_rx_tag(rtwdev, rx_ring, skb);
		if (ret != -EAGAIN)
			break;
	} while (rx_tag_retry--);

	/* update target rx_tag for next RX */
	rx_ring->target_rx_tag = rx_info->tag + 1;

	return ret;
}

static void rtw89_pci_ctrl_txdma_ch_ax(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_reg_def *dma_stop1 = &info->dma_stop1;
	const struct rtw89_reg_def *dma_stop2 = &info->dma_stop2;

	if (enable) {
		rtw89_write32_clr(rtwdev, dma_stop1->addr, dma_stop1->mask);
		if (dma_stop2->addr)
			rtw89_write32_clr(rtwdev, dma_stop2->addr, dma_stop2->mask);
	} else {
		rtw89_write32_set(rtwdev, dma_stop1->addr, dma_stop1->mask);
		if (dma_stop2->addr)
			rtw89_write32_set(rtwdev, dma_stop2->addr, dma_stop2->mask);
	}
}

static void rtw89_pci_ctrl_txdma_fw_ch_ax(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_reg_def *dma_stop1 = &info->dma_stop1;

	if (enable)
		rtw89_write32_clr(rtwdev, dma_stop1->addr, B_AX_STOP_CH12);
	else
		rtw89_write32_set(rtwdev, dma_stop1->addr, B_AX_STOP_CH12);
}

static bool
rtw89_skb_put_rx_data(struct rtw89_dev *rtwdev, bool fs, bool ls,
		      struct sk_buff *new,
		      const struct sk_buff *skb, u32 offset,
		      const struct rtw89_pci_rx_info *rx_info,
		      const struct rtw89_rx_desc_info *desc_info)
{
	u32 copy_len = rx_info->len - offset;

	if (unlikely(skb_tailroom(new) < copy_len)) {
		rtw89_debug(rtwdev, RTW89_DBG_TXRX,
			    "invalid rx data length bd_len=%d desc_len=%d offset=%d (fs=%d ls=%d)\n",
			    rx_info->len, desc_info->pkt_size, offset, fs, ls);
		rtw89_hex_dump(rtwdev, RTW89_DBG_TXRX, "rx_data: ",
			       skb->data, rx_info->len);
		/* length of a single segment skb is desc_info->pkt_size */
		if (fs && ls) {
			copy_len = desc_info->pkt_size;
		} else {
			rtw89_info(rtwdev, "drop rx data due to invalid length\n");
			return false;
		}
	}

	skb_put_data(new, skb->data + offset, copy_len);

	return true;
}

static u32 rtw89_pci_get_rx_skb_idx(struct rtw89_dev *rtwdev,
				    struct rtw89_pci_dma_ring *bd_ring)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 wp = bd_ring->wp;

	if (!info->rx_ring_eq_is_full)
		return wp;

	if (++wp >= bd_ring->len)
		wp = 0;

	return wp;
}

static u32 rtw89_pci_rxbd_deliver_skbs(struct rtw89_dev *rtwdev,
				       struct rtw89_pci_rx_ring *rx_ring)
{
	struct rtw89_rx_desc_info *desc_info = &rx_ring->diliver_desc;
	struct rtw89_pci_dma_ring *bd_ring = &rx_ring->bd_ring;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	struct sk_buff *new = rx_ring->diliver_skb;
	struct rtw89_pci_rx_info *rx_info;
	struct sk_buff *skb;
	u32 rxinfo_size = sizeof(struct rtw89_pci_rxbd_info);
	u32 skb_idx;
	u32 offset;
	u32 cnt = 1;
	bool fs, ls;
	int ret;

	skb_idx = rtw89_pci_get_rx_skb_idx(rtwdev, bd_ring);
	skb = rx_ring->buf[skb_idx];

	ret = rtw89_pci_sync_skb_for_device_and_validate_rx_info(rtwdev, rx_ring, skb);
	if (ret) {
		rtw89_err(rtwdev, "failed to update %d RXBD info: %d\n",
			  bd_ring->wp, ret);
		goto err_sync_device;
	}

	rx_info = RTW89_PCI_RX_SKB_CB(skb);
	fs = info->no_rxbd_fs ? !new : rx_info->fs;
	ls = rx_info->ls;

	if (unlikely(!fs || !ls))
		rtw89_debug(rtwdev, RTW89_DBG_UNEXP,
			    "unexpected fs/ls=%d/%d tag=%u len=%u new->len=%u\n",
			    fs, ls, rx_info->tag, rx_info->len, new ? new->len : 0);

	if (fs) {
		if (new) {
			rtw89_debug(rtwdev, RTW89_DBG_UNEXP,
				    "skb should not be ready before first segment start\n");
			goto err_sync_device;
		}
		if (desc_info->ready) {
			rtw89_warn(rtwdev, "desc info should not be ready before first segment start\n");
			goto err_sync_device;
		}

		rtw89_chip_query_rxdesc(rtwdev, desc_info, skb->data, rxinfo_size);

		new = rtw89_alloc_skb_for_rx(rtwdev, desc_info->pkt_size);
		if (!new)
			goto err_sync_device;

		rx_ring->diliver_skb = new;

		/* first segment has RX desc */
		offset = desc_info->offset + desc_info->rxd_len;
	} else {
		offset = sizeof(struct rtw89_pci_rxbd_info);
		if (!new) {
			rtw89_debug(rtwdev, RTW89_DBG_UNEXP, "no last skb\n");
			goto err_sync_device;
		}
	}
	if (!rtw89_skb_put_rx_data(rtwdev, fs, ls, new, skb, offset, rx_info, desc_info))
		goto err_sync_device;
	rtw89_pci_sync_skb_for_device(rtwdev, skb);
	rtw89_pci_rxbd_increase(rx_ring, 1);

	if (!desc_info->ready) {
		rtw89_warn(rtwdev, "no rx desc information\n");
		goto err_free_resource;
	}
	if (ls) {
		rtw89_core_rx(rtwdev, desc_info, new);
		rx_ring->diliver_skb = NULL;
		desc_info->ready = false;
	}

	return cnt;

err_sync_device:
	rtw89_pci_sync_skb_for_device(rtwdev, skb);
	rtw89_pci_rxbd_increase(rx_ring, 1);
err_free_resource:
	if (new)
		dev_kfree_skb_any(new);
	rx_ring->diliver_skb = NULL;
	desc_info->ready = false;

	return cnt;
}

static void rtw89_pci_rxbd_deliver(struct rtw89_dev *rtwdev,
				   struct rtw89_pci_rx_ring *rx_ring,
				   u32 cnt)
{
	struct rtw89_pci_dma_ring *bd_ring = &rx_ring->bd_ring;
	u32 rx_cnt;

	while (cnt && rtwdev->napi_budget_countdown > 0) {
		rx_cnt = rtw89_pci_rxbd_deliver_skbs(rtwdev, rx_ring);
		if (!rx_cnt) {
			rtw89_err(rtwdev, "failed to deliver RXBD skb\n");

			/* skip the rest RXBD bufs */
			rtw89_pci_rxbd_increase(rx_ring, cnt);
			break;
		}

		cnt -= rx_cnt;
	}

	rtw89_write16(rtwdev, bd_ring->addr.idx, bd_ring->wp);
}

static int rtw89_pci_poll_rxq_dma(struct rtw89_dev *rtwdev,
				  struct rtw89_pci *rtwpci, int budget)
{
	struct rtw89_pci_rx_ring *rx_ring;
	int countdown = rtwdev->napi_budget_countdown;
	u32 cnt;

	rx_ring = &rtwpci->rx_rings[RTW89_RXCH_RXQ];

	cnt = rtw89_pci_rxbd_recalc(rtwdev, rx_ring);
	if (!cnt)
		return 0;

	cnt = min_t(u32, budget, cnt);

	rtw89_pci_rxbd_deliver(rtwdev, rx_ring, cnt);

	/* In case of flushing pending SKBs, the countdown may exceed. */
	if (rtwdev->napi_budget_countdown <= 0)
		return budget;

	return budget - countdown;
}

static void rtw89_pci_tx_status(struct rtw89_dev *rtwdev,
				struct rtw89_pci_tx_ring *tx_ring,
				struct sk_buff *skb, u8 tx_status)
{
	struct rtw89_tx_skb_data *skb_data = RTW89_TX_SKB_CB(skb);
	struct ieee80211_tx_info *info;

	rtw89_core_tx_wait_complete(rtwdev, skb_data, tx_status == RTW89_TX_DONE);

	info = IEEE80211_SKB_CB(skb);
	ieee80211_tx_info_clear_status(info);

	if (info->flags & IEEE80211_TX_CTL_NO_ACK)
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;
	if (tx_status == RTW89_TX_DONE) {
		info->flags |= IEEE80211_TX_STAT_ACK;
		tx_ring->tx_acked++;
	} else {
		if (info->flags & IEEE80211_TX_CTL_REQ_TX_STATUS)
			rtw89_debug(rtwdev, RTW89_DBG_FW,
				    "failed to TX of status %x\n", tx_status);
		switch (tx_status) {
		case RTW89_TX_RETRY_LIMIT:
			tx_ring->tx_retry_lmt++;
			break;
		case RTW89_TX_LIFE_TIME:
			tx_ring->tx_life_time++;
			break;
		case RTW89_TX_MACID_DROP:
			tx_ring->tx_mac_id_drop++;
			break;
		default:
			rtw89_warn(rtwdev, "invalid TX status %x\n", tx_status);
			break;
		}
	}

	ieee80211_tx_status_ni(rtwdev->hw, skb);
}

static void rtw89_pci_reclaim_txbd(struct rtw89_dev *rtwdev, struct rtw89_pci_tx_ring *tx_ring)
{
	struct rtw89_pci_tx_wd *txwd;
	u32 cnt;

	cnt = rtw89_pci_txbd_recalc(rtwdev, tx_ring);
	while (cnt--) {
		txwd = list_first_entry_or_null(&tx_ring->busy_pages, struct rtw89_pci_tx_wd, list);
		if (!txwd) {
			rtw89_warn(rtwdev, "No busy txwd pages available\n");
			break;
		}

		list_del_init(&txwd->list);

		/* this skb has been freed by RPP */
		if (skb_queue_len(&txwd->queue) == 0)
			rtw89_pci_enqueue_txwd(tx_ring, txwd);
	}
}

static void rtw89_pci_release_busy_txwd(struct rtw89_dev *rtwdev,
					struct rtw89_pci_tx_ring *tx_ring)
{
	struct rtw89_pci_tx_wd_ring *wd_ring = &tx_ring->wd_ring;
	struct rtw89_pci_tx_wd *txwd;
	int i;

	for (i = 0; i < wd_ring->page_num; i++) {
		txwd = list_first_entry_or_null(&tx_ring->busy_pages, struct rtw89_pci_tx_wd, list);
		if (!txwd)
			break;

		list_del_init(&txwd->list);
	}
}

static void rtw89_pci_release_txwd_skb(struct rtw89_dev *rtwdev,
				       struct rtw89_pci_tx_ring *tx_ring,
				       struct rtw89_pci_tx_wd *txwd, u16 seq,
				       u8 tx_status)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_data *tx_data;
	struct sk_buff *skb, *tmp;
	u8 txch = tx_ring->txch;

	if (!list_empty(&txwd->list)) {
		rtw89_pci_reclaim_txbd(rtwdev, tx_ring);
		/* In low power mode, RPP can receive before updating of TX BD.
		 * In normal mode, it should not happen so give it a warning.
		 */
		if (!rtwpci->low_power && !list_empty(&txwd->list))
			rtw89_warn(rtwdev, "queue %d txwd %d is not idle\n",
				   txch, seq);
	}

	skb_queue_walk_safe(&txwd->queue, skb, tmp) {
		skb_unlink(skb, &txwd->queue);

		tx_data = RTW89_PCI_TX_SKB_CB(skb);
		dma_unmap_single(&rtwpci->pdev->dev, tx_data->dma, skb->len,
				 DMA_TO_DEVICE);

		rtw89_pci_tx_status(rtwdev, tx_ring, skb, tx_status);
	}

	if (list_empty(&txwd->list))
		rtw89_pci_enqueue_txwd(tx_ring, txwd);
}

static void rtw89_pci_release_rpp(struct rtw89_dev *rtwdev,
				  struct rtw89_pci_rpp_fmt *rpp)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring;
	struct rtw89_pci_tx_wd_ring *wd_ring;
	struct rtw89_pci_tx_wd *txwd;
	u16 seq;
	u8 qsel, tx_status, txch;

	seq = le32_get_bits(rpp->dword, RTW89_PCI_RPP_SEQ);
	qsel = le32_get_bits(rpp->dword, RTW89_PCI_RPP_QSEL);
	tx_status = le32_get_bits(rpp->dword, RTW89_PCI_RPP_TX_STATUS);
	txch = rtw89_core_get_ch_dma(rtwdev, qsel);

	if (txch == RTW89_TXCH_CH12) {
		rtw89_warn(rtwdev, "should no fwcmd release report\n");
		return;
	}

	tx_ring = &rtwpci->tx_rings[txch];
	wd_ring = &tx_ring->wd_ring;
	txwd = &wd_ring->pages[seq];

	rtw89_pci_release_txwd_skb(rtwdev, tx_ring, txwd, seq, tx_status);
}

static void rtw89_pci_release_pending_txwd_skb(struct rtw89_dev *rtwdev,
					       struct rtw89_pci_tx_ring *tx_ring)
{
	struct rtw89_pci_tx_wd_ring *wd_ring = &tx_ring->wd_ring;
	struct rtw89_pci_tx_wd *txwd;
	int i;

	for (i = 0; i < wd_ring->page_num; i++) {
		txwd = &wd_ring->pages[i];

		if (!list_empty(&txwd->list))
			continue;

		rtw89_pci_release_txwd_skb(rtwdev, tx_ring, txwd, i, RTW89_TX_MACID_DROP);
	}
}

static u32 rtw89_pci_release_tx_skbs(struct rtw89_dev *rtwdev,
				     struct rtw89_pci_rx_ring *rx_ring,
				     u32 max_cnt)
{
	struct rtw89_pci_dma_ring *bd_ring = &rx_ring->bd_ring;
	struct rtw89_pci_rx_info *rx_info;
	struct rtw89_pci_rpp_fmt *rpp;
	struct rtw89_rx_desc_info desc_info = {};
	struct sk_buff *skb;
	u32 cnt = 0;
	u32 rpp_size = sizeof(struct rtw89_pci_rpp_fmt);
	u32 rxinfo_size = sizeof(struct rtw89_pci_rxbd_info);
	u32 skb_idx;
	u32 offset;
	int ret;

	skb_idx = rtw89_pci_get_rx_skb_idx(rtwdev, bd_ring);
	skb = rx_ring->buf[skb_idx];

	ret = rtw89_pci_sync_skb_for_device_and_validate_rx_info(rtwdev, rx_ring, skb);
	if (ret) {
		rtw89_err(rtwdev, "failed to update %d RXBD info: %d\n",
			  bd_ring->wp, ret);
		goto err_sync_device;
	}

	rx_info = RTW89_PCI_RX_SKB_CB(skb);
	if (!rx_info->fs || !rx_info->ls) {
		rtw89_err(rtwdev, "cannot process RP frame not set FS/LS\n");
		return cnt;
	}

	rtw89_chip_query_rxdesc(rtwdev, &desc_info, skb->data, rxinfo_size);

	/* first segment has RX desc */
	offset = desc_info.offset + desc_info.rxd_len;
	for (; offset + rpp_size <= rx_info->len; offset += rpp_size) {
		rpp = (struct rtw89_pci_rpp_fmt *)(skb->data + offset);
		rtw89_pci_release_rpp(rtwdev, rpp);
	}

	rtw89_pci_sync_skb_for_device(rtwdev, skb);
	rtw89_pci_rxbd_increase(rx_ring, 1);
	cnt++;

	return cnt;

err_sync_device:
	rtw89_pci_sync_skb_for_device(rtwdev, skb);
	return 0;
}

static void rtw89_pci_release_tx(struct rtw89_dev *rtwdev,
				 struct rtw89_pci_rx_ring *rx_ring,
				 u32 cnt)
{
	struct rtw89_pci_dma_ring *bd_ring = &rx_ring->bd_ring;
	u32 release_cnt;

	while (cnt) {
		release_cnt = rtw89_pci_release_tx_skbs(rtwdev, rx_ring, cnt);
		if (!release_cnt) {
			rtw89_err(rtwdev, "failed to release TX skbs\n");

			/* skip the rest RXBD bufs */
			rtw89_pci_rxbd_increase(rx_ring, cnt);
			break;
		}

		cnt -= release_cnt;
	}

	rtw89_write16(rtwdev, bd_ring->addr.idx, bd_ring->wp);
}

static int rtw89_pci_poll_rpq_dma(struct rtw89_dev *rtwdev,
				  struct rtw89_pci *rtwpci, int budget)
{
	struct rtw89_pci_rx_ring *rx_ring;
	u32 cnt;
	int work_done;

	rx_ring = &rtwpci->rx_rings[RTW89_RXCH_RPQ];

	spin_lock_bh(&rtwpci->trx_lock);

	cnt = rtw89_pci_rxbd_recalc(rtwdev, rx_ring);
	if (cnt == 0)
		goto out_unlock;

	rtw89_pci_release_tx(rtwdev, rx_ring, cnt);

out_unlock:
	spin_unlock_bh(&rtwpci->trx_lock);

	/* always release all RPQ */
	work_done = min_t(int, cnt, budget);
	rtwdev->napi_budget_countdown -= work_done;

	return work_done;
}

static void rtw89_pci_isr_rxd_unavail(struct rtw89_dev *rtwdev,
				      struct rtw89_pci *rtwpci)
{
	struct rtw89_pci_rx_ring *rx_ring;
	struct rtw89_pci_dma_ring *bd_ring;
	u32 reg_idx;
	u16 hw_idx, hw_idx_next, host_idx;
	int i;

	for (i = 0; i < RTW89_RXCH_NUM; i++) {
		rx_ring = &rtwpci->rx_rings[i];
		bd_ring = &rx_ring->bd_ring;

		reg_idx = rtw89_read32(rtwdev, bd_ring->addr.idx);
		hw_idx = FIELD_GET(TXBD_HW_IDX_MASK, reg_idx);
		host_idx = FIELD_GET(TXBD_HOST_IDX_MASK, reg_idx);
		hw_idx_next = (hw_idx + 1) % bd_ring->len;

		if (hw_idx_next == host_idx)
			rtw89_debug(rtwdev, RTW89_DBG_UNEXP, "%d RXD unavailable\n", i);

		rtw89_debug(rtwdev, RTW89_DBG_TXRX,
			    "%d RXD unavailable, idx=0x%08x, len=%d\n",
			    i, reg_idx, bd_ring->len);
	}
}

void rtw89_pci_recognize_intrs(struct rtw89_dev *rtwdev,
			       struct rtw89_pci *rtwpci,
			       struct rtw89_pci_isrs *isrs)
{
	isrs->halt_c2h_isrs = rtw89_read32(rtwdev, R_AX_HISR0) & rtwpci->halt_c2h_intrs;
	isrs->isrs[0] = rtw89_read32(rtwdev, R_AX_PCIE_HISR00) & rtwpci->intrs[0];
	isrs->isrs[1] = rtw89_read32(rtwdev, R_AX_PCIE_HISR10) & rtwpci->intrs[1];

	rtw89_write32(rtwdev, R_AX_HISR0, isrs->halt_c2h_isrs);
	rtw89_write32(rtwdev, R_AX_PCIE_HISR00, isrs->isrs[0]);
	rtw89_write32(rtwdev, R_AX_PCIE_HISR10, isrs->isrs[1]);
}
EXPORT_SYMBOL(rtw89_pci_recognize_intrs);

void rtw89_pci_recognize_intrs_v1(struct rtw89_dev *rtwdev,
				  struct rtw89_pci *rtwpci,
				  struct rtw89_pci_isrs *isrs)
{
	isrs->ind_isrs = rtw89_read32(rtwdev, R_AX_PCIE_HISR00_V1) & rtwpci->ind_intrs;
	isrs->halt_c2h_isrs = isrs->ind_isrs & B_AX_HS0ISR_IND_INT_EN ?
			      rtw89_read32(rtwdev, R_AX_HISR0) & rtwpci->halt_c2h_intrs : 0;
	isrs->isrs[0] = isrs->ind_isrs & B_AX_HCI_AXIDMA_INT_EN ?
			rtw89_read32(rtwdev, R_AX_HAXI_HISR00) & rtwpci->intrs[0] : 0;
	isrs->isrs[1] = isrs->ind_isrs & B_AX_HS1ISR_IND_INT_EN ?
			rtw89_read32(rtwdev, R_AX_HISR1) & rtwpci->intrs[1] : 0;

	if (isrs->halt_c2h_isrs)
		rtw89_write32(rtwdev, R_AX_HISR0, isrs->halt_c2h_isrs);
	if (isrs->isrs[0])
		rtw89_write32(rtwdev, R_AX_HAXI_HISR00, isrs->isrs[0]);
	if (isrs->isrs[1])
		rtw89_write32(rtwdev, R_AX_HISR1, isrs->isrs[1]);
}
EXPORT_SYMBOL(rtw89_pci_recognize_intrs_v1);

void rtw89_pci_recognize_intrs_v2(struct rtw89_dev *rtwdev,
				  struct rtw89_pci *rtwpci,
				  struct rtw89_pci_isrs *isrs)
{
	isrs->ind_isrs = rtw89_read32(rtwdev, R_BE_PCIE_HISR) & rtwpci->ind_intrs;
	isrs->halt_c2h_isrs = isrs->ind_isrs & B_BE_HS0ISR_IND_INT ?
			      rtw89_read32(rtwdev, R_BE_HISR0) & rtwpci->halt_c2h_intrs : 0;
	isrs->isrs[0] = isrs->ind_isrs & B_BE_HCI_AXIDMA_INT ?
			rtw89_read32(rtwdev, R_BE_HAXI_HISR00) & rtwpci->intrs[0] : 0;
	isrs->isrs[1] = rtw89_read32(rtwdev, R_BE_PCIE_DMA_ISR) & rtwpci->intrs[1];

	if (isrs->halt_c2h_isrs)
		rtw89_write32(rtwdev, R_BE_HISR0, isrs->halt_c2h_isrs);
	if (isrs->isrs[0])
		rtw89_write32(rtwdev, R_BE_HAXI_HISR00, isrs->isrs[0]);
	if (isrs->isrs[1])
		rtw89_write32(rtwdev, R_BE_PCIE_DMA_ISR, isrs->isrs[1]);
	rtw89_write32(rtwdev, R_BE_PCIE_HISR, isrs->ind_isrs);
}
EXPORT_SYMBOL(rtw89_pci_recognize_intrs_v2);

void rtw89_pci_enable_intr(struct rtw89_dev *rtwdev, struct rtw89_pci *rtwpci)
{
	rtw89_write32(rtwdev, R_AX_HIMR0, rtwpci->halt_c2h_intrs);
	rtw89_write32(rtwdev, R_AX_PCIE_HIMR00, rtwpci->intrs[0]);
	rtw89_write32(rtwdev, R_AX_PCIE_HIMR10, rtwpci->intrs[1]);
}
EXPORT_SYMBOL(rtw89_pci_enable_intr);

void rtw89_pci_disable_intr(struct rtw89_dev *rtwdev, struct rtw89_pci *rtwpci)
{
	rtw89_write32(rtwdev, R_AX_HIMR0, 0);
	rtw89_write32(rtwdev, R_AX_PCIE_HIMR00, 0);
	rtw89_write32(rtwdev, R_AX_PCIE_HIMR10, 0);
}
EXPORT_SYMBOL(rtw89_pci_disable_intr);

void rtw89_pci_enable_intr_v1(struct rtw89_dev *rtwdev, struct rtw89_pci *rtwpci)
{
	rtw89_write32(rtwdev, R_AX_PCIE_HIMR00_V1, rtwpci->ind_intrs);
	rtw89_write32(rtwdev, R_AX_HIMR0, rtwpci->halt_c2h_intrs);
	rtw89_write32(rtwdev, R_AX_HAXI_HIMR00, rtwpci->intrs[0]);
	rtw89_write32(rtwdev, R_AX_HIMR1, rtwpci->intrs[1]);
}
EXPORT_SYMBOL(rtw89_pci_enable_intr_v1);

void rtw89_pci_disable_intr_v1(struct rtw89_dev *rtwdev, struct rtw89_pci *rtwpci)
{
	rtw89_write32(rtwdev, R_AX_PCIE_HIMR00_V1, 0);
}
EXPORT_SYMBOL(rtw89_pci_disable_intr_v1);

void rtw89_pci_enable_intr_v2(struct rtw89_dev *rtwdev, struct rtw89_pci *rtwpci)
{
	rtw89_write32(rtwdev, R_BE_HIMR0, rtwpci->halt_c2h_intrs);
	rtw89_write32(rtwdev, R_BE_HAXI_HIMR00, rtwpci->intrs[0]);
	rtw89_write32(rtwdev, R_BE_PCIE_DMA_IMR_0_V1, rtwpci->intrs[1]);
	rtw89_write32(rtwdev, R_BE_PCIE_HIMR0, rtwpci->ind_intrs);
}
EXPORT_SYMBOL(rtw89_pci_enable_intr_v2);

void rtw89_pci_disable_intr_v2(struct rtw89_dev *rtwdev, struct rtw89_pci *rtwpci)
{
	rtw89_write32(rtwdev, R_BE_PCIE_HIMR0, 0);
	rtw89_write32(rtwdev, R_BE_PCIE_DMA_IMR_0_V1, 0);
}
EXPORT_SYMBOL(rtw89_pci_disable_intr_v2);

static void rtw89_pci_ops_recovery_start(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&rtwpci->irq_lock, flags);
	rtw89_chip_disable_intr(rtwdev, rtwpci);
	rtw89_chip_config_intr_mask(rtwdev, RTW89_PCI_INTR_MASK_RECOVERY_START);
	rtw89_chip_enable_intr(rtwdev, rtwpci);
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);
}

static void rtw89_pci_ops_recovery_complete(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&rtwpci->irq_lock, flags);
	rtw89_chip_disable_intr(rtwdev, rtwpci);
	rtw89_chip_config_intr_mask(rtwdev, RTW89_PCI_INTR_MASK_RECOVERY_COMPLETE);
	rtw89_chip_enable_intr(rtwdev, rtwpci);
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);
}

static void rtw89_pci_low_power_interrupt_handler(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	int budget = NAPI_POLL_WEIGHT;

	/* To prevent RXQ get stuck due to run out of budget. */
	rtwdev->napi_budget_countdown = budget;

	rtw89_pci_poll_rpq_dma(rtwdev, rtwpci, budget);
	rtw89_pci_poll_rxq_dma(rtwdev, rtwpci, budget);
}

static irqreturn_t rtw89_pci_interrupt_threadfn(int irq, void *dev)
{
	struct rtw89_dev *rtwdev = dev;
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_gen_def *gen_def = info->gen_def;
	struct rtw89_pci_isrs isrs;
	unsigned long flags;

	spin_lock_irqsave(&rtwpci->irq_lock, flags);
	rtw89_chip_recognize_intrs(rtwdev, rtwpci, &isrs);
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);

	if (unlikely(isrs.isrs[0] & gen_def->isr_rdu))
		rtw89_pci_isr_rxd_unavail(rtwdev, rtwpci);

	if (unlikely(isrs.halt_c2h_isrs & gen_def->isr_halt_c2h))
		rtw89_ser_notify(rtwdev, rtw89_mac_get_err_status(rtwdev));

	if (unlikely(isrs.halt_c2h_isrs & gen_def->isr_wdt_timeout))
		rtw89_ser_notify(rtwdev, MAC_AX_ERR_L2_ERR_WDT_TIMEOUT_INT);

	if (unlikely(rtwpci->under_recovery))
		goto enable_intr;

	if (unlikely(rtwpci->low_power)) {
		rtw89_pci_low_power_interrupt_handler(rtwdev);
		goto enable_intr;
	}

	if (likely(rtwpci->running)) {
		local_bh_disable();
		napi_schedule(&rtwdev->napi);
		local_bh_enable();
	}

	return IRQ_HANDLED;

enable_intr:
	spin_lock_irqsave(&rtwpci->irq_lock, flags);
	if (likely(rtwpci->running))
		rtw89_chip_enable_intr(rtwdev, rtwpci);
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t rtw89_pci_interrupt_handler(int irq, void *dev)
{
	struct rtw89_dev *rtwdev = dev;
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	unsigned long flags;
	irqreturn_t irqret = IRQ_WAKE_THREAD;

	spin_lock_irqsave(&rtwpci->irq_lock, flags);

	/* If interrupt event is on the road, it is still trigger interrupt
	 * even we have done pci_stop() to turn off IMR.
	 */
	if (unlikely(!rtwpci->running)) {
		irqret = IRQ_HANDLED;
		goto exit;
	}

	rtw89_chip_disable_intr(rtwdev, rtwpci);
exit:
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);

	return irqret;
}

#define DEF_TXCHADDRS_TYPE2(gen, ch_idx, txch, v...) \
	[RTW89_TXCH_##ch_idx] = { \
		.num = R_##gen##_##txch##_TXBD_NUM ##v, \
		.idx = R_##gen##_##txch##_TXBD_IDX ##v, \
		.bdram = 0, \
		.desa_l = R_##gen##_##txch##_TXBD_DESA_L ##v, \
		.desa_h = R_##gen##_##txch##_TXBD_DESA_H ##v, \
	}

#define DEF_TXCHADDRS_TYPE1(info, txch, v...) \
	[RTW89_TXCH_##txch] = { \
		.num = R_AX_##txch##_TXBD_NUM ##v, \
		.idx = R_AX_##txch##_TXBD_IDX ##v, \
		.bdram = R_AX_##txch##_BDRAM_CTRL ##v, \
		.desa_l = R_AX_##txch##_TXBD_DESA_L ##v, \
		.desa_h = R_AX_##txch##_TXBD_DESA_H ##v, \
	}

#define DEF_TXCHADDRS(info, txch, v...) \
	[RTW89_TXCH_##txch] = { \
		.num = R_AX_##txch##_TXBD_NUM, \
		.idx = R_AX_##txch##_TXBD_IDX, \
		.bdram = R_AX_##txch##_BDRAM_CTRL ##v, \
		.desa_l = R_AX_##txch##_TXBD_DESA_L ##v, \
		.desa_h = R_AX_##txch##_TXBD_DESA_H ##v, \
	}

#define DEF_RXCHADDRS(gen, ch_idx, rxch, v...) \
	[RTW89_RXCH_##ch_idx] = { \
		.num = R_##gen##_##rxch##_RXBD_NUM ##v, \
		.idx = R_##gen##_##rxch##_RXBD_IDX ##v, \
		.desa_l = R_##gen##_##rxch##_RXBD_DESA_L ##v, \
		.desa_h = R_##gen##_##rxch##_RXBD_DESA_H ##v, \
	}

const struct rtw89_pci_ch_dma_addr_set rtw89_pci_ch_dma_addr_set = {
	.tx = {
		DEF_TXCHADDRS(info, ACH0),
		DEF_TXCHADDRS(info, ACH1),
		DEF_TXCHADDRS(info, ACH2),
		DEF_TXCHADDRS(info, ACH3),
		DEF_TXCHADDRS(info, ACH4),
		DEF_TXCHADDRS(info, ACH5),
		DEF_TXCHADDRS(info, ACH6),
		DEF_TXCHADDRS(info, ACH7),
		DEF_TXCHADDRS(info, CH8),
		DEF_TXCHADDRS(info, CH9),
		DEF_TXCHADDRS_TYPE1(info, CH10),
		DEF_TXCHADDRS_TYPE1(info, CH11),
		DEF_TXCHADDRS(info, CH12),
	},
	.rx = {
		DEF_RXCHADDRS(AX, RXQ, RXQ),
		DEF_RXCHADDRS(AX, RPQ, RPQ),
	},
};
EXPORT_SYMBOL(rtw89_pci_ch_dma_addr_set);

const struct rtw89_pci_ch_dma_addr_set rtw89_pci_ch_dma_addr_set_v1 = {
	.tx = {
		DEF_TXCHADDRS(info, ACH0, _V1),
		DEF_TXCHADDRS(info, ACH1, _V1),
		DEF_TXCHADDRS(info, ACH2, _V1),
		DEF_TXCHADDRS(info, ACH3, _V1),
		DEF_TXCHADDRS(info, ACH4, _V1),
		DEF_TXCHADDRS(info, ACH5, _V1),
		DEF_TXCHADDRS(info, ACH6, _V1),
		DEF_TXCHADDRS(info, ACH7, _V1),
		DEF_TXCHADDRS(info, CH8, _V1),
		DEF_TXCHADDRS(info, CH9, _V1),
		DEF_TXCHADDRS_TYPE1(info, CH10, _V1),
		DEF_TXCHADDRS_TYPE1(info, CH11, _V1),
		DEF_TXCHADDRS(info, CH12, _V1),
	},
	.rx = {
		DEF_RXCHADDRS(AX, RXQ, RXQ, _V1),
		DEF_RXCHADDRS(AX, RPQ, RPQ, _V1),
	},
};
EXPORT_SYMBOL(rtw89_pci_ch_dma_addr_set_v1);

const struct rtw89_pci_ch_dma_addr_set rtw89_pci_ch_dma_addr_set_be = {
	.tx = {
		DEF_TXCHADDRS_TYPE2(BE, ACH0, CH0, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH1, CH1, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH2, CH2, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH3, CH3, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH4, CH4, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH5, CH5, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH6, CH6, _V1),
		DEF_TXCHADDRS_TYPE2(BE, ACH7, CH7, _V1),
		DEF_TXCHADDRS_TYPE2(BE, CH8, CH8, _V1),
		DEF_TXCHADDRS_TYPE2(BE, CH9, CH9, _V1),
		DEF_TXCHADDRS_TYPE2(BE, CH10, CH10, _V1),
		DEF_TXCHADDRS_TYPE2(BE, CH11, CH11, _V1),
		DEF_TXCHADDRS_TYPE2(BE, CH12, CH12, _V1),
	},
	.rx = {
		DEF_RXCHADDRS(BE, RXQ, RXQ0, _V1),
		DEF_RXCHADDRS(BE, RPQ, RPQ0, _V1),
	},
};
EXPORT_SYMBOL(rtw89_pci_ch_dma_addr_set_be);

#undef DEF_TXCHADDRS_TYPE1
#undef DEF_TXCHADDRS
#undef DEF_RXCHADDRS

static int rtw89_pci_get_txch_addrs(struct rtw89_dev *rtwdev,
				    enum rtw89_tx_channel txch,
				    const struct rtw89_pci_ch_dma_addr **addr)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;

	if (txch >= RTW89_TXCH_NUM)
		return -EINVAL;

	*addr = &info->dma_addr_set->tx[txch];

	return 0;
}

static int rtw89_pci_get_rxch_addrs(struct rtw89_dev *rtwdev,
				    enum rtw89_rx_channel rxch,
				    const struct rtw89_pci_ch_dma_addr **addr)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;

	if (rxch >= RTW89_RXCH_NUM)
		return -EINVAL;

	*addr = &info->dma_addr_set->rx[rxch];

	return 0;
}

static u32 rtw89_pci_get_avail_txbd_num(struct rtw89_pci_tx_ring *ring)
{
	struct rtw89_pci_dma_ring *bd_ring = &ring->bd_ring;

	/* reserved 1 desc check ring is full or not */
	if (bd_ring->rp > bd_ring->wp)
		return bd_ring->rp - bd_ring->wp - 1;

	return bd_ring->len - (bd_ring->wp - bd_ring->rp) - 1;
}

static
u32 __rtw89_pci_check_and_reclaim_tx_fwcmd_resource(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring = &rtwpci->tx_rings[RTW89_TXCH_CH12];
	u32 cnt;

	spin_lock_bh(&rtwpci->trx_lock);
	rtw89_pci_reclaim_tx_fwcmd(rtwdev, rtwpci);
	cnt = rtw89_pci_get_avail_txbd_num(tx_ring);
	spin_unlock_bh(&rtwpci->trx_lock);

	return cnt;
}

static
u32 __rtw89_pci_check_and_reclaim_tx_resource_noio(struct rtw89_dev *rtwdev,
						   u8 txch)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring = &rtwpci->tx_rings[txch];
	struct rtw89_pci_tx_wd_ring *wd_ring = &tx_ring->wd_ring;
	u32 cnt;

	spin_lock_bh(&rtwpci->trx_lock);
	cnt = rtw89_pci_get_avail_txbd_num(tx_ring);
	if (txch != RTW89_TXCH_CH12)
		cnt = min(cnt, wd_ring->curr_num);
	spin_unlock_bh(&rtwpci->trx_lock);

	return cnt;
}

static u32 __rtw89_pci_check_and_reclaim_tx_resource(struct rtw89_dev *rtwdev,
						     u8 txch)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring = &rtwpci->tx_rings[txch];
	struct rtw89_pci_tx_wd_ring *wd_ring = &tx_ring->wd_ring;
	const struct rtw89_chip_info *chip = rtwdev->chip;
	u32 bd_cnt, wd_cnt, min_cnt = 0;
	struct rtw89_pci_rx_ring *rx_ring;
	enum rtw89_debug_mask debug_mask;
	u32 cnt;

	rx_ring = &rtwpci->rx_rings[RTW89_RXCH_RPQ];

	spin_lock_bh(&rtwpci->trx_lock);
	bd_cnt = rtw89_pci_get_avail_txbd_num(tx_ring);
	wd_cnt = wd_ring->curr_num;

	if (wd_cnt == 0 || bd_cnt == 0) {
		cnt = rtw89_pci_rxbd_recalc(rtwdev, rx_ring);
		if (cnt)
			rtw89_pci_release_tx(rtwdev, rx_ring, cnt);
		else if (wd_cnt == 0)
			goto out_unlock;

		bd_cnt = rtw89_pci_get_avail_txbd_num(tx_ring);
		if (bd_cnt == 0)
			rtw89_pci_reclaim_txbd(rtwdev, tx_ring);
	}

	bd_cnt = rtw89_pci_get_avail_txbd_num(tx_ring);
	wd_cnt = wd_ring->curr_num;
	min_cnt = min(bd_cnt, wd_cnt);
	if (min_cnt == 0) {
		/* This message can be frequently shown in low power mode or
		 * high traffic with small FIFO chips, and we have recognized it as normal
		 * behavior, so print with mask RTW89_DBG_TXRX in these situations.
		 */
		if (rtwpci->low_power || chip->small_fifo_size)
			debug_mask = RTW89_DBG_TXRX;
		else
			debug_mask = RTW89_DBG_UNEXP;

		rtw89_debug(rtwdev, debug_mask,
			    "still no tx resource after reclaim: wd_cnt=%d bd_cnt=%d\n",
			    wd_cnt, bd_cnt);
	}

out_unlock:
	spin_unlock_bh(&rtwpci->trx_lock);

	return min_cnt;
}

static u32 rtw89_pci_check_and_reclaim_tx_resource(struct rtw89_dev *rtwdev,
						   u8 txch)
{
	if (rtwdev->hci.paused)
		return __rtw89_pci_check_and_reclaim_tx_resource_noio(rtwdev, txch);

	if (txch == RTW89_TXCH_CH12)
		return __rtw89_pci_check_and_reclaim_tx_fwcmd_resource(rtwdev);

	return __rtw89_pci_check_and_reclaim_tx_resource(rtwdev, txch);
}

static void __rtw89_pci_tx_kick_off(struct rtw89_dev *rtwdev, struct rtw89_pci_tx_ring *tx_ring)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_dma_ring *bd_ring = &tx_ring->bd_ring;
	u32 host_idx, addr;

	spin_lock_bh(&rtwpci->trx_lock);

	addr = bd_ring->addr.idx;
	host_idx = bd_ring->wp;
	rtw89_write16(rtwdev, addr, host_idx);

	spin_unlock_bh(&rtwpci->trx_lock);
}

static void rtw89_pci_tx_bd_ring_update(struct rtw89_dev *rtwdev, struct rtw89_pci_tx_ring *tx_ring,
					int n_txbd)
{
	struct rtw89_pci_dma_ring *bd_ring = &tx_ring->bd_ring;
	u32 host_idx, len;

	len = bd_ring->len;
	host_idx = bd_ring->wp + n_txbd;
	host_idx = host_idx < len ? host_idx : host_idx - len;

	bd_ring->wp = host_idx;
}

static void rtw89_pci_ops_tx_kick_off(struct rtw89_dev *rtwdev, u8 txch)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring = &rtwpci->tx_rings[txch];

	if (rtwdev->hci.paused) {
		set_bit(txch, rtwpci->kick_map);
		return;
	}

	__rtw89_pci_tx_kick_off(rtwdev, tx_ring);
}

static void rtw89_pci_tx_kick_off_pending(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring;
	int txch;

	for (txch = 0; txch < RTW89_TXCH_NUM; txch++) {
		if (!test_and_clear_bit(txch, rtwpci->kick_map))
			continue;

		tx_ring = &rtwpci->tx_rings[txch];
		__rtw89_pci_tx_kick_off(rtwdev, tx_ring);
	}
}

static void __pci_flush_txch(struct rtw89_dev *rtwdev, u8 txch, bool drop)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring = &rtwpci->tx_rings[txch];
	struct rtw89_pci_dma_ring *bd_ring = &tx_ring->bd_ring;
	u32 cur_idx, cur_rp;
	u8 i;

	/* Because the time taked by the I/O is a bit dynamic, it's hard to
	 * define a reasonable fixed total timeout to use read_poll_timeout*
	 * helper. Instead, we can ensure a reasonable polling times, so we
	 * just use for loop with udelay here.
	 */
	for (i = 0; i < 60; i++) {
		cur_idx = rtw89_read32(rtwdev, bd_ring->addr.idx);
		cur_rp = FIELD_GET(TXBD_HW_IDX_MASK, cur_idx);
		if (cur_rp == bd_ring->wp)
			return;

		udelay(1);
	}

	if (!drop)
		rtw89_info(rtwdev, "timed out to flush pci txch: %d\n", txch);
}

static void __rtw89_pci_ops_flush_txchs(struct rtw89_dev *rtwdev, u32 txchs,
					bool drop)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u8 i;

	for (i = 0; i < RTW89_TXCH_NUM; i++) {
		/* It may be unnecessary to flush FWCMD queue. */
		if (i == RTW89_TXCH_CH12)
			continue;
		if (info->tx_dma_ch_mask & BIT(i))
			continue;

		if (txchs & BIT(i))
			__pci_flush_txch(rtwdev, i, drop);
	}
}

static void rtw89_pci_ops_flush_queues(struct rtw89_dev *rtwdev, u32 queues,
				       bool drop)
{
	__rtw89_pci_ops_flush_txchs(rtwdev, BIT(RTW89_TXCH_NUM) - 1, drop);
}

u32 rtw89_pci_fill_txaddr_info(struct rtw89_dev *rtwdev,
			       void *txaddr_info_addr, u32 total_len,
			       dma_addr_t dma, u8 *add_info_nr)
{
	struct rtw89_pci_tx_addr_info_32 *txaddr_info = txaddr_info_addr;
	__le16 option;

	txaddr_info->length = cpu_to_le16(total_len);
	option = cpu_to_le16(RTW89_PCI_ADDR_MSDU_LS | RTW89_PCI_ADDR_NUM(1));
	option |= le16_encode_bits(upper_32_bits(dma), RTW89_PCI_ADDR_HIGH_MASK);
	txaddr_info->option = option;
	txaddr_info->dma = cpu_to_le32(dma);

	*add_info_nr = 1;

	return sizeof(*txaddr_info);
}
EXPORT_SYMBOL(rtw89_pci_fill_txaddr_info);

u32 rtw89_pci_fill_txaddr_info_v1(struct rtw89_dev *rtwdev,
				  void *txaddr_info_addr, u32 total_len,
				  dma_addr_t dma, u8 *add_info_nr)
{
	struct rtw89_pci_tx_addr_info_32_v1 *txaddr_info = txaddr_info_addr;
	u32 remain = total_len;
	u32 len;
	u16 length_option;
	int n;

	for (n = 0; n < RTW89_TXADDR_INFO_NR_V1 && remain; n++) {
		len = remain >= TXADDR_INFO_LENTHG_V1_MAX ?
		      TXADDR_INFO_LENTHG_V1_MAX : remain;
		remain -= len;

		length_option = FIELD_PREP(B_PCIADDR_LEN_V1_MASK, len) |
				FIELD_PREP(B_PCIADDR_HIGH_SEL_V1_MASK, 0) |
				FIELD_PREP(B_PCIADDR_LS_V1_MASK, remain == 0);
		length_option |= u16_encode_bits(upper_32_bits(dma),
						 B_PCIADDR_HIGH_SEL_V1_MASK);
		txaddr_info->length_opt = cpu_to_le16(length_option);
		txaddr_info->dma_low_lsb = cpu_to_le16(FIELD_GET(GENMASK(15, 0), dma));
		txaddr_info->dma_low_msb = cpu_to_le16(FIELD_GET(GENMASK(31, 16), dma));

		dma += len;
		txaddr_info++;
	}

	WARN_ONCE(remain, "length overflow remain=%u total_len=%u",
		  remain, total_len);

	*add_info_nr = n;

	return n * sizeof(*txaddr_info);
}
EXPORT_SYMBOL(rtw89_pci_fill_txaddr_info_v1);

static int rtw89_pci_txwd_submit(struct rtw89_dev *rtwdev,
				 struct rtw89_pci_tx_ring *tx_ring,
				 struct rtw89_pci_tx_wd *txwd,
				 struct rtw89_core_tx_request *tx_req)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_chip_info *chip = rtwdev->chip;
	struct rtw89_tx_desc_info *desc_info = &tx_req->desc_info;
	struct rtw89_pci_tx_wp_info *txwp_info;
	void *txaddr_info_addr;
	struct pci_dev *pdev = rtwpci->pdev;
	struct sk_buff *skb = tx_req->skb;
	struct rtw89_pci_tx_data *tx_data = RTW89_PCI_TX_SKB_CB(skb);
	struct rtw89_tx_skb_data *skb_data = RTW89_TX_SKB_CB(skb);
	bool en_wd_info = desc_info->en_wd_info;
	u32 txwd_len;
	u32 txwp_len;
	u32 txaddr_info_len;
	dma_addr_t dma;
	int ret;

	dma = dma_map_single(&pdev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, dma)) {
		rtw89_err(rtwdev, "failed to map skb dma data\n");
		ret = -EBUSY;
		goto err;
	}

	tx_data->dma = dma;
	rcu_assign_pointer(skb_data->wait, NULL);

	txwp_len = sizeof(*txwp_info);
	txwd_len = chip->txwd_body_size;
	txwd_len += en_wd_info ? chip->txwd_info_size : 0;

#if defined(__linux__)
	txwp_info = txwd->vaddr + txwd_len;
#elif defined(__FreeBSD__)
	txwp_info = (struct rtw89_pci_tx_wp_info *)((u8 *)txwd->vaddr + txwd_len);
#endif
	txwp_info->seq0 = cpu_to_le16(txwd->seq | RTW89_PCI_TXWP_VALID);
	txwp_info->seq1 = 0;
	txwp_info->seq2 = 0;
	txwp_info->seq3 = 0;

	tx_ring->tx_cnt++;
#if defined(__linux__)
	txaddr_info_addr = txwd->vaddr + txwd_len + txwp_len;
#elif defined(__FreeBSD__)
	txaddr_info_addr = (u8 *)txwd->vaddr + txwd_len + txwp_len;
#endif
	txaddr_info_len =
		rtw89_chip_fill_txaddr_info(rtwdev, txaddr_info_addr, skb->len,
					    dma, &desc_info->addr_info_nr);

	txwd->len = txwd_len + txwp_len + txaddr_info_len;

	rtw89_chip_fill_txdesc(rtwdev, desc_info, txwd->vaddr);

	skb_queue_tail(&txwd->queue, skb);

	return 0;

err:
	return ret;
}

static int rtw89_pci_fwcmd_submit(struct rtw89_dev *rtwdev,
				  struct rtw89_pci_tx_ring *tx_ring,
				  struct rtw89_pci_tx_bd_32 *txbd,
				  struct rtw89_core_tx_request *tx_req)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_chip_info *chip = rtwdev->chip;
	struct rtw89_tx_desc_info *desc_info = &tx_req->desc_info;
	void *txdesc;
	int txdesc_size = chip->h2c_desc_size;
	struct pci_dev *pdev = rtwpci->pdev;
	struct sk_buff *skb = tx_req->skb;
	struct rtw89_pci_tx_data *tx_data = RTW89_PCI_TX_SKB_CB(skb);
	dma_addr_t dma;
	__le16 opt;

	txdesc = skb_push(skb, txdesc_size);
	memset(txdesc, 0, txdesc_size);
	rtw89_chip_fill_txdesc_fwcmd(rtwdev, desc_info, txdesc);

	dma = dma_map_single(&pdev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, dma)) {
		rtw89_err(rtwdev, "failed to map fwcmd dma data\n");
		return -EBUSY;
	}

	tx_data->dma = dma;
	opt = cpu_to_le16(RTW89_PCI_TXBD_OPT_LS);
	opt |= le16_encode_bits(upper_32_bits(dma), RTW89_PCI_TXBD_OPT_DMA_HI);
	txbd->opt = opt;
	txbd->length = cpu_to_le16(skb->len);
	txbd->dma = cpu_to_le32(tx_data->dma);
	skb_queue_tail(&rtwpci->h2c_queue, skb);

	rtw89_pci_tx_bd_ring_update(rtwdev, tx_ring, 1);

	return 0;
}

static int rtw89_pci_txbd_submit(struct rtw89_dev *rtwdev,
				 struct rtw89_pci_tx_ring *tx_ring,
				 struct rtw89_pci_tx_bd_32 *txbd,
				 struct rtw89_core_tx_request *tx_req)
{
	struct rtw89_pci_tx_wd *txwd;
	__le16 opt;
	int ret;

	/* FWCMD queue doesn't have wd pages. Instead, it submits the CMD
	 * buffer with WD BODY only. So here we don't need to check the free
	 * pages of the wd ring.
	 */
	if (tx_ring->txch == RTW89_TXCH_CH12)
		return rtw89_pci_fwcmd_submit(rtwdev, tx_ring, txbd, tx_req);

	txwd = rtw89_pci_dequeue_txwd(tx_ring);
	if (!txwd) {
		rtw89_err(rtwdev, "no available TXWD\n");
		ret = -ENOSPC;
		goto err;
	}

	ret = rtw89_pci_txwd_submit(rtwdev, tx_ring, txwd, tx_req);
	if (ret) {
		rtw89_err(rtwdev, "failed to submit TXWD %d\n", txwd->seq);
		goto err_enqueue_wd;
	}

	list_add_tail(&txwd->list, &tx_ring->busy_pages);

	opt = cpu_to_le16(RTW89_PCI_TXBD_OPT_LS);
	opt |= le16_encode_bits(upper_32_bits(txwd->paddr), RTW89_PCI_TXBD_OPT_DMA_HI);
	txbd->opt = opt;
	txbd->length = cpu_to_le16(txwd->len);
	txbd->dma = cpu_to_le32(txwd->paddr);

	rtw89_pci_tx_bd_ring_update(rtwdev, tx_ring, 1);

	return 0;

err_enqueue_wd:
	rtw89_pci_enqueue_txwd(tx_ring, txwd);
err:
	return ret;
}

static int rtw89_pci_tx_write(struct rtw89_dev *rtwdev, struct rtw89_core_tx_request *tx_req,
			      u8 txch)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_tx_ring *tx_ring;
	struct rtw89_pci_tx_bd_32 *txbd;
	u32 n_avail_txbd;
	int ret = 0;

	/* check the tx type and dma channel for fw cmd queue */
	if ((txch == RTW89_TXCH_CH12 ||
	     tx_req->tx_type == RTW89_CORE_TX_TYPE_FWCMD) &&
	    (txch != RTW89_TXCH_CH12 ||
	     tx_req->tx_type != RTW89_CORE_TX_TYPE_FWCMD)) {
		rtw89_err(rtwdev, "only fw cmd uses dma channel 12\n");
		return -EINVAL;
	}

	tx_ring = &rtwpci->tx_rings[txch];
	spin_lock_bh(&rtwpci->trx_lock);

	n_avail_txbd = rtw89_pci_get_avail_txbd_num(tx_ring);
	if (n_avail_txbd == 0) {
		rtw89_err(rtwdev, "no available TXBD\n");
		ret = -ENOSPC;
		goto err_unlock;
	}

	txbd = rtw89_pci_get_next_txbd(tx_ring);
	ret = rtw89_pci_txbd_submit(rtwdev, tx_ring, txbd, tx_req);
	if (ret) {
		rtw89_err(rtwdev, "failed to submit TXBD\n");
		goto err_unlock;
	}

	spin_unlock_bh(&rtwpci->trx_lock);
	return 0;

err_unlock:
	spin_unlock_bh(&rtwpci->trx_lock);
	return ret;
}

static int rtw89_pci_ops_tx_write(struct rtw89_dev *rtwdev, struct rtw89_core_tx_request *tx_req)
{
	struct rtw89_tx_desc_info *desc_info = &tx_req->desc_info;
	int ret;

	ret = rtw89_pci_tx_write(rtwdev, tx_req, desc_info->ch_dma);
	if (ret) {
		rtw89_err(rtwdev, "failed to TX Queue %d\n", desc_info->ch_dma);
		return ret;
	}

	return 0;
}

const struct rtw89_pci_bd_ram rtw89_bd_ram_table_dual[RTW89_TXCH_NUM] = {
	[RTW89_TXCH_ACH0] = {.start_idx = 0,  .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH1] = {.start_idx = 5,  .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH2] = {.start_idx = 10, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH3] = {.start_idx = 15, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH4] = {.start_idx = 20, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH5] = {.start_idx = 25, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH6] = {.start_idx = 30, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH7] = {.start_idx = 35, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_CH8]  = {.start_idx = 40, .max_num = 5, .min_num = 1},
	[RTW89_TXCH_CH9]  = {.start_idx = 45, .max_num = 5, .min_num = 1},
	[RTW89_TXCH_CH10] = {.start_idx = 50, .max_num = 5, .min_num = 1},
	[RTW89_TXCH_CH11] = {.start_idx = 55, .max_num = 5, .min_num = 1},
	[RTW89_TXCH_CH12] = {.start_idx = 60, .max_num = 4, .min_num = 1},
};
EXPORT_SYMBOL(rtw89_bd_ram_table_dual);

const struct rtw89_pci_bd_ram rtw89_bd_ram_table_single[RTW89_TXCH_NUM] = {
	[RTW89_TXCH_ACH0] = {.start_idx = 0,  .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH1] = {.start_idx = 5,  .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH2] = {.start_idx = 10, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_ACH3] = {.start_idx = 15, .max_num = 5, .min_num = 2},
	[RTW89_TXCH_CH8]  = {.start_idx = 20, .max_num = 4, .min_num = 1},
	[RTW89_TXCH_CH9]  = {.start_idx = 24, .max_num = 4, .min_num = 1},
	[RTW89_TXCH_CH12] = {.start_idx = 28, .max_num = 4, .min_num = 1},
};
EXPORT_SYMBOL(rtw89_bd_ram_table_single);

static void rtw89_pci_init_wp_16sel(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 addr = info->wp_sel_addr;
	u32 val;
	int i;

	if (!info->wp_sel_addr)
		return;

	for (i = 0; i < 16; i += 4) {
		val = u32_encode_bits(i + 0, MASKBYTE0) |
		      u32_encode_bits(i + 1, MASKBYTE1) |
		      u32_encode_bits(i + 2, MASKBYTE2) |
		      u32_encode_bits(i + 3, MASKBYTE3);
		rtw89_write32(rtwdev, addr + i, val);
	}
}

static void rtw89_pci_reset_trx_rings(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_bd_ram *bd_ram_table = *info->bd_ram_table;
	struct rtw89_pci_tx_ring *tx_ring;
	struct rtw89_pci_rx_ring *rx_ring;
	struct rtw89_pci_dma_ring *bd_ring;
	const struct rtw89_pci_bd_ram *bd_ram;
	u32 addr_num;
	u32 addr_idx;
	u32 addr_bdram;
	u32 addr_desa_l;
	u32 val32;
	int i;

	for (i = 0; i < RTW89_TXCH_NUM; i++) {
		if (info->tx_dma_ch_mask & BIT(i))
			continue;

		tx_ring = &rtwpci->tx_rings[i];
		bd_ring = &tx_ring->bd_ring;
		bd_ram = bd_ram_table ? &bd_ram_table[i] : NULL;
		addr_num = bd_ring->addr.num;
		addr_bdram = bd_ring->addr.bdram;
		addr_desa_l = bd_ring->addr.desa_l;
		bd_ring->wp = 0;
		bd_ring->rp = 0;

		rtw89_write16(rtwdev, addr_num, bd_ring->len);
		if (addr_bdram && bd_ram) {
			val32 = FIELD_PREP(BDRAM_SIDX_MASK, bd_ram->start_idx) |
				FIELD_PREP(BDRAM_MAX_MASK, bd_ram->max_num) |
				FIELD_PREP(BDRAM_MIN_MASK, bd_ram->min_num);

			rtw89_write32(rtwdev, addr_bdram, val32);
		}
		rtw89_write32(rtwdev, addr_desa_l, bd_ring->dma);
		rtw89_write32(rtwdev, addr_desa_l + 4, upper_32_bits(bd_ring->dma));
	}

	for (i = 0; i < RTW89_RXCH_NUM; i++) {
		rx_ring = &rtwpci->rx_rings[i];
		bd_ring = &rx_ring->bd_ring;
		addr_num = bd_ring->addr.num;
		addr_idx = bd_ring->addr.idx;
		addr_desa_l = bd_ring->addr.desa_l;
		if (info->rx_ring_eq_is_full)
			bd_ring->wp = bd_ring->len - 1;
		else
			bd_ring->wp = 0;
		bd_ring->rp = 0;
		rx_ring->diliver_skb = NULL;
		rx_ring->diliver_desc.ready = false;
		rx_ring->target_rx_tag = 0;

		rtw89_write16(rtwdev, addr_num, bd_ring->len);
		rtw89_write32(rtwdev, addr_desa_l, bd_ring->dma);
		rtw89_write32(rtwdev, addr_desa_l + 4, upper_32_bits(bd_ring->dma));

		if (info->rx_ring_eq_is_full)
			rtw89_write16(rtwdev, addr_idx, bd_ring->wp);
	}

	rtw89_pci_init_wp_16sel(rtwdev);
}

static void rtw89_pci_release_tx_ring(struct rtw89_dev *rtwdev,
				      struct rtw89_pci_tx_ring *tx_ring)
{
	rtw89_pci_release_busy_txwd(rtwdev, tx_ring);
	rtw89_pci_release_pending_txwd_skb(rtwdev, tx_ring);
}

void rtw89_pci_ops_reset(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	int txch;

	rtw89_pci_reset_trx_rings(rtwdev);

	spin_lock_bh(&rtwpci->trx_lock);
	for (txch = 0; txch < RTW89_TXCH_NUM; txch++) {
		if (info->tx_dma_ch_mask & BIT(txch))
			continue;
		if (txch == RTW89_TXCH_CH12) {
			rtw89_pci_release_fwcmd(rtwdev, rtwpci,
						skb_queue_len(&rtwpci->h2c_queue), true);
			continue;
		}
		rtw89_pci_release_tx_ring(rtwdev, &rtwpci->tx_rings[txch]);
	}
	spin_unlock_bh(&rtwpci->trx_lock);
}

static void rtw89_pci_enable_intr_lock(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&rtwpci->irq_lock, flags);
	rtwpci->running = true;
	rtw89_chip_enable_intr(rtwdev, rtwpci);
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);
}

static void rtw89_pci_disable_intr_lock(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&rtwpci->irq_lock, flags);
	rtwpci->running = false;
	rtw89_chip_disable_intr(rtwdev, rtwpci);
	spin_unlock_irqrestore(&rtwpci->irq_lock, flags);
}

static int rtw89_pci_ops_start(struct rtw89_dev *rtwdev)
{
	rtw89_core_napi_start(rtwdev);
	rtw89_pci_enable_intr_lock(rtwdev);

	return 0;
}

static void rtw89_pci_ops_stop(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;

	rtw89_pci_disable_intr_lock(rtwdev);
	synchronize_irq(pdev->irq);
	rtw89_core_napi_stop(rtwdev);
}

static void rtw89_pci_ops_pause(struct rtw89_dev *rtwdev, bool pause)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;

	if (pause) {
		rtw89_pci_disable_intr_lock(rtwdev);
		synchronize_irq(pdev->irq);
		if (test_bit(RTW89_FLAG_NAPI_RUNNING, rtwdev->flags))
			napi_synchronize(&rtwdev->napi);
	} else {
		rtw89_pci_enable_intr_lock(rtwdev);
		rtw89_pci_tx_kick_off_pending(rtwdev);
	}
}

static
void rtw89_pci_switch_bd_idx_addr(struct rtw89_dev *rtwdev, bool low_power)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_bd_idx_addr *bd_idx_addr = info->bd_idx_addr_low_power;
	const struct rtw89_pci_ch_dma_addr_set *dma_addr_set = info->dma_addr_set;
	struct rtw89_pci_tx_ring *tx_ring;
	struct rtw89_pci_rx_ring *rx_ring;
	int i;

	if (WARN(!bd_idx_addr, "only HCI with low power mode needs this\n"))
		return;

	for (i = 0; i < RTW89_TXCH_NUM; i++) {
		tx_ring = &rtwpci->tx_rings[i];
		tx_ring->bd_ring.addr.idx = low_power ?
					    bd_idx_addr->tx_bd_addrs[i] :
					    dma_addr_set->tx[i].idx;
	}

	for (i = 0; i < RTW89_RXCH_NUM; i++) {
		rx_ring = &rtwpci->rx_rings[i];
		rx_ring->bd_ring.addr.idx = low_power ?
					    bd_idx_addr->rx_bd_addrs[i] :
					    dma_addr_set->rx[i].idx;
	}
}

static void rtw89_pci_ops_switch_mode(struct rtw89_dev *rtwdev, bool low_power)
{
	enum rtw89_pci_intr_mask_cfg cfg;

	WARN(!rtwdev->hci.paused, "HCI isn't paused\n");

	cfg = low_power ? RTW89_PCI_INTR_MASK_LOW_POWER : RTW89_PCI_INTR_MASK_NORMAL;
	rtw89_chip_config_intr_mask(rtwdev, cfg);
	rtw89_pci_switch_bd_idx_addr(rtwdev, low_power);
}

static void rtw89_pci_ops_write32(struct rtw89_dev *rtwdev, u32 addr, u32 data);

static u32 rtw89_pci_ops_read32_cmac(struct rtw89_dev *rtwdev, u32 addr)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
#if defined(__linux__)
	u32 val = readl(rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	u32 val;

	val = bus_read_4((struct resource *)rtwpci->mmap, addr);
	rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "R32 (%#010x) -> %#010x\n", addr, val);
#endif
	int count;

	for (count = 0; ; count++) {
		if (val != RTW89_R32_DEAD)
			return val;
		if (count >= MAC_REG_POOL_COUNT) {
			rtw89_warn(rtwdev, "addr %#x = %#x\n", addr, val);
			return RTW89_R32_DEAD;
		}
		rtw89_pci_ops_write32(rtwdev, R_AX_CK_EN, B_AX_CMAC_ALLCKEN);
#if defined(__linux__)
		val = readl(rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
		val = bus_read_4((struct resource *)rtwpci->mmap, addr);
		rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "R32 (%#010x) -> %#010x\n", addr, val);
#endif
	}

	return val;
}

static u8 rtw89_pci_ops_read8(struct rtw89_dev *rtwdev, u32 addr)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	u32 addr32, val32, shift;

	if (!ACCESS_CMAC(addr))
#if defined(__linux__)
		return readb(rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	{
		u8 val;

		val = bus_read_1((struct resource *)rtwpci->mmap, addr);
		rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "R08 (%#010x) -> %#04x\n", addr, val);
		return (val);
	}
#endif

	addr32 = addr & ~0x3;
	shift = (addr & 0x3) * 8;
	val32 = rtw89_pci_ops_read32_cmac(rtwdev, addr32);
	return val32 >> shift;
}

static u16 rtw89_pci_ops_read16(struct rtw89_dev *rtwdev, u32 addr)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	u32 addr32, val32, shift;

	if (!ACCESS_CMAC(addr))
#if defined(__linux__)
		return readw(rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	{
		u16 val;

		val = bus_read_2((struct resource *)rtwpci->mmap, addr);
		rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "R16 (%#010x) -> %#06x\n", addr, val);
		return (val);
	}
#endif

	addr32 = addr & ~0x3;
	shift = (addr & 0x3) * 8;
	val32 = rtw89_pci_ops_read32_cmac(rtwdev, addr32);
	return val32 >> shift;
}

static u32 rtw89_pci_ops_read32(struct rtw89_dev *rtwdev, u32 addr)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	if (!ACCESS_CMAC(addr))
#if defined(__linux__)
		return readl(rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	{
		u32 val;

		val = bus_read_4((struct resource *)rtwpci->mmap, addr);
		rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "R32 (%#010x) -> %#010x\n", addr, val);
		return (val);
	}
#endif

	return rtw89_pci_ops_read32_cmac(rtwdev, addr);
}

static void rtw89_pci_ops_write8(struct rtw89_dev *rtwdev, u32 addr, u8 data)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

#if defined(__linux__)
	writeb(data, rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "W08 (%#010x) <- %#04x\n", addr, data);
	return (bus_write_1((struct resource *)rtwpci->mmap, addr, data));
#endif
}

static void rtw89_pci_ops_write16(struct rtw89_dev *rtwdev, u32 addr, u16 data)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

#if defined(__linux__)
	writew(data, rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "W16 (%#010x) <- %#06x\n", addr, data);
	return (bus_write_2((struct resource *)rtwpci->mmap, addr, data));
#endif
}

static void rtw89_pci_ops_write32(struct rtw89_dev *rtwdev, u32 addr, u32 data)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

#if defined(__linux__)
	writel(data, rtwpci->mmap + addr);
#elif defined(__FreeBSD__)
	rtw89_debug(rtwdev, RTW89_DBG_IO_RW, "W32 (%#010x) <- %#010x\n", addr, data);
	return (bus_write_4((struct resource *)rtwpci->mmap, addr, data));
#endif
}

static void rtw89_pci_ctrl_dma_trx(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;

	if (enable)
		rtw89_write32_set(rtwdev, info->init_cfg_reg,
				  info->rxhci_en_bit | info->txhci_en_bit);
	else
		rtw89_write32_clr(rtwdev, info->init_cfg_reg,
				  info->rxhci_en_bit | info->txhci_en_bit);
}

static void rtw89_pci_ctrl_dma_io(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_reg_def *reg = &info->dma_io_stop;

	if (enable)
		rtw89_write32_clr(rtwdev, reg->addr, reg->mask);
	else
		rtw89_write32_set(rtwdev, reg->addr, reg->mask);
}

void rtw89_pci_ctrl_dma_all(struct rtw89_dev *rtwdev, bool enable)
{
	rtw89_pci_ctrl_dma_io(rtwdev, enable);
	rtw89_pci_ctrl_dma_trx(rtwdev, enable);
}

static int rtw89_pci_check_mdio(struct rtw89_dev *rtwdev, u8 addr, u8 speed, u16 rw_bit)
{
	u16 val;

	rtw89_write8(rtwdev, R_AX_MDIO_CFG, addr & 0x1F);

	val = rtw89_read16(rtwdev, R_AX_MDIO_CFG);
	switch (speed) {
	case PCIE_PHY_GEN1:
		if (addr < 0x20)
			val = u16_replace_bits(val, MDIO_PG0_G1, B_AX_MDIO_PHY_ADDR_MASK);
		else
			val = u16_replace_bits(val, MDIO_PG1_G1, B_AX_MDIO_PHY_ADDR_MASK);
		break;
	case PCIE_PHY_GEN2:
		if (addr < 0x20)
			val = u16_replace_bits(val, MDIO_PG0_G2, B_AX_MDIO_PHY_ADDR_MASK);
		else
			val = u16_replace_bits(val, MDIO_PG1_G2, B_AX_MDIO_PHY_ADDR_MASK);
		break;
	default:
		rtw89_err(rtwdev, "[ERR]Error Speed %d!\n", speed);
		return -EINVAL;
	}
	rtw89_write16(rtwdev, R_AX_MDIO_CFG, val);
	rtw89_write16_set(rtwdev, R_AX_MDIO_CFG, rw_bit);

	return read_poll_timeout(rtw89_read16, val, !(val & rw_bit), 10, 2000,
				 false, rtwdev, R_AX_MDIO_CFG);
}

static int
rtw89_read16_mdio(struct rtw89_dev *rtwdev, u8 addr, u8 speed, u16 *val)
{
	int ret;

	ret = rtw89_pci_check_mdio(rtwdev, addr, speed, B_AX_MDIO_RFLAG);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]MDIO R16 0x%X fail ret=%d!\n", addr, ret);
		return ret;
	}
	*val = rtw89_read16(rtwdev, R_AX_MDIO_RDATA);

	return 0;
}

static int
rtw89_write16_mdio(struct rtw89_dev *rtwdev, u8 addr, u16 data, u8 speed)
{
	int ret;

	rtw89_write16(rtwdev, R_AX_MDIO_WDATA, data);
	ret = rtw89_pci_check_mdio(rtwdev, addr, speed, B_AX_MDIO_WFLAG);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]MDIO W16 0x%X = %x fail ret=%d!\n", addr, data, ret);
		return ret;
	}

	return 0;
}

static int
rtw89_write16_mdio_mask(struct rtw89_dev *rtwdev, u8 addr, u16 mask, u16 data, u8 speed)
{
	u32 shift;
	int ret;
	u16 val;

	ret = rtw89_read16_mdio(rtwdev, addr, speed, &val);
	if (ret)
		return ret;

	shift = __ffs(mask);
	val &= ~mask;
	val |= ((data << shift) & mask);

	ret = rtw89_write16_mdio(rtwdev, addr, val, speed);
	if (ret)
		return ret;

	return 0;
}

static int rtw89_write16_mdio_set(struct rtw89_dev *rtwdev, u8 addr, u16 mask, u8 speed)
{
	int ret;
	u16 val;

	ret = rtw89_read16_mdio(rtwdev, addr, speed, &val);
	if (ret)
		return ret;
	ret = rtw89_write16_mdio(rtwdev, addr, val | mask, speed);
	if (ret)
		return ret;

	return 0;
}

static int rtw89_write16_mdio_clr(struct rtw89_dev *rtwdev, u8 addr, u16 mask, u8 speed)
{
	int ret;
	u16 val;

	ret = rtw89_read16_mdio(rtwdev, addr, speed, &val);
	if (ret)
		return ret;
	ret = rtw89_write16_mdio(rtwdev, addr, val & ~mask, speed);
	if (ret)
		return ret;

	return 0;
}

static int rtw89_dbi_write8(struct rtw89_dev *rtwdev, u16 addr, u8 data)
{
	u16 addr_2lsb = addr & B_AX_DBI_2LSB;
	u16 write_addr;
	u8 flag;
	int ret;

	write_addr = addr & B_AX_DBI_ADDR_MSK;
	write_addr |= u16_encode_bits(BIT(addr_2lsb), B_AX_DBI_WREN_MSK);
	rtw89_write8(rtwdev, R_AX_DBI_WDATA + addr_2lsb, data);
	rtw89_write16(rtwdev, R_AX_DBI_FLAG, write_addr);
	rtw89_write8(rtwdev, R_AX_DBI_FLAG + 2, B_AX_DBI_WFLAG >> 16);

	ret = read_poll_timeout_atomic(rtw89_read8, flag, !flag, 10,
				       10 * RTW89_PCI_WR_RETRY_CNT, false,
				       rtwdev, R_AX_DBI_FLAG + 2);
	if (ret)
		rtw89_err(rtwdev, "failed to write DBI register, addr=0x%X\n",
			  addr);

	return ret;
}

static int rtw89_dbi_read8(struct rtw89_dev *rtwdev, u16 addr, u8 *value)
{
	u16 read_addr = addr & B_AX_DBI_ADDR_MSK;
	u8 flag;
	int ret;

	rtw89_write16(rtwdev, R_AX_DBI_FLAG, read_addr);
	rtw89_write8(rtwdev, R_AX_DBI_FLAG + 2, B_AX_DBI_RFLAG >> 16);

	ret = read_poll_timeout_atomic(rtw89_read8, flag, !flag, 10,
				       10 * RTW89_PCI_WR_RETRY_CNT, false,
				       rtwdev, R_AX_DBI_FLAG + 2);
	if (ret) {
		rtw89_err(rtwdev, "failed to read DBI register, addr=0x%X\n",
			  addr);
		return ret;
	}

	read_addr = R_AX_DBI_RDATA + (addr & 3);
	*value = rtw89_read8(rtwdev, read_addr);

	return 0;
}

static int rtw89_pci_write_config_byte(struct rtw89_dev *rtwdev, u16 addr,
				       u8 data)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	struct pci_dev *pdev = rtwpci->pdev;
	int ret;

	ret = pci_write_config_byte(pdev, addr, data);
	if (!ret)
		return 0;

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev))
		ret = rtw89_dbi_write8(rtwdev, addr, data);

	return ret;
}

static int rtw89_pci_read_config_byte(struct rtw89_dev *rtwdev, u16 addr,
				      u8 *value)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	struct pci_dev *pdev = rtwpci->pdev;
	int ret;

	ret = pci_read_config_byte(pdev, addr, value);
	if (!ret)
		return 0;

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev))
		ret = rtw89_dbi_read8(rtwdev, addr, value);

	return ret;
}

static int rtw89_pci_config_byte_set(struct rtw89_dev *rtwdev, u16 addr,
				     u8 bit)
{
	u8 value;
	int ret;

	ret = rtw89_pci_read_config_byte(rtwdev, addr, &value);
	if (ret)
		return ret;

	value |= bit;
	ret = rtw89_pci_write_config_byte(rtwdev, addr, value);

	return ret;
}

static int rtw89_pci_config_byte_clr(struct rtw89_dev *rtwdev, u16 addr,
				     u8 bit)
{
	u8 value;
	int ret;

	ret = rtw89_pci_read_config_byte(rtwdev, addr, &value);
	if (ret)
		return ret;

	value &= ~bit;
	ret = rtw89_pci_write_config_byte(rtwdev, addr, value);

	return ret;
}

static int
__get_target(struct rtw89_dev *rtwdev, u16 *target, enum rtw89_pcie_phy phy_rate)
{
	u16 val, tar;
	int ret;

	/* Enable counter */
	ret = rtw89_read16_mdio(rtwdev, RAC_CTRL_PPR_V1, phy_rate, &val);
	if (ret)
		return ret;
	ret = rtw89_write16_mdio(rtwdev, RAC_CTRL_PPR_V1, val & ~B_AX_CLK_CALIB_EN,
				 phy_rate);
	if (ret)
		return ret;
	ret = rtw89_write16_mdio(rtwdev, RAC_CTRL_PPR_V1, val | B_AX_CLK_CALIB_EN,
				 phy_rate);
	if (ret)
		return ret;

	fsleep(300);

	ret = rtw89_read16_mdio(rtwdev, RAC_CTRL_PPR_V1, phy_rate, &tar);
	if (ret)
		return ret;
	ret = rtw89_write16_mdio(rtwdev, RAC_CTRL_PPR_V1, val & ~B_AX_CLK_CALIB_EN,
				 phy_rate);
	if (ret)
		return ret;

	tar = tar & 0x0FFF;
	if (tar == 0 || tar == 0x0FFF) {
		rtw89_err(rtwdev, "[ERR]Get target failed.\n");
		return -EINVAL;
	}

	*target = tar;

	return 0;
}

static int rtw89_pci_autok_x(struct rtw89_dev *rtwdev)
{
	int ret;

	if (!rtw89_is_rtl885xb(rtwdev))
		return 0;

	ret = rtw89_write16_mdio_mask(rtwdev, RAC_REG_FLD_0, BAC_AUTOK_N_MASK,
				      PCIE_AUTOK_4, PCIE_PHY_GEN1);
	return ret;
}

static int rtw89_pci_auto_refclk_cal(struct rtw89_dev *rtwdev, bool autook_en)
{
	enum rtw89_pcie_phy phy_rate;
	u16 val16, mgn_set, div_set, tar;
	u8 val8, bdr_ori;
	bool l1_flag = false;
	int ret = 0;

	if (!rtw89_is_rtl885xb(rtwdev))
		return 0;

	ret = rtw89_pci_read_config_byte(rtwdev, RTW89_PCIE_PHY_RATE, &val8);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]pci config read %X\n",
			  RTW89_PCIE_PHY_RATE);
		return ret;
	}

	if (FIELD_GET(RTW89_PCIE_PHY_RATE_MASK, val8) == 0x1) {
		phy_rate = PCIE_PHY_GEN1;
	} else if (FIELD_GET(RTW89_PCIE_PHY_RATE_MASK, val8) == 0x2) {
		phy_rate = PCIE_PHY_GEN2;
	} else {
		rtw89_err(rtwdev, "[ERR]PCIe PHY rate %#x not support\n", val8);
		return -EOPNOTSUPP;
	}
	/* Disable L1BD */
	ret = rtw89_pci_read_config_byte(rtwdev, RTW89_PCIE_L1_CTRL, &bdr_ori);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]pci config read %X\n", RTW89_PCIE_L1_CTRL);
		return ret;
	}

	if (bdr_ori & RTW89_PCIE_BIT_L1) {
		ret = rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_L1_CTRL,
						  bdr_ori & ~RTW89_PCIE_BIT_L1);
		if (ret) {
			rtw89_err(rtwdev, "[ERR]pci config write %X\n",
				  RTW89_PCIE_L1_CTRL);
			return ret;
		}
		l1_flag = true;
	}

	ret = rtw89_read16_mdio(rtwdev, RAC_CTRL_PPR_V1, phy_rate, &val16);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]mdio_r16_pcie %X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	if (val16 & B_AX_CALIB_EN) {
		ret = rtw89_write16_mdio(rtwdev, RAC_CTRL_PPR_V1,
					 val16 & ~B_AX_CALIB_EN, phy_rate);
		if (ret) {
			rtw89_err(rtwdev, "[ERR]mdio_w16_pcie %X\n", RAC_CTRL_PPR_V1);
			goto end;
		}
	}

	if (!autook_en)
		goto end;
	/* Set div */
	ret = rtw89_write16_mdio_clr(rtwdev, RAC_CTRL_PPR_V1, B_AX_DIV, phy_rate);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]mdio_w16_pcie %X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	/* Obtain div and margin */
	ret = __get_target(rtwdev, &tar, phy_rate);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]1st get target fail %d\n", ret);
		goto end;
	}

	mgn_set = tar * INTF_INTGRA_HOSTREF_V1 / INTF_INTGRA_MINREF_V1 - tar;

	if (mgn_set >= 128) {
		div_set = 0x0003;
		mgn_set = 0x000F;
	} else if (mgn_set >= 64) {
		div_set = 0x0003;
		mgn_set >>= 3;
	} else if (mgn_set >= 32) {
		div_set = 0x0002;
		mgn_set >>= 2;
	} else if (mgn_set >= 16) {
		div_set = 0x0001;
		mgn_set >>= 1;
	} else if (mgn_set == 0) {
		rtw89_err(rtwdev, "[ERR]cal mgn is 0,tar = %d\n", tar);
		goto end;
	} else {
		div_set = 0x0000;
	}

	ret = rtw89_read16_mdio(rtwdev, RAC_CTRL_PPR_V1, phy_rate, &val16);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]mdio_r16_pcie %X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	val16 |= u16_encode_bits(div_set, B_AX_DIV);

	ret = rtw89_write16_mdio(rtwdev, RAC_CTRL_PPR_V1, val16, phy_rate);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]mdio_w16_pcie %X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	ret = __get_target(rtwdev, &tar, phy_rate);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]2nd get target fail %d\n", ret);
		goto end;
	}

	rtw89_debug(rtwdev, RTW89_DBG_HCI, "[TRACE]target = 0x%X, div = 0x%X, margin = 0x%X\n",
		    tar, div_set, mgn_set);
	ret = rtw89_write16_mdio(rtwdev, RAC_SET_PPR_V1,
				 (tar & 0x0FFF) | (mgn_set << 12), phy_rate);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]mdio_w16_pcie %X\n", RAC_SET_PPR_V1);
		goto end;
	}

	/* Enable function */
	ret = rtw89_write16_mdio_set(rtwdev, RAC_CTRL_PPR_V1, B_AX_CALIB_EN, phy_rate);
	if (ret) {
		rtw89_err(rtwdev, "[ERR]mdio_w16_pcie %X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	/* CLK delay = 0 */
	ret = rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_CLK_CTRL,
					  PCIE_CLKDLY_HW_0);

end:
	/* Set L1BD to ori */
	if (l1_flag) {
		ret = rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_L1_CTRL,
						  bdr_ori);
		if (ret) {
			rtw89_err(rtwdev, "[ERR]pci config write %X\n",
				  RTW89_PCIE_L1_CTRL);
			return ret;
		}
	}

	return ret;
}

static int rtw89_pci_deglitch_setting(struct rtw89_dev *rtwdev)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	int ret;

	if (chip_id == RTL8852A) {
		ret = rtw89_write16_mdio_clr(rtwdev, RAC_ANA24, B_AX_DEGLITCH,
					     PCIE_PHY_GEN1);
		if (ret)
			return ret;
		ret = rtw89_write16_mdio_clr(rtwdev, RAC_ANA24, B_AX_DEGLITCH,
					     PCIE_PHY_GEN2);
		if (ret)
			return ret;
	} else if (chip_id == RTL8852C) {
		rtw89_write16_clr(rtwdev, R_RAC_DIRECT_OFFSET_G1 + RAC_ANA24 * 2,
				  B_AX_DEGLITCH);
		rtw89_write16_clr(rtwdev, R_RAC_DIRECT_OFFSET_G2 + RAC_ANA24 * 2,
				  B_AX_DEGLITCH);
	}

	return 0;
}

static void rtw89_pci_disable_eq_ax(struct rtw89_dev *rtwdev)
{
	u16 g1_oobs, g2_oobs;
	u32 backup_aspm;
	u32 phy_offset;
	u16 offset_cal;
	u16 oobs_val;
	int ret;
	u8 gen;

	if (rtwdev->chip->chip_id != RTL8852C)
		return;

	g1_oobs = rtw89_read16_mask(rtwdev, R_RAC_DIRECT_OFFSET_G1 +
					    RAC_ANA09 * RAC_MULT, BAC_OOBS_SEL);
	g2_oobs = rtw89_read16_mask(rtwdev, R_RAC_DIRECT_OFFSET_G2 +
					    RAC_ANA09 * RAC_MULT, BAC_OOBS_SEL);
	if (g1_oobs && g2_oobs)
		return;

	backup_aspm = rtw89_read32(rtwdev, R_AX_PCIE_MIX_CFG_V1);
	rtw89_write32_clr(rtwdev, R_AX_PCIE_MIX_CFG_V1, B_AX_ASPM_CTRL_MASK);

	ret = rtw89_pci_get_phy_offset_by_link_speed(rtwdev, &phy_offset);
	if (ret)
		goto out;

	rtw89_write16_set(rtwdev, phy_offset + RAC_ANA0D * RAC_MULT, BAC_RX_TEST_EN);
	rtw89_write16(rtwdev, phy_offset + RAC_ANA10 * RAC_MULT, ADDR_SEL_PINOUT_DIS_VAL);
	rtw89_write16_set(rtwdev, phy_offset + RAC_ANA19 * RAC_MULT, B_PCIE_BIT_RD_SEL);

	oobs_val = rtw89_read16_mask(rtwdev, phy_offset + RAC_ANA1F * RAC_MULT,
				     OOBS_LEVEL_MASK);

	rtw89_write16_mask(rtwdev, R_RAC_DIRECT_OFFSET_G1 + RAC_ANA03 * RAC_MULT,
			   OOBS_SEN_MASK, oobs_val);
	rtw89_write16_set(rtwdev, R_RAC_DIRECT_OFFSET_G1 + RAC_ANA09 * RAC_MULT,
			  BAC_OOBS_SEL);

	rtw89_write16_mask(rtwdev, R_RAC_DIRECT_OFFSET_G2 + RAC_ANA03 * RAC_MULT,
			   OOBS_SEN_MASK, oobs_val);
	rtw89_write16_set(rtwdev, R_RAC_DIRECT_OFFSET_G2 + RAC_ANA09 * RAC_MULT,
			  BAC_OOBS_SEL);

	/* offset K */
	for (gen = 1; gen <= 2; gen++) {
		phy_offset = gen == 1 ? R_RAC_DIRECT_OFFSET_G1 :
					R_RAC_DIRECT_OFFSET_G2;

		rtw89_write16_clr(rtwdev, phy_offset + RAC_ANA19 * RAC_MULT,
				  B_PCIE_BIT_RD_SEL);
	}

	offset_cal = rtw89_read16_mask(rtwdev, R_RAC_DIRECT_OFFSET_G1 +
					       RAC_ANA1F * RAC_MULT, OFFSET_CAL_MASK);

	for (gen = 1; gen <= 2; gen++) {
		phy_offset = gen == 1 ? R_RAC_DIRECT_OFFSET_G1 :
					R_RAC_DIRECT_OFFSET_G2;

		rtw89_write16_mask(rtwdev, phy_offset + RAC_ANA0B * RAC_MULT,
				   MANUAL_LVL_MASK, offset_cal);
		rtw89_write16_clr(rtwdev, phy_offset + RAC_ANA0D * RAC_MULT,
				  OFFSET_CAL_MODE);
	}

out:
	rtw89_write32(rtwdev, R_AX_PCIE_MIX_CFG_V1, backup_aspm);
}

static void rtw89_pci_ber(struct rtw89_dev *rtwdev)
{
	u32 phy_offset;

	if (!test_bit(RTW89_QUIRK_PCI_BER, rtwdev->quirks))
		return;

	phy_offset = R_RAC_DIRECT_OFFSET_G1;
	rtw89_write16(rtwdev, phy_offset + RAC_ANA1E * RAC_MULT, RAC_ANA1E_G1_VAL);
	rtw89_write16(rtwdev, phy_offset + RAC_ANA2E * RAC_MULT, RAC_ANA2E_VAL);

	phy_offset = R_RAC_DIRECT_OFFSET_G2;
	rtw89_write16(rtwdev, phy_offset + RAC_ANA1E * RAC_MULT, RAC_ANA1E_G2_VAL);
	rtw89_write16(rtwdev, phy_offset + RAC_ANA2E * RAC_MULT, RAC_ANA2E_VAL);
}

static void rtw89_pci_rxdma_prefth(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id != RTL8852A)
		return;

	rtw89_write32_set(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_DIS_RXDMA_PRE);
}

static void rtw89_pci_l1off_pwroff(struct rtw89_dev *rtwdev)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;

	if (chip_id != RTL8852A && !rtw89_is_rtl885xb(rtwdev))
		return;

	rtw89_write32_clr(rtwdev, R_AX_PCIE_PS_CTRL, B_AX_L1OFF_PWR_OFF_EN);
}

static u32 rtw89_pci_l2_rxen_lat(struct rtw89_dev *rtwdev)
{
	int ret;

	if (rtwdev->chip->chip_id != RTL8852A)
		return 0;

	ret = rtw89_write16_mdio_clr(rtwdev, RAC_ANA26, B_AX_RXEN,
				     PCIE_PHY_GEN1);
	if (ret)
		return ret;

	ret = rtw89_write16_mdio_clr(rtwdev, RAC_ANA26, B_AX_RXEN,
				     PCIE_PHY_GEN2);
	if (ret)
		return ret;

	return 0;
}

static void rtw89_pci_aphy_pwrcut(struct rtw89_dev *rtwdev)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;

	if (chip_id != RTL8852A && !rtw89_is_rtl885xb(rtwdev))
		return;

	rtw89_write32_clr(rtwdev, R_AX_SYS_PW_CTRL, B_AX_PSUS_OFF_CAPC_EN);
}

static void rtw89_pci_hci_ldo(struct rtw89_dev *rtwdev)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		rtw89_write32_set(rtwdev, R_AX_SYS_SDIO_CTRL,
				  B_AX_PCIE_DIS_L2_CTRL_LDO_HCI);
		rtw89_write32_clr(rtwdev, R_AX_SYS_SDIO_CTRL,
				  B_AX_PCIE_DIS_WLSUS_AFT_PDN);
	} else if (rtwdev->chip->chip_id == RTL8852C) {
		rtw89_write32_clr(rtwdev, R_AX_SYS_SDIO_CTRL,
				  B_AX_PCIE_DIS_L2_CTRL_LDO_HCI);
	}
}

static int rtw89_pci_dphy_delay(struct rtw89_dev *rtwdev)
{
	if (!rtw89_is_rtl885xb(rtwdev))
		return 0;

	return rtw89_write16_mdio_mask(rtwdev, RAC_REG_REV2, BAC_CMU_EN_DLY_MASK,
				       PCIE_DPHY_DLY_25US, PCIE_PHY_GEN1);
}

static void rtw89_pci_power_wake_ax(struct rtw89_dev *rtwdev, bool pwr_up)
{
	if (pwr_up)
		rtw89_write32_set(rtwdev, R_AX_HCI_OPT_CTRL, BIT_WAKE_CTRL);
	else
		rtw89_write32_clr(rtwdev, R_AX_HCI_OPT_CTRL, BIT_WAKE_CTRL);
}

static void rtw89_pci_autoload_hang(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id != RTL8852C)
		return;

	rtw89_write32_set(rtwdev, R_AX_PCIE_BG_CLR, B_AX_BG_CLR_ASYNC_M3);
	rtw89_write32_clr(rtwdev, R_AX_PCIE_BG_CLR, B_AX_BG_CLR_ASYNC_M3);
}

static void rtw89_pci_l12_vmain(struct rtw89_dev *rtwdev)
{
	if (!(rtwdev->chip->chip_id == RTL8852C && rtwdev->hal.cv == CHIP_CAV))
		return;

	rtw89_write32_set(rtwdev, R_AX_SYS_SDIO_CTRL, B_AX_PCIE_FORCE_PWR_NGAT);
}

static void rtw89_pci_gen2_force_ib(struct rtw89_dev *rtwdev)
{
	if (!(rtwdev->chip->chip_id == RTL8852C && rtwdev->hal.cv == CHIP_CAV))
		return;

	rtw89_write32_set(rtwdev, R_AX_PMC_DBG_CTRL2,
			  B_AX_SYSON_DIS_PMCR_AX_WRMSK);
	rtw89_write32_set(rtwdev, R_AX_HCI_BG_CTRL, B_AX_BG_CLR_ASYNC_M3);
	rtw89_write32_clr(rtwdev, R_AX_PMC_DBG_CTRL2,
			  B_AX_SYSON_DIS_PMCR_AX_WRMSK);
}

static void rtw89_pci_l1_ent_lat(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id != RTL8852C)
		return;

	rtw89_write32_clr(rtwdev, R_AX_PCIE_PS_CTRL_V1, B_AX_SEL_REQ_ENTR_L1);
}

static void rtw89_pci_wd_exit_l1(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id != RTL8852C)
		return;

	rtw89_write32_set(rtwdev, R_AX_PCIE_PS_CTRL_V1, B_AX_DMAC0_EXIT_L1_EN);
}

static void rtw89_pci_set_sic(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id == RTL8852C)
		return;

	rtw89_write32_clr(rtwdev, R_AX_PCIE_EXP_CTRL,
			  B_AX_SIC_EN_FORCE_CLKREQ);
}

static void rtw89_pci_set_lbc(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 lbc;

	if (rtwdev->chip->chip_id == RTL8852C)
		return;

	lbc = rtw89_read32(rtwdev, R_AX_LBC_WATCHDOG);
	if (info->lbc_en == MAC_AX_PCIE_ENABLE) {
		lbc = u32_replace_bits(lbc, info->lbc_tmr, B_AX_LBC_TIMER);
		lbc |= B_AX_LBC_FLAG | B_AX_LBC_EN;
		rtw89_write32(rtwdev, R_AX_LBC_WATCHDOG, lbc);
	} else {
		lbc &= ~B_AX_LBC_EN;
	}
	rtw89_write32_set(rtwdev, R_AX_LBC_WATCHDOG, lbc);
}

static void rtw89_pci_set_io_rcy(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 val32;

	if (rtwdev->chip->chip_id != RTL8852C)
		return;

	if (info->io_rcy_en == MAC_AX_PCIE_ENABLE) {
		val32 = FIELD_PREP(B_AX_PCIE_WDT_TIMER_M1_MASK,
				   info->io_rcy_tmr);
		rtw89_write32(rtwdev, R_AX_PCIE_WDT_TIMER_M1, val32);
		rtw89_write32(rtwdev, R_AX_PCIE_WDT_TIMER_M2, val32);
		rtw89_write32(rtwdev, R_AX_PCIE_WDT_TIMER_E0, val32);

		rtw89_write32_set(rtwdev, R_AX_PCIE_IO_RCY_M1, B_AX_PCIE_IO_RCY_WDT_MODE_M1);
		rtw89_write32_set(rtwdev, R_AX_PCIE_IO_RCY_M2, B_AX_PCIE_IO_RCY_WDT_MODE_M2);
		rtw89_write32_set(rtwdev, R_AX_PCIE_IO_RCY_E0, B_AX_PCIE_IO_RCY_WDT_MODE_E0);
	} else {
		rtw89_write32_clr(rtwdev, R_AX_PCIE_IO_RCY_M1, B_AX_PCIE_IO_RCY_WDT_MODE_M1);
		rtw89_write32_clr(rtwdev, R_AX_PCIE_IO_RCY_M2, B_AX_PCIE_IO_RCY_WDT_MODE_M2);
		rtw89_write32_clr(rtwdev, R_AX_PCIE_IO_RCY_E0, B_AX_PCIE_IO_RCY_WDT_MODE_E0);
	}

	rtw89_write32_clr(rtwdev, R_AX_PCIE_IO_RCY_S1, B_AX_PCIE_IO_RCY_WDT_MODE_S1);
}

static void rtw89_pci_set_dbg(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id == RTL8852C)
		return;

	rtw89_write32_set(rtwdev, R_AX_PCIE_DBG_CTRL,
			  B_AX_ASFF_FULL_NO_STK | B_AX_EN_STUCK_DBG);

	if (rtwdev->chip->chip_id == RTL8852A)
		rtw89_write32_set(rtwdev, R_AX_PCIE_EXP_CTRL,
				  B_AX_EN_CHKDSC_NO_RX_STUCK);
}

static void rtw89_pci_set_keep_reg(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id == RTL8852C)
		return;

	rtw89_write32_set(rtwdev, R_AX_PCIE_INIT_CFG1,
			  B_AX_PCIE_TXRST_KEEP_REG | B_AX_PCIE_RXRST_KEEP_REG);
}

static void rtw89_pci_clr_idx_all_ax(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	u32 val = B_AX_CLR_ACH0_IDX | B_AX_CLR_ACH1_IDX | B_AX_CLR_ACH2_IDX |
		  B_AX_CLR_ACH3_IDX | B_AX_CLR_CH8_IDX | B_AX_CLR_CH9_IDX |
		  B_AX_CLR_CH12_IDX;
	u32 rxbd_rwptr_clr = info->rxbd_rwptr_clr_reg;
	u32 txbd_rwptr_clr2 = info->txbd_rwptr_clr2_reg;

	if (chip_id == RTL8852A || chip_id == RTL8852C)
		val |= B_AX_CLR_ACH4_IDX | B_AX_CLR_ACH5_IDX |
		       B_AX_CLR_ACH6_IDX | B_AX_CLR_ACH7_IDX;
	/* clear DMA indexes */
	rtw89_write32_set(rtwdev, R_AX_TXBD_RWPTR_CLR1, val);
	if (chip_id == RTL8852A || chip_id == RTL8852C)
		rtw89_write32_set(rtwdev, txbd_rwptr_clr2,
				  B_AX_CLR_CH10_IDX | B_AX_CLR_CH11_IDX);
	rtw89_write32_set(rtwdev, rxbd_rwptr_clr,
			  B_AX_CLR_RXQ_IDX | B_AX_CLR_RPQ_IDX);
}

static int rtw89_pci_poll_txdma_ch_idle_ax(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 dma_busy1 = info->dma_busy1.addr;
	u32 dma_busy2 = info->dma_busy2_reg;
	u32 check, dma_busy;
	int ret;

	check = info->dma_busy1.mask;

	ret = read_poll_timeout(rtw89_read32, dma_busy, (dma_busy & check) == 0,
				10, 100, false, rtwdev, dma_busy1);
	if (ret)
		return ret;

	if (!dma_busy2)
		return 0;

	check = B_AX_CH10_BUSY | B_AX_CH11_BUSY;

	ret = read_poll_timeout(rtw89_read32, dma_busy, (dma_busy & check) == 0,
				10, 100, false, rtwdev, dma_busy2);
	if (ret)
		return ret;

	return 0;
}

static int rtw89_pci_poll_rxdma_ch_idle_ax(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	u32 dma_busy3 = info->dma_busy3_reg;
	u32 check, dma_busy;
	int ret;

	check = B_AX_RXQ_BUSY | B_AX_RPQ_BUSY;

	ret = read_poll_timeout(rtw89_read32, dma_busy, (dma_busy & check) == 0,
				10, 100, false, rtwdev, dma_busy3);
	if (ret)
		return ret;

	return 0;
}

static int rtw89_pci_poll_dma_all_idle(struct rtw89_dev *rtwdev)
{
	u32 ret;

	ret = rtw89_pci_poll_txdma_ch_idle_ax(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "txdma ch busy\n");
		return ret;
	}

	ret = rtw89_pci_poll_rxdma_ch_idle_ax(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "rxdma ch busy\n");
		return ret;
	}

	return 0;
}

static int rtw89_pci_mode_op(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	enum mac_ax_bd_trunc_mode txbd_trunc_mode = info->txbd_trunc_mode;
	enum mac_ax_bd_trunc_mode rxbd_trunc_mode = info->rxbd_trunc_mode;
	enum mac_ax_rxbd_mode rxbd_mode = info->rxbd_mode;
	enum mac_ax_tag_mode tag_mode = info->tag_mode;
	enum mac_ax_wd_dma_intvl wd_dma_idle_intvl = info->wd_dma_idle_intvl;
	enum mac_ax_wd_dma_intvl wd_dma_act_intvl = info->wd_dma_act_intvl;
	enum mac_ax_tx_burst tx_burst = info->tx_burst;
	enum mac_ax_rx_burst rx_burst = info->rx_burst;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	u8 cv = rtwdev->hal.cv;
	u32 val32;

	if (txbd_trunc_mode == MAC_AX_BD_TRUNC) {
		if (chip_id == RTL8852A && cv == CHIP_CBV)
			rtw89_write32_set(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_TX_TRUNC_MODE);
	} else if (txbd_trunc_mode == MAC_AX_BD_NORM) {
		if (chip_id == RTL8852A || chip_id == RTL8852B)
			rtw89_write32_clr(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_TX_TRUNC_MODE);
	}

	if (rxbd_trunc_mode == MAC_AX_BD_TRUNC) {
		if (chip_id == RTL8852A && cv == CHIP_CBV)
			rtw89_write32_set(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_RX_TRUNC_MODE);
	} else if (rxbd_trunc_mode == MAC_AX_BD_NORM) {
		if (chip_id == RTL8852A || chip_id == RTL8852B)
			rtw89_write32_clr(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_RX_TRUNC_MODE);
	}

	if (rxbd_mode == MAC_AX_RXBD_PKT) {
		rtw89_write32_clr(rtwdev, info->init_cfg_reg, info->rxbd_mode_bit);
	} else if (rxbd_mode == MAC_AX_RXBD_SEP) {
		rtw89_write32_set(rtwdev, info->init_cfg_reg, info->rxbd_mode_bit);

		if (chip_id == RTL8852A || chip_id == RTL8852B)
			rtw89_write32_mask(rtwdev, R_AX_PCIE_INIT_CFG2,
					   B_AX_PCIE_RX_APPLEN_MASK, 0);
	}

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		rtw89_write32_mask(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_PCIE_MAX_TXDMA_MASK, tx_burst);
		rtw89_write32_mask(rtwdev, R_AX_PCIE_INIT_CFG1, B_AX_PCIE_MAX_RXDMA_MASK, rx_burst);
	} else if (chip_id == RTL8852C) {
		rtw89_write32_mask(rtwdev, R_AX_HAXI_INIT_CFG1, B_AX_HAXI_MAX_TXDMA_MASK, tx_burst);
		rtw89_write32_mask(rtwdev, R_AX_HAXI_INIT_CFG1, B_AX_HAXI_MAX_RXDMA_MASK, rx_burst);
	}

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		if (tag_mode == MAC_AX_TAG_SGL) {
			val32 = rtw89_read32(rtwdev, R_AX_PCIE_INIT_CFG1) &
					    ~B_AX_LATENCY_CONTROL;
			rtw89_write32(rtwdev, R_AX_PCIE_INIT_CFG1, val32);
		} else if (tag_mode == MAC_AX_TAG_MULTI) {
			val32 = rtw89_read32(rtwdev, R_AX_PCIE_INIT_CFG1) |
					    B_AX_LATENCY_CONTROL;
			rtw89_write32(rtwdev, R_AX_PCIE_INIT_CFG1, val32);
		}
	}

	rtw89_write32_mask(rtwdev, info->exp_ctrl_reg, info->max_tag_num_mask,
			   info->multi_tag_num);

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		rtw89_write32_mask(rtwdev, R_AX_PCIE_INIT_CFG2, B_AX_WD_ITVL_IDLE,
				   wd_dma_idle_intvl);
		rtw89_write32_mask(rtwdev, R_AX_PCIE_INIT_CFG2, B_AX_WD_ITVL_ACT,
				   wd_dma_act_intvl);
	} else if (chip_id == RTL8852C) {
		rtw89_write32_mask(rtwdev, R_AX_HAXI_INIT_CFG1, B_AX_WD_ITVL_IDLE_V1_MASK,
				   wd_dma_idle_intvl);
		rtw89_write32_mask(rtwdev, R_AX_HAXI_INIT_CFG1, B_AX_WD_ITVL_ACT_V1_MASK,
				   wd_dma_act_intvl);
	}

	if (txbd_trunc_mode == MAC_AX_BD_TRUNC) {
		rtw89_write32_set(rtwdev, R_AX_TX_ADDRESS_INFO_MODE_SETTING,
				  B_AX_HOST_ADDR_INFO_8B_SEL);
		rtw89_write32_clr(rtwdev, R_AX_PKTIN_SETTING, B_AX_WD_ADDR_INFO_LENGTH);
	} else if (txbd_trunc_mode == MAC_AX_BD_NORM) {
		rtw89_write32_clr(rtwdev, R_AX_TX_ADDRESS_INFO_MODE_SETTING,
				  B_AX_HOST_ADDR_INFO_8B_SEL);
		rtw89_write32_set(rtwdev, R_AX_PKTIN_SETTING, B_AX_WD_ADDR_INFO_LENGTH);
	}

	return 0;
}

static int rtw89_pci_ops_deinit(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;

	rtw89_pci_power_wake(rtwdev, false);

	if (rtwdev->chip->chip_id == RTL8852A) {
		/* ltr sw trigger */
		rtw89_write32_set(rtwdev, R_AX_LTR_CTRL_0, B_AX_APP_LTR_IDLE);
	}
	info->ltr_set(rtwdev, false);
	rtw89_pci_ctrl_dma_all(rtwdev, false);
	rtw89_pci_clr_idx_all(rtwdev);

	return 0;
}

static int rtw89_pci_ops_mac_pre_init_ax(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	int ret;

	rtw89_pci_ber(rtwdev);
	rtw89_pci_rxdma_prefth(rtwdev);
	rtw89_pci_l1off_pwroff(rtwdev);
	rtw89_pci_deglitch_setting(rtwdev);
	ret = rtw89_pci_l2_rxen_lat(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "[ERR] pcie l2 rxen lat %d\n", ret);
		return ret;
	}

	rtw89_pci_aphy_pwrcut(rtwdev);
	rtw89_pci_hci_ldo(rtwdev);
	rtw89_pci_dphy_delay(rtwdev);

	ret = rtw89_pci_autok_x(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "[ERR] pcie autok_x fail %d\n", ret);
		return ret;
	}

	ret = rtw89_pci_auto_refclk_cal(rtwdev, false);
	if (ret) {
		rtw89_err(rtwdev, "[ERR] pcie autok fail %d\n", ret);
		return ret;
	}

	rtw89_pci_power_wake_ax(rtwdev, true);
	rtw89_pci_autoload_hang(rtwdev);
	rtw89_pci_l12_vmain(rtwdev);
	rtw89_pci_gen2_force_ib(rtwdev);
	rtw89_pci_l1_ent_lat(rtwdev);
	rtw89_pci_wd_exit_l1(rtwdev);
	rtw89_pci_set_sic(rtwdev);
	rtw89_pci_set_lbc(rtwdev);
	rtw89_pci_set_io_rcy(rtwdev);
	rtw89_pci_set_dbg(rtwdev);
	rtw89_pci_set_keep_reg(rtwdev);

	rtw89_write32_set(rtwdev, info->dma_stop1.addr, B_AX_STOP_WPDMA);

	/* stop DMA activities */
	rtw89_pci_ctrl_dma_all(rtwdev, false);

	ret = rtw89_pci_poll_dma_all_idle(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "[ERR] poll pcie dma all idle\n");
		return ret;
	}

	rtw89_pci_clr_idx_all(rtwdev);
	rtw89_pci_mode_op(rtwdev);

	/* fill TRX BD indexes */
	rtw89_pci_ops_reset(rtwdev);

	ret = rtw89_pci_rst_bdram_ax(rtwdev);
	if (ret) {
		rtw89_warn(rtwdev, "reset bdram busy\n");
		return ret;
	}

	/* disable all channels except to FW CMD channel to download firmware */
	rtw89_pci_ctrl_txdma_ch_ax(rtwdev, false);
	rtw89_pci_ctrl_txdma_fw_ch_ax(rtwdev, true);

	/* start DMA activities */
	rtw89_pci_ctrl_dma_all(rtwdev, true);

	return 0;
}

static int rtw89_pci_ops_mac_pre_deinit_ax(struct rtw89_dev *rtwdev)
{
	rtw89_pci_power_wake_ax(rtwdev, false);

	return 0;
}

int rtw89_pci_ltr_set(struct rtw89_dev *rtwdev, bool en)
{
	u32 val;

	if (!en)
		return 0;

	val = rtw89_read32(rtwdev, R_AX_LTR_CTRL_0);
	if (rtw89_pci_ltr_is_err_reg_val(val))
		return -EINVAL;
	val = rtw89_read32(rtwdev, R_AX_LTR_CTRL_1);
	if (rtw89_pci_ltr_is_err_reg_val(val))
		return -EINVAL;
	val = rtw89_read32(rtwdev, R_AX_LTR_IDLE_LATENCY);
	if (rtw89_pci_ltr_is_err_reg_val(val))
		return -EINVAL;
	val = rtw89_read32(rtwdev, R_AX_LTR_ACTIVE_LATENCY);
	if (rtw89_pci_ltr_is_err_reg_val(val))
		return -EINVAL;

	rtw89_write32_set(rtwdev, R_AX_LTR_CTRL_0, B_AX_LTR_HW_EN | B_AX_LTR_EN |
						   B_AX_LTR_WD_NOEMP_CHK);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_0, B_AX_LTR_SPACE_IDX_MASK,
			   PCI_LTR_SPC_500US);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_0, B_AX_LTR_IDLE_TIMER_IDX_MASK,
			   PCI_LTR_IDLE_TIMER_3_2MS);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_1, B_AX_LTR_RX0_TH_MASK, 0x28);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_1, B_AX_LTR_RX1_TH_MASK, 0x28);
	rtw89_write32(rtwdev, R_AX_LTR_IDLE_LATENCY, 0x90039003);
	rtw89_write32(rtwdev, R_AX_LTR_ACTIVE_LATENCY, 0x880b880b);

	return 0;
}
EXPORT_SYMBOL(rtw89_pci_ltr_set);

int rtw89_pci_ltr_set_v1(struct rtw89_dev *rtwdev, bool en)
{
	u32 dec_ctrl;
	u32 val32;

	val32 = rtw89_read32(rtwdev, R_AX_LTR_CTRL_0);
	if (rtw89_pci_ltr_is_err_reg_val(val32))
		return -EINVAL;
	val32 = rtw89_read32(rtwdev, R_AX_LTR_CTRL_1);
	if (rtw89_pci_ltr_is_err_reg_val(val32))
		return -EINVAL;
	dec_ctrl = rtw89_read32(rtwdev, R_AX_LTR_DEC_CTRL);
	if (rtw89_pci_ltr_is_err_reg_val(dec_ctrl))
		return -EINVAL;
	val32 = rtw89_read32(rtwdev, R_AX_LTR_LATENCY_IDX3);
	if (rtw89_pci_ltr_is_err_reg_val(val32))
		return -EINVAL;
	val32 = rtw89_read32(rtwdev, R_AX_LTR_LATENCY_IDX0);
	if (rtw89_pci_ltr_is_err_reg_val(val32))
		return -EINVAL;

	if (!en) {
		dec_ctrl &= ~(LTR_EN_BITS | B_AX_LTR_IDX_DRV_MASK | B_AX_LTR_HW_DEC_EN);
		dec_ctrl |= FIELD_PREP(B_AX_LTR_IDX_DRV_MASK, PCIE_LTR_IDX_IDLE) |
			    B_AX_LTR_REQ_DRV;
	} else {
		dec_ctrl |= B_AX_LTR_HW_DEC_EN;
	}

	dec_ctrl &= ~B_AX_LTR_SPACE_IDX_V1_MASK;
	dec_ctrl |= FIELD_PREP(B_AX_LTR_SPACE_IDX_V1_MASK, PCI_LTR_SPC_500US);

	if (en)
		rtw89_write32_set(rtwdev, R_AX_LTR_CTRL_0,
				  B_AX_LTR_WD_NOEMP_CHK_V1 | B_AX_LTR_HW_EN);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_0, B_AX_LTR_IDLE_TIMER_IDX_MASK,
			   PCI_LTR_IDLE_TIMER_3_2MS);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_1, B_AX_LTR_RX0_TH_MASK, 0x28);
	rtw89_write32_mask(rtwdev, R_AX_LTR_CTRL_1, B_AX_LTR_RX1_TH_MASK, 0x28);
	rtw89_write32(rtwdev, R_AX_LTR_DEC_CTRL, dec_ctrl);
	rtw89_write32(rtwdev, R_AX_LTR_LATENCY_IDX3, 0x90039003);
	rtw89_write32(rtwdev, R_AX_LTR_LATENCY_IDX0, 0x880b880b);

	return 0;
}
EXPORT_SYMBOL(rtw89_pci_ltr_set_v1);

static int rtw89_pci_ops_mac_post_init_ax(struct rtw89_dev *rtwdev)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	int ret;

	ret = info->ltr_set(rtwdev, true);
	if (ret) {
		rtw89_err(rtwdev, "pci ltr set fail\n");
		return ret;
	}
	if (chip_id == RTL8852A) {
		/* ltr sw trigger */
		rtw89_write32_set(rtwdev, R_AX_LTR_CTRL_0, B_AX_APP_LTR_ACT);
	}
	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		/* ADDR info 8-byte mode */
		rtw89_write32_set(rtwdev, R_AX_TX_ADDRESS_INFO_MODE_SETTING,
				  B_AX_HOST_ADDR_INFO_8B_SEL);
		rtw89_write32_clr(rtwdev, R_AX_PKTIN_SETTING, B_AX_WD_ADDR_INFO_LENGTH);
	}

	/* enable DMA for all queues */
	rtw89_pci_ctrl_txdma_ch_ax(rtwdev, true);

	/* Release PCI IO */
	rtw89_write32_clr(rtwdev, info->dma_stop1.addr,
			  B_AX_STOP_WPDMA | B_AX_STOP_PCIEIO);

	return 0;
}

static int rtw89_pci_claim_device(struct rtw89_dev *rtwdev,
				  struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	int ret;

	ret = pci_enable_device(pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to enable pci device\n");
		return ret;
	}

	pci_set_master(pdev);
	pci_set_drvdata(pdev, rtwdev->hw);

	rtwpci->pdev = pdev;

	return 0;
}

static void rtw89_pci_declaim_device(struct rtw89_dev *rtwdev,
				     struct pci_dev *pdev)
{
	pci_disable_device(pdev);
}

static bool rtw89_pci_chip_is_manual_dac(struct rtw89_dev *rtwdev)
{
	const struct rtw89_chip_info *chip = rtwdev->chip;

	switch (chip->chip_id) {
	case RTL8852A:
	case RTL8852B:
	case RTL8851B:
	case RTL8852BT:
		return true;
	default:
		return false;
	}
}

static bool rtw89_pci_is_dac_compatible_bridge(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *bridge = pci_upstream_bridge(rtwpci->pdev);

	if (!rtw89_pci_chip_is_manual_dac(rtwdev))
		return true;

	if (!bridge)
		return false;

	switch (bridge->vendor) {
	case PCI_VENDOR_ID_INTEL:
		return true;
	case PCI_VENDOR_ID_ASMEDIA:
		if (bridge->device == 0x2806)
			return true;
		break;
	}

	return false;
}

static void rtw89_pci_cfg_dac(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	if (!rtwpci->enable_dac)
		return;

	if (!rtw89_pci_chip_is_manual_dac(rtwdev))
		return;

	rtw89_pci_config_byte_set(rtwdev, RTW89_PCIE_L1_CTRL, RTW89_PCIE_BIT_EN_64BITS);
}

static int rtw89_pci_setup_mapping(struct rtw89_dev *rtwdev,
				   struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	unsigned long resource_len;
	u8 bar_id = 2;
	int ret;

	ret = pci_request_regions(pdev, KBUILD_MODNAME);
	if (ret) {
		rtw89_err(rtwdev, "failed to request pci regions\n");
		goto err;
	}

	if (!rtw89_pci_is_dac_compatible_bridge(rtwdev))
		goto no_dac;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	if (!ret) {
		rtwpci->enable_dac = true;
		rtw89_pci_cfg_dac(rtwdev);
	} else {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret) {
			rtw89_err(rtwdev,
				  "failed to set dma and consistent mask to 32/36-bit\n");
			goto err_release_regions;
		}
	}
no_dac:

#if defined(__FreeBSD__)
	linuxkpi_pcim_want_to_use_bus_functions(pdev);
#endif
	resource_len = pci_resource_len(pdev, bar_id);
	rtwpci->mmap = pci_iomap(pdev, bar_id, resource_len);
	if (!rtwpci->mmap) {
		rtw89_err(rtwdev, "failed to map pci io\n");
		ret = -EIO;
		goto err_release_regions;
	}

	return 0;

err_release_regions:
	pci_release_regions(pdev);
err:
	return ret;
}

static void rtw89_pci_clear_mapping(struct rtw89_dev *rtwdev,
				    struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	if (rtwpci->mmap) {
		pci_iounmap(pdev, rtwpci->mmap);
		pci_release_regions(pdev);
	}
}

static void rtw89_pci_free_tx_wd_ring(struct rtw89_dev *rtwdev,
				      struct pci_dev *pdev,
				      struct rtw89_pci_tx_ring *tx_ring)
{
	struct rtw89_pci_tx_wd_ring *wd_ring = &tx_ring->wd_ring;
	u8 *head = wd_ring->head;
	dma_addr_t dma = wd_ring->dma;
	u32 page_size = wd_ring->page_size;
	u32 page_num = wd_ring->page_num;
	u32 ring_sz = page_size * page_num;

	dma_free_coherent(&pdev->dev, ring_sz, head, dma);
	wd_ring->head = NULL;
}

static void rtw89_pci_free_tx_ring(struct rtw89_dev *rtwdev,
				   struct pci_dev *pdev,
				   struct rtw89_pci_tx_ring *tx_ring)
{
	int ring_sz;
	u8 *head;
	dma_addr_t dma;

	head = tx_ring->bd_ring.head;
	dma = tx_ring->bd_ring.dma;
	ring_sz = tx_ring->bd_ring.desc_size * tx_ring->bd_ring.len;
	dma_free_coherent(&pdev->dev, ring_sz, head, dma);

	tx_ring->bd_ring.head = NULL;
}

static void rtw89_pci_free_tx_rings(struct rtw89_dev *rtwdev,
				    struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	struct rtw89_pci_tx_ring *tx_ring;
	int i;

	for (i = 0; i < RTW89_TXCH_NUM; i++) {
		if (info->tx_dma_ch_mask & BIT(i))
			continue;
		tx_ring = &rtwpci->tx_rings[i];
		rtw89_pci_free_tx_wd_ring(rtwdev, pdev, tx_ring);
		rtw89_pci_free_tx_ring(rtwdev, pdev, tx_ring);
	}
}

static void rtw89_pci_free_rx_ring(struct rtw89_dev *rtwdev,
				   struct pci_dev *pdev,
				   struct rtw89_pci_rx_ring *rx_ring)
{
	struct rtw89_pci_rx_info *rx_info;
	struct sk_buff *skb;
	dma_addr_t dma;
	u32 buf_sz;
	u8 *head;
	int ring_sz = rx_ring->bd_ring.desc_size * rx_ring->bd_ring.len;
	int i;

	buf_sz = rx_ring->buf_sz;
	for (i = 0; i < rx_ring->bd_ring.len; i++) {
		skb = rx_ring->buf[i];
		if (!skb)
			continue;

		rx_info = RTW89_PCI_RX_SKB_CB(skb);
		dma = rx_info->dma;
		dma_unmap_single(&pdev->dev, dma, buf_sz, DMA_FROM_DEVICE);
		dev_kfree_skb(skb);
		rx_ring->buf[i] = NULL;
	}

	head = rx_ring->bd_ring.head;
	dma = rx_ring->bd_ring.dma;
	dma_free_coherent(&pdev->dev, ring_sz, head, dma);

	rx_ring->bd_ring.head = NULL;
}

static void rtw89_pci_free_rx_rings(struct rtw89_dev *rtwdev,
				    struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_rx_ring *rx_ring;
	int i;

	for (i = 0; i < RTW89_RXCH_NUM; i++) {
		rx_ring = &rtwpci->rx_rings[i];
		rtw89_pci_free_rx_ring(rtwdev, pdev, rx_ring);
	}
}

static void rtw89_pci_free_trx_rings(struct rtw89_dev *rtwdev,
				     struct pci_dev *pdev)
{
	rtw89_pci_free_rx_rings(rtwdev, pdev);
	rtw89_pci_free_tx_rings(rtwdev, pdev);
}

static int rtw89_pci_init_rx_bd(struct rtw89_dev *rtwdev, struct pci_dev *pdev,
				struct rtw89_pci_rx_ring *rx_ring,
				struct sk_buff *skb, int buf_sz, u32 idx)
{
	struct rtw89_pci_rx_info *rx_info;
	struct rtw89_pci_rx_bd_32 *rx_bd;
	dma_addr_t dma;

	if (!skb)
		return -EINVAL;

	dma = dma_map_single(&pdev->dev, skb->data, buf_sz, DMA_FROM_DEVICE);
	if (dma_mapping_error(&pdev->dev, dma))
		return -EBUSY;

	rx_info = RTW89_PCI_RX_SKB_CB(skb);
	rx_bd = RTW89_PCI_RX_BD(rx_ring, idx);

	memset(rx_bd, 0, sizeof(*rx_bd));
	rx_bd->buf_size = cpu_to_le16(buf_sz);
	rx_bd->dma = cpu_to_le32(dma);
	rx_bd->opt = le16_encode_bits(upper_32_bits(dma), RTW89_PCI_RXBD_OPT_DMA_HI);
	rx_info->dma = dma;

	return 0;
}

static int rtw89_pci_alloc_tx_wd_ring(struct rtw89_dev *rtwdev,
				      struct pci_dev *pdev,
				      struct rtw89_pci_tx_ring *tx_ring,
				      enum rtw89_tx_channel txch)
{
	struct rtw89_pci_tx_wd_ring *wd_ring = &tx_ring->wd_ring;
	struct rtw89_pci_tx_wd *txwd;
	dma_addr_t dma;
	dma_addr_t cur_paddr;
	u8 *head;
	u8 *cur_vaddr;
	u32 page_size = RTW89_PCI_TXWD_PAGE_SIZE;
	u32 page_num = RTW89_PCI_TXWD_NUM_MAX;
	u32 ring_sz = page_size * page_num;
	u32 page_offset;
	int i;

	/* FWCMD queue doesn't use txwd as pages */
	if (txch == RTW89_TXCH_CH12)
		return 0;

	head = dma_alloc_coherent(&pdev->dev, ring_sz, &dma, GFP_KERNEL);
	if (!head)
		return -ENOMEM;

	INIT_LIST_HEAD(&wd_ring->free_pages);
	wd_ring->head = head;
	wd_ring->dma = dma;
	wd_ring->page_size = page_size;
	wd_ring->page_num = page_num;

	page_offset = 0;
	for (i = 0; i < page_num; i++) {
		txwd = &wd_ring->pages[i];
		cur_paddr = dma + page_offset;
		cur_vaddr = head + page_offset;

		skb_queue_head_init(&txwd->queue);
		INIT_LIST_HEAD(&txwd->list);
		txwd->paddr = cur_paddr;
		txwd->vaddr = cur_vaddr;
		txwd->len = page_size;
		txwd->seq = i;
		rtw89_pci_enqueue_txwd(tx_ring, txwd);

		page_offset += page_size;
	}

	return 0;
}

static int rtw89_pci_alloc_tx_ring(struct rtw89_dev *rtwdev,
				   struct pci_dev *pdev,
				   struct rtw89_pci_tx_ring *tx_ring,
				   u32 desc_size, u32 len,
				   enum rtw89_tx_channel txch)
{
	const struct rtw89_pci_ch_dma_addr *txch_addr;
	int ring_sz = desc_size * len;
	u8 *head;
	dma_addr_t dma;
	int ret;

	ret = rtw89_pci_alloc_tx_wd_ring(rtwdev, pdev, tx_ring, txch);
	if (ret) {
		rtw89_err(rtwdev, "failed to alloc txwd ring of txch %d\n", txch);
		goto err;
	}

	ret = rtw89_pci_get_txch_addrs(rtwdev, txch, &txch_addr);
	if (ret) {
		rtw89_err(rtwdev, "failed to get address of txch %d", txch);
		goto err_free_wd_ring;
	}

	head = dma_alloc_coherent(&pdev->dev, ring_sz, &dma, GFP_KERNEL);
	if (!head) {
		ret = -ENOMEM;
		goto err_free_wd_ring;
	}

	INIT_LIST_HEAD(&tx_ring->busy_pages);
	tx_ring->bd_ring.head = head;
	tx_ring->bd_ring.dma = dma;
	tx_ring->bd_ring.len = len;
	tx_ring->bd_ring.desc_size = desc_size;
	tx_ring->bd_ring.addr = *txch_addr;
	tx_ring->bd_ring.wp = 0;
	tx_ring->bd_ring.rp = 0;
	tx_ring->txch = txch;

	return 0;

err_free_wd_ring:
	rtw89_pci_free_tx_wd_ring(rtwdev, pdev, tx_ring);
err:
	return ret;
}

static int rtw89_pci_alloc_tx_rings(struct rtw89_dev *rtwdev,
				    struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	struct rtw89_pci_tx_ring *tx_ring;
	u32 desc_size;
	u32 len;
	u32 i, tx_allocated;
	int ret;

	for (i = 0; i < RTW89_TXCH_NUM; i++) {
		if (info->tx_dma_ch_mask & BIT(i))
			continue;
		tx_ring = &rtwpci->tx_rings[i];
		desc_size = sizeof(struct rtw89_pci_tx_bd_32);
		len = RTW89_PCI_TXBD_NUM_MAX;
		ret = rtw89_pci_alloc_tx_ring(rtwdev, pdev, tx_ring,
					      desc_size, len, i);
		if (ret) {
#if defined(__linux__)
			rtw89_err(rtwdev, "failed to alloc tx ring %d\n", i);
#elif defined(__FreeBSD__)
			rtw89_err(rtwdev, "failed to alloc tx ring %d: ret=%d\n", i, ret);
#endif
			goto err_free;
		}
	}

	return 0;

err_free:
	tx_allocated = i;
	for (i = 0; i < tx_allocated; i++) {
		tx_ring = &rtwpci->tx_rings[i];
		rtw89_pci_free_tx_ring(rtwdev, pdev, tx_ring);
	}

	return ret;
}

static int rtw89_pci_alloc_rx_ring(struct rtw89_dev *rtwdev,
				   struct pci_dev *pdev,
				   struct rtw89_pci_rx_ring *rx_ring,
				   u32 desc_size, u32 len, u32 rxch)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_ch_dma_addr *rxch_addr;
	struct sk_buff *skb;
	u8 *head;
	dma_addr_t dma;
	int ring_sz = desc_size * len;
	int buf_sz = RTW89_PCI_RX_BUF_SIZE;
	int i, allocated;
	int ret;

	ret = rtw89_pci_get_rxch_addrs(rtwdev, rxch, &rxch_addr);
	if (ret) {
		rtw89_err(rtwdev, "failed to get address of rxch %d", rxch);
		return ret;
	}

	head = dma_alloc_coherent(&pdev->dev, ring_sz, &dma, GFP_KERNEL);
	if (!head) {
		ret = -ENOMEM;
		goto err;
	}

	rx_ring->bd_ring.head = head;
	rx_ring->bd_ring.dma = dma;
	rx_ring->bd_ring.len = len;
	rx_ring->bd_ring.desc_size = desc_size;
	rx_ring->bd_ring.addr = *rxch_addr;
	if (info->rx_ring_eq_is_full)
		rx_ring->bd_ring.wp = len - 1;
	else
		rx_ring->bd_ring.wp = 0;
	rx_ring->bd_ring.rp = 0;
	rx_ring->buf_sz = buf_sz;
	rx_ring->diliver_skb = NULL;
	rx_ring->diliver_desc.ready = false;
	rx_ring->target_rx_tag = 0;

	for (i = 0; i < len; i++) {
		skb = dev_alloc_skb(buf_sz);
		if (!skb) {
			ret = -ENOMEM;
			goto err_free;
		}

		memset(skb->data, 0, buf_sz);
		rx_ring->buf[i] = skb;
		ret = rtw89_pci_init_rx_bd(rtwdev, pdev, rx_ring, skb,
					   buf_sz, i);
		if (ret) {
#if defined(__linux__)
			rtw89_err(rtwdev, "failed to init rx buf %d\n", i);
#elif defined(__FreeBSD__)
			rtw89_err(rtwdev, "failed to init rx buf %d ret=%d\n", i, ret);
#endif
			dev_kfree_skb_any(skb);
			rx_ring->buf[i] = NULL;
			goto err_free;
		}
	}

	return 0;

err_free:
	allocated = i;
	for (i = 0; i < allocated; i++) {
		skb = rx_ring->buf[i];
		if (!skb)
			continue;
		dma = *((dma_addr_t *)skb->cb);
		dma_unmap_single(&pdev->dev, dma, buf_sz, DMA_FROM_DEVICE);
		dev_kfree_skb(skb);
		rx_ring->buf[i] = NULL;
	}

	head = rx_ring->bd_ring.head;
	dma = rx_ring->bd_ring.dma;
	dma_free_coherent(&pdev->dev, ring_sz, head, dma);

	rx_ring->bd_ring.head = NULL;
err:
	return ret;
}

static int rtw89_pci_alloc_rx_rings(struct rtw89_dev *rtwdev,
				    struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct rtw89_pci_rx_ring *rx_ring;
	u32 desc_size;
	u32 len;
	int i, rx_allocated;
	int ret;

	for (i = 0; i < RTW89_RXCH_NUM; i++) {
		rx_ring = &rtwpci->rx_rings[i];
		desc_size = sizeof(struct rtw89_pci_rx_bd_32);
		len = RTW89_PCI_RXBD_NUM_MAX;
		ret = rtw89_pci_alloc_rx_ring(rtwdev, pdev, rx_ring,
					      desc_size, len, i);
		if (ret) {
			rtw89_err(rtwdev, "failed to alloc rx ring %d\n", i);
			goto err_free;
		}
	}

	return 0;

err_free:
	rx_allocated = i;
	for (i = 0; i < rx_allocated; i++) {
		rx_ring = &rtwpci->rx_rings[i];
		rtw89_pci_free_rx_ring(rtwdev, pdev, rx_ring);
	}

	return ret;
}

static int rtw89_pci_alloc_trx_rings(struct rtw89_dev *rtwdev,
				     struct pci_dev *pdev)
{
	int ret;

	ret = rtw89_pci_alloc_tx_rings(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to alloc dma tx rings\n");
		goto err;
	}

	ret = rtw89_pci_alloc_rx_rings(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to alloc dma rx rings\n");
		goto err_free_tx_rings;
	}

	return 0;

err_free_tx_rings:
	rtw89_pci_free_tx_rings(rtwdev, pdev);
err:
	return ret;
}

static void rtw89_pci_h2c_init(struct rtw89_dev *rtwdev,
			       struct rtw89_pci *rtwpci)
{
	skb_queue_head_init(&rtwpci->h2c_queue);
	skb_queue_head_init(&rtwpci->h2c_release_queue);
}

static int rtw89_pci_setup_resource(struct rtw89_dev *rtwdev,
				    struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	int ret;

	ret = rtw89_pci_setup_mapping(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to setup pci mapping\n");
		goto err;
	}

	ret = rtw89_pci_alloc_trx_rings(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to alloc pci trx rings\n");
		goto err_pci_unmap;
	}

	rtw89_pci_h2c_init(rtwdev, rtwpci);

	spin_lock_init(&rtwpci->irq_lock);
	spin_lock_init(&rtwpci->trx_lock);

	return 0;

err_pci_unmap:
	rtw89_pci_clear_mapping(rtwdev, pdev);
err:
	return ret;
}

static void rtw89_pci_clear_resource(struct rtw89_dev *rtwdev,
				     struct pci_dev *pdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtw89_pci_free_trx_rings(rtwdev, pdev);
	rtw89_pci_clear_mapping(rtwdev, pdev);
	rtw89_pci_release_fwcmd(rtwdev, rtwpci,
				skb_queue_len(&rtwpci->h2c_queue), true);
}

void rtw89_pci_config_intr_mask(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_chip_info *chip = rtwdev->chip;
	u32 hs0isr_ind_int_en = B_AX_HS0ISR_IND_INT_EN;

	if (chip->chip_id == RTL8851B)
		hs0isr_ind_int_en = B_AX_HS0ISR_IND_INT_EN_WKARND;

	rtwpci->halt_c2h_intrs = B_AX_HALT_C2H_INT_EN | 0;

	if (rtwpci->under_recovery) {
		rtwpci->intrs[0] = hs0isr_ind_int_en;
		rtwpci->intrs[1] = 0;
	} else {
		rtwpci->intrs[0] = B_AX_TXDMA_STUCK_INT_EN |
				   B_AX_RXDMA_INT_EN |
				   B_AX_RXP1DMA_INT_EN |
				   B_AX_RPQDMA_INT_EN |
				   B_AX_RXDMA_STUCK_INT_EN |
				   B_AX_RDU_INT_EN |
				   B_AX_RPQBD_FULL_INT_EN |
				   hs0isr_ind_int_en;

		rtwpci->intrs[1] = B_AX_HC10ISR_IND_INT_EN;
	}
}
EXPORT_SYMBOL(rtw89_pci_config_intr_mask);

static void rtw89_pci_recovery_intr_mask_v1(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtwpci->ind_intrs = B_AX_HS0ISR_IND_INT_EN;
	rtwpci->halt_c2h_intrs = B_AX_HALT_C2H_INT_EN | B_AX_WDT_TIMEOUT_INT_EN;
	rtwpci->intrs[0] = 0;
	rtwpci->intrs[1] = 0;
}

static void rtw89_pci_default_intr_mask_v1(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtwpci->ind_intrs = B_AX_HCI_AXIDMA_INT_EN |
			    B_AX_HS1ISR_IND_INT_EN |
			    B_AX_HS0ISR_IND_INT_EN;
	rtwpci->halt_c2h_intrs = B_AX_HALT_C2H_INT_EN | B_AX_WDT_TIMEOUT_INT_EN;
	rtwpci->intrs[0] = B_AX_TXDMA_STUCK_INT_EN |
			   B_AX_RXDMA_INT_EN |
			   B_AX_RXP1DMA_INT_EN |
			   B_AX_RPQDMA_INT_EN |
			   B_AX_RXDMA_STUCK_INT_EN |
			   B_AX_RDU_INT_EN |
			   B_AX_RPQBD_FULL_INT_EN;
	rtwpci->intrs[1] = B_AX_GPIO18_INT_EN;
}

static void rtw89_pci_low_power_intr_mask_v1(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtwpci->ind_intrs = B_AX_HS1ISR_IND_INT_EN |
			    B_AX_HS0ISR_IND_INT_EN;
	rtwpci->halt_c2h_intrs = B_AX_HALT_C2H_INT_EN | B_AX_WDT_TIMEOUT_INT_EN;
	rtwpci->intrs[0] = 0;
	rtwpci->intrs[1] = B_AX_GPIO18_INT_EN;
}

void rtw89_pci_config_intr_mask_v1(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	if (rtwpci->under_recovery)
		rtw89_pci_recovery_intr_mask_v1(rtwdev);
	else if (rtwpci->low_power)
		rtw89_pci_low_power_intr_mask_v1(rtwdev);
	else
		rtw89_pci_default_intr_mask_v1(rtwdev);
}
EXPORT_SYMBOL(rtw89_pci_config_intr_mask_v1);

static void rtw89_pci_recovery_intr_mask_v2(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtwpci->ind_intrs = B_BE_HS0_IND_INT_EN0;
	rtwpci->halt_c2h_intrs = B_BE_HALT_C2H_INT_EN | B_BE_WDT_TIMEOUT_INT_EN;
	rtwpci->intrs[0] = 0;
	rtwpci->intrs[1] = 0;
}

static void rtw89_pci_default_intr_mask_v2(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtwpci->ind_intrs = B_BE_HCI_AXIDMA_INT_EN0 |
			    B_BE_HS0_IND_INT_EN0;
	rtwpci->halt_c2h_intrs = B_BE_HALT_C2H_INT_EN | B_BE_WDT_TIMEOUT_INT_EN;
	rtwpci->intrs[0] = B_BE_RDU_CH1_INT_IMR_V1 |
			   B_BE_RDU_CH0_INT_IMR_V1;
	rtwpci->intrs[1] = B_BE_PCIE_RX_RX0P2_IMR0_V1 |
			   B_BE_PCIE_RX_RPQ0_IMR0_V1;
}

static void rtw89_pci_low_power_intr_mask_v2(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	rtwpci->ind_intrs = B_BE_HS0_IND_INT_EN0 |
			    B_BE_HS1_IND_INT_EN0;
	rtwpci->halt_c2h_intrs = B_BE_HALT_C2H_INT_EN | B_BE_WDT_TIMEOUT_INT_EN;
	rtwpci->intrs[0] = 0;
	rtwpci->intrs[1] = B_BE_PCIE_RX_RX0P2_IMR0_V1 |
			   B_BE_PCIE_RX_RPQ0_IMR0_V1;
}

void rtw89_pci_config_intr_mask_v2(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;

	if (rtwpci->under_recovery)
		rtw89_pci_recovery_intr_mask_v2(rtwdev);
	else if (rtwpci->low_power)
		rtw89_pci_low_power_intr_mask_v2(rtwdev);
	else
		rtw89_pci_default_intr_mask_v2(rtwdev);
}
EXPORT_SYMBOL(rtw89_pci_config_intr_mask_v2);

static int rtw89_pci_request_irq(struct rtw89_dev *rtwdev,
				 struct pci_dev *pdev)
{
	unsigned long flags = 0;
	int ret;

	flags |= PCI_IRQ_INTX | PCI_IRQ_MSI;
	ret = pci_alloc_irq_vectors(pdev, 1, 1, flags);
	if (ret < 0) {
		rtw89_err(rtwdev, "failed to alloc irq vectors, ret %d\n", ret);
		goto err;
	}

	ret = devm_request_threaded_irq(rtwdev->dev, pdev->irq,
					rtw89_pci_interrupt_handler,
					rtw89_pci_interrupt_threadfn,
					IRQF_SHARED, KBUILD_MODNAME, rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to request threaded irq\n");
		goto err_free_vector;
	}

	rtw89_chip_config_intr_mask(rtwdev, RTW89_PCI_INTR_MASK_RESET);

	return 0;

err_free_vector:
	pci_free_irq_vectors(pdev);
err:
	return ret;
}

static void rtw89_pci_free_irq(struct rtw89_dev *rtwdev,
			       struct pci_dev *pdev)
{
	devm_free_irq(rtwdev->dev, pdev->irq, rtwdev);
	pci_free_irq_vectors(pdev);
}

static u16 gray_code_to_bin(u16 gray_code)
{
	u16 binary = gray_code;

	while (gray_code) {
		gray_code >>= 1;
		binary ^= gray_code;
	}

	return binary;
}

static int rtw89_pci_filter_out(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;
	u16 val16, filter_out_val;
	u32 val, phy_offset;
	int ret;

	if (rtwdev->chip->chip_id != RTL8852C)
		return 0;

	val = rtw89_read32_mask(rtwdev, R_AX_PCIE_MIX_CFG_V1, B_AX_ASPM_CTRL_MASK);
	if (val == B_AX_ASPM_CTRL_L1)
		return 0;

	ret = pci_read_config_dword(pdev, RTW89_PCIE_L1_STS_V1, &val);
	if (ret)
		return ret;

	val = FIELD_GET(RTW89_BCFG_LINK_SPEED_MASK, val);
	if (val == RTW89_PCIE_GEN1_SPEED) {
		phy_offset = R_RAC_DIRECT_OFFSET_G1;
	} else if (val == RTW89_PCIE_GEN2_SPEED) {
		phy_offset = R_RAC_DIRECT_OFFSET_G2;
		val16 = rtw89_read16(rtwdev, phy_offset + RAC_ANA10 * RAC_MULT);
		rtw89_write16_set(rtwdev, phy_offset + RAC_ANA10 * RAC_MULT,
				  val16 | B_PCIE_BIT_PINOUT_DIS);
		rtw89_write16_set(rtwdev, phy_offset + RAC_ANA19 * RAC_MULT,
				  val16 & ~B_PCIE_BIT_RD_SEL);

		val16 = rtw89_read16_mask(rtwdev,
					  phy_offset + RAC_ANA1F * RAC_MULT,
					  FILTER_OUT_EQ_MASK);
		val16 = gray_code_to_bin(val16);
		filter_out_val = rtw89_read16(rtwdev, phy_offset + RAC_ANA24 *
					      RAC_MULT);
		filter_out_val &= ~REG_FILTER_OUT_MASK;
		filter_out_val |= FIELD_PREP(REG_FILTER_OUT_MASK, val16);

		rtw89_write16(rtwdev, phy_offset + RAC_ANA24 * RAC_MULT,
			      filter_out_val);
		rtw89_write16_set(rtwdev, phy_offset + RAC_ANA0A * RAC_MULT,
				  B_BAC_EQ_SEL);
		rtw89_write16_set(rtwdev,
				  R_RAC_DIRECT_OFFSET_G1 + RAC_ANA0C * RAC_MULT,
				  B_PCIE_BIT_PSAVE);
	} else {
		return -EOPNOTSUPP;
	}
	rtw89_write16_set(rtwdev, phy_offset + RAC_ANA0C * RAC_MULT,
			  B_PCIE_BIT_PSAVE);

	return 0;
}

static void rtw89_pci_clkreq_set(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_gen_def *gen_def = info->gen_def;

	if (rtw89_pci_disable_clkreq)
		return;

	gen_def->clkreq_set(rtwdev, enable);
}

static void rtw89_pci_clkreq_set_ax(struct rtw89_dev *rtwdev, bool enable)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	int ret;

	ret = rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_CLK_CTRL,
					  PCIE_CLKDLY_HW_30US);
	if (ret)
		rtw89_err(rtwdev, "failed to set CLKREQ Delay\n");

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		if (enable)
			ret = rtw89_pci_config_byte_set(rtwdev,
							RTW89_PCIE_L1_CTRL,
							RTW89_PCIE_BIT_CLK);
		else
			ret = rtw89_pci_config_byte_clr(rtwdev,
							RTW89_PCIE_L1_CTRL,
							RTW89_PCIE_BIT_CLK);
		if (ret)
			rtw89_err(rtwdev, "failed to %s CLKREQ_L1, ret=%d",
				  enable ? "set" : "unset", ret);
	} else if (chip_id == RTL8852C) {
		rtw89_write32_set(rtwdev, R_AX_PCIE_LAT_CTRL,
				  B_AX_CLK_REQ_SEL_OPT | B_AX_CLK_REQ_SEL);
		if (enable)
			rtw89_write32_set(rtwdev, R_AX_L1_CLK_CTRL,
					  B_AX_CLK_REQ_N);
		else
			rtw89_write32_clr(rtwdev, R_AX_L1_CLK_CTRL,
					  B_AX_CLK_REQ_N);
	}
}

static void rtw89_pci_aspm_set(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_gen_def *gen_def = info->gen_def;

	if (rtw89_pci_disable_aspm_l1)
		return;

	gen_def->aspm_set(rtwdev, enable);
}

static void rtw89_pci_aspm_set_ax(struct rtw89_dev *rtwdev, bool enable)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	u8 value = 0;
	int ret;

	ret = rtw89_pci_read_config_byte(rtwdev, RTW89_PCIE_ASPM_CTRL, &value);
	if (ret)
		rtw89_warn(rtwdev, "failed to read ASPM Delay\n");

	u8p_replace_bits(&value, PCIE_L1DLY_16US, RTW89_L1DLY_MASK);
	u8p_replace_bits(&value, PCIE_L0SDLY_4US, RTW89_L0DLY_MASK);

	ret = rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_ASPM_CTRL, value);
	if (ret)
		rtw89_warn(rtwdev, "failed to read ASPM Delay\n");

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		if (enable)
			ret = rtw89_pci_config_byte_set(rtwdev,
							RTW89_PCIE_L1_CTRL,
							RTW89_PCIE_BIT_L1);
		else
			ret = rtw89_pci_config_byte_clr(rtwdev,
							RTW89_PCIE_L1_CTRL,
							RTW89_PCIE_BIT_L1);
	} else if (chip_id == RTL8852C) {
		if (enable)
			rtw89_write32_set(rtwdev, R_AX_PCIE_MIX_CFG_V1,
					  B_AX_ASPM_CTRL_L1);
		else
			rtw89_write32_clr(rtwdev, R_AX_PCIE_MIX_CFG_V1,
					  B_AX_ASPM_CTRL_L1);
	}
	if (ret)
		rtw89_err(rtwdev, "failed to %s ASPM L1, ret=%d",
			  enable ? "set" : "unset", ret);
}

static void rtw89_pci_recalc_int_mit(struct rtw89_dev *rtwdev)
{
	enum rtw89_chip_gen chip_gen = rtwdev->chip->chip_gen;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	struct rtw89_traffic_stats *stats = &rtwdev->stats;
	enum rtw89_tfc_lv tx_tfc_lv = stats->tx_tfc_lv;
	enum rtw89_tfc_lv rx_tfc_lv = stats->rx_tfc_lv;
	u32 val = 0;

	if (rtwdev->scanning ||
	    (tx_tfc_lv < RTW89_TFC_HIGH && rx_tfc_lv < RTW89_TFC_HIGH))
		goto out;

	if (chip_gen == RTW89_CHIP_BE)
		val = B_BE_PCIE_MIT_RX0P2_EN | B_BE_PCIE_MIT_RX0P1_EN;
	else
		val = B_AX_RXMIT_RXP2_SEL | B_AX_RXMIT_RXP1_SEL |
		      FIELD_PREP(B_AX_RXCOUNTER_MATCH_MASK, RTW89_PCI_RXBD_NUM_MAX / 2) |
		      FIELD_PREP(B_AX_RXTIMER_UNIT_MASK, AX_RXTIMER_UNIT_64US) |
		      FIELD_PREP(B_AX_RXTIMER_MATCH_MASK, 2048 / 64);

out:
	rtw89_write32(rtwdev, info->mit_addr, val);
}

static void rtw89_pci_link_cfg(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;
	u16 link_ctrl;
	int ret;

	/* Though there is standard PCIE configuration space to set the
	 * link control register, but by Realtek's design, driver should
	 * check if host supports CLKREQ/ASPM to enable the HW module.
	 *
	 * These functions are implemented by two HW modules associated,
	 * one is responsible to access PCIE configuration space to
	 * follow the host settings, and another is in charge of doing
	 * CLKREQ/ASPM mechanisms, it is default disabled. Because sometimes
	 * the host does not support it, and due to some reasons or wrong
	 * settings (ex. CLKREQ# not Bi-Direction), it could lead to device
	 * loss if HW misbehaves on the link.
	 *
	 * Hence it's designed that driver should first check the PCIE
	 * configuration space is sync'ed and enabled, then driver can turn
	 * on the other module that is actually working on the mechanism.
	 */
	ret = pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &link_ctrl);
	if (ret) {
		rtw89_err(rtwdev, "failed to read PCI cap, ret=%d\n", ret);
		return;
	}

	if (link_ctrl & PCI_EXP_LNKCTL_CLKREQ_EN)
		rtw89_pci_clkreq_set(rtwdev, true);

	if (link_ctrl & PCI_EXP_LNKCTL_ASPM_L1)
		rtw89_pci_aspm_set(rtwdev, true);
}

static void rtw89_pci_l1ss_set(struct rtw89_dev *rtwdev, bool enable)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_gen_def *gen_def = info->gen_def;

	if (rtw89_pci_disable_l1ss)
		return;

	gen_def->l1ss_set(rtwdev, enable);
}

static void rtw89_pci_l1ss_set_ax(struct rtw89_dev *rtwdev, bool enable)
{
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;
	int ret;

	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		if (enable)
			ret = rtw89_pci_config_byte_set(rtwdev,
							RTW89_PCIE_TIMER_CTRL,
							RTW89_PCIE_BIT_L1SUB);
		else
			ret = rtw89_pci_config_byte_clr(rtwdev,
							RTW89_PCIE_TIMER_CTRL,
							RTW89_PCIE_BIT_L1SUB);
		if (ret)
			rtw89_err(rtwdev, "failed to %s L1SS, ret=%d",
				  enable ? "set" : "unset", ret);
	} else if (chip_id == RTL8852C) {
		ret = rtw89_pci_config_byte_clr(rtwdev, RTW89_PCIE_L1SS_STS_V1,
						RTW89_PCIE_BIT_ASPM_L11 |
						RTW89_PCIE_BIT_PCI_L11);
		if (ret)
			rtw89_warn(rtwdev, "failed to unset ASPM L1.1, ret=%d", ret);
		if (enable)
			rtw89_write32_clr(rtwdev, R_AX_PCIE_MIX_CFG_V1,
					  B_AX_L1SUB_DISABLE);
		else
			rtw89_write32_set(rtwdev, R_AX_PCIE_MIX_CFG_V1,
					  B_AX_L1SUB_DISABLE);
	}
}

static void rtw89_pci_l1ss_cfg(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;
	u32 l1ss_cap_ptr, l1ss_ctrl;

	if (rtw89_pci_disable_l1ss)
		return;

	l1ss_cap_ptr = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_ptr)
		return;

	pci_read_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL1, &l1ss_ctrl);

	if (l1ss_ctrl & PCI_L1SS_CTL1_L1SS_MASK)
		rtw89_pci_l1ss_set(rtwdev, true);
}

static void rtw89_pci_cpl_timeout_cfg(struct rtw89_dev *rtwdev)
{
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	struct pci_dev *pdev = rtwpci->pdev;

	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL2,
				 PCI_EXP_DEVCTL2_COMP_TMOUT_DIS);
}

static int rtw89_pci_poll_io_idle_ax(struct rtw89_dev *rtwdev)
{
	int ret = 0;
	u32 sts;
	u32 busy = B_AX_PCIEIO_BUSY | B_AX_PCIEIO_TX_BUSY | B_AX_PCIEIO_RX_BUSY;

	ret = read_poll_timeout_atomic(rtw89_read32, sts, (sts & busy) == 0x0,
				       10, 1000, false, rtwdev,
				       R_AX_PCIE_DMA_BUSY1);
	if (ret) {
		rtw89_err(rtwdev, "pci dmach busy1 0x%X\n",
			  rtw89_read32(rtwdev, R_AX_PCIE_DMA_BUSY1));
		return -EINVAL;
	}
	return ret;
}

static int rtw89_pci_lv1rst_stop_dma_ax(struct rtw89_dev *rtwdev)
{
	u32 val;
	int ret;

	if (rtwdev->chip->chip_id == RTL8852C)
		return 0;

	rtw89_pci_ctrl_dma_all(rtwdev, false);
	ret = rtw89_pci_poll_io_idle_ax(rtwdev);
	if (ret) {
		val = rtw89_read32(rtwdev, R_AX_DBG_ERR_FLAG);
		rtw89_debug(rtwdev, RTW89_DBG_HCI,
			    "[PCIe] poll_io_idle fail, before 0x%08x: 0x%08x\n",
			    R_AX_DBG_ERR_FLAG, val);
		if (val & B_AX_TX_STUCK || val & B_AX_PCIE_TXBD_LEN0)
			rtw89_mac_ctrl_hci_dma_tx(rtwdev, false);
		if (val & B_AX_RX_STUCK)
			rtw89_mac_ctrl_hci_dma_rx(rtwdev, false);
		rtw89_mac_ctrl_hci_dma_trx(rtwdev, true);
		ret = rtw89_pci_poll_io_idle_ax(rtwdev);
		val = rtw89_read32(rtwdev, R_AX_DBG_ERR_FLAG);
		rtw89_debug(rtwdev, RTW89_DBG_HCI,
			    "[PCIe] poll_io_idle fail, after 0x%08x: 0x%08x\n",
			    R_AX_DBG_ERR_FLAG, val);
	}

	return ret;
}

static int rtw89_pci_lv1rst_start_dma_ax(struct rtw89_dev *rtwdev)
{
	u32 ret;

	if (rtwdev->chip->chip_id == RTL8852C)
		return 0;

	rtw89_mac_ctrl_hci_dma_trx(rtwdev, false);
	rtw89_mac_ctrl_hci_dma_trx(rtwdev, true);
	rtw89_pci_clr_idx_all(rtwdev);

	ret = rtw89_pci_rst_bdram_ax(rtwdev);
	if (ret)
		return ret;

	rtw89_pci_ctrl_dma_all(rtwdev, true);
	return ret;
}

static int rtw89_pci_ops_mac_lv1_recovery(struct rtw89_dev *rtwdev,
					  enum rtw89_lv1_rcvy_step step)
{
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_gen_def *gen_def = info->gen_def;
	int ret;

	switch (step) {
	case RTW89_LV1_RCVY_STEP_1:
		ret = gen_def->lv1rst_stop_dma(rtwdev);
		if (ret)
			rtw89_err(rtwdev, "lv1 rcvy pci stop dma fail\n");

		break;

	case RTW89_LV1_RCVY_STEP_2:
		ret = gen_def->lv1rst_start_dma(rtwdev);
		if (ret)
			rtw89_err(rtwdev, "lv1 rcvy pci start dma fail\n");
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static void rtw89_pci_ops_dump_err_status(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_gen == RTW89_CHIP_BE)
		return;

	if (rtwdev->chip->chip_id == RTL8852C) {
		rtw89_info(rtwdev, "R_AX_DBG_ERR_FLAG=0x%08x\n",
			   rtw89_read32(rtwdev, R_AX_DBG_ERR_FLAG_V1));
		rtw89_info(rtwdev, "R_AX_LBC_WATCHDOG=0x%08x\n",
			   rtw89_read32(rtwdev, R_AX_LBC_WATCHDOG_V1));
	} else {
		rtw89_info(rtwdev, "R_AX_RPQ_RXBD_IDX =0x%08x\n",
			   rtw89_read32(rtwdev, R_AX_RPQ_RXBD_IDX));
		rtw89_info(rtwdev, "R_AX_DBG_ERR_FLAG=0x%08x\n",
			   rtw89_read32(rtwdev, R_AX_DBG_ERR_FLAG));
		rtw89_info(rtwdev, "R_AX_LBC_WATCHDOG=0x%08x\n",
			   rtw89_read32(rtwdev, R_AX_LBC_WATCHDOG));
	}
}

static int rtw89_pci_napi_poll(struct napi_struct *napi, int budget)
{
	struct rtw89_dev *rtwdev = container_of(napi, struct rtw89_dev, napi);
	struct rtw89_pci *rtwpci = (struct rtw89_pci *)rtwdev->priv;
	const struct rtw89_pci_info *info = rtwdev->pci_info;
	const struct rtw89_pci_gen_def *gen_def = info->gen_def;
	unsigned long flags;
	int work_done;

	rtwdev->napi_budget_countdown = budget;

	rtw89_write32(rtwdev, gen_def->isr_clear_rpq.addr, gen_def->isr_clear_rpq.data);
	work_done = rtw89_pci_poll_rpq_dma(rtwdev, rtwpci, rtwdev->napi_budget_countdown);
	if (work_done == budget)
		return budget;

	rtw89_write32(rtwdev, gen_def->isr_clear_rxq.addr, gen_def->isr_clear_rxq.data);
	work_done += rtw89_pci_poll_rxq_dma(rtwdev, rtwpci, rtwdev->napi_budget_countdown);
	if (work_done < budget && napi_complete_done(napi, work_done)) {
		spin_lock_irqsave(&rtwpci->irq_lock, flags);
		if (likely(rtwpci->running))
			rtw89_chip_enable_intr(rtwdev, rtwpci);
		spin_unlock_irqrestore(&rtwpci->irq_lock, flags);
	}

	return work_done;
}

static
void rtw89_check_pci_ssid_quirks(struct rtw89_dev *rtwdev,
				 struct pci_dev *pdev,
				 const struct rtw89_pci_ssid_quirk *ssid_quirks)
{
	int i;

	if (!ssid_quirks)
		return;

	for (i = 0; i < 200; i++, ssid_quirks++) {
		if (ssid_quirks->vendor == 0 && ssid_quirks->device == 0)
			break;

		if (ssid_quirks->vendor != pdev->vendor ||
		    ssid_quirks->device != pdev->device ||
		    ssid_quirks->subsystem_vendor != pdev->subsystem_vendor ||
		    ssid_quirks->subsystem_device != pdev->subsystem_device)
			continue;

		bitmap_or(rtwdev->quirks, rtwdev->quirks, &ssid_quirks->bitmap,
			  NUM_OF_RTW89_QUIRKS);
		rtwdev->custid = ssid_quirks->custid;
		break;
	}

	rtw89_debug(rtwdev, RTW89_DBG_HCI, "quirks=%*ph custid=%d\n",
		    (int)sizeof(rtwdev->quirks), rtwdev->quirks, rtwdev->custid);
}

static int __maybe_unused rtw89_pci_suspend(struct device *dev)
{
	struct ieee80211_hw *hw = dev_get_drvdata(dev);
	struct rtw89_dev *rtwdev = hw->priv;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;

	rtw89_write32_set(rtwdev, R_AX_RSV_CTRL, B_AX_WLOCK_1C_BIT6);
	rtw89_write32_set(rtwdev, R_AX_RSV_CTRL, B_AX_R_DIS_PRST);
	rtw89_write32_clr(rtwdev, R_AX_RSV_CTRL, B_AX_WLOCK_1C_BIT6);
	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		rtw89_write32_clr(rtwdev, R_AX_SYS_SDIO_CTRL,
				  B_AX_PCIE_DIS_L2_CTRL_LDO_HCI);
		rtw89_write32_set(rtwdev, R_AX_PCIE_INIT_CFG1,
				  B_AX_PCIE_PERST_KEEP_REG | B_AX_PCIE_TRAIN_KEEP_REG);
	} else {
		rtw89_write32_clr(rtwdev, R_AX_PCIE_PS_CTRL_V1,
				  B_AX_CMAC_EXIT_L1_EN | B_AX_DMAC0_EXIT_L1_EN);
	}

	return 0;
}

static void rtw89_pci_l2_hci_ldo(struct rtw89_dev *rtwdev)
{
	if (rtwdev->chip->chip_id == RTL8852C)
		return;

	/* Hardware need write the reg twice to ensure the setting work */
	rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_RST_MSTATE,
				    RTW89_PCIE_BIT_CFG_RST_MSTATE);
	rtw89_pci_write_config_byte(rtwdev, RTW89_PCIE_RST_MSTATE,
				    RTW89_PCIE_BIT_CFG_RST_MSTATE);
}

void rtw89_pci_basic_cfg(struct rtw89_dev *rtwdev, bool resume)
{
	if (resume)
		rtw89_pci_cfg_dac(rtwdev);

	rtw89_pci_disable_eq(rtwdev);
	rtw89_pci_filter_out(rtwdev);
	rtw89_pci_cpl_timeout_cfg(rtwdev);
	rtw89_pci_link_cfg(rtwdev);
	rtw89_pci_l1ss_cfg(rtwdev);
}

static int __maybe_unused rtw89_pci_resume(struct device *dev)
{
	struct ieee80211_hw *hw = dev_get_drvdata(dev);
	struct rtw89_dev *rtwdev = hw->priv;
	enum rtw89_core_chip_id chip_id = rtwdev->chip->chip_id;

	rtw89_write32_set(rtwdev, R_AX_RSV_CTRL, B_AX_WLOCK_1C_BIT6);
	rtw89_write32_clr(rtwdev, R_AX_RSV_CTRL, B_AX_R_DIS_PRST);
	rtw89_write32_clr(rtwdev, R_AX_RSV_CTRL, B_AX_WLOCK_1C_BIT6);
	if (chip_id == RTL8852A || rtw89_is_rtl885xb(rtwdev)) {
		rtw89_write32_set(rtwdev, R_AX_SYS_SDIO_CTRL,
				  B_AX_PCIE_DIS_L2_CTRL_LDO_HCI);
		rtw89_write32_clr(rtwdev, R_AX_PCIE_INIT_CFG1,
				  B_AX_PCIE_PERST_KEEP_REG | B_AX_PCIE_TRAIN_KEEP_REG);
	} else {
		rtw89_write32_set(rtwdev, R_AX_PCIE_PS_CTRL_V1,
				  B_AX_CMAC_EXIT_L1_EN | B_AX_DMAC0_EXIT_L1_EN);
		rtw89_write32_clr(rtwdev, R_AX_PCIE_PS_CTRL_V1,
				  B_AX_SEL_REQ_ENTR_L1);
	}
	rtw89_pci_l2_hci_ldo(rtwdev);

	rtw89_pci_basic_cfg(rtwdev, true);

	return 0;
}

SIMPLE_DEV_PM_OPS(rtw89_pm_ops, rtw89_pci_suspend, rtw89_pci_resume);
EXPORT_SYMBOL(rtw89_pm_ops);

const struct rtw89_pci_gen_def rtw89_pci_gen_ax = {
	.isr_rdu = B_AX_RDU_INT,
	.isr_halt_c2h = B_AX_HALT_C2H_INT_EN,
	.isr_wdt_timeout = B_AX_WDT_TIMEOUT_INT_EN,
	.isr_clear_rpq = {R_AX_PCIE_HISR00, B_AX_RPQDMA_INT | B_AX_RPQBD_FULL_INT},
	.isr_clear_rxq = {R_AX_PCIE_HISR00, B_AX_RXP1DMA_INT | B_AX_RXDMA_INT |
					    B_AX_RDU_INT},

	.mac_pre_init = rtw89_pci_ops_mac_pre_init_ax,
	.mac_pre_deinit = rtw89_pci_ops_mac_pre_deinit_ax,
	.mac_post_init = rtw89_pci_ops_mac_post_init_ax,

	.clr_idx_all = rtw89_pci_clr_idx_all_ax,
	.rst_bdram = rtw89_pci_rst_bdram_ax,

	.lv1rst_stop_dma = rtw89_pci_lv1rst_stop_dma_ax,
	.lv1rst_start_dma = rtw89_pci_lv1rst_start_dma_ax,

	.ctrl_txdma_ch = rtw89_pci_ctrl_txdma_ch_ax,
	.ctrl_txdma_fw_ch = rtw89_pci_ctrl_txdma_fw_ch_ax,
	.poll_txdma_ch_idle = rtw89_pci_poll_txdma_ch_idle_ax,

	.aspm_set = rtw89_pci_aspm_set_ax,
	.clkreq_set = rtw89_pci_clkreq_set_ax,
	.l1ss_set = rtw89_pci_l1ss_set_ax,

	.disable_eq = rtw89_pci_disable_eq_ax,
	.power_wake = rtw89_pci_power_wake_ax,
};
EXPORT_SYMBOL(rtw89_pci_gen_ax);

static const struct rtw89_hci_ops rtw89_pci_ops = {
	.tx_write	= rtw89_pci_ops_tx_write,
	.tx_kick_off	= rtw89_pci_ops_tx_kick_off,
	.flush_queues	= rtw89_pci_ops_flush_queues,
	.reset		= rtw89_pci_ops_reset,
	.start		= rtw89_pci_ops_start,
	.stop		= rtw89_pci_ops_stop,
	.pause		= rtw89_pci_ops_pause,
	.switch_mode	= rtw89_pci_ops_switch_mode,
	.recalc_int_mit = rtw89_pci_recalc_int_mit,

	.read8		= rtw89_pci_ops_read8,
	.read16		= rtw89_pci_ops_read16,
	.read32		= rtw89_pci_ops_read32,
	.write8		= rtw89_pci_ops_write8,
	.write16	= rtw89_pci_ops_write16,
	.write32	= rtw89_pci_ops_write32,

	.mac_pre_init	= rtw89_pci_ops_mac_pre_init,
	.mac_pre_deinit	= rtw89_pci_ops_mac_pre_deinit,
	.mac_post_init	= rtw89_pci_ops_mac_post_init,
	.deinit		= rtw89_pci_ops_deinit,

	.check_and_reclaim_tx_resource = rtw89_pci_check_and_reclaim_tx_resource,
	.mac_lv1_rcvy	= rtw89_pci_ops_mac_lv1_recovery,
	.dump_err_status = rtw89_pci_ops_dump_err_status,
	.napi_poll	= rtw89_pci_napi_poll,

	.recovery_start = rtw89_pci_ops_recovery_start,
	.recovery_complete = rtw89_pci_ops_recovery_complete,

	.ctrl_txdma_ch	= rtw89_pci_ctrl_txdma_ch,
	.ctrl_txdma_fw_ch = rtw89_pci_ctrl_txdma_fw_ch,
	.ctrl_trxhci	= rtw89_pci_ctrl_dma_trx,
	.poll_txdma_ch_idle = rtw89_pci_poll_txdma_ch_idle,

	.clr_idx_all	= rtw89_pci_clr_idx_all,
	.clear		= rtw89_pci_clear_resource,
	.disable_intr	= rtw89_pci_disable_intr_lock,
	.enable_intr	= rtw89_pci_enable_intr_lock,
	.rst_bdram	= rtw89_pci_reset_bdram,
};

int rtw89_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct rtw89_dev *rtwdev;
	const struct rtw89_driver_info *info;
	const struct rtw89_pci_info *pci_info;
	int ret;

	info = (const struct rtw89_driver_info *)id->driver_data;

	rtwdev = rtw89_alloc_ieee80211_hw(&pdev->dev,
					  sizeof(struct rtw89_pci),
					  info->chip, info->variant);
	if (!rtwdev) {
		dev_err(&pdev->dev, "failed to allocate hw\n");
		return -ENOMEM;
	}

	pci_info = info->bus.pci;

	rtwdev->pci_info = info->bus.pci;
	rtwdev->hci.ops = &rtw89_pci_ops;
	rtwdev->hci.type = RTW89_HCI_TYPE_PCIE;
	rtwdev->hci.rpwm_addr = pci_info->rpwm_addr;
	rtwdev->hci.cpwm_addr = pci_info->cpwm_addr;

	rtw89_check_quirks(rtwdev, info->quirks);
	rtw89_check_pci_ssid_quirks(rtwdev, pdev, pci_info->ssid_quirks);

	SET_IEEE80211_DEV(rtwdev->hw, &pdev->dev);

	ret = rtw89_core_init(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to initialise core\n");
		goto err_release_hw;
	}

	ret = rtw89_pci_claim_device(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to claim pci device\n");
		goto err_core_deinit;
	}

	ret = rtw89_pci_setup_resource(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to setup pci resource\n");
		goto err_declaim_pci;
	}

	ret = rtw89_chip_info_setup(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to setup chip information\n");
		goto err_clear_resource;
	}

	rtw89_pci_basic_cfg(rtwdev, false);

	ret = rtw89_core_napi_init(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to init napi\n");
		goto err_clear_resource;
	}

	ret = rtw89_pci_request_irq(rtwdev, pdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to request pci irq\n");
		goto err_deinit_napi;
	}

	ret = rtw89_core_register(rtwdev);
	if (ret) {
		rtw89_err(rtwdev, "failed to register core\n");
		goto err_free_irq;
	}

	set_bit(RTW89_FLAG_PROBE_DONE, rtwdev->flags);

	return 0;

err_free_irq:
	rtw89_pci_free_irq(rtwdev, pdev);
err_deinit_napi:
	rtw89_core_napi_deinit(rtwdev);
err_clear_resource:
	rtw89_pci_clear_resource(rtwdev, pdev);
err_declaim_pci:
	rtw89_pci_declaim_device(rtwdev, pdev);
err_core_deinit:
	rtw89_core_deinit(rtwdev);
err_release_hw:
	rtw89_free_ieee80211_hw(rtwdev);

	return ret;
}
EXPORT_SYMBOL(rtw89_pci_probe);

void rtw89_pci_remove(struct pci_dev *pdev)
{
	struct ieee80211_hw *hw = pci_get_drvdata(pdev);
	struct rtw89_dev *rtwdev;

	rtwdev = hw->priv;

	rtw89_pci_free_irq(rtwdev, pdev);
	rtw89_core_napi_deinit(rtwdev);
	rtw89_core_unregister(rtwdev);
	rtw89_pci_clear_resource(rtwdev, pdev);
	rtw89_pci_declaim_device(rtwdev, pdev);
	rtw89_core_deinit(rtwdev);
	rtw89_free_ieee80211_hw(rtwdev);
}
EXPORT_SYMBOL(rtw89_pci_remove);

MODULE_AUTHOR("Realtek Corporation");
MODULE_DESCRIPTION("Realtek PCI 802.11ax wireless driver");
MODULE_LICENSE("Dual BSD/GPL");
#if defined(__FreeBSD__)
MODULE_VERSION(rtw89_pci, 1);
MODULE_DEPEND(rtw89_pci, linuxkpi, 1, 1, 1);
MODULE_DEPEND(rtw89_pci, linuxkpi_wlan, 1, 1, 1);
#ifdef CONFIG_RTW89_DEBUGFS
MODULE_DEPEND(rtw89_pci, lindebugfs, 1, 1, 1);
#endif
#endif
