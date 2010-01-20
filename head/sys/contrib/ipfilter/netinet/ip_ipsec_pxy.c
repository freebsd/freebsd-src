/*
 * Copyright (C) 2001-2003 by Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Simple ISAKMP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 *
 * $Id: ip_ipsec_pxy.c,v 2.20.2.8 2006/07/14 06:12:14 darrenr Exp $
 *
 */
#define	IPF_IPSEC_PROXY


int ippr_ipsec_init __P((void));
void ippr_ipsec_fini __P((void));
int ippr_ipsec_new __P((fr_info_t *, ap_session_t *, nat_t *));
void ippr_ipsec_del __P((ap_session_t *));
int ippr_ipsec_inout __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_ipsec_match __P((fr_info_t *, ap_session_t *, nat_t *));

static	frentry_t	ipsecfr;
static	ipftq_t		*ipsecnattqe;
static	ipftq_t		*ipsecstatetqe;
static	char	ipsec_buffer[1500];

int	ipsec_proxy_init = 0;
int	ipsec_proxy_ttl = 60;

/*
 * IPSec application proxy initialization.
 */
int ippr_ipsec_init()
{
	bzero((char *)&ipsecfr, sizeof(ipsecfr));
	ipsecfr.fr_ref = 1;
	ipsecfr.fr_flags = FR_OUTQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	MUTEX_INIT(&ipsecfr.fr_lock, "IPsec proxy rule lock");
	ipsec_proxy_init = 1;

	ipsecnattqe = fr_addtimeoutqueue(&nat_utqe, ipsec_proxy_ttl);
	if (ipsecnattqe == NULL)
		return -1;
	ipsecstatetqe = fr_addtimeoutqueue(&ips_utqe, ipsec_proxy_ttl);
	if (ipsecstatetqe == NULL) {
		if (fr_deletetimeoutqueue(ipsecnattqe) == 0)
			fr_freetimeoutqueue(ipsecnattqe);
		ipsecnattqe = NULL;
		return -1;
	}

	ipsecnattqe->ifq_flags |= IFQF_PROXY;
	ipsecstatetqe->ifq_flags |= IFQF_PROXY;

	ipsecfr.fr_age[0] = ipsec_proxy_ttl;
	ipsecfr.fr_age[1] = ipsec_proxy_ttl;
	return 0;
}


void ippr_ipsec_fini()
{
	if (ipsecnattqe != NULL) {
		if (fr_deletetimeoutqueue(ipsecnattqe) == 0)
			fr_freetimeoutqueue(ipsecnattqe);
	}
	ipsecnattqe = NULL;
	if (ipsecstatetqe != NULL) {
		if (fr_deletetimeoutqueue(ipsecstatetqe) == 0)
			fr_freetimeoutqueue(ipsecstatetqe);
	}
	ipsecstatetqe = NULL;

	if (ipsec_proxy_init == 1) {
		MUTEX_DESTROY(&ipsecfr.fr_lock);
		ipsec_proxy_init = 0;
	}
}


/*
 * Setup for a new IPSEC proxy.
 */
int ippr_ipsec_new(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	ipsec_pxy_t *ipsec;
	fr_info_t fi;
	ipnat_t *ipn;
	char *ptr;
	int p, off, dlen, ttl;
	mb_t *m;
	ip_t *ip;

	off = fin->fin_plen - fin->fin_dlen + fin->fin_ipoff;
	bzero(ipsec_buffer, sizeof(ipsec_buffer));
	ip = fin->fin_ip;
	m = fin->fin_m;

	dlen = M_LEN(m) - off;
	if (dlen < 16)
		return -1;
	COPYDATA(m, off, MIN(sizeof(ipsec_buffer), dlen), ipsec_buffer);

	if (nat_outlookup(fin, 0, IPPROTO_ESP, nat->nat_inip,
			  ip->ip_dst) != NULL)
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
	ttl = IPF_TTLVAL(ipsecnattqe->ifq_ttl);
	ipn->in_tqehead[0] = fr_addtimeoutqueue(&nat_utqe, ttl);
	ipn->in_tqehead[1] = fr_addtimeoutqueue(&nat_utqe, ttl);
	ipn->in_ifps[0] = fin->fin_ifp;
	ipn->in_apr = NULL;
	ipn->in_use = 1;
	ipn->in_hits = 1;
	ipn->in_nip = ntohl(nat->nat_outip.s_addr);
	ipn->in_ippip = 1;
	ipn->in_inip = nat->nat_inip.s_addr;
	ipn->in_inmsk = 0xffffffff;
	ipn->in_outip = fin->fin_saddr;
	ipn->in_outmsk = nat->nat_outip.s_addr;
	ipn->in_srcip = fin->fin_saddr;
	ipn->in_srcmsk = 0xffffffff;
	ipn->in_redir = NAT_MAP;
	bcopy(nat->nat_ptr->in_ifnames[0], ipn->in_ifnames[0],
	      sizeof(ipn->in_ifnames[0]));
	ipn->in_p = IPPROTO_ESP;

	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_state = NULL;
	fi.fin_nat = NULL;
	fi.fin_fi.fi_p = IPPROTO_ESP;
	fi.fin_fr = &ipsecfr;
	fi.fin_data[0] = 0;
	fi.fin_data[1] = 0;
	p = ip->ip_p;
	ip->ip_p = IPPROTO_ESP;
	fi.fin_flx &= ~(FI_TCPUDP|FI_STATE|FI_FRAG);
	fi.fin_flx |= FI_IGNORE;

	ptr = ipsec_buffer;
	bcopy(ptr, (char *)ipsec->ipsc_icookie, sizeof(ipsec_cookie_t));
	ptr += sizeof(ipsec_cookie_t);
	bcopy(ptr, (char *)ipsec->ipsc_rcookie, sizeof(ipsec_cookie_t));
	/*
	 * The responder cookie should only be non-zero if the initiator
	 * cookie is non-zero.  Therefore, it is safe to assume(!) that the
	 * cookies are both set after copying if the responder is non-zero.
	 */
	if ((ipsec->ipsc_rcookie[0]|ipsec->ipsc_rcookie[1]) != 0)
		ipsec->ipsc_rckset = 1;

	ipsec->ipsc_nat = nat_new(&fi, ipn, &ipsec->ipsc_nat,
				  NAT_SLAVE|SI_WILDP, NAT_OUTBOUND);
	if (ipsec->ipsc_nat != NULL) {
		(void) nat_proto(&fi, ipsec->ipsc_nat, 0);
		nat_update(&fi, ipsec->ipsc_nat, ipn);

		fi.fin_data[0] = 0;
		fi.fin_data[1] = 0;
		ipsec->ipsc_state = fr_addstate(&fi, &ipsec->ipsc_state,
						SI_WILDP);
		if (fi.fin_state != NULL)
			fr_statederef((ipstate_t **)&fi.fin_state);
	}
	ip->ip_p = p & 0xff;
	return 0;
}


