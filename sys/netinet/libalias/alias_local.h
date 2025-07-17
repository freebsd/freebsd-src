/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
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
 */

/*
 * Alias_local.h contains the function prototypes for alias.c,
 * alias_db.c, alias_util.c and alias_ftp.c, alias_irc.c (as well
 * as any future add-ons).  It also includes macros, globals and
 * struct definitions shared by more than one alias*.c file.
 *
 * This include file is intended to be used only within the aliasing
 * software.  Outside world interfaces are defined in alias.h
 *
 * This software is placed into the public domain with no restrictions
 * on its distribution.
 *
 * Initial version:  August, 1996  (cjm)
 *
 * <updated several times by original author and Eivind Eklund>
 */

#ifndef _ALIAS_LOCAL_H_
#define	_ALIAS_LOCAL_H_

#include <sys/tree.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#ifdef _KERNEL
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>

/* XXX: LibAliasSetTarget() uses this constant. */
#define	INADDR_NONE	0xffffffff

#include <netinet/libalias/alias_sctp.h>
#else
#include "alias_sctp.h"
#endif

/* Sizes of input and output link tables */
#define	GET_ALIAS_PORT		-1
#define	GET_ALIAS_ID		GET_ALIAS_PORT

#ifdef _KERNEL
#define INET_NTOA_BUF(buf) (buf)
#else
#define INET_NTOA_BUF(buf) (buf), sizeof(buf)
#endif

struct proxy_entry;

struct group_in {
	struct in_addr	alias_addr;
	u_short		alias_port;
	int		link_type;
	SPLAY_ENTRY(group_in)	in;
	LIST_HEAD(, alias_link)	full, partial;
};

struct libalias {
	LIST_ENTRY(libalias) instancelist;
	/* Mode flags documented in alias.h */
	int		packetAliasMode;
	/* Address written onto source field of IP packet. */
	struct in_addr	aliasAddress;
	/* IP address incoming packets are sent to
	 * if no aliasing link already exists */
	struct in_addr	targetAddress;
	/* Lookup table of pointers to chains of link records.
	 * Each link record is indexed into input,
	 * output and "internal endpoint" lookup tables. */
	SPLAY_HEAD(splay_out, alias_link) linkSplayOut;
	SPLAY_HEAD(splay_in,  group_in)   linkSplayIn;
	SPLAY_HEAD(splay_internal_endpoint, alias_link)
	    linkSplayInternalEndpoint;
	LIST_HEAD (, alias_link) pptpList;
	/* HouseKeeping */
	TAILQ_HEAD    (, alias_link) checkExpire;
	/* Link statistics */
	unsigned int	icmpLinkCount;
	unsigned int	udpLinkCount;
	unsigned int	tcpLinkCount;
	unsigned int	pptpLinkCount;
	unsigned int	protoLinkCount;
	unsigned int	fragmentIdLinkCount;
	unsigned int	fragmentPtrLinkCount;
	unsigned int	sockCount;
	/* log descriptor */
#ifdef _KERNEL
	char	       *logDesc;
#else
	FILE	       *logDesc;
#endif

#ifndef NO_FW_PUNCH
	/* File descriptor to be able to control firewall.
	 * Opened by PacketAliasSetMode on first setting
	 * the PKT_ALIAS_PUNCH_FW flag. */
	int		fireWallFD;
	/* The first firewall entry free for our use */
	int		fireWallBaseNum;
	/* How many entries can we use? */
	int		fireWallNumNums;
	/* Which entry did we last use? */
	int		fireWallActiveNum;
	/* bool array for entries */
	char	       *fireWallField;
#endif
	/* TCP port used by the Skinny protocol. */
	unsigned int	skinnyPort;

	struct proxy_entry *proxyList;

	struct in_addr	true_addr;	/* in network byte order. */
	u_short		true_port;	/* in host byte order. */

	/* Port ranges for aliasing. */
	u_short		aliasPortLower;
	u_short		aliasPortLength;

	/*
	 * sctp code support
	 */

	/* counts associations that have progressed to UP and not yet removed */
	int		sctpLinkCount;
#ifdef _KERNEL
	/* timing queue for keeping track of association timeouts */
	struct sctp_nat_timer sctpNatTimer;
	/* size of hash table used in this instance */
	u_int sctpNatTableSize;
	/* local look up table sorted by l_vtag/l_port */
	LIST_HEAD(sctpNatTableL, sctp_nat_assoc) *sctpTableLocal;
	/* global look up table sorted by g_vtag/g_port */
	LIST_HEAD(sctpNatTableG, sctp_nat_assoc) *sctpTableGlobal;

