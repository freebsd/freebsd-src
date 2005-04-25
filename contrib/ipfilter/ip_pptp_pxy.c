/*	$NetBSD$	*/

/*
 * Copyright (C) 2002-2003 by Darren Reed
 *
 * Simple PPTP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 *
 * Id: ip_pptp_pxy.c,v 2.10.2.9 2005/03/16 18:17:34 darrenr Exp
 *
 */
#define	IPF_PPTP_PROXY

typedef	struct pptp_hdr {
	u_short	pptph_len;
	u_short	pptph_type;
	u_32_t	pptph_cookie;
} pptp_hdr_t;

#define	PPTP_MSGTYPE_CTL	1
#define	PPTP_MTCTL_STARTREQ	1
#define	PPTP_MTCTL_STARTREP	2
#define	PPTP_MTCTL_STOPREQ	3
#define	PPTP_MTCTL_STOPREP	4
#define	PPTP_MTCTL_ECHOREQ	5
#define	PPTP_MTCTL_ECHOREP	6
#define	PPTP_MTCTL_OUTREQ	7
#define	PPTP_MTCTL_OUTREP	8
#define	PPTP_MTCTL_INREQ	9
#define	PPTP_MTCTL_INREP	10
#define	PPTP_MTCTL_INCONNECT	11
#define	PPTP_MTCTL_CLEAR	12
#define	PPTP_MTCTL_DISCONNECT	13
#define	PPTP_MTCTL_WANERROR	14
#define	PPTP_MTCTL_LINKINFO	15


int ippr_pptp_init __P((void));
void ippr_pptp_fini __P((void));
int ippr_pptp_new __P((fr_info_t *, ap_session_t *, nat_t *));
void ippr_pptp_del __P((ap_session_t *));
int ippr_pptp_inout __P((fr_info_t *, ap_session_t *, nat_t *));
void ippr_pptp_donatstate __P((fr_info_t *, nat_t *, pptp_pxy_t *));
int ippr_pptp_message __P((fr_info_t *, nat_t *, pptp_pxy_t *, pptp_side_t *));
int ippr_pptp_nextmessage __P((fr_info_t *, nat_t *, pptp_pxy_t *, int));
int ippr_pptp_mctl __P((fr_info_t *, nat_t *, pptp_pxy_t *, pptp_side_t *));

static	frentry_t	pptpfr;

int	pptp_proxy_init = 0;
int	ippr_pptp_debug = 0;
int	ippr_pptp_gretimeout = IPF_TTLVAL(120);	/* 2 minutes */


/*
 * PPTP application proxy initialization.
 */
int ippr_pptp_init()
{
	bzero((char *)&pptpfr, sizeof(pptpfr));
	pptpfr.fr_ref = 1;
	pptpfr.fr_age[0] = ippr_pptp_gretimeout;
	pptpfr.fr_age[1] = ippr_pptp_gretimeout;
	pptpfr.fr_flags = FR_OUTQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	MUTEX_INIT(&pptpfr.fr_lock, "PPTP proxy rule lock");
	pptp_proxy_init = 1;

	return 0;
}


void ippr_pptp_fini()
{
	if (pptp_proxy_init == 1) {
		MUTEX_DESTROY(&pptpfr.fr_lock);
		pptp_proxy_init = 0;
	}
}


/*
 * Setup for a new PPTP proxy.
 */
