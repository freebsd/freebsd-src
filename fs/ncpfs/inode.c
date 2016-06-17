/*
 *  inode.c
 *
 *  Copyright (C) 1995, 1996 by Volker Lendecke
 *  Modified for big endian by J.F. Chadima and David S. Miller
 *  Modified 1997 Peter Waltenberg, Bill Hawes, David Woodhouse for 2.1 dcache
 *  Modified 1998 Wolfram Pienkoss for NLS
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/init.h>

#include <linux/ncp_fs.h>

#include "ncplib_kernel.h"

static void ncp_delete_inode(struct inode *);
static void ncp_put_super(struct super_block *);
static int  ncp_statfs(struct super_block *, struct statfs *);

static struct super_operations ncp_sops =
{
	put_inode:	force_delete,
	delete_inode:	ncp_delete_inode,
	put_super:	ncp_put_super,
	statfs:		ncp_statfs,
};

extern struct dentry_operations ncp_root_dentry_operations;
#ifdef CONFIG_NCPFS_EXTRAS
extern struct address_space_operations ncp_symlink_aops;
extern int ncp_symlink(struct inode*, struct dentry*, const char*);
#endif

/*
 * Fill in the ncpfs-specific information in the inode.
 */
void ncp_update_inode(struct inode *inode, struct ncp_entry_info *nwinfo)
{
	NCP_FINFO(inode)->DosDirNum = nwinfo->i.DosDirNum;
	NCP_FINFO(inode)->dirEntNum = nwinfo->i.dirEntNum;
	NCP_FINFO(inode)->volNumber = nwinfo->i.volNumber;

#ifdef CONFIG_NCPFS_STRONG
	NCP_FINFO(inode)->nwattr = nwinfo->i.attributes;
#endif
	NCP_FINFO(inode)->access = nwinfo->access;
	NCP_FINFO(inode)->server_file_handle = nwinfo->server_file_handle;
	memcpy(NCP_FINFO(inode)->file_handle, nwinfo->file_handle,
			sizeof(nwinfo->file_handle));
	DPRINTK("ncp_update_inode: updated %s, volnum=%d, dirent=%u\n",
		nwinfo->i.entryName, NCP_FINFO(inode)->volNumber,
		NCP_FINFO(inode)->dirEntNum);
}

void ncp_update_inode2(struct inode* inode, struct ncp_entry_info *nwinfo)
{
	struct nw_info_struct *nwi = &nwinfo->i;
	struct ncp_server *server = NCP_SERVER(inode);

	if (!atomic_read(&NCP_FINFO(inode)->opened)) {
#ifdef CONFIG_NCPFS_STRONG
		NCP_FINFO(inode)->nwattr = nwi->attributes;
#endif
		if (nwi->attributes & aDIR) {
			inode->i_mode = server->m.dir_mode;
			inode->i_size = NCP_BLOCK_SIZE;
		} else {
			inode->i_mode = server->m.file_mode;
			inode->i_size = le32_to_cpu(nwi->dataStreamSize);
#ifdef CONFIG_NCPFS_EXTRAS
			if ((server->m.flags & (NCP_MOUNT_EXTRAS|NCP_MOUNT_SYMLINKS)) && (nwi->attributes & aSHARED)) {
				switch (nwi->attributes & (aHIDDEN|aSYSTEM)) {
					case aHIDDEN:
						if (server->m.flags & NCP_MOUNT_SYMLINKS) {
							if ( /* (inode->i_size >= NCP_MIN_SYMLINK_SIZE)
							 && */ (inode->i_size <= NCP_MAX_SYMLINK_SIZE)) {
								inode->i_mode = (inode->i_mode & ~S_IFMT) | S_IFLNK;
								break;
							}
						}
						/* FALLTHROUGH */
					case 0:
						if (server->m.flags & NCP_MOUNT_EXTRAS)
							inode->i_mode |= 0444;
						break;
					case aSYSTEM:
						if (server->m.flags & NCP_MOUNT_EXTRAS)
							inode->i_mode |= (inode->i_mode >> 2) & 0111;
						break;
					/* case aSYSTEM|aHIDDEN: */
					default:
						/* reserved combination */
						break;
				}
			}
#endif
		}
		if (nwi->attributes & aRONLY) inode->i_mode &= ~0222;
	}
	inode->i_blocks = (inode->i_size + NCP_BLOCK_SIZE - 1) >> NCP_BLOCK_SHIFT;

	inode->i_mtime = ncp_date_dos2unix(le16_to_cpu(nwi->modifyTime),
					   le16_to_cpu(nwi->modifyDate));
	inode->i_ctime = ncp_date_dos2unix(le16_to_cpu(nwi->creationTime),
					   le16_to_cpu(nwi->creationDate));
	inode->i_atime = ncp_date_dos2unix(0, le16_to_cpu(nwi->lastAccessDate));

	NCP_FINFO(inode)->DosDirNum = nwi->DosDirNum;
	NCP_FINFO(inode)->dirEntNum = nwi->dirEntNum;
	NCP_FINFO(inode)->volNumber = nwi->volNumber;
}

