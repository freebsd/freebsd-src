/*
 * Copyright (C) 1993-2001 by Darren Reed.
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
static const char rcsid[] = "@(#)$Id: mls_ipl.c,v 2.2.2.1 2001/06/26 10:43:20 darrenr Exp $";
#endif

extern	int	ipldetach __P((void));
#ifndef	IPFILTER_LOG
#define	iplread	nulldev
#endif
extern	int	nulldev __P((void));
extern	int	errno;

extern int nodev __P((void));

static	int	unload __P((void));
static	int	ipl_attach __P((void));
int	xxxinit __P((u_int, struct vddrv *, caddr_t, struct vdstat *));
static	char	*ipf_devfiles[] = { IPL_NAME, IPL_NAT, IPL_STATE, IPL_AUTH,
				    NULL };


struct	cdevsw	ipldevsw = 
{
	iplopen, iplclose, iplread, nulldev,
	iplioctl, nulldev, nulldev, nulldev,
	0, nulldev,
};


struct	dev_ops	ipl_ops = 
{
	1,
	iplidentify,
	iplattach,
	iplopen,
	iplclose,
	iplread,
	NULL,		/* write */
	NULL,		/* strategy */
	NULL,		/* dump */
	0,		/* psize */
        iplioctl,
	NULL,		/* reset */
	NULL		/* mmap */
};

int	ipl_major = 0;

#ifdef sun4m
struct	vdldrv	vd = 
{
	VDMAGIC_PSEUDO,
	IPL_VERSION,
	&ipl_ops,
	NULL,
	&ipldevsw,
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
	&ipl_ops,	/* dev_ops */
#else
	NULL,		/* struct mb_ctlr *mb_ctlr */
	NULL,		/* struct mb_driver *mb_driver */
	NULL,		/* struct mb_device *mb_device */
	0,		/* num ctlrs */
	1,		/* numdevs */
#endif /* sun4c */
	NULL,		/* bdevsw */
	&ipldevsw,	/* cdevsw */
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
					ipl_major = vdc->vdc_data;
					break;
				}

		if (!ipl_major) {
			while (ipl_major < nchrdev &&
			       cdevsw[ipl_major].d_open != vd_unuseddev)
				ipl_major++;
			if (ipl_major == nchrdev)
				return ENODEV;
		}
		vdp->vdd_vdtab = (struct vdlinkage *)&vd;
		vd.Drv_charmajor = ipl_major;
		return ipl_attach();
	    }
	case VDUNLOAD:
		return unload();
	case VDSTAT:
		return 0;
	default:
		return EIO;
	}
}


static	int	unload()
{
	char *name;
	int err, i;

	err = ipldetach();
	if (err)
		return err;
	for (i = 0; (name = ipf_devfiles[i]); i++)
		(void) vn_remove(name, UIO_SYSSPACE, FILE);
	return 0;
}


static	int	ipl_attach()
{
	struct vnode *vp;
	struct vattr vattr;
	int error = 0, fmode = S_IFCHR|0600, i;
	char *name;

	error = iplattach();
	if (error)
		return error;

        for (i = 0; (name = ipf_devfiles[i]); i++) {
		(void) vn_remove(name, UIO_SYSSPACE, FILE);
		vattr_null(&vattr);
		vattr.va_type = MFTOVT(fmode);
		vattr.va_mode = (fmode & 07777);
		vattr.va_rdev = (ipl_major << 8) | i;

		error = vn_create(name, UIO_SYSSPACE, &vattr, EXCL, 0, &vp);
		if (error) {
			printf("IP Filter: vn_create(%s) = %d\n", name, error);
			break;
		} else {
			VN_RELE(vp);
		}
	}
	return error;
}
