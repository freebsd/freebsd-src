/*
 *  linux/kernel/sys.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/mman.h>
#include <linux/smp_lock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/prctl.h>
#include <linux/init.h>
#include <linux/highuid.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#ifndef SET_UNALIGN_CTL
# define SET_UNALIGN_CTL(a,b)	(-EINVAL)
#endif
#ifndef GET_UNALIGN_CTL
# define GET_UNALIGN_CTL(a,b)	(-EINVAL)
#endif
#ifndef SET_FPEMU_CTL
# define SET_FPEMU_CTL(a,b)	(-EINVAL)
#endif
#ifndef GET_FPEMU_CTL
# define GET_FPEMU_CTL(a,b)	(-EINVAL)
#endif
#ifndef SET_FPEXC_CTL
# define SET_FPEXC_CTL(a,b)	(-EINVAL)
#endif
#ifndef GET_FPEXC_CTL
# define GET_FPEXC_CTL(a,b)	(-EINVAL)
#endif

/*
 * this is where the system-wide overflow UID and GID are defined, for
 * architectures that now have 32-bit UID/GID but didn't in the past
 */

int overflowuid = DEFAULT_OVERFLOWUID;
int overflowgid = DEFAULT_OVERFLOWGID;

/*
 * the same as above, but for filesystems which can only store a 16-bit
 * UID and GID. as such, this is needed on all architectures
 */

int fs_overflowuid = DEFAULT_FS_OVERFLOWUID;
int fs_overflowgid = DEFAULT_FS_OVERFLOWUID;

/*
 * this indicates whether you can reboot with ctrl-alt-del: the default is yes
 */

int C_A_D = 1;
int cad_pid = 1;


/*
 *	Notifier list for kernel code which wants to be called
 *	at shutdown. This is used to stop any idling DMA operations
 *	and the like. 
 */

static struct notifier_block *reboot_notifier_list;
rwlock_t notifier_lock = RW_LOCK_UNLOCKED;

/**
 *	notifier_chain_register	- Add notifier to a notifier chain
 *	@list: Pointer to root list pointer
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to a notifier chain.
 *
 *	Currently always returns zero.
 */
 
int notifier_chain_register(struct notifier_block **list, struct notifier_block *n)
{
	write_lock(&notifier_lock);
	while(*list)
	{
		if(n->priority > (*list)->priority)
			break;
		list= &((*list)->next);
	}
	n->next = *list;
	*list=n;
	write_unlock(&notifier_lock);
	return 0;
}

/**
 *	notifier_chain_unregister - Remove notifier from a notifier chain
 *	@nl: Pointer to root list pointer
 *	@n: New entry in notifier chain
 *
 *	Removes a notifier from a notifier chain.
 *
 *	Returns zero on success, or %-ENOENT on failure.
 */
 
int notifier_chain_unregister(struct notifier_block **nl, struct notifier_block *n)
{
	write_lock(&notifier_lock);
	while((*nl)!=NULL)
	{
		if((*nl)==n)
		{
			*nl=n->next;
			write_unlock(&notifier_lock);
			return 0;
		}
		nl=&((*nl)->next);
	}
	write_unlock(&notifier_lock);
	return -ENOENT;
}

/**
 *	notifier_call_chain - Call functions in a notifier chain
 *	@n: Pointer to root pointer of notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *
 *	Calls each function in a notifier chain in turn.
 *
 *	If the return value of the notifier can be and'd
 *	with %NOTIFY_STOP_MASK, then notifier_call_chain
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise, the return value is the return value
 *	of the last notifier function called.
 */
 
int notifier_call_chain(struct notifier_block **n, unsigned long val, void *v)
{
	int ret=NOTIFY_DONE;
	struct notifier_block *nb = *n;

	while(nb)
	{
		ret=nb->notifier_call(nb,val,v);
		if(ret&NOTIFY_STOP_MASK)
		{
			return ret;
		}
		nb=nb->next;
	}
	return ret;
}

/**
 *	register_reboot_notifier - Register function to be called at reboot time
 *	@nb: Info about notifier function to be called
 *
 *	Registers a function with the list of functions
 *	to be called at reboot time.
 *
 *	Currently always returns zero, as notifier_chain_register
 *	always returns zero.
 */
 
int register_reboot_notifier(struct notifier_block * nb)
{
	return notifier_chain_register(&reboot_notifier_list, nb);
}

/**
 *	unregister_reboot_notifier - Unregister previously registered reboot notifier
 *	@nb: Hook to be unregistered
 *
 *	Unregisters a previously registered reboot
 *	notifier function.
 *
 *	Returns zero on success, or %-ENOENT on failure.
 */
 
