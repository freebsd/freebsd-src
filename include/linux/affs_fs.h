#ifndef _AFFS_FS_H
#define _AFFS_FS_H
/*
 * The affs filesystem constants/structures
 */

#include <linux/types.h>

#define AFFS_SUPER_MAGIC 0xadff

struct affs_date;

/* --- Prototypes -----------------------------------------------------------------------------	*/

/* amigaffs.c */

extern int	affs_insert_hash(struct inode *inode, struct buffer_head *bh);
extern int	affs_remove_hash(struct inode *dir, struct buffer_head *rem_bh);
extern int	affs_remove_header(struct dentry *dentry);
extern u32	affs_checksum_block(struct super_block *sb, struct buffer_head *bh);
extern void	affs_fix_checksum(struct super_block *sb, struct buffer_head *bh);
extern void	secs_to_datestamp(time_t secs, struct affs_date *ds);
extern mode_t	prot_to_mode(u32 prot);
extern void	mode_to_prot(struct inode *inode);
extern void	affs_error(struct super_block *sb, const char *function, const char *fmt, ...);
extern void	affs_warning(struct super_block *sb, const char *function, const char *fmt, ...);
extern int	affs_check_name(const unsigned char *name, int len);
extern int	affs_copy_name(unsigned char *bstr, struct dentry *dentry);

/* bitmap. c */

extern u32	affs_count_free_bits(u32 blocksize, const void *data);
extern u32	affs_count_free_blocks(struct super_block *s);
extern void	affs_free_block(struct super_block *sb, u32 block);
extern u32	affs_alloc_block(struct inode *inode, u32 goal);
extern int	affs_init_bitmap(struct super_block *sb);

/* namei.c */

extern int	affs_hash_name(struct super_block *sb, const u8 *name, unsigned int len);
extern struct dentry *affs_lookup(struct inode *dir, struct dentry *dentry);
extern int	affs_unlink(struct inode *dir, struct dentry *dentry);
extern int	affs_create(struct inode *dir, struct dentry *dentry, int mode);
extern int	affs_mkdir(struct inode *dir, struct dentry *dentry, int mode);
extern int	affs_rmdir(struct inode *dir, struct dentry *dentry);
extern int	affs_link(struct dentry *olddentry, struct inode *dir,
			  struct dentry *dentry);
extern int	affs_symlink(struct inode *dir, struct dentry *dentry,
			     const char *symname);
extern int	affs_rename(struct inode *old_dir, struct dentry *old_dentry,
			    struct inode *new_dir, struct dentry *new_dentry);

/* inode.c */

extern unsigned long		 affs_parent_ino(struct inode *dir);
extern struct inode		*affs_new_inode(struct inode *dir);
extern int			 affs_notify_change(struct dentry *dentry, struct iattr *attr);
extern void			 affs_put_inode(struct inode *inode);
extern void			 affs_delete_inode(struct inode *inode);
extern void			 affs_clear_inode(struct inode *inode);
extern void			 affs_read_inode(struct inode *inode);
extern void			 affs_write_inode(struct inode *inode, int);
extern int			 affs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry, s32 type);

/* super.c */

extern int			 affs_fs(void);

/* file.c */

void		affs_free_prealloc(struct inode *inode);
extern void	affs_truncate(struct inode *);

/* dir.c */

extern void   affs_dir_truncate(struct inode *);

/* jump tables */

extern struct inode_operations	 affs_file_inode_operations;
extern struct inode_operations	 affs_dir_inode_operations;
extern struct inode_operations   affs_symlink_inode_operations;
extern struct file_operations	 affs_file_operations;
extern struct file_operations	 affs_file_operations_ofs;
extern struct file_operations	 affs_dir_operations;
extern struct address_space_operations	 affs_symlink_aops;
extern struct address_space_operations	 affs_aops;
extern struct address_space_operations	 affs_aops_ofs;

extern struct dentry_operations	 affs_dentry_operations;
extern struct dentry_operations	 affs_dentry_operations_intl;

#endif
