# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MACHINE_ARCH} == "sparc64"
.if ${MK_LIBTHR} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif
.elif ${MK_LIBPTHREAD} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
