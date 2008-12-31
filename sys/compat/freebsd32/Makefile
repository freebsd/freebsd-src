# Makefile for syscall tables
#
# $FreeBSD: src/sys/compat/freebsd32/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

all:
	@echo "make sysent only"

sysent:  freebsd32_sysent.c freebsd32_syscall.h freebsd32_proto.h

freebsd32_sysent.c freebsd32_syscalls.c freebsd32_syscall.h freebsd32_proto.h: \
	    ../../kern/makesyscalls.sh syscalls.master syscalls.conf
	-mv -f freebsd32_sysent.c freebsd32_sysent.c.bak
	-mv -f freebsd32_syscalls.c freebsd32_syscalls.c.bak
	-mv -f freebsd32_syscall.h freebsd32_syscall.h.bak
	-mv -f freebsd32_proto.h freebsd32_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf

clean:
	rm -f freebsd32_sysent.c freebsd32_syscalls.c freebsd32_syscall.h freebsd32_proto.h
