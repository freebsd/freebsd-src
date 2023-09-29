
/*
 * Copyright (C) 2012 by Darren Reed.
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define	KERNEL  1
# define	_KERNEL 1
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/select.h>
#ifdef __FreeBSD__
# include <sys/selinfo.h>
# include <sys/jail.h>
# ifdef _KERNEL
#  include <net/vnet.h>
# else
#  define CURVNET_SET(arg)
#  define CURVNET_RESTORE()
#  define	VNET_DEFINE(_t, _v)	_t _v
#  define	VNET_DECLARE(_t, _v)	extern _t _v
#  define	VNET(arg)	arg
# endif
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

VNET_DECLARE(ipf_main_softc_t, ipfmain);
#define	V_ipfmain		VNET(ipfmain)

#ifdef __FreeBSD__
static struct cdev *ipf_devs[IPL_LOGSIZE];
#else
static dev_t ipf_devs[IPL_LOGSIZE];
#endif

static int sysctl_ipf_int ( SYSCTL_HANDLER_ARGS );
static int sysctl_ipf_int_nat ( SYSCTL_HANDLER_ARGS );
static int sysctl_ipf_int_state ( SYSCTL_HANDLER_ARGS );
static int sysctl_ipf_int_auth ( SYSCTL_HANDLER_ARGS );
static int sysctl_ipf_int_frag ( SYSCTL_HANDLER_ARGS );
static int ipf_modload(void);
static int ipf_modunload(void);
static int ipf_fbsd_sysctl_create(void);
static int ipf_fbsd_sysctl_destroy(void);

#ifdef __FreeBSD__
static	int	ipfopen(struct cdev*, int, int, struct thread *);
static	int	ipfclose(struct cdev*, int, int, struct thread *);
static	int	ipfread(struct cdev*, struct uio *, int);
static	int	ipfwrite(struct cdev*, struct uio *, int);
#else
static	int	ipfopen(dev_t, int, int, struct proc *);
static	int	ipfclose(dev_t, int, int, struct proc *);
static	int	ipfread(dev_t, struct uio *, int);
static	int	ipfwrite(dev_t, struct uio *, int);
#endif

#ifdef LARGE_NAT
#define IPF_LARGE_NAT	1
#else
#define IPF_LARGE_NAT	0
#endif

SYSCTL_DECL(_net_inet);
#define SYSCTL_IPF(parent, nbr, name, access, ptr, val, descr) \
    SYSCTL_OID(parent, nbr, name, \
	CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_MPSAFE | access, \
	ptr, val, sysctl_ipf_int, "I", descr)
#define SYSCTL_DYN_IPF_NAT(parent, nbr, name, access,ptr, val, descr) \
    SYSCTL_ADD_OID(&ipf_clist, SYSCTL_STATIC_CHILDREN(parent), nbr, name, \
	CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_MPSAFE |access, \
	ptr, val, sysctl_ipf_int_nat, "I", descr)
#define SYSCTL_DYN_IPF_STATE(parent, nbr, name, access,ptr, val, descr) \
    SYSCTL_ADD_OID(&ipf_clist, SYSCTL_STATIC_CHILDREN(parent), nbr, name, \
	CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_MPSAFE | access, \
	ptr, val, sysctl_ipf_int_state, "I", descr)
#define SYSCTL_DYN_IPF_FRAG(parent, nbr, name, access,ptr, val, descr) \
    SYSCTL_ADD_OID(&ipf_clist, SYSCTL_STATIC_CHILDREN(parent), nbr, name, \
	CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_MPSAFE | access, \
	ptr, val, sysctl_ipf_int_frag, "I", descr)
#define SYSCTL_DYN_IPF_AUTH(parent, nbr, name, access,ptr, val, descr) \
    SYSCTL_ADD_OID(&ipf_clist, SYSCTL_STATIC_CHILDREN(parent), nbr, name, \
	CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_MPSAFE | access, \
	ptr, val, sysctl_ipf_int_auth, "I", descr)
static struct sysctl_ctx_list ipf_clist;
#define	CTLFLAG_OFF	0x00800000	/* IPFilter must be disabled */
#define	CTLFLAG_RWO	(CTLFLAG_RW|CTLFLAG_OFF)
SYSCTL_NODE(_net_inet, OID_AUTO, ipf, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPF");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_flags, CTLFLAG_RW, &VNET_NAME(ipfmain.ipf_flags), 0, "IPF flags");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_pass, CTLFLAG_RW, &VNET_NAME(ipfmain.ipf_pass), 0, "default pass/block");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_active, CTLFLAG_RD, &VNET_NAME(ipfmain.ipf_active), 0, "IPF is active");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpidletimeout, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_tcpidletimeout), 0, "TCP idle timeout in seconds");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcphalfclosed, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_tcphalfclosed), 0, "timeout for half closed TCP sessions");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpclosewait, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_tcpclosewait), 0, "timeout for TCP sessions in closewait status");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcplastack, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_tcplastack), 0, "timeout for TCP sessions in last ack status");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcptimeout, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_tcptimeout), 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpclosed, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_tcpclosed), 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_udptimeout, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_udptimeout), 0, "UDP timeout");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_udpacktimeout, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_udpacktimeout), 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_icmptimeout, CTLFLAG_RWO,
	   &VNET_NAME(ipfmain.ipf_icmptimeout), 0, "ICMP timeout");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_running, CTLFLAG_RD,
	   &VNET_NAME(ipfmain.ipf_running), 0, "IPF is running");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_chksrc, CTLFLAG_RW, &VNET_NAME(ipfmain.ipf_chksrc), 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_minttl, CTLFLAG_RW, &VNET_NAME(ipfmain.ipf_minttl), 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, large_nat, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &VNET_NAME(ipfmain.ipf_large_nat), 0, "large_nat");

