# Makefile for syscall tables
#
# $FreeBSD$

all:
	@echo "make sysent only"

sysent:  freebsd_oabi_sysent.c freebsd_oabi_syscall.h freebsd_oabi_proto.h

freebsd_oabi_sysent.c freebsd_oabi_syscalls.c freebsd_oabi_syscall.h \
freebsd_oabi_proto.h: \
	    ../../kern/makesyscalls.sh syscalls.master syscalls.conf
	-mv -f freebsd_oabi_sysent.c freebsd_oabi_sysent.c.bak
	-mv -f freebsd_oabi_syscalls.c freebsd_oabi_syscalls.c.bak
	-mv -f freebsd_oabi_syscall.h freebsd_oabi_syscall.h.bak
	-mv -f freebsd_oabi_proto.h freebsd_oabi_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf

clean:
	rm -f freebsd_oabi_sysent.c freebsd_oabi_syscalls.c
	rm -f freebsd_oabi_syscall.h freebsd_oabi_proto.h
