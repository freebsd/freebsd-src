# $Id$

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/tools/nslookup
.PATH:	${BIND_DIR}/man

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c commands.c
MAN8=	nslookup.8

LDADD=	-ll
DPADD=	${LIBL}

CLEANFILES+= commands.c lex.yy.c lex.yy.o

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${BIND_DIR}/tools/nslookup/nslookup.help \
		${DESTDIR}/usr/share/misc/nslookup.help

.include <bsd.prog.mk>
