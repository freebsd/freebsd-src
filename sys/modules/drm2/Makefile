# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
.endif

SUBDIR = \
	drm2 \
	i915kms \
	radeonkms \
	${_radeonkmsfw}

.include <bsd.subdir.mk>
