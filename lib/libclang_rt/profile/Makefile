
.include <bsd.init.mk>

LIB=		clang_rt.profile-${CRTARCH}

CFLAGS+=	-DCOMPILER_RT_HAS_ATOMICS=1
CFLAGS+=	-DCOMPILER_RT_HAS_FCNTL_LCK=1
CFLAGS+=	-DCOMPILER_RT_HAS_UNAME=1

# This is needed for --coverage
CXXFLAGS+=	-fvisibility=default

SRCS+=		profile/GCDAProfiling.c
SRCS+=		profile/InstrProfiling.c
SRCS+=		profile/InstrProfilingBuffer.c
SRCS+=		profile/InstrProfilingFile.c
SRCS+=		profile/InstrProfilingInternal.c
SRCS+=		profile/InstrProfilingMerge.c
SRCS+=		profile/InstrProfilingMergeFile.c
SRCS+=		profile/InstrProfilingNameVar.c
SRCS+=		profile/InstrProfilingPlatformLinux.c
SRCS+=		profile/InstrProfilingPlatformOther.c
SRCS+=		profile/InstrProfilingRuntime.cpp
SRCS+=		profile/InstrProfilingUtil.c
SRCS+=		profile/InstrProfilingValue.c
SRCS+=		profile/InstrProfilingVersionVar.c
SRCS+=		profile/InstrProfilingWriter.c

.PATH:		${CRTSRC}/include/profile
INCSDIR=	${CLANGDIR}/include/profile
INCS+=		InstrProfData.inc
INCS+=		MemProfData.inc

.include <bsd.lib.mk>
