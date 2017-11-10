# $FreeBSD$

LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no
LOADER_FDT_SUPPORT=	${MK_FDT}

.include <bsd.init.mk>

FILES=		ubldr ubldr.bin

NEWVERSWHAT=	"U-Boot loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b
WARNS?=		1
# Address at which ubldr will be loaded.
# This varies for different boards and SOCs.
UBLDR_LOADADDR?=	0x1000000

# Architecture-specific loader code
SRCS=		start.S conf.c self_reloc.c vers.c

.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} > 40201
CWARNFLAGS.self_reloc.c+=	-Wno-error=maybe-uninitialized
.endif

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
CFLAGS+=	-I.

CLEANFILES+=	loader.help

CFLAGS+=	-ffreestanding -msoft-float

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.${MACHINE_CPUARCH}
LDFLAGS+=	-Wl,-znotext

# Pull in common loader code
.include	"${BOOTSRC}/uboot.mk"

CFLAGS+=	-fPIC

DPADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBUBOOT_FDT} ${LIBSA}
LDADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBUBOOT_FDT} ${LIBSA}

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

loader.help: help.common help.uboot ${BOOTSRC}/fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

ldscript.abs:
	echo "UBLDR_LOADADDR = ${UBLDR_LOADADDR};" >${.TARGET}

ldscript.pie:
	echo "UBLDR_LOADADDR = 0;" >${.TARGET}

ubldr: ${OBJS} ldscript.abs ${.CURDIR}/ldscript.${MACHINE_CPUARCH} ${DPADD}
	${CC} ${CFLAGS} -T ldscript.abs ${LDFLAGS} \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.pie: ${OBJS} ldscript.pie ${.CURDIR}/ldscript.${MACHINE_CPUARCH} ${DPADD}
	${CC} ${CFLAGS} -T ldscript.pie ${LDFLAGS} -pie -Wl,-Bsymbolic \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.bin: ubldr.pie
	${OBJCOPY} -S -O binary ubldr.pie ${.TARGET}

CLEANFILES+=	ldscript.abs ldscript.pie ubldr ubldr.pie ubldr.bin

.if !defined(LOADER_ONLY)
.PATH: ${BOOTSRC}/forth
.include	"${BOOTSRC}/forth/Makefile.inc"

# Install loader.rc.
FILES+=	loader.rc
# Put sample menu.rc on disk but don't enable it by default.
FILES+=	menu.rc
FILESNAME_menu.rc=	menu.rc.sample
.endif

.include <bsd.prog.mk>
