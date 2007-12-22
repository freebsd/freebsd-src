/*
 * Copyright (C) 2002-2003 by Ryan Beasley <ryanb@goddamnbastard.org>
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/*
 * Overview:
 *   This is an in-kernel application proxy for Sun's RPCBIND (nee portmap)
 *   protocol as defined in RFC1833.  It is far from complete, mostly
 *   lacking in less-likely corner cases, but it's definitely functional.
 *
 *   Invocation:
 *     rdr <int> <e_ip>/32 port <e_p> -> <i_ip> port <i_p> udp proxy rpcbu
 *
 *   If the host running IP Filter is the same as the RPC server, it's
 *   perfectly legal for both the internal and external addresses and ports
 *   to match.
 *
 *   When triggered by appropriate IP NAT rules, this proxy works by
 *   examining data contained in received packets.  Requests and replies are
 *   modified, NAT and state table entries created, etc., as necessary.
 */
/*
 * TODO / NOTES
 *
 *   o Must implement locking to protect proxy session data.
 *   o Fragmentation isn't supported.
 *   o Only supports UDP.
 *   o Doesn't support multiple RPC records in a single request.
 *   o Errors should be more fine-grained.  (e.g., malloc failure vs.
 *     illegal RPCB request / reply)
 *   o Even with the limit on the total amount of recorded transactions,
 *     should there be a timeout on transaction removal?
 *   o There is a potential collision between cloning, wildcard NAT and
 *     state entries.  There should be an appr_getport routine for
 *     to avoid this.
 *   o The enclosed hack of STREAMS support is pretty sick and most likely
 *     broken.
 *
 *	$Id: ip_rpcb_pxy.c,v 2.25.2.7 2007/06/04 09:16:31 darrenr Exp $
 */

#define	IPF_RPCB_PROXY

/*
 * Function prototypes
 */
int	ippr_rpcb_init __P((void));
void	ippr_rpcb_fini __P((void));
int	ippr_rpcb_new __P((fr_info_t *, ap_session_t *, nat_t *));
void	ippr_rpcb_del __P((ap_session_t *));
int	ippr_rpcb_in __P((fr_info_t *, ap_session_t *, nat_t *));
int	ippr_rpcb_out __P((fr_info_t *, ap_session_t *, nat_t *));

static void	ippr_rpcb_flush __P((rpcb_session_t *));
static int	ippr_rpcb_decodereq __P((fr_info_t *, nat_t *,
	rpcb_session_t *, rpc_msg_t *));
static int	ippr_rpcb_skipauth __P((rpc_msg_t *, xdr_auth_t *, u_32_t **));
static int	ippr_rpcb_insert __P((rpcb_session_t *, rpcb_xact_t *));
static int	ippr_rpcb_xdrrpcb __P((rpc_msg_t *, u_32_t *, rpcb_args_t *));
static int	ippr_rpcb_getuaddr __P((rpc_msg_t *, xdr_uaddr_t *,
	u_32_t **));
static u_int	ippr_rpcb_atoi __P((char *));
static int	ippr_rpcb_modreq __P((fr_info_t *, nat_t *, rpc_msg_t *,
	mb_t *, u_int));
static int	ippr_rpcb_decoderep __P((fr_info_t *, nat_t *,
	rpcb_session_t *, rpc_msg_t *, rpcb_xact_t **));
static rpcb_xact_t *	ippr_rpcb_lookup __P((rpcb_session_t *, u_32_t));
static void	ippr_rpcb_deref __P((rpcb_session_t *, rpcb_xact_t *));
static int	ippr_rpcb_getproto __P((rpc_msg_t *, xdr_proto_t *,
	u_32_t **));
static int	ippr_rpcb_getnat __P((fr_info_t *, nat_t *, u_int, u_int));
static int	ippr_rpcb_modv3 __P((fr_info_t *, nat_t *, rpc_msg_t *,
	mb_t *, u_int));
static int	ippr_rpcb_modv4 __P((fr_info_t *, nat_t *, rpc_msg_t *,
	mb_t *, u_int));
static void     ippr_rpcb_fixlen __P((fr_info_t *, int));

/*
 * Global variables
 */
static	frentry_t	rpcbfr;	/* Skeleton rule for reference by entities
				   this proxy creates. */
static	int	rpcbcnt;	/* Upper bound of allocated RPCB sessions. */
				/* XXX rpcbcnt still requires locking. */

int	rpcb_proxy_init = 0;


/*
 * Since rpc_msg contains only pointers, one should use this macro as a
 * handy way to get to the goods.  (In case you're wondering about the name,
 * this started as BYTEREF -> BREF -> B.)
 */
#define	B(r)	(u_32_t)ntohl(*(r))

