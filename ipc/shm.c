/*
 * linux/ipc/shm.c
 * Copyright (C) 1992, 1993 Krishna Balasubramanian
 *	 Many improvements/fixes by Bruno Haible.
 * Replaced `struct shm_desc' by `struct vm_area_struct', July 1994.
 * Fixed the shm swap deallocation (shm_unuse()), August 1998 Andrea Arcangeli.
 *
 * /proc/sysvipc/shm support (c) 1999 Dragos Acostachioaie <dragos@iname.com>
 * BIGMEM support, Andrea Arcangeli <andrea@suse.de>
 * SMP thread shm, Jean-Luc Boyard <jean-luc.boyard@siemens.fr>
 * HIGHMEM support, Ingo Molnar <mingo@redhat.com>
 * Make shmmax, shmall, shmmni sysctl'able, Christoph Rohland <cr@sap.com>
 * Shared /dev/zero support, Kanoj Sarcar <kanoj@sgi.com>
 * Move the mm functionality over to mm/shmem.c, Christoph Rohland <cr@sap.com>
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/shm.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/mman.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "util.h"

struct shmid_kernel /* private to the kernel */
{	
	struct kern_ipc_perm	shm_perm;
	struct file *		shm_file;
	int			id;
	unsigned long		shm_nattch;
	unsigned long		shm_segsz;
	time_t			shm_atim;
	time_t			shm_dtim;
	time_t			shm_ctim;
	pid_t			shm_cprid;
	pid_t			shm_lprid;
};

#define shm_flags	shm_perm.mode

static struct file_operations shm_file_operations;
static struct vm_operations_struct shm_vm_ops;

static struct ipc_ids shm_ids;

#define shm_lock(id)	((struct shmid_kernel*)ipc_lock(&shm_ids,id))
#define shm_unlock(id)	ipc_unlock(&shm_ids,id)
#define shm_lockall()	ipc_lockall(&shm_ids)
#define shm_unlockall()	ipc_unlockall(&shm_ids)
#define shm_get(id)	((struct shmid_kernel*)ipc_get(&shm_ids,id))
#define shm_buildid(id, seq) \
	ipc_buildid(&shm_ids, id, seq)

static int newseg (key_t key, int shmflg, size_t size);
static void shm_open (struct vm_area_struct *shmd);
static void shm_close (struct vm_area_struct *shmd);
#ifdef CONFIG_PROC_FS
static int sysvipc_shm_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data);
#endif

size_t	shm_ctlmax = SHMMAX;
size_t 	shm_ctlall = SHMALL;
int 	shm_ctlmni = SHMMNI;

static int shm_tot; /* total number of shared memory pages */

void __init shm_init (void)
{
	ipc_init_ids(&shm_ids, 1);
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("sysvipc/shm", 0, 0, sysvipc_shm_read_proc, NULL);
#endif
}

static inline int shm_checkid(struct shmid_kernel *s, int id)
{
	if (ipc_checkid(&shm_ids,&s->shm_perm,id))
		return -EIDRM;
	return 0;
}

static inline struct shmid_kernel *shm_rmid(int id)
{
	return (struct shmid_kernel *)ipc_rmid(&shm_ids,id);
}

static inline int shm_addid(struct shmid_kernel *shp)
{
	return ipc_addid(&shm_ids, &shp->shm_perm, shm_ctlmni+1);
}



static inline void shm_inc (int id) {
	struct shmid_kernel *shp;

	if(!(shp = shm_lock(id)))
		BUG();
	shp->shm_atim = CURRENT_TIME;
	shp->shm_lprid = current->pid;
	shp->shm_nattch++;
	shm_unlock(id);
}

/* This is called by fork, once for every shm attach. */
static void shm_open (struct vm_area_struct *shmd)
{
	shm_inc (shmd->vm_file->f_dentry->d_inode->i_ino);
}

/*
 * shm_destroy - free the struct shmid_kernel
 *
 * @shp: struct to free
 *
 * It has to be called with shp and shm_ids.sem locked,
 * but returns with shp unlocked and freed.
 */
static void shm_destroy (struct shmid_kernel *shp)
{
	shm_tot -= (shp->shm_segsz + PAGE_SIZE - 1) >> PAGE_SHIFT;
	shm_rmid (shp->id);
	shm_unlock(shp->id);
	shmem_lock(shp->shm_file, 0);
	fput (shp->shm_file);
	kfree (shp);
}

/*
 * remove the attach descriptor shmd.
 * free memory for segment if it is marked destroyed.
 * The descriptor has already been removed from the current->mm->mmap list
 * and will later be kfree()d.
 */
