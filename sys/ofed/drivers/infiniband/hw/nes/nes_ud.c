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

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/idr.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/device.h>

#include <rdma/ib_umem.h>
#include <rdma/ib_user_verbs.h>

#include "nes.h"
#include "nes_ud.h"

#define NES_UD_BASE_XMIT_NIC_QPID  28
#define NES_UD_BASE_RECV_NIC_IDX   12
#define NES_UD_BASE_XMIT_NIC_IDX   8
#define NES_UD_MAX_NIC_CNT 8
#define NES_UD_CLEANUP_TIMEOUT (HZ)
#define NES_UD_MCAST_TBL_SZ 128
#define NES_UD_SINGLE_HDR_SZ 64
#define NES_UD_CQE_NUM NES_NIC_WQ_SIZE
#define NES_UD_SKSQ_WAIT_TIMEOUT	100000
#define NES_UD_MAX_REG_CNT 128
#define NES_UD_MAX_MCAST_PER_QP 40

#define NES_UD_MAX_ADAPTERS 4 /* number of supported interfaces for  RAW ETH */

#define NES_UD_MAX_REG_HASH_CNT 256 /* last byte of the STAG is hash key */

/*
 * the same multicast could be allocated up to 2 owners so there could be
 *  two differentmcast entries allocated for the same mcas address
 */
struct nes_ud_file;
struct nes_ud_mcast {
	u8 addr[3];
	u8 in_use;
	struct nes_ud_file *owner;
	u8 nic_mask;
};

struct nes_ud_mem_region {
	struct list_head list;
	dma_addr_t *addrs;
	u64 va;
	u64 length;
	u32 pg_cnt;
	u32 in_use;
	u32 stag; /* stag related this structure */
};

struct nic_queue_info {
	u32 qpn;
	u32 nic_index;
	u32 logical_port;
	enum nes_ud_dev_priority prio;
	enum nes_ud_queue_type queue_type;
	struct nes_ud_file *file;
	struct nes_ud_file file_body;
};

struct nes_ud_resources {
	int num_logport_confed;
	int num_allocated_nics;
	u8 logport_2_map;
	u8 logport_3_map;
	u32 original_6000;
	u32 original_60b8;
	struct nic_queue_info nics[NES_UD_MAX_NIC_CNT];
	struct mutex          mutex;
	struct nes_ud_mcast mcast[NES_UD_MCAST_TBL_SZ];
	u32 adapter_no;  /* the allocated adapter no */

	/* the unique ID of the NE020 adapter */
	/*- it is allocated once per HW */
	struct nes_adapter *pAdap;
};

/* memory hash list entry */
struct nes_ud_hash_mem {
    struct list_head   list;
    int                read_stats;
};



struct nes_ud_mem {
	/* hash list of registered STAGs */
	struct nes_ud_hash_mem mrs[NES_UD_MAX_REG_HASH_CNT];
	struct mutex          mutex;
};

/* the QP in format x.y.z where x is adapter no, */
/* y is ud file idx in adapter, z is a qp no */
static struct nes_ud_mem ud_mem;

struct nes_ud_send_wr {
    u32               wr_cnt;
    u32               qpn;
    u32               flags;
    u32               resv[1];
    struct ib_sge     sg_list[64];
};

struct nes_ud_recv_wr {
    u32               wr_cnt;
    u32               qpn;
    u32	              resv[2];
    struct ib_sge     sg_list[64];
};

static struct nes_ud_resources nes_ud_rsc[NES_UD_MAX_ADAPTERS];
static struct workqueue_struct *nes_ud_workqueue;

/*
 * locate_ud_adapter
 *
 * the function locates the UD adapter
*  on base of the adapter unique ID (structure nes_adapter)
 */
static inline
struct nes_ud_resources *locate_ud_adapter(struct nes_adapter *pAdapt)
{
	int i;
	struct nes_ud_resources *pRsc;

	for (i = 0; i < NES_UD_MAX_ADAPTERS; i++) {
		pRsc = &nes_ud_rsc[i];

		if (pRsc->pAdap == pAdapt)
			return pRsc;

	}
	return NULL;
}

/*
 * allocate_ud_adapter()
 *
 * function allocates a new adapter
 */
static inline
struct nes_ud_resources *allocate_ud_adapter(struct nes_adapter *pAdapt)
{
    int i;
    struct nes_ud_resources *pRsc;

    for (i = 0; i < NES_UD_MAX_ADAPTERS; i++) {
	pRsc = &nes_ud_rsc[i];
	if (pRsc->pAdap == NULL) {
		pRsc->pAdap = pAdapt;
		nes_debug(NES_DBG_UD, "new UD Adapter allocated %d"
		" for adapter %p no =%d\n", i, pAdapt, pRsc->adapter_no);
		return pRsc;
		}
	}
	nes_debug(NES_DBG_UD, "Unable to allocate adapter\n");
	return NULL;
}

static inline
struct nes_ud_file *allocate_nic_queue(struct nes_vnic *nesvnic,
					enum nes_ud_queue_type queue_type)
{
    struct nes_ud_file *file = NULL;
	int i = 0;
	u8 select_log_port = 0xf;
	struct nes_device *nesdev = nesvnic->nesdev;
	int log_port_2_alloced = 0;
	int log_port_3_alloced = 0;
	int ret = 0;
	struct nes_ud_resources *pRsc;

	/* the first thing that must be done is determine the adapter */
	/* number max the adapter could have up to 2 interfaces */
	if (nesvnic->nic_index != 0 && nesvnic->nic_index != 1) {
		nes_debug(NES_DBG_UD, "nic queue allocation failed"
		" nesvnic->nic_index = %d\n", nesvnic->nic_index);
		return NULL;
	}

	/* locate device on base of nesvnic */
	/* - when it is an unknown card a new one is allocated */
	pRsc = locate_ud_adapter(nesdev->nesadapter);
	if (pRsc == NULL)
		return NULL;

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active == 0)
			continue;
		if (pRsc->nics[i].logical_port == 2 &&
		    queue_type == pRsc->nics[i].queue_type)
				log_port_2_alloced++;
		if (pRsc->nics[i].logical_port == 3 &&
		    queue_type == pRsc->nics[i].queue_type)
				log_port_3_alloced++;
	}

	/* check dual/single card */
	if (pRsc->logport_2_map != pRsc->logport_3_map) {
		/* a dual port card */
		/* allocation is NIC2, NIC2, NIC3, NIC3 */
		/*- no RX packat replication supported */
		if (log_port_2_alloced < 2 &&
		    pRsc->logport_2_map == nesvnic->nic_index)
				select_log_port = 2;
		else if (log_port_3_alloced < 2 &&
			 pRsc->logport_3_map == nesvnic->nic_index)
				select_log_port = 3;
	} else {
		/* single port card */
		/* change allocation scheme to NIC2,NIC3,NIC2,NIC3 */
		switch (log_port_2_alloced + log_port_3_alloced) {
		case 0: /* no QPs allocated - use NIC2 */
			if (pRsc->logport_2_map == nesvnic->nic_index)
				select_log_port = 2;

			break;
		case 1: /* NIC2 or NIC3 allocated */
			if (log_port_2_alloced > 0) {
				/* if NIC2 allocated use NIC3 */
				if (pRsc->logport_3_map == nesvnic->nic_index)
					select_log_port = 3;

			} else {
				/* when NIC3 allocated use NIC2 */
				if (pRsc->logport_2_map == nesvnic->nic_index)
					select_log_port = 2;

			}
			break;

		case 2:
		/* NIC2 and NIC3 allocated or both ports on NIC3 - use NIC2 */
			if ((log_port_2_alloced == 1) ||
			    (log_port_3_alloced == 2)) {
				if (pRsc->logport_2_map == nesvnic->nic_index)
					select_log_port = 2;

			} else {
				/* both ports allocated on NIC2 - use NIC3 */
				if (pRsc->logport_3_map == nesvnic->nic_index)
					select_log_port = 3;

			}
				break;
		case 3:
			/* when both NIC2 allocated use NIC3 */
			if (log_port_2_alloced == 2) {
				if (pRsc->logport_3_map == nesvnic->nic_index)
					select_log_port = 3;

			} else {
				/* when both NIC3 alloced use NIC2 */
				if (pRsc->logport_2_map == nesvnic->nic_index)
					select_log_port = 2;
			}
			break;

		default:
			break;
		}
	}
	if (select_log_port == 0xf) {
		ret = -1;
		nes_debug(NES_DBG_UD, "%s(%d) logport allocation failed "
		"log_port_2_alloced=%d log_port_3_alloced=%d\n",
			__func__, __LINE__, log_port_2_alloced,
			log_port_3_alloced);
		goto out;
	}

	nes_debug(NES_DBG_UD, "%s(%d) log_port_2_alloced=%d "
		"log_port_3_alloced=%d select_log_port=%d\n",
		__func__, __LINE__, log_port_2_alloced,
		log_port_3_alloced, select_log_port);

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active == 1)
			continue;
		if (pRsc->nics[i].logical_port == select_log_port &&
		    queue_type == pRsc->nics[i].queue_type) {

			/* file is preallocated during initialization */
			file = pRsc->nics[i].file;
			memset(file, 0, sizeof(*file));

			file->nesvnic = nesvnic;
			file->queue_type = queue_type;

			file->prio = pRsc->nics[i].prio;
			file->qpn = pRsc->nics[i].qpn;
			file->nes_ud_nic_index = pRsc->nics[i].nic_index;
			file->rsc_idx = i;
			file->adapter_no = pRsc->adapter_no;
			goto out;
		}
	}

out:
	return file;
}

