/*
 * linux/kernel/ldt.c
 *
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 * Copyright (C) 1999 Ingo Molnar <mingo@redhat.com>
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/ldt.h>
#include <asm/desc.h>

#ifdef CONFIG_SMP /* avoids "defined but not used" warnig */
static void flush_ldt(void *mm)
{
	if (current->active_mm)
		load_LDT(&current->active_mm->context);
}
#endif

static int alloc_ldt(mm_context_t *pc, int mincount, int reload)
{
	void *oldldt;
	void *newldt;
	int oldsize;

	if (mincount <= pc->size)
		return 0;
	oldsize = pc->size;
	mincount = (mincount+511)&(~511);
	if (mincount*LDT_ENTRY_SIZE > PAGE_SIZE)
		newldt = vmalloc(mincount*LDT_ENTRY_SIZE);
	else
		newldt = kmalloc(mincount*LDT_ENTRY_SIZE, GFP_KERNEL);

	if (!newldt)
		return -ENOMEM;

	if (oldsize)
		memcpy(newldt, pc->ldt, oldsize*LDT_ENTRY_SIZE);

	oldldt = pc->ldt;
	memset(newldt+oldsize*LDT_ENTRY_SIZE, 0, (mincount-oldsize)*LDT_ENTRY_SIZE);
	wmb();
	pc->ldt = newldt;
	pc->size = mincount;
	if (reload) {
		load_LDT(pc);
#ifdef CONFIG_SMP
		if (current->mm->cpu_vm_mask != (1<<smp_processor_id()))
			smp_call_function(flush_ldt, 0, 1, 1);
#endif
	}
	wmb();
	if (oldsize) {
		if (oldsize*LDT_ENTRY_SIZE > PAGE_SIZE)
			vfree(oldldt);
		else
			kfree(oldldt);
	}
	return 0;
}

static inline int copy_ldt(mm_context_t *new, mm_context_t *old)
{
	int err = alloc_ldt(new, old->size, 0);
	if (err < 0) {
		printk(KERN_WARNING "ldt allocation failed\n");
		new->size = 0;
		return err;
	}
	memcpy(new->ldt, old->ldt, old->size*LDT_ENTRY_SIZE);
	return 0;
}

/*
 * we do not have to muck with descriptors here, that is
 * done in switch_mm() as needed.
 */
int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	struct mm_struct * old_mm;
	int retval = 0;

	init_MUTEX(&mm->context.sem);
	mm->context.size = 0;
	old_mm = current->mm;
	if (old_mm && old_mm->context.size > 0) {
		down(&old_mm->context.sem);
		retval = copy_ldt(&mm->context, &old_mm->context);
		up(&old_mm->context.sem);
	}
	return retval;
}

/*
 * No need to lock the MM as we are the last user
 * Do not touch the ldt register, we are already
 * in the next thread.
 */
void destroy_context(struct mm_struct *mm)
{
	if (mm->context.size) {
		if (mm->context.size*LDT_ENTRY_SIZE > PAGE_SIZE)
			vfree(mm->context.ldt);
		else
			kfree(mm->context.ldt);
		mm->context.size = 0;
	}
}

static int read_ldt(void * ptr, unsigned long bytecount)
{
	int err;
	unsigned long size;
	struct mm_struct * mm = current->mm;

	if (!mm->context.size)
		return 0;
	if (bytecount > LDT_ENTRY_SIZE*LDT_ENTRIES)
		bytecount = LDT_ENTRY_SIZE*LDT_ENTRIES;

	down(&mm->context.sem);
	size = mm->context.size*LDT_ENTRY_SIZE;
	if (size > bytecount)
		size = bytecount;

	err = 0;
	if (copy_to_user(ptr, mm->context.ldt, size))
		err = -EFAULT;
	up(&mm->context.sem);
	if (err < 0)
		return err;
	if (size != bytecount) {
		/* zero-fill the rest */
		clear_user(ptr+size, bytecount-size);
	}
	return bytecount;
}

static int read_default_ldt(void * ptr, unsigned long bytecount)
{
	int err;
	unsigned long size;
	void *address;

	err = 0;
	address = &default_ldt[0];
	size = 5*sizeof(struct desc_struct);
	if (size > bytecount)
		size = bytecount;

	err = size;
	if (copy_to_user(ptr, address, size))
		err = -EFAULT;

	return err;
}

static int write_ldt(void * ptr, unsigned long bytecount, int oldmode)
{
	struct mm_struct * mm = current->mm;
	__u32 entry_1, entry_2, *lp;
	int error;
	struct modify_ldt_ldt_s ldt_info;

	error = -EINVAL;
	if (bytecount != sizeof(ldt_info))
		goto out;
	error = -EFAULT; 	
	if (copy_from_user(&ldt_info, ptr, sizeof(ldt_info)))
		goto out;

	error = -EINVAL;
	if (ldt_info.entry_number >= LDT_ENTRIES)
		goto out;
	if (ldt_info.contents == 3) {
		if (oldmode)
			goto out;
		if (ldt_info.seg_not_present == 0)
			goto out;
	}

	down(&mm->context.sem);
	if (ldt_info.entry_number >= mm->context.size) {
		error = alloc_ldt(&current->mm->context, ldt_info.entry_number+1, 1);
		if (error < 0)
			goto out_unlock;
	}

	lp = (__u32 *) ((ldt_info.entry_number << 3) + (char *) mm->context.ldt);

   	/* Allow LDTs to be cleared by the user. */
   	if (ldt_info.base_addr == 0 && ldt_info.limit == 0) {
		if (oldmode ||
		    (ldt_info.contents == 0		&&
		     ldt_info.read_exec_only == 1	&&
		     ldt_info.seg_32bit == 0		&&
		     ldt_info.limit_in_pages == 0	&&
		     ldt_info.seg_not_present == 1	&&
		     ldt_info.useable == 0 )) {
			entry_1 = 0;
			entry_2 = 0;
			goto install;
		}
	}

	entry_1 = ((ldt_info.base_addr & 0x0000ffff) << 16) |
		  (ldt_info.limit & 0x0ffff);
	entry_2 = (ldt_info.base_addr & 0xff000000) |
		  ((ldt_info.base_addr & 0x00ff0000) >> 16) |
		  (ldt_info.limit & 0xf0000) |
		  ((ldt_info.read_exec_only ^ 1) << 9) |
		  (ldt_info.contents << 10) |
		  ((ldt_info.seg_not_present ^ 1) << 15) |
		  (ldt_info.seg_32bit << 22) |
		  (ldt_info.limit_in_pages << 23) |
		  0x7000;
	if (!oldmode)
		entry_2 |= (ldt_info.useable << 20);

	/* Install the new entry ...  */
install:
	*lp	= entry_1;
	*(lp+1)	= entry_2;
	error = 0;

out_unlock:
	up(&mm->context.sem);
out:
	return error;
}

asmlinkage int sys_modify_ldt(int func, void *ptr, unsigned long bytecount)
{
	int ret = -ENOSYS;

	switch (func) {
	case 0:
		ret = read_ldt(ptr, bytecount);
		break;
	case 1:
		ret = write_ldt(ptr, bytecount, 1);
		break;
	case 2:
		ret = read_default_ldt(ptr, bytecount);
		break;
	case 0x11:
		ret = write_ldt(ptr, bytecount, 0);
		break;
	}
	return ret;
}
