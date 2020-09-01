# $FreeBSD$

.include <src.opts.mk>

SUBDIR= \
	ctfconvert \
	ctfdump \
	ctfmerge \
	${_zinject} \
	${_zstream} \
	${_zstreamdump} \
	${_ztest}

SUBDIR.${MK_TESTS}+= tests

.if ${MK_ZFS} != "no"
_zinject= zinject
.if ${MK_LIBTHR} != "no"
_ztest=	ztest
_zstream = zstream
_zstreamdump = zstreamdump
.endif
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
