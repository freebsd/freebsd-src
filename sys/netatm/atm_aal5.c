/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM AAL5 socket protocol processing
 *
 */

#include <netatm/kern_include.h>
#include <sys/stat.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Global variables
 */
u_long		atm_aal5_sendspace = 64 * 1024;	/* XXX */
u_long		atm_aal5_recvspace = 64 * 1024;	/* XXX */


/*
 * Local functions
 */
static int	atm_aal5_attach __P((struct socket *, int, struct proc *));
static int	atm_aal5_detach __P((struct socket *));
static int	atm_aal5_bind __P((struct socket *, struct sockaddr *, 
			struct proc *));
static int	atm_aal5_listen __P((struct socket *, struct proc *));
static int	atm_aal5_connect __P((struct socket *, struct sockaddr *,
			struct proc *));
static int	atm_aal5_accept __P((struct socket *, struct sockaddr **));
static int	atm_aal5_disconnect __P((struct socket *));
static int	atm_aal5_shutdown __P((struct socket *));
static int	atm_aal5_send __P((struct socket *, int, KBuffer *,
			struct sockaddr *, KBuffer *, struct proc *));
static int	atm_aal5_abort __P((struct socket *));
static int	atm_aal5_control __P((struct socket *, u_long, caddr_t, 
			struct ifnet *, struct proc *));
static int	atm_aal5_sense __P((struct socket *, struct stat *));
static int	atm_aal5_sockaddr __P((struct socket *, struct sockaddr **));
static int	atm_aal5_peeraddr __P((struct socket *, struct sockaddr **));
static int	atm_aal5_incoming __P((void *, Atm_connection *,
			Atm_attributes *, void **));
static void	atm_aal5_cpcs_data __P((void *, KBuffer *));
static caddr_t	atm_aal5_getname __P((void *));


#if (defined(__FreeBSD__) && (BSD >= 199506))
/*
 * New-style socket request routines
 */
struct pr_usrreqs	atm_aal5_usrreqs = {
	atm_aal5_abort,			/* pru_abort */
	atm_aal5_accept,		/* pru_accept */
	atm_aal5_attach,		/* pru_attach */
	atm_aal5_bind,			/* pru_bind */
	atm_aal5_connect,		/* pru_connect */
	pru_connect2_notsupp,		/* pru_connect2 */
	atm_aal5_control,		/* pru_control */
	atm_aal5_detach,		/* pru_detach */
	atm_aal5_disconnect,		/* pru_disconnect */
	atm_aal5_listen,		/* pru_listen */
	atm_aal5_peeraddr,		/* pru_peeraddr */
	pru_rcvd_notsupp,		/* pru_rcvd */
	pru_rcvoob_notsupp,		/* pru_rcvoob */
	atm_aal5_send,			/* pru_send */
	atm_aal5_sense,			/* pru_sense */
	atm_aal5_shutdown,		/* pru_shutdown */
	atm_aal5_sockaddr,		/* pru_sockaddr */
	sosend,				/* pru_sosend */
	soreceive,			/* pru_soreceive */
	sopoll				/* pru_sopoll */
};
#endif


/*
 * Local variables
 */
static Atm_endpoint	atm_aal5_endpt = {
	NULL,
	ENDPT_SOCK_AAL5,
	NULL,
	atm_aal5_getname,
	atm_sock_connected,
	atm_sock_cleared,
	atm_aal5_incoming,
	NULL,
	NULL,
	NULL,
	atm_aal5_cpcs_data,
	NULL,
	NULL,
	NULL,
	NULL
};

static Atm_attributes	atm_aal5_defattr = {
	NULL,			/* nif */
	CMAPI_CPCS,		/* api */
	0,			/* api_init */
	0,			/* headin */
	0,			/* headout */
	{			/* aal */
		T_ATM_PRESENT,
		ATM_AAL5
	},
	{			/* traffic */
		T_ATM_ABSENT,
	},
	{			/* bearer */
		T_ATM_ABSENT,
	},
	{			/* bhli */
		T_ATM_ABSENT
	},
	{			/* blli */
		T_ATM_ABSENT,
		T_ATM_ABSENT,
	},
	{			/* llc */
		T_ATM_ABSENT,
	},
	{			/* called */
		T_ATM_ABSENT,
		{
			T_ATM_ABSENT,
			0
		},
		{
			T_ATM_ABSENT,
			0
		}
	},
	{			/* calling */
		T_ATM_ABSENT
	},
	{			/* qos */
		T_ATM_ABSENT,
	},
	{			/* transit */
		T_ATM_ABSENT
	},
	{			/* cause */
		T_ATM_ABSENT
	}
};


