
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_UFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no

.include <bsd.init.mk>

PROG=		loader
NEWVERSWHAT=	"Open Firmware loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c vers.c main.c elf_freebsd.c ppc64_elf_freebsd.c start.c
SRCS+=		ucmpdi2.c

.include	"${BOOTSRC}/fdt.mk"
.if ${MK_FDT} == "yes"
SRCS+=		ofwfdt.c
.endif

.if ${MACHINE_ARCH:Mpowerpc64*} != ""
SRCS+=		cas.c
CFLAGS+=	-DCAS
.endif

.if ${MACHINE_ARCH} == "powerpc64le"
SRCS+=		trampolineLE.S
.endif

HELP_FILES=	${FDTSRC}/help.fdt
HELP_FILENAME=	loader.help.ofw

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

.PATH:		${SYSDIR}/libkern

# load address. set in linker script
RELOC?=		0x1C00000
CFLAGS+=	-DRELOC=${RELOC} -g

LDFLAGS=	-nostdlib -static

.if ${MACHINE_ARCH} == "powerpc64le"
LDFLAGS+=	-T ${.CURDIR}/ldscript.powerpcle
.else
LDFLAGS+=	-T ${.CURDIR}/ldscript.powerpc
.endif

# Open Firmware standalone support library
LIBOFW=		${BOOTOBJ}/libofw/libofw.a
CFLAGS+=	-I${BOOTSRC}/libofw

DPADD=		${LDR_INTERP} ${LIBOFW} ${LIBSAFDT} ${LIBSA}
LDADD=		${LDR_INTERP} ${LIBOFW} ${LIBSAFDT} ${LIBSA}

MK_PIE=		no

.include <bsd.prog.mk>
