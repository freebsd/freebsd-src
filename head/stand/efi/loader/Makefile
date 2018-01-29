# $FreeBSD$

MAN=

LOADER_NET_SUPPORT?=	yes
LOADER_MSDOS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no

.include <bsd.init.mk>

MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=
WARNS?=		3

# architecture-specific loader code
SRCS=	autoload.c \
	bootinfo.c \
	conf.c \
	copy.c \
	efi_main.c \
	framebuffer.c \
	main.c \
	self_reloc.c \
	smbios.c \
	vers.c

.if ${MK_ZFS} != "no"
LIBZFSBOOT=	${BOOTOBJ}/zfs/libzfsboot.a
CFLAGS+=	-I${ZFSSRC}
CFLAGS+=	-DEFI_ZFS_BOOT
.endif

.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} > 40201
CWARNFLAGS.self_reloc.c+=	-Wno-error=maybe-uninitialized
.endif

# We implement a slightly non-standard %S in that it always takes a
# CHAR16 that's common in UEFI-land instead of a wchar_t. This only
# seems to matter on arm64 where wchar_t defaults to an int instead
# of a short. There's no good cast to use here so just ignore the
# warnings for now.
CWARNFLAGS.main.c+=	-Wno-format

.PATH: ${.CURDIR}/arch/${MACHINE}
# For smbios.c
.PATH: ${BOOTSRC}/i386/libi386
.include "${.CURDIR}/arch/${MACHINE}/Makefile.inc"

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/arch/${MACHINE}
CFLAGS+=	-I${EFISRC}/include
CFLAGS+=	-I${EFISRC}/include/${MACHINE}
CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
CFLAGS+=	-I${BOOTSRC}/i386/libi386
CFLAGS+=	-DNO_PCI -DEFI

.if !defined(BOOT_HIDE_SERIAL_NUMBERS)
# Export serial numbers, UUID, and asset tag from loader.
CFLAGS+= -DSMBIOS_SERIAL_NUMBERS
.if defined(BOOT_LITTLE_ENDIAN_UUID)
# Use little-endian UUID format as defined in SMBIOS 2.6.
CFLAGS+= -DSMBIOS_LITTLE_ENDIAN_UUID
.elif defined(BOOT_NETWORK_ENDIAN_UUID)
# Use network-endian UUID format for backward compatibility.
CFLAGS+= -DSMBIOS_NETWORK_ENDIAN_UUID
.endif
.endif

.if defined(HAVE_FDT) && ${MK_FDT} != "no"
.include	"${BOOTSRC}/fdt.mk"
LIBEFI_FDT=	${BOOTOBJ}/efi/fdt/libefi_fdt.a
.endif

# Include bcache code.
HAVE_BCACHE=    yes

.if defined(EFI_STAGING_SIZE)
CFLAGS+=	-DEFI_STAGING_SIZE=${EFI_STAGING_SIZE}
.endif

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

FILES+=	loader.efi
FILESMODE_loader.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS+=	-Wl,-T${LDSCRIPT},-Bsymbolic,-znotext -shared

CLEANFILES+=	loader.efi

NEWVERSWHAT=	"EFI loader" ${MACHINE}

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
loader.efi: ${PROG}
	if ${NM} ${.ALLSRC} | grep ' U '; then \
		echo "Undefined symbols in ${.ALLSRC}"; \
		exit 1; \
	fi
	SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH} \
	${OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame -j set_Xcommand_set \
		-j set_Xficl_compile_set \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

LIBEFI=		${BOOTOBJ}/efi/libefi/libefi.a

DPADD=		${LIBFICL} ${LIBEFI} ${LIBFDT} ${LIBEFI_FDT} ${LIBZFSBOOT} ${LIBSA} \
		${LDSCRIPT}
LDADD=		${LIBFICL} ${LIBEFI} ${LIBFDT} ${LIBEFI_FDT} ${LIBZFSBOOT} ${LIBSA}

.include <bsd.prog.mk>
