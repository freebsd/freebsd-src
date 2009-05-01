# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
