# $FreeBSD$

SUBDIR=	browser \
	${_compositor_demos} \
	writefile

# XXX: The latter three should be buildable without cheri but aren't yet.
.if ${MK_CHERI} = "no"
SUBDIR+=	cheripoint \
		minifile \
		pictview \
		pngsb
.endif

.include <bsd.subdir.mk>
