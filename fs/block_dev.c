/*
 *  linux/fs/block_dev.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2001  Andrea Arcangeli <andrea@suse.de> SuSE
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/locks.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/major.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/smp_lock.h>
#include <linux/iobuf.h>
#include <linux/highmem.h>
#include <linux/blkdev.h>
#include <linux/module.h>

#include <asm/uaccess.h>

static unsigned long max_block(kdev_t dev)
{
	unsigned int retval = ~0U;
	int major = MAJOR(dev);

	if (blk_size[major]) {
		int minor = MINOR(dev);
		unsigned int blocks = blk_size[major][minor];
		if (blocks) {
			unsigned int size = block_size(dev);
			unsigned int sizebits = blksize_bits(size);
			blocks += (size-1) >> BLOCK_SIZE_BITS;
			retval = blocks << (BLOCK_SIZE_BITS - sizebits);
			if (sizebits > BLOCK_SIZE_BITS)
				retval = blocks >> (sizebits - BLOCK_SIZE_BITS);
		}
	}
	return retval;
}

static loff_t blkdev_size(kdev_t dev)
{
	unsigned int blocks = ~0U;
	int major = MAJOR(dev);

	if (blk_size[major]) {
		int minor = MINOR(dev);
		blocks = blk_size[major][minor];
	}
	return (loff_t) blocks << BLOCK_SIZE_BITS;
}

/* Kill _all_ buffers, dirty or not.. */
static void kill_bdev(struct block_device *bdev)
{
	invalidate_bdev(bdev, 1);
	truncate_inode_pages(bdev->bd_inode->i_mapping, 0);
}	

int set_blocksize(kdev_t dev, int size)
{
	int oldsize;
	struct block_device *bdev;

	/* Size must be a power of two, and between 512 and PAGE_SIZE */
	if (size > PAGE_SIZE || size < 512 || (size & (size-1)))
		return -EINVAL;

	/* Size cannot be smaller than the size supported by the device */
	if (size < get_hardsect_size(dev))
		return -EINVAL;

	/* No blocksize array? Implies hardcoded BLOCK_SIZE */
	if (!blksize_size[MAJOR(dev)]) {
		if (size == BLOCK_SIZE)
			return 0;
		return -EINVAL;
	}

	oldsize = blksize_size[MAJOR(dev)][MINOR(dev)];
	if (oldsize == size)
		return 0;

	if (!oldsize && size == BLOCK_SIZE) {
		blksize_size[MAJOR(dev)][MINOR(dev)] = size;
		return 0;
	}

	/* Ok, we're actually changing the blocksize.. */
	bdev = bdget(dev);
	sync_buffers(dev, 2);
	blksize_size[MAJOR(dev)][MINOR(dev)] = size;
	bdev->bd_inode->i_blkbits = blksize_bits(size);
	kill_bdev(bdev);
	bdput(bdev);
	return 0;
}

int sb_set_blocksize(struct super_block *sb, int size)
{
	int bits;
	if (set_blocksize(sb->s_dev, size) < 0)
		return 0;
	sb->s_blocksize = size;
	for (bits = 9, size >>= 9; size >>= 1; bits++)
		;
	sb->s_blocksize_bits = bits;
	return sb->s_blocksize;
}

int sb_min_blocksize(struct super_block *sb, int size)
{
	int minsize = get_hardsect_size(sb->s_dev);
	if (size < minsize)
		size = minsize;
	return sb_set_blocksize(sb, size);
}

static int blkdev_get_block(struct inode * inode, long iblock, struct buffer_head * bh, int create)
{
	if (iblock >= max_block(inode->i_rdev))
		return -EIO;

	bh->b_dev = inode->i_rdev;
	bh->b_blocknr = iblock;
	bh->b_state |= 1UL << BH_Mapped;
	return 0;
}

static int blkdev_direct_IO(int rw, struct inode * inode, struct kiobuf * iobuf, unsigned long blocknr, int blocksize)
{
	return generic_direct_IO(rw, inode, iobuf, blocknr, blocksize, blkdev_get_block);
}

static int blkdev_writepage(struct page * page)
{
	return block_write_full_page(page, blkdev_get_block);
}

