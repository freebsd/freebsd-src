#
# $FreeBSD$
#

T4FW = ${.CURDIR}/../../../dev/cxgbe/firmware
.PATH: ${T4FW}

KMOD = t4fw_cfg
FIRMWS = ${KMOD}.txt:${KMOD}:1.0.0.0

# You can have additional configuration files in the ${T4FW} directory.
# t4fw_cfg_<name>.txt
CFG_FILES != cd ${T4FW} && echo ${KMOD}_*.txt
.for F in ${CFG_FILES}
.if exists(${F})
FIRMWS += ${F}:${F:C/.txt//}:1.0.0.0
.endif
.endfor

# The firmware binary is optional.
# t4fw-<a>.<b>.<c>.<d>.bin
FW_BIN != cd ${T4FW} && echo t4fw-*.bin
.if exists(${FW_BIN})
FIRMWS += ${FW_BIN}:t4fw:${FW_BIN:C/t4fw-//:C/.bin//}
.endif

.include <bsd.kmod.mk>
