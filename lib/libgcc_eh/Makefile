# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	clibs
LIB=		gcc_eh
NO_PIC=
MK_SSP=		no
WARNS?=	2

.include "Makefile.inc"

.if ${.MAKE.LEVEL} > 0
# avoid circular dependencies
GENDIRDEPS_FILTER+= Nlib/msun
CFLAGS+= -I${SRCTOP}/lib/msun/src
.endif

.include <bsd.lib.mk>
