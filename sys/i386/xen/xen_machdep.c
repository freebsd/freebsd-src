/*
 *
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2004-2006 Kip Macy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian Limpach.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/sysproto.h>


#include <machine/xen/xen-os.h>


#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/segments.h>
#include <machine/pcb.h>
#include <machine/stdarg.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/asmacros.h>



#include <machine/xen/hypervisor.h>
#include <machine/xen/xenvar.h>
#include <machine/xen/xenfunc.h>
#include <machine/xen/xenpmap.h>
#include <machine/xen/xenbus.h>
#include <machine/xen/xenfunc.h>
#include <xen/interface/memory.h>
#include <machine/xen/features.h>
#ifdef SMP
#include <machine/privatespace.h>
#endif


#include <vm/vm_page.h>


#define	IDTVEC(name)	__CONCAT(X,name)

extern inthand_t
IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(xmm), IDTVEC(lcall_syscall), IDTVEC(int0x80_syscall);


int xendebug_flags; 
start_info_t *xen_start_info;
shared_info_t *HYPERVISOR_shared_info;
xen_pfn_t *xen_machine_phys = machine_to_phys_mapping;
xen_pfn_t *xen_phys_machine;
int preemptable, init_first;
extern unsigned int avail_space;

void ni_cli(void);
void ni_sti(void);


void
ni_cli(void)
{
	__asm__("pushl %edx;"
		"pushl %eax;"
		);
	__cli();
	__asm__("popl %eax;"
		"popl %edx;"
		);
}


void
ni_sti(void)
{
	__asm__("pushl %edx;"
		"pushl %esi;"
		"pushl %eax;"
		);
	__sti();
	__asm__("popl %eax;"
		"popl %esi;"
		"popl %edx;"
		);
}

/*
 * Modify the cmd_line by converting ',' to NULLs so that it is in a  format 
 * suitable for the static env vars.
 */
char *
xen_setbootenv(char *cmd_line)
{
	char *cmd_line_next;
    
        /* Skip leading spaces */
        for (; *cmd_line == ' '; cmd_line++);

	printk("xen_setbootenv(): cmd_line='%s'\n", cmd_line);

	for (cmd_line_next = cmd_line; strsep(&cmd_line_next, ",") != NULL;);
	return cmd_line;
}

static struct 
{
	const char	*ev;
	int		mask;
} howto_names[] = {
	{"boot_askname",	RB_ASKNAME},
	{"boot_single",	RB_SINGLE},
	{"boot_nosync",	RB_NOSYNC},
	{"boot_halt",	RB_ASKNAME},
	{"boot_serial",	RB_SERIAL},
	{"boot_cdrom",	RB_CDROM},
	{"boot_gdb",	RB_GDB},
	{"boot_gdb_pause",	RB_RESERVED1},
	{"boot_verbose",	RB_VERBOSE},
	{"boot_multicons",	RB_MULTIPLE},
	{NULL,	0}
};

int 
xen_boothowto(char *envp)
{
	int i, howto = 0;

	/* get equivalents from the environment */
	for (i = 0; howto_names[i].ev != NULL; i++)
		if (getenv(howto_names[i].ev) != NULL)
			howto |= howto_names[i].mask;
	return howto;
}

#define PRINTK_BUFSIZE 1024
void
printk(const char *fmt, ...)
{
        __va_list ap;
        int retval;
        static char buf[PRINTK_BUFSIZE];

        va_start(ap, fmt);
        retval = vsnprintf(buf, PRINTK_BUFSIZE - 1, fmt, ap);
        va_end(ap);
        buf[retval] = 0;
        (void)HYPERVISOR_console_write(buf, retval);
}


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

#define XPQ_QUEUE xpq_queue[vcpu]
#define XPQ_IDX xpq_idx[vcpu]
#define SET_VCPU() int vcpu = smp_processor_id()


#define XPQ_QUEUE_LOG xpq_queue_log[vcpu]
#else
	
static mmu_update_t xpq_queue[XPQUEUE_SIZE];
static struct mmu_log xpq_queue_log[XPQUEUE_SIZE];
static int xpq_idx = 0;

#define XPQ_QUEUE xpq_queue
#define XPQ_IDX xpq_idx
#define SET_VCPU()
#endif /* !SMP */

#define XPQ_IDX_INC atomic_add_int(&XPQ_IDX, 1);

#if 0
static void
xen_dump_queue(void)
{
	int _xpq_idx = XPQ_IDX;
	int i;

	if (_xpq_idx <= 1)
		return;

	printk("xen_dump_queue(): %u entries\n", _xpq_idx);
	for (i = 0; i < _xpq_idx; i++) {
		printk(" val: %llx ptr: %llx\n", XPQ_QUEUE[i].val, XPQ_QUEUE[i].ptr);
	}
}
#endif


