/*
 *  linux/fs/minix/dir.c
 *
 *  Copyright (C) 1991, 1992 Linus Torvalds
 *
 *  minix directory handling functions
 */

#include <linux/fs.h>
#include <linux/minix_fs.h>
#include <linux/pagemap.h>

typedef struct minix_dir_entry minix_dirent;

static int minix_readdir(struct file *, void *, filldir_t);

struct file_operations minix_dir_operations = {
	read:		generic_read_dir,
	readdir:	minix_readdir,
	fsync:		minix_sync_file,
};

static inline void dir_put_page(struct page *page)
{
	kunmap(page);
	page_cache_release(page);
}

static inline unsigned long dir_pages(struct inode *inode)
{
	return (inode->i_size+PAGE_CACHE_SIZE-1)>>PAGE_CACHE_SHIFT;
}

static int dir_commit_chunk(struct page *page, unsigned from, unsigned to)
{
	struct inode *dir = (struct inode *)page->mapping->host;
	int err = 0;
	page->mapping->a_ops->commit_write(NULL, page, from, to);
	if (IS_SYNC(dir)) {
		int err2;
		err = writeout_one_page(page);
		err2 = waitfor_one_page(page);
		if (err == 0)
			err = err2;
	}
	return err;
}

static struct page * dir_get_page(struct inode *dir, unsigned long n)
{
	struct address_space *mapping = dir->i_mapping;
	struct page *page = read_cache_page(mapping, n,
				(filler_t*)mapping->a_ops->readpage, NULL);
	if (!IS_ERR(page)) {
		wait_on_page(page);
		kmap(page);
		if (!Page_Uptodate(page))
			goto fail;
	}
	return page;

fail:
	dir_put_page(page);
	return ERR_PTR(-EIO);
}

static inline void *minix_next_entry(void *de, struct minix_sb_info *sbi)
{
	return (void*)((char*)de + sbi->s_dirsize);
}

static int minix_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	unsigned long pos = filp->f_pos;
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	unsigned offset = pos & ~PAGE_CACHE_MASK;
	unsigned long n = pos >> PAGE_CACHE_SHIFT;
	unsigned long npages = dir_pages(inode);
	struct minix_sb_info *sbi = &sb->u.minix_sb;
	unsigned chunk_size = sbi->s_dirsize;

	pos = (pos + chunk_size-1) & ~(chunk_size-1);
	if (pos >= inode->i_size)
		goto done;

	for ( ; n < npages; n++, offset = 0) {
		char *p, *kaddr, *limit;
		struct page *page = dir_get_page(inode, n);

		if (IS_ERR(page))
			continue;
		kaddr = (char *)page_address(page);
		p = kaddr+offset;
		limit = kaddr + PAGE_CACHE_SIZE - chunk_size;
		for ( ; p <= limit ; p = minix_next_entry(p, sbi)) {
			minix_dirent *de = (minix_dirent *)p;
			if (de->inode) {
				int over;
				unsigned l = strnlen(de->name,sbi->s_namelen);

				offset = p - kaddr;
				over = filldir(dirent, de->name, l,
						(n<<PAGE_CACHE_SHIFT) | offset,
						de->inode, DT_UNKNOWN);
				if (over) {
					dir_put_page(page);
					goto done;
				}
			}
		}
		dir_put_page(page);
	}

done:
	filp->f_pos = (n << PAGE_CACHE_SHIFT) | offset;
	UPDATE_ATIME(inode);
	return 0;
}

static inline int namecompare(int len, int maxlen,
	const char * name, const char * buffer)
{
	if (len < maxlen && buffer[len])
		return 0;
	return !memcmp(name, buffer, len);
}

/*
 *	minix_find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself (as a parameter - res_dir). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 */
minix_dirent *minix_find_entry(struct dentry *dentry, struct page **res_page)
{
	const char * name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct inode * dir = dentry->d_parent->d_inode;
	struct super_block * sb = dir->i_sb;
	struct minix_sb_info * sbi = &sb->u.minix_sb;
	unsigned long n;
	unsigned long npages = dir_pages(dir);
	struct page *page = NULL;
	struct minix_dir_entry *de;

	*res_page = NULL;

	for (n = 0; n < npages; n++) {
		char *kaddr;
		page = dir_get_page(dir, n);
		if (IS_ERR(page))
			continue;

		kaddr = (char*)page_address(page);
		de = (struct minix_dir_entry *) kaddr;
		kaddr += PAGE_CACHE_SIZE - sbi->s_dirsize;
		for ( ; (char *) de <= kaddr ; de = minix_next_entry(de,sbi)) {
			if (!de->inode)
				continue;
			if (namecompare(namelen,sbi->s_namelen,name,de->name))
				goto found;
		}
		dir_put_page(page);
	}
	return NULL;

found:
	*res_page = page;
	return de;
}

