# $FreeBSD$

.include <bsd.own.mk>

.if ${MACHINE_CPUARCH} == "amd64"
_radeonkms=	radeonkms
. if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
. endif
.endif

.if ${MACHINE_CPUARCH} == "i386"
. if ${MACHINE} != "pc98"
_radeonkms=	radeonkms
.  if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
.  endif
. endif
.endif

SUBDIR = \
	drm2 \
	i915kms \
	${_radeonkms} \
	${_radeonkmsfw}

.include <bsd.subdir.mk>
