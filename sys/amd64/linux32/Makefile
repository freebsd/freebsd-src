# Makefile for syscall tables
#
# $FreeBSD: src/sys/amd64/linux32/Makefile,v 1.1.30.2.2.1 2012/03/03 06:15:13 kensmith Exp $

all:
	@echo "make sysent only"

sysent:  linux32_sysent.c linux32_syscall.h linux32_proto.h linux32_syscalls.c linux32_systrace_args.c

linux32_sysent.c linux32_syscall.h linux32_proto.h linux32_syscalls.c linux32_systrace_args.c: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f linux32_sysent.c linux32_sysent.c.bak
	-mv -f linux32_syscall.h linux32_syscall.h.bak
	-mv -f linux32_proto.h linux32_proto.h.bak
	-mv -f linux32_syscalls.c linux32_syscalls.c.bak
	-mv -f linux32_systrace_args.c linux32_systrace_args.c.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
