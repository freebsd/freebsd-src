/*
 * sysirix.c: IRIX system call emulation.
 *
 * Copyright (C) 1996 David S. Miller
 * Copyright (C) 1997 Miguel de Icaza
 * Copyright (C) 1997, 1998, 1999, 2000 Ralf Baechle
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/errno.h>
#include <linux/timex.h>
#include <linux/times.h>
#include <linux/elf.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/utsname.h>
#include <linux/file.h>

#include <asm/ptrace.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/inventory.h>

/* 2,191 lines of complete and utter shit coming up... */

extern int max_threads;

/* The sysmp commands supported thus far. */
#define MP_NPROCS       	1 /* # processor in complex */
#define MP_NAPROCS      	2 /* # active processors in complex */
#define MP_PGSIZE           	14 /* Return system page size in v1. */

asmlinkage int irix_sysmp(struct pt_regs *regs)
{
	unsigned long cmd;
	int base = 0;
	int error = 0;

	if(regs->regs[2] == 1000)
		base = 1;
	cmd = regs->regs[base + 4];
	switch(cmd) {
	case MP_PGSIZE:
		error = PAGE_SIZE;
		break;
	case MP_NPROCS:
	case MP_NAPROCS:
		error = smp_num_cpus;
		break;
	default:
		printk("SYSMP[%s:%d]: Unsupported opcode %d\n",
		       current->comm, current->pid, (int)cmd);
		error = -EINVAL;
		break;
	}

	return error;
}

/* The prctl commands. */
#define PR_MAXPROCS          1 /* Tasks/user. */
#define PR_ISBLOCKED         2 /* If blocked, return 1. */
#define PR_SETSTACKSIZE      3 /* Set largest task stack size. */
#define PR_GETSTACKSIZE      4 /* Get largest task stack size. */
#define PR_MAXPPROCS         5 /* Num parallel tasks. */
#define PR_UNBLKONEXEC       6 /* When task exec/exit's, unblock. */
#define PR_SETEXITSIG        8 /* When task exit's, set signal. */
#define PR_RESIDENT          9 /* Make task unswappable. */
#define PR_ATTACHADDR       10 /* (Re-)Connect a vma to a task. */
#define PR_DETACHADDR       11 /* Disconnect a vma from a task. */
#define PR_TERMCHILD        12 /* When parent sleeps with fishes, kill child. */
#define PR_GETSHMASK        13 /* Get the sproc() share mask. */
#define PR_GETNSHARE        14 /* Number of share group members. */
#define PR_COREPID          15 /* Add task pid to name when it core. */
#define	PR_ATTACHADDRPERM   16 /* (Re-)Connect vma, with specified prot. */
#define PR_PTHREADEXIT      17 /* Kill a pthread without prejudice. */

asmlinkage int irix_prctl(struct pt_regs *regs)
{
	unsigned long cmd;
	int error = 0, base = 0;

	if (regs->regs[2] == 1000)
		base = 1;
	cmd = regs->regs[base + 4];
	switch (cmd) {
	case PR_MAXPROCS:
		printk("irix_prctl[%s:%d]: Wants PR_MAXPROCS\n",
		       current->comm, current->pid);
		error = max_threads;
		break;

	case PR_ISBLOCKED: {
		struct task_struct *task;

		printk("irix_prctl[%s:%d]: Wants PR_ISBLOCKED\n",
		       current->comm, current->pid);
		read_lock(&tasklist_lock);
		task = find_task_by_pid(regs->regs[base + 5]);
		error = -ESRCH;
		if (error)
			error = (task->run_list.next != NULL);
		read_unlock(&tasklist_lock);
		/* Can _your_ OS find this out that fast? */
		break;
	}

	case PR_SETSTACKSIZE: {
		long value = regs->regs[base + 5];

		printk("irix_prctl[%s:%d]: Wants PR_SETSTACKSIZE<%08lx>\n",
		       current->comm, current->pid, (unsigned long) value);
		if (value > RLIM_INFINITY)
			value = RLIM_INFINITY;
		if (capable(CAP_SYS_ADMIN)) {
			current->rlim[RLIMIT_STACK].rlim_max =
				current->rlim[RLIMIT_STACK].rlim_cur = value;
			error = value;
			break;
		}
		if (value > current->rlim[RLIMIT_STACK].rlim_max) {
			error = -EINVAL;
			break;
		}
		current->rlim[RLIMIT_STACK].rlim_cur = value;
		error = value;
		break;
	}

	case PR_GETSTACKSIZE:
		printk("irix_prctl[%s:%d]: Wants PR_GETSTACKSIZE\n",
		       current->comm, current->pid);
		error = current->rlim[RLIMIT_STACK].rlim_cur;
		break;

	case PR_MAXPPROCS:
		printk("irix_prctl[%s:%d]: Wants PR_MAXPROCS\n",
		       current->comm, current->pid);
		error = 1;
		break;

	case PR_UNBLKONEXEC:
		printk("irix_prctl[%s:%d]: Wants PR_UNBLKONEXEC\n",
		       current->comm, current->pid);
		error = -EINVAL;
		break;

	case PR_SETEXITSIG:
		printk("irix_prctl[%s:%d]: Wants PR_SETEXITSIG\n",
		       current->comm, current->pid);

		/* We can probably play some game where we set the task
		 * exit_code to some non-zero value when this is requested,
		 * and check whether exit_code is already set in do_exit().
		 */
		error = -EINVAL;
		break;

	case PR_RESIDENT:
		printk("irix_prctl[%s:%d]: Wants PR_RESIDENT\n",
		       current->comm, current->pid);
		error = 0; /* Compatibility indeed. */
		break;

	case PR_ATTACHADDR:
		printk("irix_prctl[%s:%d]: Wants PR_ATTACHADDR\n",
		       current->comm, current->pid);
		error = -EINVAL;
		break;

	case PR_DETACHADDR:
		printk("irix_prctl[%s:%d]: Wants PR_DETACHADDR\n",
		       current->comm, current->pid);
		error = -EINVAL;
		break;

	case PR_TERMCHILD:
		printk("irix_prctl[%s:%d]: Wants PR_TERMCHILD\n",
		       current->comm, current->pid);
		error = -EINVAL;
		break;

	case PR_GETSHMASK:
		printk("irix_prctl[%s:%d]: Wants PR_GETSHMASK\n",
		       current->comm, current->pid);
		error = -EINVAL; /* Until I have the sproc() stuff in. */
		break;

	case PR_GETNSHARE:
		error = 0;       /* Until I have the sproc() stuff in. */
		break;

	case PR_COREPID:
		printk("irix_prctl[%s:%d]: Wants PR_COREPID\n",
		       current->comm, current->pid);
		error = -EINVAL;
		break;

	case PR_ATTACHADDRPERM:
		printk("irix_prctl[%s:%d]: Wants PR_ATTACHADDRPERM\n",
		       current->comm, current->pid);
		error = -EINVAL;
		break;

	case PR_PTHREADEXIT:
		printk("irix_prctl[%s:%d]: Wants PR_PTHREADEXIT\n",
		       current->comm, current->pid);
		do_exit(regs->regs[base + 5]);

	default:
		printk("irix_prctl[%s:%d]: Non-existant opcode %d\n",
		       current->comm, current->pid, (int)cmd);
		error = -EINVAL;
		break;
	}

	return error;
}

#undef DEBUG_PROCGRPS

extern unsigned long irix_mapelf(int fd, struct elf_phdr *user_phdrp, int cnt);
extern asmlinkage int sys_setpgid(pid_t pid, pid_t pgid);
extern void sys_sync(void);
extern asmlinkage int sys_getsid(pid_t pid);
extern asmlinkage long sys_write (unsigned int fd, const char *buf, unsigned long count);
extern asmlinkage long sys_lseek (unsigned int fd, off_t offset, unsigned int origin);
extern asmlinkage int sys_getgroups(int gidsetsize, gid_t *grouplist);
extern asmlinkage int sys_setgroups(int gidsetsize, gid_t *grouplist);
extern int getrusage(struct task_struct *p, int who, struct rusage *ru);
extern char *prom_getenv(char *name);
extern long prom_setenv(char *name, char *value);

