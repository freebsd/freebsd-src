/*
 * ntp_request.c - respond to information requests
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_request.h"
#include "ntp_control.h"
#include "ntp_refclock.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <stddef.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "recvbuff.h"

#ifdef KERNEL_PLL
#include "ntp_syscall.h"
#endif /* KERNEL_PLL */

/*
 * Structure to hold request procedure information
 */
#define	NOAUTH	0
#define	AUTH	1

#define	NO_REQUEST	(-1)
/*
 * Because we now have v6 addresses in the messages, we need to compensate
 * for the larger size.  Therefore, we introduce the alternate size to 
 * keep us friendly with older implementations.  A little ugly.
 */
static int client_v6_capable = 0;   /* the client can handle longer messages */

#define v6sizeof(type)	(client_v6_capable ? sizeof(type) : v4sizeof(type))

struct req_proc {
	short request_code;	/* defined request code */
	short needs_auth;	/* true when authentication needed */
	short sizeofitem;	/* size of request data item (older size)*/
	short v6_sizeofitem;	/* size of request data item (new size)*/
	void (*handler) P((struct sockaddr_storage *, struct interface *,
			   struct req_pkt *));	/* routine to handle request */
};

/*
 * Universal request codes
 */
static	struct req_proc univ_codes[] = {
	{ NO_REQUEST,		NOAUTH,	 0,	0 }
};

static	void	req_ack	P((struct sockaddr_storage *, struct interface *, struct req_pkt *, int));
static	char *	prepare_pkt	P((struct sockaddr_storage *, struct interface *, struct req_pkt *, u_int));
static	char *	more_pkt	P((void));
static	void	flush_pkt	P((void));
static	void	peer_list	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	peer_list_sum	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	peer_info	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	peer_stats	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	sys_info	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	sys_stats	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	mem_stats	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	io_stats	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	timer_stats	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	loop_info	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_conf		P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_unconf	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	set_sys_flag	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	clr_sys_flag	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	setclr_flags	P((struct sockaddr_storage *, struct interface *, struct req_pkt *, u_long));
static	void	list_restrict	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_resaddflags	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_ressubflags	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_unrestrict	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_restrict	P((struct sockaddr_storage *, struct interface *, struct req_pkt *, int));
static	void	mon_getlist_0	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	mon_getlist_1	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	reset_stats	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	reset_peer	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_key_reread	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	trust_key	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	untrust_key	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_trustkey	P((struct sockaddr_storage *, struct interface *, struct req_pkt *, u_long));
static	void	get_auth_info	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	reset_auth_stats P((void));
static	void	req_get_traps	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	req_set_trap	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	req_clr_trap	P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_setclr_trap	P((struct sockaddr_storage *, struct interface *, struct req_pkt *, int));
static	void	set_request_keyid P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	set_control_keyid P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	get_ctl_stats   P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	get_if_stats    P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	do_if_reload    P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
#ifdef KERNEL_PLL
static	void	get_kernel_info P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
#endif /* KERNEL_PLL */
#ifdef REFCLOCK
static	void	get_clock_info P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
static	void	set_clock_fudge P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
#endif	/* REFCLOCK */
#ifdef REFCLOCK
static	void	get_clkbug_info P((struct sockaddr_storage *, struct interface *, struct req_pkt *));
#endif	/* REFCLOCK */

/*
 * ntpd request codes
 */
static	struct req_proc ntp_codes[] = {
	{ REQ_PEER_LIST,	NOAUTH,	0, 0,	peer_list },
	{ REQ_PEER_LIST_SUM,	NOAUTH,	0, 0,	peer_list_sum },
	{ REQ_PEER_INFO,    NOAUTH, v4sizeof(struct info_peer_list),
				sizeof(struct info_peer_list), peer_info},
	{ REQ_PEER_STATS,   NOAUTH, v4sizeof(struct info_peer_list),
				sizeof(struct info_peer_list), peer_stats},
	{ REQ_SYS_INFO,		NOAUTH,	0, 0,	sys_info },
	{ REQ_SYS_STATS,	NOAUTH,	0, 0,	sys_stats },
	{ REQ_IO_STATS,		NOAUTH,	0, 0,	io_stats },
	{ REQ_MEM_STATS,	NOAUTH,	0, 0,	mem_stats },
	{ REQ_LOOP_INFO,	NOAUTH,	0, 0,	loop_info },
	{ REQ_TIMER_STATS,	NOAUTH,	0, 0,	timer_stats },
	{ REQ_CONFIG,	    AUTH, v4sizeof(struct conf_peer),
				sizeof(struct conf_peer), do_conf },
	{ REQ_UNCONFIG,	    AUTH, v4sizeof(struct conf_unpeer),
				sizeof(struct conf_unpeer), do_unconf },
	{ REQ_SET_SYS_FLAG, AUTH, sizeof(struct conf_sys_flags),
				sizeof(struct conf_sys_flags), set_sys_flag },
	{ REQ_CLR_SYS_FLAG, AUTH, sizeof(struct conf_sys_flags), 
				sizeof(struct conf_sys_flags),  clr_sys_flag },
	{ REQ_GET_RESTRICT,	NOAUTH,	0, 0,	list_restrict },
	{ REQ_RESADDFLAGS, AUTH, v4sizeof(struct conf_restrict),
				sizeof(struct conf_restrict), do_resaddflags },
	{ REQ_RESSUBFLAGS, AUTH, v4sizeof(struct conf_restrict),
				sizeof(struct conf_restrict), do_ressubflags },
	{ REQ_UNRESTRICT, AUTH, v4sizeof(struct conf_restrict),
				sizeof(struct conf_restrict), do_unrestrict },
	{ REQ_MON_GETLIST,	NOAUTH,	0, 0,	mon_getlist_0 },
	{ REQ_MON_GETLIST_1,	NOAUTH,	0, 0,	mon_getlist_1 },
	{ REQ_RESET_STATS, AUTH, sizeof(struct reset_flags), 0, reset_stats },
	{ REQ_RESET_PEER,  AUTH, v4sizeof(struct conf_unpeer),
				sizeof(struct conf_unpeer), reset_peer },
	{ REQ_REREAD_KEYS,	AUTH,	0, 0,	do_key_reread },
	{ REQ_TRUSTKEY,   AUTH, sizeof(u_long), sizeof(u_long), trust_key },
	{ REQ_UNTRUSTKEY, AUTH, sizeof(u_long), sizeof(u_long), untrust_key },
	{ REQ_AUTHINFO,		NOAUTH,	0, 0,	get_auth_info },
	{ REQ_TRAPS,		NOAUTH, 0, 0,	req_get_traps },
	{ REQ_ADD_TRAP,	AUTH, v4sizeof(struct conf_trap),
				sizeof(struct conf_trap), req_set_trap },
	{ REQ_CLR_TRAP,	AUTH, v4sizeof(struct conf_trap),
				sizeof(struct conf_trap), req_clr_trap },
	{ REQ_REQUEST_KEY, AUTH, sizeof(u_long), sizeof(u_long), 
				set_request_keyid },
	{ REQ_CONTROL_KEY, AUTH, sizeof(u_long), sizeof(u_long), 
				set_control_keyid },
	{ REQ_GET_CTLSTATS,	NOAUTH,	0, 0,	get_ctl_stats },
#ifdef KERNEL_PLL
	{ REQ_GET_KERNEL,	NOAUTH,	0, 0,	get_kernel_info },
#endif
#ifdef REFCLOCK
	{ REQ_GET_CLOCKINFO, NOAUTH, sizeof(u_int32), sizeof(u_int32), 
				get_clock_info },
	{ REQ_SET_CLKFUDGE, AUTH, sizeof(struct conf_fudge), 
				sizeof(struct conf_fudge), set_clock_fudge },
	{ REQ_GET_CLKBUGINFO, NOAUTH, sizeof(u_int32), sizeof(u_int32),
				get_clkbug_info },
#endif
	{ REQ_IF_STATS,		AUTH, 0, 0,	get_if_stats },
	{ REQ_IF_RELOAD,        AUTH, 0, 0,	do_if_reload },

	{ NO_REQUEST,		NOAUTH,	0, 0,	0 }
};


/*
 * Authentication keyid used to authenticate requests.  Zero means we
 * don't allow writing anything.
 */
keyid_t info_auth_keyid;

/*
 * Statistic counters to keep track of requests and responses.
 */
u_long numrequests;		/* number of requests we've received */
u_long numresppkts;		/* number of resp packets sent with data */

u_long errorcounter[INFO_ERR_AUTH+1];	/* lazy way to count errors, indexed */
/* by the error code */

/*
 * A hack.  To keep the authentication module clear of ntp-ism's, we
 * include a time reset variable for its stats here.
 */
static u_long auth_timereset;

/*
 * Response packet used by these routines.  Also some state information
 * so that we can handle packet formatting within a common set of
 * subroutines.  Note we try to enter data in place whenever possible,
 * but the need to set the more bit correctly means we occasionally
 * use the extra buffer and copy.
 */
static struct resp_pkt rpkt;
static int reqver;
static int seqno;
static int nitems;
static int itemsize;
static int databytes;
static char exbuf[RESP_DATA_SIZE];
static int usingexbuf;
static struct sockaddr_storage *toaddr;
static struct interface *frominter;

/*
 * init_request - initialize request data
 */
void
init_request (void)
{
	int i;

	numrequests = 0;
	numresppkts = 0;
	auth_timereset = 0;
	info_auth_keyid = 0;	/* by default, can't do this */

	for (i = 0; i < sizeof(errorcounter)/sizeof(errorcounter[0]); i++)
	    errorcounter[i] = 0;
}


/*
 * req_ack - acknowledge request with no data
 */
static void
req_ack(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt,
	int errcode
	)
{
	/*
	 * fill in the fields
	 */
	rpkt.rm_vn_mode = RM_VN_MODE(RESP_BIT, 0, reqver);
	rpkt.auth_seq = AUTH_SEQ(0, 0);
	rpkt.implementation = inpkt->implementation;
	rpkt.request = inpkt->request;
	rpkt.err_nitems = ERR_NITEMS(errcode, 0);
	rpkt.mbz_itemsize = MBZ_ITEMSIZE(0);

	/*
	 * send packet and bump counters
	 */
	sendpkt(srcadr, inter, -1, (struct pkt *)&rpkt, RESP_HEADER_SIZE);
	errorcounter[errcode]++;
}


/*
 * prepare_pkt - prepare response packet for transmission, return pointer
 *		 to storage for data item.
 */
static char *
prepare_pkt(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *pkt,
	u_int structsize
	)
{
#ifdef DEBUG
	if (debug > 3)
	    printf("request: preparing pkt\n");
#endif

	/*
	 * Fill in the implementation, request and itemsize fields
	 * since these won't change.
	 */
	rpkt.implementation = pkt->implementation;
	rpkt.request = pkt->request;
	rpkt.mbz_itemsize = MBZ_ITEMSIZE(structsize);

	/*
	 * Compute the static data needed to carry on.
	 */
	toaddr = srcadr;
	frominter = inter;
	seqno = 0;
	nitems = 0;
	itemsize = structsize;
	databytes = 0;
	usingexbuf = 0;

	/*
	 * return the beginning of the packet buffer.
	 */
	return &rpkt.data[0];
}


