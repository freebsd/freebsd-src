/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/*
 * 29/12/94 Added code from Marc Huber <huber@fzi.de> to allow it to allocate
 * its own major char number! Way cool patch!
 */


#include <sys/param.h>

#if defined(__FreeBSD__)
# ifdef	IPFILTER_LKM
#  ifndef __FreeBSD_cc_version
#   include <osreldate.h>
#  else
#   if __FreeBSD_cc_version < 430000
#    include <osreldate.h>
#   endif
#  endif
#  define	ACTUALLY_LKM_NOT_KERNEL
# else
#  ifndef __FreeBSD_cc_version
#   include <sys/osreldate.h>
#  else
#   if __FreeBSD_cc_version < 430000
#    include <sys/osreldate.h>
#   endif
#  endif
# endif
#endif
#include <sys/systm.h>
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 220000)
# ifndef ACTUALLY_LKM_NOT_KERNEL
#  include "opt_devfs.h"
# endif
# include <sys/conf.h>
# include <sys/kernel.h>
# ifdef DEVFS
#  include <sys/devfsext.h>
# endif /*DEVFS*/
#endif
#include <sys/conf.h>
#include <sys/file.h>
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
# include <sys/lock.h>
#endif
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/mbuf.h>
#if	BSD >= 199506
# include <sys/sysctl.h>
#endif
#if (__FreeBSD_version >= 300000)
# include <sys/socket.h>
#endif
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <net/if.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include "netinet/ipl.h"
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_state.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_proxy.h"


#if !defined(VOP_LEASE) && defined(LEASE_CHECK)
#define	VOP_LEASE	LEASE_CHECK
#endif

#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

int	xxxinit __P((struct lkm_table *, int, int));

#ifdef  SYSCTL_INT
SYSCTL_NODE(_net_inet, OID_AUTO, ipf, CTLFLAG_RW, 0, "IPF");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_flags, CTLFLAG_RW, &fr_flags, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_pass, CTLFLAG_RW, &fr_pass, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_active, CTLFLAG_RD, &fr_active, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_chksrc, CTLFLAG_RW, &fr_chksrc, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_minttl, CTLFLAG_RW, &fr_minttl, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_minttllog, CTLFLAG_RW,
	   &fr_minttllog, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcpidletimeout, CTLFLAG_RW,
	   &fr_tcpidletimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcphalfclosed, CTLFLAG_RW,
	   &fr_tcphalfclosed, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcpclosewait, CTLFLAG_RW,
	   &fr_tcpclosewait, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcplastack, CTLFLAG_RW,
	   &fr_tcplastack, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcptimeout, CTLFLAG_RW,
	   &fr_tcptimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcpclosed, CTLFLAG_RW,
	   &fr_tcpclosed, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_udptimeout, CTLFLAG_RW,
	   &fr_udptimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_icmptimeout, CTLFLAG_RW,
	   &fr_icmptimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_defnatage, CTLFLAG_RW,
	   &fr_defnatage, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_ipfrttl, CTLFLAG_RW,
	   &fr_ipfrttl, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, ipl_unreach, CTLFLAG_RW,
	   &ipl_unreach, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_running, CTLFLAG_RD,
	   &fr_running, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_authsize, CTLFLAG_RD,
	   &fr_authsize, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_authused, CTLFLAG_RD,
	   &fr_authused, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_defaultauthage, CTLFLAG_RW,
	   &fr_defaultauthage, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, ippr_ftp_pasvonly, CTLFLAG_RW,
	   &ippr_ftp_pasvonly, 0, "");
#endif

#ifdef DEVFS
static void *ipf_devfs[IPL_LOGMAX + 1];
#endif

#if !defined(__FreeBSD_version) || (__FreeBSD_version < 220000)
int	ipl_major = 0;

static struct   cdevsw  ipldevsw =
{
        iplopen,                /* open */
        iplclose,               /* close */
        iplread,                /* read */
        (void *)nullop,         /* write */
        iplioctl,               /* ioctl */
        (void *)nullop,         /* stop */
        (void *)nullop,         /* reset */
        (void *)NULL,           /* tty */
        (void *)nullop,         /* select */
        (void *)nullop,         /* mmap */
        NULL                    /* strategy */
};

