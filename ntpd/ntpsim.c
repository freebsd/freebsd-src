/* ntpdsim.c
 *
 * The source code for the ntp discrete event simulator. 
 *
 * Written By:	Sachin Kamboj
 *		University of Delaware
 *		Newark, DE 19711
 * Copyright (c) 2006
 * (Some code shamelessly based on the original NTP discrete event simulator)
 */

#ifdef SIM
#include "ntpd.h"
#include "ntpsim.h"
#include "ntp_data_structures.h"


/* Global Variable Definitions */

sim_info simulation;		/* Simulation Control Variables */
local_clock_info simclock;	/* Local Clock Variables */
queue *event_queue;		/* Event Queue */
queue *recv_queue;		/* Receive Queue */
static double sys_residual = 0;	/* adjustment residue (s) */

void (*event_ptr[]) (Event *) = {
    sim_event_beep, sim_update_clocks, sim_event_timer, sim_event_recv_packet
};			/* Function pointer to the events */


/* Define a function to compare two events to determine which one occurs first
 */

int determine_event_ordering(Event *e1, Event *e2);

int determine_event_ordering(Event *e1, Event *e2)
{
    return (e1->time - e2->time);
}

/* Define a function to compare two received packets to determine which one
 * is received first
 */
int determine_recv_buf_ordering(struct recvbuf *b1, struct recvbuf *b2);

int determine_recv_buf_ordering(struct recvbuf *b1, struct recvbuf *b2)
{
    double recv_time1, recv_time2;

    /* Simply convert the time received to double and subtract */
    LFPTOD(&b1->recv_time, recv_time1);
    LFPTOD(&b2->recv_time, recv_time2);
    return ((int)(recv_time1 - recv_time2));
}

/* Define a function to create the server associations */
void create_server_associations()
{
    int i;
    for (i = 0;i < simulation.num_of_servers;++i) {
	printf("%s\n", stoa(simulation.servers[i].addr));
	if (peer_config(simulation.servers[i].addr,
			ANY_INTERFACE_CHOOSE(simulation.servers[i].addr),
			MODE_CLIENT,
			NTP_VERSION,
			NTP_MINDPOLL,
			NTP_MAXDPOLL,
			0, /* peerflags */
			0, /* ttl */
			0, /* peerkey */
			(u_char *)"*" /* peerkeystr */) == 0) {
	    fprintf(stderr, "ERROR!! Could not create association for: %s",
		    stoa(simulation.servers[i].addr));
	}
    }
}


/* Main Simulator Code */

int ntpsim(int argc, char *argv[])
{
    Event *curr_event;
    struct timeval seed;

    /* Initialize the local Clock 
     */
    simclock.local_time = 0;
    simclock.adj = 0;
    simclock.slew = 0;

    /* Initialize the simulation 
     */
    simulation.num_of_servers = 0;
    simulation.beep_delay = BEEP_DLY;
    simulation.sim_time = 0;
    simulation.end_time = SIM_TIME;

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
    init_request();
    init_control();
    init_peer();
    init_proto();
    init_io();
    init_loopfilter();
    mon_start(MON_OFF);    

    /* Call getconfig to parse the configuration file */
    getconfig(argc, argv);
    initializing = 0;
    loop_config(LOOP_DRIFTCOMP, old_drift / 1e6);

    /*
     * Watch out here, we want the real time, not the silly stuff.
     */
    gettimeofday(&seed, NULL);
    ntp_srandom(seed.tv_usec);


    /* Initialize the event queue */
    event_queue = create_priority_queue((int(*)(void *, void*)) 
					determine_event_ordering);

    /* Initialize the receive queue */
    recv_queue = create_priority_queue((int(*)(void *, void*))
				       determine_recv_buf_ordering);

    /* Push a beep and a timer on the event queue */
    enqueue(event_queue, event(0, BEEP));
    enqueue(event_queue, event(simulation.sim_time + 1.0, TIMER));
    /* 
     * Pop the queue until nothing is left or time is exceeded
     */
    /* maxtime = simulation.sim_time + simulation.end_time;*/
    while (simulation.sim_time <= simulation.end_time &&
	   (!empty(event_queue))) {
	curr_event = dequeue(event_queue);
	/* Update all the clocks to the time on the event */
	sim_update_clocks(curr_event);

	/* Execute the function associated with the event */
	event_ptr[curr_event->function](curr_event);
	free_node(curr_event);
    }
    return (0);
}



/* Define a function to create an return an Event  */

