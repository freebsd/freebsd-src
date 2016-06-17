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

#include <linux/config.h>
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

#include <asm/uaccess.h>
#include <asm/ipc.h>
#include <asm/semaphore.h>

void
check_bugs(void)
{
}

int sys_ioperm(unsigned long from, unsigned long num, int on)
{
	printk(KERN_ERR "sys_ioperm()\n");
	return -EIO;
}

/*
 * sys_ipc() is the de-multiplexer for the SysV IPC calls..
 *
 * This is really horribly ugly.
 */
int
sys_ipc(uint call, int first, int second, int third, void *ptr, long fifth)
{
	int version, ret;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	ret = -EINVAL;
	switch (call) {
	case SEMOP:
		ret = sys_semtimedop(first, (struct sembuf *)ptr, second, NULL);
		break;
	case SEMTIMEDOP:
		ret = sys_semtimedop(first, (struct sembuf *)ptr, second,
				     (const struct timespec *)fifth);
		break;
	case SEMGET:
		ret = sys_semget(first, second, third);
		break;
	case SEMCTL: {
		union semun fourth;

		if (!ptr)
			break;
		if ((ret = verify_area(VERIFY_READ, ptr, sizeof(long)))
		    || (ret = get_user(fourth.__pad, (void **)ptr)))
			break;
		ret = sys_semctl(first, second, third, fourth);
		break;
		}
	case MSGSND:
		ret = sys_msgsnd(first, (struct msgbuf *) ptr, second, third);
		break;
	case MSGRCV:
		switch (version) {
		case 0: {
			struct ipc_kludge tmp;

			if (!ptr)
				break;
			if ((ret = verify_area(VERIFY_READ, ptr, sizeof(tmp)))
			    || (ret = copy_from_user(&tmp,
						(struct ipc_kludge *) ptr,
						sizeof (tmp))))
				break;
			ret = sys_msgrcv(first, tmp.msgp, second, tmp.msgtyp,
					 third);
			break;
			}
		default:
			ret = sys_msgrcv(first, (struct msgbuf *) ptr,
					 second, fifth, third);
			break;
		}
		break;
	case MSGGET:
		ret = sys_msgget((key_t) first, second);
		break;
	case MSGCTL:
		ret = sys_msgctl(first, second, (struct msqid_ds *) ptr);
		break;
	case SHMAT:
		switch (version) {
		default: {
			ulong raddr;

			if ((ret = verify_area(VERIFY_WRITE, (ulong*) third,
					       sizeof(ulong))))
				break;
			ret = sys_shmat(first, (char *) ptr, second, &raddr);
			if (ret)
				break;
			ret = put_user(raddr, (ulong *) third);
			break;
			}
		case 1:	/* iBCS2 emulator entry point */
			if (!segment_eq(get_fs(), get_ds()))
				break;
			ret = sys_shmat(first, (char *) ptr, second,
					(ulong *) third);
			break;
		}
		break;
	case SHMDT:
		ret = sys_shmdt((char *)ptr);
		break;
	case SHMGET:
		ret = sys_shmget(first, second, third);
		break;
	case SHMCTL:
		ret = sys_shmctl(first, second, (struct shmid_ds *) ptr);
		break;
	default:
		ret = -ENOSYS;
	}

	return ret;
}

/*
 * sys_pipe() is the normal C calling standard for creating
 * a pipe. It's not the way unix traditionally does this, though.
 */
int sys_pipe(int *fildes)
{
	int fd[2];
	int error;

	error = do_pipe(fd);
	if (!error) {
		if (copy_to_user(fildes, fd, 2*sizeof(int)))
			error = -EFAULT;
	}
	return error;
}

#ifndef CONFIG_40x
#define allow_mmap_address(addr)	1
#else
/* Blech.  On 40x allowing mmap() (MAP_FIXED) at the first few pages
 * of (any process's) virtual memory is a security hole due to chip
 * erratum #67 (and possibly also due to the (documented) bizarre
 * prefetch behaviour around 'sc' see S3.8.2.1 of the user manual). */
#define allow_mmap_address(addr)	((((addr) & PAGE_MASK) >= 0x2100) || suser())
#endif

static inline unsigned long
do_mmap2(unsigned long addr, size_t len,
	 unsigned long prot, unsigned long flags,
	 unsigned long fd, unsigned long pgoff)
{
	struct file * file = NULL;
	int ret = -EBADF;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		if (!(file = fget(fd)))
			goto out;
	}

	ret = -EINVAL;
	if ((! allow_mmap_address(addr)) && (flags & MAP_FIXED))
		goto out;
	
	down_write(&current->mm->mmap_sem);
	ret = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);
	if (file)
		fput(file);
out:
	return ret;
}

unsigned long sys_mmap2(unsigned long addr, size_t len,
			unsigned long prot, unsigned long flags,
			unsigned long fd, unsigned long pgoff)
{
	return do_mmap2(addr, len, prot, flags, fd, pgoff);
}

unsigned long sys_mmap(unsigned long addr, size_t len,
		       unsigned long prot, unsigned long flags,
		       unsigned long fd, off_t offset)
{
	int err = -EINVAL;
	unsigned long off = offset;

	if (offset & ~PAGE_MASK)
		goto out;

	err = do_mmap2(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
out:
	return err;
}

extern int sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

/*
 * Due to some executables calling the wrong select we sometimes
 * get wrong args.  This determines how the args are being passed
 * (a single ptr to them all args passed) then calls
 * sys_select() with the appropriate args. -- Cort
 */
int
ppc_select(int n, fd_set *inp, fd_set *outp, fd_set *exp, struct timeval *tvp)
{
	if ( (unsigned long)n >= 4096 )
	{
		unsigned long *buffer = (unsigned long *)n;
		if (verify_area(VERIFY_READ, buffer, 5*sizeof(unsigned long))
		    || __get_user(n, buffer)
		    || __get_user(inp, ((fd_set **)(buffer+1)))
		    || __get_user(outp, ((fd_set **)(buffer+2)))
		    || __get_user(exp, ((fd_set **)(buffer+3)))
		    || __get_user(tvp, ((struct timeval **)(buffer+4))))
			return -EFAULT;
	}
	return sys_select(n, inp, outp, exp, tvp);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return -ERESTARTNOHAND;
}

int sys_uname(struct old_utsname * name)
{
	int err = -EFAULT;

	down_read(&uts_sem);
	if (name && !copy_to_user(name, &system_utsname, sizeof (*name)))
		err = 0;
	up_read(&uts_sem);
	return err;
}

int sys_olduname(struct oldold_utsname * name)
{
	int error;

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
	return error;
}

#ifndef CONFIG_PCI
/*
 * Those are normally defined in arch/ppc/kernel/pci.c. But when CONFIG_PCI is
 * not defined, this file is not linked at all, so here are the "empty" versions
 */
int sys_pciconfig_read(void) { return -ENOSYS; }
int sys_pciconfig_write(void) { return -ENOSYS; }
long sys_pciconfig_iobase(void) { return -ENOSYS; }
#endif
