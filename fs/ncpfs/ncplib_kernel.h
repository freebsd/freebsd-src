/*
 *  ncplib_kernel.h
 *
 *  Copyright (C) 1995, 1996 by Volker Lendecke
 *  Modified for big endian by J.F. Chadima and David S. Miller
 *  Modified 1997 Peter Waltenberg, Bill Hawes, David Woodhouse for 2.1 dcache
 *  Modified 1998, 1999 Wolfram Pienkoss for NLS
 *  Modified 1999 Wolfram Pienkoss for directory caching
 *
 */

#ifndef _NCPLIB_H
#define _NCPLIB_H

#include <linux/config.h>

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/pagemap.h>

#include <asm/uaccess.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <asm/string.h>

#ifdef CONFIG_NCPFS_NLS
#include <linux/nls.h>
#else
#include <linux/ctype.h>
#endif /* CONFIG_NCPFS_NLS */

#include <linux/ncp_fs.h>

#define NCP_MIN_SYMLINK_SIZE	8
#define NCP_MAX_SYMLINK_SIZE	512

#define NCP_BLOCK_SHIFT		9
#define NCP_BLOCK_SIZE		(1 << (NCP_BLOCK_SHIFT))

int ncp_negotiate_buffersize(struct ncp_server *, int, int *);
int ncp_negotiate_size_and_options(struct ncp_server *server, int size,
  			  int options, int *ret_size, int *ret_options);
int ncp_get_volume_info_with_number(struct ncp_server *, int,
				struct ncp_volume_info *);
int ncp_close_file(struct ncp_server *, const char *);
static inline int ncp_read_bounce_size(__u32 size) {
	return sizeof(struct ncp_reply_header) + 2 + 2 + size + 8;
};
int ncp_read_bounce(struct ncp_server *, const char *, __u32, __u16, 
		char *, int *, void* bounce, __u32 bouncelen);
int ncp_read_kernel(struct ncp_server *, const char *, __u32, __u16, 
		char *, int *);
int ncp_write_kernel(struct ncp_server *, const char *, __u32, __u16,
		const char *, int *);

static inline void ncp_inode_close(struct inode *inode) {
	atomic_dec(&NCP_FINFO(inode)->opened);
}

int ncp_obtain_info(struct ncp_server *server, struct inode *, char *,
		struct nw_info_struct *target);
int ncp_lookup_volume(struct ncp_server *, char *, struct nw_info_struct *);
int ncp_modify_file_or_subdir_dos_info(struct ncp_server *, struct inode *,
	 __u32, const struct nw_modify_dos_info *info);
int ncp_modify_file_or_subdir_dos_info_path(struct ncp_server *, struct inode *,
	 const char* path, __u32, const struct nw_modify_dos_info *info);

int ncp_del_file_or_subdir2(struct ncp_server *, struct dentry*);
int ncp_del_file_or_subdir(struct ncp_server *, struct inode *, char *);
int ncp_open_create_file_or_subdir(struct ncp_server *, struct inode *, char *,
				int, __u32, int, struct ncp_entry_info *);

int ncp_initialize_search(struct ncp_server *, struct inode *,
		      struct nw_search_sequence *target);
int ncp_search_for_file_or_subdir(struct ncp_server *server,
			      struct nw_search_sequence *seq,
			      struct nw_info_struct *target);

int ncp_ren_or_mov_file_or_subdir(struct ncp_server *server,
			      struct inode *, char *, struct inode *, char *);


int
ncp_LogPhysicalRecord(struct ncp_server *server,
		      const char *file_id, __u8 locktype,
		      __u32 offset, __u32 length, __u16 timeout);

#ifdef CONFIG_NCPFS_IOCTL_LOCKING
int
ncp_ClearPhysicalRecord(struct ncp_server *server,
			const char *file_id,
			__u32 offset, __u32 length);
#endif	/* CONFIG_NCPFS_IOCTL_LOCKING */

int
ncp_mount_subdir(struct ncp_server *, struct nw_info_struct *,
			__u8, __u8, __u32);

#ifdef CONFIG_NCPFS_NLS

unsigned char ncp__tolower(struct nls_table *, unsigned char);
unsigned char ncp__toupper(struct nls_table *, unsigned char);
int ncp__io2vol(struct ncp_server *, unsigned char *, unsigned int *,
				const unsigned char *, unsigned int, int);
