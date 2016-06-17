/*
 *	fs/bfs/dir.c
 *	BFS directory operations.
 *	Copyright (C) 1999,2000  Tigran Aivazian <tigran@veritas.com>
 */

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/bfs_fs.h>
#include <linux/locks.h>

#include "bfs_defs.h"

#undef DEBUG

#ifdef DEBUG
#define dprintf(x...)	printf(x)
#else
#define dprintf(x...)
#endif

static int bfs_add_entry(struct inode * dir, const char * name, int namelen, int ino);
static struct buffer_head * bfs_find_entry(struct inode * dir, 
	const char * name, int namelen, struct bfs_dirent ** res_dir);

static int bfs_readdir(struct file * f, void * dirent, filldir_t filldir)
{
	struct inode * dir = f->f_dentry->d_inode;
	struct buffer_head * bh;
	struct bfs_dirent * de;
	kdev_t dev = dir->i_dev;
	unsigned int offset;
	int block;

	if (f->f_pos & (BFS_DIRENT_SIZE-1)) {
		printf("Bad f_pos=%08lx for %s:%08lx\n", (unsigned long)f->f_pos, 
			bdevname(dev), dir->i_ino);
		return -EBADF;
	}

	while (f->f_pos < dir->i_size) {
		offset = f->f_pos & (BFS_BSIZE-1);
		block = dir->iu_sblock + (f->f_pos >> BFS_BSIZE_BITS);
		bh = sb_bread(dir->i_sb, block);
		if (!bh) {
			f->f_pos += BFS_BSIZE - offset;
			continue;
		}
		do {
			de = (struct bfs_dirent *)(bh->b_data + offset);
			if (de->ino) {
				int size = strnlen(de->name, BFS_NAMELEN);
				if (filldir(dirent, de->name, size, f->f_pos, de->ino, DT_UNKNOWN) < 0) {
					brelse(bh);
					return 0;
				}
			}
			offset += BFS_DIRENT_SIZE;
			f->f_pos += BFS_DIRENT_SIZE;
		} while (offset < BFS_BSIZE && f->f_pos < dir->i_size);
		brelse(bh);
	}

	UPDATE_ATIME(dir);
	return 0;	
}

struct file_operations bfs_dir_operations = {
	read:		generic_read_dir,
	readdir:	bfs_readdir,
	fsync:		file_fsync,
};

extern void dump_imap(const char *, struct super_block *);

static int bfs_create(struct inode * dir, struct dentry * dentry, int mode)
{
	int err;
	struct inode * inode;
	struct super_block * s = dir->i_sb;
	unsigned long ino;

	inode = new_inode(s);
	if (!inode)
		return -ENOSPC;
	ino = find_first_zero_bit(s->su_imap, s->su_lasti);
	if (ino > s->su_lasti) {
		iput(inode);
		return -ENOSPC;
	}
	set_bit(ino, s->su_imap);	
	s->su_freei--;
	inode->i_uid = current->fsuid;
	inode->i_gid = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_blocks = inode->i_blksize = 0;
	inode->i_op = &bfs_file_inops;
	inode->i_fop = &bfs_file_operations;
	inode->i_mapping->a_ops = &bfs_aops;
	inode->i_mode = mode;
	inode->i_ino = inode->iu_dsk_ino = ino;
	inode->iu_sblock = inode->iu_eblock = 0;
	insert_inode_hash(inode);
        mark_inode_dirty(inode);
	dump_imap("create",s);

	err = bfs_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode->i_ino);
	if (err) {
		inode->i_nlink--;
		mark_inode_dirty(inode);
		iput(inode);
		return err;
	}
	d_instantiate(dentry, inode);
	return 0;
}

static struct dentry * bfs_lookup(struct inode * dir, struct dentry * dentry)
{
	struct inode * inode = NULL;
	struct buffer_head * bh;
	struct bfs_dirent * de;

	if (dentry->d_name.len > BFS_NAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	bh = bfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len, &de);
	if (bh) {
		unsigned long ino = le32_to_cpu(de->ino);
		brelse(bh);
		inode = iget(dir->i_sb, ino);
		if (!inode)
			return ERR_PTR(-EACCES);
	}
	d_add(dentry, inode);
	return NULL;
}

static int bfs_link(struct dentry * old, struct inode * dir, struct dentry * new)
{
	struct inode * inode = old->d_inode;
	int err;

	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	err = bfs_add_entry(dir, new->d_name.name, new->d_name.len, inode->i_ino);
	if (err)
		return err;
	inode->i_nlink++;
	inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
	atomic_inc(&inode->i_count);
	d_instantiate(new, inode);
	return 0;
}


static int bfs_unlink(struct inode * dir, struct dentry * dentry)
{
	int error = -ENOENT;
	struct inode * inode;
	struct buffer_head * bh;
	struct bfs_dirent * de;

	inode = dentry->d_inode;
	bh = bfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len, &de);
	if (!bh || de->ino != inode->i_ino) 
		goto out_brelse;

	if (!inode->i_nlink) {
		printf("unlinking non-existent file %s:%lu (nlink=%d)\n", bdevname(inode->i_dev), 
				inode->i_ino, inode->i_nlink);
		inode->i_nlink = 1;
	}
	de->ino = 0;
	dir->i_version = ++event;
	mark_buffer_dirty(bh);
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);
	inode->i_nlink--;
	inode->i_ctime = dir->i_ctime;
	mark_inode_dirty(inode);
	error = 0;