/*
 * more_pkt - return a data pointer for a new item.
 */
static char *
more_pkt(void)
{
	/*
	 * If we were using the extra buffer, send the packet.
	 */
	if (usingexbuf) {
#ifdef DEBUG
		if (debug > 2)
		    printf("request: sending pkt\n");
#endif
		rpkt.rm_vn_mode = RM_VN_MODE(RESP_BIT, MORE_BIT, reqver);
		rpkt.auth_seq = AUTH_SEQ(0, seqno);
		rpkt.err_nitems = htons((u_short)nitems);
		sendpkt(toaddr, frominter, -1, (struct pkt *)&rpkt,
			RESP_HEADER_SIZE+databytes);
		numresppkts++;

		/*
		 * Copy data out of exbuf into the packet.
		 */
		memmove(&rpkt.data[0], exbuf, (unsigned)itemsize);
		seqno++;
		databytes = 0;
		nitems = 0;
		usingexbuf = 0;
	}

	databytes += itemsize;
	nitems++;
	if (databytes + itemsize <= RESP_DATA_SIZE) {
#ifdef DEBUG
		if (debug > 3)
		    printf("request: giving him more data\n");
#endif
		/*
		 * More room in packet.  Give him the
		 * next address.
		 */
		return &rpkt.data[databytes];
	} else {
		/*
		 * No room in packet.  Give him the extra
		 * buffer unless this was the last in the sequence.
		 */
#ifdef DEBUG
		if (debug > 3)
		    printf("request: into extra buffer\n");
#endif
		if (seqno == MAXSEQ)
		    return (char *)0;
		else {
			usingexbuf = 1;
			return exbuf;
		}
	}
}


/*
 * flush_pkt - we're done, return remaining information.
 */
static void
flush_pkt(void)
{
#ifdef DEBUG
	if (debug > 2)
	    printf("request: flushing packet, %d items\n", nitems);
#endif
	/*
	 * Must send the last packet.  If nothing in here and nothing
	 * has been sent, send an error saying no data to be found.
	 */
	if (seqno == 0 && nitems == 0)
	    req_ack(toaddr, frominter, (struct req_pkt *)&rpkt,
		    INFO_ERR_NODATA);
	else {
		rpkt.rm_vn_mode = RM_VN_MODE(RESP_BIT, 0, reqver);
		rpkt.auth_seq = AUTH_SEQ(0, seqno);
		rpkt.err_nitems = htons((u_short)nitems);
		sendpkt(toaddr, frominter, -1, (struct pkt *)&rpkt,
			RESP_HEADER_SIZE+databytes);
		numresppkts++;
	}
}



/*
 * process_private - process private mode (7) packets
 */
void
process_private(
	struct recvbuf *rbufp,
	int mod_okay
	)
{
	struct req_pkt *inpkt;
	struct req_pkt_tail *tailinpkt;
	struct sockaddr_storage *srcadr;
	struct interface *inter;
	struct req_proc *proc;
	int ec;
	short temp_size;

	/*
	 * Initialize pointers, for convenience
	 */
	inpkt = (struct req_pkt *)&rbufp->recv_pkt;
	srcadr = &rbufp->recv_srcadr;
	inter = rbufp->dstadr;

#ifdef DEBUG
	if (debug > 2)
	    printf("process_private: impl %d req %d\n",
		   inpkt->implementation, inpkt->request);
#endif

	/*
	 * Do some sanity checks on the packet.  Return a format
	 * error if it fails.
	 */
	ec = 0;
	if (   (++ec, ISRESPONSE(inpkt->rm_vn_mode))
	    || (++ec, ISMORE(inpkt->rm_vn_mode))
	    || (++ec, INFO_VERSION(inpkt->rm_vn_mode) > NTP_VERSION)
	    || (++ec, INFO_VERSION(inpkt->rm_vn_mode) < NTP_OLDVERSION)
	    || (++ec, INFO_SEQ(inpkt->auth_seq) != 0)
	    || (++ec, INFO_ERR(inpkt->err_nitems) != 0)
	    || (++ec, INFO_MBZ(inpkt->mbz_itemsize) != 0)
	    || (++ec, rbufp->recv_length < REQ_LEN_HDR)
		) {
		msyslog(LOG_ERR, "process_private: INFO_ERR_FMT: test %d failed, pkt from %s", ec, stoa(srcadr));
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	reqver = INFO_VERSION(inpkt->rm_vn_mode);

	/*
	 * Get the appropriate procedure list to search.
	 */
	if (inpkt->implementation == IMPL_UNIV)
	    proc = univ_codes;
	else if ((inpkt->implementation == IMPL_XNTPD) ||
		 (inpkt->implementation == IMPL_XNTPD_OLD))
	    proc = ntp_codes;
	else {
		req_ack(srcadr, inter, inpkt, INFO_ERR_IMPL);
		return;
	}

	/*
	 * Search the list for the request codes.  If it isn't one
	 * we know, return an error.
	 */
	while (proc->request_code != NO_REQUEST) {
		if (proc->request_code == (short) inpkt->request)
		    break;
		proc++;
	}
	if (proc->request_code == NO_REQUEST) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_REQ);
		return;
	}

#ifdef DEBUG
	if (debug > 3)
	    printf("found request in tables\n");
#endif

	/*
	 * If we need data, check to see if we have some.  If we
	 * don't, check to see that there is none (picky, picky).
	 */	

	/* This part is a bit tricky, we want to be sure that the size
	 * returned is either the old or the new size.  We also can find
	 * out if the client can accept both types of messages this way. 
	 *
	 * Handle the exception of REQ_CONFIG. It can have two data sizes.
	 */
	temp_size = INFO_ITEMSIZE(inpkt->mbz_itemsize);
	if ((temp_size != proc->sizeofitem &&
	    temp_size != proc->v6_sizeofitem) &&
	    !(inpkt->implementation == IMPL_XNTPD &&
	    inpkt->request == REQ_CONFIG &&
	    temp_size == sizeof(struct old_conf_peer))) {
#ifdef DEBUG
		if (debug > 2)
			printf("process_private: wrong item size, received %d, should be %d or %d\n",
			    temp_size, proc->sizeofitem, proc->v6_sizeofitem);
#endif
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}
	if ((proc->sizeofitem != 0) &&
	    ((temp_size * INFO_NITEMS(inpkt->err_nitems)) >
	    (rbufp->recv_length - REQ_LEN_HDR))) {
#ifdef DEBUG
		if (debug > 2)
			printf("process_private: not enough data\n");
#endif
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	switch (inpkt->implementation) {
	case IMPL_XNTPD:
		client_v6_capable = 1;
		break;
	case IMPL_XNTPD_OLD:
		client_v6_capable = 0;
		break;
	default:
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	/*
	 * If we need to authenticate, do so.  Note that an
	 * authenticatable packet must include a mac field, must
	 * have used key info_auth_keyid and must have included
	 * a time stamp in the appropriate field.  The time stamp
	 * must be within INFO_TS_MAXSKEW of the receive
	 * time stamp.
	 */
	if (proc->needs_auth && sys_authenticate) {
		l_fp ftmp;
		double dtemp;
	
		if (rbufp->recv_length < (int)((REQ_LEN_HDR +
		    (INFO_ITEMSIZE(inpkt->mbz_itemsize) *
		    INFO_NITEMS(inpkt->err_nitems))
		    + sizeof(struct req_pkt_tail)))) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		}
		tailinpkt = (struct req_pkt_tail *)((char *)&rbufp->recv_pkt +
		    rbufp->recv_length - sizeof(struct req_pkt_tail));

		/*
		 * If this guy is restricted from doing this, don't let him
		 * If wrong key was used, or packet doesn't have mac, return.
		 */
		if (!INFO_IS_AUTH(inpkt->auth_seq) || info_auth_keyid == 0
		    || ntohl(tailinpkt->keyid) != info_auth_keyid) {
#ifdef DEBUG
			if (debug > 4)
			    printf("failed auth %d info_auth_keyid %lu pkt keyid %lu\n",
				   INFO_IS_AUTH(inpkt->auth_seq),
				   (u_long)info_auth_keyid,
				   (u_long)ntohl(tailinpkt->keyid));
			msyslog(LOG_DEBUG,
				"process_private: failed auth %d info_auth_keyid %lu pkt keyid %lu\n",
				INFO_IS_AUTH(inpkt->auth_seq),
				(u_long)info_auth_keyid,
				(u_long)ntohl(tailinpkt->keyid));
#endif
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}
		if (rbufp->recv_length > REQ_LEN_MAC) {
#ifdef DEBUG
			if (debug > 4)
			    printf("bad pkt length %d\n",
				   rbufp->recv_length);
#endif
			msyslog(LOG_ERR, "process_private: bad pkt length %d",
				rbufp->recv_length);
			req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
			return;
		}
		if (!mod_okay || !authhavekey(info_auth_keyid)) {
#ifdef DEBUG
			if (debug > 4)
			    printf("failed auth mod_okay %d\n", mod_okay);
			msyslog(LOG_DEBUG,
				"process_private: failed auth mod_okay %d\n",
				mod_okay);
#endif
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}

		/*
		 * calculate absolute time difference between xmit time stamp
		 * and receive time stamp.  If too large, too bad.
		 */
		NTOHL_FP(&tailinpkt->tstamp, &ftmp);
		L_SUB(&ftmp, &rbufp->recv_time);
		LFPTOD(&ftmp, dtemp);
		if (fabs(dtemp) >= INFO_TS_MAXSKEW) {
			/*
			 * He's a loser.  Tell him.
			 */
#ifdef DEBUG
			if (debug > 4)
			    printf("xmit/rcv timestamp delta > INFO_TS_MAXSKEW\n");
#endif
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}

		/*
		 * So far so good.  See if decryption works out okay.
		 */
		if (!authdecrypt(info_auth_keyid, (u_int32 *)inpkt,
		    rbufp->recv_length - sizeof(struct req_pkt_tail) +
		    REQ_LEN_HDR, sizeof(struct req_pkt_tail) - REQ_LEN_HDR)) {
#ifdef DEBUG
			if (debug > 4)
			    printf("authdecrypt failed\n");
#endif
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}
	}

#ifdef DEBUG
	if (debug > 3)
	    printf("process_private: all okay, into handler\n");
#endif

	/*
	 * Packet is okay.  Call the handler to send him data.
	 */
	(proc->handler)(srcadr, inter, inpkt);
}


/*
 * peer_list - send a list of the peers
 */
