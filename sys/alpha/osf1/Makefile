# $FreeBSD: src/sys/alpha/osf1/Makefile,v 1.2.34.1.10.1 2008/10/02 02:57:24 kensmith Exp $

# Makefile for syscall tables.

all:
	@echo "make sysent only"

sysent:  osf1_sysent.c osf1_syscall.h osf1_proto.h

osf1_sysent.c osf1_syscall.h osf1_proto.h: ../../kern/makesyscalls.sh \
    syscalls.master syscalls.conf
	-mv -f osf1_sysent.c osf1_sysent.c.bak
	-mv -f osf1_syscall.h osf1_syscall.h.bak
	-mv -f osf1_proto.h osf1_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