int minix_add_link(struct dentry *dentry, struct inode *inode)
{
	struct inode *dir = dentry->d_parent->d_inode;
	const char * name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct super_block * sb = dir->i_sb;
	struct minix_sb_info * sbi = &sb->u.minix_sb;
	struct page *page = NULL;
	struct minix_dir_entry * de;
	unsigned long npages = dir_pages(dir);
	unsigned long n;
	char *kaddr;
	unsigned from, to;
	int err;

	/* We take care of directory expansion in the same loop */
	for (n = 0; n <= npages; n++) {
		page = dir_get_page(dir, n);
		err = PTR_ERR(page);
		if (IS_ERR(page))
			goto out;
		kaddr = (char*)page_address(page);
		de = (minix_dirent *)kaddr;
		kaddr += PAGE_CACHE_SIZE - sbi->s_dirsize;
		while ((char *)de <= kaddr) {
			if (!de->inode)
				goto got_it;
			err = -EEXIST;
			if (namecompare(namelen,sbi->s_namelen,name,de->name))
				goto out_page;
			de = minix_next_entry(de, sbi);
		}
		dir_put_page(page);
	}
	BUG();
	return -EINVAL;

got_it:
	from = (char*)de - (char*)page_address(page);
	to = from + sbi->s_dirsize;
	lock_page(page);
	err = page->mapping->a_ops->prepare_write(NULL, page, from, to);
	if (err)
		goto out_unlock;
	memcpy (de->name, name, namelen);
	memset (de->name + namelen, 0, sbi->s_dirsize - namelen - 2);
	de->inode = inode->i_ino;
	err = dir_commit_chunk(page, from, to);
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	mark_inode_dirty(dir);
out_unlock:
	UnlockPage(page);
out_page:
	dir_put_page(page);
out:
	return err;
}

int minix_delete_entry(struct minix_dir_entry *de, struct page *page)
{
	struct address_space *mapping = page->mapping;
	struct inode *inode = (struct inode*)mapping->host;
	char *kaddr = (char*)page_address(page);
	unsigned from = (char*)de - kaddr;
	unsigned to = from + inode->i_sb->u.minix_sb.s_dirsize;
	int err;

	lock_page(page);
	err = mapping->a_ops->prepare_write(NULL, page, from, to);
	if (err == 0) {
		de->inode = 0;
		err = dir_commit_chunk(page, from, to);
	}
	UnlockPage(page);
	dir_put_page(page);
	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	mark_inode_dirty(inode);
	return err;
}

int minix_make_empty(struct inode *inode, struct inode *dir)
{
	struct address_space *mapping = inode->i_mapping;
	struct page *page = grab_cache_page(mapping, 0);
	struct minix_sb_info * sbi = &inode->i_sb->u.minix_sb;
	struct minix_dir_entry * de;
	char *base;
	int err;

	if (!page)
		return -ENOMEM;
	err = mapping->a_ops->prepare_write(NULL, page, 0, 2 * sbi->s_dirsize);
	if (err)
		goto fail;

	base = (char*)page_address(page);
	memset(base, 0, PAGE_CACHE_SIZE);

	de = (struct minix_dir_entry *) base;
	de->inode = inode->i_ino;
	strcpy(de->name,".");
	de = minix_next_entry(de, sbi);
	de->inode = dir->i_ino;
	strcpy(de->name,"..");

	err = dir_commit_chunk(page, 0, 2 * sbi->s_dirsize);
fail:
	UnlockPage(page);
	page_cache_release(page);
	return err;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
int minix_empty_dir(struct inode * inode)
{
	struct page *page = NULL;
	unsigned long i, npages = dir_pages(inode);
	struct minix_sb_info *sbi = &inode->i_sb->u.minix_sb;

	for (i = 0; i < npages; i++) {
		char *kaddr;
		minix_dirent * de;
		page = dir_get_page(inode, i);

		if (IS_ERR(page))
			continue;

		kaddr = (char *)page_address(page);
		de = (minix_dirent *)kaddr;
		kaddr += PAGE_CACHE_SIZE - sbi->s_dirsize;

		while ((char *)de <= kaddr) {
			if (de->inode != 0) {
				/* check for . and .. */
				if (de->name[0] != '.')
					goto not_empty;
				if (!de->name[1]) {
					if (de->inode != inode->i_ino)
						goto not_empty;
				} else if (de->name[1] != '.')
					goto not_empty;
				else if (de->name[2])
					goto not_empty;
			}
			de = minix_next_entry(de, sbi);
		}
		dir_put_page(page);
	}
	return 1;

not_empty:
	dir_put_page(page);
	return 0;
}

/* Releases the page */
void minix_set_link(struct minix_dir_entry *de, struct page *page,
	struct inode *inode)
{
	struct inode *dir = (struct inode*)page->mapping->host;
	struct minix_sb_info *sbi = &dir->i_sb->u.minix_sb;
	unsigned from = (char *)de-(char*)page_address(page);
	unsigned to = from + sbi->s_dirsize;
	int err;

	lock_page(page);
	err = page->mapping->a_ops->prepare_write(NULL, page, from, to);
	if (err == 0) {
		de->inode = inode->i_ino;
		err = dir_commit_chunk(page, from, to);
	}
	UnlockPage(page);
	dir_put_page(page);
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	mark_inode_dirty(dir);
}

struct minix_dir_entry * minix_dotdot (struct inode *dir, struct page **p)
{
	struct page *page = dir_get_page(dir, 0);
	struct minix_sb_info *sbi = &dir->i_sb->u.minix_sb;
	struct minix_dir_entry *de = NULL;

	if (!IS_ERR(page)) {
		de = minix_next_entry(page_address(page), sbi);
		*p = page;
	}
	return de;
}

ino_t minix_inode_by_name(struct dentry *dentry)
{
	struct page *page;
	struct minix_dir_entry *de = minix_find_entry(dentry, &page);
	ino_t res = 0;

	if (de) {
		res = de->inode;
		dir_put_page(page);
	}
	return res;
}
