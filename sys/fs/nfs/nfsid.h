/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef _NFS_NFSID_H_
#define	_NFS_NFSID_H_

/* Definitions for id<-->name mapping. */
struct nfsd_idargs {
	int		nid_flag;	/* Flags (see below) */
	uid_t		nid_uid;	/* user/group id */
	gid_t		nid_gid;
	int		nid_usermax;	/* Upper bound on user name cache */
	int		nid_usertimeout;/* User name timeout (minutes) */
	u_char		*nid_name;	/* Name */
	int		nid_namelen;	/* and its length */
	gid_t		*nid_grps;	/* and the list */
	int		nid_ngroup;	/* Size of groups list */
};

/* And bits for nid_flag */
#define	NFSID_INITIALIZE	0x0001
#define	NFSID_ADDUID		0x0002
#define	NFSID_DELUID		0x0004
#define	NFSID_ADDUSERNAME	0x0008
#define	NFSID_DELUSERNAME	0x0010
#define	NFSID_ADDGID		0x0020
#define	NFSID_DELGID		0x0040
#define	NFSID_ADDGROUPNAME	0x0080
#define	NFSID_DELGROUPNAME	0x0100
#define	NFSID_SYSSPACE		0x0200

#if defined(_KERNEL) || defined(KERNEL)
int nfssvc_idname(struct nfsd_idargs *);
#endif

#endif	/* _NFS_NFSID_H */
