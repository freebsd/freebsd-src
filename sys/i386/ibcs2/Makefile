# Makefile for syscall tables
#
# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

MAKESYSCALLS=		../../kern/makesyscalls.sh
BASE_GENERATED=		ibcs2_syscall.h		\
			ibcs2_sysent.c		\
			ibcs2_proto.h
ISC_GENERATED=		ibcs2_isc_syscall.h	\
			ibcs2_isc_sysent.c
XENIX_GENERATED=	ibcs2_xenix_syscall.h	\
			ibcs2_xenix_sysent.c	\
			ibcs2_xenix_proto.h

all:
	@echo "make sysent, isc_sysent or xenix_sysent only"

.ORDER: ${BASE_GENERATED}
.ORDER: ${ISC_GENERATED}
.ORDER: ${XENIX_GENERATED}

sysent:  ${BASE_GENERATED}
isc_sysent: ${ISC_GENERATED}
xenix_sysent: ${XENIX_GENERATED}

${BASE_GENERATED}: ${MAKESYSCALLS} syscalls.master syscalls.conf
	sh ${MAKESYSCALLS} syscalls.master syscalls.conf

${ISC_GENERATED}: ${MAKESYSCALLS} syscalls.isc syscalls.isc.conf
	sh ${MAKESYSCALLS} syscalls.isc syscalls.isc.conf

${XENIX_GENERATED}: ${MAKESYSCALLS} syscalls.xenix syscalls.xenix.conf
	sh ${MAKESYSCALLS} syscalls.xenix syscalls.xenix.conf
