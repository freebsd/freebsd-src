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
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/clock.h>
#include <sys/time.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_rq.h>
#include <netncp/nwerror.h>

#include <nwfs/nwfs.h>
#include <nwfs/nwfs_node.h>
#include <nwfs/nwfs_subr.h>

MALLOC_DEFINE(M_NWFSDATA, "NWFS data", "NWFS private data");

static void 
ncp_extract_file_info(struct nwmount *nmp, struct ncp_rq *rqp, struct nw_entry_info *target) {
	u_char name_len;
	const int info_struct_size = sizeof(struct nw_entry_info) - 257;

	ncp_rp_mem(rqp,(caddr_t)target,info_struct_size);
	name_len = ncp_rp_byte(rqp);
	target->nameLen = name_len;
	ncp_rp_mem(rqp,(caddr_t)target->entryName, name_len);
	target->entryName[name_len] = '\0';
	ncp_path2unix(target->entryName, target->entryName, name_len, &nmp->m.nls);
	return;
}

static void 
ncp_update_file_info(struct nwmount *nmp, struct ncp_rq *rqp, 
	struct nw_entry_info *target)
{
	int info_struct_size = sizeof(struct nw_entry_info) - 257;

	ncp_rp_mem(rqp,(caddr_t)target,info_struct_size);
	return;
}

int
ncp_initsearch(struct vnode *dvp,struct proc *p,struct ucred *cred)
{
	struct nwmount *nmp = VTONWFS(dvp);
	struct ncp_conn *conn = NWFSTOCONN(nmp);
	struct nwnode *np = VTONW(dvp);
	u_int8_t volnum = nmp->n_volume;
	u_int32_t dirent = np->n_fid.f_id;
	int error;
	DECLARE_RQ;

	NCPNDEBUG("vol=%d,dir=%d\n", volnum, dirent);
	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 2);		/* subfunction */
	ncp_rq_byte(rqp, nmp->name_space);
	ncp_rq_byte(rqp, 0);		/* reserved */
	ncp_rq_dbase_path(rqp, volnum, dirent, 0, NULL, NULL);
	checkbad(ncp_request(conn,rqp));
	ncp_rp_mem(rqp,(caddr_t)&np->n_seq, sizeof(np->n_seq));
	NCP_RQ_EXIT;
	return error;
}

int 
ncp_search_for_file_or_subdir(struct nwmount *nmp,
			      struct nw_search_seq *seq,
			      struct nw_entry_info *target,
			      struct proc *p,struct ucred *cred)
{
	struct ncp_conn *conn = NWFSTOCONN(nmp);
	int error;
	DECLARE_RQ;

	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 3);		/* subfunction */
	ncp_rq_byte(rqp, nmp->name_space);
	ncp_rq_byte(rqp, 0);		/* data stream */
	ncp_rq_word_lh(rqp, 0xffff);	/* Search attribs */
	ncp_rq_dword(rqp, IM_ALL);	/* return info mask */
	ncp_rq_mem(rqp, (caddr_t)seq, 9);
	ncp_rq_byte(rqp, 2);		/* 2 byte pattern */
	ncp_rq_byte(rqp, 0xff);		/* following is a wildcard */
	ncp_rq_byte(rqp, '*');
	checkbad(ncp_request(conn,rqp));
	ncp_rp_mem(rqp,(caddr_t)seq, sizeof(*seq));
	ncp_rp_byte(rqp);		/* skip */
	ncp_extract_file_info(nmp, rqp, target);
	NCP_RQ_EXIT;
	return error;
}

/*
 * Returns information for a (one-component) name relative to the specified
 * directory.
 */
int 
ncp_obtain_info(struct nwmount *nmp,  u_int32_t dirent,
		int namelen, char *path, struct nw_entry_info *target,
		struct proc *p,struct ucred *cred)
{
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	int error;
	u_char volnum = nmp->n_volume, ns;
	DECLARE_RQ;

