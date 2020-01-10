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

# We .ORDER these explicitly so that we only run MAKESYSCALLS once, rather than
# potentially once for each ${GENERATED} file.
.ORDER: ${GENERATED}
sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} ${SRCS}
	${LUA} ${MAKESYSCALLS} syscalls.master syscalls.conf
