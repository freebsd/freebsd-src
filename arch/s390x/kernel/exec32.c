/*
 * Support for 32-bit Linux for S390 ELF binaries.
 *
 * Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 * Author(s): Gerhard Tonn (ton@de.ibm.com)
 *
 * Seperated from binfmt_elf32.c to reduce exports for module enablement.
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/mman.h>
#include <linux/a.out.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/spinlock.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif


extern void put_dirty_page(struct task_struct * tsk, struct page *page, unsigned long address);

#undef STACK_TOP
#define STACK_TOP TASK31_SIZE

int setup_arg_pages32(struct linux_binprm *bprm)
{
	unsigned long stack_base;
	struct vm_area_struct *mpnt;
	int i;

	stack_base = STACK_TOP - MAX_ARG_PAGES*PAGE_SIZE;

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
		mpnt->vm_end = STACK_TOP;
		mpnt->vm_page_prot = PAGE_COPY;
		mpnt->vm_flags = VM_STACK_FLAGS;
		mpnt->vm_ops = NULL;
		mpnt->vm_pgoff = 0;
		mpnt->vm_file = NULL;
		mpnt->vm_private_data = (void *) 0;
		insert_vm_struct(current->mm, mpnt);
		current->mm->total_vm = (mpnt->vm_end - mpnt->vm_start) >> PAGE_SHIFT;
	} 

	for (i = 0 ; i < MAX_ARG_PAGES ; i++) {
		struct page *page = bprm->page[i];
		if (page) {
			bprm->page[i] = NULL;
			put_dirty_page(current,page,stack_base);
		}
		stack_base += PAGE_SIZE;
	}
	up_write(&current->mm->mmap_sem);
	
	return 0;
}

EXPORT_SYMBOL(setup_arg_pages32);
