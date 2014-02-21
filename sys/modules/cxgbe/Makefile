#
# $FreeBSD$
#

.include <bsd.own.mk>

SUBDIR = if_cxgbe
SUBDIR+= t4_firmware
SUBDIR+= t5_firmware
SUBDIR+= ${_tom}
SUBDIR+= ${_iw_cxgbe}

.if ${MACHINE_CPUARCH} == "amd64"
_tom=		tom
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_iw_cxgbe=	iw_cxgbe
.endif
.endif

.if ${MACHINE_CPUARCH} == "i386"
_tom=		tom
.endif

.include <bsd.subdir.mk>
