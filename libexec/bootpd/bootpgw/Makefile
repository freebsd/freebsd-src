# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.4.26.1 2005/02/13 07:23:15 obrien Exp $

PROG=	bootpgw
NOMAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
