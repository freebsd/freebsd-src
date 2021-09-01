#	@(#)Makefile	8.2 (Berkeley) 3/21/94
# $FreeBSD$
#
# Makefile for init_sysent

SYSENT_CONF=
GENERATED=	init_sysent.c			\
		syscalls.c			\
		systrace_args.c			\
		${SYSDIR}/sys/syscall.h		\
		${SYSDIR}/sys/syscall.mk	\
		${SYSDIR}/sys/sysproto.h

.include "../conf/sysent.mk"
