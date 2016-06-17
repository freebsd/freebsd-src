/*
 * IA-32 ELF support.
 *
 * Copyright (C) 1999 Arun Sharma <arun.sharma@intel.com>
 * Copyright (C) 2001 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 06/16/00	A. Mallick	initialize csd/ssd/tssd/cflg for ia32_load_state
 * 04/13/01	D. Mosberger	dropped saving tssd in ar.k1---it's not needed
 * 09/14/01	D. Mosberger	fixed memory management for gdt/tss page
 */
#include <linux/config.h>

#include <linux/types.h>

#include <asm/param.h>
#include <asm/signal.h>
#include <asm/ia32.h>

#define CONFIG_BINFMT_ELF32

/* Override some function names */
#undef start_thread
#define start_thread			ia32_start_thread
#define elf_format			elf32_format
#define init_elf_binfmt			init_elf32_binfmt
#define exit_elf_binfmt			exit_elf32_binfmt

#undef CONFIG_BINFMT_ELF
#ifdef CONFIG_BINFMT_ELF32
# define CONFIG_BINFMT_ELF		CONFIG_BINFMT_ELF32
#endif

#undef CONFIG_BINFMT_ELF_MODULE
#ifdef CONFIG_BINFMT_ELF32_MODULE
# define CONFIG_BINFMT_ELF_MODULE	CONFIG_BINFMT_ELF32_MODULE
#endif

#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC	IA32_CLOCKS_PER_SEC

extern void ia64_elf32_init (struct pt_regs *regs);
extern void put_dirty_page (struct task_struct * tsk, struct page *page, unsigned long address);

static void elf32_set_personality (void);

#define ELF_PLAT_INIT(_r, load_addr)	ia64_elf32_init(_r)
#define setup_arg_pages(bprm)		ia32_setup_arg_pages(bprm)
#define elf_map				elf32_map

#undef SET_PERSONALITY
#define SET_PERSONALITY(ex, ibcs2)	elf32_set_personality()

/* Ugly but avoids duplication */
#include "../../../fs/binfmt_elf.c"

extern struct page *ia32_shared_page[];
extern unsigned long *ia32_gdt;

struct page *
ia32_install_shared_page (struct vm_area_struct *vma, unsigned long address, int no_share)
{
	struct page *pg = ia32_shared_page[(address - vma->vm_start)/PAGE_SIZE];

	get_page(pg);
	return pg;
}

static struct vm_operations_struct ia32_shared_page_vm_ops = {
	.nopage =ia32_install_shared_page
};

