/*
 * linux/arch/parisc/kernel/sys_parisc.c
 *
 * this implements syscalls which are handled per-arch.
 */

#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/linkage.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/shm.h>
#include <linux/smp_lock.h>

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

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return -ERESTARTNOHAND;
}

static unsigned long get_unshared_area(unsigned long addr, unsigned long len)
{
	struct vm_area_struct *vma;

	if (!addr)
		addr = TASK_UNMAPPED_BASE;
	addr = PAGE_ALIGN(addr);

	for (vma = find_vma(current->mm, addr); ; vma = vma->vm_next) {
		/* At this point:  (!vma || addr < vma->vm_end). */
		if (TASK_SIZE - len < addr)
			return -ENOMEM;
		if (!vma || addr + len <= vma->vm_start)
			return addr;
		addr = vma->vm_end;
	}
}

#define DCACHE_ALIGN(addr) (((addr) + (SHMLBA - 1)) &~ (SHMLBA - 1))

static unsigned long get_shared_area(struct inode *inode, unsigned long addr,
		unsigned long len, unsigned long pgoff)
{
	struct vm_area_struct *vma, *first_vma;
	int offset;

	first_vma = inode->i_mapping->i_mmap_shared;
	offset = (first_vma->vm_start + ((pgoff - first_vma->vm_pgoff) << PAGE_SHIFT)) & (SHMLBA - 1);

	if (!addr)
		addr = TASK_UNMAPPED_BASE;
	addr = DCACHE_ALIGN(addr - offset) + offset;

	for (vma = find_vma(current->mm, addr); ; vma = vma->vm_next) {
		/* At this point:  (!vma || addr < vma->vm_end). */
		if (TASK_SIZE - len < addr)
			return -ENOMEM;
		if (!vma || addr + len <= vma->vm_start)
			return addr;
		addr = DCACHE_ALIGN(vma->vm_end - offset) + offset;
		if (addr < vma->vm_end) /* handle wraparound */
			return -ENOMEM;
	}
}

unsigned long arch_get_unmapped_area(struct file *filp, unsigned long addr,
		unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct inode *inode = NULL;

	if (len > TASK_SIZE)
		return -ENOMEM;

	if (filp) {
		inode = filp->f_dentry->d_inode;
	}

	if (inode && (flags & MAP_SHARED) && (inode->i_mapping->i_mmap_shared)) {
		addr = get_shared_area(inode, addr, len, pgoff);
	} else {
		addr = get_unshared_area(addr, len);
	}
	return addr;
}

static unsigned long do_mmap2(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags, unsigned long fd,
	unsigned long pgoff)
{
	struct file * file = NULL;
	unsigned long error = -EBADF;
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file != NULL)
		fput(file);
out:
	return error;
}

asmlinkage unsigned long sys_mmap2(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags, unsigned long fd,
	unsigned long pgoff)
{
	/* Make sure the shift for mmap2 is constant (12), no matter what PAGE_SIZE
	   we have. */
	return do_mmap2(addr, len, prot, flags, fd, pgoff >> (PAGE_SHIFT - 12));
}

asmlinkage unsigned long sys_mmap(unsigned long addr, unsigned long len,
		unsigned long prot, unsigned long flags, unsigned long fd,
		unsigned long offset)
{
	if (!(offset & ~PAGE_MASK)) {
		return do_mmap2(addr, len, prot, flags, fd, offset >> PAGE_SHIFT);
	} else {
		return -EINVAL;
	}
}

long sys_shmat_wrapper(int shmid, char *shmaddr, int shmflag)
{
	unsigned long raddr;
	int r;

	r = sys_shmat(shmid, shmaddr, shmflag, &raddr);
	if (r < 0)
		return r;
	return raddr;
}


/*
 * FIXME, please remove this crap as soon as possible
 *
 * This is here to fix up broken glibc structures, 
 * which are already fixed in newer glibcs
 */
#include <linux/msg.h>
#include <linux/sem.h>
#include <linux/shm.h>
#include "sys32.h"

struct broken_ipc_perm
{
    key_t key;			/* Key.  */
    uid_t uid;			/* Owner's user ID.  */
    gid_t gid;			/* Owner's group ID.  */
    uid_t cuid;			/* Creator's user ID.  */
    gid_t cgid;			/* Creator's group ID.  */
    unsigned short int mode;		/* Read/write permission.  */
    unsigned short int __pad1;
    unsigned short int seq;		/* Sequence number.  */
    unsigned short int __pad2;
    unsigned long int __unused1;
    unsigned long int __unused2;
};
		    
struct broken_shmid64_ds {
	struct broken_ipc_perm	shm_perm;	/* operation perms */
	size_t			shm_segsz;	/* size of segment (bytes) */
#ifndef __LP64__
	unsigned int		__pad1;
#endif
	__kernel_time_t		shm_atime;	/* last attach time */
#ifndef __LP64__
	unsigned int		__pad2;
#endif
	__kernel_time_t		shm_dtime;	/* last detach time */
#ifndef __LP64__
	unsigned int		__pad3;
#endif
	__kernel_time_t		shm_ctime;	/* last change time */
	__kernel_pid_t		shm_cpid;	/* pid of creator */
	__kernel_pid_t		shm_lpid;	/* pid of last operator */
	unsigned int		shm_nattch;	/* no. of current attaches */
	unsigned int		__unused1;
	unsigned int		__unused2;
};

static void convert_broken_perm (struct broken_ipc_perm *out, struct ipc64_perm *in)
{
	out->key  = in->key;
	out->uid  = in->uid;
	out->gid  = in->gid;
	out->cuid = in->cuid;
	out->cgid = in->cgid;
	out->mode = in->mode;
	out->seq  = in->seq;
}

static int copyout_broken_shmid64(struct broken_shmid64_ds *buf, struct shmid64_ds *sbuf)
{
	struct broken_shmid64_ds tbuf;
	
	memset(&tbuf, 0, sizeof tbuf);
	convert_broken_perm (&tbuf.shm_perm, &sbuf->shm_perm);
	tbuf.shm_segsz = sbuf->shm_segsz;
	tbuf.shm_atime = sbuf->shm_atime;
	tbuf.shm_dtime = sbuf->shm_dtime;
	tbuf.shm_ctime = sbuf->shm_ctime;
	tbuf.shm_cpid = sbuf->shm_cpid;
	tbuf.shm_lpid = sbuf->shm_lpid;
	tbuf.shm_nattch = sbuf->shm_nattch;
	return copy_to_user(buf, &tbuf, sizeof tbuf);
}

int sys_msgctl_broken(int msqid, int cmd, struct msqid_ds *buf)
{
	return sys_msgctl (msqid, cmd & ~IPC_64, buf);
}

int sys_semctl_broken(int semid, int semnum, int cmd, union semun arg)
{
	return sys_semctl (semid, semnum, cmd & ~IPC_64, arg);
}

int sys_shmctl_broken(int shmid, int cmd, struct shmid64_ds *buf)
{
	struct shmid64_ds sbuf;
	int err;

	if (cmd & IPC_64) {
		cmd &= ~IPC_64;
		if (cmd == IPC_STAT || cmd == SHM_STAT) {
			KERNEL_SYSCALL(err, sys_shmctl, shmid, cmd, (struct shmid_ds *)&sbuf);
			if (err == 0)
				err = copyout_broken_shmid64((struct broken_shmid64_ds *)buf, &sbuf);
			return err;
		}
	}
	return sys_shmctl (shmid, cmd, (struct shmid_ds *)buf);
}

