/*
 *  linux/fs/pipe.c
 *
 *  Copyright (C) 1991, 1992, 1999  Linus Torvalds
 */

#include <linux/mm.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/ioctls.h>

/*
 * We use a start+len construction, which provides full use of the 
 * allocated memory.
 * -- Florian Coosmann (FGC)
 * 
 * Reads with count = 0 should always return 0.
 * -- Julian Bradfield 1999-06-07.
 */

/* Drop the inode semaphore and wait for a pipe event, atomically */
void pipe_wait(struct inode * inode)
{
	DECLARE_WAITQUEUE(wait, current);
	current->state = TASK_INTERRUPTIBLE;
	add_wait_queue(PIPE_WAIT(*inode), &wait);
	up(PIPE_SEM(*inode));
	schedule();
	remove_wait_queue(PIPE_WAIT(*inode), &wait);
	current->state = TASK_RUNNING;
	down(PIPE_SEM(*inode));
}

static ssize_t
pipe_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	ssize_t size, read, ret;

	/* Seeks are not allowed on pipes.  */
	ret = -ESPIPE;
	read = 0;
	if (ppos != &filp->f_pos)
		goto out_nolock;

	/* Always return 0 on null read.  */
	ret = 0;
	if (count == 0)
		goto out_nolock;

	/* Get the pipe semaphore */
	ret = -ERESTARTSYS;
	if (down_interruptible(PIPE_SEM(*inode)))
		goto out_nolock;

	if (PIPE_EMPTY(*inode)) {
do_more_read:
		ret = 0;
		if (!PIPE_WRITERS(*inode))
			goto out;

		ret = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			goto out;

		for (;;) {
			PIPE_WAITING_READERS(*inode)++;
			pipe_wait(inode);
			PIPE_WAITING_READERS(*inode)--;
			ret = -ERESTARTSYS;
			if (signal_pending(current))
				goto out;
			ret = 0;
			if (!PIPE_EMPTY(*inode))
				break;
			if (!PIPE_WRITERS(*inode))
				goto out;
		}
	}

	/* Read what data is available.  */
	ret = -EFAULT;
	while (count > 0 && (size = PIPE_LEN(*inode))) {
		char *pipebuf = PIPE_BASE(*inode) + PIPE_START(*inode);
		ssize_t chars = PIPE_MAX_RCHUNK(*inode);

		if (chars > count)
			chars = count;
		if (chars > size)
			chars = size;

		if (copy_to_user(buf, pipebuf, chars))
			goto out;

		read += chars;
		PIPE_START(*inode) += chars;
		PIPE_START(*inode) &= (PIPE_SIZE - 1);
		PIPE_LEN(*inode) -= chars;
		count -= chars;
		buf += chars;
	}

	/* Cache behaviour optimization */
	if (!PIPE_LEN(*inode))
		PIPE_START(*inode) = 0;

	if (count && PIPE_WAITING_WRITERS(*inode) && !(filp->f_flags & O_NONBLOCK)) {
		/*
		 * We know that we are going to sleep: signal
		 * writers synchronously that there is more
		 * room.
		 */
		wake_up_interruptible_sync(PIPE_WAIT(*inode));
		if (!PIPE_EMPTY(*inode))
			BUG();
		goto do_more_read;
	}
	/* Signal writers asynchronously that there is more room.  */
	wake_up_interruptible(PIPE_WAIT(*inode));

	ret = read;
out:
	up(PIPE_SEM(*inode));
out_nolock:
	if (read)
		ret = read;

	UPDATE_ATIME(inode);
	return ret;
}

