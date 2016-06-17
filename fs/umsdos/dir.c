/*
 *  linux/fs/umsdos/dir.c
 *
 *  Written 1993 by Jacques Gelinas
 *      Inspired from linux/fs/msdos/... : Werner Almesberger
 *
 *  Extended MS-DOS directory handling functions
 */

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/msdos_fs.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/limits.h>
#include <linux/umsdos_fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>

#define UMSDOS_SPECIAL_DIRFPOS	3
extern struct dentry *saved_root;
extern struct inode *pseudo_root;

/* #define UMSDOS_DEBUG_VERBOSE 1 */

/*
 * Dentry operations routines
 */

/* nothing for now ... */
static int umsdos_dentry_validate(struct dentry *dentry, int flags)
{
	return 1;
}

/* for now, drop everything to force lookups ... */
/* ITYM s/everything/& positive/... */
static int umsdos_dentry_dput(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	if (inode) {
		return 1;
	}
	return 0;
}

struct dentry_operations umsdos_dentry_operations =
{
	d_revalidate:	umsdos_dentry_validate,
	d_delete:	umsdos_dentry_dput,
};

struct UMSDOS_DIR_ONCE {
	void *dirbuf;
	filldir_t filldir;
	int count;
	int stop;
};

/*
 * Record a single entry the first call.
 * Return -EINVAL the next one.
 * NOTE: filldir DOES NOT use a dentry
 */

static int umsdos_dir_once (	void *buf,
				const char *name,
				int len,
				loff_t offset,
				ino_t ino,
				unsigned type)
{
	int ret = -EINVAL;
	struct UMSDOS_DIR_ONCE *d = (struct UMSDOS_DIR_ONCE *) buf;

	if (d->count == 0) {
		PRINTK ((KERN_DEBUG "dir_once :%.*s: offset %Ld\n", 
			len, name, offset));
		ret = d->filldir (d->dirbuf, name, len, offset, ino, DT_UNKNOWN);
		d->stop = ret < 0;
		d->count = 1;
	}
	return ret;
}


/*
 * Read count directory entries from directory filp
 * Return a negative value from linux/errno.h.
 * Return > 0 if success (the number of bytes written by filldir).
 * 
 * This function is used by the normal readdir VFS entry point,
 * and in order to get the directory entry from a file's dentry.
 * See umsdos_dentry_to_entry() below.
 */
 
