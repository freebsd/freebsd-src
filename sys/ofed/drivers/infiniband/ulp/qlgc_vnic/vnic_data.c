/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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
 */

#include <net/inet_sock.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/vmalloc.h>

#include "vnic_util.h"
#include "vnic_viport.h"
#include "vnic_main.h"
#include "vnic_data.h"
#include "vnic_trailer.h"
#include "vnic_stats.h"

static void data_received_kick(struct io *io);
static void data_xmit_complete(struct io *io);

static void mc_data_recv_routine(struct io *io);
static void mc_data_post_recvs(struct mc_data *mc_data);
static void mc_data_recv_to_skbuff(struct viport *viport, struct sk_buff *skb,
	struct viport_trailer *trailer);

static u32 min_rcv_skb = 60;
module_param(min_rcv_skb, int, 0444);
MODULE_PARM_DESC(min_rcv_skb, "Packets of size (in bytes) less than"
		 " or equal this value will be copied during receive."
		 " Default 60");

static u32 min_xmt_skb = 60;
module_param(min_xmt_skb, int, 0444);
MODULE_PARM_DESC(min_xmit_skb, "Packets of size (in bytes) less than"
		 " or equal to this value will be copied during transmit."
		 "Default 60");

int data_init(struct data *data, struct viport *viport,
	      struct data_config *config, struct ib_pd *pd)
{
	DATA_FUNCTION("data_init()\n");

	data->parent = viport;
	data->config = config;
	data->ib_conn.viport = viport;
	data->ib_conn.ib_config = &config->ib_config;
	data->ib_conn.state = IB_CONN_UNINITTED;
	data->ib_conn.callback_thread = NULL;
	data->ib_conn.callback_thread_end = 0;

	if ((min_xmt_skb < 60) || (min_xmt_skb > 9000)) {
		DATA_ERROR("min_xmt_skb (%d) must be between 60 and 9000\n",
			   min_xmt_skb);
		goto failure;
	}
	if (vnic_ib_conn_init(&data->ib_conn, viport, pd,
			      &config->ib_config)) {
		DATA_ERROR("Data IB connection initialization failed\n");
		goto failure;
	}
	data->mr = ib_get_dma_mr(pd,
				 IB_ACCESS_LOCAL_WRITE |
				 IB_ACCESS_REMOTE_READ |
				 IB_ACCESS_REMOTE_WRITE);
	if (IS_ERR(data->mr)) {
		DATA_ERROR("failed to register memory for"
			   " data connection\n");
		goto destroy_conn;
	}

	data->ib_conn.cm_id = ib_create_cm_id(viport->config->ibdev,
					      vnic_ib_cm_handler,
					      &data->ib_conn);

	if (IS_ERR(data->ib_conn.cm_id)) {
		DATA_ERROR("creating data CM ID failed\n");
		goto dereg_mr;
	}

	return 0;

dereg_mr:
	ib_dereg_mr(data->mr);
destroy_conn:
	vnic_completion_cleanup(&data->ib_conn);
	ib_destroy_qp(data->ib_conn.qp);
	ib_destroy_cq(data->ib_conn.cq);
failure:
	return -1;
}

static void data_post_recvs(struct data *data)
{
	unsigned long flags;
	int i = 0;

	DATA_FUNCTION("data_post_recvs()\n");
	spin_lock_irqsave(&data->recv_ios_lock, flags);
	while (!list_empty(&data->recv_ios)) {
		struct io *io = list_entry(data->recv_ios.next,
					   struct io, list_ptrs);
		struct recv_io *recv_io = (struct recv_io *)io;

		list_del(&recv_io->io.list_ptrs);
		spin_unlock_irqrestore(&data->recv_ios_lock, flags);
		if (vnic_ib_post_recv(&data->ib_conn, &recv_io->io)) {
			viport_failure(data->parent);
			return;
		}
		i++;
		spin_lock_irqsave(&data->recv_ios_lock, flags);
	}
	spin_unlock_irqrestore(&data->recv_ios_lock, flags);
	DATA_INFO("data posted %d %p\n", i, &data->recv_ios);
}

static void data_init_pool_work_reqs(struct data *data,
				      struct recv_io *recv_io)
{
	struct recv_pool	*recv_pool = &data->recv_pool;
	struct xmit_pool	*xmit_pool = &data->xmit_pool;
	struct rdma_io		*rdma_io;
	struct rdma_dest	*rdma_dest;
	dma_addr_t		xmit_dma;
	u8			*xmit_data;
	unsigned int		i;

	INIT_LIST_HEAD(&data->recv_ios);
	spin_lock_init(&data->recv_ios_lock);
	spin_lock_init(&data->xmit_buf_lock);
	for (i = 0; i < data->config->num_recvs; i++) {
		recv_io[i].io.viport = data->parent;
		recv_io[i].io.routine = data_received_kick;
		recv_io[i].list.addr = data->region_data_dma;
		recv_io[i].list.length = 4;
		recv_io[i].list.lkey = data->mr->lkey;

		recv_io[i].io.rwr.wr_id = (u64)&recv_io[i].io;
		recv_io[i].io.rwr.sg_list = &recv_io[i].list;
		recv_io[i].io.rwr.num_sge = 1;

		list_add(&recv_io[i].io.list_ptrs, &data->recv_ios);
	}

	INIT_LIST_HEAD(&recv_pool->avail_recv_bufs);
	for (i = 0; i < recv_pool->pool_sz; i++) {
		rdma_dest = &recv_pool->recv_bufs[i];
		list_add(&rdma_dest->list_ptrs,
			 &recv_pool->avail_recv_bufs);
	}

	xmit_dma = xmit_pool->xmitdata_dma;
	xmit_data = xmit_pool->xmit_data;

	for (i = 0; i < xmit_pool->num_xmit_bufs; i++) {
		rdma_io = &xmit_pool->xmit_bufs[i];
		rdma_io->index = i;
		rdma_io->io.viport = data->parent;
		rdma_io->io.routine = data_xmit_complete;

		rdma_io->list[0].lkey = data->mr->lkey;
		rdma_io->list[1].lkey = data->mr->lkey;
		rdma_io->io.swr.wr_id = (u64)rdma_io;
		rdma_io->io.swr.sg_list = rdma_io->list;
		rdma_io->io.swr.num_sge = 2;
		rdma_io->io.swr.opcode = IB_WR_RDMA_WRITE;
		rdma_io->io.swr.send_flags = IB_SEND_SIGNALED;
		rdma_io->io.type = RDMA;

		rdma_io->data = xmit_data;
		rdma_io->data_dma = xmit_dma;

		xmit_data += ALIGN(min_xmt_skb, VIPORT_TRAILER_ALIGNMENT);
		xmit_dma += ALIGN(min_xmt_skb, VIPORT_TRAILER_ALIGNMENT);
		rdma_io->trailer = (struct viport_trailer *)xmit_data;
		rdma_io->trailer_dma = xmit_dma;
		xmit_data += sizeof(struct viport_trailer);
		xmit_dma += sizeof(struct viport_trailer);
	}

	xmit_pool->rdma_rkey = data->mr->rkey;
	xmit_pool->rdma_addr = xmit_pool->buf_pool_dma;
}