static __inline void
_xen_flush_queue(void)
{
	SET_VCPU();
	int _xpq_idx = XPQ_IDX;
	int error, i;
	/* window of vulnerability here? */

	if (__predict_true(gdtset))
		critical_enter();
	XPQ_IDX = 0;
	/* Make sure index is cleared first to avoid double updates. */
	error = HYPERVISOR_mmu_update((mmu_update_t *)&XPQ_QUEUE,
				      _xpq_idx, NULL, DOMID_SELF);
    
#if 0
	if (__predict_true(gdtset))
	for (i = _xpq_idx; i > 0;) {
		if (i >= 3) {
			CTR6(KTR_PMAP, "mmu:val: %lx ptr: %lx val: %lx ptr: %lx val: %lx ptr: %lx",
			    (XPQ_QUEUE[i-1].val & 0xffffffff), (XPQ_QUEUE[i-1].ptr & 0xffffffff),
			    (XPQ_QUEUE[i-2].val & 0xffffffff), (XPQ_QUEUE[i-2].ptr & 0xffffffff),
			    (XPQ_QUEUE[i-3].val & 0xffffffff), (XPQ_QUEUE[i-3].ptr & 0xffffffff));
			    i -= 3;
		} else if (i == 2) {
			CTR4(KTR_PMAP, "mmu: val: %lx ptr: %lx val: %lx ptr: %lx",
			    (XPQ_QUEUE[i-1].val & 0xffffffff), (XPQ_QUEUE[i-1].ptr & 0xffffffff),
			    (XPQ_QUEUE[i-2].val & 0xffffffff), (XPQ_QUEUE[i-2].ptr & 0xffffffff));
			i = 0;
		} else {
			CTR2(KTR_PMAP, "mmu: val: %lx ptr: %lx", 
			    (XPQ_QUEUE[i-1].val & 0xffffffff), (XPQ_QUEUE[i-1].ptr & 0xffffffff));
			i = 0;
		}
	}
#endif	
	if (__predict_true(gdtset))
		critical_exit();
	if (__predict_false(error < 0)) {
		for (i = 0; i < _xpq_idx; i++)
			printf("val: %llx ptr: %llx\n", XPQ_QUEUE[i].val, XPQ_QUEUE[i].ptr);
		panic("Failed to execute MMU updates: %d", error);
	}

}

void
xen_flush_queue(void)
{
	SET_VCPU();
	if (XPQ_IDX != 0) _xen_flush_queue();
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

void
xen_load_cr3(u_int val)
{
	struct mmuext_op op;
#ifdef INVARIANTS
	SET_VCPU();
	
	KASSERT(XPQ_IDX == 0, ("pending operations XPQ_IDX=%d", XPQ_IDX));
#endif
	op.cmd = MMUEXT_NEW_BASEPTR;
	op.arg1.mfn = xpmap_ptom(val) >> PAGE_SHIFT;
	PANIC_IF(HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF) < 0);
}

void
xen_restore_flags(u_int eflags)
{

	__restore_flags(eflags);
}

int
xen_save_and_cli(void)
{
	int eflags;
	
	__save_and_cli(eflags);
	return (eflags);
}

void
xen_cli(void)
{
	__cli();
}

