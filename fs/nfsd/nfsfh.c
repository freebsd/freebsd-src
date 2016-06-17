/*
 * linux/fs/nfsd/nfsfh.c
 *
 * NFS server file handle treatment.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 * Portions Copyright (C) 1999 G. Allen Morris III <gam3@acm.org>
 * Extensive rewrite by Neil Brown <neilb@cse.unsw.edu.au> Southern-Spring 1999
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/dcache.h>
#include <asm/pgtable.h>

#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>

#define NFSDDBG_FACILITY		NFSDDBG_FH
#define NFSD_PARANOIA 1
/* #define NFSD_DEBUG_VERBOSE 1 */


static int nfsd_nr_verified;
static int nfsd_nr_put;


struct nfsd_getdents_callback {
	char *name;		/* name that was found. It already points to a buffer NAME_MAX+1 is size */
	unsigned long ino;	/* the inum we are looking for */
	int found;		/* inode matched? */
	int sequence;		/* sequence counter */
};

/*
 * A rather strange filldir function to capture
 * the name matching the specified inode number.
 */
static int filldir_one(void * __buf, const char * name, int len,
			loff_t pos, ino_t ino, unsigned int d_type)
{
	struct nfsd_getdents_callback *buf = __buf;
	int result = 0;

	buf->sequence++;
#ifdef NFSD_DEBUG_VERBOSE
dprintk("filldir_one: seq=%d, ino=%ld, name=%s\n", buf->sequence, ino, name);
#endif
	if (buf->ino == ino) {
		memcpy(buf->name, name, len);
		buf->name[len] = '\0';
		buf->found = 1;
		result = -1;
	}
	return result;
}

/**
 * nfsd_get_name - default nfsd_operations->get_name function
 * @dentry: the directory in which to find a name
 * @name:   a pointer to a %NAME_MAX+1 char buffer to store the name
 * @child:  the dentry for the child directory.
 *
 * calls readdir on the parent until it finds an entry with
 * the same inode number as the child, and returns that.
 */
static int nfsd_get_name(struct dentry *dentry, char *name,
			struct dentry *child)
{
	struct inode *dir = dentry->d_inode;
	int error;
	struct file file;
	struct nfsd_getdents_callback buffer;

	error = -ENOTDIR;
	if (!dir || !S_ISDIR(dir->i_mode))
		goto out;
	error = -EINVAL;
	if (!dir->i_fop)
		goto out;
	/*
	 * Open the directory ...
	 */
	error = init_private_file(&file, dentry, FMODE_READ);
	if (error)
		goto out;
	error = -EINVAL;
	if (!file.f_op->readdir)
		goto out_close;

	buffer.name = name;
	buffer.ino = child->d_inode->i_ino;
	buffer.found = 0;
	buffer.sequence = 0;
	while (1) {
		int old_seq = buffer.sequence;

		error = vfs_readdir(&file, filldir_one, &buffer);

		if (error < 0)
			break;

		error = 0;
		if (buffer.found)
			break;
		error = -ENOENT;
		if (old_seq == buffer.sequence)
			break;
	}

out_close:
	if (file.f_op->release)
		file.f_op->release(dir, &file);
out:
	return error;
}

/* this should be provided by each filesystem in an nfsd_operations interface as
 * iget isn't really the right interface
 */
static struct dentry *nfsd_iget(struct super_block *sb, unsigned long ino, __u32 generation)
{

