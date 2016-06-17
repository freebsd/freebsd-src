/*
 *  ncp_fs_sb.h
 *
 *  Copyright (C) 1995, 1996 by Volker Lendecke
 *
 */

#ifndef _NCP_FS_SB
#define _NCP_FS_SB

#include <linux/types.h>
#include <linux/ncp_mount.h>

#ifdef __KERNEL__

#define NCP_DEFAULT_OPTIONS 0		/* 2 for packet signatures */

struct ncp_server {

	struct ncp_mount_data_kernel m;	/* Nearly all of the mount data is of
					   interest for us later, so we store
					   it completely. */

	__u8 name_space[NCP_NUMBER_OF_VOLUMES + 2];

	struct file *ncp_filp;	/* File pointer to ncp socket */

	u8 sequence;
	u8 task;
	u16 connection;		/* Remote connection number */

	u8 completion;		/* Status message from server */
	u8 conn_status;		/* Bit 4 = 1 ==> Server going down, no
				   requests allowed anymore.
				   Bit 0 = 1 ==> Server is down. */

	int buffer_size;	/* Negotiated bufsize */

	int reply_size;		/* Size of last reply */

	int packet_size;
	unsigned char *packet;	/* Here we prepare requests and
				   receive replies */

	int lock;		/* To prevent mismatch in protocols. */
	struct semaphore sem;

	int current_size;	/* for packet preparation */
	int has_subfunction;
	int ncp_reply_size;

	int root_setuped;

	/* info for packet signing */
	int sign_wanted;	/* 1=Server needs signed packets */
	int sign_active;	/* 0=don't do signing, 1=do */
	char sign_root[8];	/* generated from password and encr. key */
	char sign_last[16];	

	/* Authentication info: NDS or BINDERY, username */
	struct {
		int	auth_type;
		size_t	object_name_len;
		void*	object_name;
		int	object_type;
	} auth;
	/* Password info */
	struct {
		size_t	len;
		void*	data;
	} priv;

	/* nls info: codepage for volume and charset for I/O */
	struct nls_table *nls_vol;
	struct nls_table *nls_io;

	/* maximum age in jiffies */
	int dentry_ttl;

	/* miscellaneous */
	unsigned int flags;
};

#define ncp_sb_info	ncp_server

#define NCP_FLAG_UTF8	1

#define NCP_CLR_FLAG(server, flag)	((server)->flags &= ~(flag))
#define NCP_SET_FLAG(server, flag)	((server)->flags |= (flag))
#define NCP_IS_FLAG(server, flag)	((server)->flags & (flag))

static inline int ncp_conn_valid(struct ncp_server *server)
{
	return ((server->conn_status & 0x11) == 0);
}

static inline void ncp_invalidate_conn(struct ncp_server *server)
{
	server->conn_status |= 0x01;
}

#endif				/* __KERNEL__ */

#endif
 
