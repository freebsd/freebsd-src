#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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

# Test scenario for sendfile corruption of read only input file

# Scenario by Ming Fu <Ming Fu watchguard com>

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > sendfile2.c
mycc -o sendfile2 -Wall sendfile2.c
rm -f sendfile2.c
[ -d "$RUNDIR" ] || mkdir -p $RUNDIR
cd $RUNDIR

dd if=/dev/random of=large bs=1m count=3 status=none
md1=`md5 large`

nc -l 7000 > lf &
sleep 0.1
/tmp/sendfile2
kill $! 2>/dev/null
wait

md2=`md5 large`
[ "$md1" != "$md2" ] && printf "%s\n%s\n" "$md1" "$md2"

rm -f /tmp/sendfile2 large lf
exit
EOF
#include <sys/types.h>
#include <arpa/inet.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int
main () {
        int s, f;
        struct sockaddr_in addr;
	struct hostent *hostent;
        int flags;
        char str[32]="\r\n800\r\n";
        char *p = str;
        struct stat sb;
        int n;
        fd_set wset;
        int64_t size;
        off_t sbytes;
        off_t sent = 0;
        int chunk;

	alarm(120);
        s = socket(AF_INET, SOCK_STREAM, 0);
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(7000);
	hostent = gethostbyname ("localhost");
	memcpy (&addr.sin_addr.s_addr, hostent->h_addr,
		sizeof (struct in_addr));

        n = connect(s, (struct sockaddr *)&addr, sizeof (addr));
        if (n < 0)
                warn ("fail to connect");
        flags = fcntl(s, F_GETFL);
        flags |= O_NONBLOCK;
        fcntl(s, F_SETFL, flags);

        f = open("large", O_RDONLY);
        if (f < 0)
                warn("fail to open file");
        n = fstat(f, &sb);
        if (n < 0)
                warn("fstat failed");

        size = sb.st_size;
        chunk = 0;
        while (size > 0) {
		FD_ZERO(&wset);
                FD_SET(s, &wset);
                n = select(f+1, NULL, &wset, NULL, NULL);
                if (n < 0)
                        continue;
                if (chunk > 0) {
                        sbytes = 0;
                        n = sendfile(f, s, sent, chunk, NULL, &sbytes, 0);
                        if (n < 0)
                                continue;
                        chunk -= sbytes;
                        size -= sbytes;
                        sent += sbytes;
                        continue;
                }
                if (size > 2048)
                        chunk = 2048;
                else
                        chunk = size;
                n = sprintf(str, "\r\n%x\r\n", 2048);
                p = str;
                write(s, p, n);
        }

	return (0);
}
