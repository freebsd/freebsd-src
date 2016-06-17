/*
 *  linux/kernel/acct.c
 *
 *  BSD Process Accounting for Linux
 *
 *  Author: Marco van Wieringen <mvw@planets.elm.net>
 *
 *  Some code based on ideas and code from:
 *  Thomas K. Dyas <tdyas@eden.rutgers.edu>
 *
 *  This file implements BSD-style process accounting. Whenever any
 *  process exits, an accounting record of type "struct acct" is
 *  written to the file specified with the acct() system call. It is
 *  up to user-level programs to do useful things with the accounting
 *  log. The kernel just provides the raw accounting information.
 *
 * (C) Copyright 1995 - 1997 Marco van Wieringen - ELM Consultancy B.V.
 *
 *  Plugged two leaks. 1) It didn't return acct_file into the free_filps if
 *  the file happened to be read-only. 2) If the accounting was suspended
 *  due to the lack of space it happily allowed to reopen it and completely
 *  lost the old acct_file. 3/10/98, Al Viro.
 *
 *  Now we silently close acct_file on attempt to reopen. Cleaned sys_acct().
 *  XTerms and EMACS are manifestations of pure evil. 21/10/98, AV.
 *
 *  Fixed a nasty interaction with with sys_umount(). If the accointing
 *  was suspeneded we failed to stop it on umount(). Messy.
 *  Another one: remount to readonly didn't stop accounting.
 *	Question: what should we do if we have CAP_SYS_ADMIN but not
 *  CAP_SYS_PACCT? Current code does the following: umount returns -EBUSY
 *  unless we are messing with the root. In that case we are getting a
 *  real mess with do_remount_sb(). 9/11/98, AV.
 *
 *  Fixed a bunch of races (and pair of leaks). Probably not the best way,
 *  but this one obviously doesn't introduce deadlocks. Later. BTW, found
 *  one race (and leak) in BSD implementation.
 *  OK, that's better. ANOTHER race and leak in BSD variant. There always
 *  is one more bug... 10/11/98, AV.
 *
 *	Oh, fsck... Oopsable SMP race in do_process_acct() - we must hold
 * ->mmap_sem to walk the vma list of current->mm. Nasty, since it leaks
 * a struct file opened for write. Fixed. 2/6/2000, AV.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#ifdef CONFIG_BSD_PROCESS_ACCT
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/acct.h>
#include <linux/smp_lock.h>
#include <linux/file.h>
#include <linux/tty.h>

#include <asm/uaccess.h>

/*
 * These constants control the amount of freespace that suspend and
 * resume the process accounting system, and the time delay between
 * each check.
 * Turned into sysctl-controllable parameters. AV, 12/11/98
 */

int acct_parm[3] = {4, 2, 30};
#define RESUME		(acct_parm[0])	/* >foo% free space - resume */
#define SUSPEND		(acct_parm[1])	/* <foo% free space - suspend */
#define ACCT_TIMEOUT	(acct_parm[2])	/* foo second timeout between checks */

/*
 * External references and all of the globals.
 */

static volatile int acct_active;
static volatile int acct_needcheck;
static struct file *acct_file;
static struct timer_list acct_timer;
static void do_acct_process(long, struct file *);

/*
 * Called whenever the timer says to check the free space.
 */
static void acct_timeout(unsigned long unused)
{
	acct_needcheck = 1;
}

/*
 * Check the amount of free space and suspend/resume accordingly.
 */
