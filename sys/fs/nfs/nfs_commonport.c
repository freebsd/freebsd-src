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
 *
 */

#include <sys/cdefs.h>
/*
 * Functions that need to be different for different versions of BSD
 * kernel should be kept here, along with any global storage specific
 * to this BSD variant.
 */
#include <fs/nfs/nfsport.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <rpc/rpc_com.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

extern int nfscl_ticks;
extern void (*nfsd_call_recall)(struct vnode *, int, struct ucred *,
    struct thread *);
extern int nfsrv_useacl;
int newnfs_numnfsd = 0;
struct nfsstatsv1 nfsstatsv1;
int nfs_numnfscbd = 0;
int nfscl_debuglevel = 0;
char nfsv4_callbackaddr[INET6_ADDRSTRLEN];
int nfsrv_lughashsize = 100;
struct mtx nfsrv_dslock_mtx;
struct nfsdevicehead nfsrv_devidhead;
volatile int nfsrv_devidcnt = 0;
void (*ncl_call_invalcaches)(struct vnode *) = NULL;
vop_advlock_t *nfs_advlock_p = NULL;
vop_reclaim_t *nfs_reclaim_p = NULL;
uint32_t nfs_srvmaxio = NFS_SRVMAXIO;

NFSD_VNET_DEFINE(struct nfsstatsv1 *, nfsstatsv1_p);

NFSD_VNET_DECLARE(struct nfssockreq, nfsrv_nfsuserdsock);
NFSD_VNET_DECLARE(nfsuserd_state, nfsrv_nfsuserd);

int nfs_pnfsio(task_fn_t *, void *);

static int nfs_realign_test;
static int nfs_realign_count;
static struct ext_nfsstats oldnfsstats;
static struct nfsstatsov1 nfsstatsov1;

SYSCTL_NODE(_vfs, OID_AUTO, nfs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "NFS filesystem");
SYSCTL_INT(_vfs_nfs, OID_AUTO, realign_test, CTLFLAG_RW, &nfs_realign_test,
    0, "Number of realign tests done");
SYSCTL_INT(_vfs_nfs, OID_AUTO, realign_count, CTLFLAG_RW, &nfs_realign_count,
    0, "Number of mbuf realignments done");
SYSCTL_STRING(_vfs_nfs, OID_AUTO, callback_addr, CTLFLAG_RW,
    nfsv4_callbackaddr, sizeof(nfsv4_callbackaddr),
    "NFSv4 callback addr for server to use");
SYSCTL_INT(_vfs_nfs, OID_AUTO, debuglevel, CTLFLAG_RW, &nfscl_debuglevel,
    0, "Debug level for NFS client");
SYSCTL_INT(_vfs_nfs, OID_AUTO, userhashsize, CTLFLAG_RDTUN, &nfsrv_lughashsize,
    0, "Size of hash tables for uid/name mapping");
int nfs_pnfsiothreads = -1;
SYSCTL_INT(_vfs_nfs, OID_AUTO, pnfsiothreads, CTLFLAG_RW, &nfs_pnfsiothreads,
    0, "Number of pNFS mirror I/O threads");

/*
 * Defines for malloc
 * (Here for FreeBSD, since they allocate storage.)
 */
MALLOC_DEFINE(M_NEWNFSRVCACHE, "NFSD srvcache", "NFSD Server Request Cache");
MALLOC_DEFINE(M_NEWNFSDCLIENT, "NFSD V4client", "NFSD V4 Client Id");
MALLOC_DEFINE(M_NEWNFSDSTATE, "NFSD V4state",
    "NFSD V4 State (Openowner, Open, Lockowner, Delegation");
MALLOC_DEFINE(M_NEWNFSDLOCK, "NFSD V4lock", "NFSD V4 byte range lock");
MALLOC_DEFINE(M_NEWNFSDLOCKFILE, "NFSD lckfile", "NFSD Open/Lock file");
MALLOC_DEFINE(M_NEWNFSSTRING, "NFSD string", "NFSD V4 long string");
MALLOC_DEFINE(M_NEWNFSUSERGROUP, "NFSD usrgroup", "NFSD V4 User/group map");
MALLOC_DEFINE(M_NEWNFSDREQ, "NFS req", "NFS request header");
MALLOC_DEFINE(M_NEWNFSFH, "NFS fh", "NFS file handle");
MALLOC_DEFINE(M_NEWNFSCLOWNER, "NFSCL owner", "NFSCL Open Owner");
MALLOC_DEFINE(M_NEWNFSCLOPEN, "NFSCL open", "NFSCL Open");
MALLOC_DEFINE(M_NEWNFSCLDELEG, "NFSCL deleg", "NFSCL Delegation");
MALLOC_DEFINE(M_NEWNFSCLCLIENT, "NFSCL client", "NFSCL Client");
MALLOC_DEFINE(M_NEWNFSCLLOCKOWNER, "NFSCL lckown", "NFSCL Lock Owner");
MALLOC_DEFINE(M_NEWNFSCLLOCK, "NFSCL lck", "NFSCL Lock");
MALLOC_DEFINE(M_NEWNFSV4NODE, "NEWNFSnode", "NFS vnode");
MALLOC_DEFINE(M_NEWNFSDIROFF, "NFSCL diroff",
    "NFS directory offset data");