Event *event(double t, funcTkn f)
{
    Event *e;

    if ((e = get_node(sizeof(*e))) == NULL)
	abortsim("get_node failed in event");
    e->time = t;
    e->function = f;
    return (e);
}

/* NTP SIMULATION FUNCTIONS */

/* Define a function for processing a timer interrupt.
 * On every timer interrupt, call the NTP timer to send packets and process
 * the clock and then call the receive function to receive packets.
 */
void sim_event_timer(Event *e)
{
    struct recvbuf *rbuf;

    /* Call the NTP timer.
     * This will be responsible for actually "sending the packets."
     * Since this is a simulation, the packets sent over the network
     * will be processed by the simulate_server routine below.
     */
    timer();

    /* Process received buffers */
    while (!empty(recv_queue)) {
	rbuf = (struct recvbuf *)dequeue(recv_queue);
	(rbuf->receiver)(rbuf);
	free_node(rbuf);
    }

    /* Arm the next timer interrupt. */
    enqueue(event_queue, 
	    event(simulation.sim_time + (1 << EVENT_TIMEOUT), TIMER));
}



/* Define a function to simulate a server.
 * This function processes the sent packet according to the server script,
 * creates a reply packet and pushes the reply packet onto the event queue
 */
int simulate_server(
    sockaddr_u *serv_addr,		/* Address of the server */
    struct interface *inter,		/* Interface on which the reply should
					   be inserted */
    struct pkt *rpkt			/* Packet sent to the server that
					   needs to be processed. */
)
{
    struct pkt xpkt;	       /* Packet to be transmitted back
				  to the client */
    struct recvbuf rbuf;       /* Buffer for the received packet */
    Event *e;		       /* Packet receive event */
    server_info *server;       /* Pointer to the server being simulated */
    script_info *curr_script;  /* Current script being processed */
    int i;
    double d1, d2, d3;	       /* Delays while the packet is enroute */
    double t1, t2, t3, t4;     /* The four timestamps in the packet */

    memset(&xpkt, 0, sizeof(xpkt));
    memset(&rbuf, 0, sizeof(rbuf));

    /* Search for the server with the desired address */
    server = NULL;
    for (i = 0; i < simulation.num_of_servers; ++i) {
	fprintf(stderr,"Checking address: %s\n", stoa(simulation.servers[i].addr));
	if (memcmp(simulation.servers[i].addr, serv_addr, 
		   sizeof(*serv_addr)) == 0) { 
	    server = &simulation.servers[i];
	    break;
	}
    }

    fprintf(stderr, "Received packet for: %s\n", stoa(serv_addr));
    if (server == NULL)
	abortsim("Server with specified address not found!!!");
    
    /* Get the current script for the server */
    curr_script = server->curr_script;

    /* Create a server reply packet. 
     * Masquerade the reply as a stratum-1 server with a GPS clock
     */
    xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING, NTP_VERSION,
                                     MODE_SERVER);
    xpkt.stratum = STRATUM_TO_PKT(((u_char)1));
    memcpy(&xpkt.refid, "GPS", 4);
    xpkt.ppoll = rpkt->ppoll;
    xpkt.precision = rpkt->precision;
    xpkt.rootdelay = 0;
    xpkt.rootdisp = 0;

    /* TIMESTAMP CALCULATIONS
	    t1				 t4
	     \				/
	  d1  \			       / d3
	       \		      /
	       t2 ----------------- t3
			 d2
    */
    /* Compute the delays */
    d1 = poisson(curr_script->prop_delay, curr_script->jitter);
    d2 = poisson(curr_script->proc_delay, 0);
    d3 = poisson(curr_script->prop_delay, curr_script->jitter);

    /* Note: In the transmitted packet: 
     * 1. t1 and t4 are times in the client according to the local clock.
     * 2. t2 and t3 are server times according to the simulated server.
     * Compute t1, t2, t3 and t4
     * Note: This function is called at time t1. 
     */

    LFPTOD(&rpkt->xmt, t1);
    t2 = server->server_time + d1;
    t3 = server->server_time + d1 + d2;
    t4 = t1 + d1 + d2 + d3;

    /* Save the timestamps */
    xpkt.org = rpkt->xmt;     
    DTOLFP(t2, &xpkt.rec);
    DTOLFP(t3, &xpkt.xmt);
    xpkt.reftime = xpkt.xmt;



    /* Ok, we are done with the packet. Now initialize the receive buffer for
     * the packet.
     */
    rbuf.receiver = receive;   /* Function to call to process the packet */
    rbuf.recv_length = LEN_PKT_NOMAC;
    rbuf.recv_pkt = xpkt;
    rbuf.used = 1;

    memcpy(&rbuf.srcadr, serv_addr, sizeof(rbuf.srcadr));
    memcpy(&rbuf.recv_srcadr, serv_addr, sizeof(rbuf.recv_srcadr));
    if ((rbuf.dstadr = malloc(sizeof(*rbuf.dstadr))) == NULL)
	abortsim("malloc failed in simulate_server");
    memcpy(rbuf.dstadr, inter, sizeof(*rbuf.dstadr));
    /* rbuf.link = NULL; */

    /* Create a packet event and insert it onto the event_queue at the 
     * arrival time (t4) of the packet at the client 
     */
    e = event(t4, PACKET);
    e->rcv_buf = rbuf;
    enqueue(event_queue, e);
    

    /* Check if the time of the script has expired. If yes, delete the script.
     * If not, re-enqueue the script onto the server script queue 
     */
    if (curr_script->duration > simulation.sim_time && 
	!empty(server->script)) {
	printf("Hello\n");
	/* 
	 * For some reason freeing up the curr_script memory kills the
	 * simulation. Further debugging is needed to determine why.
	 * free_node(curr_script);
	 */
	curr_script = dequeue(server->script);
    }

    return (0);
}


