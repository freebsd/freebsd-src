/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "mlx4_en.h"

#include <linux/mlx4/cq.h>
#include <linux/mlx4/qp.h>
#include <linux/vmalloc.h>

#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <sys/mbuf.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/udp.h>

enum {
	MAX_INLINE = 104, /* 128 - 16 - 4 - 4 */
	MAX_BF = 256,
};

static int inline_thold = MAX_INLINE;

module_param_named(inline_thold, inline_thold, int, 0444);
MODULE_PARM_DESC(inline_thold, "treshold for using inline data");

int mlx4_en_create_tx_ring(struct mlx4_en_priv *priv,
			   struct mlx4_en_tx_ring *ring, u32 size,
			   u16 stride)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int tmp;
	int err;

	ring->size = size;
	ring->size_mask = size - 1;
	ring->stride = stride;

	inline_thold = min(inline_thold, MAX_INLINE);

	mtx_init(&ring->tx_lock.m, "mlx4 tx", NULL, MTX_DEF);
	mtx_init(&ring->comp_lock.m, "mlx4 comp", NULL, MTX_DEF);

	/* Allocate the buf ring */
	ring->br = buf_ring_alloc(MLX4_EN_DEF_TX_QUEUE_SIZE, M_DEVBUF,
	    M_WAITOK, &ring->tx_lock.m);
	if (ring->br == NULL) {
		en_err(priv, "Failed allocating tx_info ring\n");
		return -ENOMEM;
	}

	tmp = size * sizeof(struct mlx4_en_tx_info);
	ring->tx_info = kmalloc(tmp, GFP_KERNEL);
	if (!ring->tx_info) {
		en_err(priv, "Failed allocating tx_info ring\n");
		err = -ENOMEM;
		goto err_tx;
	}
	en_dbg(DRV, priv, "Allocated tx_info ring at addr:%p size:%d\n",
		 ring->tx_info, tmp);

	ring->bounce_buf = kmalloc(MAX_DESC_SIZE, GFP_KERNEL);
	if (!ring->bounce_buf) {
		en_err(priv, "Failed allocating bounce buffer\n");
		err = -ENOMEM;
		goto err_tx;
	}
	ring->buf_size = ALIGN(size * ring->stride, MLX4_EN_PAGE_SIZE);

	err = mlx4_alloc_hwq_res(mdev->dev, &ring->wqres, ring->buf_size,
				 2 * PAGE_SIZE);
	if (err) {
		en_err(priv, "Failed allocating hwq resources\n");
		goto err_bounce;
	}

	err = mlx4_en_map_buffer(&ring->wqres.buf);
	if (err) {
		en_err(priv, "Failed to map TX buffer\n");
		goto err_hwq_res;
	}

	ring->buf = ring->wqres.buf.direct.buf;

	en_dbg(DRV, priv, "Allocated TX ring (addr:%p) - buf:%p size:%d "
	       "buf_size:%d dma:%llx\n", ring, ring->buf, ring->size,
	       ring->buf_size, (unsigned long long) ring->wqres.buf.direct.map);

	err = mlx4_qp_reserve_range(mdev->dev, 1, 256, &ring->qpn);
	if (err) {
		en_err(priv, "Failed reserving qp for tx ring.\n");
		goto err_map;
	}

	err = mlx4_qp_alloc(mdev->dev, ring->qpn, &ring->qp);
	if (err) {
		en_err(priv, "Failed allocating qp %d\n", ring->qpn);
		goto err_reserve;
	}
	ring->qp.event = mlx4_en_sqp_event;

	err = mlx4_bf_alloc(mdev->dev, &ring->bf);
	if (err) {
		ring->bf.uar = &mdev->priv_uar;
		ring->bf.uar->map = mdev->uar_map;
		ring->bf_enabled = false;
	} else
		ring->bf_enabled = true;

	return 0;

err_reserve:
	mlx4_qp_release_range(mdev->dev, ring->qpn, 1);
err_map:
	mlx4_en_unmap_buffer(&ring->wqres.buf);
err_hwq_res:
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size);
err_bounce:
	kfree(ring->bounce_buf);
	ring->bounce_buf = NULL;
err_tx:
	buf_ring_free(ring->br, M_DEVBUF);
	kfree(ring->tx_info);
	ring->tx_info = NULL;
	return err;
}

