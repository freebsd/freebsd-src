/*
 * ntpsim.h - Prototypes for ntpsim
 */

#ifndef __ntpsim_h
#define __ntpsim_h

#include <stdio.h>
#include <math.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ntp_syslog.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_select.h"
#include "ntp_malloc.h"
#include "ntp_refclock.h"
#include "recvbuff.h"
#include "ntp_io.h"
#include "ntp_stdlib.h"

#define PI 3.1415926535

/*
 * ntpsim declarations
 */
typedef enum {
        BEEP, CLOCK, TIMER, PACKET
} funcTkn;

typedef struct {
        double time;
        union {
                struct pkt evnt_pkt;
		struct recvbuf evnt_buf;
        } buffer;
#define ntp_pkt buffer.evnt_pkt
#define rcv_buf buffer.evnt_buf
        funcTkn function;
} Event;

typedef struct List {
        Event event;
        struct List *next;
} *Queue;

typedef struct nde {
        double	time;			/* simulation time */
	double	sim_time;		/* end simulation time */
	double	ntp_time;		/* client disciplined time */
	double	adj;			/* remaining time correction */
	double	slew;			/* correction slew rate */

	double	clk_time;		/* server time */
	double	ferr;			/* frequency errort */
	double	fnse;			/* random walk noise */
	double	ndly;			/* network delay */
	double	snse;			/* phase noise */
	double	pdly;			/* processing delay */
	double	bdly;			/* beep interval */

	double	last_time;		/* last clock read time */
        Queue	events;			/* Node Event Queue */
	struct recvbuf *rbuflist;	/* Node Receive Buffer */
} Node;

/*
 * Function prototypes
 */
int	ntpsim		P((int argc, char *argv[]));
Event	event		P((double, funcTkn));
Queue	queue		P((Event, Queue ));
Node	node		P((void));
void	push		P((Event, Queue *));
Event	pop		P((Queue *));
void	ndbeep		P((Node *, Event));
void	ndeclk		P((Node *, Event));
void	ntptmr		P((Node *, Event));
void	netpkt		P((Node *, Event));
int	srvr_rply	P((Node *, struct sockaddr_storage *,
			    struct interface *, struct pkt *));
double	gauss		P((double, double));
double	poisson		P((double, double));
int	node_clock	P((Node *, double));
void	abortsim	P((char *));

/*
 * The global Node
 */
Node ntp_node;

#endif