MALLOC_DEFINE(M_NEWNFSDROLLBACK, "NFSD rollback",
    "NFS local lock rollback");
MALLOC_DEFINE(M_NEWNFSLAYOUT, "NFSCL layout", "NFSv4.1 Layout");
MALLOC_DEFINE(M_NEWNFSFLAYOUT, "NFSCL flayout", "NFSv4.1 File Layout");
MALLOC_DEFINE(M_NEWNFSDEVINFO, "NFSCL devinfo", "NFSv4.1 Device Info");
MALLOC_DEFINE(M_NEWNFSSOCKREQ, "NFSCL sockreq", "NFS Sock Req");
MALLOC_DEFINE(M_NEWNFSCLDS, "NFSCL session", "NFSv4.1 Session");
MALLOC_DEFINE(M_NEWNFSLAYRECALL, "NFSCL layrecall", "NFSv4.1 Layout Recall");
MALLOC_DEFINE(M_NEWNFSDSESSION, "NFSD session", "NFSD Session for a client");

/*
 * Definition of mutex locks.
 * newnfsd_mtx is used in nfsrvd_nfsd() to protect the nfs socket list
 * and assorted other nfsd structures.
 */
struct mtx newnfsd_mtx;
struct mtx nfs_sockl_mutex;
struct mtx nfs_state_mutex;
struct mtx nfs_nameid_mutex;
struct mtx nfs_req_mutex;
struct mtx nfs_slock_mutex;
struct mtx nfs_clstate_mutex;

/* local functions */
static int nfssvc_call(struct thread *, struct nfssvc_args *, struct ucred *);

#ifdef __NO_STRICT_ALIGNMENT
/*
 * These architectures don't need re-alignment, so just return.
 */
int
newnfs_realign(struct mbuf **pm, int how)
{

	return (0);
}
#else	/* !__NO_STRICT_ALIGNMENT */
/*
 *	newnfs_realign:
 *
 *	Check for badly aligned mbuf data and realign by copying the unaligned
 *	portion of the data into a new mbuf chain and freeing the portions
 *	of the old chain that were replaced.
 *
 *	We cannot simply realign the data within the existing mbuf chain
 *	because the underlying buffers may contain other rpc commands and
 *	we cannot afford to overwrite them.
 *
 *	We would prefer to avoid this situation entirely.  The situation does
 *	not occur with NFS/UDP and is supposed to only occasionally occur
 *	with TCP.  Use vfs.nfs.realign_count and realign_test to check this.
 *
 */
int
newnfs_realign(struct mbuf **pm, int how)
{
	struct mbuf *m, *n;
	int off, space;

	++nfs_realign_test;
	while ((m = *pm) != NULL) {
		if ((m->m_len & 0x3) || (mtod(m, intptr_t) & 0x3)) {
			/*
			 * NB: we can't depend on m_pkthdr.len to help us
			 * decide what to do here.  May not be worth doing
			 * the m_length calculation as m_copyback will
			 * expand the mbuf chain below as needed.
			 */
			space = m_length(m, NULL);
			if (space >= MINCLSIZE) {
				/* NB: m_copyback handles space > MCLBYTES */
				n = m_getcl(how, MT_DATA, 0);
			} else
				n = m_get(how, MT_DATA);
			if (n == NULL)
				return (ENOMEM);
			/*
			 * Align the remainder of the mbuf chain.
			 */
			n->m_len = 0;
			off = 0;
			while (m != NULL) {
				m_copyback(n, off, m->m_len, mtod(m, caddr_t));
				off += m->m_len;
				m = m->m_next;
			}
			m_freem(*pm);
			*pm = n;
			++nfs_realign_count;
			break;
		}
		pm = &m->m_next;
	}

	return (0);
}
#endif	/* __NO_STRICT_ALIGNMENT */

#ifdef notdef
static void
nfsrv_object_create(struct vnode *vp, struct thread *td)
{

	if (vp == NULL || vp->v_type != VREG)
		return;
	(void) vfs_object_create(vp, td, td->td_ucred);
}
#endif

/*
 * Look up a file name. Basically just initialize stuff and call namei().
 */
int
nfsrv_lookupfilename(struct nameidata *ndp, char *fname, NFSPROC_T *p __unused)
{
	int error;

	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, fname);
	error = namei(ndp);
	if (!error) {
		NDFREE_PNBUF(ndp);
	}
	return (error);
}

/*
 * Copy NFS uid, gids to the cred structure.
 */
void
newnfs_copycred(struct nfscred *nfscr, struct ucred *cr)
{

	KASSERT(nfscr->nfsc_ngroups >= 0,
	    ("newnfs_copycred: negative nfsc_ngroups"));
	cr->cr_uid = nfscr->nfsc_uid;
	crsetgroups_and_egid(cr, nfscr->nfsc_ngroups, nfscr->nfsc_groups,
	    GID_NOGROUP);
}

/*
 * Map args from nfsmsleep() to msleep().
 */
int
nfsmsleep(void *chan, void *mutex, int prio, const char *wmesg,
    struct timespec *ts)
{
	u_int64_t nsecval;
	int error, timeo;