static inline int  del_rsc_list(struct nes_ud_file *file)
{
	int logport_2_cnt = 0;
	int logport_3_cnt = 0;
	struct nes_device *nesdev = file->nesvnic->nesdev;
	int i = 0;
	struct nes_ud_resources *pRsc;

	if (file == NULL) {
		nes_debug(NES_DBG_UD, "%s(%d) file is NULL\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (file->nesvnic == NULL) {
		nes_debug(NES_DBG_UD, "%s(%d) file->nesvnic is NULL\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (nesdev == NULL) {
		nes_debug(NES_DBG_UD, "%s(%d) nesdev is NULL\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	/* locate device on base of nesvnic */
	/*- when it is an unknown card a new one is allocated */
	pRsc = locate_ud_adapter(nesdev->nesadapter);
	if (pRsc == NULL) {
		nes_debug(NES_DBG_UD, "%s(%d) cannot locate an allocated "
			"adapter  is NULL\n", __func__, __LINE__);
		return -EFAULT;
	}
	if (pRsc->num_allocated_nics == 0)
		return 0;

	if (--pRsc->num_allocated_nics == 0) {
		nes_write_indexed(nesdev, 0x60b8, pRsc->original_60b8);
		nes_write_indexed(nesdev, 0x6000, pRsc->original_6000);
		pRsc->num_logport_confed = 0;
	}
	BUG_ON(file->rsc_idx >= NES_UD_MAX_NIC_CNT);

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active &&
		    pRsc->nics[i].logical_port == 2)
			logport_2_cnt++;
		if (pRsc->nics[i].file->active &&
		    pRsc->nics[i].logical_port == 3)
			logport_3_cnt++;
	}

	if (pRsc->num_logport_confed != 0x3 && logport_2_cnt == 0)
		pRsc->logport_2_map = 0xf;

	if (pRsc->num_logport_confed != 0x3 && logport_3_cnt == 0)
		pRsc->logport_3_map = 0xf;
	return 0;
}

/*
* the QPN contains now the number of the RAW ETH
* adapter and QPN number on the adapter
* the adapter number is located in the highier
* 8 bits so QPN is stored as [adapter:qpn]
*/
static inline
struct nes_ud_file *get_file_by_qpn(struct nes_ud_resources *pRsc, int qpn)
{
	int i = 0;

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active &&
		    pRsc->nics[i].qpn == (qpn & 0xff))
			return pRsc->nics[i].file;

	}
	return NULL;
}

/* function counts all ETH RAW entities that have  */
/* a specific type and relation to specific vnic */
static inline
int count_files_by_nic(struct nes_vnic *nesvnic,
			enum nes_ud_queue_type queue_type)
{
	int count = 0;
	int i = 0;
	struct nes_ud_resources *pRsc;

	pRsc = locate_ud_adapter(nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return 0;

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active &&
		    pRsc->nics[i].file->nesvnic == nesvnic &&
		    pRsc->nics[i].queue_type == queue_type)
				count++;
	}
	return count;
}

/* function counts all RAW ETH  entities the have a specific type */
static inline
int count_files(struct nes_vnic *nesvnic, enum nes_ud_queue_type queue_type)
{
	int count = 0;
	int i = 0;
	struct nes_ud_resources *pRsc;

	pRsc = locate_ud_adapter(nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return 0;

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active &&
		    pRsc->nics[i].queue_type == queue_type)
			count++;
	}
	return count;
}

/*
 * the function locates the entry allocated by IGMP and modifies the
 * PFT entry with the list of the NICs allowed to receive that multicast
 * the NIC0/NIC1 are removed due to performance issue so tcpdum
 * like tools cannot receive the accelerated multicasts
 */
static void mcast_fix_filter_table_single(struct nes_ud_file *file, u8 *addr)
{
  struct nes_device *nesdev = file->nesvnic->nesdev;
  int i = 0;
  u32 macaddr_low;
  u32 orig_low;
  u32 macaddr_high;
  u32 prev_high;

  for (i = 0; i < 48; i++) {
	macaddr_low = nes_read_indexed(nesdev,
					NES_IDX_PERFECT_FILTER_LOW + i*8);
	orig_low = macaddr_low;
	macaddr_high = nes_read_indexed(nesdev,
					NES_IDX_PERFECT_FILTER_LOW + 4 + i*8);
	if (!(macaddr_high & NES_MAC_ADDR_VALID))
		continue;
	if ((macaddr_high & 0xffff) != 0x0100)
		continue;
	if ((macaddr_low & 0xff) != addr[2])
		continue;
	macaddr_low >>= 8;
	if ((macaddr_low & 0xff) != addr[1])
		continue;
	macaddr_low >>= 8;
	if ((macaddr_low & 0xff) != addr[0])
		continue;
	macaddr_low >>= 8;
	if ((macaddr_low & 0xff) != 0x5e)
		continue;
	/* hit - that means Linux or other UD set this bit earlier  */
	prev_high = macaddr_high;
	nes_write_indexed(nesdev, NES_IDX_PERFECT_FILTER_LOW + 4 + i*8, 0);
	macaddr_high = (macaddr_high & 0xfffcffff) |
					((1<<file->nes_ud_nic_index) << 16);

	nes_debug(NES_DBG_UD, "%s(%d) found addr to fix, "
		 "i=%d, macaddr_high=0x%x  macaddr_low=0x%x "
		 "nic_idx=%d prev_high=0x%x\n",
		 __func__, __LINE__, i, macaddr_high, orig_low,
		file->nes_ud_nic_index, prev_high);
	nes_write_indexed(nesdev,
			NES_IDX_PERFECT_FILTER_LOW + 4 + i*8, macaddr_high);
	break;
  }
}

/* this function is implemented that way because the Linux multicast API
   use the multicast list approach. When a new multicast address is added
   all PFT table is reinitialized by linux and all entries must be fixed
   by this procedure
*/
static void mcast_fix_filter_table(struct nes_ud_file *file)
{
	int i;
	struct nes_ud_resources *pRsc;

	pRsc = locate_ud_adapter(file->nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return;

	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if (pRsc->mcast[i].in_use != 0)
			mcast_fix_filter_table_single(pRsc->mcast[i].owner,
							pRsc->mcast[i].addr);
	}
}

/* function invalidates the PFT entry */
static void remove_mcast_from_pft(struct nes_ud_file *file, u8 *addr)
{
  struct nes_device *nesdev = file->nesvnic->nesdev;
  int i = 0;
   u32 macaddr_low;
  u32 orig_low;
  u32 macaddr_high;
  u32 prev_high;

  for (i = 0; i < 48; i++) {
	macaddr_low = nes_read_indexed(nesdev,
					NES_IDX_PERFECT_FILTER_LOW + i*8);
	orig_low = macaddr_low;
	macaddr_high = nes_read_indexed(nesdev,
					NES_IDX_PERFECT_FILTER_LOW + 4 + i*8);
	if (!(macaddr_high & NES_MAC_ADDR_VALID))
		continue;

	if ((macaddr_high & 0xffff) != 0x0100)
		continue;
	if ((macaddr_low & 0xff) != addr[2])
		continue;
	macaddr_low >>= 8;
	if ((macaddr_low & 0xff) != addr[1])
		continue;
	macaddr_low >>= 8;
	if ((macaddr_low & 0xff) != addr[0])
		continue;
	macaddr_low >>= 8;
	if ((macaddr_low & 0xff) != 0x5e)
		continue;
	/* hit - that means Linux or other UD set this bit earlier */
	/* so remove the NIC from MAC address reception */
	prev_high = macaddr_high;
	macaddr_high = (macaddr_high & 0xfffcffff) &
					~((1<<file->nes_ud_nic_index) << 16);
	nes_debug(NES_DBG_UD, "%s(%d) found addr to mcast remove,"
		"i=%d, macaddr_high=0x%x  macaddr_low=0x%x "
		"nic_idx=%d prev_high=0x%x\n", __func__, __LINE__, i,
		macaddr_high, orig_low, file->nes_ud_nic_index, prev_high);
	nes_write_indexed(nesdev, NES_IDX_PERFECT_FILTER_LOW + 4 + i*8,
							macaddr_high);
	break;
	}

}

/*
* the function returns a mask of the NICs
* assotiated with given multicast address
*/
static int nes_ud_mcast_filter(struct nes_vnic *nesvnic, __u8 *dmi_addr)
{
	int i = 0;
	int ret = 0;
	int mask = 0;
	struct nes_ud_resources *pRsc;

	pRsc = locate_ud_adapter(nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return 0;

	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if (pRsc->mcast[i].in_use &&
		    pRsc->mcast[i].addr[0] == dmi_addr[3] &&
		    pRsc->mcast[i].addr[1] == dmi_addr[4] &&
		    pRsc->mcast[i].addr[2] == dmi_addr[5]) {
			mask = (pRsc->mcast[i].owner->mcast_mode ==
				NES_UD_MCAST_PFT_MODE) ?
				pRsc->mcast[i].owner->nes_ud_nic_index : 0;

			ret = ret | (1 << mask);
			nes_debug(NES_DBG_UD, "mcast filter, "
				"fpr=%02X%02X%02X ret=%d\n",
				dmi_addr[3], dmi_addr[4], dmi_addr[5], ret);
		}
	}
	if (ret == 0)
		return -1;
	else
		return ret;

}

static __u32 mqueue_key[4] = { 0x0, 0x80, 0x0, 0x0 };