void
xen_sti(void)
{
	__sti();
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

	if (__predict_true(gdtset))	
		mtx_assert(&vm_page_queue_mtx, MA_OWNED);

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
xen_pgdpt_pin(vm_paddr_t ma)
{
	struct mmuext_op op;
	op.cmd = MMUEXT_PIN_L3_TABLE;
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
	printk("xen_pt_pin(): mfn=%x\n", op.arg1.mfn);
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

void
xen_update_descriptor(union descriptor *table, union descriptor *entry)
{
	vm_paddr_t pa;
	pt_entry_t *ptp;

	ptp = vtopte((vm_offset_t)table);
	pa = (*ptp & PG_FRAME) | ((vm_offset_t)table & PAGE_MASK);
	if (HYPERVISOR_update_descriptor(pa, *(uint64_t *)entry))
		panic("HYPERVISOR_update_descriptor failed\n");
}


#if 0
/*
 * Bitmap is indexed by page number. If bit is set, the page is part of a
 * xen_create_contiguous_region() area of memory.
 */
unsigned long *contiguous_bitmap;

static void 
contiguous_bitmap_set(unsigned long first_page, unsigned long nr_pages)
{
	unsigned long start_off, end_off, curr_idx, end_idx;

	curr_idx  = first_page / BITS_PER_LONG;
	start_off = first_page & (BITS_PER_LONG-1);
	end_idx   = (first_page + nr_pages) / BITS_PER_LONG;
	end_off   = (first_page + nr_pages) & (BITS_PER_LONG-1);

	if (curr_idx == end_idx) {
		contiguous_bitmap[curr_idx] |=
			((1UL<<end_off)-1) & -(1UL<<start_off);
	} else {
		contiguous_bitmap[curr_idx] |= -(1UL<<start_off);
		while ( ++curr_idx < end_idx )
			contiguous_bitmap[curr_idx] = ~0UL;
		contiguous_bitmap[curr_idx] |= (1UL<<end_off)-1;
	}
}

static void 
contiguous_bitmap_clear(unsigned long first_page, unsigned long nr_pages)
{
	unsigned long start_off, end_off, curr_idx, end_idx;

	curr_idx  = first_page / BITS_PER_LONG;
	start_off = first_page & (BITS_PER_LONG-1);
	end_idx   = (first_page + nr_pages) / BITS_PER_LONG;
	end_off   = (first_page + nr_pages) & (BITS_PER_LONG-1);

	if (curr_idx == end_idx) {
		contiguous_bitmap[curr_idx] &=
			-(1UL<<end_off) | ((1UL<<start_off)-1);
	} else {
		contiguous_bitmap[curr_idx] &= (1UL<<start_off)-1;
		while ( ++curr_idx != end_idx )
			contiguous_bitmap[curr_idx] = 0;
		contiguous_bitmap[curr_idx] &= -(1UL<<end_off);
	}
}
#endif

/* Ensure multi-page extents are contiguous in machine memory. */
int 
xen_create_contiguous_region(vm_page_t pages, int npages)
{
	unsigned long  mfn, i, flags;
	int order;
	struct xen_memory_reservation reservation = {
		.nr_extents   = 1,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};
	set_xen_guest_handle(reservation.extent_start, &mfn);

	
	balloon_lock(flags);

	/* can currently only handle power of two allocation */
	PANIC_IF(ffs(npages) != fls(npages));

	/* 0. determine order */
	order = (ffs(npages) == fls(npages)) ? fls(npages) - 1 : fls(npages);
	
	/* 1. give away machine pages. */
	for (i = 0; i < (1 << order); i++) {
		int pfn;
		pfn = VM_PAGE_TO_PHYS(&pages[i]) >> PAGE_SHIFT;
		mfn = PFNTOMFN(pfn);
		PFNTOMFN(pfn) = INVALID_P2M_ENTRY;
		PANIC_IF(HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation) != 1);
	}


	/* 2. Get a new contiguous memory extent. */
	reservation.extent_order = order;
	/* xenlinux hardcodes this because of aacraid - maybe set to 0 if we're not 
	 * running with a broxen driver XXXEN
	 */
	reservation.address_bits = 31; 
	if (HYPERVISOR_memory_op(XENMEM_increase_reservation, &reservation) != 1)
		goto fail;

	/* 3. Map the new extent in place of old pages. */
	for (i = 0; i < (1 << order); i++) {
		int pfn;
		pfn = VM_PAGE_TO_PHYS(&pages[i]) >> PAGE_SHIFT;
		xen_machphys_update(mfn+i, pfn);
		PFNTOMFN(pfn) = mfn+i;
	}

	xen_tlb_flush();

#if 0
	contiguous_bitmap_set(VM_PAGE_TO_PHYS(&pages[0]) >> PAGE_SHIFT, 1UL << order);
#endif

	balloon_unlock(flags);

	return 0;

 fail:
	reservation.extent_order = 0;
	reservation.address_bits = 0;

	for (i = 0; i < (1 << order); i++) {
		int pfn;
		pfn = VM_PAGE_TO_PHYS(&pages[i]) >> PAGE_SHIFT;
		PANIC_IF(HYPERVISOR_memory_op(
			XENMEM_increase_reservation, &reservation) != 1);
		xen_machphys_update(mfn, pfn);
		PFNTOMFN(pfn) = mfn;
	}

	xen_tlb_flush();

	balloon_unlock(flags);

	return ENOMEM;
}

void 
xen_destroy_contiguous_region(void *addr, int npages)
{
	unsigned long  mfn, i, flags, order, pfn0;
	struct xen_memory_reservation reservation = {
		.nr_extents   = 1,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};
	set_xen_guest_handle(reservation.extent_start, &mfn);
	
	pfn0 = vtophys(addr) >> PAGE_SHIFT;
#if 0
	scrub_pages(vstart, 1 << order);
#endif
	/* can currently only handle power of two allocation */
	PANIC_IF(ffs(npages) != fls(npages));

	/* 0. determine order */
	order = (ffs(npages) == fls(npages)) ? fls(npages) - 1 : fls(npages);

	balloon_lock(flags);

#if 0
	contiguous_bitmap_clear(vtophys(addr) >> PAGE_SHIFT, 1UL << order);
#endif

	/* 1. Zap current PTEs, giving away the underlying pages. */
	for (i = 0; i < (1 << order); i++) {
		int pfn;
		uint64_t new_val = 0;
		pfn = vtomach((char *)addr + i*PAGE_SIZE) >> PAGE_SHIFT;

		PANIC_IF(HYPERVISOR_update_va_mapping((vm_offset_t)((char *)addr + (i * PAGE_SIZE)), new_val, 0));
		PFNTOMFN(pfn) = INVALID_P2M_ENTRY;
		PANIC_IF(HYPERVISOR_memory_op(
			XENMEM_decrease_reservation, &reservation) != 1);
	}

	/* 2. Map new pages in place of old pages. */
	for (i = 0; i < (1 << order); i++) {
		int pfn;
		uint64_t new_val;
		pfn = pfn0 + i;
		PANIC_IF(HYPERVISOR_memory_op(XENMEM_increase_reservation, &reservation) != 1);
		
		new_val = mfn << PAGE_SHIFT;
		PANIC_IF(HYPERVISOR_update_va_mapping((vm_offset_t)addr + (i * PAGE_SIZE), 
						      new_val, PG_KERNEL));
		xen_machphys_update(mfn, pfn);
		PFNTOMFN(pfn) = mfn;
	}

	xen_tlb_flush();

	balloon_unlock(flags);
}

extern unsigned long cpu0prvpage;
extern unsigned long *SMPpt;
extern  struct user	*proc0uarea;
extern  vm_offset_t	proc0kstack;
extern int vm86paddr, vm86phystk;
char *bootmem_start, *bootmem_current, *bootmem_end; 

pteinfo_t *pteinfo_list;
void initvalues(start_info_t *startinfo);

struct ringbuf_head *xen_store; /* XXX move me */
char *console_page;

void *
bootmem_alloc(unsigned int size) 
{
	char *retptr;
	
	retptr = bootmem_current;
	PANIC_IF(retptr + size > bootmem_end);
	bootmem_current += size;

	return retptr;
}

void 
bootmem_free(void *ptr, unsigned int size) 
{
	char *tptr;
	
	tptr = ptr;
	PANIC_IF(tptr != bootmem_current - size ||
		bootmem_current - size < bootmem_start);	

	bootmem_current -= size;
}

#if 0
static vm_paddr_t
xpmap_mtop2(vm_paddr_t mpa)
{
        return ((machine_to_phys_mapping[mpa >> PAGE_SHIFT] << PAGE_SHIFT)
            ) | (mpa & ~PG_FRAME);
}

static pd_entry_t 
xpmap_get_bootpde(vm_paddr_t va)
{

        return ((pd_entry_t *)xen_start_info->pt_base)[va >> 22];
}

static pd_entry_t
xpmap_get_vbootpde(vm_paddr_t va)
{
        pd_entry_t pde;

        pde = xpmap_get_bootpde(va);
        if ((pde & PG_V) == 0)
                return (pde & ~PG_FRAME);
        return (pde & ~PG_FRAME) |
                (xpmap_mtop2(pde & PG_FRAME) + KERNBASE);
}

static pt_entry_t 8*
xpmap_get_bootptep(vm_paddr_t va)
{
        pd_entry_t pde;

        pde = xpmap_get_vbootpde(va);
        if ((pde & PG_V) == 0)
                return (void *)-1;
#define PT_MASK         0x003ff000      /* page table address bits */
        return &(((pt_entry_t *)(pde & PG_FRAME))[(va & PT_MASK) >> PAGE_SHIFT]);
}

static pt_entry_t
xpmap_get_bootpte(vm_paddr_t va)
{

        return xpmap_get_bootptep(va)[0];
}
#endif


#ifdef ADD_ISA_HOLE
static void
shift_phys_machine(unsigned long *phys_machine, int nr_pages)
{

        unsigned long *tmp_page, *current_page, *next_page;
	int i;

	tmp_page = bootmem_alloc(PAGE_SIZE);
	current_page = phys_machine + nr_pages - (PAGE_SIZE/sizeof(unsigned long));  
	next_page = current_page - (PAGE_SIZE/sizeof(unsigned long));  
	bcopy(phys_machine, tmp_page, PAGE_SIZE);

	while (current_page > phys_machine) { 
	        /*  save next page */
	        bcopy(next_page, tmp_page, PAGE_SIZE);
	        /* shift down page */
		bcopy(current_page, next_page, PAGE_SIZE);
	        /*  finish swap */
	        bcopy(tmp_page, current_page, PAGE_SIZE);
	  
		current_page -= (PAGE_SIZE/sizeof(unsigned long));
		next_page -= (PAGE_SIZE/sizeof(unsigned long));
	}
	bootmem_free(tmp_page, PAGE_SIZE);	
	
	for (i = 0; i < nr_pages; i++) {
	        xen_machphys_update(phys_machine[i], i);
	}
	memset(phys_machine, INVALID_P2M_ENTRY, PAGE_SIZE);

}
#endif /* ADD_ISA_HOLE */

extern unsigned long physfree;
void
initvalues(start_info_t *startinfo)
{ 
	int l3_pages, l2_pages, l1_pages, offset;
	vm_offset_t cur_space;
	struct physdev_set_iopl set_iopl;
	
	vm_paddr_t KPTphys, IdlePTDma;
	vm_paddr_t console_page_ma, xen_store_ma;
	vm_offset_t KPTphysoff, tmpva;
	vm_paddr_t shinfo;
#ifdef PAE
	vm_paddr_t IdlePDPTma, IdlePDPTnewma;
	vm_paddr_t IdlePTDnewma[4];
	pd_entry_t *IdlePDPTnew, *IdlePTDnew;
#else
	vm_paddr_t pdir_shadow_ma;
#endif
	unsigned long i;


	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_writable_pagetables);
	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_4gb_segments);	
