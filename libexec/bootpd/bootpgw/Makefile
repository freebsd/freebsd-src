# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.6 2004/12/21 08:47:03 ru Exp $

PROG=	bootpgw
NO_MAN=
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