	/* iget isn't really right if the inode is currently unallocated!!
	 * This should really all be done inside each filesystem
	 *
	 * ext2fs' read_inode has been strengthed to return a bad_inode if the inode
	 *   had been deleted.
	 *
	 * Currently we don't know the generation for parent directory, so a generation
	 * of 0 means "accept any"
	 */
	struct inode *inode;
	struct list_head *lp;
	struct dentry *result;
	if (ino == 0)
		return ERR_PTR(-ESTALE);
	inode = iget(sb, ino);
	if (inode == NULL)
		return ERR_PTR(-ENOMEM);
	if (is_bad_inode(inode)
	    || (generation && inode->i_generation != generation)
		) {
		/* we didn't find the right inode.. */
		dprintk("fh_verify: Inode %lu, Bad count: %d %d or version  %u %u\n",
			inode->i_ino,
			inode->i_nlink, atomic_read(&inode->i_count),
			inode->i_generation,
			generation);

		iput(inode);
		return ERR_PTR(-ESTALE);
	}
	/* now to find a dentry.
	 * If possible, get a well-connected one
	 */
	spin_lock(&dcache_lock);
	for (lp = inode->i_dentry.next; lp != &inode->i_dentry ; lp=lp->next) {
		result = list_entry(lp,struct dentry, d_alias);
		if (! (result->d_flags & DCACHE_NFSD_DISCONNECTED)) {
			dget_locked(result);
			result->d_vfs_flags |= DCACHE_REFERENCED;
			spin_unlock(&dcache_lock);
			iput(inode);
			return result;
		}
	}
	spin_unlock(&dcache_lock);
	result = d_alloc_root(inode);
	if (result == NULL) {
		iput(inode);
		return ERR_PTR(-ENOMEM);
	}
	result->d_flags |= DCACHE_NFSD_DISCONNECTED;
	return result;
}

static struct dentry *nfsd_get_dentry(struct super_block *sb, __u32 *fh,
					     int len, int fhtype, int parent)
{
	if (sb->s_op->fh_to_dentry)
		return sb->s_op->fh_to_dentry(sb, fh, len, fhtype, parent);
	switch (fhtype) {
	case 1:
		if (len < 2)
			break;
		if (parent)
			break;
		return nfsd_iget(sb, fh[0], fh[1]);

	case 2:
		if (len < 3)
			break;
		if (parent)
			return nfsd_iget(sb,fh[2],0);
		return nfsd_iget(sb,fh[0],fh[1]);
	default: break;
	}
	return ERR_PTR(-EINVAL);
}


/* this routine links an IS_ROOT dentry into the dcache tree.  It gains "parent"
 * as a parent and "name" as a name
 * It should possibly go in dcache.c
 */
int d_splice(struct dentry *target, struct dentry *parent, struct qstr *name)
{
	struct dentry *tdentry;
#ifdef NFSD_PARANOIA
	if (!IS_ROOT(target))
		printk("nfsd: d_splice with no-root target: %s/%s\n", parent->d_name.name, name->name);
	if (!(target->d_flags & DCACHE_NFSD_DISCONNECTED))
		printk("nfsd: d_splice with non-DISCONNECTED target: %s/%s\n", parent->d_name.name, name->name);
#endif
	tdentry = d_alloc(parent, name);
	if (tdentry == NULL)
		return -ENOMEM;
	d_move(target, tdentry);

	/* tdentry will have been made a "child" of target (the parent of target)
	 * make it an IS_ROOT instead
	 */
	spin_lock(&dcache_lock);
	list_del_init(&tdentry->d_child);
	tdentry->d_parent = tdentry;
	spin_unlock(&dcache_lock);
	d_rehash(target);
	dput(tdentry);

	/* if parent is properly connected, then we can assert that
	 * the children are connected, but it must be a singluar (non-forking)
	 * branch
	 */
	if (!(parent->d_flags & DCACHE_NFSD_DISCONNECTED)) {
		while (target) {
			target->d_flags &= ~DCACHE_NFSD_DISCONNECTED;
			parent = target;
			spin_lock(&dcache_lock);
			if (list_empty(&parent->d_subdirs))
				target = NULL;
			else {
				target = list_entry(parent->d_subdirs.next, struct dentry, d_child);
#ifdef NFSD_PARANOIA
				/* must be only child */
				if (target->d_child.next != &parent->d_subdirs
				    || target->d_child.prev != &parent->d_subdirs)
					printk("nfsd: d_splice found non-singular disconnected branch: %s/%s\n",
					       parent->d_name.name, target->d_name.name);
#endif
			}
			spin_unlock(&dcache_lock);
		}
	}
	return 0;
}