static inline __u8 nes_ud_calculate_hash(__u8 dest_addr_lsb)
{
  __u8 in[8];
  __u32 key_arr[4];
  int i;
  __u32 result = 0;
  int j, k;
  __u8 shift_in, next_shift_in;

  in[0] = 0;
  in[1] = 0;
  in[2] = 0;
  in[3] = 0;

  in[4] = 0;

  in[5] = 0;
  in[6] = 0;
  in[7] = dest_addr_lsb;



	for (i = 0; i < 4; i++)
		key_arr[3-i] = mqueue_key[i];



	for (i = 0; i < 8; i++) {
		for (j = 7; j >= 0; j--) {
			if (in[i] & (1 << j))
				result = result ^ key_arr[0];

			shift_in = 0;
			for (k = 3; k >= 0; k--) {
				next_shift_in = key_arr[k] >> 31;
				key_arr[k] = (key_arr[k] << 1) + shift_in;
				shift_in = next_shift_in;
			}
		}
	}
	return result & 0x7f;
}

static inline void nes_ud_enable_mqueue(struct nes_ud_file *file)
{
	struct nes_device *nesdev = file->nesvnic->nesdev;
	int mqueue_config0;
	int mqueue_config2;
	int instance = file->nes_ud_nic_index & 0x1;

	mqueue_config0 = nes_read_indexed(nesdev, 0x6400);
	mqueue_config0 |= (4 | (instance & 0x3)) << (file->nes_ud_nic_index*3);
	nes_write_indexed(nesdev, 0x6400, mqueue_config0);
	mqueue_config0 = nes_read_indexed(nesdev, 0x6400);

	mqueue_config2 = nes_read_indexed(nesdev, 0x6408);
	mqueue_config2 |= (2 << (instance*2)) | (6 << (instance*3+8));
	nes_write_indexed(nesdev, 0x6408, mqueue_config2);
	mqueue_config2 = nes_read_indexed(nesdev, 0x6408);

	nes_write_indexed(nesdev, 0x64a0+instance*0x100, mqueue_key[0]);
	nes_write_indexed(nesdev, 0x64a4+instance*0x100, mqueue_key[1]);
	nes_write_indexed(nesdev, 0x64a8+instance*0x100, mqueue_key[2]);
	nes_write_indexed(nesdev, 0x64ac+instance*0x100, mqueue_key[3]);

	nes_debug(NES_DBG_UD, "mq_config0=0x%x mq_config2=0x%x nic_idx= %d\n",
		  mqueue_config0, mqueue_config2, file->nes_ud_nic_index);

}



static inline
void nes_ud_redirect_from_mqueue(struct nes_ud_file *file, int num_queues)
{
	struct nes_device *nesdev = file->nesvnic->nesdev;
	int instance = file->nes_ud_nic_index & 0x1;
	unsigned addr = 0x6420+instance*0x100;
	unsigned value;
	int i;

	value  = (file->prio == NES_UD_DEV_PRIO_LOW || num_queues == 1) ?
							0x0 : 0x11111111;
	for (i = 0; i < 16; i++)
		nes_write_indexed(nesdev, addr+i*4, value);
}


static int nes_ud_create_nic(struct nes_ud_file *file)
{
	struct nes_vnic *nesvnic = file->nesvnic;
	struct nes_device *nesdev = nesvnic->nesdev;
	struct nes_hw_nic_qp_context *nic_context;
	struct nes_hw_cqp_wqe *cqp_wqe;
	struct nes_cqp_request *cqp_request;
	unsigned long flags;
	void *vmem;
	dma_addr_t pmem;
	u64 u64temp;
	int ret = 0;

	BUG_ON(file->nic_vbase != NULL);

	file->nic_mem_size = 256 +
			(NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_sq_wqe)) +
			sizeof(struct nes_hw_nic_qp_context);

	file->nic_vbase = pci_alloc_consistent(nesdev->pcidev,
						file->nic_mem_size,
						&file->nic_pbase);
	if (!file->nic_vbase) {
		nes_debug(NES_DBG_UD, "Unable to allocate memory for NIC host "
			"descriptor rings\n");
		return -ENOMEM;
	}

	memset(file->nic_vbase, 0, file->nic_mem_size);

	vmem = (void *)(((unsigned long long)file->nic_vbase + (256 - 1)) &
			~(unsigned long long)(256 - 1));
	pmem = (dma_addr_t)(((unsigned long long)file->nic_pbase + (256 - 1)) &
			~(unsigned long long)(256 - 1));

	file->wq_vbase = vmem;
	file->wq_pbase = pmem;
	file->head = 0;
	file->tail = 0;

	vmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_sq_wqe));
	pmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_sq_wqe));

	cqp_request = nesvnic->get_cqp_request(nesdev);
	if (cqp_request == NULL) {
		nes_debug(NES_DBG_QP, "Failed to get a cqp_request.\n");
		goto fail_cqp_req_alloc;
	}
	cqp_request->waiting = 1;
	cqp_wqe = &cqp_request->cqp_wqe;

	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] =
			cpu_to_le32(NES_CQP_CREATE_QP | NES_CQP_QP_TYPE_NIC);
	cqp_wqe->wqe_words[NES_CQP_WQE_ID_IDX] = cpu_to_le32(file->qpn);
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_CTX_LOW_IDX] =
			cpu_to_le32((u32)((u64)(&nesdev->cqp)));
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_CTX_HIGH_IDX] =
			cpu_to_le32((u32)(((u64)(&nesdev->cqp))>>32));
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_SCRATCH_LOW_IDX] = 0;
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_SCRATCH_HIGH_IDX] = 0;


	nic_context = vmem;

	nic_context->context_words[NES_NIC_CTX_MISC_IDX] =
			cpu_to_le32((u32)NES_NIC_CTX_SIZE |
			((u32)PCI_FUNC(nesdev->pcidev->devfn) << 12) |
			(1 << 18));

	nic_context->context_words[NES_NIC_CTX_SQ_LOW_IDX] = 0;
	nic_context->context_words[NES_NIC_CTX_SQ_HIGH_IDX] = 0;
	nic_context->context_words[NES_NIC_CTX_RQ_LOW_IDX] = 0;
	nic_context->context_words[NES_NIC_CTX_RQ_HIGH_IDX] = 0;

	u64temp = (u64)file->wq_pbase;
	if (file->queue_type == NES_UD_SEND_QUEUE) {
		nic_context->context_words[NES_NIC_CTX_SQ_LOW_IDX] =
						cpu_to_le32((u32)u64temp);
		nic_context->context_words[NES_NIC_CTX_SQ_HIGH_IDX] =
					cpu_to_le32((u32)(u64temp >> 32));
	} else {
		nic_context->context_words[NES_NIC_CTX_RQ_LOW_IDX] =
						cpu_to_le32((u32)u64temp);
		nic_context->context_words[NES_NIC_CTX_RQ_HIGH_IDX] =
					cpu_to_le32((u32)(u64temp >> 32));
	}

	u64temp = (u64)pmem;

	cqp_wqe->wqe_words[NES_CQP_QP_WQE_CONTEXT_LOW_IDX] =
						cpu_to_le32((u32)u64temp);
	cqp_wqe->wqe_words[NES_CQP_QP_WQE_CONTEXT_HIGH_IDX] =
					cpu_to_le32((u32)(u64temp >> 32));

	atomic_set(&cqp_request->refcount, 2);
	nesvnic->post_cqp_request(nesdev, cqp_request);

	/* Wait for CQP */
	wait_event_timeout(cqp_request->waitq,
				(cqp_request->request_done != 0),
				NES_EVENT_TIMEOUT);


	if (atomic_dec_and_test(&cqp_request->refcount)) {
		if (cqp_request->dynamic) {
			kfree(cqp_request);
		} else {
			spin_lock_irqsave(&nesdev->cqp.lock, flags);
			list_add_tail(&cqp_request->list,
						&nesdev->cqp_avail_reqs);
			spin_unlock_irqrestore(&nesdev->cqp.lock, flags);
		}
	}
	nes_debug(NES_DBG_UD, "Created NIC, qpn=%d, SQ/RQ pa=0x%p va=%p "
		"virt_to_phys=%p\n", file->qpn,
		(void *)file->wq_pbase, (void *)file->nic_vbase,
		(void *)virt_to_phys(file->nic_vbase));
	return ret;

 fail_cqp_req_alloc:
	pci_free_consistent(nesdev->pcidev, file->nic_mem_size, file->nic_vbase,
			file->nic_pbase);
	file->nic_vbase = NULL;
	return -EFAULT;
}


static void nes_ud_destroy_nic(struct nes_ud_file *file)
{
	struct nes_vnic *nesvnic = file->nesvnic;
	struct nes_device *nesdev = nesvnic->nesdev;
	struct nes_hw_cqp_wqe *cqp_wqe;
	struct nes_cqp_request *cqp_request;
	unsigned long flags;
	int ret = 0;

	cqp_request = nesvnic->get_cqp_request(nesdev);
	if (cqp_request == NULL) {
		nes_debug(NES_DBG_QP, "Failed to get a cqp_request.\n");
		return;
	}
	cqp_request->waiting = 1;
	cqp_wqe = &cqp_request->cqp_wqe;

	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] =
			cpu_to_le32(NES_CQP_DESTROY_QP | NES_CQP_QP_TYPE_NIC);
	cqp_wqe->wqe_words[NES_CQP_WQE_ID_IDX] = cpu_to_le32(file->qpn);
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_CTX_LOW_IDX] =
			cpu_to_le32((u32)((u64)(&nesdev->cqp)));
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_CTX_HIGH_IDX] =
			cpu_to_le32((u32)(((u64)(&nesdev->cqp)) >> 32));
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_SCRATCH_LOW_IDX] = 0;
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_SCRATCH_HIGH_IDX] = 0;

	atomic_set(&cqp_request->refcount, 2);
	nesvnic->post_cqp_request(nesdev, cqp_request);

	/* Wait for CQP */
	ret = wait_event_timeout(cqp_request->waitq,
			(cqp_request->request_done != 0),
			NES_EVENT_TIMEOUT);
	if (!ret)
		nes_debug(NES_DBG_UD, "NES_UD NIC QP%u "
			"destroy timeout expired\n", file->qpn);

	if (atomic_dec_and_test(&cqp_request->refcount)) {
		if (cqp_request->dynamic) {
			kfree(cqp_request);
		} else {
			spin_lock_irqsave(&nesdev->cqp.lock, flags);
			list_add_tail(&cqp_request->list,
					&nesdev->cqp_avail_reqs);
			spin_unlock_irqrestore(&nesdev->cqp.lock, flags);
		}
	}

	pci_free_consistent(nesdev->pcidev, file->nic_mem_size, file->nic_vbase,
			file->nic_pbase);
	file->nic_vbase = NULL;
	file->qp_ptr = NULL;

	return;
}

