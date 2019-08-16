# $FreeBSD$
# All the dts files for Marvell systems we support.

.if ${MACHINE_ARCH} == "armv7"
DTS=	\
	armada-388-clearfog.dts \
	armada-388-gp.dts
.elif ${MACHINE_ARCH} == "aarch64"
DTS=	\
	marvell/armada-3720-espressobin.dts
.endif

.include <bsd.dtb.mk>
