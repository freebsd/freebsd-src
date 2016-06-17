/*
 * linux/arch/x86_64/kernel/ldt.c
 *
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 * Copyright (C) 1999 Ingo Molnar <mingo@redhat.com>
 * Copyright (C) 2002 Andi Kleen
 * 
 * Manage the local descriptor table for user processes.
 * This handles calls from both 32bit and 64bit mode.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/ldt.h>
#include <asm/desc.h>

/*
 * read_ldt() is not really atomic - this is not a problem since
 * synchronization of reads and writes done to the LDT has to be
 * assured by user-space anyway. Writes are atomic, to protect
 * the security checks done on new descriptors.
 */
static int read_ldt(void * ptr, unsigned long bytecount)
{
	int err;
	unsigned long size;
	struct mm_struct * mm = current->mm;

	err = 0;
	if (!mm->context.segments)
		goto out;

	size = LDT_ENTRIES*LDT_ENTRY_SIZE;
	if (size > bytecount)
		size = bytecount;

	err = size;
	if (copy_to_user(ptr, mm->context.segments, size))
		err = -EFAULT;
out:
	return err;
}

static int read_default_ldt(void * ptr, unsigned long bytecount)
{
	/* Arbitary number */ 
	if (bytecount > 128) 
		bytecount = 128; 	
	if (clear_user(ptr, bytecount))
		return -EFAULT;
	return bytecount; 
}

static int write_ldt(void * ptr, unsigned long bytecount, int oldmode)
{
	struct task_struct *me = current;
	struct mm_struct * mm = me->mm;
	__u32 entry_1, entry_2, *lp;
	int error;
	struct modify_ldt_ldt_s ldt_info;

	error = -EINVAL;

	if (bytecount != sizeof(ldt_info))
		goto out;
	error = -EFAULT; 	
	if (copy_from_user(&ldt_info, ptr, bytecount))
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

	/*
	 * the GDT index of the LDT is allocated dynamically, and is
	 * limited by MAX_LDT_DESCRIPTORS.
	 */
	down_write(&mm->mmap_sem);
	if (!mm->context.segments) {
		void * segments = vmalloc(LDT_ENTRIES*LDT_ENTRY_SIZE);
		error = -ENOMEM;
		if (!segments)
			goto out_unlock;
		memset(segments, 0, LDT_ENTRIES*LDT_ENTRY_SIZE);
		wmb();
		mm->context.segments = segments;
		mm->context.cpuvalid = 1UL << smp_processor_id();
		load_LDT(mm);
	}

	lp = (__u32 *) ((ldt_info.entry_number << 3) + (char *) mm->context.segments);

   	/* Allow LDTs to be cleared by the user. */
   	if (ldt_info.base_addr == 0 && ldt_info.limit == 0) {
		if (oldmode ||
		    (ldt_info.contents == 0		&&
		     ldt_info.read_exec_only == 1	&&
		     ldt_info.seg_32bit == 0		&&
		     ldt_info.limit_in_pages == 0	&&
		     ldt_info.seg_not_present == 1	&&
		     ldt_info.useable == 0 && 
		     ldt_info.lm == 0)) {
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
		  (ldt_info.lm << 21) |
		  0x7000;
	if (!oldmode)
		entry_2 |= (ldt_info.useable << 20);

	/* Install the new entry ...  */
install:
	*lp	= entry_1;
	*(lp+1)	= entry_2;
	error = 0;

out_unlock:
	up_write(&mm->mmap_sem);
out:
	return error;
}

asmlinkage long sys_modify_ldt(int func, void *ptr, unsigned long bytecount)
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
