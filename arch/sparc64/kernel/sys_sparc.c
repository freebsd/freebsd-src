/* $Id: sys_sparc.c,v 1.55.2.1 2001/12/21 04:58:23 davem Exp $
 * linux/arch/sparc64/kernel/sys_sparc.c
 *
 * This file contains various random system calls that
 * have a non-standard calling sequence on the Linux/sparc
 * platform.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/utsname.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/ipc.h>
#include <linux/personality.h>

#include <asm/uaccess.h>
#include <asm/ipc.h>
#include <asm/utrap.h>
#include <asm/perfctr.h>

/* #define DEBUG_UNIMP_SYSCALL */

/* XXX Make this per-binary type, this way we can detect the type of
 * XXX a binary.  Every Sparc executable calls this very early on.
 */
asmlinkage unsigned long sys_getpagesize(void)
{
	return PAGE_SIZE;
}

#define COLOUR_ALIGN(addr,pgoff)		\
	((((addr)+SHMLBA-1)&~(SHMLBA-1)) +	\
	 (((pgoff)<<PAGE_SHIFT) & (SHMLBA-1)))

unsigned long arch_get_unmapped_area(struct file *filp, unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct vm_area_struct * vmm;
	unsigned long task_size = TASK_SIZE;
	int do_color_align;

	if (flags & MAP_FIXED) {
		/* We do not accept a shared mapping if it would violate
		 * cache aliasing constraints.
		 */
		if ((flags & MAP_SHARED) && (addr & (SHMLBA - 1)))
			return -EINVAL;
		return addr;
	}

	if (current->thread.flags & SPARC_FLAG_32BIT)
		task_size = 0xf0000000UL;
	if (len > task_size || len > -PAGE_OFFSET)
		return -ENOMEM;
	if (!addr)
		addr = TASK_UNMAPPED_BASE;

	do_color_align = 0;
	if (filp || (flags & MAP_SHARED))
		do_color_align = 1;

	if (do_color_align)
		addr = COLOUR_ALIGN(addr, pgoff);
	else
		addr = PAGE_ALIGN(addr);
	task_size -= len;

	for (vmm = find_vma(current->mm, addr); ; vmm = vmm->vm_next) {
		/* At this point:  (!vmm || addr < vmm->vm_end). */
		if (addr < PAGE_OFFSET && -PAGE_OFFSET - len < addr) {
			addr = PAGE_OFFSET;
			vmm = find_vma(current->mm, PAGE_OFFSET);
		}
		if (task_size < addr)
			return -ENOMEM;
		if (!vmm || addr + len <= vmm->vm_start)
			return addr;
		addr = vmm->vm_end;
		if (do_color_align)
			addr = COLOUR_ALIGN(addr, pgoff);
	}
}

/* Try to align mapping such that we align it as much as possible. */
unsigned long get_fb_unmapped_area(struct file *filp, unsigned long orig_addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
	unsigned long align_goal, addr = -ENOMEM;

	if (flags & MAP_FIXED) {
		/* Ok, don't mess with it. */
		return get_unmapped_area(NULL, addr, len, pgoff, flags);
	}
	flags &= ~MAP_SHARED;

	align_goal = PAGE_SIZE;
	if (len >= (4UL * 1024 * 1024))
		align_goal = (4UL * 1024 * 1024);
	else if (len >= (512UL * 1024))
		align_goal = (512UL * 1024);
	else if (len >= (64UL * 1024))
		align_goal = (64UL * 1024);

	do {
		addr = get_unmapped_area(NULL, orig_addr, len + (align_goal - PAGE_SIZE), pgoff, flags);
		if (!(addr & ~PAGE_MASK)) {
			addr = (addr + (align_goal - 1UL)) & ~(align_goal - 1UL);
			break;
		}

		if (align_goal == (4UL * 1024 * 1024))
			align_goal = (512UL * 1024);
		else if (align_goal == (512UL * 1024))
			align_goal = (64UL * 1024);
		else
			align_goal = PAGE_SIZE;
	} while ((addr & ~PAGE_MASK) && align_goal > PAGE_SIZE);

	/* Mapping is smaller than 64K or larger areas could not
	 * be obtained.
	 */
	if (addr & ~PAGE_MASK)
		addr = get_unmapped_area(NULL, orig_addr, len, pgoff, flags);

	return addr;
}

