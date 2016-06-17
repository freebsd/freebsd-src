/*
 *  linux/fs/umsdos/emd.c
 *
 *  Written 1993 by Jacques Gelinas
 *
 *  Extended MS-DOS directory handling functions
 */

#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/msdos_fs.h>
#include <linux/umsdos_fs.h>
#include <linux/dcache.h>
#include <linux/pagemap.h>
#include <linux/delay.h>

void put_entry (struct umsdos_dirent *p, struct umsdos_dirent *q)
{
	p->name_len = q->name_len;
	p->flags = q->flags;
	p->nlink = cpu_to_le16(q->nlink);
	p->uid = cpu_to_le16(q->uid);
	p->gid = cpu_to_le16(q->gid);
	p->atime = cpu_to_le32(q->atime);
	p->mtime = cpu_to_le32(q->mtime);
	p->ctime = cpu_to_le32(q->ctime);
	p->rdev = cpu_to_le16(q->rdev);
	p->mode = cpu_to_le16(q->mode);
}

static void get_entry(struct umsdos_dirent *p, struct umsdos_dirent *q)
{
	p->name_len = q->name_len;
	p->name[p->name_len]='\0';
	p->flags = q->flags;
	p->nlink = le16_to_cpu (q->nlink);
	/* FIXME -- 32bit UID/GID issues */
	p->uid = le16_to_cpu (q->uid);
	p->gid = le16_to_cpu (q->gid);
	p->atime = le32_to_cpu (q->atime);
	p->mtime = le32_to_cpu (q->mtime);
	p->ctime = le32_to_cpu (q->ctime);
	p->rdev = le16_to_cpu (q->rdev);
	p->mode = le16_to_cpu (q->mode);
}

/*
 * Lookup the EMD dentry for a directory.
 *
 * Note: the caller must hold a lock on the parent directory.
 */
struct dentry *umsdos_get_emd_dentry(struct dentry *parent)
{
	struct dentry *demd;

	demd = umsdos_lookup_dentry(parent, UMSDOS_EMD_FILE, 
					UMSDOS_EMD_NAMELEN, 1);
	return demd;
}

/*
 * Check whether a directory has an EMD file.
 *
 * Note: the caller must hold a lock on the parent directory.
 */
int umsdos_have_emd(struct dentry *dir)
{
	struct dentry *demd = umsdos_get_emd_dentry (dir);
	int found = 0;

	if (!IS_ERR(demd)) {
		if (demd->d_inode)
			found = 1;
		dput(demd);
	}
	return found;
}

/*
 * Create the EMD file for a directory if it doesn't
 * already exist. Returns 0 or an error code.
 *
 * Note: the caller must hold a lock on the parent directory.
 */
int umsdos_make_emd(struct dentry *parent)
{
	struct dentry *demd = umsdos_get_emd_dentry(parent);
	int err = PTR_ERR(demd);

	if (IS_ERR(demd)) {
		printk("umsdos_make_emd: can't get dentry in %s, err=%d\n",
			parent->d_name.name, err);
		goto out;
	}

	/* already created? */
	err = 0;
	if (demd->d_inode)
		goto out_set;

Printk(("umsdos_make_emd: creating EMD %s/%s\n",
parent->d_name.name, demd->d_name.name));

	err = msdos_create(parent->d_inode, demd, S_IFREG | 0777);
	if (err) {
		printk (KERN_WARNING
			"umsdos_make_emd: create %s/%s failed, err=%d\n",
			parent->d_name.name, demd->d_name.name, err);
	}
out_set:
	dput(demd);
out:
	return err;
}


/*
 * Read an entry from the EMD file.
 * Support variable length record.
 * Return -EIO if error, 0 if OK.
 *
 * does not change {d,i}_count
 */

