/*
 * Simple ISAKMP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 *
 * $Id: ip_ipsec_pxy.c,v 1.1.2.10 2002/01/13 04:58:29 darrenr Exp $
 *
 */
#define	IPF_IPSEC_PROXY


int ippr_ipsec_init __P((void));
int ippr_ipsec_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
void ippr_ipsec_del __P((ap_session_t *));
int ippr_ipsec_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ipsec_match __P((fr_info_t *, ap_session_t *, nat_t *));

static	frentry_t	ipsecfr;


static	char	ipsec_buffer[1500];

/*
 * RCMD application proxy initialization.
 */
int ippr_ipsec_init()
{
	bzero((char *)&ipsecfr, sizeof(ipsecfr));
	ipsecfr.fr_ref = 1;
	ipsecfr.fr_flags = FR_OUTQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	return 0;
}


/*
 * Setup for a new IPSEC proxy.
 */
int ippr_ipsec_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	ipsec_pxy_t *ipsec;
	fr_info_t fi;
	ipnat_t *ipn;
	char *ptr;
	int p, off, dlen;
	mb_t *m;

	bzero(ipsec_buffer, sizeof(ipsec_buffer));
	off = fin->fin_hlen + sizeof(udphdr_t);
#ifdef	_KERNEL
# if     SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	if (dlen < 16)
		return -1;
	copyout_mblk(m, off, MIN(sizeof(ipsec_buffer), dlen), ipsec_buffer);
# else
	m = *(mb_t **)fin->fin_mp;
	dlen = mbufchainlen(m) - off;
	if (dlen < 16)
		return -1;
	m_copydata(m, off, MIN(sizeof(ipsec_buffer), dlen), ipsec_buffer);
# endif
#else
	m = *(mb_t **)fin->fin_mp;
	dlen = ip->ip_len - off;
	ptr = (char *)m;
	ptr += off;
	bcopy(ptr, ipsec_buffer, MIN(sizeof(ipsec_buffer), dlen));
#endif

	/*
	 * Because _new() gets called from nat_new(), ipf_nat is held with a
	 * write lock so pass rw=1 to nat_outlookup().
	 */
	if (nat_outlookup(fin, 0, IPPROTO_ESP, nat->nat_inip,
			  ip->ip_dst, 1) != NULL)
		return -1;

	aps->aps_psiz = sizeof(*ipsec);
	KMALLOCS(aps->aps_data, ipsec_pxy_t *, sizeof(*ipsec));
	if (aps->aps_data == NULL)
		return -1;

	ipsec = aps->aps_data;
	bzero((char *)ipsec, sizeof(*ipsec));

	/*
	 * Create NAT rule against which the tunnel/transport mapping is
	 * created.  This is required because the current NAT rule does not
	 * describe ESP but UDP instead.
	 */
	ipn = &ipsec->ipsc_rule;
	ipn->in_ifp = fin->fin_ifp;
	ipn->in_apr = NULL;
	ipn->in_use = 1;
	ipn->in_hits = 1;
	ipn->in_nip = ntohl(nat->nat_outip.s_addr);
	ipn->in_ippip = 1;
	ipn->in_inip = nat->nat_inip.s_addr;
	ipn->in_inmsk = 0xffffffff;
	ipn->in_outip = nat->nat_outip.s_addr;
	ipn->in_outmsk = 0xffffffff;
	ipn->in_srcip = fin->fin_saddr;
	ipn->in_srcmsk = 0xffffffff;
	ipn->in_redir = NAT_MAP;
	bcopy(nat->nat_ptr->in_ifname, ipn->in_ifname, sizeof(ipn->in_ifname));
	ipn->in_p = IPPROTO_ESP;

	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_fi.fi_p = IPPROTO_ESP;
	fi.fin_fr = &ipsecfr;
	fi.fin_data[0] = 0;
	fi.fin_data[1] = 0;
	p = ip->ip_p;
	ip->ip_p = IPPROTO_ESP;
	fi.fin_fl &= ~FI_TCPUDP;

	ptr = ipsec_buffer;
	bcopy(ptr, ipsec->ipsc_icookie, sizeof(ipsec_cookie_t));
	ptr += sizeof(ipsec_cookie_t);
	bcopy(ptr, ipsec->ipsc_rcookie, sizeof(ipsec_cookie_t));
	/*
	 * The responder cookie should only be non-zero if the initiator
	 * cookie is non-zero.  Therefore, it is safe to assume(!) that the
	 * cookies are both set after copying if the responder is non-zero.
	 */
	if ((ipsec->ipsc_rcookie[0]|ipsec->ipsc_rcookie[1]) != 0)
		ipsec->ipsc_rckset = 1;
	else
		nat->nat_age = 60;	/* 30 seconds */

	ipsec->ipsc_nat = nat_new(&fi, ip, ipn, &ipsec->ipsc_nat, FI_IGNOREPKT,
				   NAT_OUTBOUND);
	if (ipsec->ipsc_nat != NULL) {
		fi.fin_data[0] = 0;
		fi.fin_data[1] = 0;
		ipsec->ipsc_state = fr_addstate(ip, &fi, &ipsec->ipsc_state,
						FI_IGNOREPKT|FI_NORULE);
	}
	ip->ip_p = p;
	return 0;
}


