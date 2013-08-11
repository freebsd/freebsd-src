/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/* #pragma ident   "@(#)solaris.c	1.12 6/5/96 (C) 1995 Darren Reed"*/
#pragma ident "@(#)$Id$"

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/cred.h>
#include <sys/dditypes.h>
#include <sys/stream.h>
#include <sys/poll.h>
#include <sys/autoconf.h>
#include <sys/byteorder.h>
#include <sys/socket.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/sockio.h>
#include <net/if.h>
#if SOLARIS2 >= 6
# include <net/if_types.h>
#endif
#include <net/af.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "netinet/ip_compat.h"
#include "netinet/ipl.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_state.h"
#include "netinet/ip_sync.h"
#include "netinet/ip_lookup.h"
#ifdef INSTANCES
# include <sys/hook_event.h>
#endif

extern	void	ipf_rand_push(void *, int);

static	int	ipf_getinfo __P((dev_info_t *, ddi_info_cmd_t,
				 void *, void **));
#if SOLARIS2 < 10
static	int	ipf_identify __P((dev_info_t *));
#endif
static	int	ipf_attach __P((dev_info_t *, ddi_attach_cmd_t));
static	int	ipf_detach __P((dev_info_t *, ddi_detach_cmd_t));
static	int	ipf_detach_instance __P((ipf_main_softc_t *));
static 	int	ipfpoll __P((dev_t, short, int, short *, struct pollhead **));
static	char	*ipf_devfiles[] = { IPL_NAME, IPNAT_NAME, IPSTATE_NAME,
				    IPAUTH_NAME, IPSYNC_NAME, IPSCAN_NAME,
				    IPLOOKUP_NAME, NULL };
static	int	ipfclose __P((dev_t, int, int, cred_t *));
static	int	ipfopen __P((dev_t *, int, int, cred_t *));
static	int	ipfread __P((dev_t, struct uio *, cred_t *));
static	int	ipfwrite __P((dev_t, struct uio *, cred_t *));
static	int	ipf_property_update __P((dev_info_t *));
static	int	ipf_solaris_init __P((void));
static	int	ipf_stack_init __P((void));
static	void	ipf_stack_fini __P((void));
#if !defined(INSTANCES)
static	int	ipf_qifsync __P((ip_t *, int, void *, int, void *, mblk_t **));
#else
static	void	ipf_attach_loopback __P((ipf_main_softc_t *));
static	void	ipf_detach_loopback __P((ipf_main_softc_t *));
#endif


static struct cb_ops ipf_cb_ops = {
	ipfopen,
	ipfclose,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	ipfread,
	ipfwrite,	/* write */
	ipfioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	ipfpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_MTSAFE,
#if SOLARIS2 > 4
	CB_REV,
	nodev,		/* aread */
	nodev,		/* awrite */
#endif
};

static struct dev_ops ipf_ops = {
	DEVO_REV,
	0,
	ipf_getinfo,
#if SOLARIS2 >= 10
	nulldev,
#else
	ipf_identify,
#endif
	nulldev,
	ipf_attach,
	ipf_detach,
	nodev,		/* reset */
	&ipf_cb_ops,
	(struct bus_ops *)0
};

extern struct mod_ops mod_driverops;
static struct modldrv iplmod = {
	&mod_driverops, IPL_VERSION, &ipf_ops };
static struct modlinkage modlink1 = { MODREV_1, &iplmod, NULL };
static int ipf_pkts = 0;

#if SOLARIS2 >= 6
static	size_t	hdrsizes[57][2] = {
	{ 0, 0 },
	{ IFT_OTHER, 0 },
	{ IFT_1822, 0 },
	{ IFT_HDH1822, 0 },
	{ IFT_X25DDN, 0 },
	{ IFT_X25, 0 },
	{ IFT_ETHER, 14 },
	{ IFT_ISO88023, 0 },
	{ IFT_ISO88024, 0 },
	{ IFT_ISO88025, 0 },
	{ IFT_ISO88026, 0 },
	{ IFT_STARLAN, 0 },
	{ IFT_P10, 0 },
	{ IFT_P80, 0 },
	{ IFT_HY, 0 },
	{ IFT_FDDI, 24 },
	{ IFT_LAPB, 0 },
	{ IFT_SDLC, 0 },
	{ IFT_T1, 0 },
	{ IFT_CEPT, 0 },
	{ IFT_ISDNBASIC, 0 },
	{ IFT_ISDNPRIMARY, 0 },
	{ IFT_PTPSERIAL, 0 },
	{ IFT_PPP, 0 },
	{ IFT_LOOP, 0 },
	{ IFT_EON, 0 },
	{ IFT_XETHER, 0 },
	{ IFT_NSIP, 0 },
	{ IFT_SLIP, 0 },
	{ IFT_ULTRA, 0 },
	{ IFT_DS3, 0 },
	{ IFT_SIP, 0 },
	{ IFT_FRELAY, 0 },
	{ IFT_RS232, 0 },
	{ IFT_PARA, 0 },
	{ IFT_ARCNET, 0 },
	{ IFT_ARCNETPLUS, 0 },
	{ IFT_ATM, 0 },
	{ IFT_MIOX25, 0 },
	{ IFT_SONET, 0 },
	{ IFT_X25PLE, 0 },
	{ IFT_ISO88022LLC, 0 },
	{ IFT_LOCALTALK, 0 },
	{ IFT_SMDSDXI, 0 },
	{ IFT_FRELAYDCE, 0 },
	{ IFT_V35, 0 },
	{ IFT_HSSI, 0 },
	{ IFT_HIPPI, 0 },
	{ IFT_MODEM, 0 },
	{ IFT_AAL5, 0 },
	{ IFT_SONETPATH, 0 },
	{ IFT_SONETVT, 0 },
	{ IFT_SMDSICIP, 0 },
	{ IFT_PROPVIRTUAL, 0 },
	{ IFT_PROPMUX, 0 },
};
#endif /* SOLARIS2 >= 6 */

