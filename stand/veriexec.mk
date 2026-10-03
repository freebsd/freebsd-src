.if ${MK_LOADER_VERIEXEC} != "no"
.if exists(${VERSION_FILE}.veriexec)
VERSION_FILE:= ${VERSION_FILE}.veriexec
.endif
CFLAGS+= -DLOADER_VERIEXEC -I${SRCTOP}/lib/libsecureboot/h
.if ${MK_LOADER_VERIEXEC_VECTX} != "no"
CFLAGS+= -DLOADER_VERIEXEC_VECTX
.endif
.if ${MK_LOADER_VERIEXEC_PASS_MANIFEST} != "no"
CFLAGS+= -DLOADER_VERIEXEC_PASS_MANIFEST
.endif
# ELEVATED implies the strict threshold (the ACCEPT_NO_FP_DEFAULT cascade keys
# off either), and additionally turns verification on early and fail-closed.
.if ${MK_LOADER_VERIEXEC_STRICT} != "no" || ${MK_LOADER_VERIEXEC_ELEVATED} != "no"
CFLAGS+= -DLOADER_VERIEXEC_STRICT
.endif
.if ${MK_LOADER_VERIEXEC_ELEVATED} != "no"
CFLAGS+= -DLOADER_VERIEXEC_ELEVATED
.endif
.endif
