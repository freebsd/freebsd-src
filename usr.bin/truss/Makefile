# $FreeBSD$

NO_WERROR=
PROG=	truss
SRCS=	main.c setup.c syscalls.c syscalls.h ioctl.c

.if exists(${.CURDIR}/${MACHINE_ARCH}-fbsd.c)
SRCS+= ${MACHINE_ARCH}-fbsd.c
.else
SRCS+= ${MACHINE_CPUARCH}-fbsd.c
.endif

.PATH:	${.CURDIR:H}/kdump
SRCS+=	utrace.c

CFLAGS+= -I${.CURDIR} -I. -I${.CURDIR}/../../sys
CLEANFILES= syscalls.master syscalls.h ioctl.c

.SUFFIXES: .master

syscalls.master:	${.CURDIR}/../../sys/kern/syscalls.master
	cat ${.ALLSRC} > syscalls.master

syscalls.h:	syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh syscalls.master \
		${.CURDIR}/i386.conf

ioctl.c: ${.CURDIR}/../kdump/mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
		/bin/sh ${.CURDIR}/../kdump/mkioctls return ${DESTDIR}${INCLUDEDIR} > ${.TARGET}

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	i386-linux.c linux_syscalls.h
CLEANFILES+=i386l-syscalls.master linux_syscalls.h

i386l-syscalls.master:	${.CURDIR}/../../sys/i386/linux/syscalls.master
	cat ${.ALLSRC} > ${.TARGET}

linux_syscalls.h:	i386l-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh ${.ALLSRC} \
		${.CURDIR}/i386linux.conf
.endif

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	amd64-linux32.c linux32_syscalls.h
CLEANFILES+=amd64l32-syscalls.master linux32_syscalls.h

amd64l32-syscalls.master: ${.CURDIR}/../../sys/amd64/linux32/syscalls.master
	cat ${.ALLSRC} > ${.TARGET}

linux32_syscalls.h:	amd64l32-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh ${.ALLSRC} \
		${.CURDIR}/amd64linux32.conf

SRCS+=	amd64-fbsd32.c freebsd32_syscalls.h
CLEANFILES+=fbsd32-syscalls.master freebsd32_syscalls.h

fbsd32-syscalls.master: ${.CURDIR}/../../sys/compat/freebsd32/syscalls.master
	cat ${.ALLSRC} > ${.TARGET}

freebsd32_syscalls.h:	fbsd32-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh ${.ALLSRC} \
		${.CURDIR}/fbsd32.conf

SRCS+=	amd64-cloudabi64.c cloudabi64_syscalls.h
CLEANFILES+=amd64cloudabi64-syscalls.master cloudabi64_syscalls.h

amd64cloudabi64-syscalls.master: ${.CURDIR}/../../sys/compat/cloudabi64/syscalls.master
	cat ${.ALLSRC} > ${.TARGET}

cloudabi64_syscalls.h:	amd64cloudabi64-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh ${.ALLSRC} \
		${.CURDIR}/amd64cloudabi64.conf
.endif

.if ${MACHINE_ARCH} == "powerpc64"
SRCS+=	powerpc-fbsd.c freebsd32_syscalls.h
CLEANFILES+=fbsd32-syscalls.master freebsd32_syscalls.h

fbsd32-syscalls.master: ${.CURDIR}/../../sys/compat/freebsd32/syscalls.master
	cat ${.ALLSRC} > ${.TARGET}

freebsd32_syscalls.h:	fbsd32-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh ${.ALLSRC} \
		${.CURDIR}/fbsd32.conf
.endif

.include <bsd.prog.mk>