/* this routine finds the dentry of the parent of a given directory
 * it should be in the filesystem accessed by nfsd_operations
 * it assumes lookup("..") works.
 */
struct dentry *nfsd_findparent(struct dentry *child)
{
	struct dentry *tdentry, *pdentry;
	tdentry = d_alloc(child, &(const struct qstr) {"..", 2, 0});
	if (!tdentry)
		return ERR_PTR(-ENOMEM);

	/* I'm going to assume that if the returned dentry is different, then
	 * it is well connected.  But nobody returns different dentrys do they?
	 */
	down(&child->d_inode->i_sem);
	pdentry = child->d_inode->i_op->lookup(child->d_inode, tdentry);
	up(&child->d_inode->i_sem);
	d_drop(tdentry); /* we never want ".." hashed */
	if (!pdentry && tdentry->d_inode == NULL) {
		/* File system cannot find ".." ... sad but possible */
		pdentry = ERR_PTR(-EINVAL);
	}
	if (!pdentry) {
		/* I don't want to return a ".." dentry.
		 * I would prefer to return an unconnected "IS_ROOT" dentry,
		 * though a properly connected dentry is even better
		 */
		/* if first or last of alias list is not tdentry, use that
		 * else make a root dentry
		 */
		struct list_head *aliases = &tdentry->d_inode->i_dentry;
		spin_lock(&dcache_lock);
		if (aliases->next != aliases) {
			pdentry = list_entry(aliases->next, struct dentry, d_alias);
			if (pdentry == tdentry)
				pdentry = list_entry(aliases->prev, struct dentry, d_alias);
			if (pdentry == tdentry)
				pdentry = NULL;
			if (pdentry) dget_locked(pdentry);
		}
		spin_unlock(&dcache_lock);
		if (pdentry == NULL) {
			pdentry = d_alloc_root(tdentry->d_inode);
			if (pdentry) {
				igrab(tdentry->d_inode);
				pdentry->d_flags |= DCACHE_NFSD_DISCONNECTED;
				pdentry->d_op = child->d_op;
			}
		}
		if (pdentry == NULL)
			pdentry = ERR_PTR(-ENOMEM);
	}
	dput(tdentry); /* it is not hashed, it will be discarded */
	return pdentry;
}

static struct dentry *splice(struct dentry *child, struct dentry *parent)
{
	int err = 0, nerr;
	struct qstr qs;
	char namebuf[256];
	struct list_head *lp;
	/* child is an IS_ROOT (anonymous) dentry, but it is hypothesised that
	 * it should be a child of parent.
	 * We see if we can find a name and, if we can - splice it in.
	 * We lookup the name before locking (i_sem) the directory as namelookup
	 * also claims i_sem.  If the name gets changed then we will loop around
	 * and try again in find_fh_dentry.
	 */

	nerr = nfsd_get_name(parent, namebuf, child);

	/*
	 * We now claim the parent i_sem so that no-one else tries to create
	 * a dentry in the parent while we are.
	 */
	
	down(&parent->d_inode->i_sem);

	/* Now, things might have changed while we waited.
	 * Possibly a friendly filesystem found child and spliced it in in response
	 * to a lookup (though nobody does this yet).  In this case, just succeed.
	 */
	if (child->d_parent == parent) goto out;
	
	/* Possibly a new dentry has been made for this child->d_inode in
	 * parent by a lookup.  In this case return that dentry. Caller must
	 * notice and act accordingly
	 */
	spin_lock(&dcache_lock);
	list_for_each(lp, &child->d_inode->i_dentry) {
		struct dentry *tmp = list_entry(lp,struct dentry, d_alias);
		if (!list_empty(&tmp->d_hash) &&
		    tmp->d_parent == parent) {
			child = dget_locked(tmp);
			spin_unlock(&dcache_lock);
			goto out;
		}
	}
	spin_unlock(&dcache_lock);

