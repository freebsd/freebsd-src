# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

MAKESYSCALLS=	../../tools/makesyscalls.lua
SRCS=		syscalls.conf		\
		syscalls.master
GENERATED=	linux_proto.h		\
		linux_syscall.h		\
		linux_syscalls.c	\
		linux_sysent.c		\
		linux_systrace_args.c

all:
	@echo "make sysent only"

sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} syscalls.master syscalls.conf
