# $FreeBSD$

PROG= 		loader.sym
INTERNALPROG=
NOMAN=
NEWVERSWHAT=	"EFI boot" ${MACHINE_ARCH}

SRCS=		start.S main.c conf.c dev_net.c vers.c

.PATH: ${.CURDIR}/../libefi/arch/${MACHINE_ARCH}

.if !defined(NOFORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/${MACHINE_ARCH}
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

LIBEFI=		${.OBJDIR}/../libefi/libefi.a

# Always add MI sources 
.PATH: ${.CURDIR}/../../common
.include "${.CURDIR}/../../common/Makefile.inc"

CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+= -I${.CURDIR}/../../common
CFLAGS+= -I${.CURDIR}/../libefi
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand
CFLAGS+= -DLOADER

LDSCRIPT=	${.CURDIR}/../libefi/arch/${MACHINE_ARCH}/ldscript.${MACHINE_ARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -shared -symbolic
OBJCOPY?=	objcopy

CLEANFILES=	vers.c loader.efi loader.help

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

loader.efi: loader.sym
	${OBJCOPY} -j .text \
		-j .data \
		-j .sdata \
		-j .dynamic \
		-j .rela.dyn \
		-j .reloc \
		-j .hash \
		-j .dynsym \
		-j .dynstr \
		--target=efi-app-${MACHINE_ARCH} \
		loader.sym loader.efi

loader.help:	help.common
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
FILES=	loader.efi loader.help loader.4th support.4th loader.conf
FILESMODE_loader.efi= ${BINMODE}
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=	loader.rc
.endif

DPADD=	${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=	${LIBFICL} ${LIBEFI} -lstand

.include <bsd.prog.mk>