	/* now we need that name.  If there was an error getting it, now is th
	 * time to bail out.
	 */
	if ((err = nerr))
		goto out;
	qs.name = namebuf;
	qs.len = strlen(namebuf);
	if (find_inode_number(parent, &qs) != 0) {
		/* Now that IS odd.  I wonder what it means... */
		err = -EEXIST;
		printk("nfsd-fh: found a name that I didn't expect: %s/%s\n", parent->d_name.name, qs.name);
		goto out;
	}
	err = d_splice(child, parent, &qs);
	dprintk("nfsd_fh: found name %s for ino %ld\n", child->d_name.name, child->d_inode->i_ino);
 out:
	up(&parent->d_inode->i_sem);
	if (err)
		return ERR_PTR(err);
	else
		return child;
}

/*
 * This is the basic lookup mechanism for turning an NFS file handle
 * into a dentry.
 * We use nfsd_iget and if that doesn't return a suitably connected dentry,
 * we try to find the parent, and the parent of that and so-on until a
 * connection if made.
 */
static struct dentry *
find_fh_dentry(struct super_block *sb, __u32 *datap, int len, int fhtype, int needpath)
{
	struct dentry *dentry, *result = NULL;
	struct dentry *tmp;
	int err = -ESTALE;
	/* the sb->s_nfsd_free_path_sem semaphore is needed to make sure that only one unconnected (free)
	 * dcache path ever exists, as otherwise two partial paths might get
	 * joined together, which would be very confusing.
	 * If there is ever an unconnected non-root directory, then this lock
	 * must be held.
	 */


	nfsdstats.fh_lookup++;
	/*
	 * Attempt to find the inode.
	 */
 retry:
	down(&sb->s_nfsd_free_path_sem);
	result = nfsd_get_dentry(sb, datap, len, fhtype, 0);
	if (IS_ERR(result)
	    || !(result->d_flags & DCACHE_NFSD_DISCONNECTED)
	    || (!S_ISDIR(result->d_inode->i_mode) && ! needpath)) {
		up(&sb->s_nfsd_free_path_sem);
	    
		err = PTR_ERR(result);
		if (IS_ERR(result))
			goto err_out;
		if ((result->d_flags & DCACHE_NFSD_DISCONNECTED))
			nfsdstats.fh_anon++;
		return result;
	}

	/* It's a directory, or we are required to confirm the file's
	 * location in the tree.
	 */
	dprintk("nfs_fh: need to look harder for %d/%d\n",sb->s_dev,datap[0]);

	if (!S_ISDIR(result->d_inode->i_mode)) {
		nfsdstats.fh_nocache_nondir++;
			/* need to iget dirino and make sure this inode is in that directory */
			dentry = nfsd_get_dentry(sb, datap, len, fhtype, 1);
			err = PTR_ERR(dentry);
			if (IS_ERR(dentry))
				goto err_result;
			err = -ESTALE;
			if (!dentry->d_inode
			    || !S_ISDIR(dentry->d_inode->i_mode)) {
				goto err_dentry;
			}
			tmp = splice(result, dentry);
			err = PTR_ERR(tmp);
			if (IS_ERR(tmp))
				goto err_dentry;
			if (tmp != result) {
				/* it is safe to just use tmp instead, but we must discard result first */
				d_drop(result);
				dput(result);
				result = tmp;
			}
	} else {
		nfsdstats.fh_nocache_dir++;
		dentry = dget(result);
	}

	while(dentry->d_flags & DCACHE_NFSD_DISCONNECTED) {
		/* LOOP INVARIANT */
		/* haven't found a place in the tree yet, but we do have a free path
		 * from dentry down to result, and dentry is a directory.
		 * Have a hold on dentry and result */
		struct dentry *pdentry;
		struct inode *parent;

		pdentry = nfsd_findparent(dentry);
		err = PTR_ERR(pdentry);
		if (IS_ERR(pdentry))
			goto err_dentry;
		parent = pdentry->d_inode;
		err = -EACCES;
		if (!parent) {
			dput(pdentry);
			goto err_dentry;
		}

		tmp = splice(dentry, pdentry);
		if (tmp != dentry) {
			/* Something wrong.  We need to drop the whole dentry->result path
			 * whatever it was
			 */
			struct dentry *d;
			for (d=result ; d ; d=(d->d_parent == d)?NULL:d->d_parent)
				d_drop(d);
		}
		if (IS_ERR(tmp)) {
			err = PTR_ERR(tmp);
			dput(pdentry);
			goto err_dentry;
		}
		if (tmp != dentry) {
			/* we lost a race,  try again
			 */
			dput(pdentry);
			dput(tmp);
			dput(dentry);
			dput(result);	/* this will discard the whole free path, so we can up the semaphore */
			up(&sb->s_nfsd_free_path_sem);
			goto retry;
		}
		dput(dentry);
		dentry = pdentry;
	}
	dput(dentry);
	up(&sb->s_nfsd_free_path_sem);
	return result;

err_dentry:
	dput(dentry);
err_result:
	dput(result);
	up(&sb->s_nfsd_free_path_sem);
err_out:
	if (err == -ESTALE)
		nfsdstats.fh_stale++;
	return ERR_PTR(err);
}

