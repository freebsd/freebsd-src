/*
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
 * $FreeBSD: src/sys/nwfs/nwfs_subr.h,v 1.2.2.1 2000/04/17 08:34:20 bp Exp $
 */
#ifndef _NWFS_SUBR_H_
#define _NWFS_SUBR_H_

extern int nwfs_debuglevel;

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NWFSDATA);
#endif

struct ncp_conn;
struct ncp_nlstables;
struct ncp_open_info;
struct nw_entry_info;
struct nw_search_info;
struct nwmount;
struct proc;
struct timespec;
struct ucred;
struct vattr;
struct vnode;

int  ncp_initsearch(struct vnode *dvp,struct proc *p,struct ucred *cred);
int  ncp_search_for_file_or_subdir(struct nwmount *nmp,struct nw_search_seq *seq,
		struct nw_entry_info *target,
		struct proc *p,struct ucred *cred);
int  ncp_lookup(struct vnode *dvp, int len, char *name, struct nw_entry_info *fap,
		struct proc *p,struct ucred *cred);
int  ncp_lookup_volume(struct ncp_conn *conn, char *volname, 
		u_char *volNum, u_int32_t *dirEnt,
		struct proc *p,struct ucred *cred);
int  ncp_close_file(struct ncp_conn *conn, ncp_fh *fh,
		struct proc *p,struct ucred *cred);
int  ncp_open_create_file_or_subdir(struct nwmount *nmp,struct vnode *dvp, int namelen,char *name,
		int open_create_mode, u_int32_t create_attributes,
		int desired_acc_rights, struct ncp_open_info *nop,
		struct proc *p,struct ucred *cred);
int  ncp_DeleteNSEntry(struct nwmount *nmp, 
		u_int32_t dirent, int namelen, char *name,
		struct proc *p,struct ucred *cred);
int  ncp_nsrename(struct ncp_conn *conn, int volume, int ns, int oldtype, 
	struct ncp_nlstables *nt,
	nwdirent fdir, char *old_name, int oldlen,
	nwdirent tdir, char *new_name, int newlen,
	struct proc *p, struct ucred *cred);
int  ncp_obtain_info(struct nwmount *nmp, u_int32_t dirent,
		int namelen, char *path, struct nw_entry_info *target,
		struct proc *p,struct ucred *cred);
int  ncp_modify_file_or_subdir_dos_info(struct nwmount *nmp, struct vnode *vp, 
		u_int32_t info_mask,
		struct nw_modify_dos_info *info,
		struct proc *p,struct ucred *cred);
int  ncp_setattr(struct vnode *,struct vattr *,struct ucred *,struct proc *);
int  ncp_get_namespaces(struct ncp_conn *conn, u_int32_t volume, int *nsf,
		struct proc *p,struct ucred *cred);
int  ncp_get_volume_info_with_number(struct ncp_conn *conn, 
		int n, struct ncp_volume_info *target,
		struct proc *p,struct ucred *cred);

void ncp_unix2dostime (struct timespec *tsp, int tz, u_int16_t *ddp, 
	     u_int16_t *dtp, u_int8_t *dhp);
void ncp_dos2unixtime (u_int dd, u_int dt, u_int dh, int tz, struct timespec *tsp);

#endif /* !_NWFS_SUBR_H_ */