/*
 * Handy common code macros
 */
#ifdef DIAGNOSTIC
#define ATM_INTRO(f)						\
	int		s, err = 0;				\
	s = splnet();						\
	ATM_DEBUG2("aal5 socket %s (%p)\n", f, so);		\
	/*							\
	 * Stack queue should have been drained			\
	 */							\
	if (atm_stackq_head != NULL)				\
		panic("atm_aal5: stack queue not empty");	\
	;
#else /* !DIAGNOSTIC */
#define ATM_INTRO(f)						\
	int		s, err = 0;				\
	s = splnet();						\
	;
#endif /* DIAGNOSTIC */

#define	ATM_OUTRO()						\
	/*							\
	 * Drain any deferred calls				\
	 */							\
	STACK_DRAIN();						\
	(void) splx(s);						\
	return (err);						\
	;

#define	ATM_RETERR(errno) {					\
	err = errno;						\
	goto out;						\
}


/*
 * Attach protocol to socket
 *
 * Arguments:
 *	so	pointer to socket
 *	proto	protocol identifier
 *	p	pointer to process
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_attach(so, proto, p)
	struct socket	*so;
	int		proto;
	struct proc	*p;
{
	Atm_pcb		*atp;

	ATM_INTRO("attach");

	/*
	 * Do general attach stuff
	 */
	err = atm_sock_attach(so, atm_aal5_sendspace, atm_aal5_recvspace);
	if (err)
		ATM_RETERR(err);

	/*
	 * Finish up any protocol specific stuff
	 */
	atp = sotoatmpcb(so);
	atp->atp_type = ATPT_AAL5;

	/*
	 * Set default connection attributes
	 */
	atp->atp_attr = atm_aal5_defattr;
	strncpy(atp->atp_name, "(AAL5)", T_ATM_APP_NAME_LEN);

out:
	ATM_OUTRO();
}


/*
 * Detach protocol from socket
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_detach(so)
	struct socket	*so;
{
	ATM_INTRO("detach");

	err = atm_sock_detach(so);

	ATM_OUTRO();
}


/*
 * Bind address to socket
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to protocol address
 *	p	pointer to process
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_bind(so, addr, p)
	struct socket	*so;
	struct sockaddr	*addr;
	struct proc	*p;
{
	ATM_INTRO("bind");

	err = atm_sock_bind(so, addr);

	ATM_OUTRO();
}


/*
 * Listen for incoming connections
 *
 * Arguments:
 *	so	pointer to socket
 *	p	pointer to process
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_listen(so, p)
	struct socket	*so;
	struct proc	*p;
{
	ATM_INTRO("listen");

	err = atm_sock_listen(so, &atm_aal5_endpt);

	ATM_OUTRO();
}


/*
 * Connect socket to peer
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to protocol address
 *	p	pointer to process
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_connect(so, addr, p)
	struct socket	*so;
	struct sockaddr	*addr;
	struct proc	*p;
{
	Atm_pcb		*atp;

	ATM_INTRO("connect");

	atp = sotoatmpcb(so);

	/*
	 * Resize send socket buffer to maximum sdu size
	 */
	if (atp->atp_attr.aal.tag == T_ATM_PRESENT) {
		long	size;

		size = atp->atp_attr.aal.v.aal5.forward_max_SDU_size;
		if (size != T_ATM_ABSENT)
			if (!sbreserve(&so->so_snd, size, so, p)) {
				err = ENOBUFS;
				ATM_OUTRO();
			}
				
	}

	/*
	 * Now get the socket connected
	 */
	err = atm_sock_connect(so, addr, &atm_aal5_endpt);

	ATM_OUTRO();
}


/*
 * Accept pending connection
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to contain protocol address
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_accept(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	ATM_INTRO("accept");

	/*
	 * Everything is pretty much done already, we just need to
	 * return the caller's address to the user.
	 */
	err = atm_sock_peeraddr(so, addr);

	ATM_OUTRO();
}


