/*	$FreeBSD: src/sys/contrib/ipfilter/netinet/ip_raudio_pxy.c,v 1.13 2007/06/04 02:54:36 darrenr Exp $	*/

/*
 * $FreeBSD: src/sys/contrib/ipfilter/netinet/ip_raudio_pxy.c,v 1.13 2007/06/04 02:54:36 darrenr Exp $
 * Copyright (C) 1998-2003 by Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ip_raudio_pxy.c,v 1.40.2.4 2006/07/14 06:12:17 darrenr Exp $
 */

#define	IPF_RAUDIO_PROXY


int ippr_raudio_init __P((void));
void ippr_raudio_fini __P((void));
int ippr_raudio_new __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_raudio_in __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_raudio_out __P((fr_info_t *, ap_session_t *, nat_t *));

static	frentry_t	raudiofr;

int	raudio_proxy_init = 0;


/*
 * Real Audio application proxy initialization.
 */
int ippr_raudio_init()
{
	bzero((char *)&raudiofr, sizeof(raudiofr));
	raudiofr.fr_ref = 1;
	raudiofr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	MUTEX_INIT(&raudiofr.fr_lock, "Real Audio proxy rule lock");
	raudio_proxy_init = 1;

	return 0;
}


void ippr_raudio_fini()
{
	if (raudio_proxy_init == 1) {
		MUTEX_DESTROY(&raudiofr.fr_lock);
		raudio_proxy_init = 0;
	}
}


/*
 * Setup for a new proxy to handle Real Audio.
 */
int ippr_raudio_new(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	raudio_t *rap;

	KMALLOCS(aps->aps_data, void *, sizeof(raudio_t));
	if (aps->aps_data == NULL)
		return -1;

	fin = fin;	/* LINT */
	nat = nat;	/* LINT */

	bzero(aps->aps_data, sizeof(raudio_t));
	rap = aps->aps_data;
	aps->aps_psiz = sizeof(raudio_t);
	rap->rap_mode = RAP_M_TCP;	/* default is for TCP */
	return 0;
}



int ippr_raudio_out(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	raudio_t *rap = aps->aps_data;
	unsigned char membuf[512 + 1], *s;
	u_short id = 0;
	tcphdr_t *tcp;
	int off, dlen;
	int len = 0;
	mb_t *m;

	nat = nat;	/* LINT */

	/*
	 * If we've already processed the start messages, then nothing left
	 * for the proxy to do.
	 */
	if (rap->rap_eos == 1)
		return 0;

	m = fin->fin_m;
	tcp = (tcphdr_t *)fin->fin_dp;
	off = (char *)tcp - (char *)fin->fin_ip;
	off += (TCP_OFF(tcp) << 2) + fin->fin_ipoff;

#ifdef __sgi
	dlen = fin->fin_plen - off;
#else
	dlen = MSGDSIZE(m) - off;
#endif
	if (dlen <= 0)
		return 0;

	if (dlen > sizeof(membuf))
		dlen = sizeof(membuf);

	bzero((char *)membuf, sizeof(membuf));
	COPYDATA(m, off, dlen, (char *)membuf);
	/*
	 * In all the startup parsing, ensure that we don't go outside
	 * the packet buffer boundary.
	 */
	/*
	 * Look for the start of connection "PNA" string if not seen yet.
	 */
	if (rap->rap_seenpna == 0) {
		s = (u_char *)memstr("PNA", (char *)membuf, 3, dlen);
		if (s == NULL)
			return 0;
		s += 3;
		rap->rap_seenpna = 1;
	} else
		s = membuf;

	/*
	 * Directly after the PNA will be the version number of this
	 * connection.
	 */
	if (rap->rap_seenpna == 1 && rap->rap_seenver == 0) {
		if ((s + 1) - membuf < dlen) {
			rap->rap_version = (*s << 8) | *(s + 1);
			s += 2;
			rap->rap_seenver = 1;
		} else
			return 0;
	}

	/*
	 * Now that we've been past the PNA and version number, we're into the
	 * startup messages block.  This ends when a message with an ID of 0.
	 */
	while ((rap->rap_eos == 0) && ((s + 1) - membuf < dlen)) {
		if (rap->rap_gotid == 0) {
			id = (*s << 8) | *(s + 1);
			s += 2;
			rap->rap_gotid = 1;
			if (id == RA_ID_END) {
				rap->rap_eos = 1;
				break;
			}
		} else if (rap->rap_gotlen == 0) {
			len = (*s << 8) | *(s + 1);
			s += 2;
			rap->rap_gotlen = 1;
		}

		if (rap->rap_gotid == 1 && rap->rap_gotlen == 1) {
			if (id == RA_ID_UDP) {
				rap->rap_mode &= ~RAP_M_TCP;
				rap->rap_mode |= RAP_M_UDP;
				rap->rap_plport = (*s << 8) | *(s + 1);
			} else if (id == RA_ID_ROBUST) {
				rap->rap_mode |= RAP_M_ROBUST;
				rap->rap_prport = (*s << 8) | *(s + 1);
			}
			s += len;
			rap->rap_gotlen = 0;
			rap->rap_gotid = 0;
		}
	}
	return 0;
}


