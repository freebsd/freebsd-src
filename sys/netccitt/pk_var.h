/*
 * Copyright (c) University of British Columbia, 1984
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Laboratory for Computation Vision and the Computer Science Department
 * of the University of British Columbia.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)pk_var.h	7.11 (Berkeley) 5/29/91
 *	$Id: pk_var.h,v 1.5 1993/12/19 00:52:25 wollman Exp $
 */


#ifndef _NETCCITT_PK_VAR_H_
#define _NETCCITT_PK_VAR_H_ 1

/*
 *
 *  X.25 Logical Channel Descriptor
 *
 */

struct pklcd {
	struct 	pklcd_q {
		struct	pklcd_q *q_forw;	/* debugging chain */
		struct	pklcd_q *q_back;	/* debugging chain */
	} lcd_q;
	void	(*lcd_upper)(struct pklcd *, struct mbuf *);
				/* switch to socket vs datagram vs ...*/
	caddr_t	lcd_upnext;		/* reference for lcd_upper() */
	int	(*lcd_send)();		/* if X.25 front end, direct connect */
	caddr_t lcd_downnext;		/* reference for lcd_send() */
	short   lcd_lcn;		/* Logical channel number */
	short   lcd_state;		/* Logical Channel state */
	short   lcd_timer;		/* Various timer values */
	short   lcd_dg_timer;		/* to reclaim idle datagram circuits */
        bool	lcd_intrconf_pending;	/* Interrupt confirmation pending */
	octet	lcd_intrdata;		/* Octet of incoming intr data */
	char	lcd_retry;		/* Timer retry count */
	char	lcd_rsn;		/* Seq no of last received packet */
	char	lcd_ssn;		/* Seq no of next packet to send */
	char	lcd_output_window;	/* Output flow control window */
	char	lcd_input_window;	/* Input flow control window */
	char	lcd_last_transmitted_pr;/* Last Pr value transmitted */
        bool	lcd_rnr_condition;	/* Remote in busy condition */
        bool	lcd_window_condition;	/* Output window size exceeded */
        bool	lcd_reset_condition;	/* True, if waiting reset confirm */
	bool	lcd_rxrnr_condition;	/* True, if we have sent rnr */
	char	lcd_packetsize;		/* Maximum packet size */
	char	lcd_windowsize;		/* Window size - both directions */
        octet	lcd_closed_user_group;	/* Closed user group specification */
	char	lcd_flags;		/* copy of sockaddr_x25 op_flags */
	struct	mbuf *lcd_facilities;	/* user supplied facilities for cr */
	struct	mbuf *lcd_template;	/* Address of response packet */
	struct	socket *lcd_so;		/* Socket addr for connection */
	struct	sockaddr_x25 *lcd_craddr;/* Calling address pointer */
	struct	sockaddr_x25 *lcd_ceaddr;/* Called address pointer */
	time_t	lcd_stime;		/* time circuit established */
	long    lcd_txcnt;		/* Data packet transmit count */
	long    lcd_rxcnt;		/* Data packet receive count */
	short   lcd_intrcnt;		/* Interrupt packet transmit count */
	struct	pklcd *lcd_listen;	/* Next lcd on listen queue */
	struct	pkcb *lcd_pkp;		/* Network this lcd is attached to */
	struct	mbuf *lcd_cps;		/* Complete Packet Sequence reassembly*/
	long	lcd_cpsmax;		/* Max length for CPS */
	struct	sockaddr_x25 lcd_faddr;	/* Remote Address (Calling) */
	struct	sockaddr_x25 lcd_laddr;	/* Local Address (Called) */
	struct	sockbuf lcd_sb;		/* alternate for datagram service */
};

/*
 * Per network information, allocated dynamically
 * when a new network is configured.
 */

struct	pkcb {
	struct	pkcb *pk_next;
	short	pk_state;		/* packet level status */
	short	pk_maxlcn;		/* local copy of xc_maxlcn */
	int	(*pk_lloutput) ();	/* link level output procedure */
	caddr_t pk_llnext;		/* handle for next level down */
	struct	x25config *pk_xcp;	/* network specific configuration */
	struct	x25_ifaddr *pk_ia;	/* backpointer to ifaddr */
	struct	pklcd **pk_chan;	/* actual size == xc_maxlcn+1 */
};
/*
 *	Interface address, x25 version. Exactly one of these structures is 
 *	allocated for each interface with an x25 address.
 *
 *	The ifaddr structure conatins the protocol-independent part
 *	of the structure, and is assumed to be first.
 */
struct x25_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define ia_ifp	ia_ifa.ifa_ifp
#define	ia_flags ia_ifa.ifa_flags
	struct	x25config ia_xc;	/* network specific configuration */
#define ia_maxlcn ia_xc.xc_maxlcn
	int	(*ia_start) (struct pklcd *); /* connect, confirm method */
	struct	sockaddr_x25 ia_dstaddr; /* reserve space for route dst */
};

/*
 * ``Link-Level'' extension to Routing Entry for upper level
 * packet switching via X.25 virtual circuits.
 */
