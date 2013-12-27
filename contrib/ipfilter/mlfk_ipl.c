/* $FreeBSD$ */

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/select.h>
#if __FreeBSD_version >= 500000
# include <sys/selinfo.h>
#endif
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>


#include "netinet/ipl.h"
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_state.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_sync.h"

extern ipf_main_softc_t ipfmain;

#if __FreeBSD_version >= 502116
static struct cdev *ipf_devs[IPL_LOGSIZE];
#else
static dev_t ipf_devs[IPL_LOGSIZE];
#endif

#if 0
static int sysctl_ipf_int ( SYSCTL_HANDLER_ARGS );
#endif
static int ipf_modload(void);
static int ipf_modunload(void);

#if (__FreeBSD_version >= 500024)
# if (__FreeBSD_version >= 502116)
static	int	ipfopen __P((struct cdev*, int, int, struct thread *));
static	int	ipfclose __P((struct cdev*, int, int, struct thread *));
# else
static	int	ipfopen __P((dev_t, int, int, struct thread *));
static	int	ipfclose __P((dev_t, int, int, struct thread *));
# endif /* __FreeBSD_version >= 502116 */
#else
static	int	ipfopen __P((dev_t, int, int, struct proc *));
static	int	ipfclose __P((dev_t, int, int, struct proc *));
#endif
#if (__FreeBSD_version >= 502116)
static	int	ipfread __P((struct cdev*, struct uio *, int));
static	int	ipfwrite __P((struct cdev*, struct uio *, int));
#else
static	int	ipfread __P((dev_t, struct uio *, int));
static	int	ipfwrite __P((dev_t, struct uio *, int));
#endif /* __FreeBSD_version >= 502116 */



SYSCTL_DECL(_net_inet);
#define SYSCTL_IPF(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_INT|access, \
		   ptr, val, sysctl_ipf_int, "I", descr);
#define	CTLFLAG_OFF	0x00800000	/* IPFilter must be disabled */
#define	CTLFLAG_RWO	(CTLFLAG_RW|CTLFLAG_OFF)
SYSCTL_NODE(_net_inet, OID_AUTO, ipf, CTLFLAG_RW, 0, "IPF");
#if 0
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_flags, CTLFLAG_RW, &ipf_flags, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_pass, CTLFLAG_RW, &ipf_pass, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_active, CTLFLAG_RD, &ipf_active, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpidletimeout, CTLFLAG_RWO,
	   &ipf_tcpidletimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcphalfclosed, CTLFLAG_RWO,
	   &ipf_tcphalfclosed, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpclosewait, CTLFLAG_RWO,
	   &ipf_tcpclosewait, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcplastack, CTLFLAG_RWO,
	   &ipf_tcplastack, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcptimeout, CTLFLAG_RWO,
	   &ipf_tcptimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpclosed, CTLFLAG_RWO,
	   &ipf_tcpclosed, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_udptimeout, CTLFLAG_RWO,
	   &ipf_udptimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_udpacktimeout, CTLFLAG_RWO,
	   &ipf_udpacktimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_icmptimeout, CTLFLAG_RWO,
	   &ipf_icmptimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_defnatage, CTLFLAG_RWO,
	   &ipf_nat_defage, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_ipfrttl, CTLFLAG_RW,
	   &ipf_ipfrttl, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_running, CTLFLAG_RD,
	   &ipf_running, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_statesize, CTLFLAG_RWO,
	   &ipf_state_size, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_statemax, CTLFLAG_RWO,
	   &ipf_state_max, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_nattable_sz, CTLFLAG_RWO,
	   &ipf_nat_table_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_natrules_sz, CTLFLAG_RWO,
	   &ipf_nat_maprules_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_rdrrules_sz, CTLFLAG_RWO,
	   &ipf_nat_rdrrules_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_hostmap_sz, CTLFLAG_RWO,
	   &ipf_nat_hostmap_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_authsize, CTLFLAG_RWO,
	   &ipf_auth_size, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_authused, CTLFLAG_RD,
	   &ipf_auth_used, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_defaultauthage, CTLFLAG_RW,
	   &ipf_auth_defaultage, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_chksrc, CTLFLAG_RW, &ipf_chksrc, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_minttl, CTLFLAG_RW, &ipf_minttl, 0, "");
