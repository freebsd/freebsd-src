# $Id$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${BIND_DIR}/tools \
	${BIND_DIR}/tools/nslookup \
	${BIND_DIR}/man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c

BINDIR=	${DESTBIN}

.include <bsd.prog.mk>
