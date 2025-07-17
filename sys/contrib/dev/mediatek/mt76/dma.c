// SPDX-License-Identifier: ISC
/*
 * Copyright (C) 2016 Felix Fietkau <nbd@nbd.name>
 */

#include <linux/dma-mapping.h>
#if defined(__FreeBSD__)
#include <linux/cache.h>
#include <net/page_pool.h>
#endif
#include "mt76.h"
#include "dma.h"

#if IS_ENABLED(CONFIG_NET_MEDIATEK_SOC_WED)

#define Q_READ(_q, _field) ({						\
	u32 _offset = offsetof(struct mt76_queue_regs, _field);		\
	u32 _val;							\
	if ((_q)->flags & MT_QFLAG_WED)					\
		_val = mtk_wed_device_reg_read((_q)->wed,		\
					       ((_q)->wed_regs +	\
					        _offset));		\
	else								\
		_val = readl(&(_q)->regs->_field);			\
	_val;								\
})

#define Q_WRITE(_q, _field, _val)	do {				\
	u32 _offset = offsetof(struct mt76_queue_regs, _field);		\
	if ((_q)->flags & MT_QFLAG_WED)					\
		mtk_wed_device_reg_write((_q)->wed,			\
					 ((_q)->wed_regs + _offset),	\
					 _val);				\
	else								\
		writel(_val, &(_q)->regs->_field);			\
} while (0)

#else

#define Q_READ(_q, _field)		readl(&(_q)->regs->_field)
#define Q_WRITE(_q, _field, _val)	writel(_val, &(_q)->regs->_field)

#endif

static struct mt76_txwi_cache *
mt76_alloc_txwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t;
	dma_addr_t addr;
	u8 *txwi;
	int size;

	size = L1_CACHE_ALIGN(dev->drv->txwi_size + sizeof(*t));
	txwi = kzalloc(size, GFP_ATOMIC);
	if (!txwi)
		return NULL;

	addr = dma_map_single(dev->dma_dev, txwi, dev->drv->txwi_size,
			      DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(dev->dma_dev, addr))) {
		kfree(txwi);
		return NULL;
	}

	t = (struct mt76_txwi_cache *)(txwi + dev->drv->txwi_size);
	t->dma_addr = addr;

	return t;
}

static struct mt76_txwi_cache *
mt76_alloc_rxwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t;

	t = kzalloc(L1_CACHE_ALIGN(sizeof(*t)), GFP_ATOMIC);
	if (!t)
		return NULL;

	t->ptr = NULL;
	return t;
}

static struct mt76_txwi_cache *
__mt76_get_txwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t = NULL;

	spin_lock(&dev->lock);
	if (!list_empty(&dev->txwi_cache)) {
		t = list_first_entry(&dev->txwi_cache, struct mt76_txwi_cache,
				     list);
		list_del(&t->list);
	}
	spin_unlock(&dev->lock);

	return t;
}

static struct mt76_txwi_cache *
__mt76_get_rxwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t = NULL;

	spin_lock_bh(&dev->wed_lock);
	if (!list_empty(&dev->rxwi_cache)) {
		t = list_first_entry(&dev->rxwi_cache, struct mt76_txwi_cache,
				     list);
		list_del(&t->list);
	}
	spin_unlock_bh(&dev->wed_lock);

	return t;
}

static struct mt76_txwi_cache *
mt76_get_txwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t = __mt76_get_txwi(dev);

	if (t)
		return t;

	return mt76_alloc_txwi(dev);
}

struct mt76_txwi_cache *
mt76_get_rxwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t = __mt76_get_rxwi(dev);

	if (t)
		return t;

	return mt76_alloc_rxwi(dev);
}
EXPORT_SYMBOL_GPL(mt76_get_rxwi);

void
mt76_put_txwi(struct mt76_dev *dev, struct mt76_txwi_cache *t)
{
	if (!t)
		return;

	spin_lock(&dev->lock);
	list_add(&t->list, &dev->txwi_cache);
	spin_unlock(&dev->lock);
}
EXPORT_SYMBOL_GPL(mt76_put_txwi);