extern asmlinkage unsigned long sys_brk(unsigned long brk);

asmlinkage unsigned long sparc_brk(unsigned long brk)
{
	/* People could try to be nasty and use ta 0x6d in 32bit programs */
	if ((current->thread.flags & SPARC_FLAG_32BIT) &&
	    brk >= 0xf0000000UL)
		return current->mm->brk;

	if ((current->mm->brk & PAGE_OFFSET) != (brk & PAGE_OFFSET))
		return current->mm->brk;
	return sys_brk(brk);
}
                                                                
/*
 * sys_pipe() is the normal C calling standard for creating
 * a pipe. It's not the way unix traditionally does this, though.
 */
asmlinkage int sparc_pipe(struct pt_regs *regs)
{
	int fd[2];
	int error;

	error = do_pipe(fd);
	if (error)
		goto out;
	regs->u_regs[UREG_I1] = fd[1];
	error = fd[0];
out:
	return error;
}

/*
 * sys_ipc() is the de-multiplexer for the SysV IPC calls..
 *
 * This is really horribly ugly.
 */

asmlinkage int sys_ipc (unsigned call, int first, int second, unsigned long third, void *ptr, long fifth)
{
	int err;

	/* No need for backward compatibility. We can start fresh... */

	if (call <= SEMCTL)
		switch (call) {
		case SEMOP:
			err = sys_semtimedop (first, (struct sembuf *)ptr, second, NULL);
			goto out;
		case SEMTIMEDOP:
			err = sys_semtimedop (first, (struct sembuf *)ptr, second, (const struct timespec *) fifth);
			goto out;
		case SEMGET:
			err = sys_semget (first, second, (int)third);
			goto out;
		case SEMCTL: {
			union semun fourth;
			err = -EINVAL;
			if (!ptr)
				goto out;
			err = -EFAULT;
			if(get_user(fourth.__pad, (void **)ptr))
				goto out;
			err = sys_semctl (first, second | IPC_64, (int)third, fourth);
			goto out;
			}
		default:
			err = -ENOSYS;
			goto out;
		}
	if (call <= MSGCTL) 
		switch (call) {
		case MSGSND:
			err = sys_msgsnd (first, (struct msgbuf *) ptr, 
					  second, (int)third);
			goto out;
		case MSGRCV:
			err = sys_msgrcv (first, (struct msgbuf *) ptr, second, fifth, (int)third);
			goto out;
		case MSGGET:
			err = sys_msgget ((key_t) first, second);
			goto out;
		case MSGCTL:
			err = sys_msgctl (first, second | IPC_64, (struct msqid_ds *) ptr);
			goto out;
		default:
			err = -ENOSYS;
			goto out;
		}
	if (call <= SHMCTL) 
		switch (call) {
		case SHMAT: {
			ulong raddr;
			err = sys_shmat (first, (char *) ptr, second, &raddr);
			if (!err) {
				if (put_user(raddr, (ulong *) third))
					err = -EFAULT;
			}
			goto out;
		}
		case SHMDT:
			err = sys_shmdt ((char *)ptr);
			goto out;
		case SHMGET:
			err = sys_shmget (first, second, (int)third);
			goto out;
		case SHMCTL:
			err = sys_shmctl (first, second | IPC_64, (struct shmid_ds *) ptr);
			goto out;
		default:
			err = -ENOSYS;
			goto out;
		}
	else
		err = -ENOSYS;
out:
	return err;
}

extern asmlinkage int sys_newuname(struct new_utsname * name);

asmlinkage int sparc64_newuname(struct new_utsname * name)
{
	int ret = sys_newuname(name);
	
	if (current->personality == PER_LINUX32 && !ret) {
		ret = copy_to_user(name->machine, "sparc\0\0", 8);
	}
	return ret;
}

extern asmlinkage long sys_personality(unsigned long);

asmlinkage int sparc64_personality(unsigned long personality)
{
	int ret;

	if (current->personality == PER_LINUX32 && personality == PER_LINUX)
		personality = PER_LINUX32;
	ret = sys_personality(personality);
	if (ret == PER_LINUX32)
		ret = PER_LINUX;

	return ret;
}