MOD_DEV(IPL_VERSION, LM_DT_CHAR, -1, &ipldevsw);

extern struct cdevsw cdevsw[];
extern int vd_unuseddev __P((void));
extern int nchrdev;
#else

static struct cdevsw ipl_cdevsw = {
	iplopen,	iplclose,	iplread,	nowrite, /* 79 */
	iplioctl,	nostop,		noreset,	nodevtotty,
#if (__FreeBSD_version >= 300000)
	seltrue,	nommap,		nostrategy,	"ipl",
#else
	noselect,	nommap,		nostrategy,	"ipl",
#endif
	NULL,	-1
};
#endif

static void ipl_drvinit __P((void *));

#ifdef ACTUALLY_LKM_NOT_KERNEL
static  int     if_ipl_unload __P((struct lkm_table *, int));
static  int     if_ipl_load __P((struct lkm_table *, int));
static  int     if_ipl_remove __P((void));
static  int     ipl_major = CDEV_MAJOR;

static int iplaction __P((struct lkm_table *, int));
static char *ipf_devfiles[] = { IPL_NAME, IPL_NAT, IPL_STATE, IPL_AUTH, NULL };

extern	int	lkmenodev __P((void));

static int iplaction(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
#if !defined(__FreeBSD_version) || (__FreeBSD_version < 220000)
	int i = ipl_major;
	struct lkm_dev *args = lkmtp->private.lkm_dev;
#endif
	int err = 0;

	switch (cmd)
	{
	case LKM_E_LOAD :
		if (lkmexists(lkmtp))
			return EEXIST;

#if !defined(__FreeBSD_version) || (__FreeBSD_version < 220000)
		for (i = 0; i < nchrdev; i++)
			if (cdevsw[i].d_open == lkmenodev ||
			    cdevsw[i].d_open == iplopen)
				break;
		if (i == nchrdev) {
			printf("IP Filter: No free cdevsw slots\n");
			return ENODEV;
		}

		ipl_major = i;
		args->lkm_offset = i;   /* slot in cdevsw[] */
#endif
		printf("IP Filter: loaded into slot %d\n", ipl_major);
		err = if_ipl_load(lkmtp, cmd);
		if (!err)
			ipl_drvinit((void *)NULL);
		return err;
		break;
	case LKM_E_UNLOAD :
		err = if_ipl_unload(lkmtp, cmd);
		if (!err) {
			printf("IP Filter: unloaded from slot %d\n",
				ipl_major);
#ifdef	DEVFS
			if (ipf_devfs[IPL_LOGIPF])
				devfs_remove_dev(ipf_devfs[IPL_LOGIPF]);
			if (ipf_devfs[IPL_LOGNAT])
				devfs_remove_dev(ipf_devfs[IPL_LOGNAT]);
			if (ipf_devfs[IPL_LOGSTATE])
				devfs_remove_dev(ipf_devfs[IPL_LOGSTATE]);
			if (ipf_devfs[IPL_LOGAUTH])
				devfs_remove_dev(ipf_devfs[IPL_LOGAUTH]);
#endif
		}
		return err;
	case LKM_E_STAT :
		break;
	default:
		err = EIO;
		break;
	}
	return 0;
}


static int if_ipl_remove __P((void))
{
	char *name;
	struct nameidata nd;
	int error, i;

	for (i = 0; (name = ipf_devfiles[i]); i++) {
		NDINIT(&nd, DELETE, LOCKPARENT, UIO_SYSSPACE, name, curproc);
		if ((error = namei(&nd)))
			return (error);
		VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
#if (__FreeBSD_version >= 300000)
		VOP_LOCK(nd.ni_vp, LK_RETRY | LK_EXCLUSIVE, curproc);
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
		(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);

		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp != NULLVP)
			vput(nd.ni_vp);
#else
		VOP_LOCK(nd.ni_vp);
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
		(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
#endif
	}

	return 0;
}


static int if_ipl_unload(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
	int error = 0;

	error = ipldetach();
	if (!error)
		error = if_ipl_remove();
	return error;
}


