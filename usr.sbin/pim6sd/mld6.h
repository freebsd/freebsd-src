/*
 * Copyright (C) 1998 WIDE Project.
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
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/mld6.h,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */


#ifndef MLD6_H
#define MLD6_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define RECV_BUF_SIZE			64*1024
#define SO_RECV_BUF_SIZE_MAX	256*1024
#define SO_RECV_BUF_SIZE_MIN	48*1024
#define MINHLIM							1


/*
 * Constans for Multicast Listener Discovery protocol for IPv6.
 */
#define MLD6_ROBUSTNESS_VARIABLE        2
#define MLD6_QUERY_INTERVAL 125 /* in seconds */
#define MLD6_QUERY_RESPONSE_INTERVAL 10000 /* in milliseconds */
#ifndef MLD6_TIMER_SCALE
#define MLD6_TIMER_SCALE 1000

#endif
#define MLD6_LISTENER_INTERVAL (MLD6_ROBUSTNESS_VARIABLE * \
                MLD6_QUERY_INTERVAL + \
                MLD6_QUERY_RESPONSE_INTERVAL / MLD6_TIMER_SCALE)
#define MLD6_LAST_LISTENER_QUERY_INTERVAL   1000 /* in milliseconds */
#define MLD6_LAST_LISTENER_QUERY_COUNT      MLD6_ROBUSTNESS_VARIABLE
#define MLD6_OTHER_QUERIER_PRESENT_INTERVAL (MLD6_ROBUSTNESS_VARIABLE * \
		MLD6_QUERY_INTERVAL + \
		MLD6_QUERY_RESPONSE_INTERVAL / (2 * MLD6_TIMER_SCALE))

extern int mld6_socket;
extern char *mld6_recv_buf;
extern struct sockaddr_in6 allrouters_group;
extern struct sockaddr_in6 allnodes_group;
extern char *mld6_send_buf;

void init_mld6 __P((void));
void send_mld6 __P((int type, int code, struct sockaddr_in6 *src,
		    struct sockaddr_in6 *dst, struct in6_addr *group,
		    int index, int delay, int datalen, int alert));

#endif
