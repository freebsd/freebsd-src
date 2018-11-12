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
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/buf.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sundev/mbvar.h>
#include <sun/autoconf.h>
#include <sun/vddrv.h>
#if defined(sun4c) || defined(sun4m)
#include <sun/openprom.h>
#endif

#ifndef	IPL_NAME
#define	IPL_NAME	"/dev/ipf"
#endif

extern	int	ipfattach(), ipfopen(), ipfclose(), ipfioctl(), ipfread();
extern	int	nulldev(), ipfidentify(), errno;

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
	NULL,		/* write */
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
	"ipf",
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
	"ipf",		/* name */
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

extern int vd_unuseddev();
extern struct cdevsw cdevsw[];
extern int nchrdev;

xxxinit(fc, vdp, vdi, vds)
	u_int	fc;
	struct	vddrv	*vdp;
	caddr_t	vdi;
	struct	vdstat	*vds;
{
	struct	vdlinkage *v;
	int	i;

	switch (fc)
	{
	case VDLOAD:
		while (ipf_major < nchrdev &&
		       cdevsw[ipf_major].d_open != vd_unuseddev)
			ipf_major++;
		if (ipf_major == nchrdev)
			return ENODEV;
		vd.Drv_charmajor = ipf_major;
		vdp->vdd_vdtab = (struct vdlinkage *)&vd;
		return ipf_attach(vdi);
	case VDUNLOAD:
		return unload(vdp, vdi);

	case VDSTAT:
		return 0;

	default:
		return EIO;
	}
}

static unload(vdp, vdi)
	struct vddrv *vdp;
	struct vdioctl_unload  *vdi;
{
	int	i;

	(void) vn_remove(IPL_NAME, UIO_SYSSPACE, FILE);
	return ipfdetach();
}


static	int	ipf_attach(vdi)
struct	vdioctl_load	*vdi;
{
	struct	vnode	*vp;
	struct	vattr	vattr;
	int		error = 0, fmode = S_IFCHR|0600;

	(void) vn_remove(IPL_NAME, UIO_SYSSPACE, FILE);
	vattr_null(&vattr);
	vattr.va_type = MFTOVT(fmode);
	vattr.va_mode = (fmode & 07777);
	vattr.va_rdev = ipf_major<<8;

	error = vn_create(IPL_NAME, UIO_SYSSPACE, &vattr, EXCL, 0, &vp);
	if (error == 0)
		VN_RELE(vp);
	return ipfattach(0);
}
