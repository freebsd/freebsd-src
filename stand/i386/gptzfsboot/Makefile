.include <bsd.init.mk>

.PATH:		${BOOTSRC}/i386/boot2 ${BOOTSRC}/i386/gptboot \
		${BOOTSRC}/i386/common \
		${BOOTSRC}/common

FILES=		gptzfsboot
MAN=		gptzfsboot.8

BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 115200
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0x7c00
ORG2=	0x0

CFLAGS+=-DBOOTPROG=\"gptzfsboot\" \
	-O1 \
	-DBOOT2 \
	-DLOADER_GPT_SUPPORT \
	-DLOADER_MBR_SUPPORT \
	-DLOADER_ZFS_SUPPORT \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${LDRSRC} \
	-I${BOOTSRC}/i386/common \
	-I${BOOTSRC}/i386/libi386 \
	-I${ZFSSRC} \
	-I${SYSDIR}/crypto/skein \
	-I${SYSDIR}/cddl/boot/zfs \
	-I${SYSDIR}/contrib/openzfs/include \
	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/spl \
	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs \
	-I${SYSDIR}/cddl/contrib/opensolaris/common/lz4 \
	-I${BOOTSRC}/i386/btx/lib \
	-I${BOOTSRC}/i386/boot2 \
	-DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib \
	-Wall -Waggregate-return -Wbad-function-cast \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Wno-pointer-sign

CFLAGS.clang+=	-Wno-tentative-definition-incomplete-type

NO_WCAST_ALIGN=

CFLAGS.gcc+=	--param max-inline-insns-single=100

LD_FLAGS+=${LD_FLAGS_BIN}

CLEANFILES+=	gptzfsboot

gptzfsboot: gptldr.bin gptzfsboot.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l gptldr.bin \
	    -o ${.TARGET} gptzfsboot.bin

CLEANFILES+=	gptldr.bin gptldr.out gptldr.o

gptldr.bin: gptldr.out
	${OBJCOPY} -S -O binary gptldr.out ${.TARGET}

gptldr.out: gptldr.o
	${LD} ${LD_FLAGS} -e start --defsym ORG=${ORG1} -T ${LDSCRIPT} -o ${.TARGET} gptldr.o

OBJS=	zfsboot.o sio.o cons.o bcache.o devopen.o disk.o part.o zfs_cmd.o misc.o
CLEANFILES+=	gptzfsboot.bin gptzfsboot.out ${OBJS}

# i386 standalone support library
LIBI386=	${BOOTOBJ}/i386/libi386/libi386.a

gptzfsboot.bin: gptzfsboot.out
	${OBJCOPY} -S -O binary gptzfsboot.out ${.TARGET}

gptzfsboot.out: ${BTXCRT} ${OBJS}
	${LD} ${LD_FLAGS} --defsym ORG=${ORG2} -T ${LDSCRIPT} -o ${.TARGET} ${.ALLSRC} ${LIBI386} ${LIBSA32}

zfsboot.o: ${ZFSSRC}/zfsimpl.c

.include <bsd.prog.mk>
