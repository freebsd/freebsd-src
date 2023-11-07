
LOADER_NET_SUPPORT?=	yes
LOADER_MSDOS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no

.include <bsd.init.mk>

LOADER?=	loader_${LOADER_INTERP}
PROG=		${LOADER}.sym
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
	vers.c \
	gfx_fb.c \
	8x16.c

CFLAGS+=	-I${.CURDIR}/../loader
.if ${MK_LOADER_ZFS} != "no"
CFLAGS+=	-I${ZFSSRC}
CFLAGS+=        -I${SYSDIR}/contrib/openzfs/include
CFLAGS+=        -I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+=	-DEFI_ZFS_BOOT
HAVE_ZFS=	yes
.endif

CFLAGS.bootinfo.c += -I$(SRCTOP)/sys/teken
CFLAGS.bootinfo.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.framebuffer.c += -I$(SRCTOP)/sys/teken
CFLAGS.framebuffer.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.main.c += -I$(SRCTOP)/sys/teken
CFLAGS.main.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.gfx_fb.c += -I$(SRCTOP)/sys/teken
CFLAGS.gfx_fb.c += -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4
CFLAGS.gfx_fb.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.gfx_fb.c += -DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib

# We implement a slightly non-standard %S in that it always takes a
# CHAR16 that's common in UEFI-land instead of a wchar_t. This only
# seems to matter on arm64 where wchar_t defaults to an int instead
# of a short. There's no good cast to use here so just ignore the
# warnings for now.
CWARNFLAGS.main.c+=	-Wno-format

.PATH: ${.CURDIR}/../loader
.PATH: ${.CURDIR}/../loader/arch/${MACHINE}
.include "${.CURDIR}/../loader/arch/${MACHINE}/Makefile.inc"

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/arch/${MACHINE}
CFLAGS+=	-I${EFISRC}/include
CFLAGS+=	-I${EFISRC}/include/${MACHINE}
CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
CFLAGS+=	-I${BOOTSRC}/i386/libi386
CFLAGS+=	-DEFI

.if defined(HAVE_FDT) && ${MK_FDT} != "no"
.include	"${BOOTSRC}/fdt.mk"
LIBEFI_FDT=	${BOOTOBJ}/efi/fdt/libefi_fdt.a
HELP_FILES+=	${FDTSRC}/help.fdt
.endif

# Include bcache code.
HAVE_BCACHE=    yes

.if defined(EFI_STAGING_SIZE)
CFLAGS+=	-DEFI_STAGING_SIZE=${EFI_STAGING_SIZE}
.endif

.if ${MK_LOADER_EFI_SECUREBOOT} != "no"
CFLAGS+= -DEFI_SECUREBOOT
.endif

NEWVERSWHAT=	"EFI loader" ${MACHINE}
VERSION_FILE=	${.CURDIR}/../loader/version
HELP_FILENAME=	loader.help.efi

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

CLEANFILES+=	8x16.c

8x16.c:		${SRCTOP}/contrib/terminus/ter-u16b.bdf
	vtfontcvt -f compressed-source -o ${.TARGET} ${.ALLSRC}

FILES+=	${LOADER}.efi
FILESMODE_${LOADER}.efi=	${BINMODE}

.if ${LOADER_INTERP} == ${LOADER_DEFAULT_INTERP}
LINKS+=		${BINDIR}/${LOADER}.efi ${BINDIR}/loader.efi
.endif

LDSCRIPT=	${.CURDIR}/../loader/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS+=	-Wl,-T${LDSCRIPT},-Bsymbolic,-znotext -pie
.if ${LINKER_TYPE} == "bfd" && ${LINKER_VERSION} >= 23400
LDFLAGS+=	-Wl,--no-dynamic-linker
.endif

CLEANFILES+=	${LOADER}.efi

${LOADER}.efi: ${PROG}
	@if ${NM} ${.ALLSRC} | grep ' U '; then \
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

DPADD=		${LDR_INTERP} ${LIBEFI} ${LIBSAFDT} ${LIBEFI_FDT} ${LIBSA} ${LDSCRIPT}
LDADD=		${LDR_INTERP} ${LIBEFI} ${LIBSAFDT} ${LIBEFI_FDT} ${LIBSA}

.include <bsd.prog.mk>
