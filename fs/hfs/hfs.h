/* 
 * linux/fs/hfs/hfs.h
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 */

#ifndef _HFS_H
#define _HFS_H

#include <linux/hfs_sysdep.h>

#define HFS_NEW(X)	((X) = hfs_malloc(sizeof(*(X))))
#define HFS_DELETE(X)	do { hfs_free((X), sizeof(*(X))); (X) = NULL; } \
                        while (0)
 
/* offsets to various blocks */
#define HFS_DD_BLK		0 /* Driver Descriptor block */
#define HFS_PMAP_BLK		1 /* First block of partition map */
#define HFS_MDB_BLK		2 /* Block (w/i partition) of MDB */

/* magic numbers for various disk blocks */
#define HFS_DRVR_DESC_MAGIC	0x4552 /* "ER": driver descriptor map */
#define HFS_OLD_PMAP_MAGIC	0x5453 /* "TS": old-type partition map */
#define HFS_NEW_PMAP_MAGIC	0x504D /* "PM": new-type partition map */
#define HFS_SUPER_MAGIC		0x4244 /* "BD": HFS MDB (super block) */
#define HFS_MFS_SUPER_MAGIC	0xD2D7 /* MFS MDB (super block) */

/* magic numbers for various internal structures */
#define HFS_FILE_MAGIC		0x4801
#define HFS_DIR_MAGIC		0x4802
#define HFS_MDB_MAGIC		0x4803
#define HFS_EXT_MAGIC		0x4804 /* XXX currently unused */
#define HFS_BREC_MAGIC		0x4811 /* XXX currently unused */
#define HFS_BTREE_MAGIC		0x4812
#define HFS_BNODE_MAGIC		0x4813

/* various FIXED size parameters */
#define HFS_SECTOR_SIZE		512    /* size of an HFS sector */
#define HFS_SECTOR_SIZE_BITS	9      /* log_2(HFS_SECTOR_SIZE) */
#define HFS_NAMELEN		31     /* maximum length of an HFS filename */
#define HFS_NAMEMAX		(3*31) /* max size of ENCODED filename */
#define HFS_BM_MAXBLOCKS	(16)   /* max number of bitmap blocks */
#define HFS_BM_BPB (8*HFS_SECTOR_SIZE) /* number of bits per bitmap block */
#define HFS_MAX_VALENCE		32767U
#define HFS_FORK_MAX		(0x7FFFFFFF)

/* Meanings of the drAtrb field of the MDB,
 * Reference: _Inside Macintosh: Files_ p. 2-61
 */
#define HFS_SB_ATTRIB_HLOCK 0x0080
#define HFS_SB_ATTRIB_CLEAN 0x0100
#define HFS_SB_ATTRIB_SPARED 0x0200
#define HFS_SB_ATTRIB_SLOCK 0x8000

/* 2**16 - 1 */
#define HFS_USHRT_MAX	65535

/* Some special File ID numbers */
#define HFS_POR_CNID	1	/* Parent Of the Root */
#define HFS_ROOT_CNID	2	/* ROOT directory */
#define HFS_EXT_CNID	3	/* EXTents B-tree */
#define HFS_CAT_CNID	4	/* CATalog B-tree */
#define HFS_BAD_CNID	5	/* BAD blocks file */
#define HFS_ALLOC_CNID  6       /* ALLOCation file (HFS+) */
#define HFS_START_CNID  7       /* STARTup file (HFS+) */
#define HFS_ATTR_CNID   8       /* ATTRibutes file (HFS+) */
#define HFS_EXCH_CNID  15       /* ExchangeFiles temp id */

/* values for hfs_cat_rec.cdrType */
#define HFS_CDR_DIR    0x01    /* folder (directory) */
#define HFS_CDR_FIL    0x02    /* file */
#define HFS_CDR_THD    0x03    /* folder (directory) thread */
#define HFS_CDR_FTH    0x04    /* file thread */

/* legal values for hfs_ext_key.FkType and hfs_file.fork */
#define HFS_FK_DATA	0x00
#define HFS_FK_RSRC	0xFF

/* bits in hfs_fil_entry.Flags */
#define HFS_FIL_LOCK	0x01  /* locked */
#define HFS_FIL_THD	0x02  /* file thread */
#define HFS_FIL_DOPEN   0x04  /* data fork open */
#define HFS_FIL_ROPEN   0x08  /* resource fork open */
#define HFS_FIL_DIR     0x10  /* directory (always clear) */
#define HFS_FIL_RSRV1   0x20  /* reserved */
#define HFS_FIL_NOCOPY  0x40  /* copy-protected file */
#define HFS_FIL_USED	0x80  /* open */

