/*-
 * Copyright (c) 1989, 1993, 1995
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
 *	from nfs_vfsops.c	8.12 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include "opt_bootp.h"
#include "opt_nfsroot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/clock.h>
#include <sys/jail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <fs/nfs/nfsport.h>
#include <fs/nfsclient/nfsnode.h>
#include <fs/nfsclient/nfsmount.h>
#include <fs/nfsclient/nfs.h>
#include <nfs/nfsdiskless.h>

FEATURE(nfscl, "NFSv4 client");

extern int nfscl_ticks;
extern struct timeval nfsboottime;
extern struct nfsstats	newnfsstats;

MALLOC_DEFINE(M_NEWNFSREQ, "newnfsclient_req", "New NFS request header");
MALLOC_DEFINE(M_NEWNFSMNT, "newnfsmnt", "New NFS mount struct");

SYSCTL_DECL(_vfs_newnfs);
SYSCTL_STRUCT(_vfs_newnfs, NFS_NFSSTATS, nfsstats, CTLFLAG_RW,
	&newnfsstats, nfsstats, "S,nfsstats");
static int nfs_ip_paranoia = 1;
SYSCTL_INT(_vfs_newnfs, OID_AUTO, nfs_ip_paranoia, CTLFLAG_RW,
    &nfs_ip_paranoia, 0, "");
static int nfs_tprintf_initial_delay = NFS_TPRINTF_INITIAL_DELAY;
SYSCTL_INT(_vfs_newnfs, NFS_TPRINTF_INITIAL_DELAY,
        downdelayinitial, CTLFLAG_RW, &nfs_tprintf_initial_delay, 0, "");
/* how long between console messages "nfs server foo not responding" */
static int nfs_tprintf_delay = NFS_TPRINTF_DELAY;
SYSCTL_INT(_vfs_newnfs, NFS_TPRINTF_DELAY,
        downdelayinterval, CTLFLAG_RW, &nfs_tprintf_delay, 0, "");

static int	nfs_mountroot(struct mount *);
static void	nfs_sec_name(char *, int *);
static void	nfs_decode_args(struct mount *mp, struct nfsmount *nmp,
		    struct nfs_args *argp, const char *, struct ucred *,
		    struct thread *);
static int	mountnfs(struct nfs_args *, struct mount *,
		    struct sockaddr *, char *, u_char *, int, u_char *, int,
		    u_char *, int, struct vnode **, struct ucred *,
		    struct thread *, int);
static void	nfs_getnlminfo(struct vnode *, uint8_t *, size_t *,
		    struct sockaddr_storage *, int *, off_t *,
		    struct timeval *);
static vfs_mount_t nfs_mount;
static vfs_cmount_t nfs_cmount;
static vfs_unmount_t nfs_unmount;
static vfs_root_t nfs_root;
static vfs_statfs_t nfs_statfs;
static vfs_sync_t nfs_sync;
static vfs_sysctl_t nfs_sysctl;

/*
 * nfs vfs operations.
 */
static struct vfsops nfs_vfsops = {
	.vfs_init =		ncl_init,
	.vfs_mount =		nfs_mount,
	.vfs_cmount =		nfs_cmount,
	.vfs_root =		nfs_root,
	.vfs_statfs =		nfs_statfs,
	.vfs_sync =		nfs_sync,
	.vfs_uninit =		ncl_uninit,
	.vfs_unmount =		nfs_unmount,
	.vfs_sysctl =		nfs_sysctl,
};
VFS_SET(nfs_vfsops, nfs, VFCF_NETWORK);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(nfs, 1);
MODULE_DEPEND(nfs, nfscommon, 1, 1, 1);
MODULE_DEPEND(nfs, krpc, 1, 1, 1);
MODULE_DEPEND(nfs, nfssvc, 1, 1, 1);
MODULE_DEPEND(nfs, nfslock, 1, 1, 1);

/*
 * This structure is now defined in sys/nfs/nfs_diskless.c so that it
 * can be shared by both NFS clients. It is declared here so that it
 * will be defined for kernels built without NFS_ROOT, although it
 * isn't used in that case.
 */
#if !defined(NFS_ROOT) && !defined(NFSCLIENT)
struct nfs_diskless	nfs_diskless = { { { 0 } } };
struct nfsv3_diskless	nfsv3_diskless = { { { 0 } } };
int			nfs_diskless_valid = 0;
#endif

SYSCTL_INT(_vfs_newnfs, OID_AUTO, diskless_valid, CTLFLAG_RD,
    &nfs_diskless_valid, 0,
    "Has the diskless struct been filled correctly");

SYSCTL_STRING(_vfs_newnfs, OID_AUTO, diskless_rootpath, CTLFLAG_RD,
    nfsv3_diskless.root_hostnam, 0, "Path to nfs root");

SYSCTL_OPAQUE(_vfs_newnfs, OID_AUTO, diskless_rootaddr, CTLFLAG_RD,
    &nfsv3_diskless.root_saddr, sizeof(nfsv3_diskless.root_saddr),
    "%Ssockaddr_in", "Diskless root nfs address");


void		newnfsargs_ntoh(struct nfs_args *);
static int	nfs_mountdiskless(char *,
		    struct sockaddr_in *, struct nfs_args *,
		    struct thread *, struct vnode **, struct mount *);
static void	nfs_convert_diskless(void);
static void	nfs_convert_oargs(struct nfs_args *args,
		    struct onfs_args *oargs);

int
newnfs_iosize(struct nfsmount *nmp)
{
	int iosize, maxio;

	/* First, set the upper limit for iosize */
	if (nmp->nm_flag & NFSMNT_NFSV4) {
		maxio = NFS_MAXBSIZE;
	} else if (nmp->nm_flag & NFSMNT_NFSV3) {
		if (nmp->nm_sotype == SOCK_DGRAM)
			maxio = NFS_MAXDGRAMDATA;
		else
			maxio = NFS_MAXBSIZE;
	} else {
		maxio = NFS_V2MAXDATA;
	}
	if (nmp->nm_rsize > maxio || nmp->nm_rsize == 0)
		nmp->nm_rsize = maxio;
	if (nmp->nm_rsize > MAXBSIZE)
		nmp->nm_rsize = MAXBSIZE;
	if (nmp->nm_readdirsize > maxio || nmp->nm_readdirsize == 0)
		nmp->nm_readdirsize = maxio;
	if (nmp->nm_readdirsize > nmp->nm_rsize)
		nmp->nm_readdirsize = nmp->nm_rsize;
	if (nmp->nm_wsize > maxio || nmp->nm_wsize == 0)
		nmp->nm_wsize = maxio;
	if (nmp->nm_wsize > MAXBSIZE)
		nmp->nm_wsize = MAXBSIZE;

	/*
	 * Calculate the size used for io buffers.  Use the larger
	 * of the two sizes to minimise nfs requests but make sure
	 * that it is at least one VM page to avoid wasting buffer
	 * space.
	 */
	iosize = imax(nmp->nm_rsize, nmp->nm_wsize);
	iosize = imax(iosize, PAGE_SIZE);
	nmp->nm_mountp->mnt_stat.f_iosize = iosize;
	return (iosize);
}

