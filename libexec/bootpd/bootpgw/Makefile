# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
