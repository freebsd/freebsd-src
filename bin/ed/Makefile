# $FreeBSD: src/bin/ed/Makefile,v 1.29 2003/07/24 18:30:24 markm Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=crypto
CFLAGS+=-DDES
WARNS?=	2
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif

.include <bsd.prog.mk>
