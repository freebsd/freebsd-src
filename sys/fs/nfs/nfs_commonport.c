/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Functions that need to be different for different versions of BSD
 * kernel should be kept here, along with any global storage specific
 * to this BSD variant.
 */
#include <fs/nfs/nfsport.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

extern int nfscl_ticks;
extern int nfsrv_nfsuserd;
extern struct nfssockreq nfsrv_nfsuserdsock;
extern void (*nfsd_call_recall)(struct vnode *, int, struct ucred *,
    struct thread *);
extern int nfsrv_useacl;
struct mount nfsv4root_mnt;
int newnfs_numnfsd = 0;
struct nfsstats newnfsstats;
int nfs_numnfscbd = 0;
int nfscl_debuglevel = 0;
char nfsv4_callbackaddr[INET6_ADDRSTRLEN];
struct callout newnfsd_callout;
int nfsrv_lughashsize = 100;
void (*nfsd_call_servertimer)(void) = NULL;
void (*ncl_call_invalcaches)(struct vnode *) = NULL;

static int nfs_realign_test;
static int nfs_realign_count;

SYSCTL_NODE(_vfs, OID_AUTO, nfs, CTLFLAG_RW, 0, "NFS filesystem");
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
MALLOC_DEFINE(M_NEWNFSDIRECTIO, "NEWdirectio", "NFS Direct IO buffer");
MALLOC_DEFINE(M_NEWNFSDIROFF, "NFSCL diroffdiroff",
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
 *	not occur with NFS/UDP and is supposed to only occassionally occur
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
nfsrv_lookupfilename(struct nameidata *ndp, char *fname, NFSPROC_T *p)
{
	int error;

	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, fname,
	    p);
	error = namei(ndp);
	if (!error) {
		NDFREE(ndp, NDF_ONLY_PNBUF);
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
	crsetgroups(cr, nfscr->nfsc_ngroups, nfscr->nfsc_groups);
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
		pref = NFS_SRVMAXIO;
	sip->fs_rtmax = NFS_SRVMAXIO;
	sip->fs_rtpref = pref;
	sip->fs_rtmult = NFS_FABLKSIZE;
	sip->fs_wtmax = NFS_SRVMAXIO;
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
nfsvno_pathconf(struct vnode *vp, int flag, register_t *retf,
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
			*retf = LINK_MAX;
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
		};
		error = 0;
	}
	NFSEXITCODE(error);
	return (error);
}

/* Fake nfsrv_atroot. Just return 0 */
int
nfsrv_atroot(struct vnode *vp, long *retp)
{

	return (0);
}

/*
 * Set the credentials to refer to root.
 * If only the various BSDen could agree on whether cr_gid is a separate
 * field or cr_groups[0]...
 */