static void
peer_list(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_peer_list *ip;
	register struct peer *pp;
	register int i;
	register int skip = 0;

	ip = (struct info_peer_list *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_peer_list));
	for (i = 0; i < NTP_HASH_SIZE && ip != 0; i++) {
		pp = peer_hash[i];
		while (pp != 0 && ip != 0) {
			if (pp->srcadr.ss_family == AF_INET6) {
				if (client_v6_capable) {
					ip->addr6 = GET_INADDR6(pp->srcadr);
					ip->v6_flag = 1;
					skip = 0;
				} else {
					skip = 1;
					break;
				}
			} else {
				ip->addr = GET_INADDR(pp->srcadr);
				if (client_v6_capable)
					ip->v6_flag = 0;
				skip = 0;
			}

			if(!skip) {
				ip->port = NSRCPORT(&pp->srcadr);
				ip->hmode = pp->hmode;
				ip->flags = 0;
				if (pp->flags & FLAG_CONFIG)
				    ip->flags |= INFO_FLAG_CONFIG;
				if (pp == sys_peer)
				    ip->flags |= INFO_FLAG_SYSPEER;
				if (pp->status == CTL_PST_SEL_SYNCCAND)
				    ip->flags |= INFO_FLAG_SEL_CANDIDATE;
				if (pp->status >= CTL_PST_SEL_SYSPEER)
				    ip->flags |= INFO_FLAG_SHORTLIST;
				ip = (struct info_peer_list *)more_pkt();
			}
			pp = pp->next; 
		}
	}
	flush_pkt();
}


/*
 * peer_list_sum - return extended peer list
 */
static void
peer_list_sum(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_peer_summary *ips;
	register struct peer *pp;
	register int i;
	l_fp ltmp;
	register int skip;

#ifdef DEBUG
	if (debug > 2)
	    printf("wants peer list summary\n");
#endif
	ips = (struct info_peer_summary *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_peer_summary));
	for (i = 0; i < NTP_HASH_SIZE && ips != 0; i++) {
		pp = peer_hash[i];
		while (pp != 0 && ips != 0) {
#ifdef DEBUG
			if (debug > 3)
			    printf("sum: got one\n");
#endif
			/*
			 * Be careful here not to return v6 peers when we
			 * want only v4.
			 */
			if (pp->srcadr.ss_family == AF_INET6) {
				if (client_v6_capable) {
					ips->srcadr6 = GET_INADDR6(pp->srcadr);
					ips->v6_flag = 1;
					if (pp->dstadr)
						ips->dstadr6 = GET_INADDR6(pp->dstadr->sin);
					else
						memset(&ips->dstadr6, 0, sizeof(ips->dstadr6));
					skip = 0;
				} else {
					skip = 1;
					break;
				}
			} else {
				ips->srcadr = GET_INADDR(pp->srcadr);
				if (client_v6_capable)
					ips->v6_flag = 0;
/* XXX PDM This code is buggy. Need to replace with a straightforward assignment */
				
				if (pp->dstadr)
					ips->dstadr = (pp->processed) ?
						pp->cast_flags == MDF_BCAST ?
						GET_INADDR(pp->dstadr->bcast):
						pp->cast_flags ?
						GET_INADDR(pp->dstadr->sin) ?
						GET_INADDR(pp->dstadr->sin):
						GET_INADDR(pp->dstadr->bcast):
						1 : GET_INADDR(pp->dstadr->sin);
				else
						memset(&ips->dstadr, 0, sizeof(ips->dstadr));

				skip = 0;
			}
			
			if (!skip){ 
				ips->srcport = NSRCPORT(&pp->srcadr);
				ips->stratum = pp->stratum;
				ips->hpoll = pp->hpoll;
				ips->ppoll = pp->ppoll;
				ips->reach = pp->reach;
				ips->flags = 0;
				if (pp == sys_peer)
				    ips->flags |= INFO_FLAG_SYSPEER;
				if (pp->flags & FLAG_CONFIG)
				    ips->flags |= INFO_FLAG_CONFIG;
				if (pp->flags & FLAG_REFCLOCK)
				    ips->flags |= INFO_FLAG_REFCLOCK;
				if (pp->flags & FLAG_AUTHENABLE)
				    ips->flags |= INFO_FLAG_AUTHENABLE;
				if (pp->flags & FLAG_PREFER)
				    ips->flags |= INFO_FLAG_PREFER;
				if (pp->flags & FLAG_BURST)
				    ips->flags |= INFO_FLAG_BURST;
				if (pp->status == CTL_PST_SEL_SYNCCAND)
				    ips->flags |= INFO_FLAG_SEL_CANDIDATE;
				if (pp->status >= CTL_PST_SEL_SYSPEER)
				    ips->flags |= INFO_FLAG_SHORTLIST;
				ips->hmode = pp->hmode;
				ips->delay = HTONS_FP(DTOFP(pp->delay));
				DTOLFP(pp->offset, &ltmp);
				HTONL_FP(&ltmp, &ips->offset);
				ips->dispersion = HTONS_FP(DTOUFP(SQRT(pp->disp)));
			}	
			pp = pp->next; 
			ips = (struct info_peer_summary *)more_pkt();
		}
	}
	flush_pkt();
}


/*
 * peer_info - send information for one or more peers
 */
static void
peer_info (
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_peer_list *ipl;
	register struct peer *pp;
	register struct info_peer *ip;
	register int items;
	register int i, j;
	struct sockaddr_storage addr;
	extern struct peer *sys_peer;
	l_fp ltmp;

	memset((char *)&addr, 0, sizeof addr);
	items = INFO_NITEMS(inpkt->err_nitems);
	ipl = (struct info_peer_list *) inpkt->data;

	ip = (struct info_peer *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_peer));
	while (items-- > 0 && ip != 0) {
		memset((char *)&addr, 0, sizeof(addr));
		NSRCPORT(&addr) = ipl->port;
		if (client_v6_capable && ipl->v6_flag != 0) {
			addr.ss_family = AF_INET6;
			GET_INADDR6(addr) = ipl->addr6;
		} else {
			addr.ss_family = AF_INET;
			GET_INADDR(addr) = ipl->addr;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		addr.ss_len = SOCKLEN(&addr);
#endif
		ipl++;
		if ((pp = findexistingpeer(&addr, (struct peer *)0, -1)) == 0)
		    continue;
		if (pp->srcadr.ss_family == AF_INET6) {
			if (pp->dstadr)
				ip->dstadr6 = pp->cast_flags == MDF_BCAST ?
					GET_INADDR6(pp->dstadr->bcast) :
					GET_INADDR6(pp->dstadr->sin);
			else
				memset(&ip->dstadr6, 0, sizeof(ip->dstadr6));

			ip->srcadr6 = GET_INADDR6(pp->srcadr);
			ip->v6_flag = 1;
		} else {
/* XXX PDM This code is buggy. Need to replace with a straightforward assignment */
			if (pp->dstadr)
				ip->dstadr = (pp->processed) ?
					pp->cast_flags == MDF_BCAST ?
					GET_INADDR(pp->dstadr->bcast):
					pp->cast_flags ?
					GET_INADDR(pp->dstadr->sin) ?
					GET_INADDR(pp->dstadr->sin):
					GET_INADDR(pp->dstadr->bcast):
					2 : GET_INADDR(pp->dstadr->sin);
			else
				memset(&ip->dstadr, 0, sizeof(ip->dstadr));

			ip->srcadr = GET_INADDR(pp->srcadr);
			if (client_v6_capable)
				ip->v6_flag = 0;
		}
		ip->srcport = NSRCPORT(&pp->srcadr);
		ip->flags = 0;
		if (pp == sys_peer)
		    ip->flags |= INFO_FLAG_SYSPEER;
		if (pp->flags & FLAG_CONFIG)
		    ip->flags |= INFO_FLAG_CONFIG;
		if (pp->flags & FLAG_REFCLOCK)
		    ip->flags |= INFO_FLAG_REFCLOCK;
		if (pp->flags & FLAG_AUTHENABLE)
		    ip->flags |= INFO_FLAG_AUTHENABLE;
		if (pp->flags & FLAG_PREFER)
		    ip->flags |= INFO_FLAG_PREFER;
		if (pp->flags & FLAG_BURST)
		    ip->flags |= INFO_FLAG_BURST;
		if (pp->status == CTL_PST_SEL_SYNCCAND)
		    ip->flags |= INFO_FLAG_SEL_CANDIDATE;
		if (pp->status >= CTL_PST_SEL_SYSPEER)
		    ip->flags |= INFO_FLAG_SHORTLIST;
		ip->leap = pp->leap;
		ip->hmode = pp->hmode;
		ip->keyid = pp->keyid;
		ip->stratum = pp->stratum;
		ip->ppoll = pp->ppoll;
		ip->hpoll = pp->hpoll;
		ip->precision = pp->precision;
		ip->version = pp->version;
		ip->reach = pp->reach;
		ip->unreach = (u_char) pp->unreach;
		ip->flash = (u_char)pp->flash;
		ip->flash2 = (u_short) pp->flash;
		ip->estbdelay = HTONS_FP(DTOFP(pp->estbdelay));
		ip->ttl = pp->ttl;
		ip->associd = htons(pp->associd);
		ip->rootdelay = HTONS_FP(DTOUFP(pp->rootdelay));
		ip->rootdispersion = HTONS_FP(DTOUFP(pp->rootdispersion));
		ip->refid = pp->refid;
		HTONL_FP(&pp->reftime, &ip->reftime);
		HTONL_FP(&pp->org, &ip->org);
		HTONL_FP(&pp->rec, &ip->rec);
		HTONL_FP(&pp->xmt, &ip->xmt);
		j = pp->filter_nextpt - 1;
		for (i = 0; i < NTP_SHIFT; i++, j--) {
			if (j < 0)
			    j = NTP_SHIFT-1;
			ip->filtdelay[i] = HTONS_FP(DTOFP(pp->filter_delay[j]));
			DTOLFP(pp->filter_offset[j], &ltmp);
			HTONL_FP(&ltmp, &ip->filtoffset[i]);
			ip->order[i] = (u_char)((pp->filter_nextpt+NTP_SHIFT-1)
				- pp->filter_order[i]);
			if (ip->order[i] >= NTP_SHIFT)
			    ip->order[i] -= NTP_SHIFT;
		}
		DTOLFP(pp->offset, &ltmp);
		HTONL_FP(&ltmp, &ip->offset);
		ip->delay = HTONS_FP(DTOFP(pp->delay));
		ip->dispersion = HTONS_FP(DTOUFP(SQRT(pp->disp)));
		ip->selectdisp = HTONS_FP(DTOUFP(SQRT(pp->jitter)));
		ip = (struct info_peer *)more_pkt();
	}
	flush_pkt();
}


/*
 * peer_stats - send statistics for one or more peers
 */
