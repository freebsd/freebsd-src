/*
 *  linux/fs/umsdos/ioctl.c
 *
 *  Written 1993 by Jacques Gelinas
 *
 *  Extended MS-DOS ioctl directory handling functions
 */

#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/msdos_fs.h>
#include <linux/umsdos_fs.h>

struct UMSDOS_DIR_ONCE {
	struct dirent *ent;
	int count;
};

/*
 * Record a single entry the first call.
 * Return -EINVAL the next one.
 */
static int umsdos_ioctl_fill (
				     void *buf,
				     const char *name,
				     int name_len,
				     loff_t offset,
				     ino_t ino,
				     unsigned type)
{
	int ret = -EINVAL;
	struct UMSDOS_DIR_ONCE *d = (struct UMSDOS_DIR_ONCE *) buf;

	if (d->count == 0) {
		copy_to_user (d->ent->d_name, name, name_len);
		put_user ('\0', d->ent->d_name + name_len);
		put_user (name_len, &d->ent->d_reclen);
		put_user (ino, &d->ent->d_ino);
		put_user (offset, &d->ent->d_off);
		d->count = 1;
		ret = 0;
	}
	return ret;
}


/*
 * Perform special function on a directory
 */
/* #Specification: ioctl / prototypes
 * The official prototype for the umsdos ioctl on directory
 * is:
 * 
 * int ioctl (
 * int fd,          // File handle of the directory
 * int cmd, // command
 * struct umsdos_ioctl *data)
 * 
 * The struct and the commands are defined in linux/umsdos_fs.h.
 * 
 * umsdos_progs/umsdosio.c provide an interface in C++ to all
 * these ioctl. umsdos_progs/udosctl is a small utility showing
 * all this.
 * 
 * These ioctl generally allow one to work on the EMD or the
 * DOS directory independently. These are essential to implement
 * the synchronise.
 */
