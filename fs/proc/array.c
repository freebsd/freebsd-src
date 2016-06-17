/*
 *  linux/fs/proc/array.c
 *
 *  Copyright (C) 1992  by Linus Torvalds
 *  based on ideas by Darren Senn
 *
 * Fixes:
 * Michael. K. Johnson: stat,statm extensions.
 *                      <johnsonm@stolaf.edu>
 *
 * Pauline Middelink :  Made cmdline,envline only break at '\0's, to
 *                      make sure SET_PROCTITLE works. Also removed
 *                      bad '!' which forced address recalculation for
 *                      EVERY character on the current page.
 *                      <middelin@polyware.iaf.nl>
 *
 * Danny ter Haar    :	added cpuinfo
 *			<dth@cistron.nl>
 *
 * Alessandro Rubini :  profile extension.
 *                      <rubini@ipvvis.unipv.it>
 *
 * Jeff Tranter      :  added BogoMips field to cpuinfo
 *                      <Jeff_Tranter@Mitel.COM>
 *
 * Bruno Haible      :  remove 4K limit for the maps file
 *			<haible@ma2s2.mathematik.uni-karlsruhe.de>
 *
 * Yves Arrouye      :  remove removal of trailing spaces in get_array.
 *			<Yves.Arrouye@marin.fdn.fr>
 *
 * Jerome Forissier  :  added per-CPU time information to /proc/stat
 *                      and /proc/<pid>/cpu extension
 *                      <forissier@isia.cma.fr>
 *			- Incorporation and non-SMP safe operation
 *			of forissier patch in 2.1.78 by
 *			Hans Marcus <crowbar@concepts.nl>
 *
 * aeb@cwi.nl        :  /proc/partitions
 *
 *
 * Alan Cox	     :  security fixes.
 *			<Alan.Cox@linux.org>
 *
 * Al Viro           :  safe handling of mm_struct
 *
 * Gerhard Wichert   :  added BIGMEM support
 * Siemens AG           <Gerhard.Wichert@pdb.siemens.de>
 *
 * Al Viro & Jeff Garzik :  moved most of the thing into base.c and
 *			 :  proc_misc.c. The rest may eventually go into
 *			 :  base.c too.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/mman.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/signal.h>
#include <linux/highmem.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/processor.h>

/* Gcc optimizes away "strlen(x)" for constant x */
#define ADDBUF(buffer, string) \
do { memcpy(buffer, string, strlen(string)); \
     buffer += strlen(string); } while (0)

static inline char * task_name(struct task_struct *p, char * buf)
{
	int i;
	char * name;

	ADDBUF(buf, "Name:\t");
	name = p->comm;
	i = sizeof(p->comm);
	do {
		unsigned char c = *name;
		name++;
		i--;
		*buf = c;
		if (!c)
			break;
		if (c == '\\') {
			buf[1] = c;
			buf += 2;
			continue;
		}
		if (c == '\n') {
			buf[0] = '\\';
			buf[1] = 'n';
			buf += 2;
			continue;
		}
		buf++;
	} while (i);
	*buf = '\n';
	return buf+1;
}

/*
 * The task state array is a strange "bitmap" of
 * reasons to sleep. Thus "running" is zero, and
 * you can test for combinations of others with
 * simple bit tests.
 */
static const char *task_state_array[] = {
	"R (running)",		/*  0 */
	"S (sleeping)",		/*  1 */
	"D (disk sleep)",	/*  2 */
	"Z (zombie)",		/*  4 */
	"T (stopped)",		/*  8 */
	"W (paging)"		/* 16 */
};

static inline const char * get_task_state(struct task_struct *tsk)
{
	unsigned int state = tsk->state & (TASK_RUNNING |
					   TASK_INTERRUPTIBLE |
					   TASK_UNINTERRUPTIBLE |
					   TASK_ZOMBIE |
					   TASK_STOPPED);
	const char **p = &task_state_array[0];

	while (state) {
		p++;
		state >>= 1;
	}
	return *p;
}

static inline char * task_state(struct task_struct *p, char *buffer)
{
	int g;

	read_lock(&tasklist_lock);
	buffer += sprintf(buffer,
		"State:\t%s\n"
		"Tgid:\t%d\n"
		"Pid:\t%d\n"
		"PPid:\t%d\n"
		"TracerPid:\t%d\n"
		"Uid:\t%d\t%d\t%d\t%d\n"
		"Gid:\t%d\t%d\t%d\t%d\n",
		get_task_state(p), p->tgid,
		p->pid, p->pid ? p->p_opptr->pid : 0, 0,
		p->uid, p->euid, p->suid, p->fsuid,
		p->gid, p->egid, p->sgid, p->fsgid);
	read_unlock(&tasklist_lock);	
	task_lock(p);
	buffer += sprintf(buffer,
		"FDSize:\t%d\n"
		"Groups:\t",
		p->files ? p->files->max_fds : 0);
	task_unlock(p);

	for (g = 0; g < p->ngroups; g++)
		buffer += sprintf(buffer, "%d ", p->groups[g]);

	buffer += sprintf(buffer, "\n");
	return buffer;
}

