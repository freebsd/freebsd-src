# $FreeBSD$

PROG=	truss
SRCS=	main.c setup.c syscalls.c syscalls.h ioctl.c
.if (${MACHINE_ARCH} == "alpha")
SRCS+=	alpha-fbsd.c
.elif (${MACHINE_ARCH} == "i386")
SRCS+=	i386-fbsd.c i386-linux.c linux_syscalls.h
.endif

WARNS?=	1
NO_WERROR=yes

CFLAGS+= -I${.CURDIR} -I.
CLEANFILES+=i386l-syscalls.master syscalls.master linux_syscalls.h \
	syscalls.h ioctl.c

.SUFFIXES: .master

i386l-syscalls.master:	${.CURDIR}/../../sys/i386/linux/syscalls.master
	cp ${.ALLSRC} i386l-syscalls.master

linux_syscalls.h:	i386l-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh i386l-syscalls.master \
		${.CURDIR}/i386linux.conf

syscalls.master:	${.CURDIR}/../../sys/kern/syscalls.master
	cp ${.ALLSRC} syscalls.master

syscalls.h:	syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh syscalls.master \
		${.CURDIR}/i386.conf

.if make(depend)
ioctl.c:	.PHONY
.else
ioctl.c:	${.CURDIR}/../kdump/mkioctls
.endif
	sh ${.CURDIR}/../kdump/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>
