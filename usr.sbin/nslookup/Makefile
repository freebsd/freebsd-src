# $Id: Makefile,v 1.6 1998/05/03 05:14:56 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c commands.l
MAN8=	nslookup.8

CFLAGS+=-D_PATH_HELPFILE=\"${DESTHELP}/nslookup.help\"
LDADD+=	-ll
DPADD+=	${LIBL}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${BIND_DIR}/bin/nslookup/nslookup.help \
		${DESTDIR}${DESTHELP}/nslookup.help

.include <bsd.prog.mk>
