#
# $Id: Makefile,v 1.7.2.1 1997/10/26 22:47:43 jkh Exp $
#

PROG =	cpp
SRCS =	cccp.c cexp.c
BINDIR=	/usr/libexec
SRCS+=	obstack.c version.c
MAN1=	cccp.1
MLINKS=	cccp.1 cpp.1

.ORDER: cexp.c cexp.h
cexp.c cexp.h: cexp.y
	${BISON} -d ${GCCDIR}/cexp.y -o cexp.c

CLEANFILES+= cexp.c cexp.h

.include <bsd.prog.mk>