	/* avoid races in libalias: every public function has to use it. */
	struct mtx mutex;
#endif
};

/* Macros */

#ifdef _KERNEL
#define LIBALIAS_LOCK_INIT(l) \
	mtx_init(&l->mutex, "per-instance libalias mutex", NULL, MTX_DEF)
#define LIBALIAS_LOCK_ASSERT(l) mtx_assert(&l->mutex, MA_OWNED)
#define LIBALIAS_LOCK(l) mtx_lock(&l->mutex)
#define LIBALIAS_UNLOCK(l) mtx_unlock(&l->mutex)
#define LIBALIAS_LOCK_DESTROY(l)	mtx_destroy(&l->mutex)
#else
#define LIBALIAS_LOCK_INIT(l)
#define LIBALIAS_LOCK_ASSERT(l)
#define LIBALIAS_LOCK(l)
#define LIBALIAS_UNLOCK(l)
#define LIBALIAS_LOCK_DESTROY(l)
#endif

/*
 * The following macro is used to update an
 * internet checksum.  "delta" is a 32-bit
 * accumulation of all the changes to the
 * checksum (adding in new 16-bit words and
 * subtracting out old words), and "cksum"
 * is the checksum value to be updated.
 */
#define	ADJUST_CHECKSUM(acc, cksum) \
	do { \
		acc += cksum; \
		if (acc < 0) { \
			acc = -acc; \
			acc = (acc >> 16) + (acc & 0xffff); \
			acc += acc >> 16; \
			cksum = (u_short) ~acc; \
		} else { \
			acc = (acc >> 16) + (acc & 0xffff); \
			acc += acc >> 16; \
			cksum = (u_short) acc; \
		} \
	} while (0)

/* Prototypes */

/* System time in seconds for current packet */
extern int LibAliasTime;

/*
 * SctpFunction prototypes
 *
 */
void AliasSctpInit(struct libalias *la);
void AliasSctpTerm(struct libalias *la);
int SctpAlias(struct libalias *la, struct ip *ip, int direction);

/*
 * We do not calculate TCP checksums when libalias is a kernel
 * module, since it has no idea about checksum offloading.
 * If TCP data has changed, then we just set checksum to zero,
 * and caller must recalculate it himself.
 * In case if libalias will edit UDP data, the same approach
 * should be used.
 */
#ifndef _KERNEL
u_short		IpChecksum(struct ip *_pip);
u_short		TcpChecksum(struct ip *_pip);
#endif
void
DifferentialChecksum(u_short * _cksum, void * _new, void * _old, int _n);

/* Internal data access */
struct alias_link *
AddLink(struct libalias *la, struct in_addr src_addr, struct in_addr dst_addr,
    struct in_addr alias_addr, u_short src_port, u_short dst_port,
    int alias_param, int link_type);
int
FindIcmpIn(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _id_alias, int _create, struct alias_link **_lnkp);
int
FindIcmpOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_short _id, int _create, struct alias_link **_lnkp);
struct alias_link *
FindFragmentIn1(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _ip_id);
struct alias_link *
FindFragmentIn2(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _ip_id);
struct alias_link *
AddFragmentPtrLink(struct libalias *la, struct in_addr _dst_addr, u_short _ip_id);
struct alias_link *
FindFragmentPtr(struct libalias *la, struct in_addr _dst_addr, u_short _ip_id);
int
FindProtoIn(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_char _proto, struct alias_link **_lnkp);
int
FindProtoOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_char _proto, struct alias_link **_lnkp);
int
FindUdpTcpIn(struct libalias *la, struct in_addr _dst_addr, struct in_addr _alias_addr,
    u_short _dst_port, u_short _alias_port, u_char _proto, int _create,
    struct alias_link **_lnkp);
int
FindUdpTcpOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_short _src_port, u_short _dst_port, u_char _proto, int _create,
    struct alias_link **_lnkp);
struct alias_link *
AddPptp(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    struct in_addr _alias_addr, u_int16_t _src_call_id);
struct alias_link *
FindPptpOutByCallId(struct libalias *la, struct in_addr _src_addr,
    struct in_addr _dst_addr, u_int16_t _src_call_id);
struct alias_link *
FindPptpInByCallId(struct libalias *la, struct in_addr _dst_addr,
    struct in_addr _alias_addr, u_int16_t _dst_call_id);
struct alias_link *
FindPptpOutByPeerCallId(struct libalias *la, struct in_addr _src_addr,
    struct in_addr _dst_addr, u_int16_t _dst_call_id);
