# $FreeBSD$

PROG=		loader.sym
INTERNALPROG=
SRCS=		conf.c dev_net.c efimd.c main.c start.S vers.c
NO_MAN=

CFLAGS+=	-DLOADER
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../../efi/libefi
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

.if !defined(NO_FORTH)
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE_ARCH}
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Always add MI sources 
.PATH: ${.CURDIR}/../../common
.include "${.CURDIR}/../../common/Makefile.inc"

CFLAGS+=	-I${.CURDIR}/../../common

.PATH: ${.CURDIR}/../../forth
FILES=		loader.efi loader.help loader.4th support.4th loader.conf
FILESMODE_loader.efi= ${BINMODE}
FILESDIR_loader.conf= /boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=		loader.rc
.endif

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_ARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -shared -symbolic

${PROG}: ${LDSCRIPT}

CLEANFILES=	vers.c loader.efi loader.help

NEWVERSWHAT=	"EFI boot" ${MACHINE_ARCH}

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

loader.efi: loader.sym
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .data -j .dynamic -j .dynstr -j .dynsym -j .hash \
	    -j .rela.dyn -j .reloc -j .sdata -j .text \
	    --target=efi-app-${MACHINE_ARCH} ${.ALLSRC} ${.TARGET}

loader.help:	help.common
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

LIBEFI=		${.OBJDIR}/../../efi/libefi/libefi.a

DPADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBEFI} -lstand

.include <bsd.prog.mk>