static int if_ipl_load(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
	struct nameidata nd;
	struct vattr vattr;
	int error = 0, fmode = S_IFCHR|0600, i;
	char *name;

	error = iplattach();
	if (error)
		return error;
	(void) if_ipl_remove();

	for (i = 0; (name = ipf_devfiles[i]); i++) {
		NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, name, curproc);
		if ((error = namei(&nd)))
			return error;
		if (nd.ni_vp != NULL) {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			if (nd.ni_dvp == nd.ni_vp)
				vrele(nd.ni_dvp);
			else
				vput(nd.ni_dvp);
			vrele(nd.ni_vp);
			return (EEXIST);
		}
		VATTR_NULL(&vattr);
		vattr.va_type = VCHR;
		vattr.va_mode = (fmode & 07777);
		vattr.va_rdev = (ipl_major << 8) | i;
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
#if (__FreeBSD_version >= 300000)
                vput(nd.ni_dvp);
#endif
		if (error)
			return error;
	}
	return 0;
}

#endif  /* actually LKM */

#if defined(__FreeBSD_version) && (__FreeBSD_version < 220000)
/*
 * strlen isn't present in 2.1.* kernels.
 */
size_t strlen(string)
char *string;
{
        register char *s;

        for (s = string; *s; s++)
		;
        return (size_t)(s - string);
}


int xxxinit(lkmtp, cmd, ver)
struct lkm_table *lkmtp;
int cmd, ver;
{
	DISPATCH(lkmtp, cmd, ver, iplaction, iplaction, iplaction);
}
#else	/* __FREEBSD_version >= 220000 */
# ifdef	IPFILTER_LKM
#  include <sys/exec.h>

#  if (__FreeBSD_version >= 300000)
MOD_DEV(if_ipl, LM_DT_CHAR, CDEV_MAJOR, &ipl_cdevsw);
#  else
MOD_DECL(if_ipl);


static struct lkm_dev _module = {
	LM_DEV,
	LKM_VERSION,
	IPL_VERSION,
	CDEV_MAJOR,
	LM_DT_CHAR,
	{ (void *)&ipl_cdevsw }
};
#  endif


int if_ipl __P((struct lkm_table *, int, int));


int if_ipl(lkmtp, cmd, ver)
struct lkm_table *lkmtp;
int cmd, ver;
{
#  if (__FreeBSD_version >= 300000)
	MOD_DISPATCH(if_ipl, lkmtp, cmd, ver, iplaction, iplaction, iplaction);
#  else
	DISPATCH(lkmtp, cmd, ver, iplaction, iplaction, iplaction);
#  endif
}
# endif /* IPFILTER_LKM */
static int	ipl_devsw_installed = 0;

static void ipl_drvinit __P((void *unused))
{
	dev_t dev;
# ifdef	DEVFS
	void **tp = ipf_devfs;
# endif

	if (!ipl_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &ipl_cdevsw, NULL);
		ipl_devsw_installed = 1;

# ifdef	DEVFS
		tp[IPL_LOGIPF] = devfs_add_devswf(&ipl_cdevsw, IPL_LOGIPF,
						  DV_CHR, 0, 0, 0600, "ipf");
		tp[IPL_LOGNAT] = devfs_add_devswf(&ipl_cdevsw, IPL_LOGNAT,
						  DV_CHR, 0, 0, 0600, "ipnat");
		tp[IPL_LOGSTATE] = devfs_add_devswf(&ipl_cdevsw, IPL_LOGSTATE,
					            DV_CHR, 0, 0, 0600,
						    "ipstate");
		tp[IPL_LOGAUTH] = devfs_add_devswf(&ipl_cdevsw, IPL_LOGAUTH,
					           DV_CHR, 0, 0, 0600,
						   "ipauth");
# endif
	}
}

# if defined(IPFILTER_LKM) || \
     defined(__FreeBSD_version) && (__FreeBSD_version >= 220000)
SYSINIT(ipldev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ipl_drvinit,NULL)
# endif /* IPFILTER_LKM */
#endif /* _FreeBSD_version */