static int umsdos_readdir_x (struct inode *dir, struct file *filp,
				void *dirbuf, struct umsdos_dirent *u_entry,
				filldir_t filldir)
{
	struct dentry *demd;
	off_t start_fpos;
	int ret = 0;
	loff_t pos;

	umsdos_startlookup (dir);

	if (filp->f_pos == UMSDOS_SPECIAL_DIRFPOS && dir == pseudo_root) {

		/*
		 * We don't need to simulate this pseudo directory
		 * when umsdos_readdir_x is called for internal operation
		 * of umsdos. This is why dirent_in_fs is tested
		 */
		/* #Specification: pseudo root / directory /DOS
		 * When umsdos operates in pseudo root mode (C:\linux is the
		 * linux root), it simulate a directory /DOS which points to
		 * the real root of the file system.
		 */

		Printk ((KERN_WARNING "umsdos_readdir_x: pseudo_root thing UMSDOS_SPECIAL_DIRFPOS\n"));
		if (filldir (dirbuf, "DOS", 3, 
				UMSDOS_SPECIAL_DIRFPOS, UMSDOS_ROOT_INO, DT_DIR) == 0) {
			filp->f_pos++;
		}
		goto out_end;
	}

	if (filp->f_pos < 2 || 
	    (dir->i_ino != UMSDOS_ROOT_INO && filp->f_pos == 32)) {
	
		int last_f_pos = filp->f_pos;
		struct UMSDOS_DIR_ONCE bufk;

		Printk (("umsdos_readdir_x: . or .. /mn/?\n"));

		bufk.dirbuf = dirbuf;
		bufk.filldir = filldir;
		bufk.count = 0;

		ret = fat_readdir (filp, &bufk, umsdos_dir_once);
		if (last_f_pos > 0 && filp->f_pos > last_f_pos)
			filp->f_pos = UMSDOS_SPECIAL_DIRFPOS;
		if (u_entry != NULL)
			u_entry->flags = 0;
		goto out_end;
	}

	Printk (("umsdos_readdir_x: normal file /mn/?\n"));

	/* get the EMD dentry */
	demd = umsdos_get_emd_dentry(filp->f_dentry);
	ret = PTR_ERR(demd);
	if (IS_ERR(demd))
		goto out_end;
	ret = -EIO;
	if (!demd->d_inode) {
		printk(KERN_WARNING 
			"umsdos_readir_x: EMD file %s/%s not found\n",
			demd->d_parent->d_name.name, demd->d_name.name);
		goto out_dput;
	}

	pos = filp->f_pos;
	start_fpos = filp->f_pos;

	if (pos <= UMSDOS_SPECIAL_DIRFPOS + 1)
		pos = 0;
	ret = 0;
	while (pos < demd->d_inode->i_size) {
		off_t cur_f_pos = pos;
		struct dentry *dret;
		struct inode *inode;
		struct umsdos_dirent entry;
		struct umsdos_info info;

		ret = -EIO;
		if (umsdos_emd_dir_readentry (demd, &pos, &entry) != 0)
			break;
		if (entry.name_len == 0)
			continue;
#ifdef UMSDOS_DEBUG_VERBOSE
if (entry.flags & UMSDOS_HLINK)
printk("umsdos_readdir_x: %s/%s is hardlink\n",
filp->f_dentry->d_name.name, entry.name);
#endif

		umsdos_parse (entry.name, entry.name_len, &info);
		info.f_pos = cur_f_pos;
		umsdos_manglename (&info);
		/*
		 * Do a real lookup on the short name.
		 */
		dret = umsdos_covered(filp->f_dentry, info.fake.fname,
						 info.fake.len);
		ret = PTR_ERR(dret);
		if (IS_ERR(dret))
			break;
		/*
		 * If the file wasn't found, remove it from the EMD.
		 */
		inode = dret->d_inode;
		if (!inode)
			goto remove_name;
#ifdef UMSDOS_DEBUG_VERBOSE
if (inode->u.umsdos_i.i_is_hlink)
printk("umsdos_readdir_x: %s/%s already resolved, ino=%ld\n",
dret->d_parent->d_name.name, dret->d_name.name, inode->i_ino);
#endif

Printk (("Found %s/%s, ino=%ld, flags=%x\n",
dret->d_parent->d_name.name, info.fake.fname, dret->d_inode->i_ino,
entry.flags));
		/* check whether to resolve a hard-link */
		if ((entry.flags & UMSDOS_HLINK) &&
		    !inode->u.umsdos_i.i_is_hlink) {
			dret = umsdos_solve_hlink (dret);
			ret = PTR_ERR(dret);
			if (IS_ERR(dret))
				break;
			inode = dret->d_inode;
			if (!inode) {
printk("umsdos_readdir_x: %s/%s negative after link\n",
dret->d_parent->d_name.name, dret->d_name.name);
				goto clean_up;
			}
		}

		/* #Specification:  pseudo root / reading real root
		 * The pseudo root (/linux) is logically
		 * erased from the real root.  This means that
		 * ls /DOS, won't show "linux". This avoids
		 * infinite recursion (/DOS/linux/DOS/linux/...) while
		 * walking the file system.
		 */
		if (inode != pseudo_root && !(entry.flags & UMSDOS_HIDDEN)) {
			if (filldir (dirbuf, entry.name, entry.name_len,
				 cur_f_pos, inode->i_ino, DT_UNKNOWN) < 0) {
				pos = cur_f_pos;
			}
Printk(("umsdos_readdir_x: got %s/%s, ino=%ld\n",
dret->d_parent->d_name.name, dret->d_name.name, inode->i_ino));
			if (u_entry != NULL)
				*u_entry = entry;
			dput(dret);
			ret = 0;
			break;
		}
	clean_up:
		dput(dret);
		continue;

	remove_name:
		/* #Specification:  umsdos / readdir / not in MSDOS
		 * During a readdir operation, if the file is not
		 * in the MS-DOS directory any more, the entry is
		 * removed from the EMD file silently.
		 */
#ifdef UMSDOS_PARANOIA
printk("umsdos_readdir_x: %s/%s out of sync, erasing\n",
filp->f_dentry->d_name.name, info.entry.name);
#endif
		ret = umsdos_delentry(filp->f_dentry, &info, 
					S_ISDIR(info.entry.mode));
		if (ret)
			printk(KERN_WARNING 
				"umsdos_readdir_x: delentry %s, err=%d\n",
				info.entry.name, ret);
		goto clean_up;
	}
	/*
	 * If the fillbuf has failed, f_pos is back to 0.
	 * To avoid getting back into the . and .. state
	 * (see comments at the beginning), we put back
	 * the special offset.
	 */
	filp->f_pos = pos;
	if (filp->f_pos == 0)
		filp->f_pos = start_fpos;
out_dput:
	dput(demd);

out_end:
	umsdos_endlookup (dir);
	
	Printk ((KERN_DEBUG "read dir %p pos %Ld ret %d\n",
		dir, filp->f_pos, ret));
	return ret;
}