static int check_free_space(struct file *file)
{
	struct statfs sbuf;
	int res;
	int act;

	lock_kernel();
	res = acct_active;
	if (!file || !acct_needcheck)
		goto out;
	unlock_kernel();

	/* May block */
	if (vfs_statfs(file->f_dentry->d_inode->i_sb, &sbuf))
		return res;

	if (sbuf.f_bavail <= SUSPEND * sbuf.f_blocks / 100)
		act = -1;
	else if (sbuf.f_bavail >= RESUME * sbuf.f_blocks / 100)
		act = 1;
	else
		act = 0;

	/*
	 * If some joker switched acct_file under us we'ld better be
	 * silent and _not_ touch anything.
	 */
	lock_kernel();
	if (file != acct_file) {
		if (act)
			res = act>0;
		goto out;
	}

	if (acct_active) {
		if (act < 0) {
			acct_active = 0;
			printk(KERN_INFO "Process accounting paused\n");
		}
	} else {
		if (act > 0) {
			acct_active = 1;
			printk(KERN_INFO "Process accounting resumed\n");
		}
	}

	del_timer(&acct_timer);
	acct_needcheck = 0;
	acct_timer.expires = jiffies + ACCT_TIMEOUT*HZ;
	add_timer(&acct_timer);
	res = acct_active;
out:
	unlock_kernel();
	return res;
}

/*
 *  sys_acct() is the only system call needed to implement process
 *  accounting. It takes the name of the file where accounting records
 *  should be written. If the filename is NULL, accounting will be
 *  shutdown.
 */
asmlinkage long sys_acct(const char *name)
{
	struct file *file = NULL, *old_acct = NULL;
	char *tmp;
	int error;

	if (!capable(CAP_SYS_PACCT))
		return -EPERM;

	if (name) {
		tmp = getname(name);
		error = PTR_ERR(tmp);
		if (IS_ERR(tmp))
			goto out;
		/* Difference from BSD - they don't do O_APPEND */
		file = filp_open(tmp, O_WRONLY|O_APPEND, 0);
		putname(tmp);
		if (IS_ERR(file)) {
			error = PTR_ERR(file);
			goto out;
		}
		error = -EACCES;
		if (!S_ISREG(file->f_dentry->d_inode->i_mode)) 
			goto out_err;

		error = -EIO;
		if (!file->f_op->write) 
			goto out_err;
	}

	error = 0;
	lock_kernel();
	if (acct_file) {
		old_acct = acct_file;
		del_timer(&acct_timer);
		acct_active = 0;
		acct_needcheck = 0;
		acct_file = NULL;
	}
	if (name) {
		acct_file = file;
		acct_needcheck = 0;
		acct_active = 1;
		/* It's been deleted if it was used before so this is safe */
		init_timer(&acct_timer);
		acct_timer.function = acct_timeout;
		acct_timer.expires = jiffies + ACCT_TIMEOUT*HZ;
		add_timer(&acct_timer);
	}
	unlock_kernel();
	if (old_acct) {
		do_acct_process(0,old_acct);
		filp_close(old_acct, NULL);
	}
out:
	return error;
out_err:
	filp_close(file, NULL);
	goto out;
}

void acct_auto_close(kdev_t dev)
{
	lock_kernel();
	if (acct_file && acct_file->f_dentry->d_inode->i_dev == dev)
		sys_acct(NULL);
	unlock_kernel();
}

/*
 *  encode an unsigned long into a comp_t
 *
 *  This routine has been adopted from the encode_comp_t() function in
 *  the kern_acct.c file of the FreeBSD operating system. The encoding
 *  is a 13-bit fraction with a 3-bit (base 8) exponent.
 */

#define	MANTSIZE	13			/* 13 bit mantissa. */
#define	EXPSIZE		3			/* Base 8 (3 bit) exponent. */
#define	MAXFRACT	((1 << MANTSIZE) - 1)	/* Maximum fractional value. */

static comp_t encode_comp_t(unsigned long value)
{
	int exp, rnd;

	exp = rnd = 0;
	while (value > MAXFRACT) {
		rnd = value & (1 << (EXPSIZE - 1));	/* Round up? */
		value >>= EXPSIZE;	/* Base 8 exponent == 3 bit shift. */
		exp++;
	}

	/*
         * If we need to round up, do it (and handle overflow correctly).
         */
	if (rnd && (++value > MAXFRACT)) {
		value >>= EXPSIZE;
		exp++;
	}

	/*
         * Clean it up and polish it off.
         */
	exp <<= MANTSIZE;		/* Shift the exponent into place */
	exp += value;			/* and add on the mantissa. */
	return exp;
}

