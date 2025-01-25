/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
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

#ifndef __X86_IOMMU_AMD_IOMMU_H
#define	__X86_IOMMU_AMD_IOMMU_H

#include <dev/iommu/iommu.h>

#define	AMDIOMMU_DEV_REPORTED	0x00000001

struct amdiommu_unit;

struct amdiommu_domain {
	struct iommu_domain iodom;
	int domain;			/* (c) DID, written in context entry */
	struct amdiommu_unit *unit;	/* (c) */

	u_int ctx_cnt;			/* (u) Number of contexts owned */
	u_int refs;			/* (u) Refs, including ctx */
	LIST_ENTRY(amdiommu_domain) link;/* (u) Member in the iommu list */
	vm_object_t pgtbl_obj;		/* (c) Page table pages */
	vm_page_t pgtblr;		/* (c) Page table root page */
	u_int pglvl;			/* (c) Page table levels */
};

struct amdiommu_ctx {
	struct iommu_ctx context;
	struct amdiommu_irte_basic_novapic *irtb;
	struct amdiommu_irte_basic_vapic_x2 *irtx2;
	vmem_t *irtids;
};

struct amdiommu_unit {
	struct iommu_unit iommu;
	struct x86_unit_common x86c;
	u_int		unit_dom;	/* Served PCI domain, from IVRS */
	u_int		device_id;	/* basically PCI RID */
	u_int		unit_id;	/* Hypertransport Unit ID, deprecated */
	TAILQ_ENTRY(amdiommu_unit) unit_next;
	int		seccap_reg;
	uint64_t	efr;
	vm_paddr_t	mmio_base;
	vm_size_t	mmio_sz;
	struct resource	*mmio_res;
	int		mmio_rid;
	uint64_t	hw_ctrl;

	u_int		numirqs;
	struct resource *msix_table;
	int		msix_table_rid;
	int		irq_cmdev_rid;
	struct resource *irq_cmdev;
	void		*irq_cmdev_cookie;

	struct amdiommu_dte *dev_tbl;
	vm_object_t	devtbl_obj;

	LIST_HEAD(, amdiommu_domain) domains;
	struct unrhdr	*domids;

	struct mtx	event_lock;
	struct amdiommu_event_generic *event_log;
	u_int		event_log_size;
	u_int		event_log_head;
	u_int		event_log_tail;
	struct task	event_task;
	struct taskqueue *event_taskqueue;
	struct amdiommu_event_generic event_copy_log[16];
	u_int		event_copy_head;
	u_int		event_copy_tail;

	int		irte_enabled;	/* int for sysctl type */
	bool		irte_x2apic;
	u_int		irte_nentries;
};

#define	AMD2IOMMU(unit)	(&((unit)->iommu))
#define	IOMMU2AMD(unit)	\
	__containerof((unit), struct amdiommu_unit, iommu)

#define	AMDIOMMU_LOCK(unit)		mtx_lock(&AMD2IOMMU(unit)->lock)
#define	AMDIOMMU_UNLOCK(unit)		mtx_unlock(&AMD2IOMMU(unit)->lock)
#define	AMDIOMMU_ASSERT_LOCKED(unit)	mtx_assert(&AMD2IOMMU(unit)->lock, \
    MA_OWNED)

#define	AMDIOMMU_EVENT_LOCK(unit)	mtx_lock_spin(&(unit)->event_lock)
#define	AMDIOMMU_EVENT_UNLOCK(unit)	mtx_unlock_spin(&(unit)->event_lock)
#define	AMDIOMMU_EVENT_ASSERT_LOCKED(unit) \
    mtx_assert(&(unit)->event_lock, MA_OWNED)

#define	DOM2IODOM(domain)	(&((domain)->iodom))
#define	IODOM2DOM(domain)	\
	__containerof((domain), struct amdiommu_domain, iodom)

#define	CTX2IOCTX(ctx)		(&((ctx)->context))
#define	IOCTX2CTX(ctx)		\
	__containerof((ctx), struct amdiommu_ctx, context)

#define	CTX2DOM(ctx)		IODOM2DOM((ctx)->context.domain)
#define	CTX2AMD(ctx)		(CTX2DOM(ctx)->unit)
#define	DOM2AMD(domain)		((domain)->unit)

#define	AMDIOMMU_DOMAIN_LOCK(dom)	mtx_lock(&(dom)->iodom.lock)
#define	AMDIOMMU_DOMAIN_UNLOCK(dom)	mtx_unlock(&(dom)->iodom.lock)
#define	AMDIOMMU_DOMAIN_ASSERT_LOCKED(dom) \
	mtx_assert(&(dom)->iodom.lock, MA_OWNED)

#define	AMDIOMMU_DOMAIN_PGLOCK(dom)	VM_OBJECT_WLOCK((dom)->pgtbl_obj)
#define	AMDIOMMU_DOMAIN_PGTRYLOCK(dom)	VM_OBJECT_TRYWLOCK((dom)->pgtbl_obj)
#define	AMDIOMMU_DOMAIN_PGUNLOCK(dom)	VM_OBJECT_WUNLOCK((dom)->pgtbl_obj)
#define	AMDIOMMU_DOMAIN_ASSERT_PGLOCKED(dom) \
	VM_OBJECT_ASSERT_WLOCKED((dom)->pgtbl_obj)