/* bits in hfs_dir_entry.Flags. dirflags is 16 bits. */
#define HFS_DIR_LOCK        0x01  /* locked */
#define HFS_DIR_THD         0x02  /* directory thread */
#define HFS_DIR_INEXPFOLDER 0x04  /* in a shared area */
#define HFS_DIR_MOUNTED     0x08  /* mounted */
#define HFS_DIR_DIR         0x10  /* directory (always set) */
#define HFS_DIR_EXPFOLDER   0x20  /* share point */
#define HFS_DIR_RSRV1       0x40  /* reserved */
#define HFS_DIR_RSRV2       0x80  /* reserved */

/* Access types used when requesting access to a B-node */
#define HFS_LOCK_NONE	0x0000	/* Illegal */
#define HFS_LOCK_READ	0x0001	/* read-only access */
#define HFS_LOCK_RESRV	0x0002	/* might potentially modify */
#define HFS_LOCK_WRITE	0x0003	/* will modify now (exclusive access) */
#define HFS_LOCK_MASK	0x000f

/* Flags field of the hfs_path_elem */
#define HFS_BPATH_FIRST		0x0100
#define HFS_BPATH_OVERFLOW	0x0200
#define HFS_BPATH_UNDERFLOW	0x0400
#define HFS_BPATH_MASK		0x0f00

/* Flags for hfs_bfind() */
#define HFS_BFIND_EXACT		0x0010
#define HFS_BFIND_LOCK		0x0020

/* Modes for hfs_bfind() */
#define HFS_BFIND_WRITE   (HFS_LOCK_RESRV|HFS_BFIND_EXACT|HFS_BFIND_LOCK)
#define HFS_BFIND_READ_EQ (HFS_LOCK_READ|HFS_BFIND_EXACT)
#define HFS_BFIND_READ_LE (HFS_LOCK_READ)
#define HFS_BFIND_INSERT  (HFS_LOCK_RESRV|HFS_BPATH_FIRST|HFS_BPATH_OVERFLOW)
#define HFS_BFIND_DELETE \
	 (HFS_LOCK_RESRV|HFS_BFIND_EXACT|HFS_BPATH_FIRST|HFS_BPATH_UNDERFLOW)

/*======== HFS structures as they appear on the disk ========*/

/* Pascal-style string of up to 31 characters */
struct hfs_name {
	hfs_byte_t	Len;
	hfs_byte_t	Name[31];
} __attribute__((packed));

typedef struct {
	hfs_word_t	v;
	hfs_word_t	h;
} hfs_point_t;

typedef struct {
	hfs_word_t	top;
	hfs_word_t	left;
	hfs_word_t	bottom;
	hfs_word_t	right;
} hfs_rect_t;

typedef struct {
	hfs_lword_t	 fdType;
	hfs_lword_t	 fdCreator;
	hfs_word_t	 fdFlags;
	hfs_point_t	 fdLocation;
	hfs_word_t	 fdFldr;
} __attribute__((packed)) hfs_finfo_t;

typedef struct {
	hfs_word_t	fdIconID;
	hfs_byte_t	fdUnused[8];
	hfs_word_t	fdComment;
	hfs_lword_t	fdPutAway;
} __attribute__((packed)) hfs_fxinfo_t;

typedef struct {
	hfs_rect_t	frRect;
	hfs_word_t	frFlags;
	hfs_point_t	frLocation;
	hfs_word_t	frView;
} __attribute__((packed)) hfs_dinfo_t;

typedef struct {
	hfs_point_t	frScroll;
	hfs_lword_t	frOpenChain;
	hfs_word_t	frUnused;
	hfs_word_t	frComment;
	hfs_lword_t	frPutAway;
} __attribute__((packed)) hfs_dxinfo_t;

union hfs_finder_info {
	struct {
		hfs_finfo_t	finfo;
		hfs_fxinfo_t	fxinfo;
	} file;
	struct {
		hfs_dinfo_t	dinfo;
		hfs_dxinfo_t	dxinfo;
	} dir;
};

/* A btree record key on disk */
struct hfs_bkey {
	hfs_byte_t	KeyLen;		/* number of bytes in the key */
	hfs_byte_t	value[1];	/* (KeyLen) bytes of key */
} __attribute__((packed));