/*
 * Disconnect connected socket
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_disconnect(so)
	struct socket	*so;
{
	ATM_INTRO("disconnect");

	err = atm_sock_disconnect(so);

	ATM_OUTRO();
}


/*
 * Shut down socket data transmission
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_shutdown(so)
	struct socket	*so;
{
	ATM_INTRO("shutdown");

	socantsendmore(so);

	ATM_OUTRO();
}


/*
 * Send user data
 *
 * Arguments:
 *	so	pointer to socket
 *	flags	send data flags
 *	m	pointer to buffer containing user data
 *	addr	pointer to protocol address
 *	control	pointer to buffer containing protocol control data
 *	p	pointer to process
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_send(so, flags, m, addr, control, p)
	struct socket	*so;
	int		flags;
	KBuffer		*m;
	struct sockaddr	*addr;
	KBuffer		*control;
	struct proc	*p;
{
	Atm_pcb		*atp;

	ATM_INTRO("send");

	/*
	 * We don't support any control functions
	 */
	if (control) {
		int	clen;

		clen = KB_LEN(control);
		KB_FREEALL(control);
		if (clen) {
			KB_FREEALL(m);
			ATM_RETERR(EINVAL);
		}
	}

	/*
	 * We also don't support any flags or send-level addressing
	 */
	if (flags || addr) {
		KB_FREEALL(m);
		ATM_RETERR(EINVAL);
	}

	/*
	 * All we've got left is the data, so push it out
	 */
	atp = sotoatmpcb(so);
	err = atm_cm_cpcs_data(atp->atp_conn, m);
	if (err) {
		/*
		 * Output problem, drop packet
		 */
		atm_sock_stat.as_outdrop[atp->atp_type]++;
		KB_FREEALL(m);
	}

out:
	ATM_OUTRO();
}


/*
 * Abnormally terminate service
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_abort(so)
	struct socket	*so;
{
	ATM_INTRO("abort");

	so->so_error = ECONNABORTED;
	err = atm_sock_detach(so);

	ATM_OUTRO();
}


/*
 * Do control operation - ioctl system call
 *
 * Arguments:
 *	so	pointer to socket
 *	cmd	ioctl code
 *	data	pointer to code specific parameter data area
 *	ifp	pointer to ifnet structure if it's an interface ioctl
 *	p	pointer to process
 *
 * Returns:
 *	0 	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_control(so, cmd, data, ifp, p)
	struct socket	*so;
	u_long		cmd;
	caddr_t		data;
	struct ifnet	*ifp;
	struct proc	*p;
{
	ATM_INTRO("control");

	switch (cmd) {

	default:
		err = EOPNOTSUPP;
	}

	ATM_OUTRO();
}

/*
 * Sense socket status - fstat system call
 *
 * Arguments:
 *	so	pointer to socket
 *	st	pointer to file status structure
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_sense(so, st)
	struct socket	*so;
	struct stat	*st;
{
	ATM_INTRO("sense");

	/*
	 * Just return the max sdu size for the connection
	 */
	st->st_blksize = so->so_snd.sb_hiwat;

	ATM_OUTRO();
}


/*
 * Retrieve local socket address
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to contain protocol address
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_sockaddr(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	ATM_INTRO("sockaddr");

	err = atm_sock_sockaddr(so, addr);

	ATM_OUTRO();
}


/*
 * Retrieve peer socket address
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to contain protocol address
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
static int
atm_aal5_peeraddr(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	ATM_INTRO("peeraddr");

	err = atm_sock_peeraddr(so, addr);

	ATM_OUTRO();
}


/*
 * Process Incoming Calls
 *
 * This function will receive control when an incoming call has been matched
 * to one of our registered listen parameter blocks.  Assuming the call passes
 * acceptance criteria and all required resources are available, we will
 * create a new protocol control block and socket association.  We must
 * then await notification of the final SVC setup results.  If any
 * problems are encountered, we will just tell the connection manager to
 * reject the call.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tok	owner's matched listening token
 *	cop	pointer to incoming call's connection block
 *	ap	pointer to incoming call's attributes
 *	tokp	pointer to location to store our connection token
 *
 * Returns:
 *	0	call is accepted
 *	errno	call rejected - reason indicated
 *
 */
