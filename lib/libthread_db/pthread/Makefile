# $FreeBSD$

LIB=pthread_db
INTERNALLIB=yes
NOPROFILE=yes
NOPIC=yes
CFLAGS+=-DPTHREAD_KERNEL -fpic -DPIC
CFLAGS+=-I${.CURDIR}/../include \
	-I${.CURDIR}/../../libc/include -I${.CURDIR}/../../libpthread/thread \
	-I${.CURDIR}/../../../include
CFLAGS+=-I${.CURDIR}/../../libpthread/arch/${MACHINE_ARCH}/include
CFLAGS+=-I${.CURDIR}/../../libpthread/sys
CFLAGS+=-Wall

.PATH: ${.CURDIR}

SRCS+=pthread_db.c pthread_db_${MACHINE_ARCH}.c

.include <bsd.lib.mk>