/*
 * Read count directory entries from directory filp.
 * Return a negative value from linux/errno.h.
 * Return 0 or positive if successful.
 */
 
static int UMSDOS_readdir (struct file *filp, void *dirbuf, filldir_t filldir)
{
	struct inode *dir = filp->f_dentry->d_inode;
	int ret = 0, count = 0;
	struct UMSDOS_DIR_ONCE bufk;

	bufk.dirbuf = dirbuf;
	bufk.filldir = filldir;
	bufk.stop = 0;

	Printk (("UMSDOS_readdir in\n"));
	while (ret == 0 && bufk.stop == 0) {
		struct umsdos_dirent entry;

		bufk.count = 0;
		ret = umsdos_readdir_x (dir, filp, &bufk, &entry, 
					umsdos_dir_once);
		if (bufk.count == 0)
			break;
		count += bufk.count;
	}
	Printk (("UMSDOS_readdir out %d count %d pos %Ld\n", 
		ret, count, filp->f_pos));
	return count ? : ret;
}


/*
 * Complete the inode content with info from the EMD file.
 *
 * This function modifies the state of a dir inode.  It decides
 * whether the dir is a UMSDOS or DOS directory.  This is done
 * deeper in umsdos_patch_inode() called at the end of this function.
 * 
 * Because it is does disk access, umsdos_patch_inode() may block.
 * At the same time, another process may get here to initialise
 * the same directory inode. There are three cases.
 * 
 * 1) The inode is already initialised.  We do nothing.
 * 2) The inode is not initialised.  We lock access and do it.
 * 3) Like 2 but another process has locked the inode, so we try
 * to lock it and check right afterward check whether
 * initialisation is still needed.
 * 
 * 
 * Thanks to the "mem" option of the kernel command line, it was
 * possible to consistently reproduce this problem by limiting
 * my memory to 4 MB and running X.
 *
 * Do this only if the inode is freshly read, because we will lose
 * the current (updated) content.
 *
 * A lookup of a mount point directory yield the inode into
 * the other fs, so we don't care about initialising it. iget()
 * does this automatically.
 */

void umsdos_lookup_patch_new(struct dentry *dentry, struct umsdos_info *info)
{
	struct inode *inode = dentry->d_inode;
	struct umsdos_dirent *entry = &info->entry;

	/*
	 * This part of the initialization depends only on i_patched.
	 */
	if (inode->u.umsdos_i.i_patched)
		goto out;
	inode->u.umsdos_i.i_patched = 1;
	if (S_ISREG (entry->mode))
		entry->mtime = inode->i_mtime;
	inode->i_mode = entry->mode;
	inode->i_rdev = to_kdev_t (entry->rdev);
	inode->i_atime = entry->atime;
	inode->i_ctime = entry->ctime;
	inode->i_mtime = entry->mtime;
	inode->i_uid = entry->uid;
	inode->i_gid = entry->gid;

	/* #Specification: umsdos / i_nlink
	 * The nlink field of an inode is maintained by the MSDOS file system
	 * for directory and by UMSDOS for other files.  The logic is that
	 * MSDOS is already figuring out what to do for directories and
	 * does nothing for other files.  For MSDOS, there are no hard links
	 * so all file carry nlink==1.  UMSDOS use some info in the
	 * EMD file to plug the correct value.
	 */
	if (!S_ISDIR (entry->mode)) {
		if (entry->nlink > 0) {
			inode->i_nlink = entry->nlink;
		} else {
			printk (KERN_ERR 
				"UMSDOS:  lookup_patch entry->nlink < 1 ???\n");
		}
	}
	/*
	 * The mode may have changed, so patch the inode again.
	 */
	umsdos_patch_dentry_inode(dentry, info->f_pos);
	umsdos_set_dirinfo_new(dentry, info->f_pos);

out:
	return;
}


