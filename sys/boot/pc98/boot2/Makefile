# $FreeBSD$

.include <bsd.own.mk>

FILES=		boot${BOOT_SUFFIX} boot1${BOOT2_SUFFIX} boot2${BOOT2_SUFFIX}

SRCDIR=		${.CURDIR}/../boot2

NM?=		nm

BOOT_COMCONSOLE_PORT?= 0x238
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

REL1=	0x700
ORG1=	0
ORG2=	0x2000

# Decide level of UFS support.
BOOT2_UFS?=	UFS2_ONLY
#BOOT2_UFS?=	UFS1_AND_UFS2

.if ${BOOT2_UFS} == UFS1_ONLY
BOOT_SUFFIX=	.ufs1
BOOT2_SUFFIX=	ufs1
.elif ${BOOT2_UFS} == UFS2_ONLY
BOOT_SUFFIX=	.ufs2
BOOT2_SUFFIX=	ufs2
LINKS=		${BINDIR}/boot${BOOT_SUFFIX} ${BINDIR}/boot \
		${BINDIR}/boot1${BOOT2_SUFFIX} ${BINDIR}/boot1 \
		${BINDIR}/boot2${BOOT2_SUFFIX} ${BINDIR}/boot2
.endif

# Decide boot block size.
.if ${BOOT2_UFS} == UFS2_ONLY
BOOT_NSECT=	32
.else
BOOT_NSECT=	16
.endif
BOOT2_SIZE=	(${BOOT_NSECT}-1)*512

CFLAGS=	-Os \
	-fno-guess-branch-probability \
	-fomit-frame-pointer \
	-fno-unit-at-a-time \
	-ffunction-sections \
	-fdata-sections \
	-mno-align-long-strings \
	-mrtd \
	-mregparm=3 \
	-D${BOOT2_UFS} \
	-DFLAGS=${BOOT_BOOT1_FLAGS} \
	-DNSECT=${BOOT_NSECT} \
	-DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} \
	-DSIOSPD=${BOOT_COMCONSOLE_SPEED} \
	-I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../i386/boot2 \
	-I${.CURDIR}/../../common \
	-I${.CURDIR}/../btx/lib -I. \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Winline --param max-inline-insns-single=100 \
	${CLANG_OPT_SMALL}

# Set machine type to PC98_SYSTEM_PARAMETER
#CFLAGS+=	-DSET_MACHINE_TYPE

# Initialize the bi_bios_geom using the BIOS geometry
#CFLAGS+=	-DGET_BIOSGEOM

LDFLAGS=-static -N --gc-sections

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

.PATH:	${.CURDIR}/../../i386/boot2

CLEANFILES=	boot${BOOT_SUFFIX}

boot${BOOT_SUFFIX}: boot1${BOOT2_SUFFIX} boot2${BOOT2_SUFFIX}
	cat ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	boot1${BOOT2_SUFFIX} boot1.out boot1.o

boot1${BOOT2_SUFFIX}: boot1.out
	objcopy -S -O binary boot1.out ${.TARGET}

boot1.out: boot1.o
	${LD} ${LDFLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} boot1.o

CLEANFILES+=	boot2${BOOT2_SUFFIX} boot2.ld boot2.ldr boot2.bin boot2.out \
		boot2.o boot2.s boot2.s.tmp boot2.h sio.o

boot2${BOOT2_SUFFIX}: boot2.ld
	@set -- `ls -l boot2.ld`; x=$$((${BOOT2_SIZE}-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0
	dd if=boot2.ld of=${.TARGET} obs=$$((${BOOT2_SIZE})) conv=osync

boot2.ld: boot2.ldr boot2.bin ${BTXKERN}
	btxld -v -E ${ORG2} -f bin -b ${BTXKERN} -l boot2.ldr \
	    -o ${.TARGET} -P 1 boot2.bin

boot2.ldr:
	dd if=/dev/zero of=${.TARGET} bs=276 count=1

boot2.bin: boot2.out
	objcopy -S -O binary boot2.out ${.TARGET}

boot2.out: ${BTXCRT} boot2.o sio.o
	${LD} ${LDFLAGS} -Ttext ${ORG2} -o ${.TARGET} ${.ALLSRC}

boot2.o: boot2.s

SRCS=	boot2.c boot2.h

boot2.s: boot2.c boot2.h ${.CURDIR}/../../common/ufsread.c
	${CC} ${CFLAGS} -S -o boot2.s.tmp ${SRCDIR}/boot2.c
	sed -e '/align/d' -e '/nop/d' < boot2.s.tmp > boot2.s
	rm -f boot2.s.tmp

boot2.h: boot1.out
	${NM} -t d ${.ALLSRC} | awk '/([0-9])+ T (read|putc)/ \
	    { x = $$1 - ORG1; \
	    printf("#define %sORG %#x\n", toupper($$3), REL1 + x) }' \
	    ORG1=`printf "%d" ${ORG1}` \
	    REL1=`printf "%d" ${REL1}` > ${.TARGET}

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.boot1.S=		${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