/*
 * Perform sanity checks on the dentry in a client's file handle.
 *
 * Note that the file handle dentry may need to be freed even after
 * an error return.
 *
 * This is only called at the start of an nfsproc call, so fhp points to
 * a svc_fh which is all 0 except for the over-the-wire file handle.
 */
u32
fh_verify(struct svc_rqst *rqstp, struct svc_fh *fhp, int type, int access)
{
	struct knfsd_fh	*fh = &fhp->fh_handle;
	struct svc_export *exp;
	struct dentry	*dentry;
	struct inode	*inode;
	u32		error = 0;

	dprintk("nfsd: fh_verify(%s)\n", SVCFH_fmt(fhp));

	/* keep this filehandle for possible reference  when encoding attributes */
	rqstp->rq_reffh = fh;

	if (!fhp->fh_dentry) {
		kdev_t xdev = NODEV;
		ino_t xino = 0;
		__u32 *datap=NULL;
		int data_left = fh->fh_size/4;
		int nfsdev;
		int fsid = 0;

		error = nfserr_stale;
		if (rqstp->rq_vers == 3)
			error = nfserr_badhandle;
		if (fh->fh_version == 1) {
			
			datap = fh->fh_auth;
			if (--data_left<0) goto out;
			switch (fh->fh_auth_type) {
			case 0: break;
			default: goto out;
			}

			switch (fh->fh_fsid_type) {
			case 0:
				if ((data_left-=2)<0) goto out;
				nfsdev = ntohl(*datap++);
				xdev = MKDEV(nfsdev>>16, nfsdev&0xFFFF);
				xino = *datap++;
				break;
			case 1:
				if ((data_left-=1)<0) goto out;
				fsid = *datap++;
				break;
			default:
				goto out;
			}
		} else {
			if (fh->fh_size != NFS_FHSIZE)
				goto out;
			/* assume old filehandle format */
			xdev = u32_to_kdev_t(fh->ofh_xdev);
			xino = u32_to_ino_t(fh->ofh_xino);
		}

		/*
		 * Look up the export entry.
		 */
		error = nfserr_stale; 
		if (fh->fh_version == 1 && fh->fh_fsid_type == 1)
			exp = exp_get_fsid(rqstp->rq_client, fsid);
		else
			exp = exp_get(rqstp->rq_client, xdev, xino);

		if (!exp)
			/* export entry revoked */
			goto out;

		/* Check if the request originated from a secure port. */
		error = nfserr_perm;
		if (!rqstp->rq_secure && EX_SECURE(exp)) {
			printk(KERN_WARNING
			       "nfsd: request from insecure port (%08x:%d)!\n",
			       ntohl(rqstp->rq_addr.sin_addr.s_addr),
			       ntohs(rqstp->rq_addr.sin_port));
			goto out;
		}

		/* Set user creds if we haven't done so already. */
		nfsd_setuser(rqstp, exp);

		/*
		 * Look up the dentry using the NFS file handle.
		 */
		error = nfserr_stale;
		if (rqstp->rq_vers == 3)
			error = nfserr_badhandle;

		if (fh->fh_version == 1) {
			/* if fileid_type != 0, and super_operations provide fh_to_dentry lookup,
			 *  then should use that */
			switch (fh->fh_fileid_type) {
			case 0:
				dentry = dget(exp->ex_dentry);
				/* need to revalidate the inode */
				inode = dentry->d_inode;
				if (inode->i_op && inode->i_op->revalidate)
					if (inode->i_op->revalidate(dentry)) {
						dput(dentry);
						dentry = ERR_PTR(-ESTALE);
					}
				break;
			default:
				dentry = find_fh_dentry(exp->ex_dentry->d_inode->i_sb,
							datap, data_left, fh->fh_fileid_type,
							!(exp->ex_flags & NFSEXP_NOSUBTREECHECK));
			}
		} else {
			__u32 tfh[3];
			tfh[0] = fh->ofh_ino;
			tfh[1] = fh->ofh_generation;
			tfh[2] = fh->ofh_dirino;
			dentry = find_fh_dentry(exp->ex_dentry->d_inode->i_sb,
						tfh, 3, fh->ofh_dirino?2:1,
						!(exp->ex_flags & NFSEXP_NOSUBTREECHECK));
		}
		if (IS_ERR(dentry)) {
			if (PTR_ERR(dentry) != -EINVAL)
				error = nfserrno(PTR_ERR(dentry));
			goto out;
		}
#ifdef NFSD_PARANOIA
		if (S_ISDIR(dentry->d_inode->i_mode) &&
		    (dentry->d_flags & DCACHE_NFSD_DISCONNECTED)) {
			printk("nfsd: find_fh_dentry returned a DISCONNECTED directory: %s/%s\n",
			       dentry->d_parent->d_name.name, dentry->d_name.name);
		}
#endif

		fhp->fh_dentry = dentry;
		fhp->fh_export = exp;
		nfsd_nr_verified++;
	} else {
		/* just rechecking permissions
		 * (e.g. nfsproc_create calls fh_verify, then nfsd_create does as well)
		 */
		dprintk("nfsd: fh_verify - just checking\n");
		dentry = fhp->fh_dentry;
		exp = fhp->fh_export;
	}

	inode = dentry->d_inode;

	/* Type check. The correct error return for type mismatches
	 * does not seem to be generally agreed upon. SunOS seems to
	 * use EISDIR if file isn't S_IFREG; a comment in the NFSv3
	 * spec says this is incorrect (implementation notes for the
	 * write call).
	 */

	/* Type can be negative to e.g. exclude directories from linking */
	if (type > 0 && (inode->i_mode & S_IFMT) != type) {
		if (type == S_IFDIR)
			error = nfserr_notdir;
		else if ((inode->i_mode & S_IFMT) == S_IFDIR)
			error = nfserr_isdir;
		else
			error = nfserr_inval;
		goto out;
	}
	if (type < 0 && (inode->i_mode & S_IFMT) == -type) {
		error = (type == -S_IFDIR)? nfserr_isdir : nfserr_notdir;
		goto out;
	}

	/*
	 * Security: Check that the export is valid for dentry <gam3@acm.org>
	 */
	error = 0;

	if (!(exp->ex_flags & NFSEXP_NOSUBTREECHECK)) {
		struct dentry *tdentry = dentry;

		while (tdentry != exp->ex_dentry && !IS_ROOT(tdentry)) {
			struct dentry *parent = tdentry->d_parent;

			/* make sure parents give x permission to user */
			error = permission(parent->d_inode, MAY_EXEC);
			if (error)
				break;
			tdentry = parent;
		}
		if (exp->ex_dentry != tdentry) {
			error = nfserr_stale;
			printk("fh_verify: no root_squashed access at %s/%s.\n",
			       dentry->d_parent->d_name.name,
			       dentry->d_name.name);
			goto out;
		}
	}

	/* Finally, check access permissions. */
	if (!error) {
		error = nfsd_permission(exp, dentry, access);
	}
#ifdef NFSD_PARANOIA_EXTREME
	if (error) {
		printk("fh_verify: %s/%s permission failure, acc=%x, error=%d\n",
		       dentry->d_parent->d_name.name, dentry->d_name.name, access, (error >> 24));
	}
#endif
out:
	if (error == nfserr_stale)
		nfsdstats.fh_stale++;
	return error;
}

