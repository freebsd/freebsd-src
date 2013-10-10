/* $FreeBSD$ */

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
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
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <sys/lkm.h>
#include "ipl.h"
#include "ip_compat.h"
#include "ip_fil.h"

#define vn_lock(v,f) VOP_LOCK(v)

#if !defined(VOP_LEASE) && defined(LEASE_CHECK)
#define	VOP_LEASE	LEASE_CHECK
#endif


extern	int	lkmenodev __P((void));

#if OpenBSD >= 200311
int	if_ipf_lkmentry __P((struct lkm_table *, int, int));
#else
int	if_ipf __P((struct lkm_table *, int, int));
#endif
static	int	ipf_unload __P((void));
static	int	ipf_load __P((void));
static	int	ipf_remove __P((void));
static	int	ipfaction __P((struct lkm_table *, int));
static	char	*ipf_devfiles[] = { IPL_NAME, IPNAT_NAME, IPSTATE_NAME,
				    IPAUTH_NAME, IPSYNC_NAME, IPSCAN_NAME,
				    IPLOOKUP_NAME, NULL };


struct	cdevsw	ipfdevsw =
{
	ipfopen,		/* open */
	ipfclose,		/* close */
	ipfread,		/* read */
	(void *)nullop,		/* write */
	ipfioctl,		/* ioctl */
	(void *)nullop,		/* stop */
	(void *)NULL,		/* tty */
	(void *)nullop,		/* select */
	(void *)nullop,		/* mmap */
	NULL			/* strategy */
};

int	ipf_major = 0;

MOD_DEV(IPL_VERSION, LM_DT_CHAR, -1, &ipfdevsw);

extern int vd_unuseddev __P((void));
extern struct cdevsw cdevsw[];
extern int nchrdev;


#if OpenBSD >= 200311
int if_ipf_lkmentry (lkmtp, cmd, ver)
#else
int if_ipf(lkmtp, cmd, ver)
#endif
	struct lkm_table *lkmtp;
	int cmd, ver;
{
	DISPATCH(lkmtp, cmd, ver, ipfaction, ipfaction, ipfaction);
}

int lkmexists __P((struct lkm_table *)); /* defined in /sys/kern/kern_lkm.c */

static int ipfaction(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	int i;
	struct lkm_dev *args = lkmtp->private.lkm_dev;
	int err = 0;

	switch (cmd)
	{
	case LKM_E_LOAD :
		if (lkmexists(lkmtp))
			return EEXIST;

		for (i = 0; i < nchrdev; i++)
			if (cdevsw[i].d_open == (dev_type_open((*)))lkmenodev ||
			    cdevsw[i].d_open == ipfopen)
				break;
		if (i == nchrdev) {
			printf("IP Filter: No free cdevsw slots\n");
			return ENODEV;
		}

		ipf_major = i;
		args->lkm_offset = i;   /* slot in cdevsw[] */
		printf("IP Filter: loaded into slot %d\n", ipf_major);
		return ipf_load();
	case LKM_E_UNLOAD :
		err = ipf_unload();
		if (!err)
			printf("IP Filter: unloaded from slot %d\n",
			       ipf_major);
		break;
	case LKM_E_STAT :
		break;
	default:
		err = EIO;
		break;
	}
	return err;
}