int umsdos_emd_dir_readentry (struct dentry *demd, loff_t *pos, struct umsdos_dirent *entry)
{
	struct address_space *mapping = demd->d_inode->i_mapping;
	struct page *page;
	struct umsdos_dirent *p;
	int offs = *pos & ~PAGE_CACHE_MASK;
	int recsize;
	int ret = 0;

	page = read_cache_page(mapping, *pos>>PAGE_CACHE_SHIFT,
			(filler_t*)mapping->a_ops->readpage, NULL);
	if (IS_ERR(page))
		goto sync_fail;
	wait_on_page(page);
	if (!Page_Uptodate(page))
		goto async_fail;
	p = (struct umsdos_dirent*)(kmap(page)+offs);

	/* if this is an invalid entry (invalid name length), ignore it */
	if( p->name_len > UMSDOS_MAXNAME )
	{
		printk (KERN_WARNING "Ignoring invalid EMD entry with size %d\n", entry->name_len);
		p->name_len = 0; 
		ret = -ENAMETOOLONG; /* notify umssync(8) code that something is wrong */
		/* FIXME: does not work if we did 'ls -l' before 'udosctl uls' ?! */
	}

	recsize = umsdos_evalrecsize(p->name_len);
	if (offs + recsize > PAGE_CACHE_SIZE) {
		struct page *page2;
		int part = (char *)(page_address(page) + PAGE_CACHE_SIZE) - p->spare;
		page2 = read_cache_page(mapping, 1+(*pos>>PAGE_CACHE_SHIFT),
				(filler_t*)mapping->a_ops->readpage, NULL);
		if (IS_ERR(page2)) {
			kunmap(page);
			page_cache_release(page);
			page = page2;
			goto sync_fail;
		}
		wait_on_page(page2);
		if (!Page_Uptodate(page2)) {
			kunmap(page);
			page_cache_release(page2);
			goto async_fail;
		}
		memcpy(entry->spare,p->spare,part);
		memcpy(entry->spare+part,kmap(page2),
				recsize+offs-PAGE_CACHE_SIZE);
		kunmap(page2);
		page_cache_release(page2);
	} else
		memcpy(entry->spare,p->spare,((char*)p+recsize)-p->spare);
	get_entry(entry, p);
	kunmap(page);
	page_cache_release(page);
	*pos += recsize;
	return ret;
async_fail:
	page_cache_release(page);
	page = ERR_PTR(-EIO);
sync_fail:
	return PTR_ERR(page);
}


/*
 * Write an entry in the EMD file.
 * Return 0 if OK, -EIO if some error.
 *
 * Note: the caller must hold a lock on the parent directory.
 */
