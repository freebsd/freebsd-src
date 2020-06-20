# $FreeBSD$

.include <bsd.init.mk>

.PATH:		${BOOTSRC}/i386/boot2 ${BOOTSRC}/i386/common ${BOOTSRC}/common

FILES=		zfsboot
MAN=		zfsboot.8

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x2000

CFLAGS+=-DBOOTPROG=\"zfsboot\" \
	-O1 \
	-DBOOT2 \
	-DLOADER_GPT_SUPPORT \
	-DLOADER_MBR_SUPPORT \
	-DLOADER_ZFS_SUPPORT \
	-DLOADER_UFS_SUPPORT \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${LDRSRC} \
	-I${BOOTSRC}/i386/common \
	-I${BOOTSRC}/i386/libi386 \
	-I${ZFSSRC} \
	-I${SYSDIR}/crypto/skein \
	-I${SYSDIR}/cddl/boot/zfs \
	-I${SYSDIR}/cddl/contrib/opensolaris/uts/common \
	-I${SYSDIR}/cddl/contrib/opensolaris/common/lz4 \
	-I${BOOTSRC}/i386/boot2 \
	-Wall -Waggregate-return -Wbad-function-cast -Wno-cast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings

CFLAGS.part.c+= -DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib

CFLAGS.gcc+=	--param max-inline-insns-single=100

LD_FLAGS+=${LD_FLAGS_BIN}

CLEANFILES+=	zfsboot

zfsboot: zfsboot1 zfsboot2
	cat zfsboot1 zfsboot2 > zfsboot

CLEANFILES+=	zfsboot1 zfsldr.out zfsldr.o

zfsboot1: zfsldr.out
	${OBJCOPY} -S -O binary zfsldr.out ${.TARGET}

zfsldr.out: zfsldr.o
	${LD} ${LD_FLAGS} -e start --defsym ORG=${ORG1} -T ${LDSCRIPT} -o ${.TARGET} zfsldr.o

OBJS=	zfsboot.o sio.o cons.o bcache.o devopen.o disk.o part.o zfs_cmd.o
CLEANFILES+=	zfsboot2 zfsboot.ld zfsboot.ldr zfsboot.bin zfsboot.out \
		${OBJS}

# We currently allow 256k bytes for zfsboot - in practice it could be
# any size up to 3.5Mb but keeping it fixed size simplifies zfsldr.
# 
BOOT2SIZE=	262144

# i386 standalone support library
LIBI386=	${BOOTOBJ}/i386/libi386/libi386.a

zfsboot2: zfsboot.ld
	@set -- `ls -l ${.ALLSRC}`; x=$$((${BOOT2SIZE}-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0
	${DD} if=${.ALLSRC} of=${.TARGET} bs=${BOOT2SIZE} conv=sync

zfsboot.ld: zfsboot.ldr zfsboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l zfsboot.ldr \
	    -o ${.TARGET} -P 1 zfsboot.bin

zfsboot.ldr:
	cp /dev/null ${.TARGET}

zfsboot.bin: zfsboot.out
	${OBJCOPY} -S -O binary zfsboot.out ${.TARGET}

zfsboot.out: ${BTXCRT} ${OBJS}
	${LD} ${LD_FLAGS} --defsym ORG=${ORG2} -T ${LDSCRIPT} -o ${.TARGET} ${.ALLSRC} ${LIBI386} ${LIBSA32}

SRCS=	zfsboot.c

.include <bsd.prog.mk>
