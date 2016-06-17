#define MSNFS	/* HACK HACK */
/*
 * linux/fs/nfsd/vfs.c
 *
 * File operations used by nfsd. Some of these have been ripped from
 * other parts of the kernel because they weren't in ksyms.c, others
 * are partial duplicates with added or changed functionality.
 *
 * Note that several functions dget() the dentry upon which they want
 * to act, most notably those that create directory entries. Response
 * dentry's are dput()'d if necessary in the release callback.
 * So if you notice code paths that apparently fail to dput() the
 * dentry, don't worry--they have been taken care of.
 *
 * Copyright (C) 1995-1999 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/ext2_fs.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/net.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/in.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#ifdef CONFIG_NFSD_V3
#include <linux/nfs3.h>
#include <linux/nfsd/xdr3.h>
#endif /* CONFIG_NFSD_V3 */
#include <linux/nfsd/nfsfh.h>
#include <linux/quotaops.h>

#include <asm/uaccess.h>

#define NFSDDBG_FACILITY		NFSDDBG_FILEOP
#define NFSD_PARANOIA


/* We must ignore files (but only files) which might have mandatory
 * locks on them because there is no way to know if the accesser has
 * the lock.
 */
#define IS_ISMNDLK(i)	(S_ISREG((i)->i_mode) && MANDATORY_LOCK(i))

/*
 * This is a cache of readahead params that help us choose the proper
 * readahead strategy. Initially, we set all readahead parameters to 0
 * and let the VFS handle things.
 * If you increase the number of cached files very much, you'll need to
 * add a hash table here.
 */
struct raparms {
	struct raparms		*p_next;
	unsigned int		p_count;
	ino_t			p_ino;
	dev_t			p_dev;
	unsigned long		p_reada,
				p_ramax,
				p_raend,
				p_ralen,
				p_rawin;
};

static struct raparms *		raparml;
static struct raparms *		raparm_cache;

/*
 * Look up one component of a pathname.
 * N.B. After this call _both_ fhp and resfh need an fh_put
 *
 * If the lookup would cross a mountpoint, and the mounted filesystem
 * is exported to the client with NFSEXP_NOHIDE, then the lookup is
 * accepted as it stands and the mounted directory is
 * returned. Otherwise the covered directory is returned.
 * NOTE: this mountpoint crossing is not supported properly by all
 *   clients and is explicitly disallowed for NFSv3
 *      NeilBrown <neilb@cse.unsw.edu.au>
 */
