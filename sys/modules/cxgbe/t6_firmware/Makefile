#
# $FreeBSD$
#

T6FW=	${SRCTOP}/sys/dev/cxgbe/firmware
.PATH: ${T6FW}

KMOD=	t6fw_cfg
FIRMWS=	${KMOD}.txt:${KMOD}:1.0.0.0

# You can have additional configuration files in the ${T6FW} directory.
# t6fw_cfg_<name>.txt
CFG_FILES != cd ${T6FW} && echo ${KMOD}_*.txt
.for F in ${CFG_FILES}
.if exists(${F})
FIRMWS+=	${F}:${F:C/.txt//}:1.0.0.0
.endif
.endfor

T6FW_VER=	1.26.6.0
FIRMWS+=	t6fw-${T6FW_VER}.bin:t6fw:${T6FW_VER}

.include <bsd.kmod.mk>