/*
 * Fill in the inode based on the ncp_entry_info structure.
 */
static void ncp_set_attr(struct inode *inode, struct ncp_entry_info *nwinfo)
{
	struct nw_info_struct *nwi = &nwinfo->i;
	struct ncp_server *server = NCP_SERVER(inode);

	if (nwi->attributes & aDIR) {
		inode->i_mode = server->m.dir_mode;
		/* for directories dataStreamSize seems to be some
		   Object ID ??? */
		inode->i_size = NCP_BLOCK_SIZE;
	} else {
		inode->i_mode = server->m.file_mode;
		inode->i_size = le32_to_cpu(nwi->dataStreamSize);
#ifdef CONFIG_NCPFS_EXTRAS
		if ((server->m.flags & (NCP_MOUNT_EXTRAS|NCP_MOUNT_SYMLINKS)) 
		 && (nwi->attributes & aSHARED)) {
			switch (nwi->attributes & (aHIDDEN|aSYSTEM)) {
				case aHIDDEN:
					if (server->m.flags & NCP_MOUNT_SYMLINKS) {
						if (/* (inode->i_size >= NCP_MIN_SYMLINK_SIZE)
						 && */ (inode->i_size <= NCP_MAX_SYMLINK_SIZE)) {
							inode->i_mode = (inode->i_mode & ~S_IFMT) | S_IFLNK;
							break;
						}
					}
					/* FALLTHROUGH */
				case 0:
					if (server->m.flags & NCP_MOUNT_EXTRAS)
						inode->i_mode |= 0444;
					break;
				case aSYSTEM:
					if (server->m.flags & NCP_MOUNT_EXTRAS)
						inode->i_mode |= (inode->i_mode >> 2) & 0111;
					break;
				/* case aSYSTEM|aHIDDEN: */
				default:
					/* reserved combination */
					break;
			}
		}
#endif
	}
	if (nwi->attributes & aRONLY) inode->i_mode &= ~0222;

	DDPRINTK("ncp_read_inode: inode->i_mode = %u\n", inode->i_mode);

	inode->i_nlink = 1;
	inode->i_uid = server->m.uid;
	inode->i_gid = server->m.gid;
	inode->i_rdev = 0;
	inode->i_blksize = NCP_BLOCK_SIZE;

	inode->i_blocks = (inode->i_size + NCP_BLOCK_SIZE - 1) >> NCP_BLOCK_SHIFT;

	inode->i_mtime = ncp_date_dos2unix(le16_to_cpu(nwi->modifyTime),
			  		   le16_to_cpu(nwi->modifyDate));
	inode->i_ctime = ncp_date_dos2unix(le16_to_cpu(nwi->creationTime),
			    		   le16_to_cpu(nwi->creationDate));
	inode->i_atime = ncp_date_dos2unix(0,
					   le16_to_cpu(nwi->lastAccessDate));
	ncp_update_inode(inode, nwinfo);
}

static struct inode_operations ncp_symlink_inode_operations = {
	readlink:	page_readlink,
	follow_link:	page_follow_link,
	setattr:	ncp_notify_change,
};

/*
 * Get a new inode.
 */
struct inode * 
ncp_iget(struct super_block *sb, struct ncp_entry_info *info)
{
	struct inode *inode;

	if (info == NULL) {
		printk(KERN_ERR "ncp_iget: info is NULL\n");
		return NULL;
	}

	inode = new_inode(sb);
	if (inode) {
		init_MUTEX(&NCP_FINFO(inode)->open_sem);
		atomic_set(&NCP_FINFO(inode)->opened, info->opened);

		inode->i_ino = info->ino;
		ncp_set_attr(inode, info);
		if (S_ISREG(inode->i_mode)) {
			inode->i_op = &ncp_file_inode_operations;
			inode->i_fop = &ncp_file_operations;
		} else if (S_ISDIR(inode->i_mode)) {
			inode->i_op = &ncp_dir_inode_operations;
			inode->i_fop = &ncp_dir_operations;
#ifdef CONFIG_NCPFS_EXTRAS
		} else if (S_ISLNK(inode->i_mode)) {
			inode->i_op = &ncp_symlink_inode_operations;
			inode->i_data.a_ops = &ncp_symlink_aops;
#endif
		}
		insert_inode_hash(inode);
	} else
		printk(KERN_ERR "ncp_iget: iget failed!\n");
	return inode;
}

