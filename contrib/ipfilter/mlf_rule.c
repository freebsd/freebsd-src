/*	$NetBSD$	*/

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

#if defined(__FreeBSD__) && (__FreeBSD__ > 1)
# ifdef	IPFILTER_LKM
#  include <osreldate.h>
#  define	ACTUALLY_LKM_NOT_KERNEL
# else
#  include <sys/osreldate.h>
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
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
# include <sys/lock.h>
#endif
#include <sys/stat.h>
#include <sys/proc.h>
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
#if (__FreeBSD_version >= 199511)
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#endif
#if (__FreeBSD__ > 1)
# include <sys/sysent.h>
#endif
#include <sys/lkm.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_rules.h"


int	xxxinit __P((struct lkm_table *, int, int));

#if !defined(__FreeBSD_version) || (__FreeBSD_version < 220000)
MOD_DEV(IPL_VERSION, LM_DT_CHAR, -1, &ipldevsw);
#endif

static int ipfrule_ioctl __P((struct lkm_table *, int));

#if defined(__FreeBSD_version) && (__FreeBSD_version < 220000)

int xxxinit(lkmtp, cmd, ver)
struct lkm_table *lkmtp;
int cmd, ver;
{
	DISPATCH(lkmtp, cmd, ver, ipfrule_ioctl, ipfrule_ioctl, ipfrule_ioctl);
}
#else	/* __FREEBSD_version >= 220000 */
# ifdef	IPFILTER_LKM
#  include <sys/exec.h>

#  if (__FreeBSD_version >= 300000)
MOD_MISC(ipfrule);
#  else
MOD_DECL(ipfrule);


static struct lkm_misc _module = {
	LM_MISC,
	LKM_VERSION,
	"IP Filter rules",
	0,
};
#  endif


int ipfrule __P((struct lkm_table *, int, int));


int ipfrule(lkmtp, cmd, ver)
struct lkm_table *lkmtp;
int cmd, ver;
{
#  if (__FreeBSD_version >= 300000)
	MOD_DISPATCH(ipfrule, lkmtp, cmd, ver, ipfrule_ioctl, ipfrule_ioctl,
		     ipfrule_ioctl);
#  else
	DISPATCH(lkmtp, cmd, ver, ipfrule_ioctl, ipfrule_ioctl, ipfrule_ioctl);
#  endif
}
# endif /* IPFILTER_LKM */


int ipfrule_load(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
	return ipfrule_add();
}


int ipfrule_unload(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
	return ipfrule_remove();
}


static int ipfrule_ioctl(lkmtp, cmd)
struct lkm_table *lkmtp;
int cmd;
{
	int err = 0;

	switch (cmd)
	{
	case LKM_E_LOAD :
		if (lkmexists(lkmtp))
			return EEXIST;

		err = ipfrule_load(lkmtp, cmd);
		if (!err)
			fr_refcnt++;
		break;
	case LKM_E_UNLOAD :
		err = ipfrule_unload(lkmtp, cmd);
		if (!err)
			fr_refcnt--;
		break;
	case LKM_E_STAT :
		break;
	default:
		err = EIO;
		break;
	}
	return err;
}
#endif /* _FreeBSD_version */