/* Define a function to update all the clocks 
 * Most of the code is modified from the systime.c file by Prof. Mills
 */

void sim_update_clocks (Event *e)
{
    double time_gap;
    double adj;
    int i;

    /* Compute the time between the last update event and this update */
    time_gap = e->time - simulation.sim_time;

    /* Advance the client clock */
    simclock.local_time = e->time + time_gap;

    /* Advance the simulation time */
    simulation.sim_time = e->time;

    /* Advance the server clocks adjusted for systematic and random frequency
     * errors. The random error is a random walk computed as the
     * integral of samples from a Gaussian distribution.
     */
    for (i = 0;i < simulation.num_of_servers; ++i) {
	simulation.servers[i].curr_script->freq_offset +=
	    gauss(0, time_gap * simulation.servers[i].curr_script->wander);

	simulation.servers[i].server_time += time_gap * 
	    (1 + simulation.servers[i].curr_script->freq_offset);
    }


    /* Perform the adjtime() function. If the adjustment completed
     * in the previous interval, amortize the entire amount; if not,
     * carry the leftover to the next interval.
     */

    adj = time_gap * simclock.slew;
    if (adj < fabs(simclock.adj)) {
	if (simclock.adj < 0) {
	    simclock.adj += adj;
	    simclock.local_time -= adj;
	} 
	else {
	    simclock.adj -= adj;
	    simclock.local_time += adj;
	}    
    } 
    else {
	simclock.local_time += simclock.adj;
	simclock.adj = 0;
    }
}


/* Define a function that processes a receive packet event. 
 * This function simply inserts the packet received onto the receive queue
 */   

void sim_event_recv_packet(Event *e)
{
    struct recvbuf *rbuf;

    /* Allocate a receive buffer and copy the packet to it */
    if ((rbuf = get_node(sizeof(*rbuf))) == NULL)
	abortsim("get_node failed in sim_event_recv_packet");
    memcpy(rbuf, &e->rcv_buf, sizeof(*rbuf));

    /* Store the local time in the received packet */
    DTOLFP(simclock.local_time, &rbuf->recv_time);

    /* Insert the packet received onto the receive queue */
    enqueue(recv_queue, rbuf);
}



/* Define a function to output simulation statistics on a beep event
 */

