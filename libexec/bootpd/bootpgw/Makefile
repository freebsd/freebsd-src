# Makefile
# $FreeBSD: src/libexec/bootpd/bootpgw/Makefile,v 1.3.2.1 1999/08/29 15:02:59 peter Exp $

PROG=	bootpgw
NOMAN=  true
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
