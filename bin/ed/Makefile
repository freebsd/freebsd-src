# $FreeBSD: src/bin/ed/Makefile,v 1.21 2001/12/06 10:50:23 ru Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=crypto
CFLAGS+=-DDES
DPADD=	${LIBCIPHER}
LDADD=	-lcipher
.endif

.include <bsd.prog.mk>
