#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.if (${MACHINE_ARCH} == "amd64")
SFX=		32
.endif

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump_subr.c kdump.c ioctl.c subr.c
DPSRCS=		kdump_subr.h 
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../.. -I.

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+=		linux_syscalls.c
.endif

NO_WERROR?=	YES

CLEANFILES=	ioctl.c kdump_subr.c kdump_subr.h linux_syscalls.c

ioctl.c: mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
	    sh ${.CURDIR}/mkioctls print ${DESTDIR}/usr/include > ${.TARGET}

kdump_subr.h: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include | \
	    sed -n 's/^\([a-z].*)\)$$/void \1;/p' >${.TARGET}

kdump_subr.c: mksubr kdump_subr.h
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include >${.TARGET}

linux_syscalls.c:
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh \
	    ${.CURDIR}/../../sys/${MACHINE_ARCH}/linux${SFX}/syscalls.master ${.CURDIR}/linux_syscalls.conf
	echo "int nlinux_syscalls = sizeof(linux_syscallnames) / sizeof(linux_syscallnames[0]);" \
	    >> linux_syscalls.c

.include <bsd.prog.mk>