static void
nfs_convert_oargs(struct nfs_args *args, struct onfs_args *oargs)
{

	args->version = NFS_ARGSVERSION;
	args->addr = oargs->addr;
	args->addrlen = oargs->addrlen;
	args->sotype = oargs->sotype;
	args->proto = oargs->proto;
	args->fh = oargs->fh;
	args->fhsize = oargs->fhsize;
	args->flags = oargs->flags;
	args->wsize = oargs->wsize;
	args->rsize = oargs->rsize;
	args->readdirsize = oargs->readdirsize;
	args->timeo = oargs->timeo;
	args->retrans = oargs->retrans;
	args->readahead = oargs->readahead;
	args->hostname = oargs->hostname;
}

static void
nfs_convert_diskless(void)
{

	bcopy(&nfs_diskless.myif, &nfsv3_diskless.myif,
		sizeof(struct ifaliasreq));
	bcopy(&nfs_diskless.mygateway, &nfsv3_diskless.mygateway,
		sizeof(struct sockaddr_in));
	nfs_convert_oargs(&nfsv3_diskless.root_args,&nfs_diskless.root_args);
	if (nfsv3_diskless.root_args.flags & NFSMNT_NFSV3) {
		nfsv3_diskless.root_fhsize = NFSX_MYFH;
		bcopy(nfs_diskless.root_fh, nfsv3_diskless.root_fh, NFSX_MYFH);
	} else {
		nfsv3_diskless.root_fhsize = NFSX_V2FH;
		bcopy(nfs_diskless.root_fh, nfsv3_diskless.root_fh, NFSX_V2FH);
	}
	bcopy(&nfs_diskless.root_saddr,&nfsv3_diskless.root_saddr,
		sizeof(struct sockaddr_in));
	bcopy(nfs_diskless.root_hostnam, nfsv3_diskless.root_hostnam, MNAMELEN);
	nfsv3_diskless.root_time = nfs_diskless.root_time;
	bcopy(nfs_diskless.my_hostnam, nfsv3_diskless.my_hostnam,
		MAXHOSTNAMELEN);
	nfs_diskless_valid = 3;
}

/*
 * nfs statfs call
 */
static int
nfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct vnode *vp;
	struct thread *td;
	struct nfsmount *nmp = VFSTONFS(mp);
	struct nfsvattr nfsva;
	struct nfsfsinfo fs;
	struct nfsstatfs sb;
	int error = 0, attrflag, gotfsinfo = 0, ret;
	struct nfsnode *np;

	td = curthread;

	error = vfs_busy(mp, MBF_NOWAIT);
	if (error)
		return (error);
	error = ncl_nget(mp, nmp->nm_fh, nmp->nm_fhsize, &np, LK_EXCLUSIVE);
	if (error) {
		vfs_unbusy(mp);
		return (error);
	}
	vp = NFSTOV(np);
	mtx_lock(&nmp->nm_mtx);
	if (NFSHASNFSV3(nmp) && !NFSHASGOTFSINFO(nmp)) {
		mtx_unlock(&nmp->nm_mtx);
		error = nfsrpc_fsinfo(vp, &fs, td->td_ucred, td, &nfsva,
		    &attrflag, NULL);
		if (!error)
			gotfsinfo = 1;
	} else
		mtx_unlock(&nmp->nm_mtx);
	if (!error)
		error = nfsrpc_statfs(vp, &sb, &fs, td->td_ucred, td, &nfsva,
		    &attrflag, NULL);
	if (attrflag == 0) {
		ret = nfsrpc_getattrnovp(nmp, nmp->nm_fh, nmp->nm_fhsize, 1,
		    td->td_ucred, td, &nfsva, NULL);
		if (ret) {
			/*
			 * Just set default values to get things going.
			 */
			NFSBZERO((caddr_t)&nfsva, sizeof (struct nfsvattr));
			nfsva.na_vattr.va_type = VDIR;
			nfsva.na_vattr.va_mode = 0777;
			nfsva.na_vattr.va_nlink = 100;
			nfsva.na_vattr.va_uid = (uid_t)0;
			nfsva.na_vattr.va_gid = (gid_t)0;
			nfsva.na_vattr.va_fileid = 2;
			nfsva.na_vattr.va_gen = 1;
			nfsva.na_vattr.va_blocksize = NFS_FABLKSIZE;
			nfsva.na_vattr.va_size = 512 * 1024;
		}
	}
	(void) nfscl_loadattrcache(&vp, &nfsva, NULL, NULL, 0, 1);
	if (!error) {
	    mtx_lock(&nmp->nm_mtx);
	    if (gotfsinfo || (nmp->nm_flag & NFSMNT_NFSV4))
		nfscl_loadfsinfo(nmp, &fs);
	    nfscl_loadsbinfo(nmp, &sb, sbp);
	    sbp->f_iosize = newnfs_iosize(nmp);
	    mtx_unlock(&nmp->nm_mtx);
	    if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	    }
	    strncpy(&sbp->f_fstypename[0], mp->mnt_vfc->vfc_name, MFSNAMELEN);
	} else if (NFS_ISV4(vp)) {
		error = nfscl_maperr(td, error, (uid_t)0, (gid_t)0);
	}
	vput(vp);
	vfs_unbusy(mp);
	return (error);
}

/*
 * nfs version 3 fsinfo rpc call
 */
int
ncl_fsinfo(struct nfsmount *nmp, struct vnode *vp, struct ucred *cred,
    struct thread *td)
{
	struct nfsfsinfo fs;
	struct nfsvattr nfsva;
	int error, attrflag;
	
	error = nfsrpc_fsinfo(vp, &fs, cred, td, &nfsva, &attrflag, NULL);
	if (!error) {
		if (attrflag)
			(void) nfscl_loadattrcache(&vp, &nfsva, NULL, NULL, 0,
			    1);
		mtx_lock(&nmp->nm_mtx);
		nfscl_loadfsinfo(nmp, &fs);
		mtx_unlock(&nmp->nm_mtx);
	}
	return (error);
}

/*
 * Mount a remote root fs via. nfs. This depends on the info in the
 * nfs_diskless structure that has been filled in properly by some primary
 * bootstrap.
 * It goes something like this:
 * - do enough of "ifconfig" by calling ifioctl() so that the system
 *   can talk to the server
 * - If nfs_diskless.mygateway is filled in, use that address as
 *   a default gateway.
 * - build the rootfs mount point and call mountnfs() to do the rest.
 *
 * It is assumed to be safe to read, modify, and write the nfsv3_diskless
 * structure, as well as other global NFS client variables here, as
 * nfs_mountroot() will be called once in the boot before any other NFS
 * client activity occurs.
 */