static dev_info_t	*ipf_dev_info = NULL;
#if defined(FW_HOOKS)
ipf_main_softc_t	*ipf_instances = NULL;
net_instance_t		*ipf_inst = NULL;
#else
ipf_main_softc_t	ipfmain;
ipf_main_softc_t	*ipf_instances = &ipfmain;
#endif


int
_init()
{
	int rval;

	rval = mod_install(&modlink1);
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: _init() = %d", rval);
#endif
	if (rval != 0)
		return rval;

	rval = ipf_solaris_init();
	if (rval != 0)
		(void) mod_remove(&modlink1);
	return rval;
}


int
_fini(void)
{
	int rval;

	rval = mod_remove(&modlink1);
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: _fini() = %d", rval);
#endif
	if (rval == 0) {
		ipf_unload_all();
		ipf_stack_fini();
	}
	return rval;
}


int
_info(modinfop)
	struct modinfo *modinfop;
{
	int ipfinst;

	ipfinst = mod_info(&modlink1, modinfop);
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: _info(%x) = %x", modinfop, ipfinst);
#endif
	return ipfinst;
}


#if SOLARIS2 < 10
static int
ipf_identify(dip)
	dev_info_t *dip;
{
# ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_identify(%x)", dip);
# endif
	if (strcmp(ddi_get_name(dip), "ipf") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}
#endif


static int
ipf_attach(dip, cmd)
	dev_info_t *dip;
	ddi_attach_cmd_t cmd;
{
	ipf_main_softc_t *softc;
	char *s;
	int i;
#ifdef	IPFDEBUG
	int instance;

	cmn_err(CE_NOTE, "IP Filter: ipf_attach(%x,%x)", dip, cmd);
#endif

	ipf_rand_push(dip, sizeof(*dip));

#if !defined(INSTANCES)
	if ((pfilinterface != PFIL_INTERFACE) || (PFIL_INTERFACE < 2000000)) {
		cmn_err(CE_NOTE, "pfilinterface(%d) != %d\n", pfilinterface,
			PFIL_INTERFACE);
		return EINVAL;
	}
#endif
	softc = GET_SOFTC(0);
	if (softc == NULL)
		return ENXIO;

	switch (cmd)
	{
	case DDI_ATTACH:
		if (softc->ipf_running != 0)
			break;
#ifdef	IPFDEBUG
		instance = ddi_get_instance(dip);

		cmn_err(CE_NOTE, "IP Filter: attach ipf instance %d", instance);
#endif

		softc->ipf_dip = dip;

		(void) ipf_property_update(dip);

		for (i = 0; ((s = ipf_devfiles[i]) != NULL); i++) {
			s = strrchr(s, '/');
			if (s == NULL)
				continue;
			s++;
			if (ddi_create_minor_node(dip, s, S_IFCHR, i,
						  DDI_PSEUDO, 0) ==
			    DDI_FAILURE) {
				ddi_remove_minor_node(dip, NULL);
				goto attach_failed;
			}
		}

		ipf_dev_info = dip;
		/*
		 * Initialize mutex's
		 */

		cmn_err(CE_CONT, "!%s, loaded.\n", ipfilter_version);

		return DDI_SUCCESS;
		/* NOTREACHED */
	default:
		break;
	}

attach_failed:
	cmn_err(CE_NOTE, "IP Filter: failed to attach\n");
	/*
	 * Use our own detach routine to toss
	 * away any stuff we allocated above.
	 */
	(void) ipf_detach(dip, DDI_DETACH);
	return DDI_FAILURE;
}


static int
ipf_detach(dip, cmd)
	dev_info_t *dip;
	ddi_detach_cmd_t cmd;
{
	ipf_main_softc_t *tmp;
	int i;

#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_detach(%x,%x)", dip, cmd);
#endif

	switch (cmd) {
	case DDI_DETACH:
		for (tmp = ipf_instances; tmp != NULL; tmp = tmp->ipf_next)
			if (ipf_detach_instance(tmp) != DDI_SUCCESS)
				break;

		if (tmp != NULL)
			break;

		/*
		 * Undo what we did in ipf_attach, freeing resources
		 * and removing things we installed.  The system
		 * framework guarantees we are not active with this devinfo
		 * node in any other entry points at this time.
		 */
		ddi_prop_remove_all(dip);
		i = ddi_get_instance(dip);
		if (i > 0) {
			cmn_err(CE_CONT, "IP Filter: still attached (%d)\n", i);
			return DDI_FAILURE;
		}

		cmn_err(CE_CONT, "!%s detached.\n", ipfilter_version);
		return (DDI_SUCCESS);
	default:
		break;
	}
	cmn_err(CE_NOTE, "IP Filter: failed to detach\n");
	return DDI_FAILURE;
}


/*ARGSUSED*/
static int
ipf_getinfo(dip, infocmd, arg, result)
	dev_info_t *dip;
	ddi_info_cmd_t infocmd;
	void *arg, **result;
{
	int error;

	error = DDI_FAILURE;
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_getinfo(%x,%x,%x)", dip, infocmd, arg);
#endif
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = ipf_dev_info;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}
	return (error);
}


