/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net/iso88025.h,v 1.3 1999/08/28 00:48:24 peter Exp $
 *
 * Information gathered from tokenring@freebsd, /sys/net/ethernet.h and
 * the Mach token ring driver.
 */

/*
 * Fundamental constants relating to iso 802.5
 */

#ifndef _NET_ISO88025_H_
#define _NET_ISO88025_H_

/*
 * The number of bytes in an iso 802.5 (MAC) address.
 */
#define	ISO88025_ADDR_LEN	6

/*
 */
#define ISO88025_HDR_LEN        (ISO88025_CF_LEN + ISO88025_ADDR_LEN*2)
#define ISO88025_CF_LEN         2
#define RCF_LEN                 2
#define RIF_LEN                 16


/*
 * The minimum packet length.
 */
#define	ISO88025_MIN_LEN	0 /* This offends my morality */

/*
 * The maximum packet length.
 */
#define	ISO88025_MAX_LEN	17960	

/*
 * A macro to validate a length with
 */
#define	ISO88025_IS_VALID_LEN(foo)	\
	((foo) >= ISO88025_MIN_LEN && (foo) <= ISO88025_MAX_LEN)

/*
 * ISO 802.5 physical header 
 */
struct iso88025_header {
        u_char   ac;                                    /* access control field */
        u_char   fc;                                    /* frame control field */
        u_char   iso88025_dhost[ISO88025_ADDR_LEN];     /* destination address */
        u_char   iso88025_shost[ISO88025_ADDR_LEN];     /* source address */
        u_short  rcf;                                   /* route control field */
        u_short  rseg[RIF_LEN];                         /* routing registers */
};

struct iso88025_sockaddr_data {
	u_char ether_dhost[ISO88025_ADDR_LEN];
	u_char ether_shost[ISO88025_ADDR_LEN];
	u_char ac;
	u_char fc;
};

/*
 * Structure of a 48-bit iso 802.5 address.
 *  ( We could also add the 16 bit addresses as a union)
 */
struct	iso88025_addr {
	u_char octet[ISO88025_ADDR_LEN];
};

#define ISO88025MTU     18000
#define ISO88025_DEFAULT_MTU     1500
#define senderr(e) { error = (e); goto bad;}

void   iso88025_ifattach __P((struct ifnet *));
int    iso88025_ioctl __P((struct ifnet *, int , caddr_t ));
int    iso88025_output __P((struct ifnet *, struct mbuf *, struct sockaddr *, struct rtentry *));
void   iso88025_input __P((struct ifnet *, struct iso88025_header *, struct mbuf *));


#endif