static void data_init_free_bufs_swrs(struct data *data)
{
	struct rdma_io		*rdma_io;
	struct send_io		*send_io;

	rdma_io = &data->free_bufs_io;
	rdma_io->io.viport = data->parent;
	rdma_io->io.routine = NULL;

	rdma_io->list[0].lkey = data->mr->lkey;

	rdma_io->io.swr.wr_id = (u64)rdma_io;
	rdma_io->io.swr.sg_list = rdma_io->list;
	rdma_io->io.swr.num_sge = 1;
	rdma_io->io.swr.opcode = IB_WR_RDMA_WRITE;
	rdma_io->io.swr.send_flags = IB_SEND_SIGNALED;
	rdma_io->io.type = RDMA;

	send_io = &data->kick_io;
	send_io->io.viport = data->parent;
	send_io->io.routine = NULL;

	send_io->list.addr = data->region_data_dma;
	send_io->list.length = 0;
	send_io->list.lkey = data->mr->lkey;

	send_io->io.swr.wr_id = (u64)send_io;
	send_io->io.swr.sg_list = &send_io->list;
	send_io->io.swr.num_sge = 1;
	send_io->io.swr.opcode = IB_WR_SEND;
	send_io->io.swr.send_flags = IB_SEND_SIGNALED;
	send_io->io.type = SEND;
}

static int data_init_buf_pools(struct data *data)
{
	struct recv_pool	*recv_pool = &data->recv_pool;
	struct xmit_pool	*xmit_pool = &data->xmit_pool;
	struct viport		*viport = data->parent;

	recv_pool->buf_pool_len =
	    sizeof(struct buff_pool_entry) * recv_pool->eioc_pool_sz;

	recv_pool->buf_pool = kzalloc(recv_pool->buf_pool_len, GFP_KERNEL);

	if (!recv_pool->buf_pool) {
		DATA_ERROR("failed allocating %d bytes"
			   " for recv pool bufpool\n",
			   recv_pool->buf_pool_len);
		goto failure;
	}

	recv_pool->buf_pool_dma =
	    ib_dma_map_single(viport->config->ibdev,
			      recv_pool->buf_pool, recv_pool->buf_pool_len,
			      DMA_TO_DEVICE);

	if (ib_dma_mapping_error(viport->config->ibdev, recv_pool->buf_pool_dma)) {
		DATA_ERROR("xmit buf_pool dma map error\n");
		goto free_recv_pool;
	}

	xmit_pool->buf_pool_len =
	    sizeof(struct buff_pool_entry) * xmit_pool->pool_sz;
	xmit_pool->buf_pool = kzalloc(xmit_pool->buf_pool_len, GFP_KERNEL);

	if (!xmit_pool->buf_pool) {
		DATA_ERROR("failed allocating %d bytes"
			   " for xmit pool bufpool\n",
			   xmit_pool->buf_pool_len);
		goto unmap_recv_pool;
	}

	xmit_pool->buf_pool_dma =
	    ib_dma_map_single(viport->config->ibdev,
			      xmit_pool->buf_pool, xmit_pool->buf_pool_len,
			      DMA_FROM_DEVICE);

	if (ib_dma_mapping_error(viport->config->ibdev, xmit_pool->buf_pool_dma)) {
		DATA_ERROR("xmit buf_pool dma map error\n");
		goto free_xmit_pool;
	}

	xmit_pool->xmit_data = kzalloc(xmit_pool->xmitdata_len, GFP_KERNEL);

	if (!xmit_pool->xmit_data) {
		DATA_ERROR("failed allocating %d bytes for xmit data\n",
			   xmit_pool->xmitdata_len);
		goto unmap_xmit_pool;
	}

	xmit_pool->xmitdata_dma =
	    ib_dma_map_single(viport->config->ibdev,
			      xmit_pool->xmit_data, xmit_pool->xmitdata_len,
			      DMA_TO_DEVICE);

	if (ib_dma_mapping_error(viport->config->ibdev, xmit_pool->xmitdata_dma)) {
		DATA_ERROR("xmit data dma map error\n");
		goto free_xmit_data;
	}

	return 0;

free_xmit_data:
	kfree(xmit_pool->xmit_data);
unmap_xmit_pool:
	ib_dma_unmap_single(data->parent->config->ibdev,
			    xmit_pool->buf_pool_dma,
			    xmit_pool->buf_pool_len, DMA_FROM_DEVICE);
free_xmit_pool:
	kfree(xmit_pool->buf_pool);
unmap_recv_pool:
	ib_dma_unmap_single(data->parent->config->ibdev,
			    recv_pool->buf_pool_dma,
			    recv_pool->buf_pool_len, DMA_TO_DEVICE);
free_recv_pool:
	kfree(recv_pool->buf_pool);
failure:
	return -1;
}

static void data_init_xmit_pool(struct data *data)
{
	struct xmit_pool	*xmit_pool = &data->xmit_pool;

	xmit_pool->pool_sz =
		be32_to_cpu(data->eioc_pool_parms.num_recv_pool_entries);
	xmit_pool->buffer_sz =
		be32_to_cpu(data->eioc_pool_parms.size_recv_pool_entry);

	xmit_pool->notify_count = 0;
	xmit_pool->notify_bundle = data->config->notify_bundle;
	xmit_pool->next_xmit_pool = 0;
	xmit_pool->num_xmit_bufs = xmit_pool->notify_bundle * 2;
	xmit_pool->next_xmit_buf = 0;
	xmit_pool->last_comp_buf = xmit_pool->num_xmit_bufs - 1;
	/* This assumes that data_init_recv_pool has been called
	 * before.
	 */
	data->max_mtu = MAX_PAYLOAD(min((data)->recv_pool.buffer_sz,
				   (data)->xmit_pool.buffer_sz)) - VLAN_ETH_HLEN;

	xmit_pool->kick_count = 0;
	xmit_pool->kick_byte_count = 0;

	xmit_pool->send_kicks =
	  be32_to_cpu(data->
			eioc_pool_parms.num_recv_pool_entries_before_kick)
	  || be32_to_cpu(data->
			eioc_pool_parms.num_recv_pool_bytes_before_kick);
	xmit_pool->kick_bundle =
	    be32_to_cpu(data->
			eioc_pool_parms.num_recv_pool_entries_before_kick);
	xmit_pool->kick_byte_bundle =
	    be32_to_cpu(data->
			eioc_pool_parms.num_recv_pool_bytes_before_kick);

	xmit_pool->need_buffers = 1;

	xmit_pool->xmitdata_len =
	    BUFFER_SIZE(min_xmt_skb) * xmit_pool->num_xmit_bufs;
}

static void data_init_recv_pool(struct data *data)
{
	struct recv_pool	*recv_pool = &data->recv_pool;

	recv_pool->pool_sz = data->config->host_recv_pool_entries;
	recv_pool->eioc_pool_sz =
		be32_to_cpu(data->host_pool_parms.num_recv_pool_entries);
	if (recv_pool->pool_sz > recv_pool->eioc_pool_sz)
		recv_pool->pool_sz =
		    be32_to_cpu(data->host_pool_parms.num_recv_pool_entries);

	recv_pool->buffer_sz =
		    be32_to_cpu(data->host_pool_parms.size_recv_pool_entry);

	recv_pool->sz_free_bundle =
		be32_to_cpu(data->
			host_pool_parms.free_recv_pool_entries_per_update);
	recv_pool->num_free_bufs = 0;
	recv_pool->num_posted_bufs = 0;

	recv_pool->next_full_buf = 0;
	recv_pool->next_free_buf = 0;
	recv_pool->kick_on_free  = 0;
}