void mlx4_en_destroy_tx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_tx_ring *ring)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	en_dbg(DRV, priv, "Destroying tx ring, qpn: %d\n", ring->qpn);

	buf_ring_free(ring->br, M_DEVBUF);
	if (ring->bf_enabled)
		mlx4_bf_free(mdev->dev, &ring->bf);
	mlx4_qp_remove(mdev->dev, &ring->qp);
	mlx4_qp_free(mdev->dev, &ring->qp);
	mlx4_qp_release_range(mdev->dev, ring->qpn, 1);
	mlx4_en_unmap_buffer(&ring->wqres.buf);
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size);
	kfree(ring->bounce_buf);
	ring->bounce_buf = NULL;
	kfree(ring->tx_info);
	ring->tx_info = NULL;
	mtx_destroy(&ring->tx_lock.m);
	mtx_destroy(&ring->comp_lock.m);
}

int mlx4_en_activate_tx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_tx_ring *ring,
			     int cq)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	ring->cqn = cq;
	ring->prod = 0;
	ring->cons = 0xffffffff;
	ring->last_nr_txbb = 1;
	ring->poll_cnt = 0;
	ring->blocked = 0;
	memset(ring->tx_info, 0, ring->size * sizeof(struct mlx4_en_tx_info));
	memset(ring->buf, 0, ring->buf_size);

	ring->qp_state = MLX4_QP_STATE_RST;
	ring->doorbell_qpn = swab32(ring->qp.qpn << 8);

	mlx4_en_fill_qp_context(priv, ring->size, ring->stride, 1, 0, ring->qpn,
				ring->cqn, &ring->context);
	if (ring->bf_enabled)
		ring->context.usr_page = cpu_to_be32(ring->bf.uar->index);

	err = mlx4_qp_to_ready(mdev->dev, &ring->wqres.mtt, &ring->context,
			       &ring->qp, &ring->qp_state);

	return err;
}

void mlx4_en_deactivate_tx_ring(struct mlx4_en_priv *priv,
				struct mlx4_en_tx_ring *ring)
{
	struct mlx4_en_dev *mdev = priv->mdev;

	mlx4_qp_modify(mdev->dev, NULL, ring->qp_state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &ring->qp);
}


static u32 mlx4_en_free_tx_desc(struct mlx4_en_priv *priv,
				struct mlx4_en_tx_ring *ring,
				int index, u8 owner)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_tx_info *tx_info = &ring->tx_info[index];
	struct mlx4_en_tx_desc *tx_desc = ring->buf + index * TXBB_SIZE;
	struct mlx4_wqe_data_seg *data = (void *) tx_desc + tx_info->data_offset;
	struct mbuf *mb = tx_info->mb;
	void *end = ring->buf + ring->buf_size;
	int frags = tx_info->nr_segs;
	int i;
	__be32 *ptr = (__be32 *)tx_desc;
	__be32 stamp = cpu_to_be32(STAMP_VAL | (!!owner << STAMP_SHIFT));

	/* Optimize the common case when there are no wraparounds */
	if (likely((void *) tx_desc + tx_info->nr_txbb * TXBB_SIZE <= end)) {
		if (!tx_info->inl) {
			for (i = 0; i < frags; i++) {
				pci_unmap_single(mdev->pdev,
					(dma_addr_t) be64_to_cpu(data[i].addr),
					data[i].byte_count, PCI_DMA_TODEVICE);
			}
		}
		/* Stamp the freed descriptor */
		for (i = 0; i < tx_info->nr_txbb * TXBB_SIZE; i += STAMP_STRIDE) {
			*ptr = stamp;
			ptr += STAMP_DWORDS;
		}

	} else {
		if (!tx_info->inl) {
			for (i = 0; i < frags; i++) {
				/* Check for wraparound before unmapping */
				if ((void *) data >= end)
					data = (struct mlx4_wqe_data_seg *) ring->buf;
				pci_unmap_single(mdev->pdev,
					(dma_addr_t) be64_to_cpu(data->addr),
					data->byte_count, PCI_DMA_TODEVICE);
				++data;
			}
		}
		/* Stamp the freed descriptor */
		for (i = 0; i < tx_info->nr_txbb * TXBB_SIZE; i += STAMP_STRIDE) {
			*ptr = stamp;
			ptr += STAMP_DWORDS;
			if ((void *) ptr >= end) {
				ptr = ring->buf;
				stamp ^= cpu_to_be32(0x80000000);
			}
		}

	}
	m_freem(mb);
	return tx_info->nr_txbb;
}