int unregister_reboot_notifier(struct notifier_block * nb)
{
	return notifier_chain_unregister(&reboot_notifier_list, nb);
}

asmlinkage long sys_ni_syscall(void)
{
	return -ENOSYS;
}

static int proc_sel(struct task_struct *p, int which, int who)
{
	if(p->pid)
	{
		switch (which) {
			case PRIO_PROCESS:
				if (!who && p == current)
					return 1;
				return(p->pid == who);
			case PRIO_PGRP:
				if (!who)
					who = current->pgrp;
				return(p->pgrp == who);
			case PRIO_USER:
				if (!who)
					who = current->uid;
				return(p->uid == who);
		}
	}
	return 0;
}

asmlinkage long sys_setpriority(int which, int who, int niceval)
{
	struct task_struct *p;
	int error;

	if (which > 2 || which < 0)
		return -EINVAL;

	/* normalize: avoid signed division (rounding problems) */
	error = -ESRCH;
	if (niceval < -20)
		niceval = -20;
	if (niceval > 19)
		niceval = 19;

	read_lock(&tasklist_lock);
	for_each_task(p) {
		if (!proc_sel(p, which, who))
			continue;
		if (p->uid != current->euid &&
			p->uid != current->uid && !capable(CAP_SYS_NICE)) {
			error = -EPERM;
			continue;
		}
		if (error == -ESRCH)
			error = 0;
		if (niceval < p->nice && !capable(CAP_SYS_NICE))
			error = -EACCES;
		else
			p->nice = niceval;
	}
	read_unlock(&tasklist_lock);

	return error;
}

/*
 * Ugh. To avoid negative return values, "getpriority()" will
 * not return the normal nice-value, but a negated value that
 * has been offset by 20 (ie it returns 40..1 instead of -20..19)
 * to stay compatible.
 */
asmlinkage long sys_getpriority(int which, int who)
{
	struct task_struct *p;
	long retval = -ESRCH;

	if (which > 2 || which < 0)
		return -EINVAL;

	read_lock(&tasklist_lock);
	for_each_task (p) {
		long niceval;
		if (!proc_sel(p, which, who))
			continue;
		niceval = 20 - p->nice;
		if (niceval > retval)
			retval = niceval;
	}
	read_unlock(&tasklist_lock);

	return retval;
}


/*
 * Reboot system call: for obvious reasons only root may call it,
 * and even root needs to set up some magic numbers in the registers
 * so that some mistake won't make this reboot the whole machine.
 * You can also set the meaning of the ctrl-alt-del-key here.
 *
 * reboot doesn't sync: do that yourself before calling this.
 */
asmlinkage long sys_reboot(int magic1, int magic2, unsigned int cmd, void * arg)
{
	char buffer[256];

	/* We only trust the superuser with rebooting the system. */
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;

	/* For safety, we require "magic" arguments. */
	if (magic1 != LINUX_REBOOT_MAGIC1 ||
	    (magic2 != LINUX_REBOOT_MAGIC2 && magic2 != LINUX_REBOOT_MAGIC2A &&
			magic2 != LINUX_REBOOT_MAGIC2B))
		return -EINVAL;

	lock_kernel();
	switch (cmd) {
	case LINUX_REBOOT_CMD_RESTART:
		notifier_call_chain(&reboot_notifier_list, SYS_RESTART, NULL);
		printk(KERN_EMERG "Restarting system.\n");
		machine_restart(NULL);
		break;

	case LINUX_REBOOT_CMD_CAD_ON:
		C_A_D = 1;
		break;

	case LINUX_REBOOT_CMD_CAD_OFF:
		C_A_D = 0;
		break;

	case LINUX_REBOOT_CMD_HALT:
		notifier_call_chain(&reboot_notifier_list, SYS_HALT, NULL);
		printk(KERN_EMERG "System halted.\n");
		machine_halt();
		do_exit(0);
		break;

	case LINUX_REBOOT_CMD_POWER_OFF:
		notifier_call_chain(&reboot_notifier_list, SYS_POWER_OFF, NULL);
		printk(KERN_EMERG "Power down.\n");
		machine_power_off();
		do_exit(0);
		break;

	case LINUX_REBOOT_CMD_RESTART2:
		if (strncpy_from_user(&buffer[0], (char *)arg, sizeof(buffer) - 1) < 0) {
			unlock_kernel();
			return -EFAULT;
		}
		buffer[sizeof(buffer) - 1] = '\0';

		notifier_call_chain(&reboot_notifier_list, SYS_RESTART, buffer);
		printk(KERN_EMERG "Restarting system with command '%s'.\n", buffer);
		machine_restart(buffer);
		break;

	default:
		unlock_kernel();
		return -EINVAL;
	}
	unlock_kernel();
	return 0;
}

