/*
 * Copyright (c) 1998 Luigi Rizzo
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 *	$Id: ip_dummynet.h,v 1.3 1999/01/23 23:59:50 archie Exp $
 */

#ifndef _IP_DUMMYNET_H
#define _IP_DUMMYNET_H

/*
 * Definition of dummynet data structures.
 * Dummynet handles a list of pipes, each one identified by a unique
 * number (hopefully the list is short so we use a linked list).
 *
 * Each list contains a set of parameters identifying the pipe, and
 * a set of packets queued on the pipe itself.
 *
 * I could have used queue macros, but the management i have
 * is pretty simple and this makes the code more portable.
 */

/*
 * struct dn_pkt identifies a packet in the dummynet queue. The
 * first part is really an m_hdr for implementation purposes, and some
 * fields are saved there. When passing the packet back to the ip_input/
 * ip_output(), the struct is prepended to the mbuf chain with type
 * MT_DUMMYNET, and contains the pointer to the matching rule.
 */
struct dn_pkt {
	struct m_hdr hdr ;
#define dn_next	hdr.mh_nextpkt	/* next element in queue */
#define dn_m	hdr.mh_next	/* packet to be forwarded */
#define dn_hlen	hdr.mh_len	/* hlen, for ip_output			*/
#define dn_dir	hdr.mh_flags	/* IP_FW_F_IN or IP_FW_F_OUT		*/
        int     delay;		/* stays queued until delay=0		*/
        struct ifnet *ifp;	/* interface, for ip_output		*/
        struct route ro;	/* route, for ip_output. MUST COPY	*/

#ifdef   DUMMYNET_DEBUG
        struct timeval beg, mid;        /* testing only */
        int     act_delay;      /* testing only */
        int     in_delay;       /* testing only */
#endif
};

struct dn_queue {
	struct dn_pkt *head, *tail;
} ;

/*
 * descriptor of a pipe. The flags field will be used to speed up the
 * forwarding code paths, in case some of the parameters are not
 * used.
 */
struct dn_pipe {			/* a pipe */
	struct dn_pipe *next ;

	u_short	pipe_nr ;		/* number	*/
	u_short	flags ;			/* to speed up things	*/
#define DN_HAVE_BW	1
#define DN_HAVE_QUEUE	2
#define DN_HAVE_DELAY	4
	int	bandwidth;		/* really, bytes/tick.	*/
	int	queue_size ;
	int	queue_size_bytes ;
	int	delay ;			/* really, ticks	*/
	int	plr ;		/* pkt loss rate (2^31-1 means 100%) */

        struct	dn_queue r;
        int	r_len;			/* elements in r_queue */
        int	r_len_bytes;		/* bytes in r_queue */
        int	r_drops;		/* drops from r_queue */
        struct	dn_queue p ;
        int     ticks_from_last_insert;
        long    numbytes;		/* which can send or receive */
};

/*
 * The following is used to define a new mbuf type that is
 * prepended to the packet when it comes out of a pipe. The definition
 * ought to go in /sys/sys/mbuf.h but here it is less intrusive.
 */

#define MT_DUMMYNET MT_CONTROL
/*
 * what to do of a packet when it comes out of a pipe
 */
#define DN_TO_IP_OUT	1
#define DN_TO_IP_IN	2
#define DN_TO_BDG_FWD	3

#ifdef KERNEL

MALLOC_DECLARE(M_IPFW);

typedef int ip_dn_ctl_t __P((struct sockopt *)) ;
extern ip_dn_ctl_t *ip_dn_ctl_ptr;

void dn_rule_delete(void *r);		/* used in ip_fw.c */
int dummynet_io(int pipe, int dir,
	struct mbuf *m, struct ifnet *ifp, struct route *ro, int hlen,
	struct ip_fw_chain *rule);
#endif /* KERNEL */

#endif /* _IP_DUMMYNET_H */
