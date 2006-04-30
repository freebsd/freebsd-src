/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000 by Darren Reed.
 *
 * $FreeBSD$
 * See the IPFILTER.LICENCE file for details on licencing.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>


#include <netinet/ipl.h>
#include <netinet/ip_compat.h>
#include <netinet/ip_fil.h>
#include <netinet/ip_state.h>
#include <netinet/ip_nat.h>
#include <netinet/ip_auth.h>
#include <netinet/ip_frag.h>

#if __FreeBSD_version >= 502116
static struct cdev *ipf_devs[IPL_LOGSIZE];
#else
static dev_t ipf_devs[IPL_LOGSIZE];
#endif

static int sysctl_ipf_int ( SYSCTL_HANDLER_ARGS );
static int ipf_modload(void);
static int ipf_modunload(void);

SYSCTL_DECL(_net_inet);
#define SYSCTL_IPF(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_INT|access, \
		   ptr, val, sysctl_ipf_int, "I", descr);
#define	CTLFLAG_OFF	0x00800000	/* IPFilter must be disabled */
#define	CTLFLAG_RWO	(CTLFLAG_RW|CTLFLAG_OFF)
SYSCTL_NODE(_net_inet, OID_AUTO, ipf, CTLFLAG_RW, 0, "IPF");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_flags, CTLFLAG_RW, &fr_flags, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_pass, CTLFLAG_RW, &fr_pass, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_active, CTLFLAG_RD, &fr_active, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpidletimeout, CTLFLAG_RWO,
	   &fr_tcpidletimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcphalfclosed, CTLFLAG_RWO,
	   &fr_tcphalfclosed, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpclosewait, CTLFLAG_RWO,
	   &fr_tcpclosewait, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcplastack, CTLFLAG_RWO,
	   &fr_tcplastack, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcptimeout, CTLFLAG_RWO,
	   &fr_tcptimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_tcpclosed, CTLFLAG_RWO,
	   &fr_tcpclosed, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_udptimeout, CTLFLAG_RWO,
	   &fr_udptimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_udpacktimeout, CTLFLAG_RWO,
	   &fr_udpacktimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_icmptimeout, CTLFLAG_RWO,
	   &fr_icmptimeout, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_defnatage, CTLFLAG_RWO,
	   &fr_defnatage, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_ipfrttl, CTLFLAG_RW,
	   &fr_ipfrttl, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_running, CTLFLAG_RD,
	   &fr_running, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_statesize, CTLFLAG_RWO,
	   &fr_statesize, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_statemax, CTLFLAG_RWO,
	   &fr_statemax, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_nattable_sz, CTLFLAG_RWO,
	   &ipf_nattable_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_natrules_sz, CTLFLAG_RWO,
	   &ipf_natrules_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_rdrrules_sz, CTLFLAG_RWO,
	   &ipf_rdrrules_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, ipf_hostmap_sz, CTLFLAG_RWO,
	   &ipf_hostmap_sz, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_authsize, CTLFLAG_RWO,
	   &fr_authsize, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_authused, CTLFLAG_RD,
	   &fr_authused, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_defaultauthage, CTLFLAG_RW,
	   &fr_defaultauthage, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_chksrc, CTLFLAG_RW, &fr_chksrc, 0, "");
SYSCTL_IPF(_net_inet_ipf, OID_AUTO, fr_minttl, CTLFLAG_RW, &fr_minttl, 0, "");

#define CDEV_MAJOR 79
#if __FreeBSD_version >= 501000
static struct cdevsw ipl_cdevsw = {
# if __FreeBSD_version >= 502103
	.d_version =	D_VERSION,
	.d_flags =	0,	/* D_NEEDGIANT - Should be SMP safe */
# endif
	.d_open =	iplopen,
	.d_close =	iplclose,
	.d_read =	iplread,
	.d_ioctl =	iplioctl,
	.d_name =	"ipl",
# if __FreeBSD_version < 600000
	.d_maj =	CDEV_MAJOR,
# endif
};
#else
static struct cdevsw ipl_cdevsw = {
	/* open */	iplopen,
	/* close */	iplclose,
	/* read */	iplread,
	/* write */	iplwrite,
	/* ioctl */	iplioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ipl",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
# if (__FreeBSD_version < 500043)
	/* bmaj */	-1,
# endif
	/* kqfilter */	NULL
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

	error = iplattach();
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
		ipf_devs[i] = make_dev(&ipl_cdevsw, i, 0, 0, 0600, c);
	}

	if (FR_ISPASS(fr_pass))
		defpass = "pass";
	else if (FR_ISBLOCK(fr_pass))
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

	if (fr_refcnt)
		return EBUSY;

	if (fr_running >= 0) {
		error = ipldetach();
		if (error != 0)
			return error;
	} else
		error = 0;

	fr_running = -2;

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
		if ((oidp->oid_kind & CTLFLAG_OFF) && (fr_running > 0))
			error = EBUSY;
		else
			error = SYSCTL_IN(req, arg1, sizeof(int));
	}
	return (error);
}
#endif
