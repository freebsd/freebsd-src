# Makefile for syscall tables
#
# $FreeBSD: src/sys/i386/linux/Makefile,v 1.5.24.1.2.1 2008/11/25 02:59:29 kensmith Exp $

all:
	@echo "make sysent only"

sysent:  linux_sysent.c linux_syscall.h linux_proto.h

linux_sysent.c linux_syscall.h linux_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f linux_sysent.c linux_sysent.c.bak
	-mv -f linux_syscall.h linux_syscall.h.bak
	-mv -f linux_proto.h linux_proto.h.bak
	-mv -f linux_systrace.c linux_systrace.c.bak
	-mv -f linux_syscallnames.c linux_syscallnames.c.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