int umsdos_writeentry (struct dentry *parent, struct umsdos_info *info,
				int free_entry)
{
	struct inode *dir = parent->d_inode;
	struct umsdos_dirent *entry = &info->entry;
	struct dentry *emd_dentry;
	int ret;
	struct umsdos_dirent entry0,*p;
	struct address_space *mapping;
	struct page *page, *page2 = NULL;
	int offs;

	emd_dentry = umsdos_get_emd_dentry(parent);
	ret = PTR_ERR(emd_dentry);
	if (IS_ERR(emd_dentry))
		goto out;
	/* make sure there's an EMD file */
	ret = -EIO;
	if (!emd_dentry->d_inode) {
		printk(KERN_WARNING
			"umsdos_writeentry: no EMD file in %s/%s\n",
			parent->d_parent->d_name.name, parent->d_name.name);
		goto out_dput;
	}

	if (free_entry) {
		/* #Specification: EMD file / empty entries
		 * Unused entries in the EMD file are identified
		 * by the name_len field equal to 0. However to
		 * help future extension (or bug correction :-( ),
		 * empty entries are filled with 0.
		 */
		memset (&entry0, 0, sizeof (entry0));
		entry = &entry0;
	} else if (entry->name_len > 0) {
		memset (entry->name + entry->name_len, '\0', 
			sizeof (entry->name) - entry->name_len);
		/* #Specification: EMD file / spare bytes
		 * 10 bytes are unused in each record of the EMD. They
		 * are set to 0 all the time, so it will be possible
		 * to do new stuff and rely on the state of those
		 * bytes in old EMD files.
		 */
		memset (entry->spare, 0, sizeof (entry->spare));
	}

	/* write the entry and update the parent timestamps */
	mapping = emd_dentry->d_inode->i_mapping;
	offs = info->f_pos & ~PAGE_CACHE_MASK;
	ret = -ENOMEM;
	page = grab_cache_page(mapping, info->f_pos>>PAGE_CACHE_SHIFT);
	if (!page)
		goto out_dput;
	p = (struct umsdos_dirent *) (page_address(page) + offs);
	if (offs + info->recsize > PAGE_CACHE_SIZE) {
		ret = mapping->a_ops->prepare_write(NULL,page,offs,
					PAGE_CACHE_SIZE);
		if (ret)
			goto out_unlock;
		page2 = grab_cache_page(mapping,
					(info->f_pos>>PAGE_CACHE_SHIFT)+1);
		if (!page2)
			goto out_unlock2;
		ret = mapping->a_ops->prepare_write(NULL,page2,0,
					offs+info->recsize-PAGE_CACHE_SIZE);
		if (ret)
			goto out_unlock3;
		put_entry (p, entry);
		memcpy(p->spare,entry->spare,
			(char *)(page_address(page) + PAGE_CACHE_SIZE) - p->spare);
		memcpy(page_address(page2),
				((char*)entry)+PAGE_CACHE_SIZE-offs,
				offs+info->recsize-PAGE_CACHE_SIZE);
		ret = mapping->a_ops->commit_write(NULL,page2,0,
					offs+info->recsize-PAGE_CACHE_SIZE);
		if (ret)
			goto out_unlock3;
		ret = mapping->a_ops->commit_write(NULL,page,offs,
					PAGE_CACHE_SIZE);
		UnlockPage(page2);
		page_cache_release(page2);
		if (ret)
			goto out_unlock;
	} else {
		ret = mapping->a_ops->prepare_write(NULL,page,offs,
					offs + info->recsize);
		if (ret)
			goto out_unlock;
		put_entry (p, entry);
		memcpy(p->spare,entry->spare,((char*)p+info->recsize)-p->spare);
		ret = mapping->a_ops->commit_write(NULL,page,offs,
					offs + info->recsize);
		if (ret)
			goto out_unlock;
	}
	UnlockPage(page);
	page_cache_release(page);
		
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);

out_dput:
	dput(emd_dentry);
out:
	Printk (("umsdos_writeentry /mn/: returning %d...\n", ret));
	return ret;
out_unlock3:
	UnlockPage(page2);
	page_cache_release(page2);
out_unlock2:
	ClearPageUptodate(page);
	kunmap(page);
out_unlock:
	UnlockPage(page);
	page_cache_release(page);
	printk ("UMSDOS:  problem with EMD file:  can't write\n");
	goto out_dput;
}

/*
 * General search, locate a name in the EMD file or an empty slot to
 * store it. if info->entry.name_len == 0, search the first empty
 * slot (of the proper size).
 * 
 * Return 0 if found, -ENOENT if not found, another error code if
 * other problem.
 * 
 * So this routine is used to either find an existing entry or to
 * create a new one, while making sure it is a new one. After you
 * get -ENOENT, you make sure the entry is stuffed correctly and
 * call umsdos_writeentry().
 * 
 * To delete an entry, you find it, zero out the entry (memset)
 * and call umsdos_writeentry().
 * 
 * All this to say that umsdos_writeentry must be called after this
 * function since it relies on the f_pos field of info.
 *
 * Note: the caller must hold a lock on the parent directory.
 */
/* #Specification: EMD file structure
 * The EMD file uses a fairly simple layout.  It is made of records
 * (UMSDOS_REC_SIZE == 64).  When a name can't be written in a single
 * record, multiple contiguous records are allocated.
 */

