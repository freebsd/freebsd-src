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
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_rules.h"


extern	int	errno;


int	xxxinit __P((u_int, struct vddrv *, caddr_t, struct vdstat *));

int	ipl_major = 0;

#ifdef sun4m
struct	vdldrv	vd =
{
	VDMAGIC_USER,
	"IP Filter rules",
	NULL,
	NULL,
	NULL,
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
	VDMAGIC_USER,	/* magic */
	"IP Filter rules",
#ifdef sun4c
	NULL,	/* dev_ops */
#else
	NULL,		/* struct mb_ctlr *mb_ctlr */
	NULL,		/* struct mb_driver *mb_driver */
	NULL,		/* struct mb_device *mb_device */
	0,		/* num ctlrs */
	1,		/* numdevs */
#endif /* sun4c */
	NULL,		/* bdevsw */
	NULL,		/* cdevsw */
	0,		/* block major */
	0,		/* char major */
};
#endif /* sun4m */


xxxinit(fc, vdp, data, vds)
	u_int	fc;
	struct	vddrv	*vdp;
	caddr_t	data;
	struct	vdstat	*vds;
{
	struct vdioctl_load *vdi = (struct vdioctl_load *)data;
	int err;

	switch (fc)
	{
	case VDLOAD:
		err = ipfrule_add();
		if (!err)
			ipf_refcnt++;
		break;
	case VDUNLOAD:
		err = ipfrule_remove();
		if (!err)
			ipf_refcnt--;
		break;
	case VDSTAT:
		err = 0;
		break;
	default:
		err = EIO;
		break;
	}
}