/* The syssgi commands supported thus far. */
#define SGI_SYSID         1       /* Return unique per-machine identifier. */
#define SGI_INVENT        5       /* Fetch inventory  */
#   define SGI_INV_SIZEOF 1
#   define SGI_INV_READ   2
#define SGI_RDNAME        6       /* Return string name of a process. */
#define SGI_SETNVRAM	  8	  /* Set PROM variable. */
#define SGI_GETNVRAM	  9	  /* Get PROM variable. */
#define SGI_SETPGID      21       /* Set process group id. */
#define SGI_SYSCONF      22       /* POSIX sysconf garbage. */
#define SGI_PATHCONF     24       /* POSIX sysconf garbage. */
#define SGI_SETGROUPS    40       /* POSIX sysconf garbage. */
#define SGI_GETGROUPS    41       /* POSIX sysconf garbage. */
#define SGI_RUSAGE       56       /* BSD style rusage(). */
#define SGI_SSYNC        62       /* Synchronous fs sync. */
#define SGI_GETSID       65       /* SysVr4 get session id. */
#define SGI_ELFMAP       68       /* Map an elf image. */
#define SGI_TOSSTSAVE   108       /* Toss saved vma's. */
#define SGI_FP_BCOPY    129       /* Should FPU bcopy be used on this machine? */
#define SGI_PHYSP      1011       /* Translate virtual into physical page. */

asmlinkage int irix_syssgi(struct pt_regs *regs)
{
	unsigned long cmd;
	int retval, base = 0;

	if (regs->regs[2] == 1000)
		base = 1;

	cmd = regs->regs[base + 4];
	switch(cmd) {
	case SGI_SYSID: {
		char *buf = (char *) regs->regs[base + 5];

		/* XXX Use ethernet addr.... */
		retval = clear_user(buf, 64);
		break;
	}
#if 0
	case SGI_RDNAME: {
		int pid = (int) regs->regs[base + 5];
		char *buf = (char *) regs->regs[base + 6];
		struct task_struct *p;
		char comm[16];

		retval = verify_area(VERIFY_WRITE, buf, 16);
		if (retval)
			break;
		read_lock(&tasklist_lock);
		p = find_task_by_pid(pid);
		if (!p) {
			read_unlock(&tasklist_lock);
			retval = -ESRCH;
			break;
		}
		memcpy(comm, p->comm, 16);
		read_unlock(&tasklist_lock);

		/* XXX Need to check sizes. */
		copy_to_user(buf, p->comm, 16);
		retval = 0;
		break;
	}

	case SGI_GETNVRAM: {
		char *name = (char *) regs->regs[base+5];
		char *buf = (char *) regs->regs[base+6];
		char *value;
		return -EINVAL;	/* til I fix it */
		retval = verify_area(VERIFY_WRITE, buf, 128);
		if (retval)
			break;
		value = prom_getenv(name);	/* PROM lock?  */
		if (!value) {
			retval = -EINVAL;
			break;
		}
		/* Do I strlen() for the length? */
		copy_to_user(buf, value, 128);
		retval = 0;
		break;
	}

	case SGI_SETNVRAM: {
		char *name = (char *) regs->regs[base+5];
		char *value = (char *) regs->regs[base+6];
		return -EINVAL;	/* til I fix it */
		retval = prom_setenv(name, value);
		/* XXX make sure retval conforms to syssgi(2) */
		printk("[%s:%d] setnvram(\"%s\", \"%s\"): retval %d",
		       current->comm, current->pid, name, value, retval);
/*		if (retval == PROM_ENOENT)
		  	retval = -ENOENT; */
		break;
	}
#endif

	case SGI_SETPGID: {
#ifdef DEBUG_PROCGRPS
		printk("[%s:%d] setpgid(%d, %d) ",
		       current->comm, current->pid,
		       (int) regs->regs[base + 5], (int)regs->regs[base + 6]);
#endif
		retval = sys_setpgid(regs->regs[base + 5], regs->regs[base + 6]);

#ifdef DEBUG_PROCGRPS
		printk("retval=%d\n", retval);
#endif
	}

	case SGI_SYSCONF: {
		switch(regs->regs[base + 5]) {
		case 1:
			retval = (MAX_ARG_PAGES >> 4); /* XXX estimate... */
			goto out;
		case 2:
			retval = max_threads;
			goto out;
		case 3:
			retval = HZ;
			goto out;
		case 4:
			retval = NGROUPS;
			goto out;
		case 5:
			retval = NR_OPEN;
			goto out;
		case 6:
			retval = 1;
			goto out;
		case 7:
			retval = 1;
			goto out;
		case 8:
			retval = 199009;
			goto out;
		case 11:
			retval = PAGE_SIZE;
			goto out;
		case 12:
			retval = 4;
			goto out;
		case 25:
		case 26:
		case 27:
		case 28:
		case 29:
		case 30:
			retval = 0;
			goto out;
		case 31:
			retval = 32;
			goto out;
		default:
			retval = -EINVAL;
			goto out;
		};
	}

	case SGI_SETGROUPS:
		retval = sys_setgroups((int) regs->regs[base + 5],
		                       (gid_t *) regs->regs[base + 6]);
		break;

	case SGI_GETGROUPS:
		retval = sys_getgroups((int) regs->regs[base + 5],
		                       (gid_t *) regs->regs[base + 6]);
		break;

	case SGI_RUSAGE: {
		struct rusage *ru = (struct rusage *) regs->regs[base + 6];

		switch((int) regs->regs[base + 5]) {
		case 0:
			/* rusage self */
			retval = getrusage(current, RUSAGE_SELF, ru);
			goto out;

		case -1:
			/* rusage children */
			retval = getrusage(current, RUSAGE_CHILDREN, ru);
			goto out;

		default:
			retval = -EINVAL;
			goto out;
		};
	}

	case SGI_SSYNC:
		sys_sync();
		retval = 0;
		break;

	case SGI_GETSID:
#ifdef DEBUG_PROCGRPS
		printk("[%s:%d] getsid(%d) ", current->comm, current->pid,
		       (int) regs->regs[base + 5]);
#endif
		retval = sys_getsid(regs->regs[base + 5]);
#ifdef DEBUG_PROCGRPS
		printk("retval=%d\n", retval);
#endif
		break;

	case SGI_ELFMAP:
		retval = irix_mapelf((int) regs->regs[base + 5],
				     (struct elf_phdr *) regs->regs[base + 6],
				     (int) regs->regs[base + 7]);
		break;

	case SGI_TOSSTSAVE:
		/* XXX We don't need to do anything? */
		retval = 0;
		break;

	case SGI_FP_BCOPY:
		retval = 0;
		break;

	case SGI_PHYSP: {
		unsigned long addr = regs->regs[base + 5];
		int *pageno = (int *) (regs->regs[base + 6]);
		struct mm_struct *mm = current->mm;
		pgd_t *pgdp;
		pmd_t *pmdp;
		pte_t *ptep;

		retval = verify_area(VERIFY_WRITE, pageno, sizeof(int));
		if (retval)
			return retval;

		down_read(&mm->mmap_sem);
		pgdp = pgd_offset(mm, addr);
		pmdp = pmd_offset(pgdp, addr);
		ptep = pte_offset(pmdp, addr);
		retval = -EINVAL;
		if (ptep) {
			pte_t pte = *ptep;

			if (pte_val(pte) & (_PAGE_VALID | _PAGE_PRESENT)) {
				retval =  put_user((pte_val(pte) & PAGE_MASK) >>
				                   PAGE_SHIFT, pageno);
			}
		}
		up_read(&mm->mmap_sem);
		break;
	}

	case SGI_INVENT: {
		int  arg1    = (int)    regs->regs [base + 5];
		void *buffer = (void *) regs->regs [base + 6];
		int  count   = (int)    regs->regs [base + 7];

		switch (arg1) {
		case SGI_INV_SIZEOF:
			retval = sizeof (inventory_t);
			break;
		case SGI_INV_READ:
			retval = dump_inventory_to_user (buffer, count);
			break;
		default:
			retval = -EINVAL;
		}
		break;
	}

	default:
		printk("irix_syssgi: Unsupported command %d\n", (int)cmd);
		retval = -EINVAL;
		break;
	};

out:
	return retval;
}

asmlinkage int irix_gtime(struct pt_regs *regs)
{
	return CURRENT_TIME;
}