static int blkdev_readpage(struct file * file, struct page * page)
{
	return block_read_full_page(page, blkdev_get_block);
}

static int blkdev_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return block_prepare_write(page, from, to, blkdev_get_block);
}

static int blkdev_commit_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return block_commit_write(page, from, to);
}

/*
 * private llseek:
 * for a block special file file->f_dentry->d_inode->i_size is zero
 * so we compute the size by hand (just as in block_read/write above)
 */
static loff_t block_llseek(struct file *file, loff_t offset, int origin)
{
	/* ewww */
	loff_t size = file->f_dentry->d_inode->i_bdev->bd_inode->i_size;
	loff_t retval;

	switch (origin) {
		case 2:
			offset += size;
			break;
		case 1:
			offset += file->f_pos;
	}
	retval = -EINVAL;
	if (offset >= 0 && offset <= size) {
		if (offset != file->f_pos) {
			file->f_pos = offset;
			file->f_reada = 0;
			file->f_version = ++event;
		}
		retval = offset;
	}
	return retval;
}
	

static int __block_fsync(struct inode * inode)
{
	int ret, err;

	ret = filemap_fdatasync(inode->i_mapping);
	err = sync_buffers(inode->i_rdev, 1);
	if (err && !ret)
		ret = err;
	err = filemap_fdatawait(inode->i_mapping);
	if (err && !ret)
		ret = err;

	return ret;
}

/*
 *	Filp may be NULL when we are called by an msync of a vma
 *	since the vma has no handle.
 */
 
static int block_fsync(struct file *filp, struct dentry *dentry, int datasync)
{
	struct inode * inode = dentry->d_inode;

	return __block_fsync(inode);
}

/*
 * pseudo-fs
 */

static struct super_block *bd_read_super(struct super_block *sb, void *data, int silent)
{
	static struct super_operations sops = {};
	struct inode *root = new_inode(sb);
	if (!root)
		return NULL;
	root->i_mode = S_IFDIR | S_IRUSR | S_IWUSR;
	root->i_uid = root->i_gid = 0;
	root->i_atime = root->i_mtime = root->i_ctime = CURRENT_TIME;
	sb->s_maxbytes = ~0ULL;
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_magic = 0x62646576;
	sb->s_op = &sops;
	sb->s_root = d_alloc(NULL, &(const struct qstr) { "bdev:", 5, 0 });
	if (!sb->s_root) {
		iput(root);
		return NULL;
	}
	sb->s_root->d_sb = sb;
	sb->s_root->d_parent = sb->s_root;
	d_instantiate(sb->s_root, root);
	return sb;
}

static DECLARE_FSTYPE(bd_type, "bdev", bd_read_super, FS_NOMOUNT);

static struct vfsmount *bd_mnt;

/*
 * bdev cache handling - shamelessly stolen from inode.c
 * We use smaller hashtable, though.
 */

#define HASH_BITS	6
#define HASH_SIZE	(1UL << HASH_BITS)
#define HASH_MASK	(HASH_SIZE-1)
static struct list_head bdev_hashtable[HASH_SIZE];
static spinlock_t bdev_lock __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;
static kmem_cache_t * bdev_cachep;

#define alloc_bdev() \
	 ((struct block_device *) kmem_cache_alloc(bdev_cachep, SLAB_KERNEL))
#define destroy_bdev(bdev) kmem_cache_free(bdev_cachep, (bdev))

static void init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
{
	struct block_device * bdev = (struct block_device *) foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR)
	{
		memset(bdev, 0, sizeof(*bdev));
		sema_init(&bdev->bd_sem, 1);
		INIT_LIST_HEAD(&bdev->bd_inodes);
	}
}

void __init bdev_cache_init(void)
{
	int i, err;
	struct list_head *head = bdev_hashtable;

	i = HASH_SIZE;
	do {
		INIT_LIST_HEAD(head);
		head++;
		i--;
	} while (i);

	bdev_cachep = kmem_cache_create("bdev_cache",
					 sizeof(struct block_device),
					 0, SLAB_HWCACHE_ALIGN, init_once,
					 NULL);
	if (!bdev_cachep)
		panic("Cannot create bdev_cache SLAB cache");
	err = register_filesystem(&bd_type);
	if (err)
		panic("Cannot register bdev pseudo-fs");
	bd_mnt = kern_mount(&bd_type);
	err = PTR_ERR(bd_mnt);
	if (IS_ERR(bd_mnt))
		panic("Cannot create bdev pseudo-fs");
}

