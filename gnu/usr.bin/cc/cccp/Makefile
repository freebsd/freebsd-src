# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	cccp.c cexp.y obstack.c prefix.c version.c
YFLAGS=
BINDIR=	/usr/libexec
MAN1=	cccp.1
LINKS=	${BINDIR}/cpp ${BINDIR}/cpp0
MLINKS=	cccp.1 cpp.1

.include <bsd.prog.mk>