int nes_ud_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		int attr_mask, struct ib_udata *udata)
{
	struct nes_qp *nesqp = to_nesqp(ibqp);
	int ret = 0;

	if (attr_mask & IB_QP_STATE) {
		switch (attr->qp_state) {
		case IB_QPS_ERR:
			if (nesqp->rx_ud_wq)
				nes_ud_destroy_nic(nesqp->rx_ud_wq);

			if (nesqp->tx_ud_wq && (ret == 0))
				nes_ud_destroy_nic(nesqp->tx_ud_wq);

			nesqp->ibqp_state = IB_QPS_ERR;
			break;

		default:
			break;
		}
	}
	return ret;
}

static void nes_ud_free_resources(struct nes_ud_file *file)
{
	struct nes_device *nesdev = file->nesvnic->nesdev;
	int nic_active = 0;
	int mcast_all = 0;
	int mcast_en = 0;
	int wqm_config0 = 0;
	wait_queue_head_t     waitq;
	int num_queues  = 0;
	nes_debug(NES_DBG_UD, " %s(%d) NAME=%s nes_ud_qpid=%d\n",
		__func__, __LINE__, file->ifrn_name, file->qpn);

	if (!file->nesvnic || !file->active)
		return;

	if (file->queue_type == NES_UD_SEND_QUEUE) {
		nic_active = nes_read_indexed(nesdev, NES_IDX_NIC_ACTIVE);
		nic_active &= ~(1 << file->nes_ud_nic_index);
		nes_write_indexed(nesdev, NES_IDX_NIC_ACTIVE, nic_active);
		nic_active = nes_read_indexed(nesdev, NES_IDX_NIC_ACTIVE);
	} else {
		num_queues = count_files_by_nic(file->nesvnic,
						file->queue_type);

	if (num_queues == 1) {

		nic_active = nes_read_indexed(nesdev, NES_IDX_NIC_ACTIVE);
		nic_active &= ~(1 << file->nes_ud_nic_index);
		nes_write_indexed(nesdev, NES_IDX_NIC_ACTIVE, nic_active);
		nic_active = nes_read_indexed(nesdev, NES_IDX_NIC_ACTIVE);

		mcast_all = nes_read_indexed(nesdev, NES_IDX_NIC_MULTICAST_ALL);
		mcast_all &= ~(1 << file->nes_ud_nic_index);
		nes_write_indexed(nesdev, NES_IDX_NIC_MULTICAST_ALL, mcast_all);
		mcast_all = nes_read_indexed(nesdev, NES_IDX_NIC_MULTICAST_ALL);

		mcast_en = nes_read_indexed(nesdev,
						NES_IDX_NIC_MULTICAST_ENABLE);
		mcast_en &= ~(1 << file->nes_ud_nic_index);
		nes_write_indexed(nesdev, NES_IDX_NIC_MULTICAST_ENABLE,
								mcast_en);
		mcast_en = nes_read_indexed(nesdev,
						NES_IDX_NIC_MULTICAST_ENABLE);

		nes_debug(NES_DBG_UD, "nic_active=0x%x, mcast_en=0x%x, "
			"mcast_all=0x%x nic_index=%d num_queues=%d\n",
			nic_active, mcast_en, mcast_all,
			file->nes_ud_nic_index, num_queues);
	  }

	 nes_ud_redirect_from_mqueue(file, num_queues);
	 num_queues = count_files(file->nesvnic, file->queue_type);
	if (num_queues == 1) {
		nes_debug(NES_DBG_UD, "Last receive queue, "
			"restoring MPP debug register\n");
		nes_write_indexed(nesdev, 0xA00, 0x200);
		nes_write_indexed(nesdev, 0xA40, 0x200);
	  }
	}

	if (file->qp_ptr)
		if (file->qp_ptr->ibqp_state != IB_QPS_ERR)
			nes_ud_destroy_nic(file);

	if (file->queue_type == NES_UD_RECV_QUEUE) {
		wqm_config0 = nes_read_indexed(nesdev, 0x5000);
		wqm_config0 &= ~0x8000;
		nes_write_indexed(nesdev, 0x5000, wqm_config0);

		init_waitqueue_head(&waitq);

		wait_event_timeout(waitq, 0, NES_UD_CLEANUP_TIMEOUT);

		nes_debug(NES_DBG_UD, "%s(%d) enabling stall_no_wqes\n",
			__func__, __LINE__);
		wqm_config0 = nes_read_indexed(nesdev, 0x5000);
		wqm_config0 |= 0x8000;
		nes_write_indexed(nesdev, 0x5000, wqm_config0);
	}

	dev_put(file->nesvnic->netdev);

	file->active = 0;

	nes_debug(NES_DBG_UD, "%s(%d) done\n", __func__, __LINE__);
}


static int nes_ud_init_channel(struct nes_ud_file *file)
{
	struct nes_device *nesdev = NULL;
	int ret = 0;
	int nic_active = 0;
	int mcast_all = 0;
	int mcast_en = 0;
	int link_ag = 0;
	int mpp4_dbg = 0;

	nesdev = file->nesvnic->nesdev;

	ret = nes_ud_create_nic(file);
	if (ret != 0)
		return ret;

	if (file->queue_type == NES_UD_RECV_QUEUE) {

		file->nesvnic->mcrq_mcast_filter = nes_ud_mcast_filter;

		mcast_en = nes_read_indexed(nesdev,
					NES_IDX_NIC_MULTICAST_ENABLE);
		mcast_en |= 1 << file->nes_ud_nic_index;
		nes_write_indexed(nesdev, NES_IDX_NIC_MULTICAST_ENABLE,
								mcast_en);
		mcast_en = nes_read_indexed(nesdev,
					NES_IDX_NIC_MULTICAST_ENABLE);

		/* the only case when we use PFT is for single port
		two functions, which probably would be the
		most common usage model :), but anyway */
	if (file->mcast_mode == NES_UD_MCAST_ALL_MODE) {
		mcast_all = nes_read_indexed(nesdev, NES_IDX_NIC_MULTICAST_ALL);
		mcast_all |= 1 << file->nes_ud_nic_index;
		nes_write_indexed(nesdev, NES_IDX_NIC_MULTICAST_ALL, mcast_all);
		mcast_all = nes_read_indexed(nesdev, NES_IDX_NIC_MULTICAST_ALL);
	}
	if (nesdev->nesadapter->port_count <= 2) {
		link_ag = 0x00;
		nes_write_indexed(nesdev, 0x6038, link_ag);
		link_ag = nes_read_indexed(nesdev, 0x6038);
	}
	if (nesdev->nesadapter->netdev_count <= 2)
		nes_ud_enable_mqueue(file);

	nes_write_indexed(nesdev, 0xA00, 0x245);
	nes_write_indexed(nesdev, 0xA40, 0x245);

	}
	/* NES_UD_SEND_QUEUE */
	else {
		mpp4_dbg = nes_read_indexed(nesdev, 0xb00);
		mpp4_dbg |= 1 << 12;
		nes_write_indexed(nesdev, 0xb00, mpp4_dbg);
		mpp4_dbg = nes_read_indexed(nesdev, 0xb00);
	}

	nic_active = nes_read_indexed(nesdev, NES_IDX_NIC_ACTIVE);
	nic_active |= 1 << file->nes_ud_nic_index;
	nes_write_indexed(nesdev, NES_IDX_NIC_ACTIVE, nic_active);
	nic_active = nes_read_indexed(nesdev, NES_IDX_NIC_ACTIVE);

	nes_debug(NES_DBG_UD, "nic_active=0x%x, mcast_en=0x%x, "
		"mcast_all=0x%x nic_index=%d link_ag=0x%x mpp4_dbg=0x%x\n",
		nic_active, mcast_en, mcast_all, file->nes_ud_nic_index,
		link_ag, mpp4_dbg);

	return ret;
}

static struct nes_ud_file *nes_ud_get_nxt_channel(struct nes_vnic *nesvnic,
					enum nes_ud_queue_type queue_type)
{
	struct nes_ud_file *file = NULL;
	struct net_device *netdev = NULL;
	struct nes_device *nesdev = NULL;
	struct nes_ud_resources *pRsc;

	netdev = nesvnic->netdev;
	nesdev = nesvnic->nesdev;

