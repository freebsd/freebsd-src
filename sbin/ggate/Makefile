# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBPTHREAD} != "no" && (${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no")
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
