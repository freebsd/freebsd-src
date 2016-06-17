/*
 * include/linux/nfsd/const.h
 *
 * Various constants related to NFS.
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_NFSD_CONST_H
#define _LINUX_NFSD_CONST_H

#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs3.h>

/*
 * Maximum protocol version supported by knfsd
 */
#define NFSSVC_MAXVERS		3

/*
 * Maximum blocksize supported by daemon currently at 8K
 */
#define NFSSVC_MAXBLKSIZE	(8*1024)

#ifdef __KERNEL__

#ifndef NFS_SUPER_MAGIC
# define NFS_SUPER_MAGIC	0x6969
#endif

#endif /* __KERNEL__ */

#endif /* _LINUX_NFSD_CONST_H */
