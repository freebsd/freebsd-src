/*
 * ntp_request.c - respond to information requests
 */
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "signal.h"
#include "ntp_request.h"
#include "ntp_control.h"
#include "ntp_refclock.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

#ifdef KERNEL_PLL
#ifdef HAVE_SYS_TIMEX_H
#include <sys/timex.h>
#else
#include "sys/timex.h"
#endif

#ifndef NTP_SYSCALLS_LIBC
#define ntp_gettime(t)	syscall(SYS_ntp_gettime, (t))
#define ntp_adjtime(t)	syscall(SYS_ntp_adjtime, (t))
#endif
#endif /* KERNEL_PLL */

/*
 * Structure to hold request procedure information
 */
#define	NOAUTH	0
#define	AUTH	1

#define	NO_REQUEST	(-1)

struct req_proc {
	short request_code;	/* defined request code */
	short needs_auth;	/* true when authentication needed */
	short sizeofitem;	/* size of request data item */
	void (*handler)();	/* routine to handle request */
};

/*
 * Universal request codes
 */
static	struct req_proc univ_codes[] = {
	{ NO_REQUEST,		NOAUTH,	 0,	0 }
};

static	void	req_ack	P((struct sockaddr_in *, struct interface *, struct req_pkt *, int));
static	char *	prepare_pkt	P((struct sockaddr_in *, struct interface *, struct req_pkt *, u_int));
static	char *	more_pkt	P((void));
static	void	flush_pkt	P((void));
static	void	peer_list	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	peer_list_sum	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	peer_info	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	peer_stats	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	sys_info	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	sys_stats	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	mem_stats	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	io_stats	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	timer_stats	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	loop_info	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_conf		P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_unconf	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	set_sys_flag	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	clr_sys_flag	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	setclr_flags	P((struct sockaddr_in *, struct interface *, struct req_pkt *, u_long));
static	void	do_monitor	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_nomonitor	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	list_restrict	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_resaddflags	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_ressubflags	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_unrestrict	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_restrict	P((struct sockaddr_in *, struct interface *, struct req_pkt *, int));
static	void	mon_getlist_0	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	mon_getlist_1	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	reset_stats	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	reset_peer	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_key_reread	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_dirty_hack	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	dont_dirty_hack	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	trust_key	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	untrust_key	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_trustkey	P((struct sockaddr_in *, struct interface *, struct req_pkt *, int));
static	void	get_auth_info	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	reset_auth_stats P((void));
static	void	req_get_traps	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	req_set_trap	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	req_clr_trap	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	do_setclr_trap	P((struct sockaddr_in *, struct interface *, struct req_pkt *, int));
static	void	set_request_keyid P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	set_control_keyid P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	get_ctl_stats P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	get_leap_info P((struct sockaddr_in *, struct interface *, struct req_pkt *));
#ifdef KERNEL_PLL
static	void	get_kernel_info P((struct sockaddr_in *, struct interface *, struct req_pkt *));
#endif /* KERNEL_PLL */
#ifdef REFCLOCK
static	void	get_clock_info P((struct sockaddr_in *, struct interface *, struct req_pkt *));
static	void	set_clock_fudge P((struct sockaddr_in *, struct interface *, struct req_pkt *));
#endif	/* REFCLOCK */
static	void	set_precision	P((struct sockaddr_in *, struct interface *, struct req_pkt *));
#ifdef REFCLOCK
static	void	get_clkbug_info P((struct sockaddr_in *, struct interface *, struct req_pkt *));
#endif	/* REFCLOCK */

/*
 * Xntpd request codes
 */
static	struct req_proc xntp_codes[] = {
	{ REQ_PEER_LIST,	NOAUTH,	0,	peer_list },
	{ REQ_PEER_LIST_SUM,	NOAUTH,	0,	peer_list_sum },
	{ REQ_PEER_INFO,    NOAUTH, sizeof(struct info_peer_list), peer_info },
	{ REQ_PEER_STATS,   NOAUTH, sizeof(struct info_peer_list), peer_stats },
	{ REQ_SYS_INFO,		NOAUTH,	0,	sys_info },
	{ REQ_SYS_STATS,	NOAUTH,	0,	sys_stats },
	{ REQ_IO_STATS,		NOAUTH,	0,	io_stats },
	{ REQ_MEM_STATS,	NOAUTH,	0,	mem_stats },
	{ REQ_LOOP_INFO,	NOAUTH,	0,	loop_info },
	{ REQ_TIMER_STATS,	NOAUTH,	0,	timer_stats },
	{ REQ_CONFIG,	    AUTH, sizeof(struct conf_peer), do_conf },
	{ REQ_UNCONFIG,	    AUTH, sizeof(struct conf_unpeer), do_unconf },
	{ REQ_SET_SYS_FLAG, AUTH, sizeof(struct conf_sys_flags), set_sys_flag },
	{ REQ_CLR_SYS_FLAG, AUTH, sizeof(struct conf_sys_flags), clr_sys_flag },
	{ REQ_MONITOR,		AUTH,	0,	do_monitor },
	{ REQ_NOMONITOR,	AUTH,	0,	do_nomonitor },
	{ REQ_GET_RESTRICT,	NOAUTH,	0,	list_restrict },
	{ REQ_RESADDFLAGS, AUTH, sizeof(struct conf_restrict), do_resaddflags },
	{ REQ_RESSUBFLAGS, AUTH, sizeof(struct conf_restrict), do_ressubflags },
	{ REQ_UNRESTRICT,  AUTH, sizeof(struct conf_restrict), do_unrestrict },
	{ REQ_MON_GETLIST,	NOAUTH,	0,	mon_getlist_0 },
	{ REQ_MON_GETLIST_1,	NOAUTH,	0,	mon_getlist_1 },
	{ REQ_RESET_STATS, AUTH, sizeof(struct reset_flags), reset_stats },
	{ REQ_RESET_PEER,  AUTH, sizeof(struct conf_unpeer), reset_peer },
	{ REQ_REREAD_KEYS,	AUTH,	0,	do_key_reread },
	{ REQ_DO_DIRTY_HACK,	AUTH,	0,	do_dirty_hack },
	{ REQ_DONT_DIRTY_HACK,	AUTH,	0,	dont_dirty_hack },
	{ REQ_TRUSTKEY,    AUTH, sizeof(u_long),	trust_key },
	{ REQ_UNTRUSTKEY,  AUTH, sizeof(u_long),	untrust_key },
	{ REQ_AUTHINFO,		NOAUTH,	0,	get_auth_info },
	{ REQ_TRAPS,		NOAUTH, 0,	req_get_traps },
	{ REQ_ADD_TRAP,	   AUTH, sizeof(struct conf_trap), req_set_trap },
	{ REQ_CLR_TRAP,	   AUTH, sizeof(struct conf_trap), req_clr_trap },
	{ REQ_REQUEST_KEY, AUTH, sizeof(u_long),	set_request_keyid },
	{ REQ_CONTROL_KEY, AUTH, sizeof(u_long),	set_control_keyid },
	{ REQ_GET_CTLSTATS,	NOAUTH,	0,	get_ctl_stats },
	{ REQ_GET_LEAPINFO,	NOAUTH,	0,	get_leap_info },
	{ REQ_SET_PRECISION, AUTH, sizeof(long),	set_precision },
#ifdef KERNEL_PLL
	{ REQ_GET_KERNEL,	NOAUTH,	0,	get_kernel_info },
#endif /* KERNEL_PLL */
#ifdef REFCLOCK
	{ REQ_GET_CLOCKINFO, NOAUTH, sizeof(U_LONG),	get_clock_info },
	{ REQ_SET_CLKFUDGE, AUTH, sizeof(struct conf_fudge), set_clock_fudge },
	{ REQ_GET_CLKBUGINFO, NOAUTH, sizeof(U_LONG),	get_clkbug_info },
#endif
	{ NO_REQUEST,		NOAUTH,	0,	0 }
};


