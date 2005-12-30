/*	$FreeBSD$	*/

/*
 * Copyright (C) 1997-2003 by Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Simple FTP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 *
 * $FreeBSD$
 * Id: ip_ftp_pxy.c,v 2.88.2.15 2005/03/19 19:38:10 darrenr Exp
 */

#define	IPF_FTP_PROXY

#define	IPF_MINPORTLEN	18
#define	IPF_MAXPORTLEN	30
#define	IPF_MIN227LEN	39
#define	IPF_MAX227LEN	51
#define	IPF_MIN229LEN	47
#define	IPF_MAX229LEN	51

#define	FTPXY_GO	0
#define	FTPXY_INIT	1
#define	FTPXY_USER_1	2
#define	FTPXY_USOK_1	3
#define	FTPXY_PASS_1	4
#define	FTPXY_PAOK_1	5
#define	FTPXY_AUTH_1	6
#define	FTPXY_AUOK_1	7
#define	FTPXY_ADAT_1	8
#define	FTPXY_ADOK_1	9
#define	FTPXY_ACCT_1	10
#define	FTPXY_ACOK_1	11
#define	FTPXY_USER_2	12
#define	FTPXY_USOK_2	13
#define	FTPXY_PASS_2	14
#define	FTPXY_PAOK_2	15

/*
 * Values for FTP commands.  Numerics cover 0-999
 */
#define	FTPXY_C_PASV	1000

