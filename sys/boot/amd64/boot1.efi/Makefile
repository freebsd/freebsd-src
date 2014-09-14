# $FreeBSD$

MAN=

.include <bsd.own.mk>

# In-tree GCC does not support __attribute__((ms_abi)).
.if ${COMPILER_TYPE} != "gcc"

MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=

# architecture-specific loader code
SRCS=	boot1.c reloc.c start.S

CFLAGS+=	-fPIC
CFLAGS+=	-I.
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_CPUARCH}
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica/include
CFLAGS+=	-I${.CURDIR}/../../..

# Always add MI sources and REGULAR efi loader bits
.PATH:		${.CURDIR}/../efi ${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	boot1.efi boot1.efifat
FILESMODE_boot1.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/../efi/ldscript.${MACHINE_CPUARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -Wl,-Bsymbolic -shared -Wl,-znocombreloc

${PROG}:	${LDSCRIPT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

.if ${MACHINE_CPUARCH} == "amd64"
EFI_TARGET=	efi-app-x86_64
.else
EFI_TARGET=	efi-app-ia32
.endif

boot1.efi: loader.sym
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame -j set_Xcommand_set \
		--target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

CFLAGS+=	-I${.CURDIR}/../../common

boot1.o: ${.CURDIR}/../../common/ufsread.c

# The following inserts out objects into a template FAT file system
# created by generate-fat.sh

.include "${.CURDIR}/Makefile.fat"

boot1.efifat: boot1.efi
	echo ${.OBJDIR}
	uudecode ${.CURDIR}/fat.tmpl.bz2.uu
	mv fat.tmpl.bz2 ${.TARGET}.bz2
	bzip2 -f -d ${.TARGET}.bz2
	dd if=boot1.efi of=${.TARGET} seek=${BOOT1_OFFSET} conv=notrunc

CLEANFILES= boot1.efifat

.endif # ${COMPILER_TYPE} != "gcc"

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine x86

CLEANFILES+=   machine x86 boot1.efi

machine:
	ln -sf ${.CURDIR}/../../../amd64/include machine

x86:
	ln -sf ${.CURDIR}/../../../x86/include x86