static void
ncp_delete_inode(struct inode *inode)
{
	if (S_ISDIR(inode->i_mode)) {
		DDPRINTK("ncp_delete_inode: put directory %ld\n", inode->i_ino);
	}

	if (ncp_make_closed(inode) != 0) {
		/* We can't do anything but complain. */
		printk(KERN_ERR "ncp_delete_inode: could not close\n");
	}
	clear_inode(inode);
}

struct super_block *
ncp_read_super(struct super_block *sb, void *raw_data, int silent)
{
	struct ncp_mount_data_kernel data;
	struct ncp_server *server;
	struct file *ncp_filp;
	struct inode *root_inode;
	struct inode *sock_inode;
	struct socket *sock;
	int error;
	int default_bufsize;
#ifdef CONFIG_NCPFS_PACKET_SIGNING
	int options;
#endif
	struct ncp_entry_info finfo;

	if (raw_data == NULL)
		goto out_no_data;
	switch (*(int*)raw_data) {
		case NCP_MOUNT_VERSION:
			{
				struct ncp_mount_data* md = (struct ncp_mount_data*)raw_data;

				data.flags = md->flags;
				data.int_flags = NCP_IMOUNT_LOGGEDIN_POSSIBLE;
				data.mounted_uid = md->mounted_uid;
				data.wdog_pid = md->wdog_pid;
				data.ncp_fd = md->ncp_fd;
				data.time_out = md->time_out;
				data.retry_count = md->retry_count;
				data.uid = md->uid;
				data.gid = md->gid;
				data.file_mode = md->file_mode;
				data.dir_mode = md->dir_mode;
				memcpy(data.mounted_vol, md->mounted_vol,
					NCP_VOLNAME_LEN+1);
			}
			break;
		case NCP_MOUNT_VERSION_V4:
			{
				struct ncp_mount_data_v4* md = (struct ncp_mount_data_v4*)raw_data;

				data.flags = md->flags;
				data.int_flags = 0;
				data.mounted_uid = md->mounted_uid;
				data.wdog_pid = md->wdog_pid;
				data.ncp_fd = md->ncp_fd;
				data.time_out = md->time_out;
				data.retry_count = md->retry_count;
				data.uid = md->uid;
				data.gid = md->gid;
				data.file_mode = md->file_mode;
				data.dir_mode = md->dir_mode;
				data.mounted_vol[0] = 0;
			}
			break;
		default:
			goto out_bad_mount;
	}
	ncp_filp = fget(data.ncp_fd);
	if (!ncp_filp)
		goto out_bad_file;
	sock_inode = ncp_filp->f_dentry->d_inode;
	if (!S_ISSOCK(sock_inode->i_mode))
		goto out_bad_file2;
	sock = &sock_inode->u.socket_i;
	if (!sock)
		goto out_bad_file2;
		
	if (sock->type == SOCK_STREAM)
		default_bufsize = 61440;
	else
		default_bufsize = 1024;

	sb->s_blocksize = 1024;	/* Eh...  Is this correct? */
	sb->s_blocksize_bits = 10;
	sb->s_magic = NCP_SUPER_MAGIC;
	sb->s_op = &ncp_sops;

	server = NCP_SBP(sb);
	memset(server, 0, sizeof(*server));

	server->ncp_filp = ncp_filp;
/*	server->lock = 0;	*/
	init_MUTEX(&server->sem);
	server->packet = NULL;
/*	server->buffer_size = 0;	*/
/*	server->conn_status = 0;	*/
/*	server->root_dentry = NULL;	*/
/*	server->root_setuped = 0;	*/
#ifdef CONFIG_NCPFS_PACKET_SIGNING
/*	server->sign_wanted = 0;	*/
/*	server->sign_active = 0;	*/
#endif
	server->auth.auth_type = NCP_AUTH_NONE;
/*	server->auth.object_name_len = 0;	*/
/*	server->auth.object_name = NULL;	*/
/*	server->auth.object_type = 0;		*/
/*	server->priv.len = 0;			*/
/*	server->priv.data = NULL;		*/

	server->m = data;
	/* Althought anything producing this is buggy, it happens
	   now because of PATH_MAX changes.. */
	if (server->m.time_out < 1) {
		server->m.time_out = 10;
		printk(KERN_INFO "You need to recompile your ncpfs utils..\n");
	}
	server->m.time_out = server->m.time_out * HZ / 100;
	server->m.file_mode = (server->m.file_mode &
			       (S_IRWXU | S_IRWXG | S_IRWXO)) | S_IFREG;
	server->m.dir_mode = (server->m.dir_mode &
			      (S_IRWXU | S_IRWXG | S_IRWXO)) | S_IFDIR;

#ifdef CONFIG_NCPFS_NLS
	/* load the default NLS charsets */
	server->nls_vol = load_nls_default();
	server->nls_io = load_nls_default();
#endif /* CONFIG_NCPFS_NLS */

	server->dentry_ttl = 0;	/* no caching */

#undef NCP_PACKET_SIZE
#define NCP_PACKET_SIZE 65536
	server->packet_size = NCP_PACKET_SIZE;
	server->packet = vmalloc(NCP_PACKET_SIZE);
	if (server->packet == NULL)
		goto out_no_packet;

	ncp_lock_server(server);
	error = ncp_connect(server);
	ncp_unlock_server(server);
	if (error < 0)
		goto out_no_connect;
	DPRINTK("ncp_read_super: NCP_SBP(sb) = %x\n", (int) NCP_SBP(sb));

#ifdef CONFIG_NCPFS_PACKET_SIGNING
	if (ncp_negotiate_size_and_options(server, default_bufsize,
		NCP_DEFAULT_OPTIONS, &(server->buffer_size), &options) == 0)
	{
		if (options != NCP_DEFAULT_OPTIONS)
		{
			if (ncp_negotiate_size_and_options(server, 
				default_bufsize,
				options & 2, 
				&(server->buffer_size), &options) != 0)
				
			{
				goto out_no_bufsize;
			}
		}
		if (options & 2)
			server->sign_wanted = 1;
	}
	else 
#endif	/* CONFIG_NCPFS_PACKET_SIGNING */
	if (ncp_negotiate_buffersize(server, default_bufsize,
  				     &(server->buffer_size)) != 0)
		goto out_no_bufsize;
	DPRINTK("ncpfs: bufsize = %d\n", server->buffer_size);

	memset(&finfo, 0, sizeof(finfo));
	finfo.i.attributes	= aDIR;
	finfo.i.dataStreamSize	= NCP_BLOCK_SIZE;
	finfo.i.dirEntNum	= 0;
	finfo.i.DosDirNum	= 0;
#ifdef CONFIG_NCPFS_SMALLDOS
	finfo.i.NSCreator	= NW_NS_DOS;
#endif
	finfo.i.volNumber	= NCP_NUMBER_OF_VOLUMES + 1;	/* illegal volnum */
	/* set dates of mountpoint to Jan 1, 1986; 00:00 */
	finfo.i.creationTime	= finfo.i.modifyTime
				= cpu_to_le16(0x0000);
	finfo.i.creationDate	= finfo.i.modifyDate
				= finfo.i.lastAccessDate
				= cpu_to_le16(0x0C21);
	finfo.i.nameLen		= 0;
	finfo.i.entryName[0]	= '\0';

	finfo.opened		= 0;
	finfo.ino		= 2;	/* tradition */

	server->name_space[finfo.i.volNumber] = NW_NS_DOS;
        root_inode = ncp_iget(sb, &finfo);
        if (!root_inode)
		goto out_no_root;
	DPRINTK("ncp_read_super: root vol=%d\n", NCP_FINFO(root_inode)->volNumber);
	sb->s_root = d_alloc_root(root_inode);
        if (!sb->s_root)
		goto out_no_root;
	sb->s_root->d_op = &ncp_root_dentry_operations;
	return sb;

out_no_root:
	printk(KERN_ERR "ncp_read_super: get root inode failed\n");
	iput(root_inode);
	goto out_disconnect;
out_no_bufsize:
	printk(KERN_ERR "ncp_read_super: could not get bufsize\n");
out_disconnect:
	ncp_lock_server(server);
	ncp_disconnect(server);
	ncp_unlock_server(server);
	goto out_free_packet;
out_no_connect:
	printk(KERN_ERR "ncp_read_super: Failed connection, error=%d\n", error);
out_free_packet:
	vfree(server->packet);
	goto out_free_server;
out_no_packet:
	printk(KERN_ERR "ncp_read_super: could not alloc packet\n");
out_free_server:
#ifdef CONFIG_NCPFS_NLS
	unload_nls(server->nls_io);
	unload_nls(server->nls_vol);
#endif
	/* 23/12/1998 Marcin Dalecki <dalecki@cs.net.pl>:
	 * 
	 * The previously used put_filp(ncp_filp); was bogous, since
	 * it doesn't proper unlocking.
	 */
	fput(ncp_filp);
	goto out;

out_bad_file2:
	fput(ncp_filp);
out_bad_file:
	printk(KERN_ERR "ncp_read_super: invalid ncp socket\n");
	goto out;
out_bad_mount:
	printk(KERN_INFO "ncp_read_super: kernel requires mount version %d\n",
		NCP_MOUNT_VERSION);
	goto out;
out_no_data:
	printk(KERN_ERR "ncp_read_super: missing data argument\n");
out:
	return NULL;
}