static int
atm_aal5_incoming(tok, cop, ap, tokp)
	void		*tok;
	Atm_connection	*cop;
	Atm_attributes	*ap;
	void		**tokp;
{
	Atm_pcb		*atp0 = tok, *atp;
	struct socket	*so;
	int		err = 0;

	/*
	 * Allocate a new socket and pcb for this connection.
	 *
	 * Note that our attach function will be called via sonewconn
	 * and it will allocate and setup most of the pcb.
	 */
	atm_sock_stat.as_inconn[atp0->atp_type]++;
#if (defined(BSD) && (BSD >= 199103))
	so = sonewconn(atp0->atp_socket, 0);
#else
	so = sonewconn(atp0->atp_socket);
#endif

	if (so) {
		/*
		 * Finish pcb setup and pass pcb back to CM
		 */
		atp = sotoatmpcb(so);
		atp->atp_conn = cop;
		atp->atp_attr = *atp0->atp_conn->co_lattr;
		strncpy(atp->atp_name, atp0->atp_name, T_ATM_APP_NAME_LEN);
		*tokp = atp;
	} else {
		err = ECONNABORTED;
		atm_sock_stat.as_connfail[atp0->atp_type]++;
	}

	return (err);
}


/*
 * Process Socket VCC Input Data
 *
 * Arguments:
 *	tok	owner's connection token (atm_pcb)
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
atm_aal5_cpcs_data(tok, m)
	void		*tok;
	KBuffer		*m;
{
	Atm_pcb		*atp = tok;
	struct socket	*so;
	int		len;

	so = atp->atp_socket;

	KB_PLENGET(m, len);

	/*
	 * Ensure that the socket is able to receive data and
	 * that there's room in the socket buffer
	 */
	if (((so->so_state & SS_ISCONNECTED) == 0) ||
	    (so->so_state & SS_CANTRCVMORE) ||
	    (len > sbspace(&so->so_rcv))) {
		atm_sock_stat.as_indrop[atp->atp_type]++;
		KB_FREEALL(m);
		return;
	}

	/*
	 * Queue the data and notify the user
	 */
	sbappendrecord(&so->so_rcv, m);
	sorwakeup(so);

	return;
}


/*
 * Process getsockopt/setsockopt system calls
 *
 * Arguments:
 *	so	pointer to socket
 *	sopt	pointer to socket option info
 *
 * Returns:
 *	0 	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_aal5_ctloutput(so, sopt)
	struct socket	*so;
	struct sockopt	*sopt;
{
	Atm_pcb		*atp;

	ATM_INTRO("ctloutput");

	/*
	 * Make sure this is for us
	 */
	if (sopt->sopt_level != T_ATM_SIGNALING) {
		ATM_RETERR(EINVAL);
	}
	atp = sotoatmpcb(so);
	if (atp == NULL) {
		ATM_RETERR(ENOTCONN);
	}

	switch (sopt->sopt_dir) {

	case SOPT_SET:
		/*
		 * setsockopt()
		 */

		/*
		 * Validate socket state
		 */
		switch (sopt->sopt_name) {

		case T_ATM_ADD_LEAF:
		case T_ATM_DROP_LEAF:
			if ((so->so_state & SS_ISCONNECTED) == 0) {
				ATM_RETERR(ENOTCONN);
			}
			break;

		case T_ATM_CAUSE:
		case T_ATM_APP_NAME:
			break;

		default:
			if (so->so_state & SS_ISCONNECTED) {
				ATM_RETERR(EISCONN);
			}
			break;
		}

		/*
		 * Validate and save user-supplied option data
		 */
		err = atm_sock_setopt(so, sopt, atp);

		break;

	case SOPT_GET:
		/*
		 * getsockopt()
		 */

		/*
		 * Return option data
		 */
		err = atm_sock_getopt(so, sopt, atp);

		break;
	}

out:
	ATM_OUTRO();
}


/*
 * Initialize AAL5 Sockets
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
atm_aal5_init()
{
	/*
	 * Register our endpoint
	 */
	if (atm_endpoint_register(&atm_aal5_endpt))
		panic("atm_aal5_init: register");

	/*
	 * Set default connection attributes
	 */
	atm_aal5_defattr.aal.v.aal5.forward_max_SDU_size = T_ATM_ABSENT;
	atm_aal5_defattr.aal.v.aal5.backward_max_SDU_size = T_ATM_ABSENT;
	atm_aal5_defattr.aal.v.aal5.SSCS_type = T_ATM_NULL;
}


/*
 * Get Connection's Application/Owner Name
 *
 * Arguments:
 *	tok	owner's connection token (atm_pcb)
 *
 * Returns:
 *	addr	pointer to string containing our name
 *
 */
static caddr_t
atm_aal5_getname(tok)
	void		*tok;
{
	Atm_pcb		*atp = tok;

	return (atp->atp_name);
}