/*
 * Compose a file handle for an NFS reply.
 *
 * Note that when first composed, the dentry may not yet have
 * an inode.  In this case a call to fh_update should be made
 * before the fh goes out on the wire ...
 */
inline int _fh_update(struct dentry *dentry, struct svc_export *exp,
		      __u32 *datap, int *maxsize)
{
	struct super_block *sb = dentry->d_inode->i_sb;
	
	if (dentry == exp->ex_dentry) {
		*maxsize = 0;
		return 0;
	}

	if (sb->s_op->dentry_to_fh) {
		int need_parent = !S_ISDIR(dentry->d_inode->i_mode) &&
			!(exp->ex_flags & NFSEXP_NOSUBTREECHECK);
		
		int type = sb->s_op->dentry_to_fh(dentry, datap, maxsize, need_parent);
		return type;
	}

	if (*maxsize < 2)
		return 255;
	*datap++ = ino_t_to_u32(dentry->d_inode->i_ino);
	*datap++ = dentry->d_inode->i_generation;
	if (*maxsize ==2 ||
	    S_ISDIR(dentry->d_inode->i_mode) ||
	    (exp->ex_flags & NFSEXP_NOSUBTREECHECK)) {
		*maxsize = 2;
		return 1;
	}
	*datap++ = ino_t_to_u32(dentry->d_parent->d_inode->i_ino);
	*maxsize = 3;
	return 2;
}