int data_connect(struct data *data)
{
	struct xmit_pool	*xmit_pool = &data->xmit_pool;
	struct recv_pool	*recv_pool = &data->recv_pool;
	struct recv_io		*recv_io;
	unsigned int		sz;
	struct viport		*viport = data->parent;

	DATA_FUNCTION("data_connect()\n");

	/* Do not interchange the order of the functions
	 * called below as this will affect the MAX MTU
	 * calculation
	 */

	data_init_recv_pool(data);
	data_init_xmit_pool(data);

	sz = sizeof(struct rdma_dest) * recv_pool->pool_sz    +
	     sizeof(struct recv_io) * data->config->num_recvs +
	     sizeof(struct rdma_io) * xmit_pool->num_xmit_bufs;

	data->local_storage = vmalloc(sz);

	if (!data->local_storage) {
		DATA_ERROR("failed allocating %d bytes"
			   " local storage\n", sz);
		goto out;
	}

	memset(data->local_storage, 0, sz);

	recv_pool->recv_bufs = (struct rdma_dest *)data->local_storage;
	sz = sizeof(struct rdma_dest) * recv_pool->pool_sz;

	recv_io = (struct recv_io *)(data->local_storage + sz);
	sz += sizeof(struct recv_io) * data->config->num_recvs;

	xmit_pool->xmit_bufs = (struct rdma_io *)(data->local_storage + sz);
	data->region_data = kzalloc(4, GFP_KERNEL);

	if (!data->region_data) {
		DATA_ERROR("failed to alloc memory for region data\n");
		goto free_local_storage;
	}

	data->region_data_dma =
	    ib_dma_map_single(viport->config->ibdev,
			      data->region_data, 4, DMA_BIDIRECTIONAL);

	if (ib_dma_mapping_error(viport->config->ibdev, data->region_data_dma)) {
		DATA_ERROR("region data dma map error\n");
		goto free_region_data;
	}

	if (data_init_buf_pools(data))
		goto unmap_region_data;

	data_init_free_bufs_swrs(data);
	data_init_pool_work_reqs(data, recv_io);

	data_post_recvs(data);

	if (vnic_ib_cm_connect(&data->ib_conn))
		goto unmap_region_data;

	return 0;

unmap_region_data:
	ib_dma_unmap_single(data->parent->config->ibdev,
			    data->region_data_dma, 4, DMA_BIDIRECTIONAL);
free_region_data:
		kfree(data->region_data);
free_local_storage:
		vfree(data->local_storage);
out:
	return -1;
}

static void data_add_free_buffer(struct data *data, int index,
				 struct rdma_dest *rdma_dest)
{
	struct recv_pool *pool = &data->recv_pool;
	struct buff_pool_entry *bpe;
	dma_addr_t vaddr_dma;

	DATA_FUNCTION("data_add_free_buffer()\n");
	rdma_dest->trailer->connection_hash_and_valid = 0;
	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   pool->buf_pool_dma, pool->buf_pool_len,
				   DMA_TO_DEVICE);

	bpe = &pool->buf_pool[index];
	bpe->rkey = cpu_to_be32(data->mr->rkey);
	vaddr_dma = ib_dma_map_single(data->parent->config->ibdev,
					rdma_dest->data, pool->buffer_sz,
					DMA_FROM_DEVICE);
	if (ib_dma_mapping_error(data->parent->config->ibdev, vaddr_dma)) {
		DATA_ERROR("rdma_dest->data dma map error\n");
		goto failure;
	}
	bpe->remote_addr = cpu_to_be64(vaddr_dma);
	bpe->valid = (u32) (rdma_dest - &pool->recv_bufs[0]) + 1;
	++pool->num_free_bufs;
failure:
	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      pool->buf_pool_dma, pool->buf_pool_len,
				      DMA_TO_DEVICE);
}

/* NOTE: this routine is not reentrant */
static void data_alloc_buffers(struct data *data, int initial_allocation)
{
	struct recv_pool *pool = &data->recv_pool;
	struct rdma_dest *rdma_dest;
	struct sk_buff *skb;
	int index;

	DATA_FUNCTION("data_alloc_buffers()\n");
	index = ADD(pool->next_free_buf, pool->num_free_bufs,
		    pool->eioc_pool_sz);

	while (!list_empty(&pool->avail_recv_bufs)) {
		rdma_dest =
		    list_entry(pool->avail_recv_bufs.next,
			       struct rdma_dest, list_ptrs);
		if (!rdma_dest->skb) {
			if (initial_allocation)
				skb = alloc_skb(pool->buffer_sz + 2,
						GFP_KERNEL);
			else
				skb = dev_alloc_skb(pool->buffer_sz + 2);
			if (!skb)
				break;
			skb_reserve(skb, 2);
			skb_put(skb, pool->buffer_sz);
			rdma_dest->skb = skb;
			rdma_dest->data = skb->data;
			rdma_dest->trailer =
			  (struct viport_trailer *)(rdma_dest->data +
						    pool->buffer_sz -
						    sizeof(struct
							   viport_trailer));
		}
		rdma_dest->trailer->connection_hash_and_valid = 0;

		list_del_init(&rdma_dest->list_ptrs);

		data_add_free_buffer(data, index, rdma_dest);
		index = NEXT(index, pool->eioc_pool_sz);
	}
}

static void data_send_kick_message(struct data *data)
{
	struct xmit_pool *pool = &data->xmit_pool;
	DATA_FUNCTION("data_send_kick_message()\n");
	/* stop timer for bundle_timeout */
	if (data->kick_timer_on) {
		del_timer(&data->kick_timer);
		data->kick_timer_on = 0;
	}
	pool->kick_count = 0;
	pool->kick_byte_count = 0;

	/* TODO: keep track of when kick is outstanding, and
	 * don't reuse until complete
	 */
	if (vnic_ib_post_send(&data->ib_conn, &data->free_bufs_io.io)) {
		DATA_ERROR("failed to post send\n");
		viport_failure(data->parent);
	}
}

