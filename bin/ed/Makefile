# $FreeBSD: src/bin/ed/Makefile,v 1.22 2003/05/19 15:51:58 des Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT)
DISTRIBUTION=crypto
CFLAGS+=-DDES
DPADD=	${LIBCIPHER}
LDADD=	-lcipher
.endif

.include <bsd.prog.mk>
