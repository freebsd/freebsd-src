# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "sparc64"
.if ${MK_LIBC_R} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif
.elif ${MK_LIBPTHREAD} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