int
nfsd_lookup(struct svc_rqst *rqstp, struct svc_fh *fhp, const char *name,
					int len, struct svc_fh *resfh)
{
	struct svc_export	*exp;
	struct dentry		*dparent;
	struct dentry		*dentry;
	int			err;

	dprintk("nfsd: nfsd_lookup(fh %s, %.*s)\n", SVCFH_fmt(fhp), len,name);

	/* Obtain dentry and export. */
	err = fh_verify(rqstp, fhp, S_IFDIR, MAY_EXEC);
	if (err)
		goto out;

	dparent = fhp->fh_dentry;
	exp  = fhp->fh_export;

	err = nfserr_acces;

	/* Lookup the name, but don't follow links */
	if (isdotent(name, len)) {
		if (len==1)
			dentry = dget(dparent);
		else if (dparent != exp->ex_dentry)
			dentry = dget(dparent->d_parent);
		else if (!EX_NOHIDE(exp))
			dentry = dget(dparent); /* .. == . just like at / */
		else {
			/* checking mountpoint crossing is very different when stepping up */
			struct svc_export *exp2 = NULL;
			struct dentry *dp;
			struct vfsmount *mnt = mntget(exp->ex_mnt);
			dentry = dget(dparent);
			while(follow_up(&mnt, &dentry))
				;
			dp = dget(dentry->d_parent);
			dput(dentry);
			dentry = dp;
			for ( ; exp2 == NULL && dp->d_parent != dp;
			      dp=dp->d_parent)
				exp2 = exp_get(exp->ex_client, dp->d_inode->i_dev, dp->d_inode->i_ino);
			if (exp2==NULL) {
				dput(dentry);
				dentry = dget(dparent);
			} else {
				exp = exp2;
			}
			mntput(mnt);
		}
	} else {
		fh_lock(fhp);
		dentry = lookup_one_len(name, dparent, len);
		err = PTR_ERR(dentry);
		if (IS_ERR(dentry))
			goto out_nfserr;
		/*
		 * check if we have crossed a mount point ...
		 */
		if (d_mountpoint(dentry)) {
			struct svc_export *exp2 = NULL;
			struct vfsmount *mnt = mntget(exp->ex_mnt);
			struct dentry *mounts = dget(dentry);
			while (follow_down(&mnt,&mounts)&&d_mountpoint(mounts))
				;
			exp2 = exp_get(rqstp->rq_client,
				       mounts->d_inode->i_dev,
				       mounts->d_inode->i_ino);
			if (exp2 && EX_NOHIDE(exp2)) {
				/* successfully crossed mount point */
				exp = exp2;
				dput(dentry);
				dentry = mounts;
			} else
				dput(mounts);
			mntput(mnt);
		}
	}

	if (dentry->d_inode && dentry->d_inode->i_op &&
	    dentry->d_inode->i_op->revalidate &&
	    dentry->d_inode->i_op->revalidate(dentry))
		err = nfserr_noent;
	else
		err = fh_compose(resfh, exp, dentry, fhp);
	if (!err && !dentry->d_inode)
		err = nfserr_noent;
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

/*
 * Set various file attributes.
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_setattr(struct svc_rqst *rqstp, struct svc_fh *fhp, struct iattr *iap,
	     int check_guard, time_t guardtime)
{
	struct dentry	*dentry;
	struct inode	*inode;
	int		accmode = MAY_SATTR;
	int		ftype = 0;
	int		imode;
	int		err;
	int		size_change = 0;

	if (iap->ia_valid & (ATTR_ATIME | ATTR_MTIME | ATTR_SIZE))
		accmode |= MAY_WRITE|MAY_OWNER_OVERRIDE;
	if (iap->ia_valid & ATTR_SIZE)
		ftype = S_IFREG;

	/* Get inode */
	err = fh_verify(rqstp, fhp, ftype, accmode);
	if (err || !iap->ia_valid)
		goto out;

	dentry = fhp->fh_dentry;
	inode = dentry->d_inode;

	/* NFSv2 does not differentiate between "set-[ac]time-to-now"
	 * which only requires access, and "set-[ac]time-to-X" which
	 * requires ownership.
	 * So if it looks like it might be "set both to the same time which
	 * is close to now", and if inode_change_ok fails, then we
	 * convert to "set to now" instead of "set to explicit time"
	 *
	 * We only call inode_change_ok as the last test as technically
	 * it is not an interface that we should be using.  It is only
	 * valid if the filesystem does not define it's own i_op->setattr.
	 */
#define BOTH_TIME_SET (ATTR_ATIME_SET | ATTR_MTIME_SET)
#define	MAX_TOUCH_TIME_ERROR (30*60)
	if ((iap->ia_valid & BOTH_TIME_SET) == BOTH_TIME_SET
	    && iap->ia_mtime == iap->ia_atime
	    ) {
	    /* Looks probable.  Now just make sure time is in the right ballpark.
	     * Solaris, at least, doesn't seem to care what the time request is.
	     * We require it be within 30 minutes of now.
	     */
	    time_t delta = iap->ia_atime - CURRENT_TIME;
	    if (delta<0) delta = -delta;
	    if (delta < MAX_TOUCH_TIME_ERROR &&
		inode_change_ok(inode, iap) != 0) {
		/* turn off ATTR_[AM]TIME_SET but leave ATTR_[AM]TIME
		 * this will cause notify_change to set these times to "now"
		 */
		iap->ia_valid &= ~BOTH_TIME_SET;
	    }
	}
	    
	/* The size case is special. It changes the file as well as the attributes.  */
	if (iap->ia_valid & ATTR_SIZE) {
		if (iap->ia_size < inode->i_size) {
			err = nfsd_permission(fhp->fh_export, dentry, MAY_TRUNC|MAY_OWNER_OVERRIDE);
			if (err)
				goto out;
		}

		/*
		 * If we are changing the size of the file, then
		 * we need to break all leases.
		 */
		err = get_lease(inode, FMODE_WRITE);
		if (err)
			goto out_nfserr;

		err = get_write_access(inode);
		if (err)
			goto out_nfserr;

		err = locks_verify_truncate(inode, NULL, iap->ia_size);
		if (err) {
			put_write_access(inode);
			goto out_nfserr;
		}
		DQUOT_INIT(inode);
	}

	imode = inode->i_mode;
	if (iap->ia_valid & ATTR_MODE) {
		iap->ia_mode &= S_IALLUGO;
		imode = iap->ia_mode |= (imode & ~S_IALLUGO);
	}

	/* Revoke setuid/setgid bit on chown/chgrp */
	if ((iap->ia_valid & ATTR_UID) && (imode & S_ISUID)
	 && iap->ia_uid != inode->i_uid) {
		iap->ia_valid |= ATTR_MODE;
		iap->ia_mode = imode &= ~S_ISUID;
	}
	if ((iap->ia_valid & ATTR_GID) && (imode & S_ISGID)
	 && iap->ia_gid != inode->i_gid) {
		iap->ia_valid |= ATTR_MODE;
		iap->ia_mode = imode &= ~S_ISGID;
	}

	/* Change the attributes. */


	iap->ia_valid |= ATTR_CTIME;

	if (iap->ia_valid & ATTR_SIZE) {
		fh_lock(fhp);
		size_change = 1;
	}
	err = nfserr_notsync;
	if (!check_guard || guardtime == inode->i_ctime) {
		err = notify_change(dentry, iap);
		err = nfserrno(err);
	}
	if (size_change) {
		fh_unlock(fhp);
		put_write_access(inode);
	}
	if (!err)
		if (EX_ISSYNC(fhp->fh_export))
			write_inode_now(inode, 1);
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

#ifdef CONFIG_NFSD_V3
/*
 * Check server access rights to a file system object
 */
struct accessmap {
	u32		access;
	int		how;
};
static struct accessmap	nfs3_regaccess[] = {
    {	NFS3_ACCESS_READ,	MAY_READ			},
    {	NFS3_ACCESS_EXECUTE,	MAY_EXEC			},
    {	NFS3_ACCESS_MODIFY,	MAY_WRITE|MAY_TRUNC		},
    {	NFS3_ACCESS_EXTEND,	MAY_WRITE			},

    {	0,			0				}
};

static struct accessmap	nfs3_diraccess[] = {
    {	NFS3_ACCESS_READ,	MAY_READ			},
    {	NFS3_ACCESS_LOOKUP,	MAY_EXEC			},
    {	NFS3_ACCESS_MODIFY,	MAY_EXEC|MAY_WRITE|MAY_TRUNC	},
    {	NFS3_ACCESS_EXTEND,	MAY_EXEC|MAY_WRITE		},
    {	NFS3_ACCESS_DELETE,	MAY_REMOVE			},

    {	0,			0				}
};

static struct accessmap	nfs3_anyaccess[] = {
	/* Some clients - Solaris 2.6 at least, make an access call
	 * to the server to check for access for things like /dev/null
	 * (which really, the server doesn't care about).  So
	 * We provide simple access checking for them, looking
	 * mainly at mode bits
	 */
    {	NFS3_ACCESS_READ,	MAY_READ			},
    {	NFS3_ACCESS_EXECUTE,	MAY_EXEC			},
    {	NFS3_ACCESS_MODIFY,	MAY_WRITE			},
    {	NFS3_ACCESS_EXTEND,	MAY_WRITE			},

    {	0,			0				}
};

int
nfsd_access(struct svc_rqst *rqstp, struct svc_fh *fhp, u32 *access)
{
	struct accessmap	*map;
	struct svc_export	*export;
	struct dentry		*dentry;
	u32			query, result = 0;
	unsigned int		error;

	error = fh_verify(rqstp, fhp, 0, MAY_NOP);
	if (error)
		goto out;

	export = fhp->fh_export;
	dentry = fhp->fh_dentry;

	if (S_ISREG(dentry->d_inode->i_mode))
		map = nfs3_regaccess;
	else if (S_ISDIR(dentry->d_inode->i_mode))
		map = nfs3_diraccess;
	else
		map = nfs3_anyaccess;


	query = *access;
	for  (; map->access; map++) {
		if (map->access & query) {
			unsigned int err2;
			err2 = nfsd_permission(export, dentry, map->how);
			switch (err2) {
			case nfs_ok:
				result |= map->access;
				break;
				
			/* the following error codes just mean the access was not allowed,
			 * rather than an error occurred */
			case nfserr_rofs:
			case nfserr_acces:
			case nfserr_perm:
				/* simply don't "or" in the access bit. */
				break;
			default:
				error = err2;
				goto out;
			}
		}
	}
	*access = result;

 out:
	return error;
}
#endif /* CONFIG_NFSD_V3 */



/*
 * Open an existing file or directory.
 * The access argument indicates the type of open (read/write/lock)
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_open(struct svc_rqst *rqstp, struct svc_fh *fhp, int type,
			int access, struct file *filp)
{
	struct dentry	*dentry;
	struct inode	*inode;
	int		err;

	/* If we get here, then the client has already done an "open", and (hopefully)
	 * checked permission - so allow OWNER_OVERRIDE in case a chmod has now revoked
	 * permission */
	err = fh_verify(rqstp, fhp, type, access | MAY_OWNER_OVERRIDE);
	if (err)
		goto out;

	dentry = fhp->fh_dentry;
	inode = dentry->d_inode;

	/* Disallow access to files with the append-only bit set or
	 * with mandatory locking enabled
	 */
	err = nfserr_perm;
	if (IS_APPEND(inode) || IS_ISMNDLK(inode))
		goto out;
	if (!inode->i_fop)
		goto out;

	/*
	 * Check to see if there are any leases on this file.
	 * This may block while leases are broken.
	 */
	err = get_lease(inode, (access & MAY_WRITE) ? FMODE_WRITE : 0);
	if (err)
		goto out_nfserr;

	if ((access & MAY_WRITE) && (err = get_write_access(inode)) != 0)
		goto out_nfserr;

	memset(filp, 0, sizeof(*filp));
	filp->f_op    = fops_get(inode->i_fop);
	atomic_set(&filp->f_count, 1);
	filp->f_dentry = dentry;
	filp->f_vfsmnt = fhp->fh_export->ex_mnt;
	if (access & MAY_WRITE) {
		filp->f_flags = O_WRONLY|O_LARGEFILE;
		filp->f_mode  = FMODE_WRITE;
		DQUOT_INIT(inode);
	} else {
		filp->f_flags = O_RDONLY|O_LARGEFILE;
		filp->f_mode  = FMODE_READ;
	}

	err = 0;
	if (filp->f_op && filp->f_op->open) {
		err = filp->f_op->open(inode, filp);
		if (err) {
			fops_put(filp->f_op);
			if (access & MAY_WRITE)
				put_write_access(inode);

			/* I nearly added put_filp() call here, but this filp
			 * is really on callers stack frame. -DaveM
			 */
			atomic_dec(&filp->f_count);
		}
	}
out_nfserr:
	if (err)
		err = nfserrno(err);
out:
	return err;
}

