/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1995 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#ifndef	_SVR4_SOCKIO_H_
#define	_SVR4_SOCKIO_H_

#define	SVR4_IFF_UP		0x0001
#define	SVR4_IFF_BROADCAST	0x0002
#define	SVR4_IFF_DEBUG		0x0004
#define	SVR4_IFF_LOOPBACK	0x0008
#define	SVR4_IFF_POINTOPOINT	0x0010
#define	SVR4_IFF_NOTRAILERS	0x0020
#define	SVR4_IFF_RUNNING	0x0040
#define	SVR4_IFF_NOARP		0x0080	
#define	SVR4_IFF_PROMISC	0x0100
#define	SVR4_IFF_ALLMULTI	0x0200
#define	SVR4_IFF_INTELLIGENT	0x0400
#define	SVR4_IFF_MULTICAST	0x0800
#define	SVR4_IFF_MULTI_BCAST	0x1000
#define	SVR4_IFF_UNNUMBERED	0x2000
#define	SVR4_IFF_PRIVATE	0x8000	

struct svr4_ifreq {
#define	SVR4_IFNAMSIZ	16
	char	svr4_ifr_name[SVR4_IFNAMSIZ];
	union {
		struct	osockaddr	ifru_addr;
		struct	osockaddr	ifru_dstaddr;
		struct	osockaddr	ifru_broadaddr;
		short			ifru_flags;
		int			ifru_metric;
		char			ifru_data;	
		char			ifru_enaddr[6];
		int			if_muxid[2];

	} ifr_ifru;

#define	svr4_ifr_addr			ifr_ifru.ifru_addr
#define	svr4_ifr_dstaddr		ifr_ifru.ifru_dstaddr
#define	svr4_ifr_broadaddr		ifr_ifru.ifru_broadaddr
#define	svr4_ifr_flags			ifr_ifru.ifru_flags
#define	svr4_ifr_metric			ifr_ifru.ifru_metric
#define	svr4_ifr_data			ifr_ifru.ifru_data
#define	svr4_ifr_enaddr			ifr_ifru.ifru_enaddr
#define	svr4_ifr_muxid			ifr_ifru.ifru_muxid

};

struct svr4_ifconf {
	int	svr4_ifc_len;
	union {
		caddr_t			 ifcu_buf;
		struct svr4_ifreq 	*ifcu_req;
	} ifc_ifcu;

#define	svr4_ifc_buf	ifc_ifcu.ifcu_buf
#define	svr4_ifc_req	ifc_ifcu.ifcu_req
};

#define SVR4_SIOC	('i' << 8)

#define	SVR4_SIOCGIFFLAGS	SVR4_IOWR('i', 17, struct svr4_ifreq)
#define	SVR4_SIOCGIFCONF	SVR4_IOWR('i', 20, struct svr4_ifconf)
#define	SVR4_SIOCGIFNUM		SVR4_IOR('i', 87, int)

#endif /* !_SVR4_SOCKIO_H_ */