static void
peer_stats (
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_peer_list *ipl;
	register struct peer *pp;
	register struct info_peer_stats *ip;
	register int items;
	struct sockaddr_storage addr;
	extern struct peer *sys_peer;

#ifdef DEBUG
	if (debug)
	     printf("peer_stats: called\n");
#endif
	items = INFO_NITEMS(inpkt->err_nitems);
	ipl = (struct info_peer_list *) inpkt->data;
	ip = (struct info_peer_stats *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_peer_stats));
	while (items-- > 0 && ip != 0) {
		memset((char *)&addr, 0, sizeof(addr));
		NSRCPORT(&addr) = ipl->port;
		if (client_v6_capable && ipl->v6_flag) {
			addr.ss_family = AF_INET6;
			GET_INADDR6(addr) = ipl->addr6;
		} else {
			addr.ss_family = AF_INET;
			GET_INADDR(addr) = ipl->addr;
		}	
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		addr.ss_len = SOCKLEN(&addr);
#endif
#ifdef DEBUG
		if (debug)
		    printf("peer_stats: looking for %s, %d, %d\n", stoa(&addr),
		    ipl->port, ((struct sockaddr_in6 *)&addr)->sin6_port);
#endif
		ipl = (struct info_peer_list *)((char *)ipl +
		    INFO_ITEMSIZE(inpkt->mbz_itemsize));

		if ((pp = findexistingpeer(&addr, (struct peer *)0, -1)) == 0)
		    continue;
#ifdef DEBUG
		if (debug)
		     printf("peer_stats: found %s\n", stoa(&addr));
#endif
		if (pp->srcadr.ss_family == AF_INET) {
			if (pp->dstadr)
				ip->dstadr = (pp->processed) ?
					pp->cast_flags == MDF_BCAST ?
					GET_INADDR(pp->dstadr->bcast):
					pp->cast_flags ?
					GET_INADDR(pp->dstadr->sin) ?
					GET_INADDR(pp->dstadr->sin):
					GET_INADDR(pp->dstadr->bcast):
					3 : 7;
			else
				memset(&ip->dstadr, 0, sizeof(ip->dstadr));
			
			ip->srcadr = GET_INADDR(pp->srcadr);
			if (client_v6_capable)
				ip->v6_flag = 0;
		} else {
			if (pp->dstadr)
				ip->dstadr6 = pp->cast_flags == MDF_BCAST ?
					GET_INADDR6(pp->dstadr->bcast):
					GET_INADDR6(pp->dstadr->sin);
			else
				memset(&ip->dstadr6, 0, sizeof(ip->dstadr6));
			
			ip->srcadr6 = GET_INADDR6(pp->srcadr);
			ip->v6_flag = 1;
		}	
		ip->srcport = NSRCPORT(&pp->srcadr);
		ip->flags = 0;
		if (pp == sys_peer)
		    ip->flags |= INFO_FLAG_SYSPEER;
		if (pp->flags & FLAG_CONFIG)
		    ip->flags |= INFO_FLAG_CONFIG;
		if (pp->flags & FLAG_REFCLOCK)
		    ip->flags |= INFO_FLAG_REFCLOCK;
		if (pp->flags & FLAG_AUTHENABLE)
		    ip->flags |= INFO_FLAG_AUTHENABLE;
		if (pp->flags & FLAG_PREFER)
		    ip->flags |= INFO_FLAG_PREFER;
		if (pp->flags & FLAG_BURST)
		    ip->flags |= INFO_FLAG_BURST;
		if (pp->flags & FLAG_IBURST)
		    ip->flags |= INFO_FLAG_IBURST;
		if (pp->status == CTL_PST_SEL_SYNCCAND)
		    ip->flags |= INFO_FLAG_SEL_CANDIDATE;
		if (pp->status >= CTL_PST_SEL_SYSPEER)
		    ip->flags |= INFO_FLAG_SHORTLIST;
		ip->flags = htons(ip->flags);
		ip->timereceived = htonl((u_int32)(current_time - pp->timereceived));
		ip->timetosend = htonl(pp->nextdate - current_time);
		ip->timereachable = htonl((u_int32)(current_time - pp->timereachable));
		ip->sent = htonl((u_int32)(pp->sent));
		ip->processed = htonl((u_int32)(pp->processed));
		ip->badauth = htonl((u_int32)(pp->badauth));
		ip->bogusorg = htonl((u_int32)(pp->bogusorg));
		ip->oldpkt = htonl((u_int32)(pp->oldpkt));
		ip->seldisp = htonl((u_int32)(pp->seldisptoolarge));
		ip->selbroken = htonl((u_int32)(pp->selbroken));
		ip->candidate = pp->status;
		ip = (struct info_peer_stats *)more_pkt();
	}
	flush_pkt();
}


/*
 * sys_info - return system info
 */
static void
sys_info(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_sys *is;

	is = (struct info_sys *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_sys));

	if (sys_peer != 0) {
		if (sys_peer->srcadr.ss_family == AF_INET) {
			is->peer = GET_INADDR(sys_peer->srcadr);
			if (client_v6_capable)
				is->v6_flag = 0;
		} else if (client_v6_capable) {
			is->peer6 = GET_INADDR6(sys_peer->srcadr);
			is->v6_flag = 1;
		}
		is->peer_mode = sys_peer->hmode;
	} else {
		is->peer = 0;
		if (client_v6_capable) {
			is->v6_flag = 0;
		}
		is->peer_mode = 0;
	}

	is->leap = sys_leap;
	is->stratum = sys_stratum;
	is->precision = sys_precision;
	is->rootdelay = htonl(DTOFP(sys_rootdelay));
	is->rootdispersion = htonl(DTOUFP(sys_rootdispersion));
	is->frequency = htonl(DTOFP(sys_jitter));
	is->stability = htonl(DTOUFP(clock_stability));
	is->refid = sys_refid;
	HTONL_FP(&sys_reftime, &is->reftime);

	is->poll = sys_poll;
	
	is->flags = 0;
	if (sys_authenticate)
		is->flags |= INFO_FLAG_AUTHENTICATE;
	if (sys_bclient)
		is->flags |= INFO_FLAG_BCLIENT;
#ifdef REFCLOCK
	if (cal_enable)
		is->flags |= INFO_FLAG_CAL;
#endif /* REFCLOCK */
	if (kern_enable)
		is->flags |= INFO_FLAG_KERNEL;
	if (mon_enabled != MON_OFF)
		is->flags |= INFO_FLAG_MONITOR;
	if (ntp_enable)
		is->flags |= INFO_FLAG_NTP;
	if (pps_enable)
		is->flags |= INFO_FLAG_PPS_SYNC;
	if (stats_control)
		is->flags |= INFO_FLAG_FILEGEN;
	is->bdelay = HTONS_FP(DTOFP(sys_bdelay));
	HTONL_UF(sys_authdelay.l_f, &is->authdelay);

	(void) more_pkt();
	flush_pkt();
}


/*
 * sys_stats - return system statistics
 */
static void
sys_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_sys_stats *ss;

	/*
	 * Importations from the protocol module
	 */
	ss = (struct info_sys_stats *)prepare_pkt(srcadr, inter, inpkt,
		sizeof(struct info_sys_stats));
	ss->timeup = htonl((u_int32)current_time);
	ss->timereset = htonl((u_int32)(current_time - sys_stattime));
	ss->denied = htonl((u_int32)sys_restricted);
	ss->oldversionpkt = htonl((u_int32)sys_oldversionpkt);
	ss->newversionpkt = htonl((u_int32)sys_newversionpkt);
	ss->unknownversion = htonl((u_int32)sys_unknownversion);
	ss->badlength = htonl((u_int32)sys_badlength);
	ss->processed = htonl((u_int32)sys_processed);
	ss->badauth = htonl((u_int32)sys_badauth);
	ss->limitrejected = htonl((u_int32)sys_limitrejected);
	ss->received = htonl((u_int32)sys_received);
	(void) more_pkt();
	flush_pkt();
}


/*
 * mem_stats - return memory statistics
 */
static void
mem_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_mem_stats *ms;
	register int i;

	/*
	 * Importations from the peer module
	 */
	extern int peer_hash_count[NTP_HASH_SIZE];
	extern int peer_free_count;
	extern u_long peer_timereset;
	extern u_long findpeer_calls;
	extern u_long peer_allocations;
	extern u_long peer_demobilizations;
	extern int total_peer_structs;

	ms = (struct info_mem_stats *)prepare_pkt(srcadr, inter, inpkt,
						  sizeof(struct info_mem_stats));

	ms->timereset = htonl((u_int32)(current_time - peer_timereset));
	ms->totalpeermem = htons((u_short)total_peer_structs);
	ms->freepeermem = htons((u_short)peer_free_count);
	ms->findpeer_calls = htonl((u_int32)findpeer_calls);
	ms->allocations = htonl((u_int32)peer_allocations);
	ms->demobilizations = htonl((u_int32)peer_demobilizations);

	for (i = 0; i < NTP_HASH_SIZE; i++) {
		if (peer_hash_count[i] > 255)
		    ms->hashcount[i] = 255;
		else
		    ms->hashcount[i] = (u_char)peer_hash_count[i];
	}

	(void) more_pkt();
	flush_pkt();
}


/*
 * io_stats - return io statistics
 */
static void
io_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_io_stats *io;

	/*
	 * Importations from the io module
	 */
	extern u_long io_timereset;
	
	io = (struct info_io_stats *)prepare_pkt(srcadr, inter, inpkt,
						 sizeof(struct info_io_stats));

	io->timereset = htonl((u_int32)(current_time - io_timereset));
	io->totalrecvbufs = htons((u_short) total_recvbuffs());
	io->freerecvbufs = htons((u_short) free_recvbuffs());
	io->fullrecvbufs = htons((u_short) full_recvbuffs());
	io->lowwater = htons((u_short) lowater_additions());
	io->dropped = htonl((u_int32)packets_dropped);
	io->ignored = htonl((u_int32)packets_ignored);
	io->received = htonl((u_int32)packets_received);
	io->sent = htonl((u_int32)packets_sent);
	io->notsent = htonl((u_int32)packets_notsent);
	io->interrupts = htonl((u_int32)handler_calls);
	io->int_received = htonl((u_int32)handler_pkts);

	(void) more_pkt();
	flush_pkt();
}


/*
 * timer_stats - return timer statistics
 */
static void
timer_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_timer_stats *ts;

	/*
	 * Importations from the timer module
	 */
	extern u_long timer_timereset;
	extern u_long timer_overflows;
	extern u_long timer_xmtcalls;

	ts = (struct info_timer_stats *)prepare_pkt(srcadr, inter, inpkt,
						    sizeof(struct info_timer_stats));

	ts->timereset = htonl((u_int32)(current_time - timer_timereset));
	ts->alarms = htonl((u_int32)alarm_overflow);
	ts->overflows = htonl((u_int32)timer_overflows);
	ts->xmtcalls = htonl((u_int32)timer_xmtcalls);

	(void) more_pkt();
	flush_pkt();
}


