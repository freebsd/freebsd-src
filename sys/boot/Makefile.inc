# $FreeBSD$

.include <src.opts.mk>

.if !defined(__BOOT_MAKEFILE_INC__)
__BOOT_MAKEFILE_INC__=${MFILE}

SASRC=${SRCTOP}/lib/libstand
# Normal stand alone library
LIBSA=${OBJTOP}/sys/boot/libsa/libstand.a
# stand alone library compiled for 32-bit version of the processor
LIBSA32=${OBJTOP}/sys/boot/libstand32/libstand.a
# stand along library compiled for userboot
LIBSAU=${OBJTOP}/sys/boot/userboot/libstand/libstand.a

CFLAGS+=-I${SASRC}

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

# The boot loader build uses dd status=none, where possible, for reproducible
# build output (since performance varies from run to run). Trouble is that
# option was recently (10.3) added to FreeBSD and is non-standard. Only use it
# when this test succeeds rather than require dd to be a bootstrap tool.
DD_NOSTATUS!=(dd status=none count=0 2> /dev/null && echo status=none) || true
DD=dd ${DD_NOSTATUS}

.if ${MK_LOADER_FORCE_LE} != "no"
.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-mlittle-endian
.endif
.endif

.endif