int ippr_pptp_new(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	pptp_pxy_t *pptp;
	ipnat_t *ipn;
	ip_t *ip;
	int off;

	ip = fin->fin_ip;
	off = fin->fin_hlen + sizeof(udphdr_t);

	if (nat_outlookup(fin, 0, IPPROTO_GRE, nat->nat_inip,
			  ip->ip_dst) != NULL) {
		if (ippr_pptp_debug > 0)
			printf("ippr_pptp_new: GRE session already exists\n");
		return -1;
	}

	aps->aps_psiz = sizeof(*pptp);
	KMALLOCS(aps->aps_data, pptp_pxy_t *, sizeof(*pptp));
	if (aps->aps_data == NULL) {
		if (ippr_pptp_debug > 0)
			printf("ippr_pptp_new: malloc for aps_data failed\n");
		return -1;
	}

	/*
	 * Create NAT rule against which the tunnel/transport mapping is
	 * created.  This is required because the current NAT rule does not
	 * describe GRE but TCP instead.
	 */
	pptp = aps->aps_data;
	bzero((char *)pptp, sizeof(*pptp));
	ipn = &pptp->pptp_rule;
	ipn->in_ifps[0] = fin->fin_ifp;
	ipn->in_apr = NULL;
	ipn->in_use = 1;
	ipn->in_hits = 1;
	ipn->in_ippip = 1;
	if (nat->nat_dir == NAT_OUTBOUND) {
		ipn->in_nip = ntohl(nat->nat_outip.s_addr);
		ipn->in_outip = fin->fin_saddr;
		ipn->in_redir = NAT_MAP;
	} else if (nat->nat_dir == NAT_INBOUND) {
		ipn->in_nip = 0;
		ipn->in_outip = nat->nat_outip.s_addr;
		ipn->in_redir = NAT_REDIRECT;
	}
	ipn->in_inip = nat->nat_inip.s_addr;
	ipn->in_inmsk = 0xffffffff;
	ipn->in_outmsk = 0xffffffff;
	ipn->in_srcip = fin->fin_saddr;
	ipn->in_srcmsk = 0xffffffff;
	bcopy(nat->nat_ptr->in_ifnames[0], ipn->in_ifnames[0],
	      sizeof(ipn->in_ifnames[0]));
	ipn->in_p = IPPROTO_GRE;

	pptp->pptp_side[0].pptps_wptr = pptp->pptp_side[0].pptps_buffer;
	pptp->pptp_side[1].pptps_wptr = pptp->pptp_side[1].pptps_buffer;
	return 0;
}


void ippr_pptp_donatstate(fin, nat, pptp)
fr_info_t *fin;
nat_t *nat;
pptp_pxy_t *pptp;
{
	fr_info_t fi;
	grehdr_t gre;
	nat_t *nat2;
	u_char p;
	ip_t *ip;

	ip = fin->fin_ip;
	p = ip->ip_p;

	nat2 = pptp->pptp_nat;
	if ((nat2 == NULL) || (pptp->pptp_state == NULL)) {
		bcopy((char *)fin, (char *)&fi, sizeof(fi));
		bzero((char *)&gre, sizeof(gre));
		fi.fin_state = NULL;
		fi.fin_nat = NULL;
		fi.fin_fi.fi_p = IPPROTO_GRE;
		fi.fin_fr = &pptpfr;
		if ((nat->nat_dir == NAT_OUTBOUND && fin->fin_out) ||
		    (nat->nat_dir == NAT_INBOUND && !fin->fin_out)) {
			fi.fin_data[0] = pptp->pptp_call[0];
			fi.fin_data[1] = pptp->pptp_call[1];
		} else {
			fi.fin_data[0] = pptp->pptp_call[1];
			fi.fin_data[1] = pptp->pptp_call[0];
		}
		ip = fin->fin_ip;
		ip->ip_p = IPPROTO_GRE;
		fi.fin_flx &= ~(FI_TCPUDP|FI_STATE|FI_FRAG);
		fi.fin_flx |= FI_IGNORE;
		fi.fin_dp = &gre;
		gre.gr_flags = htons(1 << 13);
		if (fin->fin_out && nat->nat_dir == NAT_INBOUND) {
			fi.fin_fi.fi_saddr = fin->fin_fi.fi_daddr;
			fi.fin_fi.fi_daddr = nat->nat_outip.s_addr;
		} else if (!fin->fin_out && nat->nat_dir == NAT_OUTBOUND) {
			fi.fin_fi.fi_saddr = nat->nat_inip.s_addr;
			fi.fin_fi.fi_daddr = fin->fin_fi.fi_saddr;
		}
	}

	/*
	 * Update NAT timeout/create NAT if missing.
	 */
	if (nat2 != NULL)
		fr_queueback(&nat2->nat_tqe);
	else {
		nat2 = nat_new(&fi, &pptp->pptp_rule, &pptp->pptp_nat,
			       NAT_SLAVE, nat->nat_dir);
		pptp->pptp_nat = nat2;
		if (nat2 != NULL) {
			(void) nat_proto(&fi, nat2, 0);
			nat_update(&fi, nat2, nat2->nat_ptr);
		}
	}

	READ_ENTER(&ipf_state);
	if (pptp->pptp_state != NULL) {
		fr_queueback(&pptp->pptp_state->is_sti);
		RWLOCK_EXIT(&ipf_state);
	} else {
		RWLOCK_EXIT(&ipf_state);
		if (nat->nat_dir == NAT_INBOUND)
			fi.fin_fi.fi_daddr = nat2->nat_inip.s_addr;
		else
			fi.fin_fi.fi_saddr = nat2->nat_inip.s_addr;
		fi.fin_ifp = NULL;
		pptp->pptp_state = fr_addstate(&fi, &pptp->pptp_state,
					       0);
		if (fi.fin_state != NULL)
			fr_statederef(&fi, (ipstate_t **)&fi.fin_state);
	}
	ip->ip_p = p;
	return;
}