static int
nfs_mountroot(struct mount *mp)
{
	struct thread *td = curthread;
	struct nfsv3_diskless *nd = &nfsv3_diskless;
	struct socket *so;
	struct vnode *vp;
	struct ifreq ir;
	int error;
	u_long l;
	char buf[128];
	char *cp;

#if defined(BOOTP_NFSROOT) && defined(BOOTP)
	bootpc_init();		/* use bootp to get nfs_diskless filled in */
#elif defined(NFS_ROOT)
	nfs_setup_diskless();
#endif

	if (nfs_diskless_valid == 0)
		return (-1);
	if (nfs_diskless_valid == 1)
		nfs_convert_diskless();

	/*
	 * XXX splnet, so networks will receive...
	 */
	splnet();

	/*
	 * Do enough of ifconfig(8) so that the critical net interface can
	 * talk to the server.
	 */
	error = socreate(nd->myif.ifra_addr.sa_family, &so, nd->root_args.sotype, 0,
	    td->td_ucred, td);
	if (error)
		panic("nfs_mountroot: socreate(%04x): %d",
			nd->myif.ifra_addr.sa_family, error);

#if 0 /* XXX Bad idea */
	/*
	 * We might not have been told the right interface, so we pass
	 * over the first ten interfaces of the same kind, until we get
	 * one of them configured.
	 */

	for (i = strlen(nd->myif.ifra_name) - 1;
		nd->myif.ifra_name[i] >= '0' &&
		nd->myif.ifra_name[i] <= '9';
		nd->myif.ifra_name[i] ++) {
		error = ifioctl(so, SIOCAIFADDR, (caddr_t)&nd->myif, td);
		if(!error)
			break;
	}
#endif
	error = ifioctl(so, SIOCAIFADDR, (caddr_t)&nd->myif, td);
	if (error)
		panic("nfs_mountroot: SIOCAIFADDR: %d", error);
	if ((cp = getenv("boot.netif.mtu")) != NULL) {
		ir.ifr_mtu = strtol(cp, NULL, 10);
		bcopy(nd->myif.ifra_name, ir.ifr_name, IFNAMSIZ);
		freeenv(cp);
		error = ifioctl(so, SIOCSIFMTU, (caddr_t)&ir, td);
		if (error)
			printf("nfs_mountroot: SIOCSIFMTU: %d", error);
	}
	soclose(so);

	/*
	 * If the gateway field is filled in, set it as the default route.
	 * Note that pxeboot will set a default route of 0 if the route
	 * is not set by the DHCP server.  Check also for a value of 0
	 * to avoid panicking inappropriately in that situation.
	 */
	if (nd->mygateway.sin_len != 0 &&
	    nd->mygateway.sin_addr.s_addr != 0) {
		struct sockaddr_in mask, sin;

		bzero((caddr_t)&mask, sizeof(mask));
		sin = mask;
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
                /* XXX MRT use table 0 for this sort of thing */
		CURVNET_SET(TD_TO_VNET(td));
		error = rtrequest(RTM_ADD, (struct sockaddr *)&sin,
		    (struct sockaddr *)&nd->mygateway,
		    (struct sockaddr *)&mask,
		    RTF_UP | RTF_GATEWAY, NULL);
		CURVNET_RESTORE();
		if (error)
			panic("nfs_mountroot: RTM_ADD: %d", error);
	}

	/*
	 * Create the rootfs mount point.
	 */
	nd->root_args.fh = nd->root_fh;
	nd->root_args.fhsize = nd->root_fhsize;
	l = ntohl(nd->root_saddr.sin_addr.s_addr);
	snprintf(buf, sizeof(buf), "%ld.%ld.%ld.%ld:%s",
		(l >> 24) & 0xff, (l >> 16) & 0xff,
		(l >>  8) & 0xff, (l >>  0) & 0xff, nd->root_hostnam);
	printf("NFS ROOT: %s\n", buf);
	nd->root_args.hostname = buf;
	if ((error = nfs_mountdiskless(buf,
	    &nd->root_saddr, &nd->root_args, td, &vp, mp)) != 0) {
		return (error);
	}

	/*
	 * This is not really an nfs issue, but it is much easier to
	 * set hostname here and then let the "/etc/rc.xxx" files
	 * mount the right /var based upon its preset value.
	 */
	mtx_lock(&prison0.pr_mtx);
	strlcpy(prison0.pr_hostname, nd->my_hostnam,
	    sizeof(prison0.pr_hostname));
	mtx_unlock(&prison0.pr_mtx);
	inittodr(ntohl(nd->root_time));
	return (0);
}

/*
 * Internal version of mount system call for diskless setup.
 */
static int
nfs_mountdiskless(char *path,
    struct sockaddr_in *sin, struct nfs_args *args, struct thread *td,
    struct vnode **vpp, struct mount *mp)
{
	struct sockaddr *nam;
	int dirlen, error;
	char *dirpath;

	/*
	 * Find the directory path in "path", which also has the server's
	 * name/ip address in it.
	 */
	dirpath = strchr(path, ':');
	if (dirpath != NULL)
		dirlen = strlen(++dirpath);
	else
		dirlen = 0;
	nam = sodupsockaddr((struct sockaddr *)sin, M_WAITOK);
	if ((error = mountnfs(args, mp, nam, path, NULL, 0, dirpath, dirlen,
	    NULL, 0, vpp, td->td_ucred, td, NFS_DEFAULT_NEGNAMETIMEO)) != 0) {
		printf("nfs_mountroot: mount %s on /: %d\n", path, error);
		return (error);
	}
	return (0);
}

static void
nfs_sec_name(char *sec, int *flagsp)
{
	if (!strcmp(sec, "krb5"))
		*flagsp |= NFSMNT_KERB;
	else if (!strcmp(sec, "krb5i"))
		*flagsp |= (NFSMNT_KERB | NFSMNT_INTEGRITY);
	else if (!strcmp(sec, "krb5p"))
		*flagsp |= (NFSMNT_KERB | NFSMNT_PRIVACY);
}