static int
ipf_solaris_init()
{
	int rval = DDI_SUCCESS;

	rval = ipf_load_all();
	if (rval != 0) {
#ifdef	IPFDEBUG
		cmn_err(CE_NOTE, "IP Filter: ipf_load_all() failed\n");
#endif
		return rval;
	}

#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_load_all() done\n");
#endif
	rval = ipf_stack_init();
	if (rval != 0) {
		ipf_unload_all();
		return rval;
	}
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_stack_init() done\n");
#endif
	return 0;
}


#if !defined(INSTANCES)
/*
 * look for bad consistancies between the list of interfaces the filter knows
 * about and those which are currently configured.
 */
/*ARGSUSED*/
static int
ipf_qifsync(ip, hlen, il, out, qif, mp)
	ip_t *ip;
	int hlen;
	void *il;
	int out;
	void *qif;
	mblk_t **mp;
{
	ipf_main_softc_t *softc = &ipfmain;

	ipf_sync(softc, qif);
	/*
	 * Resync. any NAT `connections' using this interface and its IP #.
	 */
	ipf_nat_sync(softc, qif);
	ipf_state_sync(softc, qif);
	ipf_lookup_sync(softc, qif);
	return 0;
}


/*
 * look for bad consistancies between the list of interfaces the filter knows
 * about and those which are currently configured.
 */
int
ipfsync()
{
	ipf_sync(&ipfmain, NULL);
	return 0;
}
#endif


/*
 * Fetch configuration file values that have been entered into the ipf.conf
 * driver file.
 */
int
ipf_property_update(dip)
	dev_info_t *dip;
{
	ipf_main_softc_t *softc;
	ipftuneable_t *ipft;
	int64_t i64;
	char *name;
	u_int one;
	int *i32p;
	int err;

#ifdef DDI_NO_AUTODETACH
	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip,
				DDI_NO_AUTODETACH, 1) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "!updating DDI_NO_AUTODETACH failed");
		return DDI_FAILURE;
	}
#else
	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip,
				"ddi-no-autodetach", 1) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "!updating ddi-no-autodetach failed");
		return DDI_FAILURE;
	}
#endif

	softc = GET_SOFTC(0);

	ipft = softc->ipf_tuners;
	if (ipft == NULL) {
		cmn_err(CE_WARN, "!no tuners loaded");
		return DDI_FAILURE;
	}

	err = DDI_SUCCESS;
	for (; (ipft != NULL) && ((name = (char *)ipft->ipft_name) != NULL);
	     ipft = ipft->ipft_next) {
		one = 1;
		i32p = NULL;
		err = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
						0, name, &i32p, &one);
		if (err == DDI_PROP_NOT_FOUND || i32p == NULL)
			continue;
		if (*i32p >= ipft->ipft_min && *i32p <= ipft->ipft_max) {
			if (ipft->ipft_sz == sizeof(int)) {
  				*ipft->ipft_pint = *i32p;
			} else {
				i64 = *(u_int *)i32p;
				*(u_long *)ipft->ipft_pint = i64;
			}
		}
	}

	return err;
}


