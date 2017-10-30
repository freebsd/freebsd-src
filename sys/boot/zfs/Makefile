# $FreeBSD$

.include <bsd.init.mk>

LIB=		zfsboot
INTERNALLIB=

.PATH:		${ZFSSRC}
SRCS+=		zfs.c skein.c skein_block.c
# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111
.PATH:		${SYSDIR}/crypto/skein

CFLAGS+=	-DBOOTPROG=\"zfsloader\"
CFLAGS+=	-I${LDRSRC} -I${SYSDIR} -I.
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

.if ${MACHINE_CPUARCH} == "i386" || \
	(${MACHINE_CPUARCH} == "amd64" && defined(ZFS32))
CFLAGS+=	-march=i386
.endif
.if ${MACHINE_CPUARCH} == "amd64" && defined(ZFS32)
CFLAGS+=	-m32
.endif

CFLAGS+=	-Wformat -Wall

.if ${MACHINE_CPUARCH} == "amd64" && defined(ZFS32)
CLEANFILES+=    machine
machine: .NOMETA
	ln -sf ${SYSDIR}/i386/include machine
.endif

.include <bsd.stand.mk>
.include <bsd.lib.mk>

.if ${MACHINE_CPUARCH} == "amd64" && defined(ZFS32)
.if !exists(machine)
beforedepend ${OBJS}: machine
.endif
.endif
