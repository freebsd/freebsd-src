# $Id: Makefile,v 1.5 1997/02/22 16:08:30 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c commands.c
MAN8=	nslookup.8

CFLAGS+=-D_PATH_HELPFILE=\"${DESTHELP}/nslookup.help\"
LDADD+=	-ll
DPADD+=	${LIBL}

CLEANFILES+= commands.c lex.yy.c lex.yy.o

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${BIND_DIR}/bin/nslookup/nslookup.help \
		${DESTDIR}${DESTHELP}/nslookup.help

.include <bsd.prog.mk>
