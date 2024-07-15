PACKAGE=	clibs
SHLIB_NAME=	libgcc_s.so.1
SHLIBDIR?=	/lib

# Enabling UBSan triggers "undefined reference to vtable for __cxxabiv1::__function_type_info"
MK_UBSAN:=	no

.include <bsd.opts.mk>

MK_SSP=		no
WARNS?=	2

LDFLAGS+=	-nodefaultlibs
LIBADD+=	c

VERSION_DEF=	${.CURDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map
# Export ARM AEABI unwind routines needed by libc and libthr.
.if exists(${.CURDIR}/${MACHINE_CPUARCH}/Symbol.map)
SYMBOL_MAPS+=	${.CURDIR}/${MACHINE_CPUARCH}/Symbol.map
.else
SYMBOL_MAPS+=	${.CURDIR}/SymbolDefault.map
.endif

.include "../libcompiler_rt/Makefile.inc"
.include "../libgcc_eh/Makefile.inc"

# gcc has incompatible internal declarations for __divtc3 and __multc3, but has
# no option to silence its warning, so make warnings non-fatal.
MK_WERROR.gcc=	no

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
SRCS+=		s_logb.c
SRCS+=		s_logbf.c
SRCS+=		s_scalbn.c
SRCS+=		s_scalbnf.c

# Don't include long double routines on architectures where long double
# is the same size as double.
.if ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "powerpc"
SRCS+=		s_fmaxl.c
SRCS+=		s_logbl.c
SRCS+=		s_scalbnl.c
.endif

.include <bsd.lib.mk>
