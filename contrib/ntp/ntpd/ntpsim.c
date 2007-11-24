/*
 * NTP simulator engine - Harish Nair
 * University of Delaware, 2001
 */
#include "ntpd.h"
#include "ntpsim.h"

/*
 * Defines...
 */
#define SIM_TIME 86400		/* end simulation time */
#define NET_DLY .001            /* network delay */
#define PROC_DLY .001		/* processing delay */
#define BEEP_DLY 3600           /* beep interval (s) */
#define	SLEW	500e-6		/* correction rate (PPM) */

/*
 * Function pointers
 */
void (*funcPtr[]) (Node *, Event) = {
	&ndbeep, &ndeclk, &ntptmr, &netpkt
};


/*
 * ntpsim - initialize global variables and event queue and start
 */
int
ntpsim(
	int	argc,
	char	*argv[]
	)
{
	Event	e;
	double	maxtime;
	struct timeval seed;

	/*
	 * Initialize the global node
	 */
	ntp_node.time = 0;		/* simulation time */
	ntp_node.sim_time = SIM_TIME;	/* end simulation time (-S) */
	ntp_node.ntp_time = 0;		/* client disciplined time */
	ntp_node.adj = 0;		/* remaining time correction */
	ntp_node.slew = SLEW;		/* correction rate (-H) */

	ntp_node.clk_time = 0;		/* server time (-O) */
	ntp_node.ferr = 0;		/* frequency error (-T) */
	ntp_node.fnse = 0;		/* random walk noise (-W) */
	ntp_node.ndly = NET_DLY;	/* network delay (-Y) */
	ntp_node.snse = 0;		/* phase noise (-C) */
	ntp_node.pdly = PROC_DLY;	/* processing delay (-Z) */
	ntp_node.bdly = BEEP_DLY;	/* beep interval (-B) */

	ntp_node.events = NULL;
	ntp_node.rbuflist = NULL;

	/*
	 * Initialize ntp variables
	 */
	initializing = 1;
        init_auth();
        init_util();
        init_restrict();
        init_mon();
        init_timer();
        init_lib();
        init_random();
        init_request();
        init_control();
        init_peer();
        init_proto();
        init_io();
        init_loopfilter();
        mon_start(MON_OFF);
	getconfig(argc, argv);
        initializing = 0;

	/*
	 * Watch out here, we want the real time, not the silly stuff.
	 */
	gettimeofday(&seed, NULL);
	srand48(seed.tv_usec);

	/*
	 * Push a beep and timer interrupt on the queue
	 */
	push(event(0, BEEP), &ntp_node.events);
	push(event(ntp_node.time + 1.0, TIMER), &ntp_node.events);

	/*
	 * Pop the queue until nothing is left or time is exceeded
	 */
	maxtime = ntp_node.time + ntp_node.sim_time;
	while (ntp_node.time <= maxtime && ntp_node.events != NULL ) {
		e = pop(&ntp_node.events);
		ndeclk(&ntp_node, e);
		funcPtr[e.function](&ntp_node, e);
	}
	return (0);
}


/*
 * Return an event
 */
Event
event(
	double t,
	funcTkn f
	)
{
	Event e;

	e.time = t;
	e.function = f;
	return (e);
}

/*
 * Create an event queue
 */
Queue
queue(
	Event e,
	Queue q
	)
{
	Queue ret;

	if ((ret = (Queue)malloc(sizeof(struct List))) == NULL)
                abortsim("queue-malloc");
	ret->event = e;
	ret->next = q;
	return (ret);
}


/*
 * Push an event into the event queue
 */
void push(
	Event e,
	Queue *qp
	)
{
	Queue *tmp = qp;

	while (*tmp != NULL && ((*tmp)->event.time < e.time))
		tmp = &((*tmp)->next);
	*tmp = queue(e, (*tmp));
}


/*
 * Pop the first event from the event queue
 */
Event
pop(
	Queue *qp
	)
{
	Event ret;
	Queue tmp;

	tmp = *qp;
	if (tmp == NULL)
	    abortsim("pop - empty queue");
	ret = tmp->event;
	*qp = tmp->next;
	free(tmp);
	return (ret);
}


/*
 * Update clocks
 */
void
ndeclk(
	Node *n,
	Event e
	)
{
	node_clock(n, e.time);
}


/*
 * Timer interrupt. Eventually, this results in calling the
 * srvr_rplyi() routine below.
 */
void
ntptmr(
	Node *n,
	Event e
	)
{
	struct recvbuf *rbuf;

	timer();

	/*
	 * Process buffers received. They had better be in order by
	 * receive timestamp.
	 */
	while (n->rbuflist != NULL) {
		rbuf = n->rbuflist;
		n->rbuflist = rbuf->next;
		(rbuf->receiver)(rbuf);
		free(rbuf);
	}

	/*
	 * Arm the next timer interrupt.
	 */
	push(event(e.time + (1 << EVENT_TIMEOUT), TIMER), &n->events);
}