	if (ts) {
		timeo = hz * ts->tv_sec;
		nsecval = (u_int64_t)ts->tv_nsec;
		nsecval = ((nsecval * ((u_int64_t)hz)) + 500000000) /
		    1000000000;
		timeo += (int)nsecval;
	} else {
		timeo = 0;
	}
	error = msleep(chan, (struct mtx *)mutex, prio, wmesg, timeo);
	return (error);
}

/*
 * Get the file system info for the server. For now, just assume FFS.
 */
void
nfsvno_getfs(struct nfsfsinfo *sip, int isdgram)
{
	int pref;

	/*
	 * XXX
	 * There should be file system VFS OP(s) to get this information.
	 * For now, assume ufs.
	 */
	if (isdgram)
		pref = NFS_MAXDGRAMDATA;
	else
		pref = nfs_srvmaxio;
	sip->fs_rtmax = nfs_srvmaxio;
	sip->fs_rtpref = pref;
	sip->fs_rtmult = NFS_FABLKSIZE;
	sip->fs_wtmax = nfs_srvmaxio;
	sip->fs_wtpref = pref;
	sip->fs_wtmult = NFS_FABLKSIZE;
	sip->fs_dtpref = pref;
	sip->fs_maxfilesize = 0xffffffffffffffffull;
	sip->fs_timedelta.tv_sec = 0;
	sip->fs_timedelta.tv_nsec = 1;
	sip->fs_properties = (NFSV3FSINFO_LINK |
	    NFSV3FSINFO_SYMLINK | NFSV3FSINFO_HOMOGENEOUS |
	    NFSV3FSINFO_CANSETTIME);
}

/*
 * Do the pathconf vnode op.
 */
int
nfsvno_pathconf(struct vnode *vp, int flag, long *retf,
    struct ucred *cred, struct thread *p)
{
	int error;

	error = VOP_PATHCONF(vp, flag, retf);
	if (error == EOPNOTSUPP || error == EINVAL) {
		/*
		 * Some file systems return EINVAL for name arguments not
		 * supported and some return EOPNOTSUPP for this case.
		 * So the NFSv3 Pathconf RPC doesn't fail for these cases,
		 * just fake them.
		 */
		switch (flag) {
		case _PC_LINK_MAX:
			*retf = NFS_LINK_MAX;
			break;
		case _PC_NAME_MAX:
			*retf = NAME_MAX;
			break;
		case _PC_CHOWN_RESTRICTED:
			*retf = 1;
			break;
		case _PC_NO_TRUNC:
			*retf = 1;
			break;
		default:
			/*
			 * Only happens if a _PC_xxx is added to the server,
			 * but this isn't updated.
			 */
			*retf = 0;
			printf("nfsrvd pathconf flag=%d not supp\n", flag);
		}
		error = 0;
	}
	NFSEXITCODE(error);
	return (error);
}

/* Fake nfsrv_atroot. Just return 0 */
int
nfsrv_atroot(struct vnode *vp, uint64_t *retp)
{

	return (0);
}

/*
 * Set the credentials to refer to root.
 */
void
newnfs_setroot(struct ucred *cred)
{

	cred->cr_uid = 0;
	cred->cr_gid = 0;
	cred->cr_ngroups = 0;
}

/*
 * Get the client credential. Used for Renew and recovery.
 */
struct ucred *
newnfs_getcred(void)
{
	struct ucred *cred;
	struct thread *td = curthread;

	cred = crdup(td->td_ucred);
	newnfs_setroot(cred);
	return (cred);
}

/*
 * Sleep for a short period of time unless errval == NFSERR_GRACE, where
 * the sleep should be for 5 seconds.
 * Since lbolt doesn't exist in FreeBSD-CURRENT, just use a timeout on
 * an event that never gets a wakeup. Only return EINTR or 0.
 */
int
nfs_catnap(int prio, int errval, const char *wmesg)
{
	static int non_event;
	int ret;

	if (errval == NFSERR_GRACE)
		ret = tsleep(&non_event, prio, wmesg, 5 * hz);
	else
		ret = tsleep(&non_event, prio, wmesg, 1);
	if (ret != EINTR)
		ret = 0;
	return (ret);
}

/*
 * Get referral. For now, just fail.
 */
struct nfsreferral *
nfsv4root_getreferral(struct vnode *vp, struct vnode *dvp, u_int32_t fileno)
{

	return (NULL);
}

static int
nfssvc_nfscommon(struct thread *td, struct nfssvc_args *uap)
{
	int error;

	NFSD_CURVNET_SET(NFSD_TD_TO_VNET(td));
	error = nfssvc_call(td, uap, td->td_ucred);
	NFSD_CURVNET_RESTORE();
	NFSEXITCODE(error);
	return (error);
}

