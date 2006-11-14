# $FreeBSD$

SUBDIR=	hfa pci

.ifdef notyet
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=eisa
.endif

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR+=sbus
.endif
.endif

.include <bsd.subdir.mk>