/*
 * For outgoing IKE packets.  refresh timeouts for NAT & state entries, if
 * we can.  If they have disappeared, recreate them.
 */
int ippr_ipsec_inout(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	ipsec_pxy_t *ipsec;
	fr_info_t fi;
	ip_t *ip;
	int p;

	if ((fin->fin_out == 1) && (nat->nat_dir == NAT_INBOUND))
		return 0;

	if ((fin->fin_out == 0) && (nat->nat_dir == NAT_OUTBOUND))
		return 0;

	ipsec = aps->aps_data;

	if (ipsec != NULL) {
		ip = fin->fin_ip;
		p = ip->ip_p;

		if ((ipsec->ipsc_nat == NULL) || (ipsec->ipsc_state == NULL)) {
			bcopy((char *)fin, (char *)&fi, sizeof(fi));
			fi.fin_state = NULL;
			fi.fin_nat = NULL;
			fi.fin_fi.fi_p = IPPROTO_ESP;
			fi.fin_fr = &ipsecfr;
			fi.fin_data[0] = 0;
			fi.fin_data[1] = 0;
			ip->ip_p = IPPROTO_ESP;
			fi.fin_flx &= ~(FI_TCPUDP|FI_STATE|FI_FRAG);
			fi.fin_flx |= FI_IGNORE;
		}

		/*
		 * Update NAT timeout/create NAT if missing.
		 */
		if (ipsec->ipsc_nat != NULL)
			fr_queueback(&ipsec->ipsc_nat->nat_tqe);
		else {
			ipsec->ipsc_nat = nat_new(&fi, &ipsec->ipsc_rule,
						  &ipsec->ipsc_nat,
						  NAT_SLAVE|SI_WILDP,
						  nat->nat_dir);
			if (ipsec->ipsc_nat != NULL) {
				(void) nat_proto(&fi, ipsec->ipsc_nat, 0);
				nat_update(&fi, ipsec->ipsc_nat,
					   &ipsec->ipsc_rule);
			}
		}

		/*
		 * Update state timeout/create state if missing.
		 */
		READ_ENTER(&ipf_state);
		if (ipsec->ipsc_state != NULL) {
			fr_queueback(&ipsec->ipsc_state->is_sti);
			ipsec->ipsc_state->is_die = nat->nat_age;
			RWLOCK_EXIT(&ipf_state);
		} else {
			RWLOCK_EXIT(&ipf_state);
			fi.fin_data[0] = 0;
			fi.fin_data[1] = 0;
			ipsec->ipsc_state = fr_addstate(&fi,
							&ipsec->ipsc_state,
							SI_WILDP);
			if (fi.fin_state != NULL)
				fr_statederef((ipstate_t **)&fi.fin_state);
		}
		ip->ip_p = p;
	}
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

	nat = nat;	/* LINT */

	if ((fin->fin_dlen < sizeof(cookies)) || (fin->fin_flx & FI_FRAG))
		return -1;

	off = fin->fin_plen - fin->fin_dlen + fin->fin_ipoff;
	ipsec = aps->aps_data;
	m = fin->fin_m;
	COPYDATA(m, off, sizeof(cookies), (char *)cookies);

	if ((cookies[0] != ipsec->ipsc_icookie[0]) ||
	    (cookies[1] != ipsec->ipsc_icookie[1]))
		return -1;

	if (ipsec->ipsc_rckset == 0) {
		if ((cookies[2]|cookies[3]) == 0) {
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
		 * Don't bother changing any of the NAT structure details,
		 * *_del() is on a callback from aps_free(), from nat_delete()
		 */

		READ_ENTER(&ipf_state);
		if (ipsec->ipsc_state != NULL) {
			ipsec->ipsc_state->is_die = fr_ticks + 1;
			ipsec->ipsc_state->is_me = NULL;
			fr_queuefront(&ipsec->ipsc_state->is_sti);
		}
		RWLOCK_EXIT(&ipf_state);

		ipsec->ipsc_state = NULL;
		ipsec->ipsc_nat = NULL;
	}
}