#define CDEV_MAJOR 79
#include <sys/poll.h>
#ifdef __FreeBSD__
# include <sys/select.h>
static int ipfpoll(struct cdev *dev, int events, struct thread *td);

static struct cdevsw ipf_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,	/* D_NEEDGIANT - Should be SMP safe */
	.d_open =	ipfopen,
	.d_close =	ipfclose,
	.d_read =	ipfread,
	.d_write =	ipfwrite,
	.d_ioctl =	ipfioctl,
	.d_poll =	ipfpoll,
	.d_name =	"ipf",
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
	return (error);
}


static void
vnet_ipf_init(void)
{
	char *defpass;
	int error;

	if (ipf_create_all(&V_ipfmain) == NULL)
		return;

	error = ipfattach(&V_ipfmain);
	if (error) {
		ipf_destroy_all(&V_ipfmain);
		return;
	}

	if (FR_ISPASS(V_ipfmain.ipf_pass))
		defpass = "pass";
	else if (FR_ISBLOCK(V_ipfmain.ipf_pass))
		defpass = "block";
	else
		defpass = "no-match -> block";

	if (IS_DEFAULT_VNET(curvnet)) {
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
	} else {
		(void)ipf_pfil_hook();
		ipf_event_reg();
	}
}
VNET_SYSINIT(vnet_ipf_init, SI_SUB_PROTO_FIREWALL, SI_ORDER_THIRD,
    vnet_ipf_init, NULL);

static int
ipf_modload(void)
{
	char *c, *str;
	int i, j, error;

	if (ipf_load_all() != 0)
		return (EIO);

	if (ipf_fbsd_sysctl_create() != 0) {
		return (EIO);
	}

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
		ipf_devs[i] = make_dev(&ipf_cdevsw, i, 0, 0, 0600, "%s", c);
	}

	error = ipf_pfil_hook();
	if (error != 0)
		return (error);
	ipf_event_reg();

	return (0);
}

static void
vnet_ipf_uninit(void)
{

	if (V_ipfmain.ipf_refcnt)
		return;

	if (V_ipfmain.ipf_running >= 0) {

		if (ipfdetach(&V_ipfmain) != 0)
			return;

		V_ipfmain.ipf_running = -2;

		ipf_destroy_all(&V_ipfmain);
		if (!IS_DEFAULT_VNET(curvnet)) {
			ipf_event_dereg();
			(void)ipf_pfil_unhook();
		}
	}
}
VNET_SYSUNINIT(vnet_ipf_uninit, SI_SUB_PROTO_FIREWALL, SI_ORDER_THIRD,
    vnet_ipf_uninit, NULL);

static int
ipf_modunload(void)
{
	int error, i;

	ipf_event_dereg();

	ipf_fbsd_sysctl_destroy();

	error = ipf_pfil_unhook();
	if (error != 0)
		return (error);

	for (i = 0; ipf_devfiles[i]; i++) {
		if (ipf_devs[i] != NULL)
			destroy_dev(ipf_devs[i]);
	}

	ipf_unload_all();

	printf("%s unloaded\n", ipfilter_version);

	return (0);
}


static moduledata_t ipfiltermod = {
	"ipfilter",
	ipfilter_modevent,
	0
};


DECLARE_MODULE(ipfilter, ipfiltermod, SI_SUB_PROTO_FIREWALL, SI_ORDER_SECOND);
#ifdef	MODULE_VERSION
MODULE_VERSION(ipfilter, 1);
#endif


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
		goto sysctl_error;

	if (!arg1)
		error = EPERM;
	else {
		if ((oidp->oid_kind & CTLFLAG_OFF) && (V_ipfmain.ipf_running > 0))
			error = EBUSY;
		else
			error = SYSCTL_IN(req, arg1, sizeof(int));
	}

