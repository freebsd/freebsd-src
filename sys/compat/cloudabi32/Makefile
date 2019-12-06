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

sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} \
	    ../../contrib/cloudabi/syscalls32.master syscalls.conf