/*
 * srvr_rply() - send packet
 */
int srvr_rply(
	Node *n,
	struct sockaddr_storage *dest,
	struct interface *inter, struct pkt *rpkt
	)
{
	struct pkt xpkt;
	struct recvbuf rbuf;
	Event   xvnt;
	double	dtemp, etemp;

	/*
	 * Insert packet header values. We make this look like a
	 * stratum-1 server with a GPS clock, but nobody will ever
	 * notice that.
	 */
	xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING, NTP_VERSION,
	    MODE_SERVER);
	xpkt.stratum = STRATUM_TO_PKT(((u_char)1));
	memcpy(&xpkt.refid, "GPS", 4);
	xpkt.ppoll = rpkt->ppoll;
        xpkt.precision = rpkt->precision;
        xpkt.rootdelay = 0;
        xpkt.rootdispersion = 0;

	/*
	 * Insert the timestamps.
	 */
        xpkt.org = rpkt->xmt;
	dtemp = poisson(n->ndly, n->snse); /* client->server delay */
	DTOLFP(dtemp + n->clk_time, &xpkt.rec);
	dtemp += poisson(n->pdly, 0);	/* server delay */
	DTOLFP(dtemp + n->clk_time, &xpkt.xmt);
	xpkt.reftime = xpkt.xmt;
	dtemp += poisson(n->ndly, n->snse); /* server->client delay */

	/*
	 * Insert the I/O stuff.
	 */
	rbuf.receiver = receive;
        get_systime(&rbuf.recv_time);
        rbuf.recv_length = LEN_PKT_NOMAC;
        rbuf.recv_pkt = xpkt;
        memcpy(&rbuf.srcadr, dest, sizeof(struct sockaddr_storage));
        memcpy(&rbuf.recv_srcadr, dest,
	    sizeof(struct sockaddr_storage));
        if ((rbuf.dstadr = malloc(sizeof(struct interface))) == NULL)
		abortsim("server-malloc");
        memcpy(rbuf.dstadr, inter, sizeof(struct interface));
        rbuf.next = NULL;

	/*
	 * Very carefully predict the time of arrival for the received
	 * packet. 
	 */ 
	LFPTOD(&xpkt.org, etemp);
	etemp += dtemp;
	xvnt = event(etemp, PACKET);
	xvnt.rcv_buf = rbuf;
	push(xvnt, &n->events);
	return (0);
}


/*
 * netpkt() - receive packet
 */
void
netpkt(
	Node *n,
	Event e
	)
{
	struct recvbuf *rbuf;
	struct recvbuf *obuf;

	/*
	 * Insert the packet on the receive queue and record the arrival
	 * time.
	 */
	if ((rbuf = malloc(sizeof(struct recvbuf))) == NULL)
		abortsim("ntprcv-malloc");
	memcpy(rbuf, &e.rcv_buf, sizeof(struct recvbuf));
	rbuf->receiver = receive;
	DTOLFP(n->ntp_time, &rbuf->recv_time);
	rbuf->next = NULL;
	obuf = n->rbuflist;

	/*
	 * In the present incarnation, no more than one buffer can be on
	 * the queue; however, we sniff the queue anyway as a hint for
	 * further development.
	 */
	if (obuf == NULL) {
		n->rbuflist = rbuf;
	} else {
		while (obuf->next != NULL)
			obuf = obuf->next;
		obuf->next = rbuf;
	}
}


/*
 * ndbeep() - progress indicator
 */
void
ndbeep(
	Node *n,
	Event e
	)
{
	static int first_time = 1;
	char *dash = "-----------------";

	if(n->bdly > 0) {
		if (first_time) {
			printf(
			    "\t%4c    T    %4c\t%4c  T+ERR  %3c\t%5cT+ERR+NTP\n", ' ', ' ', ' ', ' ',' ');
			printf("\t%s\t%s\t%s\n", dash, dash, dash);
			first_time = 0;
			push(event(n->bdly, BEEP), &n->events);  
        		push(event(n->sim_time, BEEP), &n->events);
			printf("\t%16.6f\t%16.6f\t%16.6f\n",
                            n->time, n->clk_time, n->ntp_time);
			return;
		}
		printf("\t%16.6f\t%16.6f\t%16.6f\n",
		    n->time, n->clk_time, n->ntp_time);
		push(event(e.time + n->bdly, BEEP), &n->events);
	}
}


/*
 * Abort simulation
 */
void
abortsim(
	char *errmsg
	)
{
        perror(errmsg);
        exit(1);
}
