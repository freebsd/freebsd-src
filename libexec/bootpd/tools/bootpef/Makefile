# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootpef/Makefile,v 1.4.2.1 2001/04/25 10:40:50 ru Exp $

PROG=	bootpef
MAN=	bootpef.8
SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c lookup.c \
	hwaddr.c report.c tzone.c rtmsg.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