int mlx4_en_free_tx_buf(struct net_device *dev, struct mlx4_en_tx_ring *ring)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int cnt = 0;

	/* Skip last polled descriptor */
	ring->cons += ring->last_nr_txbb;
	en_dbg(DRV, priv, "Freeing Tx buf - cons:0x%x prod:0x%x\n",
		 ring->cons, ring->prod);

	if ((u32) (ring->prod - ring->cons) > ring->size) {
		en_warn(priv, "Tx consumer passed producer!\n");
		return 0;
	}

	while (ring->cons != ring->prod) {
		ring->last_nr_txbb = mlx4_en_free_tx_desc(priv, ring,
						ring->cons & ring->size_mask,
						!!(ring->cons & ring->size));
		ring->cons += ring->last_nr_txbb;
		cnt++;
	}

	if (cnt)
		en_dbg(DRV, priv, "Freed %d uncompleted tx descriptors\n", cnt);

	return cnt;
}

void mlx4_en_set_prio_map(struct mlx4_en_priv *priv, u16 *prio_map, u32 ring_num)
{
	int block = 8 / ring_num;
	int extra = 8 - (block * ring_num);
	int num = 0;
	u16 ring = 1;
	int prio;

	if (ring_num == 1) {
		for (prio = 0; prio < 8; prio++)
			prio_map[prio] = 0;
		return;
	}

	for (prio = 0; prio < 8; prio++) {
		if (extra && (num == block + 1)) {
			ring++;
			num = 0;
			extra--;
		} else if (!extra && (num == block)) {
			ring++;
			num = 0;
		}
		prio_map[prio] = ring;
		en_dbg(DRV, priv, " prio:%d --> ring:%d\n", prio, ring);
		num++;
	}
}

static void mlx4_en_process_tx_cq(struct net_device *dev, struct mlx4_en_cq *cq)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_cq *mcq = &cq->mcq;
	struct mlx4_en_tx_ring *ring = &priv->tx_ring[cq->ring];
	struct mlx4_cqe *cqe = cq->buf;
	u16 index;
	u16 new_index;
	u32 txbbs_skipped = 0;
	u32 cq_last_sav;

	/* index always points to the first TXBB of the last polled descriptor */
	index = ring->cons & ring->size_mask;
	new_index = be16_to_cpu(cqe->wqe_index) & ring->size_mask;
	if (index == new_index)
		return;

	if (!priv->port_up)
		return;

	/*
	 * We use a two-stage loop:
	 * - the first samples the HW-updated CQE
	 * - the second frees TXBBs until the last sample
	 * This lets us amortize CQE cache misses, while still polling the CQ
	 * until is quiescent.
	 */
	cq_last_sav = mcq->cons_index;
	do {
		do {
			/* Skip over last polled CQE */
			index = (index + ring->last_nr_txbb) & ring->size_mask;
			txbbs_skipped += ring->last_nr_txbb;

			/* Poll next CQE */
			ring->last_nr_txbb = mlx4_en_free_tx_desc(
						priv, ring, index,
						!!((ring->cons + txbbs_skipped) &
						   ring->size));
			++mcq->cons_index;

		} while (index != new_index);

		new_index = be16_to_cpu(cqe->wqe_index) & ring->size_mask;
	} while (index != new_index);
	AVG_PERF_COUNTER(priv->pstats.tx_coal_avg,
			 (u32) (mcq->cons_index - cq_last_sav));

	/*
	 * To prevent CQ overflow we first update CQ consumer and only then
	 * the ring consumer.
	 */
	mlx4_cq_set_ci(mcq);
	wmb();
	ring->cons += txbbs_skipped;

	/* Wakeup Tx queue if this ring stopped it */
	if (unlikely(ring->blocked)) {
		if ((u32) (ring->prod - ring->cons) <=
		     ring->size - HEADROOM - MAX_DESC_TXBBS) {
			ring->blocked = 0;
			if (atomic_fetchadd_int(&priv->blocked, -1) == 1)
				atomic_clear_int(&dev->if_drv_flags,
				    IFF_DRV_OACTIVE);
			priv->port_stats.wake_queue++;
		}
	}
}

void mlx4_en_tx_irq(struct mlx4_cq *mcq)
{
	struct mlx4_en_cq *cq = container_of(mcq, struct mlx4_en_cq, mcq);
	struct mlx4_en_priv *priv = netdev_priv(cq->dev);
	struct mlx4_en_tx_ring *ring = &priv->tx_ring[cq->ring];

	if (!spin_trylock(&ring->comp_lock))
		return;
	mlx4_en_process_tx_cq(cq->dev, cq);
	mod_timer(&cq->timer, jiffies + 1);
	spin_unlock(&ring->comp_lock);
}


