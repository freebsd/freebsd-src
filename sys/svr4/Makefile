# Makefile for syscall tables
#
# $FreeBSD: src/sys/svr4/Makefile,v 1.3 1999/09/01 00:32:18 newton Exp $

MAINTAINER=	newton@freebsd.org

all:
	@echo "make svr4_sysent.c only"

svr4_sysent.c svr4_syscall.h svr4_proto.h: ../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f svr4_sysent.c svr4_sysent.c.bak
	-mv -f svr4_syscall.h svr4_syscall.h.bak
	-mv -f svr4_proto.h svr4_proto.h.bak
	sh ../kern/makesyscalls.sh syscalls.master syscalls.conf
