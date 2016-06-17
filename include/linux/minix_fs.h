#ifndef _LINUX_MINIX_FS_H
#define _LINUX_MINIX_FS_H

/*
 * The minix filesystem constants/structures
 */

/*
 * Thanks to Kees J Bot for sending me the definitions of the new
 * minix filesystem (aka V2) with bigger inodes and 32-bit block
 * pointers.
 */

#define MINIX_ROOT_INO 1

/* Not the same as the bogus LINK_MAX in <linux/limits.h>. Oh well. */
#define MINIX_LINK_MAX	250
#define MINIX2_LINK_MAX	65530

#define MINIX_I_MAP_SLOTS	8
#define MINIX_Z_MAP_SLOTS	64
#define MINIX_SUPER_MAGIC	0x137F		/* original minix fs */
#define MINIX_SUPER_MAGIC2	0x138F		/* minix fs, 30 char names */
#define MINIX2_SUPER_MAGIC	0x2468		/* minix V2 fs */
#define MINIX2_SUPER_MAGIC2	0x2478		/* minix V2 fs, 30 char names */
#define MINIX_VALID_FS		0x0001		/* Clean fs. */
#define MINIX_ERROR_FS		0x0002		/* fs has errors. */

#define MINIX_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct minix_inode)))
#define MINIX2_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct minix2_inode)))

#define MINIX_V1		0x0001		/* original minix fs */
#define MINIX_V2		0x0002		/* minix V2 fs */

#define INODE_VERSION(inode)	inode->i_sb->u.minix_sb.s_version

/*
 * This is the original minix inode layout on disk.
 * Note the 8-bit gid and atime and ctime.
 */
struct minix_inode {
	__u16 i_mode;
	__u16 i_uid;
	__u32 i_size;
	__u32 i_time;
	__u8  i_gid;
	__u8  i_nlinks;
	__u16 i_zone[9];
};

/*
 * The new minix inode has all the time entries, as well as
 * long block numbers and a third indirect block (7+1+1+1
 * instead of 7+1+1). Also, some previously 8-bit values are
 * now 16-bit. The inode is now 64 bytes instead of 32.
 */
struct minix2_inode {
	__u16 i_mode;
	__u16 i_nlinks;
	__u16 i_uid;
	__u16 i_gid;
	__u32 i_size;
	__u32 i_atime;
	__u32 i_mtime;
	__u32 i_ctime;
	__u32 i_zone[10];
};

/*
 * minix super-block data on disk
 */
struct minix_super_block {
	__u16 s_ninodes;
	__u16 s_nzones;
	__u16 s_imap_blocks;
	__u16 s_zmap_blocks;
	__u16 s_firstdatazone;
	__u16 s_log_zone_size;
	__u32 s_max_size;
	__u16 s_magic;
	__u16 s_state;
	__u32 s_zones;
};

struct minix_dir_entry {
	__u16 inode;
	char name[0];
};

#ifdef __KERNEL__

/*
 * change the define below to 0 if you want names > info->s_namelen chars to be
 * truncated. Else they will be disallowed (ENAMETOOLONG).
 */
#define NO_TRUNCATE 1

extern struct minix_inode * minix_V1_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern struct minix2_inode * minix_V2_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern struct inode * minix_new_inode(const struct inode * dir, int * error);
extern void minix_free_inode(struct inode * inode);
extern unsigned long minix_count_free_inodes(struct super_block *sb);
extern int minix_new_block(struct inode * inode);
extern void minix_free_block(struct inode * inode, int block);
extern unsigned long minix_count_free_blocks(struct super_block *sb);

extern void V1_minix_truncate(struct inode *);
extern void V2_minix_truncate(struct inode *);
extern void minix_truncate(struct inode *);
extern int minix_sync_inode(struct inode *);
extern void minix_set_inode(struct inode *, dev_t);
extern int V1_minix_get_block(struct inode *, long, struct buffer_head *, int);
extern int V2_minix_get_block(struct inode *, long, struct buffer_head *, int);

extern struct minix_dir_entry *minix_find_entry(struct dentry*, struct page**);
extern int minix_add_link(struct dentry*, struct inode*);
extern int minix_delete_entry(struct minix_dir_entry*, struct page*);
extern int minix_make_empty(struct inode*, struct inode*);
extern int minix_empty_dir(struct inode*);
extern void minix_set_link(struct minix_dir_entry*, struct page*, struct inode*);
extern struct minix_dir_entry *minix_dotdot(struct inode*, struct page**);
extern ino_t minix_inode_by_name(struct dentry*);

extern int minix_sync_file(struct file *, struct dentry *, int);

extern struct inode_operations minix_file_inode_operations;
extern struct inode_operations minix_dir_inode_operations;
extern struct file_operations minix_file_operations;
extern struct file_operations minix_dir_operations;
extern struct dentry_operations minix_dentry_operations;

#endif /* __KERNEL__ */

#endif
