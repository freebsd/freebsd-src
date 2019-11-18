# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

all:
	@echo "make sysent only"

sysent:  linux_sysent.c linux_syscall.h linux_proto.h linux_syscalls.c linux_systrace_args.c

linux_sysent.c linux_syscall.h linux_proto.h linux_syscalls.c linux_systrace_args.c: \
		../../tools/makesyscalls.lua syscalls.master ${.CURDIR}/syscalls.conf
	${LUA} ../../tools/makesyscalls.lua syscalls.master ${.CURDIR}/syscalls.conf
