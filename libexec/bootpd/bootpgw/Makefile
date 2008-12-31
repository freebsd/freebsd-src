# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