static void deferred_cad(void *dummy)
{
	notifier_call_chain(&reboot_notifier_list, SYS_RESTART, NULL);
	machine_restart(NULL);
}

/*
 * This function gets called by ctrl-alt-del - ie the keyboard interrupt.
 * As it's called within an interrupt, it may NOT sync: the only choice
 * is whether to reboot at once, or just ignore the ctrl-alt-del.
 */
void ctrl_alt_del(void)
{
	static struct tq_struct cad_tq = {
		routine: deferred_cad,
	};

	if (C_A_D)
		schedule_task(&cad_tq);
	else
		kill_proc(cad_pid, SIGINT, 1);
}
	

/*
 * Unprivileged users may change the real gid to the effective gid
 * or vice versa.  (BSD-style)
 *
 * If you set the real gid at all, or set the effective gid to a value not
 * equal to the real gid, then the saved gid is set to the new effective gid.
 *
 * This makes it possible for a setgid program to completely drop its
 * privileges, which is often a useful assertion to make when you are doing
 * a security audit over a program.
 *
 * The general idea is that a program which uses just setregid() will be
 * 100% compatible with BSD.  A program which uses just setgid() will be
 * 100% compatible with POSIX with saved IDs. 
 *
 * SMP: There are not races, the GIDs are checked only by filesystem
 *      operations (as far as semantic preservation is concerned).
 */
asmlinkage long sys_setregid(gid_t rgid, gid_t egid)
{
	int old_rgid = current->gid;
	int old_egid = current->egid;
	int new_rgid = old_rgid;
	int new_egid = old_egid;

	if (rgid != (gid_t) -1) {
		if ((old_rgid == rgid) ||
		    (current->egid==rgid) ||
		    capable(CAP_SETGID))
			new_rgid = rgid;
		else
			return -EPERM;
	}
	if (egid != (gid_t) -1) {
		if ((old_rgid == egid) ||
		    (current->egid == egid) ||
		    (current->sgid == egid) ||
		    capable(CAP_SETGID))
			new_egid = egid;
		else {
			return -EPERM;
		}
	}
	if (new_egid != old_egid)
	{
		current->mm->dumpable = 0;
		wmb();
	}
	if (rgid != (gid_t) -1 ||
	    (egid != (gid_t) -1 && egid != old_rgid))
		current->sgid = new_egid;
	current->fsgid = new_egid;
	current->egid = new_egid;
	current->gid = new_rgid;
	return 0;
}

/*
 * setgid() is implemented like SysV w/ SAVED_IDS 
 *
 * SMP: Same implicit races as above.
 */
asmlinkage long sys_setgid(gid_t gid)
{
	int old_egid = current->egid;

	if (capable(CAP_SETGID))
	{
		if(old_egid != gid)
		{
			current->mm->dumpable=0;
			wmb();
		}
		current->gid = current->egid = current->sgid = current->fsgid = gid;
	}
	else if ((gid == current->gid) || (gid == current->sgid))
	{
		if(old_egid != gid)
		{
			current->mm->dumpable=0;
			wmb();
		}
		current->egid = current->fsgid = gid;
	}
	else
		return -EPERM;
	return 0;
}
  
/* 
 * cap_emulate_setxuid() fixes the effective / permitted capabilities of
 * a process after a call to setuid, setreuid, or setresuid.
 *
 *  1) When set*uiding _from_ one of {r,e,s}uid == 0 _to_ all of
 *  {r,e,s}uid != 0, the permitted and effective capabilities are
 *  cleared.
 *
 *  2) When set*uiding _from_ euid == 0 _to_ euid != 0, the effective
 *  capabilities of the process are cleared.
 *
 *  3) When set*uiding _from_ euid != 0 _to_ euid == 0, the effective
 *  capabilities are set to the permitted capabilities.
 *
 *  fsuid is handled elsewhere. fsuid == 0 and {r,e,s}uid!= 0 should 
 *  never happen.
 *
 *  -astor 
 *
 * cevans - New behaviour, Oct '99
 * A process may, via prctl(), elect to keep its capabilities when it
 * calls setuid() and switches away from uid==0. Both permitted and
 * effective sets will be retained.
 * Without this change, it was impossible for a daemon to drop only some
 * of its privilege. The call to setuid(!=0) would drop all privileges!
 * Keeping uid 0 is not an option because uid 0 owns too many vital
 * files..
 * Thanks to Olaf Kirch and Peter Benie for spotting this.
 */
