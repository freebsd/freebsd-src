/* $FreeBSD$ */

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
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
# define NETBSD_PF
#endif

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
#include <sys/poll.h>
#include <sys/select.h>
#include "ipl.h"
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_auth.h"
#include "ip_state.h"
#include "ip_nat.h"
#include "ip_sync.h"

#if !defined(__NetBSD_Version__) || __NetBSD_Version__ < 103050000
#define vn_lock(v,f) VOP_LOCK(v)
#endif

#if !defined(VOP_LEASE) && defined(LEASE_CHECK)
#define	VOP_LEASE	LEASE_CHECK
#endif


extern	int	lkmenodev __P((void));

#if NetBSD >= 199706
int	ipflkm_lkmentry __P((struct lkm_table *, int, int));
#else
int	xxxinit __P((struct lkm_table *, int, int));
#endif
static	int	ipf_unload __P((void));
static	int	ipf_load __P((void));
static	int	ipf_remove __P((void));
static	int	ipfaction __P((struct lkm_table *, int));
static	char	*ipf_devfiles[] = { IPL_NAME, IPNAT_NAME, IPSTATE_NAME,
				    IPAUTH_NAME, IPSYNC_NAME, IPSCAN_NAME,
				    IPLOOKUP_NAME, NULL };

int				ipf_major = 0;
extern	ipf_main_softc_t	ipfmain;
extern	const struct cdevsw ipl_cdevsw;

#if defined(__NetBSD__) && (__NetBSD_Version__ >= 106080000)
MOD_DEV(IPL_VERSION, "ipf", NULL, -1, &ipl_cdevsw, -1);
#else
MOD_DEV(IPL_VERSION, LM_DT_CHAR, -1, &ipldevsw);
#endif

extern int vd_unuseddev __P((void));
extern struct cdevsw cdevsw[];
extern int nchrdev;


int
#if NetBSD >= 199706
ipflkm_lkmentry(lkmtp, cmd, ver)
#else
xxxinit(lkmtp, cmd, ver)
#endif
	struct lkm_table *lkmtp;
	int cmd, ver;
{
	DISPATCH(lkmtp, cmd, ver, ipfaction, ipfaction, ipfaction);
}


