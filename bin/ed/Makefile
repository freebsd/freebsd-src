# $FreeBSD: src/bin/ed/Makefile,v 1.31 2004/12/21 10:15:59 ru Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
CFLAGS+=-DDES
WARNS?=	2
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif

.include <bsd.prog.mk>