/*
 * for composing old style file handles
 */
inline void _fh_update_old(struct dentry *dentry, struct svc_export *exp,
			   struct knfsd_fh *fh)
{
	fh->ofh_ino = ino_t_to_u32(dentry->d_inode->i_ino);
	fh->ofh_generation = dentry->d_inode->i_generation;
	if (S_ISDIR(dentry->d_inode->i_mode) ||
	    (exp->ex_flags & NFSEXP_NOSUBTREECHECK))
		fh->ofh_dirino = 0;
}

int
fh_compose(struct svc_fh *fhp, struct svc_export *exp, struct dentry *dentry, struct svc_fh *ref_fh)
{
	/* ref_fh is a reference file handle.
	 * if it is non-null, then we should compose a filehandle which is
	 * of the same version, where possible.
	 * Currently, that means that if ref_fh->fh_handle.fh_version == 0xca
	 * Then create a 32byte filehandle using nfs_fhbase_old
	 * But only do this if dentry_to_fh is not available
	 *
	 */
	
	struct inode * inode = dentry->d_inode;
	struct dentry *parent = dentry->d_parent;
	__u32 *datap;

	dprintk("nfsd: fh_compose(exp %x/%ld %s/%s, ino=%ld)\n",
		exp->ex_dev, (long) exp->ex_ino,
		parent->d_name.name, dentry->d_name.name,
		(inode ? inode->i_ino : 0));

	if (fhp->fh_locked || fhp->fh_dentry) {
		printk(KERN_ERR "fh_compose: fh %s/%s not initialized!\n",
			parent->d_name.name, dentry->d_name.name);
	}
	if (fhp->fh_maxsize < NFS_FHSIZE)
		printk(KERN_ERR "fh_compose: called with maxsize %d! %s/%s\n",
		       fhp->fh_maxsize, parent->d_name.name, dentry->d_name.name);

	fhp->fh_dentry = dentry; /* our internal copy */
	fhp->fh_export = exp;

	if (ref_fh &&
	    ref_fh->fh_handle.fh_version == 0xca &&
	    parent->d_inode->i_sb->s_op->dentry_to_fh == NULL) {
		/* old style filehandle please */
		memset(&fhp->fh_handle.fh_base, 0, NFS_FHSIZE);
		fhp->fh_handle.fh_size = NFS_FHSIZE;
		fhp->fh_handle.ofh_dcookie = 0xfeebbaca;
		fhp->fh_handle.ofh_dev =  htonl((MAJOR(exp->ex_dev)<<16)| MINOR(exp->ex_dev));
		fhp->fh_handle.ofh_xdev = fhp->fh_handle.ofh_dev;
		fhp->fh_handle.ofh_xino = ino_t_to_u32(exp->ex_ino);
		fhp->fh_handle.ofh_dirino = ino_t_to_u32(dentry->d_parent->d_inode->i_ino);
		if (inode)
			_fh_update_old(dentry, exp, &fhp->fh_handle);
	} else {
		fhp->fh_handle.fh_version = 1;
		fhp->fh_handle.fh_auth_type = 0;
		datap = fhp->fh_handle.fh_auth+0;
		if ((exp->ex_flags & NFSEXP_FSID) &&
		    (!ref_fh || ref_fh->fh_handle.fh_fsid_type == 1)) {
			fhp->fh_handle.fh_fsid_type = 1;
			/* fsid_type 1 == 4 bytes filesystem id */
			*datap++ = exp->ex_fsid;
			fhp->fh_handle.fh_size = 2*4;
		} else {
			fhp->fh_handle.fh_fsid_type = 0;
			/* fsid_type 0 == 2byte major, 2byte minor, 4byte inode */
			*datap++ = htonl((MAJOR(exp->ex_dev)<<16)| MINOR(exp->ex_dev));
			*datap++ = ino_t_to_u32(exp->ex_ino);
			fhp->fh_handle.fh_size = 3*4;
		}
		if (inode) {
			int size = fhp->fh_maxsize/4 - 3;
			fhp->fh_handle.fh_fileid_type =
				_fh_update(dentry, exp, datap, &size);
			fhp->fh_handle.fh_size += size*4;
		}
	}

	nfsd_nr_verified++;
	if (fhp->fh_handle.fh_fileid_type == 255)
		return nfserr_opnotsupp;
	return 0;
}

