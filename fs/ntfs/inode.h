/*
 * inode.h -  Header file for inode.c
 *
 * Copyright (C) 1997 Régis Duchesne
 * Copyright (C) 1998 Martin von Löwis
 * Copyright (c) 2001 Anton Altparmakov (AIA)
 */

ntfs_attribute *ntfs_find_attr(ntfs_inode *ino, int type, char *name);

int ntfs_read_attr(ntfs_inode *ino, int type, char *name, __s64 offset,
		ntfs_io *buf);

int ntfs_write_attr(ntfs_inode *ino, int type, char *name, __s64 offset,
		ntfs_io *buf);

int ntfs_init_inode(ntfs_inode *ino, ntfs_volume *vol, int inum);

void ntfs_clear_inode(ntfs_inode *ino);

int ntfs_check_mft_record(ntfs_volume *vol, char *record);

int ntfs_alloc_inode(ntfs_inode *dir, ntfs_inode *result, const char *filename,
		int namelen, ntfs_u32);

int ntfs_alloc_file(ntfs_inode *dir, ntfs_inode *result, char *filename,
		int namelen);

int ntfs_update_inode(ntfs_inode *ino);

int ntfs_vcn_to_lcn(ntfs_inode *ino, int vcn);

int ntfs_readwrite_attr(ntfs_inode *ino, ntfs_attribute *attr, __s64 offset,
		ntfs_io *dest);

int ntfs_allocate_attr_number(ntfs_inode *ino, int *result);

int ntfs_decompress_run(unsigned char **data, int *length,
		ntfs_cluster_t *cluster, int *ctype);

void ntfs_decompress(unsigned char *dest, unsigned char *src, ntfs_size_t l);

int splice_runlists(ntfs_runlist **rl1, int *r1len, const ntfs_runlist *rl2,
		int r2len);

/*
 * NOTE: Neither of the ntfs_*_bit functions are atomic! But we don't need
 * them atomic at present as we never operate on shared/cached bitmaps.
 */
static __inline__ int ntfs_test_and_set_bit(unsigned char *byte, const int bit)
{
	unsigned char *ptr = byte + (bit >> 3);
	int b = 1 << (bit & 7);
	int oldbit = *ptr & b ? 1 : 0;
	*ptr |= b;
	return oldbit;
}

