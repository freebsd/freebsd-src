# $FreeBSD$

NO_MAN=

.include <bsd.own.mk>
MK_SSP=		no

PROG=		skiload
STRIP=		# We must not strip skiload at install time.

SRCS=		acpi_stub.c conf.c delay.c efi_stub.c exit.c main.c \
		pal_stub.S sal_stub.c skiconsole.c skifs.c skimd.c \
		ssc.c start.S time.c vers.c

CFLAGS+=	-I${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_CPUARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT}

NEWVERSWHAT=	"SKI boot" ${MACHINE_CPUARCH}

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

CLEANFILES=	vers.c

LIBIA64=	${.OBJDIR}/../common/libia64.a
.if ${MK_FORTH} != "no"
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

DPADD=		${LIBIA64} ${LIBFICL} ${LIBSTAND}
LDADD=		-Wl,--whole-archive ${LIBIA64} -Wl,--no-whole-archive \
		    ${LIBFICL} -lstand

.include <bsd.prog.mk>
