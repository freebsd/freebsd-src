# Makefile for syscall tables
#
# $FreeBSD$

all:
	@echo "make sysent only"

sysent:  svr4_sysent.c svr4_syscall.h svr4_proto.h

svr4_syscallnames.c svr4_sysent.c svr4_syscall.h svr4_proto.h: \
		../../kern/makesyscalls.sh syscalls.master syscalls.conf
	-mv -f svr4_syscallnames.c svr4_syscallnames.c.bak
	-mv -f svr4_sysent.c svr4_sysent.c.bak
	-mv -f svr4_syscall.h svr4_syscall.h.bak
	-mv -f svr4_proto.h svr4_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf

