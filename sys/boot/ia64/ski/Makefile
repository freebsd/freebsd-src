# $FreeBSD$

PROG=		skiload
NO_MAN=
NEWVERSWHAT=	"ia64 SKI boot" ${MACHINE_ARCH}
STRIP=		# We must not strip skiload at install time.

SRCS=		acpi_stub.c bootinfo.c conf.c copy.c delay.c devicename.c \
		efi_stub.c elf_freebsd.c exit.c main.c pal_stub.S sal_stub.c \
		skiconsole.c skifs.c ssc.c start.S time.c vers.c

CFLAGS+=	-DLOADER
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_ARCH}
LDFLAGS=	-Wl,-T${.CURDIR}/ldscript.ia64

.if !defined(NO_FORTH)
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE_ARCH}
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
BOOT_FORTH=	yes
.endif

# Always add MI sources (needs BOOT_FORTH)
.PATH: ${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../common
.include "${.CURDIR}/../../common/Makefile.inc"

DPADD=	${LIBFICL} ${LIBSTAND}
LDADD=	${LIBFICL} -lstand

CLEANFILES=	vers.c ${PROG}.help

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

${PROG}.help: help.common
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

FILES=	${PROG}.help

.include <bsd.prog.mk>