static int
ipfpoll(dev, events, anyyet, reventsp, phpp)
	dev_t dev;
	short events;
	int anyyet;
	short *reventsp;
	struct pollhead **phpp;
{
	ipf_main_softc_t *softc;
	u_int xmin = getminor(dev);
	int revents = 0;

	if (xmin < 0 || xmin > IPL_LOGMAX)
		return ENXIO;

	softc = GET_SOFTC(getzoneid());

	switch (xmin)
	{
	case IPL_LOGIPF :
	case IPL_LOGNAT :
	case IPL_LOGSTATE :
#ifdef IPFILTER_LOG
		if ((events & (POLLIN | POLLRDNORM)) && ipf_log_canread(softc, xmin))
			revents |= events & (POLLIN | POLLRDNORM);
#endif
		break;
	case IPL_LOGAUTH :
		if ((events & (POLLIN | POLLRDNORM)) &&
		    ipf_auth_waiting(softc))
			revents |= events & (POLLIN | POLLRDNORM);
		break;
	case IPL_LOGSYNC :
		if ((events & (POLLIN | POLLRDNORM)) &&
		    ipf_sync_canread(softc))
			revents |= events & (POLLIN | POLLRDNORM);
		if ((events & (POLLOUT | POLLWRNORM)) &&
		    ipf_sync_canwrite(softc))
			revents |= events & POLLOUT;
#ifdef POLLOUTNORM
			revents |= events & POLLOUTNORM;
#endif
		break;
	case IPL_LOGSCAN :
	case IPL_LOGLOOKUP :
	default :
		break;
	}

	if (revents) {
		*reventsp = revents;
	} else {
		*reventsp = 0;
		if (!anyyet)
			*phpp = &softc->ipf_poll_head[xmin];
	}
	return 0;
}


/*
 * routines below for saving IP headers to buffer
 */
