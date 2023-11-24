/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Dmitry Chagin <dchagin@FreeBSD.org>
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

#ifndef _LINUX_COMMON_H_
#define _LINUX_COMMON_H_

int	ifname_bsd_to_linux_ifp(struct ifnet *, char *, size_t);
int	ifname_bsd_to_linux_idx(u_int, char *, size_t);
int	ifname_bsd_to_linux_name(const char *, char *, size_t);
struct ifnet *ifname_linux_to_ifp(struct thread *, const char *);
int	ifname_linux_to_bsd(struct thread *, const char *, char *);

unsigned short	linux_ifflags(struct ifnet *);
int		linux_ifhwaddr(struct ifnet *ifp, struct l_sockaddr *lsa);

unsigned short	bsd_to_linux_ifflags(int);
int		linux_to_bsd_domain(int domain);
int		bsd_to_linux_domain(int domain);
int		bsd_to_linux_sockaddr(const struct sockaddr *sa,
		    struct l_sockaddr **lsa, socklen_t len);
int		linux_to_bsd_sockaddr(const struct l_sockaddr *lsa,
		    struct sockaddr **sap, socklen_t *len);
void		linux_to_bsd_poll_events(struct thread *td, int fd,
		    short lev, short *bev);
void		bsd_to_linux_poll_events(short bev, short *lev);

#endif /* _LINUX_COMMON_H_ */