static int umsdos_find (struct dentry *demd, struct umsdos_info *info)
{
	struct umsdos_dirent *entry = &info->entry;
	int recsize = info->recsize;
	struct inode *emd_dir;
	int ret = -ENOENT;
	struct {
		off_t posok;	/* Position available to store the entry */
		off_t one;	/* One empty position -> maybe <- large enough */
	} empty;
	int found = 0;
	int empty_size = 0;
	struct address_space *mapping;
	filler_t *readpage;
	struct page *page = NULL;
	int index = -1;
	int offs = PAGE_CACHE_SIZE,max_offs = PAGE_CACHE_SIZE;
	char *p = NULL;
	loff_t pos = 0;

	/* make sure there's an EMD file ... */
	ret = -ENOENT;
	emd_dir = demd->d_inode;
	if (!emd_dir)
		goto out_dput;
	mapping = emd_dir->i_mapping;
	readpage = (filler_t*)mapping->a_ops->readpage;

	empty.posok = emd_dir->i_size;
	while (1) {
		struct umsdos_dirent *rentry;
		int entry_size;

		if (offs >= max_offs) {
			if (page) {
				kunmap(page);
				page_cache_release(page);
				page = NULL;
			}
			if (pos >= emd_dir->i_size) {
				info->f_pos = empty.posok;
				break;
			}
			if (++index == (emd_dir->i_size>>PAGE_CACHE_SHIFT))
				max_offs = emd_dir->i_size & ~PAGE_CACHE_MASK;
			offs -= PAGE_CACHE_SIZE;
			page = read_cache_page(mapping,index,readpage,NULL);
			if (IS_ERR(page))
				goto sync_fail;
			wait_on_page(page);
			if (!Page_Uptodate(page))
				goto async_fail;
			p = kmap(page);
		}

		rentry = (struct umsdos_dirent *)(p+offs);

		if (rentry->name_len == 0) {
			/* We are looking for an empty section at least */
			/* as large as recsize. */
			if (entry->name_len == 0) {
				info->f_pos = pos;
				ret = 0;
				break;
			}
			offs += UMSDOS_REC_SIZE;
			pos += UMSDOS_REC_SIZE;
			if (found)
				continue;
			if (!empty_size)
				empty.one = pos-UMSDOS_REC_SIZE;
			empty_size += UMSDOS_REC_SIZE;
			if (empty_size == recsize) {
				/* Here is a large enough section. */
				empty.posok = empty.one;
				found = 1;
			}
			continue;
		}

		entry_size = umsdos_evalrecsize(rentry->name_len);
		if (entry_size > PAGE_CACHE_SIZE)
			goto async_fail;
		empty_size = 0;
		if (entry->name_len != rentry->name_len)
			goto skip_it;

		if (entry_size + offs > PAGE_CACHE_SIZE) {
			/* Sucker spans the page boundary */
			int len = (p+PAGE_CACHE_SIZE)-rentry->name;
			struct page *next_page;
			char *q;
			next_page = read_cache_page(mapping,index+1,readpage,NULL);
			if (IS_ERR(next_page)) {
				page_cache_release(page);
				page = next_page;
				goto sync_fail;
			}
			wait_on_page(next_page);
			if (!Page_Uptodate(next_page)) {
				page_cache_release(page);
				page = next_page;
				goto async_fail;
			}
			q = kmap(next_page);
			if (memcmp(entry->name, rentry->name, len) ||
			    memcmp(entry->name+len, q, entry->name_len-len)) {
				kunmap(next_page);
				page_cache_release(next_page);
				goto skip_it;
			}
			kunmap(next_page);
			page_cache_release(next_page);
		} else if (memcmp (entry->name, rentry->name, entry->name_len))
			goto skip_it;

		info->f_pos = pos;
		get_entry(entry, rentry);
		ret = 0;
		break;
skip_it:
		offs+=entry_size;
		pos+=entry_size;
	}
	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
	umsdos_manglename (info);

out_dput:
	dput(demd);
	return ret;

async_fail:
	page_cache_release(page);
	page = ERR_PTR(-EIO);
sync_fail:
	return PTR_ERR(page);
}


/*
 * Add a new entry in the EMD file.
 * Return 0 if OK or a negative error code.
 * Return -EEXIST if the entry already exists.
 *
 * Complete the information missing in info.
 * 
 * N.B. What if the EMD file doesn't exist?
 */

