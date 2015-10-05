/* $FreeBSD$ */

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/*
 * 29/12/94 Added code from Marc Huber <huber@fzi.de> to allow it to allocate
 * its own major char number! Way cool patch!
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sundev/mbvar.h>
#include <sun/autoconf.h>
#include <sun/vddrv.h>
#if defined(sun4c) || defined(sun4m)
# include <sun/openprom.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <net/if.h>
#include "ipl.h"
#include "ip_compat.h"
#include "ip_fil.h"


#if !defined(lint)
static const char sccsid[] = "@(#)mls_ipl.c	2.6 10/15/95 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id$";
#endif

extern	int	ipfdetach __P((void));
#ifndef	IPFILTER_LOG
#define	ipfread	nulldev
#endif
extern	int	nulldev __P((void));
extern	int	errno;

extern int nodev __P((void));

static	int	unload __P((void));
static	int	ipf_attach __P((void));
int	xxxinit __P((u_int, struct vddrv *, caddr_t, struct vdstat *));
static	char	*ipf_devfiles[] = { IPL_NAME, IPNAT_NAME, IPSTATE_NAME,
				    IPAUTH_NAME, IPSYNC_NAME, IPSCAN_NAME,
				    IPLOOKUP_NAME, NULL };
static	int	ipfopen __P((dev_t, int));
static	int	ipfclose __P((dev_t, int));
static	int	ipfread __P((dev_t, struct uio *));
static	int	ipfwrite __P((dev_t, struct uio *));


struct	cdevsw	ipfdevsw =
{
	ipfopen, ipfclose, ipfread, nulldev,
	ipfioctl, nulldev, nulldev, nulldev,
	0, nulldev,
};


struct	dev_ops	ipf_ops =
{
	1,
	ipfidentify,
	ipfattach,
	ipfopen,
	ipfclose,
	ipfread,
	ipfwrite,
	NULL,		/* strategy */
	NULL,		/* dump */
	0,		/* psize */
        ipfioctl,
	NULL,		/* reset */
	NULL		/* mmap */
};

int	ipf_major = 0;

#ifdef sun4m
struct	vdldrv	vd =
{
	VDMAGIC_PSEUDO,
	IPL_VERSION,
	&ipf_ops,
	NULL,
	&ipfdevsw,
	0,
	0,
	NULL,
	NULL,
	NULL,
	0,
	1,
};
#else /* sun4m */
struct vdldrv vd =
{
	VDMAGIC_PSEUDO,	/* magic */
	IPL_VERSION,
#ifdef sun4c
	&ipf_ops,	/* dev_ops */
#else
	NULL,		/* struct mb_ctlr *mb_ctlr */
	NULL,		/* struct mb_driver *mb_driver */
	NULL,		/* struct mb_device *mb_device */
	0,		/* num ctlrs */
	1,		/* numdevs */
#endif /* sun4c */
	NULL,		/* bdevsw */
	&ipfdevsw,	/* cdevsw */
	0,		/* block major */
	0,		/* char major */
};
#endif /* sun4m */

extern int vd_unuseddev __P((void));
extern struct cdevsw cdevsw[];
extern int nchrdev;

xxxinit(fc, vdp, data, vds)
	u_int	fc;
	struct	vddrv	*vdp;
	caddr_t	data;
	struct	vdstat	*vds;
{
	struct vdioctl_load *vdi = (struct vdioctl_load *)data;

	switch (fc)
	{
	case VDLOAD:
	    {
		struct vdconf *vdc;
		if (vdi && vdi->vdi_userconf)
			for (vdc = vdi->vdi_userconf; vdc->vdc_type; vdc++)
				if (vdc->vdc_type == VDCCHARMAJOR) {
					ipf_major = vdc->vdc_data;
					break;
				}

		if (!ipf_major) {
			while (ipf_major < nchrdev &&
			       cdevsw[ipf_major].d_open != vd_unuseddev)
				ipf_major++;
			if (ipf_major == nchrdev)
				return ENODEV;
		}
		vdp->vdd_vdtab = (struct vdlinkage *)&vd;
		vd.Drv_charmajor = ipf_major;
		return ipf_attach();
	    }
	case VDUNLOAD:
		return unload();
	case VDSTAT:
		return 0;
	default:
		return EIO;
	}
}


static int
unload()
{
	int err = 0, i;
	char *name;

	if (ipf_refcnt != 0)
		err = EBUSY;
	else if (ipf_running >= 0)
		err = ipfdetach();
	if (err)
		return err;

	ipf_running = -2;
	for (i = 0; (name = ipf_devfiles[i]); i++)
		(void) vn_remove(name, UIO_SYSSPACE, FILE);
	printf("%s unloaded\n", ipfilter_version);
	return 0;
}


static int
ipf_attach()
{
	struct vnode *vp;
	struct vattr vattr;
	int error = 0, fmode = S_IFCHR|0600, i;
	char *name;

	error = ipfattach();
	if (error)
		return error;

        for (i = 0; (name = ipf_devfiles[i]); i++) {
		(void) vn_remove(name, UIO_SYSSPACE, FILE);
		vattr_null(&vattr);
		vattr.va_type = MFTOVT(fmode);
		vattr.va_mode = (fmode & 07777);
		vattr.va_rdev = (ipf_major << 8) | i;

		error = vn_create(name, UIO_SYSSPACE, &vattr, EXCL, 0, &vp);
		if (error) {
			printf("IP Filter: vn_create(%s) = %d\n", name, error);
			break;
		} else {
			VN_RELE(vp);
		}
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
static int
ipfopen(dev, flags)
	dev_t dev;
	int flags;
{
	u_int unit = GET_MINOR(dev);
	int error;

	if (IPL_LOGMAX < unit) {
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


static int
ipfclose(dev, flags)
	dev_t dev;
	int flags;
{
	u_int	unit = GET_MINOR(dev);

	if (IPL_LOGMAX < unit)
		unit = ENXIO;
	else
		unit = 0;
	return unit;
}


/*
 * ipfread/ipflog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
static int
ipfread(dev, uio)
	dev_t dev;
	register struct uio *uio;
{

	if (ipf_running < 1) {
		ipfmain.ipf_interror = 130006;
		return EIO;
	}

#ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
#else
	ipfmain.ipf_interror = 130007;
	return ENXIO;
#endif
}


/*
 * ipfwrite
 */
static int
ipfwrite(dev, uio)
	dev_t dev;
	register struct uio *uio;
{

	if (ipf_running < 1) {
		ipfmain.ipf_interror = 130008;
		return EIO;
	}

	if (getminor(dev) == IPL_LOGSYNC)
		return ipfsync_write(uio);
	ipfmain.ipf_interror = 130009;
	return ENXIO;
}
