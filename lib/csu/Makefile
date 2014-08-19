# $FreeBSD$

.include <src.opts.mk>

ARCH= ${MACHINE_ARCH:S/i386/i386-elf/}
.if exists(${.CURDIR}/${ARCH})
SUBDIR+= ${ARCH}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif

# XXXRW: And arguable also mips64-dependent
.if ${MK_CHERI} != "no"
SUBDIR+=	cheri
.endif

.include <bsd.subdir.mk>
