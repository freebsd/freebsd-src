/*
 * Copyright (c) 2009 - 2010 Intel Corporation.  All rights reserved.
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
#ifndef __NES_UD_H
#define __NES_UD_H

enum nes_ud_dev_priority {
	NES_UD_DEV_PRIO_HIGH,
	NES_UD_DEV_PRIO_LOW,
};

enum nes_ud_queue_type {
  NES_UD_RECV_QUEUE,
  NES_UD_SEND_QUEUE,
};

enum nes_ud_mcast_mode {
  NES_UD_MCAST_ALL_MODE,
  NES_UD_MCAST_PFT_MODE,
};


struct nes_ud_file {
	struct nes_vnic *nesvnic;
	u8 active;
	char ifrn_name[IFNAMSIZ];
	int nes_ud_nic_index;
	int qpn;
	enum nes_ud_dev_priority prio;
	enum nes_ud_mcast_mode mcast_mode;
	enum nes_ud_queue_type queue_type;
	void      *nic_vbase;
	dma_addr_t nic_pbase;
	int        nic_mem_size;
	void      *wq_vbase;
	dma_addr_t wq_pbase;
	int mss;
	struct delayed_work mcast_cleanup_work;
	int head;
	int tail;
	u32 rsc_idx;
	struct nes_qp *qp_ptr; /* it is association used for CQ replacement */
	u32 adapter_no;    /* assotiation to allocated adapter */
};

int nes_ud_init(void);
int nes_ud_exit(void);
struct nes_ud_file *nes_ud_create_wq(struct nes_vnic *nesvnic, int isrecv);
int nes_ud_destroy_wq(struct nes_ud_file *file);
u32 nes_ud_reg_mr(struct ib_umem *region, u64 length, u64 virt, u32 stag);
int nes_ud_dereg_mr(u32 stag);
int nes_ud_subscribe_mcast(struct nes_ud_file *file, union ib_gid *gid);
int nes_ud_unsubscribe_mcast(struct nes_ud_file *file, union ib_gid *gid);
int nes_ud_cq_replace(struct nes_vnic *nesvnic, struct nes_cq *cq);
int nes_ud_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		int attr_mask, struct ib_udata *udata);

#endif