static inline char * task_mem(struct mm_struct *mm, char *buffer)
{
	struct vm_area_struct * vma;
	unsigned long data = 0, stack = 0;
	unsigned long exec = 0, lib = 0;

	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		unsigned long len = (vma->vm_end - vma->vm_start) >> 10;
		if (!vma->vm_file) {
			data += len;
			if (vma->vm_flags & VM_GROWSDOWN)
				stack += len;
			continue;
		}
		if (vma->vm_flags & VM_WRITE)
			continue;
		if (vma->vm_flags & VM_EXEC) {
			exec += len;
			if (vma->vm_flags & VM_EXECUTABLE)
				continue;
			lib += len;
		}
	}
	buffer += sprintf(buffer,
		"VmSize:\t%8lu kB\n"
		"VmLck:\t%8lu kB\n"
		"VmRSS:\t%8lu kB\n"
		"VmData:\t%8lu kB\n"
		"VmStk:\t%8lu kB\n"
		"VmExe:\t%8lu kB\n"
		"VmLib:\t%8lu kB\n",
		mm->total_vm << (PAGE_SHIFT-10),
		mm->locked_vm << (PAGE_SHIFT-10),
		mm->rss << (PAGE_SHIFT-10),
		data - stack, stack,
		exec - lib, lib);
	up_read(&mm->mmap_sem);
	return buffer;
}

static void collect_sigign_sigcatch(struct task_struct *p, sigset_t *ign,
				    sigset_t *catch)
{
	struct k_sigaction *k;
	int i;

	sigemptyset(ign);
	sigemptyset(catch);

	spin_lock_irq(&p->sigmask_lock);

	if (p->sig) {
		k = p->sig->action;
		for (i = 1; i <= _NSIG; ++i, ++k) {
			if (k->sa.sa_handler == SIG_IGN)
				sigaddset(ign, i);
			else if (k->sa.sa_handler != SIG_DFL)
				sigaddset(catch, i);
		}
	}
	spin_unlock_irq(&p->sigmask_lock);
}

static inline char * task_sig(struct task_struct *p, char *buffer)
{
	sigset_t ign, catch;

	buffer += sprintf(buffer, "SigPnd:\t");
	buffer = render_sigset_t(&p->pending.signal, buffer);
	*buffer++ = '\n';
	buffer += sprintf(buffer, "SigBlk:\t");
	buffer = render_sigset_t(&p->blocked, buffer);
	*buffer++ = '\n';

	collect_sigign_sigcatch(p, &ign, &catch);
	buffer += sprintf(buffer, "SigIgn:\t");
	buffer = render_sigset_t(&ign, buffer);
	*buffer++ = '\n';
	buffer += sprintf(buffer, "SigCgt:\t"); /* Linux 2.0 uses "SigCgt" */
	buffer = render_sigset_t(&catch, buffer);
	*buffer++ = '\n';

	return buffer;
}

static inline char *task_cap(struct task_struct *p, char *buffer)
{
    return buffer + sprintf(buffer, "CapInh:\t%016x\n"
			    "CapPrm:\t%016x\n"
			    "CapEff:\t%016x\n",
			    cap_t(p->cap_inheritable),
			    cap_t(p->cap_permitted),
			    cap_t(p->cap_effective));
}


int proc_pid_status(struct task_struct *task, char * buffer)
{
	char * orig = buffer;
	struct mm_struct *mm;

	buffer = task_name(task, buffer);
	buffer = task_state(task, buffer);
	task_lock(task);
	mm = task->mm;
	if(mm)
		atomic_inc(&mm->mm_users);
	task_unlock(task);
	if (mm) {
		buffer = task_mem(mm, buffer);
		mmput(mm);
	}
	buffer = task_sig(task, buffer);
	buffer = task_cap(task, buffer);
#if defined(CONFIG_ARCH_S390)
	buffer = task_show_regs(task, buffer);
#endif
	return buffer - orig;
}