/*
 * loop_info - return the current state of the loop filter
 */
static void
loop_info(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_loop *li;
	l_fp ltmp;

	/*
	 * Importations from the loop filter module
	 */
	extern double last_offset;
	extern double drift_comp;
	extern int tc_counter;
	extern u_long sys_clocktime;

	li = (struct info_loop *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_loop));

	DTOLFP(last_offset, &ltmp);
	HTONL_FP(&ltmp, &li->last_offset);
	DTOLFP(drift_comp * 1e6, &ltmp);
	HTONL_FP(&ltmp, &li->drift_comp);
	li->compliance = htonl((u_int32)(tc_counter));
	li->watchdog_timer = htonl((u_int32)(current_time - sys_clocktime));

	(void) more_pkt();
	flush_pkt();
}


/*
 * do_conf - add a peer to the configuration list
 */
static void
do_conf(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	int items;
	u_int fl;
	struct conf_peer *cp; 
	struct conf_peer temp_cp;
	struct sockaddr_storage peeraddr;
	struct sockaddr_in tmp_clock;

	/*
	 * Do a check of everything to see that it looks
	 * okay.  If not, complain about it.  Note we are
	 * very picky here.
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_peer *)inpkt->data;
	memset(&temp_cp, 0, sizeof(struct conf_peer));
	memcpy(&temp_cp, (char *)cp, INFO_ITEMSIZE(inpkt->mbz_itemsize));
	fl = 0;
	while (items-- > 0 && !fl) {
		if (((temp_cp.version) > NTP_VERSION)
		    || ((temp_cp.version) < NTP_OLDVERSION))
		    fl = 1;
		if (temp_cp.hmode != MODE_ACTIVE
		    && temp_cp.hmode != MODE_CLIENT
		    && temp_cp.hmode != MODE_BROADCAST)
		    fl = 1;
		if (temp_cp.flags & ~(CONF_FLAG_AUTHENABLE | CONF_FLAG_PREFER
				  | CONF_FLAG_BURST | CONF_FLAG_IBURST | CONF_FLAG_SKEY))
		    fl = 1;
		cp = (struct conf_peer *)
		    ((char *)cp + INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	if (fl) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	/*
	 * Looks okay, try it out
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_peer *)inpkt->data;  

	while (items-- > 0) {
		memset(&temp_cp, 0, sizeof(struct conf_peer));
		memcpy(&temp_cp, (char *)cp, INFO_ITEMSIZE(inpkt->mbz_itemsize));
		memset((char *)&peeraddr, 0, sizeof(struct sockaddr_storage));

		fl = 0;
		if (temp_cp.flags & CONF_FLAG_AUTHENABLE)
			fl |= FLAG_AUTHENABLE;
		if (temp_cp.flags & CONF_FLAG_PREFER)
			fl |= FLAG_PREFER;
		if (temp_cp.flags & CONF_FLAG_BURST)
		    fl |= FLAG_BURST;
		if (temp_cp.flags & CONF_FLAG_IBURST)
		    fl |= FLAG_IBURST;
		if (temp_cp.flags & CONF_FLAG_SKEY)
			fl |= FLAG_SKEY;
		
		if (client_v6_capable && temp_cp.v6_flag != 0) {
			peeraddr.ss_family = AF_INET6;
			GET_INADDR6(peeraddr) = temp_cp.peeraddr6; 
		} else {
			peeraddr.ss_family = AF_INET;
			GET_INADDR(peeraddr) = temp_cp.peeraddr;
			/*
			 * Make sure the address is valid
			 */
			tmp_clock = *CAST_V4(peeraddr);
			if (
#ifdef REFCLOCK
				!ISREFCLOCKADR(&tmp_clock) &&
#endif
				ISBADADR(&tmp_clock)) {
				req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
				return;
			}

		}
		NSRCPORT(&peeraddr) = htons(NTP_PORT);
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		peeraddr.ss_len = SOCKLEN(&peeraddr);
#endif

		/* XXX W2DO? minpoll/maxpoll arguments ??? */
		if (peer_config(&peeraddr, (struct interface *)0,
		    temp_cp.hmode, temp_cp.version, temp_cp.minpoll, 
		    temp_cp.maxpoll, fl, temp_cp.ttl, temp_cp.keyid,
		    NULL) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}
		cp = (struct conf_peer *)
		    ((char *)cp + INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}

#if 0
/* XXX */
/*
 * dns_a - Snarf DNS info for an association ID
 */
