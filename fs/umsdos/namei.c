/*
 *  linux/fs/umsdos/namei.c
 *
 *      Written 1993 by Jacques Gelinas 
 *      Inspired from linux/fs/msdos/... by Werner Almesberger
 *
 * Maintain and access the --linux alternate directory file.
 */
 /*
  * You are in the maze of twisted functions - half of them shouldn't
  * be here...
  */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/msdos_fs.h>
#include <linux/umsdos_fs.h>
#include <linux/slab.h>

#define UMSDOS_DIR_LOCK

#ifdef UMSDOS_DIR_LOCK

static inline void u_sleep_on (struct inode *dir)
{
	sleep_on (&dir->u.umsdos_i.dir_info.p);
}

static inline void u_wake_up (struct inode *dir)
{
    	wake_up (&dir->u.umsdos_i.dir_info.p);
}

/*
 * Wait for creation exclusivity.
 * Return 0 if the dir was already available.
 * Return 1 if a wait was necessary.
 * When 1 is return, it means a wait was done. It does not
 * mean the directory is available.
 */
static int umsdos_waitcreate (struct inode *dir)
{
	int ret = 0;

	if (dir->u.umsdos_i.dir_info.creating
	    && dir->u.umsdos_i.dir_info.pid != current->pid) {
	    	PRINTK (("creating && dir_info.pid=%lu, current->pid=%u\n", dir->u.umsdos_i.dir_info.pid, current->pid));
	    	u_sleep_on (dir);
		ret = 1;
	}
	return ret;
}

/*
 * Wait for any lookup process to finish
 */
static void umsdos_waitlookup (struct inode *dir)
{
	while (dir->u.umsdos_i.dir_info.looking) {
	    	u_sleep_on (dir);
	}
}

/*
 * Lock all other process out of this directory.
 */
/* #Specification: file creation / not atomic
 * File creation is a two step process. First we create (allocate)
 * an entry in the EMD file and then (using the entry offset) we
 * build a unique name for MSDOS. We create this name in the msdos
 * space.
 * 
 * We have to use semaphore (sleep_on/wake_up) to prevent lookup
 * into a directory when we create a file or directory and to
 * prevent creation while a lookup is going on. Since many lookup
 * may happen at the same time, the semaphore is a counter.
 * 
 * Only one creation is allowed at the same time. This protection
 * may not be necessary. The problem arise mainly when a lookup
 * or a readdir is done while a file is partially created. The
 * lookup process see that as a "normal" problem and silently
 * erase the file from the EMD file. Normal because a file
 * may be erased during a MSDOS session, but not removed from
 * the EMD file.
 * 
 * The locking is done on a directory per directory basis. Each
 * directory inode has its wait_queue.
 * 
 * For some operation like hard link, things even get worse. Many
 * creation must occur at once (atomic). To simplify the design
 * a process is allowed to recursively lock the directory for
 * creation. The pid of the locking process is kept along with
 * a counter so a second level of locking is granted or not.
 */
void umsdos_lockcreate (struct inode *dir)
{
	/*
	 * Wait for any creation process to finish except
	 * if we (the process) own the lock
	 */
	while (umsdos_waitcreate (dir) != 0);
	dir->u.umsdos_i.dir_info.creating++;
	dir->u.umsdos_i.dir_info.pid = current->pid;
	umsdos_waitlookup (dir);
}

/*
 * Lock all other process out of those two directories.
 */
static void umsdos_lockcreate2 (struct inode *dir1, struct inode *dir2)
{
	/*
	 * We must check that both directory are available before
	 * locking anyone of them. This is to avoid some deadlock.
	 * Thanks to dglaude@is1.vub.ac.be (GLAUDE DAVID) for pointing
	 * this to me.
	 */
	while (1) {
		if (umsdos_waitcreate (dir1) == 0
		    && umsdos_waitcreate (dir2) == 0) {
			/* We own both now */
			dir1->u.umsdos_i.dir_info.creating++;
			dir1->u.umsdos_i.dir_info.pid = current->pid;
			dir2->u.umsdos_i.dir_info.creating++;
			dir2->u.umsdos_i.dir_info.pid = current->pid;
			break;
		}
	}
	umsdos_waitlookup (dir1);
	umsdos_waitlookup (dir2);
}

/*
 * Wait until creation is finish in this directory.
 */
void umsdos_startlookup (struct inode *dir)
{
	while (umsdos_waitcreate (dir) != 0);
	dir->u.umsdos_i.dir_info.looking++;
}

/*
 * Unlock the directory.
 */
