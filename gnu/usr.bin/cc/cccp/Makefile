# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp0
SRCS=	cccp.c cexp.y obstack.c prefix.c version.c
YFLAGS=
BINDIR=	/usr/libexec
NOSHARED=yes
MAN=	cccp.1
MLINKS=	cccp.1 cpp.1

.include <bsd.prog.mk>
