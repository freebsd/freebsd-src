#
# $Id: Makefile,v 1.7 1994/11/15 04:51:27 phk Exp $
#

PROG =	cc
SRCS =	gcc.c
BINDIR=	/usr/bin
.PATH: ${.CURDIR}/../cc_int
SRCS+=	obstack.c version.c
LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS= cc.1 gcc.1

.include <bsd.prog.mk>
