# Makefile for syscall tables
#
# $FreeBSD: src/sys/amd64/ia32/Makefile,v 1.2 2003/05/31 06:49:53 peter Exp $

all:
	@echo "make sysent only"

sysent:  ia32_sysent.c ia32_syscall.h ia32_proto.h

ia32_sysent.c ia32_syscalls.c ia32_syscall.h ia32_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f ia32_sysent.c ia32_sysent.c.bak
	-mv -f ia32_syscalls.c ia32_syscalls.c.bak
	-mv -f ia32_syscall.h ia32_syscall.h.bak
	-mv -f ia32_proto.h ia32_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
