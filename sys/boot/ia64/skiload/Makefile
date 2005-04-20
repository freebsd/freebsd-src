# $FreeBSD$

.PATH: ${.CURDIR}/../common

PROG= 		skiload
NOMAN=
NEWVERSWHAT=	"ia64 SKI boot" ${MACHINE_ARCH}
BINDIR?=	/boot
STRIP=		# We must not strip skiload at install time.

SRCS=		conf.c main.c start.S vers.c

.if !defined(NOFORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/${MACHINE_ARCH}
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

LIBSKI=		${.OBJDIR}/../libski/libski.a

# Always add MI sources 
.PATH: ${.CURDIR}/../../common
.include "${.CURDIR}/../../common/Makefile.inc"

CFLAGS+= -I${.CURDIR}/../../common
CFLAGS+= -I${.CURDIR}/../libski
CFLAGS+= -DLOADER

LDFLAGS=	-Wl,-T${.CURDIR}/ldscript.ia64

CLEANFILES=	vers.c ${PROG}.help

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

${PROG}.help: help.common
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

FILES=	${PROG}.help

DPADD=	${LIBFICL} ${LIBSKI} ${LIBSTAND}
LDADD=	${LIBFICL} ${LIBSKI} -lstand

.include <bsd.prog.mk>
