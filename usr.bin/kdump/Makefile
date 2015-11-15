#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump_subr.c kdump.c ioctl.c subr.c utrace.c
DPSRCS=		kdump_subr.h
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../.. -I.

.if ${MK_CASPER} != "no"
LIBADD=	capsicum
CFLAGS+=-DHAVE_LIBCAPSICUM
.endif

.if ${MK_PF} != "no"
CFLAGS+=-DPF
.endif

NO_WERROR?=	YES

CLEANFILES=	ioctl.c kdump_subr.c kdump_subr.h

beforedepend: ioctl.c

.if (${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386")
beforedepend: linux_syscalls.c

CLEANFILES+=	linux_syscalls.c
kdump.o: linux_syscalls.c
linux_syscalls.c:	linux_syscalls.conf
	sh ${.CURDIR}/../../sys/kern/makesyscalls.sh \
	    ${.CURDIR}/../../sys/${MACHINE_ARCH}/linux/syscalls.master ${.CURDIR}/linux_syscalls.conf
.endif
.if (${MACHINE_ARCH} == "amd64")
beforedepend: linux32_syscalls.c

CLEANFILES+=	linux32_syscalls.c
kdump.o: linux32_syscalls.c
linux32_syscalls.c: linux32_syscalls.conf
	sh ${.CURDIR}/../../sys/kern/makesyscalls.sh \
	    ${.CURDIR}/../../sys/${MACHINE_ARCH}/linux32/syscalls.master ${.CURDIR}/linux32_syscalls.conf
.endif

ioctl.c: mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
	    sh ${.CURDIR}/mkioctls print ${DESTDIR}${INCLUDEDIR} > ${.TARGET}

kdump_subr.h: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}${INCLUDEDIR} | \
	    sed -n 's/^\([a-z].*)\)$$/void \1;/p' >${.TARGET}

kdump_subr.c: mksubr kdump_subr.h
	sh ${.CURDIR}/mksubr ${DESTDIR}${INCLUDEDIR} >${.TARGET}

.include <bsd.prog.mk>
