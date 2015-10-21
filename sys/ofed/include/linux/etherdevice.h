/*-
 * Copyright (c) 2015 Mellanox Technologies, Ltd. All rights reserved.
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
#ifndef _LINUX_ETHERDEVICE
#define	_LINUX_ETHERDEVICE

#include <linux/types.h>

static inline bool 
is_zero_ether_addr(const u8 * addr)
{
	return ((addr[0] + addr[1] + addr[2] + addr[3] + addr[4] + addr[5]) == 0x00);
}

static inline bool 
is_multicast_ether_addr(const u8 * addr)
{
	return (0x01 & addr[0]);
}

static inline bool 
is_broadcast_ether_addr(const u8 * addr)
{
	return ((addr[0] + addr[1] + addr[2] + addr[3] + addr[4] + addr[5]) == (6 * 0xff));
}

static inline bool 
is_valid_ether_addr(const u8 * addr)
{
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

static inline void 
ether_addr_copy(u8 * dst, const u8 * src)
{
	memcpy(dst, src, 6);
}

#endif					/* _LINUX_ETHERDEVICE */
