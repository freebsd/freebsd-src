/*-
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
 * $FreeBSD$
 *
 * Information gathered from tokenring@freebsd, /sys/net/ethernet.h and
 * the Mach token ring driver.
 */

/*
 * Fundamental constants relating to iso 802.5
 */

#ifndef _NET_ISO88025_H_
#define	_NET_ISO88025_H_

/*
 * General ISO 802.5 definitions
 */
#define	ISO88025_ADDR_LEN	6
#define	ISO88025_CF_LEN		2
#define	ISO88025_HDR_LEN	(ISO88025_CF_LEN + (ISO88025_ADDR_LEN * 2))
#define	RCF_LEN			2
#define	RIF_MAX_RD		14
#define	RIF_MAX_LEN		16

#define	TR_AC			0x10
#define	TR_LLC_FRAME		0x40

#define	TR_4MBPS		4000000
#define	TR_16MBPS		16000000
#define	TR_100MBPS		100000000

/*
 * Source routing 
 */
#define	TR_RII			0x80
#define	TR_RCF_BCST_MASK	0xe000
#define	TR_RCF_LEN_MASK		0x1f00
#define	TR_RCF_DIR		0x0080
#define	TR_RCF_LF_MASK		0x0070

#define	TR_RCF_RIFLEN(x)	((ntohs(x) & TR_RCF_LEN_MASK) >> 8)

/*
 * Minimum and maximum packet payload lengths.
 */
#define	ISO88025_MIN_LEN	0 
#define	ISO88025_MAX_LEN_4	4464
#define	ISO88025_MAX_LEN_16	17960	
#define	ISO88025_MAX_LEN	ISO88025_MAX_LEN_16

/*
 * A macro to validate a length with
 */
#define	ISO88025_IS_VALID_LEN(foo)	\
	((foo) >= ISO88025_MIN_LEN && (foo) <= ISO88025_MAX_LEN)

/* Access Control field */
#define	AC_PRI_MASK		0xe0	/* Priority bits 		*/
#define	AC_TOKEN		0x10	/* Token bit: 0=Token, 1=Frame	*/
#define	AC_MONITOR		0x08	/* Monitor			*/
#define	AC_RESV_MASK		0x07	/* Reservation bits		*/

/* Frame Control field */
#define	FC_FT_MASK		0xc0	/* Frame Type			*/
#define	FC_FT_MAC		0x00	/* MAC frame			*/
#define	FC_FT_LLC		0x40	/* LLC frame			*/
#define	FC_ATTN_MASK		0x0f	/* Attention bits		*/
#define	FC_ATTN_EB		0x01	/* Express buffer		*/
#define	FC_ATTN_BE		0x02	/* Beacon			*/
#define	FC_ATTN_CT		0x03	/* Claim token			*/
#define	FC_ATTN_RP		0x04	/* Ring purge			*/
#define	FC_ATTN_AMP		0x05	/* Active monitor present	*/
#define	FC_ATTN_SMP		0x06	/* Standby monitor present	*/

/* Token Ring destination address */
#define	DA_IG			0x80	/* Individual/group address.	*/
					/* 0=Individual, 1=Group	*/
#define	DA_UL			0x40	/* Universal/local address.	*/
					/* 0=Universal, 1=Local		*/
/* Token Ring source address */
#define	SA_RII			0x80	/* Routing information indicator */
#define	SA_IG			0x40	/* Individual/group address	*/
					/* 0=Group, 1=Individual	*/

/*
 * ISO 802.5 physical header
 */
struct iso88025_header {
	u_int8_t	ac;				    /* access control field */
	u_int8_t	fc;				    /* frame control field */
	u_int8_t	iso88025_dhost[ISO88025_ADDR_LEN];  /* destination address */
	u_int8_t	iso88025_shost[ISO88025_ADDR_LEN];  /* source address */
	u_int16_t	rcf;				    /* route control field */
	u_int16_t	rd[RIF_MAX_RD];			    /* routing designators */
} __packed;

struct iso88025_rif {
	u_int16_t	rcf;				    /* route control field */
	u_int16_t	rd[RIF_MAX_RD];			    /* routing designators */
} __packed;

struct iso88025_sockaddr_data {
	u_char ether_dhost[ISO88025_ADDR_LEN];
	u_char ether_shost[ISO88025_ADDR_LEN];
	u_char ac;
	u_char fc;
};

struct iso88025_sockaddr_dl_data {
	u_short	 trld_rcf;
	u_short	*trld_route[RIF_MAX_LEN];
};

#define	ISO88025_MAX(a, b)	(((a)>(b))?(a):(b))
#define	SDL_ISO88025(s)		((struct iso88025_sockaddr_dl_data *)	\
				 ((s)->sdl_data + \
				  ISO88025_MAX((s)->sdl_nlen + (s)->sdl_alen + \
					       (s)->sdl_slen, 12)))

/*
 * Structure of a 48-bit iso 802.5 address.
 *  ( We could also add the 16 bit addresses as a union)
 */
struct	iso88025_addr {
	u_char octet[ISO88025_ADDR_LEN];
};

#define	ISO88025_MAX_MTU		18000
#define	ISO88025_DEFAULT_MTU		1500

#define	ISO88025_BPF_UNSUPPORTED	0
#define	ISO88025_BPF_SUPPORTED		1

void	iso88025_ifattach	(struct ifnet *, int);
void	iso88025_ifdetach	(struct ifnet *, int);
int	iso88025_ioctl		(struct ifnet *, int , caddr_t );
int	iso88025_output		(struct ifnet *, struct mbuf *, struct sockaddr *,
				 struct rtentry *);
void	iso88025_input		(struct ifnet *, struct mbuf *);

#endif
