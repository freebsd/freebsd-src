/*
 * Copyright (c) 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Id: traceroute.h,v 1.1 2000/11/23 20:06:54 leres Exp $ (LBL)
 */

#ifndef	TRACEROUTE_H_INCLUDED
#define	TRACEROUTE_H_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#define MAX_GATEWAYS 127
extern const char *gateways[];
extern int ngateways;

extern int as_path;
extern char *as_server;
extern int options;
extern int ecnflag;
extern int first_ttl;
extern int max_ttl;
extern int nflag;
extern int nprobes;
extern int waittime;
extern int verbose;
extern int packlen;
extern int printdiff;
extern int fixedPort;
extern int sump;
extern int tos;
extern int doipcksum;
extern int Iflag;
extern int Nflag;
extern int Sflag;
extern int Tflag;
extern int Uflag;
extern int requestPort;
extern unsigned int pausemsecs;
extern unsigned short off;
extern char *source;
extern char *prog;
extern char *protoname;
extern char *hostname;
extern char *device;

int	traceroute4(struct sockaddr *);
int	traceroute6(struct sockaddr *);

void	setsin(struct sockaddr_in *, u_int32_t);
int	str2val(const char *str, const char *what, int mi, int ma);

#endif	/* !TRACEROUTE_H_INCLUDED */