/* Cast to a pointer to a generic bkey */
#define	HFS_BKEY(X)	(((void)((X)->KeyLen)), ((struct hfs_bkey *)(X)))

/* The key used in the catalog b-tree: */
struct hfs_cat_key {
	hfs_byte_t	KeyLen;	/* number of bytes in the key */
	hfs_byte_t	Resrv1;	/* padding */
	hfs_lword_t	ParID;	/* CNID of the parent dir */
	struct hfs_name	CName;	/* The filename of the entry */
} __attribute__((packed));

/* The key used in the extents b-tree: */
struct hfs_ext_key {
	hfs_byte_t	KeyLen;	/* number of bytes in the key */
	hfs_byte_t	FkType;	/* HFS_FK_{DATA,RSRC} */
	hfs_lword_t	FNum;	/* The File ID of the file */
	hfs_word_t	FABN;	/* allocation blocks number*/
} __attribute__((packed));

/*======== Data structures kept in memory ========*/

/*
 * struct hfs_mdb
 *
 * The fields from the MDB of an HFS filesystem
 */
struct hfs_mdb {
	int			magic;		/* A magic number */
	unsigned char		vname[28];	/* The volume name */
	hfs_sysmdb		sys_mdb;	/* superblock */
	hfs_buffer		buf;		/* The hfs_buffer
						   holding the real
						   superblock (aka VIB
						   or MDB) */
	hfs_buffer		alt_buf;	/* The hfs_buffer holding
						   the alternate superblock */
	hfs_buffer		bitmap[16];	/* The hfs_buffer holding the
						   allocation bitmap */
	struct hfs_btree *	ext_tree;	/* Information about
						   the extents b-tree */
	struct hfs_btree *	cat_tree;	/* Information about
						   the catalog b-tree */
	hfs_u32			file_count;	/* The number of
						   regular files in
						   the filesystem */
	hfs_u32			dir_count;	/* The number of
						   directories in the
						   filesystem */
	hfs_u32			next_id;	/* The next available
						   file id number */
	hfs_u32			clumpablks;	/* The number of allocation
						   blocks to try to add when
						   extending a file */
	hfs_u32			write_count;	/* The number of MDB
						   writes (a sort of
						   version number) */
	hfs_u32			fs_start;	/* The first 512-byte
						   block represented
						   in the bitmap */
	hfs_u32			create_date;	/* In network byte-order */
	hfs_u32			modify_date;	/* In network byte-order */
	hfs_u32			backup_date;	/* In network byte-order */
	hfs_u16			root_files;	/* The number of
						   regular
						   (non-directory)
						   files in the root
						   directory */
	hfs_u16			root_dirs;	/* The number of
						   directories in the
						   root directory */
	hfs_u16			fs_ablocks;	/* The number of
						   allocation blocks
						   in the filesystem */
	hfs_u16			free_ablocks;	/* The number of unused
						   allocation blocks
						   in the filesystem */
	hfs_u32			alloc_blksz;	/* The number of
						   512-byte blocks per
						   "allocation block" */
	hfs_u16			attrib;		/* Attribute word */
	hfs_wait_queue		rename_wait;
	int			rename_lock;
	hfs_wait_queue		bitmap_wait;
	int			bitmap_lock;
        struct list_head        entry_dirty;
};

/*
 * struct hfs_extent
 *
 * The offset to allocation block mapping for a given file is
 * contained in a series of these structures.  Each (struct
 * hfs_extent) records up to three runs of contiguous allocation
 * blocks.  An allocation block is a contiguous group of physical
 * blocks.
 */
struct hfs_extent {
	int		   magic;     /* A magic number */
	unsigned short	   start;     /* Where in the file this record
					 begins (in allocation blocks) */
	unsigned short	   end;	      /* Where in the file this record
					 ends (in allocation blocks) */
	unsigned short	   block[3];  /* The allocation block on disk which
					 begins this extent */
	unsigned short	   length[3]; /* The number of allocation blocks
					 in this extent */
	struct hfs_extent  *next;     /* Next extent record for this file */
	struct hfs_extent  *prev;     /* Previous extent record for this file */
	int		   count;     /* Number of times it is used */
};

/*
 * struct hfs_dir 
 *
 * This structure holds information specific
 * to a directory in an HFS filesystem.
 */