static void shm_close (struct vm_area_struct *shmd)
{
	struct file * file = shmd->vm_file;
	int id = file->f_dentry->d_inode->i_ino;
	struct shmid_kernel *shp;

	down (&shm_ids.sem);
	/* remove from the list of attaches of the shm segment */
	if(!(shp = shm_lock(id)))
		BUG();
	shp->shm_lprid = current->pid;
	shp->shm_dtim = CURRENT_TIME;
	shp->shm_nattch--;
	if(shp->shm_nattch == 0 &&
	   shp->shm_flags & SHM_DEST)
		shm_destroy (shp);
	else
		shm_unlock(id);
	up (&shm_ids.sem);
}

static int shm_mmap(struct file * file, struct vm_area_struct * vma)
{
	UPDATE_ATIME(file->f_dentry->d_inode);
	vma->vm_ops = &shm_vm_ops;
	shm_inc(file->f_dentry->d_inode->i_ino);
	return 0;
}

static struct file_operations shm_file_operations = {
	mmap:	shm_mmap
};

static struct vm_operations_struct shm_vm_ops = {
	open:	shm_open,	/* callback for a new vm-area open */
	close:	shm_close,	/* callback for when the vm-area is released */
	nopage:	shmem_nopage,
};

static int newseg (key_t key, int shmflg, size_t size)
{
	int error;
	struct shmid_kernel *shp;
	int numpages = (size + PAGE_SIZE -1) >> PAGE_SHIFT;
	struct file * file;
	char name[13];
	int id;

	if (size < SHMMIN || size > shm_ctlmax)
		return -EINVAL;

	if (shm_tot + numpages >= shm_ctlall)
		return -ENOSPC;

	shp = (struct shmid_kernel *) kmalloc (sizeof (*shp), GFP_USER);
	if (!shp)
		return -ENOMEM;
	sprintf (name, "SYSV%08x", key);
	file = shmem_file_setup(name, size);
	error = PTR_ERR(file);
	if (IS_ERR(file))
		goto no_file;

	error = -ENOSPC;
	id = shm_addid(shp);
	if(id == -1) 
		goto no_id;
	shp->shm_perm.key = key;
	shp->shm_flags = (shmflg & S_IRWXUGO);
	shp->shm_cprid = current->pid;
	shp->shm_lprid = 0;
	shp->shm_atim = shp->shm_dtim = 0;
	shp->shm_ctim = CURRENT_TIME;
	shp->shm_segsz = size;
	shp->shm_nattch = 0;
	shp->id = shm_buildid(id,shp->shm_perm.seq);
	shp->shm_file = file;
	file->f_dentry->d_inode->i_ino = shp->id;
	file->f_op = &shm_file_operations;
	shm_tot += numpages;
	shm_unlock (id);
	return shp->id;

no_id:
	fput(file);
no_file:
	kfree(shp);
	return error;
}

asmlinkage long sys_shmget (key_t key, size_t size, int shmflg)
{
	struct shmid_kernel *shp;
	int err, id = 0;

	down(&shm_ids.sem);
	if (key == IPC_PRIVATE) {
		err = newseg(key, shmflg, size);
	} else if ((id = ipc_findkey(&shm_ids, key)) == -1) {
		if (!(shmflg & IPC_CREAT))
			err = -ENOENT;
		else
			err = newseg(key, shmflg, size);
	} else if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
		err = -EEXIST;
	} else {
		shp = shm_lock(id);
		if(shp==NULL)
			BUG();
		if (shp->shm_segsz < size)
			err = -EINVAL;
		else if (ipcperms(&shp->shm_perm, shmflg))
			err = -EACCES;
		else
			err = shm_buildid(id, shp->shm_perm.seq);
		shm_unlock(id);
	}
	up(&shm_ids.sem);
	return err;
}