int ippr_ftp_client __P((fr_info_t *, ip_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_complete __P((char *, size_t));
int ippr_ftp_in __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_ftp_init __P((void));
void ippr_ftp_fini __P((void));
int ippr_ftp_new __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_ftp_out __P((fr_info_t *, ap_session_t *, nat_t *));
int ippr_ftp_pasv __P((fr_info_t *, ip_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_epsv __P((fr_info_t *, ip_t *, nat_t *, ftpside_t *, int));
int ippr_ftp_port __P((fr_info_t *, ip_t *, nat_t *, ftpside_t *, int));
int ippr_ftp_process __P((fr_info_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_server __P((fr_info_t *, ip_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_valid __P((ftpinfo_t *, int, char *, size_t));
int ippr_ftp_server_valid __P((ftpside_t *, char *, size_t));
int ippr_ftp_client_valid __P((ftpside_t *, char *, size_t));
u_short ippr_ftp_atoi __P((char **));
int ippr_ftp_pasvreply __P((fr_info_t *, ip_t *, nat_t *, ftpside_t *,
			    u_int, char *, char *, u_int));


int	ftp_proxy_init = 0;
int	ippr_ftp_pasvonly = 0;
int	ippr_ftp_insecure = 0;	/* Do not require logins before transfers */
int	ippr_ftp_pasvrdr = 0;
int	ippr_ftp_forcepasv = 0;	/* PASV must be last command prior to 227 */
#if defined(_KERNEL)
int	ippr_ftp_debug = 0;
#else
int	ippr_ftp_debug = 2;
#endif
/*
 * 1 - security
 * 2 - errors
 * 3 - error debugging
 * 4 - parsing errors
 * 5 - parsing info
 * 6 - parsing debug
 */

static	frentry_t	ftppxyfr;
static	ipftuneable_t	ftptune = {
	{ &ippr_ftp_debug },
	"ippr_ftp_debug",
	0,
	10,
	sizeof(ippr_ftp_debug),
	0,
	NULL
};


/*
 * Initialize local structures.
 */
int ippr_ftp_init()
{
	bzero((char *)&ftppxyfr, sizeof(ftppxyfr));
	ftppxyfr.fr_ref = 1;
	ftppxyfr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	MUTEX_INIT(&ftppxyfr.fr_lock, "FTP Proxy Mutex");
	ftp_proxy_init = 1;
	(void) fr_addipftune(&ftptune);

	return 0;
}


void ippr_ftp_fini()
{
	(void) fr_delipftune(&ftptune);

	if (ftp_proxy_init == 1) {
		MUTEX_DESTROY(&ftppxyfr.fr_lock);
		ftp_proxy_init = 0;
	}
}


int ippr_ftp_new(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;
	ftpside_t *f;

	KMALLOC(ftp, ftpinfo_t *);
	if (ftp == NULL)
		return -1;

	fin = fin;	/* LINT */
	nat = nat;	/* LINT */

	aps->aps_data = ftp;
	aps->aps_psiz = sizeof(ftpinfo_t);

	bzero((char *)ftp, sizeof(*ftp));
	f = &ftp->ftp_side[0];
	f->ftps_rptr = f->ftps_buf;
	f->ftps_wptr = f->ftps_buf;
	f = &ftp->ftp_side[1];
	f->ftps_rptr = f->ftps_buf;
	f->ftps_wptr = f->ftps_buf;
	ftp->ftp_passok = FTPXY_INIT;
	ftp->ftp_incok = 0;
	return 0;
}


int ippr_ftp_port(fin, ip, nat, f, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpside_t *f;
int dlen;
{
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	char newbuf[IPF_FTPBUFSZ], *s;
	struct in_addr swip, swip2;
	u_int a1, a2, a3, a4;
	int inc, off, flags;
	u_short a5, a6, sp;
	size_t nlen, olen;
	fr_info_t fi;
	nat_t *nat2;
	mb_t *m;

	m = fin->fin_m;
	tcp = (tcphdr_t *)fin->fin_dp;
	off = (char *)tcp - (char *)ip + (TCP_OFF(tcp) << 2) + fin->fin_ipoff;

	/*
	 * Check for client sending out PORT message.
	 */
	if (dlen < IPF_MINPORTLEN) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_port:dlen(%d) < IPF_MINPORTLEN\n",
			       dlen);
		return 0;
	}
	/*
	 * Skip the PORT command + space
	 */
	s = f->ftps_rptr + 5;
	/*
	 * Pick out the address components, two at a time.
	 */
	a1 = ippr_ftp_atoi(&s);
	if (s == NULL) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_port:ippr_ftp_atoi(%d) failed\n", 1);
		return 0;
	}
	a2 = ippr_ftp_atoi(&s);
	if (s == NULL) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_port:ippr_ftp_atoi(%d) failed\n", 2);
		return 0;
	}

	/*
	 * Check that IP address in the PORT/PASV reply is the same as the
	 * sender of the command - prevents using PORT for port scanning.
	 */
	a1 <<= 16;
	a1 |= a2;
	if (((nat->nat_dir == NAT_OUTBOUND) &&
	     (a1 != ntohl(nat->nat_inip.s_addr))) ||
	    ((nat->nat_dir == NAT_INBOUND) &&
	     (a1 != ntohl(nat->nat_oip.s_addr)))) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_port:%s != nat->nat_inip\n", "a1");
		return APR_ERR(1);
	}

	a5 = ippr_ftp_atoi(&s);
	if (s == NULL) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_port:ippr_ftp_atoi(%d) failed\n", 3);
		return 0;
	}
	if (*s == ')')
		s++;

	/*
	 * check for CR-LF at the end.
	 */
	if (*s == '\n')
		s--;
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
		a6 = a5 & 0xff;
	} else {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_port:missing %s\n", "cr-lf");
		return 0;
	}

	a5 >>= 8;
	a5 &= 0xff;
	sp = a5 << 8 | a6;
	/*
	 * Don't allow the PORT command to specify a port < 1024 due to
	 * security crap.
	 */
	if (sp < 1024) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_port:sp(%d) < 1024\n", sp);
		return 0;
	}
	/*
	 * Calculate new address parts for PORT command
	 */
	if (nat->nat_dir == NAT_INBOUND)
		a1 = ntohl(nat->nat_oip.s_addr);
	else
		a1 = ntohl(ip->ip_src.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	olen = s - f->ftps_rptr;
	/* DO NOT change this to snprintf! */
#if defined(SNPRINTF) && defined(_KERNEL)
	SNPRINTF(newbuf, sizeof(newbuf), "%s %u,%u,%u,%u,%u,%u\r\n",
		 "PORT", a1, a2, a3, a4, a5, a6);
#else
	(void) sprintf(newbuf, "%s %u,%u,%u,%u,%u,%u\r\n",
		       "PORT", a1, a2, a3, a4, a5, a6);
#endif

	nlen = strlen(newbuf);
	inc = nlen - olen;
	if ((inc + ip->ip_len) > 65535) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_port:inc(%d) + ip->ip_len > 65535\n",
			       inc);
		return 0;
	}

#if !defined(_KERNEL)
	bcopy(newbuf, MTOD(m, char *) + off, nlen);
#else
# if defined(MENTAT)
	if (inc < 0)
		(void)adjmsg(m, inc);
# else /* defined(MENTAT) */
	/*
	 * m_adj takes care of pkthdr.len, if required and treats inc<0 to
	 * mean remove -len bytes from the end of the packet.
	 * The mbuf chain will be extended if necessary by m_copyback().
	 */
	if (inc < 0)
		m_adj(m, inc);
# endif /* defined(MENTAT) */
#endif /* !defined(_KERNEL) */
	COPYBACK(m, off, nlen, newbuf);

	if (inc != 0) {
		ip->ip_len += inc;
		fin->fin_dlen += inc;
		fin->fin_plen += inc;
	}

	/*
	 * The server may not make the connection back from port 20, but
	 * it is the most likely so use it here to check for a conflicting
	 * mapping.
	 */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_state = NULL;
	fi.fin_nat = NULL;
	fi.fin_flx |= FI_IGNORE;
	fi.fin_data[0] = sp;
	fi.fin_data[1] = fin->fin_data[1] - 1;
	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	if (nat->nat_dir == NAT_OUTBOUND)
		nat2 = nat_outlookup(&fi, NAT_SEARCH|IPN_TCP, nat->nat_p,
				     nat->nat_inip, nat->nat_oip);
	else
		nat2 = nat_inlookup(&fi, NAT_SEARCH|IPN_TCP, nat->nat_p,
				    nat->nat_inip, nat->nat_oip);
	if (nat2 == NULL) {
		int slen;

		slen = ip->ip_len;
		ip->ip_len = fin->fin_hlen + sizeof(*tcp2);
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = htons(sp);
		TCP_OFF_A(tcp2, 5);
		tcp2->th_flags = TH_SYN;
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		fi.fin_data[1] = 0;
		fi.fin_dlen = sizeof(*tcp2);
		fi.fin_plen = fi.fin_hlen + sizeof(*tcp2);
		fi.fin_dp = (char *)tcp2;
		fi.fin_fr = &ftppxyfr;
		fi.fin_out = nat->nat_dir;
		fi.fin_flx &= FI_LOWTTL|FI_FRAG|FI_TCPUDP|FI_OPTIONS|FI_IGNORE;
		swip = ip->ip_src;
		swip2 = ip->ip_dst;
		if (nat->nat_dir == NAT_OUTBOUND) {
			fi.fin_fi.fi_saddr = nat->nat_inip.s_addr;
			ip->ip_src = nat->nat_inip;
		} else if (nat->nat_dir == NAT_INBOUND) {
			fi.fin_fi.fi_saddr = nat->nat_oip.s_addr;
			ip->ip_src = nat->nat_oip;
		}

		flags = NAT_SLAVE|IPN_TCP|SI_W_DPORT;
		if (nat->nat_dir == NAT_INBOUND)
			flags |= NAT_NOTRULEPORT;
		nat2 = nat_new(&fi, nat->nat_ptr, NULL, flags, nat->nat_dir);

		if (nat2 != NULL) {
			(void) nat_proto(&fi, nat2, IPN_TCP);
			nat_update(&fi, nat2, nat->nat_ptr);
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
	} else {
		ipstate_t *is;

		nat_update(&fi, nat2, nat->nat_ptr);
		READ_ENTER(&ipf_state);
		is = nat2->nat_state;
		if (is != NULL) {
			MUTEX_ENTER(&is->is_lock);
			(void)fr_tcp_age(&is->is_sti, &fi, ips_tqtqb,
					 is->is_flags);
			MUTEX_EXIT(&is->is_lock);
		}
		RWLOCK_EXIT(&ipf_state);
	}
	return APR_INC(inc);
}


int ippr_ftp_client(fin, ip, nat, ftp, dlen)
fr_info_t *fin;
nat_t *nat;
ftpinfo_t *ftp;
ip_t *ip;
int dlen;
{
	char *rptr, *wptr, cmd[6], c;
	ftpside_t *f;
	int inc, i;

	inc = 0;
	f = &ftp->ftp_side[0];
	rptr = f->ftps_rptr;
	wptr = f->ftps_wptr;

	for (i = 0; (i < 5) && (i < dlen); i++) {
		c = rptr[i];
		if (ISALPHA(c)) {
			cmd[i] = TOUPPER(c);
		} else {
			cmd[i] = c;
		}
	}
	cmd[i] = '\0';

	ftp->ftp_incok = 0;
	if (!strncmp(cmd, "USER ", 5) || !strncmp(cmd, "XAUT ", 5)) {
		if (ftp->ftp_passok == FTPXY_ADOK_1 ||
		    ftp->ftp_passok == FTPXY_AUOK_1) {
			ftp->ftp_passok = FTPXY_USER_2;
			ftp->ftp_incok = 1;
		} else {
			ftp->ftp_passok = FTPXY_USER_1;
			ftp->ftp_incok = 1;
		}
	} else if (!strncmp(cmd, "AUTH ", 5)) {
		ftp->ftp_passok = FTPXY_AUTH_1;
		ftp->ftp_incok = 1;
	} else if (!strncmp(cmd, "PASS ", 5)) {
		if (ftp->ftp_passok == FTPXY_USOK_1) {
			ftp->ftp_passok = FTPXY_PASS_1;
			ftp->ftp_incok = 1;
		} else if (ftp->ftp_passok == FTPXY_USOK_2) {
			ftp->ftp_passok = FTPXY_PASS_2;
			ftp->ftp_incok = 1;
		}
	} else if ((ftp->ftp_passok == FTPXY_AUOK_1) &&
		   !strncmp(cmd, "ADAT ", 5)) {
		ftp->ftp_passok = FTPXY_ADAT_1;
		ftp->ftp_incok = 1;
	} else if ((ftp->ftp_passok == FTPXY_PAOK_1 ||
		    ftp->ftp_passok == FTPXY_PAOK_2) &&
		 !strncmp(cmd, "ACCT ", 5)) {
		ftp->ftp_passok = FTPXY_ACCT_1;
		ftp->ftp_incok = 1;
	} else if ((ftp->ftp_passok == FTPXY_GO) && !ippr_ftp_pasvonly &&
		 !strncmp(cmd, "PORT ", 5)) {
		inc = ippr_ftp_port(fin, ip, nat, f, dlen);
	} else if (ippr_ftp_insecure && !ippr_ftp_pasvonly &&
		   !strncmp(cmd, "PORT ", 5)) {
		inc = ippr_ftp_port(fin, ip, nat, f, dlen);
	}

	while ((*rptr++ != '\n') && (rptr < wptr))
		;
	f->ftps_rptr = rptr;
	return inc;
}


int ippr_ftp_pasv(fin, ip, nat, ftp, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpinfo_t *ftp;
int dlen;
{
	u_int a1, a2, a3, a4, data_ip;
	char newbuf[IPF_FTPBUFSZ];
	const char *brackets[2];
	u_short a5, a6;
	ftpside_t *f;
	char *s;

	if (ippr_ftp_forcepasv != 0 &&
	    ftp->ftp_side[0].ftps_cmds != FTPXY_C_PASV) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_pasv:ftps_cmds(%d) != FTPXY_C_PASV\n",
			       ftp->ftp_side[0].ftps_cmds);
		return 0;
	}

	f = &ftp->ftp_side[1];

#define	PASV_REPLEN	24
	/*
	 * Check for PASV reply message.
	 */
	if (dlen < IPF_MIN227LEN) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_pasv:dlen(%d) < IPF_MIN227LEN\n",
			       dlen);
		return 0;
	} else if (strncmp(f->ftps_rptr,
			   "227 Entering Passive Mod", PASV_REPLEN)) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_pasv:%d reply wrong\n", 227);
		return 0;
	}

	brackets[0] = "";
	brackets[1] = "";
	/*
	 * Skip the PASV reply + space
	 */
	s = f->ftps_rptr + PASV_REPLEN;
	while (*s && !ISDIGIT(*s)) {
		if (*s == '(') {
			brackets[0] = "(";
			brackets[1] = ")";
		}
		s++;
	}

	/*
	 * Pick out the address components, two at a time.
	 */
	a1 = ippr_ftp_atoi(&s);
	if (s == NULL) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_pasv:ippr_ftp_atoi(%d) failed\n", 1);
		return 0;
	}
	a2 = ippr_ftp_atoi(&s);
	if (s == NULL) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_pasv:ippr_ftp_atoi(%d) failed\n", 2);
		return 0;
	}

	/*
	 * check that IP address in the PASV reply is the same as the
	 * sender of the command - prevents using PASV for port scanning.
	 */
	a1 <<= 16;
	a1 |= a2;

	if (((nat->nat_dir == NAT_INBOUND) &&
	     (a1 != ntohl(nat->nat_inip.s_addr))) ||
	    ((nat->nat_dir == NAT_OUTBOUND) &&
	     (a1 != ntohl(nat->nat_oip.s_addr)))) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_pasv:%s != nat->nat_oip\n", "a1");
		return 0;
	}

	a5 = ippr_ftp_atoi(&s);
	if (s == NULL) {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_pasv:ippr_ftp_atoi(%d) failed\n", 3);
		return 0;
	}

	if (*s == ')')
		s++;
	if (*s == '.')
		s++;
	if (*s == '\n')
		s--;
	/*
	 * check for CR-LF at the end.
	 */
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
	} else {
		if (ippr_ftp_debug > 1)
			printf("ippr_ftp_pasv:missing %s", "cr-lf\n");
		return 0;
	}

	a6 = a5 & 0xff;
	a5 >>= 8;
	/*
	 * Calculate new address parts for 227 reply
	 */
	if (nat->nat_dir == NAT_INBOUND) {
		data_ip = nat->nat_outip.s_addr;
		a1 = ntohl(data_ip);
	} else
		data_ip = htonl(a1);

	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;

