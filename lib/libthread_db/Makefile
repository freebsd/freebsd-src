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

CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../libexec/rtld-elf
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf/${MACHINE_ARCH}
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
CFLAGS+=-DTLS_DTV_AT_TCB
.else
CFLAGS+=-DTLS_DTV_AT_TP
.endif

SRCS+=	libpthread.h
CLEANFILES+= libpthread.h

LIBPTHREAD= ${.CURDIR}/../libpthread
LIBPTHREAD_ARCH= ${LIBPTHREAD}/arch/${MACHINE_ARCH}/include

libpthread.h:
	@echo '#define LIBTHREAD_DB 1' > ${.TARGET}
	@echo '#include "${LIBPTHREAD}/sys/lock.h"' >> ${.TARGET}
	@echo '#include "${LIBPTHREAD_ARCH}/pthread_md.h"' >> ${.TARGET}
	@echo '#include "${LIBPTHREAD}/thread/thr_private.h"' >> ${.TARGET}

.include <bsd.lib.mk>
