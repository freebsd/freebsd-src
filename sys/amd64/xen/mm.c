/* $FreeBSD$ */

/*
 * This file contains xen routines that interface with the hypervisor
 * to operate the mmu.
 * Based on portions of:
 * releng/9.0/sys/i386/xen/xen_machdep.c 216963 2011-01-04 16:29:07Z cperciva
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#include <machine/_inttypes.h> /* For PRIxxx macros */

#include <machine/cpufunc.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenpmap.h>
#include <machine/xen/xenfunc.h>
#include <machine/xen/xenvar.h>

#include <xen/hypervisor.h>


#define XPQUEUE_SIZE 128

struct mmu_log {
	char *file;
	int line;
};

#ifdef SMP
/* per-cpu queues and indices */
#ifdef INVARIANTS
static struct mmu_log xpq_queue_log[MAX_VIRT_CPUS][XPQUEUE_SIZE];
#endif

static int xpq_idx[MAX_VIRT_CPUS];  
static mmu_update_t xpq_queue[MAX_VIRT_CPUS][XPQUEUE_SIZE];

#define	XPQ_QUEUE_LOG xpq_queue_log[vcpu]
#define	XPQ_QUEUE xpq_queue[vcpu]
#define	XPQ_IDX xpq_idx[vcpu]
#define	SET_VCPU() int vcpu = smp_processor_id()
#else
#ifdef INVARIANTS	
static struct mmu_log xpq_queue_log[XPQUEUE_SIZE];
#endif

static mmu_update_t xpq_queue[XPQUEUE_SIZE];
static int xpq_idx = 0;

#define	XPQ_QUEUE_LOG xpq_queue_log
#define	XPQ_QUEUE xpq_queue
#define	XPQ_IDX xpq_idx
#define	SET_VCPU()
#endif /* !SMP */

static __inline void
_xen_flush_queue(void)
{
	SET_VCPU();
	int _xpq_idx = XPQ_IDX;
	int error, i;

#ifdef INVARIANTS
	if (__predict_true(gdtset))
		CRITICAL_ASSERT(curthread);
#endif

	XPQ_IDX = 0;
	/* Make sure index is cleared first to avoid double updates. */
	error = HYPERVISOR_mmu_update((mmu_update_t *)&XPQ_QUEUE,
				      _xpq_idx, NULL, DOMID_SELF);
    
#if 0
	if (__predict_true(gdtset))
	for (i = _xpq_idx; i > 0;) {
		if (i >= 3) {
			CTR6(KTR_PMAP, "mmu:val: %lx ptr: %lx val: %lx "
			    "ptr: %lx val: %lx ptr: %lx",
			    (XPQ_QUEUE[i-1].val & 0xffffffff),
			    (XPQ_QUEUE[i-1].ptr & 0xffffffff),
			    (XPQ_QUEUE[i-2].val & 0xffffffff),
			    (XPQ_QUEUE[i-2].ptr & 0xffffffff),
			    (XPQ_QUEUE[i-3].val & 0xffffffff),
			    (XPQ_QUEUE[i-3].ptr & 0xffffffff));
			    i -= 3;
		} else if (i == 2) {
			CTR4(KTR_PMAP, "mmu: val: %lx ptr: %lx val: %lx ptr: %lx",
			    (XPQ_QUEUE[i-1].val & 0xffffffff),
			    (XPQ_QUEUE[i-1].ptr & 0xffffffff),
			    (XPQ_QUEUE[i-2].val & 0xffffffff),
			    (XPQ_QUEUE[i-2].ptr & 0xffffffff));
			i = 0;
		} else {
			CTR2(KTR_PMAP, "mmu: val: %lx ptr: %lx", 
			    (XPQ_QUEUE[i-1].val & 0xffffffff),
			    (XPQ_QUEUE[i-1].ptr & 0xffffffff));
			i = 0;
		}
	}
#endif	
	if (__predict_false(error < 0)) {
		for (i = 0; i < _xpq_idx; i++)
			printf("val: %"PRIx64" ptr: %"PRIx64"\n",
			    XPQ_QUEUE[i].val, XPQ_QUEUE[i].ptr);
		panic("Failed to execute MMU updates: %d", error);
	}

}

