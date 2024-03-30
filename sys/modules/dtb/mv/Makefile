# All the dts files for Marvell systems we support.

.if ${MACHINE_ARCH} == "armv7"
DTS=	\
	marvell/armada-388-clearfog.dts \
	marvell/armada-388-gp.dts
.elif ${MACHINE_ARCH} == "aarch64"
DTS=	\
	marvell/armada-3720-espressobin.dts \
	marvell/armada-8040-clearfog-gt-8k.dts \
	marvell/armada-8040-db \
	marvell/armada-8040-mcbin.dts \
	marvell/armada-8040-mcbin-singleshot.dts
.endif

.include <bsd.dtb.mk>
