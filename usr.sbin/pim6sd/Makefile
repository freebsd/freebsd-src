# Copyright (c) 1999 WIDE Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# $FreeBSD$

PROG=	pim6sd
SRCS=	mld6.c mld6_proto.c\
	inet6.c kern.c main.c config.c debug.c routesock.c vers.c callout.c\
	route.c vif.c timer.c mrt.c pim6.c pim6_proto.c rp.c crc.c trace.c\
	cfparse.y cftoken.l
SRCS+=	y.tab.h
y.tab.h: cfparse.y
CLEANFILES+= lex.yy.c y.tab.h y.tab.c
CFLAGS+= -g
CFLAGS+=-DINET6 -DPIM -DIOCTL_OK_ON_RAW_SOCKET -I${.OBJDIR}
CFLAGS+=-DHAVE_STDARG_H
DPADD=	${LIBY} ${LIBL}
LDADD=	-ly -ll

MAN1=	pim6stat.1
MAN5=	pim6sd.conf.5
MAN8=	pim6sd.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pim6stat ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
