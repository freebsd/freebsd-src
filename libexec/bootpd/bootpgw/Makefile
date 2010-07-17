# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
