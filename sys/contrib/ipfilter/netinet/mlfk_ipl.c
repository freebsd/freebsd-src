/*
 * Copyright 1999 Guido van Rooij.  All rights reserved.
 * 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
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
#include <netinet/ip.h>
#if (__FreeBSD_version >= 199511)
# include <net/route.h>
# include <netinet/ip_var.h>
# include <netinet/tcp.h>
# include <netinet/tcpip.h>
#endif


#include <netinet/ipl.h>
#include <netinet/ip_compat.h>
#include <netinet/ip_fil.h>
#include <netinet/ip_state.h>
#include <netinet/ip_nat.h>
#include <netinet/ip_auth.h>
#include <netinet/ip_frag.h>
#include <netinet/ip_proxy.h>

static dev_t ipf_devs[IPL_LOGMAX + 1];

SYSCTL_DECL(_net_inet);
SYSCTL_NODE(_net_inet, OID_AUTO, ipf, CTLFLAG_RW, 0, "IPF");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_flags, CTLFLAG_RW, &fr_flags, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_pass, CTLFLAG_RW, &fr_pass, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_active, CTLFLAG_RD, &fr_active, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcpidletimeout, CTLFLAG_RW,
	   &fr_tcpidletimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcpclosewait, CTLFLAG_RW,
	   &fr_tcpclosewait, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcplastack, CTLFLAG_RW,
	   &fr_tcplastack, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcptimeout, CTLFLAG_RW,
	   &fr_tcptimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcpclosed, CTLFLAG_RW,
	   &fr_tcpclosed, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_tcphalfclosed, CTLFLAG_RW,
	   &fr_tcphalfclosed, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_udptimeout, CTLFLAG_RW,
	   &fr_udptimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_udpacktimeout, CTLFLAG_RW,
	   &fr_udpacktimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_icmptimeout, CTLFLAG_RW,
	   &fr_icmptimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_icmpacktimeout, CTLFLAG_RW,
	   &fr_icmpacktimeout, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_defnatage, CTLFLAG_RW,
	   &fr_defnatage, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_ipfrttl, CTLFLAG_RW,
	   &fr_ipfrttl, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, ipl_unreach, CTLFLAG_RW,
	   &ipl_unreach, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_running, CTLFLAG_RD,
	   &fr_running, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_authsize, CTLFLAG_RD,
	   &fr_authsize, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_authused, CTLFLAG_RD,
	   &fr_authused, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_defaultauthage, CTLFLAG_RW,
	   &fr_defaultauthage, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_chksrc, CTLFLAG_RW, &fr_chksrc, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, ippr_ftp_pasvonly, CTLFLAG_RW,
	   &ippr_ftp_pasvonly, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_minttl, CTLFLAG_RW, &fr_minttl, 0, "");
SYSCTL_INT(_net_inet_ipf, OID_AUTO, fr_minttllog, CTLFLAG_RW,
	   &fr_minttllog, 0, "");

#define CDEV_MAJOR 79
static struct cdevsw ipl_cdevsw = {
	/* open */	iplopen,
	/* close */	iplclose,
	/* read */	iplread,
	/* write */	nowrite,
	/* ioctl */	iplioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ipl",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static int
ipfilter_modevent(module_t mod, int type, void *unused)
{
	char	*c;
	int	i, error = 0;

	switch (type) {
	case MOD_LOAD :

		error = iplattach();
		if (error)
			break;

		c = NULL;
		for(i=strlen(IPL_NAME); i>0; i--)
			if (IPL_NAME[i] == '/') {
				c = &IPL_NAME[i+1];
				break;
			}
		if (!c)
			c = IPL_NAME;
		ipf_devs[IPL_LOGIPF] =
		    make_dev(&ipl_cdevsw, IPL_LOGIPF, 0, 0, 0600, c);

		c = NULL;
		for(i=strlen(IPL_NAT); i>0; i--)
			if (IPL_NAT[i] == '/') {
				c = &IPL_NAT[i+1];
				break;
			}
		if (!c)
			c = IPL_NAT;
		ipf_devs[IPL_LOGNAT] =
		    make_dev(&ipl_cdevsw, IPL_LOGNAT, 0, 0, 0600, c);

		c = NULL;
		for(i=strlen(IPL_STATE); i>0; i--)
			if (IPL_STATE[i] == '/') {
				c = &IPL_STATE[i+1];
				break;
			}
		if (!c)
			c = IPL_STATE;
		ipf_devs[IPL_LOGSTATE] =
		    make_dev(&ipl_cdevsw, IPL_LOGSTATE, 0, 0, 0600, c);

		c = NULL;
		for(i=strlen(IPL_AUTH); i>0; i--)
			if (IPL_AUTH[i] == '/') {
				c = &IPL_AUTH[i+1];
				break;
			}
		if (!c)
			c = IPL_AUTH;
		ipf_devs[IPL_LOGAUTH] =
		    make_dev(&ipl_cdevsw, IPL_LOGAUTH, 0, 0, 0600, c);

		break;
	case MOD_UNLOAD :
		destroy_dev(ipf_devs[IPL_LOGIPF]);
		destroy_dev(ipf_devs[IPL_LOGNAT]);
		destroy_dev(ipf_devs[IPL_LOGSTATE]);
		destroy_dev(ipf_devs[IPL_LOGAUTH]);
		error = ipldetach();
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static moduledata_t ipfiltermod = {
	IPL_VERSION,
	ipfilter_modevent,
        0
};
DECLARE_MODULE(ipfilter, ipfiltermod, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
