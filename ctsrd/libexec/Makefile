# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	minifile-capsicum-helper \
	readpng-capsicum-helper

.if ${MK_CHERI} != "no"
SUBDIR+=	minifile-cheri-helper \
		readpng-cheri-helper
.endif

.include <bsd.subdir.mk>
