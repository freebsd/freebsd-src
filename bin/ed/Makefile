# $FreeBSD$

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if exists(${.CURDIR}/../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=crypto
CFLAGS+=-DDES
WARNS=	2
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif

.include <bsd.prog.mk>