static int ipf_remove()
{
	struct nameidata nd;
	int error, i;
	char *name;

        for (i = 0; (name = ipf_devfiles[i]); i++) {
#if OpenBSD >= 200311
		NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_SYSSPACE,
		       name, curproc);
#else
		NDINIT(&nd, DELETE, LOCKPARENT, UIO_SYSSPACE, name, curproc);
#endif
		if ((error = namei(&nd)))
			return (error);
		VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
#if OpenBSD < 200311
		VOP_LOCK(nd.ni_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
#else
		(void)uvm_vnp_uncache(nd.ni_vp);

		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
		VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
#endif
		(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	}
	return 0;
}


static int ipf_unload()
{
	int error = 0;

	/*
	 * Unloading - remove the filter rule check from the IP
	 * input/output stream.
	 */
        if (ipf_refcnt)
                error = EBUSY;
	else if (ipf_running >= 0)
		error = ipfdetach();

	if (error == 0) {
		ipf_running = -2;
		error = ipf_remove();
		printf("%s unloaded\n", ipfilter_version);
	}
	return error;
}


static int ipf_load()
{
	struct nameidata nd;
	struct vattr vattr;
	int error = 0, fmode = S_IFCHR|0600, i;
	char *name;

	/*
	 * XXX Remove existing device nodes prior to creating new ones
	 * XXX using the assigned LKM device slot's major number.  In a
	 * XXX perfect world we could use the ones specified by cdevsw[].
	 */
	(void)ipf_remove();

	error = ipfattach();

	for (i = 0; (error == 0) && (name = ipf_devfiles[i]); i++) {
		NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, name, curproc);
		if ((error = namei(&nd)))
			break;
		if (nd.ni_vp != NULL) {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			if (nd.ni_dvp == nd.ni_vp)
				vrele(nd.ni_dvp);
			else
				vput(nd.ni_dvp);
			vrele(nd.ni_vp);
			error = EEXIST;
			break;
		}
		VATTR_NULL(&vattr);
		vattr.va_type = VCHR;
		vattr.va_mode = (fmode & 07777);
		vattr.va_rdev = (ipf_major << 8) | i;
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	}

	if (error == 0) {
		char *defpass;

		if (FR_ISPASS(ipf_pass))
			defpass = "pass";
		else if (FR_ISBLOCK(ipf_pass))
			defpass = "block";
		else
			defpass = "no-match -> block";

		printf("%s initialized.  Default = %s all, Logging = %s%s\n",
			ipfilter_version, defpass,
#ifdef IPFILTER_LOG
			"enabled",
#else
			"disabled",
#endif
#ifdef IPFILTER_COMPILED
			" (COMPILED)"
#else
			""
#endif
			);
		ipf_running = 1;
	}
	return error;
}


/*
 * routines below for saving IP headers to buffer
 */
int
ipfopen(dev, flags, devtype, p)
	dev_t dev;
	int flags;
	int devtype;
	struct proc *p;
{
	u_int min = GET_MINOR(dev);
	int error;

	if (IPL_LOGMAX < min) {
		error = ENXIO;
	} else {
		switch (unit)
		{
		case IPL_LOGIPF :
		case IPL_LOGNAT :
		case IPL_LOGSTATE :
		case IPL_LOGAUTH :
		case IPL_LOGLOOKUP :
		case IPL_LOGSYNC :
#ifdef IPFILTER_SCAN
		case IPL_LOGSCAN :
#endif
			error = 0;
			break;
		default :
			error = ENXIO;
			break;
		}
	}
	return error;
}


int
ipfclose(dev, flags, devtype, p)
	dev_t dev;
	int flags;
	int devtype;
	struct proc *p;
{
	u_int   min = GET_MINOR(dev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}


/*
 * ipfread/ipflog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int
ipfread(dev, uio, ioflag)
	dev_t dev;
	register struct uio *uio;
	int ioflag;
{

	if (ipf_running < 1)
		return EIO;

	if (GET_MINOR(dev) == IPL_LOGSYNC)
		return ipfsync_read(uio);

#ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
#else
	return ENXIO;
#endif
}


/*
 * ipfwrite
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int
#if (BSD >= 199306)
ipfwrite(dev, uio, ioflag)
	int ioflag;
#else
ipfwrite(dev, uio)
#endif
	dev_t dev;
	register struct uio *uio;
{

	if (ipf_running < 1)
		return EIO;

	if (GET_MINOR(dev) == IPL_LOGSYNC)
		return ipfsync_write(uio);
	return ENXIO;
}