int vm_enough_memory(long pages);

/*
 * IRIX is completely broken... it returns 0 on success, otherwise
 * ENOMEM.
 */
asmlinkage int irix_brk(unsigned long brk)
{
	unsigned long rlim;
	unsigned long newbrk, oldbrk;
	struct mm_struct *mm = current->mm;
	int ret;

	down_write(&mm->mmap_sem);
	if (brk < mm->end_code) {
		ret = -ENOMEM;
		goto out;
	}

	newbrk = PAGE_ALIGN(brk);
	oldbrk = PAGE_ALIGN(mm->brk);
	if (oldbrk == newbrk) {
		mm->brk = brk;
		ret = 0;
		goto out;
	}

	/*
	 * Always allow shrinking brk
	 */
	if (brk <= mm->brk) {
		mm->brk = brk;
		do_munmap(mm, newbrk, oldbrk-newbrk);
		ret = 0;
		goto out;
	}
	/*
	 * Check against rlimit and stack..
	 */
	rlim = current->rlim[RLIMIT_DATA].rlim_cur;
	if (rlim >= RLIM_INFINITY)
		rlim = ~0;
	if (brk - mm->end_code > rlim) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * Check against existing mmap mappings.
	 */
	if (find_vma_intersection(mm, oldbrk, newbrk+PAGE_SIZE)) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * Check if we have enough memory..
	 */
	if (!vm_enough_memory((newbrk-oldbrk) >> PAGE_SHIFT)) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * Ok, looks good - let it rip.
	 */
	mm->brk = brk;
	do_brk(oldbrk, newbrk-oldbrk);
	ret = 0;

out:
	up_write(&mm->mmap_sem);
	return ret;
}

asmlinkage int irix_getpid(struct pt_regs *regs)
{
	regs->regs[3] = current->p_opptr->pid;
	return current->pid;
}

asmlinkage int irix_getuid(struct pt_regs *regs)
{
	regs->regs[3] = current->euid;
	return current->uid;
}

asmlinkage int irix_getgid(struct pt_regs *regs)
{
	regs->regs[3] = current->egid;
	return current->gid;
}

extern rwlock_t xtime_lock;

asmlinkage int irix_stime(int value)
{
	if (!capable(CAP_SYS_TIME))
		return -EPERM;

	write_lock_irq(&xtime_lock);
	xtime.tv_sec = value;
	xtime.tv_usec = 0;
	time_adjust = 0;			/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);

	return 0;
}

extern int do_setitimer(int which, struct itimerval *value,
                        struct itimerval *ovalue);

static inline void jiffiestotv(unsigned long jiffies, struct timeval *value)
{
	value->tv_usec = (jiffies % HZ) * (1000000 / HZ);
	value->tv_sec = jiffies / HZ;
}

static inline void getitimer_real(struct itimerval *value)
{
	register unsigned long val, interval;

	interval = current->it_real_incr;
	val = 0;
	if (del_timer(&current->real_timer)) {
		unsigned long now = jiffies;
		val = current->real_timer.expires;
		add_timer(&current->real_timer);
		/* look out for negative/zero itimer.. */
		if (val <= now)
			val = now+1;
		val -= now;
	}
	jiffiestotv(val, &value->it_value);
	jiffiestotv(interval, &value->it_interval);
}

asmlinkage unsigned int irix_alarm(unsigned int seconds)
{
	struct itimerval it_new, it_old;
	unsigned int oldalarm;

	if (!seconds) {
		getitimer_real(&it_old);
		del_timer(&current->real_timer);
	} else {
		it_new.it_interval.tv_sec = it_new.it_interval.tv_usec = 0;
		it_new.it_value.tv_sec = seconds;
		it_new.it_value.tv_usec = 0;
		do_setitimer(ITIMER_REAL, &it_new, &it_old);
	}
	oldalarm = it_old.it_value.tv_sec;
	/*
	 * ehhh.. We can't return 0 if we have an alarm pending ...
	 * And we'd better return too much than too little anyway
	 */
	if (it_old.it_value.tv_usec)
		oldalarm++;

	return oldalarm;
}

asmlinkage int irix_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();

	return -EINTR;
}

extern asmlinkage long sys_mount(char * dev_name, char * dir_name, char * type,
				unsigned long new_flags, void * data);

/* XXX need more than this... */
asmlinkage int irix_mount(char *dev_name, char *dir_name, unsigned long flags,
			  char *type, void *data, int datalen)
{
	printk("[%s:%d] irix_mount(%p,%p,%08lx,%p,%p,%d)\n",
	       current->comm, current->pid,
	       dev_name, dir_name, flags, type, data, datalen);

	return sys_mount(dev_name, dir_name, type, flags, data);
}

struct irix_statfs {
	short f_type;
        long  f_bsize, f_frsize, f_blocks, f_bfree, f_files, f_ffree;
	char  f_fname[6], f_fpack[6];
};

asmlinkage int irix_statfs(const char *path, struct irix_statfs *buf,
			   int len, int fs_type)
{
	struct nameidata nd;
	struct statfs kbuf;
	int error, i;

	/* We don't support this feature yet. */
	if (fs_type) {
		error = -EINVAL;
		goto out;
	}
	error = verify_area(VERIFY_WRITE, buf, sizeof(struct irix_statfs));
	if (error)
		goto out;
	error = user_path_walk(path, &nd);
	if (error)
		goto out;

	error = vfs_statfs(nd.dentry->d_inode->i_sb, &kbuf);
	if (error)
		goto dput_and_out;

	__put_user(kbuf.f_type, &buf->f_type);
	__put_user(kbuf.f_bsize, &buf->f_bsize);
	__put_user(kbuf.f_frsize, &buf->f_frsize);
	__put_user(kbuf.f_blocks, &buf->f_blocks);
	__put_user(kbuf.f_bfree, &buf->f_bfree);
	__put_user(kbuf.f_files, &buf->f_files);
	__put_user(kbuf.f_ffree, &buf->f_ffree);
	for (i = 0; i < 6; i++) {
		__put_user(0, &buf->f_fname[i]);
		__put_user(0, &buf->f_fpack[i]);
	}
	error = 0;

dput_and_out:
	path_release(&nd);
out:
	return error;
}

asmlinkage int irix_fstatfs(unsigned int fd, struct irix_statfs *buf)
{
	struct statfs kbuf;
	struct file *file;
	int error, i;

	error = verify_area(VERIFY_WRITE, buf, sizeof(struct irix_statfs));
	if (error)
		goto out;
	if (!(file = fget(fd))) {
		error = -EBADF;
		goto out;
	}

	error = vfs_statfs(file->f_dentry->d_inode->i_sb, &kbuf);
	if (error)
		goto out_f;

	__put_user(kbuf.f_type, &buf->f_type);
	__put_user(kbuf.f_bsize, &buf->f_bsize);
	__put_user(kbuf.f_frsize, &buf->f_frsize);
	__put_user(kbuf.f_blocks, &buf->f_blocks);
	__put_user(kbuf.f_bfree, &buf->f_bfree);
	__put_user(kbuf.f_files, &buf->f_files);
	__put_user(kbuf.f_ffree, &buf->f_ffree);
	for(i = 0; i < 6; i++) {
		__put_user(0, &buf->f_fname[i]);
		__put_user(0, &buf->f_fpack[i]);
	}

out_f:
	fput(file);
out:
	return error;
}

extern asmlinkage int sys_setpgid(pid_t pid, pid_t pgid);
extern asmlinkage int sys_setsid(void);

asmlinkage int irix_setpgrp(int flags)
{
	int error;

#ifdef DEBUG_PROCGRPS
	printk("[%s:%d] setpgrp(%d) ", current->comm, current->pid, flags);
#endif
	if(!flags)
		error = current->pgrp;
	else
		error = sys_setsid();
#ifdef DEBUG_PROCGRPS
	printk("returning %d\n", current->pgrp);
#endif

	return error;
}

asmlinkage int irix_times(struct tms * tbuf)
{
	int err = 0;

	if (tbuf) {
		err = verify_area(VERIFY_WRITE,tbuf,sizeof *tbuf);
		if (err)
			return err;
		err |= __put_user(current->times.tms_utime,&tbuf->tms_utime);
		err |= __put_user(current->times.tms_stime,&tbuf->tms_stime);
		err |= __put_user(current->times.tms_cutime,&tbuf->tms_cutime);
		err |= __put_user(current->times.tms_cstime,&tbuf->tms_cstime);
	}

	return err;
}

