# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	cccp.c cexp.y obstack.c prefix.c version.c
CFLAGS+= -DPREFIX=\"/usr\"
YFLAGS=
BINDIR=	/usr/libexec
MAN1=	cccp.1
MLINKS=	cccp.1 cpp.1

.include <bsd.prog.mk>