static inline void cap_emulate_setxuid(int old_ruid, int old_euid, 
				       int old_suid)
{
	if ((old_ruid == 0 || old_euid == 0 || old_suid == 0) &&
	    (current->uid != 0 && current->euid != 0 && current->suid != 0) &&
	    !current->keep_capabilities) {
		cap_clear(current->cap_permitted);
		cap_clear(current->cap_effective);
	}
	if (old_euid == 0 && current->euid != 0) {
		cap_clear(current->cap_effective);
	}
	if (old_euid != 0 && current->euid == 0) {
		current->cap_effective = current->cap_permitted;
	}
}

static int set_user(uid_t new_ruid, int dumpclear)
{
	struct user_struct *new_user;

	new_user = alloc_uid(new_ruid);
	if (!new_user)
		return -EAGAIN;
	switch_uid(new_user);

	if(dumpclear)
	{
		current->mm->dumpable = 0;
		wmb();
	}
	current->uid = new_ruid;
	return 0;
}

/*
 * Unprivileged users may change the real uid to the effective uid
 * or vice versa.  (BSD-style)
 *
 * If you set the real uid at all, or set the effective uid to a value not
 * equal to the real uid, then the saved uid is set to the new effective uid.
 *
 * This makes it possible for a setuid program to completely drop its
 * privileges, which is often a useful assertion to make when you are doing
 * a security audit over a program.
 *
 * The general idea is that a program which uses just setreuid() will be
 * 100% compatible with BSD.  A program which uses just setuid() will be
 * 100% compatible with POSIX with saved IDs. 
 */
asmlinkage long sys_setreuid(uid_t ruid, uid_t euid)
{
	int old_ruid, old_euid, old_suid, new_ruid, new_euid;

	new_ruid = old_ruid = current->uid;
	new_euid = old_euid = current->euid;
	old_suid = current->suid;

	if (ruid != (uid_t) -1) {
		new_ruid = ruid;
		if ((old_ruid != ruid) &&
		    (current->euid != ruid) &&
		    !capable(CAP_SETUID))
			return -EPERM;
	}

	if (euid != (uid_t) -1) {
		new_euid = euid;
		if ((old_ruid != euid) &&
		    (current->euid != euid) &&
		    (current->suid != euid) &&
		    !capable(CAP_SETUID))
			return -EPERM;
	}

	if (new_ruid != old_ruid && set_user(new_ruid, new_euid != old_euid) < 0)
		return -EAGAIN;

	if (new_euid != old_euid)
	{
		current->mm->dumpable=0;
		wmb();
	}
	current->fsuid = current->euid = new_euid;
	if (ruid != (uid_t) -1 ||
	    (euid != (uid_t) -1 && euid != old_ruid))
		current->suid = current->euid;
	current->fsuid = current->euid;

	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		cap_emulate_setxuid(old_ruid, old_euid, old_suid);
	}

	return 0;
}


		
/*
 * setuid() is implemented like SysV with SAVED_IDS 
 * 
 * Note that SAVED_ID's is deficient in that a setuid root program
 * like sendmail, for example, cannot set its uid to be a normal 
 * user and then switch back, because if you're root, setuid() sets
 * the saved uid too.  If you don't like this, blame the bright people
 * in the POSIX committee and/or USG.  Note that the BSD-style setreuid()
 * will allow a root program to temporarily drop privileges and be able to
 * regain them by swapping the real and effective uid.  
 */
asmlinkage long sys_setuid(uid_t uid)
{
	int old_euid = current->euid;
	int old_ruid, old_suid, new_ruid, new_suid;

	old_ruid = new_ruid = current->uid;
	old_suid = current->suid;
	new_suid = old_suid;
	
	if (capable(CAP_SETUID)) {
		if (uid != old_ruid && set_user(uid, old_euid != uid) < 0)
			return -EAGAIN;
		new_suid = uid;
	} else if ((uid != current->uid) && (uid != new_suid))
		return -EPERM;

	if (old_euid != uid)
	{
		current->mm->dumpable = 0;
		wmb();
	}
	current->fsuid = current->euid = uid;
	current->suid = new_suid;

	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		cap_emulate_setxuid(old_ruid, old_euid, old_suid);
	}

	return 0;
}


/*
 * This function implements a generic ability to update ruid, euid,
 * and suid.  This allows you to implement the 4.4 compatible seteuid().
 */
