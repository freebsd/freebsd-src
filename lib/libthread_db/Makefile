# $FreeBSD: src/lib/libthread_db/Makefile,v 1.5.8.1.10.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}

LIB=	thread_db
SHLIB_MAJOR= 2
SRCS=	thread_db.c
SRCS+=	libpthread_db.c libpthread_md.c
SRCS+=	libc_r_db.c libc_r_md.c
SRCS+=	libthr_db.c
INCS=	thread_db.h
WARNS?= 1

CFLAGS+=-I. -I${.CURDIR}

.include <bsd.lib.mk>
