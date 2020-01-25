# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

MAKESYSCALLS=	../../kern/makesyscalls.sh
GENERATED=	svr4_sysent.c		\
		svr4_syscall.h		\
		svr4_syscallnames.c	\
		svr4_proto.h

all:
	@echo "make sysent only"

.ORDER: ${GENERATED}
sysent:  ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} syscalls.master syscalls.conf
	sh ${MAKESYSCALLS} syscalls.master syscalls.conf

