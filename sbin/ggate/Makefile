# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no" || \
    (${MACHINE_ARCH} != "sparc64" && ${MK_LIBPTHREAD} != "no")   
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