void umsdos_unlockcreate (struct inode *dir)
{
	dir->u.umsdos_i.dir_info.creating--;
	if (dir->u.umsdos_i.dir_info.creating < 0) {
		printk ("UMSDOS: dir->u.umsdos_i.dir_info.creating < 0: %d"
			,dir->u.umsdos_i.dir_info.creating);
	}
    	u_wake_up (dir);
}

/*
 * Tell directory lookup is over.
 */
void umsdos_endlookup (struct inode *dir)
{
	dir->u.umsdos_i.dir_info.looking--;
	if (dir->u.umsdos_i.dir_info.looking < 0) {
		printk ("UMSDOS: dir->u.umsdos_i.dir_info.looking < 0: %d"
			,dir->u.umsdos_i.dir_info.looking);
	}
    	u_wake_up (dir);
}

#else
static void umsdos_lockcreate (struct inode *dir)
{
}
static void umsdos_lockcreate2 (struct inode *dir1, struct inode *dir2)
{
}
void umsdos_startlookup (struct inode *dir)
{
}
static void umsdos_unlockcreate (struct inode *dir)
{
}
void umsdos_endlookup (struct inode *dir)
{
}

#endif

static int umsdos_nevercreat (struct inode *dir, struct dentry *dentry,
				int errcod)
{
	int ret = 0;

	if (umsdos_is_pseudodos (dir, dentry)) {
		/* #Specification: pseudo root / any file creation /DOS
		 * The pseudo sub-directory /DOS can't be created!
		 * EEXIST is returned.
		 * 
		 * The pseudo sub-directory /DOS can't be removed!
		 * EPERM is returned.
		 */
		ret = errcod;
	}
	return ret;
}

/*
 * Add a new file (ordinary or special) into the alternate directory.
 * The file is added to the real MSDOS directory. If successful, it
 * is then added to the EMD file.
 * 
 * Return the status of the operation. 0 mean success.
 *
 * #Specification: create / file exists in DOS
 * Here is a situation: we are trying to create a file with
 * UMSDOS. The file is unknown to UMSDOS but already
 * exists in the DOS directory.
 * 
 * Here is what we are NOT doing:
 * 
 * We could silently assume that everything is fine
 * and allows the creation to succeed.
 * 
 * It is possible not all files in the partition
 * are meant to be visible from linux. By trying to create
 * those file in some directory, one user may get access
 * to those file without proper permissions. Looks like
 * a security hole to me. Off course sharing a file system
 * with DOS is some kind of security hole :-)
 * 
 * So ?
 * 
 * We return EEXIST in this case.
 * The same is true for directory creation.
 */
static int umsdos_create_any (struct inode *dir, struct dentry *dentry,
				int mode, int rdev, char flags)
{
	struct dentry *fake;
	struct inode *inode;
	int ret;
	struct umsdos_info info;

	ret = umsdos_nevercreat (dir, dentry, -EEXIST);
	if (ret)
		goto out;

	ret = umsdos_parse (dentry->d_name.name, dentry->d_name.len, &info);
	if (ret)
		goto out;

	info.entry.mode = mode;
	info.entry.rdev = rdev;
	info.entry.flags = flags;
	info.entry.uid = current->fsuid;
	info.entry.gid = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
	info.entry.ctime = info.entry.atime = info.entry.mtime = CURRENT_TIME;
	info.entry.nlink = 1;
	ret = umsdos_newentry (dentry->d_parent, &info);
	if (ret)
		goto out;

	/* do a real lookup to get the short name dentry */
	fake = umsdos_covered(dentry->d_parent, info.fake.fname, info.fake.len);
	ret = PTR_ERR(fake);
	if (IS_ERR(fake))
		goto out_remove;

	/* should not exist yet ... */
	ret = -EEXIST;
	if (fake->d_inode)
		goto out_remove_dput;

	ret = msdos_create (dir, fake, S_IFREG | 0777);
	if (ret)
		goto out_remove_dput;

	inode = fake->d_inode;
	atomic_inc(&inode->i_count);
	d_instantiate (dentry, inode);
	dput(fake);
	if (atomic_read(&inode->i_count) > 1) {
		printk(KERN_WARNING
			"umsdos_create_any: %s/%s, ino=%ld, icount=%d??\n",
			dentry->d_parent->d_name.name, dentry->d_name.name,
			inode->i_ino, atomic_read(&inode->i_count));
	}
	umsdos_lookup_patch_new(dentry, &info);

out:
	return ret;

	/* Creation failed ... remove the EMD entry */
out_remove_dput:
	dput(fake);
out_remove:
	if (ret == -EEXIST)
		printk(KERN_WARNING "UMSDOS: out of sync, deleting %s/%s\n",
			dentry->d_parent->d_name.name, info.fake.fname);
	umsdos_delentry (dentry->d_parent, &info, S_ISDIR (info.entry.mode));
	goto out;
}

