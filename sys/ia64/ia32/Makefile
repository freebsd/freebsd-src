# Makefile for syscall tables
#
# $FreeBSD: src/sys/ia64/ia32/Makefile,v 1.1 2002/04/10 19:34:51 dfr Exp $

all:
	@echo "make sysent only"

sysent:  ia32_sysent.c ia32_syscall.h ia32_proto.h

ia32_sysent.c ia32_syscall.h ia32_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f ia32_sysent.c ia32_sysent.c.bak
	-mv -f ia32_syscall.h ia32_syscall.h.bak
	-mv -f ia32_proto.h ia32_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