static ssize_t
pipe_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	ssize_t free, written, ret;

	/* Seeks are not allowed on pipes.  */
	ret = -ESPIPE;
	written = 0;
	if (ppos != &filp->f_pos)
		goto out_nolock;

	/* Null write succeeds.  */
	ret = 0;
	if (count == 0)
		goto out_nolock;

	ret = -ERESTARTSYS;
	if (down_interruptible(PIPE_SEM(*inode)))
		goto out_nolock;

	/* No readers yields SIGPIPE.  */
	if (!PIPE_READERS(*inode))
		goto sigpipe;

	/* If count <= PIPE_BUF, we have to make it atomic.  */
	free = (count <= PIPE_BUF ? count : 1);

	/* Wait, or check for, available space.  */
	if (filp->f_flags & O_NONBLOCK) {
		ret = -EAGAIN;
		if (PIPE_FREE(*inode) < free)
			goto out;
	} else {
		while (PIPE_FREE(*inode) < free) {
			PIPE_WAITING_WRITERS(*inode)++;
			pipe_wait(inode);
			PIPE_WAITING_WRITERS(*inode)--;
			ret = -ERESTARTSYS;
			if (signal_pending(current))
				goto out;

			if (!PIPE_READERS(*inode))
				goto sigpipe;
		}
	}

	/* Copy into available space.  */
	ret = -EFAULT;
	while (count > 0) {
		int space;
		char *pipebuf = PIPE_BASE(*inode) + PIPE_END(*inode);
		ssize_t chars = PIPE_MAX_WCHUNK(*inode);

		if ((space = PIPE_FREE(*inode)) != 0) {
			if (chars > count)
				chars = count;
			if (chars > space)
				chars = space;

			if (copy_from_user(pipebuf, buf, chars))
				goto out;

			written += chars;
			PIPE_LEN(*inode) += chars;
			count -= chars;
			buf += chars;
			space = PIPE_FREE(*inode);
			continue;
		}

		ret = written;
		if (filp->f_flags & O_NONBLOCK)
			break;

		do {
			/*
			 * Synchronous wake-up: it knows that this process
			 * is going to give up this CPU, so it doesn't have
			 * to do idle reschedules.
			 */
			wake_up_interruptible_sync(PIPE_WAIT(*inode));
			PIPE_WAITING_WRITERS(*inode)++;
			pipe_wait(inode);
			PIPE_WAITING_WRITERS(*inode)--;
			if (signal_pending(current))
				goto out;
			if (!PIPE_READERS(*inode))
				goto sigpipe;
		} while (!PIPE_FREE(*inode));
		ret = -EFAULT;
	}

	/* Signal readers asynchronously that there is more data.  */
	wake_up_interruptible(PIPE_WAIT(*inode));

	update_mctime(inode);

out:
	up(PIPE_SEM(*inode));
out_nolock:
	if (written)
		ret = written;
	return ret;

sigpipe:
	if (written)
		goto out;
	up(PIPE_SEM(*inode));
	send_sig(SIGPIPE, current, 0);
	return -EPIPE;
}

static ssize_t
bad_pipe_r(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	return -EBADF;
}

static ssize_t
bad_pipe_w(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	return -EBADF;
}

static int
pipe_ioctl(struct inode *pino, struct file *filp,
	   unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
		case FIONREAD:
			return put_user(PIPE_LEN(*pino), (int *)arg);
		default:
			return -EINVAL;
	}
}

/* No kernel lock held - fine */
static unsigned int
pipe_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask;
	struct inode *inode = filp->f_dentry->d_inode;

	poll_wait(filp, PIPE_WAIT(*inode), wait);

	/* Reading only -- no need for acquiring the semaphore.  */
	mask = POLLIN | POLLRDNORM;
	if (PIPE_EMPTY(*inode))
		mask = POLLOUT | POLLWRNORM;
	if (!PIPE_WRITERS(*inode) && filp->f_version != PIPE_WCOUNTER(*inode))
		mask |= POLLHUP;
	if (!PIPE_READERS(*inode))
		mask |= POLLERR;

	return mask;
}

/* FIXME: most Unices do not set POLLERR for fifos */
#define fifo_poll pipe_poll

static int
pipe_release(struct inode *inode, int decr, int decw)
{
	down(PIPE_SEM(*inode));
	PIPE_READERS(*inode) -= decr;
	PIPE_WRITERS(*inode) -= decw;
	if (!PIPE_READERS(*inode) && !PIPE_WRITERS(*inode)) {
		struct pipe_inode_info *info = inode->i_pipe;
		inode->i_pipe = NULL;
		free_page((unsigned long) info->base);
		kfree(info);
	} else {
		wake_up_interruptible(PIPE_WAIT(*inode));
	}
	up(PIPE_SEM(*inode));

	return 0;
}

static int
pipe_read_release(struct inode *inode, struct file *filp)
{
	return pipe_release(inode, 1, 0);
}

static int
pipe_write_release(struct inode *inode, struct file *filp)
{
	return pipe_release(inode, 0, 1);
}

static int
pipe_rdwr_release(struct inode *inode, struct file *filp)
{
	int decr, decw;

	decr = (filp->f_mode & FMODE_READ) != 0;
	decw = (filp->f_mode & FMODE_WRITE) != 0;
	return pipe_release(inode, decr, decw);
}