void mlx4_en_poll_tx_cq(unsigned long data)
{
	struct mlx4_en_cq *cq = (struct mlx4_en_cq *) data;
	struct mlx4_en_priv *priv = netdev_priv(cq->dev);
	struct mlx4_en_tx_ring *ring = &priv->tx_ring[cq->ring];
	u32 inflight;

	INC_PERF_COUNTER(priv->pstats.tx_poll);

	if (!spin_trylock(&ring->comp_lock)) {
		mod_timer(&cq->timer, jiffies + MLX4_EN_TX_POLL_TIMEOUT);
		return;
	}
	mlx4_en_process_tx_cq(cq->dev, cq);
	inflight = (u32) (ring->prod - ring->cons - ring->last_nr_txbb);

	/* If there are still packets in flight and the timer has not already
	 * been scheduled by the Tx routine then schedule it here to guarantee
	 * completion processing of these packets */
	if (inflight && priv->port_up)
		mod_timer(&cq->timer, jiffies + MLX4_EN_TX_POLL_TIMEOUT);

	spin_unlock(&ring->comp_lock);
}

static struct mlx4_en_tx_desc *mlx4_en_bounce_to_desc(struct mlx4_en_priv *priv,
						      struct mlx4_en_tx_ring *ring,
						      u32 index,
						      unsigned int desc_size)
{
	u32 copy = (ring->size - index) * TXBB_SIZE;
	int i;

	for (i = desc_size - copy - 4; i >= 0; i -= 4) {
		if ((i & (TXBB_SIZE - 1)) == 0)
			wmb();

		*((u32 *) (ring->buf + i)) =
			*((u32 *) (ring->bounce_buf + copy + i));
	}

	for (i = copy - 4; i >= 4 ; i -= 4) {
		if ((i & (TXBB_SIZE - 1)) == 0)
			wmb();

		*((u32 *) (ring->buf + index * TXBB_SIZE + i)) =
			*((u32 *) (ring->bounce_buf + i));
	}

	/* Return real descriptor location */
	return ring->buf + index * TXBB_SIZE;
}

static inline void mlx4_en_xmit_poll(struct mlx4_en_priv *priv, int tx_ind)
{
	struct mlx4_en_cq *cq = &priv->tx_cq[tx_ind];
	struct mlx4_en_tx_ring *ring = &priv->tx_ring[tx_ind];

	/* If we don't have a pending timer, set one up to catch our recent
	   post in case the interface becomes idle */
	if (!timer_pending(&cq->timer))
		mod_timer(&cq->timer, jiffies + MLX4_EN_TX_POLL_TIMEOUT);

	/* Poll the CQ every mlx4_en_TX_MODER_POLL packets */
	if ((++ring->poll_cnt & (MLX4_EN_TX_POLL_MODER - 1)) == 0)
		if (spin_trylock(&ring->comp_lock)) {
			mlx4_en_process_tx_cq(priv->dev, cq);
			spin_unlock(&ring->comp_lock);
		}
}

static int is_inline(struct mbuf *mb)
{

	if (inline_thold && mb->m_pkthdr.len <= inline_thold &&
	    (mb->m_pkthdr.csum_flags & CSUM_TSO) == 0)
		return 1;

	return 0;
}

static int inline_size(struct mbuf *mb)
{
	int len;

	len = mb->m_pkthdr.len;
	if (len + CTRL_SIZE + sizeof(struct mlx4_wqe_inline_seg)
	    <= MLX4_INLINE_ALIGN)
		return ALIGN(len + CTRL_SIZE +
			     sizeof(struct mlx4_wqe_inline_seg), 16);
	else
		return ALIGN(len + CTRL_SIZE + 2 *
			     sizeof(struct mlx4_wqe_inline_seg), 16);
}

static int get_head_size(struct mbuf *mb)
{
	struct tcphdr *th;
	struct ip *ip;
	int ip_hlen, tcp_hlen;
	int len;

	len = ETHER_HDR_LEN;
	if (mb->m_len < len + sizeof(struct ip))
		return (0);
	ip = (struct ip *)(mtod(mb, char *) + len);
	if (ip->ip_p != IPPROTO_TCP)
		return (0);
	ip_hlen = ip->ip_hl << 2;
	len += ip_hlen;
	if (mb->m_len < len + sizeof(struct tcphdr))
		return (0);
	th = (struct tcphdr *)(mtod(mb, char *) + len);
	tcp_hlen = th->th_off << 2;
	len += tcp_hlen;
	if (mb->m_len < len)
		return (0);
	return (len);
}

