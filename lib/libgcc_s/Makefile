# $FreeBSD$

PKG=		clibs
SHLIB_NAME=	libgcc_s.so.1
SHLIBDIR?=	/lib

WARNS?=	2

LDFLAGS+=	-nodefaultlibs
VERSION_MAP=	${.CURDIR}/Version.map

.include "../libcompiler_rt/Makefile.inc"
.include "../libgcc_eh/Makefile.inc"

LIBCSRCDIR=	${SRCTOP}/lib/libc
LIBMSRCDIR=	${SRCTOP}/lib/msun/src
CFLAGS+=	-I${LIBCSRCDIR}/include -I${LIBCSRCDIR}/${MACHINE_CPUARCH}
CFLAGS+=	-I${LIBMSRCDIR}
.PATH:		${LIBMSRCDIR}
SRCS+=		s_fabs.c
SRCS+=		s_fabsf.c
SRCS+=		s_fabsl.c
SRCS+=		s_fmax.c
SRCS+=		s_fmaxf.c
SRCS+=		s_fmaxl.c
SRCS+=		s_logb.c
SRCS+=		s_logbf.c
SRCS+=		s_logbl.c
SRCS+=		s_scalbn.c
SRCS+=		s_scalbnf.c
SRCS+=		s_scalbnl.c

.include <bsd.lib.mk>