/*ARGSUSED*/
static int
ipfopen(devp, flags, otype, cred)
	dev_t *devp;
	int flags, otype;
	cred_t *cred;
{
	minor_t unit = getminor(*devp);
	int error;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfopen(%x,%x,%x,%x)\n", devp, flags, otype, cred);
#endif
	if (!(otype & OTYP_CHR))
		return ENXIO;

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


/*ARGSUSED*/
static int
ipfclose(dev, flags, otype, cred)
	dev_t dev;
	int flags, otype;
	cred_t *cred;
{
	minor_t	min = getminor(dev);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplclose(%x,%x,%x,%x)\n", dev, flags, otype, cred);
#endif

	min = (IPL_LOGMAX < min) ? ENXIO : 0;
	return min;
}


/*
 * ipfread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
/*ARGSUSED*/
static int
ipfread(dev, uio, cp)
	dev_t dev;
	register struct uio *uio;
	cred_t *cp;
{
	ipf_main_softc_t *softc;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfread(%x,%x,%x)\n", dev, uio, cp);
#endif

	softc = GET_SOFTC(crgetzoneid(cp));

	if (softc->ipf_running < 1)
		return EIO;

	if (getminor(dev) == IPL_LOGSYNC)
		return ipf_sync_read(softc, uio);

#ifdef	IPFILTER_LOG
	return ipf_log_read(softc, getminor(dev), uio);
#else
	return ENXIO;
#endif
}


/*
 * ipfread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
static int
ipfwrite(dev, uio, cp)
	dev_t dev;
	register struct uio *uio;
	cred_t *cp;
{
	ipf_main_softc_t *softc;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfwrite(%x,%x,%x)\n", dev, uio, cp);
#endif

	softc = GET_SOFTC(crgetzoneid(cp));

	if (softc->ipf_running < 1)
		return EIO;

	if (getminor(dev) == IPL_LOGSYNC)
		return ipf_sync_write(softc, uio);
	return ENXIO;
}

#if !defined(INSTANCES)

static int
ipf_bounce(ctx, ip, hlen, ifp, out, qif, mp)
	void *ctx;
	ip_t *ip;
	int hlen;
	void *ifp;
	int out;
	void *qif;
	mb_t **mp;
{
	if ((++ipf_pkts & 0xffff) == 0)
		ipf_rand_push(*mp, M_LEN(*mp));

	return ipf_check(ctx, ip, hlen, ifp, out, qif, mp);
}

void
ipf_pfil_hooks_add()
{
	if (pfil_add_hook(ipf_bounce, PFIL_IN|PFIL_OUT, &pfh_inet4))
		cmn_err(CE_WARN, "IP Filter: %s(pfh_inet4) failed",
			"pfil_add_hook");
# ifdef USE_INET6
	if (pfil_add_hook(ipf_bounce, PFIL_IN|PFIL_OUT, &pfh_inet6))
		cmn_err(CE_WARN, "IP Filter: %s(pfh_inet6) failed",
			"pfil_add_hook");
# endif
	if (pfil_add_hook(ipf_qifsync, PFIL_IN|PFIL_OUT, &pfh_sync))
		cmn_err(CE_WARN, "IP Filter: %s(pfh_sync) failed",
			"pfil_add_hook");
}

void
ipf_pfil_hooks_remove()
{
	if (pfil_remove_hook(ipf_bounce, PFIL_IN|PFIL_OUT, &pfh_inet4))
		cmn_err(CE_WARN, "IP Filter: %s(pfh_inet4) failed",
			"pfil_remove_hook");
# ifdef USE_INET6
	if (pfil_remove_hook(ipf_bounce, PFIL_IN|PFIL_OUT, &pfh_inet6))
		cmn_err(CE_WARN, "IP Filter: %s(pfh_inet6) failed",
			"pfil_add_hook");
# endif
	if (pfil_remove_hook(ipf_qifsync, PFIL_IN|PFIL_OUT, &pfh_sync))
		cmn_err(CE_WARN, "IP Filter: %s(pfh_sync) failed",
			"pfil_remove_hook");

}


static int
ipf_stack_init()
{
	ipf_create_all(&ipfmain);
	return (0);
}


static void
ipf_stack_fini()
{
	ipf_destroy_all(&ipfmain);
}
#else


int
ipf_hk_v4_in(tok, data, arg)
	hook_event_token_t tok;
	hook_data_t data;
	void *arg;
{
	hook_pkt_event_t *hpe = (hook_pkt_event_t *)data;
	ipf_main_softc_t *softc = (ipf_main_softc_t *)arg;
	ip_t *ip = hpe->hpe_hdr;
	qpktinfo_t qpi;
	int rval;

	qpi.qpi_real = (void *)hpe->hpe_ifp;
	qpi.qpi_ill = (void *)hpe->hpe_ifp;
	qpi.qpi_q = NULL;
	qpi.qpi_m = hpe->hpe_mb;
	qpi.qpi_data = hpe->hpe_hdr;
	qpi.qpi_off = 0;
	qpi.qpi_flags = hpe->hpe_flags;
	if ((++ipf_pkts & 0xffff) == 0)
		ipf_rand_push(qpi.qpi_m, M_LEN(qpi.qpi_m));

	rval = ipf_check(softc, hpe->hpe_hdr, ip->ip_hl << 2,
			 (void *)hpe->hpe_ifp, 0, &qpi, hpe->hpe_mp);
	if (rval == 0 && *hpe->hpe_mp == NULL)
		rval = 1;

	hpe->hpe_hdr = qpi.qpi_data;
	hpe->hpe_mb = qpi.qpi_m;

	return rval;
}


int
ipf_hk_v4_out(tok, data, arg)
	hook_event_token_t tok;
	hook_data_t data;
	void *arg;
{
	hook_pkt_event_t *hpe = (hook_pkt_event_t *)data;
	ipf_main_softc_t *softc = (ipf_main_softc_t *)arg;
	ip_t *ip = hpe->hpe_hdr;
	qpktinfo_t qpi;
	int rval;

	qpi.qpi_real = (void *)hpe->hpe_ofp;
	qpi.qpi_ill = (void *)hpe->hpe_ofp;
	qpi.qpi_q = NULL;
	qpi.qpi_m = hpe->hpe_mb;
	qpi.qpi_data = hpe->hpe_hdr;
	qpi.qpi_off = 0;
	qpi.qpi_flags = hpe->hpe_flags;
	if ((++ipf_pkts & 0xffff) == 0)
		ipf_rand_push(qpi.qpi_m, M_LEN(qpi.qpi_m));

	rval = ipf_check(softc, hpe->hpe_hdr, ip->ip_hl << 2,
			 (void *)hpe->hpe_ofp, 1, &qpi, hpe->hpe_mp);
	if (rval == 0 && *hpe->hpe_mp == NULL)
		rval = 1;

	hpe->hpe_hdr = qpi.qpi_data;
	hpe->hpe_mb = qpi.qpi_m;

	return rval;
}


int
ipf_hk_v4_nic(tok, data, arg)
	hook_event_token_t tok;
	hook_data_t data;
	void *arg;
{
	hook_nic_event_t *nic = (hook_nic_event_t *)data;
	ipf_main_softc_t *softc = (ipf_main_softc_t *)arg;

	/*
	 * Should pass the family through...
	 */
	switch (nic->hne_event)
	{
	case NE_PLUMB :
	case NE_UNPLUMB :
		ipf_sync(softc, (void *)nic->hne_nic);
		ipf_nat_sync(softc, (void *)nic->hne_nic);
		ipf_state_sync(softc, (void *)nic->hne_nic);
		break;
	case NE_UP :
	case NE_DOWN :
		break;
	case NE_ADDRESS_CHANGE :
		if (nic->hne_lif == 0) {
			ipf_sync(softc, (void *)nic->hne_nic);
			ipf_nat_sync(softc, (void *)nic->hne_nic);
		}
		break;
	}

	return 0;
}