struct llinfo_x25 {
	struct	llinfo_x25 *lx_next;	/* chain together in linked list */
	struct	llinfo_x25 *lx_prev;	/* chain together in linked list */
	struct	rtentry *lx_rt;		/* back pointer to route */
	struct	pklcd *lx_lcd;		/* local connection block */
	struct	x25_ifaddr *lx_ia;	/* may not be same as rt_ifa */
	int	lx_state;		/* can't trust lcd->lcd_state */
	int	lx_flags;
	int	lx_timer;		/* for idle timeout */
	int	lx_family;		/* for dispatch */
};

/* States for lx_state */
#define LXS_NEWBORN		0
#define LXS_RESOLVING		1
#define LXS_FREE		2
#define LXS_CONNECTING		3
#define LXS_CONNECTED		4
#define LXS_DISCONNECTING 	5
#define LXS_LISTENING 		6

/* flags */
#define LXF_VALID	0x1		/* Circuit is live, etc. */
#define LXF_RTHELD	0x2		/* this lcb references rtentry */
#define LXF_LISTEN	0x4		/* accepting incoming calls */

/*
 * miscellenous debugging info
 */
struct mbuf_cache {
	int	mbc_size;
	int	mbc_num;
	int	mbc_oldsize;
	struct	mbuf **mbc_cache;
};

#if defined(KERNEL) && defined(CCITT)
struct	pkcb *pkcbhead;		/* head of linked list of networks */
struct	pklcd *pk_listenhead;

struct sockaddr_in;
struct sockaddr_x25;

extern void x25_connect_callback(struct pklcd *, struct mbuf *);
extern void x25_rtrequest(int, struct rtentry *, struct sockaddr *);
extern void x25_rtinvert(int, struct sockaddr *, struct rtentry *);
extern void x25_ddnip_to_ccitt(struct sockaddr_in *, struct rtentry *);
extern void x25_rtattach(struct pklcd *, struct rtentry *);

/* From pk_subr.c: */
extern struct pklcd *pk_attach(struct socket *);
extern void pk_disconnect(struct pklcd *);
extern void pk_close(struct pklcd *);
extern struct mbuf *pk_template(int, int);
extern void pk_restart(struct pkcb *, int);
extern void pk_freelcd(struct pklcd *);
extern int pk_bind(struct pklcd *, struct mbuf *);
extern int pk_listen(struct pklcd *);
extern int pk_protolisten(int, int, void (*)(/* XXX */));
extern void pk_assoc(struct pkcb *, struct pklcd *, struct sockaddr_x25 *);
extern int pk_connect(struct pklcd *, struct sockaddr_x25 *);
extern void pk_callrequest(struct pklcd *, struct sockaddr_x25 *, struct x25config *);
extern void pk_build_facilities(struct mbuf *, struct sockaddr_x25 *, int);
extern int pk_getlcn(struct pkcb *);
extern void pk_clear(struct pklcd *, int, int);
extern void pk_flowcontrol(struct pklcd *, int, int);
extern void pk_flush(struct pklcd *);
extern void pk_procerror(int, struct pklcd *, const char *, int);
extern int pk_ack(struct pklcd *, unsigned);
extern int pk_decode(struct x25_packet *);
extern void pk_restartcause(struct pkcb *, struct x25_packet *);
extern void pk_resetcause(struct pkcb *, struct x25_packet *);
extern void pk_clearcause(struct pkcb *, struct x25_packet *);
extern void pk_message(int, struct x25config *, const char *, ...);
extern int pk_fragment(struct pklcd *, struct mbuf *, int, int, int);

/* From pk_input.c: */
extern struct pkcb *pk_newlink(struct x25_ifaddr *, caddr_t);
extern int pk_resize(struct pkcb *);
extern int /*void*/ pk_ctlinput(int, struct pkcb *);
extern struct ifqueue pkintrq;
extern void pkintr(void);	/* called from locore */
extern void pk_input(struct mbuf *);
extern void pk_incoming_call(struct pkcb *, struct mbuf *);
extern void pk_call_accepted(struct pklcd *, struct mbuf *);
extern void pk_parse_facilities(octet *, struct sockaddr_x25 *);

/* From pk_output.c: */
extern int pk_output(struct pklcd *, struct mbuf *);

/* From pk_acct.c: */
extern int pk_accton(char *);
extern void pk_acct(struct pklcd *);

/* From pk_debug.c: */
extern const char *const pk_name[], *const pk_state[];
extern void pk_trace(struct x25config *, struct mbuf *, const char *);
extern void mbuf_cache(struct mbuf_cache *, struct mbuf *);

/* From pk_timer.c: */
extern int pk_t20, pk_t21, pk_t23;
extern void pk_timer(void);

/* From pk_usrreq.c: */
extern int pk_usrreq(struct socket *, int, struct mbuf *, struct mbuf *, struct mbuf *);
extern int pk_start(struct pklcd *);
extern int pk_control(struct socket *, int, caddr_t, struct ifnet *);
extern int pk_ctloutput(int, struct socket *, int, int, struct mbuf **);
extern int pk_checksockaddr(struct mbuf *);
extern int pk_send(struct pklcd *, struct mbuf *);

#endif
#endif /* _NETCCITT_PK_VAR_H_ */