#ifdef notyet
	/*
	 * need to install handler
	 */
	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_4gb_segments_notify);	
#endif	
	xen_start_info = startinfo;
	xen_phys_machine = (xen_pfn_t *)startinfo->mfn_list;

	/* number of pages allocated after the pts + 1*/;
	cur_space = xen_start_info->pt_base +
	    ((xen_start_info->nr_pt_frames) + 3 )*PAGE_SIZE;
	printk("initvalues(): wooh - availmem=%x,%x\n", avail_space, cur_space);

	printk("KERNBASE=%x,pt_base=%x, VTOPFN(base)=%x, nr_pt_frames=%x\n", KERNBASE,xen_start_info->pt_base, VTOPFN(xen_start_info->pt_base), xen_start_info->nr_pt_frames);
	xendebug_flags = 0; /* 0xffffffff; */

	/* allocate 4 pages for bootmem allocator */
	bootmem_start = bootmem_current = (char *)cur_space;
	cur_space += (4 * PAGE_SIZE);
	bootmem_end = (char *)cur_space;
	
#ifdef ADD_ISA_HOLE
	shift_phys_machine(xen_phys_machine, xen_start_info->nr_pages);
#endif
	/* 
	 * pre-zero unused mapped pages - mapped on 4MB boundary
	 */
