# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	lib .WAIT \
	sbin usr.bin usr.sbin

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
