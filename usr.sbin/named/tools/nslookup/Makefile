#	@(#)Makefile	5.23 (Berkeley) 6/24/90

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c
OBJS+=	commands.o
MAN8=	nslookup.8
LFLAGS=	-I
CFLAGS+=-I${.CURDIR}
DPADD=	${LIBLN}
LDADD=	-lln
CLEANFILES+=commands.c lex.yy.c lex.yy.o

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/nslookup.help \
	    ${DESTDIR}/usr/share/misc

.include "../../../Makefile.inc"
.include <bsd.prog.mk>
