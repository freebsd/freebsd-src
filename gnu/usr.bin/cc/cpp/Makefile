#
# $FreeBSD$
#

PROG =	cpp
SRCS =	cccp.c cexp.c
BINDIR=	/usr/libexec
SRCS+=	obstack.c version.c
MAN1=	cccp.1
MLINKS=	cccp.1 cpp.1

cexp.c cexp.h: cexp.y
	${BISON} -d ${GCCDIR}/cexp.y -o cexp.c

CLEANFILES+= cexp.c cexp.h

.include <bsd.prog.mk>