	if (target == NULL) {
		NCPFATAL("target == NULL\n");
		return EINVAL;
	}
	ns = (path == NULL || path[0] == 0) ? NW_NS_DOS : nmp->name_space;
	NCP_RQ_HEAD(87, p, cred);
	ncp_rq_byte(rqp, 6);			/* subfunction */
	ncp_rq_byte(rqp, ns);
	ncp_rq_byte(rqp, ns);	/* DestNameSpace */
	ncp_rq_word(rqp, htons(0xff00));	/* get all */
	ncp_rq_dword(rqp, IM_ALL);
	ncp_rq_dbase_path(rqp, volnum, dirent, namelen, path, &nmp->m.nls);
	checkbad(ncp_request(conn,rqp));
	if (path)
		ncp_extract_file_info(nmp, rqp, target);
	else
		ncp_update_file_info(nmp, rqp, target);
	NCP_RQ_EXIT;
	return error;
}
/* 
 * lookup name pointed by cnp in directory dvp and return file info in np.
 * May be I should create a little cache, but another way is to minimize
 * number of calls, on other hand, in multiprocess environment ...
 */
int
ncp_lookup(struct vnode *dvp, int len, char *name, struct nw_entry_info *fap,
		struct proc *p,struct ucred *cred)
{
	struct nwmount *nmp;
	struct nwnode *dnp = VTONW(dvp);
	struct ncp_conn *conn;
	int error;

	if (!dvp || dvp->v_type != VDIR) {
		nwfs_printf("dvp is NULL or not a directory.\n");
		return (ENOENT);
	}
	nmp = VTONWFS(dvp);
	conn = NWFSTOCONN(nmp);

	if (len == 1 && name[0] == '.') {
		if (strcmp(dnp->n_name, NWFS_ROOTVOL) == 0) {
			error = ncp_obtain_info(nmp, dnp->n_fid.f_id, 0, NULL,
				fap, p, cred);
		} else {
			error = ncp_obtain_info(nmp, dnp->n_fid.f_parent, 
				dnp->n_nmlen, dnp->n_name, fap, p, cred);
		}
		return error;
	} else if (len == 2 && name[0] == '.' && name[1] == '.') {
		printf("%s: knows NOTHING about '..'\n", __FUNCTION__);
		return EIO;
	} else {
		error = ncp_obtain_info(nmp, dnp->n_fid.f_id, 
			len, name, fap, p, cred);
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
	    struct proc *p,struct ucred *cred)
{
	
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	u_int16_t search_attribs = SA_ALL & (~SA_SUBDIR_FILES);
	u_int8_t volnum;
	u_int32_t dirent;
	int error;
	DECLARE_RQ;

	volnum = nmp->n_volume;
	dirent = VTONW(dvp)->n_fid.f_id;
	if ((create_attributes & aDIR) != 0) {
		search_attribs |= SA_SUBDIR_FILES;
	}
	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 1);/* subfunction */
	ncp_rq_byte(rqp, nmp->name_space);
	ncp_rq_byte(rqp, open_create_mode);
	ncp_rq_word(rqp, search_attribs);
	ncp_rq_dword(rqp, IM_ALL);
	ncp_rq_dword(rqp, create_attributes);
	/*
	 * The desired acc rights seem to be the inherited rights mask for
	 * directories
	 */
	ncp_rq_word(rqp, desired_acc_rights);
	ncp_rq_dbase_path(rqp, volnum, dirent, namelen, name, &nmp->m.nls);
	checkbad(ncp_request(conn,rqp));

	nop->origfh = ncp_rp_dword_lh(rqp);
	nop->action = ncp_rp_byte(rqp);
	ncp_rp_byte(rqp);	/* skip */
	ncp_extract_file_info(nmp, rqp, &nop->fattr);
	ConvertToNWfromDWORD(nop->origfh, &nop->fh);
	NCP_RQ_EXIT;
	switch(error) {
	    case NWE_FILE_NO_CREATE_PRIV:
		error = EACCES;
		break;
	}
	return error;
}

int
ncp_close_file(struct ncp_conn *conn, ncp_fh *fh,struct proc *p,struct ucred *cred) {
	int error;
	DECLARE_RQ;

	NCP_RQ_HEAD(66,p,cred);
	ncp_rq_byte(rqp, 0);
	ncp_rq_mem(rqp, (caddr_t)fh, 6);
	error = ncp_request(conn,rqp);
	NCP_RQ_EXIT_NB;
	return error;
}

int
ncp_DeleteNSEntry(struct nwmount *nmp, u_int32_t dirent,
			int namelen,char *name,struct proc *p,struct ucred *cred)
{
	int error;
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	DECLARE_RQ;

	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 8);		/* subfunction */
	ncp_rq_byte(rqp, nmp->name_space);
	ncp_rq_byte(rqp, 0);		/* reserved */
	ncp_rq_word(rqp, SA_ALL);	/* search attribs: all */
	ncp_rq_dbase_path(rqp, nmp->n_volume, dirent, namelen, name, &nmp->m.nls);
	error = ncp_request(conn,rqp);
	NCP_RQ_EXIT_NB;
	return error;
}