asmlinkage int irix_exec(struct pt_regs *regs)
{
	int error, base = 0;
	char *filename;

	if(regs->regs[2] == 1000)
		base = 1;
	filename = getname((char *) (long)regs->regs[base + 4]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		return error;

	error = do_execve(filename, (char **) (long)regs->regs[base + 5],
	                  (char **) 0, regs);
	putname(filename);

	return error;
}

asmlinkage int irix_exece(struct pt_regs *regs)
{
	int error, base = 0;
	char *filename;

	if (regs->regs[2] == 1000)
		base = 1;
	filename = getname((char *) (long)regs->regs[base + 4]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		return error;
	error = do_execve(filename, (char **) (long)regs->regs[base + 5],
	                  (char **) (long)regs->regs[base + 6], regs);
	putname(filename);

	return error;
}

asmlinkage unsigned long irix_gethostid(void)
{
	printk("[%s:%d]: irix_gethostid() called...\n",
	       current->comm, current->pid);

	return -EINVAL;
}

asmlinkage unsigned long irix_sethostid(unsigned long val)
{
	printk("[%s:%d]: irix_sethostid(%08lx) called...\n",
	       current->comm, current->pid, val);

	return -EINVAL;
}

extern asmlinkage int sys_socket(int family, int type, int protocol);

asmlinkage int irix_socket(int family, int type, int protocol)
{
	switch(type) {
	case 1:
		type = SOCK_DGRAM;
		break;

	case 2:
		type = SOCK_STREAM;
		break;

	case 3:
		type = 9; /* Invalid... */
		break;

	case 4:
		type = SOCK_RAW;
		break;

	case 5:
		type = SOCK_RDM;
		break;

	case 6:
		type = SOCK_SEQPACKET;
		break;

	default:
		break;
	}

	return sys_socket(family, type, protocol);
}

asmlinkage int irix_getdomainname(char *name, int len)
{
	int error;

	error = verify_area(VERIFY_WRITE, name, len);
	if (error)
		return error;

	down_read(&uts_sem);
	if(len > (__NEW_UTS_LEN - 1))
		len = __NEW_UTS_LEN - 1;
	error = 0;
	if (copy_to_user(name, system_utsname.domainname, len))
		error = -EFAULT;
	up_read(&uts_sem);

	return error;
}

asmlinkage unsigned long irix_getpagesize(void)
{
	return PAGE_SIZE;
}

asmlinkage int irix_msgsys(int opcode, unsigned long arg0, unsigned long arg1,
			   unsigned long arg2, unsigned long arg3,
			   unsigned long arg4)
{
	switch (opcode) {
	case 0:
		return sys_msgget((key_t) arg0, (int) arg1);
	case 1:
		return sys_msgctl((int) arg0, (int) arg1, (struct msqid_ds *)arg2);
	case 2:
		return sys_msgrcv((int) arg0, (struct msgbuf *) arg1,
				  (size_t) arg2, (long) arg3, (int) arg4);
	case 3:
		return sys_msgsnd((int) arg0, (struct msgbuf *) arg1,
				  (size_t) arg2, (int) arg3);
	default:
		return -EINVAL;
	}
}

asmlinkage int irix_shmsys(int opcode, unsigned long arg0, unsigned long arg1,
			   unsigned long arg2, unsigned long arg3)
{
	switch (opcode) {
	case 0:
		return sys_shmat((int) arg0, (char *)arg1, (int) arg2,
				 (unsigned long *) arg3);
	case 1:
		return sys_shmctl((int)arg0, (int)arg1, (struct shmid_ds *)arg2);
	case 2:
		return sys_shmdt((char *)arg0);
	case 3:
		return sys_shmget((key_t) arg0, (int) arg1, (int) arg2);
	default:
		return -EINVAL;
	}
}

asmlinkage int irix_semsys(int opcode, unsigned long arg0, unsigned long arg1,
			   unsigned long arg2, int arg3)
{
	switch (opcode) {
	case 0:
		return sys_semctl((int) arg0, (int) arg1, (int) arg2,
				  (union semun) arg3);
	case 1:
		return sys_semget((key_t) arg0, (int) arg1, (int) arg2);
	case 2:
		return sys_semop((int) arg0, (struct sembuf *)arg1,
				 (unsigned int) arg2);
	default:
		return -EINVAL;
	}
}

static inline loff_t llseek(struct file *file, loff_t offset, int origin)
{
	loff_t (*fn)(struct file *, loff_t, int);
	loff_t retval;

	fn = default_llseek;
	if (file->f_op && file->f_op->llseek)
        fn = file->f_op->llseek;
	lock_kernel();
	retval = fn(file, offset, origin);
	unlock_kernel();
	return retval;
}

asmlinkage int irix_lseek64(int fd, int _unused, int offhi, int offlow,
                            int origin)
{
	int retval;
	struct file * file;
	loff_t offset;

	retval = -EBADF;
	file = fget(fd);
	if (!file)
		goto bad;
	retval = -EINVAL;
	if (origin > 2)
		goto out_putf;

	offset = llseek(file, ((loff_t) offhi << 32) | offlow, origin);
	retval = (int) offset;

out_putf:
	fput(file);
bad:
	return retval;
}

asmlinkage int irix_sginap(int ticks)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(ticks);
	return 0;
}

asmlinkage int irix_sgikopt(char *istring, char *ostring, int len)
{
	return -EINVAL;
}

asmlinkage int irix_gettimeofday(struct timeval *tv)
{
	int retval;

	retval = copy_to_user(tv, &xtime, sizeof(*tv)) ? -EFAULT : 0;
	return retval;
}

#define IRIX_MAP_AUTOGROW 0x40

asmlinkage unsigned long irix_mmap32(unsigned long addr, size_t len, int prot,
				     int flags, int fd, off_t offset)
{
	struct file *file = NULL;
	unsigned long retval;

	if (!(flags & MAP_ANONYMOUS)) {
		if (!(file = fget(fd)))
			return -EBADF;

		/* Ok, bad taste hack follows, try to think in something else
		 * when reading this.  */
		if (flags & IRIX_MAP_AUTOGROW) {
			unsigned long old_pos;
			long max_size = offset + len;

			if (max_size > file->f_dentry->d_inode->i_size) {
				old_pos = sys_lseek (fd, max_size - 1, 0);
				sys_write (fd, "", 1);
				sys_lseek (fd, old_pos, 0);
			}
		}
	}

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	down_write(&current->mm->mmap_sem);
	retval = do_mmap(file, addr, len, prot, flags, offset);
	up_write(&current->mm->mmap_sem);
	if (file)
		fput(file);

	return retval;
}

asmlinkage int irix_madvise(unsigned long addr, int len, int behavior)
{
	printk("[%s:%d] Wheee.. irix_madvise(%08lx,%d,%d)\n",
	       current->comm, current->pid, addr, len, behavior);

	return -EINVAL;
}

asmlinkage int irix_pagelock(char *addr, int len, int op)
{
	printk("[%s:%d] Wheee.. irix_pagelock(%p,%d,%d)\n",
	       current->comm, current->pid, addr, len, op);

	return -EINVAL;
}

asmlinkage int irix_quotactl(struct pt_regs *regs)
{
	printk("[%s:%d] Wheee.. irix_quotactl()\n",
	       current->comm, current->pid);

	return -EINVAL;
}

asmlinkage int irix_BSDsetpgrp(int pid, int pgrp)
{
	int error;

#ifdef DEBUG_PROCGRPS
	printk("[%s:%d] BSDsetpgrp(%d, %d) ", current->comm, current->pid,
	       pid, pgrp);
#endif
	if(!pid)
		pid = current->pid;

	/* Wheee, weird sysv thing... */
	if ((pgrp == 0) && (pid == current->pid))
		error = sys_setsid();
	else
		error = sys_setpgid(pid, pgrp);

#ifdef DEBUG_PROCGRPS
	printk("error = %d\n", error);
#endif

	return error;
}

asmlinkage int irix_systeminfo(int cmd, char *buf, int cnt)
{
	printk("[%s:%d] Wheee.. irix_systeminfo(%d,%p,%d)\n",
	       current->comm, current->pid, cmd, buf, cnt);

	return -EINVAL;
}

struct iuname {
	char sysname[257], nodename[257], release[257];
	char version[257], machine[257];
	char m_type[257], base_rel[257];
	char _unused0[257], _unused1[257], _unused2[257];
	char _unused3[257], _unused4[257], _unused5[257];
};

asmlinkage int irix_uname(struct iuname *buf)
{
	down_read(&uts_sem);
	if (copy_to_user(system_utsname.sysname, buf->sysname, 65)
	    || copy_to_user(system_utsname.nodename, buf->nodename, 65)
	    || copy_to_user(system_utsname.release, buf->release, 65)
	    || copy_to_user(system_utsname.version, buf->version, 65)
	    || copy_to_user(system_utsname.machine, buf->machine, 65)) {
		return -EFAULT;
	}
	up_read(&uts_sem);

	return 1;
}

#undef DEBUG_XSTAT

static inline u32
linux_to_irix_dev_t (dev_t t)
{
	return MAJOR (t) << 18 | MINOR (t);
}

static inline int irix_xstat32_xlate(struct stat *kb, void *ubuf)
{
	struct xstat32 {
		u32 st_dev, st_pad1[3], st_ino, st_mode, st_nlink, st_uid, st_gid;
		u32 st_rdev, st_pad2[2], st_size, st_pad3;
		u32 st_atime0, st_atime1;
		u32 st_mtime0, st_mtime1;
		u32 st_ctime0, st_ctime1;
		u32 st_blksize, st_blocks;
		char st_fstype[16];
		u32 st_pad4[8];
	} ub;

	ub.st_dev     = linux_to_irix_dev_t (kb->st_dev);
	ub.st_ino     = kb->st_ino;
	ub.st_mode    = kb->st_mode;
	ub.st_nlink   = kb->st_nlink;
	ub.st_uid     = kb->st_uid;
	ub.st_gid     = kb->st_gid;
	ub.st_rdev    = linux_to_irix_dev_t (kb->st_rdev);
	ub.st_size    = kb->st_size;
	ub.st_atime0  = kb->st_atime;
	ub.st_atime1  = 0;
	ub.st_mtime0  = kb->st_mtime;
	ub.st_mtime1  = 0;
	ub.st_ctime0  = kb->st_ctime;
	ub.st_ctime1  = 0;
	ub.st_blksize = kb->st_blksize;
	ub.st_blocks  = kb->st_blocks;
	strcpy (ub.st_fstype, "efs");

	return copy_to_user(ubuf, &ub, sizeof(ub)) ? -EFAULT : 0;
}

static inline void irix_xstat64_xlate(struct stat *sb)
{
	struct xstat64 {
		u32 st_dev; s32 st_pad1[3];
		unsigned long long st_ino;
		u32 st_mode;
		u32 st_nlink; s32 st_uid; s32 st_gid; u32 st_rdev;
		s32 st_pad2[2];
		long long st_size;
		s32 st_pad3;
		struct { s32 tv_sec, tv_nsec; } st_atime, st_mtime, st_ctime;
		s32 st_blksize;
		long long  st_blocks;
		char st_fstype[16];
		s32 st_pad4[8];
	} ks;

	ks.st_dev = linux_to_irix_dev_t (sb->st_dev);
	ks.st_pad1[0] = ks.st_pad1[1] = ks.st_pad1[2] = 0;
	ks.st_ino = (unsigned long long) sb->st_ino;
	ks.st_mode = (u32) sb->st_mode;
	ks.st_nlink = (u32) sb->st_nlink;
	ks.st_uid = (s32) sb->st_uid;
	ks.st_gid = (s32) sb->st_gid;
	ks.st_rdev = linux_to_irix_dev_t (sb->st_rdev);
	ks.st_pad2[0] = ks.st_pad2[1] = 0;
	ks.st_size = (long long) sb->st_size;
	ks.st_pad3 = 0;

	/* XXX hackety hack... */
	ks.st_atime.tv_sec = (s32) sb->st_atime; ks.st_atime.tv_nsec = 0;
	ks.st_mtime.tv_sec = (s32) sb->st_atime; ks.st_mtime.tv_nsec = 0;
	ks.st_ctime.tv_sec = (s32) sb->st_atime; ks.st_ctime.tv_nsec = 0;

	ks.st_blksize = (s32) sb->st_blksize;
	ks.st_blocks = (long long) sb->st_blocks;
	memset(ks.st_fstype, 0, 16);
	ks.st_pad4[0] = ks.st_pad4[1] = ks.st_pad4[2] = ks.st_pad4[3] = 0;
	ks.st_pad4[4] = ks.st_pad4[5] = ks.st_pad4[6] = ks.st_pad4[7] = 0;

	/* Now write it all back. */
	copy_to_user(sb, &ks, sizeof(struct xstat64));
}

extern asmlinkage int sys_newstat(char * filename, struct stat * statbuf);

asmlinkage int irix_xstat(int version, char *filename, struct stat *statbuf)
{
	int retval;

#ifdef DEBUG_XSTAT
	printk("[%s:%d] Wheee.. irix_xstat(%d,%s,%p) ",
	       current->comm, current->pid, version, filename, statbuf);
#endif
	switch(version) {
	case 2: {
		struct stat kb;
		mm_segment_t old_fs;

		old_fs = get_fs(); set_fs(get_ds());
		retval = sys_newstat(filename, &kb);
		set_fs(old_fs);
#ifdef DEBUG_XSTAT
		printk("retval[%d]\n", retval);
#endif
		if(retval)
			goto out;
		retval = irix_xstat32_xlate(&kb, statbuf);
		goto out;
	}

	case 3: {
		retval = sys_newstat(filename, statbuf);
#ifdef DEBUG_XSTAT
		printk("retval[%d]\n", retval);
#endif
		if(retval)
			goto out;

		irix_xstat64_xlate(statbuf);
		retval = 0;
		break;
	}

	default:
		retval = -EINVAL;
		break;
	}

out:
	return retval;
}

extern asmlinkage int sys_newlstat(char * filename, struct stat * statbuf);

asmlinkage int irix_lxstat(int version, char *filename, struct stat *statbuf)
{
	int error;

#ifdef DEBUG_XSTAT
	printk("[%s:%d] Wheee.. irix_lxstat(%d,%s,%p) ",
	       current->comm, current->pid, version, filename, statbuf);
#endif
	switch(version) {
	case 2: {
		struct stat kb;
		mm_segment_t old_fs;

		old_fs = get_fs(); set_fs(get_ds());
		error = sys_newlstat(filename, &kb);
		set_fs(old_fs);
#ifdef DEBUG_XSTAT
		printk("error[%d]\n", error);
#endif
		if(error)
			goto out;
		error = irix_xstat32_xlate(&kb, statbuf);
		goto out;
	}

	case 3: {
		error = sys_newlstat(filename, statbuf);
#ifdef DEBUG_XSTAT
		printk("error[%d]\n", error);
#endif
		if(error)
			goto out;

		irix_xstat64_xlate(statbuf);
		error = 0;
		goto out;
	}

	default:
		error = -EINVAL;
		goto out;
	}

out:
	return error;
}

extern asmlinkage int sys_newfstat(unsigned int fd, struct stat * statbuf);

asmlinkage int irix_fxstat(int version, int fd, struct stat *statbuf)
{
	int error;

#ifdef DEBUG_XSTAT
	printk("[%s:%d] Wheee.. irix_fxstat(%d,%d,%p) ",
	       current->comm, current->pid, version, fd, statbuf);
#endif
	switch(version) {
	case 2: {
		struct stat kb;
		mm_segment_t old_fs;

		old_fs = get_fs(); set_fs(get_ds());
		error = sys_newfstat(fd, &kb);
		set_fs(old_fs);
#ifdef DEBUG_XSTAT
		printk("error[%d]\n", error);
#endif
		if(error)
			goto out;
		error = irix_xstat32_xlate(&kb, statbuf);
		goto out;
	}

	case 3: {
		error = sys_newfstat(fd, statbuf);
#ifdef DEBUG_XSTAT
		printk("error[%d]\n", error);
#endif
		if(error)
			goto out;

		irix_xstat64_xlate(statbuf);
		error = 0;
		goto out;
	}

	default:
		error = -EINVAL;
		goto out;
	}

out:
	return error;
}

extern asmlinkage int sys_mknod(const char * filename, int mode, dev_t dev);

asmlinkage int irix_xmknod(int ver, char *filename, int mode, dev_t dev)
{
	int retval;

	printk("[%s:%d] Wheee.. irix_xmknod(%d,%s,%x,%x)\n",
	       current->comm, current->pid, ver, filename, mode, (int) dev);

	switch(ver) {
	case 2:
		retval = sys_mknod(filename, mode, dev);
		break;

	default:
		retval = -EINVAL;
		break;
	};

	return retval;
}

asmlinkage int irix_swapctl(int cmd, char *arg)
{
	printk("[%s:%d] Wheee.. irix_swapctl(%d,%p)\n",
	       current->comm, current->pid, cmd, arg);

	return -EINVAL;
}

struct irix_statvfs {
	u32 f_bsize; u32 f_frsize; u32 f_blocks;
	u32 f_bfree; u32 f_bavail; u32 f_files; u32 f_ffree; u32 f_favail;
	u32 f_fsid; char f_basetype[16];
	u32 f_flag; u32 f_namemax;
	char	f_fstr[32]; u32 f_filler[16];
};

asmlinkage int irix_statvfs(char *fname, struct irix_statvfs *buf)
{
	struct nameidata nd;
	struct statfs kbuf;
	int error, i;

	printk("[%s:%d] Wheee.. irix_statvfs(%s,%p)\n",
	       current->comm, current->pid, fname, buf);
	error = verify_area(VERIFY_WRITE, buf, sizeof(struct irix_statvfs));
	if (error)
		goto out;
	error = user_path_walk(fname, &nd);
	if (error)
		goto out;
	error = vfs_statfs(nd.dentry->d_inode->i_sb, &kbuf);
	if (error)
		goto dput_and_out;

	__put_user(kbuf.f_bsize, &buf->f_bsize);
	__put_user(kbuf.f_frsize, &buf->f_frsize);
	__put_user(kbuf.f_blocks, &buf->f_blocks);
	__put_user(kbuf.f_bfree, &buf->f_bfree);
	__put_user(kbuf.f_bfree, &buf->f_bavail);  /* XXX hackety hack... */
	__put_user(kbuf.f_files, &buf->f_files);
	__put_user(kbuf.f_ffree, &buf->f_ffree);
	__put_user(kbuf.f_ffree, &buf->f_favail);  /* XXX hackety hack... */
#ifdef __MIPSEB__
	__put_user(kbuf.f_fsid.val[1], &buf->f_fsid);
#else
	__put_user(kbuf.f_fsid.val[0], &buf->f_fsid);
#endif
	for (i = 0; i < 16; i++)
		__put_user(0, &buf->f_basetype[i]);
	__put_user(0, &buf->f_flag);
	__put_user(kbuf.f_namelen, &buf->f_namemax);
	for (i = 0; i < 32; i++)
		__put_user(0, &buf->f_fstr[i]);

	error = 0;

dput_and_out:
	path_release(&nd);
out:
	return error;
}

asmlinkage int irix_fstatvfs(int fd, struct irix_statvfs *buf)
{
	struct statfs kbuf;
	struct file *file;
	int error, i;

	printk("[%s:%d] Wheee.. irix_fstatvfs(%d,%p)\n",
	       current->comm, current->pid, fd, buf);

	error = verify_area(VERIFY_WRITE, buf, sizeof(struct irix_statvfs));
	if (error)
		goto out;
	if (!(file = fget(fd))) {
		error = -EBADF;
		goto out;
	}
	error = vfs_statfs(file->f_dentry->d_inode->i_sb, &kbuf);
	if (error)
		goto out_f;

	__put_user(kbuf.f_bsize, &buf->f_bsize);
	__put_user(kbuf.f_frsize, &buf->f_frsize);
	__put_user(kbuf.f_blocks, &buf->f_blocks);
	__put_user(kbuf.f_bfree, &buf->f_bfree);
	__put_user(kbuf.f_bfree, &buf->f_bavail); /* XXX hackety hack... */
	__put_user(kbuf.f_files, &buf->f_files);
	__put_user(kbuf.f_ffree, &buf->f_ffree);
	__put_user(kbuf.f_ffree, &buf->f_favail); /* XXX hackety hack... */
#ifdef __MIPSEB__
	__put_user(kbuf.f_fsid.val[1], &buf->f_fsid);
#else
	__put_user(kbuf.f_fsid.val[0], &buf->f_fsid);
#endif
	for(i = 0; i < 16; i++)
		__put_user(0, &buf->f_basetype[i]);
	__put_user(0, &buf->f_flag);
	__put_user(kbuf.f_namelen, &buf->f_namemax);
	__clear_user(&buf->f_fstr, sizeof(buf->f_fstr));

out_f:
	fput(file);
out:
	return error;
}

asmlinkage int irix_priocntl(struct pt_regs *regs)
{
	printk("[%s:%d] Wheee.. irix_priocntl()\n",
	       current->comm, current->pid);

	return -EINVAL;
}

asmlinkage int irix_sigqueue(int pid, int sig, int code, int val)
{
	printk("[%s:%d] Wheee.. irix_sigqueue(%d,%d,%d,%d)\n",
	       current->comm, current->pid, pid, sig, code, val);

	return -EINVAL;
}

extern asmlinkage int sys_truncate(const char * path, unsigned long length);
extern asmlinkage int sys_ftruncate(unsigned int fd, unsigned long length);

asmlinkage int irix_truncate64(char *name, int pad, int size1, int size2)
{
	int retval;

	if (size1) {
		retval = -EINVAL;
		goto out;
	}
	retval = sys_truncate(name, size2);

out:
	return retval;
}

asmlinkage int irix_ftruncate64(int fd, int pad, int size1, int size2)
{
	int retval;

	if (size1) {
		retval = -EINVAL;
		goto out;
	}
	retval = sys_ftruncate(fd, size2);

out:
	return retval;
}

extern asmlinkage unsigned long
sys_mmap(unsigned long addr, size_t len, int prot, int flags, int fd,
         off_t offset);

asmlinkage int irix_mmap64(struct pt_regs *regs)
{
	int len, prot, flags, fd, off1, off2, error, base = 0;
	unsigned long addr, pgoff, *sp;
	struct file *file = NULL;

	if (regs->regs[2] == 1000)
		base = 1;
	sp = (unsigned long *) (regs->regs[29] + 16);
	addr = regs->regs[base + 4];
	len = regs->regs[base + 5];
	prot = regs->regs[base + 6];
	if (!base) {
		flags = regs->regs[base + 7];
		error = verify_area(VERIFY_READ, sp, (4 * sizeof(unsigned long)));
		if(error)
			goto out;
		fd = sp[0];
		__get_user(off1, &sp[1]);
		__get_user(off2, &sp[2]);
	} else {
		error = verify_area(VERIFY_READ, sp, (5 * sizeof(unsigned long)));
		if(error)
			goto out;
		__get_user(flags, &sp[0]);
		__get_user(fd, &sp[1]);
		__get_user(off1, &sp[2]);
		__get_user(off2, &sp[3]);
	}

	if (off1 & PAGE_MASK) {
		error = -EOVERFLOW;
		goto out;
	}

	pgoff = (off1 << (32 - PAGE_SHIFT)) | (off2 >> PAGE_SHIFT);

	if (!(flags & MAP_ANONYMOUS)) {
		if (!(file = fget(fd))) {
			error = -EBADF;
			goto out;
		}

		/* Ok, bad taste hack follows, try to think in something else
		   when reading this */
		if (flags & IRIX_MAP_AUTOGROW) {
			unsigned long old_pos;
			long max_size = off2 + len;

			if (max_size > file->f_dentry->d_inode->i_size) {
				old_pos = sys_lseek (fd, max_size - 1, 0);
				sys_write (fd, "", 1);
				sys_lseek (fd, old_pos, 0);
			}
		}
	}

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);

out:
	return error;
}

