/*-
 * Copyright (c) 1999, 2001 Boris Popov
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
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#include <sys/malloc.h>
#include <sys/clock.h>
#include <sys/time.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_rq.h>
#include <netncp/nwerror.h>

#include <fs/nwfs/nwfs.h>
#include <fs/nwfs/nwfs_node.h>
#include <fs/nwfs/nwfs_subr.h>

#define NCP_INFOSZ	(sizeof(struct nw_entry_info) - 257)

MALLOC_DEFINE(M_NWFSDATA, "nwfs_data", "NWFS private data");

static int
ncp_extract_file_info(struct nwmount *nmp, struct ncp_rq *rqp,
	struct nw_entry_info *target, int withname)
{
	u_int8_t name_len;

	md_get_mem(&rqp->rp, (caddr_t)target, NCP_INFOSZ, MB_MSYSTEM);
	if (!withname)
		return 0;
	md_get_uint8(&rqp->rp, &name_len);
	target->nameLen = name_len;
	md_get_mem(&rqp->rp, (caddr_t)target->entryName, name_len, MB_MSYSTEM);
	target->entryName[name_len] = '\0';
	ncp_path2unix(target->entryName, target->entryName, name_len, &nmp->m.nls);
	return 0;
}

int
ncp_initsearch(struct vnode *dvp, struct thread *td, struct ucred *cred)
{
	struct nwmount *nmp = VTONWFS(dvp);
	struct ncp_conn *conn = NWFSTOCONN(nmp);
	struct nwnode *np = VTONW(dvp);
	struct ncp_rq *rqp;
	u_int8_t volnum = nmp->n_volume;
	u_int32_t dirent = np->n_fid.f_id;
	int error;

	NCPNDEBUG("vol=%d,dir=%d\n", volnum, dirent);
	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 2);		/* subfunction */
	mb_put_uint8(&rqp->rq, nmp->name_space);
	mb_put_uint8(&rqp->rq, 0);		/* reserved */
	ncp_rq_dbase_path(rqp, volnum, dirent, 0, NULL, NULL);
	rqp->nr_minrplen = sizeof(np->n_seq);
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_mem(&rqp->rp, (caddr_t)&np->n_seq, sizeof(np->n_seq), MB_MSYSTEM);
	ncp_rq_done(rqp);
	return 0;
}

int 
ncp_search_for_file_or_subdir(struct nwmount *nmp,
			      struct nw_search_seq *seq,
			      struct nw_entry_info *target,
			      struct thread *td,struct ucred *cred)
{
	struct ncp_conn *conn = NWFSTOCONN(nmp);
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 3);		/* subfunction */
	mb_put_uint8(&rqp->rq, nmp->name_space);
	mb_put_uint8(&rqp->rq, 0);		/* data stream */
	mb_put_uint16le(&rqp->rq, 0xffff);	/* Search attribs */
	mb_put_uint32le(&rqp->rq, IM_ALL);	/* return info mask */
	mb_put_mem(&rqp->rq, (caddr_t)seq, 9, MB_MSYSTEM);
	mb_put_uint8(&rqp->rq, 2);		/* 2 byte pattern */
	mb_put_uint8(&rqp->rq, 0xff);		/* following is a wildcard */
	mb_put_uint8(&rqp->rq, '*');
	rqp->nr_minrplen = sizeof(*seq) +  1 + NCP_INFOSZ + 1;
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_mem(&rqp->rp, (caddr_t)seq, sizeof(*seq), MB_MSYSTEM);
	md_get_uint8(&rqp->rp, NULL);		/* skip */
	error = ncp_extract_file_info(nmp, rqp, target, 1);
	ncp_rq_done(rqp);
	return error;
}

/*
 * Returns information for a (one-component) name relative to the specified
 * directory.
 */
int 
ncp_obtain_info(struct nwmount *nmp,  u_int32_t dirent,
		int namelen, char *path, struct nw_entry_info *target,
		struct thread *td,struct ucred *cred)
{
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	struct ncp_rq *rqp;
	int error;
	u_char volnum = nmp->n_volume, ns;

	if (target == NULL) {
		NCPFATAL("target == NULL\n");
		return EINVAL;
	}
	ns = (path == NULL || path[0] == 0) ? NW_NS_DOS : nmp->name_space;
	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 6);	/* subfunction */
	mb_put_uint8(&rqp->rq, ns);
	mb_put_uint8(&rqp->rq, ns);	/* DestNameSpace */
	mb_put_uint16le(&rqp->rq, 0xff);	/* get all */
	mb_put_uint32le(&rqp->rq, IM_ALL);
	ncp_rq_dbase_path(rqp, volnum, dirent, namelen, path, &nmp->m.nls);
	error = ncp_request(rqp);
	if (error)
		return error;
	error = ncp_extract_file_info(nmp, rqp, target, path != NULL);
	ncp_rq_done(rqp);
	return error;
}
/* 
 * lookup name pointed by cnp in directory dvp and return file info in np.
 * May be I should create a little cache, but another way is to minimize
 * number of calls, on other hand, in multiprocess environment ...
 */