/*
 * Add a new file into the alternate directory.
 * The file is added to the real MSDOS directory. If successful, it
 * is then added to the EMD file.
 * 
 * Return the status of the operation. 0 mean success.
 */
int UMSDOS_create (struct inode *dir, struct dentry *dentry, int mode)
{
	return umsdos_create_any (dir, dentry, mode, 0, 0);
}


/*
 * Initialise the new_entry from the old for a rename operation.
 * (Only useful for umsdos_rename_f() below).
 */
static void umsdos_ren_init (struct umsdos_info *new_info,
			     struct umsdos_info *old_info)
{
	new_info->entry.mode = old_info->entry.mode;
	new_info->entry.rdev = old_info->entry.rdev;
	new_info->entry.uid = old_info->entry.uid;
	new_info->entry.gid = old_info->entry.gid;
	new_info->entry.ctime = old_info->entry.ctime;
	new_info->entry.atime = old_info->entry.atime;
	new_info->entry.mtime = old_info->entry.mtime;
	new_info->entry.flags = old_info->entry.flags;
	new_info->entry.nlink = old_info->entry.nlink;
}

/*
 * Rename a file (move) in the file system.
 */
 
static int umsdos_rename_f (struct inode *old_dir, struct dentry *old_dentry,
			    struct inode *new_dir, struct dentry *new_dentry,
			    int flags)
{
	struct inode *old_inode = old_dentry->d_inode;
	struct dentry *old, *new, *old_emd;
	int err, ret;
	struct umsdos_info old_info;
	struct umsdos_info new_info;

 	ret = -EPERM;
	err = umsdos_parse (old_dentry->d_name.name,
				old_dentry->d_name.len, &old_info);
	if (err)
		goto out;
	err = umsdos_parse (new_dentry->d_name.name,
				new_dentry->d_name.len, &new_info);
	if (err)
		goto out;

	/* Get the EMD dentry for the old parent */
	old_emd = umsdos_get_emd_dentry(old_dentry->d_parent);
	ret = PTR_ERR(old_emd);
	if (IS_ERR(old_emd))
		goto out;

	umsdos_lockcreate2 (old_dir, new_dir);

	ret = umsdos_findentry(old_emd->d_parent, &old_info, 0);
	if (ret)
		goto out_unlock;

	err = umsdos_findentry(new_dentry->d_parent, &new_info, 0);
	if (err == 0) {
		/* check whether it _really_ exists ... */
		ret = -EEXIST;
		if (new_dentry->d_inode)
			goto out_unlock;

		/* bogus lookup? complain and fix up the EMD ... */
		printk(KERN_WARNING
			"umsdos_rename_f: entry %s/%s exists, inode NULL??\n",
			new_dentry->d_parent->d_name.name, new_info.entry.name);
		err = umsdos_delentry(new_dentry->d_parent, &new_info,
					S_ISDIR(new_info.entry.mode));
	}

	umsdos_ren_init (&new_info, &old_info);
	if (flags)
		new_info.entry.flags = flags;
	ret = umsdos_newentry (new_dentry->d_parent, &new_info);
	if (ret)
		goto out_unlock;

	/* If we're moving a hardlink, drop it first */
	if (old_info.entry.flags & UMSDOS_HLINK) {
		d_drop(old_dentry);
	}

	old = umsdos_covered(old_dentry->d_parent, old_info.fake.fname, 
					old_info.fake.len);
	ret = PTR_ERR(old);
	if (IS_ERR(old))
		goto out_unlock;
	/* make sure it's the same inode! */
	ret = -ENOENT;
	/*
	 * note: for hardlinks they will be different!
	 *  old_inode will contain inode of .LINKxxx file containing data, and
	 *  old->d_inode will contain inode of file containing path to .LINKxxx file
	 */
	if (!(old_info.entry.flags & UMSDOS_HLINK)) {
	 	if (old->d_inode != old_inode)
 			goto out_dput;
	}

	new = umsdos_covered(new_dentry->d_parent, new_info.fake.fname, 
					new_info.fake.len);
	ret = PTR_ERR(new);
	if (IS_ERR(new))
		goto out_dput;

	/* Do the msdos-level rename */
	ret = msdos_rename (old_dir, old, new_dir, new);

	dput(new);

	/* If the rename failed, remove the new EMD entry */
	if (ret != 0) {
		umsdos_delentry (new_dentry->d_parent, &new_info,
				 S_ISDIR (new_info.entry.mode));
		goto out_dput;
	}

	/*
	 * Rename successful ... remove the old name from the EMD.
	 * Note that we use the EMD parent here, as the old dentry
	 * may have moved to a new parent ...
	 */
	err = umsdos_delentry (old_emd->d_parent, &old_info,
				S_ISDIR (old_info.entry.mode));
	if (err) {
		/* Failed? Complain a bit, but don't fail the operation */
		printk(KERN_WARNING 
			"umsdos_rename_f: delentry %s/%s failed, error=%d\n",
			old_emd->d_parent->d_name.name, old_info.entry.name,
			err);
	}

	/*
	 * Update f_pos so notify_change will succeed
	 * if the file was already in use.
	 */
	umsdos_set_dirinfo_new(old_dentry, new_info.f_pos);

	/* dput() the dentry if we haven't already */
out_dput:
	dput(old);

out_unlock:
	dput(old_emd);
	umsdos_unlockcreate (old_dir);
	umsdos_unlockcreate (new_dir);

out:
	Printk ((" _ret=%d\n", ret));
	return ret;
}

