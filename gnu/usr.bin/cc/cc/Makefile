#
# $Id: Makefile,v 1.8.4.2 1996/06/05 02:39:25 jkh Exp $
#

PROG =	cc
SRCS =	gcc.c
BINDIR=	/usr/bin
.PATH: ${.CURDIR}/../cc_int
SRCS+=	obstack.c version.c
LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS= cc.1 gcc.1

.include <bsd.prog.mk>