/*
 * Return != 0 if an entry is the pseudo DOS entry in the pseudo root.
 */

int umsdos_is_pseudodos (struct inode *dir, struct dentry *dentry)
{
	/* #Specification: pseudo root / DOS hard coded
	 * The pseudo sub-directory DOS in the pseudo root is hard coded.
	 * The name is DOS. This is done this way to help standardised
	 * the umsdos layout. The idea is that from now on /DOS is
	 * a reserved path and nobody will think of using such a path
	 * for a package.
	 */
	return dir == pseudo_root
	    && dentry->d_name.len == 3
	    && dentry->d_name.name[0] == 'D'
	    && dentry->d_name.name[1] == 'O'
	    && dentry->d_name.name[2] == 'S';
}


/*
 * Check whether a file exists in the current directory.
 * Return 0 if OK, negative error code if not (ex: -ENOENT).
 *
 * fills dentry->d_inode with found inode, and increments its count.
 * if not found, return -ENOENT.
 */
/* #Specification: umsdos / lookup
 * A lookup for a file is done in two steps.  First, we
 * locate the file in the EMD file.  If not present, we
 * return an error code (-ENOENT).  If it is there, we
 * repeat the operation on the msdos file system. If
 * this fails, it means that the file system is not in
 * sync with the EMD file.   We silently remove this
 * entry from the EMD file, and return ENOENT.
 */

struct dentry *umsdos_lookup_x (struct inode *dir, struct dentry *dentry, int nopseudo)
{				
	struct dentry *dret = NULL;
	struct inode *inode;
	int ret = -ENOENT;
	struct umsdos_info info;

#ifdef UMSDOS_DEBUG_VERBOSE
printk("umsdos_lookup_x: looking for %s/%s\n", 
dentry->d_parent->d_name.name, dentry->d_name.name);
#endif

	umsdos_startlookup (dir);
	if (umsdos_is_pseudodos (dir, dentry)) {
		/* #Specification: pseudo root / lookup(DOS)
		 * A lookup of DOS in the pseudo root will always succeed
		 * and return the inode of the real root.
		 */
		Printk ((KERN_DEBUG "umsdos_lookup_x: following /DOS\n"));
		inode = saved_root->d_inode;
		goto out_add;
	}

	ret = umsdos_parse (dentry->d_name.name, dentry->d_name.len, &info);
	if (ret) {
printk("umsdos_lookup_x: %s/%s parse failed, ret=%d\n", 
dentry->d_parent->d_name.name, dentry->d_name.name, ret);
		goto out;
	}

	ret = umsdos_findentry (dentry->d_parent, &info, 0);
	if (ret) {
if (ret != -ENOENT)
printk("umsdos_lookup_x: %s/%s findentry failed, ret=%d\n", 
dentry->d_parent->d_name.name, dentry->d_name.name, ret);
		goto out;
	}
Printk (("lookup %.*s pos %lu ret %d len %d ", 
info.fake.len, info.fake.fname, info.f_pos, ret, info.fake.len));

	/* do a real lookup to get the short name ... */
	dret = umsdos_covered(dentry->d_parent, info.fake.fname, info.fake.len);
	ret = PTR_ERR(dret);
	if (IS_ERR(dret)) {
printk("umsdos_lookup_x: %s/%s real lookup failed, ret=%d\n", 
dentry->d_parent->d_name.name, dentry->d_name.name, ret);
		goto out;
	}
	inode = dret->d_inode;
	if (!inode)
		goto out_remove;
	umsdos_lookup_patch_new(dret, &info);
#ifdef UMSDOS_DEBUG_VERBOSE
printk("umsdos_lookup_x: found %s/%s, ino=%ld\n", 
dret->d_parent->d_name.name, dret->d_name.name, dret->d_inode->i_ino);
#endif

	/* Check for a hard link */
	if ((info.entry.flags & UMSDOS_HLINK) &&
	    !inode->u.umsdos_i.i_is_hlink) {
		dret = umsdos_solve_hlink (dret);
		ret = PTR_ERR(dret);
		if (IS_ERR(dret))
			goto out;
		ret = -ENOENT;
		inode = dret->d_inode;
		if (!inode) {
printk("umsdos_lookup_x: %s/%s negative after link\n", 
dret->d_parent->d_name.name, dret->d_name.name);
			goto out_dput;
		}
	}