void
mt76_put_rxwi(struct mt76_dev *dev, struct mt76_txwi_cache *t)
{
	if (!t)
		return;

	spin_lock_bh(&dev->wed_lock);
	list_add(&t->list, &dev->rxwi_cache);
	spin_unlock_bh(&dev->wed_lock);
}
EXPORT_SYMBOL_GPL(mt76_put_rxwi);

static void
mt76_free_pending_txwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t;

	local_bh_disable();
	while ((t = __mt76_get_txwi(dev)) != NULL) {
		dma_unmap_single(dev->dma_dev, t->dma_addr, dev->drv->txwi_size,
				 DMA_TO_DEVICE);
		kfree(mt76_get_txwi_ptr(dev, t));
	}
	local_bh_enable();
}

void
mt76_free_pending_rxwi(struct mt76_dev *dev)
{
	struct mt76_txwi_cache *t;

	local_bh_disable();
	while ((t = __mt76_get_rxwi(dev)) != NULL) {
		if (t->ptr)
			mt76_put_page_pool_buf(t->ptr, false);
		kfree(t);
	}
	local_bh_enable();
}
EXPORT_SYMBOL_GPL(mt76_free_pending_rxwi);

static void
mt76_dma_sync_idx(struct mt76_dev *dev, struct mt76_queue *q)
{
	Q_WRITE(q, desc_base, q->desc_dma);
	if (q->flags & MT_QFLAG_WED_RRO_EN)
		Q_WRITE(q, ring_size, MT_DMA_RRO_EN | q->ndesc);
	else
		Q_WRITE(q, ring_size, q->ndesc);
	q->head = Q_READ(q, dma_idx);
	q->tail = q->head;
}

void __mt76_dma_queue_reset(struct mt76_dev *dev, struct mt76_queue *q,
			    bool reset_idx)
{
	if (!q || !q->ndesc)
		return;

	if (!mt76_queue_is_wed_rro_ind(q)) {
		int i;

		/* clear descriptors */
		for (i = 0; i < q->ndesc; i++)
			q->desc[i].ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);
	}

	if (reset_idx) {
		Q_WRITE(q, cpu_idx, 0);
		Q_WRITE(q, dma_idx, 0);
	}
	mt76_dma_sync_idx(dev, q);
}

void mt76_dma_queue_reset(struct mt76_dev *dev, struct mt76_queue *q)
{
	__mt76_dma_queue_reset(dev, q, true);
}

static int
mt76_dma_add_rx_buf(struct mt76_dev *dev, struct mt76_queue *q,
		    struct mt76_queue_buf *buf, void *data)
{
	struct mt76_queue_entry *entry = &q->entry[q->head];
	struct mt76_txwi_cache *txwi = NULL;
	struct mt76_desc *desc;
	int idx = q->head;
	u32 buf1 = 0, ctrl;
	int rx_token;

	if (mt76_queue_is_wed_rro_ind(q)) {
		struct mt76_wed_rro_desc *rro_desc;

		rro_desc = (struct mt76_wed_rro_desc *)q->desc;
		data = &rro_desc[q->head];
		goto done;
	}

	desc = &q->desc[q->head];
	ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, buf[0].len);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	buf1 = FIELD_PREP(MT_DMA_CTL_SDP0_H, buf->addr >> 32);
#endif

	if (mt76_queue_is_wed_rx(q)) {
		txwi = mt76_get_rxwi(dev);
		if (!txwi)
			return -ENOMEM;

		rx_token = mt76_rx_token_consume(dev, data, txwi, buf->addr);
		if (rx_token < 0) {
			mt76_put_rxwi(dev, txwi);
			return -ENOMEM;
		}

		buf1 |= FIELD_PREP(MT_DMA_CTL_TOKEN, rx_token);
		ctrl |= MT_DMA_CTL_TO_HOST;
	}

	WRITE_ONCE(desc->buf0, cpu_to_le32(buf->addr));
	WRITE_ONCE(desc->buf1, cpu_to_le32(buf1));
	WRITE_ONCE(desc->ctrl, cpu_to_le32(ctrl));
	WRITE_ONCE(desc->info, 0);

done:
	entry->dma_addr[0] = buf->addr;
	entry->dma_len[0] = buf->len;
	entry->txwi = txwi;
	entry->buf = data;
	entry->wcid = 0xffff;
	entry->skip_buf1 = true;
	q->head = (q->head + 1) % q->ndesc;
	q->queued++;

	return idx;
}

