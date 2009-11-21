# $FreeBSD: src/tools/tools/ath/athpoke/Makefile,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	athpoke
LINKS=	${BINDIR}/${PROG} ${BINDIR}/athpeek

.PATH.c: ${.CURDIR}/../common

SRCS=	athpoke.c
SRCS+=	dumpregs_5210.c
SRCS+=	dumpregs_5211.c
SRCS+=	dumpregs_5212.c
SRCS+=	dumpregs_5416.c

.include <../Makefile.inc>

.include <bsd.prog.mk>