/*
 * Authentication keyid used to authenticate requests.  Zero means we
 * don't allow writing anything.
 */
u_long info_auth_keyid;


/*
 * Statistic counters to keep track of requests and responses.
 */
u_long numrequests;		/* number of requests we've received */
u_long numresppkts;		/* number of resp packets sent with data */

u_long errorcounter[INFO_ERR_AUTH+1];	/* lazy way to count errors, indexed */
					/* by the error code */

#if defined(KERNEL_PLL) && !defined(NTP_SYSCALLS_LIBC)
extern int syscall	P((int, void *, ...));
#endif /* KERNEL_PLL */

/*
 * Imported from the I/O module
 */
extern struct interface *any_interface;

/*
 * Imported from the main routines
 */
extern int debug;

/*
 * Imported from the timer module
 */
extern u_long current_time;

/*
 * Imported from ntp_loopfilter.c
 */
extern int pll_control;
extern int pll_enable;
extern int pps_control;

/*
 * Imported from ntp_monitor.c
 */
extern int mon_enabled;

/*
 * Imported from ntp_util.c
 */
extern int stats_control;

extern struct peer *peer_hash[];
extern struct peer *sys_peer;

/*
 * A hack.  To keep the authentication module clear of xntp-ism's, we
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
static int seqno;
static int nitems;
static int itemsize;
static int databytes;
static char exbuf[RESP_DATA_SIZE];
static int usingexbuf;
static struct sockaddr_in *toaddr;
static struct interface *frominter;

/*
 * init_request - initialize request data
 */
void
init_request()
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
req_ack(srcadr, inter, inpkt, errcode)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
	int errcode;
{
	/*
	 * fill in the fields
	 */
	rpkt.rm_vn_mode = RM_VN_MODE(RESP_BIT, 0);
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
prepare_pkt(srcadr, inter, pkt, structsize)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *pkt;
	u_int structsize;
{
#ifdef DEBUG
	if (debug > 3)
		printf("request: preparing pkt\n");
#endif

	/*
	 * Fill in the implementation, reqest and itemsize fields
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
more_pkt()
{
	/*
	 * If we were using the extra buffer, send the packet.
	 */
	if (usingexbuf) {
#ifdef DEBUG
		if (debug > 2)
			printf("request: sending pkt\n");
#endif
		rpkt.rm_vn_mode = RM_VN_MODE(RESP_BIT, MORE_BIT);
		rpkt.auth_seq = AUTH_SEQ(0, seqno);
		rpkt.err_nitems = htons((u_short)nitems);
		sendpkt(toaddr, frominter, -1, (struct pkt *)&rpkt,
		    RESP_HEADER_SIZE+databytes);
		numresppkts++;

		/*
		 * Copy data out of exbuf into the packet.
		 */
		memmove(&rpkt.data[0], exbuf, itemsize);
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
flush_pkt()
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
		rpkt.rm_vn_mode = RM_VN_MODE(RESP_BIT, 0);
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
process_private(rbufp, mod_okay)
	struct recvbuf *rbufp;
	int mod_okay;
{
	struct req_pkt *inpkt;
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_proc *proc;

	/*
	 * Initialize pointers, for convenience
	 */
	inpkt = (struct req_pkt *)&rbufp->recv_pkt;
	srcadr = &rbufp->recv_srcadr;
	inter = rbufp->dstadr;

#ifdef DEBUG
	if (debug > 2)
		printf("prepare_pkt: impl %d req %d\n",
		    inpkt->implementation, inpkt->request);
#endif

	/*
	 * Do some sanity checks on the packet.  Return a format
	 * error if it fails.
	 */
	if (ISRESPONSE(inpkt->rm_vn_mode)
	    || ISMORE(inpkt->rm_vn_mode)
	    || INFO_VERSION(inpkt->rm_vn_mode) > NTP_VERSION
	    || INFO_VERSION(inpkt->rm_vn_mode) < NTP_OLDVERSION
	    || INFO_SEQ(inpkt->auth_seq) != 0
	    || INFO_ERR(inpkt->err_nitems) != 0
	    || INFO_MBZ(inpkt->mbz_itemsize) != 0
	    || rbufp->recv_length > REQ_LEN_MAC
	    || rbufp->recv_length < REQ_LEN_NOMAC) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}


	/*
	 * Get the appropriate procedure list to search.
	 */
	if (inpkt->implementation == IMPL_UNIV)
		proc = univ_codes;
	else if (inpkt->implementation == IMPL_XNTPD)
		proc = xntp_codes;
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
	 * If we need to authenticate, do so.  Note that an
	 * authenticatable packet must include a mac field, must
	 * have used key info_auth_keyid and must have included
	 * a time stamp in the appropriate field.  The time stamp
	 * must be within INFO_TS_MAXSKEW of the receive
	 * time stamp.
	 */
	if (proc->needs_auth) {
		l_fp ftmp;
		
		/*
		 * If this guy is restricted from doing this, don't let him
		 * If wrong key was used, or packet doesn't have mac, return.
		 */
		if (!INFO_IS_AUTH(inpkt->auth_seq) || info_auth_keyid == 0
		    || ntohl(inpkt->keyid) != info_auth_keyid) {
#ifdef DEBUG
			if (debug > 4)
				printf(
			"failed auth %d info_auth_keyid %lu pkt keyid %u\n",
				    INFO_IS_AUTH(inpkt->auth_seq),
				    info_auth_keyid, ntohl(inpkt->keyid));
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
			req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
			return;
		}
		if (!mod_okay || !authhavekey(info_auth_keyid)) {
#ifdef DEBUG
			if (debug > 4)
				printf("failed auth mod_okay %d\n", mod_okay);
#endif
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}

		/*
		 * calculate absolute time difference between xmit time stamp
		 * and receive time stamp.  If too large, too bad.
		 */
		NTOHL_FP(&inpkt->tstamp, &ftmp);
		L_SUB(&ftmp, &rbufp->recv_time);
		if (L_ISNEG(&ftmp))
			L_NEG(&ftmp);
		
		if (ftmp.l_ui >= INFO_TS_MAXSKEW_UI) {
			/*
			 * He's a loser.  Tell him.
			 */
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}

		/*
		 * So far so good.  See if decryption works out okay.
		 */
		if (!authdecrypt(info_auth_keyid, (U_LONG *)inpkt,
		    REQ_LEN_NOMAC)) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_AUTH);
			return;
		}
	}

	/*
	 * If we need data, check to see if we have some.  If we
	 * don't, check to see that there is none (picky, picky).
	 */
	if (INFO_ITEMSIZE(inpkt->mbz_itemsize) != proc->sizeofitem) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}
	if (proc->sizeofitem != 0)
		if (proc->sizeofitem*INFO_NITEMS(inpkt->err_nitems)
		    > sizeof(inpkt->data)) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
			return;
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
peer_list(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_peer_list *ip;
	register struct peer *pp;
	register int i;

	ip = (struct info_peer_list *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_peer_list));
	for (i = 0; i < HASH_SIZE && ip != 0; i++) {
		pp = peer_hash[i];
		while (pp != 0 && ip != 0) {
			ip->address = pp->srcadr.sin_addr.s_addr;
			ip->port = pp->srcadr.sin_port;
			ip->hmode = pp->hmode;
			ip->flags = 0;
			if (pp->flags & FLAG_CONFIG)
				ip->flags |= INFO_FLAG_CONFIG;
			if (pp == sys_peer)
				ip->flags |= INFO_FLAG_SYSPEER;
			if (pp->candidate != 0)
				ip->flags |= INFO_FLAG_SEL_CANDIDATE;
			if (pp->select != 0)
				ip->flags |= INFO_FLAG_SHORTLIST;
			ip = (struct info_peer_list *)more_pkt();
			pp = pp->next;
		}
	}
	flush_pkt();
}