asmlinkage int irix_dmi(struct pt_regs *regs)
{
	printk("[%s:%d] Wheee.. irix_dmi()\n",
	       current->comm, current->pid);

	return -EINVAL;
}

asmlinkage int irix_pread(int fd, char *buf, int cnt, int off64,
			  int off1, int off2)
{
	printk("[%s:%d] Wheee.. irix_pread(%d,%p,%d,%d,%d,%d)\n",
	       current->comm, current->pid, fd, buf, cnt, off64, off1, off2);

	return -EINVAL;
}

asmlinkage int irix_pwrite(int fd, char *buf, int cnt, int off64,
			   int off1, int off2)
{
	printk("[%s:%d] Wheee.. irix_pwrite(%d,%p,%d,%d,%d,%d)\n",
	       current->comm, current->pid, fd, buf, cnt, off64, off1, off2);

	return -EINVAL;
}

asmlinkage int irix_sgifastpath(int cmd, unsigned long arg0, unsigned long arg1,
				unsigned long arg2, unsigned long arg3,
				unsigned long arg4, unsigned long arg5)
{
	printk("[%s:%d] Wheee.. irix_fastpath(%d,%08lx,%08lx,%08lx,%08lx,"
	       "%08lx,%08lx)\n",
	       current->comm, current->pid, cmd, arg0, arg1, arg2,
	       arg3, arg4, arg5);

	return -EINVAL;
}

