/*-
 * Copyright (c) 2004 Poul-Henning Kamp <phk@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/libalias/alias_old.c,v 1.8 2006/09/26 23:26:53 piso Exp $");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/proc.h>
#else
#include <sys/types.h>
#include <stdlib.h>
#endif

#include <netinet/in.h>

#ifdef _KERNEL
#include <netinet/libalias/alias.h>
#else
#include "alias.h"
#endif

/*
 * These functions are for backwards compatibility and because apps may
 * be linked against shlib versions, they have to be actual functions,
 * we cannot inline them.
 */

static struct libalias *la;

void
PacketAliasInit(void)
{

	la = LibAliasInit(la);
}

void
PacketAliasSetAddress(struct in_addr _addr)
{

	LibAliasSetAddress(la, _addr);
}

void
PacketAliasSetFWBase(unsigned int _base, unsigned int _num)
{

	LibAliasSetFWBase(la, _base, _num);
}

void
PacketAliasSetSkinnyPort(unsigned int _port)
{

	LibAliasSetSkinnyPort(la, _port);
}

unsigned int
PacketAliasSetMode(unsigned int _flags, unsigned int _mask)
{

	return LibAliasSetMode(la, _flags, _mask);
}

void
PacketAliasUninit(void)
{

	LibAliasUninit(la);
	la = NULL;
}

int
PacketAliasIn(char *_ptr, int _maxpacketsize)
{
	return LibAliasIn(la, _ptr, _maxpacketsize);
}

int
PacketAliasOut(char *_ptr, int _maxpacketsize)
{

	return LibAliasOut(la, _ptr, _maxpacketsize);
}

int
PacketUnaliasOut(char *_ptr, int _maxpacketsize)
{

	return LibAliasUnaliasOut(la, _ptr, _maxpacketsize);
}

int
PacketAliasAddServer(struct alias_link *_lnk,
    struct in_addr _addr, unsigned short _port)
{

	return LibAliasAddServer(la, _lnk, _addr, _port);
}

struct alias_link *
PacketAliasRedirectAddr(struct in_addr _src_addr,
    struct in_addr _alias_addr)
{

	return LibAliasRedirectAddr(la, _src_addr, _alias_addr);
}


int
PacketAliasRedirectDynamic(struct alias_link *_lnk)
{

	return LibAliasRedirectDynamic(la, _lnk);
}

void
PacketAliasRedirectDelete(struct alias_link *_lnk)
{

	LibAliasRedirectDelete(la, _lnk);
}

struct alias_link *
PacketAliasRedirectPort(struct in_addr _src_addr,
    unsigned short _src_port, struct in_addr _dst_addr,
    unsigned short _dst_port, struct in_addr _alias_addr,
    unsigned short _alias_port, unsigned char _proto)
{

	return LibAliasRedirectPort(la, _src_addr, _src_port, _dst_addr,
	    _dst_port, _alias_addr, _alias_port, _proto);
}

struct alias_link *
PacketAliasRedirectProto(struct in_addr _src_addr,
    struct in_addr _dst_addr, struct in_addr _alias_addr,
    unsigned char _proto)
{

	return LibAliasRedirectProto(la, _src_addr, _dst_addr, _alias_addr,
	    _proto);
}

void
PacketAliasFragmentIn(char *_ptr, char *_ptr_fragment)
{

	LibAliasFragmentIn(la, _ptr, _ptr_fragment);
}

char           *
PacketAliasGetFragment(char *_ptr)
{

	return LibAliasGetFragment(la, _ptr);
}

int
PacketAliasSaveFragment(char *_ptr)
{
	return LibAliasSaveFragment(la, _ptr);
}

int
PacketAliasCheckNewLink(void)
{

	return LibAliasCheckNewLink(la);
}

unsigned short
PacketAliasInternetChecksum(unsigned short *_ptr, int _nbytes)
{

	return LibAliasInternetChecksum(la, _ptr, _nbytes);
}

void
PacketAliasSetTarget(struct in_addr _target_addr)
{

	LibAliasSetTarget(la, _target_addr);
}

/* Transparent proxying routines. */
int
PacketAliasProxyRule(const char *_cmd)
{

	return LibAliasProxyRule(la, _cmd);
}
