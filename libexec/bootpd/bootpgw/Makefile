# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