/*
	bzero((char *)cur_space, (cur_space + 0x3fffff) % 0x400000);
 */

#ifdef PAE
	IdlePDPT = (pd_entry_t *)startinfo->pt_base;
	IdlePDPTma = xpmap_ptom(VTOP(startinfo->pt_base));
	IdlePTD = (pd_entry_t *)((uint8_t *)startinfo->pt_base + PAGE_SIZE);
	IdlePTDma = xpmap_ptom(VTOP(IdlePTD));
	l3_pages = 1;
#else	
	IdlePTD = (pd_entry_t *)startinfo->pt_base;
	IdlePTDma = xpmap_ptom(VTOP(startinfo->pt_base));
	l3_pages = 0;
#endif
	l2_pages = 1;
	l1_pages = 4; /* XXX not certain if this varies */
	KPTphysoff = (l2_pages + l3_pages)*PAGE_SIZE;
	
	KPTphys = xpmap_ptom(VTOP(startinfo->pt_base + KPTphysoff));
	XENPRINTF("IdlePTD %p\n", IdlePTD);
	XENPRINTF("nr_pages: %ld shared_info: 0x%lx flags: 0x%lx pt_base: 0x%lx "
		  "mod_start: 0x%lx mod_len: 0x%lx\n",
		  xen_start_info->nr_pages, xen_start_info->shared_info, 
		  xen_start_info->flags, xen_start_info->pt_base, 
		  xen_start_info->mod_start, xen_start_info->mod_len);
	/* Map proc0's KSTACK */

	proc0kstack = cur_space; cur_space += (KSTACK_PAGES * PAGE_SIZE);
	printk("proc0kstack=%u\n", proc0kstack);
    
	/* vm86/bios stack */
	cur_space += PAGE_SIZE;

	/* Map space for the vm86 region */
	vm86paddr = (vm_offset_t)cur_space;
	cur_space += (PAGE_SIZE * 3);

#ifdef PAE
	IdlePDPTnew = (pd_entry_t *)cur_space; cur_space += PAGE_SIZE;
	bzero(IdlePDPTnew, PAGE_SIZE);

	IdlePDPTnewma =  xpmap_ptom(VTOP(IdlePDPTnew));
	IdlePTDnew = (pd_entry_t *)cur_space; cur_space += 4*PAGE_SIZE;
	bzero(IdlePTDnew, 4*PAGE_SIZE);

	for (i = 0; i < 4; i++) 
		IdlePTDnewma[i] =
		    xpmap_ptom(VTOP((uint8_t *)IdlePTDnew + i*PAGE_SIZE));
	/*
	 * L3
	 *
	 * Copy the 4 machine addresses of the new PTDs in to the PDPT
	 * 
	 */
	for (i = 0; i < 4; i++)
		IdlePDPTnew[i] = IdlePTDnewma[i] | PG_V;

	__asm__("nop;");
	/*
	 *
	 * re-map the new PDPT read-only
	 */
	PT_SET_MA(IdlePDPTnew, IdlePDPTnewma | PG_V);
	/*
	 * 
	 * Unpin the current PDPT
	 */
	xen_pt_unpin(IdlePDPTma);
#endif  /* PAE */
	
	/* unmap remaining pages from initial 4MB chunk */
	for (tmpva = cur_space; (tmpva & ((1<<22)-1)) != 0; tmpva += PAGE_SIZE) {
		bzero((char *)tmpva, PAGE_SIZE);
		PT_SET_MA(tmpva, (vm_paddr_t)0);
	}
	
#ifdef PAE
	offset = 0;
#else	
	offset = KPTDI;