static int
nfssvc_call(struct thread *p, struct nfssvc_args *uap, struct ucred *cred)
{
	int error = EINVAL, i, j;
	struct nfsd_idargs nid;
	struct nfsd_oidargs onid;
	struct {
		int vers;	/* Just the first field of nfsstats. */
	} nfsstatver;

	if (uap->flag & NFSSVC_IDNAME) {
		if ((uap->flag & NFSSVC_NEWSTRUCT) != 0)
			error = copyin(uap->argp, &nid, sizeof(nid));
		else {
			error = copyin(uap->argp, &onid, sizeof(onid));
			if (error == 0) {
				nid.nid_flag = onid.nid_flag;
				nid.nid_uid = onid.nid_uid;
				nid.nid_gid = onid.nid_gid;
				nid.nid_usermax = onid.nid_usermax;
				nid.nid_usertimeout = onid.nid_usertimeout;
				nid.nid_name = onid.nid_name;
				nid.nid_namelen = onid.nid_namelen;
				nid.nid_ngroup = 0;
				nid.nid_grps = NULL;
			}
		}
		if (error)
			goto out;
		error = nfssvc_idname(&nid);
		goto out;
	} else if (uap->flag & NFSSVC_GETSTATS) {
		if ((uap->flag & NFSSVC_NEWSTRUCT) == 0) {
			/* Copy fields to the old ext_nfsstat structure. */
			oldnfsstats.attrcache_hits =
			    NFSD_VNET(nfsstatsv1_p)->attrcache_hits;
			oldnfsstats.attrcache_misses =
			    NFSD_VNET(nfsstatsv1_p)->attrcache_misses;
			oldnfsstats.lookupcache_hits =
			    NFSD_VNET(nfsstatsv1_p)->lookupcache_hits;
			oldnfsstats.lookupcache_misses =
			    NFSD_VNET(nfsstatsv1_p)->lookupcache_misses;
			oldnfsstats.direofcache_hits =
			    NFSD_VNET(nfsstatsv1_p)->direofcache_hits;
			oldnfsstats.direofcache_misses =
			    NFSD_VNET(nfsstatsv1_p)->direofcache_misses;
			oldnfsstats.accesscache_hits =
			    NFSD_VNET(nfsstatsv1_p)->accesscache_hits;
			oldnfsstats.accesscache_misses =
			    NFSD_VNET(nfsstatsv1_p)->accesscache_misses;
			oldnfsstats.biocache_reads =
			    NFSD_VNET(nfsstatsv1_p)->biocache_reads;
			oldnfsstats.read_bios =
			    NFSD_VNET(nfsstatsv1_p)->read_bios;
			oldnfsstats.read_physios =
			    NFSD_VNET(nfsstatsv1_p)->read_physios;
			oldnfsstats.biocache_writes =
			    NFSD_VNET(nfsstatsv1_p)->biocache_writes;
			oldnfsstats.write_bios =
			    NFSD_VNET(nfsstatsv1_p)->write_bios;
			oldnfsstats.write_physios =
			    NFSD_VNET(nfsstatsv1_p)->write_physios;
			oldnfsstats.biocache_readlinks =
			    NFSD_VNET(nfsstatsv1_p)->biocache_readlinks;
			oldnfsstats.readlink_bios =
			    NFSD_VNET(nfsstatsv1_p)->readlink_bios;
			oldnfsstats.biocache_readdirs =
			    NFSD_VNET(nfsstatsv1_p)->biocache_readdirs;
			oldnfsstats.readdir_bios =
			    NFSD_VNET(nfsstatsv1_p)->readdir_bios;
			for (i = 0; i < NFSV4_NPROCS; i++)
				oldnfsstats.rpccnt[i] =
				    NFSD_VNET(nfsstatsv1_p)->rpccnt[i];
			oldnfsstats.rpcretries =
			    NFSD_VNET(nfsstatsv1_p)->rpcretries;
			for (i = 0; i < NFSV4OP_NOPS; i++)
				oldnfsstats.srvrpccnt[i] =
				    NFSD_VNET(nfsstatsv1_p)->srvrpccnt[i];
			for (i = NFSV42_NOPS, j = NFSV4OP_NOPS;
			    i < NFSV42_NOPS + NFSV4OP_FAKENOPS; i++, j++)
				oldnfsstats.srvrpccnt[j] =
				    NFSD_VNET(nfsstatsv1_p)->srvrpccnt[i];
			oldnfsstats.reserved_0 = 0;
			oldnfsstats.reserved_1 = 0;
			oldnfsstats.rpcrequests =
			    NFSD_VNET(nfsstatsv1_p)->rpcrequests;
			oldnfsstats.rpctimeouts =
			    NFSD_VNET(nfsstatsv1_p)->rpctimeouts;
			oldnfsstats.rpcunexpected =
			    NFSD_VNET(nfsstatsv1_p)->rpcunexpected;
			oldnfsstats.rpcinvalid =
			    NFSD_VNET(nfsstatsv1_p)->rpcinvalid;
			oldnfsstats.srvcache_inproghits =
			    NFSD_VNET(nfsstatsv1_p)->srvcache_inproghits;
			oldnfsstats.reserved_2 = 0;
			oldnfsstats.srvcache_nonidemdonehits =
			    NFSD_VNET(nfsstatsv1_p)->srvcache_nonidemdonehits;
			oldnfsstats.srvcache_misses =
			    NFSD_VNET(nfsstatsv1_p)->srvcache_misses;
			oldnfsstats.srvcache_tcppeak =
			    NFSD_VNET(nfsstatsv1_p)->srvcache_tcppeak;
			oldnfsstats.srvcache_size =
			    NFSD_VNET(nfsstatsv1_p)->srvcache_size;
			oldnfsstats.srvclients =
			    NFSD_VNET(nfsstatsv1_p)->srvclients;
			oldnfsstats.srvopenowners =
			    NFSD_VNET(nfsstatsv1_p)->srvopenowners;
			oldnfsstats.srvopens =
			    NFSD_VNET(nfsstatsv1_p)->srvopens;
			oldnfsstats.srvlockowners =
			    NFSD_VNET(nfsstatsv1_p)->srvlockowners;
			oldnfsstats.srvlocks =
			    NFSD_VNET(nfsstatsv1_p)->srvlocks;
			oldnfsstats.srvdelegates =
			    NFSD_VNET(nfsstatsv1_p)->srvdelegates;
			for (i = 0; i < NFSV4OP_CBNOPS; i++)
				oldnfsstats.cbrpccnt[i] =
				    NFSD_VNET(nfsstatsv1_p)->cbrpccnt[i];
			oldnfsstats.clopenowners =
			    NFSD_VNET(nfsstatsv1_p)->clopenowners;
			oldnfsstats.clopens = NFSD_VNET(nfsstatsv1_p)->clopens;
			oldnfsstats.cllockowners =
			    NFSD_VNET(nfsstatsv1_p)->cllockowners;
			oldnfsstats.cllocks = NFSD_VNET(nfsstatsv1_p)->cllocks;
			oldnfsstats.cldelegates =
			    NFSD_VNET(nfsstatsv1_p)->cldelegates;
			oldnfsstats.cllocalopenowners =
			    NFSD_VNET(nfsstatsv1_p)->cllocalopenowners;
			oldnfsstats.cllocalopens =
			    NFSD_VNET(nfsstatsv1_p)->cllocalopens;
			oldnfsstats.cllocallockowners =
			    NFSD_VNET(nfsstatsv1_p)->cllocallockowners;
			oldnfsstats.cllocallocks =
			    NFSD_VNET(nfsstatsv1_p)->cllocallocks;
			error = copyout(&oldnfsstats, uap->argp,
			    sizeof (oldnfsstats));
		} else {
			error = copyin(uap->argp, &nfsstatver,
			    sizeof(nfsstatver));
			if (error == 0) {
				if (nfsstatver.vers == NFSSTATS_OV1) {
					/* Copy nfsstatsv1 to nfsstatsov1. */
					nfsstatsov1.attrcache_hits =
					    NFSD_VNET(nfsstatsv1_p)->attrcache_hits;
					nfsstatsov1.attrcache_misses =
					    NFSD_VNET(nfsstatsv1_p)->attrcache_misses;
					nfsstatsov1.lookupcache_hits =
					    NFSD_VNET(nfsstatsv1_p)->lookupcache_hits;
					nfsstatsov1.lookupcache_misses =
					    NFSD_VNET(nfsstatsv1_p)->lookupcache_misses;
					nfsstatsov1.direofcache_hits =
					    NFSD_VNET(nfsstatsv1_p)->direofcache_hits;
					nfsstatsov1.direofcache_misses =
					    NFSD_VNET(nfsstatsv1_p)->direofcache_misses;
					nfsstatsov1.accesscache_hits =
					    NFSD_VNET(nfsstatsv1_p)->accesscache_hits;
					nfsstatsov1.accesscache_misses =
					    NFSD_VNET(nfsstatsv1_p)->accesscache_misses;
					nfsstatsov1.biocache_reads =
					    NFSD_VNET(nfsstatsv1_p)->biocache_reads;
					nfsstatsov1.read_bios =
					    NFSD_VNET(nfsstatsv1_p)->read_bios;
					nfsstatsov1.read_physios =
					    NFSD_VNET(nfsstatsv1_p)->read_physios;
					nfsstatsov1.biocache_writes =
					    NFSD_VNET(nfsstatsv1_p)->biocache_writes;
					nfsstatsov1.write_bios =
					    NFSD_VNET(nfsstatsv1_p)->write_bios;
					nfsstatsov1.write_physios =
					    NFSD_VNET(nfsstatsv1_p)->write_physios;
					nfsstatsov1.biocache_readlinks =
					    NFSD_VNET(nfsstatsv1_p)->biocache_readlinks;
					nfsstatsov1.readlink_bios =
					    NFSD_VNET(nfsstatsv1_p)->readlink_bios;
					nfsstatsov1.biocache_readdirs =
					    NFSD_VNET(nfsstatsv1_p)->biocache_readdirs;
					nfsstatsov1.readdir_bios =
					    NFSD_VNET(nfsstatsv1_p)->readdir_bios;
					for (i = 0; i < NFSV42_OLDNPROCS; i++)
						nfsstatsov1.rpccnt[i] =
						    NFSD_VNET(nfsstatsv1_p)->rpccnt[i];
					nfsstatsov1.rpcretries =
					    NFSD_VNET(nfsstatsv1_p)->rpcretries;
					for (i = 0; i < NFSV42_PURENOPS; i++)
						nfsstatsov1.srvrpccnt[i] =
						    NFSD_VNET(nfsstatsv1_p)->srvrpccnt[i];
					for (i = NFSV42_NOPS,
					     j = NFSV42_PURENOPS;
					     i < NFSV42_NOPS + NFSV4OP_FAKENOPS;
					     i++, j++)
						nfsstatsov1.srvrpccnt[j] =
						    NFSD_VNET(nfsstatsv1_p)->srvrpccnt[i];
					nfsstatsov1.reserved_0 = 0;
					nfsstatsov1.reserved_1 = 0;
					nfsstatsov1.rpcrequests =
					    NFSD_VNET(nfsstatsv1_p)->rpcrequests;
					nfsstatsov1.rpctimeouts =
					    NFSD_VNET(nfsstatsv1_p)->rpctimeouts;
					nfsstatsov1.rpcunexpected =
					    NFSD_VNET(nfsstatsv1_p)->rpcunexpected;
					nfsstatsov1.rpcinvalid =
					    NFSD_VNET(nfsstatsv1_p)->rpcinvalid;
					nfsstatsov1.srvcache_inproghits =
					    NFSD_VNET(nfsstatsv1_p)->srvcache_inproghits;
					nfsstatsov1.reserved_2 = 0;
					nfsstatsov1.srvcache_nonidemdonehits =
					    NFSD_VNET(nfsstatsv1_p)->srvcache_nonidemdonehits;
					nfsstatsov1.srvcache_misses =
					    NFSD_VNET(nfsstatsv1_p)->srvcache_misses;
					nfsstatsov1.srvcache_tcppeak =
					    NFSD_VNET(nfsstatsv1_p)->srvcache_tcppeak;
					nfsstatsov1.srvcache_size =
					    NFSD_VNET(nfsstatsv1_p)->srvcache_size;
					nfsstatsov1.srvclients =
					    NFSD_VNET(nfsstatsv1_p)->srvclients;
					nfsstatsov1.srvopenowners =
					    NFSD_VNET(nfsstatsv1_p)->srvopenowners;
					nfsstatsov1.srvopens =
					    NFSD_VNET(nfsstatsv1_p)->srvopens;
					nfsstatsov1.srvlockowners =
					    NFSD_VNET(nfsstatsv1_p)->srvlockowners;
					nfsstatsov1.srvlocks =
					    NFSD_VNET(nfsstatsv1_p)->srvlocks;
					nfsstatsov1.srvdelegates =
					    NFSD_VNET(nfsstatsv1_p)->srvdelegates;
					for (i = 0; i < NFSV42_CBNOPS; i++)
						nfsstatsov1.cbrpccnt[i] =
						    NFSD_VNET(nfsstatsv1_p)->cbrpccnt[i];
					nfsstatsov1.clopenowners =
					    NFSD_VNET(nfsstatsv1_p)->clopenowners;
					nfsstatsov1.clopens =
					    NFSD_VNET(nfsstatsv1_p)->clopens;
					nfsstatsov1.cllockowners =
					    NFSD_VNET(nfsstatsv1_p)->cllockowners;
					nfsstatsov1.cllocks =
					    NFSD_VNET(nfsstatsv1_p)->cllocks;
					nfsstatsov1.cldelegates =
					    NFSD_VNET(nfsstatsv1_p)->cldelegates;
					nfsstatsov1.cllocalopenowners =
					    NFSD_VNET(nfsstatsv1_p)->cllocalopenowners;
					nfsstatsov1.cllocalopens =
					    NFSD_VNET(nfsstatsv1_p)->cllocalopens;
					nfsstatsov1.cllocallockowners =
					    NFSD_VNET(nfsstatsv1_p)->cllocallockowners;
					nfsstatsov1.cllocallocks =
					    NFSD_VNET(nfsstatsv1_p)->cllocallocks;
					nfsstatsov1.srvstartcnt =
					    NFSD_VNET(nfsstatsv1_p)->srvstartcnt;
					nfsstatsov1.srvdonecnt =
					    NFSD_VNET(nfsstatsv1_p)->srvdonecnt;
					for (i = NFSV42_NOPS,
					     j = NFSV42_PURENOPS;
					     i < NFSV42_NOPS + NFSV4OP_FAKENOPS;
					     i++, j++) {
						nfsstatsov1.srvbytes[j] =
						    NFSD_VNET(nfsstatsv1_p)->srvbytes[i];
						nfsstatsov1.srvops[j] =
						    NFSD_VNET(nfsstatsv1_p)->srvops[i];
						nfsstatsov1.srvduration[j] =
						    NFSD_VNET(nfsstatsv1_p)->srvduration[i];
					}
					nfsstatsov1.busyfrom =
					    NFSD_VNET(nfsstatsv1_p)->busyfrom;
					nfsstatsov1.busyfrom =
					    NFSD_VNET(nfsstatsv1_p)->busyfrom;
					error = copyout(&nfsstatsov1, uap->argp,
					    sizeof(nfsstatsov1));
				} else if (nfsstatver.vers != NFSSTATS_V1)
					error = EPERM;
				else
					error = copyout(NFSD_VNET(nfsstatsv1_p),
					    uap->argp, sizeof(nfsstatsv1));
			}
		}
		if (error == 0) {
			if ((uap->flag & NFSSVC_ZEROCLTSTATS) != 0) {
				NFSD_VNET(nfsstatsv1_p)->attrcache_hits = 0;
				NFSD_VNET(nfsstatsv1_p)->attrcache_misses = 0;
				NFSD_VNET(nfsstatsv1_p)->lookupcache_hits = 0;
				NFSD_VNET(nfsstatsv1_p)->lookupcache_misses = 0;
				NFSD_VNET(nfsstatsv1_p)->direofcache_hits = 0;
				NFSD_VNET(nfsstatsv1_p)->direofcache_misses = 0;
				NFSD_VNET(nfsstatsv1_p)->accesscache_hits = 0;
				NFSD_VNET(nfsstatsv1_p)->accesscache_misses = 0;
				NFSD_VNET(nfsstatsv1_p)->biocache_reads = 0;
				NFSD_VNET(nfsstatsv1_p)->read_bios = 0;
				NFSD_VNET(nfsstatsv1_p)->read_physios = 0;
				NFSD_VNET(nfsstatsv1_p)->biocache_writes = 0;
				NFSD_VNET(nfsstatsv1_p)->write_bios = 0;
				NFSD_VNET(nfsstatsv1_p)->write_physios = 0;
				NFSD_VNET(nfsstatsv1_p)->biocache_readlinks = 0;
				NFSD_VNET(nfsstatsv1_p)->readlink_bios = 0;
				NFSD_VNET(nfsstatsv1_p)->biocache_readdirs = 0;
				NFSD_VNET(nfsstatsv1_p)->readdir_bios = 0;
				NFSD_VNET(nfsstatsv1_p)->rpcretries = 0;
				NFSD_VNET(nfsstatsv1_p)->rpcrequests = 0;
				NFSD_VNET(nfsstatsv1_p)->rpctimeouts = 0;
				NFSD_VNET(nfsstatsv1_p)->rpcunexpected = 0;
				NFSD_VNET(nfsstatsv1_p)->rpcinvalid = 0;
				bzero(NFSD_VNET(nfsstatsv1_p)->rpccnt,
				    sizeof(NFSD_VNET(nfsstatsv1_p)->rpccnt));
			}
			if ((uap->flag & NFSSVC_ZEROSRVSTATS) != 0) {
				NFSD_VNET(nfsstatsv1_p)->srvcache_inproghits = 0;
				NFSD_VNET(nfsstatsv1_p)->srvcache_nonidemdonehits = 0;
				NFSD_VNET(nfsstatsv1_p)->srvcache_misses = 0;
				NFSD_VNET(nfsstatsv1_p)->srvcache_tcppeak = 0;
				bzero(NFSD_VNET(nfsstatsv1_p)->srvrpccnt,
				    sizeof(NFSD_VNET(nfsstatsv1_p)->srvrpccnt));
				bzero(NFSD_VNET(nfsstatsv1_p)->cbrpccnt,
				    sizeof(NFSD_VNET(nfsstatsv1_p)->cbrpccnt));
			}
		}
		goto out;
	} else if (uap->flag & NFSSVC_NFSUSERDPORT) {
		u_short sockport;
		struct nfsuserd_args nargs;

		if ((uap->flag & NFSSVC_NEWSTRUCT) == 0) {
			error = copyin(uap->argp, (caddr_t)&sockport,
			    sizeof (u_short));
			if (error == 0) {
				nargs.nuserd_family = AF_INET;
				nargs.nuserd_port = sockport;
			}
		} else {
			/*
			 * New nfsuserd_args structure, which indicates
			 * which IP version to use along with the port#.
			 */
			error = copyin(uap->argp, &nargs, sizeof(nargs));
		}
		if (!error)
			error = nfsrv_nfsuserdport(&nargs, p);
	} else if (uap->flag & NFSSVC_NFSUSERDDELPORT) {
		nfsrv_nfsuserddelport();
		error = 0;
	}

out:
	NFSEXITCODE(error);
	return (error);
}