/*
 * Try and build up the next PPTP message in the TCP stream and if we can
 * build it up completely (fits in our buffer) then pass it off to the message
 * parsing function.
 */
int ippr_pptp_nextmessage(fin, nat, pptp, rev)
fr_info_t *fin;
nat_t *nat;
pptp_pxy_t *pptp;
int rev;
{
	static char *funcname = "ippr_pptp_nextmessage";
	pptp_side_t *pptps;
	u_32_t start, end;
	pptp_hdr_t *hdr;
	tcphdr_t *tcp;
	int dlen, off;
	u_short len;
	char *msg;

	tcp = fin->fin_dp;
	dlen = fin->fin_dlen - (TCP_OFF(tcp) << 2);
	start = ntohl(tcp->th_seq);
	pptps = &pptp->pptp_side[rev];
	off = (char *)tcp - (char *)fin->fin_ip + (TCP_OFF(tcp) << 2) +
	      fin->fin_ipoff;

	if (dlen <= 0)
		return 0;
	/*
	 * If the complete data packet is before what we expect to see
	 * "next", just ignore it as the chances are we've already seen it.
	 * The next if statement following this one really just causes packets
	 * ahead of what we've seen to be dropped, implying that something in
	 * the middle went missing and we want to see that first.
	 */
	end = start + dlen;
	if (pptps->pptps_next > end && pptps->pptps_next > start)
		return 0;

	if (pptps->pptps_next != start) {
		if (ippr_pptp_debug > 5)
			printf("%s: next (%x) != start (%x)\n", funcname,
				pptps->pptps_next, start);
		return -1;
	}

	msg = (char *)fin->fin_dp + (TCP_OFF(tcp) << 2);

	while (dlen > 0) {
		off += pptps->pptps_bytes;
		if (pptps->pptps_gothdr == 0) {
			/*
			 * PPTP has an 8 byte header that inclues the cookie.
			 * The start of every message should include one and
			 * it should match 1a2b3c4d.  Byte order is ignored,
			 * deliberately, when printing out the error.
			 */
			len = MIN(8 - pptps->pptps_bytes, dlen);
			COPYDATA(fin->fin_m, off, len, pptps->pptps_wptr);
			pptps->pptps_bytes += len;
			pptps->pptps_wptr += len;
			hdr = (pptp_hdr_t *)pptps->pptps_buffer;
			if (pptps->pptps_bytes == 8) {
				pptps->pptps_next += 8;
				if (ntohl(hdr->pptph_cookie) != 0x1a2b3c4d) {
					if (ippr_pptp_debug > 1)
						printf("%s: bad cookie (%x)\n",
						       funcname,
						       hdr->pptph_cookie);
					return -1;
				}
			}
			dlen -= len;
			msg += len;
			off += len;

			pptps->pptps_gothdr = 1;
			len = ntohs(hdr->pptph_len);
			pptps->pptps_len = len;
			pptps->pptps_nexthdr += len;

			/*
			 * If a message is too big for the buffer, just set
			 * the fields for the next message to come along.
			 * The messages defined in RFC 2637 will not exceed
			 * 512 bytes (in total length) so this is likely a
			 * bad data packet, anyway.
			 */
			if (len > sizeof(pptps->pptps_buffer)) {
				if (ippr_pptp_debug > 3)
					printf("%s: message too big (%d)\n",
					       funcname, len);
				pptps->pptps_next = pptps->pptps_nexthdr;
				pptps->pptps_wptr = pptps->pptps_buffer;
				pptps->pptps_gothdr = 0;
				pptps->pptps_bytes = 0;
				pptps->pptps_len = 0;
				break;
			}
		}

		len = MIN(pptps->pptps_len - pptps->pptps_bytes, dlen);
		COPYDATA(fin->fin_m, off, len, pptps->pptps_wptr);
		pptps->pptps_bytes += len;
		pptps->pptps_wptr += len;
		pptps->pptps_next += len;

		if (pptps->pptps_len > pptps->pptps_bytes)
			break;

		ippr_pptp_message(fin, nat, pptp, pptps);
		pptps->pptps_wptr = pptps->pptps_buffer;
		pptps->pptps_gothdr = 0;
		pptps->pptps_bytes = 0;
		pptps->pptps_len = 0;

		start += len;
		msg += len;
		dlen -= len;
	}

	return 0;
}


