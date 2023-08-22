
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no
LOADER_DISK_SUPPORT?=	yes

.include <bsd.init.mk>

.include "${.CURDIR}/arch/${MACHINE_CPUARCH}/Makefile.inc"

LOADER_UBLDR_BIN?=	yes
.if ${LOADER_UBLDR_BIN} != "no"
FILES+=		ubldr ubldr.bin
.else
PROG=		ubldr
.endif

NEWVERSWHAT=	"U-Boot loader" ${MACHINE_ARCH}
BINDIR= 	/boot/uboot
INSTALLFLAGS=	-b
WARNS?=		1

.PATH:		${BOOTSRC}/common
.PATH:		${.CURDIR}/arch/${MACHINE_CPUARCH}


.if ${COMPILER_TYPE} == "gcc"
CWARNFLAGS.self_reloc.c+=	-Wno-error=maybe-uninitialized
.endif

HELP_FILES=	${.CURDIR}/help.uboot ${BOOTSRC}/fdt/help.fdt
HELP_FILENAME=	loader.help.uboot

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

LDSCRIPT=	${.CURDIR}/arch/${MACHINE_CPUARCH}/ldscript.${MACHINE_CPUARCH}
LDFLAGS=	-nostdlib -static -T ${LDSCRIPT}
LDFLAGS+=	-Wl,-znotext

SRCS+=		main.c vers.c
SRCS+=		copy.c devicename.c elf_freebsd.c glue.c
SRCS+=		net.c reboot.c time.c gfx_fb_stub.c
SRCS+=		uboot_console.c uboot_disk.c uboot_fdt.c uboot_module.c

CFLAGS.gfx_fb_stub.c += -I${SRCTOP}/contrib/pnglite -I${SRCTOP}/sys/teken
CFLAGS.glue.c+=	-DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib

CFLAGS+=	-I${BOOTSRC}/common
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.OBJDIR}

.if ${MACHINE_CPUARCH} == "arm"
SRCS+=	metadata.c
.endif

.include "${BOOTSRC}/fdt.mk"

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${LDRSRC}

# libfdt headers
CFLAGS+=	-I${FDTSRC}

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

.if ${LOADER_UBLDR_BIN} != "no"
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}
ubldr ubldr.bin ubldr.pie: ${OBJS}
.endif

DPADD=		${LDR_INTERP} ${LIBFDT} ${LIBSA}
LDADD=		${LDR_INTERP} ${LIBFDT} ${LIBSA}

.include <bsd.prog.mk>