/*
 * called by all three modevent routines, so that it gets things
 * initialized soon enough.
 */
void
newnfs_portinit(void)
{
	static int inited = 0;

	if (inited)
		return;
	inited = 1;
	/* Initialize SMP locks used by both client and server. */
	mtx_init(&newnfsd_mtx, "newnfsd_mtx", NULL, MTX_DEF);
	mtx_init(&nfs_state_mutex, "nfs_state_mutex", NULL, MTX_DEF);
	mtx_init(&nfs_clstate_mutex, "nfs_clstate_mutex", NULL, MTX_DEF);
}

/*
 * Determine if the file system supports NFSv4 ACLs.
 * Return 1 if it does, 0 otherwise.
 */
int
nfs_supportsnfsv4acls(struct vnode *vp)
{
	int error;
	long retval;

	ASSERT_VOP_LOCKED(vp, "nfs supports nfsv4acls");

	if (nfsrv_useacl == 0)
		return (0);
	error = VOP_PATHCONF(vp, _PC_ACL_NFS4, &retval);
	if (error == 0 && retval != 0)
		return (1);
	return (0);
}

/*
 * These are the first fields of all the context structures passed into
 * nfs_pnfsio().
 */
struct pnfsio {
	int		done;
	int		inprog;
	struct task	tsk;
};

