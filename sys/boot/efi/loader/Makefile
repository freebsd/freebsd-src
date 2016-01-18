# $FreeBSD$

MAN=

.include <bsd.own.mk>

# In-tree GCC does not support __attribute__((ms_abi)).
.if ${COMPILER_TYPE} != "gcc"

MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=

# architecture-specific loader code
SRCS=	autoload.c \
	bootinfo.c \
	conf.c \
	copy.c \
	devicename.c \
	main.c \
	smbios.c \
	vers.c

.PATH: ${.CURDIR}/arch/${MACHINE}
# For smbios.c
.PATH: ${.CURDIR}/../../i386/libi386
.include "${.CURDIR}/arch/${MACHINE}/Makefile.inc"

CFLAGS+=	-fPIC
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/arch/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica/include
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../i386/libi386
CFLAGS+=	-DNO_PCI -DEFI

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE}
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Include bcache code.
HAVE_BCACHE=    yes

.if defined(EFI_STAGING_SIZE)
CFLAGS+=	-DEFI_STAGING_SIZE=${EFI_STAGING_SIZE}
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	loader.efi
FILESMODE_loader.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS=	-Wl,-T${LDSCRIPT} -Wl,-Bsymbolic -shared -Wl,-znocombreloc

CLEANFILES=	vers.c loader.efi

NEWVERSWHAT=	"EFI loader" ${MACHINE}

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../efi/loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

.if ${MACHINE_CPUARCH} == "amd64"
EFI_TARGET=	efi-app-x86_64
.elif ${MACHINE_CPUARCH} == "i386"
EFI_TARGET=	efi-app-ia32
.endif

loader.efi: ${PROG}
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame -j set_Xcommand_set \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

LIBEFI=		${.OBJDIR}/../libefi/libefi.a

DPADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND} ${LDSCRIPT}
LDADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND}

.endif # ${COMPILER_TYPE} != "gcc"

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine x86

CLEANFILES+=   machine x86

machine:
	ln -sf ${.CURDIR}/../../../amd64/include machine

x86:
	ln -sf ${.CURDIR}/../../../x86/include x86
