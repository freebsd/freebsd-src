# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

MAKESYSCALLS=	../../tools/makesyscalls.lua
SRCS=		../../kern/capabilities.conf	\
		syscalls.conf			\
		syscalls.master
GENERATED=	freebsd32_proto.h		\
		freebsd32_syscall.h		\
		freebsd32_syscalls.c		\
		freebsd32_sysent.c		\
		freebsd32_systrace_args.c

all:
	@echo "make sysent only"

sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} syscalls.master syscalls.conf
