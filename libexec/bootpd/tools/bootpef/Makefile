# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootpef/Makefile,v 1.4 1999/08/28 00:09:23 peter Exp $

PROG=	bootpef
MAN8=	bootpef.8
SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c lookup.c \
	hwaddr.c report.c tzone.c rtmsg.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