/* Linux version of mmap */
asmlinkage unsigned long sys_mmap(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags, unsigned long fd,
	unsigned long off)
{
	struct file * file = NULL;
	unsigned long retval = -EBADF;

	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}
	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	len = PAGE_ALIGN(len);
	retval = -EINVAL;

	if (current->thread.flags & SPARC_FLAG_32BIT) {
		if (len > 0xf0000000UL ||
		    ((flags & MAP_FIXED) && addr > 0xf0000000UL - len))
			goto out_putf;
	} else {
		if (len > -PAGE_OFFSET ||
		    ((flags & MAP_FIXED) &&
		     addr < PAGE_OFFSET && addr + len > -PAGE_OFFSET))
			goto out_putf;
	}

	down_write(&current->mm->mmap_sem);
	retval = do_mmap(file, addr, len, prot, flags, off);
	up_write(&current->mm->mmap_sem);

out_putf:
	if (file)
		fput(file);
out:
	return retval;
}

asmlinkage long sys64_munmap(unsigned long addr, size_t len)
{
	long ret;

	if (len > -PAGE_OFFSET ||
	    (addr < PAGE_OFFSET && addr + len > -PAGE_OFFSET))
		return -EINVAL;
	down_write(&current->mm->mmap_sem);
	ret = do_munmap(current->mm, addr, len);
	up_write(&current->mm->mmap_sem);
	return ret;
}

extern unsigned long do_mremap(unsigned long addr,
	unsigned long old_len, unsigned long new_len,
	unsigned long flags, unsigned long new_addr);
                
asmlinkage unsigned long sys64_mremap(unsigned long addr,
	unsigned long old_len, unsigned long new_len,
	unsigned long flags, unsigned long new_addr)
{
	struct vm_area_struct *vma;
	unsigned long ret = -EINVAL;
	if (current->thread.flags & SPARC_FLAG_32BIT)
		goto out;
	if (old_len > -PAGE_OFFSET || new_len > -PAGE_OFFSET)
		goto out;
	if (addr < PAGE_OFFSET && addr + old_len > -PAGE_OFFSET)
		goto out;
	down_write(&current->mm->mmap_sem);
	if (flags & MREMAP_FIXED) {
		if (new_addr < PAGE_OFFSET &&
		    new_addr + new_len > -PAGE_OFFSET)
			goto out_sem;
	} else if (addr < PAGE_OFFSET && addr + new_len > -PAGE_OFFSET) {
		unsigned long map_flags = 0;
		struct file *file = NULL;

		ret = -ENOMEM;
		if (!(flags & MREMAP_MAYMOVE))
			goto out_sem;

		vma = find_vma(current->mm, addr);
		if (vma) {
			if (vma->vm_flags & VM_SHARED)
				map_flags |= MAP_SHARED;
			file = vma->vm_file;
		}

		/* MREMAP_FIXED checked above. */
		new_addr = get_unmapped_area(file, addr, new_len,
				    vma ? vma->vm_pgoff : 0,
				    map_flags);
		ret = new_addr;
		if (new_addr & ~PAGE_MASK)
			goto out_sem;
		flags |= MREMAP_FIXED;
	}
	ret = do_mremap(addr, old_len, new_len, flags, new_addr);
out_sem:
	up_write(&current->mm->mmap_sem);
out:
	return ret;       
}

/* we come to here via sys_nis_syscall so it can setup the regs argument */
asmlinkage unsigned long
c_sys_nis_syscall (struct pt_regs *regs)
{
	static int count;
	
	/* Don't make the system unusable, if someone goes stuck */
	if (count++ > 5)
		return -ENOSYS;

	printk ("Unimplemented SPARC system call %ld\n",regs->u_regs[1]);
#ifdef DEBUG_UNIMP_SYSCALL	
	show_regs (regs);
#endif

	return -ENOSYS;
}

/* #define DEBUG_SPARC_BREAKPOINT */