static int
mt76_dma_add_buf(struct mt76_dev *dev, struct mt76_queue *q,
		 struct mt76_queue_buf *buf, int nbufs, u32 info,
		 struct sk_buff *skb, void *txwi)
{
	struct mt76_queue_entry *entry;
	struct mt76_desc *desc;
	int i, idx = -1;
	u32 ctrl, next;

	if (txwi) {
		q->entry[q->head].txwi = DMA_DUMMY_DATA;
		q->entry[q->head].skip_buf0 = true;
	}

	for (i = 0; i < nbufs; i += 2, buf += 2) {
		u32 buf0 = buf[0].addr, buf1 = 0;

		idx = q->head;
		next = (q->head + 1) % q->ndesc;

		desc = &q->desc[idx];
		entry = &q->entry[idx];

		if (buf[0].skip_unmap)
			entry->skip_buf0 = true;
		entry->skip_buf1 = i == nbufs - 1;

		entry->dma_addr[0] = buf[0].addr;
		entry->dma_len[0] = buf[0].len;

		ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, buf[0].len);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		info |= FIELD_PREP(MT_DMA_CTL_SDP0_H, buf[0].addr >> 32);
#endif
		if (i < nbufs - 1) {
			entry->dma_addr[1] = buf[1].addr;
			entry->dma_len[1] = buf[1].len;
			buf1 = buf[1].addr;
			ctrl |= FIELD_PREP(MT_DMA_CTL_SD_LEN1, buf[1].len);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			info |= FIELD_PREP(MT_DMA_CTL_SDP1_H,
					   buf[1].addr >> 32);
#endif
			if (buf[1].skip_unmap)
				entry->skip_buf1 = true;
		}

		if (i == nbufs - 1)
			ctrl |= MT_DMA_CTL_LAST_SEC0;
		else if (i == nbufs - 2)
			ctrl |= MT_DMA_CTL_LAST_SEC1;

		WRITE_ONCE(desc->buf0, cpu_to_le32(buf0));
		WRITE_ONCE(desc->buf1, cpu_to_le32(buf1));
		WRITE_ONCE(desc->info, cpu_to_le32(info));
		WRITE_ONCE(desc->ctrl, cpu_to_le32(ctrl));

		q->head = next;
		q->queued++;
	}

	q->entry[idx].txwi = txwi;
	q->entry[idx].skb = skb;
	q->entry[idx].wcid = 0xffff;

	return idx;
}

static void
mt76_dma_tx_cleanup_idx(struct mt76_dev *dev, struct mt76_queue *q, int idx,
			struct mt76_queue_entry *prev_e)
{
	struct mt76_queue_entry *e = &q->entry[idx];

	if (!e->skip_buf0)
		dma_unmap_single(dev->dma_dev, e->dma_addr[0], e->dma_len[0],
				 DMA_TO_DEVICE);

	if (!e->skip_buf1)
		dma_unmap_single(dev->dma_dev, e->dma_addr[1], e->dma_len[1],
				 DMA_TO_DEVICE);

	if (e->txwi == DMA_DUMMY_DATA)
		e->txwi = NULL;

	*prev_e = *e;
	memset(e, 0, sizeof(*e));
}

static void
mt76_dma_kick_queue(struct mt76_dev *dev, struct mt76_queue *q)
{
	wmb();
	Q_WRITE(q, cpu_idx, q->head);
}

static void
mt76_dma_tx_cleanup(struct mt76_dev *dev, struct mt76_queue *q, bool flush)
{
	struct mt76_queue_entry entry;
	int last;

	if (!q || !q->ndesc)
		return;

	spin_lock_bh(&q->cleanup_lock);
	if (flush)
		last = -1;
	else
		last = Q_READ(q, dma_idx);

	while (q->queued > 0 && q->tail != last) {
		mt76_dma_tx_cleanup_idx(dev, q, q->tail, &entry);
		mt76_queue_tx_complete(dev, q, &entry);

		if (entry.txwi) {
			if (!(dev->drv->drv_flags & MT_DRV_TXWI_NO_FREE))
				mt76_put_txwi(dev, entry.txwi);
		}

		if (!flush && q->tail == last)
			last = Q_READ(q, dma_idx);
	}
	spin_unlock_bh(&q->cleanup_lock);

	if (flush) {
		spin_lock_bh(&q->lock);
		mt76_dma_sync_idx(dev, q);
		mt76_dma_kick_queue(dev, q);
		spin_unlock_bh(&q->lock);
	}

	if (!q->queued)
		wake_up(&dev->tx_wait);
}

