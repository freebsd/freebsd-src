
COMPILERRTDIR=	${SRCTOP}/contrib/llvm-project/compiler-rt
UNWINDINCDIR=	${SRCTOP}/contrib/llvm-project/libunwind/include
UNWINDSRCDIR=	${SRCTOP}/contrib/llvm-project/libunwind/src

STATIC_CFLAGS+=${PICFLAG} -fvisibility=hidden -DVISIBILITY_HIDDEN

.PATH: ${COMPILERRTDIR}/lib/builtins
.PATH: ${UNWINDSRCDIR}
SRCS_EXC+=	gcc_personality_v0.c
SRCS_EXC+=	Unwind-EHABI.cpp
SRCS_EXC+=	Unwind-sjlj.c
SRCS_EXC+=	UnwindLevel1-gcc-ext.c
SRCS_EXC+=	UnwindLevel1.c
SRCS_EXC+=	UnwindRegistersRestore.S
SRCS_EXC+=	UnwindRegistersSave.S
SRCS_EXC+=	libunwind.cpp

SRCS+=		${SRCS_EXC}
.for file in ${SRCS_EXC:M*.c}
CFLAGS.${file}+=	-fno-exceptions -funwind-tables
.if ${MK_ASAN} != "no"
# False-positives during stack unwinding
CFLAGS.${file}+=	-fno-sanitize=address
.endif
.endfor
.for file in ${SRCS_EXC:M*.cpp}
CXXFLAGS.${file}+=	-fno-exceptions -funwind-tables
.if ${MK_ASAN} != "no"
# False-positives during stack unwinding
CXXFLAGS.${file}+=	-fno-sanitize=address
.endif
.endfor

CFLAGS+=	-I${UNWINDINCDIR}
CFLAGS+=	-D_LIBUNWIND_IS_NATIVE_ONLY
CFLAGS+=	-D_LIBUNWIND_USE_FRAME_HEADER_CACHE
CXXFLAGS+=	-fno-rtti
CXXSTD?=	c++11
STATIC_CXXFLAGS+= -fvisibility=hidden -fPIC
# Probably need to just move this earlier or use CXXFLAGS
.if ${MK_DIRDEPS_BUILD} == "yes"
# Avoid dependency on lib/libc++
CFLAGS+=	-isystem ${SRCTOP}/contrib/llvm-project/libcxx/include -nostdinc++

# we need this for __config_site
CFLAGS+=	-I${SRCTOP}/lib/libc++
.if ${.MAKE.LEVEL} > 0
GENDIRDEPS_FILTER+= Nlib/libc++
.endif
.endif
