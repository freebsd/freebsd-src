# $FreeBSD: src/sys/modules/syscons/Makefile,v 1.11.2.1 2000/05/06 02:09:48 obrien Exp $

SUBDIR =
SUBDIR += blank
SUBDIR += daemon
SUBDIR += fade
SUBDIR += fire
SUBDIR += green
SUBDIR += logo
SUBDIR += rain
SUBDIR += snake
SUBDIR += star
SUBDIR += warp

.if ${MACHINE_ARCH} == "i386"
SUBDIR += apm
.endif

.include <bsd.subdir.mk>
