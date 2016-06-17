/*
 * linux/kernel/ptrace.c
 *
 * (C) Copyright 1999 Linus Torvalds
 *
 * Common interfaces for "ptrace()" which we do not want
 * to continually duplicate across every architecture.
 */

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/smp_lock.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>

/*
 * Check that we have indeed attached to the thing..
 */
int ptrace_check_attach(struct task_struct *child, int kill)
{

	if (!(child->ptrace & PT_PTRACED))
		return -ESRCH;

	if (child->p_pptr != current)
		return -ESRCH;

	if (!kill) {
		if (child->state != TASK_STOPPED)
			return -ESRCH;
#ifdef CONFIG_SMP
		/* Make sure the child gets off its CPU.. */
		for (;;) {
			task_lock(child);
			if (!task_has_cpu(child))
				break;
			task_unlock(child);
			do {
				if (child->state != TASK_STOPPED)
					return -ESRCH;
				barrier();
				cpu_relax();
			} while (task_has_cpu(child));
		}
		task_unlock(child);
#endif		
	}

	/* All systems go.. */
	return 0;
}

int ptrace_attach(struct task_struct *task)
{
	task_lock(task);
	if (task->pid <= 1)
		goto bad;
	if (task == current)
		goto bad;
	if (!task->mm)
		goto bad;
	if(((current->uid != task->euid) ||
	    (current->uid != task->suid) ||
	    (current->uid != task->uid) ||
 	    (current->gid != task->egid) ||
 	    (current->gid != task->sgid) ||
 	    (!cap_issubset(task->cap_permitted, current->cap_permitted)) ||
 	    (current->gid != task->gid)) && !capable(CAP_SYS_PTRACE))
		goto bad;
	rmb();
	if (!is_dumpable(task) && !capable(CAP_SYS_PTRACE))
		goto bad;
	/* the same process cannot be attached many times */
	if (task->ptrace & PT_PTRACED)
		goto bad;

	/* Go */
	task->ptrace |= PT_PTRACED;
	if (capable(CAP_SYS_PTRACE))
		task->ptrace |= PT_PTRACE_CAP;
	task_unlock(task);

	write_lock_irq(&tasklist_lock);
	if (task->p_pptr != current) {
		REMOVE_LINKS(task);
		task->p_pptr = current;
		SET_LINKS(task);
	}
	write_unlock_irq(&tasklist_lock);

	send_sig(SIGSTOP, task, 1);
	return 0;

bad:
	task_unlock(task);
	return -EPERM;
}

int ptrace_detach(struct task_struct *child, unsigned int data)
{
	if ((unsigned long) data > _NSIG)
		return	-EIO;

	/* Architecture-specific hardware disable .. */
	ptrace_disable(child);

	/* .. re-parent .. */
	child->ptrace = 0;
	child->exit_code = data;
	write_lock_irq(&tasklist_lock);
	REMOVE_LINKS(child);
	child->p_pptr = child->p_opptr;
	SET_LINKS(child);
	write_unlock_irq(&tasklist_lock);

	/* .. and wake it up. */
	wake_up_process(child);
	return 0;
}

/*
 * Access another process' address space.
 * Source/target buffer must be kernel space, 
 * Do not walk the page table directly, use get_user_pages
 */

int access_process_vm(struct task_struct *tsk, unsigned long addr, void *buf, int len, int write)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct page *page;
	void *old_buf = buf;

	/* Worry about races with exit() */
	task_lock(tsk);
	mm = tsk->mm;
	if (mm)
		atomic_inc(&mm->mm_users);
	task_unlock(tsk);
	if (!mm)
		return 0;

	down_read(&mm->mmap_sem);
	/* ignore errors, just check how much was sucessfully transfered */
	while (len) {
		int bytes, ret, offset;
		void *maddr;

		ret = get_user_pages(current, mm, addr, 1,
				write, 1, &page, &vma);
		if (ret <= 0)
			break;

		bytes = len;
		offset = addr & (PAGE_SIZE-1);
		if (bytes > PAGE_SIZE-offset)
			bytes = PAGE_SIZE-offset;

		flush_cache_page(vma, addr);

		maddr = kmap(page);
		if (write) {
			memcpy(maddr + offset, buf, bytes);
			flush_page_to_ram(page);
			flush_icache_user_range(vma, page, addr, len);
			set_page_dirty(page);
		} else {
			memcpy(buf, maddr + offset, bytes);
			flush_page_to_ram(page);
		}
		kunmap(page);
		put_page(page);
		len -= bytes;
		buf += bytes;
		addr += bytes;
	}
	up_read(&mm->mmap_sem);
	mmput(mm);
	
	return buf - old_buf;
}

int ptrace_readdata(struct task_struct *tsk, unsigned long src, char *dst, int len)
{
	int copied = 0;

	while (len > 0) {
		char buf[128];
		int this_len, retval;

		this_len = (len > sizeof(buf)) ? sizeof(buf) : len;
		retval = access_process_vm(tsk, src, buf, this_len, 0);
		if (!retval) {
			if (copied)
				break;
			return -EIO;
		}
		if (copy_to_user(dst, buf, retval))
			return -EFAULT;
		copied += retval;
		src += retval;
		dst += retval;
		len -= retval;			
	}
	return copied;
}

int ptrace_writedata(struct task_struct *tsk, char * src, unsigned long dst, int len)
{
	int copied = 0;

	while (len > 0) {
		char buf[128];
		int this_len, retval;

		this_len = (len > sizeof(buf)) ? sizeof(buf) : len;
		if (copy_from_user(buf, src, this_len))
			return -EFAULT;
		retval = access_process_vm(tsk, dst, buf, this_len, 1);
		if (!retval) {
			if (copied)
				break;
			return -EIO;
		}
		copied += retval;
		src += retval;
		dst += retval;
		len -= retval;			
	}
	return copied;
}
