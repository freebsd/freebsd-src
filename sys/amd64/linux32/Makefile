# Makefile for syscall tables
#
# $FreeBSD$

all:
	@echo "make sysent only"

sysent:  linux32_sysent.c linux32_syscall.h linux32_proto.h linux32_syscalls.c linux32_systrace_args.c

linux32_sysent.c linux32_syscall.h linux32_proto.h linux32_syscalls.c linux32_systrace_args.c: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