static int
pipe_read_open(struct inode *inode, struct file *filp)
{
	/* We could have perhaps used atomic_t, but this and friends
	   below are the only places.  So it doesn't seem worthwhile.  */
	down(PIPE_SEM(*inode));
	PIPE_READERS(*inode)++;
	up(PIPE_SEM(*inode));

	return 0;
}

static int
pipe_write_open(struct inode *inode, struct file *filp)
{
	down(PIPE_SEM(*inode));
	PIPE_WRITERS(*inode)++;
	up(PIPE_SEM(*inode));

	return 0;
}

static int
pipe_rdwr_open(struct inode *inode, struct file *filp)
{
	down(PIPE_SEM(*inode));
	if (filp->f_mode & FMODE_READ)
		PIPE_READERS(*inode)++;
	if (filp->f_mode & FMODE_WRITE)
		PIPE_WRITERS(*inode)++;
	up(PIPE_SEM(*inode));

	return 0;
}

/*
 * The file_operations structs are not static because they
 * are also used in linux/fs/fifo.c to do operations on FIFOs.
 */
struct file_operations read_fifo_fops = {
	llseek:		no_llseek,
	read:		pipe_read,
	write:		bad_pipe_w,
	poll:		fifo_poll,
	ioctl:		pipe_ioctl,
	open:		pipe_read_open,
	release:	pipe_read_release,
};

struct file_operations write_fifo_fops = {
	llseek:		no_llseek,
	read:		bad_pipe_r,
	write:		pipe_write,
	poll:		fifo_poll,
	ioctl:		pipe_ioctl,
	open:		pipe_write_open,
	release:	pipe_write_release,
};

struct file_operations rdwr_fifo_fops = {
	llseek:		no_llseek,
	read:		pipe_read,
	write:		pipe_write,
	poll:		fifo_poll,
	ioctl:		pipe_ioctl,
	open:		pipe_rdwr_open,
	release:	pipe_rdwr_release,
};

struct file_operations read_pipe_fops = {
	llseek:		no_llseek,
	read:		pipe_read,
	write:		bad_pipe_w,
	poll:		pipe_poll,
	ioctl:		pipe_ioctl,
	open:		pipe_read_open,
	release:	pipe_read_release,
};

struct file_operations write_pipe_fops = {
	llseek:		no_llseek,
	read:		bad_pipe_r,
	write:		pipe_write,
	poll:		pipe_poll,
	ioctl:		pipe_ioctl,
	open:		pipe_write_open,
	release:	pipe_write_release,
};

struct file_operations rdwr_pipe_fops = {
	llseek:		no_llseek,
	read:		pipe_read,
	write:		pipe_write,
	poll:		pipe_poll,
	ioctl:		pipe_ioctl,
	open:		pipe_rdwr_open,
	release:	pipe_rdwr_release,
};

struct inode* pipe_new(struct inode* inode)
{
	unsigned long page;

	page = __get_free_page(GFP_USER);
	if (!page)
		return NULL;

	inode->i_pipe = kmalloc(sizeof(struct pipe_inode_info), GFP_KERNEL);
	if (!inode->i_pipe)
		goto fail_page;

	init_waitqueue_head(PIPE_WAIT(*inode));
	PIPE_BASE(*inode) = (char*) page;
	PIPE_START(*inode) = PIPE_LEN(*inode) = 0;
	PIPE_READERS(*inode) = PIPE_WRITERS(*inode) = 0;
	PIPE_WAITING_READERS(*inode) = PIPE_WAITING_WRITERS(*inode) = 0;
	PIPE_RCOUNTER(*inode) = PIPE_WCOUNTER(*inode) = 1;

	return inode;
fail_page:
	free_page(page);
	return NULL;
}

static struct vfsmount *pipe_mnt;
static int pipefs_delete_dentry(struct dentry *dentry)
{
	return 1;
}
static struct dentry_operations pipefs_dentry_operations = {
	d_delete:	pipefs_delete_dentry,
};

static struct inode * get_pipe_inode(void)
{
	struct inode *inode = new_inode(pipe_mnt->mnt_sb);

	if (!inode)
		goto fail_inode;

	if(!pipe_new(inode))
		goto fail_iput;
	PIPE_READERS(*inode) = PIPE_WRITERS(*inode) = 1;
	inode->i_fop = &rdwr_pipe_fops;

