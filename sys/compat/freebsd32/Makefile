# Makefile for syscall tables
#
# $FreeBSD$

all:
	@echo "make sysent only"

sysent:  ia32_sysent.c ia32_syscall.h ia32_proto.h

ia32_sysent.c ia32_syscall.h ia32_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f ia32_sysent.c ia32_sysent.c.bak
	-mv -f ia32_syscall.h ia32_syscall.h.bak
	-mv -f ia32_proto.h ia32_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
