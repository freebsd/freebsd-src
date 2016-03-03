# $FreeBSD$

SSP_CFLAGS=

.if ${MACHINE_CPUARCH} == "arm"
# Do not generate movt/movw, because the relocation fixup for them does not
# translate to the -Bsymbolic -pie format required by self_reloc() in loader(8).
# Also, the fpu is not available in a standalone environment.
CFLAGS.clang+=  -mllvm -arm-use-movt=0
CFLAGS.clang+=  -mfpu=none
.endif