/*
 * Most likely _very_ bad one - but then it's hardly critical for small
 * /dev and can be fixed when somebody will need really large one.
 */
static inline unsigned long hash(dev_t dev)
{
	unsigned long tmp = dev;
	tmp = tmp + (tmp >> HASH_BITS) + (tmp >> HASH_BITS*2);
	return tmp & HASH_MASK;
}

static struct block_device *bdfind(dev_t dev, struct list_head *head)
{
	struct list_head *p;
	struct block_device *bdev;
	for (p=head->next; p!=head; p=p->next) {
		bdev = list_entry(p, struct block_device, bd_hash);
		if (bdev->bd_dev != dev)
			continue;
		atomic_inc(&bdev->bd_count);
		return bdev;
	}
	return NULL;
}

struct block_device *bdget(dev_t dev)
{
	struct list_head * head = bdev_hashtable + hash(dev);
	struct block_device *bdev, *new_bdev;
	spin_lock(&bdev_lock);
	bdev = bdfind(dev, head);
	spin_unlock(&bdev_lock);
	if (bdev)
		return bdev;
	new_bdev = alloc_bdev();
	if (new_bdev) {
		struct inode *inode = new_inode(bd_mnt->mnt_sb);
		if (inode) {
			kdev_t kdev = to_kdev_t(dev);
			atomic_set(&new_bdev->bd_count,1);
			new_bdev->bd_dev = dev;
			new_bdev->bd_op = NULL;
			new_bdev->bd_inode = inode;
			inode->i_rdev = kdev;
			inode->i_dev = kdev;
			inode->i_bdev = new_bdev;
			inode->i_data.a_ops = &def_blk_aops;
			inode->i_data.gfp_mask = GFP_USER;
			inode->i_mode = S_IFBLK;
			spin_lock(&bdev_lock);
			bdev = bdfind(dev, head);
			if (!bdev) {
				list_add(&new_bdev->bd_hash, head);
				spin_unlock(&bdev_lock);
				return new_bdev;
			}
			spin_unlock(&bdev_lock);
			iput(new_bdev->bd_inode);
		}
		destroy_bdev(new_bdev);
	}
	return bdev;
}

static inline void __bd_forget(struct inode *inode)
{
	list_del_init(&inode->i_devices);
	inode->i_bdev = NULL;
	inode->i_mapping = &inode->i_data;
}

void bdput(struct block_device *bdev)
{
	if (atomic_dec_and_lock(&bdev->bd_count, &bdev_lock)) {
		struct list_head *p;
		if (bdev->bd_openers)
			BUG();
		list_del(&bdev->bd_hash);
		while ( (p = bdev->bd_inodes.next) != &bdev->bd_inodes ) {
			__bd_forget(list_entry(p, struct inode, i_devices));
		}
		spin_unlock(&bdev_lock);
		iput(bdev->bd_inode);
		destroy_bdev(bdev);
	}
}
 
int bd_acquire(struct inode *inode)
{
	struct block_device *bdev;
	spin_lock(&bdev_lock);
	if (inode->i_bdev) {
		atomic_inc(&inode->i_bdev->bd_count);
		spin_unlock(&bdev_lock);
		return 0;
	}
	spin_unlock(&bdev_lock);
	bdev = bdget(kdev_t_to_nr(inode->i_rdev));
	if (!bdev)
		return -ENOMEM;
	spin_lock(&bdev_lock);
	if (!inode->i_bdev) {
		inode->i_bdev = bdev;
		inode->i_mapping = bdev->bd_inode->i_mapping;
		list_add(&inode->i_devices, &bdev->bd_inodes);
	} else if (inode->i_bdev != bdev)
		BUG();
	spin_unlock(&bdev_lock);
	return 0;
}

/* Call when you free inode */

