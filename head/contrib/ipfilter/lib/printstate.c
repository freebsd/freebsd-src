/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
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
	synclist_t ipsync;

	if (sp->is_phnext == NULL)
		PRINTF("ORPHAN ");
	PRINTF("%s -> ", hostname(sp->is_v, &sp->is_src.in4));
	PRINTF("%s pass %#x pr %d state %d/%d",
		hostname(sp->is_v, &sp->is_dst.in4), sp->is_pass, sp->is_p,
		sp->is_state[0], sp->is_state[1]);
	if (opts & OPT_DEBUG)
		PRINTF(" bkt %d ref %d", sp->is_hv, sp->is_ref);
	PRINTF("\n\ttag %u ttl %lu", sp->is_tag, sp->is_die - now);

	if (sp->is_p == IPPROTO_TCP) {
		PRINTF("\n\t%hu -> %hu %x:%x %hu<<%d:%hu<<%d\n",
			ntohs(sp->is_sport), ntohs(sp->is_dport),
			sp->is_send, sp->is_dend,
			sp->is_maxswin, sp->is_swinscale,
			sp->is_maxdwin, sp->is_dwinscale);
		PRINTF("\tcmsk %04x smsk %04x s0 %08x/%08x\n",
			sp->is_smsk[0], sp->is_smsk[1],
			sp->is_s0[0], sp->is_s0[1]);
		PRINTF("\tFWD:ISN inc %x sumd %x\n",
			sp->is_isninc[0], sp->is_sumd[0]);
		PRINTF("\tREV:ISN inc %x sumd %x\n",
			sp->is_isninc[1], sp->is_sumd[1]);
#ifdef	IPFILTER_SCAN
		PRINTF("\tsbuf[0] [");
		printsbuf(sp->is_sbuf[0]);
		PRINTF("] sbuf[1] [");
		printsbuf(sp->is_sbuf[1]);
		PRINTF("]\n");
#endif
	} else if (sp->is_p == IPPROTO_UDP) {
		PRINTF(" %hu -> %hu\n", ntohs(sp->is_sport),
			ntohs(sp->is_dport));
	} else if (sp->is_p == IPPROTO_GRE) {
		PRINTF(" call %hx/%hx\n", ntohs(sp->is_gre.gs_call[0]),
		       ntohs(sp->is_gre.gs_call[1]));
	} else if (sp->is_p == IPPROTO_ICMP
#ifdef	USE_INET6
		 || sp->is_p == IPPROTO_ICMPV6
#endif
		)
		PRINTF(" id %hu seq %hu type %d\n", sp->is_icmp.ici_id,
			sp->is_icmp.ici_seq, sp->is_icmp.ici_type);

#ifdef        USE_QUAD_T
	PRINTF("\tforward: pkts in %lld bytes in %lld pkts out %lld bytes out %lld\n\tbackward: pkts in %lld bytes in %lld pkts out %lld bytes out %lld\n",
		sp->is_pkts[0], sp->is_bytes[0],
		sp->is_pkts[1], sp->is_bytes[1],
		sp->is_pkts[2], sp->is_bytes[2],
		sp->is_pkts[3], sp->is_bytes[3]);
#else
	PRINTF("\tforward: pkts in %ld bytes in %ld pkts out %ld bytes out %ld\n\tbackward: pkts in %ld bytes in %ld pkts out %ld bytes out %ld\n",
		sp->is_pkts[0], sp->is_bytes[0],
		sp->is_pkts[1], sp->is_bytes[1],
		sp->is_pkts[2], sp->is_bytes[2],
		sp->is_pkts[3], sp->is_bytes[3]);
#endif

	PRINTF("\t");

	/*
	 * Print out bits set in the result code for the state being
	 * kept as they would for a rule.
	 */
	if (FR_ISPASS(sp->is_pass)) {
		PRINTF("pass");
	} else if (FR_ISBLOCK(sp->is_pass)) {
		PRINTF("block");
		switch (sp->is_pass & FR_RETMASK)
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
	} else if ((sp->is_pass & FR_LOGMASK) == FR_LOG) {
			PRINTF("log");
		if (sp->is_pass & FR_LOGBODY)
			PRINTF(" body");
		if (sp->is_pass & FR_LOGFIRST)
			PRINTF(" first");
	} else if (FR_ISACCOUNT(sp->is_pass)) {
		PRINTF("count");
	} else if (FR_ISPREAUTH(sp->is_pass)) {
		PRINTF("preauth");
	} else if (FR_ISAUTH(sp->is_pass))
		PRINTF("auth");

	if (sp->is_pass & FR_OUTQUE)
		PRINTF(" out");
	else
		PRINTF(" in");

	if ((sp->is_pass & FR_LOG) != 0) {
		PRINTF(" log");
		if (sp->is_pass & FR_LOGBODY)
			PRINTF(" body");
		if (sp->is_pass & FR_LOGFIRST)
			PRINTF(" first");
		if (sp->is_pass & FR_LOGORBLOCK)
			PRINTF(" or-block");
	}
	if (sp->is_pass & FR_QUICK)
		PRINTF(" quick");
	if (sp->is_pass & FR_KEEPFRAG)
		PRINTF(" keep frags");
	/* a given; no? */
	if (sp->is_pass & FR_KEEPSTATE) {
		PRINTF(" keep state");
		if (sp->is_pass & FR_STATESYNC)	
			PRINTF(" ( sync )");
	}
	PRINTF("\tIPv%d", sp->is_v);
	PRINTF("\n");

	PRINTF("\tpkt_flags & %x(%x) = %x,\t",
		sp->is_flags & 0xf, sp->is_flags,
		sp->is_flags >> 4);
	PRINTF("\tpkt_options & %x = %x, %x = %x \n", sp->is_optmsk[0],
		sp->is_opt[0], sp->is_optmsk[1], sp->is_opt[1]);
	PRINTF("\tpkt_security & %x = %x, pkt_auth & %x = %x\n",
		sp->is_secmsk, sp->is_sec, sp->is_authmsk,
		sp->is_auth);
	PRINTF("\tis_flx %#x %#x %#x %#x\n", sp->is_flx[0][0], sp->is_flx[0][1],
	       sp->is_flx[1][0], sp->is_flx[1][1]);
	PRINTF("\tinterfaces: in %s[%s", getifname(sp->is_ifp[0]),
		sp->is_ifname[0]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", sp->is_ifp[0]);
	putchar(']');
	PRINTF(",%s[%s", getifname(sp->is_ifp[1]), sp->is_ifname[1]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", sp->is_ifp[1]);
	putchar(']');
	PRINTF(" out %s[%s", getifname(sp->is_ifp[2]), sp->is_ifname[2]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", sp->is_ifp[2]);
	putchar(']');
	PRINTF(",%s[%s", getifname(sp->is_ifp[3]), sp->is_ifname[3]);
	if (opts & OPT_DEBUG)
		PRINTF("/%p", sp->is_ifp[3]);
	PRINTF("]\n");

	if (sp->is_sync != NULL) {

		if (kmemcpy((char *)&ipsync, (u_long)sp->is_sync, sizeof(ipsync))) {
	
			PRINTF("\tSync status: status could not be retrieved\n");
			return NULL;
		}

		PRINTF("\tSync status: idx %d num %d v %d pr %d rev %d\n",
			ipsync.sl_idx, ipsync.sl_num, ipsync.sl_v,
			ipsync.sl_p, ipsync.sl_rev);
		
	} else {
		PRINTF("\tSync status: not synchronized\n");
	}

	return sp->is_next;
}
