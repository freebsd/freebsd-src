/*	$FreeBSD: src/lib/libipsec/libpfkey.h,v 1.1.2.1 2000/07/15 07:24:04 kris Exp $	*/
/*	$KAME: libpfkey.h,v 1.1 2000/06/08 21:28:32 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 */

extern void pfkey_sadump __P((struct sadb_msg *));
extern void pfkey_spdump __P((struct sadb_msg *));

struct sockaddr;
int ipsec_check_keylen __P((u_int, u_int, u_int));
u_int pfkey_set_softrate __P((u_int, u_int));
u_int pfkey_get_softrate __P((u_int));
int pfkey_send_getspi __P((int, u_int, u_int, struct sockaddr *,
	struct sockaddr *, u_int32_t, u_int32_t, u_int32_t, u_int32_t));
int pfkey_send_update __P((int, u_int, u_int, struct sockaddr *,
	struct sockaddr *, u_int32_t, u_int32_t, u_int,
	caddr_t, u_int, u_int, u_int, u_int, u_int, u_int32_t, u_int64_t,
	u_int64_t, u_int64_t, u_int32_t));
int pfkey_send_add __P((int, u_int, u_int, struct sockaddr *,
	struct sockaddr *, u_int32_t, u_int32_t, u_int,
	caddr_t, u_int, u_int, u_int, u_int, u_int, u_int32_t, u_int64_t,
	u_int64_t, u_int64_t, u_int32_t));
int pfkey_send_delete __P((int, u_int, u_int,
	struct sockaddr *, struct sockaddr *, u_int32_t));
int pfkey_send_get __P((int, u_int, u_int,
	struct sockaddr *, struct sockaddr *, u_int32_t));
int pfkey_send_register __P((int, u_int));
int pfkey_recv_register __P((int));
int pfkey_send_flush __P((int, u_int));
int pfkey_send_dump __P((int, u_int));
int pfkey_send_promisc_toggle __P((int, int));
int pfkey_send_spdadd __P((int, struct sockaddr *, u_int,
	struct sockaddr *, u_int, u_int, caddr_t, int, u_int32_t));
int pfkey_send_spdupdate __P((int, struct sockaddr *, u_int,
	struct sockaddr *, u_int, u_int, caddr_t, int, u_int32_t));
int pfkey_send_spddelete __P((int, struct sockaddr *, u_int,
	struct sockaddr *, u_int, u_int, caddr_t, int, u_int32_t));
int pfkey_send_spddelete2 __P((int, u_int32_t));
int pfkey_send_spdget __P((int, u_int32_t));
int pfkey_send_spdsetidx __P((int, struct sockaddr *, u_int,
	struct sockaddr *, u_int, u_int, caddr_t, int, u_int32_t));
int pfkey_send_spdflush __P((int));
int pfkey_send_spddump __P((int));

int pfkey_open __P((void));
void pfkey_close __P((int));
struct sadb_msg *pfkey_recv __P((int));
int pfkey_send __P((int, struct sadb_msg *, int));
int pfkey_align __P((struct sadb_msg *, caddr_t *));
int pfkey_check __P((caddr_t *));