void bd_forget(struct inode *inode)
{
	spin_lock(&bdev_lock);
	if (inode->i_bdev)
		__bd_forget(inode);
	spin_unlock(&bdev_lock);
}

static struct {
	const char *name;
	struct block_device_operations *bdops;
} blkdevs[MAX_BLKDEV];

int get_blkdev_list(char * p)
{
	int i;
	int len;

	len = sprintf(p, "\nBlock devices:\n");
	for (i = 0; i < MAX_BLKDEV ; i++) {
		if (blkdevs[i].bdops) {
			len += sprintf(p+len, "%3d %s\n", i, blkdevs[i].name);
		}
	}
	return len;
}

/*
	Return the function table of a device.
	Load the driver if needed.
*/
const struct block_device_operations * get_blkfops(unsigned int major)
{
	const struct block_device_operations *ret = NULL;

	/* major 0 is used for non-device mounts */
	if (major && major < MAX_BLKDEV) {
#ifdef CONFIG_KMOD
		if (!blkdevs[major].bdops) {
			char name[20];
			sprintf(name, "block-major-%d", major);
			request_module(name);
		}
#endif
		ret = blkdevs[major].bdops;
	}
	return ret;
}

int register_blkdev(unsigned int major, const char * name, struct block_device_operations *bdops)
{
	if (major == 0) {
		for (major = MAX_BLKDEV-1; major > 0; major--) {
			if (blkdevs[major].bdops == NULL) {
				blkdevs[major].name = name;
				blkdevs[major].bdops = bdops;
				return major;
			}
		}
		return -EBUSY;
	}
	if (major >= MAX_BLKDEV)
		return -EINVAL;
	if (blkdevs[major].bdops && blkdevs[major].bdops != bdops)
		return -EBUSY;
	blkdevs[major].name = name;
	blkdevs[major].bdops = bdops;
	return 0;
}

int unregister_blkdev(unsigned int major, const char * name)
{
	if (major >= MAX_BLKDEV)
		return -EINVAL;
	if (!blkdevs[major].bdops)
		return -EINVAL;
	if (strcmp(blkdevs[major].name, name))
		return -EINVAL;
	blkdevs[major].name = NULL;
	blkdevs[major].bdops = NULL;
	return 0;
}

/*
 * This routine checks whether a removable media has been changed,
 * and invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to lose :-)
 */
int check_disk_change(kdev_t dev)
{
	int i;
	const struct block_device_operations * bdops = NULL;

	i = MAJOR(dev);
	if (i < MAX_BLKDEV)
		bdops = blkdevs[i].bdops;
	if (bdops == NULL) {
		devfs_handle_t de;

		de = devfs_find_handle (NULL, NULL, i, MINOR (dev),
					DEVFS_SPECIAL_BLK, 0);
		if (de) {
			bdops = devfs_get_ops (de);
			devfs_put_ops (de); /* We're running in owner module */
		}
	}
	if (bdops == NULL)
		return 0;
	if (bdops->check_media_change == NULL)
		return 0;
	if (!bdops->check_media_change(dev))
		return 0;

	if (invalidate_device(dev, 0))
		printk("VFS: busy inodes on changed media.\n");

	if (bdops->revalidate)
		bdops->revalidate(dev);
	return 1;
}

int ioctl_by_bdev(struct block_device *bdev, unsigned cmd, unsigned long arg)
{
	int res;
	mm_segment_t old_fs = get_fs();

	if (!bdev->bd_op->ioctl)
		return -EINVAL;
	set_fs(KERNEL_DS);
	res = bdev->bd_op->ioctl(bdev->bd_inode, NULL, cmd, arg);
	set_fs(old_fs);
	return res;
}

