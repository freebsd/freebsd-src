# Makefile
# $FreeBSD$

PROG=	bootpef
SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c lookup.c \
	hwaddr.c report.c tzone.c rtmsg.c

SRCDIR=	${.CURDIR}/../..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
