# $FreeBSD$

.include <src.opts.mk>

.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif

# XXXRW: And arguable also mips64-dependent
.if ${MK_CHERI} != "no"
SUBDIR+=	cheri
.endif

.include <bsd.subdir.mk>