#endif

#define CDEV_MAJOR 79
#include <sys/poll.h>
#if __FreeBSD_version >= 500043
# include <sys/select.h>
static int ipfpoll(struct cdev *dev, int events, struct thread *td);

static struct cdevsw ipf_cdevsw = {
#if __FreeBSD_version >= 502103
	.d_version =	D_VERSION,
	.d_flags =	0,	/* D_NEEDGIANT - Should be SMP safe */
#endif
	.d_open =	ipfopen,
	.d_close =	ipfclose,
	.d_read =	ipfread,
	.d_write =	ipfwrite,
	.d_ioctl =	ipfioctl,
	.d_poll =	ipfpoll,
	.d_name =	"ipf",
#if __FreeBSD_version < 600000
	.d_maj =	CDEV_MAJOR,
#endif
};
#else
static int ipfpoll(dev_t dev, int events, struct proc *td);

static struct cdevsw ipf_cdevsw = {
	/* open */	ipfopen,
	/* close */	ipfclose,
	/* read */	ipfread,
	/* write */	ipfwrite,
	/* ioctl */	ipfioctl,
	/* poll */	ipfpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ipf",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
# if (__FreeBSD_version < 500043)
	/* bmaj */	-1,
# endif
# if (__FreeBSD_version >= 430000)
	/* kqfilter */	NULL
# endif
};
#endif

static char *ipf_devfiles[] = {	IPL_NAME, IPNAT_NAME, IPSTATE_NAME, IPAUTH_NAME,
				IPSYNC_NAME, IPSCAN_NAME, IPLOOKUP_NAME, NULL };


static int
ipfilter_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type)
	{
	case MOD_LOAD :
		error = ipf_modload();
		break;

	case MOD_UNLOAD :
		error = ipf_modunload();
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}


static int
ipf_modload()
{
	char *defpass, *c, *str;
	int i, j, error;

	if (ipf_load_all() != 0)
		return EIO;

	if (ipf_create_all(&ipfmain) == NULL)
		return EIO;

	error = ipfattach(&ipfmain);
	if (error)
		return error;

	for (i = 0; i < IPL_LOGSIZE; i++)
		ipf_devs[i] = NULL;

	for (i = 0; (str = ipf_devfiles[i]); i++) {
		c = NULL;
		for(j = strlen(str); j > 0; j--)
			if (str[j] == '/') {
				c = str + j + 1;
				break;
			}
		if (!c)
			c = str;
		ipf_devs[i] = make_dev(&ipf_cdevsw, i, 0, 0, 0600, c);
	}

	error = ipf_pfil_hook();
	if (error != 0)
		return error;
	ipf_event_reg();

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
	return 0;
}


static int
ipf_modunload()
{
	int error, i;

	if (ipfmain.ipf_refcnt)
		return EBUSY;

	error = ipf_pfil_unhook();
	if (error != 0)
		return error;

	if (ipfmain.ipf_running >= 0) {
		error = ipfdetach(&ipfmain);
		if (error != 0)
			return error;

		ipf_destroy_all(&ipfmain);
		ipf_unload_all();
	} else
		error = 0;

	ipfmain.ipf_running = -2;

	for (i = 0; ipf_devfiles[i]; i++) {
		if (ipf_devs[i] != NULL)
			destroy_dev(ipf_devs[i]);
	}

	printf("%s unloaded\n", ipfilter_version);

	return error;
}


static moduledata_t ipfiltermod = {
	"ipfilter",
	ipfilter_modevent,
	0
};


