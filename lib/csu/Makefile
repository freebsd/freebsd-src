# $FreeBSD$

.include <src.opts.mk>

.if !defined(LIBCHERI)
.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif
.endif

# XXXRW: And arguable also mips64-dependent
.if ${MK_CHERI} != "no"
SUBDIR+=	cheri
.endif

.include <bsd.subdir.mk>