/*
 * Setup a Symbolic link or a (pseudo) hard link
 * Return a negative error code or 0 if OK.
 */
/* #Specification: symbolic links / strategy
 * A symbolic link is simply a file which holds a path. It is
 * implemented as a normal MSDOS file (not very space efficient :-()
 * 
 * I see two different ways to do this: One is to place the link data
 * in unused entries of the EMD file; the other is to have a separate
 * file dedicated to hold all symbolic links data.
 * 
 * Let's go for simplicity...
 */

/*
 * AV. Should be called with dir->i_sem down.
 */
static int umsdos_symlink_x (struct inode *dir, struct dentry *dentry,
			const char *symname, int mode, char flags)
{
	int ret, len;

	ret = umsdos_create_any (dir, dentry, mode, 0, flags);
	if (ret) {
		printk(KERN_WARNING
			"umsdos_symlink: create failed, ret=%d\n", ret);
		goto out;
	}

	len = strlen (symname) + 1;
	ret = block_symlink(dentry->d_inode, symname, len);
	if (ret < 0)
		goto out_unlink;
out:
	return ret;

out_unlink:
	printk(KERN_WARNING "umsdos_symlink: write failed, unlinking\n");
	UMSDOS_unlink (dir, dentry);
	d_drop(dentry);
	goto out;
}

/*
 * Setup a Symbolic link.
 * Return a negative error code or 0 if OK.
 */
int UMSDOS_symlink ( struct inode *dir, struct dentry *dentry,
		 const char *symname)
{
	return umsdos_symlink_x (dir, dentry, symname, S_IFLNK | 0777, 0);
}

/*
 * Add a link to an inode in a directory
 */
int UMSDOS_link (struct dentry *olddentry, struct inode *dir,
		 struct dentry *dentry)
{
	struct inode *oldinode = olddentry->d_inode;
	struct inode *olddir = olddentry->d_parent->d_inode;
	struct dentry *temp;
	char *path;
	unsigned long buffer;
	int ret;
	struct umsdos_info old_info;
	struct umsdos_info hid_info;

#ifdef UMSDOS_DEBUG_VERBOSE
printk("umsdos_link: new %s/%s -> %s/%s\n",
dentry->d_parent->d_name.name, dentry->d_name.name, 
olddentry->d_parent->d_name.name, olddentry->d_name.name);
#endif
 
	ret = -EPERM;
	if (S_ISDIR (oldinode->i_mode))
		goto out;

	ret = umsdos_nevercreat (dir, dentry, -EPERM);
	if (ret)
		goto out;

	ret = -ENOMEM;
	buffer = get_free_page(GFP_KERNEL);
	if (!buffer)
		goto out;

	/*
	 * Lock the link parent if it's not the same directory.
	 */
	ret = -EDEADLOCK;
	if (olddir != dir) {
		if (atomic_read(&olddir->i_sem.count) < 1)
			goto out_free;
		down(&olddir->i_sem);
	}

	/*
	 * Parse the name and get the visible directory entry.
	 */
	ret = umsdos_parse (olddentry->d_name.name, olddentry->d_name.len,
				&old_info);
	if (ret)
		goto out_unlock;
	ret = umsdos_findentry (olddentry->d_parent, &old_info, 1);
	if (ret) {
printk("UMSDOS_link: %s/%s not in EMD, ret=%d\n",
olddentry->d_parent->d_name.name, olddentry->d_name.name, ret);
		goto out_unlock;
	}

