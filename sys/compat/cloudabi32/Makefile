# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

MAKESYSCALLS=	../../tools/makesyscalls.lua
SRCS=		syscalls.conf		\
		../../contrib/cloudabi/syscalls32.master
GENERATED=	cloudabi32_proto.h	\
		cloudabi32_syscall.h	\
		cloudabi32_syscalls.c	\
		cloudabi32_sysent.c	\
		cloudabi32_systrace_args.c

all:
	@echo "make sysent only"

# We .ORDER these explicitly so that we only run MAKESYSCALLS once, rather than
# potentially once for each ${GENERATED} file.
.ORDER: ${GENERATED}
sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} \
	    ../../contrib/cloudabi/syscalls32.master syscalls.conf
