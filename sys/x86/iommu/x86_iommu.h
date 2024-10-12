/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015, 2024 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __X86_IOMMU_X86_IOMMU_H
#define	__X86_IOMMU_X86_IOMMU_H

/* Both Intel and AMD are not too crazy to have different sizes. */
typedef struct iommu_pte {
	uint64_t pte;
} iommu_pte_t;

#define	IOMMU_PAGE_SIZE		PAGE_SIZE
#define	IOMMU_PAGE_MASK		(IOMMU_PAGE_SIZE - 1)
#define	IOMMU_PAGE_SHIFT	PAGE_SHIFT
#define	IOMMU_NPTEPG		(IOMMU_PAGE_SIZE / sizeof(iommu_pte_t))
#define	IOMMU_NPTEPGSHIFT 	9
#define	IOMMU_PTEMASK		(IOMMU_NPTEPG - 1)

struct sf_buf;
struct vm_object;

struct vm_page *iommu_pgalloc(struct vm_object *obj, vm_pindex_t idx,
    int flags);
void iommu_pgfree(struct vm_object *obj, vm_pindex_t idx, int flags,
    struct iommu_map_entry *entry);
void *iommu_map_pgtbl(struct vm_object *obj, vm_pindex_t idx, int flags,
    struct sf_buf **sf);
void iommu_unmap_pgtbl(struct sf_buf *sf);

extern iommu_haddr_t iommu_high;
extern int iommu_tbl_pagecnt;
extern int iommu_qi_batch_coalesce;

SYSCTL_DECL(_hw_iommu);

struct x86_unit_common;

struct x86_iommu {
	struct x86_unit_common *(*get_x86_common)(struct
	    iommu_unit *iommu);
	void (*unit_pre_instantiate_ctx)(struct iommu_unit *iommu);
	void (*qi_ensure)(struct iommu_unit *unit, int descr_count);
	void (*qi_emit_wait_descr)(struct iommu_unit *unit, uint32_t seq,
	    bool, bool, bool);
	void (*qi_advance_tail)(struct iommu_unit *unit);
	void (*qi_invalidate_emit)(struct iommu_domain *idomain,
	    iommu_gaddr_t base, iommu_gaddr_t size, struct iommu_qi_genseq *
	    pseq, bool emit_wait);
	void (*domain_unload_entry)(struct iommu_map_entry *entry, bool free,
	    bool cansleep);
	void (*domain_unload)(struct iommu_domain *iodom,
		struct iommu_map_entries_tailq *entries, bool cansleep);
	struct iommu_ctx *(*get_ctx)(struct iommu_unit *iommu,
	    device_t dev, uint16_t rid, bool id_mapped, bool rmrr_init);
	void (*free_ctx_locked)(struct iommu_unit *iommu,
	    struct iommu_ctx *context);
	void (*free_ctx)(struct iommu_ctx *context);
	struct iommu_unit *(*find)(device_t dev, bool verbose);
	int (*alloc_msi_intr)(device_t src, u_int *cookies, u_int count);
	int (*map_msi_intr)(device_t src, u_int cpu, u_int vector,
	    u_int cookie, uint64_t *addr, uint32_t *data);
	int (*unmap_msi_intr)(device_t src, u_int cookie);
	int (*map_ioapic_intr)(u_int ioapic_id, u_int cpu, u_int vector,
	    bool edge, bool activehi, int irq, u_int *cookie, uint32_t *hi,
	    uint32_t *lo);
	int (*unmap_ioapic_intr)(u_int ioapic_id, u_int *cookie);
};
void set_x86_iommu(struct x86_iommu *);
struct x86_iommu *get_x86_iommu(void);

struct iommu_msi_data {
	int irq;
	int irq_rid;
	struct resource *irq_res;
	void *intr_handle;
	int (*handler)(void *);
	int msi_data_reg;
	int msi_addr_reg;
	int msi_uaddr_reg;
	uint64_t msi_addr;
	uint32_t msi_data;
	void (*enable_intr)(struct iommu_unit *);
	void (*disable_intr)(struct iommu_unit *);
	const char *name;
};

#define	IOMMU_MAX_MSI	3

struct x86_unit_common {
	uint32_t qi_buf_maxsz;
	uint32_t qi_cmd_sz;

