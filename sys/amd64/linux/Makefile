# Makefile for syscall tables
#
# $FreeBSD$

all:
	@echo "make sysent only"

sysent:  linux_sysent.c linux_syscall.h linux_proto.h linux_syscalls.c linux_systrace_args.c

linux_sysent.c linux_syscall.h linux_proto.h linux_syscalls.c linux_systrace_args.c: \
		../../kern/makesyscalls.sh syscalls.master syscalls.conf
	-mv -f linux_sysent.c linux_sysent.c.bak
	-mv -f linux_syscall.h linux_syscall.h.bak
	-mv -f linux_proto.h linux_proto.h.bak
	-mv -f linux_syscalls.c linux_syscalls.c.bak
	-mv -f linux_systrace_args.c linux_systrace_args.c.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