	if (inode == pseudo_root && !nopseudo) {
		/* #Specification: pseudo root / dir lookup
		 * For the same reason as readdir, a lookup in /DOS for
		 * the pseudo root directory (linux) will fail.
		 */
		/*
		 * This has to be allowed for resolving hard links
		 * which are recorded independently of the pseudo-root
		 * mode.
		 */
printk("umsdos_lookup_x: skipping DOS/linux\n");
		ret = -ENOENT;
		goto out_dput;
	}

	/*
	 * We've found it OK.  Now hash the dentry with the inode.
	 */
out_add:
	atomic_inc(&inode->i_count);
	d_add (dentry, inode);
	dentry->d_op = &umsdos_dentry_operations;
	ret = 0;

out_dput:
	if (dret && dret != dentry)
		d_drop(dret);
	dput(dret);
out:
	umsdos_endlookup (dir);
	return ERR_PTR(ret);

out_remove:
	printk(KERN_WARNING "UMSDOS:  entry %s/%s out of sync, erased\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);
	umsdos_delentry (dentry->d_parent, &info, S_ISDIR (info.entry.mode));
	ret = -ENOENT;
	goto out_dput;
}


/*
 * Check whether a file exists in the current directory.
 * Return 0 if OK, negative error code if not (ex: -ENOENT).
 * 
 * Called by VFS; should fill dentry->d_inode via d_add.
 */

struct dentry *UMSDOS_lookup (struct inode *dir, struct dentry *dentry)
{
	struct dentry *ret;

	ret = umsdos_lookup_x (dir, dentry, 0);

	/* Create negative dentry if not found. */
	if (ret == ERR_PTR(-ENOENT)) {
		Printk ((KERN_DEBUG 
			"UMSDOS_lookup: converting -ENOENT to negative\n"));
		d_add (dentry, NULL);
		dentry->d_op = &umsdos_dentry_operations;
		ret = NULL;
	}
	return ret;
}

struct dentry *umsdos_covered(struct dentry *parent, char *name, int len)
{
	struct dentry *result, *dentry;
	struct qstr qstr;

	qstr.name = name;
	qstr.len  = len;
	qstr.hash = full_name_hash(name, len);
	result = ERR_PTR(-ENOMEM);
	dentry = d_alloc(parent, &qstr);
	if (dentry) {
		/* XXXXXXXXXXXXXXXXXXX Race alert! */
		result = UMSDOS_rlookup(parent->d_inode, dentry);
		d_drop(dentry);
		if (result)
			goto out_fail;
		return dentry;
	}
out:
	return result;

out_fail:
	dput(dentry);
	goto out;
}

/*
 * Lookup or create a dentry from within the filesystem.
 *
 * We need to use this instead of lookup_dentry, as the 
 * directory semaphore lock is already held.
 */
struct dentry *umsdos_lookup_dentry(struct dentry *parent, char *name, int len,
					int real)
{
	struct dentry *result, *dentry;
	struct qstr qstr;

	qstr.name = name;
	qstr.len  = len;
	qstr.hash = full_name_hash(name, len);
	result = d_lookup(parent, &qstr);
	if (!result) {
		result = ERR_PTR(-ENOMEM);
		dentry = d_alloc(parent, &qstr);
		if (dentry) {
			result = real ?
				UMSDOS_rlookup(parent->d_inode, dentry) :
				UMSDOS_lookup(parent->d_inode, dentry);
			if (result)
				goto out_fail;
			return dentry;
		}
	}
out:
	return result;

out_fail:
	dput(dentry);
	goto out;
}

/*
 * Return a path relative to our root.
 */
char * umsdos_d_path(struct dentry *dentry, char * buffer, int len)
{
	struct dentry * old_root;
	char * path;

	read_lock(&current->fs->lock);
	old_root = dget(current->fs->root);
	read_unlock(&current->fs->lock);
	spin_lock(&dcache_lock);
	path = __d_path(dentry, current->fs->rootmnt, dentry->d_sb->s_root, current->fs->rootmnt, buffer, len); /* FIXME: current->fs->rootmnt */
	spin_unlock(&dcache_lock);

	if (IS_ERR(path))
		return path;

	if (*path == '/')
		path++; /* skip leading '/' */

	if (current->fs->root->d_inode == pseudo_root)
	{
		*(path-1) = '/';
		path -= (UMSDOS_PSDROOT_LEN+1);
		memcpy(path, UMSDOS_PSDROOT_NAME, UMSDOS_PSDROOT_LEN);
	}
	dput(old_root);

	return path;
}

/*
 * Return the dentry which points to a pseudo-hardlink.
 *
 * it should try to find file it points to
 * if file is found, return new dentry/inode
 * The resolved inode will have i_is_hlink set.
 *
 * Note: the original dentry is always dput(), even if an error occurs.
 */

struct dentry *umsdos_solve_hlink (struct dentry *hlink)
{
	/* root is our root for resolving pseudo-hardlink */
	struct dentry *base = hlink->d_sb->s_root;
	struct dentry *dentry_dst;
	char *path, *pt;
	int len;
	struct address_space *mapping = hlink->d_inode->i_mapping;
	struct page *page;

