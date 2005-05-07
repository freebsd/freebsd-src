# $FreeBSD: src/sys/modules/syscons/Makefile,v 1.14 2004/01/19 12:36:08 ru Exp $

SUBDIR=	${_apm} \
	blank \
	daemon \
	dragon \
	fade \
	fire \
	green \
	logo \
	rain \
	snake \
	star \
	warp

.if ${MACHINE_ARCH} == "i386"
_apm=	apm
.endif

.include <bsd.subdir.mk>