	/*
	 * If the visible dentry is a pseudo-hardlink, the original
	 * file must be already hidden.
	 */
	if (!(old_info.entry.flags & UMSDOS_HLINK)) {
		int err;

		/* create a hidden link name */
		ret = umsdos_newhidden (olddentry->d_parent, &hid_info);
		if (ret) {
printk("umsdos_link: can't make hidden %s/%s, ret=%d\n",
olddentry->d_parent->d_name.name, hid_info.entry.name, ret);
			goto out_unlock;
		}

		/*
		 * Make a dentry and rename the original file ...
		 */
		temp = umsdos_lookup_dentry(olddentry->d_parent,
						hid_info.entry.name,
						hid_info.entry.name_len, 0); 
		ret = PTR_ERR(temp);
		if (IS_ERR(temp)) {
printk("umsdos_link: lookup %s/%s failed, ret=%d\n",
dentry->d_parent->d_name.name, hid_info.entry.name, ret);
			goto cleanup;
		}
		/* rename the link to the hidden location ... */
		ret = umsdos_rename_f(olddir, olddentry, olddir, temp,
					UMSDOS_HIDDEN);
		d_move(olddentry, temp);
		dput(temp);
		if (ret) {
printk("umsdos_link: rename to %s/%s failed, ret=%d\n",
temp->d_parent->d_name.name, temp->d_name.name, ret);
			goto cleanup;
		}
		/*
		 * Capture the path to the hidden link.
		 */
		path = umsdos_d_path(olddentry, (char *) buffer, PAGE_SIZE);
		if (IS_ERR(path)) {
			ret = PTR_ERR(path);
			goto cleanup;
		}
Printk(("umsdos_link: hidden link path=%s\n", path));

		/* mark the inode as a hardlink */
		oldinode->u.umsdos_i.i_is_hlink = 1;

		/*
		 * Recreate a dentry for the original name and symlink it,
		 * then symlink the new dentry. Don't give up if one fails,
		 * or we'll lose the file completely!
		 *
		 * Note: this counts as the "original" reference, so we 
		 * don't increment i_nlink for this one.
		 */ 
		temp = umsdos_lookup_dentry(olddentry->d_parent,
						old_info.entry.name,
						old_info.entry.name_len, 0); 
		ret = PTR_ERR(temp);
		if (!IS_ERR(temp)) {
			ret = umsdos_symlink_x (olddir, temp, path, 
						S_IFREG | 0777, UMSDOS_HLINK);
			dput(temp);
		}

		/* This symlink increments i_nlink (see below.) */
		err = umsdos_symlink_x (dir, dentry, path,
					S_IFREG | 0777, UMSDOS_HLINK);
		/* fold the two errors */
		if (!ret)
			ret = err;
		goto out_unlock;

		/* creation failed ... remove the link entry */
	cleanup:
printk("umsdos_link: link failed, ret=%d, removing %s/%s\n",
ret, olddentry->d_parent->d_name.name, hid_info.entry.name);
		err = umsdos_delentry(olddentry->d_parent, &hid_info, 0);
		goto out_unlock;
	}

Printk(("UMSDOS_link: %s/%s already hidden\n",
olddentry->d_parent->d_name.name, olddentry->d_name.name));
	/*
	 * The original file is already hidden, and we need to get 
	 * the dentry for its real name, not the visible name.
	 * N.B. make sure it's the hidden inode ...
	 */
	if (!oldinode->u.umsdos_i.i_is_hlink)
		printk("UMSDOS_link: %s/%s hidden, ino=%ld not hlink??\n",
			olddentry->d_parent->d_name.name,
			olddentry->d_name.name, oldinode->i_ino);

	/*
	 * In order to get the correct (real) inode, we just drop
	 * the original dentry.
	 */ 
	d_drop(olddentry);
Printk(("UMSDOS_link: hard link %s/%s, fake=%s\n",
olddentry->d_parent->d_name.name, olddentry->d_name.name, old_info.fake.fname));

	/* Do a real lookup to get the short name dentry */
	temp = umsdos_covered(olddentry->d_parent, old_info.fake.fname, 
					old_info.fake.len);
	ret = PTR_ERR(temp);
	if (IS_ERR(temp))
		goto out_unlock;