struct alias_link *
FindPptpInByPeerCallId(struct libalias *la, struct in_addr _dst_addr,
    struct in_addr _alias_addr, u_int16_t _alias_call_id);
struct alias_link *
FindRtspOut(struct libalias *la, struct in_addr _src_addr, struct in_addr _dst_addr,
    u_short _src_port, u_short _alias_port, u_char _proto);
struct in_addr
FindOriginalAddress(struct libalias *la, struct in_addr _alias_addr);
struct in_addr
FindAliasAddress(struct libalias *la, struct in_addr _original_addr);
struct in_addr
FindSctpRedirectAddress(struct libalias *la,  struct sctp_nat_msg *sm);

/* External data access/modification */
int		FindNewPortGroup(struct libalias *la, struct in_addr _dst_addr,
		    struct in_addr _alias_addr, u_short _src_port,
		    u_short _dst_port, u_short _port_count, u_char _proto,
		    u_char _align);
void		GetFragmentAddr(struct alias_link *_lnk, struct in_addr *_src_addr);
void		SetFragmentAddr(struct alias_link *_lnk, struct in_addr _src_addr);
void		GetFragmentPtr(struct alias_link *_lnk, void **_fptr);
void		SetFragmentPtr(struct alias_link *_lnk, void *fptr);
void		SetStateIn(struct alias_link *_lnk, int _state);
void		SetStateOut(struct alias_link *_lnk, int _state);
int		GetStateIn (struct alias_link *_lnk);
int		GetStateOut(struct alias_link *_lnk);
struct in_addr	GetOriginalAddress(struct alias_link *_lnk);
struct in_addr	GetDestAddress(struct alias_link *_lnk);
struct in_addr	GetAliasAddress(struct alias_link *_lnk);
struct in_addr	GetDefaultAliasAddress(struct libalias *la);
void		SetDefaultAliasAddress(struct libalias *la, struct in_addr _alias_addr);
u_short		GetOriginalPort(struct alias_link *_lnk);
u_short		GetAliasPort(struct alias_link *_lnk);
struct in_addr	GetProxyAddress(struct alias_link *_lnk);
void		SetProxyAddress(struct alias_link *_lnk, struct in_addr _addr);
u_short		GetProxyPort(struct alias_link *_lnk);
void		SetProxyPort(struct alias_link *_lnk, u_short _port);
void		SetAckModified(struct alias_link *_lnk);
int		GetAckModified(struct alias_link *_lnk);
int		GetDeltaAckIn(u_long, struct alias_link *_lnk);
int		GetDeltaSeqOut(u_long, struct alias_link *lnk);
void		AddSeq(struct alias_link *lnk, int delta, u_int ip_hl,
		    u_short ip_len, u_long th_seq, u_int th_off);
void		SetExpire (struct alias_link *_lnk, int _expire);
void		SetProtocolFlags(struct alias_link *_lnk, int _pflags);
int		GetProtocolFlags(struct alias_link *_lnk);
void		SetDestCallId(struct alias_link *_lnk, u_int16_t _cid);

#ifndef NO_FW_PUNCH
void		PunchFWHole(struct alias_link *_lnk);

#endif

/* Housekeeping function */
void		HouseKeeping(struct libalias *);

/* Transparent proxy routines */
int
ProxyCheck(struct libalias *la, struct in_addr *proxy_server_addr,
    u_short * proxy_server_port, struct in_addr src_addr,
    struct in_addr dst_addr, u_short dst_port, u_char ip_p);
void
ProxyModify(struct libalias *la, struct alias_link *_lnk, struct ip *_pip,
    int _maxpacketsize, int _proxy_type);

/* Tcp specific routines */
/* lint -save -library Suppress flexelint warnings */

enum alias_tcp_state {
	ALIAS_TCP_STATE_NOT_CONNECTED,
	ALIAS_TCP_STATE_CONNECTED,
	ALIAS_TCP_STATE_DISCONNECTED
};

#if defined(_NETINET_IP_H_)
static __inline void *
ip_next(struct ip *iphdr)
{
	char *p = (char *)iphdr;
	return (&p[iphdr->ip_hl * 4]);
}
#endif

#if defined(_NETINET_TCP_H_)
static __inline void *
tcp_next(struct tcphdr *tcphdr)
{
	char *p = (char *)tcphdr;
	return (&p[tcphdr->th_off * 4]);
}
#endif

#if defined(_NETINET_UDP_H_)
static __inline void *
udp_next(struct udphdr *udphdr)
{
	return ((void *)(udphdr + 1));
}
#endif

#endif				/* !_ALIAS_LOCAL_H_ */
