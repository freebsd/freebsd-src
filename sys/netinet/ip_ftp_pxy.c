/*
 * Simple FTP transparent proxy for in-kernel.
 */

#define	isdigit(x)	((x) >= '0' && (x) <= '9')

#define	IPF_FTP_PROXY

#define	IPF_MINPORTLEN	18
#define	IPF_MAXPORTLEN	30


int ippr_ftp_init(fin, ip, tcp, aps, nat)
fr_info_t *fin;
ip_t *ip;
tcphdr_t *tcp;
ap_session_t *aps;
nat_t *nat;
{
	aps->aps_sport = tcp->th_sport;
	aps->aps_dport = tcp->th_dport;
	return 0;
}


int ippr_ftp_in(fin, ip, tcp, aps, nat)
fr_info_t *fin;
ip_t *ip;
tcphdr_t *tcp;
ap_session_t *aps;
nat_t *nat;
{
	int	ch = 0;
	u_long	sum1, sum2;

	if (tcp->th_dport != aps->aps_dport) {
		sum2 = (u_long)ntohl(tcp->th_ack);
		if (aps->aps_seqoff && (sum2 > aps->aps_after)) {
			sum1 = (u_long)aps->aps_seqoff;
			tcp->th_ack = htonl(sum2 - sum1);
			return 2;
		}
	}
	return 0;
}


u_short ipf_ftp_atoi(ptr)
char **ptr;
{
	register char *s = *ptr, c;
	register u_char i = 0, j = 0;

	while ((c = *s++) && isdigit(c)) {
		i *= 10;
		i += c - '0';
	}
	if (c != ',') {
		*ptr = NULL;
		return 0;
	}
	while ((c = *s++) && isdigit(c)) {
		j *= 10;
		j += c - '0';
	}
	*ptr = s;
	return (i << 8) | j;
}


int ippr_ftp_out(fin, ip, tcp, aps, nat)
fr_info_t *fin;
ip_t *ip;
tcphdr_t *tcp;
ap_session_t *aps;
nat_t *nat;
{
	register u_long	sum1, sum2, sumd;
	char	newbuf[IPF_MAXPORTLEN+1];
	char	portbuf[IPF_MAXPORTLEN+1], *s, c;
	int	ch = 0, off = (ip->ip_hl << 2) + (tcp->th_off << 2), len;
	u_int	a1, a2, a3, a4;
	u_short	a5, a6;
	int	olen, dlen, nlen, inc = 0, blen;
	tcphdr_t tcph, *tcp2 = &tcph;
	void	*savep;
	nat_t	*ipn;
	struct	in_addr	swip;
#if	SOLARIS
	mblk_t *m1, *m = *(mblk_t **)fin->fin_mp;

	dlen = m->b_wptr - m->b_rptr - off;
	blen = m->b_datap->db_lim - m->b_datap->db_base;
	bzero(portbuf, sizeof(portbuf));
	copyout_mblk(m, off, portbuf, MIN(sizeof(portbuf), dlen));
#else
	struct mbuf *m1, *m = *(struct mbuf **)fin->fin_mp;

	dlen = m->m_len - off;
# if BSD >= 199306
	blen = (MLEN - m->m_len) - (m->m_data - m->m_dat);
# else
	blen = (MLEN - m->m_len) - m->m_off;
# endif
	if (blen < 0)
		panic("blen < 0 - size of mblk/mbuf wrong");
	bzero(portbuf, sizeof(portbuf));
	m_copydata(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#endif
	portbuf[IPF_MAXPORTLEN] = '\0';
	len = MIN(32, dlen);

	if ((len < IPF_MINPORTLEN) || strncmp(portbuf, "PORT ", 5))
		goto adjust_seqack;

	/*
	 * Skip the PORT command + space
	 */
	s = portbuf + 5;
	/*
	 * Pick out the address components, two at a time.
	 */
	(void) ipf_ftp_atoi(&s);
	if (!s)
		goto adjust_seqack;
	(void) ipf_ftp_atoi(&s);
	if (!s)
		goto adjust_seqack;
	a5 = ipf_ftp_atoi(&s);
	if (!s)
		goto adjust_seqack;
	/*
	 * check for CR-LF at the end.
	 */
	if (*s != '\n' || *(s - 1) != '\r')
		goto adjust_seqack;
	a6 = a5 & 0xff;
	a5 >>= 8;
	/*
	 * Calculate new address parts for PORT command
	 */
	a1 = ntohl(ip->ip_src.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	olen = s - portbuf + 1;
	(void) sprintf(newbuf, "PORT %d,%d,%d,%d,%d,%d\r\n",
		a1, a2, a3, a4, a5, a6);
	nlen = strlen(newbuf);
	inc = nlen - olen;
	if (tcp->th_seq > aps->aps_after) {
		aps->aps_after = ntohl(tcp->th_seq) + dlen;
		aps->aps_seqoff += inc;
	}
#if SOLARIS
	if (inc && dlen)
		if ((inc < 0) || (blen >= dlen)) {
			bcopy(m->b_rptr + off,
			      m->b_rptr + off + aps->aps_seqoff, dlen);
		}
	for (m1 = m; m1->b_cont; m1 = m1->b_cont)
		;
	m1->b_wptr += inc;
	copyin_mblk(m, off, newbuf, strlen(newbuf));
#else
	if (inc && dlen)
		if ((inc < 0) || (blen >= dlen)) {
			bcopy((char *)ip + off,
			      (char *)ip + off + aps->aps_seqoff, dlen);
		}
	m->m_len += inc;
	m_copyback(m, off, nlen, newbuf);
#endif
	ip->ip_len += inc;
	ch = 1;

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	savep = fin->fin_dp;
	fin->fin_dp = (char *)tcp2;
	tcp2->th_sport = htons(a5 << 8 | a6);
	tcp2->th_dport = htons(20);
	swip = ip->ip_src;
	ip->ip_src = nat->nat_inip;
	if ((ipn = nat_new(nat->nat_ptr, ip, fin, IPN_TCP, NAT_OUTBOUND)))
		ipn->nat_age = fr_defnatage;
	ip->ip_src = swip;
	fin->fin_dp = (char *)savep;

adjust_seqack:
	if (tcp->th_dport == aps->aps_dport) {
		sum2 = (u_long)ntohl(tcp->th_seq);
		if (aps->aps_seqoff && (sum2 > aps->aps_after)) {
			sum1 = (u_long)aps->aps_seqoff;
			tcp->th_seq = htonl(sum2 + sum1);
			ch = 1;
		}
	}

	return ch ? 2 : 0;
}
