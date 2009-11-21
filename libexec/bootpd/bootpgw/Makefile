# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