asmlinkage long sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	int old_ruid = current->uid;
	int old_euid = current->euid;
	int old_suid = current->suid;

	if (!capable(CAP_SETUID)) {
		if ((ruid != (uid_t) -1) && (ruid != current->uid) &&
		    (ruid != current->euid) && (ruid != current->suid))
			return -EPERM;
		if ((euid != (uid_t) -1) && (euid != current->uid) &&
		    (euid != current->euid) && (euid != current->suid))
			return -EPERM;
		if ((suid != (uid_t) -1) && (suid != current->uid) &&
		    (suid != current->euid) && (suid != current->suid))
			return -EPERM;
	}
	if (ruid != (uid_t) -1) {
		if (ruid != current->uid && set_user(ruid, euid != current->euid) < 0)
			return -EAGAIN;
	}
	if (euid != (uid_t) -1) {
		if (euid != current->euid)
		{
			current->mm->dumpable = 0;
			wmb();
		}
		current->euid = euid;
	}
	current->fsuid = current->euid;
	if (suid != (uid_t) -1)
		current->suid = suid;

	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		cap_emulate_setxuid(old_ruid, old_euid, old_suid);
	}

	return 0;
}

asmlinkage long sys_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
	int retval;

	if (!(retval = put_user(current->uid, ruid)) &&
	    !(retval = put_user(current->euid, euid)))
		retval = put_user(current->suid, suid);

	return retval;
}

/*
 * Same as above, but for rgid, egid, sgid.
 */
asmlinkage long sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
	if (!capable(CAP_SETGID)) {
		if ((rgid != (gid_t) -1) && (rgid != current->gid) &&
		    (rgid != current->egid) && (rgid != current->sgid))
			return -EPERM;
		if ((egid != (gid_t) -1) && (egid != current->gid) &&
		    (egid != current->egid) && (egid != current->sgid))
			return -EPERM;
		if ((sgid != (gid_t) -1) && (sgid != current->gid) &&
		    (sgid != current->egid) && (sgid != current->sgid))
			return -EPERM;
	}
	if (egid != (gid_t) -1) {
		if (egid != current->egid)
		{
			current->mm->dumpable = 0;
			wmb();
		}
		current->egid = egid;
	}
	current->fsgid = current->egid;
	if (rgid != (gid_t) -1)
		current->gid = rgid;
	if (sgid != (gid_t) -1)
		current->sgid = sgid;
	return 0;
}

asmlinkage long sys_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
	int retval;

	if (!(retval = put_user(current->gid, rgid)) &&
	    !(retval = put_user(current->egid, egid)))
		retval = put_user(current->sgid, sgid);

	return retval;
}


/*
 * "setfsuid()" sets the fsuid - the uid used for filesystem checks. This
 * is used for "access()" and for the NFS daemon (letting nfsd stay at
 * whatever uid it wants to). It normally shadows "euid", except when
 * explicitly set by setfsuid() or for access..
 */
asmlinkage long sys_setfsuid(uid_t uid)
{
	int old_fsuid;

	old_fsuid = current->fsuid;
	if (uid == current->uid || uid == current->euid ||
	    uid == current->suid || uid == current->fsuid || 
	    capable(CAP_SETUID))
	{
		if (uid != old_fsuid)
		{
			current->mm->dumpable = 0;
			wmb();
		}
		current->fsuid = uid;
	}

	/* We emulate fsuid by essentially doing a scaled-down version
	 * of what we did in setresuid and friends. However, we only
	 * operate on the fs-specific bits of the process' effective
	 * capabilities 
	 *
	 * FIXME - is fsuser used for all CAP_FS_MASK capabilities?
	 *          if not, we might be a bit too harsh here.
	 */
	
	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		if (old_fsuid == 0 && current->fsuid != 0) {
			cap_t(current->cap_effective) &= ~CAP_FS_MASK;
		}
		if (old_fsuid != 0 && current->fsuid == 0) {
			cap_t(current->cap_effective) |=
				(cap_t(current->cap_permitted) & CAP_FS_MASK);
		}
	}

	return old_fsuid;
}

/*
 * Samma på svenska..
 */
asmlinkage long sys_setfsgid(gid_t gid)
{
	int old_fsgid;

	old_fsgid = current->fsgid;
	if (gid == current->gid || gid == current->egid ||
	    gid == current->sgid || gid == current->fsgid || 
	    capable(CAP_SETGID))
	{
		if (gid != old_fsgid)
		{
			current->mm->dumpable = 0;
			wmb();
		}
		current->fsgid = gid;
	}
	return old_fsgid;
}

asmlinkage long sys_times(struct tms * tbuf)
{
	/*
	 *	In the SMP world we might just be unlucky and have one of
	 *	the times increment as we use it. Since the value is an
	 *	atomically safe type this is just fine. Conceptually its
	 *	as if the syscall took an instant longer to occur.
	 */
	if (tbuf)
		if (copy_to_user(tbuf, &current->times, sizeof(struct tms)))
			return -EFAULT;
	return jiffies;
}