	/* now resolve the link ... */
	temp = umsdos_solve_hlink(temp);
	ret = PTR_ERR(temp);
	if (IS_ERR(temp))
		goto out_unlock;
	path = umsdos_d_path(temp, (char *) buffer, PAGE_SIZE);
	dput(temp);
	if (IS_ERR(path))
		goto out_unlock;
Printk(("umsdos_link: %s/%s already hidden, path=%s\n",
olddentry->d_parent->d_name.name, olddentry->d_name.name, path));

	/* finally we can symlink it ... */
	ret = umsdos_symlink_x (dir, dentry, path, S_IFREG | 0777,UMSDOS_HLINK);

out_unlock:
	/* remain locked for the call to notify_change ... */
	if (ret == 0) {
		struct iattr newattrs;

		/* Do a real lookup to get the short name dentry */
		temp = umsdos_covered(olddentry->d_parent,
					old_info.fake.fname,
					old_info.fake.len);
		ret = PTR_ERR(temp);
		if (IS_ERR(temp))
			goto out_unlock2;

		/* now resolve the link ... */
		temp = umsdos_solve_hlink(temp);
		ret = PTR_ERR(temp);
		if (IS_ERR(temp))
			goto out_unlock2;


#ifdef UMSDOS_PARANOIA
if (!oldinode->u.umsdos_i.i_is_hlink)
printk("UMSDOS_link: %s/%s, ino=%ld, not marked as hlink!\n",
olddentry->d_parent->d_name.name, olddentry->d_name.name, oldinode->i_ino);
#endif
		temp->d_inode->i_nlink++;
Printk(("UMSDOS_link: linked %s/%s, ino=%ld, nlink=%d\n",
olddentry->d_parent->d_name.name, olddentry->d_name.name,
oldinode->i_ino, oldinode->i_nlink));
		newattrs.ia_valid = 0;
		ret = umsdos_notify_change_locked(temp, &newattrs);
 		if (ret == 0)
			mark_inode_dirty(temp->d_inode);
		dput(temp);
out_unlock2:	
		if (ret == 0)
			mark_inode_dirty(olddentry->d_inode);
	}
	if (olddir != dir)
		up(&olddir->i_sem);

out_free:
	free_page(buffer);
out:
	Printk (("umsdos_link %d\n", ret));
	return ret;
}


/*
 * Add a sub-directory in a directory
 */
/* #Specification: mkdir / Directory already exist in DOS
 * We do the same thing as for file creation.
 * For all user it is an error.
 */
/* #Specification: mkdir / umsdos directory / create EMD
 * When we created a new sub-directory in a UMSDOS
 * directory (one with full UMSDOS semantics), we
 * create immediately an EMD file in the new
 * sub-directory so it inherits UMSDOS semantics.
 */
int UMSDOS_mkdir (struct inode *dir, struct dentry *dentry, int mode)
{
	struct dentry *temp;
	struct inode *inode;
	int ret, err;
	struct umsdos_info info;

	ret = umsdos_nevercreat (dir, dentry, -EEXIST);
	if (ret)
		goto out;

	ret = umsdos_parse (dentry->d_name.name, dentry->d_name.len, &info);
	if (ret)
		goto out;

	info.entry.mode = mode | S_IFDIR;
	info.entry.rdev = 0;
	info.entry.uid = current->fsuid;
	info.entry.gid = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
	info.entry.ctime = info.entry.atime = info.entry.mtime = CURRENT_TIME;
	info.entry.flags = 0;
	info.entry.nlink = 1;
	ret = umsdos_newentry (dentry->d_parent, &info);
	if (ret)
		goto out;

	/* lookup the short name dentry */
	temp = umsdos_covered(dentry->d_parent, info.fake.fname, info.fake.len);
	ret = PTR_ERR(temp);
	if (IS_ERR(temp))
		goto out_remove;

	/* Make sure the short name doesn't exist */
	ret = -EEXIST;
	if (temp->d_inode) {
printk("umsdos_mkdir: short name %s/%s exists\n",
dentry->d_parent->d_name.name, info.fake.fname);
		goto out_remove_dput;
	}

	ret = msdos_mkdir (dir, temp, mode);
	if (ret)
		goto out_remove_dput;

	/*
	 * Lock the inode to protect the EMD creation ...
	 */
	inode = temp->d_inode;
	down(&inode->i_sem);

	atomic_inc(&inode->i_count);
	d_instantiate(dentry, inode);

	/* N.B. this should have an option to create the EMD ... */
	umsdos_lookup_patch_new(dentry, &info);

	/* 
	 * Create the EMD file, and set up the dir so it is
	 * promoted to EMD with the EMD file invisible.
	 *
	 * N.B. error return if EMD fails?
	 */
	err = umsdos_make_emd(dentry);
	umsdos_setup_dir(dentry);

	up(&inode->i_sem);
	dput(temp);

out:
	Printk(("umsdos_mkdir: %s/%s, ret=%d\n",
		dentry->d_parent->d_name.name, dentry->d_name.name, ret));
	return ret;

	/* an error occurred ... remove EMD entry. */
out_remove_dput:
	dput(temp);
out_remove:
	umsdos_delentry (dentry->d_parent, &info, 1);
	goto out;
}

