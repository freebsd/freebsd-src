# $FreeBSD$

MAN=

.include <src.opts.mk>

MK_SSP=		no

PROG=		boot1.sym
INTERNALPROG=
WARNS?=		6

# We implement a slightly non-standard %S in that it always takes a
# CHAR16 that's common in UEFI-land instead of a wchar_t. This only
# seems to matter on arm64 where wchar_t defaults to an int instead
# of a short. There's no good cast to use here so just ignore the
# warnings for now.
CWARNFLAGS.boot1.c+=	-Wno-format

# Disable warnings that are currently incompatible with the zfs boot code
CWARNFLAGS.zfs_module.c += -Wno-array-bounds
CWARNFLAGS.zfs_module.c += -Wno-cast-align
CWARNFLAGS.zfs_module.c += -Wno-cast-qual
CWARNFLAGS.zfs_module.c += -Wno-missing-prototypes
CWARNFLAGS.zfs_module.c += -Wno-sign-compare
CWARNFLAGS.zfs_module.c += -Wno-unused-parameter
CWARNFLAGS.zfs_module.c += -Wno-unused-function
CWARNFLAGS.skein.c += -Wno-cast-align
.if ${COMPILER_TYPE} == "clang"
CWARNFLAGS.skein.c += -Wno-missing-variable-declarations
.else if ${COMPILER_TYPE} == "gcc"
CWARNFLAGS.skein.c += -Wno-missing-declarations
.endif

# architecture-specific loader code
SRCS=	boot1.c self_reloc.c start.S ufs_module.c
.if ${MK_ZFS} != "no"
SRCS+=		zfs_module.c
SRCS+=		skein.c skein_block.c
# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111
.PATH:		${.CURDIR}/../../../crypto/skein
.endif

.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} > 40201
CWARNFLAGS.self_reloc.c+=	-Wno-error=maybe-uninitialized
.endif

CFLAGS+=	-I.
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica/include
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-DEFI_UFS_BOOT
.ifdef(EFI_DEBUG)
CFLAGS+=	-DEFI_DEBUG
.endif

.if ${MK_ZFS} != "no"
CFLAGS+=	-I${.CURDIR}/../../zfs/
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs/
CFLAGS+=	-I${.CURDIR}/../../../crypto/skein
CFLAGS+=	-DEFI_ZFS_BOOT
.endif

# Always add MI sources and REGULAR efi loader bits
.PATH:		${.CURDIR}/../loader/arch/${MACHINE}
.PATH:		${.CURDIR}/../loader
.PATH:		${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	boot1.efi boot1.efifat
FILESMODE_boot1.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/../loader/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS+=	-Wl,-T${LDSCRIPT},-Bsymbolic,-znotext -shared

.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-mgeneral-regs-only
.endif
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-fPIC
LDFLAGS+=	-Wl,-znocombreloc
.endif

LIBEFI=		${.OBJDIR}/../libefi/libefi.a

#
# Add libstand for the runtime functions used by the compiler - for example
# __aeabi_* (arm) or __divdi3 (i386).
# as well as required string and memory functions for all platforms.
#
DPADD+=		${LIBEFI} ${LIBSTAND}
LDADD+=		${LIBEFI} -lstand

DPADD+=		${LDSCRIPT}

NM?=		nm
OBJCOPY?=	objcopy

.if ${MACHINE_CPUARCH} == "amd64"
EFI_TARGET=	efi-app-x86_64
.elif ${MACHINE_CPUARCH} == "i386"
EFI_TARGET=	efi-app-ia32
.else
EFI_TARGET=	binary
.endif

# Arbitrarily set the PE/COFF header timestamps to 1 Jan 2016 00:00:00
# for build reproducibility.
SOURCE_DATE_EPOCH?=1451606400
boot1.efi: ${PROG}
	if ${NM} ${.ALLSRC} | grep ' U '; then \
		echo "Undefined symbols in ${.ALLSRC}"; \
		exit 1; \
	fi
	SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH} \
	${OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

boot1.o: ${.CURDIR}/../../common/ufsread.c

# The following inserts our objects into a template FAT file system
# created by generate-fat.sh

.include "${.CURDIR}/Makefile.fat"

boot1.efifat: boot1.efi
	@set -- `ls -l ${.ALLSRC}`; \
	x=$$(($$5-${BOOT1_MAXSIZE})); \
	if [ $$x -ge 0 ]; then \
	    echo "boot1 $$x bytes too large; regenerate FAT templates?" >&2 ;\
	    exit 1; \
	fi
	echo ${.OBJDIR}
	uudecode ${.CURDIR}/fat-${MACHINE}.tmpl.bz2.uu
	mv fat-${MACHINE}.tmpl.bz2 ${.TARGET}.bz2
	bzip2 -f -d ${.TARGET}.bz2
	${DD} if=${.ALLSRC} of=${.TARGET} seek=${BOOT1_OFFSET} conv=notrunc

CLEANFILES= boot1.efi boot1.efifat

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine

CLEANFILES+=   machine

machine: .NOMETA
	ln -sf ${.CURDIR}/../../../${MACHINE}/include machine

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
beforedepend ${OBJS}: x86
CLEANFILES+=   x86

x86: .NOMETA
	ln -sf ${.CURDIR}/../../../x86/include x86
.endif