#define	AMDIOMMU_RID	1001

static inline uint32_t
amdiommu_read4(const struct amdiommu_unit *unit, int reg)
{

	return (bus_read_4(unit->mmio_res, reg));
}

static inline uint64_t
amdiommu_read8(const struct amdiommu_unit *unit, int reg)
{
#ifdef __i386__
	uint32_t high, low;

	low = bus_read_4(unit->mmio_res, reg);
	high = bus_read_4(unit->mmio_res, reg + 4);
	return (low | ((uint64_t)high << 32));
#else
	return (bus_read_8(unit->mmio_res, reg));
#endif
}

static inline void
amdiommu_write4(const struct amdiommu_unit *unit, int reg, uint32_t val)
{
	bus_write_4(unit->mmio_res, reg, val);
}

static inline void
amdiommu_write8(const struct amdiommu_unit *unit, int reg, uint64_t val)
{
#ifdef __i386__
	uint32_t high, low;

	low = val;
	high = val >> 32;
	bus_write_4(unit->mmio_res, reg, low);
	bus_write_4(unit->mmio_res, reg + 4, high);
#else
	bus_write_8(unit->mmio_res, reg, val);
#endif
}

int amdiommu_find_unit(device_t dev, struct amdiommu_unit **unitp,
    uint16_t *ridp, uint8_t *dtep, uint32_t *edtep, bool verbose);
int amdiommu_find_unit_for_ioapic(int apic_id, struct amdiommu_unit **unitp,
    uint16_t *ridp, uint8_t *dtep, uint32_t *edtep, bool verbose);
int amdiommu_find_unit_for_hpet(device_t hpet, struct amdiommu_unit **unitp,
    uint16_t *ridp, uint8_t *dtep, uint32_t *edtep, bool verbose);

int amdiommu_init_cmd(struct amdiommu_unit *unit);
void amdiommu_fini_cmd(struct amdiommu_unit *unit);

void amdiommu_event_intr(struct amdiommu_unit *unit, uint64_t status);
int amdiommu_init_event(struct amdiommu_unit *unit);
void amdiommu_fini_event(struct amdiommu_unit *unit);

int amdiommu_alloc_msi_intr(device_t src, u_int *cookies, u_int count);
int amdiommu_map_msi_intr(device_t src, u_int cpu, u_int vector,
    u_int cookie, uint64_t *addr, uint32_t *data);
int amdiommu_unmap_msi_intr(device_t src, u_int cookie);
int amdiommu_map_ioapic_intr(u_int ioapic_id, u_int cpu, u_int vector,
    bool edge, bool activehi, int irq, u_int *cookie, uint32_t *hi,
    uint32_t *lo);
int amdiommu_unmap_ioapic_intr(u_int ioapic_id, u_int *cookie);
int amdiommu_init_irt(struct amdiommu_unit *unit);
void amdiommu_fini_irt(struct amdiommu_unit *unit);
int amdiommu_ctx_init_irte(struct amdiommu_ctx *ctx);
void amdiommu_ctx_fini_irte(struct amdiommu_ctx *ctx);

void amdiommu_domain_unload_entry(struct iommu_map_entry *entry, bool free,
    bool cansleep);
void amdiommu_domain_unload(struct iommu_domain *iodom,
    struct iommu_map_entries_tailq *entries, bool cansleep);
struct amdiommu_ctx *amdiommu_get_ctx_for_dev(struct amdiommu_unit *unit,
    device_t dev, uint16_t rid, int dev_domain, bool id_mapped,
    bool rmrr_init, uint8_t dte, uint32_t edte);
struct iommu_ctx *amdiommu_get_ctx(struct iommu_unit *iommu, device_t dev,
    uint16_t rid, bool id_mapped, bool rmrr_init);
struct amdiommu_ctx *amdiommu_find_ctx_locked(struct amdiommu_unit *unit,
    uint16_t rid);
void amdiommu_free_ctx_locked_method(struct iommu_unit *iommu,
    struct iommu_ctx *context);
struct amdiommu_domain *amdiommu_find_domain(struct amdiommu_unit *unit,
    uint16_t rid);

void amdiommu_qi_invalidate_ctx_locked(struct amdiommu_ctx *ctx);
void amdiommu_qi_invalidate_ctx_locked_nowait(struct amdiommu_ctx *ctx);
void amdiommu_qi_invalidate_ir_locked(struct amdiommu_unit *unit,
    uint16_t devid);
void amdiommu_qi_invalidate_ir_locked_nowait(struct amdiommu_unit *unit,
    uint16_t devid);
void amdiommu_qi_invalidate_all_pages_locked_nowait(
    struct amdiommu_domain *domain);
void amdiommu_qi_invalidate_wait_sync(struct iommu_unit *iommu);

int amdiommu_domain_alloc_pgtbl(struct amdiommu_domain *domain);
void amdiommu_domain_free_pgtbl(struct amdiommu_domain *domain);
extern const struct iommu_domain_map_ops amdiommu_domain_map_ops;

int amdiommu_is_running(void);

#endif