int 
ncp_nsrename(struct ncp_conn *conn, int volume, int ns, int oldtype, 
	struct ncp_nlstables *nt,
	nwdirent fdir, char *old_name, int oldlen,
	nwdirent tdir, char *new_name, int newlen,
	struct proc *p, struct ucred *cred)
{
	DECLARE_RQ;
	int error;

	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 4);
	ncp_rq_byte(rqp, ns);
	ncp_rq_byte(rqp, 1);
	ncp_rq_word(rqp, oldtype);
	/* source Handle Path */
	ncp_rq_byte(rqp, volume);
	ncp_rq_dword(rqp, fdir);
	ncp_rq_byte(rqp, 1);
	ncp_rq_byte(rqp, 1);	/* 1 source component */
	/* dest Handle Path */
	ncp_rq_byte(rqp, volume);
	ncp_rq_dword(rqp, tdir);
	ncp_rq_byte(rqp, 1);
	ncp_rq_byte(rqp, 1);	/* 1 destination component */
	ncp_rq_pathstring(rqp, oldlen, old_name, nt);
	ncp_rq_pathstring(rqp, newlen, new_name, nt);
	error = ncp_request(conn,rqp);
	NCP_RQ_EXIT_NB;
	return error;
}

int
ncp_modify_file_or_subdir_dos_info(struct nwmount *nmp, struct vnode *vp, 
				u_int32_t info_mask,
				struct nw_modify_dos_info *info,
				struct proc *p,struct ucred *cred)
{
	struct nwnode *np=VTONW(vp);
	u_int8_t volnum = nmp->n_volume;
	u_int32_t dirent = np->n_fid.f_id;
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	int             error;
	DECLARE_RQ;

	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 7);	/* subfunction */
	ncp_rq_byte(rqp, nmp->name_space);
	ncp_rq_byte(rqp, 0);	/* reserved */
	ncp_rq_word(rqp, htons(0x0680));	/* search attribs: all */
	ncp_rq_dword(rqp, info_mask);
	ncp_rq_mem(rqp, (caddr_t)info, sizeof(*info));
	ncp_rq_dbase_path(rqp, volnum, dirent, 0, NULL, NULL);
	error = ncp_request(conn,rqp);
	NCP_RQ_EXIT_NB;
	return error;
}

int
ncp_setattr(vp, vap, cred, procp)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *procp;
{
	struct nwmount *nmp=VTONWFS(vp);
	struct nwnode *np=VTONW(vp);
	struct ncp_open_info nwn;
	struct ncp_conn *conn=NWFSTOCONN(nmp);
	struct nw_modify_dos_info info;
	int error = 0, info_mask;
	DECLARE_RQ;

	if (vap->va_size != VNOVAL) {
		error = ncp_open_create_file_or_subdir(nmp, vp, 0, NULL, OC_MODE_OPEN, 0,
						   AR_WRITE | AR_READ, &nwn,procp,cred);
		if (error) return error;
		NCP_RQ_HEAD(73,procp,cred);
		ncp_rq_byte(rqp, 0);
		ncp_rq_mem(rqp, (caddr_t)&nwn.fh, 6);
		ncp_rq_dword(rqp, htonl(vap->va_size));
		ncp_rq_word_hl(rqp, 0);
		checkbad(ncp_request(conn,rqp));
		np->n_vattr.va_size = np->n_size = vap->va_size;
		NCP_RQ_EXIT;
		ncp_close_file(conn, &nwn.fh, procp, cred);
		if (error) return error;
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
		error = ncp_modify_file_or_subdir_dos_info(nmp, vp, info_mask, &info,procp,cred);
	}
	return (error);
}

