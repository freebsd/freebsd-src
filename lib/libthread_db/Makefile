# $FreeBSD: src/lib/libthread_db/Makefile,v 1.5 2004/08/16 03:30:16 davidxu Exp $

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