static void data_send_free_recv_buffers(struct data *data)
{
	struct recv_pool *pool = &data->recv_pool;
	struct ib_send_wr *swr = &data->free_bufs_io.io.swr;

	int bufs_sent = 0;
	u64 rdma_addr;
	u32 offset;
	u32 sz;
	unsigned int num_to_send, next_increment;

	DATA_FUNCTION("data_send_free_recv_buffers()\n");

	for (num_to_send = pool->sz_free_bundle;
	     num_to_send <= pool->num_free_bufs;
	     num_to_send += pool->sz_free_bundle) {
		/* handle multiple bundles as one when possible. */
		next_increment = num_to_send + pool->sz_free_bundle;
		if ((next_increment <= pool->num_free_bufs)
		    && (pool->next_free_buf + next_increment <=
			pool->eioc_pool_sz))
			continue;

		offset = pool->next_free_buf *
				sizeof(struct buff_pool_entry);
		sz = num_to_send * sizeof(struct buff_pool_entry);
		rdma_addr = pool->eioc_rdma_addr + offset;
		swr->sg_list->length = sz;
		swr->sg_list->addr = pool->buf_pool_dma + offset;
		swr->wr.rdma.remote_addr = rdma_addr;

		if (vnic_ib_post_send(&data->ib_conn,
		    &data->free_bufs_io.io)) {
			DATA_ERROR("failed to post send\n");
			viport_failure(data->parent);
			return;
		}
		INC(pool->next_free_buf, num_to_send, pool->eioc_pool_sz);
		pool->num_free_bufs -= num_to_send;
		pool->num_posted_bufs += num_to_send;
		bufs_sent = 1;
	}

	if (bufs_sent) {
		if (pool->kick_on_free)
			data_send_kick_message(data);
	}
	if (pool->num_posted_bufs == 0) {
		struct vnic *vnic = data->parent->vnic;
		unsigned long flags;

		spin_lock_irqsave(&vnic->current_path_lock, flags);
		if (vnic->current_path == &vnic->primary_path) {
			spin_unlock_irqrestore(&vnic->current_path_lock, flags);
			DATA_ERROR("%s: primary path: "
					"unable to allocate receive buffers\n",
					vnic->config->name);
		} else {
			if (vnic->current_path == &vnic->secondary_path) {
				spin_unlock_irqrestore(&vnic->current_path_lock,
							flags);
				DATA_ERROR("%s: secondary path: "
					"unable to allocate receive buffers\n",
					vnic->config->name);
			} else
				spin_unlock_irqrestore(&vnic->current_path_lock,
							flags);
		}
		data->ib_conn.state = IB_CONN_ERRORED;
		viport_failure(data->parent);
	}
}

void data_connected(struct data *data)
{
	DATA_FUNCTION("data_connected()\n");
	data->free_bufs_io.io.swr.wr.rdma.rkey =
				data->recv_pool.eioc_rdma_rkey;
	data_alloc_buffers(data, 1);
	data_send_free_recv_buffers(data);
	data->connected = 1;
}

void data_disconnect(struct data *data)
{
	struct xmit_pool *xmit_pool = &data->xmit_pool;
	struct recv_pool *recv_pool = &data->recv_pool;
	unsigned int i;

	DATA_FUNCTION("data_disconnect()\n");

	data->connected = 0;
	if (data->kick_timer_on) {
		del_timer_sync(&data->kick_timer);
		data->kick_timer_on = 0;
	}

	if (ib_send_cm_dreq(data->ib_conn.cm_id, NULL, 0))
		DATA_ERROR("data CM DREQ sending failed\n");
	data->ib_conn.state = IB_CONN_DISCONNECTED;

	vnic_completion_cleanup(&data->ib_conn);

	for (i = 0; i < xmit_pool->num_xmit_bufs; i++) {
		if (xmit_pool->xmit_bufs[i].skb)
			dev_kfree_skb(xmit_pool->xmit_bufs[i].skb);
		xmit_pool->xmit_bufs[i].skb = NULL;

	}
	for (i = 0; i < recv_pool->pool_sz; i++) {
		if (data->recv_pool.recv_bufs[i].skb)
			dev_kfree_skb(recv_pool->recv_bufs[i].skb);
		recv_pool->recv_bufs[i].skb = NULL;
	}
	vfree(data->local_storage);
	if (data->region_data) {
		ib_dma_unmap_single(data->parent->config->ibdev,
				    data->region_data_dma, 4,
				    DMA_BIDIRECTIONAL);
		kfree(data->region_data);
	}

	if (recv_pool->buf_pool) {
		ib_dma_unmap_single(data->parent->config->ibdev,
				    recv_pool->buf_pool_dma,
				    recv_pool->buf_pool_len, DMA_TO_DEVICE);
		kfree(recv_pool->buf_pool);
	}

	if (xmit_pool->buf_pool) {
		ib_dma_unmap_single(data->parent->config->ibdev,
				    xmit_pool->buf_pool_dma,
				    xmit_pool->buf_pool_len, DMA_FROM_DEVICE);
		kfree(xmit_pool->buf_pool);
	}

	if (xmit_pool->xmit_data) {
		ib_dma_unmap_single(data->parent->config->ibdev,
				    xmit_pool->xmitdata_dma,
				    xmit_pool->xmitdata_len, DMA_TO_DEVICE);
		kfree(xmit_pool->xmit_data);
	}
}

void data_cleanup(struct data *data)
{
	ib_destroy_cm_id(data->ib_conn.cm_id);

	/* Completion callback cleanup called again.
	 * This is to cleanup the threads in case there is an
	 * error before state LINK_DATACONNECT due to which
	 * data_disconnect is not called.
	 */
	vnic_completion_cleanup(&data->ib_conn);
	ib_destroy_qp(data->ib_conn.qp);
	ib_destroy_cq(data->ib_conn.cq);
	ib_dereg_mr(data->mr);

}

static int data_alloc_xmit_buffer(struct data *data, struct sk_buff *skb,
				  struct buff_pool_entry **pp_bpe,
				  struct rdma_io **pp_rdma_io,
				  int *last)
{
	struct xmit_pool	*pool = &data->xmit_pool;
	unsigned long		flags;
	int			ret;

	DATA_FUNCTION("data_alloc_xmit_buffer()\n");

	spin_lock_irqsave(&data->xmit_buf_lock, flags);
	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   pool->buf_pool_dma, pool->buf_pool_len,
				   DMA_TO_DEVICE);
	*last = 0;
	*pp_rdma_io = &pool->xmit_bufs[pool->next_xmit_buf];
	*pp_bpe = &pool->buf_pool[pool->next_xmit_pool];

	if ((*pp_bpe)->valid && pool->next_xmit_buf !=
	     pool->last_comp_buf) {
		INC(pool->next_xmit_buf, 1, pool->num_xmit_bufs);
		INC(pool->next_xmit_pool, 1, pool->pool_sz);
		if (!pool->buf_pool[pool->next_xmit_pool].valid) {
			DATA_INFO("just used the last EIOU"
				  " receive buffer\n");
			*last = 1;
			pool->need_buffers = 1;
			vnic_stop_xmit(data->parent->vnic,
				       data->parent->parent);
			data_kickreq_stats(data);
		} else if (pool->next_xmit_buf == pool->last_comp_buf) {
			DATA_INFO("just used our last xmit buffer\n");
			pool->need_buffers = 1;
			vnic_stop_xmit(data->parent->vnic,
				       data->parent->parent);
		}
		(*pp_rdma_io)->skb = skb;
		(*pp_bpe)->valid = 0;
		ret = 0;
	} else {
		data_no_xmitbuf_stats(data);
		DATA_ERROR("Out of xmit buffers\n");
		vnic_stop_xmit(data->parent->vnic,
			       data->parent->parent);
		ret = -1;
	}

	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      pool->buf_pool_dma,
				      pool->buf_pool_len, DMA_TO_DEVICE);
	spin_unlock_irqrestore(&data->xmit_buf_lock, flags);
	return ret;
}

