# $FreeBSD$

NO_MAN=
WITHOUT_SSP=
BUILDING_EFI=

.include <bsd.own.mk>

PROG=		loader.sym
INTERNALPROG=

# architecture-specific loader code
SRCS=	main.c exec.c conf.c vers.c reloc.c start.S elf32_freebsd.c
SRCS+=	i386_copy.c bootinfo.c autoload.c devicename.c efimd.c

CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/i386

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/i386
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Include bcache code.
HAVE_BCACHE=    yes

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	loader.efi
FILESMODE_loader.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_ARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -shared -symbolic

${PROG}:	${LDSCRIPT}

CLEANFILES=	vers.c loader.efi

NEWVERSWHAT=	"EFI loader" ${MACHINE_ARCH}

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
	    -j .rel.dyn -j .reloc -j .sdata -j .text -j set_Xcommand_set \
	    --target=efi-app-ia32 ${.ALLSRC} ${.TARGET}

LIBEFI=		${.OBJDIR}/../../efi/libefi/libefi.a
CFLAGS+=	-I${.CURDIR}/../libi386
CFLAGS+=	-I${.CURDIR}/../btx/lib

DPADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBEFI} -lstand

.include <bsd.prog.mk>