int proc_pid_stat(struct task_struct *task, char * buffer)
{
	unsigned long vsize, eip, esp, wchan;
	long priority, nice;
	int tty_pgrp = -1, tty_nr = 0;
	sigset_t sigign, sigcatch;
	char state;
	int res;
	pid_t ppid;
	struct mm_struct *mm;

	state = *get_task_state(task);
	vsize = eip = esp = 0;
	task_lock(task);
	mm = task->mm;
	if(mm)
		atomic_inc(&mm->mm_users);
	if (task->tty) {
		tty_pgrp = task->tty->pgrp;
		tty_nr = kdev_t_to_nr(task->tty->device);
	}
	task_unlock(task);
	if (mm) {
		struct vm_area_struct *vma;
		down_read(&mm->mmap_sem);
		vma = mm->mmap;
		while (vma) {
			vsize += vma->vm_end - vma->vm_start;
			vma = vma->vm_next;
		}
		eip = KSTK_EIP(task);
		esp = KSTK_ESP(task);
		up_read(&mm->mmap_sem);
	}

	wchan = get_wchan(task);

	collect_sigign_sigcatch(task, &sigign, &sigcatch);

	/* scale priority and nice values from timeslices to -20..20 */
	/* to make it look like a "normal" Unix priority/nice value  */
	priority = task->counter;
	priority = 20 - (priority * 10 + DEF_COUNTER / 2) / DEF_COUNTER;
	nice = task->nice;

	read_lock(&tasklist_lock);
	ppid = task->pid ? task->p_opptr->pid : 0;
	read_unlock(&tasklist_lock);
	res = sprintf(buffer,"%d (%s) %c %d %d %d %d %d %lu %lu \
%lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu \
%lu %lu %lu %lu %lu %lu %lu %lu %d %d\n",
		task->pid,
		task->comm,
		state,
		ppid,
		task->pgrp,
		task->session,
	        tty_nr,
		tty_pgrp,
		task->flags,
		task->min_flt,
		task->cmin_flt,
		task->maj_flt,
		task->cmaj_flt,
		task->times.tms_utime,
		task->times.tms_stime,
		task->times.tms_cutime,
		task->times.tms_cstime,
		priority,
		nice,
		0UL /* removed */,
		task->it_real_value,
		task->start_time,
		vsize,
		mm ? mm->rss : 0, /* you might want to shift this left 3 */
		task->rlim[RLIMIT_RSS].rlim_cur,
		mm ? mm->start_code : 0,
		mm ? mm->end_code : 0,
		mm ? mm->start_stack : 0,
		esp,
		eip,
		/* The signal information here is obsolete.
		 * It must be decimal for Linux 2.0 compatibility.
		 * Use /proc/#/status for real-time signals.
		 */
		task->pending.signal.sig[0] & 0x7fffffffUL,
		task->blocked.sig[0] & 0x7fffffffUL,
		sigign      .sig[0] & 0x7fffffffUL,
		sigcatch    .sig[0] & 0x7fffffffUL,
		wchan,
		task->nswap,
		task->cnswap,
		task->exit_signal,
		task->processor);
	if(mm)
		mmput(mm);
	return res;
}
		
static inline void statm_pte_range(pmd_t * pmd, unsigned long address, unsigned long size,
	int * pages, int * shared, int * dirty, int * total)
{
	pte_t * pte;
	unsigned long end;

	if (pmd_none(*pmd))
		return;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		return;
	}
	pte = pte_offset(pmd, address);
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		pte_t page = *pte;
		struct page *ptpage;

		address += PAGE_SIZE;
		pte++;
		if (pte_none(page))
			continue;
		++*total;
		if (!pte_present(page))
			continue;
		ptpage = pte_page(page);
		if ((!VALID_PAGE(ptpage)) || PageReserved(ptpage))
			continue;
		++*pages;
		if (pte_dirty(page))
			++*dirty;
		if (page_count(pte_page(page)) > 1)
			++*shared;
	} while (address < end);
}

static inline void statm_pmd_range(pgd_t * pgd, unsigned long address, unsigned long size,
	int * pages, int * shared, int * dirty, int * total)
{
	pmd_t * pmd;
	unsigned long end;

	if (pgd_none(*pgd))
		return;
	if (pgd_bad(*pgd)) {
		pgd_ERROR(*pgd);
		pgd_clear(pgd);
		return;
	}
	pmd = pmd_offset(pgd, address);
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		statm_pte_range(pmd, address, end - address, pages, shared, dirty, total);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);
}