/*
 * Do a mirror I/O on a pNFS thread.
 */
int
nfs_pnfsio(task_fn_t *func, void *context)
{
	struct pnfsio *pio;
	int ret;
	static struct taskqueue *pnfsioq = NULL;

	pio = (struct pnfsio *)context;
	if (pnfsioq == NULL) {
		if (nfs_pnfsiothreads == 0)
			return (EPERM);
		if (nfs_pnfsiothreads < 0)
			nfs_pnfsiothreads = mp_ncpus * 4;
		pnfsioq = taskqueue_create("pnfsioq", M_WAITOK,
		    taskqueue_thread_enqueue, &pnfsioq);
		if (pnfsioq == NULL)
			return (ENOMEM);
		ret = taskqueue_start_threads(&pnfsioq, nfs_pnfsiothreads,
		    0, "pnfsiot");
		if (ret != 0) {
			taskqueue_free(pnfsioq);
			pnfsioq = NULL;
			return (ret);
		}
	}
	pio->inprog = 1;
	TASK_INIT(&pio->tsk, 0, func, context);
	ret = taskqueue_enqueue(pnfsioq, &pio->tsk);
	if (ret != 0)
		pio->inprog = 0;
	return (ret);
}

/*
 * Initialize everything that needs to be initialized for a vnet.
 */
