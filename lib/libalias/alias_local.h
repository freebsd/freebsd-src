/*-
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
 *
 * $FreeBSD$
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

/* Macros */

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

/* Globals */

extern int packetAliasMode;

/* Prototypes */

/* General utilities */
u_short	 IpChecksum(struct ip *_pip);
u_short	 TcpChecksum(struct ip *_pip);
void	 DifferentialChecksum(u_short *_cksum, u_short *_new, u_short *_old,
	    int _n);

/* Internal data access */
struct alias_link *
	 FindIcmpIn(struct in_addr _dst_addr, struct in_addr _alias_addr,
	    u_short _id_alias, int _create);
struct alias_link *
	 FindIcmpOut(struct in_addr _src_addr, struct in_addr _dst_addr,
	    u_short _id, int _create);
struct alias_link *
	 FindFragmentIn1(struct in_addr _dst_addr, struct in_addr _alias_addr,
	    u_short _ip_id);
struct alias_link *
	 FindFragmentIn2(struct in_addr _dst_addr, struct in_addr _alias_addr,
	    u_short _ip_id);
struct alias_link *
	 AddFragmentPtrLink(struct in_addr _dst_addr, u_short _ip_id);
struct alias_link *
	 FindFragmentPtr(struct in_addr _dst_addr, u_short _ip_id);
struct alias_link *
	 FindProtoIn(struct in_addr _dst_addr, struct in_addr _alias_addr,
	    u_char _proto);
struct alias_link *
	 FindProtoOut(struct in_addr _src_addr, struct in_addr _dst_addr,
	    u_char _proto);
struct alias_link *
	 FindUdpTcpIn(struct in_addr _dst_addr, struct in_addr _alias_addr,
	    u_short _dst_port, u_short _alias_port, u_char _proto, int _create);
struct alias_link *
	 FindUdpTcpOut(struct in_addr _src_addr, struct in_addr _dst_addr,
	    u_short _src_port, u_short _dst_port, u_char _proto, int _create);
struct alias_link *
	 AddPptp(struct in_addr _src_addr, struct in_addr _dst_addr,
	    struct in_addr _alias_addr, u_int16_t _src_call_id);
struct alias_link *
	 FindPptpOutByCallId(struct in_addr _src_addr,
	    struct in_addr _dst_addr, u_int16_t _src_call_id);
struct alias_link *
	 FindPptpInByCallId(struct in_addr _dst_addr,
	    struct in_addr _alias_addr, u_int16_t _dst_call_id);
struct alias_link *
	 FindPptpOutByPeerCallId(struct in_addr _src_addr,
	    struct in_addr _dst_addr, u_int16_t _dst_call_id);
struct alias_link *
	 FindPptpInByPeerCallId(struct in_addr _dst_addr,
	    struct in_addr _alias_addr, u_int16_t _alias_call_id);
struct alias_link *
	 FindRtspOut(struct in_addr _src_addr, struct in_addr _dst_addr,
	    u_short _src_port, u_short _alias_port, u_char _proto);
struct in_addr
	 FindOriginalAddress(struct in_addr _alias_addr);
struct in_addr
	 FindAliasAddress(struct in_addr _original_addr);

/* External data access/modification */
int	 FindNewPortGroup(struct in_addr _dst_addr, struct in_addr _alias_addr,
                     u_short _src_port, u_short _dst_port, u_short _port_count,
		     u_char _proto, u_char _align);
void	 GetFragmentAddr(struct alias_link *_link, struct in_addr *_src_addr);
void	 SetFragmentAddr(struct alias_link *_link, struct in_addr _src_addr);
void	 GetFragmentPtr(struct alias_link *_link, char **_fptr);
void	 SetFragmentPtr(struct alias_link *_link, char *fptr);
void	 SetStateIn(struct alias_link *_link, int _state);
void	 SetStateOut(struct alias_link *_link, int _state);
int	 GetStateIn(struct alias_link *_link);
int	 GetStateOut(struct alias_link *_link);
struct in_addr
	 GetOriginalAddress(struct alias_link *_link);
struct in_addr
	 GetDestAddress(struct alias_link *_link);
struct in_addr
	 GetAliasAddress(struct alias_link *_link);
struct in_addr
	 GetDefaultAliasAddress(void);
void	 SetDefaultAliasAddress(struct in_addr _alias_addr);
u_short	 GetOriginalPort(struct alias_link *_link);
u_short	 GetAliasPort(struct alias_link *_link);
struct in_addr
	 GetProxyAddress(struct alias_link *_link);
void	 SetProxyAddress(struct alias_link *_link, struct in_addr _addr);
u_short	 GetProxyPort(struct alias_link *_link);
void	 SetProxyPort(struct alias_link *_link, u_short _port);
void	 SetAckModified(struct alias_link *_link);
int	 GetAckModified(struct alias_link *_link);
int	 GetDeltaAckIn(struct ip *_pip, struct alias_link *_link);
int	 GetDeltaSeqOut(struct ip *_pip, struct alias_link *_link);
void	 AddSeq(struct ip *_pip, struct alias_link *_link, int _delta);
void	 SetExpire(struct alias_link *_link, int _expire);
void	 ClearCheckNewLink(void);
void	 SetLastLineCrlfTermed(struct alias_link *_link, int _yes);
int	 GetLastLineCrlfTermed(struct alias_link *_link);
void	 SetDestCallId(struct alias_link *_link, u_int16_t _cid);
#ifndef NO_FW_PUNCH
void	 PunchFWHole(struct alias_link *_link);
#endif

/* Housekeeping function */
void	 HouseKeeping(void);

/* Tcp specfic routines */
/* lint -save -library Suppress flexelint warnings */

/* FTP routines */
void	 AliasHandleFtpOut(struct ip *_pip, struct alias_link *_link,
	    int _maxpacketsize);

/* IRC routines */
void	 AliasHandleIrcOut(struct ip *_pip, struct alias_link *_link,
	    int _maxsize);

/* RTSP routines */
void	 AliasHandleRtspOut(struct ip *_pip, struct alias_link *_link,
	    int _maxpacketsize);

/* PPTP routines */
void	 AliasHandlePptpOut(struct ip *_pip, struct alias_link *_link);
void	 AliasHandlePptpIn(struct ip *_pip, struct alias_link *_link);
int	 AliasHandlePptpGreOut(struct ip *_pip);
int	 AliasHandlePptpGreIn(struct ip *_pip);

/* NetBIOS routines */
int	 AliasHandleUdpNbt(struct ip *_pip, struct alias_link *_link,
	    struct in_addr *_alias_address, u_short _alias_port);
int	 AliasHandleUdpNbtNS(struct ip *_pip, struct alias_link *_link,
	    struct in_addr *_alias_address, u_short *_alias_port,
	    struct in_addr *_original_address, u_short *_original_port);

/* CUSeeMe routines */
void	 AliasHandleCUSeeMeOut(struct ip *_pip, struct alias_link *_link);
void	 AliasHandleCUSeeMeIn(struct ip *_pip, struct in_addr _original_addr);

/* Transparent proxy routines */
int	 ProxyCheck(struct ip *_pip, struct in_addr *_proxy_server_addr,
	    u_short *_proxy_server_port);
void	 ProxyModify(struct alias_link *_link, struct ip *_pip,
	    int _maxpacketsize, int _proxy_type);

enum alias_tcp_state {
	ALIAS_TCP_STATE_NOT_CONNECTED,
	ALIAS_TCP_STATE_CONNECTED,
	ALIAS_TCP_STATE_DISCONNECTED
};

/*lint -restore */

#endif /* !_ALIAS_LOCAL_H_ */
