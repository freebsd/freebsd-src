/*
 * namei.c
 *
 * Copyright (c) 1999 Al Smith
 *
 * Portions derived from work (c) 1995,1996 Christian Vogelgsang.
 */

#include <linux/string.h>
#include <linux/efs_fs.h>

static efs_ino_t efs_find_entry(struct inode *inode, const char *name, int len) {
	struct buffer_head *bh;

	int			slot, namelen;
	char			*nameptr;
	struct efs_dir		*dirblock;
	struct efs_dentry	*dirslot;
	efs_ino_t		inodenum;
	efs_block_t		block;
 
	if (inode->i_size & (EFS_DIRBSIZE-1))
		printk(KERN_WARNING "EFS: WARNING: find_entry(): directory size not a multiple of EFS_DIRBSIZE\n");

	for(block = 0; block < inode->i_blocks; block++) {

		bh = sb_bread(inode->i_sb, efs_bmap(inode, block));
		if (!bh) {
			printk(KERN_ERR "EFS: find_entry(): failed to read dir block %d\n", block);
			return 0;
		}
    
		dirblock = (struct efs_dir *) bh->b_data;

		if (be16_to_cpu(dirblock->magic) != EFS_DIRBLK_MAGIC) {
			printk(KERN_ERR "EFS: find_entry(): invalid directory block\n");
			brelse(bh);
			return(0);
		}

		for(slot = 0; slot < dirblock->slots; slot++) {
			dirslot  = (struct efs_dentry *) (((char *) bh->b_data) + EFS_SLOTAT(dirblock, slot));

			namelen  = dirslot->namelen;
			nameptr  = dirslot->name;

			if ((namelen == len) && (!memcmp(name, nameptr, len))) {
				inodenum = be32_to_cpu(dirslot->inode);
				brelse(bh);
				return(inodenum);
			}
		}
		brelse(bh);
	}
	return(0);
}

struct dentry *efs_lookup(struct inode *dir, struct dentry *dentry) {
	efs_ino_t inodenum;
	struct inode * inode;

	if (!dir || !S_ISDIR(dir->i_mode))
		return ERR_PTR(-ENOENT);

	inode = NULL;

	inodenum = efs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (inodenum) {
		if (!(inode = iget(dir->i_sb, inodenum)))
			return ERR_PTR(-EACCES);
	}

	d_add(dentry, inode);
	return NULL;
}

