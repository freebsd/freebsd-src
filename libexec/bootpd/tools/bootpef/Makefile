# Makefile
# $FreeBSD: src/libexec/bootpd/tools/bootpef/Makefile,v 1.3.2.1 1999/08/29 15:03:02 peter Exp $

PROG=	bootpef
MAN8=	bootpef.8
SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c lookup.c \
	hwaddr.c report.c tzone.c rtmsg.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
