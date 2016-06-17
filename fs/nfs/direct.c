/*
 * linux/fs/nfs/direct.c
 *
 * High-performance direct I/O for the NFS client
 *
 * When an application requests uncached I/O, all read and write requests
 * are made directly to the server; data stored or fetched via these
 * requests is not cached in the Linux page cache.  The client does not
 * correct unaligned requests from applications.  All requested bytes are
 * held on permanent storage before a direct write system call returns to
 * an application.  Applications that manage their own data caching, such
 * as databases, make very good use of direct I/O on local file systems.
 *
 * Solaris implements an uncached I/O facility called directio() that
 * is used for backups and sequential I/O to very large files.  Solaris
 * also supports uncaching whole NFS partitions with "-o forcedirectio,"
 * an undocumented mount option.
 *
 * Note that I/O to read in executables (e.g. kernel_read) cannot use
 * direct (kiobuf) reads because there is no vma backing the passed-in
 * data buffer.
 *
 * Designed by Jeff Kimmel, Chuck Lever, and Trond Myklebust.
 *
 * Initial implementation:	12/2001 by Chuck Lever <cel@netapp.com>
 *
 * TODO:
 *
 * 1.  Use concurrent asynchronous network requests rather than
 *     serialized synchronous network requests for normal (non-sync)
 *     direct I/O.
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/errno.h>
#include <linux/nfs_fs.h>
#include <linux/smp_lock.h>
#include <linux/sunrpc/clnt.h>
#include <linux/iobuf.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#define NFSDBG_FACILITY		(NFSDBG_PAGECACHE | NFSDBG_VFS)
#define VERF_SIZE		(2 * sizeof(__u32))

static inline int
nfs_direct_read_rpc(struct file *file, struct nfs_readargs *arg)
{
	int result;
	struct inode * inode = file->f_dentry->d_inode;
	struct nfs_fattr fattr;
        struct rpc_message msg;
        struct nfs_readres res = { &fattr, arg->count, 0 };

#ifdef CONFIG_NFS_V3
	msg.rpc_proc = (NFS_PROTO(inode)->version == 3) ?
						NFS3PROC_READ : NFSPROC_READ;
#else
	msg.rpc_proc = NFSPROC_READ;
#endif
	msg.rpc_argp = arg;
        msg.rpc_resp = &res;

	lock_kernel();
        msg.rpc_cred = nfs_file_cred(file);
        fattr.valid = 0;
        result = rpc_call_sync(NFS_CLIENT(inode), &msg, 0);
	nfs_refresh_inode(inode, &fattr);
	unlock_kernel();

	return result;
}

static inline int
nfs_direct_write_rpc(struct file *file, struct nfs_writeargs *arg,
	struct nfs_writeverf *verf)
{
	int result;
	struct inode *inode = file->f_dentry->d_inode;
	struct nfs_fattr fattr;
        struct rpc_message msg;
        struct nfs_writeres res = { &fattr, verf, 0 };

#ifdef CONFIG_NFS_V3
	msg.rpc_proc = (NFS_PROTO(inode)->version == 3) ?
						NFS3PROC_WRITE : NFSPROC_WRITE;
#else
	msg.rpc_proc = NFSPROC_WRITE;
#endif
	msg.rpc_argp = arg;
        msg.rpc_resp = &res;

	lock_kernel();
	msg.rpc_cred = get_rpccred(nfs_file_cred(file));
	fattr.valid = 0;
        result = rpc_call_sync(NFS_CLIENT(inode), &msg, 0);
	nfs_write_attributes(inode, &fattr);
	put_rpccred(msg.rpc_cred);
	unlock_kernel();

#ifdef CONFIG_NFS_V3
	if (NFS_PROTO(inode)->version == 3) {
		if (result > 0) {
			if ((arg->stable == NFS_FILE_SYNC) &&
			    (verf->committed != NFS_FILE_SYNC)) {
				printk(KERN_ERR
				"%s: server didn't sync stable write request\n",
				__FUNCTION__);
				return -EIO;
			}

			if (result != arg->count) {
				printk(KERN_INFO
					"%s: short write, count=%u, result=%d\n",
					__FUNCTION__, arg->count, result);
			}
		}
		return result;
	} else {
#endif
        	verf->committed = NFS_FILE_SYNC; /* NFSv2 always syncs data */
		if (result == 0)
			return arg->count;
		return result;
#ifdef CONFIG_NFS_V3
	}
#endif
}

#ifdef CONFIG_NFS_V3
static inline int
nfs_direct_commit_rpc(struct inode *inode, loff_t offset, size_t count,
	struct nfs_writeverf *verf)
{
	int result;
	struct nfs_fattr fattr;
	struct nfs_writeargs	arg = { NFS_FH(inode), offset, count, 0, 0,
					NULL };
	struct nfs_writeres	res = { &fattr, verf, 0 };
	struct rpc_message	msg = { NFS3PROC_COMMIT, &arg, &res, NULL };

	fattr.valid = 0;

	lock_kernel();
	result = rpc_call_sync(NFS_CLIENT(inode), &msg, 0);
	nfs_write_attributes(inode, &fattr);
	unlock_kernel();

	return result;
}
#else
static inline int
nfs_direct_commit_rpc(struct inode *inode, loff_t offset, size_t count,
	struct nfs_writeverf *verf)
{
	return 0;
}
#endif

/*
 * Walk through the iobuf and create an iovec for each "rsize" bytes.
 */
