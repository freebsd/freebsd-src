#
# $FreeBSD$
#

.include "${.CURDIR}/../Makefile.inc0"

GASES=		${BINUTIL_ARCH}
GASES+=		${CROSS_TOOLS} ${CROSS_TARGETS}	
.for _g in ${GASES}
.if exists(${.CURDIR}/as_${_g})
SUBDIR+=	as_${_g}
.endif
.endfor

.include <bsd.subdir.mk>
