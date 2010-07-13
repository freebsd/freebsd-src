# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}

LIB=	thread_db
SHLIB_MAJOR= 2
SRCS=	thread_db.c
SRCS+=	libpthread_db.c libpthread_md.c
# libc_r not supported on arm
.if ${MACHINE_ARCH} != "arm" && ${MACHINE_ARCH} != "ia64" && \
    ${MACHINE_ARCH} != "poewrpc" && !defined(NO_LIBC_R)
SRCS+=	libc_r_db.c libc_r_md.c
.endif
SRCS+=	libthr_db.c
INCS=	thread_db.h
WARNS?= 6

CFLAGS+=-I. -I${.CURDIR}

.include <bsd.lib.mk>