asmlinkage void
sparc_breakpoint (struct pt_regs *regs)
{
	siginfo_t info;

	if ((current->thread.flags & SPARC_FLAG_32BIT) != 0) {
		regs->tpc &= 0xffffffff;
		regs->tnpc &= 0xffffffff;
	}
#ifdef DEBUG_SPARC_BREAKPOINT
        printk ("TRAP: Entering kernel PC=%lx, nPC=%lx\n", regs->tpc, regs->tnpc);
#endif
	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code = TRAP_BRKPT;
	info.si_addr = (void *)regs->tpc;
	info.si_trapno = 0;
	force_sig_info(SIGTRAP, &info, current);
#ifdef DEBUG_SPARC_BREAKPOINT
	printk ("TRAP: Returning to space: PC=%lx nPC=%lx\n", regs->tpc, regs->tnpc);
#endif
}

extern void check_pending(int signum);

asmlinkage int sys_getdomainname(char *name, int len)
{
        int nlen;
	int err = -EFAULT;

 	down_read(&uts_sem);
 	
	nlen = strlen(system_utsname.domainname) + 1;

        if (nlen < len)
                len = nlen;
	if(len > __NEW_UTS_LEN)
		goto done;
	if(copy_to_user(name, system_utsname.domainname, len))
		goto done;
	err = 0;
done:
	up_read(&uts_sem);
	return err;
}

/* only AP+ systems have sys_aplib */
asmlinkage int sys_aplib(void)
{
	return -ENOSYS;
}

asmlinkage int solaris_syscall(struct pt_regs *regs)
{
	static int count;

	regs->tpc = regs->tnpc;
	regs->tnpc += 4;
	if ((current->thread.flags & SPARC_FLAG_32BIT) != 0) {
		regs->tpc &= 0xffffffff;
		regs->tnpc &= 0xffffffff;
	}
	if(++count <= 5) {
		printk ("For Solaris binary emulation you need solaris module loaded\n");
		show_regs (regs);
	}
	send_sig(SIGSEGV, current, 1);

	return -ENOSYS;
}

#ifndef CONFIG_SUNOS_EMUL
asmlinkage int sunos_syscall(struct pt_regs *regs)
{
	static int count;

	regs->tpc = regs->tnpc;
	regs->tnpc += 4;
	if ((current->thread.flags & SPARC_FLAG_32BIT) != 0) {
		regs->tpc &= 0xffffffff;
		regs->tnpc &= 0xffffffff;
	}
	if(++count <= 20)
		printk ("SunOS binary emulation not compiled in\n");
	force_sig(SIGSEGV, current);

	return -ENOSYS;
}
#endif

asmlinkage int sys_utrap_install(utrap_entry_t type, utrap_handler_t new_p,
				 utrap_handler_t new_d,
				 utrap_handler_t *old_p, utrap_handler_t *old_d)
{
	if (type < UT_INSTRUCTION_EXCEPTION || type > UT_TRAP_INSTRUCTION_31)
		return -EINVAL;
	if (new_p == (utrap_handler_t)(long)UTH_NOCHANGE) {
		if (old_p) {
			if (!current->thread.utraps) {
				if (put_user(NULL, old_p))
					return -EFAULT;
			} else {
				if (put_user((utrap_handler_t)(current->thread.utraps[type]), old_p))
					return -EFAULT;
			}
		}
		if (old_d) {
			if (put_user(NULL, old_d))
				return -EFAULT;
		}
		return 0;
	}
	if (!current->thread.utraps) {
		current->thread.utraps =
			kmalloc((UT_TRAP_INSTRUCTION_31+1)*sizeof(long), GFP_KERNEL);
		if (!current->thread.utraps) return -ENOMEM;
		current->thread.utraps[0] = 1;
		memset(current->thread.utraps+1, 0, UT_TRAP_INSTRUCTION_31*sizeof(long));
	} else {
		if ((utrap_handler_t)current->thread.utraps[type] != new_p &&
		    current->thread.utraps[0] > 1) {
			long *p = current->thread.utraps;

			current->thread.utraps =
				kmalloc((UT_TRAP_INSTRUCTION_31+1)*sizeof(long),
					GFP_KERNEL);
			if (!current->thread.utraps) {
				current->thread.utraps = p;
				return -ENOMEM;
			}
			p[0]--;
			current->thread.utraps[0] = 1;
			memcpy(current->thread.utraps+1, p+1,
			       UT_TRAP_INSTRUCTION_31*sizeof(long));
		}
	}
	if (old_p) {
		if (put_user((utrap_handler_t)(current->thread.utraps[type]), old_p))
			return -EFAULT;
	}
	if (old_d) {
		if (put_user(NULL, old_d))
			return -EFAULT;
	}
	current->thread.utraps[type] = (long)new_p;

	return 0;
}