struct irix_statvfs64 {
	u32  f_bsize; u32 f_frsize;
	u64  f_blocks; u64 f_bfree; u64 f_bavail;
	u64  f_files; u64 f_ffree; u64 f_favail;
	u32  f_fsid;
	char f_basetype[16];
	u32  f_flag; u32 f_namemax;
	char f_fstr[32];
	u32  f_filler[16];
};

asmlinkage int irix_statvfs64(char *fname, struct irix_statvfs64 *buf)
{
	struct nameidata nd;
	struct statfs kbuf;
	int error, i;

	printk("[%s:%d] Wheee.. irix_statvfs(%s,%p)\n",
	       current->comm, current->pid, fname, buf);
	error = verify_area(VERIFY_WRITE, buf, sizeof(struct irix_statvfs));
	if(error)
		goto out;
	error = user_path_walk(fname, &nd);
	if (error)
		goto out;
	error = vfs_statfs(nd.dentry->d_inode->i_sb, &kbuf);
	if (error)
		goto dput_and_out;

	__put_user(kbuf.f_bsize, &buf->f_bsize);
	__put_user(kbuf.f_frsize, &buf->f_frsize);
	__put_user(kbuf.f_blocks, &buf->f_blocks);
	__put_user(kbuf.f_bfree, &buf->f_bfree);
	__put_user(kbuf.f_bfree, &buf->f_bavail);  /* XXX hackety hack... */
	__put_user(kbuf.f_files, &buf->f_files);
	__put_user(kbuf.f_ffree, &buf->f_ffree);
	__put_user(kbuf.f_ffree, &buf->f_favail);  /* XXX hackety hack... */
#ifdef __MIPSEB__
	__put_user(kbuf.f_fsid.val[1], &buf->f_fsid);
#else
	__put_user(kbuf.f_fsid.val[0], &buf->f_fsid);
#endif
	for(i = 0; i < 16; i++)
		__put_user(0, &buf->f_basetype[i]);
	__put_user(0, &buf->f_flag);
	__put_user(kbuf.f_namelen, &buf->f_namemax);
	for(i = 0; i < 32; i++)
		__put_user(0, &buf->f_fstr[i]);

	error = 0;

dput_and_out:
	path_release(&nd);
out:
	return error;
}

