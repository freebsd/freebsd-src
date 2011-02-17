/*
 * Copyright (c) 1992, 1993
 *  The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Modified by Duncan Barclay.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)pt_tcp.c  8.3 (Berkeley) 3/27/94
 *
 * pt_tcp.c,v 1.1.1.1 1994/05/26 06:34:34 rgrimes Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "portald.h"

/*
 * Key will be tcplisten/host/port
 *
 * Create a TCP socket bound to the requested host and port.
 * If the host is "ANY" the receving address will be set to INADDR_ANY.
 * If the port is 0 the caller must find out the returned port number
 * using a call to getsockname.
 *
 * XXX!  The owner of the socket will be root rather then the user.  This
 * 	 may cause remote auth (identd) to return unexpected results.
 *
 */
int portal_tcplisten(struct portal_cred *pcr, char *key, char **v,
    int kso __unused, int *fdp)
{
       char host[MAXHOSTNAMELEN];
       char port[MAXHOSTNAMELEN];
       char *p = key + (v[1] ? strlen(v[1]) : 0);
       char *q;
       struct hostent *hp;
       struct servent *sp;
       struct in_addr **ipp = NULL;
       struct in_addr *ip[2];
       struct in_addr ina;
       u_short s_port;
       int any = 0;
       struct sockaddr_in sain;

       q = strchr(p, '/');
       if (q == 0 || q - p >= (int)sizeof(host))
               return (EINVAL);
       *q = '\0';
       snprintf(host, sizeof(host), "%s", p);
       p = q + 1;

       q = strchr(p, '/');
       if (q)
               *q = '\0';
       if (strlen(p) >= sizeof(port))
               return (EINVAL);
       snprintf(port, sizeof(port), "%s", p);

       if (strcmp(host, "ANY") == 0) {
               any = 1;
       } else {
               hp = gethostbyname(host);
               if (hp != 0) {
                       ipp = (struct in_addr **) hp->h_addr_list;
               } else {
                       ina.s_addr = inet_addr(host);
                       if (ina.s_addr == INADDR_NONE)
                               return (EINVAL);
                       ip[0] = &ina;
                       ip[1] = 0;
                       ipp = ip;
               }
       }
#ifdef DEBUG
       if (any)
               printf("INADDR_ANY to be used for hostname\n");
       else
               printf("inet address for %s is %s\n", host, inet_ntoa(*ipp[0]));
#endif

       sp = getservbyname(port, "tcp");
       if (sp != NULL) {
               s_port = (u_short) sp->s_port;
        } else {
               s_port = strtoul(port, &p, 0);
               if (*p != '\0')
                       return (EINVAL);
               s_port = htons(s_port);
       }
       if ((ntohs(s_port) != 0) &&
           (ntohs(s_port) <= IPPORT_RESERVED) &&
           (pcr->pcr_uid != 0))
               return (EPERM);
#ifdef DEBUG
       printf("port number for %s is %d\n", port, ntohs(s_port));
#endif

       memset(&sain, 0, sizeof(sain));
       sain.sin_len = sizeof(sain);
       sain.sin_family = AF_INET;
       sain.sin_port = s_port;

       if (any) {
               int so;
               int sock;

               so = socket(AF_INET, SOCK_STREAM, 0);
               if (so < 0) {
                       syslog(LOG_ERR, "socket: %m");
                       return (errno);
               }

               sain.sin_addr.s_addr = INADDR_ANY;
               if (bind(so, (struct sockaddr *) &sain, sizeof(sain)) == 0) {
                       listen(so, 1);
                       if ((sock = accept(so, (struct sockaddr *)0, (int *)0)) == -1) {
                               syslog(LOG_ERR, "accept: %m");
                               (void) close(so);
                               return (errno);
                       }
                       *fdp = sock;
                       (void) close(so);
                       return (0);
               }
               syslog(LOG_ERR, "bind: %m");
               (void) close(so);
               return (errno);
       }

       while (ipp[0]) {
               int so;
               int sock;

               so = socket(AF_INET, SOCK_STREAM, 0);
               if (so < 0) {
                       syslog(LOG_ERR, "socket: %m");
                       return (errno);
               }

               sain.sin_addr = *ipp[0];
               if (bind(so, (struct sockaddr *) &sain, sizeof(sain)) == 0) {
                       listen(so, 1);
                       if ((sock = accept(so, (struct sockaddr *)0, (int *)0)) == -1) {
                               syslog(LOG_ERR, "accept: %m");
                               (void) close(so);
                               return (errno);
                       }
                       *fdp = sock;
                       (void) close(so);
                       return (0);
               }
               (void) close(so);

               ipp++;
       }

       syslog(LOG_ERR, "bind: %m");
       return (errno);

}