int UMSDOS_ioctl_dir(struct inode *dir, struct file *filp, unsigned int cmd,
			unsigned long data_ptr)
{
	struct dentry *dentry = filp->f_dentry;
	struct umsdos_ioctl *idata = (struct umsdos_ioctl *) data_ptr;
	int ret;
	struct umsdos_ioctl data;

Printk(("UMSDOS_ioctl_dir: %s/%s, cmd=%d, data=%08lx\n",
dentry->d_parent->d_name.name, dentry->d_name.name, cmd, data_ptr));

	/* forward non-umsdos ioctls - this hopefully doesn't cause conflicts */
	if (cmd != UMSDOS_GETVERSION
	    && cmd != UMSDOS_READDIR_DOS
	    && cmd != UMSDOS_READDIR_EMD
	    && cmd != UMSDOS_INIT_EMD
	    && cmd != UMSDOS_CREAT_EMD
	    && cmd != UMSDOS_RENAME_DOS
	    && cmd != UMSDOS_UNLINK_EMD
	    && cmd != UMSDOS_UNLINK_DOS
	    && cmd != UMSDOS_RMDIR_DOS
	    && cmd != UMSDOS_STAT_DOS
	    && cmd != UMSDOS_DOS_SETUP)
		return fat_dir_ioctl (dir, filp, cmd, data_ptr);

	/* #Specification: ioctl / access
	 * Only root (effective id) is allowed to do IOCTL on directory
	 * in UMSDOS. EPERM is returned for other user.
	 */
	/*
	 * Well, not all cases require write access, but it simplifies
	 * the code, and let's face it, there is only one client (umssync)
	 * for all this.
	 */
	ret = verify_area (VERIFY_WRITE, (void *) data_ptr, 
				sizeof (struct umsdos_ioctl));
	if (ret < 0)
		goto out;

	ret = -EPERM;
	if (current->euid != 0 && cmd != UMSDOS_GETVERSION)
		goto out;

	ret = -EINVAL;
	if (cmd == UMSDOS_GETVERSION) {
		/* #Specification: ioctl / UMSDOS_GETVERSION
		 * The field version and release of the structure
		 * umsdos_ioctl are filled with the version and release
		 * number of the fs code in the kernel. This will allow
		 * some form of checking. Users won't be able to run
		 * incompatible utility such as the synchroniser (umssync).
		 * umsdos_progs/umsdosio.c enforce this checking.
		 * 
		 * Return always 0.
		 */
		put_user (UMSDOS_VERSION, &idata->version);
		put_user (UMSDOS_RELEASE, &idata->release);
		ret = 0;
		goto out;
	}
	if (cmd == UMSDOS_READDIR_DOS) {
		/* #Specification: ioctl / UMSDOS_READDIR_DOS
		 * One entry is read from the DOS directory at the current
		 * file position. The entry is put as is in the dos_dirent
		 * field of struct umsdos_ioctl.
		 * 
		 * Return > 0 if success.
		 */
		struct UMSDOS_DIR_ONCE bufk;

		bufk.count = 0;
		bufk.ent = &idata->dos_dirent;

		fat_readdir (filp, &bufk, umsdos_ioctl_fill);

		ret = bufk.count == 1 ? 1 : 0;
		goto out;
	}
	if (cmd == UMSDOS_READDIR_EMD) {
		/* #Specification: ioctl / UMSDOS_READDIR_EMD
		 * One entry is read from the EMD at the current
		 * file position. The entry is put as is in the umsdos_dirent
		 * field of struct umsdos_ioctl. The corresponding mangled
		 * DOS entry name is put in the dos_dirent field.
		 * 
		 * All entries are read including hidden links. Blank
		 * entries are skipped.
		 * 
		 * Return > 0 if success.
		 */
		struct dentry *demd;
		loff_t pos = filp->f_pos;

		/* The absence of the EMD is simply seen as an EOF */
		demd = umsdos_get_emd_dentry(dentry);
		ret = PTR_ERR(demd);
		if (IS_ERR(demd))
			goto out;
		ret = 0;
		if (!demd->d_inode)
			goto read_dput;

		while (pos < demd->d_inode->i_size) {
			off_t f_pos = pos;
			struct umsdos_dirent entry;
			struct umsdos_info info;

			ret = umsdos_emd_dir_readentry (demd, &pos, &entry);

			if (ret == -ENAMETOOLONG) {
				printk (KERN_INFO "Fixing EMD entry with invalid size -- zeroing out\n");
				memset (&info, 0, sizeof (info));
				info.f_pos = f_pos;
				info.recsize = UMSDOS_REC_SIZE;
				ret = umsdos_writeentry (dentry, &info, 1);
				continue;
			}

			if (ret)
				break;
			if (entry.name_len <= 0)
				continue;

			umsdos_parse (entry.name, entry.name_len, &info);
			info.f_pos = f_pos;
			umsdos_manglename (&info);
			ret = -EFAULT;
			if (copy_to_user (&idata->umsdos_dirent, &entry,
							sizeof (entry)))
				break;
			if (copy_to_user (&idata->dos_dirent.d_name,
							info.fake.fname,
				 			info.fake.len + 1))
				break;
			ret = entry.name_len;
			break;
		}
		/* update the original f_pos */
		filp->f_pos = pos;
	read_dput:
		d_drop(demd);
		dput(demd);
		goto out;
	}
	if (cmd == UMSDOS_INIT_EMD) {
		/* #Specification: ioctl / UMSDOS_INIT_EMD
		 * The UMSDOS_INIT_EMD command makes sure the EMD
		 * exists for a directory. If it does not, it is
		 * created. Also, it makes sure the directory function
		 * table (struct inode_operations) is set to the UMSDOS
		 * semantic. This mean that umssync may be applied to
		 * an "opened" msdos directory, and it will change behavior
		 * on the fly.
		 * 
		 * Return 0 if success.
		 */

		ret = umsdos_make_emd(dentry);
Printk(("UMSDOS_ioctl_dir: INIT_EMD %s/%s, ret=%d\n",
dentry->d_parent->d_name.name, dentry->d_name.name, ret));
		umsdos_setup_dir (dentry);
		goto out;
	}

	ret = -EFAULT;
	if (copy_from_user (&data, idata, sizeof (data)))
		goto out;

	if (cmd == UMSDOS_CREAT_EMD) {
		/* #Specification: ioctl / UMSDOS_CREAT_EMD
		 * The umsdos_dirent field of the struct umsdos_ioctl is used
		 * as is to create a new entry in the EMD of the directory.
		 * The DOS directory is not modified.
		 * No validation is done (yet).
		 * 
		 * Return 0 if success.
		 */
		struct umsdos_info info;

		/* This makes sure info.entry and info in general
		 * is correctly initialised
		 */
		memcpy (&info.entry, &data.umsdos_dirent,
			sizeof (data.umsdos_dirent));
		umsdos_parse (data.umsdos_dirent.name
		    ,data.umsdos_dirent.name_len, &info);
		ret = umsdos_newentry (dentry, &info);
		goto out;
	}
	else if (cmd == UMSDOS_RENAME_DOS) {
		struct dentry *old_dentry, *new_dentry;		/* FIXME */

		/* #Specification: ioctl / UMSDOS_RENAME_DOS
		 * A file or directory is renamed in a DOS directory
		 * (not moved across directory). The source name
		 * is in the dos_dirent.name field and the destination
		 * is in umsdos_dirent.name field.
		 * 
		 * This ioctl allows umssync to rename a mangled file
		 * name before syncing it back in the EMD.
		 */
		old_dentry = umsdos_lookup_dentry (dentry, 
						data.dos_dirent.d_name,
						data.dos_dirent.d_reclen ,1);
		ret = PTR_ERR(old_dentry);
		if (IS_ERR(old_dentry))
			goto out;
		new_dentry = umsdos_lookup_dentry (dentry,
						data.umsdos_dirent.name,
						data.umsdos_dirent.name_len, 1);
		ret = PTR_ERR(new_dentry);
		if (!IS_ERR(new_dentry)) {
printk("umsdos_ioctl: renaming %s/%s to %s/%s\n",
old_dentry->d_parent->d_name.name, old_dentry->d_name.name,
new_dentry->d_parent->d_name.name, new_dentry->d_name.name);
			ret = msdos_rename (dir, old_dentry, dir, new_dentry);
			d_drop(new_dentry);
			d_drop(old_dentry);
			dput(new_dentry);
		}
		dput(old_dentry);
		goto out;
	}
	else if (cmd == UMSDOS_UNLINK_EMD) {
		/* #Specification: ioctl / UMSDOS_UNLINK_EMD
		 * The umsdos_dirent field of the struct umsdos_ioctl is used
		 * as is to remove an entry from the EMD of the directory.
		 * No validation is done (yet). The mode field is used
		 * to validate S_ISDIR or S_ISREG.
		 * 
		 * Return 0 if success.
		 */
		struct umsdos_info info;

		/* This makes sure info.entry and info in general
		 * is correctly initialised
		 */
		memcpy (&info.entry, &data.umsdos_dirent,
			sizeof (data.umsdos_dirent));
		umsdos_parse (data.umsdos_dirent.name,
				data.umsdos_dirent.name_len, &info);
		ret = umsdos_delentry (dentry, &info,
				S_ISDIR (data.umsdos_dirent.mode));
		if (ret) {
			printk(KERN_WARNING
				"umsdos_ioctl: delentry %s/%s failed, ret=%d\n",
				dentry->d_name.name, info.entry.name, ret);
		}
		goto out;
	}
	else if (cmd == UMSDOS_UNLINK_DOS) {
		struct dentry *temp;

		/* #Specification: ioctl / UMSDOS_UNLINK_DOS
		 * The dos_dirent field of the struct umsdos_ioctl is used to
		 * execute a msdos_unlink operation. The d_name and d_reclen
		 * fields are used.
		 * 
		 * Return 0 if success.
		 */
		temp = umsdos_lookup_dentry(dentry, data.dos_dirent.d_name,
						data.dos_dirent.d_reclen, 1);
		ret = PTR_ERR(temp);
		if (IS_ERR(temp))
			goto out;
		ret = -ENOENT;
		if (temp->d_inode) {
			ret = -EISDIR;
			if (!S_ISDIR(temp->d_inode->i_mode))
				ret = msdos_unlink (dir, temp);
			if (!ret)
				d_delete(temp);
		}
		dput (temp);
		goto out;
	}
	else if (cmd == UMSDOS_RMDIR_DOS) {
		struct dentry *temp;

		/* #Specification: ioctl / UMSDOS_RMDIR_DOS
		 * The dos_dirent field of the struct umsdos_ioctl is used to
		 * execute a msdos_rmdir operation. The d_name and d_reclen
		 * fields are used.
		 * 
		 * Return 0 if success.
		 */
		temp = umsdos_lookup_dentry(dentry, data.dos_dirent.d_name,
					    data.dos_dirent.d_reclen, 1);
		ret = PTR_ERR(temp);
		if (IS_ERR(temp))
			goto out;
		ret = -ENOENT;
		if (temp->d_inode) {
			ret = -ENOTDIR;
			if (S_ISDIR(temp->d_inode->i_mode))
				ret = msdos_rmdir (dir, temp);
			if (!ret)
				d_delete(temp);
		}
		dput (temp);
		goto out;

	} else if (cmd == UMSDOS_STAT_DOS) {
		/* #Specification: ioctl / UMSDOS_STAT_DOS
		 * The dos_dirent field of the struct umsdos_ioctl is
		 * used to execute a stat operation in the DOS directory.
		 * The d_name and d_reclen fields are used.
		 * 
		 * The following field of umsdos_ioctl.stat are filled.
		 * 
		 * st_ino,st_mode,st_size,st_atime,st_mtime,st_ctime,
		 * Return 0 if success.
		 */
		struct dentry *dret;
		struct inode *inode;

		dret = umsdos_lookup_dentry(dentry, data.dos_dirent.d_name,
					    data.dos_dirent.d_reclen, 1);
		ret = PTR_ERR(dret);
		if (IS_ERR(dret))
			goto out;
		ret = -ENOENT;
		inode = dret->d_inode;
		if (inode) {
			data.stat.st_ino = inode->i_ino;
			data.stat.st_mode = inode->i_mode;
			data.stat.st_size = inode->i_size;
			data.stat.st_atime = inode->i_atime;
			data.stat.st_ctime = inode->i_ctime;
			data.stat.st_mtime = inode->i_mtime;
			ret = -EFAULT;
			if (!copy_to_user (&idata->stat, &data.stat, 
						sizeof (data.stat)))
				ret = 0;
		}
		dput(dret);
		goto out;
	}
	else if (cmd == UMSDOS_DOS_SETUP) {
		/* #Specification: ioctl / UMSDOS_DOS_SETUP
		 * The UMSDOS_DOS_SETUP ioctl allow changing the
		 * default permission of the MS-DOS filesystem driver
		 * on the fly.  The MS-DOS driver applies global permissions
		 * to every file and directory. Normally these permissions
		 * are controlled by a mount option. This is not
		 * available for root partition, so a special utility
		 * (umssetup) is provided to do this, normally in
		 * /etc/rc.local.
		 * 
		 * Be aware that this applies ONLY to MS-DOS directories
		 * (those without EMD --linux-.---). Umsdos directory
		 * have independent (standard) permission for each
		 * and every file.
		 * 
		 * The field umsdos_dirent provide the information needed.
		 * umsdos_dirent.uid and gid sets the owner and group.
		 * umsdos_dirent.mode set the permissions flags.
		 */
		dir->i_sb->u.msdos_sb.options.fs_uid = data.umsdos_dirent.uid;
		dir->i_sb->u.msdos_sb.options.fs_gid = data.umsdos_dirent.gid;
		dir->i_sb->u.msdos_sb.options.fs_umask = data.umsdos_dirent.mode;
		ret = 0;
	}
out:
	Printk (("ioctl %d, returning %d\n", cmd, ret));
	return ret;
}