static void *
mt76_dma_get_buf(struct mt76_dev *dev, struct mt76_queue *q, int idx,
		 int *len, u32 *info, bool *more, bool *drop)
{
	struct mt76_queue_entry *e = &q->entry[idx];
	struct mt76_desc *desc = &q->desc[idx];
	u32 ctrl, desc_info, buf1;
	void *buf = e->buf;

	if (mt76_queue_is_wed_rro_ind(q))
		goto done;

	ctrl = le32_to_cpu(READ_ONCE(desc->ctrl));
	if (len) {
		*len = FIELD_GET(MT_DMA_CTL_SD_LEN0, ctrl);
		*more = !(ctrl & MT_DMA_CTL_LAST_SEC0);
	}

	desc_info = le32_to_cpu(desc->info);
	if (info)
		*info = desc_info;

	buf1 = le32_to_cpu(desc->buf1);
	mt76_dma_should_drop_buf(drop, ctrl, buf1, desc_info);

	if (mt76_queue_is_wed_rx(q)) {
		u32 token = FIELD_GET(MT_DMA_CTL_TOKEN, buf1);
		struct mt76_txwi_cache *t = mt76_rx_token_release(dev, token);

		if (!t)
			return NULL;

		dma_sync_single_for_cpu(dev->dma_dev, t->dma_addr,
				SKB_WITH_OVERHEAD(q->buf_size),
				page_pool_get_dma_dir(q->page_pool));

		buf = t->ptr;
		t->dma_addr = 0;
		t->ptr = NULL;

		mt76_put_rxwi(dev, t);
		if (drop)
			*drop |= !!(buf1 & MT_DMA_CTL_WO_DROP);
	} else {
		dma_sync_single_for_cpu(dev->dma_dev, e->dma_addr[0],
				SKB_WITH_OVERHEAD(q->buf_size),
				page_pool_get_dma_dir(q->page_pool));
	}

done:
	e->buf = NULL;
	return buf;
}

static void *
mt76_dma_dequeue(struct mt76_dev *dev, struct mt76_queue *q, bool flush,
		 int *len, u32 *info, bool *more, bool *drop)
{
	int idx = q->tail;

	*more = false;
	if (!q->queued)
		return NULL;

	if (mt76_queue_is_wed_rro_data(q))
		return NULL;

	if (!mt76_queue_is_wed_rro_ind(q)) {
		if (flush)
			q->desc[idx].ctrl |= cpu_to_le32(MT_DMA_CTL_DMA_DONE);
		else if (!(q->desc[idx].ctrl & cpu_to_le32(MT_DMA_CTL_DMA_DONE)))
			return NULL;
	}

	q->tail = (q->tail + 1) % q->ndesc;
	q->queued--;

	return mt76_dma_get_buf(dev, q, idx, len, info, more, drop);
}

static int
mt76_dma_tx_queue_skb_raw(struct mt76_dev *dev, struct mt76_queue *q,
			  struct sk_buff *skb, u32 tx_info)
{
	struct mt76_queue_buf buf = {};
	dma_addr_t addr;

	if (test_bit(MT76_MCU_RESET, &dev->phy.state))
		goto error;

	if (q->queued + 1 >= q->ndesc - 1)
		goto error;

	addr = dma_map_single(dev->dma_dev, skb->data, skb->len,
			      DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(dev->dma_dev, addr)))
		goto error;

	buf.addr = addr;
	buf.len = skb->len;

	spin_lock_bh(&q->lock);
	mt76_dma_add_buf(dev, q, &buf, 1, tx_info, skb, NULL);
	mt76_dma_kick_queue(dev, q);
	spin_unlock_bh(&q->lock);

	return 0;

error:
	dev_kfree_skb(skb);
	return -ENOMEM;
}

