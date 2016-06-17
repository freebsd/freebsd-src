/*
 *  smb_fs_sb.h
 *
 *  Copyright (C) 1995 by Paal-Kr. Engstad and Volker Lendecke
 *  Copyright (C) 1997 by Volker Lendecke
 *
 */

#ifndef _SMB_FS_SB
#define _SMB_FS_SB

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/smb.h>

/* structure access macros */
#define server_from_inode(inode) (&(inode)->i_sb->u.smbfs_sb)
#define server_from_dentry(dentry) (&(dentry)->d_sb->u.smbfs_sb)
#define SB_of(server) ((struct super_block *) ((char *)(server) - \
	(unsigned long)(&((struct super_block *)0)->u.smbfs_sb)))

struct smb_sb_info {
        enum smb_conn_state state;
	struct file * sock_file;

        struct smb_mount_data_kernel *mnt;
        unsigned char *temp_buf;

	/* Connections are counted. Each time a new socket arrives,
	 * generation is incremented.
	 */
	unsigned int generation;
	pid_t conn_pid;
	struct smb_conn_opt opt;

	struct semaphore sem;
	wait_queue_head_t wait;

	__u32              packet_size;
	unsigned char *    packet;
        unsigned short     rcls; /* The error codes we received */
        unsigned short     err;

        /* We use our own data_ready callback, but need the original one */
        void *data_ready;

	/* nls pointers for codepage conversions */
	struct nls_table *remote_nls;
	struct nls_table *local_nls;

	/* utf8 can make strings longer so we can't do in-place conversion.
	   This is a buffer for temporary stuff. We only need one so no need
	   to put it on the stack. This points to temp_buf space. */
	char *name_buf;

	struct smb_ops *ops;
};


static inline void
smb_lock_server(struct smb_sb_info *server)
{
	down(&(server->sem));
}

static inline void
smb_unlock_server(struct smb_sb_info *server)
{
	up(&(server->sem));
}

#endif /* __KERNEL__ */

#endif
