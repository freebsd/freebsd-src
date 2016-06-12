# $FreeBSD$

LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c

CFLAGS+=	-DBOOTPROG=\"zfsloader\"
CFLAGS+=	-I${.CURDIR}/../common -I${.CURDIR}/../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../../cddl/boot/zfs

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-march=i386
.endif
.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-m32
.endif

CFLAGS+=	-Wformat -Wall

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+=    machine
machine: .NOMETA
	ln -sf ${.CURDIR}/../../i386/include machine
.endif

.include <bsd.stand.mk>
.include <bsd.lib.mk>

.if ${MACHINE_CPUARCH} == "amd64"
.if !exists(machine)
beforedepend ${OBJS}: machine
.endif
.endif