/*
 * peer_list_sum - return extended peer list
 */
static void
peer_list_sum(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_peer_summary *ips;
	register struct peer *pp;
	register int i;

#ifdef DEBUG
	if (debug > 2)
		printf("wants peer list summary\n");
#endif

	ips = (struct info_peer_summary *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_peer_summary));
	for (i = 0; i < HASH_SIZE && ips != 0; i++) {
		pp = peer_hash[i];
		while (pp != 0 && ips != 0) {
#ifdef DEBUG
			if (debug > 3)
				printf("sum: got one\n");
#endif
			ips->dstadr = (pp->processed) ?
				pp->cast_flags == MDF_BCAST ?
					pp->dstadr->bcast.sin_addr.s_addr:
				pp->cast_flags ?
				pp->dstadr->sin.sin_addr.s_addr ?
					pp->dstadr->sin.sin_addr.s_addr:
					pp->dstadr->bcast.sin_addr.s_addr:
					1 : 5;
			ips->srcadr = pp->srcadr.sin_addr.s_addr;
			ips->srcport = pp->srcadr.sin_port;
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
			if (pp->candidate != 0)
				ips->flags |= INFO_FLAG_SEL_CANDIDATE;
			if (pp->select != 0)
				ips->flags |= INFO_FLAG_SHORTLIST;
			ips->hmode = pp->hmode;
			ips->delay = HTONS_FP(pp->delay);
			HTONL_FP(&pp->offset, &ips->offset);
			ips->dispersion = HTONS_FP(pp->dispersion);

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
peer_info (srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_peer_list *ipl;
	register struct peer *pp;
	register struct info_peer *ip;
	register int items;
	register int i, j;
	struct sockaddr_in addr;
	extern struct peer *sys_peer;

	memset((char *)&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	items = INFO_NITEMS(inpkt->err_nitems);
	ipl = (struct info_peer_list *) inpkt->data;
	ip = (struct info_peer *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_peer));
	while (items-- > 0 && ip != 0) {
		addr.sin_port = ipl->port;
		addr.sin_addr.s_addr = ipl->address;
		ipl++;
		if ((pp = findexistingpeer(&addr, (struct peer *)0)) == 0)
			continue;
		ip->dstadr = (pp->processed) ?
			pp->cast_flags == MDF_BCAST ?
				pp->dstadr->bcast.sin_addr.s_addr:
			pp->cast_flags ?
			pp->dstadr->sin.sin_addr.s_addr ?
				pp->dstadr->sin.sin_addr.s_addr:
				pp->dstadr->bcast.sin_addr.s_addr:
				2 : 6;
		ip->srcadr = NSRCADR(&pp->srcadr);
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
		if (pp->candidate != 0)
			ip->flags |= INFO_FLAG_SEL_CANDIDATE;
		if (pp->select != 0)
			ip->flags |= INFO_FLAG_SHORTLIST;
		ip->leap = pp->leap;
		ip->hmode = pp->hmode;
		ip->keyid = pp->keyid;
		ip->pkeyid = pp->pkeyid;
		ip->stratum = pp->stratum;
		ip->ppoll = pp->ppoll;
		ip->hpoll = pp->hpoll;
		ip->precision = pp->precision;
		ip->version = pp->version;
		ip->valid = pp->valid;
		ip->reach = pp->reach;
		ip->unreach = pp->unreach;
		ip->flash = pp->flash;
		ip->estbdelay = HTONS_FP(pp->estbdelay);
		ip->ttl = pp->ttl;
		ip->associd = htons(pp->associd);
		ip->rootdelay = HTONS_FP(pp->rootdelay);
		ip->rootdispersion = HTONS_FP(pp->rootdispersion);
		ip->refid = pp->refid;
		ip->timer = htonl(pp->event_timer.event_time - current_time);
		HTONL_FP(&pp->reftime, &ip->reftime);
		HTONL_FP(&pp->org, &ip->org);
		HTONL_FP(&pp->rec, &ip->rec);
		HTONL_FP(&pp->xmt, &ip->xmt);
		j = pp->filter_nextpt - 1;
		for (i = 0; i < NTP_SHIFT; i++, j--) {
			if (j < 0)
				j = NTP_SHIFT-1;
			ip->filtdelay[i] = HTONS_FP(pp->filter_delay[j]);
			HTONL_FP(&pp->filter_offset[j], &ip->filtoffset[i]);
			ip->order[i] = (pp->filter_nextpt+NTP_SHIFT-1)
			    - pp->filter_order[i];
			if (ip->order[i] >= NTP_SHIFT)
				ip->order[i] -= NTP_SHIFT;
		}
		HTONL_FP(&pp->offset, &ip->offset);
		ip->delay = HTONS_FP(pp->delay);
		ip->dispersion = HTONS_FP(pp->dispersion);
		ip->selectdisp = HTONS_FP(pp->selectdisp);
		ip = (struct info_peer *)more_pkt();
	}
	flush_pkt();
}


/*
 * peer_stats - send statistics for one or more peers
 */
static void
peer_stats (srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_peer_list *ipl;
	register struct peer *pp;
	register struct info_peer_stats *ip;
	register int items;
	struct sockaddr_in addr;
	extern struct peer *sys_peer;

	memset((char *)&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	items = INFO_NITEMS(inpkt->err_nitems);
	ipl = (struct info_peer_list *) inpkt->data;
	ip = (struct info_peer_stats *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_peer_stats));
	while (items-- > 0 && ip != 0) {
		addr.sin_port = ipl->port;
		addr.sin_addr.s_addr = ipl->address;
		ipl++;
		if ((pp = findexistingpeer(&addr, (struct peer *)0)) == 0)
			continue;
		ip->dstadr = (pp->processed) ?
			pp->cast_flags == MDF_BCAST ?
				pp->dstadr->bcast.sin_addr.s_addr:
			pp->cast_flags ?
			pp->dstadr->sin.sin_addr.s_addr ?
				pp->dstadr->sin.sin_addr.s_addr:
				pp->dstadr->bcast.sin_addr.s_addr:
				3 : 7;
		ip->srcadr = NSRCADR(&pp->srcadr);
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
		if (pp->candidate != 0)
			ip->flags |= INFO_FLAG_SEL_CANDIDATE;
		if (pp->select != 0)
			ip->flags |= INFO_FLAG_SHORTLIST;
		ip->timereceived = htonl(current_time - pp->timereceived);
		ip->timetosend
		    = htonl(pp->event_timer.event_time - current_time);
		ip->timereachable = htonl(current_time - pp->timereachable);
		ip->sent = htonl(pp->sent);
		ip->processed = htonl(pp->processed);
		ip->badauth = htonl(pp->badauth);
		ip->bogusorg = htonl(pp->bogusorg);
		ip->oldpkt = htonl(pp->oldpkt);
		ip->seldisp = htonl(pp->seldisptoolarge);
		ip->selbroken = htonl(pp->selbroken);
		ip->candidate = pp->candidate;
		ip = (struct info_peer_stats *)more_pkt();
	}
	flush_pkt();
}


/*
 * sys_info - return system info
 */
static void
sys_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_sys *is;

	/*
	 * Importations from the protocol module
	 */
	extern u_char sys_leap;
	extern u_char sys_stratum;
	extern s_char sys_precision;
	extern s_fp sys_rootdelay;
	extern u_fp sys_rootdispersion;
	extern u_long sys_refid;
	extern l_fp sys_reftime;
	extern u_char sys_poll;
	extern struct peer *sys_peer;
	extern int sys_bclient;
	extern s_fp sys_bdelay;
	extern int sys_authenticate;
	extern u_long sys_authdelay;
	extern u_fp clock_stability;
	extern s_fp clock_frequency;

	is = (struct info_sys *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_sys));

	if (sys_peer != 0) {
		is->peer = NSRCADR(&sys_peer->srcadr);
		is->peer_mode = sys_peer->hmode;
	} else {
		is->peer = 0;
		is->peer_mode = 0;
	}
	is->leap = sys_leap;
	is->stratum = sys_stratum;
	is->precision = sys_precision;
	is->rootdelay = htonl(sys_rootdelay);
	is->rootdispersion = htonl(sys_rootdispersion);
	is->frequency = htonl(clock_frequency);
	is->stability = htonl(clock_stability);
	is->refid = sys_refid;
	HTONL_FP(&sys_reftime, &is->reftime);

	is->poll = sys_poll;
	
	is->flags = 0;
	if (sys_bclient)
		is->flags |= INFO_FLAG_BCLIENT;
	if (sys_authenticate)
		is->flags |= INFO_FLAG_AUTHENTICATE;
	if (pll_enable)
		is->flags |= INFO_FLAG_PLL;
	if (pll_control)
		is->flags |= INFO_FLAG_PLL_SYNC;
	if (pps_control)
		is->flags |= INFO_FLAG_PPS_SYNC;
	if (mon_enabled != MON_OFF)
		is->flags |= INFO_FLAG_MONITOR;
	if (stats_control)
		is->flags |= INFO_FLAG_FILEGEN;
	is->bdelay = HTONS_FP(sys_bdelay);
	HTONL_UF(sys_authdelay, &is->authdelay);

	(void) more_pkt();
	flush_pkt();
}