static int
mt76_dma_tx_queue_skb(struct mt76_phy *phy, struct mt76_queue *q,
		      enum mt76_txq_id qid, struct sk_buff *skb,
		      struct mt76_wcid *wcid, struct ieee80211_sta *sta)
{
	struct ieee80211_tx_status status = {
		.sta = sta,
	};
	struct mt76_tx_info tx_info = {
		.skb = skb,
	};
	struct mt76_dev *dev = phy->dev;
	struct ieee80211_hw *hw;
	int len, n = 0, ret = -ENOMEM;
	struct mt76_txwi_cache *t;
	struct sk_buff *iter;
	dma_addr_t addr;
	u8 *txwi;

	if (test_bit(MT76_RESET, &phy->state))
		goto free_skb;

	t = mt76_get_txwi(dev);
	if (!t)
		goto free_skb;

	txwi = mt76_get_txwi_ptr(dev, t);

	skb->prev = skb->next = NULL;
	if (dev->drv->drv_flags & MT_DRV_TX_ALIGNED4_SKBS)
		mt76_insert_hdr_pad(skb);

	len = skb_headlen(skb);
	addr = dma_map_single(dev->dma_dev, skb->data, len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(dev->dma_dev, addr)))
		goto free;

	tx_info.buf[n].addr = t->dma_addr;
	tx_info.buf[n++].len = dev->drv->txwi_size;
	tx_info.buf[n].addr = addr;
	tx_info.buf[n++].len = len;

	skb_walk_frags(skb, iter) {
		if (n == ARRAY_SIZE(tx_info.buf))
			goto unmap;

		addr = dma_map_single(dev->dma_dev, iter->data, iter->len,
				      DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(dev->dma_dev, addr)))
			goto unmap;

		tx_info.buf[n].addr = addr;
		tx_info.buf[n++].len = iter->len;
	}
	tx_info.nbuf = n;

	if (q->queued + (tx_info.nbuf + 1) / 2 >= q->ndesc - 1) {
		ret = -ENOMEM;
		goto unmap;
	}

	dma_sync_single_for_cpu(dev->dma_dev, t->dma_addr, dev->drv->txwi_size,
				DMA_TO_DEVICE);
	ret = dev->drv->tx_prepare_skb(dev, txwi, qid, wcid, sta, &tx_info);
	dma_sync_single_for_device(dev->dma_dev, t->dma_addr, dev->drv->txwi_size,
				   DMA_TO_DEVICE);
	if (ret < 0)
		goto unmap;

	return mt76_dma_add_buf(dev, q, tx_info.buf, tx_info.nbuf,
				tx_info.info, tx_info.skb, t);

unmap:
	for (n--; n > 0; n--)
		dma_unmap_single(dev->dma_dev, tx_info.buf[n].addr,
				 tx_info.buf[n].len, DMA_TO_DEVICE);

free:
#ifdef CONFIG_NL80211_TESTMODE
	/* fix tx_done accounting on queue overflow */
	if (mt76_is_testmode_skb(dev, skb, &hw)) {
		struct mt76_phy *phy = hw->priv;

		if (tx_info.skb == phy->test.tx_skb)
			phy->test.tx_done--;
	}
#endif

	mt76_put_txwi(dev, t);

free_skb:
	status.skb = tx_info.skb;
	hw = mt76_tx_status_get_hw(dev, tx_info.skb);
	spin_lock_bh(&dev->rx_lock);
	ieee80211_tx_status_ext(hw, &status);
	spin_unlock_bh(&dev->rx_lock);

	return ret;
}

static int
mt76_dma_rx_fill_buf(struct mt76_dev *dev, struct mt76_queue *q,
		     bool allow_direct)
{
	int len = SKB_WITH_OVERHEAD(q->buf_size);
	int frames = 0;

	if (!q->ndesc)
		return 0;

	while (q->queued < q->ndesc - 1) {
		struct mt76_queue_buf qbuf = {};
		enum dma_data_direction dir;
		dma_addr_t addr;
		int offset;
		void *buf = NULL;

		if (mt76_queue_is_wed_rro_ind(q))
			goto done;

		buf = mt76_get_page_pool_buf(q, &offset, q->buf_size);
		if (!buf)
			break;

		addr = page_pool_get_dma_addr(virt_to_head_page(buf)) + offset;
		dir = page_pool_get_dma_dir(q->page_pool);
		dma_sync_single_for_device(dev->dma_dev, addr, len, dir);

		qbuf.addr = addr + q->buf_offset;
done:
		qbuf.len = len - q->buf_offset;
		qbuf.skip_unmap = false;
		if (mt76_dma_add_rx_buf(dev, q, &qbuf, buf) < 0) {
			mt76_put_page_pool_buf(buf, allow_direct);
			break;
		}
		frames++;
	}

	if (frames || mt76_queue_is_wed_rx(q))
		mt76_dma_kick_queue(dev, q);

	return frames;
}

