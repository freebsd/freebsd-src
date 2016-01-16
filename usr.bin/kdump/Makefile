#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump_subr.c kdump_subr.h kdump.c ioctl.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../.. -I.

.if ${MK_PF} != "no"
CFLAGS+=-DPF
.endif

NO_WERROR?=	YES

CLEANFILES=	ioctl.c kdump_subr.c kdump_subr.h

.if (${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386")
CLEANFILES+=	linux_syscalls.c
.endif
.if (${MACHINE_ARCH} == "amd64")
CLEANFILES+=	linux32_syscalls.c
.endif

ioctl.c: mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
	    sh ${.CURDIR}/mkioctls print ${DESTDIR}/usr/include > ${.TARGET}

kdump_subr.h: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include | \
	    sed -n 's/^\([a-z].*)\)$$/void \1;/p' >${.TARGET}

kdump_subr.c: mksubr kdump_subr.h
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include >${.TARGET}
.if (${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386")
	sh ${.CURDIR}/../../sys/kern/makesyscalls.sh \
	    ${.CURDIR}/../../sys/${MACHINE_ARCH}/linux/syscalls.master ${.CURDIR}/linux_syscalls.conf
.endif
.if (${MACHINE_ARCH} == "amd64")
	sh ${.CURDIR}/../../sys/kern/makesyscalls.sh \
	    ${.CURDIR}/../../sys/${MACHINE_ARCH}/linux32/syscalls.master ${.CURDIR}/linux32_syscalls.conf
.endif

.include <bsd.prog.mk>