void
xen_flush_queue(void)
{
	SET_VCPU();

	if (__predict_true(gdtset))
		critical_enter();
	if (XPQ_IDX != 0) _xen_flush_queue();
	if (__predict_true(gdtset))
		critical_exit();
}

static __inline void
xen_increment_idx(void)
{
	SET_VCPU();

	XPQ_IDX++;
	if (__predict_false(XPQ_IDX == XPQUEUE_SIZE))
		xen_flush_queue();
}

void
xen_check_queue(void)
{
#ifdef INVARIANTS
	SET_VCPU();
	
	KASSERT(XPQ_IDX == 0, ("pending operations XPQ_IDX=%d", XPQ_IDX));
#endif
}

void
xen_invlpg(vm_offset_t va)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_INVLPG_ALL;
	op.arg1.linear_addr = va & ~PAGE_MASK;
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

inline void
xen_load_cr3(u_long val)
{
	xen_pt_switch(val);
}

void
xen_pt_switch(vm_paddr_t kpml4phys)
{
	struct mmuext_op op;
#ifdef INVARIANTS
	SET_VCPU();
	
	KASSERT(XPQ_IDX == 0, ("pending operations XPQ_IDX=%d", XPQ_IDX));
#endif
	op.cmd = MMUEXT_NEW_BASEPTR;
	op.arg1.mfn = xpmap_ptom(kpml4phys) >> PAGE_SHIFT;
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void
_xen_machphys_update(vm_paddr_t mfn, vm_paddr_t pfn, char *file, int line)
{
	SET_VCPU();
	
	if (__predict_true(gdtset))
		critical_enter();
	XPQ_QUEUE[XPQ_IDX].ptr = (mfn << PAGE_SHIFT) | MMU_MACHPHYS_UPDATE;
	XPQ_QUEUE[XPQ_IDX].val = pfn;
#ifdef INVARIANTS
	XPQ_QUEUE_LOG[XPQ_IDX].file = file;
	XPQ_QUEUE_LOG[XPQ_IDX].line = line;	
#endif		
	xen_increment_idx();
	if (__predict_true(gdtset))
		critical_exit();
}

void
_xen_queue_pt_update(vm_paddr_t ptr, vm_paddr_t val, char *file, int line)
{
	SET_VCPU();

#ifdef revisit
	if (__predict_true(gdtset))	
		mtx_assert(&vm_page_queue_mtx, MA_OWNED);
#endif

	KASSERT((ptr & 7) == 0, ("misaligned update"));
	
	if (__predict_true(gdtset))
		critical_enter();
	
	XPQ_QUEUE[XPQ_IDX].ptr = ((uint64_t)ptr) | MMU_NORMAL_PT_UPDATE;
	XPQ_QUEUE[XPQ_IDX].val = (uint64_t)val;
#ifdef INVARIANTS
	XPQ_QUEUE_LOG[XPQ_IDX].file = file;
	XPQ_QUEUE_LOG[XPQ_IDX].line = line;	
#endif	
	xen_increment_idx();
	if (__predict_true(gdtset))
		critical_exit();
}

void 
xen_pgdir_pin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_PIN_L4_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pgdir_unpin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_UNPIN_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pgdpt_pin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_PIN_L3_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pgdpt_unpin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_UNPIN_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pgd_pin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_PIN_L2_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pgd_unpin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_UNPIN_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pt_pin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_PIN_L1_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_pt_unpin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_UNPIN_TABLE;
	op.arg1.mfn = ma >> PAGE_SHIFT;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void 
xen_set_ldt(vm_paddr_t ptr, unsigned long len)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_SET_LDT;
	op.arg1.linear_addr = ptr;
	op.arg2.nr_ents = len;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void xen_tlb_flush(void)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_TLB_FLUSH_LOCAL;
	xen_flush_queue();
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}