static void data_rdma_packet(struct data *data, struct buff_pool_entry *bpe,
			     struct rdma_io *rdma_io)
{
	struct ib_send_wr	*swr;
	struct sk_buff		*skb;
	dma_addr_t		trailer_data_dma;
	dma_addr_t		skb_data_dma;
	struct xmit_pool	*xmit_pool = &data->xmit_pool;
	struct viport		*viport = data->parent;
	u8			*d;
	int			len;
	int			fill_len;

	DATA_FUNCTION("data_rdma_packet()\n");
	swr = &rdma_io->io.swr;
	skb = rdma_io->skb;
	len = ALIGN(rdma_io->len, VIPORT_TRAILER_ALIGNMENT);
	fill_len = len - skb->len;

	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   xmit_pool->xmitdata_dma,
				   xmit_pool->xmitdata_len, DMA_TO_DEVICE);

	d = (u8 *) rdma_io->trailer - fill_len;
	trailer_data_dma = rdma_io->trailer_dma - fill_len;
	memset(d, 0, fill_len);

	swr->sg_list[0].length = skb->len;
	if (skb->len <= min_xmt_skb) {
		memcpy(rdma_io->data, skb->data, skb->len);
		swr->sg_list[0].lkey = data->mr->lkey;
		swr->sg_list[0].addr = rdma_io->data_dma;
		dev_kfree_skb_any(skb);
		rdma_io->skb = NULL;
	} else {
		swr->sg_list[0].lkey = data->mr->lkey;

		skb_data_dma = ib_dma_map_single(viport->config->ibdev,
						skb->data, skb->len,
						DMA_TO_DEVICE);

		if (ib_dma_mapping_error(viport->config->ibdev, skb_data_dma)) {
			DATA_ERROR("skb data dma map error\n");
			goto failure;
		}

		rdma_io->skb_data_dma = skb_data_dma;

		swr->sg_list[0].addr = skb_data_dma;
		skb_orphan(skb);
	}
	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   xmit_pool->buf_pool_dma,
				   xmit_pool->buf_pool_len, DMA_TO_DEVICE);

	swr->sg_list[1].addr = trailer_data_dma;
	swr->sg_list[1].length = fill_len + sizeof(struct viport_trailer);
	swr->sg_list[0].lkey = data->mr->lkey;
	swr->wr.rdma.remote_addr = be64_to_cpu(bpe->remote_addr);
	swr->wr.rdma.remote_addr += data->xmit_pool.buffer_sz;
	swr->wr.rdma.remote_addr -= (sizeof(struct viport_trailer) + len);
	swr->wr.rdma.rkey = be32_to_cpu(bpe->rkey);

	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      xmit_pool->buf_pool_dma,
				      xmit_pool->buf_pool_len, DMA_TO_DEVICE);

	/* If VNIC_FEAT_RDMA_IMMED is supported then change the work request
	 * opcode to IB_WR_RDMA_WRITE_WITH_IMM
	 */

	if (data->parent->features_supported & VNIC_FEAT_RDMA_IMMED) {
		swr->ex.imm_data = 0;
		swr->opcode = IB_WR_RDMA_WRITE_WITH_IMM;
	}

	data->xmit_pool.notify_count++;
	if (data->xmit_pool.notify_count >= data->xmit_pool.notify_bundle) {
		data->xmit_pool.notify_count = 0;
		swr->send_flags = IB_SEND_SIGNALED;
	} else {
		swr->send_flags = 0;
	}
	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      xmit_pool->xmitdata_dma,
				      xmit_pool->xmitdata_len, DMA_TO_DEVICE);
	if (vnic_ib_post_send(&data->ib_conn, &rdma_io->io)) {
		DATA_ERROR("failed to post send for data RDMA write\n");
		viport_failure(data->parent);
		goto failure;
	}

	data_xmits_stats(data);
failure:
	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      xmit_pool->xmitdata_dma,
				      xmit_pool->xmitdata_len, DMA_TO_DEVICE);
}

static void data_kick_timeout_handler(unsigned long arg)
{
	struct data *data = (struct data *)arg;

	DATA_FUNCTION("data_kick_timeout_handler()\n");
	data->kick_timer_on = 0;
	data_send_kick_message(data);
}

int data_xmit_packet(struct data *data, struct sk_buff *skb)
{
	struct xmit_pool	*pool = &data->xmit_pool;
	struct rdma_io		*rdma_io;
	struct buff_pool_entry	*bpe;
	struct viport_trailer	*trailer;
	unsigned int		sz = skb->len;
	int			last;

	DATA_FUNCTION("data_xmit_packet()\n");
	if (sz > pool->buffer_sz) {
		DATA_ERROR("outbound packet too large, size = %d\n", sz);
		return -1;
	}

	if (data_alloc_xmit_buffer(data, skb, &bpe, &rdma_io, &last)) {
		DATA_ERROR("error in allocating data xmit buffer\n");
		return -1;
	}

	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   pool->xmitdata_dma, pool->xmitdata_len,
				   DMA_TO_DEVICE);
	trailer = rdma_io->trailer;

	memset(trailer, 0, sizeof *trailer);
	memcpy(trailer->dest_mac_addr, skb->data, ETH_ALEN);

	if (skb->sk)
		trailer->connection_hash_and_valid = 0x40 |
			 ((be16_to_cpu(inet_sk(skb->sk)->sport) +
			   be16_to_cpu(inet_sk(skb->sk)->dport)) & 0x3f);

	trailer->connection_hash_and_valid |= CHV_VALID;

	if ((sz > 16) && (*(__be16 *) (skb->data + 12) ==
			   __constant_cpu_to_be16(ETH_P_8021Q))) {
		trailer->vlan = *(__be16 *) (skb->data + 14);
		memmove(skb->data + 4, skb->data, 12);
		skb_pull(skb, 4);
		sz -= 4;
		trailer->pkt_flags |= PF_VLAN_INSERT;
	}
	if (last)
		trailer->pkt_flags |= PF_KICK;
	if (sz < ETH_ZLEN) {
		/* EIOU requires all packets to be
		 * of ethernet minimum packet size.
		 */
		trailer->data_length = __constant_cpu_to_be16(ETH_ZLEN);
		rdma_io->len = ETH_ZLEN;
	} else {
		trailer->data_length = cpu_to_be16(sz);
		rdma_io->len = sz;
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		trailer->tx_chksum_flags = TX_CHKSUM_FLAGS_CHECKSUM_V4
		    | TX_CHKSUM_FLAGS_IP_CHECKSUM
		    | TX_CHKSUM_FLAGS_TCP_CHECKSUM
		    | TX_CHKSUM_FLAGS_UDP_CHECKSUM;
	}

	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      pool->xmitdata_dma, pool->xmitdata_len,
				      DMA_TO_DEVICE);
	data_rdma_packet(data, bpe, rdma_io);

	if (pool->send_kicks) {
		/* EIOC needs kicks to inform it of sent packets */
		pool->kick_count++;
		pool->kick_byte_count += sz;
		if ((pool->kick_count >= pool->kick_bundle)
		    || (pool->kick_byte_count >= pool->kick_byte_bundle)) {
			data_send_kick_message(data);
		} else if (pool->kick_count == 1) {
			init_timer(&data->kick_timer);
			/* timeout_before_kick is in usec */
			data->kick_timer.expires =
			   msecs_to_jiffies(be32_to_cpu(data->
				eioc_pool_parms.timeout_before_kick) * 1000)
				+ jiffies;
			data->kick_timer.data = (unsigned long)data;
			data->kick_timer.function = data_kick_timeout_handler;
			add_timer(&data->kick_timer);
			data->kick_timer_on = 1;
		}
	}
	return 0;
}

