/*
 * struct.h - Structure definitions
 *
 * Copyright (C) 1997 Régis Duchesne
 * Copyright (C) 2000-2001 Anton Altaparmakov (AIA)
 */
#include <linux/ntfs_fs.h>

/* Necessary forward definition. */
struct ntfs_inode;

/* Which files should be returned from a director listing. */
#define ngt_dos   1	/* only short names, no system files */
#define ngt_nt    2	/* only long names, all-uppercase becomes 
			 * all-lowercase, no system files */
#define ngt_posix 3	/* all names except system files */
#define ngt_full  4	/* all entries */

typedef struct ntfs_sb_info ntfs_volume;

typedef struct {
	ntfs_cluster_t lcn;
	ntfs_cluster_t len;
} ntfs_runlist;

typedef struct ntfs_attribute {
	int type;
	ntfs_u16 *name;
	int namelen;
	int attrno;
	__s64 size, allocated, initialized, compsize;
	ATTR_FLAGS flags;
	__u8 resident, indexed;
	int cengine;
	union {
		void *data;             /* if resident */
		struct {
			ntfs_runlist *runlist;
			unsigned long len;
		} r;
	} d;
} ntfs_attribute;

typedef struct ntfs_inode_info ntfs_inode;

/* Structure to define IO to user buffer. do_read means that the destination
 * has to be written using fn_put, do_write means that the destination has to
 * read using fn_get. So, do_read is from a user's point of view, while put and
 * get are from the driver's point of view. The first argument is always the
 * destination of the IO. */
typedef struct ntfs_io{
	int do_read;
	void (*fn_put)(struct ntfs_io *dest, void *buf, ntfs_size_t);
	void (*fn_get)(void *buf, struct ntfs_io *src, ntfs_size_t len);
	void *param;
	unsigned long size;
} ntfs_io;

#if 0
typedef struct {
	ntfs_volume *vol;
	ntfs_inode *ino;
	int type;
	char *name;
	int mftno;
	int start_vcn;
} ntfs_attrlist_item;
#endif