int ippr_raudio_in(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	unsigned char membuf[IPF_MAXPORTLEN + 1], *s;
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	raudio_t *rap = aps->aps_data;
	struct in_addr swa, swb;
	int off, dlen, slen;
	int a1, a2, a3, a4;
	u_short sp, dp;
	fr_info_t fi;
	tcp_seq seq;
	nat_t *nat2;
	u_char swp;
	ip_t *ip;
	mb_t *m;

	/*
	 * Wait until we've seen the end of the start messages and even then
	 * only proceed further if we're using UDP.  If they want to use TCP
	 * then data is sent back on the same channel that is already open.
	 */
	if (rap->rap_sdone != 0)
		return 0;

	m = fin->fin_m;
	tcp = (tcphdr_t *)fin->fin_dp;
	off = (char *)tcp - (char *)fin->fin_ip;
	off += (TCP_OFF(tcp) << 2) + fin->fin_ipoff;

#ifdef __sgi
	dlen = fin->fin_plen - off;
#else
	dlen = MSGDSIZE(m) - off;
#endif
	if (dlen <= 0)
		return 0;

	if (dlen > sizeof(membuf))
		dlen = sizeof(membuf);

	bzero((char *)membuf, sizeof(membuf));
	COPYDATA(m, off, dlen, (char *)membuf);

	seq = ntohl(tcp->th_seq);
	/*
	 * Check to see if the data in this packet is of interest to us.
	 * We only care for the first 19 bytes coming back from the server.
	 */
	if (rap->rap_sseq == 0) {
		s = (u_char *)memstr("PNA", (char *)membuf, 3, dlen);
		if (s == NULL)
			return 0;
		a1 = s - membuf;
		dlen -= a1;
		a1 = 0;
		rap->rap_sseq = seq;
		a2 = MIN(dlen, sizeof(rap->rap_svr));
	} else if (seq <= rap->rap_sseq + sizeof(rap->rap_svr)) {
		/*
		 * seq # which is the start of data and from that the offset
		 * into the buffer array.
		 */
		a1 = seq - rap->rap_sseq;
		a2 = MIN(dlen, sizeof(rap->rap_svr));
		a2 -= a1;
		s = membuf;
	} else
		return 0;

	for (a3 = a1, a4 = a2; (a4 > 0) && (a3 < 19) && (a3 >= 0); a4--,a3++) {
		rap->rap_sbf |= (1 << a3);
		rap->rap_svr[a3] = *s++;
	}

	if ((rap->rap_sbf != 0x7ffff) || (!rap->rap_eos))	/* 19 bits */
		return 0;
	rap->rap_sdone = 1;

	s = (u_char *)rap->rap_svr + 11;
	if (((*s << 8) | *(s + 1)) == RA_ID_ROBUST) {
		s += 2;
		rap->rap_srport = (*s << 8) | *(s + 1);
	}

	ip = fin->fin_ip;
	swp = ip->ip_p;
	swa = ip->ip_src;
	swb = ip->ip_dst;

	ip->ip_p = IPPROTO_UDP;
	ip->ip_src = nat->nat_inip;
	ip->ip_dst = nat->nat_oip;

	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	bzero((char *)tcp2, sizeof(*tcp2));
	TCP_OFF_A(tcp2, 5);
	fi.fin_state = NULL;
	fi.fin_nat = NULL;
	fi.fin_flx |= FI_IGNORE;
	fi.fin_dp = (char *)tcp2;
	fi.fin_fr = &raudiofr;
	fi.fin_dlen = sizeof(*tcp2);
	fi.fin_plen = fi.fin_hlen + sizeof(*tcp2);
	tcp2->th_win = htons(8192);
	slen = ip->ip_len;
	ip->ip_len = fin->fin_hlen + sizeof(*tcp);

	if (((rap->rap_mode & RAP_M_UDP_ROBUST) == RAP_M_UDP_ROBUST) &&
	    (rap->rap_srport != 0)) {
		dp = rap->rap_srport;
		sp = rap->rap_prport;
		tcp2->th_sport = htons(sp);
		tcp2->th_dport = htons(dp);
		fi.fin_data[0] = dp;
		fi.fin_data[1] = sp;
		fi.fin_out = 0;
		nat2 = nat_new(&fi, nat->nat_ptr, NULL,
			       NAT_SLAVE|IPN_UDP | (sp ? 0 : SI_W_SPORT),
			       NAT_OUTBOUND);
		if (nat2 != NULL) {
			(void) nat_proto(&fi, nat2, IPN_UDP);
			nat_update(&fi, nat2, nat2->nat_ptr);

			(void) fr_addstate(&fi, NULL, (sp ? 0 : SI_W_SPORT));
			if (fi.fin_state != NULL)
				fr_statederef((ipstate_t **)&fi.fin_state);
		}
	}

	if ((rap->rap_mode & RAP_M_UDP) == RAP_M_UDP) {
		sp = rap->rap_plport;
		tcp2->th_sport = htons(sp);
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		fi.fin_data[0] = sp;
		fi.fin_data[1] = 0;
		fi.fin_out = 1;
		nat2 = nat_new(&fi, nat->nat_ptr, NULL,
			       NAT_SLAVE|IPN_UDP|SI_W_DPORT,
			       NAT_OUTBOUND);
		if (nat2 != NULL) {
			(void) nat_proto(&fi, nat2, IPN_UDP);
			nat_update(&fi, nat2, nat2->nat_ptr);

			(void) fr_addstate(&fi, NULL, SI_W_DPORT);
			if (fi.fin_state != NULL)
				fr_statederef((ipstate_t **)&fi.fin_state);
		}
	}

	ip->ip_p = swp;
	ip->ip_len = slen;
	ip->ip_src = swa;
	ip->ip_dst = swb;
	return 0;
}
