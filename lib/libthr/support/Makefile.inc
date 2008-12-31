# $FreeBSD: src/lib/libthr/support/Makefile.inc,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/support ${.CURDIR}/../libc/gen ${.CURDIR}/../libc/string
.PATH: ${.CURDIR}/../libc/${MACHINE_ARCH}/sys

CFLAGS+= -I${.CURDIR}/../libc/${MACHINE_ARCH}

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