int mt76_dma_rx_fill(struct mt76_dev *dev, struct mt76_queue *q,
		     bool allow_direct)
{
	int frames;

	if (!q->ndesc)
		return 0;

	spin_lock_bh(&q->lock);
	frames = mt76_dma_rx_fill_buf(dev, q, allow_direct);
	spin_unlock_bh(&q->lock);

	return frames;
}

static int
mt76_dma_alloc_queue(struct mt76_dev *dev, struct mt76_queue *q,
		     int idx, int n_desc, int bufsize,
		     u32 ring_base)
{
	int ret, size;

	spin_lock_init(&q->lock);
	spin_lock_init(&q->cleanup_lock);

#if defined(__linux__)
	q->regs = dev->mmio.regs + ring_base + idx * MT_RING_SIZE;
#elif defined(__FreeBSD__)
	q->regs = (void *)((u8 *)dev->mmio.regs + ring_base + idx * MT_RING_SIZE);
#endif
	q->ndesc = n_desc;
	q->buf_size = bufsize;
	q->hw_idx = idx;

	size = mt76_queue_is_wed_rro_ind(q) ? sizeof(struct mt76_wed_rro_desc)
					    : sizeof(struct mt76_desc);
	q->desc = dmam_alloc_coherent(dev->dma_dev, q->ndesc * size,
				      &q->desc_dma, GFP_KERNEL);
	if (!q->desc)
		return -ENOMEM;

	if (mt76_queue_is_wed_rro_ind(q)) {
		struct mt76_wed_rro_desc *rro_desc;
		int i;

		rro_desc = (struct mt76_wed_rro_desc *)q->desc;
		for (i = 0; i < q->ndesc; i++) {
			struct mt76_wed_rro_ind *cmd;

			cmd = (struct mt76_wed_rro_ind *)&rro_desc[i];
			cmd->magic_cnt = MT_DMA_WED_IND_CMD_CNT - 1;
		}
	}

	size = q->ndesc * sizeof(*q->entry);
	q->entry = devm_kzalloc(dev->dev, size, GFP_KERNEL);
	if (!q->entry)
		return -ENOMEM;

	ret = mt76_create_page_pool(dev, q);
	if (ret)
		return ret;

	ret = mt76_wed_dma_setup(dev, q, false);
	if (ret)
		return ret;

	if (mtk_wed_device_active(&dev->mmio.wed)) {
		if ((mtk_wed_get_rx_capa(&dev->mmio.wed) && mt76_queue_is_wed_rro(q)) ||
		    mt76_queue_is_wed_tx_free(q))
			return 0;
	}

	mt76_dma_queue_reset(dev, q);

	return 0;
}

static void
mt76_dma_rx_cleanup(struct mt76_dev *dev, struct mt76_queue *q)
{
	void *buf;
	bool more;

	if (!q->ndesc)
		return;

	do {
		spin_lock_bh(&q->lock);
		buf = mt76_dma_dequeue(dev, q, true, NULL, NULL, &more, NULL);
		spin_unlock_bh(&q->lock);

		if (!buf)
			break;

		if (!mt76_queue_is_wed_rro(q))
			mt76_put_page_pool_buf(buf, false);
	} while (1);

	spin_lock_bh(&q->lock);
	if (q->rx_head) {
		dev_kfree_skb(q->rx_head);
		q->rx_head = NULL;
	}

	spin_unlock_bh(&q->lock);
}