static void data_check_xmit_buffers(struct data *data)
{
	struct xmit_pool *pool = &data->xmit_pool;
	unsigned long flags;

	DATA_FUNCTION("data_check_xmit_buffers()\n");
	spin_lock_irqsave(&data->xmit_buf_lock, flags);
	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   pool->buf_pool_dma, pool->buf_pool_len,
				   DMA_TO_DEVICE);

	if (data->xmit_pool.need_buffers
	    && pool->buf_pool[pool->next_xmit_pool].valid
	    && pool->next_xmit_buf != pool->last_comp_buf) {
		data->xmit_pool.need_buffers = 0;
		vnic_restart_xmit(data->parent->vnic,
				  data->parent->parent);
		DATA_INFO("there are free xmit buffers\n");
	}
	ib_dma_sync_single_for_device(data->parent->config->ibdev,
				      pool->buf_pool_dma, pool->buf_pool_len,
				      DMA_TO_DEVICE);

	spin_unlock_irqrestore(&data->xmit_buf_lock, flags);
}

static struct sk_buff *data_recv_to_skbuff(struct data *data,
					   struct rdma_dest *rdma_dest)
{
	struct viport_trailer *trailer;
	struct sk_buff *skb = NULL;
	int start;
	unsigned int len;
	u8 rx_chksum_flags;

	DATA_FUNCTION("data_recv_to_skbuff()\n");
	trailer = rdma_dest->trailer;
	start = data_offset(data, trailer);
	len = data_len(data, trailer);

	if (len <= min_rcv_skb)
		skb = dev_alloc_skb(len + VLAN_HLEN + 2);
			 /* leave room for VLAN header and alignment */
	if (skb) {
		skb_reserve(skb, VLAN_HLEN + 2);
		memcpy(skb->data, rdma_dest->data + start, len);
		skb_put(skb, len);
	} else {
		skb = rdma_dest->skb;
		rdma_dest->skb = NULL;
		rdma_dest->trailer = NULL;
		rdma_dest->data = NULL;
		skb_pull(skb, start);
		skb_trim(skb, len);
	}

	rx_chksum_flags = trailer->rx_chksum_flags;
	DATA_INFO("rx_chksum_flags = %d, LOOP = %c, IP = %c,"
	     " TCP = %c, UDP = %c\n",
	     rx_chksum_flags,
	     (rx_chksum_flags & RX_CHKSUM_FLAGS_LOOPBACK) ? 'Y' : 'N',
	     (rx_chksum_flags & RX_CHKSUM_FLAGS_IP_CHECKSUM_SUCCEEDED) ? 'Y'
	     : (rx_chksum_flags & RX_CHKSUM_FLAGS_IP_CHECKSUM_FAILED) ? 'N' :
	     '-',
	     (rx_chksum_flags & RX_CHKSUM_FLAGS_TCP_CHECKSUM_SUCCEEDED) ? 'Y'
	     : (rx_chksum_flags & RX_CHKSUM_FLAGS_TCP_CHECKSUM_FAILED) ? 'N' :
	     '-',
	     (rx_chksum_flags & RX_CHKSUM_FLAGS_UDP_CHECKSUM_SUCCEEDED) ? 'Y'
	     : (rx_chksum_flags & RX_CHKSUM_FLAGS_UDP_CHECKSUM_FAILED) ? 'N' :
	     '-');

	if ((rx_chksum_flags & RX_CHKSUM_FLAGS_LOOPBACK)
	    || ((rx_chksum_flags & RX_CHKSUM_FLAGS_IP_CHECKSUM_SUCCEEDED)
		&& ((rx_chksum_flags & RX_CHKSUM_FLAGS_TCP_CHECKSUM_SUCCEEDED)
		    || (rx_chksum_flags &
			RX_CHKSUM_FLAGS_UDP_CHECKSUM_SUCCEEDED))))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		skb->ip_summed = CHECKSUM_NONE;

	if ((trailer->pkt_flags & PF_VLAN_INSERT) &&
		!(data->parent->features_supported & VNIC_FEAT_IGNORE_VLAN)) {
		u8 *rv;

		rv = skb_push(skb, 4);
		memmove(rv, rv + 4, 12);
		*(__be16 *) (rv + 12) = __constant_cpu_to_be16(ETH_P_8021Q);
		if (trailer->pkt_flags & PF_PVID_OVERRIDDEN)
			*(__be16 *) (rv + 14) = trailer->vlan &
					__constant_cpu_to_be16(0xF000);
		else
			*(__be16 *) (rv + 14) = trailer->vlan;
	}

	return skb;
}

static int data_incoming_recv(struct data *data)
{
	struct recv_pool *pool = &data->recv_pool;
	struct rdma_dest *rdma_dest;
	struct viport_trailer *trailer;
	struct buff_pool_entry *bpe;
	struct sk_buff *skb;
	dma_addr_t vaddr_dma;

	DATA_FUNCTION("data_incoming_recv()\n");
	if (pool->next_full_buf == pool->next_free_buf)
		return -1;
	bpe = &pool->buf_pool[pool->next_full_buf];
	vaddr_dma = be64_to_cpu(bpe->remote_addr);
	rdma_dest = &pool->recv_bufs[bpe->valid - 1];
	trailer = rdma_dest->trailer;

	if (!trailer
	    || !(trailer->connection_hash_and_valid & CHV_VALID))
		return -1;

	/* received a packet */
	if (trailer->pkt_flags & PF_KICK)
		pool->kick_on_free = 1;

	skb = data_recv_to_skbuff(data, rdma_dest);

	if (skb) {
		vnic_recv_packet(data->parent->vnic,
				 data->parent->parent, skb);
		list_add(&rdma_dest->list_ptrs, &pool->avail_recv_bufs);
	}

	ib_dma_unmap_single(data->parent->config->ibdev,
			    vaddr_dma, pool->buffer_sz,
			    DMA_FROM_DEVICE);
	ib_dma_sync_single_for_cpu(data->parent->config->ibdev,
				   pool->buf_pool_dma, pool->buf_pool_len,
				   DMA_TO_DEVICE);

	bpe->valid = 0;
	ib_dma_sync_single_for_device(data->parent->config->ibdev,
					pool->buf_pool_dma, pool->buf_pool_len,
					DMA_TO_DEVICE);

	INC(pool->next_full_buf, 1, pool->eioc_pool_sz);
	pool->num_posted_bufs--;
	data_recvs_stats(data);
	return 0;
}

