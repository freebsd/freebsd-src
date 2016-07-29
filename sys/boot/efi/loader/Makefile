# $FreeBSD$

MAN=

.include <src.opts.mk>

MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=
WARNS?=		3

# architecture-specific loader code
SRCS=	autoload.c \
	bootinfo.c \
	conf.c \
	copy.c \
	devicename.c \
	main.c \
	self_reloc.c \
	smbios.c \
	vers.c

.if ${MK_ZFS} != "no"
SRCS+=		zfs.c
.PATH:		${.CURDIR}/../../zfs

# Disable warnings that are currently incompatible with the zfs boot code
CWARNFLAGS.zfs.c+=	-Wno-sign-compare
CWARNFLAGS.zfs.c+=	-Wno-array-bounds
CWARNFLAGS.zfs.c+=	-Wno-missing-prototypes
.endif

# We implement a slightly non-standard %S in that it always takes a
# CHAR16 that's common in UEFI-land instead of a wchar_t. This only
# seems to matter on arm64 where wchar_t defaults to an int instead
# of a short. There's no good cast to use here so just ignore the
# warnings for now.
CWARNFLAGS.main.c+=	-Wno-format

.PATH: ${.CURDIR}/arch/${MACHINE}
# For smbios.c
.PATH: ${.CURDIR}/../../i386/libi386
.include "${.CURDIR}/arch/${MACHINE}/Makefile.inc"

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/arch/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica/include
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../i386/libi386
.if ${MK_ZFS} != "no"
CFLAGS+=	-I${.CURDIR}/../../zfs
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs
CFLAGS+=	-DEFI_ZFS_BOOT
.endif
CFLAGS+=	-DNO_PCI -DEFI

# make buildenv doesn't set DESTDIR, this means LIBSTAND
# will be wrong when crossbuilding.
.if exists(${.OBJDIR}/../../../../lib/libstand/libstand.a)
LIBSTAND=	${.OBJDIR}/../../../../lib/libstand/libstand.a
.endif

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE_CPUARCH}
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

LOADER_FDT_SUPPORT?=	no
.if ${MK_FDT} != "no" && ${LOADER_FDT_SUPPORT} != "no"
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBEFI_FDT=	${.OBJDIR}/../../efi/fdt/libefi_fdt.a
LIBFDT=		${.OBJDIR}/../../fdt/libfdt.a
.endif

# Include bcache code.
HAVE_BCACHE=    yes

.if defined(EFI_STAGING_SIZE)
CFLAGS+=	-DEFI_STAGING_SIZE=${EFI_STAGING_SIZE}
.endif

# Always add MI sources
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common

FILES+=	loader.efi
FILESMODE_loader.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS+=	-Wl,-T${LDSCRIPT} -Wl,-Bsymbolic -shared

CLEANFILES+=	vers.c loader.efi

NEWVERSWHAT=	"EFI loader" ${MACHINE}

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../efi/loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

NM?=		nm
OBJCOPY?=	objcopy

.if ${MACHINE_CPUARCH} == "amd64"
EFI_TARGET=	efi-app-x86_64
.elif ${MACHINE_CPUARCH} == "i386"
EFI_TARGET=	efi-app-ia32
.else
EFI_TARGET=	binary
.endif

loader.efi: ${PROG}
	if ${NM} ${.ALLSRC} | grep ' U '; then \
		echo "Undefined symbols in ${.ALLSRC}"; \
		exit 1; \
	fi
	${OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame -j set_Xcommand_set \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

LIBEFI=		${.OBJDIR}/../libefi/libefi.a

DPADD=		${LIBFICL} ${LIBEFI} ${LIBFDT} ${LIBEFI_FDT} ${LIBSTAND} \
		${LDSCRIPT}
LDADD=		${LIBFICL} ${LIBEFI} ${LIBFDT} ${LIBEFI_FDT} ${LIBSTAND}

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
