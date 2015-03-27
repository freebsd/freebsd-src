# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	readpng-capsicum-helper

.if ${MK_CHERI} != "no"
SUBDIR+=	readpng-cheri-helper
.endif

.include <bsd.subdir.mk>