int
ncp_get_volume_info_with_number(struct ncp_conn *conn, 
	    int n, struct ncp_volume_info *target,
	    struct proc *p,struct ucred *cred) {
	int error,len;
	DECLARE_RQ;

	NCP_RQ_HEAD_S(22,44,p,cred);
	ncp_rq_byte(rqp,n);
	checkbad(ncp_request(conn,rqp));
	target->total_blocks = ncp_rp_dword_lh(rqp);
	target->free_blocks = ncp_rp_dword_lh(rqp);
	target->purgeable_blocks = ncp_rp_dword_lh(rqp);
	target->not_yet_purgeable_blocks = ncp_rp_dword_lh(rqp);
	target->total_dir_entries = ncp_rp_dword_lh(rqp);
	target->available_dir_entries = ncp_rp_dword_lh(rqp);
	ncp_rp_dword_lh(rqp);
	target->sectors_per_block = ncp_rp_byte(rqp);
	bzero(&target->volume_name, sizeof(target->volume_name));
	len = ncp_rp_byte(rqp);
	if (len > NCP_VOLNAME_LEN) {
		error = ENAMETOOLONG;
	} else {
		ncp_rp_mem(rqp,(caddr_t)&target->volume_name, len);
	}
	NCP_RQ_EXIT;
	return error;
}

int
ncp_get_namespaces(struct ncp_conn *conn, u_int32_t volume, int *nsf,
	    struct proc *p,struct ucred *cred) {
	int error;
	u_int8_t ns;
	u_int16_t nscnt;
	DECLARE_RQ;

	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 24);	/* Subfunction: Get Loaded Name Spaces */
	ncp_rq_word(rqp, 0);
	ncp_rq_byte(rqp, volume);
	checkbad(ncp_request(conn,rqp));
	nscnt = ncp_rp_word_lh(rqp);
	*nsf = 0;
	while (nscnt-- > 0) {
		ns = ncp_rp_byte(rqp);
		*nsf |= 1 << ns;
	}
	NCP_RQ_EXIT;
	return error;
}

int
ncp_lookup_volume(struct ncp_conn *conn, char *volname, 
		u_char *volNum, u_int32_t *dirEnt,
		struct proc *p,struct ucred *cred)
{
	int error;
	DECLARE_RQ;

	NCPNDEBUG("looking up vol %s\n", volname);
	NCP_RQ_HEAD(87,p,cred);
	ncp_rq_byte(rqp, 22);	/* Subfunction: Generate dir handle */
	ncp_rq_byte(rqp, 0);	/* src name space */
	ncp_rq_byte(rqp, 0);	/* dst name space, always zero */
	ncp_rq_word(rqp, 0);	/* dstNSIndicator */

	ncp_rq_byte(rqp, 0);	/* faked volume number */
	ncp_rq_dword(rqp, 0);	/* faked dir_base */
	ncp_rq_byte(rqp, 0xff);	/* Don't have a dir_base */
	ncp_rq_byte(rqp, 1);	/* 1 path component */
	ncp_rq_pstring(rqp, volname);
	checkbad(ncp_request(conn,rqp));
	ncp_rp_dword_lh(rqp); 	/* NSDirectoryBase*/
	*dirEnt = ncp_rp_dword_lh(rqp);
	*volNum = ncp_rp_byte(rqp);
	NCP_RQ_EXIT;
	return error;
}

/* 
 * Time & date conversion routines taken from msdosfs. Although leap
 * year calculation is bogus, it's sufficient before 2100 :)
 */
/*
 * This is the format of the contents of the deTime field in the direntry
 * structure.
 * We don't use bitfields because we don't know how compilers for
 * arbitrary machines will lay them out.
 */
#define DT_2SECONDS_MASK	0x1F	/* seconds divided by 2 */
#define DT_2SECONDS_SHIFT	0
#define DT_MINUTES_MASK		0x7E0	/* minutes */
#define DT_MINUTES_SHIFT	5
#define DT_HOURS_MASK		0xF800	/* hours */
#define DT_HOURS_SHIFT		11

/*
 * This is the format of the contents of the deDate field in the direntry
 * structure.
 */
#define DD_DAY_MASK		0x1F	/* day of month */
#define DD_DAY_SHIFT		0
#define DD_MONTH_MASK		0x1E0	/* month */
#define DD_MONTH_SHIFT		5
#define DD_YEAR_MASK		0xFE00	/* year - 1980 */
#define DD_YEAR_SHIFT		9
/*
 * Total number of days that have passed for each month in a regular year.
 */
static u_short regyear[] = {
	31, 59, 90, 120, 151, 181,
	212, 243, 273, 304, 334, 365
};

/*
 * Total number of days that have passed for each month in a leap year.
 */
static u_short leapyear[] = {
	31, 60, 91, 121, 152, 182,
	213, 244, 274, 305, 335, 366
};

/*
 * Variables used to remember parts of the last time conversion.  Maybe we
 * can avoid a full conversion.
 */