out_brelse:
	brelse(bh);
	return error;
}

static int bfs_rename(struct inode * old_dir, struct dentry * old_dentry, 
			struct inode * new_dir, struct dentry * new_dentry)
{
	struct inode * old_inode, * new_inode;
	struct buffer_head * old_bh, * new_bh;
	struct bfs_dirent * old_de, * new_de;		
	int error = -ENOENT;

	old_bh = new_bh = NULL;
	old_inode = old_dentry->d_inode;
	if (S_ISDIR(old_inode->i_mode))
		return -EINVAL;

	old_bh = bfs_find_entry(old_dir, 
				old_dentry->d_name.name, 
				old_dentry->d_name.len, &old_de);

	if (!old_bh || old_de->ino != old_inode->i_ino)
		goto end_rename;

	error = -EPERM;
	new_inode = new_dentry->d_inode;
	new_bh = bfs_find_entry(new_dir, 
				new_dentry->d_name.name, 
				new_dentry->d_name.len, &new_de);

	if (new_bh && !new_inode) {
		brelse(new_bh);
		new_bh = NULL;
	}
	if (!new_bh) {
		error = bfs_add_entry(new_dir, 
					new_dentry->d_name.name,
			 		new_dentry->d_name.len, old_inode->i_ino);
		if (error)
			goto end_rename;
	}
	old_de->ino = 0;
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;
	old_dir->i_version = ++event;
	mark_inode_dirty(old_dir);
	if (new_inode) {
		new_inode->i_nlink--;
		new_inode->i_ctime = CURRENT_TIME;
		mark_inode_dirty(new_inode);
	}
	mark_buffer_dirty(old_bh);
	error = 0;

end_rename:
	brelse(old_bh);
	brelse(new_bh);
	return error;
}

struct inode_operations bfs_dir_inops = {
	create:			bfs_create,
	lookup:			bfs_lookup,
	link:			bfs_link,
	unlink:			bfs_unlink,
	rename:			bfs_rename,
};

static int bfs_add_entry(struct inode * dir, const char * name, int namelen, int ino)
{
	struct buffer_head * bh;
	struct bfs_dirent * de;
	int block, sblock, eblock, off;
	int i;

	dprintf("name=%s, namelen=%d\n", name, namelen);

	if (!namelen)
		return -ENOENT;
	if (namelen > BFS_NAMELEN)
		return -ENAMETOOLONG;

	sblock = dir->iu_sblock;
	eblock = dir->iu_eblock;
	for (block=sblock; block<=eblock; block++) {
		bh = sb_bread(dir->i_sb, block);
		if(!bh) 
			return -ENOSPC;
		for (off=0; off<BFS_BSIZE; off+=BFS_DIRENT_SIZE) {
			de = (struct bfs_dirent *)(bh->b_data + off);
			if (!de->ino) {
				if ((block-sblock)*BFS_BSIZE + off >= dir->i_size) {
					dir->i_size += BFS_DIRENT_SIZE;
					dir->i_ctime = CURRENT_TIME;
				}
				dir->i_mtime = CURRENT_TIME;
				mark_inode_dirty(dir);
				dir->i_version = ++event;
				de->ino = ino;
				for (i=0; i<BFS_NAMELEN; i++)
					de->name[i] = (i < namelen) ? name[i] : 0;
				mark_buffer_dirty(bh);
				brelse(bh);
				return 0;
			}
		}
		brelse(bh);
	}
	return -ENOSPC;
}

static inline int bfs_namecmp(int len, const char * name, const char * buffer)
{
	if (len < BFS_NAMELEN && buffer[len])
		return 0;
	return !memcmp(name, buffer, len);
}

static struct buffer_head * bfs_find_entry(struct inode * dir, 
	const char * name, int namelen, struct bfs_dirent ** res_dir)
{
	unsigned long block, offset;
	struct buffer_head * bh;
	struct bfs_dirent * de;

	*res_dir = NULL;
	if (namelen > BFS_NAMELEN)
		return NULL;
	bh = NULL;
	block = offset = 0;
	while (block * BFS_BSIZE + offset < dir->i_size) {
		if (!bh) {
			bh = sb_bread(dir->i_sb, dir->iu_sblock + block);
			if (!bh) {
				block++;
				continue;
			}
		}
		de = (struct bfs_dirent *)(bh->b_data + offset);
		offset += BFS_DIRENT_SIZE;
		if (de->ino && bfs_namecmp(namelen, name, de->name)) {
			*res_dir = de;
			return bh;
		}
		if (offset < bh->b_size)
			continue;
		brelse(bh);
		bh = NULL;
		offset = 0;
		block++;
	}
	brelse(bh);
	return NULL;
}