	page=read_cache_page(mapping,0,(filler_t *)mapping->a_ops->readpage,NULL);
	dentry_dst=(struct dentry *)page;
	if (IS_ERR(page))
		goto out;
	wait_on_page(page);
	if (!Page_Uptodate(page))
		goto async_fail;

	dentry_dst = ERR_PTR(-ENOMEM);
	path = (char *) kmalloc (PATH_MAX, GFP_KERNEL);
	if (path == NULL)
		goto out_release;
	memcpy(path, kmap(page), hlink->d_inode->i_size);
	kunmap(page);
	page_cache_release(page);

	len = hlink->d_inode->i_size;

	/* start at root dentry */
	dentry_dst = dget(base);
	path[len] = '\0';
	
	pt = path;
	if (*path == '/')
		pt++; /* skip leading '/' */
	
	if (base->d_inode == pseudo_root)
		pt += (UMSDOS_PSDROOT_LEN + 1);
	
	while (1) {
		struct dentry *dir = dentry_dst, *demd;
		char *start = pt;
		int real;

		while (*pt != '\0' && *pt != '/') pt++;
		len = (int) (pt - start);
		if (*pt == '/') *pt++ = '\0';

		real = 1;
		demd = umsdos_get_emd_dentry(dir);
		if (!IS_ERR(demd)) {
			if (demd->d_inode)
				real = 0;
			dput(demd);
		}

#ifdef UMSDOS_DEBUG_VERBOSE
printk ("umsdos_solve_hlink: dir %s/%s, name=%s, real=%d\n",
dir->d_parent->d_name.name, dir->d_name.name, start, real);
#endif
		dentry_dst = umsdos_lookup_dentry(dir, start, len, real);
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
		if (real)
			d_drop(dir);
		dput (dir);
		if (IS_ERR(dentry_dst))
			break;
		/* not found? stop search ... */
		if (!dentry_dst->d_inode) {
			break;
		}
		if (*pt == '\0')	/* we're finished! */
			break;
	} /* end while */

	if (!IS_ERR(dentry_dst)) {
		struct inode *inode = dentry_dst->d_inode;
		if (inode) {
			inode->u.umsdos_i.i_is_hlink = 1;
#ifdef UMSDOS_DEBUG_VERBOSE
printk ("umsdos_solve_hlink: resolved link %s/%s, ino=%ld\n",
dentry_dst->d_parent->d_name.name, dentry_dst->d_name.name, inode->i_ino);
#endif
		} else {
#ifdef UMSDOS_DEBUG_VERBOSE
printk ("umsdos_solve_hlink: resolved link %s/%s negative!\n",
dentry_dst->d_parent->d_name.name, dentry_dst->d_name.name);
#endif
		}
	} else
		printk(KERN_WARNING
			"umsdos_solve_hlink: err=%ld\n", PTR_ERR(dentry_dst));
	kfree (path);

out:
	dput(hlink);	/* original hlink no longer needed */
	return dentry_dst;

async_fail:
	dentry_dst = ERR_PTR(-EIO);
out_release:
	page_cache_release(page);
	goto out;
}	


struct file_operations umsdos_dir_operations =
{
	read:		generic_read_dir,
	readdir:	UMSDOS_readdir,
	ioctl:		UMSDOS_ioctl_dir,
};

struct inode_operations umsdos_dir_inode_operations =
{
	create:		UMSDOS_create,
	lookup:		UMSDOS_lookup,
	link:		UMSDOS_link,
	unlink:		UMSDOS_unlink,
	symlink:	UMSDOS_symlink,
	mkdir:		UMSDOS_mkdir,
	rmdir:		UMSDOS_rmdir,
	mknod:		UMSDOS_mknod,
	rename:		UMSDOS_rename,
	setattr:	UMSDOS_notify_change,
};