static void
nfs_vnetinit(const void *unused __unused)
{

	if (IS_DEFAULT_VNET(curvnet))
		NFSD_VNET(nfsstatsv1_p) = &nfsstatsv1;
	else
		NFSD_VNET(nfsstatsv1_p) = malloc(sizeof(struct nfsstatsv1),
		    M_TEMP, M_WAITOK | M_ZERO);
	mtx_init(&NFSD_VNET(nfsrv_nfsuserdsock).nr_mtx, "nfsuserd",
	    NULL, MTX_DEF);
}
VNET_SYSINIT(nfs_vnetinit, SI_SUB_VNET_DONE, SI_ORDER_FIRST,
    nfs_vnetinit, NULL);

static void
nfs_cleanup(void *unused __unused)
{

	mtx_destroy(&NFSD_VNET(nfsrv_nfsuserdsock).nr_mtx);
	if (!IS_DEFAULT_VNET(curvnet)) {
		free(NFSD_VNET(nfsstatsv1_p), M_TEMP);
		NFSD_VNET(nfsstatsv1_p) = NULL;
	}
	/* Clean out the name<-->id cache. */
	nfsrv_cleanusergroup();
}
VNET_SYSUNINIT(nfs_cleanup, SI_SUB_VNET_DONE, SI_ORDER_FIRST,
    nfs_cleanup, NULL);

