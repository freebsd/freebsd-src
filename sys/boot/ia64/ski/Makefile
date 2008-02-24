# $FreeBSD: src/sys/boot/ia64/ski/Makefile,v 1.20 2006/11/05 22:03:04 marcel Exp $

NO_MAN=

.include <bsd.own.mk>

PROG=		skiload
STRIP=		# We must not strip skiload at install time.

SRCS=		acpi_stub.c conf.c delay.c efi_stub.c exit.c main.c \
		pal_stub.S sal_stub.c skiconsole.c skifs.c skimd.c \
		ssc.c start.S time.c vers.c

CFLAGS+=	-I${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_ARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT}

NEWVERSWHAT=	"SKI boot" ${MACHINE_ARCH}

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
