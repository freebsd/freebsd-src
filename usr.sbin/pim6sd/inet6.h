/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 * $FreeBSD: src/usr.sbin/pim6sd/inet6.h,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */


#ifndef INET6_H
#define INET6_H
#include "vif.h"

extern int numerichost;

extern int  inet6_equal __P((struct sockaddr_in6 *sa1,
                     struct sockaddr_in6 *sa2)); 
extern int  inet6_lessthan  __P((struct sockaddr_in6 *sa1,
                     struct sockaddr_in6 *sa2));
extern int  inet6_localif_address __P((struct sockaddr_in6 *sa,
                       struct uvif *v));
extern int  inet6_greaterthan __P((struct sockaddr_in6 *sa1,
                       struct sockaddr_in6 *sa2));
extern int  inet6_match_prefix __P((struct sockaddr_in6 *sa1,
                    struct sockaddr_in6 *sa2,
                    struct in6_addr *mask));
extern int  inet6_mask2plen    __P((struct in6_addr *mask));
extern int  inet6_uvif2scopeid __P((struct sockaddr_in6 *sa, struct uvif *v));
extern int  inet6_valid_host __P((struct sockaddr_in6 *addr));
extern char *sa6_fmt  __P((struct sockaddr_in6 *sa6));
extern char *inet6_fmt  __P((struct in6_addr *addr));
extern char *ifindex2str    __P((int ifindex));
extern char *net6name   __P((struct in6_addr *prefix, 
                     struct in6_addr *mask));



#endif
