# $FreeBSD: src/sys/modules/hfa/Makefile,v 1.1 2002/04/17 00:24:57 mdodd Exp $

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