	/*
	 * Mark the inode dirty from the very beginning,
	 * that way it will never be moved to the dirty
	 * list because "mark_inode_dirty()" will think
	 * that it already _is_ on the dirty list.
	 */
	inode->i_state = I_DIRTY;
	inode->i_mode = S_IFIFO | S_IRUSR | S_IWUSR;
	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_blksize = PAGE_SIZE;
	return inode;

fail_iput:
	iput(inode);
fail_inode:
	return NULL;
}

int do_pipe(int *fd)
{
	struct qstr this;
	char name[32];
	struct dentry *dentry;
	struct inode * inode;
	struct file *f1, *f2;
	int error;
	int i,j;

	error = -ENFILE;
	f1 = get_empty_filp();
	if (!f1)
		goto no_files;

	f2 = get_empty_filp();
	if (!f2)
		goto close_f1;

	inode = get_pipe_inode();
	if (!inode)
		goto close_f12;

	error = get_unused_fd();
	if (error < 0)
		goto close_f12_inode;
	i = error;

	error = get_unused_fd();
	if (error < 0)
		goto close_f12_inode_i;
	j = error;

	error = -ENOMEM;
	sprintf(name, "[%lu]", inode->i_ino);
	this.name = name;
	this.len = strlen(name);
	this.hash = inode->i_ino; /* will go */
	dentry = d_alloc(pipe_mnt->mnt_sb->s_root, &this);
	if (!dentry)
		goto close_f12_inode_i_j;
	dentry->d_op = &pipefs_dentry_operations;
	d_add(dentry, inode);
	f1->f_vfsmnt = f2->f_vfsmnt = mntget(mntget(pipe_mnt));
	f1->f_dentry = f2->f_dentry = dget(dentry);

	/* read file */
	f1->f_pos = f2->f_pos = 0;
	f1->f_flags = O_RDONLY;
	f1->f_op = &read_pipe_fops;
	f1->f_mode = 1;
	f1->f_version = 0;

	/* write file */
	f2->f_flags = O_WRONLY;
	f2->f_op = &write_pipe_fops;
	f2->f_mode = 2;
	f2->f_version = 0;

	fd_install(i, f1);
	fd_install(j, f2);
	fd[0] = i;
	fd[1] = j;
	return 0;

close_f12_inode_i_j:
	put_unused_fd(j);
close_f12_inode_i:
	put_unused_fd(i);
close_f12_inode:
	free_page((unsigned long) PIPE_BASE(*inode));
	kfree(inode->i_pipe);
	inode->i_pipe = NULL;
	iput(inode);
close_f12:
	put_filp(f2);
close_f1:
	put_filp(f1);
no_files:
	return error;	
}

/*
 * pipefs should _never_ be mounted by userland - too much of security hassle,
 * no real gain from having the whole whorehouse mounted. So we don't need
 * any operations on the root directory. However, we need a non-trivial
 * d_name - pipe: will go nicely and kill the special-casing in procfs.
 */
static int pipefs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = PIPEFS_MAGIC;
	buf->f_bsize = 1024;
	buf->f_namelen = 255;
	return 0;
}

static struct super_operations pipefs_ops = {
	statfs:		pipefs_statfs,
};

static struct super_block * pipefs_read_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root = new_inode(sb);
	if (!root)
		return NULL;
	root->i_mode = S_IFDIR | S_IRUSR | S_IWUSR;
	root->i_uid = root->i_gid = 0;
	root->i_atime = root->i_mtime = root->i_ctime = CURRENT_TIME;
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_magic = PIPEFS_MAGIC;
	sb->s_op	= &pipefs_ops;
	sb->s_root = d_alloc(NULL, &(const struct qstr) { "pipe:", 5, 0 });
	if (!sb->s_root) {
		iput(root);
		return NULL;
	}
	sb->s_root->d_sb = sb;
	sb->s_root->d_parent = sb->s_root;
	d_instantiate(sb->s_root, root);
	return sb;
}

static DECLARE_FSTYPE(pipe_fs_type, "pipefs", pipefs_read_super, FS_NOMOUNT);

static int __init init_pipe_fs(void)
{
	int err = register_filesystem(&pipe_fs_type);
	if (!err) {
		pipe_mnt = kern_mount(&pipe_fs_type);
		err = PTR_ERR(pipe_mnt);
		if (IS_ERR(pipe_mnt))
			unregister_filesystem(&pipe_fs_type);
		else
			err = 0;
	}
	return err;
}

static void __exit exit_pipe_fs(void)
{
	unregister_filesystem(&pipe_fs_type);
	mntput(pipe_mnt);
}

module_init(init_pipe_fs)
module_exit(exit_pipe_fs)