static void
nfs_decode_args(struct mount *mp, struct nfsmount *nmp, struct nfs_args *argp,
    const char *hostname, struct ucred *cred, struct thread *td)
{
	int s;
	int adjsock;
	char *p;

	s = splnet();

	/*
	 * Set read-only flag if requested; otherwise, clear it if this is
	 * an update.  If this is not an update, then either the read-only
	 * flag is already clear, or this is a root mount and it was set
	 * intentionally at some previous point.
	 */
	if (vfs_getopt(mp->mnt_optnew, "ro", NULL, NULL) == 0) {
		MNT_ILOCK(mp);
		mp->mnt_flag |= MNT_RDONLY;
		MNT_IUNLOCK(mp);
	} else if (mp->mnt_flag & MNT_UPDATE) {
		MNT_ILOCK(mp);
		mp->mnt_flag &= ~MNT_RDONLY;
		MNT_IUNLOCK(mp);
	}

	/*
	 * Silently clear NFSMNT_NOCONN if it's a TCP mount, it makes
	 * no sense in that context.  Also, set up appropriate retransmit
	 * and soft timeout behavior.
	 */
	if (argp->sotype == SOCK_STREAM) {
		nmp->nm_flag &= ~NFSMNT_NOCONN;
		nmp->nm_timeo = NFS_MAXTIMEO;
		if ((argp->flags & NFSMNT_NFSV4) != 0)
			nmp->nm_retry = INT_MAX;
		else
			nmp->nm_retry = NFS_RETRANS_TCP;
	}

	/* Also clear RDIRPLUS if NFSv2, it crashes some servers */
	if ((argp->flags & (NFSMNT_NFSV3 | NFSMNT_NFSV4)) == 0) {
		argp->flags &= ~NFSMNT_RDIRPLUS;
		nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
	}

	/* Clear NFSMNT_RESVPORT for NFSv4, since it is not required. */
	if ((argp->flags & NFSMNT_NFSV4) != 0) {
		argp->flags &= ~NFSMNT_RESVPORT;
		nmp->nm_flag &= ~NFSMNT_RESVPORT;
	}

	/* Re-bind if rsrvd port requested and wasn't on one */
	adjsock = !(nmp->nm_flag & NFSMNT_RESVPORT)
		  && (argp->flags & NFSMNT_RESVPORT);
	/* Also re-bind if we're switching to/from a connected UDP socket */
	adjsock |= ((nmp->nm_flag & NFSMNT_NOCONN) !=
		    (argp->flags & NFSMNT_NOCONN));

	/* Update flags atomically.  Don't change the lock bits. */
	nmp->nm_flag = argp->flags | nmp->nm_flag;
	splx(s);

	if ((argp->flags & NFSMNT_TIMEO) && argp->timeo > 0) {
		nmp->nm_timeo = (argp->timeo * NFS_HZ + 5) / 10;
		if (nmp->nm_timeo < NFS_MINTIMEO)
			nmp->nm_timeo = NFS_MINTIMEO;
		else if (nmp->nm_timeo > NFS_MAXTIMEO)
			nmp->nm_timeo = NFS_MAXTIMEO;
	}

	if ((argp->flags & NFSMNT_RETRANS) && argp->retrans > 1) {
		nmp->nm_retry = argp->retrans;
		if (nmp->nm_retry > NFS_MAXREXMIT)
			nmp->nm_retry = NFS_MAXREXMIT;
	}

	if ((argp->flags & NFSMNT_WSIZE) && argp->wsize > 0) {
		nmp->nm_wsize = argp->wsize;
		/* Round down to multiple of blocksize */
		nmp->nm_wsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_wsize <= 0)
			nmp->nm_wsize = NFS_FABLKSIZE;
	}

	if ((argp->flags & NFSMNT_RSIZE) && argp->rsize > 0) {
		nmp->nm_rsize = argp->rsize;
		/* Round down to multiple of blocksize */
		nmp->nm_rsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_rsize <= 0)
			nmp->nm_rsize = NFS_FABLKSIZE;
	}

	if ((argp->flags & NFSMNT_READDIRSIZE) && argp->readdirsize > 0) {
		nmp->nm_readdirsize = argp->readdirsize;
	}

	if ((argp->flags & NFSMNT_ACREGMIN) && argp->acregmin >= 0)
		nmp->nm_acregmin = argp->acregmin;
	else
		nmp->nm_acregmin = NFS_MINATTRTIMO;
	if ((argp->flags & NFSMNT_ACREGMAX) && argp->acregmax >= 0)
		nmp->nm_acregmax = argp->acregmax;
	else
		nmp->nm_acregmax = NFS_MAXATTRTIMO;
	if ((argp->flags & NFSMNT_ACDIRMIN) && argp->acdirmin >= 0)
		nmp->nm_acdirmin = argp->acdirmin;
	else
		nmp->nm_acdirmin = NFS_MINDIRATTRTIMO;
	if ((argp->flags & NFSMNT_ACDIRMAX) && argp->acdirmax >= 0)
		nmp->nm_acdirmax = argp->acdirmax;
	else
		nmp->nm_acdirmax = NFS_MAXDIRATTRTIMO;
	if (nmp->nm_acdirmin > nmp->nm_acdirmax)
		nmp->nm_acdirmin = nmp->nm_acdirmax;
	if (nmp->nm_acregmin > nmp->nm_acregmax)
		nmp->nm_acregmin = nmp->nm_acregmax;

	if ((argp->flags & NFSMNT_READAHEAD) && argp->readahead >= 0) {
		if (argp->readahead <= NFS_MAXRAHEAD)
			nmp->nm_readahead = argp->readahead;
		else
			nmp->nm_readahead = NFS_MAXRAHEAD;
	}
	if ((argp->flags & NFSMNT_WCOMMITSIZE) && argp->wcommitsize >= 0) {
		if (argp->wcommitsize < nmp->nm_wsize)
			nmp->nm_wcommitsize = nmp->nm_wsize;
		else
			nmp->nm_wcommitsize = argp->wcommitsize;
	}

	adjsock |= ((nmp->nm_sotype != argp->sotype) ||
		    (nmp->nm_soproto != argp->proto));

	if (nmp->nm_client != NULL && adjsock) {
		int haslock = 0, error = 0;

		if (nmp->nm_sotype == SOCK_STREAM) {
			error = newnfs_sndlock(&nmp->nm_sockreq.nr_lock);
			if (!error)
				haslock = 1;
		}
		if (!error) {
		    newnfs_disconnect(&nmp->nm_sockreq);
		    if (haslock)
			newnfs_sndunlock(&nmp->nm_sockreq.nr_lock);
		    nmp->nm_sotype = argp->sotype;
		    nmp->nm_soproto = argp->proto;
		    if (nmp->nm_sotype == SOCK_DGRAM)
			while (newnfs_connect(nmp, &nmp->nm_sockreq,
			    cred, td, 0)) {
				printf("newnfs_args: retrying connect\n");
				(void) nfs_catnap(PSOCK, 0, "newnfscon");
			}
		}
	} else {
		nmp->nm_sotype = argp->sotype;
		nmp->nm_soproto = argp->proto;
	}

	if (hostname != NULL) {
		strlcpy(nmp->nm_hostname, hostname,
		    sizeof(nmp->nm_hostname));
		p = strchr(nmp->nm_hostname, ':');
		if (p != NULL)
			*p = '\0';
	}
}

static const char *nfs_opts[] = { "from", "nfs_args",
    "noatime", "noexec", "suiddir", "nosuid", "nosymfollow", "union",
    "noclusterr", "noclusterw", "multilabel", "acls", "force", "update",
    "async", "noconn", "nolockd", "conn", "lockd", "intr", "rdirplus",
    "readdirsize", "soft", "hard", "mntudp", "tcp", "udp", "wsize", "rsize",
    "retrans", "acregmin", "acregmax", "acdirmin", "acdirmax", "resvport",
    "readahead", "hostname", "timeout", "addr", "fh", "nfsv3", "sec",
    "principal", "nfsv4", "gssname", "allgssname", "dirpath",
    "negnametimeo",
    NULL };