sysctl_error:
	return (error);
}

/*
 * arg2 holds the offset of the relevant member in the virtualized
 * ipfmain structure.
 */
static int
sysctl_ipf_int_nat ( SYSCTL_HANDLER_ARGS )
{
	if (jailed_without_vnet(curthread->td_ucred))
		return (0);

	ipf_nat_softc_t *nat_softc;

	nat_softc = V_ipfmain.ipf_nat_soft;
	arg1 = (void *)((uintptr_t)nat_softc + (size_t)arg2);

	return (sysctl_ipf_int(oidp, arg1, 0, req));
}

static int
sysctl_ipf_int_state ( SYSCTL_HANDLER_ARGS )
{
	if (jailed_without_vnet(curthread->td_ucred))
		return (0);

	ipf_state_softc_t *state_softc;

	state_softc = V_ipfmain.ipf_state_soft;
	arg1 = (void *)((uintptr_t)state_softc + (size_t)arg2);

	return (sysctl_ipf_int(oidp, arg1, 0, req));
}

static int
sysctl_ipf_int_auth ( SYSCTL_HANDLER_ARGS )
{
	if (jailed_without_vnet(curthread->td_ucred))
		return (0);

	ipf_auth_softc_t *auth_softc;

	auth_softc = V_ipfmain.ipf_auth_soft;
	arg1 = (void *)((uintptr_t)auth_softc + (size_t)arg2);

	return (sysctl_ipf_int(oidp, arg1, 0, req));
}

static int
sysctl_ipf_int_frag ( SYSCTL_HANDLER_ARGS )
{
	if (jailed_without_vnet(curthread->td_ucred))
		return (0);

	ipf_frag_softc_t *frag_softc;

	frag_softc = V_ipfmain.ipf_frag_soft;
	arg1 = (void *)((uintptr_t)frag_softc + (size_t)arg2);

	return (sysctl_ipf_int(oidp, arg1, 0, req));
}
#endif


static int
#ifdef __FreeBSD__
ipfpoll(struct cdev *dev, int events, struct thread *td)
#else
ipfpoll(dev_t dev, int events, struct proc *td)
#endif
{
	int unit = GET_MINOR(dev);
	int revents;

	if (unit < 0 || unit > IPL_LOGMAX)
		return (0);

	revents = 0;

	CURVNET_SET(TD_TO_VNET(td));
	switch (unit)
	{
	case IPL_LOGIPF :
	case IPL_LOGNAT :
	case IPL_LOGSTATE :
#ifdef IPFILTER_LOG
		if ((events & (POLLIN | POLLRDNORM)) && ipf_log_canread(&V_ipfmain, unit))
			revents |= events & (POLLIN | POLLRDNORM);
#endif
		break;
	case IPL_LOGAUTH :
		if ((events & (POLLIN | POLLRDNORM)) && ipf_auth_waiting(&V_ipfmain))
			revents |= events & (POLLIN | POLLRDNORM);
		break;
	case IPL_LOGSYNC :
		if ((events & (POLLIN | POLLRDNORM)) && ipf_sync_canread(&V_ipfmain))
			revents |= events & (POLLIN | POLLRDNORM);
		if ((events & (POLLOUT | POLLWRNORM)) && ipf_sync_canwrite(&V_ipfmain))
			revents |= events & (POLLOUT | POLLWRNORM);
		break;
	case IPL_LOGSCAN :
	case IPL_LOGLOOKUP :
	default :
		break;
	}

	if ((revents == 0) && ((events & (POLLIN|POLLRDNORM)) != 0))
		selrecord(td, &V_ipfmain.ipf_selwait[unit]);
	CURVNET_RESTORE();

	return (revents);
}


/*
 * routines below for saving IP headers to buffer
 */
static int
#ifdef __FreeBSD__
ipfopen(struct cdev *dev, int flags, int devtype, struct thread *p)
#else
ipfopen(dev_t dev, int flags)
#endif
{
	int unit = GET_MINOR(dev);
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
	return (error);
}


static int
#ifdef __FreeBSD__
ipfclose(struct cdev *dev, int flags, int devtype, struct thread *p)
#else
ipfclose(dev_t dev, int flags)
#endif
{
	int	unit = GET_MINOR(dev);

	if (IPL_LOGMAX < unit)
		unit = ENXIO;
	else
		unit = 0;
	return (unit);
}

