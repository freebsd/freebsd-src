# $FreeBSD: src/usr.sbin/nslookup/Makefile,v 1.7.2.2 1999/12/13 15:08:30 peter Exp $

USE_LIBBIND=	yes
.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c commands.l
MAN8=	nslookup.8

CFLAGS+=-D_PATH_HELPFILE=\"${DESTHELP}/nslookup.help\"
LDADD+=	-ll -ledit -ltermcap
DPADD+=	${LIBL} ${LIBEDIT} ${LIBTERMCAP}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${BIND_DIR}/bin/nslookup/nslookup.help \
		${DESTDIR}${DESTHELP}/nslookup.help

.include <bsd.prog.mk>
