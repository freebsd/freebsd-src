/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(__sgi) && (IRIX > 602)
# include <sys/ptimers.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <stdio.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include "kmem.h"
#include "netinet/ip_compat.h"
#include "ipf.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_state.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf

ipstate_t *printstate(sp, opts)
ipstate_t *sp;
int opts;
{
	ipstate_t ips;

	if (kmemcpy((char *)&ips, (u_long)sp, sizeof(ips)))
		return NULL;

	PRINTF("%s -> ", hostname(ips.is_v, &ips.is_src.in4));
	PRINTF("%s ttl %ld pass %#x pr %d state %d/%d\n",
		hostname(ips.is_v, &ips.is_dst.in4),
		ips.is_age, ips.is_pass, ips.is_p,
		ips.is_state[0], ips.is_state[1]);
#ifdef	USE_QUAD_T
	PRINTF("\tpkts %qu bytes %qu", (unsigned long long) ips.is_pkts,
		(unsigned long long) ips.is_bytes);
#else
	PRINTF("\tpkts %ld bytes %ld", ips.is_pkts, ips.is_bytes);
#endif
	if (ips.is_p == IPPROTO_TCP)
#if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
(__FreeBSD_version >= 220000) || defined(__OpenBSD__)
		PRINTF("\t%hu -> %hu %x:%x %u<<%d:%u<<%d",
			ntohs(ips.is_sport), ntohs(ips.is_dport),
			ips.is_send, ips.is_dend,
			ips.is_maxswin>>ips.is_swscale, ips.is_swscale,
			ips.is_maxdwin>>ips.is_dwscale, ips.is_dwscale);
#else
		PRINTF("\t%hu -> %hu %x:%x %u<<%d:%u<<%d",
			ntohs(ips.is_sport), ntohs(ips.is_dport),
			ips.is_send, ips.is_dend,
			ips.is_maxswin>>ips.is_swscale, ips.is_swscale,
			ips.is_maxdwin>>ips.is_dwscale, ips.is_dwscale);
#endif
	else if (ips.is_p == IPPROTO_UDP)
		PRINTF(" %hu -> %hu", ntohs(ips.is_sport),
			ntohs(ips.is_dport));
	else if (ips.is_p == IPPROTO_ICMP
#ifdef	USE_INET6
		 || ips.is_p == IPPROTO_ICMPV6
#endif
		)
		PRINTF(" id %hu seq %hu type %d", ntohs(ips.is_icmp.ics_id),
			ntohs(ips.is_icmp.ics_seq), ips.is_icmp.ics_type);

	PRINTF("\n\t");

	/*
	 * Print out bits set in the result code for the state being
	 * kept as they would for a rule.
	 */
	if (ips.is_pass & FR_PASS) {
		PRINTF("pass");
	} else if (ips.is_pass & FR_BLOCK) {
		PRINTF("block");
		switch (ips.is_pass & FR_RETMASK)
		{
		case FR_RETICMP :
			PRINTF(" return-icmp");
			break;
		case FR_FAKEICMP :
			PRINTF(" return-icmp-as-dest");
			break;
		case FR_RETRST :
			PRINTF(" return-rst");
			break;
		default :
			break;
		}
	} else if ((ips.is_pass & FR_LOGMASK) == FR_LOG) {
			PRINTF("log");
		if (ips.is_pass & FR_LOGBODY)
			PRINTF(" body");
		if (ips.is_pass & FR_LOGFIRST)
			PRINTF(" first");
	} else if (ips.is_pass & FR_ACCOUNT)
		PRINTF("count");

	if (ips.is_pass & FR_OUTQUE)
		PRINTF(" out");
	else
		PRINTF(" in");

	if ((ips.is_pass & FR_LOG) != 0) {
		PRINTF(" log");
		if (ips.is_pass & FR_LOGBODY)
			PRINTF(" body");
		if (ips.is_pass & FR_LOGFIRST)
			PRINTF(" first");
		if (ips.is_pass & FR_LOGORBLOCK)
			PRINTF(" or-block");
	}
	if (ips.is_pass & FR_QUICK)
		PRINTF(" quick");
	if (ips.is_pass & FR_KEEPFRAG)
		PRINTF(" keep frags");
	/* a given; no? */
	if (ips.is_pass & FR_KEEPSTATE)
		PRINTF(" keep state");
	PRINTF("\tIPv%d", ips.is_v);
	PRINTF("\n");

	PRINTF("\tpkt_flags & %x(%x) = %x,\t",
		ips.is_flags & 0xf, ips.is_flags,
		ips.is_flags >> 4);
	PRINTF("\tpkt_options & %x = %x\n", ips.is_optmsk,
		ips.is_opt);
	PRINTF("\tpkt_security & %x = %x, pkt_auth & %x = %x\n",
		ips.is_secmsk, ips.is_sec, ips.is_authmsk,
		ips.is_auth);
	PRINTF("\tinterfaces: in %s", getifname(ips.is_ifp[0]));
	PRINTF(",%s", getifname(ips.is_ifp[1]));
	PRINTF(" out %s", getifname(ips.is_ifp[2]));
	PRINTF(",%s\n", getifname(ips.is_ifp[3]));

	return ips.is_next;
}
