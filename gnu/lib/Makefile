# $FreeBSD$

.include <src.opts.mk>

SUBDIR=
SUBDIR.${MK_DIALOG}+=	libdialog
SUBDIR.${MK_TESTS}+=	tests

.if ${MK_GNU_GREP} != "no"
SUBDIR+=		libregex
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
