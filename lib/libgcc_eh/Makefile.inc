# $FreeBSD$

.include <src.opts.mk>

COMPILERRTDIR=	${SRCTOP}/contrib/compiler-rt
.if ${MK_CHERI} != "no"
WANT_CHERI?=	hybrid
ALLOW_SHARED_TEXTREL=yes
UNWINDINCDIR=   ${SRCTOP}/contrib/cheri-libunwind/include
UNWINDSRCDIR=   ${SRCTOP}/contrib/cheri-libunwind/src
.else
UNWINDINCDIR=   ${SRCTOP}/contrib/cheri-libunwind/include
UNWINDSRCDIR=   ${SRCTOP}/contrib/cheri-libunwind/src
.endif

STATIC_CFLAGS+=${PICFLAG} -fvisibility=hidden -DVISIBILITY_HIDDEN

.PATH: ${COMPILERRTDIR}/lib/builtins
.PATH: ${UNWINDSRCDIR}
SRCS_EXC+=	gcc_personality_v0.c
SRCS_EXC+=	int_util.c
SRCS_EXC+=	Unwind-EHABI.cpp
SRCS_EXC+=	Unwind-sjlj.c
SRCS_EXC+=	UnwindLevel1-gcc-ext.c
SRCS_EXC+=	UnwindLevel1.c
SRCS_EXC+=	UnwindRegistersRestore.S
SRCS_EXC+=	UnwindRegistersSave.S
SRCS_EXC+=	libunwind.cpp

SRCS+=		${SRCS_EXC}
.for file in ${SRCS_EXC:M*.c}
CFLAGS.${file}+=	-fexceptions
.endfor

CFLAGS+=	-I${UNWINDINCDIR} -I${.CURDIR} -D_LIBUNWIND_IS_NATIVE_ONLY
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++11
.endif
CXXFLAGS+=	-fno-rtti
STATIC_CXXFLAGS+= -fvisibility=hidden -fPIC
.if ${MK_DIRDEPS_BUILD} == "yes"
# Avoid dependency on lib/libc++
.if ${MK_CHERI} != "no"
CFLAGS+=	-I${SRCTOP}/contrib/cheri-libc++/include
.else
CFLAGS+=	-I${SRCTOP}/contrib/cheri-libc++/include
.endif
.endif