void
newnfs_setroot(struct ucred *cred)
{

	cred->cr_uid = 0;
	cred->cr_groups[0] = 0;
	cred->cr_ngroups = 1;
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
 * Nfs timer routine
 * Call the nfsd's timer function once/sec.
 */
void
newnfs_timer(void *arg)
{
	static time_t lasttime = 0;
	/*
	 * Call the server timer, if set up.
	 * The argument indicates if it is the next second and therefore
	 * leases should be checked.
	 */
	if (lasttime != NFSD_MONOSEC) {
		lasttime = NFSD_MONOSEC;
		if (nfsd_call_servertimer != NULL)
			(*nfsd_call_servertimer)();
	}
	callout_reset(&newnfsd_callout, nfscl_ticks, newnfs_timer, NULL);
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

	error = nfssvc_call(td, uap, td->td_ucred);
	NFSEXITCODE(error);
	return (error);
}

static int
nfssvc_call(struct thread *p, struct nfssvc_args *uap, struct ucred *cred)
{
	int error = EINVAL;
	struct nfsd_idargs nid;
	struct nfsd_oidargs onid;

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
		error = copyout(&newnfsstats,
		    CAST_USER_ADDR_T(uap->argp), sizeof (newnfsstats));
		if (error == 0) {
			if ((uap->flag & NFSSVC_ZEROCLTSTATS) != 0) {
				newnfsstats.attrcache_hits = 0;
				newnfsstats.attrcache_misses = 0;
				newnfsstats.lookupcache_hits = 0;
				newnfsstats.lookupcache_misses = 0;
				newnfsstats.direofcache_hits = 0;
				newnfsstats.direofcache_misses = 0;
				newnfsstats.accesscache_hits = 0;
				newnfsstats.accesscache_misses = 0;
				newnfsstats.biocache_reads = 0;
				newnfsstats.read_bios = 0;
				newnfsstats.read_physios = 0;
				newnfsstats.biocache_writes = 0;
				newnfsstats.write_bios = 0;
				newnfsstats.write_physios = 0;
				newnfsstats.biocache_readlinks = 0;
				newnfsstats.readlink_bios = 0;
				newnfsstats.biocache_readdirs = 0;
				newnfsstats.readdir_bios = 0;
				newnfsstats.rpcretries = 0;
				newnfsstats.rpcrequests = 0;
				newnfsstats.rpctimeouts = 0;
				newnfsstats.rpcunexpected = 0;
				newnfsstats.rpcinvalid = 0;
				bzero(newnfsstats.rpccnt,
				    sizeof(newnfsstats.rpccnt));
			}
			if ((uap->flag & NFSSVC_ZEROSRVSTATS) != 0) {
				newnfsstats.srvrpc_errs = 0;
				newnfsstats.srv_errs = 0;
				newnfsstats.srvcache_inproghits = 0;
				newnfsstats.srvcache_idemdonehits = 0;
				newnfsstats.srvcache_nonidemdonehits = 0;
				newnfsstats.srvcache_misses = 0;
				newnfsstats.srvcache_tcppeak = 0;
				newnfsstats.srvclients = 0;
				newnfsstats.srvopenowners = 0;
				newnfsstats.srvopens = 0;
				newnfsstats.srvlockowners = 0;
				newnfsstats.srvlocks = 0;
				newnfsstats.srvdelegates = 0;
				newnfsstats.clopenowners = 0;
				newnfsstats.clopens = 0;
				newnfsstats.cllockowners = 0;
				newnfsstats.cllocks = 0;
				newnfsstats.cldelegates = 0;
				newnfsstats.cllocalopenowners = 0;
				newnfsstats.cllocalopens = 0;
				newnfsstats.cllocallockowners = 0;
				newnfsstats.cllocallocks = 0;
				bzero(newnfsstats.srvrpccnt,
				    sizeof(newnfsstats.srvrpccnt));
				bzero(newnfsstats.cbrpccnt,
				    sizeof(newnfsstats.cbrpccnt));
			}
		}
		goto out;
	} else if (uap->flag & NFSSVC_NFSUSERDPORT) {
		u_short sockport;

		error = copyin(uap->argp, (caddr_t)&sockport,
		    sizeof (u_short));
		if (!error)
			error = nfsrv_nfsuserdport(sockport, p);
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
}

/*
 * Determine if the file system supports NFSv4 ACLs.
 * Return 1 if it does, 0 otherwise.
 */
int
nfs_supportsnfsv4acls(struct vnode *vp)
{
	int error;
	register_t retval;

	ASSERT_VOP_LOCKED(vp, "nfs supports nfsv4acls");

	if (nfsrv_useacl == 0)
		return (0);
	error = VOP_PATHCONF(vp, _PC_ACL_NFS4, &retval);
	if (error == 0 && retval != 0)
		return (1);
	return (0);
}

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
		mtx_init(&nfsrv_nfsuserdsock.nr_mtx, "nfsuserd", NULL,
		    MTX_DEF);
		callout_init(&newnfsd_callout, 1);
		newnfs_init();
		nfsd_call_nfscommon = nfssvc_nfscommon;
		loaded = 1;
		break;

	case MOD_UNLOAD:
		if (newnfs_numnfsd != 0 || nfsrv_nfsuserd != 0 ||
		    nfs_numnfscbd != 0) {
			error = EBUSY;
			break;
		}

		nfsd_call_nfscommon = NULL;
		callout_drain(&newnfsd_callout);
		/* Clean out the name<-->id cache. */
		nfsrv_cleanusergroup();
		/* and get rid of the mutexes */
		mtx_destroy(&nfs_nameid_mutex);
		mtx_destroy(&newnfsd_mtx);
		mtx_destroy(&nfs_state_mutex);
		mtx_destroy(&nfs_sockl_mutex);
		mtx_destroy(&nfs_slock_mutex);
		mtx_destroy(&nfs_req_mutex);
		mtx_destroy(&nfsrv_nfsuserdsock.nr_mtx);
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

