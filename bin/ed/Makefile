# $FreeBSD: src/bin/ed/Makefile,v 1.30 2004/08/06 07:27:03 cperciva Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
CFLAGS+=-DDES
WARNS?=	2
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif

.include <bsd.prog.mk>
