/*	$KAME: if.h,v 1.2 2000/05/16 13:34:13 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/rtadvd/if.h,v 1.1.2.1 2000/07/15 07:36:56 kris Exp $
 */

#define RTADV_TYPE2BITMASK(type) (0x1 << type)

extern struct if_msghdr **iflist;
extern size_t ifblock_size;
extern char *ifblock;

struct nd_opt_hdr;
struct sockaddr_dl *if_nametosdl __P((char *name));
int if_getmtu __P((char *name));
int if_getflags __P((int ifindex, int oifflags));
int lladdropt_length __P((struct sockaddr_dl *sdl));
void lladdropt_fill __P((struct sockaddr_dl *sdl, struct nd_opt_hdr *ndopt));
int rtbuf_len __P((void));
int get_rtinfo __P((char *buf, size_t *len));
char *get_next_msg __P((char *buf, char *lim, int ifindex, size_t *lenp,
			   int filter));
struct in6_addr *get_addr __P((char *buf));
int get_rtm_ifindex __P((char *buf));
int get_ifm_ifindex __P((char *buf));
int get_ifam_ifindex __P((char *buf));
int get_ifm_flags __P((char *buf));
int get_prefixlen __P((char *buf));
int rtmsg_type __P((char *buf));
int ifmsg_type __P((char *buf));
int rtmsg_len __P((char *buf));
int ifmsg_len __P((char *buf));
void init_iflist __P((void));