static void
dns_a(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_dns_assoc *dp;
	register int items;
	struct sockaddr_in peeraddr;

	/*
	 * Do a check of everything to see that it looks
	 * okay.  If not, complain about it.  Note we are
	 * very picky here.
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	dp = (struct info_dns_assoc *)inpkt->data;

	/*
	 * Looks okay, try it out
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	dp = (struct info_dns_assoc *)inpkt->data;
	memset((char *)&peeraddr, 0, sizeof(struct sockaddr_in));
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_port = htons(NTP_PORT);

	/*
	 * Make sure the address is valid
	 */
	if (
#ifdef REFCLOCK
		!ISREFCLOCKADR(&peeraddr) &&
#endif
		ISBADADR(&peeraddr)) {
#ifdef REFCLOCK
		msyslog(LOG_ERR, "dns_a: !ISREFCLOCK && ISBADADR");
#else
		msyslog(LOG_ERR, "dns_a: ISBADADR");
#endif
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	while (items-- > 0) {
		associd_t associd;
		size_t hnl;
		struct peer *peer;
		int bogon = 0;

		associd = dp->associd;
		peer = findpeerbyassoc(associd);
		if (peer == 0 || peer->flags & FLAG_REFCLOCK) {
			msyslog(LOG_ERR, "dns_a: %s",
				(peer == 0)
				? "peer == 0"
				: "peer->flags & FLAG_REFCLOCK");
			++bogon;
		}
		peeraddr.sin_addr.s_addr = dp->peeraddr;
		for (hnl = 0; dp->hostname[hnl] && hnl < sizeof dp->hostname; ++hnl) ;
		if (hnl >= sizeof dp->hostname) {
			msyslog(LOG_ERR, "dns_a: hnl (%ld) >= %ld",
				(long)hnl, (long)sizeof dp->hostname);
			++bogon;
		}

		msyslog(LOG_INFO, "dns_a: <%s> for %s, AssocID %d, bogon %d",
			dp->hostname,
			stoa((struct sockaddr_storage *)&peeraddr), associd,
			bogon);

		if (bogon) {
			/* If it didn't work */
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		} else {
#if 0
#ifdef PUBKEY
			crypto_public(peer, dp->hostname);
#endif /* PUBKEY */
#endif
		}

		dp++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}
#endif /* 0 */

/*
 * do_unconf - remove a peer from the configuration list
 */
static void
do_unconf(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct conf_unpeer *cp;
	struct conf_unpeer temp_cp;
	register int items;
	register struct peer *peer;
	struct sockaddr_storage peeraddr;
	int bad, found;

	/*
	 * This is a bit unstructured, but I like to be careful.
	 * We check to see that every peer exists and is actually
	 * configured.  If so, we remove them.  If not, we return
	 * an error.
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_unpeer *)inpkt->data;

	bad = 0;
	while (items-- > 0 && !bad) {
		memset(&temp_cp, 0, sizeof(temp_cp));
		memset(&peeraddr, 0, sizeof(peeraddr));
		memcpy(&temp_cp, cp, INFO_ITEMSIZE(inpkt->mbz_itemsize));
		if (client_v6_capable && temp_cp.v6_flag != 0) {
			peeraddr.ss_family = AF_INET6;
			GET_INADDR6(peeraddr) = temp_cp.peeraddr6;
		} else {
			peeraddr.ss_family = AF_INET;
			GET_INADDR(peeraddr) = temp_cp.peeraddr;
		}
		NSRCPORT(&peeraddr) = htons(NTP_PORT);
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		peeraddr.ss_len = SOCKLEN(&peeraddr);
#endif
		found = 0;
		peer = (struct peer *)0;
#ifdef DEBUG
		if (debug)
		     printf("searching for %s\n", stoa(&peeraddr));
#endif
		while (!found) {
			peer = findexistingpeer(&peeraddr, peer, -1);
			if (peer == (struct peer *)0)
			    break;
			if (peer->flags & FLAG_CONFIG)
			    found = 1;
		}
		if (!found)
		    bad = 1;
		cp = (struct conf_unpeer *)
		    ((char *)cp + INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	if (bad) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}

	/*
	 * Now do it in earnest.
	 */

	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_unpeer *)inpkt->data;
	while (items-- > 0) {
		memset(&temp_cp, 0, sizeof(temp_cp));
		memset(&peeraddr, 0, sizeof(peeraddr));
		memcpy(&temp_cp, cp, INFO_ITEMSIZE(inpkt->mbz_itemsize));
		if (client_v6_capable && temp_cp.v6_flag != 0) {
			peeraddr.ss_family = AF_INET6;
			GET_INADDR6(peeraddr) = temp_cp.peeraddr6;
		} else {
			peeraddr.ss_family = AF_INET;
			GET_INADDR(peeraddr) = temp_cp.peeraddr;
		}
		NSRCPORT(&peeraddr) = htons(NTP_PORT);
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		peeraddr.ss_len = SOCKLEN(&peeraddr);
#endif
		peer_unconfig(&peeraddr, (struct interface *)0, -1);
		cp = (struct conf_unpeer *)
		    ((char *)cp + INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * set_sys_flag - set system flags
 */
static void
set_sys_flag(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	setclr_flags(srcadr, inter, inpkt, 1);
}


/*
 * clr_sys_flag - clear system flags
 */
static void
clr_sys_flag(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	setclr_flags(srcadr, inter, inpkt, 0);
}


/*
 * setclr_flags - do the grunge work of flag setting/clearing
 */
static void
setclr_flags(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt,
	u_long set
	)
{
	register u_int flags;
	int prev_kern_enable;

	prev_kern_enable = kern_enable;
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		msyslog(LOG_ERR, "setclr_flags: err_nitems > 1");
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	flags = ((struct conf_sys_flags *)inpkt->data)->flags;
	flags = ntohl(flags);
	
	if (flags & ~(SYS_FLAG_BCLIENT | SYS_FLAG_PPS |
		      SYS_FLAG_NTP | SYS_FLAG_KERNEL | SYS_FLAG_MONITOR |
		      SYS_FLAG_FILEGEN | SYS_FLAG_AUTH | SYS_FLAG_CAL)) {
		msyslog(LOG_ERR, "setclr_flags: extra flags: %#x",
			flags & ~(SYS_FLAG_BCLIENT | SYS_FLAG_PPS |
				  SYS_FLAG_NTP | SYS_FLAG_KERNEL |
				  SYS_FLAG_MONITOR | SYS_FLAG_FILEGEN |
				  SYS_FLAG_AUTH | SYS_FLAG_CAL));
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	if (flags & SYS_FLAG_BCLIENT)
		proto_config(PROTO_BROADCLIENT, set, 0., NULL);
	if (flags & SYS_FLAG_PPS)
		proto_config(PROTO_PPS, set, 0., NULL);
	if (flags & SYS_FLAG_NTP)
		proto_config(PROTO_NTP, set, 0., NULL);
	if (flags & SYS_FLAG_KERNEL)
		proto_config(PROTO_KERNEL, set, 0., NULL);
	if (flags & SYS_FLAG_MONITOR)
		proto_config(PROTO_MONITOR, set, 0., NULL);
	if (flags & SYS_FLAG_FILEGEN)
		proto_config(PROTO_FILEGEN, set, 0., NULL);
	if (flags & SYS_FLAG_AUTH)
		proto_config(PROTO_AUTHENTICATE, set, 0., NULL);
	if (flags & SYS_FLAG_CAL)
		proto_config(PROTO_CAL, set, 0., NULL);
	req_ack(srcadr, inter, inpkt, INFO_OKAY);

	/* Reset the kernel ntp parameters if the kernel flag changed. */
	if (prev_kern_enable && !kern_enable)
	     	loop_config(LOOP_KERN_CLEAR, 0.0);
	if (!prev_kern_enable && kern_enable)
	     	loop_config(LOOP_DRIFTCOMP, drift_comp);
}


/*
 * list_restrict - return the restrict list
 */
static void
list_restrict(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_restrict *ir;
	register struct restrictlist *rl;
	register struct restrictlist6 *rl6;

#ifdef DEBUG
	if (debug > 2)
	    printf("wants restrict list summary\n");
#endif

	ir = (struct info_restrict *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_restrict));
	
	for (rl = restrictlist; rl != 0 && ir != 0; rl = rl->next) {
		ir->addr = htonl(rl->addr);
		if (client_v6_capable) 
			ir->v6_flag = 0;
		ir->mask = htonl(rl->mask);
		ir->count = htonl((u_int32)rl->count);
		ir->flags = htons(rl->flags);
		ir->mflags = htons(rl->mflags);
		ir = (struct info_restrict *)more_pkt();
	}
	if (client_v6_capable)
		for (rl6 = restrictlist6; rl6 != 0 && ir != 0; rl6 = rl6->next) {
			ir->addr6 = rl6->addr6;
			ir->mask6 = rl6->mask6;
			ir->v6_flag = 1;
			ir->count = htonl((u_int32)rl6->count);
			ir->flags = htons(rl6->flags);
			ir->mflags = htons(rl6->mflags);
			ir = (struct info_restrict *)more_pkt();
		}
	flush_pkt();
}



/*
 * do_resaddflags - add flags to a restrict entry (or create one)
 */
static void
do_resaddflags(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_restrict(srcadr, inter, inpkt, RESTRICT_FLAGS);
}



/*
 * do_ressubflags - remove flags from a restrict entry
 */
static void
do_ressubflags(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_restrict(srcadr, inter, inpkt, RESTRICT_UNFLAG);
}


/*
 * do_unrestrict - remove a restrict entry from the list
 */
static void
do_unrestrict(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_restrict(srcadr, inter, inpkt, RESTRICT_REMOVE);
}





/*
 * do_restrict - do the dirty stuff of dealing with restrictions
 */
static void
do_restrict(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt,
	int op
	)
{
	register struct conf_restrict *cr;
	register int items;
	struct sockaddr_storage matchaddr;
	struct sockaddr_storage matchmask;
	int bad;

	/*
	 * Do a check of the flags to make sure that only
	 * the NTPPORT flag is set, if any.  If not, complain
	 * about it.  Note we are very picky here.
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cr = (struct conf_restrict *)inpkt->data;

	bad = 0;
	cr->flags = ntohs(cr->flags);
	cr->mflags = ntohs(cr->mflags);
	while (items-- > 0 && !bad) {
		if (cr->mflags & ~(RESM_NTPONLY))
		    bad |= 1;
		if (cr->flags & ~(RES_ALLFLAGS))
		    bad |= 2;
		if (cr->mask != htonl(INADDR_ANY)) {
			if (client_v6_capable && cr->v6_flag != 0) {
				if (IN6_IS_ADDR_UNSPECIFIED(&cr->addr6))
					bad |= 4;
			} else
				if (cr->addr == htonl(INADDR_ANY))
					bad |= 8;
		}
		cr = (struct conf_restrict *)((char *)cr +
		    INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	if (bad) {
		msyslog(LOG_ERR, "do_restrict: bad = %#x", bad);
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	/*
	 * Looks okay, try it out
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cr = (struct conf_restrict *)inpkt->data;
	memset((char *)&matchaddr, 0, sizeof(struct sockaddr_storage));
	memset((char *)&matchmask, 0, sizeof(struct sockaddr_storage));

	while (items-- > 0) {
		if (client_v6_capable && cr->v6_flag != 0) {
			GET_INADDR6(matchaddr) = cr->addr6;
			GET_INADDR6(matchmask) = cr->mask6;
			matchaddr.ss_family = AF_INET6;
			matchmask.ss_family = AF_INET6;
		} else {
			GET_INADDR(matchaddr) = cr->addr;
			GET_INADDR(matchmask) = cr->mask;
			matchaddr.ss_family = AF_INET;
			matchmask.ss_family = AF_INET;
		}
		hack_restrict(op, &matchaddr, &matchmask, cr->mflags,
			 cr->flags);
		cr++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * mon_getlist - return monitor data
 */
static void
mon_getlist_0(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_monitor *im;
	register struct mon_data *md;
	extern struct mon_data mon_mru_list;
	extern int mon_enabled;

#ifdef DEBUG
	if (debug > 2)
	    printf("wants monitor 0 list\n");
#endif
	if (!mon_enabled) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}
	im = (struct info_monitor *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_monitor));
	for (md = mon_mru_list.mru_next; md != &mon_mru_list && im != 0;
	     md = md->mru_next) {
		im->lasttime = htonl((u_int32)md->avg_interval);
		im->firsttime = htonl((u_int32)(current_time - md->lasttime));
		im->lastdrop = htonl((u_int32)md->drop_count);
		im->count = htonl((u_int32)(md->count));
		if (md->rmtadr.ss_family == AF_INET6) {
			if (!client_v6_capable)
				continue;
			im->addr6 = GET_INADDR6(md->rmtadr);
			im->v6_flag = 1;
		} else {
			im->addr = GET_INADDR(md->rmtadr);
			if (client_v6_capable)
				im->v6_flag = 0;
		}
		im->port = md->rmtport;
		im->mode = md->mode;
		im->version = md->version;
		im = (struct info_monitor *)more_pkt();
	}
	flush_pkt();
}

/*
 * mon_getlist - return monitor data
 */
static void
mon_getlist_1(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_monitor_1 *im;
	register struct mon_data *md;
	extern struct mon_data mon_mru_list;
	extern int mon_enabled;

	if (!mon_enabled) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}
	im = (struct info_monitor_1 *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_monitor_1));
	for (md = mon_mru_list.mru_next; md != &mon_mru_list && im != 0;
	     md = md->mru_next) {
		im->lasttime = htonl((u_int32)md->avg_interval);
		im->firsttime = htonl((u_int32)(current_time - md->lasttime));
		im->lastdrop = htonl((u_int32)md->drop_count);
		im->count = htonl((u_int32)md->count);
		if (md->rmtadr.ss_family == AF_INET6) {
			if (!client_v6_capable)
				continue;
			im->addr6 = GET_INADDR6(md->rmtadr);
			im->v6_flag = 1;
			im->daddr6 = GET_INADDR6(md->interface->sin);
		} else {
			im->addr = GET_INADDR(md->rmtadr);
			if (client_v6_capable)
				im->v6_flag = 0;
			im->daddr = (md->cast_flags == MDF_BCAST)  
				? GET_INADDR(md->interface->bcast) 
				: (md->cast_flags 
				? (GET_INADDR(md->interface->sin)
				? GET_INADDR(md->interface->sin)
				: GET_INADDR(md->interface->bcast))
				: 4);
		}
		im->flags = htonl(md->cast_flags);
		im->port = md->rmtport;
		im->mode = md->mode;
		im->version = md->version;
		im = (struct info_monitor_1 *)more_pkt();
	}
	flush_pkt();
}

/*
 * Module entry points and the flags they correspond with
 */
struct reset_entry {
	int flag;		/* flag this corresponds to */
	void (*handler) P((void)); /* routine to handle request */
};

struct reset_entry reset_entries[] = {
	{ RESET_FLAG_ALLPEERS,	peer_all_reset },
	{ RESET_FLAG_IO,	io_clr_stats },
	{ RESET_FLAG_SYS,	proto_clr_stats },
	{ RESET_FLAG_MEM,	peer_clr_stats },
	{ RESET_FLAG_TIMER,	timer_clr_stats },
	{ RESET_FLAG_AUTH,	reset_auth_stats },
	{ RESET_FLAG_CTL,	ctl_clr_stats },
	{ 0,			0 }
};

/*
 * reset_stats - reset statistic counters here and there
 */
static void
reset_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	u_long flags;
	struct reset_entry *rent;

	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		msyslog(LOG_ERR, "reset_stats: err_nitems > 1");
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	flags = ((struct reset_flags *)inpkt->data)->flags;
	flags = ntohl(flags);
     
	if (flags & ~RESET_ALLFLAGS) {
		msyslog(LOG_ERR, "reset_stats: reset leaves %#lx",
			flags & ~RESET_ALLFLAGS);
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	for (rent = reset_entries; rent->flag != 0; rent++) {
		if (flags & rent->flag)
		    (rent->handler)();
	}
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * reset_peer - clear a peer's statistics
 */
static void
reset_peer(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct conf_unpeer *cp;
	register int items;
	register struct peer *peer;
	struct sockaddr_storage peeraddr;
	int bad;

	/*
	 * We check first to see that every peer exists.  If not,
	 * we return an error.
	 */

	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_unpeer *)inpkt->data;

	bad = 0;
	while (items-- > 0 && !bad) {
		memset((char *)&peeraddr, 0, sizeof(peeraddr));
		if (client_v6_capable && cp->v6_flag != 0) {
			GET_INADDR6(peeraddr) = cp->peeraddr6;
			peeraddr.ss_family = AF_INET6;
		} else {
			GET_INADDR(peeraddr) = cp->peeraddr;
			peeraddr.ss_family = AF_INET;
		}
		NSRCPORT(&peeraddr) = htons(NTP_PORT);
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		peeraddr.ss_len = SOCKLEN(&peeraddr);
#endif
		peer = findexistingpeer(&peeraddr, (struct peer *)0, -1);
		if (peer == (struct peer *)0)
		    bad++;
		cp = (struct conf_unpeer *)((char *)cp +
		    INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	if (bad) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}

	/*
	 * Now do it in earnest.
	 */

	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_unpeer *)inpkt->data;
	while (items-- > 0) {
		memset((char *)&peeraddr, 0, sizeof(peeraddr));
		if (client_v6_capable && cp->v6_flag != 0) {
			GET_INADDR6(peeraddr) = cp->peeraddr6;
			peeraddr.ss_family = AF_INET6;
		} else {
			GET_INADDR(peeraddr) = cp->peeraddr;
			peeraddr.ss_family = AF_INET;
		}
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		peeraddr.ss_len = SOCKLEN(&peeraddr);
#endif
		peer = findexistingpeer(&peeraddr, (struct peer *)0, -1);
		while (peer != 0) {
			peer_reset(peer);
			peer = findexistingpeer(&peeraddr, (struct peer *)peer, -1);
		}
		cp = (struct conf_unpeer *)((char *)cp +
		    INFO_ITEMSIZE(inpkt->mbz_itemsize));
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * do_key_reread - reread the encryption key file
 */
static void
do_key_reread(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	rereadkeys();
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * trust_key - make one or more keys trusted
 */
static void
trust_key(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_trustkey(srcadr, inter, inpkt, 1);
}


/*
 * untrust_key - make one or more keys untrusted
 */
static void
untrust_key(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_trustkey(srcadr, inter, inpkt, 0);
}


/*
 * do_trustkey - make keys either trustable or untrustable
 */
static void
do_trustkey(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt,
	u_long trust
	)
{
	register u_long *kp;
	register int items;

	items = INFO_NITEMS(inpkt->err_nitems);
	kp = (u_long *)inpkt->data;
	while (items-- > 0) {
		authtrust(*kp, trust);
		kp++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * get_auth_info - return some stats concerning the authentication module
 */
static void
get_auth_info(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_auth *ia;

	/*
	 * Importations from the authentication module
	 */
	extern u_long authnumkeys;
	extern int authnumfreekeys;
	extern u_long authkeylookups;
	extern u_long authkeynotfound;
	extern u_long authencryptions;
	extern u_long authdecryptions;
	extern u_long authkeyuncached;
	extern u_long authkeyexpired;

	ia = (struct info_auth *)prepare_pkt(srcadr, inter, inpkt,
					     sizeof(struct info_auth));

	ia->numkeys = htonl((u_int32)authnumkeys);
	ia->numfreekeys = htonl((u_int32)authnumfreekeys);
	ia->keylookups = htonl((u_int32)authkeylookups);
	ia->keynotfound = htonl((u_int32)authkeynotfound);
	ia->encryptions = htonl((u_int32)authencryptions);
	ia->decryptions = htonl((u_int32)authdecryptions);
	ia->keyuncached = htonl((u_int32)authkeyuncached);
	ia->expired = htonl((u_int32)authkeyexpired);
	ia->timereset = htonl((u_int32)(current_time - auth_timereset));
	
	(void) more_pkt();
	flush_pkt();
}



/*
 * reset_auth_stats - reset the authentication stat counters.  Done here
 *		      to keep ntp-isms out of the authentication module
 */
static void
reset_auth_stats(void)
{
	/*
	 * Importations from the authentication module
	 */
	extern u_long authkeylookups;
	extern u_long authkeynotfound;
	extern u_long authencryptions;
	extern u_long authdecryptions;
	extern u_long authkeyuncached;

	authkeylookups = 0;
	authkeynotfound = 0;
	authencryptions = 0;
	authdecryptions = 0;
	authkeyuncached = 0;
	auth_timereset = current_time;
}


/*
 * req_get_traps - return information about current trap holders
 */
static void
req_get_traps(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_trap *it;
	register struct ctl_trap *tr;
	register int i;

	/*
	 * Imported from the control module
	 */
	extern struct ctl_trap ctl_trap[];
	extern int num_ctl_traps;

	if (num_ctl_traps == 0) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}

	it = (struct info_trap *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_trap));

	for (i = 0, tr = ctl_trap; i < CTL_MAXTRAPS; i++, tr++) {
		if (tr->tr_flags & TRAP_INUSE) {
			if (tr->tr_addr.ss_family == AF_INET) {
				if (tr->tr_localaddr == any_interface)
					it->local_address = 0;
				else
					it->local_address
					    = GET_INADDR(tr->tr_localaddr->sin);
				it->trap_address = GET_INADDR(tr->tr_addr);
				if (client_v6_capable)
					it->v6_flag = 0;
			} else {
				if (!client_v6_capable)
					continue;
				it->local_address6 
				    = GET_INADDR6(tr->tr_localaddr->sin);
				it->trap_address6 = GET_INADDR6(tr->tr_addr);
				it->v6_flag = 1;
			}
			it->trap_port = NSRCPORT(&tr->tr_addr);
			it->sequence = htons(tr->tr_sequence);
			it->settime = htonl((u_int32)(current_time - tr->tr_settime));
			it->origtime = htonl((u_int32)(current_time - tr->tr_origtime));
			it->resets = htonl((u_int32)tr->tr_resets);
			it->flags = htonl((u_int32)tr->tr_flags);
			it = (struct info_trap *)more_pkt();
		}
	}
	flush_pkt();
}


/*
 * req_set_trap - configure a trap
 */
static void
req_set_trap(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_setclr_trap(srcadr, inter, inpkt, 1);
}



/*
 * req_clr_trap - unconfigure a trap
 */
static void
req_clr_trap(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	do_setclr_trap(srcadr, inter, inpkt, 0);
}



/*
 * do_setclr_trap - do the grunge work of (un)configuring a trap
 */
static void
do_setclr_trap(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt,
	int set
	)
{
	register struct conf_trap *ct;
	register struct interface *linter;
	int res;
	struct sockaddr_storage laddr;

	/*
	 * Prepare sockaddr_storage structure
	 */
	memset((char *)&laddr, 0, sizeof laddr);
	laddr.ss_family = srcadr->ss_family;
	NSRCPORT(&laddr) = ntohs(NTP_PORT);

	/*
	 * Restrict ourselves to one item only.  This eliminates
	 * the error reporting problem.
	 */
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		msyslog(LOG_ERR, "do_setclr_trap: err_nitems > 1");
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}
	ct = (struct conf_trap *)inpkt->data;

	/*
	 * Look for the local interface.  If none, use the default.
	 */
	if (ct->local_address == 0) {
		linter = any_interface;
	} else {
		if (laddr.ss_family == AF_INET)
			GET_INADDR(laddr) = ct->local_address;
		else
			GET_INADDR6(laddr) = ct->local_address6;
		linter = findinterface(&laddr);
		if (linter == NULL) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}
	}

	if (laddr.ss_family == AF_INET)
		GET_INADDR(laddr) = ct->trap_address;
	else
		GET_INADDR6(laddr) = ct->trap_address6;
	if (ct->trap_port != 0)
	    NSRCPORT(&laddr) = ct->trap_port;
	else
	    NSRCPORT(&laddr) = htons(TRAPPORT);

	if (set) {
		res = ctlsettrap(&laddr, linter, 0,
				 INFO_VERSION(inpkt->rm_vn_mode));
	} else {
		res = ctlclrtrap(&laddr, linter, 0);
	}

	if (!res) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
	} else {
		req_ack(srcadr, inter, inpkt, INFO_OKAY);
	}
	return;
}



/*
 * set_request_keyid - set the keyid used to authenticate requests
 */
static void
set_request_keyid(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	keyid_t keyid;

	/*
	 * Restrict ourselves to one item only.
	 */
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		msyslog(LOG_ERR, "set_request_keyid: err_nitems > 1");
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	keyid = ntohl(*((u_int32 *)(inpkt->data)));
	info_auth_keyid = keyid;
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}



/*
 * set_control_keyid - set the keyid used to authenticate requests
 */
static void
set_control_keyid(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	keyid_t keyid;
	extern keyid_t ctl_auth_keyid;

	/*
	 * Restrict ourselves to one item only.
	 */
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		msyslog(LOG_ERR, "set_control_keyid: err_nitems > 1");
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	keyid = ntohl(*((u_int32 *)(inpkt->data)));
	ctl_auth_keyid = keyid;
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}



/*
 * get_ctl_stats - return some stats concerning the control message module
 */
static void
get_ctl_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_control *ic;

	/*
	 * Importations from the control module
	 */
	extern u_long ctltimereset;
	extern u_long numctlreq;
	extern u_long numctlbadpkts;
	extern u_long numctlresponses;
	extern u_long numctlfrags;
	extern u_long numctlerrors;
	extern u_long numctltooshort;
	extern u_long numctlinputresp;
	extern u_long numctlinputfrag;
	extern u_long numctlinputerr;
	extern u_long numctlbadoffset;
	extern u_long numctlbadversion;
	extern u_long numctldatatooshort;
	extern u_long numctlbadop;
	extern u_long numasyncmsgs;

	ic = (struct info_control *)prepare_pkt(srcadr, inter, inpkt,
						sizeof(struct info_control));

	ic->ctltimereset = htonl((u_int32)(current_time - ctltimereset));
	ic->numctlreq = htonl((u_int32)numctlreq);
	ic->numctlbadpkts = htonl((u_int32)numctlbadpkts);
	ic->numctlresponses = htonl((u_int32)numctlresponses);
	ic->numctlfrags = htonl((u_int32)numctlfrags);
	ic->numctlerrors = htonl((u_int32)numctlerrors);
	ic->numctltooshort = htonl((u_int32)numctltooshort);
	ic->numctlinputresp = htonl((u_int32)numctlinputresp);
	ic->numctlinputfrag = htonl((u_int32)numctlinputfrag);
	ic->numctlinputerr = htonl((u_int32)numctlinputerr);
	ic->numctlbadoffset = htonl((u_int32)numctlbadoffset);
	ic->numctlbadversion = htonl((u_int32)numctlbadversion);
	ic->numctldatatooshort = htonl((u_int32)numctldatatooshort);
	ic->numctlbadop = htonl((u_int32)numctlbadop);
	ic->numasyncmsgs = htonl((u_int32)numasyncmsgs);

	(void) more_pkt();
	flush_pkt();
}