static void statm_pgd_range(pgd_t * pgd, unsigned long address, unsigned long end,
	int * pages, int * shared, int * dirty, int * total)
{
	while (address < end) {
		statm_pmd_range(pgd, address, end - address, pages, shared, dirty, total);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgd++;
	}
}

int proc_pid_statm(struct task_struct *task, char * buffer)
{
	struct mm_struct *mm;
	int size=0, resident=0, share=0, trs=0, lrs=0, drs=0, dt=0;

	task_lock(task);
	mm = task->mm;
	if(mm)
		atomic_inc(&mm->mm_users);
	task_unlock(task);
	if (mm) {
		struct vm_area_struct * vma;
		down_read(&mm->mmap_sem);
		vma = mm->mmap;
		while (vma) {
			pgd_t *pgd = pgd_offset(mm, vma->vm_start);
			int pages = 0, shared = 0, dirty = 0, total = 0;

			statm_pgd_range(pgd, vma->vm_start, vma->vm_end, &pages, &shared, &dirty, &total);
			resident += pages;
			share += shared;
			dt += dirty;
			size += total;
			if (vma->vm_flags & VM_EXECUTABLE)
				trs += pages;	/* text */
			else if (vma->vm_flags & VM_GROWSDOWN)
				drs += pages;	/* stack */
			else if (vma->vm_end > 0x60000000)
				lrs += pages;	/* library */
			else
				drs += pages;
			vma = vma->vm_next;
		}
		up_read(&mm->mmap_sem);
		mmput(mm);
	}
	return sprintf(buffer,"%d %d %d %d %d %d %d\n",
		       size, resident, share, trs, lrs, drs, dt);
}

static int show_map(struct seq_file *m, void *v)
{
	struct vm_area_struct *map = v;
	struct file *file = map->vm_file;
	int flags = map->vm_flags;
	unsigned long ino = 0;
	dev_t dev = 0;
	int len;

	if (file) {
		struct inode *inode = map->vm_file->f_dentry->d_inode;
		dev = kdev_t_to_nr(inode->i_sb->s_dev);
		ino = inode->i_ino;
	}

	seq_printf(m, "%08lx-%08lx %c%c%c%c %08lx %02x:%02x %lu %n",
			map->vm_start,
			map->vm_end,
			flags & VM_READ ? 'r' : '-',
			flags & VM_WRITE ? 'w' : '-',
			flags & VM_EXEC ? 'x' : '-',
			flags & VM_MAYSHARE ? 's' : 'p',
			map->vm_pgoff << PAGE_SHIFT,
			MAJOR(dev), MINOR(dev), ino, &len);

	if (map->vm_file) {
		len = 25 + sizeof(void*) * 6 - len;
		if (len < 1)
			len = 1;
		seq_printf(m, "%*c", len, ' ');
		seq_path(m, file->f_vfsmnt, file->f_dentry, "");
	}
	seq_putc(m, '\n');
	return 0;
}

static void *m_start(struct seq_file *m, loff_t *pos)
{
	struct task_struct *task = m->private;
	struct mm_struct *mm;
	struct vm_area_struct * map;
	loff_t l = *pos;

	task_lock(task);
	mm = task->mm;
	if (mm)
		atomic_inc(&mm->mm_users);
	task_unlock(task);

	if (!mm)
		return NULL;

	down_read(&mm->mmap_sem);
	map = mm->mmap;
	while (l-- && map)
		map = map->vm_next;
	if (!map) {
		up_read(&mm->mmap_sem);
		mmput(mm);
	}
	return map;
}

static void m_stop(struct seq_file *m, void *v)
{
	struct vm_area_struct *map = v;
	if (map) {
		struct mm_struct *mm = map->vm_mm;
		up_read(&mm->mmap_sem);
		mmput(mm);
	}
}

static void *m_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct vm_area_struct *map = v;
	(*pos)++;
	if (map->vm_next)
		return map->vm_next;
	m_stop(m, v);
	return NULL;
}

struct seq_operations proc_pid_maps_op = {
	.start	= m_start,
	.next	= m_next,
	.stop	= m_stop,
	.show	= show_map
};

#ifdef CONFIG_SMP
int proc_pid_cpu(struct task_struct *task, char * buffer)
{
	int i, len;

	len = sprintf(buffer,
		"cpu  %lu %lu\n",
		task->times.tms_utime,
		task->times.tms_stime);
		
	for (i = 0 ; i < smp_num_cpus; i++)
		len += sprintf(buffer + len, "cpu%d %lu %lu\n",
			i,
			task->per_cpu_utime[cpu_logical_map(i)],
			task->per_cpu_stime[cpu_logical_map(i)]);

	return len;
}
#endif
