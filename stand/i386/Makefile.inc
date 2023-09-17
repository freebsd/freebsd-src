# Common defines for all of stand/i386/
#

.include "bsd.linker.mk"

LOADER_ADDRESS?=0x200000
LDFLAGS+=	-nostdlib
LDFLAGS.lld+=	-Wl,--no-rosegment
MK_PIE:=	no

# BTX components
BTXDIR=		${BOOTOBJ}/i386/btx
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

BTXSRC=		${BOOTSRC}/i386/btx
BTXLIB=		${BTXSRC}/lib

CFLAGS+=	-I${BTXLIB}

# compact binary with no padding between text, data, bss
LDSCRIPT=	${BOOTSRC}/i386/boot.ldscript
LDFLAGS_ORG=	-Wl,--defsym,ORG=${ORG},-T,${LDSCRIPT}
LDFLAGS_BIN=	-e start ${LDFLAGS_ORG} -Wl,-N,-S,--oformat,binary
.if ${LINKER_FEATURES:Mbuild-id} != ""
LDFLAGS_BIN+=	-Wl,--build-id=none
.endif
LD_FLAGS_BIN=	-static -N --gc-sections

.if ${MACHINE_CPUARCH} == "amd64"
DO32=1
.endif

.include "../Makefile.inc"
