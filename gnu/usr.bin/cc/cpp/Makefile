#
# $Id: Makefile,v 1.4 1994/11/15 04:52:05 phk Exp $
#

PROG =	cpp
SRCS =	cccp.c cexp.c
BINDIR=	/usr/libexec
SRCS+=	obstack.c version.c

cexp.c cexp.h: cexp.y
	${BISON} -d ${GCCDIR}/cexp.y -o cexp.c

CLEANFILES+= cexp.c cexp.h

.include <bsd.prog.mk>
