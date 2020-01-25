# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

all:
	@echo "make sysent, isc_sysent or xenix_sysent only"

sysent:  ibcs2_sysent.c ibcs2_syscall.h ibcs2_proto.h

ibcs2_sysent.c ibcs2_syscall.h ibcs2_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf

isc_sysent:  ibcs2_isc_sysent.c ibcs2_isc_syscall.h

ibcs2_isc_sysent.c ibcs2_isc_syscall.h : ../../kern/makesyscalls.sh \
		syscalls.isc syscalls.isc.conf
	sh ../../kern/makesyscalls.sh syscalls.isc syscalls.isc.conf

xenix_sysent:  ibcs2_xenix_sysent.c ibcs2_xenix_syscall.h ibcs2_xenix.h

ibcs2_xenix_sysent.c ibcs2_xenix_syscall.h ibcs2_xenix.h: \
		../../kern/makesyscalls.sh syscalls.xenix syscalls.xenix.conf
	sh ../../kern/makesyscalls.sh syscalls.xenix syscalls.xenix.conf
