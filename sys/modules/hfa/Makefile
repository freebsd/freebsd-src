# $FreeBSD: src/sys/modules/hfa/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

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
