# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	lib${LIB}
LIB=		compiler_rt
MK_SSP=		no
NO_PIC=
WARNS?=		2

CFLAGS+=	${PICFLAG}
CFLAGS+=	-fvisibility=hidden
CFLAGS+=	-DVISIBILITY_HIDDEN
CFLAGS+=	-I${SRCTOP}/contrib/libcxxrt

# gcc has incompatible internal declarations for __divtc3 and __multc3, but has
# no option to silence its warning, so make warnings non-fatal.
NO_WERROR.gcc=

.include "Makefile.inc"

# Out-of-line LSE atomics helpers for aarch64
.if ${MACHINE_CPUARCH} == "aarch64"
. for pat in cas swp ldadd ldclr ldeor ldset
.  for size in 1 2 4 8 16
.   for model in 1 2 3 4
.    if ${pat} == "cas" || ${size} != "16"
# Use .for to define lse_name, to get a special loop-local variable
.     for lse_name in outline_atomic_${pat}${size}_${model}.S
CLEANFILES+=	${lse_name}
STATICOBJS+=	${lse_name:R}.o
ACFLAGS.${lse_name}+= -DL_${pat} -DSIZE=${size} -DMODEL=${model} -I${CRTSRC}
${lse_name}: lse.S
	ln -sf ${.ALLSRC} ${.TARGET}
.     endfor # lse_name
.    endif
.   endfor # model
.  endfor # size
. endfor # pat
.endif

.if ${MK_INSTALLLIB} != "no"
SYMLINKS+=	libcompiler_rt.a ${LIBDIR}/libgcc.a
.endif
.if ${MK_PROFILE} != "no"
SYMLINKS+=	libcompiler_rt_p.a ${LIBDIR}/libgcc_p.a
.endif

.include <bsd.lib.mk>