/*
 * Update file handle information after changing a dentry.
 * This is only called by nfsd_create, nfsd_create_v3 and nfsd_proc_create
 */
int
fh_update(struct svc_fh *fhp)
{
	struct dentry *dentry;
	__u32 *datap;
	
	if (!fhp->fh_dentry)
		goto out_bad;

	dentry = fhp->fh_dentry;
	if (!dentry->d_inode)
		goto out_negative;
	if (fhp->fh_handle.fh_version != 1) {
		_fh_update_old(dentry, fhp->fh_export, &fhp->fh_handle);
	} else {
		int size;
		if (fhp->fh_handle.fh_fileid_type != 0)
			goto out_uptodate;
		datap = fhp->fh_handle.fh_auth+
			fhp->fh_handle.fh_size/4 -1;
		size = (fhp->fh_maxsize - fhp->fh_handle.fh_size)/4;
		fhp->fh_handle.fh_fileid_type =
			_fh_update(dentry, fhp->fh_export, datap, &size);
		fhp->fh_handle.fh_size += size*4;
	}
out:
	return 0;

out_bad:
	printk(KERN_ERR "fh_update: fh not verified!\n");
	goto out;
out_negative:
	printk(KERN_ERR "fh_update: %s/%s still negative!\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);
	goto out;
out_uptodate:
	printk(KERN_ERR "fh_update: %s/%s already up-to-date!\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);
	goto out;
}

/*
 * Release a file handle.
 */
void
fh_put(struct svc_fh *fhp)
{
	struct dentry * dentry = fhp->fh_dentry;
	if (dentry) {
		fh_unlock(fhp);
		fhp->fh_dentry = NULL;
		dput(dentry);
		nfsd_nr_put++;
	}
	return;
}
