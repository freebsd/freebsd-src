#if !defined __recvbuff_h
#define __recvbuff_h

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntp.h"
#include "ntp_fp.h"
#include "ntp_types.h"

/*
 * recvbuf memory management
 */
#define RECV_INIT	10	/* 10 buffers initially */
#define RECV_LOWAT	3	/* when we're down to three buffers get more */
#define RECV_INC	5	/* get 5 more at a time */
#define RECV_TOOMANY	40	/* this is way too many buffers */

#if defined HAVE_IO_COMPLETION_PORT
# include "ntp_iocompletionport.h"
#include "ntp_timer.h"

# define RECV_BLOCK_IO()	EnterCriticalSection(&RecvCritSection)
# define RECV_UNBLOCK_IO()	LeaveCriticalSection(&RecvCritSection)

/*  Return the event which is set when items are added to the full list
 */
extern HANDLE	get_recv_buff_event P((void));
#else
# define RECV_BLOCK_IO()	
# define RECV_UNBLOCK_IO()	
#endif


/*
 * Format of a recvbuf.  These are used by the asynchronous receive
 * routine to store incoming packets and related information.
 */

/*
 *  the maximum length NTP packet contains the NTP header, one Autokey
 *  request, one Autokey response and the MAC. Assuming certificates don't
 *  get too big, the maximum packet length is set arbitrarily at 1000.
 */   
#define	RX_BUFF_SIZE	1000		/* hail Mary */

struct recvbuf {
	struct recvbuf *next;		/* next buffer in chain */
	union {
		struct sockaddr_storage X_recv_srcadr;
		caddr_t X_recv_srcclock;
		struct peer *X_recv_peer;
	} X_from_where;
#define recv_srcadr	X_from_where.X_recv_srcadr
#define	recv_srcclock	X_from_where.X_recv_srcclock
#define recv_peer	X_from_where.X_recv_peer
#if defined HAVE_IO_COMPLETION_PORT
        IoCompletionInfo	iocompletioninfo;
	WSABUF		wsabuff;
	DWORD		AddressLength;
#else
	struct sockaddr_storage srcadr;	/* where packet came from */
#endif
	struct interface *dstadr;	/* interface datagram arrived thru */
	SOCKET	fd;			/* fd on which it was received */
	l_fp recv_time;			/* time of arrival */
	void (*receiver) P((struct recvbuf *)); /* routine to receive buffer */
	int recv_length;		/* number of octets received */
	union {
		struct pkt X_recv_pkt;
		u_char X_recv_buffer[RX_BUFF_SIZE];
	} recv_space;
#define	recv_pkt	recv_space.X_recv_pkt
#define	recv_buffer	recv_space.X_recv_buffer
};

extern	void	init_recvbuff	P((int));

/* freerecvbuf - make a single recvbuf available for reuse
 */
extern	void	freerecvbuf P((struct recvbuf *));

	
extern	struct recvbuf * getrecvbufs P((void));

/*  Get a free buffer (typically used so an async
 *  read can directly place data into the buffer
 *
 *  The buffer is removed from the free list. Make sure
 *  you put it back with freerecvbuf() or 
 */
extern	struct recvbuf *get_free_recv_buffer P((void));

/*   Add a buffer to the full list
 */
extern	void	add_full_recv_buffer	 P((struct recvbuf *));

/*extern	void	process_recv_buffers	 P((void)); */

/* number of recvbufs on freelist */
extern u_long free_recvbuffs P((void));		
extern u_long full_recvbuffs P((void));		
extern u_long total_recvbuffs P((void));
extern u_long lowater_additions P((void));
		
/*  Returns the next buffer in the full list.
 *
 */
extern	struct recvbuf *get_full_recv_buffer P((void));

#endif /* defined __recvbuff_h */