/*
 * This needs some heavy checking ...
 * I just haven't the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 *
 * OK, I think I have the protection semantics right.... this is really
 * only important on a multi-user system anyway, to make sure one user
 * can't send a signal to a process owned by another.  -TYT, 12/12/91
 *
 * Auch. Had to add the 'did_exec' flag to conform completely to POSIX.
 * LBT 04.03.94
 */

asmlinkage long sys_setpgid(pid_t pid, pid_t pgid)
{
	struct task_struct * p;
	int err = -EINVAL;

	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = pid;
	if (pgid < 0)
		return -EINVAL;

	/* From this point forward we keep holding onto the tasklist lock
	 * so that our parent does not change from under us. -DaveM
	 */
	read_lock(&tasklist_lock);

	err = -ESRCH;
	p = find_task_by_pid(pid);
	if (!p)
		goto out;

	if (p->p_pptr == current || p->p_opptr == current) {
		err = -EPERM;
		if (p->session != current->session)
			goto out;
		err = -EACCES;
		if (p->did_exec)
			goto out;
	} else if (p != current)
		goto out;
	err = -EPERM;
	if (p->leader)
		goto out;
	if (pgid != pid) {
		struct task_struct * tmp;
		for_each_task (tmp) {
			if (tmp->pgrp == pgid &&
			    tmp->session == current->session)
				goto ok_pgid;
		}
		goto out;
	}

ok_pgid:
	p->pgrp = pgid;
	err = 0;
out:
	/* All paths lead to here, thus we are safe. -DaveM */
	read_unlock(&tasklist_lock);
	return err;
}

asmlinkage long sys_getpgid(pid_t pid)
{
	if (!pid) {
		return current->pgrp;
	} else {
		int retval;
		struct task_struct *p;

		read_lock(&tasklist_lock);
		p = find_task_by_pid(pid);

		retval = -ESRCH;
		if (p)
			retval = p->pgrp;
		read_unlock(&tasklist_lock);
		return retval;
	}
}

asmlinkage long sys_getpgrp(void)
{
	/* SMP - assuming writes are word atomic this is fine */
	return current->pgrp;
}

asmlinkage long sys_getsid(pid_t pid)
{
	if (!pid) {
		return current->session;
	} else {
		int retval;
		struct task_struct *p;

		read_lock(&tasklist_lock);
		p = find_task_by_pid(pid);

		retval = -ESRCH;
		if(p)
			retval = p->session;
		read_unlock(&tasklist_lock);
		return retval;
	}
}

asmlinkage long sys_setsid(void)
{
	struct task_struct * p;
	int err = -EPERM;

	read_lock(&tasklist_lock);
	for_each_task(p) {
		if (p->pgrp == current->pid)
			goto out;
	}

	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = NULL;
	current->tty_old_pgrp = 0;
	err = current->pgrp;
out:
	read_unlock(&tasklist_lock);
	return err;
}

/*
 * Supplementary group IDs
 */
asmlinkage long sys_getgroups(int gidsetsize, gid_t *grouplist)
{
	int i;
	
	/*
	 *	SMP: Nobody else can change our grouplist. Thus we are
	 *	safe.
	 */

	if (gidsetsize < 0)
		return -EINVAL;
	i = current->ngroups;
	if (gidsetsize) {
		if (i > gidsetsize)
			return -EINVAL;
		if (copy_to_user(grouplist, current->groups, sizeof(gid_t)*i))
			return -EFAULT;
	}
	return i;
}

/*
 *	SMP: Our groups are not shared. We can copy to/from them safely
 *	without another task interfering.
 */
 
asmlinkage long sys_setgroups(int gidsetsize, gid_t *grouplist)
{
	if (!capable(CAP_SETGID))
		return -EPERM;
	if ((unsigned) gidsetsize > NGROUPS)
		return -EINVAL;
	if(copy_from_user(current->groups, grouplist, gidsetsize * sizeof(gid_t)))
		return -EFAULT;
	current->ngroups = gidsetsize;
	return 0;
}

static int supplemental_group_member(gid_t grp)
{
	int i = current->ngroups;

	if (i) {
		gid_t *groups = current->groups;
		do {
			if (*groups == grp)
				return 1;
			groups++;
			i--;
		} while (i);
	}
	return 0;
}

/*
 * Check whether we're fsgid/egid or in the supplemental group..
 */
int in_group_p(gid_t grp)
{
	int retval = 1;
	if (grp != current->fsgid)
		retval = supplemental_group_member(grp);
	return retval;
}

int in_egroup_p(gid_t grp)
{
	int retval = 1;
	if (grp != current->egid)
		retval = supplemental_group_member(grp);
	return retval;
}

