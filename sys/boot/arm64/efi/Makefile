# $FreeBSD$

.include <src.opts.mk>
NO_MAN=
BUILDING_EFI=

.include <bsd.own.mk>
MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=

# architecture-specific loader code
SRCS=	start.S main.c conf.c vers.c reloc.c autoload.c
SRCS+=	copy.c devicename.c exec.c bootinfo.c

CFLAGS+=	-fno-builtin
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/arm64

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/arm64
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	loader.efi
FILESMODE_loader.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/ldscript.arm64
LDFLAGS=	-Wl,-T${LDSCRIPT} -Wl,-Bsymbolic -shared

${PROG}:	${LDSCRIPT}

CLEANFILES=	vers.c loader.efi

NEWVERSWHAT=	"EFI loader" ${MACHINE_CPUARCH}

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

loader.efi: loader.sym
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .data -j .dynamic -j .dynstr -j .dynsym -j .hash \
	    -j .rela.dyn -j .reloc -j .sdata -j .text -j set_Xcommand_set \
	    -j .sbss -j .bss -O binary ${.ALLSRC} ${.TARGET}

LIBEFI=		${.OBJDIR}/../../efi/libefi/libefi.a

DPADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBEFI} ${.OBJDIR}/../../../../lib/libstand/libstand.a
#${LIBSTAND}

.include <bsd.prog.mk>

