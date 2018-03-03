# Common defines for all of /stand/pc98/
#
# $FreeBSD$

LOADER_ADDRESS?=0x200000
LDFLAGS+=	-nostdlib

# BTX components
BTXDIR=		${BOOTOBJ}/pc98/btx
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx

BTXSRC=		${BOOTSRC}/pc98/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

# compact binary with no padding between text, data, bss
LDSCRIPT=	${BOOTSRC}/i386/boot.ldscript
#LDFLAGS_BIN=-e start -Ttext ${ORG} -Wl,-T,${LDSCRIPT},-S,--oformat,binary
LDFLAGS_BIN=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary
#LD_FLAGS_BIN=-static -T ${LDSCRIPT} --gc-sections
LD_FLAGS_BIN=-static -N --gc-sections

WARNS?=		0

.include "../Makefile.inc"