static int get_real_size(struct mbuf *mb, struct net_device *dev, int *segsp,
    int *lso_header_size)
{
	struct mbuf *m;
	int nr_segs;

	nr_segs = 0;
	for (m = mb; m != NULL; m = m->m_next)
		if (m->m_len)
			nr_segs++;

	if (mb->m_pkthdr.csum_flags & CSUM_TSO) {
		*lso_header_size = get_head_size(mb);
		if (*lso_header_size) {
			if (mb->m_len == *lso_header_size)
				nr_segs--;
			*segsp = nr_segs;
			return CTRL_SIZE + nr_segs * DS_SIZE +
			    ALIGN(*lso_header_size + 4, DS_SIZE);
		}
	} else
		*lso_header_size = 0;
	*segsp = nr_segs;
	if (is_inline(mb))
		return inline_size(mb);
	return (CTRL_SIZE + nr_segs * DS_SIZE);
}

static struct mbuf *mb_copy(struct mbuf *mb, int *offp, char *data, int len)
{
	int bytes;
	int off;

	off = *offp;
	while (len) {
		bytes = min(mb->m_len - off, len);
		if (bytes)
			memcpy(data, mb->m_data + off, bytes);
		len -= bytes;
		data += bytes;
		off += bytes;
		if (off == mb->m_len) {
			off = 0;
			mb = mb->m_next;
		}
	}
	*offp = off;
	return (mb);
}

static void build_inline_wqe(struct mlx4_en_tx_desc *tx_desc, struct mbuf *mb,
			     int real_size, u16 *vlan_tag, int tx_ind)
{
	struct mlx4_wqe_inline_seg *inl = &tx_desc->inl;
	int spc = MLX4_INLINE_ALIGN - CTRL_SIZE - sizeof *inl;
	int len;
	int off;

	off = 0;
	len = mb->m_pkthdr.len;
	if (len <= spc) {
		inl->byte_count = cpu_to_be32(1 << 31 | len);
		mb_copy(mb, &off, (void *)(inl + 1), len);
	} else {
		inl->byte_count = cpu_to_be32(1 << 31 | spc);
		mb = mb_copy(mb, &off, (void *)(inl + 1), spc);
		inl = (void *) (inl + 1) + spc;
		mb_copy(mb, &off, (void *)(inl + 1), len - spc);
		wmb();
		inl->byte_count = cpu_to_be32(1 << 31 | (len - spc));
	}
	tx_desc->ctrl.vlan_tag = cpu_to_be16(*vlan_tag);
	tx_desc->ctrl.ins_vlan = MLX4_WQE_CTRL_INS_VLAN * !!(*vlan_tag);
	tx_desc->ctrl.fence_size = (real_size / 16) & 0x3f;
}

u16 mlx4_en_select_queue(struct net_device *dev, struct mbuf *mb)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_tx_hash_entry *entry;
	struct ether_header *eth;
	struct tcphdr *th;
	struct ip *iph;
	u32 hash_index;
	int tx_ind = 0;
	u16 vlan_tag = 0;
	int len;

	/* Obtain VLAN information if present */
	if (mb->m_flags & M_VLANTAG) {
		vlan_tag = mb->m_pkthdr.ether_vtag;
		/* Set the Tx ring to use according to vlan priority */
		tx_ind = priv->tx_prio_map[vlan_tag >> 13];
		if (tx_ind)
			return tx_ind;
	}
	if (mb->m_len <
	    ETHER_HDR_LEN + sizeof(struct ip) + sizeof(struct tcphdr))
		return MLX4_EN_NUM_HASH_RINGS;
	eth = mtod(mb, struct ether_header *);
	/* Hashing is only done for TCP/IP or UDP/IP packets */
	if (be16_to_cpu(eth->ether_type) != ETHERTYPE_IP)
		return MLX4_EN_NUM_HASH_RINGS;
	len = ETHER_HDR_LEN;
	iph = (struct ip *)(mtod(mb, char *) + len);
	len += iph->ip_hl << 2;
	th = (struct tcphdr *)(mtod(mb, char *) + len);
	hash_index = be32_to_cpu(iph->ip_dst.s_addr) & MLX4_EN_TX_HASH_MASK;
	switch(iph->ip_p) {
	case IPPROTO_UDP:
		break;
	case IPPROTO_TCP:
		if (mb->m_len < len + sizeof(struct tcphdr))
			return MLX4_EN_NUM_HASH_RINGS;
		hash_index =
		    (hash_index ^ be16_to_cpu(th->th_dport ^ th->th_sport)) &
		    MLX4_EN_TX_HASH_MASK;
		break;
	default:
		return MLX4_EN_NUM_HASH_RINGS;
	}

	entry = &priv->tx_hash[hash_index];
	if(unlikely(!entry->cnt)) {
		tx_ind = hash_index & (MLX4_EN_NUM_HASH_RINGS / 2 - 1);
		if (2 * entry->small_pkts > entry->big_pkts)
			tx_ind += MLX4_EN_NUM_HASH_RINGS / 2;
		entry->small_pkts = entry->big_pkts = 0;
		entry->ring = tx_ind;
	}

	entry->cnt++;
	if (mb->m_pkthdr.len > MLX4_EN_SMALL_PKT_SIZE)
		entry->big_pkts++;
	else
		entry->small_pkts++;
	return entry->ring;
}

