# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
