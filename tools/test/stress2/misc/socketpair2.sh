#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Default unix domain socket limits causes hang:
# 1001 880  871 0 52 0 5780 1524 keglimit D+ 0 0:00.35 /tmp/socketpair2

# Test scenario by peter@

# Fixed in r269489.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > socketpair2.c
mycc -o socketpair2 -Wall -Wextra -O2 socketpair2.c || exit
rm -f socketpair2.c

/tmp/socketpair2 > /dev/null 2>&1

rm -f /tmp/socketpair2
exit 0
EOF
/*
 Peter Wemm <peter wemm org>

 Some systems seem to base how much can be written to the pipe based
 on the size of the socket receive buffer (read-side), while others
 on the size of the socket send buffer (send-side).

 This little hack tries to make an educated guess as to what is the
 case on this particular system.
*/

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif /* !MIN */

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif /* !MAX */

#if NEED_AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif /* NEED_AF_LOCAL */

#define PACKETSIZE  (1024)

#define SEND_PIPE   (0)
#define RECV_PIPE   (1)

#define EXIT_SENDSIDE    (1)
#define EXIT_READSIDE    (0) /* looking for readside - exit 0 */
#define EXIT_UNKNOWN     (1)

static void
setsockets(const int doreverse, const size_t packetsize,
           const int s, const int r,
           size_t *sndbuf, size_t *sndbuf_set,
           size_t *rcvbuf, size_t *rcvbuf_set);

static size_t
sendtest(const int s, const char *buf, const size_t buflen);

int
main(void)
{
   size_t sent, packetcount, sndbuf, sndbuf_set, rcvbuf, rcvbuf_set;
   char buf[PACKETSIZE - 64]; /* allow for some padding between messages. */
   int datapipev[2];

   if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, datapipev) != 0) {
      perror("socketpair()");
      exit(EXIT_UNKNOWN);
   }

   setsockets(0,
              PACKETSIZE,
              datapipev[SEND_PIPE],
              datapipev[RECV_PIPE],
              &sndbuf, &sndbuf_set,
              &rcvbuf, &rcvbuf_set);

   packetcount = MIN(sndbuf, sndbuf_set) / PACKETSIZE;
   fprintf(stderr, "Requested sndbuf to be %ld, is %ld.  "
          "Requested rcvbuf to be %ld, is %ld.\n"
          "Calculated packetcount is %lu\n",
          (long)sndbuf, (long)sndbuf_set,
          (long)rcvbuf, (long)rcvbuf_set, (unsigned long)packetcount);

   sent = sendtest(datapipev[SEND_PIPE], buf, sizeof(buf));
   if (sent >= (size_t)sndbuf) {
      fprintf(stderr, "status determined by send-side\n");
      return EXIT_SENDSIDE;
   }

   /*
    * Try the reverse.  Perhaps this system wants a large rcvbuf rather than
    * a large sndbuf.
    */
   close(datapipev[SEND_PIPE]);
   close(datapipev[RECV_PIPE]);

   if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, datapipev) != 0) {
      perror("socketpair()");
      exit(EXIT_UNKNOWN);
   }

   setsockets(1,
              PACKETSIZE,
              datapipev[SEND_PIPE],
              datapipev[RECV_PIPE],
              &sndbuf, &sndbuf_set,
              &rcvbuf, &rcvbuf_set);

   packetcount = MIN(rcvbuf, rcvbuf_set) / PACKETSIZE;
   fprintf(stderr, "Requested sndbuf to be %ld, is %ld.  "
          "Requested rcvbuf to be %ld, is %ld.\n"
          "Calculated packetcount is %lu\n",
          (long)sndbuf, (long)sndbuf_set,
          (long)rcvbuf, (long)rcvbuf_set, (unsigned long)packetcount);

   sent = sendtest(datapipev[SEND_PIPE], buf, sizeof(buf));
   if (sent >= (size_t)rcvbuf) {
      fprintf(stderr, "status determined by read-side\n");
      return EXIT_READSIDE;
   }

   fprintf(stderr, "status is unknown\n");
   return EXIT_UNKNOWN;
}

