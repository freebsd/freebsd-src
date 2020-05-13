# $FreeBSD$

.include <src.opts.mk>

PROG=	procctl
MAN=	procctl.8

# Ignore deprecation warning in <sys/pioctl.h>
CFLAGS.clang+= -Wno-\#warnings
.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 40700
CFLAGS.gcc+= -Wno-cpp
.endif

.include <bsd.prog.mk>
