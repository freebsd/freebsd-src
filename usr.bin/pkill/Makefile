#	$NetBSD$

PROG=	pkill
SRCS=	pkill.c
MAN=	pkill.1

LDADD+=	-lkvm
DPADD+=	${LIBKVM}

LINKS+=	${BINDIR}/pkill ${BINDIR}/pgrep
MLINKS+=pkill.1 pgrep.1

.include <bsd.prog.mk>
