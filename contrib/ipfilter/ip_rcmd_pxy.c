/*	$NetBSD$	*/

/*
 * Copyright (C) 1998-2003 by Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: ip_rcmd_pxy.c,v 1.41.2.4 2005/02/04 10:22:55 darrenr Exp
 *
 * Simple RCMD transparent proxy for in-kernel use.  For use with the NAT
 * code.
 */

#define	IPF_RCMD_PROXY


int ippr_rcmd_init __P((void));
void ippr_rcmd_fini __P((void));
int ippr_rcmd_new __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_rcmd_out __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_rcmd_in __P((fr_info_t *, ap_session_t *, nat_t *));
u_short ipf_rcmd_atoi __P((char *));
int ippr_rcmd_portmsg __P((fr_info_t *, ap_session_t *, nat_t *));

static	frentry_t	rcmdfr;

int	rcmd_proxy_init = 0;


/*
 * RCMD application proxy initialization.
 */
int ippr_rcmd_init()
{
	bzero((char *)&rcmdfr, sizeof(rcmdfr));
	rcmdfr.fr_ref = 1;
	rcmdfr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	MUTEX_INIT(&rcmdfr.fr_lock, "RCMD proxy rule lock");
	rcmd_proxy_init = 1;

	return 0;
}


void ippr_rcmd_fini()
{
	if (rcmd_proxy_init == 1) {
		MUTEX_DESTROY(&rcmdfr.fr_lock);
		rcmd_proxy_init = 0;
	}
}


/*
 * Setup for a new RCMD proxy.
 */
int ippr_rcmd_new(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;

	fin = fin;	/* LINT */
	nat = nat;	/* LINT */

	aps->aps_psiz = sizeof(u_32_t);
	KMALLOCS(aps->aps_data, u_32_t *, sizeof(u_32_t));
	if (aps->aps_data == NULL) {
#ifdef IP_RCMD_PROXY_DEBUG
		printf("ippr_rcmd_new:KMALLOCS(%d) failed\n", sizeof(u_32_t));
#endif
		return -1;
	}
	*(u_32_t *)aps->aps_data = 0;
	aps->aps_sport = tcp->th_sport;
	aps->aps_dport = tcp->th_dport;
	return 0;
}


/*
 * ipf_rcmd_atoi - implement a simple version of atoi
 */
u_short ipf_rcmd_atoi(ptr)
char *ptr;
{
	register char *s = ptr, c;
	register u_short i = 0;

	while (((c = *s++) != '\0') && ISDIGIT(c)) {
		i *= 10;
		i += c - '0';
	}
	return i;
}


int ippr_rcmd_portmsg(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	struct in_addr swip, swip2;
	int off, dlen, nflags;
	char portbuf[8], *s;
	fr_info_t fi;
	u_short sp;
	nat_t *nat2;
	ip_t *ip;
	mb_t *m;

	tcp = (tcphdr_t *)fin->fin_dp;

	if (tcp->th_flags & TH_SYN) {
		*(u_32_t *)aps->aps_data = htonl(ntohl(tcp->th_seq) + 1);
		return 0;
	}

	if ((*(u_32_t *)aps->aps_data != 0) &&
	    (tcp->th_seq != *(u_32_t *)aps->aps_data))
		return 0;

	m = fin->fin_m;
	ip = fin->fin_ip;
	off = (char *)tcp - (char *)ip + (TCP_OFF(tcp) << 2) + fin->fin_ipoff;

#ifdef __sgi
	dlen = fin->fin_plen - off;
#else
	dlen = MSGDSIZE(m) - off;
#endif
	if (dlen <= 0)
		return 0;

	bzero(portbuf, sizeof(portbuf));
	COPYDATA(m, off, MIN(sizeof(portbuf), dlen), portbuf);

	portbuf[sizeof(portbuf) - 1] = '\0';
	s = portbuf;
	sp = ipf_rcmd_atoi(s);
	if (sp == 0) {
#ifdef IP_RCMD_PROXY_DEBUG
		printf("ippr_rcmd_portmsg:sp == 0 dlen %d [%s]\n",
		       dlen, portbuf);
#endif
		return 0;
	}

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_flx |= FI_IGNORE;
	fi.fin_data[0] = sp;
	fi.fin_data[1] = 0;
	if (nat->nat_dir == NAT_OUTBOUND)
		nat2 = nat_outlookup(&fi, NAT_SEARCH|IPN_TCP, nat->nat_p,
				     nat->nat_inip, nat->nat_oip);
	else
		nat2 = nat_inlookup(&fi, NAT_SEARCH|IPN_TCP, nat->nat_p,
				    nat->nat_inip, nat->nat_oip);
	if (nat2 == NULL) {
		int slen;

		slen = ip->ip_len;
		ip->ip_len = fin->fin_hlen + sizeof(*tcp);
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = htons(sp);
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		TCP_OFF_A(tcp2, 5);
		tcp2->th_flags = TH_SYN;
		fi.fin_dp = (char *)tcp2;
		fi.fin_fr = &rcmdfr;
		fi.fin_dlen = sizeof(*tcp2);
		fi.fin_plen = fi.fin_hlen + sizeof(*tcp2);
		fi.fin_flx &= FI_LOWTTL|FI_FRAG|FI_TCPUDP|FI_OPTIONS|FI_IGNORE;
		nflags = NAT_SLAVE|IPN_TCP|SI_W_DPORT;

		swip = ip->ip_src;
		swip2 = ip->ip_dst;

		if (nat->nat_dir == NAT_OUTBOUND) {
			fi.fin_fi.fi_saddr = nat->nat_inip.s_addr;
			ip->ip_src = nat->nat_inip;
		} else {
			fi.fin_fi.fi_saddr = nat->nat_oip.s_addr;
			ip->ip_src = nat->nat_oip;
			nflags |= NAT_NOTRULEPORT;
		}

		nat2 = nat_new(&fi, nat->nat_ptr, NULL, nflags, nat->nat_dir);

		if (nat2 != NULL) {
			(void) nat_proto(&fi, nat2, IPN_TCP);
			nat_update(&fi, nat2, nat2->nat_ptr);
			fi.fin_ifp = NULL;
			if (nat->nat_dir == NAT_INBOUND) {
				fi.fin_fi.fi_daddr = nat->nat_inip.s_addr;
				ip->ip_dst = nat->nat_inip;
			}
			(void) fr_addstate(&fi, &nat2->nat_state, SI_W_DPORT);
			if (fi.fin_state != NULL)
				fr_statederef(&fi, (ipstate_t **)&fi.fin_state);
		}
		ip->ip_len = slen;
		ip->ip_src = swip;
		ip->ip_dst = swip2;
	}
	return 0;
}


int ippr_rcmd_out(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	if (nat->nat_dir == NAT_OUTBOUND)
		return ippr_rcmd_portmsg(fin, aps, nat);
	return 0;
}


int ippr_rcmd_in(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	if (nat->nat_dir == NAT_INBOUND)
		return ippr_rcmd_portmsg(fin, aps, nat);
	return 0;
}
