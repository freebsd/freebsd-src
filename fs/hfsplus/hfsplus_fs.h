/*
 *  linux/include/linux/hfsplus_fs.h
 *
 * Copyright (C) 1999
 * Brad Boyer (flar@pants.nu)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 */

#ifndef _LINUX_HFSPLUS_FS_H
#define _LINUX_HFSPLUS_FS_H

#include <linux/fs.h>
#include <linux/version.h>
#include "hfsplus_raw.h"

#define DBG_BNODE_REFS	0x00000001
#define DBG_BNODE_MOD	0x00000002
#define DBG_CAT_MOD	0x00000004
#define DBG_INODE	0x00000008
#define DBG_SUPER	0x00000010
#define DBG_EXTENT	0x00000020

//#define DBG_MASK	(DBG_EXTENT|DBG_INODE|DBG_BNODE_MOD)
//#define DBG_MASK	(DBG_BNODE_MOD|DBG_CAT_MOD)
//#define DBG_MASK	(DBG_BNODE_REFS|DBG_INODE|DBG_BNODE_MOD)
#define DBG_MASK	(0)

#define dprint(flg, fmt, args...) \
	if (flg & DBG_MASK) printk(fmt , ## args)

/* Runtime config options */
#define HFSPLUS_CASE_ASIS      0
#define HFSPLUS_CASE_LOWER     1

#define HFSPLUS_FORK_RAW       0
#define HFSPLUS_FORK_CAP       1
#define HFSPLUS_FORK_DOUBLE    2
#define HFSPLUS_FORK_NETATALK  3

#define HFSPLUS_NAMES_TRIVIAL  0
#define HFSPLUS_NAMES_CAP      1
#define HFSPLUS_NAMES_NETATALK 2
#define HFSPLUS_NAMES_7BIT     3

#define HFSPLUS_DEF_CR_TYPE    0x3F3F3F3F  /* '????' */

#define HFSPLUS_TYPE_DATA 0x00
#define HFSPLUS_TYPE_RSRC 0xFF

typedef int (*btree_keycmp)(hfsplus_btree_key *, hfsplus_btree_key *);

#define NODE_HASH_SIZE	256

/* An HFS+ BTree held in memory */
typedef struct hfsplus_btree {
	struct super_block *sb;
	struct inode *inode;
	btree_keycmp keycmp;

	u32 cnid;
	u32 root;
	u32 leaf_count;
	u32 leaf_head;
	u32 leaf_tail;
	u32 node_count;
	u32 free_nodes;
	u32 attributes;

	unsigned int node_size;
	unsigned int node_size_shift;
	unsigned int max_key_len;
	unsigned int depth;

	//unsigned int map1_size, map_size;
	struct semaphore tree_lock;

	unsigned int pages_per_bnode;
	spinlock_t hash_lock;
	struct hfsplus_bnode *node_hash[NODE_HASH_SIZE];
	int node_hash_cnt;
} hfsplus_btree;

struct page;

/* An HFS+ BTree node in memory */
typedef struct hfsplus_bnode {
	struct hfsplus_btree *tree;

	u32 prev;
	u32 this;
	u32 next;
	u32 parent;

	u16 num_recs;
	u8 kind;
	u8 height;

	struct hfsplus_bnode *next_hash;
	unsigned long flags;
	wait_queue_head_t lock_wq;
	atomic_t refcnt;
	unsigned int page_offset;
	struct page *page[0];
} hfsplus_bnode;

#define HFSPLUS_BNODE_LOCK	0
#define HFSPLUS_BNODE_ERROR	1
#define HFSPLUS_BNODE_NEW	2
#define HFSPLUS_BNODE_DIRTY	3
#define HFSPLUS_BNODE_DELETED	4

/*
 * HFS+ superblock info (built from Volume Header on disk)
 */

struct hfsplus_vh;
struct hfsplus_btree;

struct hfsplus_sb_info {
	struct buffer_head *s_vhbh;
	struct hfsplus_vh *s_vhdr;
	struct hfsplus_btree *ext_tree;
	struct hfsplus_btree *cat_tree;
	struct hfsplus_btree *attr_tree;
	struct inode *alloc_file;
	struct inode *hidden_dir;

	/* Runtime variables */
	u32 blockoffset;
	u32 sect_count;
	//int a2b_shift;

	/* Stuff in host order from Vol Header */
	u32 total_blocks;
	u32 free_blocks;
	u32 next_alloc;
	u32 next_cnid;
	u32 file_count;
	u32 folder_count;

	/* Config options */
	u32 creator;
	u32 type;

	int charcase;
	int fork;
	int namemap;

	umode_t umask;
	uid_t uid;
	gid_t gid;

	unsigned long flags;

	atomic_t inode_cnt;
	u32 last_inode_cnt;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	struct list_head rsrc_inodes;
#else
	struct hlist_head rsrc_inodes;
#endif
};

#define HFSPLUS_SB_WRITEBACKUP	0x0001


struct hfsplus_inode_info {
	/* Device number in hfsplus_permissions in catalog */
	u32 dev;
	/* Allocation extents from catlog record or volume header */
	hfsplus_extent_rec extents;
	u32 total_blocks, extent_blocks, alloc_blocks;
	atomic_t opencnt;

	struct inode *rsrc_inode;
	unsigned long flags;

	struct list_head open_dir_list;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	unsigned long mmu_private;
#else
	loff_t mmu_private;
	struct inode vfs_inode;
#endif
};

#define HFSPLUS_FLG_RSRC	0x0001
#define HFSPLUS_FLG_DIRTYMODE	0x0002

#define HFSPLUS_IS_DATA(inode)   (!(HFSPLUS_I(inode).flags & HFSPLUS_FLG_RSRC))
#define HFSPLUS_IS_RSRC(inode)   (HFSPLUS_I(inode).flags & HFSPLUS_FLG_RSRC)

struct hfsplus_find_data {
	/* filled by caller */
	hfsplus_btree_key *search_key;
	hfsplus_btree_key *key;
	/* filled by find */
	hfsplus_btree *tree;
	hfsplus_bnode *bnode;
	/* filled by findrec */
	int record, exact;
	int keyoffset, keylength;
	int entryoffset, entrylength;
};

struct hfsplus_readdir_data {
	struct list_head list;
	struct file *file;
	hfsplus_cat_key key;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
typedef long sector_t;
#endif

/*
 * Functions in any *.c used in other files
 */

/* bfind.c */
void hfsplus_find_rec(hfsplus_bnode *, struct hfsplus_find_data *);
int hfsplus_btree_find(struct hfsplus_find_data *);
int hfsplus_btree_find_entry(struct hfsplus_find_data *,
			     void *, int);
int hfsplus_btree_move(struct hfsplus_find_data *, int);
int hfsplus_find_init(hfsplus_btree *, struct hfsplus_find_data *);
void hfsplus_find_exit(struct hfsplus_find_data *);

/* bnode.c */
struct buffer_head *hfsplus_getblk(struct inode *, unsigned long);
hfsplus_bnode *__hfsplus_find_bnode(hfsplus_btree *, u32);
void __hfsplus_bnode_remove(hfsplus_bnode *);
hfsplus_bnode *hfsplus_create_bnode(hfsplus_btree *, u32);
hfsplus_bnode *hfsplus_find_bnode(hfsplus_btree *, u32);
void hfsplus_get_bnode(hfsplus_bnode *);
void hfsplus_put_bnode(hfsplus_bnode *);
void hfsplus_bnode_free(hfsplus_bnode *);
void hfsplus_bnode_readbytes(hfsplus_bnode *, void *, unsigned long, unsigned long);
u16 hfsplus_bnode_read_u16(hfsplus_bnode *, unsigned long);
void hfsplus_bnode_writebytes(hfsplus_bnode *, void *, unsigned long, unsigned long);
void hfsplus_bnode_write_u16(hfsplus_bnode *, unsigned long, u16);
void hfsplus_bnode_copybytes(hfsplus_bnode *, unsigned long,
			     hfsplus_bnode *, unsigned long, unsigned long);
void hfsplus_bnode_movebytes(hfsplus_bnode *, unsigned long, unsigned long, unsigned long);
int hfsplus_bnode_insert_rec(struct hfsplus_find_data *, void *, int);
int hfsplus_bnode_remove_rec(struct hfsplus_find_data *);

/* brec.c */
u16 hfsplus_brec_lenoff(hfsplus_bnode *, u16, u16 *);
u16 hfsplus_brec_keylen(hfsplus_bnode *, u16);

/* btree.c */
hfsplus_btree *hfsplus_open_btree(struct super_block *, u32);
void hfsplus_close_btree(struct hfsplus_btree *);
void hfsplus_write_btree(struct hfsplus_btree *);
hfsplus_bnode *hfsplus_btree_alloc_node(hfsplus_btree *);
void hfsplus_btree_remove_node(hfsplus_bnode *);
void hfsplus_btree_free_node(hfsplus_bnode *);

/* catalog.c */
int hfsplus_cmp_cat_key(hfsplus_btree_key *, hfsplus_btree_key *);
void hfsplus_fill_cat_key(hfsplus_btree_key *, u32, struct qstr *);
int hfsplus_find_cat(struct super_block *, unsigned long, struct hfsplus_find_data *);
int hfsplus_create_cat(u32, struct inode *, struct qstr *, struct inode *);
int hfsplus_delete_cat(u32, struct inode *, struct qstr *);
int hfsplus_rename_cat(u32, struct inode *, struct qstr *,
		       struct inode *, struct qstr *);

/* extents.c */
int hfsplus_cmp_ext_key(hfsplus_btree_key *, hfsplus_btree_key *);
void hfsplus_fill_ext_key(hfsplus_btree_key *, u32, u32, u8);
int hfsplus_get_block(struct inode *, sector_t, struct buffer_head *, int);
int hfsplus_free_fork(struct super_block *, u32, hfsplus_fork_raw *, int);
int hfsplus_extend_file(struct inode *);
void hfsplus_truncate(struct inode *);

/* inode.c */
void hfsplus_inode_read_fork(struct inode *, hfsplus_fork_raw *);
void hfsplus_inode_write_fork(struct inode *, hfsplus_fork_raw *);
int hfsplus_cat_read_inode(struct inode *, struct hfsplus_find_data *);
void hfsplus_cat_write_inode(struct inode *);
struct inode *hfsplus_new_inode(struct super_block *, int);
void hfsplus_delete_inode(struct inode *);

extern struct address_space_operations hfsplus_btree_aops;

/* options.c */
int parse_options(char *, struct hfsplus_sb_info *);
void fill_defaults(struct hfsplus_sb_info *);
void fill_current(struct hfsplus_sb_info *, struct hfsplus_sb_info *);

/* tables.c */
extern u16 case_fold_table[];

/* unicode.c */
int hfsplus_unistrcmp(const hfsplus_unistr *, const hfsplus_unistr *);
int hfsplus_uni2asc(const hfsplus_unistr *, char *, int *);
int hfsplus_asc2uni(hfsplus_unistr *, const char *, int);

/* wrapper.c */
int hfsplus_read_wrapper(struct super_block *);

/* access macros */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define HFSPLUS_SB(super)        (*(struct hfsplus_sb_info *)&(super)->u)
#define HFSPLUS_I(inode)         (*(struct hfsplus_inode_info *)&(inode)->u)
#else
/*
static inline struct hfsplus_sb_info *HFSPLUS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}
static inline struct hfsplus_inode_info *HFSPLUS_I(struct inode *inode)
{
	return list_entry(inode, struct hfsplus_inode_info, vfs_inode);
}
*/
#define HFSPLUS_SB(super)	(*(struct hfsplus_sb_info *)(super)->s_fs_info)
#define HFSPLUS_I(inode)	(*list_entry(inode, struct hfsplus_inode_info, vfs_inode))
#endif

#if 1
#define hfsplus_kmap(p)		({ struct page *__p = (p); kmap(__p); })
#define hfsplus_kunmap(p)	({ struct page *__p = (p); kunmap(__p); __p; })
#else
#define hfsplus_kmap(p)		kmap(p)
#define hfsplus_kunmap(p)	kunmap(p)
#endif

/* time macros */
#define __hfsp_mt2ut(t)		(be32_to_cpu(t) - 2082844800U)
#define __hfsp_ut2mt(t)		(cpu_to_be32(t + 2082844800U))

/* compatibility */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define PageUptodate(page)	Page_Uptodate(page)
#define wait_on_page_locked(page) wait_on_page(page)
#define get_seconds()		CURRENT_TIME
#define page_symlink(i,n,l)		block_symlink(i,n,l)
#define map_bh(bh, sb, block) ({				\
	bh->b_dev = kdev_t_to_nr(sb->s_dev);			\
	bh->b_blocknr = block;					\
	bh->b_state |= (1UL << BH_Mapped);			\
})
#define set_buffer_new(bh)	(bh->b_state |= (1UL << BH_New))
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,21)
#define new_inode(sb) ({					\
	struct inode *inode = get_empty_inode();		\
	if (inode) {						\
		inode->i_sb = sb;				\
		inode->i_dev = sb->s_dev;			\
		inode->i_blkbits = sb->s_blocksize_bits;	\
	}							\
	inode;							\
})
#endif
#define	hfsp_mt2ut(t)		__hfsp_mt2ut(t)
#define hfsp_ut2mt(t)		__hfsp_ut2mt(t)
#define hfsp_now2mt()		__hfsp_ut2mt(CURRENT_TIME)
#else
#define hfsp_mt2ut(t)		(struct timespec){ .tv_sec = __hfsp_mt2ut(t) }
#define hfsp_ut2mt(t)		__hfsp_ut2mt((t).tv_sec)
#define hfsp_now2mt()		__hfsp_ut2mt(get_seconds())
#endif

#endif