	pRsc = locate_ud_adapter(nesdev->nesadapter);
	if (pRsc == NULL) {
		pRsc = allocate_ud_adapter(nesdev->nesadapter);
		if (pRsc == NULL)
			return NULL;

	}
	if (pRsc->num_logport_confed == 0) {
		pRsc->original_60b8 = nes_read_indexed(nesdev, 0x60b8);
		pRsc->original_6000 = nes_read_indexed(nesdev, 0x6000);
		/* everything goes to port 0x0 */
		if ((nesvnic->nesdev->nesadapter->port_count == 1) ||
		    (nes_drv_opt & NES_DRV_OPT_MCAST_LOGPORT_MAP)) {
			/* single port card or dual port using single if */
			pRsc->num_logport_confed = 0x3;
			pRsc->logport_2_map = 0x0;
			pRsc->logport_3_map = 0x0;
			nes_write_indexed(nesdev, 0x60b8, 0x3);
			nes_write_indexed(nesdev, 0x6000, 0x0);
		} else {
			pRsc->num_logport_confed = 0x3;
			pRsc->logport_2_map = 0x0;
			pRsc->logport_3_map = 0x1;
		}
		nes_debug(NES_DBG_UD, "%s(%d) num_logport_confed=%d "
			"original_6000=%d logport_3_map = %d nes_drv_opt=%x\n",
			__func__, __LINE__, pRsc->num_logport_confed,
			pRsc->original_6000, pRsc->logport_3_map, nes_drv_opt);
	}

	nes_debug(NES_DBG_UD, "%s(%d) logport_2_map=%d logport_3_map=%d\n",
		 __func__, __LINE__, pRsc->logport_2_map, pRsc->logport_3_map);

	file = allocate_nic_queue(nesvnic, queue_type);
	if (file == NULL) {
		nes_debug(NES_DBG_UD, "%s(%d) failed to allocate NIC\n",
			__func__, __LINE__);
		return NULL;
	}

	file->active = 1;
	memcpy(file->ifrn_name, netdev->name, IFNAMSIZ);

	/* for now use pft always */
	file->mcast_mode = NES_UD_MCAST_PFT_MODE;

	nes_debug(NES_DBG_UD, " %s(%d) NAME=%s qpn=%d nes_ud_nic_index=%d "
		"nes_ud_nic.qp_id=%d mcast_mode=%d port_count=%d "
		"netdev_count=%d\n", __func__, __LINE__, file->ifrn_name,
		 file->qpn, file->nes_ud_nic_index, file->nesvnic->mcrq_qp_id,
		 file->mcast_mode, nesdev->nesadapter->port_count,
		nesdev->nesadapter->netdev_count);

	file->mss = netdev->mtu-28;
	pRsc->num_allocated_nics++;
	BUG_ON(pRsc->num_allocated_nics > 8);

	return file;

}

static struct nes_ud_mem_region *nes_ud_allocate_mr(u32 npages)
{
	struct nes_ud_mem_region *mr = NULL;

	mr = vmalloc(sizeof(*mr));
	if (mr == NULL)
		return NULL;


	mr->addrs = vmalloc(npages * sizeof(dma_addr_t));
	if (!mr->addrs) {
		nes_debug(NES_DBG_UD, "%s(%d) Cannot allocate mr struct "
		"for %d pages\n", __func__, __LINE__, npages);
		vfree(mr);
		return NULL;
	}
	mr->pg_cnt = npages;
	mr->in_use = 1;

	INIT_LIST_HEAD(&mr->list);

	return mr;
}

static void nes_ud_free_mr(struct nes_ud_mem_region *mr)
{
	if (mr->addrs != NULL)
		vfree(mr->addrs);

	vfree(mr);
}

/* nes_ud_get_hash_entry()
 *
 * function returns a key for hash table
 */
static inline
int nes_ud_get_hash_entry(u32 stag)
{
	return stag & 0xff;
}


/* nes_ud_lookup_mr()
 *
 * function returns a pointer to mr realized by specific STAG
 */
static inline
struct nes_ud_mem_region *nes_ud_lookup_mr(u32 stag)
{
	int key;
	struct nes_ud_mem_region *mr;

	key = nes_ud_get_hash_entry(stag);

	mutex_lock(&ud_mem.mutex);
	list_for_each_entry(mr, &ud_mem.mrs[key].list, list) {
		ud_mem.mrs[key].read_stats++;
		if (mr->stag == stag) {
			mutex_unlock(&ud_mem.mutex);
			return mr;
		}
	}
	mutex_unlock(&ud_mem.mutex);
	return NULL;
}

/* nes_ud_add_mr_hash()
 *
 * the function inserts the mr entry into the hash list
 * the stag is a key
 */
static inline
int nes_ud_add_mr_hash(struct nes_ud_mem_region *mr)
{
	int key;

	/* first check if the stag is unique */
	if (nes_ud_lookup_mr(mr->stag) != NULL) {
		nes_debug(NES_DBG_UD, "%s(%d) double STAG error stag=%x\n",
			__func__, __LINE__, mr->stag);
		return -1;
	}
	key = nes_ud_get_hash_entry(mr->stag);

	/* structure is global so mutexes are necessary */
	mutex_lock(&ud_mem.mutex);

	/* add mr to the list at start  */
	list_add(&mr->list, &ud_mem.mrs[key].list);

	mutex_unlock(&ud_mem.mutex);

	return 0;

}

/* nes_ud_del_mr()
 *
 * the function removes the entry from the hash list
 * the stag is the key
 */
static inline
void nes_ud_del_mr(struct nes_ud_mem_region *mr)
{
	/* structure is global so mutexes are necessary */
	mutex_lock(&ud_mem.mutex);

	list_del(&mr->list);

	/* init entry */
	INIT_LIST_HEAD(&mr->list);

	mutex_unlock(&ud_mem.mutex);
}

/* nes_ud_cleanup_mr()
 *
 * function deletes and and frees all hash entries
 */
static inline
void nes_ud_cleanup_mr(void)
{
	struct nes_ud_mem_region *mr;
	struct nes_ud_mem_region *next;
	int i;

	/* structure is global so mutexes are necessary */
	mutex_lock(&ud_mem.mutex);

	for (i = 0; i < NES_UD_MAX_REG_HASH_CNT; i++) {
		if (list_empty(&ud_mem.mrs[i].list))
			continue;

		list_for_each_entry_safe(mr, next, &ud_mem.mrs[i].list, list) {
			nes_debug(NES_DBG_UD, "%s(%d) non free stag=%x\n",
				__func__, __LINE__, mr->stag);
			list_del_init(&mr->list);

			nes_ud_free_mr(mr);
		}
	}

	mutex_unlock(&ud_mem.mutex);
}

u32 nes_ud_reg_mr(struct ib_umem *region, u64 length, u64 virt, u32 stag)
{
	unsigned long npages =
		PAGE_ALIGN(region->length + region->offset) >> PAGE_SHIFT;
	struct nes_ud_mem_region *mr = nes_ud_allocate_mr(npages);
	struct ib_umem_chunk *chunk;
	dma_addr_t page;
	u32 chunk_pages = 0;
	int nmap_index;
	int i = 0;
	int mr_id = 0;
	nes_debug(NES_DBG_UD, "%s(%d) mr=%p length=%d virt=%p\n",
		__func__, __LINE__, mr, (int)length, (void *)virt);
	if (!mr)
		return 0;


	mr->stag = stag;

	mr->va = virt;
	mr->length = length;
	list_for_each_entry(chunk, &region->chunk_list, list) {
	for (nmap_index = 0; nmap_index < chunk->nmap; ++nmap_index) {
		page = sg_dma_address(&chunk->page_list[nmap_index]);
		chunk_pages = sg_dma_len(&chunk->page_list[nmap_index]) >> 12;
		if (page & ~PAGE_MASK)
			goto reg_user_mr_err;
		if (!chunk_pages)
			goto reg_user_mr_err;

		for (i = 0; i < chunk_pages; i++) {
			mr->addrs[mr_id] = page;
			page += PAGE_SIZE;
			if (++mr_id > npages)
				goto reg_user_mr_err;
			}
		}
	}
	nes_debug(NES_DBG_UD, "%s(%d) stag=0x%x mr_id=%d npages=%d\n",
		__func__, __LINE__, stag, mr_id, (int)npages);
	nes_ud_add_mr_hash(mr);
	return stag;

reg_user_mr_err:
	if (mr)
		nes_ud_free_mr(mr);

	return 0;
}


int nes_ud_dereg_mr(u32 stag)
{
	struct nes_ud_mem_region *mr = NULL;

	nes_debug(NES_DBG_UD, "%s(%d) stag=0x%x\n", __func__, __LINE__, stag);

	mr = nes_ud_lookup_mr(stag);
	if (mr != NULL) {
		nes_ud_del_mr(mr);
		nes_ud_free_mr(mr);
	} else {
		nes_debug(NES_DBG_UD, "%s(%d) unknown stag=0x%x\n",
		__func__, __LINE__, stag);
	}

	nes_debug(NES_DBG_UD, "%s(%d) done\n", __func__, __LINE__);
	return 0;
}


int nes_ud_unsubscribe_mcast(struct nes_ud_file *file, union ib_gid *gid)
{
	int ret = 0;
	int i;
	struct nes_ud_resources *pRsc;

	if (file->queue_type == NES_UD_SEND_QUEUE)
		return -EFAULT;

	pRsc = locate_ud_adapter(file->nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return -EFAULT;

	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if (pRsc->mcast[i].in_use &&
			pRsc->mcast[i].owner == file &&
			pRsc->mcast[i].addr[0] == gid->raw[13] &&
			pRsc->mcast[i].addr[1] == gid->raw[14] &&
			pRsc->mcast[i].addr[2] == gid->raw[15]) {
				pRsc->mcast[i].in_use = 0;
				goto out;
		}
	}

	ret = -EFAULT;
out:
	nes_debug(NES_DBG_UD, "%s(%d) %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \
		ret=%d mcast=%d\n", __func__, __LINE__, gid->raw[10],
		gid->raw[11], gid->raw[12], gid->raw[13], gid->raw[14],
		gid->raw[15], ret , i);
	return ret;

}