/*
 * Add a new device special file into a directory.
 *
 * #Specification: Special files / strategy
 * Device special file, pipes, etc ... are created like normal
 * file in the msdos file system. Of course they remain empty.
 * 
 * One strategy was to create those files only in the EMD file
 * since they were not important for MSDOS. The problem with
 * that, is that there were not getting inode number allocated.
 * The MSDOS filesystems is playing a nice game to fake inode
 * number, so why not use it.
 * 
 * The absence of inode number compatible with those allocated
 * for ordinary files was causing major trouble with hard link
 * in particular and other parts of the kernel I guess.
 */
int UMSDOS_mknod (struct inode *dir, struct dentry *dentry,
		 int mode, int rdev)
{
	return umsdos_create_any (dir, dentry, mode, rdev, 0);
}

/*
 * Remove a sub-directory.
 */
int UMSDOS_rmdir (struct inode *dir, struct dentry *dentry)
{
	struct dentry *temp;
	int ret, err, empty;
	struct umsdos_info info;

	ret = umsdos_nevercreat (dir, dentry, -EPERM);
	if (ret)
		goto out;

	ret = -EBUSY;
	if (!d_unhashed(dentry))
		goto out;

	/* check whether the EMD is empty */
	ret = -ENOTEMPTY;
	empty = umsdos_isempty (dentry);

	/* Have to remove the EMD file? */
	if (empty == 1) {
		struct dentry *demd;

		demd = umsdos_get_emd_dentry(dentry);
		if (!IS_ERR(demd)) {
			err = -ENOENT;
			if (demd->d_inode)
				err = msdos_unlink (dentry->d_inode, demd);
Printk (("UMSDOS_rmdir: unlinking empty EMD err=%d", err));
#ifdef UMSDOS_PARANOIA
if (err)
printk("umsdos_rmdir: EMD %s/%s unlink failed, err=%d\n",
demd->d_parent->d_name.name, demd->d_name.name, err);
#endif
			if (!err) {
				d_delete(demd);
				ret = 0;
			}
			dput(demd);
		}
	} else if (empty == 2)
		ret = 0;
	if (ret)
		goto out;

	umsdos_parse (dentry->d_name.name, dentry->d_name.len, &info);
	/* Call findentry to complete the mangling */
	umsdos_findentry (dentry->d_parent, &info, 2);
	temp = umsdos_covered(dentry->d_parent, info.fake.fname, info.fake.len);
	ret = PTR_ERR(temp);
	if (IS_ERR(temp))
		goto out;
	/*
	 * Attempt to remove the msdos name.
	 */
	ret = msdos_rmdir (dir, temp);
	if (ret && ret != -ENOENT)
		goto out_dput;

	d_delete(temp);
	/* OK so far ... remove the name from the EMD */
	ret = umsdos_delentry (dentry->d_parent, &info, 1);
#ifdef UMSDOS_PARANOIA
if (ret)
printk("umsdos_rmdir: delentry %s failed, ret=%d\n", info.entry.name, ret);
#endif

	/* dput() temp if we didn't do it above */
out_dput:
	dput(temp);

out:
	Printk (("umsdos_rmdir %d\n", ret));
	return ret;
}


/*
 * Remove a file from the directory.
 *
 * #Specification: hard link / deleting a link
 * When we delete a file and this file is a link,
 * we must subtract 1 from the nlink field of the
 * hidden link.
 * 
 * If the count goes to 0, we delete this hidden
 * link too.
 */
