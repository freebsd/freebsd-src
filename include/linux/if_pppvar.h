/*	From: if_pppvar.h,v 1.2 1995/06/12 11:36:51 paulus Exp */
/*
 * if_pppvar.h - private structures and declarations for PPP.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 *  ==FILEVERSION 990806==
 *
 *  NOTE TO MAINTAINERS:
 *   If you modify this file at all, please set the above date.
 *   if_pppvar.h is shipped with a PPP distribution as well as with the kernel;
 *   if everyone increases the FILEVERSION number above, then scripts
 *   can do the right thing when deciding whether to install a new if_pppvar.h
 *   file.  Don't change the format of that line otherwise, so the
 *   installation script can recognize it.
 */

/*
 * Supported network protocols.  These values are used for
 * indexing sc_npmode.
 */

#define NP_IP	0		/* Internet Protocol */
#define NP_IPX	1		/* IPX protocol */
#define NP_AT	2		/* Appletalk protocol */
#define NP_IPV6	3		/* Internet Protocol */
#define NUM_NP	4		/* Number of NPs. */

#define OBUFSIZE	256	/* # chars of output buffering */

/*
 * Structure describing each ppp unit.
 */

struct ppp {
	int		magic;		/* magic value for structure	*/
	struct ppp	*next;		/* unit with next index		*/
	unsigned long	inuse;		/* are we allocated?		*/
	int		line;		/* network interface unit #	*/
	__u32		flags;		/* miscellaneous control flags	*/
	int		mtu;		/* maximum xmit frame size	*/
	int		mru;		/* maximum receive frame size	*/
	struct slcompress *slcomp;	/* for TCP header compression	*/
	struct sk_buff_head xmt_q;	/* frames to send from pppd	*/
	struct sk_buff_head rcv_q;	/* frames for pppd to read	*/
	unsigned long	xmit_busy;	/* bit 0 set when xmitter busy  */

	/* Information specific to using ppp on async serial lines. */
	struct tty_struct *tty;		/* ptr to TTY structure	*/
	struct tty_struct *backup_tty;	/* TTY to use if tty gets closed */
	__u8		escape;		/* 0x20 if prev char was PPP_ESC */
	__u8		toss;		/* toss this frame		*/
	volatile __u8	tty_pushing;	/* internal state flag		*/
	volatile __u8	woke_up;	/* internal state flag		*/
	__u32		xmit_async_map[8]; /* 1 bit means that given control 
					   character is quoted on output*/
	__u32		recv_async_map; /* 1 bit means that given control 
					   character is ignored on input*/
	__u32		bytes_sent;	/* Bytes sent on frame	*/
	__u32		bytes_rcvd;	/* Bytes recvd on frame	*/

	/* Async transmission information */
	struct sk_buff	*tpkt;		/* frame currently being sent	*/
	int		tpkt_pos;	/* how much of it we've done	*/
	__u16		tfcs;		/* FCS so far for it		*/
	unsigned char	*optr;		/* where we're up to in sending */
	unsigned char	*olim;		/* points past last valid char	*/

	/* Async reception information */
	struct sk_buff	*rpkt;		/* frame currently being rcvd	*/
	__u16		rfcs;		/* FCS so far of rpkt		*/

	/* Queues for select() functionality */
	wait_queue_head_t read_wait;	/* queue for reading processes	*/

	/* info for detecting idle channels */
	unsigned long	last_xmit;	/* time of last transmission	*/
	unsigned long	last_recv;	/* time last packet received    */

	/* Statistic information */
	struct pppstat	stats;		/* statistic information	*/

	/* PPP compression protocol information */
	struct	compressor *sc_xcomp;	/* transmit compressor */
	void	*sc_xc_state;		/* transmit compressor state */
	struct	compressor *sc_rcomp;	/* receive decompressor */
	void	*sc_rc_state;		/* receive decompressor state */

	enum	NPmode sc_npmode[NUM_NP]; /* what to do with each NP */
	int	 sc_xfer;		/* PID of reserved PPP table */
	char	name[16];		/* space for unit name */
	struct net_device	dev;		/* net device structure */
	struct net_device_stats estats;	/* more detailed stats */

	/* tty output buffer */
	unsigned char	obuf[OBUFSIZE];	/* buffer for characters to send */
};

#define PPP_MAGIC	0x5002
#define PPP_VERSION	"2.3.7"