static void mlx4_bf_copy(unsigned long *dst, unsigned long *src, unsigned bytecnt)
{
	__iowrite64_copy(dst, src, bytecnt / 8);
}

static int mlx4_en_xmit(struct net_device *dev, int tx_ind, struct mbuf **mbp)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_tx_ring *ring;
	struct mlx4_en_cq *cq;
	struct mlx4_en_tx_desc *tx_desc;
	struct mlx4_wqe_data_seg *data;
	struct mlx4_en_tx_info *tx_info;
	struct mbuf *m;
	int nr_txbb;
	int nr_segs;
	int desc_size;
	int real_size;
	dma_addr_t dma;
	u32 index, bf_index;
	__be32 op_own;
	u16 vlan_tag = 0;
	int i;
	int lso_header_size;
	bool bounce = false;
	struct mbuf *mb;
	int defrag = 1;

	ring = &priv->tx_ring[tx_ind];
	mb = *mbp;
	if (!priv->port_up)
		goto tx_drop;

retry:
	real_size = get_real_size(mb, dev, &nr_segs, &lso_header_size);
	if (unlikely(!real_size))
		goto tx_drop;

	/* Allign descriptor to TXBB size */
	desc_size = ALIGN(real_size, TXBB_SIZE);
	nr_txbb = desc_size / TXBB_SIZE;
	if (unlikely(nr_txbb > MAX_DESC_TXBBS)) {
		if (defrag) {
			mb = m_defrag(*mbp, M_NOWAIT);
			if (mb == NULL) {
				mb = *mbp;
				goto tx_drop;
			}
			*mbp = mb;
			defrag = 0;
			goto retry;
		}
		goto tx_drop;
	}

	/* Check available TXBBs And 2K spare for prefetch */
	if (unlikely(((int)(ring->prod - ring->cons)) >
		     ring->size - HEADROOM - MAX_DESC_TXBBS)) {
		/* every full Tx ring stops queue */
		if (ring->blocked == 0)
			atomic_add_int(&priv->blocked, 1);
		atomic_set_int(&dev->if_drv_flags, IFF_DRV_OACTIVE);
		ring->blocked = 1;
		priv->port_stats.queue_stopped++;

		/* Use interrupts to find out when queue opened */
		cq = &priv->tx_cq[tx_ind];
		mlx4_en_arm_cq(priv, cq);
		return EBUSY;
	}

	/* Track current inflight packets for performance analysis */
	AVG_PERF_COUNTER(priv->pstats.inflight_avg,
			 (u32) (ring->prod - ring->cons - 1));

	/* Packet is good - grab an index and transmit it */
	index = ring->prod & ring->size_mask;
	bf_index = ring->prod;

	/* See if we have enough space for whole descriptor TXBB for setting
	 * SW ownership on next descriptor; if not, use a bounce buffer. */
	if (likely(index + nr_txbb <= ring->size))
		tx_desc = ring->buf + index * TXBB_SIZE;
	else {
		tx_desc = (struct mlx4_en_tx_desc *) ring->bounce_buf;
		bounce = true;
	}

	/* Prepare ctrl segement apart opcode+ownership, which depends on
	 * whether LSO is used */
	if (mb->m_flags & M_VLANTAG)
		vlan_tag = mb->m_pkthdr.ether_vtag;
	tx_desc->ctrl.vlan_tag = cpu_to_be16(vlan_tag);
	tx_desc->ctrl.ins_vlan = MLX4_WQE_CTRL_INS_VLAN * !!vlan_tag;
	tx_desc->ctrl.fence_size = (real_size / 16) & 0x3f;
	tx_desc->ctrl.srcrb_flags = cpu_to_be32(MLX4_WQE_CTRL_CQ_UPDATE |
						MLX4_WQE_CTRL_SOLICITED);
	if (mb->m_pkthdr.csum_flags & (CSUM_IP|CSUM_TCP|CSUM_UDP)) {
		if (mb->m_pkthdr.csum_flags & CSUM_IP)
			tx_desc->ctrl.srcrb_flags |=
			    cpu_to_be32(MLX4_WQE_CTRL_IP_CSUM);
		if (mb->m_pkthdr.csum_flags & (CSUM_TCP|CSUM_UDP))
			tx_desc->ctrl.srcrb_flags |=
			    cpu_to_be32(MLX4_WQE_CTRL_TCP_UDP_CSUM);
		priv->port_stats.tx_chksum_offload++;
	}

	if (unlikely(priv->validate_loopback)) {
		/* Copy dst mac address to wqe */
		struct ether_header *ethh;
		u64 mac;
		u32 mac_l, mac_h;

		ethh = mtod(mb, struct ether_header *);
		mac = mlx4_en_mac_to_u64(ethh->ether_dhost);
		if (mac) {
			mac_h = (u32) ((mac & 0xffff00000000ULL) >> 16);
			mac_l = (u32) (mac & 0xffffffff);
			tx_desc->ctrl.srcrb_flags |= cpu_to_be32(mac_h);
			tx_desc->ctrl.imm = cpu_to_be32(mac_l);
		}
	}

	/* Handle LSO (TSO) packets */
	if (lso_header_size) {
		int segsz;

		/* Mark opcode as LSO */
		op_own = cpu_to_be32(MLX4_OPCODE_LSO | (1 << 6)) |
			((ring->prod & ring->size) ?
				cpu_to_be32(MLX4_EN_BIT_DESC_OWN) : 0);

		/* Fill in the LSO prefix */
		tx_desc->lso.mss_hdr_size = cpu_to_be32(
			mb->m_pkthdr.tso_segsz << 16 | lso_header_size);

		/* Copy headers;
		 * note that we already verified that it is linear */
		memcpy(tx_desc->lso.header, mb->m_data, lso_header_size);
		data = ((void *) &tx_desc->lso +
			ALIGN(lso_header_size + 4, DS_SIZE));

		priv->port_stats.tso_packets++;
		segsz = mb->m_pkthdr.tso_segsz;
		i = ((mb->m_pkthdr.len - lso_header_size) / segsz) +
			!!((mb->m_pkthdr.len - lso_header_size) % segsz);
		ring->bytes += mb->m_pkthdr.len + (i - 1) * lso_header_size;
		ring->packets += i;
		mb->m_data += lso_header_size;
		mb->m_len -= lso_header_size;
	} else {
		/* Normal (Non LSO) packet */
		op_own = cpu_to_be32(MLX4_OPCODE_SEND) |
			((ring->prod & ring->size) ?
			 cpu_to_be32(MLX4_EN_BIT_DESC_OWN) : 0);
		data = &tx_desc->data;
		ring->bytes += max(mb->m_pkthdr.len,
		    (unsigned int)ETHER_MIN_LEN - ETHER_CRC_LEN);
		ring->packets++;

	}
	AVG_PERF_COUNTER(priv->pstats.tx_pktsz_avg, mb->m_pkthdr.len);

	/* Save mb in tx_info ring */
	tx_info = &ring->tx_info[index];
	tx_info->mb = mb;
	tx_info->nr_txbb = nr_txbb;
	tx_info->nr_segs = nr_segs;
	/* valid only for non inline segments */
	tx_info->data_offset = (void *) data - (void *) tx_desc;

	if (!is_inline(mb)) {
		for (i = 0, m = mb; i < nr_segs; i++, m = m->m_next) {
			if (m->m_len == 0) {
				i--;
				continue;
			}
			dma = pci_map_single(mdev->dev->pdev, m->m_data,
					     m->m_len, PCI_DMA_TODEVICE);
			data->addr = cpu_to_be64(dma);
			data->lkey = cpu_to_be32(mdev->mr.key);
			wmb();
			data->byte_count = cpu_to_be32(m->m_len);
			data++;
		}
		if (lso_header_size) {
			mb->m_data -= lso_header_size;
			mb->m_len += lso_header_size;
		}
		tx_info->inl = 0;
	} else {
		build_inline_wqe(tx_desc, mb, real_size, &vlan_tag, tx_ind);
		tx_info->inl = 1;
	}

	ring->prod += nr_txbb;

	/* If we used a bounce buffer then copy descriptor back into place */
	if (bounce)
		tx_desc = mlx4_en_bounce_to_desc(priv, ring, index, desc_size);

	if (ring->bf_enabled && desc_size <= MAX_BF && !bounce && !vlan_tag) {
		*(u32 *) (&tx_desc->ctrl.vlan_tag) |= ring->doorbell_qpn;
		op_own |= htonl((bf_index & 0xffff) << 8);
		/* Ensure new descirptor hits memory
		* before setting ownership of this descriptor to HW */
		wmb();
		tx_desc->ctrl.owner_opcode = op_own;

		wmb();

		mlx4_bf_copy(ring->bf.reg + ring->bf.offset, (unsigned long *) &tx_desc->ctrl,
		     desc_size);

		wmb();

		ring->bf.offset ^= ring->bf.buf_size;
	} else {
		/* Ensure new descirptor hits memory
		* before setting ownership of this descriptor to HW */
		wmb();
		tx_desc->ctrl.owner_opcode = op_own;
		wmb();
		writel(ring->doorbell_qpn, ring->bf.uar->map + MLX4_SEND_DOORBELL);
	}

	return 0;