static void ncp_put_super(struct super_block *sb)
{
	struct ncp_server *server = NCP_SBP(sb);

	ncp_lock_server(server);
	ncp_disconnect(server);
	ncp_unlock_server(server);

#ifdef CONFIG_NCPFS_NLS
	/* unload the NLS charsets */
	if (server->nls_vol)
	{
		unload_nls(server->nls_vol);
		server->nls_vol = NULL;
	}
	if (server->nls_io)
	{
		unload_nls(server->nls_io);
		server->nls_io = NULL;
	}
#endif /* CONFIG_NCPFS_NLS */

	fput(server->ncp_filp);
	kill_proc(server->m.wdog_pid, SIGTERM, 1);

	if (server->priv.data) 
		ncp_kfree_s(server->priv.data, server->priv.len);
	if (server->auth.object_name)
		ncp_kfree_s(server->auth.object_name, server->auth.object_name_len);
	vfree(server->packet);

}

static int ncp_statfs(struct super_block *sb, struct statfs *buf)
{
	/* We cannot say how much disk space is left on a mounted
	   NetWare Server, because free space is distributed over
	   volumes, and the current user might have disk quotas. So
	   free space is not that simple to determine. Our decision
	   here is to err conservatively. */

	buf->f_type = NCP_SUPER_MAGIC;
	buf->f_bsize = NCP_BLOCK_SIZE;
	buf->f_blocks = 0;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_namelen = 12;
	return 0;
}