int
ipf_hk_v6_in(tok, data, arg)
	hook_event_token_t tok;
	hook_data_t data;
	void *arg;
{
	hook_pkt_event_t *hpe = (hook_pkt_event_t *)data;
	ipf_main_softc_t *softc = (ipf_main_softc_t *)arg;
	qpktinfo_t qpi;
	int rval;

	qpi.qpi_real = (void *)hpe->hpe_ifp;
	qpi.qpi_ill = (void *)hpe->hpe_ifp;
	qpi.qpi_q = NULL;
	qpi.qpi_m = hpe->hpe_mb;
	qpi.qpi_data = hpe->hpe_hdr;
	qpi.qpi_off = 0;
	qpi.qpi_flags = hpe->hpe_flags;
	if ((++ipf_pkts & 0xffff) == 0)
		ipf_rand_push(qpi.qpi_m, M_LEN(qpi.qpi_m));

	rval = ipf_check(softc, hpe->hpe_hdr, sizeof(ip6_t),
			 (void *)hpe->hpe_ifp, 0, &qpi, hpe->hpe_mp);
	if (rval == 0 && *hpe->hpe_mp == NULL)
		rval = 1;

	hpe->hpe_hdr = qpi.qpi_data;
	hpe->hpe_mb = qpi.qpi_m;

	return rval;
}


int
ipf_hk_v6_out(tok, data, arg)
	hook_event_token_t tok;
	hook_data_t data;
	void *arg;
{
	hook_pkt_event_t *hpe = (hook_pkt_event_t *)data;
	ipf_main_softc_t *softc = (ipf_main_softc_t *)arg;
	qpktinfo_t qpi;
	int rval;

	qpi.qpi_real = (void *)hpe->hpe_ofp;
	qpi.qpi_ill = (void *)hpe->hpe_ofp;
	qpi.qpi_q = NULL;
	qpi.qpi_m = hpe->hpe_mb;
	qpi.qpi_data = hpe->hpe_hdr;
	qpi.qpi_off = 0;
	qpi.qpi_flags = hpe->hpe_flags;
	if ((++ipf_pkts & 0xffff) == 0)
		ipf_rand_push(qpi.qpi_m, M_LEN(qpi.qpi_m));

	rval = ipf_check(softc, hpe->hpe_hdr, sizeof(ip6_t),
			 (void *)hpe->hpe_ofp, 1, &qpi, hpe->hpe_mp);
	if (rval == 0 && *hpe->hpe_mp == NULL)
		rval = 1;

	hpe->hpe_hdr = qpi.qpi_data;
	hpe->hpe_mb = qpi.qpi_m;

	return rval;
}


int
ipf_hk_v6_nic(tok, data, arg)
	hook_event_token_t tok;
	hook_data_t data;
	void *arg;
{
	hook_nic_event_t *nic = (hook_nic_event_t *)data;
	ipf_main_softc_t *softc = (ipf_main_softc_t *)arg;

	switch (nic->hne_event)
	{
	case NE_PLUMB :
	case NE_UNPLUMB :
		break;
	case NE_UP :
	case NE_DOWN :
		break;
	case NE_ADDRESS_CHANGE :
		break;
	}

	return 0;
}


ipf_main_softc_t *
ipf_find_softc(x)
	u_long x;
{
	ipf_main_softc_t *softc;

	for (softc = ipf_instances; softc != NULL; softc = softc->ipf_next)
		if (softc->ipf_idnum == x)
			break;
	return softc;
}


static void *
ipf_instance_create(netid_t id)
{
	ipf_main_softc_t *softc;

	softc = ipf_create_all(NULL);
	if (softc != NULL) {
		softc->ipf_next = ipf_instances;
		ipf_instances = softc;

		softc->ipf_idnum = id;
	}

	return softc;
}


static void
ipf_instance_shutdown(netid_t id, void *arg)
{
	ipf_main_softc_t *softc = arg;

	(void) ipf_detach_instance(softc);
}


static void
ipf_instance_destroy(netid_t id, void *arg)
{
	ipf_main_softc_t *softc = arg;
	ipf_main_softc_t **instp;

	for (instp = &ipf_instances; *instp != NULL; ) {
		if (*instp == softc) {
			*instp = softc->ipf_next;
			break;
		}
		instp = &(*instp)->ipf_next;
	}

	ipf_destroy_all(softc);
}


static int
ipf_stack_init()
{
	ipf_inst = net_instance_alloc(NETINFO_VERSION);
	ipf_inst->nin_name = "ipf";
	ipf_inst->nin_create = ipf_instance_create;
	ipf_inst->nin_destroy = ipf_instance_destroy;
	ipf_inst->nin_shutdown = ipf_instance_shutdown;
	return net_instance_register(ipf_inst);
}


