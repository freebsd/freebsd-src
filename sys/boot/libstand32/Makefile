# $FreeBSD$

.include <bsd.own.mk>

LIBSTAND_SRC=	${.CURDIR}/../../../lib/libstand

.if ${MACHINE_CPUARCH} == "amd64"
LIBSTAND_CPUARCH=i386
.else
LIBSTAND_CPUARCH=${MACHINE_CPUARCH}
.endif
LIBC_SRC=	${LIBSTAND_SRC}/../libc
INTERNALLIB=
INCS=
MAN=
.PATH:	${LIBSTAND_SRC}

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=        -m32 -I.
.endif

WARNS?=		0

.include "${LIBSTAND_SRC}/Makefile"

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+= machine
beforedepend ${OBJS}: machine
machine:
	ln -fs ${.CURDIR}/../../i386/include machine
.endif