/*
 * sys_stats - return system statistics
 */
static void
sys_stats(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_sys_stats *ss;

	/*
	 * Importations from the protocol module
	 */
	extern u_long sys_stattime;
	extern u_long sys_badstratum;
	extern u_long sys_oldversionpkt;
	extern u_long sys_newversionpkt;
	extern u_long sys_unknownversion;
	extern u_long sys_badlength;
	extern u_long sys_processed;
	extern u_long sys_badauth;
	extern u_long sys_limitrejected;

	ss = (struct info_sys_stats *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_sys_stats));

	ss->timeup = htonl(current_time);
	ss->timereset = htonl(current_time - sys_stattime);
	ss->badstratum = htonl(sys_badstratum);
	ss->oldversionpkt = htonl(sys_oldversionpkt);
	ss->newversionpkt = htonl(sys_newversionpkt);
	ss->unknownversion = htonl(sys_unknownversion);
	ss->badlength = htonl(sys_badlength);
	ss->processed = htonl(sys_processed);
	ss->badauth = htonl(sys_badauth);
	ss->limitrejected = htonl(sys_limitrejected);
	(void) more_pkt();
	flush_pkt();
}


/*
 * mem_stats - return memory statistics
 */
static void
mem_stats(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_mem_stats *ms;
	register int i;

	/*
	 * Importations from the peer module
	 */
	extern int peer_hash_count[HASH_SIZE];
	extern int peer_free_count;
	extern u_long peer_timereset;
	extern u_long findpeer_calls;
	extern u_long peer_allocations;
	extern u_long peer_demobilizations;
	extern int total_peer_structs;

	ms = (struct info_mem_stats *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_mem_stats));

	ms->timereset = htonl(current_time - peer_timereset);
	ms->totalpeermem = htons((u_short)total_peer_structs);
	ms->freepeermem = htons((u_short)peer_free_count);
	ms->findpeer_calls = htonl(findpeer_calls);
	ms->allocations = htonl(peer_allocations);
	ms->demobilizations = htonl(peer_demobilizations);

	for (i = 0; i < HASH_SIZE; i++) {
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
io_stats(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_io_stats *io;

	/*
	 * Importations from the io module
	 */
	extern u_long io_timereset;
	extern u_long full_recvbufs;
	extern u_long free_recvbufs;
	extern u_long total_recvbufs;
	extern u_long lowater_additions;
	extern u_long packets_dropped;
	extern u_long packets_ignored;
	extern u_long packets_received;
	extern u_long packets_sent;
	extern u_long packets_notsent;
	extern u_long handler_calls;
	extern u_long handler_pkts;

	io = (struct info_io_stats *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_io_stats));

	io->timereset = htonl(current_time - io_timereset);
	io->totalrecvbufs = htons((u_short) total_recvbufs);
	io->freerecvbufs = htons((u_short) free_recvbufs);
	io->fullrecvbufs = htons((u_short) full_recvbufs);
	io->lowwater = htons((u_short) lowater_additions);
	io->dropped = htonl(packets_dropped);
	io->ignored = htonl(packets_ignored);
	io->received = htonl(packets_received);
	io->sent = htonl(packets_sent);
	io->notsent = htonl(packets_notsent);
	io->interrupts = htonl(handler_calls);
	io->int_received = htonl(handler_pkts);

	(void) more_pkt();
	flush_pkt();
}


