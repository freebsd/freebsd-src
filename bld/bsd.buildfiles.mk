# $FreeBSD$

.if ${__MKLVL__} != 1
all: genfiles
.endif

.include <bsd.dirdep.mk>
.include <bsd.genfiles.mk>
