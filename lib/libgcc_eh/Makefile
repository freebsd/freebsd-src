# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	clibs
LIB=		gcc_eh
NO_PIC=
MK_SSP=		no
WARNS?=	2

SRCS_EXC+=	int_util.c
.include "Makefile.inc"

INCS+=		${UNWINDINCDIR}/__libunwind_config.h
INCS+=		${UNWINDINCDIR}/libunwind.h
INCS+=		${UNWINDINCDIR}/unwind.h
INCS+=		${UNWINDINCDIR}/unwind_arm_ehabi.h
INCS+=		${UNWINDINCDIR}/unwind_itanium.h

.if ${.MAKE.LEVEL} > 0
# avoid circular dependencies
GENDIRDEPS_FILTER+= Nlib/msun
CFLAGS+= -I${SRCTOP}/lib/msun/src
.endif

.include <bsd.lib.mk>
