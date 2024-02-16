/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2021 - 2023 Intel Corporation
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

#ifndef _ICRDMA_OSDEP_H_
#define _ICRDMA_OSDEP_H_

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <infiniband/types.h>
#include <infiniband/verbs.h>
#include <infiniband/udma_barrier.h>
#include <sys/bus.h>
#include <sys/bus_dma.h>
#include <sys/endian.h>

#define IOMEM
#define IRDMA_NTOHL(a) ntohl(a)
#define IRDMA_NTOHS(a) ntohs(a)
#define MAKEMASK(m, s) ((m) << (s))
#define OS_TIMER timer_list
#define OS_LIST_HEAD list_head
#define OS_LIST_ENTRY list_head
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

#define BIT_ULL(a) (1ULL << (a))
#define min(a, b) ((a) > (b) ? (b) : (a))
#ifndef likely
#define likely(x)  __builtin_expect((x), 1)
#endif
#ifndef unlikely
#define unlikely(x)  __builtin_expect((x), 0)
#endif

#define __aligned_u64 uint64_t __aligned(8)

#define VLAN_PRIO_SHIFT 13
#define IB_USER_VERBS_EX_CMD_MODIFY_QP IB_USER_VERBS_CMD_MODIFY_QP

/*
 * debug definition section
 */
#define irdma_print(S, ...) printf("%s:%d "S, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define irdma_debug_buf(dev, mask, desc, buf, size)							\
do {													\
	u32 i;												\
	if (!((mask) & (dev)->debug_mask)) {								\
		break;											\
	}												\
	irdma_debug(dev, mask, "%s\n", desc);								\
	irdma_debug(dev, mask, "starting address virt=%p phy=%lxh\n", buf, irdma_get_virt_to_phy(buf));	\
	for (i = 0; i < size ; i += 8)									\
		irdma_debug(dev, mask, "index %03d val: %016lx\n", i, ((unsigned long *)(buf))[i / 8]);	\
} while(0)

#define irdma_debug(h, m, s, ...)				\
do {								\
	if (!(h)) {						\
		if ((m) == IRDMA_DEBUG_INIT)			\
			printf("irdma INIT " s, ##__VA_ARGS__);	\
	} else if (((m) & (h)->debug_mask)) {			\
		printf("irdma " s, ##__VA_ARGS__);		\
	} 							\
} while (0)
extern unsigned int irdma_dbg;
#define libirdma_debug(fmt, args...)				\
do {								\
	if (irdma_dbg)						\
		printf("libirdma-%s: " fmt, __func__, ##args);	\
} while (0)
#define irdma_dev_err(ibdev, fmt, ...) \
	pr_err("%s:%s:%d ERR "fmt, (ibdev)->name, __func__, __LINE__, ##__VA_ARGS__)
#define irdma_dev_warn(ibdev, fmt, ...) \
	pr_warn("%s:%s:%d WARN "fmt, (ibdev)->name, __func__, __LINE__, ##__VA_ARGS__)
#define irdma_dev_info(a, b, ...) printf(b, ##__VA_ARGS__)
#define irdma_pr_warn printf

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

#define irdma_pr_info(fmt, args ...) printf("%s: WARN "fmt, __func__, ## args)
#define irdma_pr_err(fmt, args ...) printf("%s: ERR "fmt, __func__, ## args)
#define irdma_memcpy(a, b, c)  memcpy((a), (b), (c))
#define irdma_memset(a, b, c)  memset((a), (b), (c))
#define irdma_usec_delay(x) DELAY(x)
#define mdelay(x) DELAY((x) * 1000)

#define rt_tos2priority(tos) (tos >> 5)
#define ah_attr_to_dmac(attr) ((attr).dmac)
#define kc_typeq_ib_wr const
#define kc_ifp_find ip_ifp_find
#define kc_ifp6_find ip6_ifp_find
#define kc_rdma_gid_attr_network_type(sgid_attr, gid_type, gid) \
	ib_gid_to_network_type(gid_type, gid)
#define irdma_del_timer_compat(tt) del_timer((tt))
#define IRDMA_TAILQ_FOREACH CK_STAILQ_FOREACH
#define IRDMA_TAILQ_FOREACH_SAFE CK_STAILQ_FOREACH_SAFE
#define between(a, b, c) (bool)(c-a >= b-a)

static inline void db_wr32(__u32 val, __u32 *wqe_word)
{
	*wqe_word = val;
}

void *hw_to_dev(struct irdma_hw *hw);

struct irdma_dma_mem {
	void *va;
	u64 pa;
	bus_dma_tag_t tag;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
	bus_size_t size;
	int nseg;
	int flags;
};

struct irdma_virt_mem {
	void *va;
	u32 size;
};

#ifndef verbs_mr
enum ibv_mr_type {
	IBV_MR_TYPE_MR,
	IBV_MR_TYPE_NULL_MR,
};

struct verbs_mr {
	struct ibv_mr		ibv_mr;
	enum ibv_mr_type	mr_type;
	int 			access;
};
#define verbs_get_mr(mr) container_of((mr), struct verbs_mr, ibv_mr)
#endif
#endif /* _ICRDMA_OSDEP_H_ */
