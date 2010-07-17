# $FreeBSD: src/bin/ed/Makefile,v 1.34.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
WARNS?=	2
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if !defined(RELEASE_CRUNCH)
.if ${MK_OPENSSL} != "no"
CFLAGS+=-DDES
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif
.endif

.include <bsd.prog.mk>
