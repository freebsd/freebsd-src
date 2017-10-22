# $FreeBSD$

.include <src.opts.mk>

FILES=		ubldr

NEWVERSWHAT=	"U-Boot loader" ${MACHINE_ARCH}
BINDIR?=	/boot
INSTALLFLAGS=	-b
WARNS?=		1
# Address at which ubldr will be loaded.
# This varies for different boards and SOCs.
UBLDR_LOADADDR?=	0xffffffff80800000

# Architecture-specific loader code
SRCS=		start.S conf.c vers.c

LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no
LOADER_FDT_SUPPORT=	${MK_FDT}

# Always add MI sources
.include	"../../loader.mk"
CFLAGS+=	-I.

CLEANFILES+=	loader.help

CFLAGS+=	-ffreestanding -msoft-float -g

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.${MACHINE_CPUARCH}

.include	"../../uboot.mk"

DPADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBUBOOT_FDT} ${LIBSA}
LDADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBUBOOT_FDT} ${LIBSA}

OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

loader.help: help.common help.uboot ${.CURDIR}/../../fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

ldscript.abs:
	echo "UBLDR_LOADADDR = ${UBLDR_LOADADDR};" >${.TARGET}

ldscript.pie:
	echo "UBLDR_LOADADDR = 0;" >${.TARGET}

ubldr: ${OBJS} ldscript.abs ${.CURDIR}/ldscript.${MACHINE_CPUARCH} ${DPADD}
	${CC} ${CFLAGS} -T ldscript.abs ${LDFLAGS} \
	    -o ${.TARGET} ${OBJS} ${LDADD}
	${OBJCOPY} -S -O binary ubldr ubldr.bin

CLEANFILES+=	ldscript.abs ldscript.pie ubldr ubldr.pie ubldr.bin

.if !defined(LOADER_ONLY)
.PATH: ${.CURDIR}/../../forth
.include	"${.CURDIR}/../../forth/Makefile.inc"

# Install loader.rc.
FILES+=	loader.rc
# Put sample menu.rc on disk but don't enable it by default.
FILES+=	menu.rc
FILESNAME_menu.rc=	menu.rc.sample
.endif

.include <bsd.stand.mk>
.include <bsd.prog.mk>