int ncp_notify_change(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int result = 0;
	int info_mask;
	struct nw_modify_dos_info info;
	struct ncp_server *server;

	result = -EIO;

	server = NCP_SERVER(inode);
	if ((!server) || !ncp_conn_valid(server))
		goto out;

	/* ageing the dentry to force validation */
	ncp_age_dentry(server, dentry);

	result = inode_change_ok(inode, attr);
	if (result < 0)
		goto out;

	result = -EPERM;
	if (((attr->ia_valid & ATTR_UID) &&
	     (attr->ia_uid != server->m.uid)))
		goto out;

	if (((attr->ia_valid & ATTR_GID) &&
	     (attr->ia_gid != server->m.gid)))
		goto out;

	if (((attr->ia_valid & ATTR_MODE) &&
	     (attr->ia_mode &
	      ~(S_IFREG | S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO))))
		goto out;

	info_mask = 0;
	memset(&info, 0, sizeof(info));

#if 1 
        if ((attr->ia_valid & ATTR_MODE) != 0)
        {
                if (S_ISDIR(inode->i_mode)) {
                	umode_t newmode;

                	info_mask |= DM_ATTRIBUTES;
                	newmode = attr->ia_mode;
                	newmode &= NCP_SERVER(inode)->m.dir_mode;

                	if (newmode & 0222)
                		info.attributes &= ~(aRONLY|aRENAMEINHIBIT|aDELETEINHIBIT);
                	else
				info.attributes |=  (aRONLY|aRENAMEINHIBIT|aDELETEINHIBIT);
                } else if (!S_ISREG(inode->i_mode))
                {
                        return -EPERM;
                }
                else
                {
			umode_t newmode;
#ifdef CONFIG_NCPFS_EXTRAS			
			int extras;
			
			extras = server->m.flags & NCP_MOUNT_EXTRAS;
#endif
                        info_mask |= DM_ATTRIBUTES;
                        newmode=attr->ia_mode;
#ifdef CONFIG_NCPFS_EXTRAS
			if (!extras)
#endif
	                        newmode &= server->m.file_mode;

                        if (newmode & 0222) /* any write bit set */
                        {
                                info.attributes &= ~(aRONLY|aRENAMEINHIBIT|aDELETEINHIBIT);
                        }
                        else
                        {
                                info.attributes |=  (aRONLY|aRENAMEINHIBIT|aDELETEINHIBIT);
                        }
#ifdef CONFIG_NCPFS_EXTRAS
			if (extras) {
				if (newmode & 0111) /* any execute bit set */
					info.attributes |= aSHARED | aSYSTEM;
				/* read for group/world and not in default file_mode */
				else if (newmode & ~server->m.file_mode & 0444)
					info.attributes |= aSHARED;
			}
#endif
                }
        }
#endif

	if ((attr->ia_valid & ATTR_CTIME) != 0) {
		info_mask |= (DM_CREATE_TIME | DM_CREATE_DATE);
		ncp_date_unix2dos(attr->ia_ctime,
			     &(info.creationTime), &(info.creationDate));
		info.creationTime = le16_to_cpu(info.creationTime);
		info.creationDate = le16_to_cpu(info.creationDate);
	}
	if ((attr->ia_valid & ATTR_MTIME) != 0) {
		info_mask |= (DM_MODIFY_TIME | DM_MODIFY_DATE);
		ncp_date_unix2dos(attr->ia_mtime,
				  &(info.modifyTime), &(info.modifyDate));
		info.modifyTime = le16_to_cpu(info.modifyTime);
		info.modifyDate = le16_to_cpu(info.modifyDate);
	}
	if ((attr->ia_valid & ATTR_ATIME) != 0) {
		__u16 dummy;
		info_mask |= (DM_LAST_ACCESS_DATE);
		ncp_date_unix2dos(attr->ia_atime,
				  &(dummy), &(info.lastAccessDate));
		info.lastAccessDate = le16_to_cpu(info.lastAccessDate);
	}
	if (info_mask != 0) {
		result = ncp_modify_file_or_subdir_dos_info(NCP_SERVER(inode),
				      inode, info_mask, &info);
		if (result != 0) {
			result = -EACCES;

			if (info_mask == (DM_CREATE_TIME | DM_CREATE_DATE)) {
				/* NetWare seems not to allow this. I
				   do not know why. So, just tell the
				   user everything went fine. This is
				   a terrible hack, but I do not know
				   how to do this correctly. */
				result = 0;
			}
		}
#ifdef CONFIG_NCPFS_STRONG		
		if ((!result) && (info_mask & DM_ATTRIBUTES))
			NCP_FINFO(inode)->nwattr = info.attributes;
#endif
	}
	if ((attr->ia_valid & ATTR_SIZE) != 0) {
		int written;

		DPRINTK("ncpfs: trying to change size to %ld\n",
			attr->ia_size);

		if ((result = ncp_make_open(inode, O_WRONLY)) < 0) {
			return -EACCES;
		}
		ncp_write_kernel(NCP_SERVER(inode), NCP_FINFO(inode)->file_handle,
			  attr->ia_size, 0, "", &written);

		/* According to ndir, the changes only take effect after
		   closing the file */
		ncp_inode_close(inode);
		result = ncp_make_closed(inode);
		if (!result)
			result = vmtruncate(inode, attr->ia_size);
	}
out:
	return result;
}

#ifdef DEBUG_NCP_MALLOC
int ncp_malloced;
int ncp_current_malloced;
#endif

static DECLARE_FSTYPE(ncp_fs_type, "ncpfs", ncp_read_super, 0);

static int __init init_ncp_fs(void)
{
	DPRINTK("ncpfs: init_module called\n");

#ifdef DEBUG_NCP_MALLOC
	ncp_malloced = 0;
	ncp_current_malloced = 0;
#endif
	return register_filesystem(&ncp_fs_type);
}

static void __exit exit_ncp_fs(void)
{
	DPRINTK("ncpfs: cleanup_module called\n");
	unregister_filesystem(&ncp_fs_type);
#ifdef DEBUG_NCP_MALLOC
	PRINTK("ncp_malloced: %d\n", ncp_malloced);
	PRINTK("ncp_current_malloced: %d\n", ncp_current_malloced);
#endif
}

EXPORT_NO_SYMBOLS;

module_init(init_ncp_fs)
module_exit(exit_ncp_fs)
MODULE_LICENSE("GPL");