static void
mt76_dma_rx_reset(struct mt76_dev *dev, enum mt76_rxq_id qid)
{
	struct mt76_queue *q = &dev->q_rx[qid];

	if (!q->ndesc)
		return;

	if (!mt76_queue_is_wed_rro_ind(q)) {
		int i;

		for (i = 0; i < q->ndesc; i++)
			q->desc[i].ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);
	}

	mt76_dma_rx_cleanup(dev, q);

	/* reset WED rx queues */
	mt76_wed_dma_setup(dev, q, true);

	if (mt76_queue_is_wed_tx_free(q))
		return;

	if (mtk_wed_device_active(&dev->mmio.wed) &&
	    mt76_queue_is_wed_rro(q))
		return;

	mt76_dma_sync_idx(dev, q);
	mt76_dma_rx_fill_buf(dev, q, false);
}

static void
mt76_add_fragment(struct mt76_dev *dev, struct mt76_queue *q, void *data,
		  int len, bool more, u32 info, bool allow_direct)
{
	struct sk_buff *skb = q->rx_head;
	struct skb_shared_info *shinfo = skb_shinfo(skb);
	int nr_frags = shinfo->nr_frags;

	if (nr_frags < ARRAY_SIZE(shinfo->frags)) {
		struct page *page = virt_to_head_page(data);
#if defined(__linux__)
		int offset = data - page_address(page) + q->buf_offset;
#elif defined(__FreeBSD__)
		int offset = (u8 *)data - (u8 *)page_address(page) + q->buf_offset;
#endif

		skb_add_rx_frag(skb, nr_frags, page, offset, len, q->buf_size);
	} else {
		mt76_put_page_pool_buf(data, allow_direct);
	}

	if (more)
		return;

	q->rx_head = NULL;
	if (nr_frags < ARRAY_SIZE(shinfo->frags))
		dev->drv->rx_skb(dev, q - dev->q_rx, skb, &info);
	else
		dev_kfree_skb(skb);
}

static int
mt76_dma_rx_process(struct mt76_dev *dev, struct mt76_queue *q, int budget)
{
	int len, data_len, done = 0, dma_idx;
	struct sk_buff *skb;
	unsigned char *data;
	bool check_ddone = false;
	bool allow_direct = !mt76_queue_is_wed_rx(q);
	bool more;

	if (IS_ENABLED(CONFIG_NET_MEDIATEK_SOC_WED) &&
	    mt76_queue_is_wed_tx_free(q)) {
		dma_idx = Q_READ(q, dma_idx);
		check_ddone = true;
	}

	while (done < budget) {
		bool drop = false;
		u32 info;

		if (check_ddone) {
			if (q->tail == dma_idx)
				dma_idx = Q_READ(q, dma_idx);

			if (q->tail == dma_idx)
				break;
		}

		data = mt76_dma_dequeue(dev, q, false, &len, &info, &more,
					&drop);
		if (!data)
			break;

		if (drop)
			goto free_frag;

		if (q->rx_head)
			data_len = q->buf_size;
		else
			data_len = SKB_WITH_OVERHEAD(q->buf_size);

		if (data_len < len + q->buf_offset) {
			dev_kfree_skb(q->rx_head);
			q->rx_head = NULL;
			goto free_frag;
		}

		if (q->rx_head) {
			mt76_add_fragment(dev, q, data, len, more, info,
					  allow_direct);
			continue;
		}

		if (!more && dev->drv->rx_check &&
		    !(dev->drv->rx_check(dev, data, len)))
			goto free_frag;

		skb = napi_build_skb(data, q->buf_size);
		if (!skb)
			goto free_frag;

		skb_reserve(skb, q->buf_offset);
		skb_mark_for_recycle(skb);

		*(u32 *)skb->cb = info;

		__skb_put(skb, len);
		done++;

		if (more) {
			q->rx_head = skb;
			continue;
		}

		dev->drv->rx_skb(dev, q - dev->q_rx, skb, &info);
		continue;

free_frag:
		mt76_put_page_pool_buf(data, allow_direct);
	}

	mt76_dma_rx_fill(dev, q, true);
	return done;
}

int mt76_dma_rx_poll(struct napi_struct *napi, int budget)
{
	struct mt76_dev *dev;
	int qid, done = 0, cur;

	dev = mt76_priv(napi->dev);
	qid = napi - dev->napi;

	rcu_read_lock();

	do {
		cur = mt76_dma_rx_process(dev, &dev->q_rx[qid], budget - done);
		mt76_rx_poll_complete(dev, qid, napi);
		done += cur;
	} while (cur && done < budget);

	rcu_read_unlock();

	if (done < budget && napi_complete(napi))
		dev->drv->rx_poll_complete(dev, qid);

	return done;
}
EXPORT_SYMBOL_GPL(mt76_dma_rx_poll);