int UMSDOS_unlink (struct inode *dir, struct dentry *dentry)
{
	struct dentry *temp, *link = NULL;
	struct inode *inode;
	int ret;
	struct umsdos_info info;

Printk(("UMSDOS_unlink: entering %s/%s\n",
dentry->d_parent->d_name.name, dentry->d_name.name));

	ret = umsdos_nevercreat (dir, dentry, -EPERM);
	if (ret)
		goto out;

	ret = umsdos_parse (dentry->d_name.name, dentry->d_name.len, &info);
	if (ret)
		goto out;

	umsdos_lockcreate (dir);
	ret = umsdos_findentry (dentry->d_parent, &info, 1);
	if (ret) {
printk("UMSDOS_unlink: %s/%s not in EMD, ret=%d\n",
dentry->d_parent->d_name.name, dentry->d_name.name, ret);
		goto out_unlock;
	}

Printk (("UMSDOS_unlink %.*s ", info.fake.len, info.fake.fname));

	/*
	 * Note! If this is a hardlink and the names are aliased,
	 * the short-name lookup will return the hardlink dentry.
	 * In order to get the correct (real) inode, we just drop
	 * the original dentry.
	 */ 
	if (info.entry.flags & UMSDOS_HLINK) {
		d_drop(dentry);
	}

	/* Do a real lookup to get the short name dentry */
	temp = umsdos_covered(dentry->d_parent, info.fake.fname, info.fake.len);
	ret = PTR_ERR(temp);
	if (IS_ERR(temp))
		goto out_unlock;

	/*
	 * Resolve hardlinks now, but defer processing until later.
	 */
	if (info.entry.flags & UMSDOS_HLINK) {
		link = umsdos_solve_hlink(dget(temp));
	}

	/* Delete the EMD entry */
	ret = umsdos_delentry (dentry->d_parent, &info, 0);
	if (ret && ret != -ENOENT) {
		printk(KERN_WARNING "UMSDOS_unlink: delentry %s, error=%d\n",
			info.entry.name, ret);
		goto out_dput;
	}

	ret = msdos_unlink(dir, temp);
	if (!ret)
		d_delete(temp);
#ifdef UMSDOS_PARANOIA
if (ret)
printk("umsdos_unlink: %s/%s unlink failed, ret=%d\n",
temp->d_parent->d_name.name, temp->d_name.name, ret);
#endif

	/* dput() temp if we didn't do it above */
out_dput:
	dput(temp);

out_unlock:
	umsdos_unlockcreate (dir);

	/*
	 * Now check for deferred handling of a hardlink.
	 */
	if (!link)
		goto out;

	if (IS_ERR(link)) {
printk("umsdos_unlink: failed to resolve %s/%s\n",
dentry->d_parent->d_name.name, dentry->d_name.name);
		if (!ret)
			ret = PTR_ERR(link);
		goto out;
	}

Printk(("umsdos_unlink: link %s/%s deferred, pending ret=%d\n",
link->d_parent->d_name.name, link->d_name.name, ret));

	/* already have an error? */
	if (ret)
		goto out_cleanup;

	/* make sure the link exists ... */
	inode = link->d_inode;
	if (!inode) {
		printk(KERN_WARNING "umsdos_unlink: hard link not found\n");
		goto out_cleanup;
	}

	/*
	 * If this was the last linked reference, delete it now.
	 *
	 * N.B. Deadlock problem? We should be holding the lock
	 * for the hardlink's parent, but another process might
	 * be holding that lock waiting for us to finish ...
	 */
	if (inode->i_nlink <= 1) {
		ret = UMSDOS_unlink (link->d_parent->d_inode, link);
		if (ret) {
			printk(KERN_WARNING
				"umsdos_unlink: link removal failed, ret=%d\n",
				 ret);
		} else
			d_delete(link);
	} else {
		struct iattr newattrs;
		inode->i_nlink--;
		newattrs.ia_valid = 0;
		ret = umsdos_notify_change_locked(link, &newattrs);
		if (!ret)
			mark_inode_dirty(link->d_inode);
	}

out_cleanup:
	d_drop(link);
	dput(link);

out:
	Printk (("umsdos_unlink %d\n", ret));
	return ret;
}

/*
 * Rename (move) a file.
 */
int UMSDOS_rename (struct inode *old_dir, struct dentry *old_dentry,
		   struct inode *new_dir, struct dentry *new_dentry)
{
	int ret;

	ret = umsdos_nevercreat (new_dir, new_dentry, -EEXIST);
	if (ret)
		return ret;

		/*
		 * If the target already exists, delete it first.
		 */
	if (new_dentry->d_inode) {
		dget(new_dentry);
		if (S_ISDIR(old_dentry->d_inode->i_mode))
			ret = UMSDOS_rmdir (new_dir, new_dentry);
		else
			ret = UMSDOS_unlink (new_dir, new_dentry);
		if (!ret)
			d_drop(new_dentry);
		dput(new_dentry);
		if (ret)
			return ret;
	}
	ret = umsdos_rename_f(old_dir, old_dentry, new_dir, new_dentry, 0);
	return ret;
}
