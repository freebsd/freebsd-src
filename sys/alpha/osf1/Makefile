# $FreeBSD$

# Makefile for syscall tables.

all:
	@echo "make osf1_sysent.c only"

osf1_sysent.c osf1_syscall.h osf1_proto.h: ../../kern/makesyscalls.sh \
    syscalls.master syscalls.conf
	-mv -f osf1_sysent.c osf1_sysent.c.bak
	-mv -f osf1_syscall.h osf1_syscall.h.bak
	-mv -f osf1_proto.h osf1_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
