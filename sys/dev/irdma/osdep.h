/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2021 - 2022 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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
/*$FreeBSD$*/

#ifndef _ICRDMA_OSDEP_H_
#define _ICRDMA_OSDEP_H_

#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jhash.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include <sys/bus.h>
#include <machine/bus.h>

#define ATOMIC atomic_t
#define IOMEM
#define IRDMA_NTOHS(a) ntohs(a)
#define MAKEMASK(m, s) ((m) << (s))
#define OS_TIMER timer_list
#define DECLARE_HASHTABLE(n, b) struct hlist_head (n)[1 << (b)]
#define HASH_MIN(v, b) (sizeof(v) <= 4 ? hash_32(v, b) : hash_long(v, b))
#define HASH_FOR_EACH_RCU(n, b, o, m) 	for ((b) = 0, o = NULL; o == NULL && (b) < ARRAY_SIZE(n);\
			(b)++)\
		hlist_for_each_entry_rcu(o, &n[(b)], m)
#define HASH_FOR_EACH_POSSIBLE_RCU(n, o, m, k)		\
	hlist_for_each_entry_rcu(o, &n[jhash(&k, sizeof(k), 0) >> (32 - ilog2(ARRAY_SIZE(n)))],\
		m)
#define HASH_FOR_EACH_POSSIBLE(n, o, m, k)		\
	hlist_for_each_entry(o, &n[jhash(&k, sizeof(k), 0) >> (32 - ilog2(ARRAY_SIZE(n)))],\
		m)
#define HASH_ADD_RCU(h, n, k) \
	hlist_add_head_rcu(n, &h[jhash(&k, sizeof(k), 0) >> (32 - ilog2(ARRAY_SIZE(h)))])
#define HASH_DEL_RCU(tbl, node) hlist_del_rcu(node)
#define HASH_ADD(h, n, k) \
	hlist_add_head(n, &h[jhash(&k, sizeof(k), 0) >> (32 - ilog2(ARRAY_SIZE(h)))])
#define HASH_DEL(tbl, node) hlist_del(node)

#define WQ_UNBOUND_MAX_ACTIVE max_t(int, 512, num_possible_cpus() * 4)
#define if_addr_rlock(x)
#define if_addr_runlock(x)

/* constants */
#define STATS_TIMER_DELAY 60000

/* a couple of linux size defines */
#define SZ_128     128
#define SZ_2K     SZ_128 * 16
#define SZ_1G   (SZ_1K * SZ_1K * SZ_1K)
#define SPEED_1000     1000
#define SPEED_10000   10000
#define SPEED_20000   20000
#define SPEED_25000   25000
#define SPEED_40000   40000
#define SPEED_100000 100000

#define irdma_mb()	mb()
#define irdma_wmb()	wmb()
#define irdma_get_virt_to_phy vtophys

#define __aligned_u64 uint64_t __aligned(8)

#define VLAN_PRIO_SHIFT 13

/*
 * debug definition section
 */
#define irdma_print(S, ...) printf("%s:%d "S, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define irdma_debug_buf(dev, mask, desc, buf, size)							\
do {													\
	u32    i;											\
	if (!((mask) & (dev)->debug_mask)) {								\
		break;											\
	}												\
	irdma_debug(dev, mask, "%s\n", desc);								\
	irdma_debug(dev, mask, "starting address virt=%p phy=%lxh\n", buf, irdma_get_virt_to_phy(buf));	\
	for (i = 0; i < size ; i += 8)									\
		irdma_debug(dev, mask, "index %03d val: %016lx\n", i, ((unsigned long *)buf)[i / 8]);	\
} while(0)

