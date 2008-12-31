# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootpef/Makefile,v 1.6.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	bootpef
MAN=	bootpef.8
SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c lookup.c \
	hwaddr.c report.c tzone.c rtmsg.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