static void data_received_kick(struct io *io)
{
	struct data *data = &io->viport->data;
	unsigned long flags;

	DATA_FUNCTION("data_received_kick()\n");
	data_note_kickrcv_time();
	spin_lock_irqsave(&data->recv_ios_lock, flags);
	list_add(&io->list_ptrs, &data->recv_ios);
	spin_unlock_irqrestore(&data->recv_ios_lock, flags);
	data_post_recvs(data);
	data_rcvkicks_stats(data);
	data_check_xmit_buffers(data);

	while (!data_incoming_recv(data));

	if (data->connected) {
		data_alloc_buffers(data, 0);
		data_send_free_recv_buffers(data);
	}
}

static void data_xmit_complete(struct io *io)
{
	struct rdma_io *rdma_io = (struct rdma_io *)io;
	struct data *data = &io->viport->data;
	struct xmit_pool *pool = &data->xmit_pool;
	struct sk_buff *skb;

	DATA_FUNCTION("data_xmit_complete()\n");

	if (rdma_io->skb)
		ib_dma_unmap_single(data->parent->config->ibdev,
				    rdma_io->skb_data_dma, rdma_io->skb->len,
				    DMA_TO_DEVICE);

	while (pool->last_comp_buf != rdma_io->index) {
		INC(pool->last_comp_buf, 1, pool->num_xmit_bufs);
		skb = pool->xmit_bufs[pool->last_comp_buf].skb;
		if (skb)
			dev_kfree_skb_any(skb);
		pool->xmit_bufs[pool->last_comp_buf].skb = NULL;
	}

	data_check_xmit_buffers(data);
}

static int mc_data_alloc_skb(struct ud_recv_io *recv_io, u32 len,
				int initial_allocation)
{
	struct sk_buff *skb;
	struct mc_data *mc_data = &recv_io->io.viport->mc_data;

	DATA_FUNCTION("mc_data_alloc_skb\n");
	if (initial_allocation)
		skb = alloc_skb(len, GFP_KERNEL);
	else
		skb = dev_alloc_skb(len);
	if (!skb) {
		DATA_ERROR("failed to alloc MULTICAST skb\n");
		return -1;
	}
	skb_put(skb, len);
	recv_io->skb = skb;

	recv_io->skb_data_dma = ib_dma_map_single(
					recv_io->io.viport->config->ibdev,
					skb->data, skb->len,
					DMA_FROM_DEVICE);

	if (ib_dma_mapping_error(recv_io->io.viport->config->ibdev,
			recv_io->skb_data_dma)) {
		DATA_ERROR("skb data dma map error\n");
		dev_kfree_skb(skb);
		return -1;
	}

	recv_io->list[0].addr = recv_io->skb_data_dma;
	recv_io->list[0].length = sizeof(struct ib_grh);
	recv_io->list[0].lkey = mc_data->mr->lkey;

	recv_io->list[1].addr = recv_io->skb_data_dma + sizeof(struct ib_grh);
	recv_io->list[1].length = len - sizeof(struct ib_grh);
	recv_io->list[1].lkey = mc_data->mr->lkey;

	recv_io->io.rwr.wr_id = (u64)&recv_io->io;
	recv_io->io.rwr.sg_list = recv_io->list;
	recv_io->io.rwr.num_sge = 2;
	recv_io->io.rwr.next = NULL;

	return 0;
}

static int mc_data_alloc_buffers(struct mc_data *mc_data)
{
	unsigned int i, num;
	struct ud_recv_io *bufs = NULL, *recv_io;

	DATA_FUNCTION("mc_data_alloc_buffers\n");
	if (!mc_data->skb_len) {
		unsigned int len;
		/* align multicast msg buffer on viport_trailer boundary */
		len = (MCAST_MSG_SIZE + VIPORT_TRAILER_ALIGNMENT - 1) &
				(~((unsigned int)VIPORT_TRAILER_ALIGNMENT - 1));
		/*
		 * Add size of grh and trailer -
		 * note, we don't need a + 4 for vlan because we have room in
		 * netbuf for grh & trailer and we'll strip them both, so there
		 * will be room enough to handle the 4 byte insertion for vlan.
		 */
		len +=	sizeof(struct ib_grh) +
				sizeof(struct viport_trailer);
		mc_data->skb_len = len;
		DATA_INFO("mc_data->skb_len %d (sizes:%d %d)\n",
					len, (int)sizeof(struct ib_grh),
					(int)sizeof(struct viport_trailer));
	}
	mc_data->recv_len = sizeof(struct ud_recv_io) * mc_data->num_recvs;
	bufs = kmalloc(mc_data->recv_len, GFP_KERNEL);
	if (!bufs) {
		DATA_ERROR("failed to allocate MULTICAST buffers size:%d\n",
				mc_data->recv_len);
		return -1;
	}
	DATA_INFO("allocated num_recvs:%d recv_len:%d \n",
			mc_data->num_recvs, mc_data->recv_len);
	for (num = 0; num < mc_data->num_recvs; num++) {
		recv_io = &bufs[num];
		recv_io->len = mc_data->skb_len;
		recv_io->io.type = RECV_UD;
		recv_io->io.viport = mc_data->parent;
		recv_io->io.routine = mc_data_recv_routine;

		if (mc_data_alloc_skb(recv_io, mc_data->skb_len, 1)) {
			for (i = 0; i < num; i++) {
				recv_io = &bufs[i];
				ib_dma_unmap_single(recv_io->io.viport->config->ibdev,
						    recv_io->skb_data_dma,
						    recv_io->skb->len,
						    DMA_FROM_DEVICE);
				dev_kfree_skb(recv_io->skb);
			}
			kfree(bufs);
			return -1;
		}
		list_add_tail(&recv_io->io.list_ptrs,
						 &mc_data->avail_recv_ios_list);
	}
	mc_data->recv_ios = bufs;
	return 0;
}

void vnic_mc_data_cleanup(struct mc_data *mc_data)
{
	unsigned int num;

	DATA_FUNCTION("vnic_mc_data_cleanup()\n");
	vnic_completion_cleanup(&mc_data->ib_conn);
	if (!IS_ERR(mc_data->ib_conn.qp)) {
		ib_destroy_qp(mc_data->ib_conn.qp);
		mc_data->ib_conn.qp = (struct ib_qp *)ERR_PTR(-EINVAL);
	}
	if (!IS_ERR(mc_data->ib_conn.cq)) {
		ib_destroy_cq(mc_data->ib_conn.cq);
		mc_data->ib_conn.cq = (struct ib_cq *)ERR_PTR(-EINVAL);
	}
	if (mc_data->recv_ios) {
		for (num = 0; num < mc_data->num_recvs; num++) {
			if (mc_data->recv_ios[num].skb)
				dev_kfree_skb(mc_data->recv_ios[num].skb);
			mc_data->recv_ios[num].skb = NULL;
		}
		kfree(mc_data->recv_ios);
		mc_data->recv_ios = (struct ud_recv_io *)NULL;
	}
	if (mc_data->mr) {
		ib_dereg_mr(mc_data->mr);
		mc_data->mr = (struct ib_mr *)NULL;
	}
	DATA_FUNCTION("vnic_mc_data_cleanup done\n");

}

int mc_data_init(struct mc_data *mc_data, struct viport *viport,
	      struct data_config *config, struct ib_pd *pd)
{
	DATA_FUNCTION("mc_data_init()\n");