#define irdma_debug(h, m, s, ...)					\
do {									\
	if (!(h)) {							\
		if ((m) == IRDMA_DEBUG_INIT)				\
			printf("irdma INIT " s, ##__VA_ARGS__);	\
	} else if (((m) & (h)->debug_mask)) {				\
		printf("irdma " s, ##__VA_ARGS__);			\
	} 								\
} while (0)
#define irdma_dev_err(a, b, ...) printf(b, ##__VA_ARGS__)
#define irdma_dev_warn(a, b, ...) printf(b, ##__VA_ARGS__) /*dev_warn(a, b)*/
#define irdma_dev_info(a, b, ...) printf(b, ##__VA_ARGS__)
#define irdma_pr_warn printf
#define ibdev_err(ibdev, fmt, ...)  irdma_dev_err(&((ibdev)->dev), fmt, ##__VA_ARGS__)

#define dump_struct(s, sz, name)	\
do {				\
	unsigned char *a;	\
	printf("%s %u", (name), (unsigned int)(sz));				\
	for (a = (unsigned char*)(s); a < (unsigned char *)(s) + (sz) ; a ++) {	\
		if ((u64)a % 8 == 0)		\
			printf("\n%p ", a);	\
		printf("%2x ", *a);		\
	}			\
	printf("\n");		\
}while(0)

/*
 * debug definition end
 */

typedef __be16 BE16;
typedef __be32 BE32;
typedef uintptr_t irdma_uintptr;

struct irdma_hw;
struct irdma_pci_f;
struct irdma_sc_dev;
struct irdma_sc_qp;
struct irdma_sc_vsi;

struct irdma_task_arg {
	struct irdma_device *iwdev;
	struct ice_rdma_peer *peer;
	atomic_t open_ongoing;
	atomic_t close_ongoing;
};

struct irdma_dev_ctx {
	bus_space_tag_t mem_bus_space_tag;
	bus_space_handle_t mem_bus_space_handle;
	bus_size_t mem_bus_space_size;
	void *dev;
	struct irdma_task_arg task_arg;
};

#define irdma_pr_info(fmt, args ...) printf("%s: WARN "fmt, __func__, ## args)
#define irdma_pr_err(fmt, args ...) printf("%s: ERR "fmt, __func__, ## args)
#define irdma_memcpy(a, b, c)  memcpy((a), (b), (c))
#define irdma_memset(a, b, c)  memset((a), (b), (c))
#define irdma_usec_delay(x) DELAY(x)
#define mdelay(x) DELAY((x) * 1000)

#define rt_tos2priority(tos) (((tos >> 1) & 0x8 >> 1) | ((tos >> 2) ^ ((tos >> 3) << 1)))
#define ah_attr_to_dmac(attr) ((attr).dmac)
#define kc_rdma_gid_attr_network_type(sgid_attr, gid_type, gid) \
        ib_gid_to_network_type(gid_type, gid)
#define irdma_del_timer_compat(tt) del_timer((tt))
#define IRDMA_TAILQ_FOREACH CK_STAILQ_FOREACH
#define IRDMA_TAILQ_FOREACH_SAFE CK_STAILQ_FOREACH_SAFE
#define between(a, b, c) (bool)(c-a >= b-a)

#define rd32(a, reg)            irdma_rd32((a)->dev_context, (reg))
#define wr32(a, reg, value)     irdma_wr32((a)->dev_context, (reg), (value))

#define rd64(a, reg)            irdma_rd64((a)->dev_context, (reg))
#define wr64(a, reg, value)     irdma_wr64((a)->dev_context, (reg), (value))
#define db_wr32(value, a)	writel((value), (a))

void *hw_to_dev(struct irdma_hw *hw);

struct irdma_dma_mem {
	void  *va;
	u64    pa;
	bus_dma_tag_t tag;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
	bus_size_t size;
	int    nseg;
	int    flags;
};

struct irdma_virt_mem {
	void  *va;
	u32    size;
};

struct irdma_dma_info {
	dma_addr_t *dmaaddrs;
};

struct list_head;
u32 irdma_rd32(struct irdma_dev_ctx *dev_ctx, u32 reg);
void irdma_wr32(struct irdma_dev_ctx *dev_ctx, u32 reg, u32 value);
u64 irdma_rd64(struct irdma_dev_ctx *dev_ctx, u32 reg);
void irdma_wr64(struct irdma_dev_ctx *dev_ctx, u32 reg, u64 value);

void irdma_term_modify_qp(struct irdma_sc_qp *qp, u8 next_state, u8 term, u8 term_len);
void irdma_terminate_done(struct irdma_sc_qp *qp, int timeout_occurred);
void irdma_terminate_start_timer(struct irdma_sc_qp *qp);
void irdma_terminate_del_timer(struct irdma_sc_qp *qp);

void irdma_hw_stats_start_timer(struct irdma_sc_vsi *vsi);
void irdma_hw_stats_stop_timer(struct irdma_sc_vsi *vsi);
void irdma_send_ieq_ack(struct irdma_sc_qp *qp);

u8* irdma_get_hw_addr(void *par);

void irdma_unmap_vm_page_list(struct irdma_hw *hw, u64 *pg_arr, u32 pg_cnt);
int irdma_map_vm_page_list(struct irdma_hw *hw, void *va,
			   u64 *pg_arr, u32 pg_cnt);

struct ib_device *irdma_get_ibdev(struct irdma_sc_dev *dev);

#endif /* _ICRDMA_OSDEP_H_ */
