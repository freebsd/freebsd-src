/*-
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/nwfs/nwfs.h,v 1.9 2005/01/14 08:09:42 phk Exp $
 */

#ifndef _NWFS_H_
#define _NWFS_H_

#include <netncp/ncp.h>
#include <fs/nwfs/nwfs_mount.h>

#define NR_OPEN 0
#define	NW_NSB_DOS	(1 << NW_NS_DOS)
#define	NW_NSB_MAC	(1 << NW_NS_MAC)
#define	NW_NSB_NFS	(1 << NW_NS_NFS)
#define	NW_NSB_FTAM	(1 << NW_NS_FTAM)
#define	NW_NSB_OS2	(1 << NW_NS_OS2)

#define	NWFSIOC_GETCONN		_IOR('n',1,int)
#define	NWFSIOC_GETEINFO	_IOR('n',2,struct nw_entry_info)
#define	NWFSIOC_GETNS		_IOR('n',3,int)

#ifdef _KERNEL

#include <sys/vnode.h>
#include <sys/mount.h>

struct nwfsnode;

struct nwmount {
	struct nwfs_args m;
	struct mount 	*mp;
	struct ncp_handle *connh;
	int 		name_space;
	struct nwnode	*n_root;
	u_int32_t	n_volume;
	ncpfid		n_rootent;
	int		n_id;
};

#define VFSTONWFS(mntp)		((struct nwmount *)((mntp)->mnt_data))
#define NWFSTOVFS(mnp)		((struct mount *)((mnp)->mount))
#define VTOVFS(vp)		((vp)->v_mount)
#define	VTONWFS(vp)		(VFSTONWFS(VTOVFS(vp)))
#define NWFSTOCONN(nmp)		((nmp)->connh->nh_conn)

int ncp_conn_logged_in(struct nwmount *);
int nwfs_ioctl(struct vop_ioctl_args *ap);
int nwfs_doio(struct vnode *vp, struct buf *bp, struct ucred *cr, struct thread *td);
int nwfs_vinvalbuf(struct vnode *vp, struct thread *td);

extern struct vop_vector nwfs_vnodeops;

#endif	/* _KERNEL */

#endif /* _NWFS_H_ */