int
ncp_lookup(struct vnode *dvp, int len, char *name, struct nw_entry_info *fap,
		struct thread *td,struct ucred *cred)
{
	struct nwmount *nmp;
	struct nwnode *dnp;
	int error;

	if (!dvp || dvp->v_type != VDIR) {
		nwfs_printf("dvp is NULL or not a directory.\n");
		return (ENOENT);
	}
	dnp = VTONW(dvp);
	nmp = VTONWFS(dvp);

	if (len == 1 && name[0] == '.') {
		if (dnp->n_flag & NVOLUME) {
			error = ncp_obtain_info(nmp, dnp->n_fid.f_id, 0, NULL,
				fap, td, cred);
		} else {
			error = ncp_obtain_info(nmp, dnp->n_fid.f_parent, 
				dnp->n_nmlen, dnp->n_name, fap, td, cred);
		}
		return error;
	} else if (len == 2 && name[0] == '.' && name[1] == '.') {
		printf("%s: knows NOTHING about '..'\n", __func__);
		return EIO;
	} else {
		error = ncp_obtain_info(nmp, dnp->n_fid.f_id, 
			len, name, fap, td, cred);
	}
	return error;
}

static void ConvertToNWfromDWORD(u_int32_t sfd, ncp_fh *fh);
static void 
ConvertToNWfromDWORD(u_int32_t sfd, ncp_fh *fh) {
	fh->val1 = (fh->val.val32 = sfd);
	return;
}

/*
 * If both dir and name are NULL, then in target there's already a looked-up
 * entry that wants to be opened.
 */
int 
ncp_open_create_file_or_subdir(struct nwmount *nmp,struct vnode *dvp,int namelen,
	    char *name, int open_create_mode, u_int32_t create_attributes,
	    int desired_acc_rights, struct ncp_open_info *nop,
	    struct thread *td,struct ucred *cred)
{
	
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	struct ncp_rq *rqp;
	u_int16_t search_attribs = SA_ALL & (~SA_SUBDIR_FILES);
	u_int8_t volnum;
	u_int32_t dirent;
	int error;

	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	volnum = nmp->n_volume;
	dirent = VTONW(dvp)->n_fid.f_id;
	if ((create_attributes & aDIR) != 0) {
		search_attribs |= SA_SUBDIR_FILES;
	}
	mb_put_uint8(&rqp->rq, 1);/* subfunction */
	mb_put_uint8(&rqp->rq, nmp->name_space);
	mb_put_uint8(&rqp->rq, open_create_mode);
	mb_put_uint16le(&rqp->rq, search_attribs);
	mb_put_uint32le(&rqp->rq, IM_ALL);
	mb_put_uint32le(&rqp->rq, create_attributes);
	/*
	 * The desired acc rights seem to be the inherited rights mask for
	 * directories
	 */
	mb_put_uint16le(&rqp->rq, desired_acc_rights);
	ncp_rq_dbase_path(rqp, volnum, dirent, namelen, name, &nmp->m.nls);
	error = ncp_request(rqp);
	if (error) {
		if (error == NWE_FILE_NO_CREATE_PRIV)
			error = EACCES;
		return error;
	}
	md_get_uint32le(&rqp->rp, &nop->origfh);
	md_get_uint8(&rqp->rp, &nop->action);
	md_get_uint8(&rqp->rp, NULL);	/* skip */
	error = ncp_extract_file_info(nmp, rqp, &nop->fattr, 1);
	ncp_rq_done(rqp);
	ConvertToNWfromDWORD(nop->origfh, &nop->fh);
	return error;
}

int
ncp_close_file(struct ncp_conn *conn, ncp_fh *fh,struct thread *td,struct ucred *cred)
{
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc(66, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 0);
	mb_put_mem(&rqp->rq, (caddr_t)fh, 6, MB_MSYSTEM);
	error = ncp_request(rqp);
	if (error)
		return error;
	ncp_rq_done(rqp);
	return error;
}

int
ncp_DeleteNSEntry(struct nwmount *nmp, u_int32_t dirent,
	int namelen,char *name,struct thread *td,struct ucred *cred)
{
	struct ncp_rq *rqp;
	int error;
	struct ncp_conn *conn=NWFSTOCONN(nmp);

	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 8);		/* subfunction */
	mb_put_uint8(&rqp->rq, nmp->name_space);
	mb_put_uint8(&rqp->rq, 0);		/* reserved */
	mb_put_uint16le(&rqp->rq, SA_ALL);	/* search attribs: all */
	ncp_rq_dbase_path(rqp, nmp->n_volume, dirent, namelen, name, &nmp->m.nls);
	error = ncp_request(rqp);
	if (!error)
		ncp_rq_done(rqp);
	return error;
}

