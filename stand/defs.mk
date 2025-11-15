
.if !defined(__BOOT_DEFS_MK__)
__BOOT_DEFS_MK__=${MFILE}

FORTIFY_SOURCE=	0

# We need to define all the MK_ options before including src.opts.mk
# because it includes bsd.own.mk which needs the right MK_ values,
# espeically MK_CTF.

MK_CTF=		no
MK_SSP=		no
MK_PIE=		no
MK_ZEROREGS=	no
MAN=
.if !defined(PIC)
NO_PIC=
INTERNALLIB=
.endif
# Should be NO_CPU_FLAGS, but bsd.cpu.mk is included too early in bsd.init.mk
# via the early include of bsd.opts.mk. Moving Makefile.inc include earlier in
# that file causes weirdness, so this is the next best thing. We need to do this
# because the loader needs very specific flags to work right, and things like
# CPUTYPE?=native prevent that, and introduce an endless game of whack-a-mole
# to disable more and more features. Boot loader performance is never improved
# enough to make that hassle worth chasing.
_CPUCFLAGS=

.if ${LDFLAGS:M-nostdlib}
# Sanitizers won't work unless we link against libc (e.g. in userboot/test).
MK_ASAN:=	no
MK_UBSAN:=	no
.endif

.include <src.opts.mk>
.include <bsd.linker.mk>

WARNS?=		1

BOOTSRC=	${SRCTOP}/stand
EFISRC=		${BOOTSRC}/efi
EFIINC=		${EFISRC}/include
# For amd64, there's a bit of mixed bag. Some of the tree (i386, lib*32) is
# built 32-bit and some 64-bit (lib*, efi). Centralize all the 32-bit magic here
# and activate it when DO32 is explicitly defined to be 1.
.if ${MACHINE_ARCH} == "amd64" && ${DO32:U0} == 1
EFIINCMD=	${EFIINC}/i386
.else
EFIINCMD=	${EFIINC}/${MACHINE}
.endif
FDTSRC=		${BOOTSRC}/fdt
FICLSRC=	${BOOTSRC}/ficl
LDRSRC=		${BOOTSRC}/common
LIBLUASRC=	${BOOTSRC}/liblua
LIBOFWSRC=	${BOOTSRC}/libofw
LUASRC=		${SRCTOP}/contrib/lua/src
SASRC=		${BOOTSRC}/libsa
SYSDIR=		${SRCTOP}/sys
UBOOTSRC=	${BOOTSRC}/uboot
ZFSSRC=		${SASRC}/zfs
OZFS=		${SRCTOP}/sys/contrib/openzfs
ZFSOSSRC=	${OZFS}/module/os/freebsd/
ZFSOSINC=	${OZFS}/include/os/freebsd
LIBCSRC=	${SRCTOP}/lib/libc

BOOTOBJ=	${OBJTOP}/stand

# BINDIR is where we install
BINDIR?=	/boot

# LUAPATH is where we search for and install lua scripts.
LUAPATH?=	/boot/lua
FLUASRC?=	${SRCTOP}/libexec/flua
FLUALIB?=	${SRCTOP}/libexec/flua

LIBSA=		${BOOTOBJ}/libsa/libsa.a
.if ${MACHINE} == "i386"
LIBSA32=	${LIBSA}
.else
LIBSA32=	${BOOTOBJ}/libsa32/libsa32.a
.endif

# Standard options:
CFLAGS+=	-nostdinc
# Allow CFLAGS_EARLY.file/target so that code that needs specific stack
# of include paths can set them up before our include paths. Normally
# the only thing that should be there are -I directives, and as few of
# those as possible.
CFLAGS+=	${CFLAGS_EARLY} ${CFLAGS_EARLY.${.IMPSRC:T}} ${CFLAGS_EARLY.${.TARGET:T}}
.if ${MACHINE_ARCH} == "amd64" && ${DO32:U0} == 1
CFLAGS+=	-I${BOOTOBJ}/libsa32
.else
CFLAGS+=	-I${BOOTOBJ}/libsa
.endif
CFLAGS+=	-I${SASRC} -D_STANDALONE
CFLAGS+=	-I${SYSDIR}
# Spike the floating point interfaces
CFLAGS+=	-Ddouble=jagged-little-pill -Dfloat=floaty-mcfloatface
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
# Slim down the image. This saves about 15% in size with clang 6 on x86
# Our most constrained /boot/loader env is BIOS booting on x86, where
# our text + data + BTX have to fit into 640k below the ISA hole.
# Experience has shown that problems arise between ~520k to ~530k.
CFLAGS.clang+=	-Oz
CFLAGS.gcc+=	-Os
CFLAGS+=	-ffunction-sections -fdata-sections
.endif

# GELI Support, with backward compat hooks (mostly)
.if defined(LOADER_NO_GELI_SUPPORT)
MK_LOADER_GELI=no
.warning "Please move from LOADER_NO_GELI_SUPPORT to WITHOUT_LOADER_GELI"
.endif
.if defined(LOADER_GELI_SUPPORT)
MK_LOADER_GELI=yes
.warning "Please move from LOADER_GELI_SUPPORT to WITH_LOADER_GELI"
.endif
.if ${MK_LOADER_GELI} == "yes"
CFLAGS+=	-DLOADER_GELI_SUPPORT
CFLAGS+=	-I${SASRC}/geli
.endif # MK_LOADER_GELI

