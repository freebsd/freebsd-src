/* lint -save -library Flexelint comment for external headers */

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

/*-
 * Alias.h defines the outside world interfaces for the packet aliasing
 * software.
 * 
 * This software is placed into the public domain with no restrictions on its
 * distribution.
 */

#ifndef _ALIAS_H_
#define	_ALIAS_H_

/* The external interface to libalias, the packet aliasing engine. */

/* Initialization and control functions. */
void	 PacketAliasInit(void);
void	 PacketAliasSetAddress(struct in_addr _addr);
void	 PacketAliasSetFWBase(unsigned int _base, unsigned int _num);
unsigned int
	 PacketAliasSetMode(unsigned int _flags, unsigned int _mask);
void	 PacketAliasUninit(void);

/* Packet Handling functions. */
int	 PacketAliasIn(char *_ptr, int _maxpacketsize);
int	 PacketAliasOut(char *_ptr, int _maxpacketsize);
int	 PacketUnaliasOut(char *_ptr, int _maxpacketsize);

/* Port and address redirection functions. */

/*
 * An anonymous structure, a pointer to which is returned from
 * PacketAliasRedirectAddr(), PacketAliasRedirectPort() or
 * PacketAliasRedirectProto(), passed to PacketAliasAddServer(),
 * and freed by PacketAliasRedirectDelete().
 */
struct	alias_link;

int	 PacketAliasAddServer(struct alias_link *_link,
	    struct in_addr _addr, unsigned short _port);
struct alias_link *
	 PacketAliasRedirectAddr(struct in_addr _src_addr,
	    struct in_addr _alias_addr);
void	 PacketAliasRedirectDelete(struct alias_link *_link);
struct alias_link *
	 PacketAliasRedirectPort(struct in_addr _src_addr,
	    unsigned short _src_port, struct in_addr _dst_addr,
	    unsigned short _dst_port, struct in_addr _alias_addr,
	    unsigned short _alias_port, unsigned char _proto);
struct alias_link *
	 PacketAliasRedirectProto(struct in_addr _src_addr,
	    struct in_addr _dst_addr, struct in_addr _alias_addr,
	    unsigned char _proto);

/* Fragment Handling functions. */
void	 PacketAliasFragmentIn(char *_ptr, char *_ptr_fragment);
char	*PacketAliasGetFragment(char *_ptr);
int	 PacketAliasSaveFragment(char *_ptr);

/* Miscellaneous functions. */
int	 PacketAliasCheckNewLink(void);
unsigned short
	 PacketAliasInternetChecksum(unsigned short *_ptr, int _nbytes);
void	 PacketAliasSetTarget(struct in_addr _target_addr);

/* Transparent proxying routines. */
int	 PacketAliasProxyRule(const char *_cmd);

/* Mode flags, set using PacketAliasSetMode() */

/*
 * If PKT_ALIAS_LOG is set, a message will be printed to /var/log/alias.log
 * every time a link is created or deleted.  This is useful for debugging.
 */
#define	PKT_ALIAS_LOG			0x01

/*
 * If PKT_ALIAS_DENY_INCOMING is set, then incoming connections (e.g. to ftp,
 * telnet or web servers will be prevented by the aliasing mechanism.
 */
#define	PKT_ALIAS_DENY_INCOMING		0x02

/*
 * If PKT_ALIAS_SAME_PORTS is set, packets will be attempted sent from the
 * same port as they originated on.  This allows e.g. rsh to work *99% of the
 * time*, but _not_ 100% (it will be slightly flakey instead of not working
 * at all).  This mode bit is set by PacketAliasInit(), so it is a default
 * mode of operation.
 */
#define	PKT_ALIAS_SAME_PORTS		0x04

/*
 * If PKT_ALIAS_USE_SOCKETS is set, then when partially specified links (e.g.
 * destination port and/or address is zero), the packet aliasing engine will
 * attempt to allocate a socket for the aliasing port it chooses.  This will
 * avoid interference with the host machine.  Fully specified links do not
 * require this.  This bit is set after a call to PacketAliasInit(), so it is
 * a default mode of operation.
 */
#define	PKT_ALIAS_USE_SOCKETS		0x08

/*-
 * If PKT_ALIAS_UNREGISTERED_ONLY is set, then only packets with
 * unregistered source addresses will be aliased.  Private
 * addresses are those in the following ranges:
 *
 *		10.0.0.0     ->   10.255.255.255
 *		172.16.0.0   ->   172.31.255.255
 *		192.168.0.0  ->   192.168.255.255
 */
#define	PKT_ALIAS_UNREGISTERED_ONLY	0x10

/*
 * If PKT_ALIAS_RESET_ON_ADDR_CHANGE is set, then the table of dynamic
 * aliasing links will be reset whenever PacketAliasSetAddress() changes the
 * default aliasing address.  If the default aliasing address is left
 * unchanged by this function call, then the table of dynamic aliasing links
 * will be left intact.  This bit is set after a call to PacketAliasInit().
 */
#define	PKT_ALIAS_RESET_ON_ADDR_CHANGE	0x20

#ifndef NO_FW_PUNCH
/*
 * If PKT_ALIAS_PUNCH_FW is set, active FTP and IRC DCC connections will
 * create a 'hole' in the firewall to allow the transfers to work.  The
 * ipfw rule number that the hole is created with is controlled by
 * PacketAliasSetFWBase().  The hole will be attached to that
 * particular alias_link, so when the link goes away the hole is deleted.
 */
#define	PKT_ALIAS_PUNCH_FW		0x100
#endif

/*
 * If PKT_ALIAS_PROXY_ONLY is set, then NAT will be disabled and only
 * transparent proxying is performed.
 */
#define	PKT_ALIAS_PROXY_ONLY		0x40

/*
 * If PKT_ALIAS_REVERSE is set, the actions of PacketAliasIn() and
 * PacketAliasOut() are reversed.
 */
#define	PKT_ALIAS_REVERSE		0x80

/* Function return codes. */
#define	PKT_ALIAS_ERROR			-1
#define	PKT_ALIAS_OK			1
#define	PKT_ALIAS_IGNORED		2
#define	PKT_ALIAS_UNRESOLVED_FRAGMENT	3
#define	PKT_ALIAS_FOUND_HEADER_FRAGMENT	4

#endif /* !_ALIAS_H_ */

/* lint -restore */
