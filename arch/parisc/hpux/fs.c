/*
 * linux/arch/parisc/kernel/sys_hpux.c
 *
 * implements HPUX syscalls.
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <asm/uaccess.h>

int hpux_execve(struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname((char *) regs->gr[26]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	error = do_execve(filename, (char **) regs->gr[25],
		(char **)regs->gr[24], regs);

	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);

out:
	return error;
}

struct hpux_dirent {
	long	d_off_pad; /* we only have a 32-bit off_t */
	long	d_off;
	ino_t	d_ino;
	short	d_reclen;
	short	d_namlen;
	char	d_name[1];
};

struct getdents_callback {
	struct hpux_dirent *current_dir;
	struct hpux_dirent *previous;
	int count;
	int error;
};

#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP(x) (((x)+sizeof(long)-1) & ~(sizeof(long)-1))

static int filldir(void * __buf, const char * name, int namlen, loff_t offset,
		ino_t ino, unsigned int d_type)
{
	struct hpux_dirent * dirent;
	struct getdents_callback * buf = (struct getdents_callback *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent)
		put_user(offset, &dirent->d_off);
	dirent = buf->current_dir;
	buf->previous = dirent;
	put_user(ino, &dirent->d_ino);
	put_user(reclen, &dirent->d_reclen);
	put_user(namlen, &dirent->d_namlen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	((char *) dirent) += reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
}

#undef NAME_OFFSET
#undef ROUND_UP

int hpux_getdents(unsigned int fd, struct hpux_dirent *dirent, unsigned int count)
{
	struct file * file;
	struct hpux_dirent * lastdirent;
	struct getdents_callback buf;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, filldir, &buf);
	if (error < 0)
		goto out_putf;
	error = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = count - buf.count;
	}

out_putf:
	fput(file);
out:
	return error;
}

int hpux_mount(const char *fs, const char *path, int mflag,
		const char *fstype, const char *dataptr, int datalen)
{
	return -ENOSYS;
}

static int cp_hpux_stat(struct inode * inode, struct hpux_stat64 * statbuf)
{
	struct hpux_stat64 tmp;
	unsigned int blocks, indirect;

	memset(&tmp, 0, sizeof(tmp));
	tmp.st_dev = kdev_t_to_nr(inode->i_dev);
	tmp.st_ino = inode->i_ino;
	tmp.st_mode = inode->i_mode;
	tmp.st_nlink = inode->i_nlink;
	tmp.st_uid = inode->i_uid;
	tmp.st_gid = inode->i_gid;
	tmp.st_rdev = kdev_t_to_nr(inode->i_rdev);
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;

#define D_B   7
#define I_B   (BLOCK_SIZE / sizeof(unsigned short))

	if (!inode->i_blksize) {
		blocks = (tmp.st_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
		if (blocks > D_B) {
			indirect = (blocks - D_B + I_B - 1) / I_B;
			blocks += indirect;
			if (indirect > 1) {
				indirect = (indirect - 1 + I_B - 1) / I_B;
				blocks += indirect;
				if (indirect > 1)
					blocks++;
			}
		}
		tmp.st_blocks = (BLOCK_SIZE / 512) * blocks;
		tmp.st_blksize = BLOCK_SIZE;
	} else {
		tmp.st_blocks = inode->i_blocks;
		tmp.st_blksize = inode->i_blksize;
	}
	return copy_to_user(statbuf,&tmp,sizeof(tmp)) ? -EFAULT : 0;
}

/*
 * Revalidate the inode. This is required for proper NFS attribute caching.
 * Blatently copied wholesale from fs/stat.c
 */
static __inline__ int
do_revalidate(struct dentry *dentry)
{
	struct inode * inode = dentry->d_inode;
	if (inode->i_op && inode->i_op->revalidate)
		return inode->i_op->revalidate(dentry);
	return 0;
}

long hpux_stat64(const char *path, struct hpux_stat64 *buf)
{
	struct nameidata nd;
	int error;

	lock_kernel();
	error = user_path_walk(path, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_hpux_stat(nd.dentry->d_inode, buf);
		path_release(&nd);
	}
	unlock_kernel();
	return error;
}

long hpux_fstat64(unsigned int fd, struct hpux_stat64 *statbuf)
{
	struct file * f;
	int err = -EBADF;

	lock_kernel();
	f = fget(fd);
	if (f) {
		struct dentry * dentry = f->f_dentry;

		err = do_revalidate(dentry);
		if (!err)
			err = cp_hpux_stat(dentry->d_inode, statbuf);
		fput(f);
	}
	unlock_kernel();
	return err;
}

long hpux_lstat64(char *filename, struct hpux_stat64 *statbuf)
{
	struct nameidata nd;
	int error;

	lock_kernel();
	error = user_path_walk_link(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_hpux_stat(nd.dentry->d_inode, statbuf);
		path_release(&nd);
	}
	unlock_kernel();
	return error;
}