static void
setsockets(doreverse, packetsize, s, r, sndbuf, sndbuf_set, rcvbuf, rcvbuf_set)
   const int doreverse;
   const size_t packetsize;
   const int s;
   const int r;
   size_t *sndbuf, *sndbuf_set;
   size_t *rcvbuf, *rcvbuf_set;
{
   socklen_t len;
   int p;

   if ((p = fcntl(s, F_GETFL, 0))        == -1
   ||  fcntl(s, F_SETFL, p | O_NONBLOCK) == -1
   ||  fcntl(r, F_SETFL, p | O_NONBLOCK) == -1) {
      perror("fcntl(F_SETFL/F_GETFL, O_NONBLOCK) failed");
      exit(EXIT_UNKNOWN);
   }

   len = sizeof(*sndbuf_set);

   if (doreverse) {
      *sndbuf = packetsize;
      if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, sndbuf, sizeof(*sndbuf)) != 0) {
         perror("setsockopt(SO_SNDBUF)");
         exit(EXIT_UNKNOWN);
      }

      if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, sndbuf_set, &len) != 0) {
         perror("getsockopt(SO_SNDBUF)");
         exit(EXIT_UNKNOWN);
      }

      *rcvbuf = *sndbuf_set * 10;
      if (setsockopt(r, SOL_SOCKET, SO_RCVBUF, rcvbuf, sizeof(*rcvbuf)) != 0) {
         perror("setsockopt(SO_RCVBUF)");
         exit(EXIT_UNKNOWN);
      }
   }
   else {
      *rcvbuf = packetsize;
      if (setsockopt(r, SOL_SOCKET, SO_RCVBUF, rcvbuf, sizeof(*rcvbuf)) != 0) {
         perror("setsockopt(SO_RCVBUF)");
         exit(EXIT_UNKNOWN);
      }

      if (getsockopt(r, SOL_SOCKET, SO_RCVBUF, rcvbuf_set, &len) != 0) {
         perror("getsockopt(SO_RCVBUF)");
         exit(EXIT_UNKNOWN);
      }

      *sndbuf = *rcvbuf_set * 10;
      if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, sndbuf, sizeof(*sndbuf)) != 0) {
         perror("setsockopt(SO_SNDBUF)");
         exit(EXIT_UNKNOWN);
      }
   }

   if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, sndbuf_set, &len) != 0
   ||  getsockopt(r, SOL_SOCKET, SO_RCVBUF, rcvbuf_set, &len) != 0) {
      perror("getsockopt(SO_SNDBUF/SO_RCVBUF)");
      exit(EXIT_UNKNOWN);
   }

   fprintf(stderr, "sndbuf is %lu, rcvbuf is %lu\n",
          (unsigned long)*sndbuf_set, (unsigned long)*rcvbuf_set);

   if (doreverse) {
      if (*rcvbuf_set < *rcvbuf) {
         fprintf(stderr, "failed to set rcvbuf to %lu.  Is %lu\n",
                 (unsigned long)*rcvbuf, (unsigned long)*rcvbuf_set);
         exit(EXIT_UNKNOWN);
      }
   }
   else {
      if (*sndbuf_set < *sndbuf) {
         fprintf(stderr, "failed to set sndbuf to %lu (is %lu)\n",
                 (unsigned long)*sndbuf, (unsigned long)*sndbuf_set);
         exit(EXIT_UNKNOWN);
      }
   }
}

static size_t
sendtest(s, buf, buflen)
   const int s;
   const char *buf;
   const size_t buflen;
{
   ssize_t rc;
   int i;

   i     = 1;
   errno = 0;
   while (errno == 0) {
      if ((rc = sendto(s, buf, buflen, 0, NULL, 0)) != (ssize_t)buflen)
         fprintf(stderr, "sendto(2) failed on iteration %d, sent %ld/%lu.  "
                "Total bytes sent: %lu.  Error on last packet: %s\n",
                i, (long)rc, (unsigned long)buflen,
                (unsigned long)(i * buflen + MAX(rc, 0)), strerror(errno));
      else
         ++i;
   }

   return (size_t)(i * buflen + MAX(rc, 0));
}
