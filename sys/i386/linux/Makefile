# Makefile for syscall tables
#
# $Id: Makefile,v 1.3 1997/03/29 11:17:26 peter Exp $

all:
	@echo "make linux_sysent.c only"

linux_sysent.c linux_syscall.h linux_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f linux_sysent.c linux_sysent.c.bak
	-mv -f linux_syscall.h linux_syscall.h.bak
	-mv -f linux_proto.h linux_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
