#	@(#)Makefile	8.2 (Berkeley) 3/21/94
# $FreeBSD$
#
# Makefile for init_sysent

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

all:
	@echo "make sysent only"

sysent: init_sysent.c syscalls.c ../sys/syscall.h ../sys/syscall.mk \
../sys/sysproto.h

init_sysent.c syscalls.c systrace_args.c ../sys/syscall.h \
../sys/syscall.mk ../sys/sysproto.h: ../tools/makesyscalls.lua syscalls.master \
capabilities.conf
	${LUA} ../tools/makesyscalls.lua syscalls.master
