#	@(#)Makefile	8.2 (Berkeley) 3/21/94
# $FreeBSD$
#
# Makefile for init_sysent

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

MAKESYSCALLS=	../tools/makesyscalls.lua
SRCS=		capabilities.conf	\
		syscalls.master
GENERATED=	init_sysent.c		\
		syscalls.c		\
		systrace_args.c		\
		../sys/syscall.h	\
		../sys/syscall.mk	\
		../sys/sysproto.h

all:
	@echo "make sysent only"

sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} syscalls.master