int 
ncp_nsrename(struct ncp_conn *conn, int volume, int ns, int oldtype, 
	struct ncp_nlstables *nt,
	nwdirent fdir, char *old_name, int oldlen,
	nwdirent tdir, char *new_name, int newlen,
	struct thread *td, struct ucred *cred)
{
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 4);
	mb_put_uint8(&rqp->rq, ns);
	mb_put_uint8(&rqp->rq, 1);	/* RRenameToMySelf */
	mb_put_uint16le(&rqp->rq, oldtype);
	/* source Handle Path */
	mb_put_uint8(&rqp->rq, volume);
	mb_put_mem(&rqp->rq, (c_caddr_t)&fdir, sizeof(fdir), MB_MSYSTEM);
	mb_put_uint8(&rqp->rq, 1);
	mb_put_uint8(&rqp->rq, 1);	/* 1 source component */
	/* dest Handle Path */
	mb_put_uint8(&rqp->rq, volume);
	mb_put_mem(&rqp->rq, (c_caddr_t)&tdir, sizeof(tdir), MB_MSYSTEM);
	mb_put_uint8(&rqp->rq, 1);
	mb_put_uint8(&rqp->rq, 1);	/* 1 destination component */
	ncp_rq_pathstring(rqp, oldlen, old_name, nt);
	ncp_rq_pathstring(rqp, newlen, new_name, nt);
	error = ncp_request(rqp);
	if (!error)
		ncp_rq_done(rqp);
	return error;
}

int
ncp_modify_file_or_subdir_dos_info(struct nwmount *nmp, struct vnode *vp, 
				u_int32_t info_mask,
				struct nw_modify_dos_info *info,
				struct thread *td,struct ucred *cred)
{
	struct nwnode *np=VTONW(vp);
	struct ncp_rq *rqp;
	u_int8_t volnum = nmp->n_volume;
	u_int32_t dirent = np->n_fid.f_id;
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	int             error;

	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 7);	/* subfunction */
	mb_put_uint8(&rqp->rq, nmp->name_space);
	mb_put_uint8(&rqp->rq, 0);	/* reserved */
	mb_put_uint16le(&rqp->rq, SA_ALL);	/* search attribs: all */
	mb_put_uint32le(&rqp->rq, info_mask);
	mb_put_mem(&rqp->rq, (caddr_t)info, sizeof(*info), MB_MSYSTEM);
	ncp_rq_dbase_path(rqp, volnum, dirent, 0, NULL, NULL);
	error = ncp_request(rqp);
	if (!error)
		ncp_rq_done(rqp);
	return error;
}

int
ncp_setattr(vp, vap, cred, td)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct thread *td;
{
	struct nwmount *nmp=VTONWFS(vp);
	struct nwnode *np=VTONW(vp);
	struct ncp_open_info nwn;
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	struct nw_modify_dos_info info;
	struct ncp_rq *rqp;
	int error = 0, info_mask;

	if (vap->va_size != VNOVAL) {
		error = ncp_open_create_file_or_subdir(nmp, vp, 0, NULL, OC_MODE_OPEN, 0,
						   AR_WRITE | AR_READ, &nwn,td,cred);
		if (error)
			return error;
		error = ncp_rq_alloc(73, conn, td, cred, &rqp);
		if (error) {
			ncp_close_file(conn, &nwn.fh, td, cred);
			return error;
		}
		mb_put_uint8(&rqp->rq, 0);
		mb_put_mem(&rqp->rq, (caddr_t)&nwn.fh, 6, MB_MSYSTEM);
		mb_put_uint32be(&rqp->rq, vap->va_size);
		mb_put_uint16be(&rqp->rq, 0);
		error = ncp_request(rqp);
		np->n_vattr.va_size = np->n_size = vap->va_size;
		if (!error)
			ncp_rq_done(rqp);
		ncp_close_file(conn, &nwn.fh, td, cred);
		if (error)
			return error;
	}
	info_mask = 0;
	bzero(&info, sizeof(info));

	if (vap->va_mtime.tv_sec != VNOVAL) {
		info_mask |= (DM_MODIFY_TIME | DM_MODIFY_DATE);
		ncp_unix2dostime(&vap->va_mtime, nmp->m.tz, &info.modifyDate, &info.modifyTime, NULL);
	}
	if (vap->va_atime.tv_sec != VNOVAL) {
		info_mask |= (DM_LAST_ACCESS_DATE);
		ncp_unix2dostime(&vap->va_atime, nmp->m.tz, &info.lastAccessDate, NULL, NULL);
	}
	if (info_mask) {
		error = ncp_modify_file_or_subdir_dos_info(nmp, vp, info_mask, &info,td,cred);
	}
	return (error);
}

