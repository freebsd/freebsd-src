# $FreeBSD$

.include <bsd.init.mk>

LIB=	clang_rt.profile-${CRTARCH}

.PATH:	${CRTSRC}/lib/profile

SRCS=	GCDAProfiling.c\
	InstrProfiling.c\
	InstrProfilingValue.c\
	InstrProfilingBuffer.c\
	InstrProfilingFile.c\
	InstrProfilingWriter.c\
	InstrProfilingPlatformDarwin.c\
	InstrProfilingPlatformLinux.c\
	InstrProfilingPlatformOther.c\
	InstrProfilingRuntime.cc\
	InstrProfilingUtil.c

.include <bsd.lib.mk>