#ifdef KERNEL_PLL
/*
 * get_kernel_info - get kernel pll/pps information
 */
static void
get_kernel_info(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_kernel *ik;
	struct timex ntx;

	if (!pll_control) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}

	memset((char *)&ntx, 0, sizeof(ntx));
	if (ntp_adjtime(&ntx) < 0)
		msyslog(LOG_ERR, "get_kernel_info: ntp_adjtime() failed: %m");
	ik = (struct info_kernel *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_kernel));

	/*
	 * pll variables
	 */
	ik->offset = htonl((u_int32)ntx.offset);
	ik->freq = htonl((u_int32)ntx.freq);
	ik->maxerror = htonl((u_int32)ntx.maxerror);
	ik->esterror = htonl((u_int32)ntx.esterror);
	ik->status = htons(ntx.status);
	ik->constant = htonl((u_int32)ntx.constant);
	ik->precision = htonl((u_int32)ntx.precision);
	ik->tolerance = htonl((u_int32)ntx.tolerance);

	/*
	 * pps variables
	 */
	ik->ppsfreq = htonl((u_int32)ntx.ppsfreq);
	ik->jitter = htonl((u_int32)ntx.jitter);
	ik->shift = htons(ntx.shift);
	ik->stabil = htonl((u_int32)ntx.stabil);
	ik->jitcnt = htonl((u_int32)ntx.jitcnt);
	ik->calcnt = htonl((u_int32)ntx.calcnt);
	ik->errcnt = htonl((u_int32)ntx.errcnt);
	ik->stbcnt = htonl((u_int32)ntx.stbcnt);
	
	(void) more_pkt();
	flush_pkt();
}
#endif /* KERNEL_PLL */


