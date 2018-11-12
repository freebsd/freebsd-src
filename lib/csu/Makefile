# $FreeBSD$

ARCH= ${MACHINE_ARCH:S/i386/i386-elf/}
.if exists(${.CURDIR}/${ARCH})
SUBDIR+= ${ARCH}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif

.include <bsd.subdir.mk>