long sparc_memory_ordering(unsigned long model, struct pt_regs *regs)
{
	if (model >= 3)
		return -EINVAL;
	regs->tstate = (regs->tstate & ~TSTATE_MM) | (model << 14);
	return 0;
}

asmlinkage int
sys_rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact,
		 void *restorer, size_t sigsetsize)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (act) {
		new_ka.ka_restorer = restorer;
		if (copy_from_user(&new_ka.sa, act, sizeof(*act)))
			return -EFAULT;
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (copy_to_user(oact, &old_ka.sa, sizeof(*oact)))
			return -EFAULT;
	}

	return ret;
}

/* Invoked by rtrap code to update performance counters in
 * user space.
 */
asmlinkage void
update_perfctrs(void)
{
	unsigned long pic, tmp;

	read_pic(pic);
	tmp = (current->thread.kernel_cntd0 += (unsigned int)pic);
	__put_user(tmp, current->thread.user_cntd0);
	tmp = (current->thread.kernel_cntd1 += (pic >> 32));
	__put_user(tmp, current->thread.user_cntd1);
	reset_pic();
}

asmlinkage int
sys_perfctr(int opcode, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	int err = 0;

	switch(opcode) {
	case PERFCTR_ON:
		current->thread.pcr_reg = arg2;
		current->thread.user_cntd0 = (u64 *) arg0;
		current->thread.user_cntd1 = (u64 *) arg1;
		current->thread.kernel_cntd0 =
			current->thread.kernel_cntd1 = 0;
		write_pcr(arg2);
		reset_pic();
		current->thread.flags |= SPARC_FLAG_PERFCTR;
		break;

	case PERFCTR_OFF:
		err = -EINVAL;
		if ((current->thread.flags & SPARC_FLAG_PERFCTR) != 0) {
			current->thread.user_cntd0 =
				current->thread.user_cntd1 = NULL;
			current->thread.pcr_reg = 0;
			write_pcr(0);
			current->thread.flags &= ~(SPARC_FLAG_PERFCTR);
			err = 0;
		}
		break;

	case PERFCTR_READ: {
		unsigned long pic, tmp;

		if (!(current->thread.flags & SPARC_FLAG_PERFCTR)) {
			err = -EINVAL;
			break;
		}
		read_pic(pic);
		tmp = (current->thread.kernel_cntd0 += (unsigned int)pic);
		err |= __put_user(tmp, current->thread.user_cntd0);
		tmp = (current->thread.kernel_cntd1 += (pic >> 32));
		err |= __put_user(tmp, current->thread.user_cntd1);
		reset_pic();
		break;
	}

	case PERFCTR_CLRPIC:
		if (!(current->thread.flags & SPARC_FLAG_PERFCTR)) {
			err = -EINVAL;
			break;
		}
		current->thread.kernel_cntd0 =
			current->thread.kernel_cntd1 = 0;
		reset_pic();
		break;

	case PERFCTR_SETPCR: {
		u64 *user_pcr = (u64 *)arg0;
		if (!(current->thread.flags & SPARC_FLAG_PERFCTR)) {
			err = -EINVAL;
			break;
		}
		err |= __get_user(current->thread.pcr_reg, user_pcr);
		write_pcr(current->thread.pcr_reg);
		current->thread.kernel_cntd0 =
			current->thread.kernel_cntd1 = 0;
		reset_pic();
		break;
	}

	case PERFCTR_GETPCR: {
		u64 *user_pcr = (u64 *)arg0;
		if (!(current->thread.flags & SPARC_FLAG_PERFCTR)) {
			err = -EINVAL;
			break;
		}
		err |= __put_user(current->thread.pcr_reg, user_pcr);
		break;
	}

	default:
		err = -EINVAL;
		break;
	};
	return err;
}
