/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
/*
 * 29/12/94 Added code from Marc Huber <huber@fzi.de> to allow it to allocate
 * its own major char number! Way cool patch!
 */


#include <sys/param.h>

/*
 * Post NetBSD 1.2 has the PFIL interface for packet filters.  This turns
 * on those hooks.  We don't need any special mods with this!
 */
#if (defined(NetBSD) && (NetBSD > 199609) && (NetBSD <= 1991011)) || \
    (defined(NetBSD1_2) && NetBSD1_2 > 1)
#  define NETBSD_PF
#endif

#if defined(__FreeBSD__) && (__FreeBSD__ > 1)
# include <osreldate.h>
# ifdef	IPFILTER_LKM
#  define	ACTUALLY_LKM_NOT_KERNEL
# endif
#endif
#include <sys/systm.h>
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 220000)
# include <sys/conf.h>
# include <sys/kernel.h>
# ifdef DEVFS
#  include <sys/devfsext.h>
# endif /*DEVFS*/
#endif
#include <sys/conf.h>
#include <sys/file.h>
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
#if defined(__NetBSD__) || (defined(__FreeBSD_version) && \
    (__FreeBSD_version >= 199511))
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#endif
#ifndef	__NetBSD__
#include <sys/sysent.h>
#endif
#include <sys/lkm.h>
#include "ipl.h"
#include "ip_compat.h"
#include "ip_fil.h"

#ifndef	IPL_NAME
#define	IPL_NAME	"/dev/ipl"
#endif
#define	IPL_NAT		"/dev/ipnat"
#define	IPL_STATE	"/dev/ipstate"

#if !defined(VOP_LEASE) && defined(LEASE_CHECK)
#define	VOP_LEASE	LEASE_CHECK
#endif

#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

extern	int	lkmenodev __P((void));


#ifdef NETBSD_PF
#include <net/pfil.h>
#endif
#ifndef IPFILTER_LOG
# ifdef NETBSD_PF
# define iplread enodev
# else
# define	iplread	nodev
# endif
#endif

#ifdef NETBSD_PF
int        (*fr_checkp) __P((struct ip *, int, struct ifnet *, int, struct mbuf **)) = NULL;
#endif

static	int	ipl_unload __P((void));
static	int	ipl_load __P((void));
static	int	ipl_remove __P((void));
int	xxxinit __P((struct lkm_table *, int, int));


#if (defined(NetBSD1_0) && (NetBSD1_0 > 1)) || \
    (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199511))
struct	cdevsw	ipldevsw = 
{
	iplopen,		/* open */
	iplclose,		/* close */
	iplread,		/* read */
	0,			/* write */
	iplioctl,		/* ioctl */
	0,			/* stop */
	0,			/* tty */
	0,			/* select */
	0,			/* mmap */
	NULL			/* strategy */
};
#else
struct	cdevsw	ipldevsw = 
{
	iplopen,		/* open */
	iplclose,		/* close */
	iplread,		/* read */
	(void *)nullop,		/* write */
	iplioctl,		/* ioctl */
	(void *)nullop,		/* stop */
	(void *)nullop,		/* reset */
	(void *)NULL,		/* tty */
	(void *)nullop,		/* select */
	(void *)nullop,		/* mmap */
	NULL			/* strategy */
};
#endif

#if !defined(__FreeBSD_version) || (__FreeBSD_version < 220000)
int	ipl_major = 0;

MOD_DEV(IPL_VERSION, LM_DT_CHAR, -1, &ipldevsw);

extern struct cdevsw cdevsw[];
extern int vd_unuseddev __P((void));
extern int nchrdev;
#else
int	ipl_major = CDEV_MAJOR;

static struct cdevsw ipl_cdevsw = {
        iplopen,	iplclose,	iplread,	nowrite, /* 79 */
	iplioctl,	nostop,		noreset,	nodevtotty,
	noselect,	nommap,		nostrategy,	"ipl",
	NULL,	-1
};
#endif



static int iplaction(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
	int i = ipl_major;
	struct lkm_dev *args = lkmtp->private.lkm_dev;
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
		return ipl_load();
		break;
	case LKM_E_UNLOAD :
		printf("IP Filter: unloaded from slot %d\n", ipl_major);
		return ipl_unload();
	case LKM_E_STAT :
		break;
	default:
		err = EIO;
		break;
	}
	return 0;
}


static int ipl_remove __P((void))
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, DELETE, LOCKPARENT, UIO_SYSSPACE, IPL_NAME, curproc);
	if ((error = namei(&nd)))
		return (error);
	VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
	VOP_LOCK(nd.ni_vp);
	VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
	(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);

	NDINIT(&nd, DELETE, LOCKPARENT, UIO_SYSSPACE, IPL_NAT, curproc);
	if ((error = namei(&nd)))
		return (error);
	VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
	VOP_LOCK(nd.ni_vp);
	VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
	(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);

	NDINIT(&nd, DELETE, LOCKPARENT, UIO_SYSSPACE, IPL_STATE, curproc);
	if ((error = namei(&nd)))
		return (error);
	VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
	VOP_LOCK(nd.ni_vp);
	VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
	(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
}


static int ipl_unload()
{
	int error = 0;

	error = ipldetach();
#ifdef NETBSD_PF
	pfil_remove_hook(fr_check, PFIL_IN|PFIL_OUT);
#endif
	if (!error)
		error = ipl_remove();
	return error;
}


static int ipl_load()
{
	struct nameidata nd;
	struct vattr vattr;
	int error = 0, fmode = S_IFCHR|0600;

	error = iplattach();
#ifdef NETBSD_PF
	pfil_add_hook(fr_check, PFIL_IN|PFIL_OUT);
#endif
	if (error)
		return error;
	(void) ipl_remove();

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, IPL_NAME, curproc);
	if (error = namei(&nd))
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
	vattr.va_rdev = ipl_major<<8;
	VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
	error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error)
		return error;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, IPL_NAT, curproc);
	if (error = namei(&nd))
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
	vattr.va_rdev = (ipl_major<<8)|1;
	VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
	error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error)
		return error;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, IPL_STATE, curproc);
	if (error = namei(&nd))
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
	vattr.va_rdev = (ipl_major<<8)|2;
	VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
	error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error)
		return error;
	return 0;
}


#if defined(__FreeBSD_version) && (__FreeBSD_version < 220000)
int xxxinit(lkmtp, cmd, ver)
struct lkm_table *lkmtp;
int cmd, ver;
{
	DISPATCH(lkmtp, cmd, ver, iplaction, iplaction, iplaction);
}
#else
#include <sys/exec.h>
#include <sys/sysent.h>

MOD_DECL(if_ipl);

static struct lkm_dev _module = {
	LM_DEV,
	LKM_VERSION,
	IPL_VERSION,
	CDEV_MAJOR,
	LM_DT_CHAR,
	(void *)&ipl_cdevsw
};

int if_ipl(lkmtp, cmd, ver)
struct lkm_table *lkmtp;
int cmd, ver;
{
	DISPATCH(lkmtp, cmd, ver, iplaction, iplaction, iplaction);
}

/*
static ipl_devsw_installed = 0;

static void ipl_drvinit __P((void *unused))
{
        dev_t dev;

        if( ! ipl_devsw_installed ) {
                dev = makedev(CDEV_MAJOR,0);
                cdevsw_add(&dev, &ipl_cdevsw,NULL);
                ipl_devsw_installed = 1;
        }
}

SYSINIT(ipldev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ipl_drvinit,NULL)
*/
#endif /* __FreeBSD__ */