struct hfs_dir {		
	int		magic;		/* A magic number */
	hfs_u16		flags;
	hfs_u16		dirs;		/* Number of directories in this one */
	hfs_u16		files;		/* Number of files in this directory */
	int		readers;
	hfs_wait_queue	read_wait;
	int		writers;
	hfs_wait_queue	write_wait;
};

/*
 * struct hfs_fork
 *
 * This structure holds the information
 * specific to a single fork of a file.
 */
struct hfs_fork {
	struct hfs_cat_entry	*entry;    /* The file this fork is part of */
	struct hfs_extent	first;     /* The first extent record for
						 this fork */
	struct hfs_extent	*cache;    /* The most-recently accessed
						 extent record for this fork */
	hfs_u32			lsize;     /* The logical size in bytes */
	hfs_u32			psize;     /* The phys size (512-byte blocks) */
        hfs_u8			fork;      /* Which fork is this? */
};

/*
 * struct hfs_file 
 *
 * This structure holds information specific
 * to a file in an HFS filesystem.
 */
struct hfs_file {
	int		   magic;
	struct hfs_fork    data_fork;
	struct hfs_fork    rsrc_fork;
	hfs_u16		   clumpablks;
	hfs_u8		   flags;
};

/*
 * struct hfs_file 
 *
 * This structure holds information about a
 * file or directory in an HFS filesystem.
 *
 * 'wait' must remain 1st and 'hash' 2nd since we do some pointer arithmetic.
 */
struct hfs_cat_entry {
	hfs_wait_queue		wait;
        struct list_head        hash;
        struct list_head        list;
	struct hfs_mdb		*mdb;
	hfs_sysentry		sys_entry;
	struct hfs_cat_key	key;
	union hfs_finder_info	info;
	hfs_u32			cnid;		/* In network byte-order */
	hfs_u32			create_date;	/* In network byte-order */
	hfs_u32			modify_date;	/* In network byte-order */
	hfs_u32			backup_date;	/* In network byte-order */
	unsigned short		count;
        unsigned long           state;
	hfs_u8			type;
	union {
		struct hfs_dir	dir;
		struct hfs_file file;
	} u;
};

/* hfs entry state bits */
#define HFS_DIRTY        1
#define HFS_KEYDIRTY     2
#define HFS_LOCK         4
#define HFS_DELETED      8

/* 
 * struct hfs_bnode_ref
 *
 * A pointer to a (struct hfs_bnode) and the type of lock held on it.
 */
struct hfs_bnode_ref {
        struct hfs_bnode *bn;
        int lock_type;
};

/*
 * struct hfs_belem
 *
 * An element of the path from the root of a B-tree to a leaf.
 * Includes the reference to a (struct hfs_bnode), the index of
 * the appropriate record in that node, and some flags.
 */
struct hfs_belem {
	struct hfs_bnode_ref	bnr;
	int			record;
	int			flags;
};

/*
 * struct hfs_brec
 *
 * The structure returned by hfs_bfind() to describe the requested record.
 */
struct hfs_brec {
	int			keep_flags;
	struct hfs_btree	*tree;
	struct hfs_belem	*top;
	struct hfs_belem	*bottom;
	struct hfs_belem	elem[9];
	struct hfs_bkey		*key;
	void			*data;	/* The actual data */
};

/*================ Function prototypes ================*/

/* bdelete.c */
extern int hfs_bdelete(struct hfs_btree *, const struct hfs_bkey *);

/* bfind.c */
extern void hfs_brec_relse(struct hfs_brec *, struct hfs_belem *);
extern int hfs_bsucc(struct hfs_brec *, int);
extern int hfs_bfind(struct hfs_brec *, struct hfs_btree *,
		     const struct hfs_bkey *, int);
 
/* binsert.c */
extern int hfs_binsert(struct hfs_btree *, const struct hfs_bkey *,
		       const void *, hfs_u16);

/* bitmap.c */
extern hfs_u16 hfs_vbm_count_free(const struct hfs_mdb *, hfs_u16);
extern hfs_u16 hfs_vbm_search_free(const struct hfs_mdb *, hfs_u16 *);
extern int hfs_set_vbm_bits(struct hfs_mdb *, hfs_u16, hfs_u16);
extern int hfs_clear_vbm_bits(struct hfs_mdb *, hfs_u16, hfs_u16);

/* bitops.c */
extern hfs_u32 hfs_find_zero_bit(const hfs_u32 *, hfs_u32, hfs_u32);
extern hfs_u32 hfs_count_zero_bits(const hfs_u32 *, hfs_u32, hfs_u32);