#ifdef REFCLOCK
/*
 * get_clock_info - get info about a clock
 */
static void
get_clock_info(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct info_clock *ic;
	register u_int32 *clkaddr;
	register int items;
	struct refclockstat clock_stat;
	struct sockaddr_storage addr;
	struct sockaddr_in tmp_clock;
	l_fp ltmp;

	memset((char *)&addr, 0, sizeof addr);
	addr.ss_family = AF_INET;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	addr.ss_len = SOCKLEN(&addr);
#endif
	NSRCPORT(&addr) = htons(NTP_PORT);
	items = INFO_NITEMS(inpkt->err_nitems);
	clkaddr = (u_int32 *) inpkt->data;

	ic = (struct info_clock *)prepare_pkt(srcadr, inter, inpkt,
					      sizeof(struct info_clock));

	while (items-- > 0) {
		tmp_clock.sin_addr.s_addr = *clkaddr++;
		CAST_V4(addr)->sin_addr = tmp_clock.sin_addr;
		if (!ISREFCLOCKADR(&tmp_clock) ||
		    findexistingpeer(&addr, (struct peer *)0, -1) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		clock_stat.kv_list = (struct ctl_var *)0;

		refclock_control(&addr, (struct refclockstat *)0, &clock_stat);

		ic->clockadr = tmp_clock.sin_addr.s_addr;
		ic->type = clock_stat.type;
		ic->flags = clock_stat.flags;
		ic->lastevent = clock_stat.lastevent;
		ic->currentstatus = clock_stat.currentstatus;
		ic->polls = htonl((u_int32)clock_stat.polls);
		ic->noresponse = htonl((u_int32)clock_stat.noresponse);
		ic->badformat = htonl((u_int32)clock_stat.badformat);
		ic->baddata = htonl((u_int32)clock_stat.baddata);
		ic->timestarted = htonl((u_int32)clock_stat.timereset);
		DTOLFP(clock_stat.fudgetime1, &ltmp);
		HTONL_FP(&ltmp, &ic->fudgetime1);
		DTOLFP(clock_stat.fudgetime2, &ltmp);
		HTONL_FP(&ltmp, &ic->fudgetime2);
		ic->fudgeval1 = htonl((u_int32)clock_stat.fudgeval1);
		ic->fudgeval2 = htonl((u_int32)clock_stat.fudgeval2);

		free_varlist(clock_stat.kv_list);

		ic = (struct info_clock *)more_pkt();
	}
	flush_pkt();
}



/*
 * set_clock_fudge - get a clock's fudge factors
 */
static void
set_clock_fudge(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register struct conf_fudge *cf;
	register int items;
	struct refclockstat clock_stat;
	struct sockaddr_storage addr;
	struct sockaddr_in tmp_clock;
	l_fp ltmp;

	memset((char *)&addr, 0, sizeof addr);
	memset((char *)&clock_stat, 0, sizeof clock_stat);
	items = INFO_NITEMS(inpkt->err_nitems);
	cf = (struct conf_fudge *) inpkt->data;

	while (items-- > 0) {
		tmp_clock.sin_addr.s_addr = cf->clockadr;
		*CAST_V4(addr) = tmp_clock;
		addr.ss_family = AF_INET;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		addr.ss_len = SOCKLEN(&addr);
#endif
		NSRCPORT(&addr) = htons(NTP_PORT);
		if (!ISREFCLOCKADR(&tmp_clock) ||
		    findexistingpeer(&addr, (struct peer *)0, -1) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		switch(ntohl(cf->which)) {
		    case FUDGE_TIME1:
			NTOHL_FP(&cf->fudgetime, &ltmp);
			LFPTOD(&ltmp, clock_stat.fudgetime1);
			clock_stat.haveflags = CLK_HAVETIME1;
			break;
		    case FUDGE_TIME2:
			NTOHL_FP(&cf->fudgetime, &ltmp);
			LFPTOD(&ltmp, clock_stat.fudgetime2);
			clock_stat.haveflags = CLK_HAVETIME2;
			break;
		    case FUDGE_VAL1:
			clock_stat.fudgeval1 = ntohl(cf->fudgeval_flags);
			clock_stat.haveflags = CLK_HAVEVAL1;
			break;
		    case FUDGE_VAL2:
			clock_stat.fudgeval2 = ntohl(cf->fudgeval_flags);
			clock_stat.haveflags = CLK_HAVEVAL2;
			break;
		    case FUDGE_FLAGS:
			clock_stat.flags = (u_char) (ntohl(cf->fudgeval_flags) & 0xf);
			clock_stat.haveflags =
				(CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3|CLK_HAVEFLAG4);
			break;
		    default:
			msyslog(LOG_ERR, "set_clock_fudge: default!");
			req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
			return;
		}

		refclock_control(&addr, &clock_stat, (struct refclockstat *)0);
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}
#endif

#ifdef REFCLOCK
/*
 * get_clkbug_info - get debugging info about a clock
 */
static void
get_clkbug_info(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	register int i;
	register struct info_clkbug *ic;
	register u_int32 *clkaddr;
	register int items;
	struct refclockbug bug;
	struct sockaddr_storage addr;
	struct sockaddr_in tmp_clock;

	memset((char *)&addr, 0, sizeof addr);
	addr.ss_family = AF_INET;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	addr.ss_len = SOCKLEN(&addr);
#endif
	NSRCPORT(&addr) = htons(NTP_PORT);
	items = INFO_NITEMS(inpkt->err_nitems);
	clkaddr = (u_int32 *) inpkt->data;

	ic = (struct info_clkbug *)prepare_pkt(srcadr, inter, inpkt,
					       sizeof(struct info_clkbug));

	while (items-- > 0) {
		tmp_clock.sin_addr.s_addr = *clkaddr++;
		GET_INADDR(addr) = tmp_clock.sin_addr.s_addr;
		if (!ISREFCLOCKADR(&tmp_clock) ||
		    findexistingpeer(&addr, (struct peer *)0, -1) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		memset((char *)&bug, 0, sizeof bug);
		refclock_buginfo(&addr, &bug);
		if (bug.nvalues == 0 && bug.ntimes == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		ic->clockadr = tmp_clock.sin_addr.s_addr;
		i = bug.nvalues;
		if (i > NUMCBUGVALUES)
		    i = NUMCBUGVALUES;
		ic->nvalues = (u_char)i;
		ic->svalues = htons((u_short) (bug.svalues & ((1<<i)-1)));
		while (--i >= 0)
		    ic->values[i] = htonl(bug.values[i]);

		i = bug.ntimes;
		if (i > NUMCBUGTIMES)
		    i = NUMCBUGTIMES;
		ic->ntimes = (u_char)i;
		ic->stimes = htonl(bug.stimes);
		while (--i >= 0) {
			HTONL_FP(&bug.times[i], &ic->times[i]);
		}

		ic = (struct info_clkbug *)more_pkt();
	}
	flush_pkt();
}
#endif

/*
 * receiver of interface structures
 */
static void
fill_info_if_stats(void *data, interface_info_t *interface_info)
{
	struct info_if_stats **ifsp = (struct info_if_stats **)data;
	struct info_if_stats *ifs = *ifsp;
	struct interface *interface = interface_info->interface;
	
	memset((char*)ifs, 0, sizeof(*ifs));
	
	if (interface->sin.ss_family == AF_INET6) {
		if (!client_v6_capable) {
			return;
		}
		ifs->v6_flag = 1;
		memcpy((char *)&ifs->unaddr.addr6, (char *)&CAST_V6(interface->sin)->sin6_addr, sizeof(struct in6_addr));
		memcpy((char *)&ifs->unbcast.addr6, (char *)&CAST_V6(interface->bcast)->sin6_addr, sizeof(struct in6_addr));
		memcpy((char *)&ifs->unmask.addr6, (char *)&CAST_V6(interface->mask)->sin6_addr, sizeof(struct in6_addr));
	} else {
		ifs->v6_flag = 0;
		memcpy((char *)&ifs->unaddr.addr, (char *)&CAST_V4(interface->sin)->sin_addr, sizeof(struct in_addr));
		memcpy((char *)&ifs->unbcast.addr, (char *)&CAST_V4(interface->bcast)->sin_addr, sizeof(struct in_addr));
		memcpy((char *)&ifs->unmask.addr, (char *)&CAST_V4(interface->mask)->sin_addr, sizeof(struct in_addr));
	}
	ifs->v6_flag = htonl(ifs->v6_flag);
	strcpy(ifs->name, interface->name);
	ifs->family = htons(interface->family);
	ifs->flags = htonl(interface->flags);
	ifs->last_ttl = htonl(interface->last_ttl);
	ifs->num_mcast = htonl(interface->num_mcast);
	ifs->received = htonl(interface->received);
	ifs->sent = htonl(interface->sent);
	ifs->notsent = htonl(interface->notsent);
	ifs->scopeid = htonl(interface->scopeid);
	ifs->ifindex = htonl(interface->ifindex);
	ifs->ifnum = htonl(interface->ifnum);
	ifs->uptime = htonl(current_time - interface->starttime);
	ifs->ignore_packets = interface->ignore_packets;
	ifs->peercnt = htonl(interface->peercnt);
	ifs->action = interface_info->action;
	
	*ifsp = (struct info_if_stats *)more_pkt();
}

/*
 * get_if_stats - get interface statistics
 */
static void
get_if_stats(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	struct info_if_stats *ifs;

	DPRINTF(3, ("wants interface statistics\n"));

	ifs = (struct info_if_stats *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_if_stats));

	interface_enumerate(fill_info_if_stats, &ifs);
	
	flush_pkt();
}

static void
do_if_reload(
	struct sockaddr_storage *srcadr,
	struct interface *inter,
	struct req_pkt *inpkt
	)
{
	struct info_if_stats *ifs;

	DPRINTF(3, ("wants interface reload\n"));

	ifs = (struct info_if_stats *)prepare_pkt(srcadr, inter, inpkt,
	    v6sizeof(struct info_if_stats));

	interface_update(fill_info_if_stats, &ifs);
	
	flush_pkt();
}