extern int (*nfsd_call_nfscommon)(struct thread *, struct nfssvc_args *);

/*
 * Called once to initialize data structures...
 */
static int
nfscommon_modevent(module_t mod, int type, void *data)
{
	int error = 0;
	static int loaded = 0;

	switch (type) {
	case MOD_LOAD:
		if (loaded)
			goto out;
		newnfs_portinit();
		mtx_init(&nfs_nameid_mutex, "nfs_nameid_mutex", NULL, MTX_DEF);
		mtx_init(&nfs_sockl_mutex, "nfs_sockl_mutex", NULL, MTX_DEF);
		mtx_init(&nfs_slock_mutex, "nfs_slock_mutex", NULL, MTX_DEF);
		mtx_init(&nfs_req_mutex, "nfs_req_mutex", NULL, MTX_DEF);
		mtx_init(&nfsrv_dslock_mtx, "nfs4ds", NULL, MTX_DEF);
		TAILQ_INIT(&nfsrv_devidhead);
		newnfs_init();
		nfsd_call_nfscommon = nfssvc_nfscommon;
		loaded = 1;
		break;

	case MOD_UNLOAD:
		if (newnfs_numnfsd != 0 ||
		    NFSD_VNET(nfsrv_nfsuserd) != NOTRUNNING ||
		    nfs_numnfscbd != 0) {
			error = EBUSY;
			break;
		}

		nfsd_call_nfscommon = NULL;
		/* and get rid of the mutexes */
		mtx_destroy(&nfs_nameid_mutex);
		mtx_destroy(&newnfsd_mtx);
		mtx_destroy(&nfs_state_mutex);
		mtx_destroy(&nfs_clstate_mutex);
		mtx_destroy(&nfs_sockl_mutex);
		mtx_destroy(&nfs_slock_mutex);
		mtx_destroy(&nfs_req_mutex);
		mtx_destroy(&nfsrv_dslock_mtx);
		loaded = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

out:
	NFSEXITCODE(error);
	return error;
}
static moduledata_t nfscommon_mod = {
	"nfscommon",
	nfscommon_modevent,
	NULL,
};
DECLARE_MODULE(nfscommon, nfscommon_mod, SI_SUB_VFS, SI_ORDER_ANY);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(nfscommon, 1);
MODULE_DEPEND(nfscommon, nfssvc, 1, 1, 1);
MODULE_DEPEND(nfscommon, krpc, 1, 1, 1);
