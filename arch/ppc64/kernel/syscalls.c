/*
 * linux/arch/ppc/kernel/sys_ppc.c
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 * Derived from "arch/i386/kernel/sys_i386.c"
 * Adapted from the i386 version by Gary Thomas
 * Modified by Cort Dougan (cort@cs.nmt.edu)
 * and Paul Mackerras (paulus@cs.anu.edu.au).
 *
 * This file contains various random system calls that
 * have a non-standard calling sequence on the Linux/PPC
 * platform.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/sys.h>
#include <linux/ipc.h>
#include <linux/utsname.h>
#include <linux/file.h>
#include <linux/init.h>
#include <linux/personality.h>

#include <asm/uaccess.h>
#include <asm/ipc.h>
#include <asm/semaphore.h>
#include <asm/ppcdebug.h>

#include <asm/time.h>

extern unsigned long wall_jiffies;
#define USEC_PER_SEC (1000000)

void
check_bugs(void)
{
}

asmlinkage int sys_ioperm(unsigned long from, unsigned long num, int on)
{
	return -EIO;
}

/*
 * sys_ipc() is the de-multiplexer for the SysV IPC calls..
 *
 * This is really horribly ugly.
 */
asmlinkage int 
sys_ipc (uint call, int first, int second, long third, void *ptr, long fifth)
{
	int version, ret;

	PPCDBG(PPCDBG_SYS64X, "sys_ipc - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	
	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	ret = -ENOSYS;
	switch (call) {
	case SEMOP:
		ret = sys_semop (first, (struct sembuf *)ptr, second);
		break;
	case SEMGET:
		ret = sys_semget (first, second, third);
		break;
	case SEMCTL: {
		union semun fourth;

		if (!ptr)
			break;
		if ((ret = verify_area (VERIFY_READ, ptr, sizeof(long)))
		    || (ret = get_user(fourth.__pad, (void **)ptr)))
			break;
		ret = sys_semctl (first, second, third, fourth);
		break;
	}
	case MSGSND:
		ret = sys_msgsnd (first, (struct msgbuf *) ptr, second, third);
		break;
	case MSGRCV:
		switch (version) {
		case 0: {
			struct ipc_kludge tmp;

			if (!ptr)
				break;
			if ((ret = verify_area (VERIFY_READ, ptr, sizeof(tmp)))
			    || (ret = copy_from_user(&tmp,
						(struct ipc_kludge *) ptr,
						sizeof (tmp))))
				break;
			ret = sys_msgrcv (first, (struct msgbuf *)tmp.msgp,
						second, tmp.msgtyp, third);
			break;
		}
		default:
			ret = sys_msgrcv (first, (struct msgbuf *) ptr,
					  second, fifth, third);
			break;
		}
		break;
	case MSGGET:
		ret = sys_msgget ((key_t) first, second);
		break;
	case MSGCTL:
		ret = sys_msgctl (first, second, (struct msqid_ds *) ptr);
		break;
	case SHMAT:
		switch (version) {
		default: {
			ulong raddr;

			if ((ret = verify_area(VERIFY_WRITE, (ulong*) third,
					       sizeof(ulong))))
				break;
			ret = sys_shmat (first, (char *) ptr, second, &raddr);
			if (ret)
				break;
			ret = put_user (raddr, (ulong *) third);
			break;
		}
		case 1:	/* iBCS2 emulator entry point */
			if (!segment_eq(get_fs(), get_ds()))
				break;
			ret = sys_shmat (first, (char *) ptr, second,
					 (ulong *) third);
			break;
		}
		break;
	case SHMDT: 
		ret = sys_shmdt ((char *)ptr);
		break;
	case SHMGET:
		ret = sys_shmget (first, second, third);
		break;
	case SHMCTL:
		ret = sys_shmctl (first, second, (struct shmid_ds *) ptr);
		break;
	}

	PPCDBG(PPCDBG_SYS64X, "sys_ipc - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return ret;
}

/*
 * sys_pipe() is the normal C calling standard for creating
 * a pipe. It's not the way unix traditionally does this, though.
 */
asmlinkage int sys_pipe(int *fildes)
{
	int fd[2];
	int error;
	
	PPCDBG(PPCDBG_SYS64X, "sys_pipe - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	error = do_pipe(fd);
	if (!error) {
		if (copy_to_user(fildes, fd, 2*sizeof(int)))
			error = -EFAULT;
	}
	
	PPCDBG(PPCDBG_SYS64X, "sys_pipe - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return error;
}

asmlinkage unsigned long sys_mmap(unsigned long addr, size_t len,
				  unsigned long prot, unsigned long flags,
				  unsigned long fd, off_t offset)
{
	struct file * file = NULL;
	unsigned long ret = -EBADF;

	PPCDBG(PPCDBG_SYS64X, "sys_mmap - entered - addr=%lx, len=%lx - pid=%ld, comm=%s \n", addr, len, current->pid, current->comm);
	
	if (!(flags & MAP_ANONYMOUS)) {
		if (!(file = fget(fd)))
			goto out;
	}
	
	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	down_write(&current->mm->mmap_sem);
	ret = do_mmap(file, addr, len, prot, flags, offset);
	up_write(&current->mm->mmap_sem);
	if (file)
		fput(file);

out:
	
	PPCDBG(PPCDBG_SYS64X, "sys_mmap - exited - ret=%x \n", ret);
	
	return ret;
}

asmlinkage int sys_pause(void)
{
	
	PPCDBG(PPCDBG_SYS64X, "sys_pause - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	
	PPCDBG(PPCDBG_SYS64X, "sys_pause - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return -ERESTARTNOHAND;
}

static int __init set_fakeppc(char *str)
{
	if (*str)
		return 0;
	init_task.personality = PER_LINUX32;
	return 1;
}
__setup("fakeppc", set_fakeppc);

asmlinkage int sys_uname(struct old_utsname * name)
{
	int err = -EFAULT;
	
	PPCDBG(PPCDBG_SYS64X, "sys_uname - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	down_read(&uts_sem);
	if (name && !copy_to_user(name, &system_utsname, sizeof (*name)))
		err = 0;
	up_read(&uts_sem);
	
	PPCDBG(PPCDBG_SYS64X, "sys_uname - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return err;
}

asmlinkage int sys_olduname(struct oldold_utsname * name)
{
	int error;
	
	PPCDBG(PPCDBG_SYS64X, "sys_olduname - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	if (!name)
		return -EFAULT;
	if (!access_ok(VERIFY_WRITE,name,sizeof(struct oldold_utsname)))
		return -EFAULT;
  
	down_read(&uts_sem);
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
	up_read(&uts_sem);

	error = error ? -EFAULT : 0;
	
	PPCDBG(PPCDBG_SYS64X, "sys_olduname - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	return error;
}

asmlinkage time_t sys64_time(time_t* tloc)
{
	time_t secs;
	time_t usecs;

	long tb_delta = tb_ticks_since(tb_last_stamp);
	tb_delta += (jiffies - wall_jiffies) * tb_ticks_per_jiffy;

	secs  = xtime.tv_sec;  
	usecs = xtime.tv_usec + tb_delta / tb_ticks_per_usec;
	while (usecs >= USEC_PER_SEC) {
		++secs;
		usecs -= USEC_PER_SEC;
	}

	if (tloc) {
		if (put_user(secs,tloc))
			secs = -EFAULT;
	}

	return secs;
}
