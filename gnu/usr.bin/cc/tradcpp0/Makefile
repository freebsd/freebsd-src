# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	tradcpp0
SRCS=	tradcpp.c tradcif.y
BINDIR=	/usr/libexec
NOMAN=	1

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