#endif	

	/* allocate remainder of NKPT pages */
	for (i = l1_pages; i < NKPT; i++, cur_space += PAGE_SIZE) {
		/*
		 * ensure that all page table pages have been zeroed
		 */
		PT_SET_MA(cur_space, xpmap_ptom(VTOP(cur_space)) | PG_V | PG_RW);
		bzero((char *)cur_space, PAGE_SIZE);
		PT_SET_MA(cur_space, 0);
		    
		xen_pt_pin(xpmap_ptom(VTOP(cur_space)));
		xen_queue_pt_update((vm_paddr_t)(IdlePTDma + (offset + i)*sizeof(vm_paddr_t)), 
		    xpmap_ptom(VTOP(cur_space)) | PG_KERNEL);
	}
	
	PT_UPDATES_FLUSH();
	memcpy((uint8_t *)IdlePTDnew + 3*PAGE_SIZE, IdlePTD, PAGE_SIZE/2);
	printk("do remapping\n");
	for (i = 0; i < 4; i++) {
		PT_SET_MA((uint8_t *)IdlePTDnew + i*PAGE_SIZE,
		    IdlePTDnewma[i] | PG_V);
	}
	xen_load_cr3(VTOP(IdlePDPTnew));
	xen_pgdpt_pin(xpmap_ptom(VTOP(IdlePDPTnew)));
	for (i = 0; i < 4; i++) {
		xen_queue_pt_update((vm_paddr_t)(IdlePTDnewma[2] + (PTDPTDI - 1024 + i)*sizeof(vm_paddr_t)), 
		    IdlePTDnewma[i] | PG_V);
	}

	/* copy	NKPT pages */
	for (i = 0; i < NKPT; i++) {
		xen_queue_pt_update(
			(vm_paddr_t)(IdlePTDnewma[3] + (i)*sizeof(vm_paddr_t)), 
			    IdlePTD[i]);
	}
	PT_UPDATES_FLUSH();
	
	IdlePTD = IdlePTDnew;
	IdlePDPT = IdlePDPTnew;
	IdlePDPTma = IdlePDPTnewma;
	
	/* allocate page for gdt */
	gdt = (union descriptor *)cur_space; cur_space += PAGE_SIZE;
	/* allocate page for ldt */
	ldt = (union descriptor *)cur_space; cur_space += PAGE_SIZE;

	HYPERVISOR_shared_info = (shared_info_t *)cur_space;
	cur_space += PAGE_SIZE;

	/*
	 * shared_info is an unsigned long so this will randomly break if
	 * it is allocated above 4GB - I guess people are used to that
	 * sort of thing with Xen ... sigh
	 */
	shinfo = xen_start_info->shared_info;
	PT_SET_MA(HYPERVISOR_shared_info, shinfo | PG_KERNEL);
	
	printk("#4\n");

	xen_store = (struct ringbuf_head *)cur_space;
	cur_space += PAGE_SIZE;
	
	xen_store_ma = (((vm_paddr_t)xen_start_info->store_mfn) << PAGE_SHIFT);
	PT_SET_MA(xen_store, xen_store_ma | PG_KERNEL);
	console_page = (char *)cur_space;
	cur_space += PAGE_SIZE;
	console_page_ma = (((vm_paddr_t)xen_start_info->console.domU.mfn) << PAGE_SHIFT);
	PT_SET_MA(console_page, console_page_ma | PG_KERNEL);

	printk("#5\n");
	HYPERVISOR_shared_info->arch.pfn_to_mfn_frame_list_list = (unsigned long)xen_phys_machine;
#if 0 && defined(SMP)
	for (i = 0; i < ncpus; i++) {
		int j, npages = (sizeof(struct privatespace) + 1)/PAGE_SIZE;

		for (j = 0; j < npages; j++) {
			vm_paddr_t ma = xpmap_ptom(cur_space);
			cur_space += PAGE_SIZE;
			PT_SET_VA_MA(SMPpt + i*npages + j, ma | PG_KERNEL, FALSE);
		}
	}
	xen_flush_queue();
#endif

	set_iopl.iopl = 1;
	PANIC_IF(HYPERVISOR_physdev_op(PHYSDEVOP_SET_IOPL, &set_iopl));
	printk("#6\n");
#if 0
	/* add page table for KERNBASE */
	xen_queue_pt_update(IdlePTDma + KPTDI*sizeof(vm_paddr_t), 
			    xpmap_ptom(VTOP(cur_space) | PG_KERNEL));
	xen_flush_queue();
#ifdef PAE	
	xen_queue_pt_update(pdir_shadow_ma[3] + KPTDI*sizeof(vm_paddr_t), 
			    xpmap_ptom(VTOP(cur_space) | PG_V | PG_A));
#else
	xen_queue_pt_update(pdir_shadow_ma + KPTDI*sizeof(vm_paddr_t), 
			    xpmap_ptom(VTOP(cur_space) | PG_V | PG_A));
#endif	
	xen_flush_queue();
	cur_space += PAGE_SIZE;
	printk("#6\n");
