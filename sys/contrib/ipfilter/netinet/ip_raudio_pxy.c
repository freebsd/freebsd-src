/*
 * $FreeBSD$
 */
#if SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
#endif

#define	IPF_RAUDIO_PROXY


int ippr_raudio_init __P((void));
int ippr_raudio_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_raudio_in __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_raudio_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));

static	frentry_t	raudiofr;


/*
 * Real Audio application proxy initialization.
 */
int ippr_raudio_init()
{
	bzero((char *)&raudiofr, sizeof(raudiofr));
	raudiofr.fr_ref = 1;
	raudiofr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	return 0;
}


/*
 * Setup for a new proxy to handle Real Audio.
 */
int ippr_raudio_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	raudio_t *rap;


	KMALLOCS(aps->aps_data, void *, sizeof(raudio_t));
	if (aps->aps_data == NULL)
		return -1;

	bzero(aps->aps_data, sizeof(raudio_t));
	rap = aps->aps_data;
	aps->aps_psiz = sizeof(raudio_t);
	rap->rap_mode = RAP_M_TCP;	/* default is for TCP */
	return 0;
}



int ippr_raudio_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	raudio_t *rap = aps->aps_data;
	unsigned char membuf[512 + 1], *s;
	u_short id = 0;
	int off, dlen;
	tcphdr_t *tcp;
	int len = 0;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	/*
	 * If we've already processed the start messages, then nothing left
	 * for the proxy to do.
	 */
	if (rap->rap_eos == 1)
		return 0;

	tcp = (tcphdr_t *)fin->fin_dp;
	off = (ip->ip_hl << 2) + (tcp->th_off << 2);
	bzero(membuf, sizeof(membuf));
#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	if (dlen <= 0)
		return 0;
	dlen = MIN(sizeof(membuf), dlen);
	copyout_mblk(m, off, dlen, (char *)membuf);
#else
	m = *(mb_t **)fin->fin_mp;

	dlen = mbufchainlen(m) - off;
	if (dlen <= 0)
		return 0;
	dlen = MIN(sizeof(membuf), dlen);
	m_copydata(m, off, dlen, (char *)membuf);
#endif
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


int ippr_raudio_in(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	unsigned char membuf[IPF_MAXPORTLEN + 1], *s;
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	raudio_t *rap = aps->aps_data;
	int off, dlen, slen, clen;
	struct in_addr swa, swb;
	int a1, a2, a3, a4;
	u_short sp, dp;
	fr_info_t fi;
	tcp_seq seq;
	nat_t *ipn;
	u_char swp;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	/*
	 * Wait until we've seen the end of the start messages and even then
	 * only proceed further if we're using UDP.  If they want to use TCP
	 * then data is sent back on the same channel that is already open.
	 */
	if (rap->rap_sdone != 0)
		return 0;

	tcp = (tcphdr_t *)fin->fin_dp;
	off = (ip->ip_hl << 2) + (tcp->th_off << 2);
	m = *(mb_t **)fin->fin_mp;

#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	if (dlen <= 0)
		return 0;
	bzero(membuf, sizeof(membuf));
	clen = MIN(sizeof(membuf), dlen);
	copyout_mblk(m, off, clen, (char *)membuf);
#else
	dlen = mbufchainlen(m) - off;
	if (dlen <= 0)
		return 0;
	bzero(membuf, sizeof(membuf));
	clen = MIN(sizeof(membuf), dlen);
	m_copydata(m, off, clen, (char *)membuf);
#endif

	seq = ntohl(tcp->th_seq);
	/*
	 * Check to see if the data in this packet is of interest to us.
	 * We only care for the first 19 bytes coming back from the server.
	 */
	if (rap->rap_sseq == 0) {
		s = (u_char *)memstr("PNA", (char *)membuf, 3, clen);
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

	swp = ip->ip_p;
	swa = ip->ip_src;
	swb = ip->ip_dst;

	ip->ip_p = IPPROTO_UDP;
	ip->ip_src = nat->nat_inip;
	ip->ip_dst = nat->nat_oip;

	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	bzero((char *)tcp2, sizeof(*tcp2));
	tcp2->th_off = 5;
	fi.fin_dp = (char *)tcp2;
	fi.fin_fr = &raudiofr;
	fi.fin_dlen = sizeof(*tcp2);
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
		ipn = nat_new(nat->nat_ptr, ip, &fi, 
			      IPN_UDP | (sp ? 0 : FI_W_SPORT), NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, sp ? 0 : FI_W_SPORT);
		}
	}

	if ((rap->rap_mode & RAP_M_UDP) == RAP_M_UDP) {
		sp = rap->rap_plport;
		tcp2->th_sport = htons(sp);
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		fi.fin_data[0] = sp;
		fi.fin_data[1] = 0;
		fi.fin_out = 1;
		ipn = nat_new(nat->nat_ptr, ip, &fi, IPN_UDP|FI_W_DPORT,
			      NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, FI_W_DPORT);
		}
	}

	ip->ip_p = swp;
	ip->ip_len = slen;
	ip->ip_src = swa;
	ip->ip_dst = swb;
	return 0;
}