DECLARE_MODULE(ipfilter, ipfiltermod, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
#ifdef	MODULE_VERSION
MODULE_VERSION(ipfilter, 1);
#endif


#if 0
#ifdef SYSCTL_IPF
int
sysctl_ipf_int ( SYSCTL_HANDLER_ARGS )
{
	int error = 0;

	if (arg1)
		error = SYSCTL_OUT(req, arg1, sizeof(int));
	else
		error = SYSCTL_OUT(req, &arg2, sizeof(int));

	if (error || !req->newptr)
		return (error);

	if (!arg1)
		error = EPERM;
	else {
		if ((oidp->oid_kind & CTLFLAG_OFF) && (ipfmain.ipf_running > 0))
			error = EBUSY;
		else
			error = SYSCTL_IN(req, arg1, sizeof(int));
	}
	return (error);
}
#endif
#endif


static int
#if __FreeBSD_version >= 500043
ipfpoll(struct cdev *dev, int events, struct thread *td)
#else
ipfpoll(dev_t dev, int events, struct proc *td)
#endif
{
	u_int unit = GET_MINOR(dev);
	int revents;

	if (unit < 0 || unit > IPL_LOGMAX)
		return 0;

	revents = 0;

	switch (unit)
	{
	case IPL_LOGIPF :
	case IPL_LOGNAT :
	case IPL_LOGSTATE :
#ifdef IPFILTER_LOG
		if ((events & (POLLIN | POLLRDNORM)) && ipf_log_canread(&ipfmain, unit))
			revents |= events & (POLLIN | POLLRDNORM);
#endif
		break;
	case IPL_LOGAUTH :
		if ((events & (POLLIN | POLLRDNORM)) && ipf_auth_waiting(&ipfmain))
			revents |= events & (POLLIN | POLLRDNORM);
		break;
	case IPL_LOGSYNC :
		if ((events & (POLLIN | POLLRDNORM)) && ipf_sync_canread(&ipfmain))
			revents |= events & (POLLIN | POLLRDNORM);
		if ((events & (POLLOUT | POLLWRNORM)) && ipf_sync_canwrite(&ipfmain))
			revents |= events & (POLLOUT | POLLWRNORM);
		break;
	case IPL_LOGSCAN :
	case IPL_LOGLOOKUP :
	default :
		break;
	}

	if ((revents == 0) && ((events & (POLLIN|POLLRDNORM)) != 0))
		selrecord(td, &ipfmain.ipf_selwait[unit]);

	return revents;
}


/*
 * routines below for saving IP headers to buffer
 */
static int ipfopen(dev, flags
#if ((BSD >= 199506) || (__FreeBSD_version >= 220000))
, devtype, p)
	int devtype;
# if (__FreeBSD_version >= 500024)
	struct thread *p;
# else
	struct proc *p;
# endif /* __FreeBSD_version >= 500024 */
#else
)
#endif
#if (__FreeBSD_version >= 502116)
	struct cdev *dev;
#else
	dev_t dev;
#endif
	int flags;
{
	u_int unit = GET_MINOR(dev);
	int error;

	if (IPL_LOGMAX < unit)
		error = ENXIO;
	else {
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


static int ipfclose(dev, flags
#if ((BSD >= 199506) || (__FreeBSD_version >= 220000))
, devtype, p)
	int devtype;
# if (__FreeBSD_version >= 500024)
	struct thread *p;
# else
	struct proc *p;
# endif /* __FreeBSD_version >= 500024 */
#else
)
#endif
#if (__FreeBSD_version >= 502116)
	struct cdev *dev;
#else
	dev_t dev;
#endif
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
#if (BSD >= 199306)
static int ipfread(dev, uio, ioflag)
	int ioflag;
#else
static int ipfread(dev, uio)
#endif
#if (__FreeBSD_version >= 502116)
	struct cdev *dev;
#else
	dev_t dev;
#endif
	struct uio *uio;
{
	u_int	unit = GET_MINOR(dev);

	if (unit < 0)
		return ENXIO;

	if (ipfmain.ipf_running < 1)
		return EIO;

	if (unit == IPL_LOGSYNC)
		return ipf_sync_read(&ipfmain, uio);

#ifdef IPFILTER_LOG
	return ipf_log_read(&ipfmain, unit, uio);
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
#if (BSD >= 199306)
static int ipfwrite(dev, uio, ioflag)
	int ioflag;
#else
static int ipfwrite(dev, uio)
#endif
#if (__FreeBSD_version >= 502116)
	struct cdev *dev;
#else
	dev_t dev;
#endif
	struct uio *uio;
{

	if (ipfmain.ipf_running < 1)
		return EIO;

	if (GET_MINOR(dev) == IPL_LOGSYNC)
		return ipf_sync_write(&ipfmain, uio);
	return ENXIO;
}
