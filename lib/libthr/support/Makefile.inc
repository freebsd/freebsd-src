# $FreeBSD$

.PATH: ${.CURDIR}/support ${SRCTOP}/lib/libc/gen ${SRCTOP}/lib/libc/string

# libc must search machine_arch, then machine_cpuarch, but libthr has all its
# code implemented in machine_cpuarch.  Cope.
.if exists(${SRCTOP}/lib/libc/${MACHINE_ARCH}/sys)
.PATH: ${SRCTOP}/lib/libc/${MACHINE_ARCH}/sys
CFLAGS+= -I${SRCTOP}/lib/libc/${MACHINE_ARCH}
.else
.PATH: ${SRCTOP}/lib/libc/${MACHINE_CPUARCH}/sys
CFLAGS+= -I${SRCTOP}/lib/libc/${MACHINE_CPUARCH}
.endif

SYSCALLS= thr_new

SYSCALL_SRC=   ${SYSCALLS:S/$/.S/}
SYSCALL_OBJ=   ${SYSCALLS:S/$/.So/}

${SYSCALL_SRC}:
	printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' > ${.TARGET}

LIBC_OBJS=

SOBJS+=	thr_libc.So
CLEANFILES+= ${SYSCALL_SRC} ${SYSCALL_OBJ} ${LIBC_OBJS}

thr_libc.So: ${SYSCALL_OBJ} ${LIBC_OBJS}
	${CC} -fPIC -nostdlib -o ${.TARGET} -r ${.ALLSRC}