/*
 * Close a file.
 */
void
nfsd_close(struct file *filp)
{
	struct dentry	*dentry = filp->f_dentry;
	struct inode	*inode = dentry->d_inode;

	if (filp->f_op && filp->f_op->release)
		filp->f_op->release(inode, filp);
	fops_put(filp->f_op);
	if (filp->f_mode & FMODE_WRITE)
		put_write_access(inode);
}

/*
 * Sync a file
 * As this calls fsync (not fdatasync) there is no need for a write_inode
 * after it.
 */
inline void nfsd_dosync(struct file *filp, struct dentry *dp, 
			struct file_operations *fop)
{
	struct inode *inode = dp->d_inode;
	int (*fsync) (struct file *, struct dentry *, int);

	filemap_fdatasync(inode->i_mapping);
	if (fop && (fsync = fop->fsync))
		fsync(filp, dp, 0);
	filemap_fdatawait(inode->i_mapping);
}
	

void
nfsd_sync(struct file *filp)
{
	struct inode *inode = filp->f_dentry->d_inode;
	dprintk("nfsd: sync file %s\n", filp->f_dentry->d_name.name);
	down(&inode->i_sem);
	nfsd_dosync(filp, filp->f_dentry, filp->f_op);
	up(&inode->i_sem);
}

void
nfsd_sync_dir(struct dentry *dp)
{
	nfsd_dosync(NULL, dp, dp->d_inode->i_fop);
}

/*
 * Obtain the readahead parameters for the file
 * specified by (dev, ino).
 */
static inline struct raparms *
nfsd_get_raparms(dev_t dev, ino_t ino)
{
	struct raparms	*ra, **rap, **frap = NULL;
	int depth = 0;
	
	for (rap = &raparm_cache; (ra = *rap); rap = &ra->p_next) {
		if (ra->p_ino == ino && ra->p_dev == dev)
			goto found;
		depth++;
		if (ra->p_count == 0)
			frap = rap;
	}
	depth = nfsdstats.ra_size*11/10;
	if (!frap)
		return NULL;
	rap = frap;
	ra = *frap;
	ra->p_dev = dev;
	ra->p_ino = ino;
	ra->p_reada = 0;
	ra->p_ramax = 0;
	ra->p_raend = 0;
	ra->p_ralen = 0;
	ra->p_rawin = 0;
found:
	if (rap != &raparm_cache) {
		*rap = ra->p_next;
		ra->p_next   = raparm_cache;
		raparm_cache = ra;
	}
	ra->p_count++;
	nfsdstats.ra_depth[depth*10/nfsdstats.ra_size]++;
	return ra;
}

