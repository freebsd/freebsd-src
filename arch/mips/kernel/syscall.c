/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 - 2000 by Ralf Baechle
 * Copyright (C) 2000 Silicon Graphics, Inc.
 *
 * TODO:  Implement the compatibility syscalls.
 *        Don't waste that much memory for empty entries in the syscall
 *        table.
 */
#undef CONF_PRINT_SYSCALLS
#undef CONF_DEBUG_IRIX

#include <linux/config.h>
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/unistd.h>
#include <asm/branch.h>
#include <asm/offset.h>
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <asm/shmparam.h>
#include <asm/uaccess.h>

extern asmlinkage void syscall_trace(void);
typedef asmlinkage int (*syscall_t)(void *a0,...);
extern asmlinkage int (*do_syscalls)(struct pt_regs *regs, syscall_t fun,
				     int narg);
extern syscall_t sys_call_table[];
extern unsigned char sys_narg_table[];

asmlinkage int sys_pipe(struct pt_regs regs)
{
	int fd[2];
	int error, res;

	error = do_pipe(fd);
	if (error) {
		res = error;
		goto out;
	}
	regs.regs[3] = fd[1];
	res = fd[0];
out:
	return res;
}

unsigned long shm_align_mask = PAGE_SIZE - 1;	/* Sane caches */

#define COLOUR_ALIGN(addr,pgoff)				\
	((((addr) + shm_align_mask) & ~shm_align_mask) +	\
	 (((pgoff) << PAGE_SHIFT) & shm_align_mask))

unsigned long arch_get_unmapped_area(struct file *filp, unsigned long addr,
	unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct vm_area_struct * vmm;
	int do_color_align;

	if (flags & MAP_FIXED) {
		/*
		 * We do not accept a shared mapping if it would violate
		 * cache aliasing constraints.
		 */
		if ((flags & MAP_SHARED) && (addr & shm_align_mask))
			return -EINVAL;
		return addr;
	}

	if (len > TASK_SIZE)
		return -ENOMEM;
	do_color_align = 0;
	if (filp || (flags & MAP_SHARED))
		do_color_align = 1;
	if (addr) {
		if (do_color_align)
			addr = COLOUR_ALIGN(addr, pgoff);
		else
			addr = PAGE_ALIGN(addr);
		vmm = find_vma(current->mm, addr);
		if (TASK_SIZE - len >= addr &&
		    (!vmm || addr + len <= vmm->vm_start))
			return addr;
	}
	addr = TASK_UNMAPPED_BASE;
	if (do_color_align)
		addr = COLOUR_ALIGN(addr, pgoff);
	else
		addr = PAGE_ALIGN(addr);

	for (vmm = find_vma(current->mm, addr); ; vmm = vmm->vm_next) {
		/* At this point:  (!vmm || addr < vmm->vm_end). */
		if (TASK_SIZE - len < addr)
			return -ENOMEM;
		if (!vmm || addr + len <= vmm->vm_start)
			return addr;
		addr = vmm->vm_end;
		if (do_color_align)
			addr = COLOUR_ALIGN(addr, pgoff);
	}
}

/* common code for old and new mmaps */
static inline long
do_mmap2(unsigned long addr, unsigned long len, unsigned long prot,
        unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	int error = -EBADF;
	struct file * file = NULL;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
out:
	return error;
}

asmlinkage unsigned long old_mmap(unsigned long addr, size_t len, int prot,
                                  int flags, int fd, off_t offset)
{
	int result;

	result = -EINVAL;
	if (offset & ~PAGE_MASK)
		goto out;

	result = do_mmap2(addr, len, prot, flags, fd, offset >> PAGE_SHIFT);

out:
	return result;
}

asmlinkage long
sys_mmap2(unsigned long addr, unsigned long len, unsigned long prot,
          unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	return do_mmap2(addr, len, prot, flags, fd, pgoff);
}

save_static_function(sys_fork);
static_unused int _sys_fork(struct pt_regs regs)
{
	int res;

	res = do_fork(SIGCHLD, regs.regs[29], &regs, 0);
	return res;
}


save_static_function(sys_clone);
static_unused int _sys_clone(struct pt_regs regs)
{
	unsigned long clone_flags;
	unsigned long newsp;
	int res;

	clone_flags = regs.regs[4];
	newsp = regs.regs[5];
	if (!newsp)
		newsp = regs.regs[29];
	res = do_fork(clone_flags, newsp, &regs, 0);
	return res;
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(struct pt_regs regs)
{
	int error;
	char * filename;

	filename = getname((char *) (long)regs.regs[4]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char **) (long)regs.regs[5],
	                  (char **) (long)regs.regs[6], &regs);
	putname(filename);

out:
	return error;
}

/*
 * Compacrapability ...
 */
asmlinkage int sys_uname(struct old_utsname * name)
{
	if (name && !copy_to_user(name, &system_utsname, sizeof (*name)))
		return 0;
	return -EFAULT;
}

/*
 * Compacrapability ...
 */
asmlinkage int sys_olduname(struct oldold_utsname * name)
{
	int error;

	if (!name)
		return -EFAULT;
	if (!access_ok(VERIFY_WRITE,name,sizeof(struct oldold_utsname)))
		return -EFAULT;

	error = __copy_to_user(&name->sysname,&system_utsname.sysname,__OLD_UTS_LEN);
	error -= __put_user(0,name->sysname+__OLD_UTS_LEN);
	error -= __copy_to_user(&name->nodename,&system_utsname.nodename,__OLD_UTS_LEN);
	error -= __put_user(0,name->nodename+__OLD_UTS_LEN);
	error -= __copy_to_user(&name->release,&system_utsname.release,__OLD_UTS_LEN);
	error -= __put_user(0,name->release+__OLD_UTS_LEN);
	error -= __copy_to_user(&name->version,&system_utsname.version,__OLD_UTS_LEN);
	error -= __put_user(0,name->version+__OLD_UTS_LEN);
	error -= __copy_to_user(&name->machine,&system_utsname.machine,__OLD_UTS_LEN);
	error = __put_user(0,name->machine+__OLD_UTS_LEN);
	error = error ? -EFAULT : 0;

	return error;
}

/*
 * If we ever come here the user sp is bad.  Zap the process right away.
 * Due to the bad stack signaling wouldn't work.
 * XXX kernel locking???
 */
asmlinkage void bad_stack(void)
{
	do_exit(SIGSEGV);
}

/*
 * Build the string table for the builtin "poor man's strace".
 */
#ifdef CONF_PRINT_SYSCALLS
#define SYS(fun, narg) #fun,
static char *sfnames[] = {
#include "syscalls.h"
};
#endif

#if defined(CONFIG_BINFMT_IRIX) && defined(CONF_DEBUG_IRIX)
#define SYS(fun, narg) #fun,
static char *irix_sys_names[] = {
#include "irix5sys.h"
};
#endif