#if defined(SNPRINTF) && defined(_KERNEL)
	SNPRINTF(newbuf, sizeof(newbuf), "%s %s%u,%u,%u,%u,%u,%u%s\r\n",
		"227 Entering Passive Mode", brackets[0], a1, a2, a3, a4,
		a5, a6, brackets[1]);
#else
	(void) sprintf(newbuf, "%s %s%u,%u,%u,%u,%u,%u%s\r\n",
		"227 Entering Passive Mode", brackets[0], a1, a2, a3, a4,
		a5, a6, brackets[1]);
#endif
	return ippr_ftp_pasvreply(fin, ip, nat, f, (a5 << 8 | a6),
				  newbuf, s, data_ip);
}

int ippr_ftp_pasvreply(fin, ip, nat, f, port, newmsg, s, data_ip)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpside_t *f;
u_int port;
char *newmsg;
char *s;
u_int data_ip;
{
	int inc, off, nflags, sflags;
	tcphdr_t *tcp, tcph, *tcp2;
	struct in_addr swip, swip2;
	struct in_addr data_addr;
	size_t nlen, olen;
	fr_info_t fi;
	nat_t *nat2;
	mb_t *m;

	m = fin->fin_m;
	tcp = (tcphdr_t *)fin->fin_dp;
	off = (char *)tcp - (char *)ip + (TCP_OFF(tcp) << 2) + fin->fin_ipoff;

	data_addr.s_addr = data_ip;
	tcp2 = &tcph;
	inc = 0;


	olen = s - f->ftps_rptr;
	nlen = strlen(newmsg);
	inc = nlen - olen;
	if ((inc + ip->ip_len) > 65535) {
		if (ippr_ftp_debug > 0)
			printf("ippr_ftp_pasv:inc(%d) + ip->ip_len > 65535\n",
			       inc);
		return 0;
	}

#if !defined(_KERNEL)
	bcopy(newmsg, MTOD(m, char *) + off, nlen);
#else
# if defined(MENTAT)
	if (inc < 0)
		(void)adjmsg(m, inc);
# else /* defined(MENTAT) */
	/*
	 * m_adj takes care of pkthdr.len, if required and treats inc<0 to
	 * mean remove -len bytes from the end of the packet.
	 * The mbuf chain will be extended if necessary by m_copyback().
	 */
	if (inc < 0)
		m_adj(m, inc);
# endif /* defined(MENTAT) */
#endif /* !defined(_KERNEL) */
	COPYBACK(m, off, nlen, newmsg);

	if (inc != 0) {
		ip->ip_len += inc;
		fin->fin_dlen += inc;
		fin->fin_plen += inc;
	}

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_state = NULL;
	fi.fin_nat = NULL;
	fi.fin_flx |= FI_IGNORE;
	fi.fin_data[0] = 0;
	fi.fin_data[1] = port;
	nflags = IPN_TCP|SI_W_SPORT;
	if (ippr_ftp_pasvrdr && f->ftps_ifp)
		nflags |= SI_W_DPORT;
	if (nat->nat_dir == NAT_OUTBOUND)
		nat2 = nat_outlookup(&fi, nflags|NAT_SEARCH,
				     nat->nat_p, nat->nat_inip, nat->nat_oip);
	else
		nat2 = nat_inlookup(&fi, nflags|NAT_SEARCH,
				    nat->nat_p, nat->nat_inip, nat->nat_oip);
	if (nat2 == NULL) {
		int slen;

		slen = ip->ip_len;
		ip->ip_len = fin->fin_hlen + sizeof(*tcp2);
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = 0;		/* XXX - fake it for nat_new */
		TCP_OFF_A(tcp2, 5);
		tcp2->th_flags = TH_SYN;
		fi.fin_data[1] = port;
		fi.fin_dlen = sizeof(*tcp2);
		tcp2->th_dport = htons(port);
		fi.fin_data[0] = 0;
		fi.fin_dp = (char *)tcp2;
		fi.fin_plen = fi.fin_hlen + sizeof(*tcp);
		fi.fin_fr = &ftppxyfr;
		fi.fin_out = nat->nat_dir;
		fi.fin_flx &= FI_LOWTTL|FI_FRAG|FI_TCPUDP|FI_OPTIONS|FI_IGNORE;
		swip = ip->ip_src;
		swip2 = ip->ip_dst;
		if (nat->nat_dir == NAT_OUTBOUND) {
			fi.fin_fi.fi_daddr = data_addr.s_addr;
			fi.fin_fi.fi_saddr = nat->nat_inip.s_addr;
			ip->ip_dst = data_addr;
			ip->ip_src = nat->nat_inip;
		} else if (nat->nat_dir == NAT_INBOUND) {
			fi.fin_fi.fi_saddr = nat->nat_oip.s_addr;
			fi.fin_fi.fi_daddr = nat->nat_outip.s_addr;
			ip->ip_src = nat->nat_oip;
			ip->ip_dst = nat->nat_outip;
		}

		sflags = nflags;
		nflags |= NAT_SLAVE;
		if (nat->nat_dir == NAT_INBOUND)
			nflags |= NAT_NOTRULEPORT;
		nat2 = nat_new(&fi, nat->nat_ptr, NULL, nflags, nat->nat_dir);
		if (nat2 != NULL) {
			(void) nat_proto(&fi, nat2, IPN_TCP);
			nat_update(&fi, nat2, nat->nat_ptr);
			fi.fin_ifp = NULL;
			if (nat->nat_dir == NAT_INBOUND) {
				fi.fin_fi.fi_daddr = nat->nat_inip.s_addr;
				ip->ip_dst = nat->nat_inip;
			}
			(void) fr_addstate(&fi, &nat2->nat_state, sflags);
			if (fi.fin_state != NULL)
				fr_statederef(&fi, (ipstate_t **)&fi.fin_state);
		}

		ip->ip_len = slen;
		ip->ip_src = swip;
		ip->ip_dst = swip2;
	} else {
		ipstate_t *is;

		nat_update(&fi, nat2, nat->nat_ptr);
		READ_ENTER(&ipf_state);
		is = nat2->nat_state;
		if (is != NULL) {
			MUTEX_ENTER(&is->is_lock);
			(void)fr_tcp_age(&is->is_sti, &fi, ips_tqtqb,
					 is->is_flags);
			MUTEX_EXIT(&is->is_lock);
		}
		RWLOCK_EXIT(&ipf_state);
	}
	return inc;
}