static void
ipf_stack_fini()
{
	net_instance_unregister(ipf_inst);
	net_instance_free(ipf_inst);
	ipf_inst = NULL;
}


void
ipf_attach_hooks(softc)
	ipf_main_softc_t *softc;
{
	softc->ipf_nd_v4 = net_protocol_lookup(softc->ipf_idnum, NHF_INET);
	softc->ipf_nd_v6 = net_protocol_lookup(softc->ipf_idnum, NHF_INET6);

	HOOK_INIT(softc->ipf_hk_v4_in, ipf_hk_v4_in, "ipf_v4_in", softc);
	if (net_hook_register(softc->ipf_nd_v4, NH_PHYSICAL_IN,
			      softc->ipf_hk_v4_in))
		cmn_err(CE_WARN, "register-hook(v4-in) failed");

	HOOK_INIT(softc->ipf_hk_v4_out, ipf_hk_v4_out, "ipf_v4_out", softc);
	if (net_hook_register(softc->ipf_nd_v4, NH_PHYSICAL_OUT,
			      softc->ipf_hk_v4_out))
		cmn_err(CE_WARN, "register-hook(v4-out) failed");

	HOOK_INIT(softc->ipf_hk_v4_nic, ipf_hk_v4_nic, "ipf_v4_event", softc);
	if (net_hook_register(softc->ipf_nd_v4, NH_NIC_EVENTS,
			      softc->ipf_hk_v4_nic))
		cmn_err(CE_WARN, "register-hook(v4-nic) failed");

	HOOK_INIT(softc->ipf_hk_v6_in, ipf_hk_v6_in, "ipf_v6_in", softc);
	if (net_hook_register(softc->ipf_nd_v6, NH_PHYSICAL_IN,
			      softc->ipf_hk_v6_in))
		cmn_err(CE_WARN, "register-hook(v6-in) failed");

	HOOK_INIT(softc->ipf_hk_v6_out, ipf_hk_v6_out, "ipf_v6_out", softc);
	if (net_hook_register(softc->ipf_nd_v6, NH_PHYSICAL_OUT,
			      softc->ipf_hk_v6_out))
		cmn_err(CE_WARN, "register-hook(v6-out) failed");

	HOOK_INIT(softc->ipf_hk_v6_nic, ipf_hk_v6_nic, "ipf_v6_event", softc);
	if (net_hook_register(softc->ipf_nd_v6, NH_NIC_EVENTS,
			      softc->ipf_hk_v6_nic))
		cmn_err(CE_WARN, "register-hook(v6-nic) failed");

	if (softc->ipf_get_loopback)
		ipf_attach_loopback(softc);
}


void
ipf_detach_hooks(softc)
	ipf_main_softc_t *softc;
{

	if (softc->ipf_get_loopback)
		ipf_detach_loopback(softc);

	if (softc->ipf_nd_v4 != NULL) {
		if (net_hook_unregister(softc->ipf_nd_v4, NH_PHYSICAL_IN,
					softc->ipf_hk_v4_in))
			cmn_err(CE_WARN, "unregister-hook(v4-in) failed");
		hook_free(softc->ipf_hk_v4_in);

		if (net_hook_unregister(softc->ipf_nd_v4, NH_PHYSICAL_OUT,
					softc->ipf_hk_v4_out))
			cmn_err(CE_WARN, "unregister-hook(v4-out) failed");
		hook_free(softc->ipf_hk_v4_out);

		if (net_hook_unregister(softc->ipf_nd_v4, NH_NIC_EVENTS,
					softc->ipf_hk_v4_nic))
			cmn_err(CE_WARN, "unregister-hook(v4-nic) failed");
		hook_free(softc->ipf_hk_v4_nic);

		net_protocol_release(softc->ipf_nd_v4);
		softc->ipf_nd_v4 = NULL;
	}

	if (softc->ipf_nd_v6 != NULL) {
		if (net_hook_unregister(softc->ipf_nd_v6, NH_PHYSICAL_IN,
					softc->ipf_hk_v6_in))
			cmn_err(CE_WARN, "unregister-hook(v6-in) failed");
		hook_free(softc->ipf_hk_v6_in);

		if (net_hook_unregister(softc->ipf_nd_v6, NH_PHYSICAL_OUT,
					softc->ipf_hk_v6_out))
			cmn_err(CE_WARN, "unregister-hook(v6-out) failed");
		hook_free(softc->ipf_hk_v6_out);

		if (net_hook_unregister(softc->ipf_nd_v6, NH_NIC_EVENTS,
					softc->ipf_hk_v6_nic))
			cmn_err(CE_WARN, "unregister-hook(v6-nic) failed");
		hook_free(softc->ipf_hk_v6_nic);

		net_protocol_release(softc->ipf_nd_v6);
		softc->ipf_nd_v6 = NULL;
	}
}