static u_long  lasttime;
static u_long  lastday;
static u_short lastddate;
static u_short lastdtime;
/*
 * Convert the unix version of time to dos's idea of time to be used in
 * file timestamps. The passed in unix time is assumed to be in GMT.
 */
void
ncp_unix2dostime(tsp, tzoff, ddp, dtp, dhp)
	struct timespec *tsp;
	int tzoff;
	u_int16_t *ddp;
	u_int16_t *dtp;
	u_int8_t *dhp;
{
	u_long t;
	u_long days;
	u_long inc;
	u_long year;
	u_long month;
	u_short *months;

	/*
	 * If the time from the last conversion is the same as now, then
	 * skip the computations and use the saved result.
	 */
	t = tsp->tv_sec - tzoff * 60 - tz.tz_minuteswest * 60 -
	    (wall_cmos_clock ? adjkerntz : 0);
	t &= ~1;
	if (lasttime != t) {
		lasttime = t;
		lastdtime = (((t / 2) % 30) << DT_2SECONDS_SHIFT)
		    + (((t / 60) % 60) << DT_MINUTES_SHIFT)
		    + (((t / 3600) % 24) << DT_HOURS_SHIFT);

		/*
		 * If the number of days since 1970 is the same as the last
		 * time we did the computation then skip all this leap year
		 * and month stuff.
		 */
		days = t / (24 * 60 * 60);
		if (days != lastday) {
			lastday = days;
			for (year = 1970;; year++) {
				inc = year & 0x03 ? 365 : 366;
				if (days < inc)
					break;
				days -= inc;
			}
			months = year & 0x03 ? regyear : leapyear;
			for (month = 0; days >= months[month]; month++)
				;
			if (month > 0)
				days -= months[month - 1];
			lastddate = ((days + 1) << DD_DAY_SHIFT)
			    + ((month + 1) << DD_MONTH_SHIFT);
			/*
			 * Remember dos's idea of time is relative to 1980.
			 * unix's is relative to 1970.  If somehow we get a
			 * time before 1980 then don't give totally crazy
			 * results.
			 */
			if (year > 1980)
				lastddate += (year - 1980) << DD_YEAR_SHIFT;
		}
	}
	if (dtp)
		*dtp = lastdtime;
	if (dhp)
		*dhp = (tsp->tv_sec & 1) * 100 + tsp->tv_nsec / 10000000;

	*ddp = lastddate;
}

/*
 * The number of seconds between Jan 1, 1970 and Jan 1, 1980. In that
 * interval there were 8 regular years and 2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

static u_short lastdosdate;
static u_long  lastseconds;

/*
 * Convert from dos' idea of time to unix'. This will probably only be
 * called from the stat(), and fstat() system calls and so probably need
 * not be too efficient.
 */
void
ncp_dos2unixtime(dd, dt, dh, tzoff, tsp)
	u_int dd;
	u_int dt;
	u_int dh;
	int tzoff;
	struct timespec *tsp;
{
	u_long seconds;
	u_long month;
	u_long year;
	u_long days;
	u_short *months;

	if (dd == 0) {
		/*
		 * Uninitialized field, return the epoch.
		 */
		tsp->tv_sec = 0;
		tsp->tv_nsec = 0;
		return;
	}
	seconds = (((dt & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) << 1)
	    + ((dt & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT) * 60
	    + ((dt & DT_HOURS_MASK) >> DT_HOURS_SHIFT) * 3600
	    + dh / 100;
	/*
	 * If the year, month, and day from the last conversion are the
	 * same then use the saved value.
	 */
	if (lastdosdate != dd) {
		lastdosdate = dd;
		days = 0;
		year = (dd & DD_YEAR_MASK) >> DD_YEAR_SHIFT;
		days = year * 365;
		days += year / 4 + 1;	/* add in leap days */
		if ((year & 0x03) == 0)
			days--;		/* if year is a leap year */
		months = year & 0x03 ? regyear : leapyear;
		month = (dd & DD_MONTH_MASK) >> DD_MONTH_SHIFT;
		if (month < 1 || month > 12) {
			month = 1;
		}
		if (month > 1)
			days += months[month - 2];
		days += ((dd & DD_DAY_MASK) >> DD_DAY_SHIFT) - 1;
		lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
	}
	tsp->tv_sec = seconds + lastseconds + tz.tz_minuteswest * 60 +
	    tzoff * 60 + (wall_cmos_clock ? adjkerntz : 0);
	tsp->tv_nsec = (dh % 100) * 10000000;
}