static int
nfs_direct_read(struct file *file, struct kiobuf *iobuf, loff_t offset,
	size_t count)
{
	int curpage, total;
	int result = 0;
	struct inode *inode = file->f_dentry->d_inode;
	int rsize = NFS_SERVER(inode)->rsize;
	struct page *pages[NFS_READ_MAXIOV];
	struct nfs_readargs args = { NFS_FH(inode), offset, 0, iobuf->offset,
				     pages };

	total = 0;
	curpage = 0;
        while (count) {
		int len, request;
		struct page **dest = pages;

                request = count;
                if (count > rsize)
                        request = rsize;
		args.count = request;
		args.offset = offset;
		args.pgbase = (iobuf->offset + total) & ~PAGE_MASK;
		len = PAGE_SIZE - args.pgbase;

		do {
			struct page *page = iobuf->maplist[curpage];

			if (curpage >= iobuf->nr_pages || !page) {
				result = -EFAULT;
				goto out_err;
			}

			*dest++ = page;
			/* zero after the first iov */
			if (request < len)
				break;
			request -= len;
			len = PAGE_SIZE;
			curpage++;
		} while (request != 0);

                result = nfs_direct_read_rpc(file, &args);

                if (result < 0)
                        break;

                total += result;
                if (result < args.count)   /* NFSv2ism */
                        break;
                count -= result;
                offset += result;
        };
out_err:
	if (!total)
		return result;
	return total;
}

/*
 * Walk through the iobuf and create an iovec for each "wsize" bytes.
 * If only one network write is necessary, or if the O_SYNC flag or
 * 'sync' mount option are present, or if this is a V2 inode, use
 * FILE_SYNC.  Otherwise, use UNSTABLE and finish with a COMMIT.
 *
 * The mechanics of this function are much the same as nfs_direct_read,
 * with the added complexity of committing unstable writes.
 */
static int
nfs_direct_write(struct file *file, struct kiobuf *iobuf,
	loff_t offset, size_t count)
{
	int curpage, total;
	int need_commit = 0;
	int result = 0;
	loff_t save_offset = offset;
	struct inode *inode = file->f_dentry->d_inode;
	int wsize = NFS_SERVER(inode)->wsize;
	struct nfs_writeverf first_verf, ret_verf;
	struct page *pages[NFS_WRITE_MAXIOV];
        struct nfs_writeargs args = { NFS_FH(inode), 0, 0, NFS_FILE_SYNC, 0,
				pages };

#ifdef CONFIG_NFS_V3
	if ((NFS_PROTO(inode)->version == 3) && (count > wsize) &&
							(!IS_SYNC(inode)))
		args.stable = NFS_UNSTABLE;
#endif

retry:
	total = 0;
	curpage = 0;
        while (count) {
		int len, request;
		struct page **dest = pages;

                request = count;
                if (count > wsize)
                        request = wsize;
		args.count = request;
		args.offset = offset;
		args.pgbase = (iobuf->offset + total) & ~PAGE_MASK;
		len = PAGE_SIZE - args.pgbase;

		do {
			struct page *page = iobuf->maplist[curpage];

			if (curpage >= iobuf->nr_pages || !page) {
				result = -EFAULT;
				goto out_err;
			}

			*dest++ = page;
			/* zero after the first iov */
			if (request < len)
				break;
			request -= len;
			len = PAGE_SIZE;
			curpage++;
		} while (request != 0);

                result = nfs_direct_write_rpc(file, &args, &ret_verf);

                if (result < 0)
                        break;

		if (!total)
			memcpy(&first_verf.verifier, &ret_verf.verifier,
								VERF_SIZE);
		if (ret_verf.committed != NFS_FILE_SYNC) {
			need_commit = 1;
			if (memcmp(&first_verf.verifier, &ret_verf.verifier,
								VERF_SIZE))
				goto print_retry;
		}

                total += result;
                count -= result;
                offset += result;
        };

out_err:
	/*
	 * Commit data written so far, even in the event of an error
	 */
	if (need_commit) {
		if (nfs_direct_commit_rpc(inode, save_offset,
					iobuf->length - count, &ret_verf))
			goto print_retry;
		if (memcmp(&first_verf.verifier, &ret_verf.verifier,
								VERF_SIZE))
			goto print_retry;
	}

	if (!total)
		return result;
	return total;

print_retry:
	printk(KERN_INFO "%s: detected server restart; retrying with FILE_SYNC\n",
			__FUNCTION__);
	args.stable = NFS_FILE_SYNC;
	offset = save_offset;
	count = iobuf->length;
	goto retry;
}

/*
 * Read or write data, moving the data directly to/from the
 * application's buffer without caching in the page cache.
 *
 * Rules for direct I/O
 *
 * 1.  block size = 512 bytes or more
 * 2.  file byte offset is block aligned
 * 3.  byte count is a multiple of block size
 * 4.  user buffer is not aligned
 * 5.  user buffer is faulted in and pinned
 *
 * These are verified before we get here.
 */
int
nfs_direct_IO(int rw, struct file *file, struct kiobuf *iobuf,
	unsigned long blocknr, int blocksize)
{
	int result = -EINVAL;
	size_t count = iobuf->length;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	loff_t offset = (loff_t) blocknr << inode->i_blkbits;

	switch (rw) {
	case READ:
		dfprintk(VFS,
			"NFS: direct_IO(READ) (%s/%s) off/cnt(%Lu/%d)\n",
				dentry->d_parent->d_name.name,
					dentry->d_name.name, offset, count);

		result = nfs_direct_read(file, iobuf, offset, count);
		break;
	case WRITE:
		dfprintk(VFS,
			"NFS: direct_IO(WRITE) (%s/%s) off/cnt(%Lu/%d)\n",
				dentry->d_parent->d_name.name,
					dentry->d_name.name, offset, count);

		result = nfs_direct_write(file, iobuf, offset, count);
		break;
	default:
		break;
	}

	dfprintk(VFS, "NFS: direct_IO result = %d\n", result);
	return result;
}