/* function returns a number of QP with allocated multicast entries */
/* in given adapter */
static int get_mcast_number_alloced(struct nes_ud_resources *pRsc)
{
	int i;
	int no = 0;
	int cnt[NES_UD_MAX_NIC_CNT];

	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++)
		cnt[i] = 0;

	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if (pRsc->mcast[i].in_use != 0)
			if (pRsc->mcast[i].owner)
				cnt[pRsc->mcast[i].owner->rsc_idx]++;
	}
	for (i = 0; i <  NES_UD_MAX_NIC_CNT; i++)
		if (cnt[i] != 0)
			no++;

	return no;
}

/*
 * function returns a number of multicast groups subscribed to given Rsc
 * Due to HW limitation it cannot exceeds 40.
 */
static int get_subscribed_mcast(struct nes_ud_resources *pRsc,
				u8 addr0,
				u8 addr1,
				u8 addr2)
{
	int i, idx;
	struct nes_ud_mcast cnt[NES_UD_MAX_MCAST_PER_QP];
	int num_alloced = 0;
	struct nes_ud_mcast *pMcast;

	for (i = 0; i < NES_UD_MAX_MCAST_PER_QP; i++)
		memset(&cnt[i], 0, sizeof(struct nes_ud_mcast));

	/* create a list of MGroups subscribed by this Rsc */
	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		pMcast = &pRsc->mcast[i];
		if (pMcast->in_use != 0) {
			for (idx = 0; idx < num_alloced; idx++) {
				if ((pMcast->addr[0] == cnt[idx].addr[0]) &&
					(pMcast->addr[1] == cnt[idx].addr[1]) &&
					(pMcast->addr[2] == cnt[idx].addr[2]))
					break;
			}
			if (idx == num_alloced) {
				cnt[idx].addr[0] = pMcast->addr[0];
				cnt[idx].addr[1] = pMcast->addr[1];
				cnt[idx].addr[2] = pMcast->addr[2];
				num_alloced++;
				if (num_alloced == NES_UD_MAX_MCAST_PER_QP)
					break;
			}
		}
	}
	/* check id a new group will have a place in PFT */
	for (i = 0; i < num_alloced; i++) {
		if ((addr0 == cnt[i].addr[0]) &&
			(addr1 == cnt[i].addr[1]) &&
			(addr2 == cnt[i].addr[2]))
				break;
	}
	return i;
}

/* function subscribe a multicast group in the system - PFT modification */
int nes_ud_subscribe_mcast(struct nes_ud_file *file, union ib_gid *gid)
{
	struct nes_device *nesdev = file->nesvnic->nesdev;
	int ret = 0;
	int i;
	__u8 hash_idx = 0;
	__u8 instance = file->nes_ud_nic_index & 0x1;
	unsigned addr = 0;
	unsigned mqueue_ind_tbl;
	struct nes_ud_resources *pRsc;

	struct net_device *netdev = file->nesvnic->netdev;
	struct dev_mc_list *mc_list;
	int	multicast_address_exist = 0;


	if (file->queue_type == NES_UD_SEND_QUEUE)
		return -EFAULT;

	pRsc = locate_ud_adapter(nesdev->nesadapter);
	if (pRsc == NULL)
		return -EFAULT;

	for (mc_list = netdev->mc_list;
		mc_list != NULL;
		mc_list = mc_list->next) {
		if (mc_list != NULL) {
			if ((mc_list->dmi_addr[3] == gid->raw[13]) &&
			    (mc_list->dmi_addr[4] == gid->raw[14]) &&
			    (mc_list->dmi_addr[5] == gid->raw[15]) &&
			    (mc_list->dmi_addr[0] == 0x01) &&
			    (mc_list->dmi_addr[1] == 0) &&
			    (mc_list->dmi_addr[2] == 0x5e)) {
				multicast_address_exist = 1;
				break;
			}
		} else {
			break;
		}
	}

	if (multicast_address_exist == 0) {
		nes_debug(NES_DBG_UD, "WARNING: multicast address not exist "
			"on multicast list\n");
		return -EFAULT;
	}

	/* first check that we have not subecribed to this mcast address, yet */
	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if ((pRsc->mcast[i].in_use > 0) &&
		    (pRsc->mcast[i].addr[0] == gid->raw[13]) &&
		    (pRsc->mcast[i].addr[1] == gid->raw[14]) &&
		    (pRsc->mcast[i].addr[2] == gid->raw[15])) {
			if (pRsc->mcast[i].owner == file) {
				nes_debug(NES_DBG_UD, "WARNING - subscribing  "
				"mcast to the same nes_ud more than once\n");
				break;
			} else {
		/* receiving the same multicast on different NICs is allowed:
			1. when two different NICS are used
			2.  exactly one QP exists on this adapter
			3. The existing QP was allocated as first
				or the second in the system
		  */
			if (pRsc->mcast[i].owner->nes_ud_nic_index !=
						file->nes_ud_nic_index) {
				if (get_mcast_number_alloced(pRsc) <= 2) {
					/* add the mask of other nics
					that subscribe this address  */
					break;
				}
			}
			printk(KERN_ERR PFX "ERROR - subscribing same mcast "
				"to the diff nes_ud's and NIC  owner_idx = %d "
				"file_idx = %d\n",
				pRsc->mcast[i].owner->nes_ud_nic_index,
				file->nes_ud_nic_index);
			ret = -EFAULT;
		    }
		    goto out;
		}
	}
	/* check if HW limitation for numebr of subscribed Mgroups per Rsc */
	/* is exceeded */
	if (get_subscribed_mcast(pRsc,
				gid->raw[13],
				gid->raw[14],
				gid->raw[15]) >= NES_UD_MAX_MCAST_PER_QP) {
		printk(KERN_ERR PFX "ERROR - subscribing too much MGroups\n");
		ret = -EFAULT;
		goto out;
	}

	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if (!pRsc->mcast[i].in_use) {
			pRsc->mcast[i].addr[0] = gid->raw[13];
			pRsc->mcast[i].addr[1] = gid->raw[14];
			pRsc->mcast[i].addr[2] = gid->raw[15];
			pRsc->mcast[i].owner = file;
			pRsc->mcast[i].in_use = 1;

			hash_idx =
				nes_ud_calculate_hash(pRsc->mcast[i].addr[2]);

			addr = 0x6420 + ((hash_idx >> 3) << 2) + instance*0x100;
			mqueue_ind_tbl = nes_read_indexed(nesdev, addr);
			if (file->prio == NES_UD_DEV_PRIO_HIGH)
				mqueue_ind_tbl &= ~(1 << ((hash_idx & 0x7)*4));
			else
				mqueue_ind_tbl |= 1 << ((hash_idx & 0x7)*4);

			nes_write_indexed(nesdev, addr, mqueue_ind_tbl);
			mqueue_ind_tbl = nes_read_indexed(nesdev, addr);

			nes_debug(NES_DBG_UD, "%s(%d) addr=0x%x "
				 "mqueue_ind_tbl=0x%x hash=0x%x, mac=0x%x\n",
				__func__, __LINE__, addr, mqueue_ind_tbl,
				hash_idx, pRsc->mcast[i].addr[2]);
			/* take care of the case when linux join_mcast
			is called before mcast_attach in that case our pft
			will already be programmed with that mcast address,
			just with wrong NIC we need just to find an address,
			and fix the NIC additionally the mask with other NICs
			that subscribed the address are added*/

			mcast_fix_filter_table(file);
			goto out;
		}
	}
	ret = -EFAULT;

out:

	nes_debug(NES_DBG_UD, "%s(%d) %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \
		ret=%d\n", __func__, __LINE__, gid->raw[10], gid->raw[11],
		gid->raw[12], gid->raw[13], gid->raw[14], gid->raw[15], ret);

	return ret;
}


static inline
int nes_ud_post_recv(struct nes_ud_file *file,
			u32 adap_no,
			struct nes_ud_recv_wr *nes_ud_wr)
{
	struct nes_hw_nic_rq_wqe *nic_rqe;
	struct nes_hw_nic_rq_wqe *rq_vbase =
				(struct nes_hw_nic_rq_wqe *)file->wq_vbase;
	struct nes_device *nesdev = file->nesvnic->nesdev;
	u16 *wqe_fragment_length = NULL;
	u32 mr_offset;
	u32 page_offset;
	u32 page_id;
	struct nes_ud_mem_region *mr = NULL;
	int remaining_length = 0;
	int wqe_fragment_index = 0;
	int err = 0;
	int i = 0;
	struct nes_ud_resources *pRsc;

	/* check if qp is activated */
	if (file->active == 0)
		return -EFAULT;

	pRsc = &nes_ud_rsc[adap_no];