static int
ipfaction(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
#if !defined(__NetBSD__) || (__NetBSD_Version__ < 106080000)
	int i;
#endif
	struct lkm_dev *args = lkmtp->private.lkm_dev;
	int err = 0;

	switch (cmd)
	{
	case LKM_E_LOAD :
		if (lkmexists(lkmtp))
			return EEXIST;

#if defined(__NetBSD__) && (__NetBSD_Version__ >= 106080000)
# if (__NetBSD_Version__ < 200000000)
		err = devsw_attach(args->lkm_devname,
				   args->lkm_bdev, &args->lkm_bdevmaj,
				   args->lkm_cdev, &args->lkm_cdevmaj);
		if (err != 0)
			return (err);
# endif
		ipf_major = args->lkm_cdevmaj;
#else
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
#endif
		printf("IP Filter: loaded into slot %d\n", ipf_major);
		return ipf_load();
	case LKM_E_UNLOAD :
#if defined(__NetBSD__) && (__NetBSD_Version__ >= 106080000)
		devsw_detach(args->lkm_bdev, args->lkm_cdev);
		args->lkm_bdevmaj = -1;
		args->lkm_cdevmaj = -1;
#endif
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


static int
ipf_remove()
{
	char *name;
	struct nameidata nd;
	int error, i;

        for (i = 0; (name = ipf_devfiles[i]); i++) {
#if (__NetBSD_Version__ > 106009999)
# if (__NetBSD_Version__ > 399001400)
#  if (__NetBSD_Version__ > 499001400)
		NDINIT(&nd, DELETE, LOCKPARENT|LOCKLEAF, UIO_SYSSPACE,
		       name);
#  else
		NDINIT(&nd, DELETE, LOCKPARENT|LOCKLEAF, UIO_SYSSPACE,
		       name, curlwp);
#  endif
# else
		NDINIT(&nd, DELETE, LOCKPARENT|LOCKLEAF, UIO_SYSSPACE,
		       name, curproc);
# endif
#else
		NDINIT(&nd, DELETE, LOCKPARENT, UIO_SYSSPACE, name, curproc);
#endif
		if ((error = namei(&nd)))
			return (error);
#if (__NetBSD_Version__ > 399001400)
# if (__NetBSD_Version__ > 399002000)
#  if (__NetBSD_Version__ < 499001400)
		VOP_LEASE(nd.ni_dvp, curlwp, curlwp->l_cred, LEASE_WRITE);
#  endif
# else
		VOP_LEASE(nd.ni_dvp, curlwp, curlwp->l_proc->p_ucred, LEASE_WRITE);
# endif
#else
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
#endif
#if !defined(__NetBSD_Version__) || (__NetBSD_Version__ < 106000000)
		vn_lock(nd.ni_vp, LK_EXCLUSIVE | LK_RETRY);
#endif
#if (__NetBSD_Version__ >= 399002000)
# if (__NetBSD_Version__ < 499001400)
		VOP_LEASE(nd.ni_vp, curlwp, curlwp->l_cred, LEASE_WRITE);
# endif
#else
# if (__NetBSD_Version__ > 399001400)
		VOP_LEASE(nd.ni_vp, curlwp, curlwp->l_proc->p_ucred, LEASE_WRITE);
# else
		VOP_LEASE(nd.ni_vp, curproc, curproc->p_ucred, LEASE_WRITE);
# endif
#endif
		(void) VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	}
	return 0;
}


static int
ipf_unload()
{
	int error = 0;

	/*
	 * Unloading - remove the filter rule check from the IP
	 * input/output stream.
	 */
	if (ipfmain.ipf_refcnt)
		error = EBUSY;
	else if (ipfmain.ipf_running >= 0) {
		error = ipfdetach(&ipfmain);
		if (error == 0) {
			ipf_destroy_all(&ipfmain);
			ipf_unload_all();
		}
	}

	if (error == 0) {
		ipfmain.ipf_running = -2;
		error = ipf_remove();
		printf("%s unloaded\n", ipfilter_version);
	}
	return error;
}


static int
ipf_load()
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

	bzero((char *)&ipfmain, sizeof(ipfmain));
        error = ipf_load_all();
	if (error != 0)
		return error;
	if (ipf_create_all(&ipfmain) == NULL) {
		ipf_unload_all();
		return EIO;
	}

	error = ipfattach(&ipfmain);
	if (error != 0) {
		(void) ipf_unload();
		return error;
	}

	for (i = 0; (error == 0) && (name = ipf_devfiles[i]); i++) {
#if (__NetBSD_Version__ > 399001400)
# if (__NetBSD_Version__ > 499001400)
		NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, name);
# else
		NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, name, curlwp);
# endif
#else
		NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, name, curproc);
#endif
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
#if (__NetBSD_Version__ > 399001400)
# if (__NetBSD_Version__ >= 399002000)
#  if (__NetBSD_Version__ < 499001400)
		VOP_LEASE(nd.ni_dvp, curlwp, curlwp->l_cred, LEASE_WRITE);
#  endif
# else
		VOP_LEASE(nd.ni_dvp, curlwp, curlwp->l_proc->p_ucred, LEASE_WRITE);
# endif
#else
		VOP_LEASE(nd.ni_dvp, curproc, curproc->p_ucred, LEASE_WRITE);
#endif
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
		if (error == 0)
			vput(nd.ni_vp);
	}

	if (error == 0) {
		char *defpass;

		if (FR_ISPASS(ipfmain.ipf_pass))
			defpass = "pass";
		else if (FR_ISBLOCK(ipfmain.ipf_pass))
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
		ipfmain.ipf_running = 1;
	}
	return error;
}
