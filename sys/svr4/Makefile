# Makefile for syscall tables
#
# $Id: Makefile,v 1.3 1997/03/29 11:17:26 peter Exp $

all:
	@echo "make svr4_sysent.c only"

svr4_sysent.c svr4_syscall.h svr4_proto.h: ../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f svr4_sysent.c svr4_sysent.c.bak
	-mv -f svr4_syscall.h svr4_syscall.h.bak
	-mv -f svr4_proto.h svr4_proto.h.bak
	sh ../kern/makesyscalls.sh syscalls.master syscalls.conf