	mc_data->num_recvs = viport->data.config->num_recvs;

	INIT_LIST_HEAD(&mc_data->avail_recv_ios_list);
	spin_lock_init(&mc_data->recv_lock);

	mc_data->parent = viport;
	mc_data->config = config;

	mc_data->ib_conn.cm_id = NULL;
	mc_data->ib_conn.viport = viport;
	mc_data->ib_conn.ib_config = &config->ib_config;
	mc_data->ib_conn.state = IB_CONN_UNINITTED;
	mc_data->ib_conn.callback_thread = NULL;
	mc_data->ib_conn.callback_thread_end = 0;

	if (vnic_ib_mc_init(mc_data, viport, pd,
			      &config->ib_config)) {
		DATA_ERROR("vnic_ib_mc_init failed\n");
		goto failure;
	}
	mc_data->mr = ib_get_dma_mr(pd,
				 IB_ACCESS_LOCAL_WRITE |
				 IB_ACCESS_REMOTE_WRITE);
	if (IS_ERR(mc_data->mr)) {
		DATA_ERROR("failed to register memory for"
			   " mc_data connection\n");
		goto destroy_conn;
	}

	if (mc_data_alloc_buffers(mc_data))
		goto dereg_mr;

	mc_data_post_recvs(mc_data);
	if (vnic_ib_mc_mod_qp_to_rts(mc_data->ib_conn.qp))
		goto dereg_mr;

	return 0;

dereg_mr:
	ib_dereg_mr(mc_data->mr);
	mc_data->mr = (struct ib_mr *)NULL;
destroy_conn:
	vnic_completion_cleanup(&mc_data->ib_conn);
	ib_destroy_qp(mc_data->ib_conn.qp);
	mc_data->ib_conn.qp = (struct ib_qp *)ERR_PTR(-EINVAL);
	ib_destroy_cq(mc_data->ib_conn.cq);
	mc_data->ib_conn.cq = (struct ib_cq *)ERR_PTR(-EINVAL);
failure:
	return -1;
}

static void mc_data_post_recvs(struct mc_data *mc_data)
{
	unsigned long flags;
	int i = 0;
	DATA_FUNCTION("mc_data_post_recvs\n");
	spin_lock_irqsave(&mc_data->recv_lock, flags);
	while (!list_empty(&mc_data->avail_recv_ios_list)) {
		struct io *io = list_entry(mc_data->avail_recv_ios_list.next,
				struct io, list_ptrs);
		struct ud_recv_io *recv_io =
					container_of(io, struct ud_recv_io, io);
		list_del(&recv_io->io.list_ptrs);
		spin_unlock_irqrestore(&mc_data->recv_lock, flags);
		if (vnic_ib_mc_post_recv(mc_data, &recv_io->io)) {
			viport_failure(mc_data->parent);
			return;
		}
		spin_lock_irqsave(&mc_data->recv_lock, flags);
		i++;
	}
	DATA_INFO("mcdata posted %d %p\n", i, &mc_data->avail_recv_ios_list);
	spin_unlock_irqrestore(&mc_data->recv_lock, flags);
}

static void mc_data_recv_routine(struct io *io)
{
	struct sk_buff *skb;
	struct ib_grh *grh;
	struct viport_trailer *trailer;
	struct mc_data *mc_data;
	unsigned long flags;
	struct ud_recv_io *recv_io = container_of(io, struct ud_recv_io, io);
	union ib_gid_cpu sgid;

	DATA_FUNCTION("mc_data_recv_routine\n");
	skb = recv_io->skb;
	grh = (struct ib_grh *)skb->data;
	mc_data = &recv_io->io.viport->mc_data;

	ib_dma_unmap_single(recv_io->io.viport->config->ibdev,
			    recv_io->skb_data_dma, recv_io->skb->len,
			    DMA_FROM_DEVICE);

	/* first - check if we've got our own mc packet  */
	/* convert sgid from host to cpu form before comparing */
	bswap_ib_gid(&grh->sgid, &sgid);
	if (cpu_to_be64(sgid.global.interface_id) ==
		io->viport->config->path_info.path.sgid.global.interface_id) {
		DATA_ERROR("dropping - our mc packet\n");
		dev_kfree_skb(skb);
	} else {
		/* GRH is at head and trailer at end. Remove GRH from head.  */
		trailer = (struct viport_trailer *)
				(skb->data + recv_io->len -
				 sizeof(struct viport_trailer));
		skb_pull(skb, sizeof(struct ib_grh));
		if (trailer->connection_hash_and_valid & CHV_VALID) {
			mc_data_recv_to_skbuff(io->viport, skb, trailer);
			vnic_recv_packet(io->viport->vnic, io->viport->parent,
					skb);
			vnic_multicast_recv_pkt_stats(io->viport->vnic);
		} else {
			DATA_ERROR("dropping - no CHV_VALID in HashAndValid\n");
			dev_kfree_skb(skb);
		}
	}
	recv_io->skb = NULL;
	if (mc_data_alloc_skb(recv_io, mc_data->skb_len, 0))
		return;

	spin_lock_irqsave(&mc_data->recv_lock, flags);
	list_add_tail(&recv_io->io.list_ptrs, &mc_data->avail_recv_ios_list);
	spin_unlock_irqrestore(&mc_data->recv_lock, flags);
	mc_data_post_recvs(mc_data);
	return;
}

static void mc_data_recv_to_skbuff(struct viport *viport, struct sk_buff *skb,
				   struct viport_trailer *trailer)
{
	u8 rx_chksum_flags = trailer->rx_chksum_flags;

	/* drop alignment bytes at start */
	skb_pull(skb, trailer->data_alignment_offset);
	/* drop excess from end */
	skb_trim(skb, __be16_to_cpu(trailer->data_length));

	if ((rx_chksum_flags & RX_CHKSUM_FLAGS_LOOPBACK)
	    || ((rx_chksum_flags & RX_CHKSUM_FLAGS_IP_CHECKSUM_SUCCEEDED)
		&& ((rx_chksum_flags & RX_CHKSUM_FLAGS_TCP_CHECKSUM_SUCCEEDED)
		    || (rx_chksum_flags &
			RX_CHKSUM_FLAGS_UDP_CHECKSUM_SUCCEEDED))))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		skb->ip_summed = CHECKSUM_NONE;

	if ((trailer->pkt_flags & PF_VLAN_INSERT) &&
	    !(viport->features_supported & VNIC_FEAT_IGNORE_VLAN)) {
		u8 *rv;

		/* insert VLAN id between source & length */
		DATA_INFO("VLAN adjustment\n");
		rv = skb_push(skb, 4);
		memmove(rv, rv + 4, 12);
		*(__be16 *) (rv + 12) = __constant_cpu_to_be16(ETH_P_8021Q);
		if (trailer->pkt_flags & PF_PVID_OVERRIDDEN)
		/*
		 *  Indicates VLAN is 0 but we keep the protocol id.
		 */
			*(__be16 *) (rv + 14) = trailer->vlan &
					__constant_cpu_to_be16(0xF000);
		else
			*(__be16 *) (rv + 14) = trailer->vlan;
		DATA_INFO("vlan:%x\n", *(int *)(rv+14));
	}

    return;
}