/*
 *  Write an accounting entry for an exiting process
 *
 *  The acct_process() call is the workhorse of the process
 *  accounting system. The struct acct is built here and then written
 *  into the accounting file. This function should only be called from
 *  do_exit().
 */

/*
 *  do_acct_process does all actual work. Caller holds the reference to file.
 */
static void do_acct_process(long exitcode, struct file *file)
{
	struct acct ac;
	mm_segment_t fs;
	unsigned long vsize;
	unsigned long flim;

	/*
	 * First check to see if there is enough free_space to continue
	 * the process accounting system.
	 */
	if (!check_free_space(file))
		return;

	/*
	 * Fill the accounting struct with the needed info as recorded
	 * by the different kernel functions.
	 */
	memset((caddr_t)&ac, 0, sizeof(struct acct));

	strncpy(ac.ac_comm, current->comm, ACCT_COMM);
	ac.ac_comm[ACCT_COMM - 1] = '\0';

	ac.ac_btime = CT_TO_SECS(current->start_time) + (xtime.tv_sec - (jiffies / HZ));
	ac.ac_etime = encode_comp_t(jiffies - current->start_time);
	ac.ac_utime = encode_comp_t(current->times.tms_utime);
	ac.ac_stime = encode_comp_t(current->times.tms_stime);
	ac.ac_uid = current->uid;
	ac.ac_gid = current->gid;
	ac.ac_tty = (current->tty) ? kdev_t_to_nr(current->tty->device) : 0;

	ac.ac_flag = 0;
	if (current->flags & PF_FORKNOEXEC)
		ac.ac_flag |= AFORK;
	if (current->flags & PF_SUPERPRIV)
		ac.ac_flag |= ASU;
	if (current->flags & PF_DUMPCORE)
		ac.ac_flag |= ACORE;
	if (current->flags & PF_SIGNALED)
		ac.ac_flag |= AXSIG;

	vsize = 0;
	if (current->mm) {
		struct vm_area_struct *vma;
		down_read(&current->mm->mmap_sem);
		vma = current->mm->mmap;
		while (vma) {
			vsize += vma->vm_end - vma->vm_start;
			vma = vma->vm_next;
		}
		up_read(&current->mm->mmap_sem);
	}
	vsize = vsize / 1024;
	ac.ac_mem = encode_comp_t(vsize);
	ac.ac_io = encode_comp_t(0 /* current->io_usage */);	/* %% */
	ac.ac_rw = encode_comp_t(ac.ac_io / 1024);
	ac.ac_minflt = encode_comp_t(current->min_flt);
	ac.ac_majflt = encode_comp_t(current->maj_flt);
	ac.ac_swaps = encode_comp_t(current->nswap);
	ac.ac_exitcode = exitcode;

	/*
         * Kernel segment override to datasegment and write it
         * to the accounting file.
         */
	fs = get_fs();
	set_fs(KERNEL_DS);
	/*
 	 * Accounting records are not subject to resource limits.
 	 */
	flim = current->rlim[RLIMIT_FSIZE].rlim_cur;
	current->rlim[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	file->f_op->write(file, (char *)&ac,
			       sizeof(struct acct), &file->f_pos);
	current->rlim[RLIMIT_FSIZE].rlim_cur = flim;
	set_fs(fs);
}

/*
 * acct_process - now just a wrapper around do_acct_process
 */
int acct_process(long exitcode)
{
	struct file *file = NULL;
	lock_kernel();
	if (acct_file) {
		file = acct_file;
		get_file(file);
		unlock_kernel();
		do_acct_process(exitcode, file);
		fput(file);
	} else
		unlock_kernel();
	return 0;
}

#else
/*
 * Dummy system call when BSD process accounting is not configured
 * into the kernel.
 */

asmlinkage long sys_acct(const char * filename)
{
	return -ENOSYS;
}
#endif