/* btree.c */
extern struct hfs_btree *hfs_btree_init(struct hfs_mdb *, ino_t,
				        hfs_byte_t *, hfs_u32, hfs_u32);
extern void hfs_btree_free(struct hfs_btree *);
extern void hfs_btree_commit(struct hfs_btree *, hfs_byte_t *, hfs_lword_t);

/* catalog.c */
extern void hfs_cat_init(void);
extern void hfs_cat_put(struct hfs_cat_entry *);
extern void hfs_cat_mark_dirty(struct hfs_cat_entry *);
extern struct hfs_cat_entry *hfs_cat_get(struct hfs_mdb *,
					 const struct hfs_cat_key *);

extern void hfs_cat_invalidate(struct hfs_mdb *);
extern void hfs_cat_commit(struct hfs_mdb *);
extern void hfs_cat_free(void);

extern int hfs_cat_compare(const struct hfs_cat_key *,
			   const struct hfs_cat_key *);
extern void hfs_cat_build_key(hfs_u32, const struct hfs_name *,
			      struct hfs_cat_key *);
extern struct hfs_cat_entry *hfs_cat_parent(struct hfs_cat_entry *);

extern int hfs_cat_open(struct hfs_cat_entry *, struct hfs_brec *);
extern int hfs_cat_next(struct hfs_cat_entry *, struct hfs_brec *,
			hfs_u16, hfs_u32 *, hfs_u8 *);
extern void hfs_cat_close(struct hfs_cat_entry *, struct hfs_brec *);

extern int hfs_cat_create(struct hfs_cat_entry *, struct hfs_cat_key *,
			  hfs_u8, hfs_u32, hfs_u32, struct hfs_cat_entry **);
extern int hfs_cat_mkdir(struct hfs_cat_entry *, struct hfs_cat_key *,
			 struct hfs_cat_entry **);
extern int hfs_cat_delete(struct hfs_cat_entry *, struct hfs_cat_entry *, int);
extern int hfs_cat_move(struct hfs_cat_entry *, struct hfs_cat_entry *,
			struct hfs_cat_entry *, struct hfs_cat_key *,
			struct hfs_cat_entry **);

/* extent.c */
extern int hfs_ext_compare(const struct hfs_ext_key *,
			   const struct hfs_ext_key *);
extern void hfs_extent_in(struct hfs_fork *, const hfs_byte_t *);
extern void hfs_extent_out(const struct hfs_fork *, hfs_byte_t *);
extern int hfs_extent_map(struct hfs_fork *, int, int);
extern void hfs_extent_adj(struct hfs_fork *);
extern void hfs_extent_free(struct hfs_fork *);

/* file.c */
extern int hfs_get_block(struct inode *, long, struct buffer_head *, int);

/* mdb.c */
extern struct hfs_mdb *hfs_mdb_get(hfs_sysmdb, int, hfs_s32);
extern void hfs_mdb_commit(struct hfs_mdb *, int);
extern void hfs_mdb_put(struct hfs_mdb *, int);

/* part_tbl.c */
extern int hfs_part_find(hfs_sysmdb, int, int, hfs_s32 *, hfs_s32 *);

/* string.c */
extern unsigned int hfs_strhash(const unsigned char *, unsigned int);
extern int hfs_strcmp(const unsigned char *, unsigned int, 
		      const unsigned char *, unsigned int);
extern int hfs_streq(const unsigned char *, unsigned int, 
		     const unsigned char *, unsigned int);
extern void hfs_tolower(unsigned char *, int);

static __inline__ struct dentry 
*hfs_lookup_dentry(struct dentry *base, const char *name, const int len)
{
  struct qstr this;

  this.name = name;
  this.len = len;
  this.hash = hfs_strhash(name, len);

  return d_lookup(base, &this);
}

/* drop a dentry for one of the special directories.
 * it's in the form of base/name/dentry. */
static __inline__ void hfs_drop_special(struct dentry *base,
					const struct hfs_name *name,
					struct dentry *dentry)
{
  struct dentry *dparent, *de;
  
  dparent = hfs_lookup_dentry(base, name->Name, name->Len);
  if (dparent) {
	  de = hfs_lookup_dentry(dparent, dentry->d_name.name, 
				 dentry->d_name.len);
	  if (de) {
		  if (!de->d_inode)
			  d_drop(de);
		  dput(de);
	  }
	  dput(dparent);
  }
}

extern struct dentry_operations hfs_dentry_operations;
#endif