/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * bcopy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
static int
nfs_mount(struct mount *mp)
{
	struct nfs_args args = {
	    .version = NFS_ARGSVERSION,
	    .addr = NULL,
	    .addrlen = sizeof (struct sockaddr_in),
	    .sotype = SOCK_STREAM,
	    .proto = 0,
	    .fh = NULL,
	    .fhsize = 0,
	    .flags = NFSMNT_RESVPORT,
	    .wsize = NFS_WSIZE,
	    .rsize = NFS_RSIZE,
	    .readdirsize = NFS_READDIRSIZE,
	    .timeo = 10,
	    .retrans = NFS_RETRANS,
	    .readahead = NFS_DEFRAHEAD,
	    .wcommitsize = 0,			/* was: NQ_DEFLEASE */
	    .hostname = NULL,
	    .acregmin = NFS_MINATTRTIMO,
	    .acregmax = NFS_MAXATTRTIMO,
	    .acdirmin = NFS_MINDIRATTRTIMO,
	    .acdirmax = NFS_MAXDIRATTRTIMO,
	};
	int error = 0, ret, len;
	struct sockaddr *nam = NULL;
	struct vnode *vp;
	struct thread *td;
	char hst[MNAMELEN];
	u_char nfh[NFSX_FHMAX], krbname[100], dirpath[100], srvkrbname[100];
	char *opt, *name, *secname;
	int negnametimeo = NFS_DEFAULT_NEGNAMETIMEO;
	int dirlen, has_nfs_args_opt, krbnamelen, srvkrbnamelen;

	has_nfs_args_opt = 0;
	if (vfs_filteropt(mp->mnt_optnew, nfs_opts)) {
		error = EINVAL;
		goto out;
	}

	td = curthread;
	if ((mp->mnt_flag & (MNT_ROOTFS | MNT_UPDATE)) == MNT_ROOTFS) {
		error = nfs_mountroot(mp);
		goto out;
	}

	nfscl_init();

	/*
	 * The old mount_nfs program passed the struct nfs_args
	 * from userspace to kernel.  The new mount_nfs program
	 * passes string options via nmount() from userspace to kernel
	 * and we populate the struct nfs_args in the kernel.
	 */
	if (vfs_getopt(mp->mnt_optnew, "nfs_args", NULL, NULL) == 0) {
		error = vfs_copyopt(mp->mnt_optnew, "nfs_args", &args,
		    sizeof(args));
		if (error != 0)
			goto out;

		if (args.version != NFS_ARGSVERSION) {
			error = EPROGMISMATCH;
			goto out;
		}
		has_nfs_args_opt = 1;
	}

	/* Handle the new style options. */
	if (vfs_getopt(mp->mnt_optnew, "noconn", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOCONN;
	if (vfs_getopt(mp->mnt_optnew, "conn", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOCONN;
	if (vfs_getopt(mp->mnt_optnew, "nolockd", NULL, NULL) == 0)
		args.flags |= NFSMNT_NOLOCKD;
	if (vfs_getopt(mp->mnt_optnew, "lockd", NULL, NULL) == 0)
		args.flags &= ~NFSMNT_NOLOCKD;
	if (vfs_getopt(mp->mnt_optnew, "intr", NULL, NULL) == 0)
		args.flags |= NFSMNT_INT;
	if (vfs_getopt(mp->mnt_optnew, "rdirplus", NULL, NULL) == 0)
		args.flags |= NFSMNT_RDIRPLUS;
	if (vfs_getopt(mp->mnt_optnew, "resvport", NULL, NULL) == 0)
		args.flags |= NFSMNT_RESVPORT;
	if (vfs_getopt(mp->mnt_optnew, "noresvport", NULL, NULL) == 0)
		args.flags &= ~NFSMNT_RESVPORT;
	if (vfs_getopt(mp->mnt_optnew, "soft", NULL, NULL) == 0)
		args.flags |= NFSMNT_SOFT;
	if (vfs_getopt(mp->mnt_optnew, "hard", NULL, NULL) == 0)
		args.flags &= ~NFSMNT_SOFT;
	if (vfs_getopt(mp->mnt_optnew, "mntudp", NULL, NULL) == 0)
		args.sotype = SOCK_DGRAM;
	if (vfs_getopt(mp->mnt_optnew, "udp", NULL, NULL) == 0)
		args.sotype = SOCK_DGRAM;
	if (vfs_getopt(mp->mnt_optnew, "tcp", NULL, NULL) == 0)
		args.sotype = SOCK_STREAM;
	if (vfs_getopt(mp->mnt_optnew, "nfsv3", NULL, NULL) == 0)
		args.flags |= NFSMNT_NFSV3;
	if (vfs_getopt(mp->mnt_optnew, "nfsv4", NULL, NULL) == 0) {
		args.flags |= NFSMNT_NFSV4;
		args.sotype = SOCK_STREAM;
	}
	if (vfs_getopt(mp->mnt_optnew, "allgssname", NULL, NULL) == 0)
		args.flags |= NFSMNT_ALLGSSNAME;
	if (vfs_getopt(mp->mnt_optnew, "readdirsize", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal readdirsize");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.readdirsize);
		if (ret != 1 || args.readdirsize <= 0) {
			vfs_mount_error(mp, "illegal readdirsize: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_READDIRSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "readahead", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal readahead");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.readahead);
		if (ret != 1 || args.readahead <= 0) {
			vfs_mount_error(mp, "illegal readahead: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_READAHEAD;
	}
	if (vfs_getopt(mp->mnt_optnew, "wsize", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal wsize");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.wsize);
		if (ret != 1 || args.wsize <= 0) {
			vfs_mount_error(mp, "illegal wsize: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_WSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "rsize", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal rsize");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.rsize);
		if (ret != 1 || args.rsize <= 0) {
			vfs_mount_error(mp, "illegal wsize: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_RSIZE;
	}
	if (vfs_getopt(mp->mnt_optnew, "retrans", (void **)&opt, NULL) == 0) {
		if (opt == NULL) { 
			vfs_mount_error(mp, "illegal retrans");
			error = EINVAL;
			goto out;
		}
		ret = sscanf(opt, "%d", &args.retrans);
		if (ret != 1 || args.retrans <= 0) {
			vfs_mount_error(mp, "illegal retrans: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_RETRANS;
	}
	if (vfs_getopt(mp->mnt_optnew, "acregmin", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acregmin);
		if (ret != 1 || args.acregmin < 0) {
			vfs_mount_error(mp, "illegal acregmin: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACREGMIN;
	}
	if (vfs_getopt(mp->mnt_optnew, "acregmax", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acregmax);
		if (ret != 1 || args.acregmax < 0) {
			vfs_mount_error(mp, "illegal acregmax: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACREGMAX;
	}
	if (vfs_getopt(mp->mnt_optnew, "acdirmin", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acdirmin);
		if (ret != 1 || args.acdirmin < 0) {
			vfs_mount_error(mp, "illegal acdirmin: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACDIRMIN;
	}
	if (vfs_getopt(mp->mnt_optnew, "acdirmax", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.acdirmax);
		if (ret != 1 || args.acdirmax < 0) {
			vfs_mount_error(mp, "illegal acdirmax: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_ACDIRMAX;
	}
	if (vfs_getopt(mp->mnt_optnew, "timeout", (void **)&opt, NULL) == 0) {
		ret = sscanf(opt, "%d", &args.timeo);
		if (ret != 1 || args.timeo <= 0) {
			vfs_mount_error(mp, "illegal timeout: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
		args.flags |= NFSMNT_TIMEO;
	}
	if (vfs_getopt(mp->mnt_optnew, "negnametimeo", (void **)&opt, NULL)
	    == 0) {
		ret = sscanf(opt, "%d", &negnametimeo);
		if (ret != 1 || negnametimeo < 0) {
			vfs_mount_error(mp, "illegal negnametimeo: %s",
			    opt);
			error = EINVAL;
			goto out;
		}
	}
	if (vfs_getopt(mp->mnt_optnew, "sec",
		(void **) &secname, NULL) == 0)
		nfs_sec_name(secname, &args.flags);

	if (mp->mnt_flag & MNT_UPDATE) {
		struct nfsmount *nmp = VFSTONFS(mp);

		if (nmp == NULL) {
			error = EIO;
			goto out;
		}
		/*
		 * When doing an update, we can't change version,
		 * security, switch lockd strategies or change cookie
		 * translation
		 */
		args.flags = (args.flags &
		    ~(NFSMNT_NFSV3 |
		      NFSMNT_NFSV4 |
		      NFSMNT_KERB |
		      NFSMNT_INTEGRITY |
		      NFSMNT_PRIVACY |
		      NFSMNT_NOLOCKD /*|NFSMNT_XLATECOOKIE*/)) |
		    (nmp->nm_flag &
			(NFSMNT_NFSV3 |
			 NFSMNT_NFSV4 |
			 NFSMNT_KERB |
			 NFSMNT_INTEGRITY |
			 NFSMNT_PRIVACY |
			 NFSMNT_NOLOCKD /*|NFSMNT_XLATECOOKIE*/));
		nfs_decode_args(mp, nmp, &args, NULL, td->td_ucred, td);
		goto out;
	}

	/*
	 * Make the nfs_ip_paranoia sysctl serve as the default connection
	 * or no-connection mode for those protocols that support 
	 * no-connection mode (the flag will be cleared later for protocols
	 * that do not support no-connection mode).  This will allow a client
	 * to receive replies from a different IP then the request was
	 * sent to.  Note: default value for nfs_ip_paranoia is 1 (paranoid),
	 * not 0.
	 */
	if (nfs_ip_paranoia == 0)
		args.flags |= NFSMNT_NOCONN;

	if (has_nfs_args_opt != 0) {
		/*
		 * In the 'nfs_args' case, the pointers in the args
		 * structure are in userland - we copy them in here.
		 */
		if (args.fhsize < 0 || args.fhsize > NFSX_V3FHMAX) {
			vfs_mount_error(mp, "Bad file handle");
			error = EINVAL;
			goto out;
		}
		error = copyin((caddr_t)args.fh, (caddr_t)nfh,
		    args.fhsize);
		if (error != 0)
			goto out;
		error = copyinstr(args.hostname, hst, MNAMELEN - 1, &len);
		if (error != 0)
			goto out;
		bzero(&hst[len], MNAMELEN - len);
		args.hostname = hst;
		/* sockargs() call must be after above copyin() calls */
		error = getsockaddr(&nam, (caddr_t)args.addr,
		    args.addrlen);
		if (error != 0)
			goto out;
	} else {
		if (vfs_getopt(mp->mnt_optnew, "fh", (void **)&args.fh,
		    &args.fhsize) == 0) {
			if (args.fhsize < 0 || args.fhsize > NFSX_FHMAX) {
				vfs_mount_error(mp, "Bad file handle");
				error = EINVAL;
				goto out;
			}
			bcopy(args.fh, nfh, args.fhsize);
		} else {
			args.fhsize = 0;
		}
		(void) vfs_getopt(mp->mnt_optnew, "hostname",
		    (void **)&args.hostname, &len);
		if (args.hostname == NULL) {
			vfs_mount_error(mp, "Invalid hostname");
			error = EINVAL;
			goto out;
		}
		bcopy(args.hostname, hst, MNAMELEN);
		hst[MNAMELEN - 1] = '\0';
	}

	if (vfs_getopt(mp->mnt_optnew, "principal", (void **)&name, NULL) == 0)
		strlcpy(srvkrbname, name, sizeof (srvkrbname));
	else
		snprintf(srvkrbname, sizeof (srvkrbname), "nfs@%s", hst);
	srvkrbnamelen = strlen(srvkrbname);

	if (vfs_getopt(mp->mnt_optnew, "gssname", (void **)&name, NULL) == 0)
		strlcpy(krbname, name, sizeof (krbname));
	else
		krbname[0] = '\0';
	krbnamelen = strlen(krbname);

	if (vfs_getopt(mp->mnt_optnew, "dirpath", (void **)&name, NULL) == 0)
		strlcpy(dirpath, name, sizeof (dirpath));
	else
		dirpath[0] = '\0';
	dirlen = strlen(dirpath);

	if (has_nfs_args_opt == 0 && vfs_getopt(mp->mnt_optnew, "addr",
	    (void **)&args.addr, &args.addrlen) == 0) {
		if (args.addrlen > SOCK_MAXADDRLEN) {
			error = ENAMETOOLONG;
			goto out;
		}
		nam = malloc(args.addrlen, M_SONAME, M_WAITOK);
		bcopy(args.addr, nam, args.addrlen);
		nam->sa_len = args.addrlen;
	}

	args.fh = nfh;
	error = mountnfs(&args, mp, nam, hst, krbname, krbnamelen, dirpath,
	    dirlen, srvkrbname, srvkrbnamelen, &vp, td->td_ucred, td,
	    negnametimeo);
out:
	if (!error) {
		MNT_ILOCK(mp);
		mp->mnt_kern_flag |= (MNTK_MPSAFE|MNTK_LOOKUP_SHARED);
		MNT_IUNLOCK(mp);
	}
	return (error);
}


/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * bcopy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
static int
nfs_cmount(struct mntarg *ma, void *data, int flags)
{
	int error;
	struct nfs_args args;

	error = copyin(data, &args, sizeof (struct nfs_args));
	if (error)
		return error;

	ma = mount_arg(ma, "nfs_args", &args, sizeof args);

	error = kernel_mount(ma, flags);
	return (error);
}

/*
 * Common code for mount and mountroot
 */
static int
mountnfs(struct nfs_args *argp, struct mount *mp, struct sockaddr *nam,
    char *hst, u_char *krbname, int krbnamelen, u_char *dirpath, int dirlen,
    u_char *srvkrbname, int srvkrbnamelen, struct vnode **vpp,
    struct ucred *cred, struct thread *td, int negnametimeo)
{
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error, trycnt, ret;
	struct nfsvattr nfsva;
	static u_int64_t clval = 0;

	if (mp->mnt_flag & MNT_UPDATE) {
		nmp = VFSTONFS(mp);
		printf("%s: MNT_UPDATE is no longer handled here\n", __func__);
		FREE(nam, M_SONAME);
		return (0);
	} else {
		MALLOC(nmp, struct nfsmount *, sizeof (struct nfsmount) +
		    krbnamelen + dirlen + srvkrbnamelen + 2,
		    M_NEWNFSMNT, M_WAITOK | M_ZERO);
		TAILQ_INIT(&nmp->nm_bufq);
		if (clval == 0)
			clval = (u_int64_t)nfsboottime.tv_sec;
		nmp->nm_clval = clval++;
		nmp->nm_krbnamelen = krbnamelen;
		nmp->nm_dirpathlen = dirlen;
		nmp->nm_srvkrbnamelen = srvkrbnamelen;
		if (td->td_ucred->cr_uid != (uid_t)0) {
			/*
			 * nm_uid is used to get KerberosV credentials for
			 * the nfsv4 state handling operations if there is
			 * no host based principal set. Use the uid of
			 * this user if not root, since they are doing the
			 * mount. I don't think setting this for root will
			 * work, since root normally does not have user
			 * credentials in a credentials cache.
			 */
			nmp->nm_uid = td->td_ucred->cr_uid;
		} else {
			/*
			 * Just set to -1, so it won't be used.
			 */
			nmp->nm_uid = (uid_t)-1;
		}

		/* Copy and null terminate all the names */
		if (nmp->nm_krbnamelen > 0) {
			bcopy(krbname, nmp->nm_krbname, nmp->nm_krbnamelen);
			nmp->nm_name[nmp->nm_krbnamelen] = '\0';
		}
		if (nmp->nm_dirpathlen > 0) {
			bcopy(dirpath, NFSMNT_DIRPATH(nmp),
			    nmp->nm_dirpathlen);
			nmp->nm_name[nmp->nm_krbnamelen + nmp->nm_dirpathlen
			    + 1] = '\0';
		}
		if (nmp->nm_srvkrbnamelen > 0) {
			bcopy(srvkrbname, NFSMNT_SRVKRBNAME(nmp),
			    nmp->nm_srvkrbnamelen);
			nmp->nm_name[nmp->nm_krbnamelen + nmp->nm_dirpathlen
			    + nmp->nm_srvkrbnamelen + 2] = '\0';
		}
		nmp->nm_sockreq.nr_cred = crhold(cred);
		mtx_init(&nmp->nm_sockreq.nr_mtx, "nfssock", NULL, MTX_DEF);
		mp->mnt_data = nmp;
		nmp->nm_getinfo = nfs_getnlminfo;
		nmp->nm_vinvalbuf = ncl_vinvalbuf;
	}
	vfs_getnewfsid(mp);
	nmp->nm_mountp = mp;
	mtx_init(&nmp->nm_mtx, "NFSmount lock", NULL, MTX_DEF | MTX_DUPOK);			
	nmp->nm_negnametimeo = negnametimeo;

	nfs_decode_args(mp, nmp, argp, hst, cred, td);

	/*
	 * V2 can only handle 32 bit filesizes.  A 4GB-1 limit may be too
	 * high, depending on whether we end up with negative offsets in
	 * the client or server somewhere.  2GB-1 may be safer.
	 *
	 * For V3, ncl_fsinfo will adjust this as necessary.  Assume maximum
	 * that we can handle until we find out otherwise.
	 * XXX Our "safe" limit on the client is what we can store in our
	 * buffer cache using signed(!) block numbers.
	 */
	if ((argp->flags & (NFSMNT_NFSV3 | NFSMNT_NFSV4)) == 0)
		nmp->nm_maxfilesize = 0xffffffffLL;
	else
		nmp->nm_maxfilesize = (u_int64_t)0x80000000 * DEV_BSIZE - 1;

	nmp->nm_timeo = NFS_TIMEO;
	nmp->nm_retry = NFS_RETRANS;
	if ((argp->flags & (NFSMNT_NFSV3 | NFSMNT_NFSV4)) == 0) {
		nmp->nm_wsize = NFS_WSIZE;
		nmp->nm_rsize = NFS_RSIZE;
		nmp->nm_readdirsize = NFS_READDIRSIZE;
	}
	nmp->nm_wcommitsize = hibufspace / (desiredvnodes / 1000);
	nmp->nm_numgrps = NFS_MAXGRPS;
	nmp->nm_readahead = NFS_DEFRAHEAD;
	nmp->nm_tprintf_delay = nfs_tprintf_delay;
	if (nmp->nm_tprintf_delay < 0)
		nmp->nm_tprintf_delay = 0;
	nmp->nm_tprintf_initial_delay = nfs_tprintf_initial_delay;
	if (nmp->nm_tprintf_initial_delay < 0)
		nmp->nm_tprintf_initial_delay = 0;
	nmp->nm_fhsize = argp->fhsize;
	if (nmp->nm_fhsize > 0)
		bcopy((caddr_t)argp->fh, (caddr_t)nmp->nm_fh, argp->fhsize);
	bcopy(hst, mp->mnt_stat.f_mntfromname, MNAMELEN);
	nmp->nm_nam = nam;
	/* Set up the sockets and per-host congestion */
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;
	nmp->nm_sockreq.nr_prog = NFS_PROG;
	if ((argp->flags & NFSMNT_NFSV4))
		nmp->nm_sockreq.nr_vers = NFS_VER4;
	else if ((argp->flags & NFSMNT_NFSV3))
		nmp->nm_sockreq.nr_vers = NFS_VER3;
	else
		nmp->nm_sockreq.nr_vers = NFS_VER2;


	if ((error = newnfs_connect(nmp, &nmp->nm_sockreq, cred, td, 0)))
		goto bad;

	/*
	 * A reference count is needed on the nfsnode representing the
	 * remote root.  If this object is not persistent, then backward
	 * traversals of the mount point (i.e. "..") will not work if
	 * the nfsnode gets flushed out of the cache. Ufs does not have
	 * this problem, because one can identify root inodes by their
	 * number == ROOTINO (2).
	 */
	if (nmp->nm_fhsize == 0 && (nmp->nm_flag & NFSMNT_NFSV4) &&
	    nmp->nm_dirpathlen > 0) {
		/*
		 * If the fhsize on the mount point == 0 for V4, the mount
		 * path needs to be looked up.
		 */
		trycnt = 3;
		do {
			error = nfsrpc_getdirpath(nmp, NFSMNT_DIRPATH(nmp),
			    cred, td);
			if (error)
				(void) nfs_catnap(PZERO, error, "nfsgetdirp");
		} while (error && --trycnt > 0);
		if (error) {
			error = nfscl_maperr(td, error, (uid_t)0, (gid_t)0);
			goto bad;
		}
	}
	if (nmp->nm_fhsize > 0) {
		/*
		 * Set f_iosize to NFS_DIRBLKSIZ so that bo_bsize gets set
		 * non-zero for the root vnode. f_iosize will be set correctly
		 * by nfs_statfs() before any I/O occurs.
		 */
		mp->mnt_stat.f_iosize = NFS_DIRBLKSIZ;
		error = ncl_nget(mp, nmp->nm_fh, nmp->nm_fhsize, &np,
		    LK_EXCLUSIVE);
		if (error)
			goto bad;
		*vpp = NFSTOV(np);
	
		/*
		 * Get file attributes and transfer parameters for the
		 * mountpoint.  This has the side effect of filling in
		 * (*vpp)->v_type with the correct value.
		 */
		ret = nfsrpc_getattrnovp(nmp, nmp->nm_fh, nmp->nm_fhsize, 1,
		    cred, td, &nfsva, NULL);
		if (ret) {
			/*
			 * Just set default values to get things going.
			 */
			NFSBZERO((caddr_t)&nfsva, sizeof (struct nfsvattr));
			nfsva.na_vattr.va_type = VDIR;
			nfsva.na_vattr.va_mode = 0777;
			nfsva.na_vattr.va_nlink = 100;
			nfsva.na_vattr.va_uid = (uid_t)0;
			nfsva.na_vattr.va_gid = (gid_t)0;
			nfsva.na_vattr.va_fileid = 2;
			nfsva.na_vattr.va_gen = 1;
			nfsva.na_vattr.va_blocksize = NFS_FABLKSIZE;
			nfsva.na_vattr.va_size = 512 * 1024;
		}
		(void) nfscl_loadattrcache(vpp, &nfsva, NULL, NULL, 0, 1);
		if (argp->flags & NFSMNT_NFSV3)
			ncl_fsinfo(nmp, *vpp, cred, td);
	
		/*
		 * Lose the lock but keep the ref.
		 */
		VOP_UNLOCK(*vpp, 0);
		return (0);
	}
	error = EIO;

bad:
	newnfs_disconnect(&nmp->nm_sockreq);
	crfree(nmp->nm_sockreq.nr_cred);
	mtx_destroy(&nmp->nm_sockreq.nr_mtx);
	mtx_destroy(&nmp->nm_mtx);
	FREE(nmp, M_NEWNFSMNT);
	FREE(nam, M_SONAME);
	return (error);
}

/*
 * unmount system call
 */
static int
nfs_unmount(struct mount *mp, int mntflags)
{
	struct thread *td;
	struct nfsmount *nmp;
	int error, flags = 0, trycnt = 0;

	td = curthread;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	nmp = VFSTONFS(mp);
	/*
	 * Goes something like this..
	 * - Call vflush() to clear out vnodes for this filesystem
	 * - Close the socket
	 * - Free up the data structures
	 */
	/* In the forced case, cancel any outstanding requests. */
	if (mntflags & MNT_FORCE) {
		error = newnfs_nmcancelreqs(nmp);
		if (error)
			goto out;
		/* For a forced close, get rid of the renew thread now */
		nfscl_umount(nmp, td);
	}
	/* We hold 1 extra ref on the root vnode; see comment in mountnfs(). */
	do {
		error = vflush(mp, 1, flags, td);
		if ((mntflags & MNT_FORCE) && error != 0 && ++trycnt < 30)
			(void) nfs_catnap(PSOCK, error, "newndm");
	} while ((mntflags & MNT_FORCE) && error != 0 && trycnt < 30);
	if (error)
		goto out;

	/*
	 * We are now committed to the unmount.
	 */
	if ((mntflags & MNT_FORCE) == 0)
		nfscl_umount(nmp, td);
	newnfs_disconnect(&nmp->nm_sockreq);
	crfree(nmp->nm_sockreq.nr_cred);
	FREE(nmp->nm_nam, M_SONAME);

	mtx_destroy(&nmp->nm_sockreq.nr_mtx);
	mtx_destroy(&nmp->nm_mtx);
	FREE(nmp, M_NEWNFSMNT);
out:
	return (error);
}

/*
 * Return root of a filesystem
 */
static int
nfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct vnode *vp;
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	nmp = VFSTONFS(mp);
	error = ncl_nget(mp, nmp->nm_fh, nmp->nm_fhsize, &np, flags);
	if (error)
		return error;
	vp = NFSTOV(np);
	/*
	 * Get transfer parameters and attributes for root vnode once.
	 */
	mtx_lock(&nmp->nm_mtx);
	if (NFSHASNFSV3(nmp) && !NFSHASGOTFSINFO(nmp)) {
		mtx_unlock(&nmp->nm_mtx);
		ncl_fsinfo(nmp, vp, curthread->td_ucred, curthread);
	} else 
		mtx_unlock(&nmp->nm_mtx);
	if (vp->v_type == VNON)
	    vp->v_type = VDIR;
	vp->v_vflag |= VV_ROOT;
	*vpp = vp;
	return (0);
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
static int
nfs_sync(struct mount *mp, int waitfor)
{
	struct vnode *vp, *mvp;
	struct thread *td;
	int error, allerror = 0;

	td = curthread;

	/*
	 * Force stale buffer cache information to be flushed.
	 */
	MNT_ILOCK(mp);
loop:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		/* XXX Racy bv_cnt check. */
		if (VOP_ISLOCKED(vp) || vp->v_bufobj.bo_dirty.bv_cnt == 0 ||
		    waitfor == MNT_LAZY) {
			VI_UNLOCK(vp);
			MNT_ILOCK(mp);
			continue;
		}
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td)) {
			MNT_ILOCK(mp);
			MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
			goto loop;
		}
		error = VOP_FSYNC(vp, waitfor, td);
		if (error)
			allerror = error;
		VOP_UNLOCK(vp, 0);
		vrele(vp);

		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (allerror);
}

static int
nfs_sysctl(struct mount *mp, fsctlop_t op, struct sysctl_req *req)
{
	struct nfsmount *nmp = VFSTONFS(mp);
	struct vfsquery vq;
	int error;

	bzero(&vq, sizeof(vq));
	switch (op) {
#if 0
	case VFS_CTL_NOLOCKS:
		val = (nmp->nm_flag & NFSMNT_NOLOCKS) ? 1 : 0;
 		if (req->oldptr != NULL) {
 			error = SYSCTL_OUT(req, &val, sizeof(val));
 			if (error)
 				return (error);
 		}
 		if (req->newptr != NULL) {
 			error = SYSCTL_IN(req, &val, sizeof(val));
 			if (error)
 				return (error);
			if (val)
				nmp->nm_flag |= NFSMNT_NOLOCKS;
			else
				nmp->nm_flag &= ~NFSMNT_NOLOCKS;
 		}
		break;
#endif
	case VFS_CTL_QUERY:
		mtx_lock(&nmp->nm_mtx);
		if (nmp->nm_state & NFSSTA_TIMEO)
			vq.vq_flags |= VQ_NOTRESP;
		mtx_unlock(&nmp->nm_mtx);
#if 0
		if (!(nmp->nm_flag & NFSMNT_NOLOCKS) &&
		    (nmp->nm_state & NFSSTA_LOCKTIMEO))
			vq.vq_flags |= VQ_NOTRESPLOCK;
#endif
		error = SYSCTL_OUT(req, &vq, sizeof(vq));
		break;
 	case VFS_CTL_TIMEO:
 		if (req->oldptr != NULL) {
 			error = SYSCTL_OUT(req, &nmp->nm_tprintf_initial_delay,
 			    sizeof(nmp->nm_tprintf_initial_delay));
 			if (error)
 				return (error);
 		}
 		if (req->newptr != NULL) {
			error = vfs_suser(mp, req->td);
			if (error)
				return (error);
 			error = SYSCTL_IN(req, &nmp->nm_tprintf_initial_delay,
 			    sizeof(nmp->nm_tprintf_initial_delay));
 			if (error)
 				return (error);
 			if (nmp->nm_tprintf_initial_delay < 0)
 				nmp->nm_tprintf_initial_delay = 0;
 		}
		break;
	default:
		return (ENOTSUP);
	}
	return (0);
}

/*
 * Extract the information needed by the nlm from the nfs vnode.
 */
static void
nfs_getnlminfo(struct vnode *vp, uint8_t *fhp, size_t *fhlenp,
    struct sockaddr_storage *sp, int *is_v3p, off_t *sizep,
    struct timeval *timeop)
{
	struct nfsmount *nmp;
	struct nfsnode *np = VTONFS(vp);

	nmp = VFSTONFS(vp->v_mount);
	if (fhlenp != NULL)
		*fhlenp = (size_t)np->n_fhp->nfh_len;
	if (fhp != NULL)
		bcopy(np->n_fhp->nfh_fh, fhp, np->n_fhp->nfh_len);
	if (sp != NULL)
		bcopy(nmp->nm_nam, sp, min(nmp->nm_nam->sa_len, sizeof(*sp)));
	if (is_v3p != NULL)
		*is_v3p = NFS_ISV3(vp);
	if (sizep != NULL)
		*sizep = np->n_size;
	if (timeop != NULL) {
		timeop->tv_sec = nmp->nm_timeo / NFS_HZ;
		timeop->tv_usec = (nmp->nm_timeo % NFS_HZ) * (1000000 / NFS_HZ);
	}
}