	char *inv_queue;
	vm_size_t inv_queue_size;
	uint32_t inv_queue_avail;
	uint32_t inv_queue_tail;

	/*
	 * Hw writes there on completion of wait descriptor
	 * processing.  Intel writes 4 bytes, while AMD does the
	 * 8-bytes write.  Due to little-endian, and use of 4-byte
	 * sequence numbers, the difference does not matter for us.
	 */
	volatile uint64_t inv_waitd_seq_hw;

	uint64_t inv_waitd_seq_hw_phys;
	uint32_t inv_waitd_seq; /* next sequence number to use for wait descr */
	u_int inv_waitd_gen;	/* seq number generation AKA seq overflows */
	u_int inv_seq_waiters;	/* count of waiters for seq */
	u_int inv_queue_full;	/* informational counter */

	/*
	 * Delayed freeing of map entries queue processing:
	 *
	 * tlb_flush_head and tlb_flush_tail are used to implement a FIFO
	 * queue that supports concurrent dequeues and enqueues.  However,
	 * there can only be a single dequeuer (accessing tlb_flush_head) and
	 * a single enqueuer (accessing tlb_flush_tail) at a time.  Since the
	 * unit's qi_task is the only dequeuer, it can access tlb_flush_head
	 * without any locking.  In contrast, there may be multiple enqueuers,
	 * so the enqueuers acquire the iommu unit lock to serialize their
	 * accesses to tlb_flush_tail.
	 *
	 * In this FIFO queue implementation, the key to enabling concurrent
	 * dequeues and enqueues is that the dequeuer never needs to access
	 * tlb_flush_tail and the enqueuer never needs to access
	 * tlb_flush_head.  In particular, tlb_flush_head and tlb_flush_tail
	 * are never NULL, so neither a dequeuer nor an enqueuer ever needs to
	 * update both.  Instead, tlb_flush_head always points to a "zombie"
	 * struct, which previously held the last dequeued item.  Thus, the
	 * zombie's next field actually points to the struct holding the first
	 * item in the queue.  When an item is dequeued, the current zombie is
	 * finally freed, and the struct that held the just dequeued item
	 * becomes the new zombie.  When the queue is empty, tlb_flush_tail
	 * also points to the zombie.
	 */
	struct iommu_map_entry *tlb_flush_head;
	struct iommu_map_entry *tlb_flush_tail;
	struct task qi_task;
	struct taskqueue *qi_taskqueue;

	struct iommu_msi_data intrs[IOMMU_MAX_MSI];
};

void iommu_domain_free_entry(struct iommu_map_entry *entry, bool free);

void iommu_qi_emit_wait_seq(struct iommu_unit *unit, struct iommu_qi_genseq *
    pseq, bool emit_wait);
void iommu_qi_wait_for_seq(struct iommu_unit *unit, const struct
    iommu_qi_genseq *gseq, bool nowait);
void iommu_qi_drain_tlb_flush(struct iommu_unit *unit);
void iommu_qi_invalidate_locked(struct iommu_domain *domain,
    struct iommu_map_entry *entry, bool emit_wait);
void iommu_qi_invalidate_sync(struct iommu_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size, bool cansleep);
void iommu_qi_common_init(struct iommu_unit *unit, task_fn_t taskfunc);
void iommu_qi_common_fini(struct iommu_unit *unit, void (*disable_qi)(
    struct iommu_unit *));

int iommu_alloc_irq(struct iommu_unit *unit, int idx);
void iommu_release_intr(struct iommu_unit *unit, int idx);

void iommu_device_tag_init(struct iommu_ctx *ctx, device_t dev);

int pglvl_pgtbl_pte_off(int pglvl, iommu_gaddr_t base, int lvl);
vm_pindex_t pglvl_pgtbl_get_pindex(int pglvl, iommu_gaddr_t base, int lvl);
vm_pindex_t pglvl_max_pages(int pglvl);
iommu_gaddr_t pglvl_page_size(int total_pglvl, int lvl);

void iommu_db_print_domain_entry(const struct iommu_map_entry *entry);
void iommu_db_print_ctx(struct iommu_ctx *ctx);
void iommu_db_domain_print_contexts(struct iommu_domain *iodom);
void iommu_db_domain_print_mappings(struct iommu_domain *iodom);

#endif
