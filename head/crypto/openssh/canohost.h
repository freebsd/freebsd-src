/* $OpenBSD: canohost.h,v 1.11 2009/05/27 06:31:25 andreas Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

const char	*get_canonical_hostname(int);
const char	*get_remote_ipaddr(void);
const char	*get_remote_name_or_ip(u_int, int);

char		*get_peer_ipaddr(int);
int		 get_peer_port(int);
char		*get_local_ipaddr(int);
char		*get_local_name(int);

int		 get_remote_port(void);
int		 get_local_port(void);
int		 get_sock_port(int, int);
void		 clear_cached_addr(void);

void		 ipv64_normalise_mapped(struct sockaddr_storage *, socklen_t *);
