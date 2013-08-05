#
# $FreeBSD$
#

SUBDIR = if_cxgbe
SUBDIR+= t4_firmware
SUBDIR+= t5_firmware
SUBDIR+= ${_tom}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
.endif

.include <bsd.subdir.mk>
