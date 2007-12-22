/* $FreeBSD$ */
/* $Id: nfs4_vfs.h,v 1.4 2003/11/05 14:59:00 rees Exp $ */

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

#ifndef _NFS4CLIENT_NFS4_VFS_H
#define _NFS4CLIENT_NFS4_VFS_H

void nfs4_vfsop_fsinfo(struct nfsv4_fattr *, struct nfsmount *);
void nfs4_vfsop_statfs(struct nfsv4_fattr *, struct statfs *, struct mount *);

#endif /* _NFS4CLIENT_NFS4_VFS_H */
