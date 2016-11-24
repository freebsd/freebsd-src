# $FreeBSD$

.include <bsd.init.mk>

LIB=		clang_rt.profile-${CRTARCH}

SRCS+=		profile/GCDAProfiling.c
SRCS+=		profile/InstrProfiling.c
SRCS+=		profile/InstrProfilingBuffer.c
SRCS+=		profile/InstrProfilingFile.c
SRCS+=		profile/InstrProfilingMerge.c
SRCS+=		profile/InstrProfilingMergeFile.c
SRCS+=		profile/InstrProfilingPlatformDarwin.c
SRCS+=		profile/InstrProfilingPlatformLinux.c
SRCS+=		profile/InstrProfilingPlatformOther.c
SRCS+=		profile/InstrProfilingRuntime.cc
SRCS+=		profile/InstrProfilingUtil.c
SRCS+=		profile/InstrProfilingValue.c
SRCS+=		profile/InstrProfilingWriter.c

.include <bsd.lib.mk>