static int do_open(struct block_device *bdev, struct inode *inode, struct file *file)
{
	int ret = -ENXIO;
	kdev_t dev = to_kdev_t(bdev->bd_dev);

	down(&bdev->bd_sem);
	lock_kernel();
	if (!bdev->bd_op)
		bdev->bd_op = get_blkfops(MAJOR(dev));
	if (bdev->bd_op) {
		ret = 0;
		if (bdev->bd_op->owner)
			__MOD_INC_USE_COUNT(bdev->bd_op->owner);
		if (bdev->bd_op->open)
			ret = bdev->bd_op->open(inode, file);
		if (!ret) {
			bdev->bd_openers++;
			bdev->bd_inode->i_size = blkdev_size(dev);
			bdev->bd_inode->i_blkbits = blksize_bits(block_size(dev));
		} else {
			if (bdev->bd_op->owner)
				__MOD_DEC_USE_COUNT(bdev->bd_op->owner);
			if (!bdev->bd_openers)
				bdev->bd_op = NULL;
		}
	}
	unlock_kernel();
	up(&bdev->bd_sem);
	if (ret)
		bdput(bdev);
	return ret;
}

int blkdev_get(struct block_device *bdev, mode_t mode, unsigned flags, int kind)
{
	/*
	 * This crockload is due to bad choice of ->open() type.
	 * It will go away.
	 * For now, block device ->open() routine must _not_
	 * examine anything in 'inode' argument except ->i_rdev.
	 */
	struct file fake_file = {};
	struct dentry fake_dentry = {};
	fake_file.f_mode = mode;
	fake_file.f_flags = flags;
	fake_file.f_dentry = &fake_dentry;
	fake_dentry.d_inode = bdev->bd_inode;

	return do_open(bdev, bdev->bd_inode, &fake_file);
}

int blkdev_open(struct inode * inode, struct file * filp)
{
	struct block_device *bdev;

	/*
	 * Preserve backwards compatibility and allow large file access
	 * even if userspace doesn't ask for it explicitly. Some mkfs
	 * binary needs it. We might want to drop this workaround
	 * during an unstable branch.
	 */
	filp->f_flags |= O_LARGEFILE;

	bd_acquire(inode);
	bdev = inode->i_bdev;

	return do_open(bdev, inode, filp);
}	

int blkdev_put(struct block_device *bdev, int kind)
{
	int ret = 0;
	kdev_t rdev = to_kdev_t(bdev->bd_dev); /* this should become bdev */
	struct inode *bd_inode = bdev->bd_inode;

	down(&bdev->bd_sem);
	lock_kernel();
	if (kind == BDEV_FILE)
		__block_fsync(bd_inode);
	else if (kind == BDEV_FS)
		fsync_no_super(rdev);
	if (!--bdev->bd_openers)
		kill_bdev(bdev);
	if (bdev->bd_op->release)
		ret = bdev->bd_op->release(bd_inode, NULL);
	if (bdev->bd_op->owner)
		__MOD_DEC_USE_COUNT(bdev->bd_op->owner);
	if (!bdev->bd_openers)
		bdev->bd_op = NULL;
	unlock_kernel();
	up(&bdev->bd_sem);
	bdput(bdev);
	return ret;
}

int blkdev_close(struct inode * inode, struct file * filp)
{
	return blkdev_put(inode->i_bdev, BDEV_FILE);
}

static int blkdev_ioctl(struct inode *inode, struct file *file, unsigned cmd,
			unsigned long arg)
{
	if (inode->i_bdev->bd_op->ioctl)
		return inode->i_bdev->bd_op->ioctl(inode, file, cmd, arg);
	return -EINVAL;
}

struct address_space_operations def_blk_aops = {
	readpage: blkdev_readpage,
	writepage: blkdev_writepage,
	sync_page: block_sync_page,
	prepare_write: blkdev_prepare_write,
	commit_write: blkdev_commit_write,
	direct_IO: blkdev_direct_IO,
};

struct file_operations def_blk_fops = {
	open:		blkdev_open,
	release:	blkdev_close,
	llseek:		block_llseek,
	read:		generic_file_read,
	write:		generic_file_write,
	mmap:		generic_file_mmap,
	fsync:		block_fsync,
	ioctl:		blkdev_ioctl,
};

const char * bdevname(kdev_t dev)
{
	static char buffer[32];
	const char * name = blkdevs[MAJOR(dev)].name;

	if (!name)
		name = "unknown-block";

	sprintf(buffer, "%s(%d,%d)", name, MAJOR(dev), MINOR(dev));
	return buffer;
}