int ncp__vol2io(struct ncp_server *, unsigned char *, unsigned int *,
				const unsigned char *, unsigned int, int);

#define NCP_ESC			':'
#define NCP_IO_TABLE(dentry)	(NCP_SERVER((dentry)->d_inode)->nls_io)
#define ncp_tolower(t, c)	ncp__tolower(t, c)
#define ncp_toupper(t, c)	ncp__toupper(t, c)
#define ncp_io2vol(S,m,i,n,k,U)	ncp__io2vol(S,m,i,n,k,U)
#define ncp_vol2io(S,m,i,n,k,U)	ncp__vol2io(S,m,i,n,k,U)

#else

int ncp__io2vol(unsigned char *, unsigned int *,
				const unsigned char *, unsigned int, int);
int ncp__vol2io(unsigned char *, unsigned int *,
				const unsigned char *, unsigned int, int);

#define NCP_IO_TABLE(dentry)	NULL
#define ncp_tolower(t, c)	tolower(c)
#define ncp_toupper(t, c)	toupper(c)
#define ncp_io2vol(S,m,i,n,k,U)	ncp__io2vol(m,i,n,k,U)
#define ncp_vol2io(S,m,i,n,k,U)	ncp__vol2io(m,i,n,k,U)

#endif /* CONFIG_NCPFS_NLS */

inline int
ncp_strnicmp(struct nls_table *,
		const unsigned char *, const unsigned char *, int);

#define NCP_GET_AGE(dentry)	(jiffies - (dentry)->d_time)
#define NCP_MAX_AGE(server)	((server)->dentry_ttl)
#define NCP_TEST_AGE(server,dentry)	(NCP_GET_AGE(dentry) < NCP_MAX_AGE(server))

static inline void
ncp_age_dentry(struct ncp_server* server, struct dentry* dentry)
{
	dentry->d_time = jiffies - server->dentry_ttl;
}

static inline void
ncp_new_dentry(struct dentry* dentry)
{
	dentry->d_time = jiffies;
}

static inline void
ncp_renew_dentries(struct dentry *parent)
{
	struct ncp_server *server = NCP_SERVER(parent->d_inode);
	struct list_head *next;
	struct dentry *dentry;

	spin_lock(&dcache_lock);
	next = parent->d_subdirs.next;
	while (next != &parent->d_subdirs) {
		dentry = list_entry(next, struct dentry, d_child);

		if (dentry->d_fsdata == NULL)
			ncp_age_dentry(server, dentry);
		else
			ncp_new_dentry(dentry);

		next = next->next;
	}
	spin_unlock(&dcache_lock);
}

static inline void
ncp_invalidate_dircache_entries(struct dentry *parent)
{
	struct ncp_server *server = NCP_SERVER(parent->d_inode);
	struct list_head *next;
	struct dentry *dentry;

	spin_lock(&dcache_lock);
	next = parent->d_subdirs.next;
	while (next != &parent->d_subdirs) {
		dentry = list_entry(next, struct dentry, d_child);
		dentry->d_fsdata = NULL;
		ncp_age_dentry(server, dentry);
		next = next->next;
	}
	spin_unlock(&dcache_lock);
}

struct ncp_cache_head {
	time_t		mtime;
	unsigned long	time;	/* cache age */
	unsigned long	end;	/* last valid fpos in cache */
	int		eof;
};

#define NCP_DIRCACHE_SIZE	((int)(PAGE_CACHE_SIZE/sizeof(struct dentry *)))
union ncp_dir_cache {
	struct ncp_cache_head	head;
	struct dentry		*dentry[NCP_DIRCACHE_SIZE];
};

#define NCP_FIRSTCACHE_SIZE	((int)((NCP_DIRCACHE_SIZE * \
	sizeof(struct dentry *) - sizeof(struct ncp_cache_head)) / \
	sizeof(struct dentry *)))

#define NCP_DIRCACHE_START	(NCP_DIRCACHE_SIZE - NCP_FIRSTCACHE_SIZE)

struct ncp_cache_control {
	struct	ncp_cache_head		head;
	struct	page			*page;
	union	ncp_dir_cache		*cache;
	unsigned long			fpos, ofs;
	int				filled, valid, idx;
};

#endif /* _NCPLIB_H */