/*
 * Read data from a file. count must contain the requested read count
 * on entry. On return, *count contains the number of bytes actually read.
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_read(struct svc_rqst *rqstp, struct svc_fh *fhp, loff_t offset,
          char *buf, unsigned long *count)
{
	struct raparms	*ra;
	mm_segment_t	oldfs;
	int		err;
	struct file	file;

	err = nfsd_open(rqstp, fhp, S_IFREG, MAY_READ, &file);
	if (err)
		goto out;
	err = nfserr_perm;
	if (!file.f_op->read)
		goto out_close;
#ifdef MSNFS
	if ((fhp->fh_export->ex_flags & NFSEXP_MSNFS) &&
		(!lock_may_read(file.f_dentry->d_inode, offset, *count)))
		goto out_close;
#endif

	/* Get readahead parameters */
	ra = nfsd_get_raparms(fhp->fh_export->ex_dev, fhp->fh_dentry->d_inode->i_ino);
	if (ra) {
		file.f_reada = ra->p_reada;
		file.f_ramax = ra->p_ramax;
		file.f_raend = ra->p_raend;
		file.f_ralen = ra->p_ralen;
		file.f_rawin = ra->p_rawin;
	}
	file.f_pos = offset;

	oldfs = get_fs(); set_fs(KERNEL_DS);
	err = file.f_op->read(&file, buf, *count, &file.f_pos);
	set_fs(oldfs);

	/* Write back readahead params */
	if (ra != NULL) {
		dprintk("nfsd: raparms %ld %ld %ld %ld %ld\n",
			file.f_reada, file.f_ramax, file.f_raend,
			file.f_ralen, file.f_rawin);
		ra->p_reada = file.f_reada;
		ra->p_ramax = file.f_ramax;
		ra->p_raend = file.f_raend;
		ra->p_ralen = file.f_ralen;
		ra->p_rawin = file.f_rawin;
		ra->p_count -= 1;
	}

	if (err >= 0) {
		nfsdstats.io_read += err;
		*count = err;
		err = 0;
	} else 
		err = nfserrno(err);
out_close:
	nfsd_close(&file);
out:
	return err;
}

/*
 * Write data to a file.
 * The stable flag requests synchronous writes.
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_write(struct svc_rqst *rqstp, struct svc_fh *fhp, loff_t offset,
				char *buf, unsigned long cnt, int *stablep)
{
	struct svc_export	*exp;
	struct file		file;
	struct dentry		*dentry;
	struct inode		*inode;
	mm_segment_t		oldfs;
	int			err = 0;
	int			stable = *stablep;

	err = nfsd_open(rqstp, fhp, S_IFREG, MAY_WRITE, &file);
	if (err)
		goto out;
	if (!cnt)
		goto out_close;
	err = nfserr_perm;
	if (!file.f_op->write)
		goto out_close;
#ifdef MSNFS
	if ((fhp->fh_export->ex_flags & NFSEXP_MSNFS) &&
		(!lock_may_write(file.f_dentry->d_inode, offset, cnt)))
		goto out_close;
#endif

	dentry = file.f_dentry;
	inode = dentry->d_inode;
	exp   = fhp->fh_export;

	/*
	 * Request sync writes if
	 *  -	the sync export option has been set, or
	 *  -	the client requested O_SYNC behavior (NFSv3 feature).
	 *  -   The file system doesn't support fsync().
	 * When gathered writes have been configured for this volume,
	 * flushing the data to disk is handled separately below.
	 */

	if (file.f_op->fsync == 0) {/* COMMIT3 cannot work */
	       stable = 2;
	       *stablep = 2; /* FILE_SYNC */
	}

	if (!EX_ISSYNC(exp))
		stable = 0;
	if (stable && !EX_WGATHER(exp))
		file.f_flags |= O_SYNC;

	file.f_pos = offset;		/* set write offset */

	/* Write the data. */
	oldfs = get_fs(); set_fs(KERNEL_DS);
	err = file.f_op->write(&file, buf, cnt, &file.f_pos);
	if (err >= 0)
		nfsdstats.io_write += cnt;
	set_fs(oldfs);

	/* clear setuid/setgid flag after write */
	if (err >= 0 && (inode->i_mode & (S_ISUID | S_ISGID))) {
		struct iattr	ia;

		ia.ia_valid = ATTR_MODE;
		ia.ia_mode  = inode->i_mode & ~(S_ISUID | S_ISGID);
		notify_change(dentry, &ia);
	}

	if (err >= 0 && stable) {
		static unsigned long	last_ino;
		static kdev_t		last_dev = NODEV;

		/*
		 * Gathered writes: If another process is currently
		 * writing to the file, there's a high chance
		 * this is another nfsd (triggered by a bulk write
		 * from a client's biod). Rather than syncing the
		 * file with each write request, we sleep for 10 msec.
		 *
		 * I don't know if this roughly approximates
		 * C. Juszak's idea of gathered writes, but it's a
		 * nice and simple solution (IMHO), and it seems to
		 * work:-)
		 */
		if (EX_WGATHER(exp)) {
			if (atomic_read(&inode->i_writecount) > 1
			    || (last_ino == inode->i_ino && last_dev == inode->i_dev)) {
				dprintk("nfsd: write defer %d\n", current->pid);
				set_current_state(TASK_UNINTERRUPTIBLE);
				schedule_timeout((HZ+99)/100);
				current->state = TASK_RUNNING;
				dprintk("nfsd: write resume %d\n", current->pid);
			}

			if (inode->i_state & I_DIRTY) {
				dprintk("nfsd: write sync %d\n", current->pid);
				nfsd_sync(&file);
			}
#if 0
			wake_up(&inode->i_wait);
#endif
		}
		last_ino = inode->i_ino;
		last_dev = inode->i_dev;
	}

	dprintk("nfsd: write complete err=%d\n", err);
	if (err >= 0)
		err = 0;
	else 
		err = nfserrno(err);
out_close:
	nfsd_close(&file);
out:
	return err;
}


#ifdef CONFIG_NFSD_V3
/*
 * Commit all pending writes to stable storage.
 * Strictly speaking, we could sync just the indicated file region here,
 * but there's currently no way we can ask the VFS to do so.
 *
 * Unfortunately we cannot lock the file to make sure we return full WCC
 * data to the client, as locking happens lower down in the filesystem.
 */