#endif /* 0 */	
#ifdef notyet
	if (xen_start_info->flags & SIF_INITDOMAIN) {
		/* Map first megabyte */
		for (i = 0; i < (256 << PAGE_SHIFT); i += PAGE_SIZE) 
			PT_SET_MA(KERNBASE + i, i | PG_KERNEL | PG_NC_PCD);
		xen_flush_queue();
	}
#endif
	/*
	 * re-map kernel text read-only
	 *
	 */
	for (i = (((vm_offset_t)&btext) & ~PAGE_MASK);
	     i < (((vm_offset_t)&etext) & ~PAGE_MASK); i += PAGE_SIZE)
		PT_SET_MA(i, xpmap_ptom(VTOP(i)) | PG_V | PG_A);
	
	printk("#7\n");
	physfree = VTOP(cur_space);
	init_first = physfree >> PAGE_SHIFT;
	IdlePTD = (pd_entry_t *)VTOP(IdlePTD);
	IdlePDPT = (pd_entry_t *)VTOP(IdlePDPT);
	setup_xen_features();
	printk("#8, proc0kstack=%u\n", proc0kstack);
}


trap_info_t trap_table[] = {
	{ 0,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(div)},
	{ 1,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(dbg)},
	{ 3,   3, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(bpt)},
	{ 4,   3, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(ofl)},
	/* This is UPL on Linux and KPL on BSD */
	{ 5,   3, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(bnd)},
	{ 6,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(ill)},
	{ 7,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(dna)},
	/*
	 * { 8,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(XXX)},
	 *   no handler for double fault
	 */
	{ 9,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(fpusegm)},
	{10,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(tss)},
	{11,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(missing)},
	{12,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(stk)},
	{13,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(prot)},
	{14,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(page)},
	{15,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(rsvd)},
	{16,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(fpu)},
	{17,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(align)},
	{18,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(mchk)},
	{19,   0, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(xmm)},
	{0x80, 3, GSEL(GCODE_SEL, SEL_KPL), (unsigned long) &IDTVEC(int0x80_syscall)},
	{  0, 0,           0, 0 }
};


static void 
shutdown_handler(struct xenbus_watch *watch,
		 const char **vec, unsigned int len)
{
	char *str;
	struct xenbus_transaction xbt;
	int err, howto;
	struct reboot_args uap;
	
	howto = 0;

 again:
	err = xenbus_transaction_start(&xbt);
	if (err)
		return;
	str = (char *)xenbus_read(xbt, "control", "shutdown", NULL);
	/* Ignore read errors and empty reads. */
	if (XENBUS_IS_ERR_READ(str)) {
		xenbus_transaction_end(xbt, 1);
		return;
	}

	xenbus_write(xbt, "control", "shutdown", "");

	err = xenbus_transaction_end(xbt, 0);
	if (err == EAGAIN) {
		free(str, M_DEVBUF);
		goto again;
	}

	if (strcmp(str, "reboot") == 0)
		howto = 0;
	else if (strcmp(str, "poweroff") == 0)
		howto |= (RB_POWEROFF | RB_HALT);
	else if (strcmp(str, "halt") == 0)
		howto |= RB_HALT;
	else if (strcmp(str, "suspend") == 0)
		howto = -1;
	else {
		printf("Ignoring shutdown request: %s\n", str);
		goto done;
	}
#ifdef notyet
	if (howto == -1) {
		do_suspend(NULL);
		goto done;
	}
#else 
	if (howto == -1) {
		printf("suspend not currently supported\n");
		goto done;
	}
#endif
	uap.opt = howto;
	reboot(curthread, &uap);
 done:
	free(str, M_DEVBUF);
}

static struct xenbus_watch shutdown_watch = {
	.node = "control/shutdown",
	.callback = shutdown_handler
};


void setup_shutdown_watcher(void *unused);


void
setup_shutdown_watcher(void *unused)
{
	if (register_xenbus_watch(&shutdown_watch))
		printf("Failed to set shutdown watcher\n");
}


SYSINIT(shutdown, SI_SUB_RUN_SCHEDULER, SI_ORDER_ANY, setup_shutdown_watcher, NULL);

#ifdef notyet

