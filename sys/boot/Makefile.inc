# $FreeBSD$

.include <bsd.own.mk>

SSP_CFLAGS=

.if ${MACHINE_CPUARCH} == "arm"
# Do not generate movt/movw, because the relocation fixup for them does not
# translate to the -Bsymbolic -pie format required by self_reloc() in loader(8).
# Also, the fpu is not available in a standalone environment.
.if ${COMPILER_VERSION} < 30800
CFLAGS.clang+=	-mllvm -arm-use-movt=0
.else
CFLAGS.clang+=	-mno-movt
.endif
CFLAGS.clang+=  -mfpu=none
.endif