asmlinkage int irix_fstatvfs64(int fd, struct irix_statvfs *buf)
{
	struct statfs kbuf;
	struct file *file;
	int error, i;

	printk("[%s:%d] Wheee.. irix_fstatvfs(%d,%p)\n",
	       current->comm, current->pid, fd, buf);

	error = verify_area(VERIFY_WRITE, buf, sizeof(struct irix_statvfs));
	if (error)
		goto out;
	if (!(file = fget(fd))) {
		error = -EBADF;
		goto out;
	}
	error = vfs_statfs(file->f_dentry->d_inode->i_sb, &kbuf);
	if (error)
		goto out_f;

	__put_user(kbuf.f_bsize, &buf->f_bsize);
	__put_user(kbuf.f_frsize, &buf->f_frsize);
	__put_user(kbuf.f_blocks, &buf->f_blocks);
	__put_user(kbuf.f_bfree, &buf->f_bfree);
	__put_user(kbuf.f_bfree, &buf->f_bavail);  /* XXX hackety hack... */
	__put_user(kbuf.f_files, &buf->f_files);
	__put_user(kbuf.f_ffree, &buf->f_ffree);
	__put_user(kbuf.f_ffree, &buf->f_favail);  /* XXX hackety hack... */
#ifdef __MIPSEB__
	__put_user(kbuf.f_fsid.val[1], &buf->f_fsid);
#else
	__put_user(kbuf.f_fsid.val[0], &buf->f_fsid);
#endif
	for(i = 0; i < 16; i++)
		__put_user(0, &buf->f_basetype[i]);
	__put_user(0, &buf->f_flag);
	__put_user(kbuf.f_namelen, &buf->f_namemax);
	__clear_user(buf->f_fstr, sizeof(buf->f_fstr[i]));

out_f:
	fput(file);
out:
	return error;
}

asmlinkage int irix_getmountid(char *fname, unsigned long *midbuf)
{
	int err;

	printk("[%s:%d] irix_getmountid(%s, %p)\n",
	       current->comm, current->pid, fname, midbuf);
	err = verify_area(VERIFY_WRITE, midbuf, (sizeof(unsigned long) * 4));
	if (err)
		return err;

	/*
	 * The idea with this system call is that when trying to determine
	 * 'pwd' and it's a toss-up for some reason, userland can use the
	 * fsid of the filesystem to try and make the right decision, but
	 * we don't have this so for now. XXX
	 */
	err |= __put_user(0, &midbuf[0]);
	err |= __put_user(0, &midbuf[1]);
	err |= __put_user(0, &midbuf[2]);
	err |= __put_user(0, &midbuf[3]);

	return err;
}

asmlinkage int irix_nsproc(unsigned long entry, unsigned long mask,
			   unsigned long arg, unsigned long sp, int slen)
{
	printk("[%s:%d] Wheee.. irix_nsproc(%08lx,%08lx,%08lx,%08lx,%d)\n",
	       current->comm, current->pid, entry, mask, arg, sp, slen);

	return -EINVAL;
}

#undef DEBUG_GETDENTS

struct irix_dirent32 {
	u32  d_ino;
	u32  d_off;
	unsigned short  d_reclen;
	char d_name[1];
};

struct irix_dirent32_callback {
	struct irix_dirent32 *current_dir;
	struct irix_dirent32 *previous;
	int count;
	int error;
};

#define NAME_OFFSET32(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP32(x) (((x)+sizeof(u32)-1) & ~(sizeof(u32)-1))

static int irix_filldir32(void *__buf, const char *name, int namlen,
                          loff_t offset, ino_t ino, unsigned int d_type)
{
	struct irix_dirent32 *dirent;
	struct irix_dirent32_callback *buf =
		 (struct irix_dirent32_callback *)__buf;
	unsigned short reclen = ROUND_UP32(NAME_OFFSET32(dirent) + namlen + 1);

#ifdef DEBUG_GETDENTS
	printk("\nirix_filldir32[reclen<%d>namlen<%d>count<%d>]",
	       reclen, namlen, buf->count);
#endif
	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent)
		__put_user(offset, &dirent->d_off);
	dirent = buf->current_dir;
	buf->previous = dirent;
	__put_user(ino, &dirent->d_ino);
	__put_user(reclen, &dirent->d_reclen);
	copy_to_user(dirent->d_name, name, namlen);
	__put_user(0, &dirent->d_name[namlen]);
	((char *) dirent) += reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;

	return 0;
}

asmlinkage int irix_ngetdents(unsigned int fd, void * dirent,
	unsigned int count, int *eob)
{
	struct file *file;
	struct irix_dirent32 *lastdirent;
	struct irix_dirent32_callback buf;
	int error;

#ifdef DEBUG_GETDENTS
	printk("[%s:%d] ngetdents(%d, %p, %d, %p) ", current->comm,
	       current->pid, fd, dirent, count, eob);
#endif
	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = (struct irix_dirent32 *) dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, irix_filldir32, &buf);
	if (error < 0)
		goto out_putf;

	error = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = count - buf.count;
	}

	if (put_user(0, eob) < 0) {
		error = -EFAULT;
		goto out_putf;
	}