/*
 * ipfread/ipflog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
#ifdef __FreeBSD__
static int ipfread(struct cdev *dev, struct uio *uio, int ioflag)
#else
static int ipfread(dev, uio, ioflag)
	int ioflag;
	dev_t dev;
	struct uio *uio;
#endif
{
	int error;
	int	unit = GET_MINOR(dev);

	if (unit < 0)
		return (ENXIO);

	CURVNET_SET(TD_TO_VNET(curthread));
	if (V_ipfmain.ipf_running < 1) {
		CURVNET_RESTORE();
		return (EIO);
	}

	if (unit == IPL_LOGSYNC) {
		error = ipf_sync_read(&V_ipfmain, uio);
		CURVNET_RESTORE();
		return (error);
	}

#ifdef IPFILTER_LOG
	error = ipf_log_read(&V_ipfmain, unit, uio);
#else
	error = ENXIO;
#endif
	CURVNET_RESTORE();
	return (error);
}


/*
 * ipfwrite
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
#ifdef __FreeBSD__
static int ipfwrite(struct cdev *dev, struct uio *uio, int ioflag)
#else
static int ipfwrite(dev, uio, ioflag)
	int ioflag;
	dev_t dev;
	struct uio *uio;
#endif
{
	int error;

	CURVNET_SET(TD_TO_VNET(curthread));
	if (V_ipfmain.ipf_running < 1) {
		CURVNET_RESTORE();
		return (EIO);
	}

	if (GET_MINOR(dev) == IPL_LOGSYNC) {
		error = ipf_sync_write(&V_ipfmain, uio);
		CURVNET_RESTORE();
		return (error);
	}
	return (ENXIO);
}

static int
ipf_fbsd_sysctl_create(void)
{

	sysctl_ctx_init(&ipf_clist);

	SYSCTL_DYN_IPF_NAT(_net_inet_ipf, OID_AUTO, "fr_defnatage", CTLFLAG_RWO,
	    NULL, offsetof(ipf_nat_softc_t, ipf_nat_defage), "");
	SYSCTL_DYN_IPF_STATE(_net_inet_ipf, OID_AUTO, "fr_statesize", CTLFLAG_RWO,
	    NULL, offsetof(ipf_state_softc_t, ipf_state_size), "");
	SYSCTL_DYN_IPF_STATE(_net_inet_ipf, OID_AUTO, "fr_statemax", CTLFLAG_RWO,
	    NULL, offsetof(ipf_state_softc_t, ipf_state_max), "");
	SYSCTL_DYN_IPF_NAT(_net_inet_ipf, OID_AUTO, "ipf_nattable_max", CTLFLAG_RWO,
	    NULL, offsetof(ipf_nat_softc_t, ipf_nat_table_max), "");
	SYSCTL_DYN_IPF_NAT(_net_inet_ipf, OID_AUTO, "ipf_nattable_sz", CTLFLAG_RWO,
	    NULL, offsetof(ipf_nat_softc_t, ipf_nat_table_sz), "");
	SYSCTL_DYN_IPF_NAT(_net_inet_ipf, OID_AUTO, "ipf_natrules_sz", CTLFLAG_RWO,
	    NULL, offsetof(ipf_nat_softc_t, ipf_nat_maprules_sz), "");
	SYSCTL_DYN_IPF_NAT(_net_inet_ipf, OID_AUTO, "ipf_rdrrules_sz", CTLFLAG_RWO,
	    NULL, offsetof(ipf_nat_softc_t, ipf_nat_rdrrules_sz), "");
	SYSCTL_DYN_IPF_NAT(_net_inet_ipf, OID_AUTO, "ipf_hostmap_sz", CTLFLAG_RWO,
	    NULL, offsetof(ipf_nat_softc_t, ipf_nat_hostmap_sz), "");
	SYSCTL_DYN_IPF_AUTH(_net_inet_ipf, OID_AUTO, "fr_authsize", CTLFLAG_RWO,
	    NULL, offsetof(ipf_auth_softc_t, ipf_auth_size), "");
	SYSCTL_DYN_IPF_AUTH(_net_inet_ipf, OID_AUTO, "fr_authused", CTLFLAG_RD,
	    NULL, offsetof(ipf_auth_softc_t, ipf_auth_used), "");
	SYSCTL_DYN_IPF_AUTH(_net_inet_ipf, OID_AUTO, "fr_defaultauthage", CTLFLAG_RW,
	    NULL, offsetof(ipf_auth_softc_t, ipf_auth_defaultage), "");
	SYSCTL_DYN_IPF_FRAG(_net_inet_ipf, OID_AUTO, "fr_ipfrttl", CTLFLAG_RW,
	    NULL, offsetof(ipf_frag_softc_t, ipfr_ttl), "");
	return (0);
}

static int
ipf_fbsd_sysctl_destroy(void)
{
	if (sysctl_ctx_free(&ipf_clist)) {
		printf("sysctl_ctx_free failed");
		return (ENOTEMPTY);
	}
	return (0);
}