static int
mt76_dma_init(struct mt76_dev *dev,
	      int (*poll)(struct napi_struct *napi, int budget))
{
	struct mt76_dev **priv;
	int i;

	dev->napi_dev = alloc_netdev_dummy(sizeof(struct mt76_dev *));
	if (!dev->napi_dev)
		return -ENOMEM;

	/* napi_dev private data points to mt76_dev parent, so, mt76_dev
	 * can be retrieved given napi_dev
	 */
	priv = netdev_priv(dev->napi_dev);
	*priv = dev;

	dev->tx_napi_dev = alloc_netdev_dummy(sizeof(struct mt76_dev *));
	if (!dev->tx_napi_dev) {
		free_netdev(dev->napi_dev);
		return -ENOMEM;
	}
	priv = netdev_priv(dev->tx_napi_dev);
	*priv = dev;

	snprintf(dev->napi_dev->name, sizeof(dev->napi_dev->name), "%s",
		 wiphy_name(dev->hw->wiphy));
	dev->napi_dev->threaded = 1;
	init_completion(&dev->mmio.wed_reset);
	init_completion(&dev->mmio.wed_reset_complete);

	mt76_for_each_q_rx(dev, i) {
		netif_napi_add(dev->napi_dev, &dev->napi[i], poll);
		mt76_dma_rx_fill_buf(dev, &dev->q_rx[i], false);
		napi_enable(&dev->napi[i]);
	}

	return 0;
}

static const struct mt76_queue_ops mt76_dma_ops = {
	.init = mt76_dma_init,
	.alloc = mt76_dma_alloc_queue,
	.reset_q = mt76_dma_queue_reset,
	.tx_queue_skb_raw = mt76_dma_tx_queue_skb_raw,
	.tx_queue_skb = mt76_dma_tx_queue_skb,
	.tx_cleanup = mt76_dma_tx_cleanup,
	.rx_cleanup = mt76_dma_rx_cleanup,
	.rx_reset = mt76_dma_rx_reset,
	.kick = mt76_dma_kick_queue,
};

void mt76_dma_attach(struct mt76_dev *dev)
{
	dev->queue_ops = &mt76_dma_ops;
}
EXPORT_SYMBOL_GPL(mt76_dma_attach);

void mt76_dma_cleanup(struct mt76_dev *dev)
{
	int i;

	mt76_worker_disable(&dev->tx_worker);
	netif_napi_del(&dev->tx_napi);

	for (i = 0; i < ARRAY_SIZE(dev->phys); i++) {
		struct mt76_phy *phy = dev->phys[i];
		int j;

		if (!phy)
			continue;

		for (j = 0; j < ARRAY_SIZE(phy->q_tx); j++)
			mt76_dma_tx_cleanup(dev, phy->q_tx[j], true);
	}

	for (i = 0; i < ARRAY_SIZE(dev->q_mcu); i++)
		mt76_dma_tx_cleanup(dev, dev->q_mcu[i], true);

	mt76_for_each_q_rx(dev, i) {
		struct mt76_queue *q = &dev->q_rx[i];

		if (mtk_wed_device_active(&dev->mmio.wed) &&
		    mt76_queue_is_wed_rro(q))
			continue;

		netif_napi_del(&dev->napi[i]);
		mt76_dma_rx_cleanup(dev, q);

		page_pool_destroy(q->page_pool);
	}

	if (mtk_wed_device_active(&dev->mmio.wed))
		mtk_wed_device_detach(&dev->mmio.wed);

	if (mtk_wed_device_active(&dev->mmio.wed_hif2))
		mtk_wed_device_detach(&dev->mmio.wed_hif2);

	mt76_free_pending_txwi(dev);
	mt76_free_pending_rxwi(dev);
	free_netdev(dev->napi_dev);
	free_netdev(dev->tx_napi_dev);
}
EXPORT_SYMBOL_GPL(mt76_dma_cleanup);
