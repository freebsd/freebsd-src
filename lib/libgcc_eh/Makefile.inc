# $FreeBSD$

COMPILERRTDIR=	${SRCTOP}/contrib/compiler-rt
UNWINDINCDIR=	${SRCTOP}/contrib/llvm/projects/libunwind/include
UNWINDSRCDIR=	${SRCTOP}/contrib/llvm/projects/libunwind/src

STATIC_CFLAGS+=${PICFLAG} -fvisibility=hidden -DVISIBILITY_HIDDEN

.PATH: ${COMPILERRTDIR}/lib/builtins
.PATH: ${UNWINDSRCDIR}
SRCS+=	gcc_personality_v0.c
SRCS+=	int_util.c
SRCS+=	Unwind-EHABI.cpp
SRCS+=	Unwind-sjlj.c
SRCS+=	UnwindLevel1-gcc-ext.c
SRCS+=	UnwindLevel1.c
SRCS+=	UnwindRegistersRestore.S
SRCS+=	UnwindRegistersSave.S
SRCS+=	libunwind.cpp

CFLAGS+=	-I${UNWINDINCDIR} -I${.CURDIR} -D_LIBUNWIND_IS_NATIVE_ONLY
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++11
.endif
CXXFLAGS+=	-fno-rtti
STATIC_CXXFLAGS+= -fvisibility=hidden -fPIC
.if ${MK_DIRDEPS_BUILD} == "yes"
# Avoid dependency on lib/libc++
CFLAGS+=	-I${SRCTOP}/contrib/libc++/include
.endif