static void 
xen_suspend(void *ignore)
{
	int i, j, k, fpp;

	extern void time_resume(void);
	extern unsigned long max_pfn;
	extern unsigned long *pfn_to_mfn_frame_list_list;
	extern unsigned long *pfn_to_mfn_frame_list[];

#ifdef CONFIG_SMP
#error "do_suspend must be run cpu 0 - need to create separate thread"
	cpumask_t prev_online_cpus;
	int vcpu_prepare(int vcpu);
#endif

	int err = 0;

	PANIC_IF(smp_processor_id() != 0);

#if defined(CONFIG_SMP) && !defined(CONFIG_HOTPLUG_CPU)
	if (num_online_cpus() > 1) {
		printk(KERN_WARNING "Can't suspend SMP guests "
		       "without CONFIG_HOTPLUG_CPU\n");
		return -EOPNOTSUPP;
	}
#endif

	xenbus_suspend();

#ifdef CONFIG_SMP
	lock_cpu_hotplug();
	/*
	 * Take all other CPUs offline. We hold the hotplug semaphore to
	 * avoid other processes bringing up CPUs under our feet.
	 */
	cpus_clear(prev_online_cpus);
	while (num_online_cpus() > 1) {
		for_each_online_cpu(i) {
			if (i == 0)
				continue;
			unlock_cpu_hotplug();
			err = cpu_down(i);
			lock_cpu_hotplug();
			if (err != 0) {
				printk(KERN_CRIT "Failed to take all CPUs "
				       "down: %d.\n", err);
				goto out_reenable_cpus;
			}
			cpu_set(i, prev_online_cpus);
		}
	}
#endif /* CONFIG_SMP */

	preempt_disable();


	__cli();
	preempt_enable();
#ifdef SMP
	unlock_cpu_hotplug();
#endif
	gnttab_suspend();

	pmap_kremove(HYPERVISOR_shared_info);

	xen_start_info->store_mfn = mfn_to_pfn(xen_start_info->store_mfn);
	xen_start_info->console.domU.mfn = mfn_to_pfn(xen_start_info->console.domU.mfn);

	/*
	 * We'll stop somewhere inside this hypercall. When it returns,
	 * we'll start resuming after the restore.
	 */
	HYPERVISOR_suspend(VTOMFN(xen_start_info));

	pmap_kenter_ma(HYPERVISOR_shared_info, xen_start_info->shared_info);
	set_fixmap(FIX_SHARED_INFO, xen_start_info->shared_info);

#if 0
	memset(empty_zero_page, 0, PAGE_SIZE);
#endif     
	HYPERVISOR_shared_info->arch.pfn_to_mfn_frame_list_list =
		VTOMFN(pfn_to_mfn_frame_list_list);
  
	fpp = PAGE_SIZE/sizeof(unsigned long);
	for (i = 0, j = 0, k = -1; i < max_pfn; i += fpp, j++) {
		if ((j % fpp) == 0) {
			k++;
			pfn_to_mfn_frame_list_list[k] = 
				VTOMFN(pfn_to_mfn_frame_list[k]);
			j = 0;
		}
		pfn_to_mfn_frame_list[k][j] = 
			VTOMFN(&phys_to_machine_mapping[i]);
	}
	HYPERVISOR_shared_info->arch.max_pfn = max_pfn;

	gnttab_resume();

	irq_resume();

	time_resume();

	__sti();

	xencons_resume();

#ifdef CONFIG_SMP
	for_each_cpu(i)
		vcpu_prepare(i);

#endif

	/* 
	 * Only resume xenbus /after/ we've prepared our VCPUs; otherwise
	 * the VCPU hotplug callback can race with our vcpu_prepare
	 */
	xenbus_resume();

#ifdef CONFIG_SMP
 out_reenable_cpus:
	for_each_cpu_mask(i, prev_online_cpus) {
		j = cpu_up(i);
		if ((j != 0) && !cpu_online(i)) {
			printk(KERN_CRIT "Failed to bring cpu "
			       "%d back up (%d).\n",
			       i, j);
			err = j;
		}
	}
#endif
	return err;
}

#endif /* notyet */
/********** CODE WORTH KEEPING ABOVE HERE *****************/ 

void xen_failsafe_handler(void);

void
xen_failsafe_handler(void)
{

	panic("xen_failsafe_handler called!\n");
}

void xen_handle_thread_switch(struct pcb *pcb);

/* This is called by cpu_switch() when switching threads. */
/* The pcb arg refers to the process control block of the */
/* next thread which is to run */
void
xen_handle_thread_switch(struct pcb *pcb)
{
    uint32_t *a = (uint32_t *)&PCPU_GET(fsgs_gdt)[0];
    uint32_t *b = (uint32_t *)&pcb->pcb_fsd;
    multicall_entry_t mcl[3];
    int i = 0;

    /* Notify Xen of task switch */
    mcl[i].op = __HYPERVISOR_stack_switch;
    mcl[i].args[0] = GSEL(GDATA_SEL, SEL_KPL);
    mcl[i++].args[1] = (unsigned long)pcb;

    /* Check for update of fsd */
    if (*a != *b || *(a+1) != *(b+1)) {
        mcl[i].op = __HYPERVISOR_update_descriptor;
        *(uint64_t *)&mcl[i].args[0] = vtomach((vm_offset_t)a);
        *(uint64_t *)&mcl[i++].args[2] = *(uint64_t *)b;
    }    

    a += 2;
    b += 2;

    /* Check for update of gsd */
    if (*a != *b || *(a+1) != *(b+1)) {
        mcl[i].op = __HYPERVISOR_update_descriptor;
        *(uint64_t *)&mcl[i].args[0] = vtomach((vm_offset_t)a);
        *(uint64_t *)&mcl[i++].args[2] = *(uint64_t *)b;
    }    

    (void)HYPERVISOR_multicall(mcl, i);
}