/*
 * handle a complete PPTP message
 */
int ippr_pptp_message(fin, nat, pptp, pptps)
fr_info_t *fin;
nat_t *nat;
pptp_pxy_t *pptp;
pptp_side_t *pptps;
{
	pptp_hdr_t *hdr = (pptp_hdr_t *)pptps->pptps_buffer;

	switch (ntohs(hdr->pptph_type))
	{
	case PPTP_MSGTYPE_CTL :
		ippr_pptp_mctl(fin, nat, pptp, pptps);
		break;

	default :
		break;
	}
	return 0;
}


/*
 * handle a complete PPTP control message
 */
int ippr_pptp_mctl(fin, nat, pptp, pptps)
fr_info_t *fin;
nat_t *nat;
pptp_pxy_t *pptp;
pptp_side_t *pptps;
{
	u_short *buffer = (u_short *)(pptps->pptps_buffer);
	pptp_side_t *pptpo;

	if (pptps == &pptp->pptp_side[0])
		pptpo = &pptp->pptp_side[1];
	else
		pptpo = &pptp->pptp_side[0];

	/*
	 * Breakout to handle all the various messages.  Most are just state
	 * transition.
	 */
	switch (ntohs(buffer[4]))
	{
	case PPTP_MTCTL_STARTREQ :
		pptps->pptps_state = PPTP_MTCTL_STARTREQ;
		break;
	case PPTP_MTCTL_STARTREP :
		if (pptpo->pptps_state == PPTP_MTCTL_STARTREQ)
			pptps->pptps_state = PPTP_MTCTL_STARTREP;
		break;
	case PPTP_MTCTL_STOPREQ :
		pptps->pptps_state = PPTP_MTCTL_STOPREQ;
		break;
	case PPTP_MTCTL_STOPREP :
		if (pptpo->pptps_state == PPTP_MTCTL_STOPREQ)
			pptps->pptps_state = PPTP_MTCTL_STOPREP;
		break;
	case PPTP_MTCTL_ECHOREQ :
		pptps->pptps_state = PPTP_MTCTL_ECHOREQ;
		break;
	case PPTP_MTCTL_ECHOREP :
		if (pptpo->pptps_state == PPTP_MTCTL_ECHOREQ)
			pptps->pptps_state = PPTP_MTCTL_ECHOREP;
		break;
	case PPTP_MTCTL_OUTREQ :
		pptps->pptps_state = PPTP_MTCTL_OUTREQ;
		break;
	case PPTP_MTCTL_OUTREP :
		if (pptpo->pptps_state == PPTP_MTCTL_OUTREQ) {
			pptps->pptps_state = PPTP_MTCTL_OUTREP;
			pptp->pptp_call[0] = buffer[7];
			pptp->pptp_call[1] = buffer[6];
			ippr_pptp_donatstate(fin, nat, pptp);
		}
		break;
	case PPTP_MTCTL_INREQ :
		pptps->pptps_state = PPTP_MTCTL_INREQ;
		break;
	case PPTP_MTCTL_INREP :
		if (pptpo->pptps_state == PPTP_MTCTL_INREQ) {
			pptps->pptps_state = PPTP_MTCTL_INREP;
			pptp->pptp_call[0] = buffer[7];
			pptp->pptp_call[1] = buffer[6];
			ippr_pptp_donatstate(fin, nat, pptp);
		}
		break;
	case PPTP_MTCTL_INCONNECT :
		pptps->pptps_state = PPTP_MTCTL_INCONNECT;
		break;
	case PPTP_MTCTL_CLEAR :
		pptps->pptps_state = PPTP_MTCTL_CLEAR;
		break;
	case PPTP_MTCTL_DISCONNECT :
		pptps->pptps_state = PPTP_MTCTL_DISCONNECT;
		break;
	case PPTP_MTCTL_WANERROR :
		pptps->pptps_state = PPTP_MTCTL_WANERROR;
		break;
	case PPTP_MTCTL_LINKINFO :
		pptps->pptps_state = PPTP_MTCTL_LINKINFO;
		break;
	}

	return 0;
}