static inline unsigned long copy_shmid_to_user(void *buf, struct shmid64_ds *in, int version)
{
	switch(version) {
	case IPC_64:
		return copy_to_user(buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct shmid_ds out;

		ipc64_perm_to_ipc_perm(&in->shm_perm, &out.shm_perm);
		out.shm_segsz	= in->shm_segsz;
		out.shm_atime	= in->shm_atime;
		out.shm_dtime	= in->shm_dtime;
		out.shm_ctime	= in->shm_ctime;
		out.shm_cpid	= in->shm_cpid;
		out.shm_lpid	= in->shm_lpid;
		out.shm_nattch	= in->shm_nattch;

		return copy_to_user(buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

struct shm_setbuf {
	uid_t	uid;
	gid_t	gid;
	mode_t	mode;
};	

static inline unsigned long copy_shmid_from_user(struct shm_setbuf *out, void *buf, int version)
{
	switch(version) {
	case IPC_64:
	    {
		struct shmid64_ds tbuf;

		if (copy_from_user(&tbuf, buf, sizeof(tbuf)))
			return -EFAULT;

		out->uid	= tbuf.shm_perm.uid;
		out->gid	= tbuf.shm_perm.gid;
		out->mode	= tbuf.shm_flags;

		return 0;
	    }
	case IPC_OLD:
	    {
		struct shmid_ds tbuf_old;

		if (copy_from_user(&tbuf_old, buf, sizeof(tbuf_old)))
			return -EFAULT;

		out->uid	= tbuf_old.shm_perm.uid;
		out->gid	= tbuf_old.shm_perm.gid;
		out->mode	= tbuf_old.shm_flags;

		return 0;
	    }
	default:
		return -EINVAL;
	}
}

static inline unsigned long copy_shminfo_to_user(void *buf, struct shminfo64 *in, int version)
{
	switch(version) {
	case IPC_64:
		return copy_to_user(buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct shminfo out;

		if(in->shmmax > INT_MAX)
			out.shmmax = INT_MAX;
		else
			out.shmmax = (int)in->shmmax;

		out.shmmin	= in->shmmin;
		out.shmmni	= in->shmmni;
		out.shmseg	= in->shmseg;
		out.shmall	= in->shmall; 

		return copy_to_user(buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

static void shm_get_stat (unsigned long *rss, unsigned long *swp) 
{
	struct shmem_inode_info *info;
	int i;

	*rss = 0;
	*swp = 0;

	for(i = 0; i <= shm_ids.max_id; i++) {
		struct shmid_kernel* shp;
		struct inode * inode;

		shp = shm_get(i);
		if(shp == NULL)
			continue;
		inode = shp->shm_file->f_dentry->d_inode;
		info = SHMEM_I(inode);
		spin_lock (&info->lock);
		*rss += inode->i_mapping->nrpages;
		*swp += info->swapped;
		spin_unlock (&info->lock);
	}
}

asmlinkage long sys_shmctl (int shmid, int cmd, struct shmid_ds *buf)
{
	struct shm_setbuf setbuf;
	struct shmid_kernel *shp;
	int err, version;

	if (cmd < 0 || shmid < 0)
		return -EINVAL;

	version = ipc_parse_version(&cmd);

	switch (cmd) { /* replace with proc interface ? */
	case IPC_INFO:
	{
		struct shminfo64 shminfo;

		memset(&shminfo,0,sizeof(shminfo));
		shminfo.shmmni = shminfo.shmseg = shm_ctlmni;
		shminfo.shmmax = shm_ctlmax;
		shminfo.shmall = shm_ctlall;

		shminfo.shmmin = SHMMIN;
		if(copy_shminfo_to_user (buf, &shminfo, version))
			return -EFAULT;
		/* reading a integer is always atomic */
		err= shm_ids.max_id;
		if(err<0)
			err = 0;
		return err;
	}
	case SHM_INFO:
	{
		struct shm_info shm_info;

		memset(&shm_info,0,sizeof(shm_info));
		down(&shm_ids.sem);
		shm_lockall();
		shm_info.used_ids = shm_ids.in_use;
		shm_get_stat (&shm_info.shm_rss, &shm_info.shm_swp);
		shm_info.shm_tot = shm_tot;
		shm_info.swap_attempts = 0;
		shm_info.swap_successes = 0;
		err = shm_ids.max_id;
		shm_unlockall();
		up(&shm_ids.sem);
		if(copy_to_user (buf, &shm_info, sizeof(shm_info)))
			return -EFAULT;

		return err < 0 ? 0 : err;
	}
	case SHM_STAT:
	case IPC_STAT:
	{
		struct shmid64_ds tbuf;
		int result;
		memset(&tbuf, 0, sizeof(tbuf));
		shp = shm_lock(shmid);
		if(shp==NULL)
			return -EINVAL;
		if(cmd==SHM_STAT) {
			err = -EINVAL;
			if (shmid > shm_ids.max_id)
				goto out_unlock;
			result = shm_buildid(shmid, shp->shm_perm.seq);
		} else {
			err = shm_checkid(shp,shmid);
			if(err)
				goto out_unlock;
			result = 0;
		}
		err=-EACCES;
		if (ipcperms (&shp->shm_perm, S_IRUGO))
			goto out_unlock;
		kernel_to_ipc64_perm(&shp->shm_perm, &tbuf.shm_perm);
		tbuf.shm_segsz	= shp->shm_segsz;
		tbuf.shm_atime	= shp->shm_atim;
		tbuf.shm_dtime	= shp->shm_dtim;
		tbuf.shm_ctime	= shp->shm_ctim;
		tbuf.shm_cpid	= shp->shm_cprid;
		tbuf.shm_lpid	= shp->shm_lprid;
		tbuf.shm_nattch	= shp->shm_nattch;
		shm_unlock(shmid);
		if(copy_shmid_to_user (buf, &tbuf, version))
			return -EFAULT;
		return result;
	}
	case SHM_LOCK:
	case SHM_UNLOCK:
	{
/* Allow superuser to lock segment in memory */
/* Should the pages be faulted in here or leave it to user? */
/* need to determine interaction with current->swappable */
		if (!capable(CAP_IPC_LOCK))
			return -EPERM;

		shp = shm_lock(shmid);
		if(shp==NULL)
			return -EINVAL;
		err = shm_checkid(shp,shmid);
		if(err)
			goto out_unlock;
		if(cmd==SHM_LOCK) {
			shmem_lock(shp->shm_file, 1);
			shp->shm_flags |= SHM_LOCKED;
		} else {
			shmem_lock(shp->shm_file, 0);
			shp->shm_flags &= ~SHM_LOCKED;
		}
		shm_unlock(shmid);
		return err;
	}
	case IPC_RMID:
	{
		/*
		 *	We cannot simply remove the file. The SVID states
		 *	that the block remains until the last person
		 *	detaches from it, then is deleted. A shmat() on
		 *	an RMID segment is legal in older Linux and if 
		 *	we change it apps break...
		 *
		 *	Instead we set a destroyed flag, and then blow
		 *	the name away when the usage hits zero.
		 */
		down(&shm_ids.sem);
		shp = shm_lock(shmid);
		err = -EINVAL;
		if (shp == NULL) 
			goto out_up;
		err = shm_checkid(shp, shmid);
		if(err)
			goto out_unlock_up;
		if (current->euid != shp->shm_perm.uid &&
		    current->euid != shp->shm_perm.cuid && 
		    !capable(CAP_SYS_ADMIN)) {
			err=-EPERM;
			goto out_unlock_up;
		}
		if (shp->shm_nattch){
			shp->shm_flags |= SHM_DEST;
			/* Do not find it any more */
			shp->shm_perm.key = IPC_PRIVATE;
			shm_unlock(shmid);
		} else
			shm_destroy (shp);
		up(&shm_ids.sem);
		return err;
	}

	case IPC_SET:
	{
		if(copy_shmid_from_user (&setbuf, buf, version))
			return -EFAULT;
		down(&shm_ids.sem);
		shp = shm_lock(shmid);
		err=-EINVAL;
		if(shp==NULL)
			goto out_up;
		err = shm_checkid(shp,shmid);
		if(err)
			goto out_unlock_up;
		err=-EPERM;
		if (current->euid != shp->shm_perm.uid &&
		    current->euid != shp->shm_perm.cuid && 
		    !capable(CAP_SYS_ADMIN)) {
			goto out_unlock_up;
		}

		shp->shm_perm.uid = setbuf.uid;
		shp->shm_perm.gid = setbuf.gid;
		shp->shm_flags = (shp->shm_flags & ~S_IRWXUGO)
			| (setbuf.mode & S_IRWXUGO);
		shp->shm_ctim = CURRENT_TIME;
		break;
	}

	default:
		return -EINVAL;
	}

	err = 0;
out_unlock_up:
	shm_unlock(shmid);
out_up:
	up(&shm_ids.sem);
	return err;
out_unlock:
	shm_unlock(shmid);
	return err;
}

/*
 * Fix shmaddr, allocate descriptor, map shm, add attach descriptor to lists.
 */
asmlinkage long sys_shmat (int shmid, char *shmaddr, int shmflg, ulong *raddr)
{
	struct shmid_kernel *shp;
	unsigned long addr;
	unsigned long size;
	struct file * file;
	int    err;
	unsigned long flags;
	unsigned long prot;
	unsigned long o_flags;
	int acc_mode;
	void *user_addr;

	if (shmid < 0)
		return -EINVAL;

	if ((addr = (ulong)shmaddr)) {
		if (addr & (SHMLBA-1)) {
			if (shmflg & SHM_RND)
				addr &= ~(SHMLBA-1);	   /* round down */
			else
				return -EINVAL;
		}
		flags = MAP_SHARED | MAP_FIXED;
	} else {
		if ((shmflg & SHM_REMAP))
			return -EINVAL;

		flags = MAP_SHARED;
	}

	if (shmflg & SHM_RDONLY) {
		prot = PROT_READ;
		o_flags = O_RDONLY;
		acc_mode = S_IRUGO;
	} else {
		prot = PROT_READ | PROT_WRITE;
		o_flags = O_RDWR;
		acc_mode = S_IRUGO | S_IWUGO;
	}

	/*
	 * We cannot rely on the fs check since SYSV IPC does have an
	 * additional creator id...
	 */
	shp = shm_lock(shmid);
	if(shp == NULL)
		return -EINVAL;
	err = shm_checkid(shp,shmid);
	if (err) {
		shm_unlock(shmid);
		return err;
	}
	if (ipcperms(&shp->shm_perm, acc_mode)) {
		shm_unlock(shmid);
		return -EACCES;
	}
	file = shp->shm_file;
	size = file->f_dentry->d_inode->i_size;
	shp->shm_nattch++;
	shm_unlock(shmid);

	down_write(&current->mm->mmap_sem);
	if (addr && !(shmflg & SHM_REMAP)) {
		user_addr = ERR_PTR(-EINVAL);
		if (find_vma_intersection(current->mm, addr, addr + size))
			goto invalid;
		/*
		 * If shm segment goes below stack, make sure there is some
		 * space left for the stack to grow (at least 4 pages).
		 */
		if (addr < current->mm->start_stack &&
		    addr > current->mm->start_stack - size - PAGE_SIZE * 5)
			goto invalid;
	}
		
	user_addr = (void*) do_mmap (file, addr, size, prot, flags, 0);

invalid:
	up_write(&current->mm->mmap_sem);

	down (&shm_ids.sem);
	if(!(shp = shm_lock(shmid)))
		BUG();
	shp->shm_nattch--;
	if(shp->shm_nattch == 0 &&
	   shp->shm_flags & SHM_DEST)
		shm_destroy (shp);
	else
		shm_unlock(shmid);
	up (&shm_ids.sem);

	*raddr = (unsigned long) user_addr;
	err = 0;
	if (IS_ERR(user_addr))
		err = PTR_ERR(user_addr);
	return err;

}

/*
 * detach and kill segment if marked destroyed.
 * The work is done in shm_close.
 */
asmlinkage long sys_shmdt (char *shmaddr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *shmd, *shmdnext;
	int retval = -EINVAL;

	down_write(&mm->mmap_sem);
	for (shmd = mm->mmap; shmd; shmd = shmdnext) {
		shmdnext = shmd->vm_next;
		if (shmd->vm_ops == &shm_vm_ops
		    && shmd->vm_start - (shmd->vm_pgoff << PAGE_SHIFT) == (ulong) shmaddr) {
			do_munmap(mm, shmd->vm_start, shmd->vm_end - shmd->vm_start);
			retval = 0;
		}
	}
	up_write(&mm->mmap_sem);
	return retval;
}

#ifdef CONFIG_PROC_FS
static int sysvipc_shm_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	off_t pos = 0;
	off_t begin = 0;
	int i, len = 0;

	down(&shm_ids.sem);
	len += sprintf(buffer, "       key      shmid perms       size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime\n");

	for(i = 0; i <= shm_ids.max_id; i++) {
		struct shmid_kernel* shp;

		shp = shm_lock(i);
		if(shp!=NULL) {
#define SMALL_STRING "%10d %10d  %4o %10u %5u %5u  %5d %5u %5u %5u %5u %10lu %10lu %10lu\n"
#define BIG_STRING   "%10d %10d  %4o %21u %5u %5u  %5d %5u %5u %5u %5u %10lu %10lu %10lu\n"
			char *format;

			if (sizeof(size_t) <= sizeof(int))
				format = SMALL_STRING;
			else
				format = BIG_STRING;
			len += sprintf(buffer + len, format,
				shp->shm_perm.key,
				shm_buildid(i, shp->shm_perm.seq),
				shp->shm_flags,
				shp->shm_segsz,
				shp->shm_cprid,
				shp->shm_lprid,
				shp->shm_nattch,
				shp->shm_perm.uid,
				shp->shm_perm.gid,
				shp->shm_perm.cuid,
				shp->shm_perm.cgid,
				shp->shm_atim,
				shp->shm_dtim,
				shp->shm_ctim);
			shm_unlock(i);

			pos += len;
			if(pos < offset) {
				len = 0;
				begin = pos;
			}
			if(pos > offset + length)
				goto done;
		}
	}
	*eof = 1;
done:
	up(&shm_ids.sem);
	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if(len > length)
		len = length;
	if(len < 0)
		len = 0;
	return len;
}
#endif
