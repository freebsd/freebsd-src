#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.6 1997/02/22 19:56:17 peter Exp $

PROG=	more
CFLAGS+=-I${.CURDIR} -DTERMIOS
SRCS=	ch.c command.c decode.c help.c input.c line.c linenum.c main.c \
	option.c os.c output.c position.c prim.c screen.c signal.c tags.c \
	ttyin.c
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/more.help \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
