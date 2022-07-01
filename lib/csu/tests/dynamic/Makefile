# $FreeBSD$

.PATH: ${.CURDIR:H}

.if exists(./Makefile.${MACHINE_ARCH})
.include "./Makefile.${MACHINE_ARCH}"
.endif


.include <src.opts.mk>
MK_PIE=	no

.include "../Makefile.tests"
.include <bsd.test.mk>