# These should be confined to loader.mk, but can't because uboot/lib
# also uses it. It's part of loader, but isn't a loader so we can't
# just include loader.mk
.if ${LOADER_DISK_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_DISK_SUPPORT
.endif

# Machine specific flags for all builds here

# Ensure PowerPC64 and PowerPC64LE boot loaders are compiled as 32 bit.
# PowerPC64LE boot loaders are 32-bit little-endian.
.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc -mbig-endian
.elif ${MACHINE_ARCH} == "powerpc64le"
CFLAGS+=	-m32 -mcpu=powerpc -mlittle-endian
.endif

.if ${MACHINE_ARCH} == "amd64" && ${DO32:U0} == 1
CFLAGS+=	-m32
# LD_FLAGS is passed directly to ${LD}, not via ${CC}:
LD_FLAGS+=	-m elf_i386_fbsd
AFLAGS+=	--32
.endif

# Add in the no float / no SIMD stuff and announce we're freestanding
# aarch64 and riscv don't have -msoft-float, but all others do.
CFLAGS+=	-ffreestanding ${CFLAGS_NO_SIMD}
.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-mgeneral-regs-only -ffixed-x18 -fPIC
.elif ${MACHINE_CPUARCH} == "riscv"
CFLAGS+=	-march=rv64imac -mabi=lp64 -fPIC
CFLAGS.clang+=	-mcmodel=medium
CFLAGS.gcc+=	-mcmodel=medany
.else
CFLAGS+=	-msoft-float
.endif

# -msoft-float seems to be insufficient for powerpcspe
.if ${MACHINE_ARCH} == "powerpcspe"
CFLAGS+=	-mno-spe
.endif

.if ${MACHINE_CPUARCH} == "i386" || (${MACHINE_CPUARCH} == "amd64" && ${DO32:U0} == 1)
CFLAGS+=	-march=i386
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
.endif
.if ${MACHINE_CPUARCH} == "amd64" && ${DO32:U0} == 0
CFLAGS+=	-fPIC -mno-red-zone
.endif

.if ${MACHINE_CPUARCH} == "arm"
# Do not generate movt/movw, because the relocation fixup for them does not
# translate to the -Bsymbolic -pie format required by self_reloc() in loader(8).
# Also, the fpu is not available in a standalone environment.
CFLAGS.clang+=	-mno-movt
CFLAGS.clang+=  -mfpu=none
CFLAGS+=	-fPIC
.endif

# Some RISC-V linkers have support for relaxations, while some (lld) do not
# yet. If this is the case we inhibit the compiler from emitting relaxations.
.if ${LINKER_FEATURES:Mriscv-relaxations} == ""
CFLAGS+=	-mno-relax
.endif

# The boot loader build uses dd status=none, where possible, for reproducible
# build output (since performance varies from run to run). Trouble is that
# option was recently (10.3) added to FreeBSD and is non-standard. Only use it
# when this test succeeds rather than require dd to be a bootstrap tool.
DD_NOSTATUS!=(dd status=none count=0 2> /dev/null && echo status=none) || true
DD=dd ${DD_NOSTATUS}

#
# Have a sensible default
#
.if ${MK_LOADER_LUA} == "yes"
LOADER_DEFAULT_INTERP?=lua
.elif ${MK_FORTH} == "yes"
LOADER_DEFAULT_INTERP?=4th
.else
LOADER_DEFAULT_INTERP?=simp
.endif
LOADER_INTERP?=${LOADER_DEFAULT_INTERP}

# Make sure we use the machine link we're about to create
CFLAGS+=-I.

.include "${BOOTSRC}/veriexec.mk"

all: ${PROG}

CLEANFILES+= teken_state.h
teken.c: teken_state.h

teken_state.h: ${SYSDIR}/teken/sequences
	awk -f ${SYSDIR}/teken/gensequences \
		${SYSDIR}/teken/sequences > teken_state.h

.if !defined(NO_OBJ)
_ILINKS=include/machine
.if ${MACHINE} != ${MACHINE_CPUARCH} && ${MACHINE} != "arm64"
_ILINKS+=include/${MACHINE_CPUARCH}
.endif
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_ILINKS+=include/x86
.endif
CFLAGS+= -Iinclude
CLEANDIRS+= include

beforedepend: ${_ILINKS}
beforebuild: ${_ILINKS}

# Ensure that the links exist without depending on it when it exists which
# causes all the modules to be rebuilt when the directory pointed to changes.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${OBJS}:       ${_link}
.endif # _link exists
.endfor

.NOPATH: ${_ILINKS}

${_ILINKS}: .NOMETA
	@case ${.TARGET:T} in \
	machine) \
		if [ ${DO32:U0} -eq 0 ]; then \
			path=${SYSDIR}/${MACHINE}/include ; \
		else \
			path=${SYSDIR}/${MACHINE:C/amd64/i386/}/include ; \
		fi ;; \
	*) \
		path=${SYSDIR}/${.TARGET:T}/include ;; \
	esac ; \
	case ${.TARGET} in \
	*/*) mkdir -p ${.TARGET:H};; \
	esac ; \
	path=`(cd $$path && /bin/pwd)` ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -fns $$path ${.TARGET}
.endif # !NO_OBJ

.-include "local.defs.mk"
.endif # __BOOT_DEFS_MK__
