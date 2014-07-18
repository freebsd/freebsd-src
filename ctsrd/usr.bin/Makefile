# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	browser \
	${_compositor_demos} \
	minifile \
	writefile

# XXX: The latter two should be buildable without cheri but aren't yet.
.if ${MK_CHERI} != "no"
SUBDIR+=	cheripoint \
		pictview \
		pngsb
.endif

.include <bsd.subdir.mk>