DECLARE_RWSEM(uts_sem);

asmlinkage long sys_newuname(struct new_utsname * name)
{
	int errno = 0;

	down_read(&uts_sem);
	if (copy_to_user(name,&system_utsname,sizeof *name))
		errno = -EFAULT;
	up_read(&uts_sem);
	return errno;
}

asmlinkage long sys_sethostname(char *name, int len)
{
	int errno;
	char tmp[__NEW_UTS_LEN];

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (len < 0 || len > __NEW_UTS_LEN)
		return -EINVAL;
	down_write(&uts_sem);
	errno = -EFAULT;
	if (!copy_from_user(tmp, name, len)) {
		memcpy(system_utsname.nodename, tmp, len);
		system_utsname.nodename[len] = 0;
		errno = 0;
	}
	up_write(&uts_sem);
	return errno;
}

asmlinkage long sys_gethostname(char *name, int len)
{
	int i, errno;

	if (len < 0)
		return -EINVAL;
	down_read(&uts_sem);
	i = 1 + strlen(system_utsname.nodename);
	if (i > len)
		i = len;
	errno = 0;
	if (copy_to_user(name, system_utsname.nodename, i))
		errno = -EFAULT;
	up_read(&uts_sem);
	return errno;
}

/*
 * Only setdomainname; getdomainname can be implemented by calling
 * uname()
 */
asmlinkage long sys_setdomainname(char *name, int len)
{
	int errno;
	char tmp[__NEW_UTS_LEN];

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (len < 0 || len > __NEW_UTS_LEN)
		return -EINVAL;

	down_write(&uts_sem);
	errno = -EFAULT;
	if (!copy_from_user(tmp, name, len)) {
		memcpy(system_utsname.domainname, tmp, len);
		system_utsname.domainname[len] = 0;
		errno = 0;
	}
	up_write(&uts_sem);
	return errno;
}

asmlinkage long sys_getrlimit(unsigned int resource, struct rlimit *rlim)
{
	if (resource >= RLIM_NLIMITS)
		return -EINVAL;
	else
		return copy_to_user(rlim, current->rlim + resource, sizeof(*rlim))
			? -EFAULT : 0;
}

#if !defined(__ia64__) 

/*
 *	Back compatibility for getrlimit. Needed for some apps.
 */
 
asmlinkage long sys_old_getrlimit(unsigned int resource, struct rlimit *rlim)
{
	struct rlimit x;
	if (resource >= RLIM_NLIMITS)
		return -EINVAL;

	memcpy(&x, current->rlim + resource, sizeof(*rlim));
	if(x.rlim_cur > 0x7FFFFFFF)
		x.rlim_cur = 0x7FFFFFFF;
	if(x.rlim_max > 0x7FFFFFFF)
		x.rlim_max = 0x7FFFFFFF;
	return copy_to_user(rlim, &x, sizeof(x))?-EFAULT:0;
}

#endif

asmlinkage long sys_setrlimit(unsigned int resource, struct rlimit *rlim)
{
	struct rlimit new_rlim, *old_rlim;

	if (resource >= RLIM_NLIMITS)
		return -EINVAL;
	if(copy_from_user(&new_rlim, rlim, sizeof(*rlim)))
		return -EFAULT;
       if (new_rlim.rlim_cur > new_rlim.rlim_max)
               return -EINVAL;
	old_rlim = current->rlim + resource;
	if (((new_rlim.rlim_cur > old_rlim->rlim_max) ||
	     (new_rlim.rlim_max > old_rlim->rlim_max)) &&
	    !capable(CAP_SYS_RESOURCE))
		return -EPERM;
	if (resource == RLIMIT_NOFILE) {
		if (new_rlim.rlim_cur > NR_OPEN || new_rlim.rlim_max > NR_OPEN)
			return -EPERM;
	}
	*old_rlim = new_rlim;
	return 0;
}

/*
 * It would make sense to put struct rusage in the task_struct,
 * except that would make the task_struct be *really big*.  After
 * task_struct gets moved into malloc'ed memory, it would
 * make sense to do this.  It will make moving the rest of the information
 * a lot simpler!  (Which we're not doing right now because we're not
 * measuring them yet).
 *
 * This is SMP safe.  Either we are called from sys_getrusage on ourselves
 * below (we know we aren't going to exit/disappear and only we change our
 * rusage counters), or we are called from wait4() on a process which is
 * either stopped or zombied.  In the zombied case the task won't get
 * reaped till shortly after the call to getrusage(), in both cases the
 * task being examined is in a frozen state so the counters won't change.
 *
 * FIXME! Get the fault counts properly!
 */