#ifdef DEBUG_GETDENTS
	printk("eob=%d returning %d\n", *eob, count - buf.count);
#endif
	error = count - buf.count;

out_putf:
	fput(file);
out:
	return error;
}

struct irix_dirent64 {
	u64            d_ino;
	u64            d_off;
	unsigned short d_reclen;
	char           d_name[1];
};

struct irix_dirent64_callback {
	struct irix_dirent64 *curr;
	struct irix_dirent64 *previous;
	int count;
	int error;
};

#define NAME_OFFSET64(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP64(x) (((x)+sizeof(u64)-1) & ~(sizeof(u64)-1))

static int irix_filldir64(void * __buf, const char * name, int namlen,
			  loff_t offset, ino_t ino, unsigned int d_type)
{
	struct irix_dirent64 *dirent;
	struct irix_dirent64_callback * buf =
		(struct irix_dirent64_callback *) __buf;
	unsigned short reclen = ROUND_UP64(NAME_OFFSET64(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent)
		__put_user(offset, &dirent->d_off);
	dirent = buf->curr;
	buf->previous = dirent;
	__put_user(ino, &dirent->d_ino);
	__put_user(reclen, &dirent->d_reclen);
	__copy_to_user(dirent->d_name, name, namlen);
	__put_user(0, &dirent->d_name[namlen]);
	((char *) dirent) += reclen;
	buf->curr = dirent;
	buf->count -= reclen;

	return 0;
}

asmlinkage int irix_getdents64(int fd, void *dirent, int cnt)
{
	struct file *file;
	struct irix_dirent64 *lastdirent;
	struct irix_dirent64_callback buf;
	int error;

#ifdef DEBUG_GETDENTS
	printk("[%s:%d] getdents64(%d, %p, %d) ", current->comm,
	       current->pid, fd, dirent, cnt);
#endif
	error = -EBADF;
	if (!(file = fget(fd)))
		goto out;

	error = -EFAULT;
	if (!access_ok(VERIFY_WRITE, dirent, cnt))
		goto out_f;

	error = -EINVAL;
	if (cnt < (sizeof(struct irix_dirent64) + 255))
		goto out_f;

	buf.curr = (struct irix_dirent64 *) dirent;
	buf.previous = NULL;
	buf.count = cnt;
	buf.error = 0;
	error = vfs_readdir(file, irix_filldir64, &buf);
	if (error < 0)
		goto out_f;
	lastdirent = buf.previous;
	if (!lastdirent) {
		error = buf.error;
		goto out_f;
	}
	lastdirent->d_off = (u64) file->f_pos;
#ifdef DEBUG_GETDENTS
	printk("returning %d\n", cnt - buf.count);
#endif
	error = cnt - buf.count;

out_f:
	fput(file);
out:
	return error;
}

asmlinkage int irix_ngetdents64(int fd, void *dirent, int cnt, int *eob)
{
	struct file *file;
	struct irix_dirent64 *lastdirent;
	struct irix_dirent64_callback buf;
	int error;

#ifdef DEBUG_GETDENTS
	printk("[%s:%d] ngetdents64(%d, %p, %d) ", current->comm,
	       current->pid, fd, dirent, cnt);
#endif
	error = -EBADF;
	if (!(file = fget(fd)))
		goto out;

	error = -EFAULT;
	if (!access_ok(VERIFY_WRITE, dirent, cnt) ||
	    !access_ok(VERIFY_WRITE, eob, sizeof(*eob)))
		goto out_f;

	error = -EINVAL;
	if (cnt < (sizeof(struct irix_dirent64) + 255))
		goto out_f;

	*eob = 0;
	buf.curr = (struct irix_dirent64 *) dirent;
	buf.previous = NULL;
	buf.count = cnt;
	buf.error = 0;
	error = vfs_readdir(file, irix_filldir64, &buf);
	if (error < 0)
		goto out_f;
	lastdirent = buf.previous;
	if (!lastdirent) {
		error = buf.error;
		goto out_f;
	}
	lastdirent->d_off = (u64) file->f_pos;
#ifdef DEBUG_GETDENTS
	printk("eob=%d returning %d\n", *eob, cnt - buf.count);
#endif
	error = cnt - buf.count;

out_f:
	fput(file);
out:
	return error;
}

asmlinkage int irix_uadmin(unsigned long op, unsigned long func, unsigned long arg)
{
	int retval;

	switch (op) {
	case 1:
		/* Reboot */
		printk("[%s:%d] irix_uadmin: Wants to reboot...\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 2:
		/* Shutdown */
		printk("[%s:%d] irix_uadmin: Wants to shutdown...\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 4:
		/* Remount-root */
		printk("[%s:%d] irix_uadmin: Wants to remount root...\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 8:
		/* Kill all tasks. */
		printk("[%s:%d] irix_uadmin: Wants to kill all tasks...\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 256:
		/* Set magic mushrooms... */
		printk("[%s:%d] irix_uadmin: Wants to set magic mushroom[%d]...\n",
		       current->comm, current->pid, (int) func);
		retval = -EINVAL;
		goto out;

	default:
		printk("[%s:%d] irix_uadmin: Unknown operation [%d]...\n",
		       current->comm, current->pid, (int) op);
		retval = -EINVAL;
		goto out;
	};

out:
	return retval;
}

asmlinkage int irix_utssys(char *inbuf, int arg, int type, char *outbuf)
{
	int retval;

	switch(type) {
	case 0:
		/* uname() */
		retval = irix_uname((struct iuname *)inbuf);
		goto out;

	case 2:
		/* ustat() */
		printk("[%s:%d] irix_utssys: Wants to do ustat()\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 3:
		/* fusers() */
		printk("[%s:%d] irix_utssys: Wants to do fusers()\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	default:
		printk("[%s:%d] irix_utssys: Wants to do unknown type[%d]\n",
		       current->comm, current->pid, (int) type);
		retval = -EINVAL;
		goto out;
	}

out:
	return retval;
}

#undef DEBUG_FCNTL

extern asmlinkage long sys_fcntl(unsigned int fd, unsigned int cmd,
				 unsigned long arg);

#define IRIX_F_ALLOCSP 10

asmlinkage int irix_fcntl(int fd, int cmd, int arg)
{
	int retval;

#ifdef DEBUG_FCNTL
	printk("[%s:%d] irix_fcntl(%d, %d, %d) ", current->comm,
	       current->pid, fd, cmd, arg);
#endif
	if (cmd == IRIX_F_ALLOCSP){
		return 0;
	}
	retval = sys_fcntl(fd, cmd, arg);
#ifdef DEBUG_FCNTL
	printk("%d\n", retval);
#endif
	return retval;
}

asmlinkage int irix_ulimit(int cmd, int arg)
{
	int retval;

	switch(cmd) {
	case 1:
		printk("[%s:%d] irix_ulimit: Wants to get file size limit.\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 2:
		printk("[%s:%d] irix_ulimit: Wants to set file size limit.\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 3:
		printk("[%s:%d] irix_ulimit: Wants to get brk limit.\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	case 4:
#if 0
		printk("[%s:%d] irix_ulimit: Wants to get fd limit.\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;
#endif
		retval = current->rlim[RLIMIT_NOFILE].rlim_cur;
		goto out;

	case 5:
		printk("[%s:%d] irix_ulimit: Wants to get txt offset.\n",
		       current->comm, current->pid);
		retval = -EINVAL;
		goto out;

	default:
		printk("[%s:%d] irix_ulimit: Unknown command [%d].\n",
		       current->comm, current->pid, cmd);
		retval = -EINVAL;
		goto out;
	}
out:
	return retval;
}

asmlinkage int irix_unimp(struct pt_regs *regs)
{
	printk("irix_unimp [%s:%d] v0=%d v1=%d a0=%08lx a1=%08lx a2=%08lx "
	       "a3=%08lx\n", current->comm, current->pid,
	       (int) regs->regs[2], (int) regs->regs[3],
	       regs->regs[4], regs->regs[5], regs->regs[6], regs->regs[7]);

	return -ENOSYS;
}
