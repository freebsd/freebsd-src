# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= \
	ctfconvert \
	ctfdump \
	ctfmerge \
	sgsmsg \
	${_ztest}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
