#
# $FreeBSD: src/gnu/usr.bin/binutils/as/Makefile,v 1.4.2.1 1999/08/29 14:31:55 peter Exp $
#

GASES=		${MACHINE_ARCH} ${CROSS_TOOLS} ${CROSS_TARGETS}
.for _g in ${GASES}
.if exists(${.CURDIR}/as_${_g})
SUBDIR+=	as_${_g}
.endif
.endfor

.include <bsd.subdir.mk>
