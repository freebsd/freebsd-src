# $FreeBSD: src/sys/modules/syscons/Makefile,v 1.15 2005/07/10 11:47:04 marius Exp $

SUBDIR=	${_apm} \
	blank \
	${_daemon} \
	${_dragon} \
	fade \
	${_fire} \
	green \
	${_logo} \
	${_rain} \
	${_snake} \
	${_star} \
	${_warp}

.if ${MACHINE_ARCH} == "i386"
_apm=		apm
.endif

.if ${MACHINE_ARCH} != "sparc64"
_daemon=	daemon
_dragon=	dragon
_fire=		fire
_logo=		logo
_rain=		rain
_snake=		snake
_star=		star
_warp=		warp
.endif

.include <bsd.subdir.mk>