/*
 * timer_stats - return timer statistics
 */
static void
timer_stats(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_timer_stats *ts;

	/*
	 * Importations from the timer module
	 */
	extern u_long alarm_overflow;
	extern u_long timer_timereset;
	extern u_long timer_overflows;
	extern u_long timer_xmtcalls;

	ts = (struct info_timer_stats *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_timer_stats));

	ts->timereset = htonl(current_time - timer_timereset);
	ts->alarms = htonl(alarm_overflow);
	ts->overflows = htonl(timer_overflows);
	ts->xmtcalls = htonl(timer_xmtcalls);

	(void) more_pkt();
	flush_pkt();
}


/*
 * loop_info - return the current state of the loop filter
 */
static void
loop_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_loop *li;
	l_fp tmp;

	/*
	 * Importations from the loop filter module
	 */
	extern l_fp last_offset;
	extern s_fp drift_comp;
	extern int tc_counter;
	extern u_long last_time;

	li = (struct info_loop *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_loop));

	HTONL_FP(&last_offset, &li->last_offset);
	FPTOLFP(drift_comp, &tmp);
	HTONL_FP(&tmp, &li->drift_comp);
	li->compliance = htonl(tc_counter);
	li->watchdog_timer = htonl(current_time - last_time);

	(void) more_pkt();
	flush_pkt();
}


/*
 * do_conf - add a peer to the configuration list
 */
