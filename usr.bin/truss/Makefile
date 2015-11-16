# $FreeBSD$

NO_WERROR=
PROG=	truss
SRCS=	cloudabi.c ioctl.c main.c setup.c syscalls.c

.PATH:	${.CURDIR:H}/kdump
SRCS+=	utrace.c

CFLAGS+= -I${.CURDIR} -I. -I${.CURDIR}/../../sys
CLEANFILES= ioctl.c

ioctl.c: ${.CURDIR}/../kdump/mkioctls
	env MACHINE=${MACHINE} CPP="${CPP}" \
		/bin/sh ${.CURDIR}/../kdump/mkioctls return ${DESTDIR}${INCLUDEDIR} > ${.TARGET}

# Define where to generate syscalls for each ABI.
ABI_SYSPATH.freebsd=		sys/kern
ABI_SYSPATH.freebsd32=		sys/compat/freebsd32
ABI_SYSPATH.cloudabi64=		sys/compat/cloudabi64
ABI_SYSPATH.i386-linux=		sys/i386/linux
ABI_SYSPATH.amd64-linux32=	sys/amd64/linux32

ABIS+=		freebsd
# Each ABI is expected to have an ABI.c, MACHINE_ARCH-ABI.c or
# MACHINE_CPUARCH-ABI.c file that will be used to map the syscall arguments.
.if ${MACHINE_ARCH} == "aarch64"
ABIS+=		cloudabi64
.endif
.if ${MACHINE_CPUARCH} == "i386"
ABIS+=		i386-linux
.endif
.if ${MACHINE_CPUARCH} == "amd64"
ABIS+=		amd64-linux32
ABIS+=		freebsd32
ABIS+=		cloudabi64
.endif
.if ${MACHINE_ARCH} == "powerpc64"
ABIS+=		freebsd32
.endif

.for abi in ${ABIS}
# Find the right file to handle this ABI.
abi_src=
ABI_SRCS=	${abi}.c ${MACHINE_ARCH}-${abi}.c ${MACHINE_CPUARCH}-${abi}.c
.for f in ${ABI_SRCS}
.if exists(${.CURDIR}/${f}) && empty(abi_src)
abi_src=	${f}
.endif
.endfor
SRCS:=		${SRCS} ${abi_src} ${abi}_syscalls.h
CLEANFILES+=	${abi}_syscalls.conf ${abi}_syscalls.master ${abi}_syscalls.h
${abi}_syscalls.conf: ${.CURDIR}/makesyscallsconf.sh
	/bin/sh ${.CURDIR}/makesyscallsconf.sh ${abi} ${.TARGET}

${abi}_syscalls.master:	${.CURDIR:H:H}/${ABI_SYSPATH.${abi}}/syscalls.master
	cp -f ${.ALLSRC} ${.TARGET}

${abi}_syscalls.h:	${abi}_syscalls.master ${abi}_syscalls.conf \
			${.CURDIR:H:H}/sys/kern/makesyscalls.sh
	/bin/sh ${.CURDIR:H:H}/sys/kern/makesyscalls.sh \
	    ${abi}_syscalls.master ${abi}_syscalls.conf
# Eliminate compiler warning about non-static global.
	sed -i '' '/^const char \*/s/^/static /' ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor

.include <bsd.prog.mk>
