#
# $Id: Makefile,v 1.3 1998/05/04 22:01:27 jb Exp $
#

GASES=		${MACHINE_ARCH} ${CROSS_TOOLS} ${CROSS_TARGETS}
.for _g in ${GASES}
.if exists(${.CURDIR}/as_${_g})
SUBDIR+=	as_${_g}
.endif
.endfor

.include <bsd.subdir.mk>
