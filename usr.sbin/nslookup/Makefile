#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	nslookup
SRCS=	main.c commands.l getinfo.c debug.c send.c skip.c list.c subr.c
MAN8=	nslookup.8
DPADD+=	${LIBL}
LDADD+=	-ll
CLEANFILES+=lex.yy.o

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/nslookup.help ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