void
ia64_elf32_init (struct pt_regs *regs)
{
	struct vm_area_struct *vma;

	/*
	 * Map GDT and TSS below 4GB, where the processor can find them.  We need to map
	 * it with privilege level 3 because the IVE uses non-privileged accesses to these
	 * tables.  IA-32 segmentation is used to protect against IA-32 accesses to them.
	 */
	vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (vma) {
		vma->vm_mm = current->mm;
		vma->vm_start = IA32_GDT_OFFSET;
		vma->vm_end = vma->vm_start + max(PAGE_SIZE, 2*IA32_PAGE_SIZE);
		vma->vm_page_prot = PAGE_SHARED;
		vma->vm_flags = VM_READ|VM_MAYREAD;
		vma->vm_ops = &ia32_shared_page_vm_ops;
		vma->vm_pgoff = 0;
		vma->vm_file = NULL;
		vma->vm_private_data = NULL;
		down_write(&current->mm->mmap_sem);
		{
			insert_vm_struct(current->mm, vma);
		}
		up_write(&current->mm->mmap_sem);
	}

	/*
	 * Install LDT as anonymous memory.  This gives us all-zero segment descriptors
	 * until a task modifies them via modify_ldt().
	 */
	vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (vma) {
		vma->vm_mm = current->mm;
		vma->vm_start = IA32_LDT_OFFSET;
		vma->vm_end = vma->vm_start + PAGE_ALIGN(IA32_LDT_ENTRIES*IA32_LDT_ENTRY_SIZE);
		vma->vm_page_prot = PAGE_SHARED;
		vma->vm_flags = VM_READ|VM_WRITE|VM_MAYREAD|VM_MAYWRITE;
		vma->vm_ops = NULL;
		vma->vm_pgoff = 0;
		vma->vm_file = NULL;
		vma->vm_private_data = NULL;
		down_write(&current->mm->mmap_sem);
		{
			insert_vm_struct(current->mm, vma);
		}
		up_write(&current->mm->mmap_sem);
	}

	ia64_psr(regs)->ac = 0;		/* turn off alignment checking */
	regs->loadrs = 0;
	/*
	 *  According to the ABI %edx points to an `atexit' handler.  Since we don't have
	 *  one we'll set it to 0 and initialize all the other registers just to make
	 *  things more deterministic, ala the i386 implementation.
	 */
	regs->r8 = 0;	/* %eax */
	regs->r11 = 0;	/* %ebx */
	regs->r9 = 0;	/* %ecx */
	regs->r10 = 0;	/* %edx */
	regs->r13 = 0;	/* %ebp */
	regs->r14 = 0;	/* %esi */
	regs->r15 = 0;	/* %edi */

	current->thread.eflag = IA32_EFLAG;
	current->thread.fsr = IA32_FSR_DEFAULT;
	current->thread.fcr = IA32_FCR_DEFAULT;
	current->thread.fir = 0;
	current->thread.fdr = 0;

	/*
	 * Setup GDTD.  Note: GDTD is the descrambled version of the pseudo-descriptor
	 * format defined by Figure 3-11 "Pseudo-Descriptor Format" in the IA-32
	 * architecture manual. Also note that the only fields that are not ignored are
	 * `base', `limit', 'G', `P' (must be 1) and `S' (must be 0).
	 */
	regs->r31 = IA32_SEG_UNSCRAMBLE(IA32_SEG_DESCRIPTOR(IA32_GDT_OFFSET, IA32_PAGE_SIZE - 1,
							    0, 0, 0, 1, 0, 0, 0));
	/* Setup the segment selectors */
	regs->r16 = (__USER_DS << 16) | __USER_DS; /* ES == DS, GS, FS are zero */
	regs->r17 = (__USER_DS << 16) | __USER_CS; /* SS, CS; ia32_load_state() sets TSS and LDT */

	ia32_load_segment_descriptors(current);
	ia32_load_state(current);
}

int
ia32_setup_arg_pages (struct linux_binprm *bprm)
{
	unsigned long stack_base;
	struct vm_area_struct *mpnt;
	int i;

	stack_base = IA32_STACK_TOP - MAX_ARG_PAGES*PAGE_SIZE;

	bprm->p += stack_base;
	if (bprm->loader)
		bprm->loader += stack_base;
	bprm->exec += stack_base;

	mpnt = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!mpnt)
		return -ENOMEM;

	down_write(&current->mm->mmap_sem);
	{
		mpnt->vm_mm = current->mm;
		mpnt->vm_start = PAGE_MASK & (unsigned long) bprm->p;
		mpnt->vm_end = IA32_STACK_TOP;
		mpnt->vm_page_prot = PAGE_COPY;
		mpnt->vm_flags = VM_STACK_FLAGS;
		mpnt->vm_ops = NULL;
		mpnt->vm_pgoff = 0;
		mpnt->vm_file = NULL;
		mpnt->vm_private_data = 0;
		insert_vm_struct(current->mm, mpnt);
		current->mm->total_vm = (mpnt->vm_end - mpnt->vm_start) >> PAGE_SHIFT;
	}

	for (i = 0 ; i < MAX_ARG_PAGES ; i++) {
		struct page *page = bprm->page[i];
		if (page) {
			bprm->page[i] = NULL;
			put_dirty_page(current, page, stack_base);
		}
		stack_base += PAGE_SIZE;
	}
	up_write(&current->mm->mmap_sem);

	return 0;
}

static void
elf32_set_personality (void)
{
	set_personality(PER_LINUX32);
	current->thread.map_base  = IA32_PAGE_OFFSET/3;
	current->thread.task_size = IA32_PAGE_OFFSET;	/* use what Linux/x86 uses... */
	current->thread.flags |= IA64_THREAD_XSTACK;	/* data must be executable */
	set_fs(USER_DS);				/* set addr limit for new TASK_SIZE */
}

static unsigned long
elf32_map (struct file *filep, unsigned long addr, struct elf_phdr *eppnt, int prot, int type)
{
	unsigned long pgoff = (eppnt->p_vaddr) & ~IA32_PAGE_MASK;

	return ia32_do_mmap(filep, (addr & IA32_PAGE_MASK), eppnt->p_filesz + pgoff, prot, type,
			    eppnt->p_offset - pgoff);
}
