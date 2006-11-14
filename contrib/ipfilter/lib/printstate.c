/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"
#include "kmem.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf

ipstate_t *printstate(sp, opts, now)
ipstate_t *sp;
int opts;
u_long now;
{
	ipstate_t ips;
	synclist_t ipsync;

	if (kmemcpy((char *)&ips, (u_long)sp, sizeof(ips)))
		return NULL;

	PRINTF("%s -> ", hostname(ips.is_v, &ips.is_src.in4));
	PRINTF("%s pass %#x pr %d state %d/%d bkt %d\n",
		hostname(ips.is_v, &ips.is_dst.in4), ips.is_pass, ips.is_p,
		ips.is_state[0], ips.is_state[1], ips.is_hv);
	PRINTF("\ttag %u ttl %lu", ips.is_tag, ips.is_die - now);

	if (ips.is_p == IPPROTO_TCP) {
		PRINTF("\n\t%hu -> %hu %x:%x %hu<<%d:%hu<<%d\n",
			ntohs(ips.is_sport), ntohs(ips.is_dport),
			ips.is_send, ips.is_dend,
			ips.is_maxswin, ips.is_swinscale,
			ips.is_maxdwin, ips.is_dwinscale);
		PRINTF("\tcmsk %04x smsk %04x isc %p s0 %08x/%08x\n",
			ips.is_smsk[0], ips.is_smsk[1], ips.is_isc,
			ips.is_s0[0], ips.is_s0[1]);
		PRINTF("\tFWD:ISN inc %x sumd %x\n",
			ips.is_isninc[0], ips.is_sumd[0]);
		PRINTF("\tREV:ISN inc %x sumd %x\n",
			ips.is_isninc[1], ips.is_sumd[1]);
#ifdef	IPFILTER_SCAN
		PRINTF("\tsbuf[0] [");
		printsbuf(ips.is_sbuf[0]);
		PRINTF("] sbuf[1] [");
		printsbuf(ips.is_sbuf[1]);
		PRINTF("]\n");
#endif
	} else if (ips.is_p == IPPROTO_UDP) {
		PRINTF(" %hu -> %hu\n", ntohs(ips.is_sport),
			ntohs(ips.is_dport));
	} else if (ips.is_p == IPPROTO_GRE) {
		PRINTF(" call %hx/%hx\n", ntohs(ips.is_gre.gs_call[0]),
		       ntohs(ips.is_gre.gs_call[1]));
	} else if (ips.is_p == IPPROTO_ICMP
#ifdef	USE_INET6
		 || ips.is_p == IPPROTO_ICMPV6
#endif
		)
		PRINTF(" id %hu seq %hu type %d\n", ips.is_icmp.ici_id,
			ips.is_icmp.ici_seq, ips.is_icmp.ici_type);

#ifdef        USE_QUAD_T
	PRINTF("\tforward: pkts in %lld bytes in %lld pkts out %lld bytes out %lld\n\tbackward: pkts in %lld bytes in %lld pkts out %lld bytes out %lld\n",
		ips.is_pkts[0], ips.is_bytes[0],
		ips.is_pkts[1], ips.is_bytes[1],
		ips.is_pkts[2], ips.is_bytes[2],
		ips.is_pkts[3], ips.is_bytes[3]);
#else
	PRINTF("\tforward: pkts in %ld bytes in %ld pkts out %ld bytes out %ld\n\tbackward: pkts in %ld bytes in %ld pkts out %ld bytes out %ld\n",
		ips.is_pkts[0], ips.is_bytes[0],
		ips.is_pkts[1], ips.is_bytes[1],
		ips.is_pkts[2], ips.is_bytes[2],
		ips.is_pkts[3], ips.is_bytes[3]);
#endif

	PRINTF("\t");

	/*
	 * Print out bits set in the result code for the state being
	 * kept as they would for a rule.
	 */
	if (FR_ISPASS(ips.is_pass)) {
		PRINTF("pass");
	} else if (FR_ISBLOCK(ips.is_pass)) {
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
	} else if (FR_ISACCOUNT(ips.is_pass)) {
		PRINTF("count");
	} else if (FR_ISPREAUTH(ips.is_pass)) {
		PRINTF("preauth");
	} else if (FR_ISAUTH(ips.is_pass))
		PRINTF("auth");

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
	if (ips.is_pass & FR_KEEPSTATE) {
		PRINTF(" keep state");
		if (ips.is_pass & FR_STATESYNC)	
			PRINTF(" ( sync )");
	}
	PRINTF("\tIPv%d", ips.is_v);
	PRINTF("\n");

	PRINTF("\tpkt_flags & %x(%x) = %x,\t",
		ips.is_flags & 0xf, ips.is_flags,
		ips.is_flags >> 4);
	PRINTF("\tpkt_options & %x = %x, %x = %x \n", ips.is_optmsk[0],
		ips.is_opt[0], ips.is_optmsk[1], ips.is_opt[1]);
	PRINTF("\tpkt_security & %x = %x, pkt_auth & %x = %x\n",
		ips.is_secmsk, ips.is_sec, ips.is_authmsk,
		ips.is_auth);
	PRINTF("\tis_flx %#x %#x %#x %#x\n", ips.is_flx[0][0], ips.is_flx[0][1],
	       ips.is_flx[1][0], ips.is_flx[1][1]);
	PRINTF("\tinterfaces: in %s[%s", getifname(ips.is_ifp[0]),
		ips.is_ifname[0]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", ips.is_ifp[0]);
	putchar(']');
	PRINTF(",%s[%s", getifname(ips.is_ifp[1]), ips.is_ifname[1]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", ips.is_ifp[1]);
	putchar(']');
	PRINTF(" out %s[%s", getifname(ips.is_ifp[2]), ips.is_ifname[2]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", ips.is_ifp[2]);
	putchar(']');
	PRINTF(",%s[%s", getifname(ips.is_ifp[3]), ips.is_ifname[3]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", ips.is_ifp[3]);
	PRINTF("]\n");

	if (ips.is_sync != NULL) {

		if (kmemcpy((char *)&ipsync, (u_long)ips.is_sync, sizeof(ipsync))) {
	
			PRINTF("\tSync status: status could not be retrieved\n");
			return NULL;
		}

		PRINTF("\tSync status: idx %d num %d v %d pr %d rev %d\n",
			ipsync.sl_idx, ipsync.sl_num, ipsync.sl_v,
			ipsync.sl_p, ipsync.sl_rev);
		
	} else {
		PRINTF("\tSync status: not synchronized\n");
	}

	return ips.is_next;
}