int
nfsd_commit(struct svc_rqst *rqstp, struct svc_fh *fhp,
               off_t offset, unsigned long count)
{
	struct file	file;
	int		err;

	if ((err = nfsd_open(rqstp, fhp, S_IFREG, MAY_WRITE, &file)) != 0)
		return err;
	if (EX_ISSYNC(fhp->fh_export)) {
		if (file.f_op && file.f_op->fsync) {
			nfsd_sync(&file);
		} else {
			err = nfserr_notsupp;
		}
	}

	nfsd_close(&file);
	return err;
}
#endif /* CONFIG_NFSD_V3 */

/*
 * Create a file (regular, directory, device, fifo); UNIX sockets 
 * not yet implemented.
 * If the response fh has been verified, the parent directory should
 * already be locked. Note that the parent directory is left locked.
 *
 * N.B. Every call to nfsd_create needs an fh_put for _both_ fhp and resfhp
 */
int
nfsd_create(struct svc_rqst *rqstp, struct svc_fh *fhp,
		char *fname, int flen, struct iattr *iap,
		int type, dev_t rdev, struct svc_fh *resfhp)
{
	struct dentry	*dentry, *dchild;
	struct inode	*dirp;
	int		err;

	err = nfserr_perm;
	if (!flen)
		goto out;
	err = nfserr_exist;
	if (isdotent(fname, flen))
		goto out;

	err = fh_verify(rqstp, fhp, S_IFDIR, MAY_CREATE);
	if (err)
		goto out;

	dentry = fhp->fh_dentry;
	dirp = dentry->d_inode;

	err = nfserr_notdir;
	if(!dirp->i_op || !dirp->i_op->lookup)
		goto out;
	/*
	 * Check whether the response file handle has been verified yet.
	 * If it has, the parent directory should already be locked.
	 */
	if (!resfhp->fh_dentry) {
		/* called from nfsd_proc_mkdir, or possibly nfsd3_proc_create */
		fh_lock(fhp);
		dchild = lookup_one_len(fname, dentry, flen);
		err = PTR_ERR(dchild);
		if (IS_ERR(dchild))
			goto out_nfserr;
		err = fh_compose(resfhp, fhp->fh_export, dchild, fhp);
		if (err)
			goto out;
	} else {
		/* called from nfsd_proc_create */
		dchild = resfhp->fh_dentry;
		if (!fhp->fh_locked) {
			/* not actually possible */
			printk(KERN_ERR
				"nfsd_create: parent %s/%s not locked!\n",
				dentry->d_parent->d_name.name,
				dentry->d_name.name);
			err = -EIO;
			goto out;
		}
	}
	/*
	 * Make sure the child dentry is still negative ...
	 */
	err = nfserr_exist;
	if (dchild->d_inode) {
		dprintk("nfsd_create: dentry %s/%s not negative!\n",
			dentry->d_name.name, dchild->d_name.name);
		goto out; 
	}

	if (!(iap->ia_valid & ATTR_MODE))
		iap->ia_mode = 0;
	iap->ia_mode = (iap->ia_mode & S_IALLUGO) | type;

	/*
	 * Get the dir op function pointer.
	 */
	err = nfserr_perm;
	switch (type) {
	case S_IFREG:
		err = vfs_create(dirp, dchild, iap->ia_mode);
		break;
	case S_IFDIR:
		err = vfs_mkdir(dirp, dchild, iap->ia_mode);
		break;
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		err = vfs_mknod(dirp, dchild, iap->ia_mode, rdev);
		break;
	default:
	        printk("nfsd: bad file type %o in nfsd_create\n", type);
		err = -EINVAL;
	}
	if (err < 0)
		goto out_nfserr;

	if (EX_ISSYNC(fhp->fh_export)) {
		nfsd_sync_dir(dentry);
		write_inode_now(dchild->d_inode, 1);
	}


	/* Set file attributes. Mode has already been set and
	 * setting uid/gid works only for root. Irix appears to
	 * send along the gid when it tries to implement setgid
	 * directories via NFS.
	 */
	err = 0;
	if ((iap->ia_valid &= ~(ATTR_UID|ATTR_GID|ATTR_MODE)) != 0)
		err = nfsd_setattr(rqstp, resfhp, iap, 0, (time_t)0);
	/*
	 * Update the file handle to get the new inode info.
	 */
	if (!err)
		err = fh_update(resfhp);
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

#ifdef CONFIG_NFSD_V3
/*
 * NFSv3 version of nfsd_create
 */
int
nfsd_create_v3(struct svc_rqst *rqstp, struct svc_fh *fhp,
		char *fname, int flen, struct iattr *iap,
		struct svc_fh *resfhp, int createmode, u32 *verifier)
{
	struct dentry	*dentry, *dchild;
	struct inode	*dirp;
	int		err;
	__u32		v_mtime=0, v_atime=0;
	int		v_mode=0;

	err = nfserr_perm;
	if (!flen)
		goto out;
	err = nfserr_exist;
	if (isdotent(fname, flen))
		goto out;
	if (!(iap->ia_valid & ATTR_MODE))
		iap->ia_mode = 0;
	err = fh_verify(rqstp, fhp, S_IFDIR, MAY_CREATE);
	if (err)
		goto out;

	dentry = fhp->fh_dentry;
	dirp = dentry->d_inode;

	/* Get all the sanity checks out of the way before
	 * we lock the parent. */
	err = nfserr_notdir;
	if(!dirp->i_op || !dirp->i_op->lookup)
		goto out;
	fh_lock(fhp);

	/*
	 * Compose the response file handle.
	 */
	dchild = lookup_one_len(fname, dentry, flen);
	err = PTR_ERR(dchild);
	if (IS_ERR(dchild))
		goto out_nfserr;

	err = fh_compose(resfhp, fhp->fh_export, dchild, fhp);
	if (err)
		goto out;

	if (createmode == NFS3_CREATE_EXCLUSIVE) {
		/* while the verifier would fit in mtime+atime,
		 * solaris7 gets confused (bugid 4218508) if these have
		 * the high bit set, so we use the mode as well
		 */
		v_mtime = verifier[0]&0x7fffffff;
		v_atime = verifier[1]&0x7fffffff;
		v_mode  = S_IFREG
			| ((verifier[0]&0x80000000) >> (32-7)) /* u+x */
			| ((verifier[1]&0x80000000) >> (32-9)) /* u+r */
			;
	}
	
	if (dchild->d_inode) {
		err = 0;

		switch (createmode) {
		case NFS3_CREATE_UNCHECKED:
			if (! S_ISREG(dchild->d_inode->i_mode))
				err = nfserr_exist;
			else {
				iap->ia_valid &= ATTR_SIZE;
				goto set_attr;
			}
			break;
		case NFS3_CREATE_EXCLUSIVE:
			if (   dchild->d_inode->i_mtime == v_mtime
			    && dchild->d_inode->i_atime == v_atime
			    && dchild->d_inode->i_mode  == v_mode
			    && dchild->d_inode->i_size  == 0 )
				break;
			 /* fallthru */
		case NFS3_CREATE_GUARDED:
			err = nfserr_exist;
		}
		goto out;
	}

	err = vfs_create(dirp, dchild, iap->ia_mode);
	if (err < 0)
		goto out_nfserr;

	if (EX_ISSYNC(fhp->fh_export)) {
		nfsd_sync_dir(dentry);
		/* setattr will sync the child (or not) */
	}

	/*
	 * Update the filehandle to get the new inode info.
	 */
	err = fh_update(resfhp);
	if (err)
		goto out;

	if (createmode == NFS3_CREATE_EXCLUSIVE) {
		/* Cram the verifier into atime/mtime/mode */
		iap->ia_valid = ATTR_MTIME|ATTR_ATIME
			| ATTR_MTIME_SET|ATTR_ATIME_SET
			| ATTR_MODE;
		iap->ia_mtime = v_mtime;
		iap->ia_atime = v_atime;
		iap->ia_mode  = v_mode;
	}

	/* Set file attributes.
	 * Mode has already been set but we might need to reset it
	 * for CREATE_EXCLUSIVE
	 * Irix appears to send along the gid when it tries to
	 * implement setgid directories via NFS. Clear out all that cruft.
	 */
 set_attr:
	if ((iap->ia_valid &= ~(ATTR_UID|ATTR_GID)) != 0)
 		err = nfsd_setattr(rqstp, resfhp, iap, 0, (time_t)0);

 out:
	fh_unlock(fhp);
 	return err;
 
 out_nfserr:
	err = nfserrno(err);
	goto out;
}
#endif /* CONFIG_NFSD_V3 */

/*
 * Read a symlink. On entry, *lenp must contain the maximum path length that
 * fits into the buffer. On return, it contains the true length.
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_readlink(struct svc_rqst *rqstp, struct svc_fh *fhp, char *buf, int *lenp)
{
	struct dentry	*dentry;
	struct inode	*inode;
	mm_segment_t	oldfs;
	int		err;

	err = fh_verify(rqstp, fhp, S_IFLNK, MAY_NOP);
	if (err)
		goto out;

	dentry = fhp->fh_dentry;
	inode = dentry->d_inode;

	err = nfserr_inval;
	if (!inode->i_op || !inode->i_op->readlink)
		goto out;

	UPDATE_ATIME(inode);
	/* N.B. Why does this call need a get_fs()??
	 * Remove the set_fs and watch the fireworks:-) --okir
	 */

	oldfs = get_fs(); set_fs(KERNEL_DS);
	err = inode->i_op->readlink(dentry, buf, *lenp);
	set_fs(oldfs);

	if (err < 0)
		goto out_nfserr;
	*lenp = err;
	err = 0;
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

/*
 * Create a symlink and look up its inode
 * N.B. After this call _both_ fhp and resfhp need an fh_put
 */
int
nfsd_symlink(struct svc_rqst *rqstp, struct svc_fh *fhp,
				char *fname, int flen,
				char *path,  int plen,
				struct svc_fh *resfhp,
				struct iattr *iap)
{
	struct dentry	*dentry, *dnew;
	int		err, cerr;

	err = nfserr_noent;
	if (!flen || !plen)
		goto out;
	err = nfserr_exist;
	if (isdotent(fname, flen))
		goto out;

	err = fh_verify(rqstp, fhp, S_IFDIR, MAY_CREATE);
	if (err)
		goto out;
	fh_lock(fhp);
	dentry = fhp->fh_dentry;
	dnew = lookup_one_len(fname, dentry, flen);
	err = PTR_ERR(dnew);
	if (IS_ERR(dnew))
		goto out_nfserr;

	err = vfs_symlink(dentry->d_inode, dnew, path);
	if (!err) {
		if (EX_ISSYNC(fhp->fh_export))
			nfsd_sync_dir(dentry);
		if (iap) {
			iap->ia_valid &= ATTR_MODE /* ~(ATTR_MODE|ATTR_UID|ATTR_GID)*/;
			if (iap->ia_valid) {
				iap->ia_valid |= ATTR_CTIME;
				iap->ia_mode = (iap->ia_mode&S_IALLUGO)
					| S_IFLNK;
				err = notify_change(dnew, iap);
				if (err)
					err = nfserrno(err);
				else if (EX_ISSYNC(fhp->fh_export))
					write_inode_now(dentry->d_inode, 1);
		       }
		}
	} else
		err = nfserrno(err);
	fh_unlock(fhp);

	/* Compose the fh so the dentry will be freed ... */
	cerr = fh_compose(resfhp, fhp->fh_export, dnew, fhp);
	if (err==0) err = cerr;
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

/*
 * Create a hardlink
 * N.B. After this call _both_ ffhp and tfhp need an fh_put
 */
int
nfsd_link(struct svc_rqst *rqstp, struct svc_fh *ffhp,
				char *name, int len, struct svc_fh *tfhp)
{
	struct dentry	*ddir, *dnew, *dold;
	struct inode	*dirp, *dest;
	int		err;

	err = fh_verify(rqstp, ffhp, S_IFDIR, MAY_CREATE);
	if (err)
		goto out;
	err = fh_verify(rqstp, tfhp, -S_IFDIR, MAY_NOP);
	if (err)
		goto out;

	err = nfserr_perm;
	if (!len)
		goto out;
	err = nfserr_exist;
	if (isdotent(name, len))
		goto out;

	fh_lock(ffhp);
	ddir = ffhp->fh_dentry;
	dirp = ddir->d_inode;

	dnew = lookup_one_len(name, ddir, len);
	err = PTR_ERR(dnew);
	if (IS_ERR(dnew))
		goto out_nfserr;

	dold = tfhp->fh_dentry;
	dest = dold->d_inode;

	err = vfs_link(dold, dirp, dnew);
	if (!err) {
		if (EX_ISSYNC(ffhp->fh_export)) {
			nfsd_sync_dir(ddir);
			write_inode_now(dest, 1);
		}
	} else {
		if (err == -EXDEV && rqstp->rq_vers == 2)
			err = nfserr_acces;
		else
			err = nfserrno(err);
	}

	fh_unlock(ffhp);
	dput(dnew);
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

/*
 * Rename a file
 * N.B. After this call _both_ ffhp and tfhp need an fh_put
 */
int
nfsd_rename(struct svc_rqst *rqstp, struct svc_fh *ffhp, char *fname, int flen,
			    struct svc_fh *tfhp, char *tname, int tlen)
{
	struct dentry	*fdentry, *tdentry, *odentry, *ndentry;
	struct inode	*fdir, *tdir;
	int		err;

	err = fh_verify(rqstp, ffhp, S_IFDIR, MAY_REMOVE);
	if (err)
		goto out;
	err = fh_verify(rqstp, tfhp, S_IFDIR, MAY_CREATE);
	if (err)
		goto out;

	fdentry = ffhp->fh_dentry;
	fdir = fdentry->d_inode;

	tdentry = tfhp->fh_dentry;
	tdir = tdentry->d_inode;

	err = (rqstp->rq_vers == 2) ? nfserr_acces : nfserr_xdev;
	if (fdir->i_dev != tdir->i_dev)
		goto out;

	err = nfserr_perm;
	if (!flen || isdotent(fname, flen) || !tlen || isdotent(tname, tlen))
		goto out;

	/* cannot use fh_lock as we need deadlock protective ordering
	 * so do it by hand */
	double_down(&tdir->i_sem, &fdir->i_sem);
	ffhp->fh_locked = tfhp->fh_locked = 1;
	fill_pre_wcc(ffhp);
	fill_pre_wcc(tfhp);

	odentry = lookup_one_len(fname, fdentry, flen);
	err = PTR_ERR(odentry);
	if (IS_ERR(odentry))
		goto out_nfserr;

	err = -ENOENT;
	if (!odentry->d_inode)
		goto out_dput_old;

	ndentry = lookup_one_len(tname, tdentry, tlen);
	err = PTR_ERR(ndentry);
	if (IS_ERR(ndentry))
		goto out_dput_old;


#ifdef MSNFS
	if ((ffhp->fh_export->ex_flags & NFSEXP_MSNFS) &&
		((atomic_read(&odentry->d_count) > 1)
		 || (atomic_read(&ndentry->d_count) > 1))) {
			err = nfserr_perm;
	} else
#endif
	err = vfs_rename(fdir, odentry, tdir, ndentry);
	if (!err && EX_ISSYNC(tfhp->fh_export)) {
		nfsd_sync_dir(tdentry);
		nfsd_sync_dir(fdentry);
	}
	dput(ndentry);

 out_dput_old:
	dput(odentry);
 out_nfserr:
	if (err)
		err = nfserrno(err);

	/* we cannot reply on fh_unlock on the two filehandles,
	 * as that would do the wrong thing if the two directories
	 * were the same, so again we do it by hand
	 */
	fill_post_wcc(ffhp);
	fill_post_wcc(tfhp);
	double_up(&tdir->i_sem, &fdir->i_sem);
	ffhp->fh_locked = tfhp->fh_locked = 0;
	
out:
	return err;
}

/*
 * Unlink a file or directory
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_unlink(struct svc_rqst *rqstp, struct svc_fh *fhp, int type,
				char *fname, int flen)
{
	struct dentry	*dentry, *rdentry;
	struct inode	*dirp;
	int		err;

	err = nfserr_acces;
	if (!flen || isdotent(fname, flen))
		goto out;
	err = fh_verify(rqstp, fhp, S_IFDIR, MAY_REMOVE);
	if (err)
		goto out;

	fh_lock(fhp);
	dentry = fhp->fh_dentry;
	dirp = dentry->d_inode;

	rdentry = lookup_one_len(fname, dentry, flen);
	err = PTR_ERR(rdentry);
	if (IS_ERR(rdentry))
		goto out_nfserr;

	if (!rdentry->d_inode) {
		dput(rdentry);
		err = nfserr_noent;
		goto out;
	}

	if (type != S_IFDIR) { /* It's UNLINK */
#ifdef MSNFS
		if ((fhp->fh_export->ex_flags & NFSEXP_MSNFS) &&
			(atomic_read(&rdentry->d_count) > 1)) {
			err = nfserr_perm;
		} else
#endif
		err = vfs_unlink(dirp, rdentry);
	} else { /* It's RMDIR */
		err = vfs_rmdir(dirp, rdentry);
	}

	dput(rdentry);

	if (err)
		goto out_nfserr;
	if (EX_ISSYNC(fhp->fh_export)) 
		nfsd_sync_dir(dentry);

out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out;
}

/*
 * Read entries from a directory.
 * The verifier is an NFSv3 thing we ignore for now.
 */
int
nfsd_readdir(struct svc_rqst *rqstp, struct svc_fh *fhp, loff_t offset, 
             encode_dent_fn func, u32 *buffer, int *countp, u32 *verf)
{
	u32		*p;
	int		oldlen, eof, err;
	struct file	file;
	struct readdir_cd cd;

	err = nfsd_open(rqstp, fhp, S_IFDIR, MAY_READ, &file);
	if (err)
		goto out;
	if (offset > ~(u32) 0)
		goto out_close;

	file.f_pos = offset;

	/* Set up the readdir context */
	memset(&cd, 0, sizeof(cd));
	cd.rqstp  = rqstp;
	cd.buffer = buffer;
	cd.buflen = *countp; /* count of words */
	cd.dirfh  = fhp;

	/*
	 * Read the directory entries. This silly loop is necessary because
	 * readdir() is not guaranteed to fill up the entire buffer, but
	 * may choose to do less.
	 */

	do {
		oldlen = cd.buflen;

		err = vfs_readdir(&file, (filldir_t) func, &cd);

		if (err < 0)
			goto out_nfserr;

	} while (oldlen != cd.buflen && !cd.eob);

	/* If we didn't fill the buffer completely, we're at EOF */
	eof = !cd.eob;

	if (cd.offset) {
		if (rqstp->rq_vers == 3)
			(void)xdr_encode_hyper(cd.offset, file.f_pos);
		else
			*cd.offset = htonl(file.f_pos);
	}

	p = cd.buffer;
	*p++ = 0;			/* no more entries */
	*p++ = htonl(eof);		/* end of directory */
	*countp = (caddr_t) p - (caddr_t) buffer;

	dprintk("nfsd: readdir result %d bytes, eof %d offset %d\n",
				*countp, eof,
				cd.offset? ntohl(*cd.offset) : -1);
	err = 0;
out_close:
	nfsd_close(&file);
out:
	return err;

out_nfserr:
	err = nfserrno(err);
	goto out_close;
}

/*
 * Get file system stats
 * N.B. After this call fhp needs an fh_put
 */
int
nfsd_statfs(struct svc_rqst *rqstp, struct svc_fh *fhp, struct statfs *stat)
{
	int err = fh_verify(rqstp, fhp, 0, MAY_NOP);
	if (!err && vfs_statfs(fhp->fh_dentry->d_inode->i_sb,stat))
		err = nfserr_io;
	return err;
}

/*
 * Check for a user's access permissions to this inode.
 */
int
nfsd_permission(struct svc_export *exp, struct dentry *dentry, int acc)
{
	struct inode	*inode = dentry->d_inode;
	int		err;

	if (acc == MAY_NOP)
		return 0;
#if 0
	dprintk("nfsd: permission 0x%x%s%s%s%s%s%s%s mode 0%o%s%s%s\n",
		acc,
		(acc & MAY_READ)?	" read"  : "",
		(acc & MAY_WRITE)?	" write" : "",
		(acc & MAY_EXEC)?	" exec"  : "",
		(acc & MAY_SATTR)?	" sattr" : "",
		(acc & MAY_TRUNC)?	" trunc" : "",
		(acc & MAY_LOCK)?	" lock"  : "",
		(acc & MAY_OWNER_OVERRIDE)? " owneroverride" : "",
		inode->i_mode,
		IS_IMMUTABLE(inode)?	" immut" : "",
		IS_APPEND(inode)?	" append" : "",
		IS_RDONLY(inode)?	" ro" : "");
	dprintk("      owner %d/%d user %d/%d\n",
		inode->i_uid, inode->i_gid, current->fsuid, current->fsgid);
#endif

	/* The following code is here to make IRIX happy, which
	 * does a permission check every time a user does
	 *	echo yaddayadda > special-file
	 * by sending a CREATE request.
	 * The original code would check read-only export status
	 * only for regular files and directories, allowing
	 * clients to chown/chmod device files and fifos even
	 * on volumes exported read-only. */
	if (!(acc & _NFSD_IRIX_BOGOSITY)
	 && (acc & (MAY_WRITE | MAY_SATTR | MAY_TRUNC))) {
		if (EX_RDONLY(exp) || IS_RDONLY(inode))
			return nfserr_rofs;
		if (/* (acc & MAY_WRITE) && */ IS_IMMUTABLE(inode))
			return nfserr_perm;
	}
	if ((acc & MAY_TRUNC) && IS_APPEND(inode))
		return nfserr_perm;

	if (acc & MAY_LOCK) {
		/* If we cannot rely on authentication in NLM requests,
		 * just allow locks, otherwise require read permission, or
		 * ownership
		 */
		if (exp->ex_flags & NFSEXP_NOAUTHNLM)
			return 0;
		else
			acc = MAY_READ | MAY_OWNER_OVERRIDE;
	}
	/*
	 * The file owner always gets access permission for accesses that
	 * would normally be checked at open time. This is to make
	 * file access work even when the client has done a fchmod(fd, 0).
	 *
	 * However, `cp foo bar' should fail nevertheless when bar is
	 * readonly. A sensible way to do this might be to reject all
	 * attempts to truncate a read-only file, because a creat() call
	 * always implies file truncation.
	 * ... but this isn't really fair.  A process may reasonably call
	 * ftruncate on an open file descriptor on a file with perm 000.
	 * We must trust the client to do permission checking - using "ACCESS"
	 * with NFSv3.
	 */
	if ((acc & MAY_OWNER_OVERRIDE) &&
	    inode->i_uid == current->fsuid)
		return 0;

	err = permission(inode, acc & (MAY_READ|MAY_WRITE|MAY_EXEC));

	/* Allow read access to binaries even when mode 111 */
	if (err == -EACCES && S_ISREG(inode->i_mode) &&
	    acc == (MAY_READ | MAY_OWNER_OVERRIDE))
		err = permission(inode, MAY_EXEC);

	return err? nfserrno(err) : 0;
}

void
nfsd_racache_shutdown(void)
{
	if (!raparm_cache)
		return;
	dprintk("nfsd: freeing readahead buffers.\n");
	kfree(raparml);
	raparm_cache = raparml = NULL;
}
/*
 * Initialize readahead param cache
 */
int
nfsd_racache_init(int cache_size)
{
	int	i;

	if (raparm_cache)
		return 0;
	raparml = kmalloc(sizeof(struct raparms) * cache_size, GFP_KERNEL);

	if (raparml != NULL) {
		dprintk("nfsd: allocating %d readahead buffers.\n",
			cache_size);
		memset(raparml, 0, sizeof(struct raparms) * cache_size);
		for (i = 0; i < cache_size - 1; i++) {
			raparml[i].p_next = raparml + i + 1;
		}
		raparm_cache = raparml;
	} else {
		printk(KERN_WARNING
		       "nfsd: Could not allocate memory read-ahead cache.\n");
		return -ENOMEM;
	}
	nfsdstats.ra_size = cache_size;
	return 0;
}
