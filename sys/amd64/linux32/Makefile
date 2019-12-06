# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

MAKESYSCALLS=	../../tools/makesyscalls.lua
SRCS=		syscalls.conf		\
		syscalls.master
GENERATED=	linux32_proto.h		\
		linux32_syscall.h	\
		linux32_syscalls.c	\
		linux32_sysent.c	\
		linux32_systrace_args.c

all:
	@echo "make sysent only"

sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} syscalls.master syscalls.conf
