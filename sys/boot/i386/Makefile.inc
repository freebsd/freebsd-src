# Common defines for all of /sys/boot/i386/
#
# $FreeBSD: src/sys/boot/i386/Makefile.inc,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

BINDIR?=	/boot

LOADER_ADDRESS?=0x200000
CFLAGS+=	-ffreestanding -mpreferred-stack-boundary=2 \
		-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3
LDFLAGS+=	-nostdlib

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-m32 -march=i386
LDFLAGS+=	-m elf_i386_fbsd
AFLAGS+=	--32
.endif

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o