int umsdos_newentry (struct dentry *parent, struct umsdos_info *info)
{
	int err, ret = -EEXIST;
	struct dentry *demd = umsdos_get_emd_dentry(parent);

	ret = PTR_ERR(demd);
	if (IS_ERR(demd))
		goto out;
	err = umsdos_find (demd, info);
	if (err && err == -ENOENT) {
		ret = umsdos_writeentry (parent, info, 0);
		Printk (("umsdos_writeentry EMD ret = %d\n", ret));
	}
out:
	return ret;
}


/*
 * Create a new hidden link.
 * Return 0 if OK, an error code if not.
 */

/* #Specification: hard link / hidden name
 * When a hard link is created, the original file is renamed
 * to a hidden name. The name is "..LINKNNN" where NNN is a
 * number define from the entry offset in the EMD file.
 */
int umsdos_newhidden (struct dentry *parent, struct umsdos_info *info)
{
	int ret;
	struct dentry *demd = umsdos_get_emd_dentry(parent);
	ret = PTR_ERR(demd);
	if (IS_ERR(demd))
		goto out;

	umsdos_parse ("..LINK", 6, info);
	info->entry.name_len = 0;
	ret = umsdos_find (demd, info);
	if (ret == -ENOENT || ret == 0) {
		info->entry.name_len = sprintf (info->entry.name,
						"..LINK%ld", info->f_pos);
		ret = 0;
	}
out:
	return ret;
}


/*
 * Remove an entry from the EMD file.
 * Return 0 if OK, a negative error code otherwise.
 * 
 * Complete the information missing in info.
 */

int umsdos_delentry (struct dentry *parent, struct umsdos_info *info, int isdir)
{
	int ret;
	struct dentry *demd = umsdos_get_emd_dentry(parent);

	ret = PTR_ERR(demd);
	if (IS_ERR(demd))
		goto out;
	ret = umsdos_find (demd, info);
	if (ret)
		goto out;
	if (info->entry.name_len == 0)
		goto out;

	if ((isdir != 0) != (S_ISDIR (info->entry.mode) != 0)) {
		if (S_ISDIR (info->entry.mode)) {
			ret = -EISDIR;
		} else {
			ret = -ENOTDIR;
		}
		goto out;
	}
	ret = umsdos_writeentry (parent, info, 1);

out:
	return ret;
}


/*
 * Verify that an EMD directory is empty.
 * Return: 
 * 0 if not empty,
 * 1 if empty (except for EMD file),
 * 2 if empty or no EMD file.
 */

int umsdos_isempty (struct dentry *dentry)
{
	struct dentry *demd;
	int ret = 2;
	loff_t pos = 0;

	demd = umsdos_get_emd_dentry(dentry);
	if (IS_ERR(demd))
		goto out;
	/* If the EMD file does not exist, it is certainly empty. :-) */
	if (!demd->d_inode)
		goto out_dput;

	ret = 1;
	while (pos < demd->d_inode->i_size) {
		struct umsdos_dirent entry;

		if (umsdos_emd_dir_readentry (demd, &pos, &entry) != 0) {
			ret = 0;
			break;
		}
		if (entry.name_len != 0) {
			ret = 0;
			break;
		}
	}

out_dput:
	dput(demd);
out:
	return ret;
}

/*
 * Locate an entry in a EMD directory.
 * Return 0 if OK, error code if not, generally -ENOENT.
 *
 * expect argument:
 * 	0: anything
 * 	1: file
 * 	2: directory
 */

int umsdos_findentry (struct dentry *parent, struct umsdos_info *info,
			int expect)
{		
	int ret;
	struct dentry *demd = umsdos_get_emd_dentry(parent);

	ret = PTR_ERR(demd);
	if (IS_ERR(demd))
		goto out;
	ret = umsdos_find (demd, info);
	if (ret)
		goto out;

	switch (expect) {
	case 1:
		if (S_ISDIR (info->entry.mode))
			ret = -EISDIR;
		break;
	case 2:
		if (!S_ISDIR (info->entry.mode))
			ret = -ENOTDIR;
	}

out:
	Printk (("umsdos_findentry: returning %d\n", ret));
	return ret;
}