/*
 * For outgoing IKE packets.  refresh timeouts for NAT & stat entries, if
 * we can.  If they have disappeared, recreate them.
 */
int ippr_ipsec_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	ipsec_pxy_t *ipsec;
	fr_info_t fi;
	int p;

	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_fi.fi_p = IPPROTO_ESP;
	fi.fin_fr = &ipsecfr;
	fi.fin_data[0] = 0;
	fi.fin_data[1] = 0;
	p = ip->ip_p;
	ip->ip_p = IPPROTO_ESP;
	fi.fin_fl &= ~FI_TCPUDP;

	ipsec = aps->aps_data;
	if (ipsec != NULL) {
		/*
		 * Update NAT timeout/create NAT if missing.
		 */
		if (ipsec->ipsc_rckset == 0)
			nat->nat_age = 60;	/* 30 seconds */
		if (ipsec->ipsc_nat != NULL)
			ipsec->ipsc_nat->nat_age = nat->nat_age;
		else
			ipsec->ipsc_nat = nat_new(&fi, ip, &ipsec->ipsc_rule,
						  &ipsec->ipsc_nat,
						  FI_IGNOREPKT, NAT_OUTBOUND);

		/*
		 * Update state timeout/create state if missing.
		 */
		READ_ENTER(&ipf_state);
		if (ipsec->ipsc_state != NULL) {
			ipsec->ipsc_state->is_age = nat->nat_age;
			RWLOCK_EXIT(&ipf_state);
		} else {
			RWLOCK_EXIT(&ipf_state);
			fi.fin_data[0] = 0;
			fi.fin_data[1] = 0;
			ipsec->ipsc_state = fr_addstate(ip, &fi,
							&ipsec->ipsc_state,
							FI_IGNOREPKT|FI_NORULE);
		}
	}
	ip->ip_p = p;
	return 0;
}


/*
 * This extends the NAT matching to be based on the cookies associated with
 * a session and found at the front of IKE packets.  The cookies are always
 * in the same order (not reversed depending on packet flow direction as with
 * UDP/TCP port numbers).
 */
int ippr_ipsec_match(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	ipsec_pxy_t *ipsec;
	u_32_t cookies[4];
	mb_t *m;
	int off;

	if ((fin->fin_dlen < sizeof(cookies)) || (fin->fin_fl & FI_FRAG))
		return -1;

	ipsec = aps->aps_data;
	off = fin->fin_hlen + sizeof(udphdr_t);
#ifdef	_KERNEL
# if     SOLARIS
	m = fin->fin_qfm;

	copyout_mblk(m, off, sizeof(cookies), (char *)cookies);
# else
	m = *(mb_t **)fin->fin_mp;
	m_copydata(m, off, sizeof(cookies), (char *)cookies);
# endif
#else
	m = *(mb_t **)fin->fin_mp;
	bcopy((char *)m + off, cookies, sizeof(cookies));
#endif

	if ((cookies[0] != ipsec->ipsc_icookie[0]) ||
	    (cookies[1] != ipsec->ipsc_icookie[1]))
		return -1;

	if (ipsec->ipsc_rckset == 0) {
		if ((cookies[2]|cookies[3]) == 0) {
			nat->nat_age = 60;	/* 30 seconds */
			return 0;
		}
		ipsec->ipsc_rckset = 1;
		ipsec->ipsc_rcookie[0] = cookies[2];
		ipsec->ipsc_rcookie[1] = cookies[3];
		return 0;
	}

	if ((cookies[2] != ipsec->ipsc_rcookie[0]) ||
	    (cookies[3] != ipsec->ipsc_rcookie[1]))
		return -1;
	return 0;
}


/*
 * clean up after ourselves.
 */
void ippr_ipsec_del(aps)
ap_session_t *aps;
{
	ipsec_pxy_t *ipsec;

	ipsec = aps->aps_data;

	if (ipsec != NULL) {
		/*
		 * Don't delete it from here, just schedule it to be
		 * deleted ASAP.
		 */
		if (ipsec->ipsc_nat != NULL) {
			ipsec->ipsc_nat->nat_age = 1;
			ipsec->ipsc_nat->nat_ptr = NULL;
		}

		READ_ENTER(&ipf_state);
		if (ipsec->ipsc_state != NULL)
			ipsec->ipsc_state->is_age = 1;
		RWLOCK_EXIT(&ipf_state);

		ipsec->ipsc_state = NULL;
		ipsec->ipsc_nat = NULL;
	}
}