/*
 * For outgoing PPTP packets.  refresh timeouts for NAT & state entries, if
 * we can.  If they have disappeared, recreate them.
 */
int ippr_pptp_inout(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	pptp_pxy_t *pptp;
	tcphdr_t *tcp;
	int rev;

	if ((fin->fin_out == 1) && (nat->nat_dir == NAT_INBOUND))
		rev = 1;
	else if ((fin->fin_out == 0) && (nat->nat_dir == NAT_OUTBOUND))
		rev = 1;
	else
		rev = 0;

	tcp = (tcphdr_t *)fin->fin_dp;
	if ((tcp->th_flags & TH_OPENING) == TH_OPENING) {
		pptp = (pptp_pxy_t *)aps->aps_data;
		pptp->pptp_side[1 - rev].pptps_next = ntohl(tcp->th_ack);
		pptp->pptp_side[1 - rev].pptps_nexthdr = ntohl(tcp->th_ack);
		pptp->pptp_side[rev].pptps_next = ntohl(tcp->th_seq) + 1;
		pptp->pptp_side[rev].pptps_nexthdr = ntohl(tcp->th_seq) + 1;
	}
	return ippr_pptp_nextmessage(fin, nat, (pptp_pxy_t *)aps->aps_data,
				     rev);
}


/*
 * clean up after ourselves.
 */
void ippr_pptp_del(aps)
ap_session_t *aps;
{
	pptp_pxy_t *pptp;

	pptp = aps->aps_data;

	if (pptp != NULL) {
		/*
		 * Don't bother changing any of the NAT structure details,
		 * *_del() is on a callback from aps_free(), from nat_delete()
		 */

		READ_ENTER(&ipf_state);
		if (pptp->pptp_state != NULL) {
			pptp->pptp_state->is_die = fr_ticks + 1;
			pptp->pptp_state->is_me = NULL;
			fr_queuefront(&pptp->pptp_state->is_sti);
		}
		RWLOCK_EXIT(&ipf_state);

		pptp->pptp_state = NULL;
		pptp->pptp_nat = NULL;
	}
}
