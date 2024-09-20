.include <src.opts.mk>

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I.

.if !defined(BOOTSTRAPPING)
WARNS?=	3
CFLAGS+=	-I${SRCTOP}/contrib/bsddialog/lib -DHAVE_BSDDIALOG
LIBADD=		bsddialog
.endif

ZONETAB=	${SRCTOP}/contrib/tzdata/zone1970.tab
baseline: ${PROG} ${ZONETAB}
	${.OBJDIR}/${PROG} -d ${ZONETAB} > ${.CURDIR}/baseline

.PHONY:	baseline

.include <bsd.prog.mk>