/*** TODO: Need to decide on how to output for multiple servers ***/
void sim_event_beep(Event *e)
{
#if 0
    static int first_time = 1;
    char *dash = "-----------------";
#endif

    fprintf(stderr, "BEEP!!!\n");
    enqueue(event_queue, event(e->time + simulation.beep_delay, BEEP));
#if 0
    if(simulation.beep_delay > 0) {
	if (first_time) {
	    printf("\t%4c    T    %4c\t%4c  T+ERR  %3c\t%5cT+ERR+NTP\n", 
	           ' ', ' ', ' ', ' ',' ');
	    printf("\t%s\t%s\t%s\n", dash, dash, dash);
	    first_time = 0;

	    printf("\t%16.6f\t%16.6f\t%16.6f\n",
	           n->time, n->clk_time, n->ntp_time);
	    return;
	}
	printf("\t%16.6f\t%16.6f\t%16.6f\n",
	       simclock.local_time, 
	       n->time, n->clk_time, n->ntp_time);
#endif

}


/* Define a function to abort the simulation on an error and spit out an
 * error message
 */

void abortsim(char *errmsg)
{
    perror(errmsg);
    exit(1);
}



/* CODE ORIGINALLY IN libntp/systime.c 
 * -----------------------------------
 * This code was a part of the original NTP simulator and originally 
 * had its home in the libntp/systime.c file. 
 *
 * It has been shamelessly moved to here and has been modified for the
 * purposes of the current simulator.
 */


/*
 * get_systime - return the system time in NTP timestamp format 
 */
void
get_systime(
    l_fp *now		/* current system time in l_fp */        )
{
    /*
     * To fool the code that determines the local clock precision,
     * we advance the clock a minimum of 200 nanoseconds on every
     * clock read. This is appropriate for a typical modern machine
     * with nanosecond clocks. Note we make no attempt here to
     * simulate reading error, since the error is so small. This may
     * change when the need comes to implement picosecond clocks.
     */
    if (simclock.local_time == simclock.last_read_time)
        simclock.local_time += 200e-9;

    simclock.last_read_time = simclock.local_time;
    DTOLFP(simclock.local_time, now);
/* OLD Code
   if (ntp_node.ntp_time == ntp_node.last_time)
   ntp_node.ntp_time += 200e-9;
   ntp_node.last_time = ntp_node.ntp_time;
   DTOLFP(ntp_node.ntp_time, now);
*/
}
 
 
/*
 * adj_systime - advance or retard the system clock exactly like the
 * real thng.
 */
int				/* always succeeds */
adj_systime(
    double now		/* time adjustment (s) */
    )
{
    struct timeval adjtv;	/* new adjustment */
    double	dtemp;
    long	ticks;
    int	isneg = 0;

    /*
     * Most Unix adjtime() implementations adjust the system clock
     * in microsecond quanta, but some adjust in 10-ms quanta. We
     * carefully round the adjustment to the nearest quantum, then
     * adjust in quanta and keep the residue for later.
     */
    dtemp = now + sys_residual;
    if (dtemp < 0) {
	isneg = 1;
	dtemp = -dtemp;
    }
    adjtv.tv_sec = (long)dtemp;
    dtemp -= adjtv.tv_sec;
    ticks = (long)(dtemp / sys_tick + .5);
    adjtv.tv_usec = (long)(ticks * sys_tick * 1e6);
    dtemp -= adjtv.tv_usec / 1e6;
    sys_residual = dtemp;

    /*
     * Convert to signed seconds and microseconds for the Unix
     * adjtime() system call. Note we purposely lose the adjtime()
     * leftover.
     */
    if (isneg) {
	adjtv.tv_sec = -adjtv.tv_sec;
	adjtv.tv_usec = -adjtv.tv_usec;
	sys_residual = -sys_residual;
    }
    simclock.adj = now;
/*	ntp_node.adj = now; */
    return (1);
}
 
 
/*
 * step_systime - step the system clock. We are religious here.
 */
int				/* always succeeds */
step_systime(
    double now		/* step adjustment (s) */
    )
{
#ifdef DEBUG
    if (debug)
	printf("step_systime: time %.6f adj %.6f\n",
	       simclock.local_time, now);
#endif
    simclock.local_time += now;
    return (1);
}
 
/*
 * gauss() - returns samples from a gaussion distribution
 */
double				/* Gaussian sample */
gauss(
    double m,		/* sample mean */
    double s		/* sample standard deviation (sigma) */
    )
{
    double q1, q2;

    /*
     * Roll a sample from a Gaussian distribution with mean m and
     * standard deviation s. For m = 0, s = 1, mean(y) = 0,
     * std(y) = 1.
     */
    if (s == 0)
        return (m);
    while ((q1 = drand48()) == 0);
    q2 = drand48();
    return (m + s * sqrt(-2. * log(q1)) * cos(2. * PI * q2));
}

 
/*
 * poisson() - returns samples from a network delay distribution
 */
double				/* delay sample (s) */
poisson(
    double m,		/* fixed propagation delay (s) */
    double s		/* exponential parameter (mu) */
    )
{
    double q1;

    /*
     * Roll a sample from a composite distribution with propagation
     * delay m and exponential distribution time with parameter s.
     * For m = 0, s = 1, mean(y) = std(y) = 1.
     */
    if (s == 0)
        return (m);
    while ((q1 = drand48()) == 0);
    return (m - s * log(q1 * s));
}

#endif