tx_drop:
	*mbp = NULL;
	m_freem(mb);
	ring->errors++;
	return EINVAL;
}


static int
mlx4_en_transmit_locked(struct ifnet *dev, int tx_ind, struct mbuf *m)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_tx_ring *ring;
	struct mbuf *next;
	int enqueued, err = 0;

	ring = &priv->tx_ring[tx_ind];
	if ((dev->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || priv->port_up == 0) {
		if (m != NULL)
			err = drbr_enqueue(dev, ring->br, m);
		return (err);  
	}

	enqueued = 0;
	if (m != NULL) {
		if ((err = drbr_enqueue(dev, ring->br, m)) != 0)
			return (err);
	}
	/* Process the queue */
	while ((next = drbr_peek(dev, ring->br)) != NULL) {
		if ((err = mlx4_en_xmit(dev, tx_ind, &next)) != 0) {
			if (next == NULL) {
				drbr_advance(dev, ring->br);
			} else {
				drbr_putback(dev, ring->br, next);
			}
			break;
		}
		drbr_advance(dev, ring->br);
		enqueued++;
		dev->if_obytes += next->m_pkthdr.len;
		if (next->m_flags & M_MCAST)
			dev->if_omcasts++;
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(dev, next);
		if ((dev->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;
	}

	if (enqueued > 0)
		ring->watchdog_time = ticks;

	return (err);
}

void
mlx4_en_tx_que(void *context, int pending)
{
	struct mlx4_en_tx_ring *ring;
	struct mlx4_en_priv *priv;
	struct net_device *dev;
	struct mlx4_en_cq *cq;
	int tx_ind;

	cq = context;
	dev = cq->dev;
	priv = dev->if_softc;
	tx_ind = cq->ring;
	ring = &priv->tx_ring[tx_ind];
        if (dev->if_drv_flags & IFF_DRV_RUNNING) {
		mlx4_en_xmit_poll(priv, tx_ind);
		spin_lock(&ring->tx_lock);
                if (!drbr_empty(dev, ring->br))
			mlx4_en_transmit_locked(dev, tx_ind, NULL);
		spin_unlock(&ring->tx_lock);
	}
}

int
mlx4_en_transmit(struct ifnet *dev, struct mbuf *m)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_tx_ring *ring;
	struct mlx4_en_cq *cq;
	int i = 0, err = 0;

	/* Which queue to use */
	if ((m->m_flags & (M_FLOWID | M_VLANTAG)) == M_FLOWID)
		i = m->m_pkthdr.flowid % (MLX4_EN_NUM_HASH_RINGS - 1);
	else
		i = mlx4_en_select_queue(dev, m);

	ring = &priv->tx_ring[i];

	if (spin_trylock(&ring->tx_lock)) {
		err = mlx4_en_transmit_locked(dev, i, m);
		spin_unlock(&ring->tx_lock);
		/* Poll CQ here */
		mlx4_en_xmit_poll(priv, i);
	} else {
		err = drbr_enqueue(dev, ring->br, m);
		cq = &priv->tx_cq[i];
		taskqueue_enqueue(cq->tq, &cq->cq_task);
	}

	return (err);
}

/*
 * Flush ring buffers.
 */
void
mlx4_en_qflush(struct ifnet *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_tx_ring *ring = priv->tx_ring;
	struct mbuf *m;

	for (int i = 0; i < priv->tx_ring_num; i++, ring++) {
		spin_lock(&ring->tx_lock);
		while ((m = buf_ring_dequeue_sc(ring->br)) != NULL)
			m_freem(m);
		spin_unlock(&ring->tx_lock);
	}
	if_qflush(dev);
}
