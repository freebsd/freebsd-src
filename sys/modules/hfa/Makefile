# $FreeBSD$

SUBDIR=	${eisa} hfa pci ${sbus}

.ifdef notyet
.if ${MACHINE_ARCH} == "i386"
eisa=	eisa
.endif

.if ${MACHINE} == "sparc64"
sbus=	sbus
.endif
.endif

.include <bsd.subdir.mk>
