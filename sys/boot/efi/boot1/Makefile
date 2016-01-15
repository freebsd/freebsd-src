# $FreeBSD$

MAN=

.include <src.opts.mk>

MK_SSP=		no

PROG=		boot1.sym
INTERNALPROG=
WARNS?=		6

# architecture-specific loader code
SRCS=	boot1.c self_reloc.c start.S ufs_module.c

CFLAGS+=	-I.
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica/include
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-DEFI_UFS_BOOT

# Always add MI sources and REGULAR efi loader bits
.PATH:		${.CURDIR}/../loader/arch/${MACHINE}
.PATH:		${.CURDIR}/../loader
.PATH:		${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	boot1.efi boot1.efifat
FILESMODE_boot1.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/../loader/arch/${MACHINE}/ldscript.${MACHINE}
LDFLAGS=	-Wl,-T${LDSCRIPT} -Wl,-Bsymbolic -shared

.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-msoft-float -mgeneral-regs-only
.endif
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-fPIC
LDFLAGS+=	-Wl,-znocombreloc
.endif

#
# Add libstand for the runtime functions used by the compiler - for example
# __aeabi_* (arm) or __divdi3 (i386).
# as well as required string and memory functions for all platforms.
#
DPADD+=		${LIBSTAND}
LDADD+=		-lstand

DPADD+=		${LDSCRIPT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

.if ${MACHINE_CPUARCH} == "amd64"
EFI_TARGET=	efi-app-x86_64
.elif ${MACHINE_CPUARCH} == "i386"
EFI_TARGET=	efi-app-ia32
.else
EFI_TARGET=	binary
.endif

boot1.efi: ${PROG}
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

boot1.o: ${.CURDIR}/../../common/ufsread.c

# The following inserts our objects into a template FAT file system
# created by generate-fat.sh

.include "${.CURDIR}/Makefile.fat"
BOOT1_MAXSIZE?=	131072

boot1.efifat: boot1.efi
	@set -- `ls -l boot1.efi`; \
	x=$$(($$5-${BOOT1_MAXSIZE})); \
	if [ $$x -ge 0 ]; then \
	    echo "boot1 $$x bytes too large; regenerate FAT templates?" >&2 ;\
	    exit 1; \
	fi
	echo ${.OBJDIR}
	uudecode ${.CURDIR}/fat-${MACHINE}.tmpl.bz2.uu
	mv fat-${MACHINE}.tmpl.bz2 ${.TARGET}.bz2
	bzip2 -f -d ${.TARGET}.bz2
	dd if=boot1.efi of=${.TARGET} seek=${BOOT1_OFFSET} conv=notrunc

CLEANFILES= boot1.efi boot1.efifat

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine

CLEANFILES+=   machine

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE}/include machine

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
beforedepend ${OBJS}: x86
CLEANFILES+=   x86

x86:
	ln -sf ${.CURDIR}/../../../x86/include x86
.endif