int
ncp_get_volume_info_with_number(struct ncp_conn *conn, 
	int n, struct ncp_volume_info *target,
	struct thread *td,struct ucred *cred)
{
	struct ncp_rq *rqp;
	u_int32_t tmp32;
	u_int8_t len;
	int error;

	error = ncp_rq_alloc_subfn(22, 44, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq,n);
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_uint32le(&rqp->rp, &target->total_blocks);
	md_get_uint32le(&rqp->rp, &target->free_blocks);
	md_get_uint32le(&rqp->rp, &target->purgeable_blocks);
	md_get_uint32le(&rqp->rp, &target->not_yet_purgeable_blocks);
	md_get_uint32le(&rqp->rp, &target->total_dir_entries);
	md_get_uint32le(&rqp->rp, &target->available_dir_entries);
	md_get_uint32le(&rqp->rp, &tmp32);
	md_get_uint8(&rqp->rp, &target->sectors_per_block);
	bzero(&target->volume_name, sizeof(target->volume_name));
	md_get_uint8(&rqp->rp, &len);
	if (len > NCP_VOLNAME_LEN) {
		error = ENAMETOOLONG;
	} else {
		md_get_mem(&rqp->rp, (caddr_t)&target->volume_name, len, MB_MSYSTEM);
	}
	ncp_rq_done(rqp);
	return error;
}

int
ncp_get_namespaces(struct ncp_conn *conn, u_int32_t volume, int *nsf,
	struct thread *td,struct ucred *cred)
{
	struct ncp_rq *rqp;
	int error;
	u_int8_t ns;
	u_int16_t nscnt;

	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 24);	/* Subfunction: Get Loaded Name Spaces */
	mb_put_uint16le(&rqp->rq, 0);	/* reserved */
	mb_put_uint8(&rqp->rq, volume);
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_uint16le(&rqp->rp, &nscnt);
	*nsf = 0;
	while (nscnt-- > 0) {
		md_get_uint8(&rqp->rp, &ns);
		*nsf |= 1 << ns;
	}
	ncp_rq_done(rqp);
	return error;
}

int
ncp_lookup_volume(struct ncp_conn *conn, char *volname, 
		u_char *volNum, u_int32_t *dirEnt,
		struct thread *td,struct ucred *cred)
{
	struct ncp_rq *rqp;
	u_int32_t tmp32;
	int error;

	NCPNDEBUG("looking up vol %s\n", volname);
	error = ncp_rq_alloc(87, conn, td, cred, &rqp);
	if (error)
		return error;
	mb_put_uint8(&rqp->rq, 22);	/* Subfunction: Generate dir handle */
	mb_put_uint8(&rqp->rq, 0);	/* src name space */
	mb_put_uint8(&rqp->rq, 0);	/* dst name space, always zero */
	mb_put_uint16le(&rqp->rq, 0);	/* dstNSIndicator (Jn) */

	mb_put_uint8(&rqp->rq, 0);	/* faked volume number */
	mb_put_uint32be(&rqp->rq, 0);	/* faked dir_base */
	mb_put_uint8(&rqp->rq, 0xff);	/* Don't have a dir_base */
	mb_put_uint8(&rqp->rq, 1);	/* 1 path component */
	ncp_rq_pstring(rqp, volname);
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_uint32le(&rqp->rp, &tmp32);
	md_get_uint32le(&rqp->rp, dirEnt);
	md_get_uint8(&rqp->rp, volNum);
	ncp_rq_done(rqp);
	return error;
}

/*
 * XXX: I think the timezone in struct nwfs_args is truly bogus, especially
 * XXX: considering that nwfs_mount(8) picks this up from the kernel in
 * XXX: the first place.  Since I can't test this, I won't attempt to fix it.
 * XXX: /phk
 */

void
ncp_unix2dostime(tsp, tzoff, ddp, dtp, dhp)
	struct timespec *tsp;
	int tzoff;
	u_int16_t *ddp;
	u_int16_t *dtp;
	u_int8_t *dhp;
{
	struct timespec t;

	t = *tsp;
	
	t.tv_sec = - tzoff * 60 - utc_offset();
	timespec2fattime(&t, 1, ddp, dtp, dhp);
}

void
ncp_dos2unixtime(dd, dt, dh, tzoff, tsp)
	u_int dd;
	u_int dt;
	u_int dh;
	int tzoff;
	struct timespec *tsp;
{

	fattime2timespec(dd, dt, dh, 1, tsp);
	tsp->tv_sec += tzoff * 60 + utc_offset();
}