int
ipf_set_loopback(softc, t, p)
	struct ipf_main_softc_s *softc;
	ipftuneable_t *t;
	ipftuneval_t *p;
{
        if (*t->ipft_pint == p->ipftu_int)
		return 0;

	*t->ipft_pint = p->ipftu_int;
	if (p->ipftu_int == 0) {
		/*
		 * Turning it off.
		 */
		 if (softc->ipf_running == 1)
			 ipf_detach_loopback(softc);
		 return 0;
	}
	 if (softc->ipf_running == 1)
		 ipf_attach_loopback(softc);
	 return 0;
}


void
ipf_attach_loopback(softc)
	ipf_main_softc_t *softc;
{

	HOOK_INIT(softc->ipf_hk_loop_v4_in, ipf_hk_v4_in,
		  "ipf_v4_loop_in", softc);
	if (net_hook_register(softc->ipf_nd_v4, NH_LOOPBACK_IN,
			      softc->ipf_hk_loop_v4_in))
		cmn_err(CE_WARN, "register-hook(v4-loop_in) failed");

	HOOK_INIT(softc->ipf_hk_loop_v4_out, ipf_hk_v4_out,
		  "ipf_v4_loop_out", softc);
	if (net_hook_register(softc->ipf_nd_v4, NH_LOOPBACK_OUT,
			      softc->ipf_hk_loop_v4_out))
		cmn_err(CE_WARN, "register-hook(v4-loop_out) failed");

	HOOK_INIT(softc->ipf_hk_loop_v6_in, ipf_hk_v6_in,
		  "ipf_v6_loop_in", softc);
	if (net_hook_register(softc->ipf_nd_v6, NH_LOOPBACK_IN,
			      softc->ipf_hk_loop_v6_in))
		cmn_err(CE_WARN, "register-hook(v6-loop_in) failed");

	HOOK_INIT(softc->ipf_hk_loop_v6_out, ipf_hk_v6_out,
		  "ipf_v6_loop_out", softc);
	if (net_hook_register(softc->ipf_nd_v6, NH_LOOPBACK_OUT,
			      softc->ipf_hk_loop_v6_out))
		cmn_err(CE_WARN, "register-hook(v6-loop_out) failed");
}


void
ipf_detach_loopback(softc)
	ipf_main_softc_t *softc;
{

	if (net_hook_unregister(softc->ipf_nd_v4, NH_LOOPBACK_IN,
				softc->ipf_hk_loop_v4_in))
		cmn_err(CE_WARN, "unregister-hook(v4-loop_in) failed");
	hook_free(softc->ipf_hk_v4_in);

	if (net_hook_unregister(softc->ipf_nd_v4, NH_LOOPBACK_OUT,
				softc->ipf_hk_loop_v4_out))
		cmn_err(CE_WARN, "unregister-hook(v4-loop_out) failed");
	hook_free(softc->ipf_hk_v4_out);

	if (net_hook_unregister(softc->ipf_nd_v6, NH_LOOPBACK_IN,
				softc->ipf_hk_loop_v6_in))
		cmn_err(CE_WARN, "unregister-hook(v6-loop_in) failed");
	hook_free(softc->ipf_hk_v6_in);

	if (net_hook_unregister(softc->ipf_nd_v6, NH_LOOPBACK_OUT,
				softc->ipf_hk_loop_v6_out))
		cmn_err(CE_WARN, "unregister-hook(v6-loop_out) failed");
	hook_free(softc->ipf_hk_v6_out);
}
#endif

static int
ipf_detach_instance(ipf_main_softc_t *softc)
{
	/*
	 * And no proxy modules loaded.
	 */
	if (softc->ipf_refcnt != 0)
		return DDI_FAILURE;
	/*
	 * If it didn't finish loading or is already
	 * unloading, fail.
	 */
	if (softc->ipf_running == -2)
		return DDI_FAILURE;

	/*
	 * Make sure we're the only one's modifying things.
	 * With this lock others should just fall out of
	 * the loop.
	 */
	WRITE_ENTER(&softc->ipf_global);
	if (softc->ipf_running == -2) {
		RWLOCK_EXIT(&softc->ipf_global);
		return DDI_FAILURE;
	}
	if (softc->ipf_running == 1)
		(void) ipfdetach(softc);
	softc->ipf_running = -2;

	RWLOCK_EXIT(&softc->ipf_global);

	if (softc->ipf_slow_ch != 0) {
		(void) untimeout(softc->ipf_slow_ch);
		softc->ipf_slow_ch = 0;
	}

	return DDI_SUCCESS;
}

