# $FreeBSD$

LIB=pthread_dbg
SHLIB_MAJOR=1
CFLAGS+=-DPTHREAD_KERNEL
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}/../libpthread/thread \
	-I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/../libpthread/arch/${MACHINE_ARCH}/include
CFLAGS+=-I${.CURDIR}/../libpthread/sys
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}/include
CFLAGS+=-Wall

PRECIOUSLIB=yes

.PATH: ${.CURDIR}

SRCS+=pthread_dbg.c

.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"
.include <bsd.lib.mk>