	/* let's assume for now that max sge count is 1 */
	for (i = 0; i < nes_ud_wr->wr_cnt; i++) {
		nic_rqe = &rq_vbase[file->head];

		mr = nes_ud_lookup_mr(nes_ud_wr->sg_list[i].lkey);
		if (mr == NULL)
			return -EFAULT;


		if (mr->va > nes_ud_wr->sg_list[i].addr ||
		    (nes_ud_wr->sg_list[i].addr + nes_ud_wr->sg_list[i].length >
							mr->va + mr->length)) {
			err = -EFAULT;
			goto out;
		}

		mr_offset = nes_ud_wr->sg_list[i].addr - mr->va;
		page_offset = nes_ud_wr->sg_list[i].addr & ~PAGE_MASK;
		page_id = ((mr->va & ~PAGE_MASK) + mr_offset) >> PAGE_SHIFT;

		nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_3_2_IDX] = 0;
		nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_1_0_IDX] = 0;

		wqe_fragment_length =
		(u16 *)&nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_1_0_IDX];

		remaining_length = nes_ud_wr->sg_list[i].length;
		wqe_fragment_index = 0;

		while (remaining_length > 0) {
			if (wqe_fragment_index >= 4) {
				err = -EFAULT;
				goto out;
			}

			set_wqe_64bit_value(nic_rqe->wqe_words,
			NES_NIC_RQ_WQE_FRAG0_LOW_IDX + 2*wqe_fragment_index,
				  mr->addrs[page_id]+page_offset);

			if (remaining_length >= (PAGE_SIZE - page_offset))
				wqe_fragment_length[wqe_fragment_index] =
					cpu_to_le16(PAGE_SIZE - page_offset);
			else
				wqe_fragment_length[wqe_fragment_index] =
					cpu_to_le16(remaining_length);

		      remaining_length -= PAGE_SIZE - page_offset;
		      page_offset = 0;
		      page_id++;
		      wqe_fragment_index++;
		}

		nes_write32(nesdev->regs+NES_WQE_ALLOC, (1 << 24) |  file->qpn);

		file->head = (file->head+1) & ~NES_NIC_WQ_SIZE;
	}
out:
	return err;
}

static inline
int nes_ud_post_send(struct nes_ud_file *file,
			u32 adap_no,
			struct nes_ud_send_wr *nes_ud_wr)
{
	struct nes_hw_nic_sq_wqe *nic_sqe;
	struct nes_hw_nic_sq_wqe *sq_vbase =
			(struct nes_hw_nic_sq_wqe *)file->wq_vbase;
	struct nes_device *nesdev = file->nesvnic->nesdev;
	u16 *wqe_fragment_length = NULL;
	u32 mr_offset;
	u32 page_offset;
	u32 page_id;
	struct nes_ud_mem_region *mr = NULL;
	int remaining_length = 0;
	int wqe_fragment_index = 0;
	int err = 0;
	int misc_flags = NES_NIC_SQ_WQE_COMPLETION;
	int i = 0;
	int vlan_tag = 0;
	struct nes_ud_resources *pRsc;

	/* check if qp is activated */
	if (file->active == 0)
		return -EFAULT;

	pRsc = &nes_ud_rsc[adap_no];

	/* check if is not set checksum */
	if (!(nes_ud_wr->flags & IB_SEND_IP_CSUM))
		misc_flags |= NES_NIC_SQ_WQE_DISABLE_CHKSUM;

#define VLAN_FLAGS 0x20
	if ((nes_ud_wr->flags & VLAN_FLAGS)) {
		vlan_tag = (nes_ud_wr->flags >> 16) & 0xffff;
		misc_flags |= NES_NIC_SQ_WQE_TAGVALUE_ENABLE;
	}
	/* let's assume for now that max sge count is 1 */
	for (i = 0; i < nes_ud_wr->wr_cnt; i++) {
		nic_sqe = &sq_vbase[file->head];

		mr = nes_ud_lookup_mr(nes_ud_wr->sg_list[i].lkey);
		if (mr == NULL)
			return -EFAULT;


		if ((mr->va > nes_ud_wr->sg_list[i].addr) ||
		     (nes_ud_wr->sg_list[i].addr+nes_ud_wr->sg_list[i].length >
							mr->va + mr->length)) {

			err = -EFAULT;
			goto out;
		}

		mr_offset = nes_ud_wr->sg_list[i].addr - mr->va;
		page_offset = nes_ud_wr->sg_list[i].addr & ~PAGE_MASK;
		page_id = ((mr->va & ~PAGE_MASK) + mr_offset) >> PAGE_SHIFT;

		wqe_fragment_length =
		(u16 *)&nic_sqe->wqe_words[NES_NIC_SQ_WQE_LENGTH_0_TAG_IDX];
		wqe_fragment_length[0] = cpu_to_le16(vlan_tag);
		wqe_fragment_length++; /* skip vlan tag */
		remaining_length = nes_ud_wr->sg_list[i].length;
		wqe_fragment_index = 0;

		while (remaining_length > 0) {
			if (wqe_fragment_index >= 4) {
				err = -EFAULT;
				goto out;
			}
			set_wqe_64bit_value(nic_sqe->wqe_words,
				NES_NIC_SQ_WQE_FRAG0_LOW_IDX +
						2*wqe_fragment_index,
				mr->addrs[page_id]+page_offset);
		      wqe_fragment_length[wqe_fragment_index] =
				cpu_to_le16(PAGE_SIZE - page_offset);
		      remaining_length -= PAGE_SIZE - page_offset;
		      page_offset = 0;
		      page_id++;
		      wqe_fragment_index++;
		}
		nic_sqe->wqe_words[NES_IWARP_SQ_WQE_TOTAL_PAYLOAD_IDX] =
				cpu_to_le32(nes_ud_wr->sg_list[i].length);
		nic_sqe->wqe_words[NES_NIC_SQ_WQE_MISC_IDX] =
						cpu_to_le32(misc_flags);

		nes_write32(nesdev->regs+NES_WQE_ALLOC,
					(1 << 24) | (1 << 23) | file->qpn);

		file->head = (file->head+1) & ~NES_NIC_WQ_SIZE;
	}
out:
	return err;
}



static void nes_ud_mcast_cleanup_work(struct nes_ud_file *file)
{
	int i = 0;
	int num_queues = count_files_by_nic(file->nesvnic, file->queue_type);
	struct nes_ud_resources *pRsc;

	pRsc = locate_ud_adapter(file->nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return;


	nes_debug(NES_DBG_UD, "%s(%d) file->rsc_idx=%d\n",
			__func__, __LINE__, file->rsc_idx);

	mutex_lock(&pRsc->mutex);
	for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++) {
		if (pRsc->mcast[i].owner == file) {
			nes_debug(NES_DBG_UD, "%s(%d) mcast cleared idx=%d "
				"%2.2X:%2.2X:%2.2X\n", __func__, __LINE__,
				i, pRsc->mcast[i].addr[0],
				pRsc->mcast[i].addr[1],
				pRsc->mcast[i].addr[2]);

			pRsc->mcast[i].in_use = 0;
			remove_mcast_from_pft(file, pRsc->mcast[i].addr);
		}
	}

	if (del_rsc_list(file) == 0) {
		if (num_queues == 1)
			file->nesvnic->mcrq_mcast_filter = NULL;

	}
	mutex_unlock(&pRsc->mutex);
}

struct nes_ud_file *nes_ud_create_wq(struct nes_vnic *nesvnic, int isrecv)
{
	struct nes_ud_file *file;
	int ret = 0;
	file = nes_ud_get_nxt_channel(nesvnic, (isrecv) ?
				NES_UD_RECV_QUEUE : NES_UD_SEND_QUEUE);
	if (!file)
		return NULL;


	ret =  nes_ud_init_channel(file);
	if (ret != 0) {
		del_rsc_list(file);
		return NULL;
	}

	dev_hold(file->nesvnic->netdev);

	nes_debug(NES_DBG_UD, "%s(%d) file=%p\n", __func__, __LINE__, file);
	return file;
}



int nes_ud_destroy_wq(struct nes_ud_file *file)
{
	struct nes_ud_resources *pRsc;
	int count = 0;
	int i;
	pRsc = locate_ud_adapter(file->nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return -EFAULT;

	if (file->active) {
		nes_ud_mcast_cleanup_work(file);
		nes_ud_free_resources(file);
	}

	/* check if the the adapter has any queues */
	for (i = 0; i < NES_UD_MAX_NIC_CNT; i++) {
		if (pRsc->nics[i].file->active != 0)
			count++;

	}
	if (count == 0) {
		nes_debug(NES_DBG_UD, "%s(%d) adapter %d "
				"is ready to next use\n",
				__func__, __LINE__, pRsc->adapter_no);
		pRsc->pAdap = NULL;
	}
	nes_debug(NES_DBG_UD, "%s(%d) done\n", __func__, __LINE__);
	return 0;
}


struct nes_ud_sksq_file {
	unsigned long shared_page;
	struct nes_ud_file *nes_ud_send_file;
	struct nes_ud_file *nes_ud_recv_file;
};

static ssize_t nes_ud_sksq_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *pos)
{
	struct nes_ud_sksq_file *file = filp->private_data;
	struct nes_ud_send_wr *nes_ud_wr =
			(struct nes_ud_send_wr *)file->shared_page;
	u32 adap_no;
	u32 nic_no;

	nic_no = ((nes_ud_wr->qpn >> 16) & 0x0f00) >> 8;
	adap_no = ((nes_ud_wr->qpn >> 16) & 0xf000) >> 12;
	if (unlikely(!file->nes_ud_send_file)) {
		struct nes_ud_file *nes_ud_file = NULL;

	nes_ud_file = nes_ud_rsc[adap_no].nics[nic_no].file;
	/* the nic must be active and previously activated */
	if ((nes_ud_file->active == 0) ||
		(nes_ud_file->qpn != ((nes_ud_wr->qpn >> 16) & 0xff)))
			return -EAGAIN;

		file->nes_ud_send_file = nes_ud_file;
		nes_debug(NES_DBG_UD, "send shared page addr = %p "
				"adap_no = %d nic_no=%d qpn=%x\n",
				nes_ud_wr, adap_no, nic_no, nes_ud_wr->qpn);
  }
  return nes_ud_post_send(file->nes_ud_send_file, adap_no, nes_ud_wr);

}

