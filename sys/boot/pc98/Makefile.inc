# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD$

BINDIR?=	/boot

LOADER_ADDRESS?=0x200000
CFLAGS+=	-march=i386 -ffreestanding
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
CFLAGS+=	${CFLAGS_NO_SIMD} -msoft-float
CFLAGS+=	-Os -DPC98
LDFLAGS+=	-nostdlib

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

# compact binary with no padding between text, data, bss
LDSCRIPT=	${SRCTOP}/sys/boot/i386/boot.ldscript
LDFLAGS_BIN=-e start -Ttext ${ORG} -Wl,-T,${LDSCRIPT},-S,--oformat,binary
LD_FLAGS_BIN=-static -T ${LDSCRIPT} --gc-sections

.include "../Makefile.inc"
