# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}

LIB=	thread_db
SHLIB_MAJOR= 2
SRCS=	thread_db.c
SRCS+=	libpthread_db.c libpthread_md.c
SRCS+=	libthr_db.c
INCS=	thread_db.h
WARNS?= 1

CFLAGS+=-I. -I${.CURDIR}
SYM_MAPS+=${.CURDIR}/Symbol.map

.if defined(SYMVER_ENABLED)
SYMBOL_MAPS=${SYM_MAPS}
VERSION_DEF=${.CURDIR}/../libc/Versions.def
.endif

.include <bsd.lib.mk>