static ssize_t nes_ud_sksq_read(struct file *filp, char __user *buf,
		size_t len, loff_t *pos)
{
	struct nes_ud_sksq_file *file = filp->private_data;
	struct nes_ud_recv_wr *nes_ud_recv_wr;
	u32 adap_no;
	u32 nic_no;

	nes_ud_recv_wr = (struct nes_ud_recv_wr *)(file->shared_page+2048);
	adap_no = (nes_ud_recv_wr->qpn & 0xf000) >> 12;
	nic_no = (nes_ud_recv_wr->qpn & 0x0f00) >> 8;

	if (unlikely(!file->nes_ud_recv_file)) {
		struct nes_ud_file *nes_ud_file = NULL;

		nes_ud_file = nes_ud_rsc[adap_no].nics[nic_no].file;
		/* the nic must be active and previously activated */
		if ((nes_ud_file->active == 0) ||
			(nes_ud_file->qpn != (nes_ud_recv_wr->qpn & 0xff)))
				return -EAGAIN;

		file->nes_ud_recv_file  = nes_ud_file;
		nes_debug(NES_DBG_UD, "recv shared page addr = %p "
				"adap_no = %d nic_no=%d qpn=%x\n",
			nes_ud_recv_wr, adap_no, nic_no, nes_ud_recv_wr->qpn);
	}
	return nes_ud_post_recv(file->nes_ud_recv_file,
				adap_no, nes_ud_recv_wr);
}

static int nes_ud_sksq_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct nes_ud_sksq_file *file = filp->private_data;

	nes_debug(NES_DBG_UD, "shared mem pgprot_val(prot)=0x%x pa=%p\n",
			(unsigned int)pgprot_val(vma->vm_page_prot),
			(void *)virt_to_phys((void *)file->shared_page));
	if (remap_pfn_range(vma, vma->vm_start,
			virt_to_phys((void *)file->shared_page) >> PAGE_SHIFT,
			 PAGE_SIZE, vma->vm_page_prot)) {
		printk(KERN_ERR "remap_pfn_range failed.\n");
		return -EAGAIN;
	}
	return 0;
}


static int nes_ud_sksq_open(struct inode *inode, struct file *filp)
{
	struct nes_ud_sksq_file *file;

	file = kmalloc(sizeof *file, GFP_KERNEL);
	if (!file)
		return -ENOMEM;

	memset(file, 0, sizeof *file);
	nes_debug(NES_DBG_UD, "%s(%d) file=%p\n",
			__func__, __LINE__, file);

	filp->private_data = file;
	file->nes_ud_send_file = NULL;
	file->nes_ud_recv_file = NULL;

	file->shared_page = __get_free_page(GFP_USER);
	return 0;
}

static int nes_ud_sksq_close(struct inode *inode, struct file *filp)
{

	struct nes_ud_sksq_file *file = filp->private_data;

	if (file->shared_page) {
		free_page(file->shared_page);
		file->shared_page = 0;
	}
	kfree(file);
	return 0;
}

static const struct file_operations nes_ud_sksq_fops = {
	.owner = THIS_MODULE,
	.open = nes_ud_sksq_open,
	.release = nes_ud_sksq_close,
	.write = nes_ud_sksq_write,
	.read = nes_ud_sksq_read,
	.mmap = nes_ud_sksq_mmap,
};


static struct miscdevice nes_ud_sksq_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "nes_ud_sksq",
	.fops = &nes_ud_sksq_fops,
};

/*
 * function replaces the CQ pointer in QP stored in the file
 * the QP must have a valid CQ pointers assotiated with it
 */
int nes_ud_cq_replace(struct nes_vnic *nesvnic, struct nes_cq *cq)
{
	u32 cq_num;
	struct nes_ud_file *file;
	struct nes_ud_resources *pRsc;

	BUG_ON(!cq);

	pRsc = locate_ud_adapter(nesvnic->nesdev->nesadapter);
	if (pRsc == NULL)
		return -EFAULT;


	/* now create a QP number on base cq and adapter no */
	cq_num = cq->hw_cq.cq_number;

	nes_debug(NES_DBG_UD, "%s(%d) cq_number=%d\n",
			__func__, __LINE__, cq_num);

	/* the QP number should have the same number like CQ number */
	file = get_file_by_qpn(pRsc, cq_num);
	if (!file) {
		nes_debug(NES_DBG_UD, "%s(%d) file not found\n",
				__func__, __LINE__);
		return -EFAULT;
	}
	if (file->qp_ptr) {
		if (file->queue_type == NES_UD_RECV_QUEUE) {
			nes_debug(NES_DBG_UD, "%s(%d) RECV file found "
			"old=%p new=%p\n", __func__, __LINE__,
			file->qp_ptr->ibqp.recv_cq, cq);
			file->qp_ptr->ibqp.recv_cq = &cq->ibcq;
		}
		if (file->queue_type == NES_UD_SEND_QUEUE) {
			nes_debug(NES_DBG_UD, "%s(%d) SEND file found "
			"old=%p new=%p\n", __func__, __LINE__,
			file->qp_ptr->ibqp.send_cq, cq);

			file->qp_ptr->ibqp.send_cq = &cq->ibcq;
		}
	}
	return 0;
}
int nes_ud_init(void)
{
	int i = 0;
	int adap_no;
	struct nes_ud_resources *pRsc;

	nes_debug(NES_DBG_UD, "%s(%d)\n", __func__, __LINE__);

	/* the memory registration is global for all NICS */
	memset(&ud_mem, 0, sizeof(ud_mem));

	/* init hash list of memory entries */
	for (i = 0; i < NES_UD_MAX_REG_HASH_CNT; i++) {
		INIT_LIST_HEAD(&ud_mem.mrs[i].list);
		ud_mem.mrs[i].read_stats = 0;
	}
	mutex_init(&ud_mem.mutex);

	/*allocate resources fro each adapter */
	for (adap_no = 0; adap_no < NES_UD_MAX_ADAPTERS; adap_no++) {
		pRsc = &nes_ud_rsc[adap_no];

		memset(pRsc, 0, sizeof(*pRsc));

		mutex_init(&pRsc->mutex);

		pRsc->adapter_no = adap_no;
		pRsc->pAdap = NULL;

		pRsc->num_logport_confed = 0;
		pRsc->num_allocated_nics = 0;
		pRsc->logport_2_map = 0xf;
		pRsc->logport_3_map = 0xf;
		for (i = 0; i < NES_UD_MCAST_TBL_SZ; i++)
			pRsc->mcast[i].in_use = 0;

		pRsc->nics[0].qpn = 20;
		pRsc->nics[0].nic_index = 2;
		pRsc->nics[0].logical_port = 2;
		pRsc->nics[0].prio = NES_UD_DEV_PRIO_HIGH;
		pRsc->nics[0].queue_type = NES_UD_RECV_QUEUE;
		pRsc->nics[0].file = &pRsc->nics[0].file_body;

		pRsc->nics[1].qpn = 22;
		pRsc->nics[1].nic_index = 3;
		pRsc->nics[1].logical_port = 3;
		pRsc->nics[1].prio = NES_UD_DEV_PRIO_HIGH;
		pRsc->nics[1].queue_type = NES_UD_RECV_QUEUE;
		pRsc->nics[1].file = &pRsc->nics[1].file_body;

		pRsc->nics[2].qpn = 21;
		pRsc->nics[2].nic_index = 2;
		pRsc->nics[2].logical_port = 2;
		pRsc->nics[2].prio = NES_UD_DEV_PRIO_LOW;
		pRsc->nics[2].queue_type = NES_UD_RECV_QUEUE;
		pRsc->nics[2].file = &pRsc->nics[2].file_body;

		pRsc->nics[3].qpn = 23;
		pRsc->nics[3].nic_index = 3;
		pRsc->nics[3].logical_port = 3;
		pRsc->nics[3].prio = NES_UD_DEV_PRIO_LOW;
		pRsc->nics[3].queue_type = NES_UD_RECV_QUEUE;
		pRsc->nics[3].file = &pRsc->nics[3].file_body;

		pRsc->nics[4].qpn = 26;
		pRsc->nics[4].nic_index = 6;
		pRsc->nics[4].logical_port = 2;
		pRsc->nics[4].prio = NES_UD_DEV_PRIO_HIGH;
		pRsc->nics[4].queue_type = NES_UD_SEND_QUEUE;
		pRsc->nics[4].file = &pRsc->nics[4].file_body;

		pRsc->nics[5].qpn = 27;
		pRsc->nics[5].nic_index = 7;
		pRsc->nics[5].logical_port = 3;
		pRsc->nics[5].prio = NES_UD_DEV_PRIO_HIGH;
		pRsc->nics[5].queue_type = NES_UD_SEND_QUEUE;
		pRsc->nics[5].file = &pRsc->nics[5].file_body;

		pRsc->nics[6].qpn = 30;
		pRsc->nics[6].nic_index = 10;
		pRsc->nics[6].logical_port = 2;
		pRsc->nics[6].prio = NES_UD_DEV_PRIO_LOW;
		pRsc->nics[6].queue_type = NES_UD_SEND_QUEUE;
		pRsc->nics[6].file = &pRsc->nics[6].file_body;

		pRsc->nics[7].qpn = 31;
		pRsc->nics[7].nic_index = 11;
		pRsc->nics[7].logical_port = 3;
		pRsc->nics[7].prio = NES_UD_DEV_PRIO_LOW;
		pRsc->nics[7].queue_type = NES_UD_SEND_QUEUE;
		pRsc->nics[7].file = &pRsc->nics[7].file_body;

	}
	nes_ud_workqueue = create_singlethread_workqueue("nes_ud");

	return misc_register(&nes_ud_sksq_misc);
}


int nes_ud_exit(void)
{
	/* clean memory hash list */
	nes_ud_cleanup_mr();
	misc_deregister(&nes_ud_sksq_misc);
	return 0;
}

