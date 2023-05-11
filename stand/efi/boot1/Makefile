# $FreeBSD$

.include <bsd.init.mk>

BOOT1?=		boot1
PROG=		${BOOT1}.sym
INTERNALPROG=
WARNS?=		6

CFLAGS+=	-DEFI_BOOT1
# We implement a slightly non-standard %S in that it always takes a
# CHAR16 that's common in UEFI-land instead of a wchar_t. This only
# seems to matter on arm64 where wchar_t defaults to an int instead
# of a short. There's no good cast to use here so just ignore the
# warnings for now.
CWARNFLAGS.proto.c+=	-Wno-format
CWARNFLAGS.boot1.c+=	-Wno-format

# Disable bogus alignment issues
CWARNFLAGS.ufs_module.c += -Wno-format
CWARNFLAGS.ufs_module.c += -Wno-cast-align

# Disable warnings that are currently incompatible with the zfs boot code
CWARNFLAGS.zfs_module.c += -Wno-array-bounds
CWARNFLAGS.zfs_module.c += -Wno-cast-align
CWARNFLAGS.zfs_module.c += -Wno-cast-qual
CWARNFLAGS.zfs_module.c += -Wno-missing-prototypes
CWARNFLAGS.zfs_module.c += -Wno-sign-compare
CWARNFLAGS.zfs_module.c += -Wno-unused-parameter
CWARNFLAGS.zfs_module.c += -Wno-unused-function

# architecture-specific loader code
SRCS+=	boot1.c proto.c self_reloc.c start.S ufs_module.c devpath.c
.if ${MK_LOADER_ZFS} != "no"
SRCS+=		zfs_module.c
CFLAGS.zfs_module.c+=	-I${ZFSSRC}
CFLAGS.zfs_module.c+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS.zfs_module.c+=	-I${SYSDIR}/crypto/skein
CFLAGS.zfs_module.c+=	-I${SYSDIR}/contrib/openzfs/include
CFLAGS.zfs_module.c+=	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/spl
CFLAGS.zfs_module.c+=	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
CFLAGS.zfs_module.c+=	-I${SYSDIR}/cddl/contrib/opensolaris/common/lz4
CFLAGS.zfs_module.c+=	-include ${ZFSOSINC}/spl/sys/ccompile.h

CFLAGS+=	-DEFI_ZFS_BOOT
.endif

CFLAGS+=	-I${EFIINC}
CFLAGS+=	-I${EFIINCMD}
CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
CFLAGS+=	-DEFI_UFS_BOOT
.ifdef(EFI_DEBUG)
CFLAGS+=	-DEFI_DEBUG
.endif

.include	"${BOOTSRC}/veriexec.mk"

# Always add MI sources and REGULAR efi loader bits
.PATH:		${EFISRC}/loader/arch/${MACHINE}
.PATH:		${EFISRC}/loader
.PATH:		${LDRSRC}
.PATH:		${EFISRC}/libefi
CFLAGS+=	-I${LDRSRC}

FILES=	${BOOT1}.efi
FILESMODE_${BOOT1}.efi=	${BINMODE}

LDSCRIPT=	${EFISRC}/loader/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS+=	-Wl,-T${LDSCRIPT},-Bsymbolic,-znotext -pie
.if ${LINKER_TYPE} == "bfd" && ${LINKER_VERSION} >= 23400
LDFLAGS+=	-Wl,--no-dynamic-linker
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-mgeneral-regs-only
.endif
.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-fPIC
LDFLAGS+=	-Wl,-znocombreloc
.endif

LIBEFI=		${BOOTOBJ}/efi/libefi/libefi.a

#
# Add libsa for the runtime functions used by the compiler as well as required
# string and memory functions for all platforms.
#
DPADD+=		${LIBEFI} ${LIBSA}
LDADD+=		${LIBEFI} ${LIBSA}

DPADD+=		${LDSCRIPT}

${BOOT1}.efi: ${PROG}
	if ${NM} ${.ALLSRC} | grep ' U '; then \
		echo "Undefined symbols in ${.ALLSRC}"; \
		exit 1; \
	fi
	SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH} \
	${OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