int ippr_ftp_server(fin, ip, nat, ftp, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpinfo_t *ftp;
int dlen;
{
	char *rptr, *wptr;
	ftpside_t *f;
	int inc;

	inc = 0;
	f = &ftp->ftp_side[1];
	rptr = f->ftps_rptr;
	wptr = f->ftps_wptr;

	if (*rptr == ' ')
		goto server_cmd_ok;
	if (!ISDIGIT(*rptr) || !ISDIGIT(*(rptr + 1)) || !ISDIGIT(*(rptr + 2)))
		return 0;
	if (ftp->ftp_passok == FTPXY_GO) {
		if (!strncmp(rptr, "227 ", 4))
			inc = ippr_ftp_pasv(fin, ip, nat, ftp, dlen);
		else if (!strncmp(rptr, "229 ", 4))
			inc = ippr_ftp_epsv(fin, ip, nat, f, dlen);
	} else if (ippr_ftp_insecure && !strncmp(rptr, "227 ", 4)) {
		inc = ippr_ftp_pasv(fin, ip, nat, ftp, dlen);
	} else if (ippr_ftp_insecure && !strncmp(rptr, "229 ", 4)) {
		inc = ippr_ftp_epsv(fin, ip, nat, f, dlen);
	} else if (*rptr == '5' || *rptr == '4')
		ftp->ftp_passok = FTPXY_INIT;
	else if (ftp->ftp_incok) {
		if (*rptr == '3') {
			if (ftp->ftp_passok == FTPXY_ACCT_1)
				ftp->ftp_passok = FTPXY_GO;
			else
				ftp->ftp_passok++;
		} else if (*rptr == '2') {
			switch (ftp->ftp_passok)
			{
			case FTPXY_USER_1 :
			case FTPXY_USER_2 :
			case FTPXY_PASS_1 :
			case FTPXY_PASS_2 :
			case FTPXY_ACCT_1 :
				ftp->ftp_passok = FTPXY_GO;
				break;
			default :
				ftp->ftp_passok += 3;
				break;
			}
		}
	}
server_cmd_ok:
	ftp->ftp_incok = 0;

	while ((*rptr++ != '\n') && (rptr < wptr))
		;
	f->ftps_rptr = rptr;
	return inc;
}


/*
 * Look to see if the buffer starts with something which we recognise as
 * being the correct syntax for the FTP protocol.
 */
int ippr_ftp_client_valid(ftps, buf, len)
ftpside_t *ftps;
char *buf;
size_t len;
{
	register char *s, c, pc;
	register size_t i = len;
	char cmd[5];

	s = buf;

	if (ftps->ftps_junk == 1)
		return 1;

	if (i < 5) {
		if (ippr_ftp_debug > 3)
			printf("ippr_ftp_client_valid:i(%d) < 5\n", (int)i);
		return 2;
	}

	i--;
	c = *s++;

	if (ISALPHA(c)) {
		cmd[0] = TOUPPER(c);
		c = *s++;
		i--;
		if (ISALPHA(c)) {
			cmd[1] = TOUPPER(c);
			c = *s++;
			i--;
			if (ISALPHA(c)) {
				cmd[2] = TOUPPER(c);
				c = *s++;
				i--;
				if (ISALPHA(c)) {
					cmd[3] = TOUPPER(c);
					c = *s++;
					i--;
					if ((c != ' ') && (c != '\r'))
						goto bad_client_command;
				} else if ((c != ' ') && (c != '\r'))
					goto bad_client_command;
			} else
				goto bad_client_command;
		} else
			goto bad_client_command;
	} else {
bad_client_command:
		if (ippr_ftp_debug > 3)
			printf("%s:bad:junk %d len %d/%d c 0x%x buf [%*.*s]\n",
			       "ippr_ftp_client_valid",
			       ftps->ftps_junk, (int)len, (int)i, c,
			       (int)len, (int)len, buf);
		return 1;
	}

	for (; i; i--) {
		pc = c;
		c = *s++;
		if ((pc == '\r') && (c == '\n')) {
			cmd[4] = '\0';
			if (!strcmp(cmd, "PASV"))
				ftps->ftps_cmds = FTPXY_C_PASV;
			else
				ftps->ftps_cmds = 0;
			return 0;
		}
	}
#if !defined(_KERNEL)
	printf("ippr_ftp_client_valid:junk after cmd[%*.*s]\n",
	       (int)len, (int)len, buf);
#endif
	return 2;
}


int ippr_ftp_server_valid(ftps, buf, len)
ftpside_t *ftps;
char *buf;
size_t len;
{
	register char *s, c, pc;
	register size_t i = len;
	int cmd;

	s = buf;
	cmd = 0;

	if (ftps->ftps_junk == 1)
		return 1;

	if (i < 5) {
		if (ippr_ftp_debug > 3)
			printf("ippr_ftp_servert_valid:i(%d) < 5\n", (int)i);
		return 2;
	}

	c = *s++;
	i--;
	if (c == ' ')
		goto search_eol;

	if (ISDIGIT(c)) {
		cmd = (c - '0') * 100;
		c = *s++;
		i--;
		if (ISDIGIT(c)) {
			cmd += (c - '0') * 10;
			c = *s++;
			i--;
			if (ISDIGIT(c)) {
				cmd += (c - '0');
				c = *s++;
				i--;
				if ((c != '-') && (c != ' '))
					goto bad_server_command;
			} else
				goto bad_server_command;
		} else
			goto bad_server_command;
	} else {
bad_server_command:
		if (ippr_ftp_debug > 3)
			printf("%s:bad:junk %d len %d/%d c 0x%x buf [%*.*s]\n",
			       "ippr_ftp_server_valid",
			       ftps->ftps_junk, (int)len, (int)i,
			       c, (int)len, (int)len, buf);
		return 1;
	}
search_eol:
	for (; i; i--) {
		pc = c;
		c = *s++;
		if ((pc == '\r') && (c == '\n')) {
			ftps->ftps_cmds = cmd;
			return 0;
		}
	}
	if (ippr_ftp_debug > 3)
		printf("ippr_ftp_server_valid:junk after cmd[%*.*s]\n",
		       (int)len, (int)len, buf);
	return 2;
}


int ippr_ftp_valid(ftp, side, buf, len)
ftpinfo_t *ftp;
int side;
char *buf;
size_t len;
{
	ftpside_t *ftps;
	int ret;

	ftps = &ftp->ftp_side[side];

	if (side == 0)
		ret = ippr_ftp_client_valid(ftps, buf, len);
	else
		ret = ippr_ftp_server_valid(ftps, buf, len);
	return ret;
}


/*
 * For map rules, the following applies:
 * rv == 0 for outbound processing,
 * rv == 1 for inbound processing.
 * For rdr rules, the following applies:
 * rv == 0 for inbound processing,
 * rv == 1 for outbound processing.
 */
int ippr_ftp_process(fin, nat, ftp, rv)
fr_info_t *fin;
nat_t *nat;
ftpinfo_t *ftp;
int rv;
{
	int mlen, len, off, inc, i, sel, sel2, ok, ackoff, seqoff;
	char *rptr, *wptr, *s;
	u_32_t thseq, thack;
	ap_session_t *aps;
	ftpside_t *f, *t;
	tcphdr_t *tcp;
	ip_t *ip;
	mb_t *m;

	m = fin->fin_m;
	ip = fin->fin_ip;
	tcp = (tcphdr_t *)fin->fin_dp;
	off = (char *)tcp - (char *)ip + (TCP_OFF(tcp) << 2) + fin->fin_ipoff;

	f = &ftp->ftp_side[rv];
	t = &ftp->ftp_side[1 - rv];
	thseq = ntohl(tcp->th_seq);
	thack = ntohl(tcp->th_ack);

#ifdef __sgi
	mlen = fin->fin_plen - off;
#else
	mlen = MSGDSIZE(m) - off;
#endif
	if (ippr_ftp_debug > 4)
		printf("ippr_ftp_process: mlen %d\n", mlen);

	if (mlen <= 0) {
		if ((tcp->th_flags & TH_OPENING) == TH_OPENING) {
			f->ftps_seq[0] = thseq + 1;
			t->ftps_seq[0] = thack;
		}
		return 0;
	}
	aps = nat->nat_aps;

	sel = aps->aps_sel[1 - rv];
	sel2 = aps->aps_sel[rv];
	if (rv == 0) {
		seqoff = aps->aps_seqoff[sel];
		if (aps->aps_seqmin[sel] > seqoff + thseq)
			seqoff = aps->aps_seqoff[!sel];
		ackoff = aps->aps_ackoff[sel2];
		if (aps->aps_ackmin[sel2] > ackoff + thack)
			ackoff = aps->aps_ackoff[!sel2];
	} else {
		seqoff = aps->aps_ackoff[sel];
		if (ippr_ftp_debug > 2)
			printf("seqoff %d thseq %x ackmin %x\n", seqoff, thseq,
			       aps->aps_ackmin[sel]);
		if (aps->aps_ackmin[sel] > seqoff + thseq)
			seqoff = aps->aps_ackoff[!sel];

		ackoff = aps->aps_seqoff[sel2];
		if (ippr_ftp_debug > 2)
			printf("ackoff %d thack %x seqmin %x\n", ackoff, thack,
			       aps->aps_seqmin[sel2]);
		if (ackoff > 0) {
			if (aps->aps_seqmin[sel2] > ackoff + thack)
				ackoff = aps->aps_seqoff[!sel2];
		} else {
			if (aps->aps_seqmin[sel2] > thack)
				ackoff = aps->aps_seqoff[!sel2];
		}
	}
	if (ippr_ftp_debug > 2) {
		printf("%s: %x seq %x/%d ack %x/%d len %d/%d off %d\n",
		       rv ? "IN" : "OUT", tcp->th_flags, thseq, seqoff,
		       thack, ackoff, mlen, fin->fin_plen, off);
		printf("sel %d seqmin %x/%x offset %d/%d\n", sel,
		       aps->aps_seqmin[sel], aps->aps_seqmin[sel2],
		       aps->aps_seqoff[sel], aps->aps_seqoff[sel2]);
		printf("sel %d ackmin %x/%x offset %d/%d\n", sel2,
		       aps->aps_ackmin[sel], aps->aps_ackmin[sel2],
		       aps->aps_ackoff[sel], aps->aps_ackoff[sel2]);
	}

	/*
	 * XXX - Ideally, this packet should get dropped because we now know
	 * that it is out of order (and there is no real danger in doing so
	 * apart from causing packets to go through here ordered).
	 */
	if (ippr_ftp_debug > 2) {
		printf("rv %d t:seq[0] %x seq[1] %x %d/%d\n",
		       rv, t->ftps_seq[0], t->ftps_seq[1], seqoff, ackoff);
	}

	ok = 0;
	if (t->ftps_seq[0] == 0) {
		t->ftps_seq[0] = thack;
		ok = 1;
	} else {
		if (ackoff == 0) {
			if (t->ftps_seq[0] == thack)
				ok = 1;
			else if (t->ftps_seq[1] == thack) {
				t->ftps_seq[0] = thack;
				ok = 1;
			}
		} else {
			if (t->ftps_seq[0] + ackoff == thack)
				ok = 1;
			else if (t->ftps_seq[0] == thack + ackoff)
				ok = 1;
			else if (t->ftps_seq[1] + ackoff == thack) {
				t->ftps_seq[0] = thack - ackoff;
				ok = 1;
			} else if (t->ftps_seq[1] == thack + ackoff) {
				t->ftps_seq[0] = thack - ackoff;
				ok = 1;
			}
		}
	}

	if (ippr_ftp_debug > 2) {
		if (!ok)
			printf("%s ok\n", "not");
	}

	if (!mlen) {
		if (t->ftps_seq[0] + ackoff != thack) {
			if (ippr_ftp_debug > 1) {
				printf("%s:seq[0](%x) + (%x) != (%x)\n",
				       "ippr_ftp_process", t->ftps_seq[0],
				       ackoff, thack);
			}
			return APR_ERR(1);
		}

		if (ippr_ftp_debug > 2) {
			printf("ippr_ftp_process:f:seq[0] %x seq[1] %x\n",
				f->ftps_seq[0], f->ftps_seq[1]);
		}

		if (tcp->th_flags & TH_FIN) {
			if (thseq == f->ftps_seq[1]) {
				f->ftps_seq[0] = f->ftps_seq[1] - seqoff;
				f->ftps_seq[1] = thseq + 1 - seqoff;
			} else {
				if (ippr_ftp_debug > 1) {
					printf("FIN: thseq %x seqoff %d ftps_seq %x\n",
					       thseq, seqoff, f->ftps_seq[0]);
				}
				return APR_ERR(1);
			}
		}
		f->ftps_len = 0;
		return 0;
	}

	ok = 0;
	if ((thseq == f->ftps_seq[0]) || (thseq == f->ftps_seq[1])) {
		ok = 1;
	/*
	 * Retransmitted data packet.
	 */
	} else if ((thseq + mlen == f->ftps_seq[0]) ||
		   (thseq + mlen == f->ftps_seq[1])) {
		ok = 1;
	}

	if (ok == 0) {
		inc = thseq - f->ftps_seq[0];
		if (ippr_ftp_debug > 1) {
			printf("inc %d sel %d rv %d\n", inc, sel, rv);
			printf("th_seq %x ftps_seq %x/%x\n",
			       thseq, f->ftps_seq[0], f->ftps_seq[1]);
			printf("ackmin %x ackoff %d\n", aps->aps_ackmin[sel],
			       aps->aps_ackoff[sel]);
			printf("seqmin %x seqoff %d\n", aps->aps_seqmin[sel],
			       aps->aps_seqoff[sel]);
		}

		return APR_ERR(1);
	}

	inc = 0;
	rptr = f->ftps_rptr;
	wptr = f->ftps_wptr;
	f->ftps_seq[0] = thseq;
	f->ftps_seq[1] = f->ftps_seq[0] + mlen;
	f->ftps_len = mlen;

	while (mlen > 0) {
		len = MIN(mlen, sizeof(f->ftps_buf) - (wptr - rptr));
		COPYDATA(m, off, len, wptr);
		mlen -= len;
		off += len;
		wptr += len;

		if (ippr_ftp_debug > 3)
			printf("%s:len %d/%d off %d wptr %lx junk %d [%*.*s]\n",
			       "ippr_ftp_process",
			       len, mlen, off, (u_long)wptr, f->ftps_junk,
			       len, len, rptr);

		f->ftps_wptr = wptr;
		if (f->ftps_junk != 0) {
			i = f->ftps_junk;
			f->ftps_junk = ippr_ftp_valid(ftp, rv, rptr,
						      wptr - rptr);

			if (ippr_ftp_debug > 5)
				printf("%s:junk %d -> %d\n",
				       "ippr_ftp_process", i, f->ftps_junk);

			if (f->ftps_junk != 0) {
				if (wptr - rptr == sizeof(f->ftps_buf)) {
					if (ippr_ftp_debug > 4)
						printf("%s:full buffer\n",
						       "ippr_ftp_process");
					f->ftps_rptr = f->ftps_buf;
					f->ftps_wptr = f->ftps_buf;
					rptr = f->ftps_rptr;
					wptr = f->ftps_wptr;
					/*
					 * Because we throw away data here that
					 * we would otherwise parse, set the
					 * junk flag to indicate just ignore
					 * any data upto the next CRLF.
					 */
					f->ftps_junk = 1;
					continue;
				}
			}
		}

		while ((f->ftps_junk == 0) && (wptr > rptr)) {
			len = wptr - rptr;
			f->ftps_junk = ippr_ftp_valid(ftp, rv, rptr, len);

			if (ippr_ftp_debug > 3) {
				printf("%s=%d len %d rv %d ptr %lx/%lx ",
				       "ippr_ftp_valid",
				       f->ftps_junk, len, rv, (u_long)rptr,
				       (u_long)wptr);
				printf("buf [%*.*s]\n", len, len, rptr);
			}

			if (f->ftps_junk == 0) {
				f->ftps_rptr = rptr;
				if (rv)
					inc += ippr_ftp_server(fin, ip, nat,
							       ftp, len);
				else
					inc += ippr_ftp_client(fin, ip, nat,
							       ftp, len);
				rptr = f->ftps_rptr;
				wptr = f->ftps_wptr;
			}
		}

		/*
		 * Off to a bad start so lets just forget about using the
		 * ftp proxy for this connection.
		 */
		if ((f->ftps_cmds == 0) && (f->ftps_junk == 1)) {
			/* f->ftps_seq[1] += inc; */

			if (ippr_ftp_debug > 1)
				printf("%s:cmds == 0 junk == 1\n",
				       "ippr_ftp_process");
			return APR_ERR(2);
		}

		if ((f->ftps_junk != 0) && (rptr < wptr)) {
			for (s = rptr; s < wptr; s++) {
				if ((*s == '\r') && (s + 1 < wptr) &&
				    (*(s + 1) == '\n')) {
					rptr = s + 2;
					f->ftps_junk = 0;
					break;
				}
			}
		}

		if (rptr == wptr) {
			rptr = wptr = f->ftps_buf;
		} else {
			/*
			 * Compact the buffer back to the start.  The junk
			 * flag should already be set and because we're not
			 * throwing away any data, it is preserved from its
			 * current state.
			 */
			if (rptr > f->ftps_buf) {
				bcopy(rptr, f->ftps_buf, len);
				wptr -= rptr - f->ftps_buf;
				rptr = f->ftps_buf;
			}
		}
		f->ftps_rptr = rptr;
		f->ftps_wptr = wptr;
	}

	/* f->ftps_seq[1] += inc; */
	if (tcp->th_flags & TH_FIN)
		f->ftps_seq[1]++;
	if (ippr_ftp_debug > 3) {
#ifdef __sgi
		mlen = fin->fin_plen;
#else
		mlen = MSGDSIZE(m);
#endif
		mlen -= off;
		printf("ftps_seq[1] = %x inc %d len %d\n",
		       f->ftps_seq[1], inc, mlen);
	}

	f->ftps_rptr = rptr;
	f->ftps_wptr = wptr;
	return APR_INC(inc);
}


int ippr_ftp_out(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;
	int rev;

	ftp = aps->aps_data;
	if (ftp == NULL)
		return 0;

	rev = (nat->nat_dir == NAT_OUTBOUND) ? 0 : 1;
	if (ftp->ftp_side[1 - rev].ftps_ifp == NULL)
		ftp->ftp_side[1 - rev].ftps_ifp = fin->fin_ifp;

	return ippr_ftp_process(fin, nat, ftp, rev);
}


int ippr_ftp_in(fin, aps, nat)
fr_info_t *fin;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;
	int rev;

	ftp = aps->aps_data;
	if (ftp == NULL)
		return 0;

	rev = (nat->nat_dir == NAT_OUTBOUND) ? 0 : 1;
	if (ftp->ftp_side[rev].ftps_ifp == NULL)
		ftp->ftp_side[rev].ftps_ifp = fin->fin_ifp;

	return ippr_ftp_process(fin, nat, ftp, 1 - rev);
}