/*
 * Public subroutines
 */

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_init						*/
/* Returns:	int - 0 == success					*/
/* Parameters:	(void)							*/
/*									*/
/* Initialize the filter rule entry and session limiter.		*/
/* --------------------------------------------------------------------	*/
int
ippr_rpcb_init()
{
	rpcbcnt = 0;

	bzero((char *)&rpcbfr, sizeof(rpcbfr));
	rpcbfr.fr_ref = 1;
	rpcbfr.fr_flags = FR_PASS|FR_QUICK|FR_KEEPSTATE;
	MUTEX_INIT(&rpcbfr.fr_lock, "ipf Sun RPCB proxy rule lock");
	rpcb_proxy_init = 1;

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_fini						*/
/* Returns:	void							*/
/* Parameters:	(void)							*/
/*									*/
/* Destroy rpcbfr's mutex to avoid a lock leak.				*/
/* --------------------------------------------------------------------	*/
void
ippr_rpcb_fini()
{
	if (rpcb_proxy_init == 1) {
		MUTEX_DESTROY(&rpcbfr.fr_lock);
		rpcb_proxy_init = 0;
	}
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_new						*/
/* Returns:	int - -1 == failure, 0 == success			*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		aps(I)	- pointer to proxy session structure		*/
/*		nat(I)	- pointer to NAT session structure		*/
/*									*/
/* Allocate resources for per-session proxy structures.			*/
/* --------------------------------------------------------------------	*/
int
ippr_rpcb_new(fin, aps, nat)
	fr_info_t *fin;
	ap_session_t *aps;
	nat_t *nat;
{
	rpcb_session_t *rs;

	fin = fin;	/* LINT */
	nat = nat;	/* LINT */

	KMALLOC(rs, rpcb_session_t *);
	if (rs == NULL)
		return(-1);

	bzero((char *)rs, sizeof(*rs));
	MUTEX_INIT(&rs->rs_rxlock, "ipf Sun RPCB proxy session lock");

	aps->aps_data = rs;

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_del						*/
/* Returns:	void							*/
/* Parameters:	aps(I)	- pointer to proxy session structure		*/
/*									*/
/* Free up a session's list of RPCB requests.				*/
/* --------------------------------------------------------------------	*/
void
ippr_rpcb_del(aps)
	ap_session_t *aps;
{
	rpcb_session_t *rs;
	rs = (rpcb_session_t *)aps->aps_data;

	MUTEX_ENTER(&rs->rs_rxlock);
	ippr_rpcb_flush(rs);
	MUTEX_EXIT(&rs->rs_rxlock);
	MUTEX_DESTROY(&rs->rs_rxlock);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_in						*/
/* Returns:	int - APR_ERR(1) == drop the packet, 			*/
/*		      APR_ERR(2) == kill the proxy session,		*/
/*		      else change in packet length (in bytes)		*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		ip(I)	- pointer to packet header			*/
/*		aps(I)	- pointer to proxy session structure		*/
/*		nat(I)	- pointer to NAT session structure		*/
/*									*/
/* Given a presumed RPCB request, perform some minor tests and pass off */
/* for decoding.  Also pass packet off for a rewrite if necessary.	*/
/* --------------------------------------------------------------------	*/
int
ippr_rpcb_in(fin, aps, nat)
	fr_info_t *fin;
	ap_session_t *aps;
	nat_t *nat;
{
	rpc_msg_t rpcmsg, *rm;
	rpcb_session_t *rs;
	u_int off, dlen;
	mb_t *m;
	int rv;

	/* Disallow fragmented or illegally short packets. */
	if ((fin->fin_flx & (FI_FRAG|FI_SHORT)) != 0)
		return(APR_ERR(1));

	/* Perform basic variable initialization. */
	rs = (rpcb_session_t *)aps->aps_data;

	m = fin->fin_m;
	off = (char *)fin->fin_dp - (char *)fin->fin_ip;
	off += sizeof(udphdr_t) + fin->fin_ipoff;
	dlen = fin->fin_dlen - sizeof(udphdr_t);

	/* Disallow packets outside legal range for supported requests. */
	if ((dlen < RPCB_REQMIN) || (dlen > RPCB_REQMAX))
		return(APR_ERR(1));

	/* Copy packet over to convenience buffer. */
	rm = &rpcmsg;
	bzero((char *)rm, sizeof(*rm));
	COPYDATA(m, off, dlen, (caddr_t)&rm->rm_msgbuf);
	rm->rm_buflen = dlen;

	/* Send off to decode request. */
	rv = ippr_rpcb_decodereq(fin, nat, rs, rm);

	switch(rv)
	{
	case -1:
		return(APR_ERR(1));
		/*NOTREACHED*/
		break;
	case 0:
		break;
	case 1:
		rv = ippr_rpcb_modreq(fin, nat, rm, m, off);
		break;
	default:
		/*CONSTANTCONDITION*/
		IPF_PANIC(1, ("illegal rv %d (ippr_rpcb_req)", rv));
	}

	return(rv);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_out						*/
/* Returns:	int - APR_ERR(1) == drop the packet, 			*/
/*		      APR_ERR(2) == kill the proxy session,		*/
/*		      else change in packet length (in bytes)		*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		ip(I)	- pointer to packet header			*/
/*		aps(I)	- pointer to proxy session structure		*/
/*		nat(I)	- pointer to NAT session structure		*/
/*									*/
/* Given a presumed RPCB reply, perform some minor tests and pass off	*/
/* for decoding.  If the message indicates a successful request with	*/
/* valid addressing information, create NAT and state structures to	*/
/* allow direct communication between RPC client and server.		*/
/* --------------------------------------------------------------------	*/
int
ippr_rpcb_out(fin, aps, nat)
	fr_info_t *fin;
	ap_session_t *aps;
	nat_t *nat;
{
	rpc_msg_t rpcmsg, *rm;
	rpcb_session_t *rs;
	rpcb_xact_t *rx;
	u_int off, dlen;
	int rv, diff;
	mb_t *m;

	/* Disallow fragmented or illegally short packets. */
	if ((fin->fin_flx & (FI_FRAG|FI_SHORT)) != 0)
		return(APR_ERR(1));

	/* Perform basic variable initialization. */
	rs = (rpcb_session_t *)aps->aps_data;
	rx = NULL;

	m = fin->fin_m;
	off = (char *)fin->fin_dp - (char *)fin->fin_ip;
	off += sizeof(udphdr_t) + fin->fin_ipoff;
	dlen = fin->fin_dlen - sizeof(udphdr_t);
	diff = 0;

	/* Disallow packets outside legal range for supported requests. */
	if ((dlen < RPCB_REPMIN) || (dlen > RPCB_REPMAX))
		return(APR_ERR(1));

	/* Copy packet over to convenience buffer. */
	rm = &rpcmsg;
	bzero((char *)rm, sizeof(*rm));
	COPYDATA(m, off, dlen, (caddr_t)&rm->rm_msgbuf);
	rm->rm_buflen = dlen;

	rx = NULL;		/* XXX gcc */

	/* Send off to decode reply. */
	rv = ippr_rpcb_decoderep(fin, nat, rs, rm, &rx);

	switch(rv)
	{
	case -1: /* Bad packet */
                if (rx != NULL) {
                        MUTEX_ENTER(&rs->rs_rxlock);
                        ippr_rpcb_deref(rs, rx);
                        MUTEX_EXIT(&rs->rs_rxlock);
                }
		return(APR_ERR(1));
		/*NOTREACHED*/
		break;
	case  0: /* Negative reply / request rejected */
		break;
	case  1: /* Positive reply */
		/*
		 * With the IP address embedded in a GETADDR(LIST) reply,
		 * we'll need to rewrite the packet in the very possible
		 * event that the internal & external addresses aren't the
		 * same.  (i.e., this box is either a router or rpcbind
		 * only listens on loopback.)
		 */
		if (nat->nat_inip.s_addr != nat->nat_outip.s_addr) {
			if (rx->rx_type == RPCB_RES_STRING)
				diff = ippr_rpcb_modv3(fin, nat, rm, m, off);
			else if (rx->rx_type == RPCB_RES_LIST)
				diff = ippr_rpcb_modv4(fin, nat, rm, m, off);
		}
		break;
	default:
		/*CONSTANTCONDITION*/
		IPF_PANIC(1, ("illegal rv %d (ippr_rpcb_decoderep)", rv));
	}

	if (rx != NULL) {
                MUTEX_ENTER(&rs->rs_rxlock);
                /* XXX Gross hack - I'm overloading the reference
                 * counter to deal with both threads and retransmitted
                 * requests.  One deref signals that this thread is
                 * finished with rx, and the other signals that we've
                 * processed its reply.
                 */
                ippr_rpcb_deref(rs, rx);
                ippr_rpcb_deref(rs, rx);
                MUTEX_EXIT(&rs->rs_rxlock);
	}

	return(diff);
}

/*
 * Private support subroutines
 */

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_flush						*/
/* Returns:	void							*/
/* Parameters:	rs(I)	- pointer to RPCB session structure		*/
/*									*/
/* Simply flushes the list of outstanding transactions, if any.		*/
/* --------------------------------------------------------------------	*/
static void
ippr_rpcb_flush(rs)
	rpcb_session_t *rs;
{
	rpcb_xact_t *r1, *r2;

	r1 = rs->rs_rxlist;
	if (r1 == NULL)
		return;

	while (r1 != NULL) {
		r2 = r1;
		r1 = r1->rx_next;
		KFREE(r2);
	}
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_decodereq					*/
/* Returns:	int - -1 == bad request or critical failure,		*/
/*		       0 == request successfully decoded,		*/
/*		       1 == request successfully decoded; requires	*/
/*			    address rewrite/modification		*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		nat(I)	- pointer to NAT session structure		*/
/*		rs(I)	- pointer to RPCB session structure		*/
/*		rm(I)	- pointer to RPC message structure		*/
/*									*/
/* Take a presumed RPCB request, decode it, and store the results in	*/
/* the transaction list.  If the internal target address needs to be	*/
/* modified, store its location in ptr.					*/
/* WARNING:  It's the responsibility of the caller to make sure there	*/
/* is enough room in rs_buf for the basic RPC message "preamble".	*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_decodereq(fin, nat, rs, rm)
	fr_info_t *fin;
	nat_t *nat;
	rpcb_session_t *rs;
	rpc_msg_t *rm;
{
	rpcb_args_t *ra;
	u_32_t xdr, *p;
	rpc_call_t *rc;
	rpcb_xact_t rx;
	int mod;

	p = (u_32_t *)rm->rm_msgbuf;
	mod = 0;

	bzero((char *)&rx, sizeof(rx));
	rc = &rm->rm_call;

	rm->rm_xid = p;
	rx.rx_xid = B(p++);	/* Record this message's XID. */

	/* Parse out and test the RPC header. */
	if ((B(p++) != RPCB_CALL) ||
	    (B(p++) != RPCB_MSG_VERSION) ||
	    (B(p++) != RPCB_PROG))
		return(-1);

	/* Record the RPCB version and procedure. */
	rc->rc_vers = p++;
	rc->rc_proc = p++;

	/* Bypass RPC authentication stuff. */
	if (ippr_rpcb_skipauth(rm, &rc->rc_authcred, &p) != 0)
		return(-1);
	if (ippr_rpcb_skipauth(rm, &rc->rc_authverf, &p) != 0)
		return(-1);

	/* Compare RPCB version and procedure numbers. */
	switch(B(rc->rc_vers))
	{
	case 2:
		/* This proxy only supports PMAP_GETPORT. */
		if (B(rc->rc_proc) != RPCB_GETPORT)
			return(-1);

		/* Portmap requests contain four 4 byte parameters. */
		if (RPCB_BUF_EQ(rm, p, 16) == 0)
			return(-1);

		p += 2; /* Skip requested program and version numbers. */

		/* Sanity check the requested protocol. */
		xdr = B(p);
		if (!(xdr == IPPROTO_UDP || xdr == IPPROTO_TCP))
			return(-1);

		rx.rx_type = RPCB_RES_PMAP;
		rx.rx_proto = xdr;
		break;
	case 3:
	case 4:
		/* GETADDRLIST is exclusive to v4; GETADDR for v3 & v4 */
		switch(B(rc->rc_proc))
		{
		case RPCB_GETADDR:
			rx.rx_type = RPCB_RES_STRING;
			rx.rx_proto = (u_int)fin->fin_p;
			break;
		case RPCB_GETADDRLIST:
			if (B(rc->rc_vers) != 4)
				return(-1);
			rx.rx_type = RPCB_RES_LIST;
			break;
		default:
			return(-1);
		}

		ra = &rc->rc_rpcbargs;

		/* Decode the 'struct rpcb' request. */
		if (ippr_rpcb_xdrrpcb(rm, p, ra) != 0)
			return(-1);

		/* Are the target address & port valid? */
		if ((ra->ra_maddr.xu_ip != nat->nat_outip.s_addr) ||
		    (ra->ra_maddr.xu_port != nat->nat_outport))
		    	return(-1);

		/* Do we need to rewrite this packet? */
		if ((nat->nat_outip.s_addr != nat->nat_inip.s_addr) ||
		    (nat->nat_outport != nat->nat_inport))
		    	mod = 1;
		break;
	default:
		return(-1);
	}

        MUTEX_ENTER(&rs->rs_rxlock);
	if (ippr_rpcb_insert(rs, &rx) != 0) {
                MUTEX_EXIT(&rs->rs_rxlock);
		return(-1);
	}
        MUTEX_EXIT(&rs->rs_rxlock);

	return(mod);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_skipauth					*/
/* Returns:	int -- -1 == illegal auth parameters (lengths)		*/
/*			0 == valid parameters, pointer advanced		*/
/* Parameters:	rm(I)	- pointer to RPC message structure		*/
/*		auth(I)	- pointer to RPC auth structure			*/
/*		buf(IO)	- pointer to location within convenience buffer	*/
/*									*/
/* Record auth data length & location of auth data, then advance past	*/
/* it.									*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_skipauth(rm, auth, buf)
	rpc_msg_t *rm;
	xdr_auth_t *auth;
	u_32_t **buf;
{
	u_32_t *p, xdr;

	p = *buf;

	/* Make sure we have enough space for expected fixed auth parms. */
	if (RPCB_BUF_GEQ(rm, p, 8) == 0)
		return(-1);

	p++; /* We don't care about auth_flavor. */

	auth->xa_string.xs_len = p;
	xdr = B(p++);		/* Length of auth_data */

	/* Test for absurdity / illegality of auth_data length. */
	if ((XDRALIGN(xdr) < xdr) || (RPCB_BUF_GEQ(rm, p, XDRALIGN(xdr)) == 0))
		return(-1);

	auth->xa_string.xs_str = (char *)p;

	p += XDRALIGN(xdr);	/* Advance our location. */

	*buf = (u_32_t *)p;

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_insert					*/
/* Returns:	int -- -1 == list insertion failed,			*/
/*			0 == item successfully added			*/
/* Parameters:	rs(I)	- pointer to RPCB session structure		*/
/*		rx(I)	- pointer to RPCB transaction structure		*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_insert(rs, rx)
	rpcb_session_t *rs;
	rpcb_xact_t *rx;
{
	rpcb_xact_t *rxp;

	rxp = ippr_rpcb_lookup(rs, rx->rx_xid);
	if (rxp != NULL) {
                ++rxp->rx_ref;
		return(0);
        }

	if (rpcbcnt == RPCB_MAXREQS)
		return(-1);

	KMALLOC(rxp, rpcb_xact_t *);
	if (rxp == NULL)
		return(-1);

	bcopy((char *)rx, (char *)rxp, sizeof(*rx));

	if (rs->rs_rxlist != NULL)
		rs->rs_rxlist->rx_pnext = &rxp->rx_next;

	rxp->rx_pnext = &rs->rs_rxlist;
	rxp->rx_next = rs->rs_rxlist;
	rs->rs_rxlist = rxp;

	rxp->rx_ref = 1;

	++rpcbcnt;

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_xdrrpcb					*/
/* Returns:	int -- -1 == failure to properly decode the request	*/
/*			0 == rpcb successfully decoded			*/
/* Parameters:	rs(I)	- pointer to RPCB session structure		*/
/*		p(I)	- pointer to location within session buffer	*/
/*		rpcb(O)	- pointer to rpcb (xdr type) structure		*/
/*									*/
/* Decode a XDR encoded rpcb structure and record its contents in rpcb  */
/* within only the context of TCP/UDP over IP networks.			*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_xdrrpcb(rm, p, ra)
	rpc_msg_t *rm;
	u_32_t *p;
	rpcb_args_t *ra;
{
	if (!RPCB_BUF_GEQ(rm, p, 20))
		return(-1);

	/* Bypass target program & version. */
	p += 2;

	/* Decode r_netid.  Must be "tcp" or "udp". */
	if (ippr_rpcb_getproto(rm, &ra->ra_netid, &p) != 0)
		return(-1);

	/* Decode r_maddr. */
	if (ippr_rpcb_getuaddr(rm, &ra->ra_maddr, &p) != 0)
		return(-1);

	/* Advance to r_owner and make sure it's empty. */
	if (!RPCB_BUF_EQ(rm, p, 4) || (B(p) != 0))
		return(-1);

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_getuaddr					*/
/* Returns:	int -- -1 == illegal string,				*/
/*			0 == string parsed; contents recorded		*/
/* Parameters:	rm(I)	- pointer to RPC message structure		*/
/*		xu(I)	- pointer to universal address structure	*/
/*		p(IO)	- pointer to location within message buffer	*/
/*									*/
/* Decode the IP address / port at p and record them in xu.		*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_getuaddr(rm, xu, p)
	rpc_msg_t *rm;
	xdr_uaddr_t *xu;
	u_32_t **p;
{
	char *c, *i, *b, *pp;
	u_int d, dd, l, t;
	char uastr[24];

	/* Test for string length. */
	if (!RPCB_BUF_GEQ(rm, *p, 4))
		return(-1);

	xu->xu_xslen = (*p)++;
	xu->xu_xsstr = (char *)*p;

	/* Length check */
	l = B(xu->xu_xslen);
	if (l < 11 || l > 23 || !RPCB_BUF_GEQ(rm, *p, XDRALIGN(l)))
		return(-1);

	/* Advance p */
	*(char **)p += XDRALIGN(l);

	/* Copy string to local buffer & terminate C style */
	bcopy(xu->xu_xsstr, uastr, l);
	uastr[l] = '\0';

	i = (char *)&xu->xu_ip;
	pp = (char *)&xu->xu_port;

	/*
	 * Expected format: a.b.c.d.e.f where [a-d] correspond to bytes of
	 * an IP address and [ef] are the bytes of a L4 port.
	 */
	if (!(ISDIGIT(uastr[0]) && ISDIGIT(uastr[l-1])))
		return(-1);
	b = uastr;
	for (c = &uastr[1], d = 0, dd = 0; c < &uastr[l-1]; c++) {
		if (ISDIGIT(*c)) {
			dd = 0;
			continue;
		}
		if (*c == '.') {
			if (dd != 0)
				return(-1);

			/* Check for ASCII byte. */
			*c = '\0';
			t = ippr_rpcb_atoi(b);
			if (t > 255)
				return(-1);

			/* Aim b at beginning of the next byte. */
			b = c + 1;

			/* Switch off IP addr vs port parsing. */
			if (d < 4)
				i[d++] = t & 0xff;
			else
				pp[d++ - 4] = t & 0xff;

			dd = 1;
			continue;
		}
		return(-1);
	}
	if (d != 5) /* String must contain exactly 5 periods. */
		return(-1);

	/* Handle the last byte (port low byte) */
	t = ippr_rpcb_atoi(b);
	if (t > 255)
		return(-1);
	pp[d - 4] = t & 0xff;

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_atoi (XXX should be generic for all proxies)	*/
/* Returns:	int -- integer representation of supplied string	*/
/* Parameters:	ptr(I)	- input string					*/
/*									*/
/* Simple version of atoi(3) ripped from ip_rcmd_pxy.c.			*/
/* --------------------------------------------------------------------	*/
static u_int
ippr_rpcb_atoi(ptr)
	char *ptr;
{
	register char *s = ptr, c;
	register u_int i = 0;

	while (((c = *s++) != '\0') && ISDIGIT(c)) {
		i *= 10;
		i += c - '0';
	}
	return i;
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_modreq					*/
/* Returns:	int -- change in datagram length			*/
/*			APR_ERR(2) - critical failure			*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		nat(I)	- pointer to NAT session			*/
/*		rm(I)	- pointer to RPC message structure		*/
/*		m(I)	- pointer to mbuf chain				*/
/*		off(I)	- current offset within mbuf chain		*/
/*									*/
/* When external and internal addresses differ, we rewrite the former	*/
/* with the latter.  (This is exclusive to protocol versions 3 & 4).	*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_modreq(fin, nat, rm, m, off)
	fr_info_t *fin;
	nat_t *nat;
	rpc_msg_t *rm;
	mb_t *m;
	u_int off;
{
	u_int len, xlen, pos, bogo;
	rpcb_args_t *ra;
	char uaddr[24];
	udphdr_t *udp;
	char *i, *p;
	int diff;

	ra = &rm->rm_call.rc_rpcbargs;
	i = (char *)&nat->nat_inip.s_addr;
	p = (char *)&nat->nat_inport;

	/* Form new string. */
	bzero(uaddr, sizeof(uaddr)); /* Just in case we need padding. */
#if defined(SNPRINTF) && defined(_KERNEL)
	SNPRINTF(uaddr, sizeof(uaddr),
#else
	(void) sprintf(uaddr,
#endif
		       "%u.%u.%u.%u.%u.%u", i[0] & 0xff, i[1] & 0xff,
		       i[2] & 0xff, i[3] & 0xff, p[0] & 0xff, p[1] & 0xff);
	len = strlen(uaddr);
	xlen = XDRALIGN(len);

	/* Determine mbuf offset to start writing to. */
	pos = (char *)ra->ra_maddr.xu_xslen - rm->rm_msgbuf;
	off += pos;

	/* Write new string length. */
	bogo = htonl(len);
	COPYBACK(m, off, 4, (caddr_t)&bogo);
	off += 4;

	/* Write new string. */
	COPYBACK(m, off, xlen, uaddr);
	off += xlen;

	/* Write in zero r_owner. */
	bogo = 0;
	COPYBACK(m, off, 4, (caddr_t)&bogo);

	/* Determine difference in data lengths. */
	diff = xlen - XDRALIGN(B(ra->ra_maddr.xu_xslen));

	/*
	 * If our new string has a different length, make necessary
	 * adjustments.
	 */
	if (diff != 0) {
		udp = fin->fin_dp;
		udp->uh_ulen = htons(ntohs(udp->uh_ulen) + diff);
		fin->fin_ip->ip_len += diff;
		fin->fin_dlen += diff;
		fin->fin_plen += diff;
		/* XXX Storage lengths. */
	}

	return(diff);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_decoderep					*/
/* Returns:	int - -1 == bad request or critical failure,		*/
/*		       0 == valid, negative reply			*/
/*		       1 == vaddlid, positive reply; needs no changes	*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		nat(I)	- pointer to NAT session structure		*/
/*		rs(I)	- pointer to RPCB session structure		*/
/*		rm(I)	- pointer to RPC message structure		*/
/*		rxp(O)	- pointer to RPCB transaction structure		*/
/*									*/
/* Take a presumed RPCB reply, extract the XID, search for the original */
/* request information, and determine whether the request was accepted	*/
/* or rejected.  With a valid accepted reply, go ahead and create NAT	*/
/* and state entries, and finish up by rewriting the packet as 		*/
/* required.								*/
/*									*/
/* WARNING:  It's the responsibility of the caller to make sure there	*/
/* is enough room in rs_buf for the basic RPC message "preamble".	*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_decoderep(fin, nat, rs, rm, rxp)
	fr_info_t *fin;
	nat_t *nat;
	rpcb_session_t *rs;
	rpc_msg_t *rm;
	rpcb_xact_t **rxp;
{
	rpcb_listp_t *rl;
	rpcb_entry_t *re;
	rpcb_xact_t *rx;
	u_32_t xdr, *p;
	rpc_resp_t *rr;
	int rv, cnt;

	p = (u_32_t *)rm->rm_msgbuf;

	bzero((char *)&rx, sizeof(rx));
	rr = &rm->rm_resp;

	rm->rm_xid = p;
	xdr = B(p++);		/* Record this message's XID. */

	/* Lookup XID */
        MUTEX_ENTER(&rs->rs_rxlock);
	if ((rx = ippr_rpcb_lookup(rs, xdr)) == NULL) {
                MUTEX_EXIT(&rs->rs_rxlock);
		return(-1);
        }
        ++rx->rx_ref;        /* per thread reference */
        MUTEX_EXIT(&rs->rs_rxlock);

	*rxp = rx;

	/* Test call vs reply */
	if (B(p++) != RPCB_REPLY)
		return(-1);

	/* Test reply_stat */
	switch(B(p++))
	{
	case RPCB_MSG_DENIED:
		return(0);
	case RPCB_MSG_ACCEPTED:
		break;
	default:
		return(-1);
	}

	/* Bypass RPC authentication stuff. */
	if (ippr_rpcb_skipauth(rm, &rr->rr_authverf, &p) != 0)
		return(-1);

	/* Test accept status */
	if (!RPCB_BUF_GEQ(rm, p, 4))
		return(-1);
	if (B(p++) != 0)
		return(0);

	/* Parse out the expected reply */
	switch(rx->rx_type)
	{
	case RPCB_RES_PMAP:
		/* There must be only one 4 byte argument. */
		if (!RPCB_BUF_EQ(rm, p, 4))
			return(-1);
		
		rr->rr_v2 = p;
		xdr = B(rr->rr_v2);
		
		/* Reply w/ a 0 port indicates service isn't registered */
		if (xdr == 0)
			return(0);
		
		/* Is the value sane? */
		if (xdr > 65535)
			return(-1);

		/* Create NAT & state table entries. */
		if (ippr_rpcb_getnat(fin, nat, rx->rx_proto, (u_int)xdr) != 0)
			return(-1);
		break;
	case RPCB_RES_STRING:
		/* Expecting a XDR string; need 4 bytes for length */
		if (!RPCB_BUF_GEQ(rm, p, 4))
			return(-1);

		rr->rr_v3.xu_str.xs_len = p++;
		rr->rr_v3.xu_str.xs_str = (char *)p;

		xdr = B(rr->rr_v3.xu_xslen);

		/* A null string indicates an unregistered service */
		if ((xdr == 0) && RPCB_BUF_EQ(rm, p, 0))
			return(0);

		/* Decode the target IP address / port. */
		if (ippr_rpcb_getuaddr(rm, &rr->rr_v3, &p) != 0)
			return(-1);

		/* Validate the IP address and port contained. */
		if (nat->nat_inip.s_addr != rr->rr_v3.xu_ip)
			return(-1);

		/* Create NAT & state table entries. */
		if (ippr_rpcb_getnat(fin, nat, rx->rx_proto,
				     (u_int)rr->rr_v3.xu_port) != 0)
			return(-1);
		break;
	case RPCB_RES_LIST:
		if (!RPCB_BUF_GEQ(rm, p, 4))
			return(-1);
		/* rpcb_entry_list_ptr */
		switch(B(p))
		{
		case 0:
			return(0);
			/*NOTREACHED*/
			break;
		case 1:
			break;
		default:
			return(-1);
		}
		rl = &rr->rr_v4;
		rl->rl_list = p++;
		cnt = 0;

		for(;;) {
			re = &rl->rl_entries[rl->rl_cnt];
			if (ippr_rpcb_getuaddr(rm, &re->re_maddr, &p) != 0)
				return(-1);
			if (ippr_rpcb_getproto(rm, &re->re_netid, &p) != 0)
				return(-1);
			/* re_semantics & re_pfamily length */
			if (!RPCB_BUF_GEQ(rm, p, 12))
				return(-1);
			p++; /* Skipping re_semantics. */
			xdr = B(p++);
			if ((xdr != 4) || strncmp((char *)p, "inet", 4))
				return(-1);
			p++;
			if (ippr_rpcb_getproto(rm, &re->re_proto, &p) != 0)
				return(-1);
			if (!RPCB_BUF_GEQ(rm, p, 4))
				return(-1);
			re->re_more = p;
			if (B(re->re_more) > 1) /* 0,1 only legal values */
				return(-1);
			++rl->rl_cnt;
			++cnt;
			if (B(re->re_more) == 0)
				break;
			/* Replies in  max out at 2; TCP and/or UDP */
			if (cnt > 2)
				return(-1);
			p++;
		}

		for(rl->rl_cnt = 0; rl->rl_cnt < cnt; rl->rl_cnt++) {
			re = &rl->rl_entries[rl->rl_cnt];
			rv = ippr_rpcb_getnat(fin, nat,
			                      re->re_proto.xp_proto,
				              (u_int)re->re_maddr.xu_port);
			if (rv != 0)
				return(-1);
		}
		break;
	default:
		/*CONSTANTCONDITION*/
		IPF_PANIC(1, ("illegal rx_type %d", rx->rx_type));
	}

	return(1);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_lookup					*/
/* Returns:	rpcb_xact_t * 	- NULL == no matching record,		*/
/*				  else pointer to relevant entry	*/
/* Parameters:	rs(I)	- pointer to RPCB session			*/
/*		xid(I)	- XID to look for				*/
/* --------------------------------------------------------------------	*/
static rpcb_xact_t *
ippr_rpcb_lookup(rs, xid)
	rpcb_session_t *rs;
	u_32_t xid;
{
	rpcb_xact_t *rx;

	if (rs->rs_rxlist == NULL)
		return(NULL);

	for (rx = rs->rs_rxlist; rx != NULL; rx = rx->rx_next)
		if (rx->rx_xid == xid)
			break;

	return(rx);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_deref					        */
/* Returns:	(void)							*/
/* Parameters:	rs(I)	- pointer to RPCB session			*/
/*		rx(I)	- pointer to RPC transaction struct to remove	*/
/*              force(I) - indicates to delete entry regardless of      */
/*                         reference count                              */
/* Locking:	rs->rs_rxlock must be held write only			*/
/*									*/
/* Free the RPCB transaction record rx from the chain of entries.	*/
/* --------------------------------------------------------------------	*/
static void
ippr_rpcb_deref(rs, rx)
	rpcb_session_t *rs;
	rpcb_xact_t *rx;
{
	rs = rs;	/* LINT */

	if (rx == NULL)
		return;

	if (--rx->rx_ref != 0)
		return;

	if (rx->rx_next != NULL)
		rx->rx_next->rx_pnext = rx->rx_pnext;

	*rx->rx_pnext = rx->rx_next;

	KFREE(rx);

	--rpcbcnt;
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_getproto					*/
/* Returns:	int - -1 == illegal protocol/netid,			*/
/*		       0 == legal protocol/netid			*/
/* Parameters:	rm(I)	- pointer to RPC message structure		*/
/*		xp(I)	- pointer to netid structure			*/
/*		p(IO)	- pointer to location within packet buffer	*/
/* 									*/
/* Decode netid/proto stored at p and record its numeric value.	 	*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_getproto(rm, xp, p)
	rpc_msg_t *rm;
	xdr_proto_t *xp;
	u_32_t **p;
{
	u_int len;

	/* Must have 4 bytes for length & 4 bytes for "tcp" or "udp". */
	if (!RPCB_BUF_GEQ(rm, p, 8))
		return(-1);

	xp->xp_xslen = (*p)++;
	xp->xp_xsstr = (char *)*p;

	/* Test the string length. */
	len = B(xp->xp_xslen);
	if (len != 3)
	 	return(-1);

	/* Test the actual string & record the protocol accordingly. */
	if (!strncmp((char *)xp->xp_xsstr, "tcp\0", 4))
		xp->xp_proto = IPPROTO_TCP;
	else if (!strncmp((char *)xp->xp_xsstr, "udp\0", 4))
		xp->xp_proto = IPPROTO_UDP;
	else {
		return(-1);
	}
	
	/* Advance past the string. */
	(*p)++;

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_getnat					*/
/* Returns:	int -- -1 == failed to create table entries,		*/
/*			0 == success					*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		nat(I)	- pointer to NAT table entry			*/
/*		proto(I) - transport protocol for new entries		*/
/*		port(I)	- new port to use w/ wildcard table entries	*/
/*									*/
/* Create state and NAT entries to handle an anticipated connection	*/
/* attempt between RPC client and server.				*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_getnat(fin, nat, proto, port)
	fr_info_t *fin;
	nat_t *nat;
	u_int proto;
	u_int port;
{
	ipnat_t *ipn, ipnat;
	tcphdr_t tcp;
	ipstate_t *is;
	fr_info_t fi;
	nat_t *natl;
	int nflags;

	ipn = nat->nat_ptr;

	/* Generate dummy fr_info */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_state = NULL;
	fi.fin_nat = NULL;
	fi.fin_out = 0;
	fi.fin_src = fin->fin_dst;
	fi.fin_dst = nat->nat_outip;
	fi.fin_p = proto;
	fi.fin_sport = 0;
	fi.fin_dport = port & 0xffff;
	fi.fin_flx |= FI_IGNORE;

	bzero((char *)&tcp, sizeof(tcp));
	tcp.th_dport = htons(port);

	if (proto == IPPROTO_TCP) {
		tcp.th_win = htons(8192);
		TCP_OFF_A(&tcp, sizeof(tcphdr_t) >> 2);
		fi.fin_dlen = sizeof(tcphdr_t);
		tcp.th_flags = TH_SYN;
		nflags = NAT_TCP;
	} else {
		fi.fin_dlen = sizeof(udphdr_t);
		nflags = NAT_UDP;
	}

	nflags |= SI_W_SPORT|NAT_SEARCH;
	fi.fin_dp = &tcp;
	fi.fin_plen = fi.fin_hlen + fi.fin_dlen;

	/*
	 * Search for existing NAT & state entries.  Pay close attention to
	 * mutexes / locks grabbed from lookup routines, as not doing so could
	 * lead to bad things.
	 *
	 * If successful, fr_stlookup returns with ipf_state locked.  We have
	 * no use for this lock, so simply unlock it if necessary.
	 */
	is = fr_stlookup(&fi, &tcp, NULL);
	if (is != NULL) {
		RWLOCK_EXIT(&ipf_state);
	}

	RWLOCK_EXIT(&ipf_nat);

	WRITE_ENTER(&ipf_nat);
	natl = nat_inlookup(&fi, nflags, proto, fi.fin_src, fi.fin_dst);

	if ((natl != NULL) && (is != NULL)) {
		MUTEX_DOWNGRADE(&ipf_nat);
		return(0);
	}

	/* Slightly modify the following structures for actual use in creating
	 * NAT and/or state entries.  We're primarily concerned with stripping
	 * flags that may be detrimental to the creation process or simply
	 * shouldn't be associated with a table entry.
	 */
	fi.fin_fr = &rpcbfr;
	fi.fin_flx &= ~FI_IGNORE;
	nflags &= ~NAT_SEARCH;

	if (natl == NULL) {
		/* XXX Since we're just copying the original ipn contents
		 * back, would we be better off just sending a pointer to
		 * the 'temp' copy off to nat_new instead?
		 */
		/* Generate template/bogus NAT rule. */
		bcopy((char *)ipn, (char *)&ipnat, sizeof(ipnat));
		ipn->in_flags = nflags & IPN_TCPUDP;
		ipn->in_apr = NULL;
		ipn->in_p = proto;
		ipn->in_pmin = htons(fi.fin_dport);
		ipn->in_pmax = htons(fi.fin_dport);
		ipn->in_pnext = htons(fi.fin_dport);
		ipn->in_space = 1;
		ipn->in_ippip = 1;
		if (ipn->in_flags & IPN_FILTER) {
			ipn->in_scmp = 0;
			ipn->in_dcmp = 0;
		}
		*ipn->in_plabel = '\0';

		/* Create NAT entry.  return NULL if this fails. */
		natl = nat_new(&fi, ipn, NULL, nflags|SI_CLONE|NAT_SLAVE,
			       NAT_INBOUND);

		bcopy((char *)&ipnat, (char *)ipn, sizeof(ipnat));

		if (natl == NULL) {
			MUTEX_DOWNGRADE(&ipf_nat);
			return(-1);
		}

		ipn->in_use++;
		(void) nat_proto(&fi, natl, nflags);
		nat_update(&fi, natl, natl->nat_ptr);
	}
	MUTEX_DOWNGRADE(&ipf_nat);

	if (is == NULL) {
		/* Create state entry.  Return NULL if this fails. */
		fi.fin_dst = nat->nat_inip;
		fi.fin_nat = (void *)natl;
		fi.fin_flx |= FI_NATED;
		fi.fin_flx &= ~FI_STATE;
		nflags &= NAT_TCPUDP;
		nflags |= SI_W_SPORT|SI_CLONE;

		is = fr_addstate(&fi, NULL, nflags);
		if (is == NULL) {
			/*
			 * XXX nat_delete is private to ip_nat.c.  Should
			 * check w/ Darren about this one.
			 *
			 * nat_delete(natl, NL_EXPIRE);
			 */
			return(-1);
		}
		if (fi.fin_state != NULL)
			fr_statederef((ipstate_t **)&fi.fin_state);
	}

	return(0);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_modv3						*/
/* Returns:	int -- change in packet length				*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		nat(I)	- pointer to NAT session			*/
/*		rm(I)	- pointer to RPC message structure		*/
/*		m(I)	- pointer to mbuf chain				*/
/*		off(I)	- offset within mbuf chain			*/
/*									*/
/* Write a new universal address string to this packet, adjusting	*/
/* lengths as necessary.						*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_modv3(fin, nat, rm, m, off)
	fr_info_t *fin;
	nat_t *nat;
	rpc_msg_t *rm;
	mb_t *m;
	u_int off;
{
	u_int len, xlen, pos, bogo;
	rpc_resp_t *rr;
	char uaddr[24];
	char *i, *p;
	int diff;

	rr = &rm->rm_resp;
	i = (char *)&nat->nat_outip.s_addr;
	p = (char *)&rr->rr_v3.xu_port;

	/* Form new string. */
	bzero(uaddr, sizeof(uaddr)); /* Just in case we need padding. */
#if defined(SNPRINTF) && defined(_KERNEL)
	SNPRINTF(uaddr, sizeof(uaddr),
#else
	(void) sprintf(uaddr,
#endif
		       "%u.%u.%u.%u.%u.%u", i[0] & 0xff, i[1] & 0xff,
		       i[2] & 0xff, i[3] & 0xff, p[0] & 0xff, p[1] & 0xff);
	len = strlen(uaddr);
	xlen = XDRALIGN(len);

	/* Determine mbuf offset to write to. */
	pos = (char *)rr->rr_v3.xu_xslen - rm->rm_msgbuf;
	off += pos;

	/* Write new string length. */
	bogo = htonl(len);
	COPYBACK(m, off, 4, (caddr_t)&bogo);
	off += 4;

	/* Write new string. */
	COPYBACK(m, off, xlen, uaddr);
	
	/* Determine difference in data lengths. */
	diff = xlen - XDRALIGN(B(rr->rr_v3.xu_xslen));

	/*
	 * If our new string has a different length, make necessary
	 * adjustments.
	 */
	if (diff != 0)
		ippr_rpcb_fixlen(fin, diff);

	return(diff);
}

/* --------------------------------------------------------------------	*/
/* Function:	ippr_rpcb_modv4						*/
/* Returns:	int -- change in packet length				*/
/* Parameters:	fin(I)	- pointer to packet information			*/
/*		nat(I)	- pointer to NAT session			*/
/*		rm(I)	- pointer to RPC message structure		*/
/*		m(I)	- pointer to mbuf chain				*/
/*		off(I)	- offset within mbuf chain			*/
/*									*/
/* Write new rpcb_entry list, adjusting	lengths as necessary.		*/
/* --------------------------------------------------------------------	*/
static int
ippr_rpcb_modv4(fin, nat, rm, m, off)
	fr_info_t *fin;
	nat_t *nat;
	rpc_msg_t *rm;
	mb_t *m;
	u_int off;
{
	u_int len, xlen, pos, bogo;
	rpcb_listp_t *rl;
	rpcb_entry_t *re;
	rpc_resp_t *rr;
	char uaddr[24];
	int diff, cnt;
	char *i, *p;

	diff = 0;
	rr = &rm->rm_resp;
	rl = &rr->rr_v4;

	i = (char *)&nat->nat_outip.s_addr;

	/* Determine mbuf offset to write to. */
	re = &rl->rl_entries[0];
	pos = (char *)re->re_maddr.xu_xslen - rm->rm_msgbuf;
	off += pos;

	for (cnt = 0; cnt < rl->rl_cnt; cnt++) {
		re = &rl->rl_entries[cnt];
		p = (char *)&re->re_maddr.xu_port;

		/* Form new string. */
		bzero(uaddr, sizeof(uaddr)); /* Just in case we need
						padding. */
#if defined(SNPRINTF) && defined(_KERNEL)
		SNPRINTF(uaddr, sizeof(uaddr),
#else
		(void) sprintf(uaddr,
#endif
			       "%u.%u.%u.%u.%u.%u", i[0] & 0xff,
			       i[1] & 0xff, i[2] & 0xff, i[3] & 0xff,
			       p[0] & 0xff, p[1] & 0xff);
		len = strlen(uaddr);
		xlen = XDRALIGN(len);

		/* Write new string length. */
		bogo = htonl(len);
		COPYBACK(m, off, 4, (caddr_t)&bogo);
		off += 4;

		/* Write new string. */
		COPYBACK(m, off, xlen, uaddr);
		off += xlen;

		/* Record any change in length. */
		diff += xlen - XDRALIGN(B(re->re_maddr.xu_xslen));

		/* If the length changed, copy back the rest of this entry. */
		len = ((char *)re->re_more + 4) -
		       (char *)re->re_netid.xp_xslen;
		if (diff != 0) {
			COPYBACK(m, off, len, (caddr_t)re->re_netid.xp_xslen);
		}
		off += len;
	}

	/*
	 * If our new string has a different length, make necessary
	 * adjustments.
	 */
	if (diff != 0)
		ippr_rpcb_fixlen(fin, diff);

	return(diff);
}


/* --------------------------------------------------------------------	*/
/* Function:    ippr_rpcb_fixlen                                        */
/* Returns:     (void)                                                  */
/* Parameters:  fin(I)  - pointer to packet information                 */
/*              len(I)  - change in packet length                       */
/*                                                                      */
/* Adjust various packet related lengths held in structure and packet   */
/* header fields.                                                       */
/* --------------------------------------------------------------------	*/
static void
ippr_rpcb_fixlen(fin, len)
        fr_info_t *fin;
        int len;
{
        udphdr_t *udp;

        udp = fin->fin_dp;
        udp->uh_ulen = htons(ntohs(udp->uh_ulen) + len);
        fin->fin_ip->ip_len += len;
        fin->fin_dlen += len;
        fin->fin_plen += len;
}

#undef B
