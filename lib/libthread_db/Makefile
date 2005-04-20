# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}

LIB=	thread_db
SHLIB_MAJOR= 1
SRCS=	thread_db.c
SRCS+=	libpthread_db.c libpthread_md.c
SRCS+=	libc_r_db.c libc_r_md.c
SRCS+=	libthr_db.c
INCS=	thread_db.h
WARNS?= 1

CFLAGS+=-I. -I${.CURDIR}

.include <bsd.lib.mk>