/*
 * ippr_ftp_atoi - implement a version of atoi which processes numbers in
 * pairs separated by commas (which are expected to be in the range 0 - 255),
 * returning a 16 bit number combining either side of the , as the MSB and
 * LSB.
 */
u_short ippr_ftp_atoi(ptr)
char **ptr;
{
	register char *s = *ptr, c;
	register u_char i = 0, j = 0;

	while (((c = *s++) != '\0') && ISDIGIT(c)) {
		i *= 10;
		i += c - '0';
	}
	if (c != ',') {
		*ptr = NULL;
		return 0;
	}
	while (((c = *s++) != '\0') && ISDIGIT(c)) {
		j *= 10;
		j += c - '0';
	}
	*ptr = s;
	i &= 0xff;
	j &= 0xff;
	return (i << 8) | j;
}


int ippr_ftp_epsv(fin, ip, nat, f, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpside_t *f;
int dlen;
{
	char newbuf[IPF_FTPBUFSZ];
	char *s;
	u_short ap = 0;

#define EPSV_REPLEN	33
	/*
	 * Check for EPSV reply message.
	 */
	if (dlen < IPF_MIN229LEN)
		return (0);
	else if (strncmp(f->ftps_rptr,
			 "229 Entering Extended Passive Mode", EPSV_REPLEN))
		return (0);

	/*
	 * Skip the EPSV command + space
	 */
	s = f->ftps_rptr + 33;
	while (*s && !ISDIGIT(*s))
		s++;

	/*
	 * As per RFC 2428, there are no addres components in the EPSV
	 * response.  So we'll go straight to getting the port.
	 */
	while (*s && ISDIGIT(*s)) {
		ap *= 10;
		ap += *s++ - '0';
	}

	if (!s)
		return 0;

	if (*s == '|')
		s++;
	if (*s == ')')
		s++;
	if (*s == '\n')
		s--;
	/*
	 * check for CR-LF at the end.
	 */
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
	} else
		return 0;

#if defined(SNPRINTF) && defined(_KERNEL)
	SNPRINTF(newbuf, sizeof(newbuf), "%s (|||%u|)\r\n",
		 "229 Entering Extended Passive Mode", ap);
#else
	(void) sprintf(newbuf, "%s (|||%u|)\r\n",
		       "229 Entering Extended Passive Mode", ap);
#endif

	return ippr_ftp_pasvreply(fin, ip, nat, f, (u_int)ap, newbuf, s,
				  ip->ip_src.s_addr);
}