int getrusage(struct task_struct *p, int who, struct rusage *ru)
{
	struct rusage r;

	memset((char *) &r, 0, sizeof(r));
	switch (who) {
		case RUSAGE_SELF:
			r.ru_utime.tv_sec = CT_TO_SECS(p->times.tms_utime);
			r.ru_utime.tv_usec = CT_TO_USECS(p->times.tms_utime);
			r.ru_stime.tv_sec = CT_TO_SECS(p->times.tms_stime);
			r.ru_stime.tv_usec = CT_TO_USECS(p->times.tms_stime);
			r.ru_minflt = p->min_flt;
			r.ru_majflt = p->maj_flt;
			r.ru_nswap = p->nswap;
			break;
		case RUSAGE_CHILDREN:
			r.ru_utime.tv_sec = CT_TO_SECS(p->times.tms_cutime);
			r.ru_utime.tv_usec = CT_TO_USECS(p->times.tms_cutime);
			r.ru_stime.tv_sec = CT_TO_SECS(p->times.tms_cstime);
			r.ru_stime.tv_usec = CT_TO_USECS(p->times.tms_cstime);
			r.ru_minflt = p->cmin_flt;
			r.ru_majflt = p->cmaj_flt;
			r.ru_nswap = p->cnswap;
			break;
		default:
			r.ru_utime.tv_sec = CT_TO_SECS(p->times.tms_utime + p->times.tms_cutime);
			r.ru_utime.tv_usec = CT_TO_USECS(p->times.tms_utime + p->times.tms_cutime);
			r.ru_stime.tv_sec = CT_TO_SECS(p->times.tms_stime + p->times.tms_cstime);
			r.ru_stime.tv_usec = CT_TO_USECS(p->times.tms_stime + p->times.tms_cstime);
			r.ru_minflt = p->min_flt + p->cmin_flt;
			r.ru_majflt = p->maj_flt + p->cmaj_flt;
			r.ru_nswap = p->nswap + p->cnswap;
			break;
	}
	return copy_to_user(ru, &r, sizeof(r)) ? -EFAULT : 0;
}

asmlinkage long sys_getrusage(int who, struct rusage *ru)
{
	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;
	return getrusage(current, who, ru);
}

asmlinkage long sys_umask(int mask)
{
	mask = xchg(&current->fs->umask, mask & S_IRWXUGO);
	return mask;
}
    
asmlinkage long sys_prctl(int option, unsigned long arg2, unsigned long arg3,
			  unsigned long arg4, unsigned long arg5)
{
	int error = 0;
	int sig;

	switch (option) {
		case PR_SET_PDEATHSIG:
			sig = arg2;
			if (sig < 0 || sig > _NSIG) {
				error = -EINVAL;
				break;
			}
			current->pdeath_signal = sig;
			break;
		case PR_GET_PDEATHSIG:
			error = put_user(current->pdeath_signal, (int *)arg2);
			break;
		case PR_GET_DUMPABLE:
			if (is_dumpable(current))
				error = 1;
			break;
		case PR_SET_DUMPABLE:
			if (arg2 != 0 && arg2 != 1) {
				error = -EINVAL;
				break;
			}
			current->mm->dumpable = arg2;
			break;

	        case PR_SET_UNALIGN:
			error = SET_UNALIGN_CTL(current, arg2);
			break;
	        case PR_GET_UNALIGN:
			error = GET_UNALIGN_CTL(current, arg2);
			break;
	        case PR_SET_FPEMU:
			error = SET_FPEMU_CTL(current, arg2);
			break;
	        case PR_GET_FPEMU:
			error = GET_FPEMU_CTL(current, arg2);
			break;
		case PR_SET_FPEXC:
			error = SET_FPEXC_CTL(current, arg2);
			break;
		case PR_GET_FPEXC:
			error = GET_FPEXC_CTL(current, arg2);
			break;

		case PR_GET_KEEPCAPS:
			if (current->keep_capabilities)
				error = 1;
			break;
		case PR_SET_KEEPCAPS:
			if (arg2 != 0 && arg2 != 1) {
				error = -EINVAL;
				break;
			}
			current->keep_capabilities = arg2;
			break;
		default:
			error = -EINVAL;
			break;
	}
	return error;
}

EXPORT_SYMBOL(notifier_chain_register);
EXPORT_SYMBOL(notifier_chain_unregister);
EXPORT_SYMBOL(notifier_call_chain);
EXPORT_SYMBOL(register_reboot_notifier);
EXPORT_SYMBOL(unregister_reboot_notifier);
EXPORT_SYMBOL(in_group_p);
EXPORT_SYMBOL(in_egroup_p);
