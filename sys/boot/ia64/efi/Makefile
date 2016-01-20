# $FreeBSD$

MAN=

.include <bsd.own.mk>
MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=
SRCS=		conf.c efimd.c main.c pal.S start.S vers.c

.PATH: ${.CURDIR}/../../../${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

CFLAGS+=	-I${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_CPUARCH}
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_CPUARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -shared -symbolic

${PROG}: ${LDSCRIPT}

NEWVERSWHAT=	"EFI boot" ${MACHINE_CPUARCH}

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

FILES=		loader.efi
FILESMODE_loader.efi= ${BINMODE}

loader.efi: loader.sym
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .data -j .dynamic -j .dynstr -j .dynsym -j .hash \
	    -j .rela.dyn -j .reloc -j .sdata -j .text \
	    --target=efi-app-${MACHINE_CPUARCH} ${.ALLSRC} ${.TARGET}

CLEANFILES=	vers.c loader.efi

LIBIA64=	${.OBJDIR}/../common/libia64.a
LIBEFI=		${.OBJDIR}/../../efi/libefi/libefi.a
.if ${MK_FORTH} != "no"
LIBFICL=        ${.OBJDIR}/../../ficl/libficl.a
.endif

DPADD=		${LIBIA64} ${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=		-Wl,--whole-archive ${LIBIA64} -Wl,--no-whole-archive \
		    ${LIBFICL} ${LIBEFI} -lstand

.include <bsd.prog.mk>