static void
do_conf(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct conf_peer *cp;
	register int items;
	struct sockaddr_in peeraddr;
	int fl;

	/*
	 * Do a check of everything to see that it looks
	 * okay.  If not, complain about it.  Note we are
	 * very picky here.
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_peer *)inpkt->data;

	fl = 0;
	while (items-- > 0 && !fl) {
		if (cp->version > NTP_VERSION
		    || cp->version < NTP_OLDVERSION)
			fl = 1;
		if (cp->hmode != MODE_ACTIVE
		    && cp->hmode != MODE_CLIENT
		    && cp->hmode != MODE_BROADCAST)
			fl = 1;
		if (cp->flags & ~(CONF_FLAG_AUTHENABLE | CONF_FLAG_PREFER))
			fl = 1;
		cp++;
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
	memset((char *)&peeraddr, 0, sizeof(struct sockaddr_in));
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_port = htons(NTP_PORT);

	/*
	 * Make sure the address is valid
	 */
#ifdef REFCLOCK
	if (!ISREFCLOCKADR(&peeraddr) && ISBADADR(&peeraddr)) {
#else
	if (ISBADADR(&peeraddr)) {
#endif
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	while (items-- > 0) {
		fl = 0;
		if (cp->flags & CONF_FLAG_AUTHENABLE)
			fl |= FLAG_AUTHENABLE;
		if (cp->flags & CONF_FLAG_PREFER)
			fl |= FLAG_PREFER;
		peeraddr.sin_addr.s_addr = cp->peeraddr;
	/* XXX W2DO? minpoll/maxpoll arguments ??? */
		if (peer_config(&peeraddr, (struct interface *)0,
		    cp->hmode, cp->version, cp->minpoll, cp->maxpoll,
			fl, cp->ttl, cp->keyid) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}
		cp++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * do_unconf - remove a peer from the configuration list
 */
static void
do_unconf(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct conf_unpeer *cp;
	register int items;
	register struct peer *peer;
	struct sockaddr_in peeraddr;
	int bad, found;

	/*
	 * This is a bit unstructured, but I like to be careful.
	 * We check to see that every peer exists and is actually
	 * configured.  If so, we remove them.  If not, we return
	 * an error.
	 */
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_port = htons(NTP_PORT);

	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_unpeer *)inpkt->data;

	bad = 0;
	while (items-- > 0 && !bad) {
		peeraddr.sin_addr.s_addr = cp->peeraddr;
		found = 0;
		peer = (struct peer *)0;
		while (!found) {
			peer = findexistingpeer(&peeraddr, peer);
			if (peer == (struct peer *)0)
				break;
			if (peer->flags & FLAG_CONFIG)
				found = 1;
		}
		if (!found)
			bad = 1;
		cp++;
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
		peeraddr.sin_addr.s_addr = cp->peeraddr;
		peer_unconfig(&peeraddr, (struct interface *)0);
		cp++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * set_sys_flag - set system flags
 */
static void
set_sys_flag(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	setclr_flags(srcadr, inter, inpkt, 1);
}


/*
 * clr_sys_flag - clear system flags
 */
static void
clr_sys_flag(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	setclr_flags(srcadr, inter, inpkt, 0);
}


/*
 * setclr_flags - do the grunge work of flag setting/clearing
 */
static void
setclr_flags(srcadr, inter, inpkt, set)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
	u_long set;
{
	register u_long flags;

	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	flags = ((struct conf_sys_flags *)inpkt->data)->flags;

	if (flags & ~(SYS_FLAG_BCLIENT | SYS_FLAG_AUTHENTICATE |
	    SYS_FLAG_PLL | SYS_FLAG_MONITOR |
	    SYS_FLAG_FILEGEN)) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	if (flags & SYS_FLAG_BCLIENT)
		proto_config(PROTO_BROADCLIENT, set);
	if (flags & SYS_FLAG_AUTHENTICATE)
		proto_config(PROTO_AUTHENTICATE, set);
	if (flags & SYS_FLAG_PLL)
		proto_config(PROTO_PLL, set);
	if (flags & SYS_FLAG_MONITOR)
		proto_config(PROTO_MONITOR, set);
	if (flags & SYS_FLAG_FILEGEN)
		proto_config(PROTO_FILEGEN, set);
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * do_monitor - turn on monitoring
 */
static void
do_monitor(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	mon_start(MON_ON);
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * do_nomonitor - turn off monitoring
 */
static void
do_nomonitor(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	mon_stop(MON_ON);
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * list_restrict - return the restrict list
 */
static void
list_restrict(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_restrict *ir;
	register struct restrictlist *rl;
	extern struct restrictlist *restrictlist;

#ifdef DEBUG
	if (debug > 2)
		printf("wants peer list summary\n");
#endif

	ir = (struct info_restrict *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_restrict));
	for (rl = restrictlist; rl != 0 && ir != 0; rl = rl->next) {
		ir->addr = htonl(rl->addr);
		ir->mask = htonl(rl->mask);
		ir->count = htonl(rl->count);
		ir->flags = htons(rl->flags);
		ir->mflags = htons(rl->mflags);
		ir = (struct info_restrict *)more_pkt();
	}
	flush_pkt();
}



/*
 * do_resaddflags - add flags to a restrict entry (or create one)
 */
static void
do_resaddflags(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_restrict(srcadr, inter, inpkt, RESTRICT_FLAGS);
}



/*
 * do_ressubflags - remove flags from a restrict entry
 */
static void
do_ressubflags(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_restrict(srcadr, inter, inpkt, RESTRICT_UNFLAG);
}


/*
 * do_unrestrict - remove a restrict entry from the list
 */
static void
do_unrestrict(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_restrict(srcadr, inter, inpkt, RESTRICT_REMOVE);
}





/*
 * do_restrict - do the dirty stuff of dealing with restrictions
 */
static void
do_restrict(srcadr, inter, inpkt, op)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
	int op;
{
	register struct conf_restrict *cr;
	register int items;
	struct sockaddr_in matchaddr;
	struct sockaddr_in matchmask;
	int bad;

	/*
	 * Do a check of the flags to make sure that only
	 * the NTPPORT flag is set, if any.  If not, complain
	 * about it.  Note we are very picky here.
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cr = (struct conf_restrict *)inpkt->data;

	bad = 0;
	while (items-- > 0 && !bad) {
		if (cr->mflags & ~(RESM_NTPONLY))
			bad = 1;
		if (cr->flags & ~(RES_ALLFLAGS))
			bad = 1;
		if (cr->addr == htonl(INADDR_ANY) && cr->mask != htonl(INADDR_ANY))
			bad = 1;
		cr++;
	}

	if (bad) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	/*
	 * Looks okay, try it out
	 */
	items = INFO_NITEMS(inpkt->err_nitems);
	cr = (struct conf_restrict *)inpkt->data;
	memset((char *)&matchaddr, 0, sizeof(struct sockaddr_in));
	memset((char *)&matchmask, 0, sizeof(struct sockaddr_in));
	matchaddr.sin_family = AF_INET;
	matchmask.sin_family = AF_INET;

	while (items-- > 0) {
		matchaddr.sin_addr.s_addr = cr->addr;
		matchmask.sin_addr.s_addr = cr->mask;
		restrict(op, &matchaddr, &matchmask, cr->mflags,
		    cr->flags);
		cr++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * mon_getlist - return monitor data
 */
static void
mon_getlist_0(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
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
	    sizeof(struct info_monitor));
	for (md = mon_mru_list.mru_next; md != &mon_mru_list && im != 0;
	    md = md->mru_next) {
		im->lasttime = htonl(current_time - md->lasttime);
		im->firsttime = htonl(current_time - md->firsttime);
		if (md->lastdrop)
			im->lastdrop = htonl(current_time - md->lastdrop);
		else
			im->lastdrop = 0;
		im->count = htonl(md->count);
		im->addr = md->rmtadr;
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
mon_getlist_1(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_monitor_1 *im;
	register struct mon_data *md;
	extern struct mon_data mon_mru_list;
	extern int mon_enabled;

#ifdef DEBUG
	if (debug > 2)
		printf("wants monitor 1 list\n");
#endif
	if (!mon_enabled) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
		return;
	}

	im = (struct info_monitor_1 *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_monitor_1));
	for (md = mon_mru_list.mru_next; md != &mon_mru_list && im != 0;
	    md = md->mru_next) {
		im->lasttime = htonl(current_time - md->lasttime);
		im->firsttime = htonl(current_time - md->firsttime);
		if (md->lastdrop)
			im->lastdrop = htonl(current_time - md->lastdrop);
		else
			im->lastdrop = 0;
		im->count = htonl(md->count);
		im->addr = md->rmtadr;
		im->daddr =	md->cast_flags == MDF_BCAST ?
					md->interface->bcast.sin_addr.s_addr :
				md->cast_flags ?
				md->interface->sin.sin_addr.s_addr ?
					md->interface->sin.sin_addr.s_addr :
					md->interface->bcast.sin_addr.s_addr :
					4;
		im->flags = md->cast_flags;
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
	void (*handler)();	/* routine to handle request */
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
reset_stats(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	u_long flags;
	struct reset_entry *rent;

	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	flags = ((struct reset_flags *)inpkt->data)->flags;

	if (flags & ~RESET_ALLFLAGS) {
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
reset_peer(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct conf_unpeer *cp;
	register int items;
	register struct peer *peer;
	struct sockaddr_in peeraddr;
	int bad;

	/*
	 * We check first to see that every peer exists.  If not,
	 * we return an error.
	 */
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_port = htons(NTP_PORT);

	items = INFO_NITEMS(inpkt->err_nitems);
	cp = (struct conf_unpeer *)inpkt->data;

	bad = 0;
	while (items-- > 0 && !bad) {
		peeraddr.sin_addr.s_addr = cp->peeraddr;
		peer = findexistingpeer(&peeraddr, (struct peer *)0);
		if (peer == (struct peer *)0)
			bad++;
		cp++;
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
		peeraddr.sin_addr.s_addr = cp->peeraddr;
		peer = findexistingpeer(&peeraddr, (struct peer *)0);
		peer_reset(peer);
		cp++;
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * do_key_reread - reread the encryption key file
 */
static void
do_key_reread(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	rereadkeys();
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * do_dirty_hack
 */
static void
do_dirty_hack(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	/* historical placeholder */
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * dont_dirty_hack
 */
static void
dont_dirty_hack(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	/* historical placeholder */
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}


/*
 * trust_key - make one or more keys trusted
 */
static void
trust_key(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_trustkey(srcadr, inter, inpkt, 1);
}


/*
 * untrust_key - make one or more keys untrusted
 */
static void
untrust_key(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_trustkey(srcadr, inter, inpkt, 0);
}


/*
 * do_trustkey - make keys either trustable or untrustable
 */
static void
do_trustkey(srcadr, inter, inpkt, trust)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
	int trust;
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
get_auth_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_auth *ia;

	/*
	 * Importations from the authentication module
	 */
	extern u_long authnumkeys;
	extern u_long authnumfreekeys;
	extern u_long authkeylookups;
	extern u_long authkeynotfound;
	extern u_long authencryptions;
	extern u_long authdecryptions;
	extern u_long authkeyuncached;

	ia = (struct info_auth *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_auth));

	ia->numkeys = htonl(authnumkeys);
	ia->numfreekeys = htonl(authnumfreekeys);
	ia->keylookups = htonl(authkeylookups);
	ia->keynotfound = htonl(authkeynotfound);
	ia->encryptions = htonl(authencryptions);
	ia->decryptions = htonl(authdecryptions);
	ia->keyuncached = htonl(authkeyuncached);
	ia->timereset = htonl(current_time - auth_timereset);
	
	(void) more_pkt();
	flush_pkt();
}



/*
 * reset_auth_stats - reset the authentication stat counters.  Done here
 *		      to keep xntp-isms out of the authentication module
 */
static void
reset_auth_stats()
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
req_get_traps(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
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
	    sizeof(struct info_trap));

	for (i = 0, tr = ctl_trap; i < CTL_MAXTRAPS; i++, tr++) {
		if (tr->tr_flags & TRAP_INUSE) {
			if (tr->tr_localaddr == any_interface)
				it->local_address = 0;
			else
				it->local_address
				    = NSRCADR(&tr->tr_localaddr->sin);
			it->trap_address = NSRCADR(&tr->tr_addr);
			it->trap_port = NSRCPORT(&tr->tr_addr);
			it->sequence = htons(tr->tr_sequence);
			it->settime = htonl(current_time - tr->tr_settime);
			it->origtime = htonl(current_time - tr->tr_origtime);
			it->resets = htonl(tr->tr_resets);
			it->flags = htonl((u_long)tr->tr_flags);
			it = (struct info_trap *)more_pkt();
		}
	}
	flush_pkt();
}


/*
 * req_set_trap - configure a trap
 */
static void
req_set_trap(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_setclr_trap(srcadr, inter, inpkt, 1);
}



/*
 * req_clr_trap - unconfigure a trap
 */
static void
req_clr_trap(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	do_setclr_trap(srcadr, inter, inpkt, 0);
}



/*
 * do_setclr_trap - do the grunge work of (un)configuring a trap
 */
static void
do_setclr_trap(srcadr, inter, inpkt, set)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
	int set;
{
	register struct conf_trap *ct;
	register struct interface *linter;
	int res;
	struct sockaddr_in laddr;

	/*
	 * Prepare sockaddr_in structure
	 */
	memset((char *)&laddr, 0, sizeof laddr);
	laddr.sin_family = AF_INET;
	laddr.sin_port = ntohs(NTP_PORT);

	/*
	 * Restrict ourselves to one item only.  This eliminates
	 * the error reporting problem.
	 */
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
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
		laddr.sin_addr.s_addr = ct->local_address;
		linter = findinterface(&laddr);
		if (linter == NULL) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}
	}

	laddr.sin_addr.s_addr = ct->trap_address;
	if (ct->trap_port != 0)
		laddr.sin_port = ct->trap_port;
	else
		laddr.sin_port = htons(TRAPPORT);

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
set_request_keyid(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	u_long keyid;

	/*
	 * Restrict ourselves to one item only.
	 */
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	keyid = ntohl(*((u_long *)(inpkt->data)));
	info_auth_keyid = keyid;
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}



/*
 * set_control_keyid - set the keyid used to authenticate requests
 */
static void
set_control_keyid(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	u_long keyid;
	extern u_long ctl_auth_keyid;

	/*
	 * Restrict ourselves to one item only.
	 */
	if (INFO_NITEMS(inpkt->err_nitems) > 1) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	keyid = ntohl(*((u_long *)(inpkt->data)));
	ctl_auth_keyid = keyid;
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}



/*
 * get_ctl_stats - return some stats concerning the control message module
 */
static void
get_ctl_stats(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
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

	ic->ctltimereset = htonl(current_time - ctltimereset);
	ic->numctlreq = htonl(numctlreq);
	ic->numctlbadpkts = htonl(numctlbadpkts);
	ic->numctlresponses = htonl(numctlresponses);
	ic->numctlfrags = htonl(numctlfrags);
	ic->numctlerrors = htonl(numctlerrors);
	ic->numctltooshort = htonl(numctltooshort);
	ic->numctlinputresp = htonl(numctlinputresp);
	ic->numctlinputfrag = htonl(numctlinputfrag);
	ic->numctlinputerr = htonl(numctlinputerr);
	ic->numctlbadoffset = htonl(numctlbadoffset);
	ic->numctlbadversion = htonl(numctlbadversion);
	ic->numctldatatooshort = htonl(numctldatatooshort);
	ic->numctlbadop = htonl(numctlbadop);
	ic->numasyncmsgs = htonl(numasyncmsgs);

	(void) more_pkt();
	flush_pkt();
}



/*
 * get_leap_info - return some stats concerning the control message module
 */
static void
get_leap_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_leap *il;

	/*
	 * Imported from the protocol module
	 */
	extern u_char sys_leap;

	/*
	 * Importations from the leap module
	 */
	extern u_char leap_indicator;
	extern u_char leap_warning;
	extern u_char leapbits;
	extern u_long leap_timer;
	extern u_long leap_processcalls;
	extern u_long leap_notclose;
	extern u_long leap_monthofleap;
	extern u_long leap_dayofleap;
	extern u_long leap_hoursfromleap;
	extern u_long leap_happened;

	il = (struct info_leap *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_leap));

	il->sys_leap = sys_leap;
	il->leap_indicator = leap_indicator;
	il->leap_warning = leap_warning;
	il->leap_bits = (leapbits & INFO_LEAP_MASK)
	    | ((leap_indicator != LEAP_NOWARNING) ? INFO_LEAP_OVERRIDE : 0);
	il->leap_timer = htonl(leap_timer - current_time);
	il->leap_processcalls = htonl(leap_processcalls);
	il->leap_notclose = htonl(leap_notclose);
	il->leap_monthofleap = htonl(leap_monthofleap);
	il->leap_dayofleap = htonl(leap_dayofleap);
	il->leap_hoursfromleap = htonl(leap_hoursfromleap);
	il->leap_happened = htonl(leap_happened);

	(void) more_pkt();
	flush_pkt();
}


#ifdef KERNEL_PLL
/*
 * get_kernel_info - get kernel pll/pps information
 */
static void
get_kernel_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_kernel *ik;
	struct timex ntx;

	if (!pll_control)
		return;
	memset((char *)&ntx, 0, sizeof(ntx));
	(void)ntp_adjtime(&ntx);

	ik = (struct info_kernel *)prepare_pkt(srcadr, inter, inpkt,
            sizeof(struct info_kernel));

	/*
	 * pll variables
	 */
	ik->offset = htonl(ntx.offset);
	ik->freq = htonl(ntx.freq);
	ik->maxerror = htonl(ntx.maxerror);
	ik->esterror = htonl(ntx.esterror);
	ik->status = htons(ntx.status);
	ik->constant = htonl(ntx.constant);
	ik->precision = htonl(ntx.precision);
	ik->tolerance = htonl(ntx.tolerance);

	/*
	 * pps variables
	 */
	ik->ppsfreq = htonl(ntx.ppsfreq);
	ik->jitter = htonl(ntx.jitter);
	ik->shift = htons(ntx.shift);
	ik->stabil = htonl(ntx.stabil);
	ik->jitcnt = htonl(ntx.jitcnt);
	ik->calcnt = htonl(ntx.calcnt);
	ik->errcnt = htonl(ntx.errcnt);
	ik->stbcnt = htonl(ntx.stbcnt);
	
	(void) more_pkt();
	flush_pkt();
}
#endif /* KERNEL_PLL */


#ifdef REFCLOCK
/*
 * get_clock_info - get info about a clock
 */
static void
get_clock_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct info_clock *ic;
	register U_LONG *clkaddr;
	register int items;
	struct refclockstat clock;
	struct sockaddr_in addr;

	memset((char *)&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NTP_PORT);
	items = INFO_NITEMS(inpkt->err_nitems);
	clkaddr = (U_LONG *) inpkt->data;

	ic = (struct info_clock *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_clock));

	while (items-- > 0) {
		addr.sin_addr.s_addr = *clkaddr++;
		if (!ISREFCLOCKADR(&addr) ||
		    findexistingpeer(&addr, (struct peer *)0) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		clock.kv_list = (struct ctl_var *)0;

		refclock_control(&addr, (struct refclockstat *)0, &clock);

		ic->clockadr = addr.sin_addr.s_addr;
		ic->type = clock.type;
		ic->flags = clock.flags;
		ic->lastevent = clock.lastevent;
		ic->currentstatus = clock.currentstatus;
		ic->polls = htonl(clock.polls);
		ic->noresponse = htonl(clock.noresponse);
		ic->badformat = htonl(clock.badformat);
		ic->baddata = htonl(clock.baddata);
		ic->timestarted = htonl(clock.timereset);
		HTONL_FP(&clock.fudgetime1, &ic->fudgetime1);
		HTONL_FP(&clock.fudgetime2, &ic->fudgetime2);
		ic->fudgeval1 = htonl(clock.fudgeval1);
		ic->fudgeval2 = htonl(clock.fudgeval2);

		free_varlist(clock.kv_list);

		ic = (struct info_clock *)more_pkt();
	}
	flush_pkt();
}



/*
 * set_clock_fudge - get a clock's fudge factors
 */
static void
set_clock_fudge(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register struct conf_fudge *cf;
	register int items;
	struct refclockstat clock;
	struct sockaddr_in addr;

	memset((char *)&addr, 0, sizeof addr);
	memset((char *)&clock, 0, sizeof clock);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NTP_PORT);
	items = INFO_NITEMS(inpkt->err_nitems);
	cf = (struct conf_fudge *) inpkt->data;

	while (items-- > 0) {
		addr.sin_addr.s_addr = cf->clockadr;
		if (!ISREFCLOCKADR(&addr) ||
		    findexistingpeer(&addr, (struct peer *)0) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		switch(ntohl(cf->which)) {
		case FUDGE_TIME1:
			NTOHL_FP(&cf->fudgetime, &clock.fudgetime1);
			clock.haveflags = CLK_HAVETIME1;
			break;
		case FUDGE_TIME2:
			NTOHL_FP(&cf->fudgetime, &clock.fudgetime2);
			clock.haveflags = CLK_HAVETIME2;
			break;
		case FUDGE_VAL1:
			clock.fudgeval1 = ntohl(cf->fudgeval_flags);
			clock.haveflags = CLK_HAVEVAL1;
			break;
		case FUDGE_VAL2:
			clock.fudgeval2 = ntohl(cf->fudgeval_flags);
			clock.haveflags = CLK_HAVEVAL2;
			break;
		case FUDGE_FLAGS:
			clock.flags = ntohl(cf->fudgeval_flags) & 0xf;
			clock.haveflags =
		(CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3|CLK_HAVEFLAG4);
			break;
		default:
			req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
			return;
		}

		refclock_control(&addr, &clock, (struct refclockstat *)0);
	}

	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}
#endif

/*
 * set_precision - set the system precision
 */
static void
set_precision(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register long precision;

	precision = ntohl(*(long *)(inpkt->data));

	if (INFO_NITEMS(inpkt->err_nitems) > 1 ||
	    precision > -1 || precision < -20) {
		req_ack(srcadr, inter, inpkt, INFO_ERR_FMT);
		return;
	}

	proto_config(PROTO_PRECISION, precision);
	req_ack(srcadr, inter, inpkt, INFO_OKAY);
}

#ifdef REFCLOCK
/*
 * get_clkbug_info - get debugging info about a clock
 */
static void
get_clkbug_info(srcadr, inter, inpkt)
	struct sockaddr_in *srcadr;
	struct interface *inter;
	struct req_pkt *inpkt;
{
	register int i;
	register struct info_clkbug *ic;
	register U_LONG *clkaddr;
	register int items;
	struct refclockbug bug;
	struct sockaddr_in addr;

	memset((char *)&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NTP_PORT);
	items = INFO_NITEMS(inpkt->err_nitems);
	clkaddr = (U_LONG *) inpkt->data;

	ic = (struct info_clkbug *)prepare_pkt(srcadr, inter, inpkt,
	    sizeof(struct info_clkbug));

	while (items-- > 0) {
		addr.sin_addr.s_addr = *clkaddr++;
		if (!ISREFCLOCKADR(&addr) ||
		    findexistingpeer(&addr, (struct peer *)0) == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		memset((char *)&bug, 0, sizeof bug);
		refclock_buginfo(&addr, &bug);
		if (bug.nvalues == 0 && bug.ntimes == 0) {
			req_ack(srcadr, inter, inpkt, INFO_ERR_NODATA);
			return;
		}

		ic->clockadr = addr.sin_addr.s_addr;
		i = bug.nvalues;
		if (i > NUMCBUGVALUES)
			i = NUMCBUGVALUES;
		ic->nvalues = (u_char)i;
		ic->svalues = htons((u_short)bug.svalues & ((1<<i)-1));
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
