#
# $FreeBSD$
#

T7FW=	${SRCTOP}/sys/dev/cxgbe/firmware
.PATH: ${T7FW}

KMOD=	t7fw_cfg
FIRMWS=	${KMOD}.txt:${KMOD}:1.0.0.0

# You can have additional configuration files in the ${T7FW} directory.
# t7fw_cfg_<name>.txt
CFG_FILES != cd ${T7FW} && echo ${KMOD}_*.txt
.for F in ${CFG_FILES}
.if exists(${F})
FIRMWS+=	${F}:${F:C/.txt//}:1.0.0.0
.endif
.endfor

#T7FW_VER=	1.27.0.71
#FIRMWS+=	t7fw-${T7FW_VER}.bin:t7fw:${T7FW_VER}

.include <bsd.kmod.mk>
