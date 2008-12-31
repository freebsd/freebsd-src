/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)portal.h	8.4 (Berkeley) 1/21/94
 *
 * $FreeBSD: src/sys/fs/portalfs/portal.h,v 1.10.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

struct portal_args {
	char		*pa_config;	/* Config file */
	int		pa_socket;	/* Socket to server */
};

struct portal_cred {
	int		pcr_flag;		/* File open mode */
	uid_t		pcr_uid;		/* From ucred */
	short		pcr_ngroups;		/* From ucred */
	gid_t		pcr_groups[NGROUPS];	/* From ucred */
};

#ifdef _KERNEL
struct portalmount {
	struct vnode	*pm_root;	/* Root node */
	struct file	*pm_server;	/* Held reference to server socket */
};

struct portalnode {
	int		pt_size;	/* Length of Arg */
	char		*pt_arg;	/* Arg to send to server */
	int		pt_fileid;	/* cookie */
};

#define VFSTOPORTAL(mp)	((struct portalmount *)((mp)->mnt_data))
#define	VTOPORTAL(vp) ((struct portalnode *)(vp)->v_data)

#define PORTAL_ROOTFILEID	2

extern struct vop_vector portal_vnodeops;
#endif /* _KERNEL */
